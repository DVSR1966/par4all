/*

  $Id$

  Copyright 1989-2009 MINES ParisTech

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/
#define _GNU_SOURCE
#include "genC.h"
#include "linear.h"
#include "ri.h"

#include "resources.h"

#include "misc.h"
#include "ri-util.h"
#include "pipsdbm.h"

#include "semantics.h"
#include "effects-generic.h"
#include "transformations.h"
#include "preprocessor.h"

#include "dg.h"

typedef dg_arc_label arc_label;
typedef dg_vertex_label vertex_label;

#include "graph.h"

#include "sac.h"
#include "patterns.tab.h"

#include "properties.h"
#include "statistics.h"

#include "misc.h"
#include <ctype.h>
#include "c_syntax.h"

#define MAX_PACK 16
#define VECTOR_POSTFIX "_vec"

static argumentInfo* arguments = NULL;
static int nbArguments = 0;
static int nbAllocatedArguments = 0;

static float gSimdCost;

/* expression is an simd vector
 * if it is a reference array
 * containing VECTOR_POSTFIX in its name
 */
static
bool simd_vector_p(expression e)
{
    syntax s = expression_syntax(e);
    return 
           syntax_reference_p(s)
        && strstr(entity_local_name(reference_variable(syntax_reference(s))),VECTOR_POSTFIX)
        && type_variable_p(entity_type(reference_variable(syntax_reference(s))))
        && ! ENDP( variable_dimensions(type_variable(entity_type(reference_variable(syntax_reference(s))))))
    ;
}

entity vectorElement_vector(vectorElement ve)
{
    return simdStatementInfo_vectors(vectorElement_statement(ve))[vectorElement_vectorIndex(ve)];
}

int vectorElement_vectorLength(vectorElement ve)
{
    return opcode_vectorSize(simdStatementInfo_opcode(vectorElement_statement(ve)));
}

/*
   This function return the basic corresponding to the argNum-th
   argument of opcode oc
   */
int get_basic_from_opcode(opcode oc, int argNum)
{
    int type = INT(gen_nth(argNum, opcode_argType(oc)));

    switch(type)
    {
        case QI_REF_TOK:
            return is_basic_int;
        case HI_REF_TOK:
            return is_basic_int;
        case SI_REF_TOK:
            return is_basic_int;
        case DI_REF_TOK:
            return is_basic_int;
        case SF_REF_TOK:
            return is_basic_float;
        case DF_REF_TOK:
            return is_basic_float;
        default:
            pips_internal_error("subword size unknown.\n");
    }

    return is_basic_int;
}

int get_subwordSize_from_opcode(opcode oc, int argNum)
{
    int type = INT(gen_nth(argNum, opcode_argType(oc)));

    switch(type)
    {
        case QI_REF_TOK:
            return 8;
        case HI_REF_TOK:
            return 16;
        case SI_REF_TOK:
            return 32;
        case DI_REF_TOK:
            return 64;
        case SF_REF_TOK:
            return 32;
        case DF_REF_TOK:
            return 64;
        default:
            pips_internal_error("subword size unknown.\n");
    }

    return 8;
}

int get_subwordSize_from_vector(entity vec)
{
    char * name = entity_local_name(vec);

    switch(name[2])
    {
        case 'q':
            return 8;
        case 'h':
            return 16;
        case 's':
            return 32;
        case 'd':
            return 64;
        default:
            pips_internal_error("subword size unknown.\n");
    }

    return 8;
}

/* Computes the optimal opcode for simdizing 'argc' statements of the
 * 'kind' operation, applied to the 'args' arguments
 */
static opcode get_optimal_opcode(opcodeClass kind, int argc, list* args)
{
    int i;
    opcode best;
    /* Based on the available implementations of the operation, decide
     * how many statements to pack together
     */
    best = opcode_undefined;
    FOREACH(OPCODE,oc,opcodeClass_opcodes(kind))
    {
        bool bTagDiff = FALSE;

        for(i = 0; i < argc; i++)
        {
            int count = 0;
            int width = 0;

            FOREACH(EXPRESSION,arg,args[i])
            {
                int bTag;
                basic bas;

                if (!expression_reference_p(arg))
                {
                    count++;
                    continue;
                }

                bas = get_basic_from_array_ref(expression_reference(arg));
                bTag = basic_tag(bas);

                if((bTag != get_basic_from_opcode(oc, count)) &&
                        (bTag != is_basic_logical))
                {
                    bTagDiff = TRUE;
                    break;
                }

                switch(bTag)
                {
                    case is_basic_int: width = 8 * basic_int(bas); break;
                    case is_basic_float: width = 8 * basic_float(bas); break;
                    case is_basic_logical: width = 1; break;
                    default: pips_user_error("basic %d not supported",bTag);
                }

                if(width > get_subwordSize_from_opcode(oc, count))
                {
                    bTagDiff = TRUE;
                    break;
                }

                count++;

            }
        }

        if ( (!bTagDiff) &&
                (opcode_vectorSize(oc) <= argc) &&
                ((best == opcode_undefined) || 
                 (opcode_vectorSize(oc) > opcode_vectorSize(best))) )
            best = oc;
    }

    return best;
}

entity get_function_entity(string name)
{
    entity e = local_name_to_top_level_entity(name); 
    if ( entity_undefined_p( e ) )
    {
        pips_user_warning("entity %s not defined, please load the appropriate definition source file",name);
    }

    return e;
}

/* 
   This function aims to fill the referenceInfo i from the reference r. 
   referenceInfo is used later to detect consecutive reference in a
   simd_load parameter list.
   This function returns TRUE if successful.
   */
bool analyse_reference(reference r, referenceInfo i)
{
    syntax s;

    // Fill the two first fields of referenceInfo
    referenceInfo_reference(i) = r;
    referenceInfo_nbDimensions(i) = gen_length(reference_indices(r));

    // If r is not an array, then FALSE is returned
    if (referenceInfo_nbDimensions(i) == 0)
        return FALSE;

    //Initialization of the two list of referenceInfo
    referenceInfo_lExp(i) = NIL;
    referenceInfo_lOffset(i) = NIL;

    // For each indices of r
    FOREACH(EXPRESSION, exp,reference_indices(r))
    {
        s = expression_syntax(exp);
        switch(syntax_tag(s))
        {
            // exp is a call expression
            case is_syntax_call:
                {
                    call c = syntax_call(s);

                    // r is for exemple A(2)
                    if (call_constant_p(c))
                    {
                        constant cn;

                        cn = value_constant(entity_initial(call_function(c)));

                        if (constant_int_p(cn))
                            referenceInfo_lOffset(i) = CONS(INT, constant_int(cn), referenceInfo_lOffset(i));
                        else
                            return FALSE;
                        /* use gen_cons instead of CONS here to bypass type check */
                        referenceInfo_lExp(i) = gen_cons(expression_undefined, referenceInfo_lExp(i));
                    }
                    // r is for exemple A(EXP + 3)(it is supported)
                    // or A(EXP1 + EXP2)(it's not supported)
                    else if (ENTITY_PLUS_P(call_function(c)))
                    {
                        cons * arg = call_arguments(c);

                        syntax e;

                        // Strange error
                        if ((arg == NIL) || (CDR(arg) == NIL))
                            return FALSE;

                        e = expression_syntax(EXPRESSION(CAR(CDR(arg))));
                        // If r is A(EXP + 3), e should contain 3
                        // If r is A(EXP1 + EXP2), e should contain EXP2
                        switch(syntax_tag(e))
                        {
                            // Fill referenceInfo in the following situation:
                            // "If r is A(EXP + 3), e should contain 3"
                            case is_syntax_call:
                                {
                                    referenceInfo_lExp(i) = CONS(EXPRESSION, EXPRESSION(CAR(arg)), referenceInfo_lExp(i));

                                    call cc = syntax_call(e);

                                    if(!call_constant_p(cc))
                                        return FALSE;

                                    constant ccn = value_constant(entity_initial(call_function(cc)));

                                    if (constant_int_p(ccn))
                                        referenceInfo_lOffset(i) = CONS(INT, constant_int(ccn), referenceInfo_lOffset(i));
                                    else
                                        return FALSE;
                                    break;
                                }

                                // Fill referenceInfo in the following situation:
                                // "If r is A(EXP1 + EXP2), e should contain EXP2"
                            case is_syntax_reference:
                                {
                                    referenceInfo_lExp(i) = CONS(EXPRESSION, exp, referenceInfo_lExp(i));
                                    referenceInfo_lOffset(i) = CONS(INT, 0, referenceInfo_lOffset(i));
                                    break;
                                }

                            default:
                            case is_syntax_range:
                                return FALSE;
                        }	    
                    }
                    // Same comments as for ENTITY_MINUS_P(call_function(c))
                    else if (ENTITY_MINUS_P(call_function(c)))
                    {
                        cons * arg = call_arguments(c);

                        syntax e;

                        if ((arg == NIL) || (CDR(arg) == NIL))
                            return FALSE;

                        e = expression_syntax(EXPRESSION(CAR(CDR(arg))));
                        switch(syntax_tag(e))
                        {
                            case is_syntax_call:
                                {
                                    referenceInfo_lExp(i) = CONS(EXPRESSION, EXPRESSION(CAR(arg)), referenceInfo_lExp(i));

                                    call cc = syntax_call(e);

                                    if(!call_constant_p(cc))
                                        return FALSE;

                                    constant ccn = value_constant(entity_initial(call_function(cc)));

                                    if (constant_int_p(ccn))
                                        referenceInfo_lOffset(i) = CONS(INT, -constant_int(ccn), referenceInfo_lOffset(i));
                                    else
                                        return FALSE;
                                    break;
                                }

                            case is_syntax_reference:
                                {
                                    referenceInfo_lExp(i) = CONS(EXPRESSION, exp, referenceInfo_lExp(i));
                                    referenceInfo_lOffset(i) = CONS(INT, 0, referenceInfo_lOffset(i));
                                    break;
                                }

                            default:
                            case is_syntax_range:
                                return FALSE;
                        }	    
                    }
                    // If nothing special has been detected
                    else
                    {
                        referenceInfo_lExp(i) = CONS(EXPRESSION, exp, referenceInfo_lExp(i));
                        referenceInfo_lOffset(i) = CONS(INT, 0, referenceInfo_lOffset(i));
                    }
                    break;
                }

                // If nothing special has been detected
            case is_syntax_reference:
                referenceInfo_lExp(i) = CONS(EXPRESSION, exp, referenceInfo_lExp(i));
                referenceInfo_lOffset(i) = CONS(INT, 0, referenceInfo_lOffset(i));
                break;

            default:
                return FALSE;
        }
    }

    return TRUE;
}

/*
   This function returns the expression of the index that corresponds
   to the "consecutive index" according to the memory disposition of
   arrays.

   For instance:
   In a fortran program, let's define A[I][J][K],
   then the "consecutive index" is I.
   In a C program, let's define A[I][J][K],
   then the "consecutive index" is K.
   */
static expression get_consec_exp(referenceInfo refInfo)
{
    if(get_bool_property("SIMD_FORTRAN_MEM_ORGANISATION"))
    {
        return EXPRESSION(CAR(gen_last(referenceInfo_lExp(refInfo))));
    }

    return EXPRESSION(CAR(referenceInfo_lExp(refInfo)));
}

/*
   This function returns the int of the index that corresponds
   to the "consecutive index" according to the memory disposition of
   arrays.

   See above.
   */
static int get_consec_offset(referenceInfo refInfo)
{
    if(get_bool_property("SIMD_FORTRAN_MEM_ORGANISATION"))
    {
        return INT(CAR(gen_last(referenceInfo_lOffset(refInfo))));
    }

    return INT(CAR(referenceInfo_lOffset(refInfo)));
}

/*
   This function returns the position of the index that corresponds
   to the "consecutive index" according to the memory disposition of
   arrays.

   See above.
   */
static int get_consec_ind_number(referenceInfo refInfo)
{
    if(get_bool_property("SIMD_FORTRAN_MEM_ORGANISATION"))
    {
        return gen_length(referenceInfo_lOffset(refInfo));
    }

    return 1;
}

/*
   This function returns TRUE if firstRef and cRef have consecutive 
   memory locations.
   */
static bool consecutive_refs_p(referenceInfo firstRef, int lastOffset, referenceInfo cRef)
{
    bool bIndEq = TRUE;
    bool bConsIndEq = FALSE;
    int counter = 0;

    // If firstRef and cRef don't have the same dimension,
    // then they are not consecutive references.
    if(referenceInfo_nbDimensions(firstRef) != referenceInfo_nbDimensions(cRef))
        return FALSE;

    list lOff1 = referenceInfo_lOffset(firstRef);
    list lOff2 = referenceInfo_lOffset(cRef);
    list lInd2 = referenceInfo_lExp(cRef);


    // Let's look at each index of the references
    FOREACH(EXPRESSION, ind1,referenceInfo_lExp(firstRef))
    {
        int off1,off2;
        expression ind2;
        counter++;

        // If TRUE, it means that ind1 corresponds to
        // the "consecutive index" (See get_consec_ind_number()
        // comments for further information), so let's not
        // check ind1 right now.
        if(counter == get_consec_ind_number(firstRef))
        {
            POP(lOff1);
            POP(lOff2);
            POP(lInd2);
            continue;
        }

        ind2 = EXPRESSION(CAR(lInd2));
        off1 = INT(CAR(lOff1));
        off2 = INT(CAR(lOff2));

        // If TRUE, it means that the two indices are different, so return FALSE.
        if(( expression_undefined_p(ind1) && !expression_undefined_p(ind2)) ||
           (!expression_undefined_p(ind1) &&  expression_undefined_p(ind2)) 
          )
        {
            bIndEq = FALSE;
        }
        // If TRUE, it means that the two indices can be identical, 
        // let's check the offsets.
        else if(expression_undefined_p(ind1) && expression_undefined_p(ind2))
        {
            // If TRUE, it means that the two indices are different, so return FALSE.
            if(off1 != off2)
                bIndEq = FALSE;
        }
        else
        {
            // If TRUE, it means that the two indices are identical
            if(!(same_expression_p(ind1, ind2) && (off1 == off2)))
                bIndEq = FALSE;
        }

        POP(lOff1);
        POP(lOff2);
        POP(lInd2);

    }

    // This if-elseif-else statement checks if get_consec_exp(firstRef)
    // and get_consec_exp(cRef) are identical
    if(expression_undefined_p(get_consec_exp(firstRef)) &&
       expression_undefined_p(get_consec_exp(cRef)))
    {
        bConsIndEq = TRUE;
    }
    else if(expression_undefined_p(get_consec_exp(firstRef)) ||
            expression_undefined_p(get_consec_exp(cRef)) )
    {
        bConsIndEq = FALSE;
    }
    else
    {
        bConsIndEq = same_expression_p(get_consec_exp(firstRef), get_consec_exp(cRef));
    }

    return     bConsIndEq
            && same_entity_p(reference_variable(referenceInfo_reference(firstRef)),reference_variable(referenceInfo_reference(cRef)))
            && (referenceInfo_nbDimensions(firstRef) == referenceInfo_nbDimensions(cRef))
            && (lastOffset + 1 == get_consec_offset(cRef))
            && bIndEq;
}

referenceInfo make_empty_referenceInfo()
{
    return make_referenceInfo(reference_undefined, 0, NIL, NIL, NIL);
}

void free_empty_referenceInfo(referenceInfo ri)
{
    referenceInfo_reference(ri) = reference_undefined;
    gen_free_list(referenceInfo_lExp(ri));
    gen_free_list(referenceInfo_lIndex(ri));
    gen_free_list(referenceInfo_lOffset(ri));
    referenceInfo_lExp(ri) = NIL;
    referenceInfo_lIndex(ri) = NIL;
    referenceInfo_lOffset(ri) = NIL;
    free_referenceInfo(ri);
}

/*
   This function returns the vector type string by reading the name of the first 
   element of lExp
   */
static string get_simd_vector_type(list lExp)
{
    string result = NULL;

    FOREACH(EXPRESSION, exp,lExp)
    {
        type t = entity_type(reference_variable(
                    syntax_reference(expression_syntax(
                            exp))));

        if(type_variable_p(t))
        {
    //        basic bas = variable_basic(type_variable(t));

            result = strdup(entity_name(
                        reference_variable(syntax_reference(expression_syntax(exp)))
                        ));
            *strrchr(result,'_')='\0';
            /* switch to upper cases... */
            result=strupper(result,result);

            break;

        }
    }

    return result;
}

/*
   This function returns the name of a vector from the data inside it
   */
static string get_vect_name_from_data(int argc, expression exp)
{
    char prefix[5];
    string result;
    basic bas;
    int itemSize;

    bas = get_basic_from_array_ref(syntax_reference(expression_syntax(exp)));

    prefix[0] = 'v';
    prefix[1] = '0'+argc;

    switch(basic_tag(bas))
    {
        case is_basic_int:
            prefix[3] = 'i';
            itemSize = 8 * basic_int(bas);
            break;

        case is_basic_float:
            prefix[3] = 'f';
            itemSize = 8 * basic_float(bas);
            break;

        case is_basic_logical:
            prefix[3] = 'i';
            itemSize = 8 * basic_logical(bas);
            break;

        default:
            return strdup("");
            break;
    }

    switch(itemSize)
    {
        case 8:  prefix[2] = 'q'; break;
        case 16: prefix[2] = 'h'; break;
        case 32: prefix[2] = 's'; break;
        case 64: prefix[2] = 'd'; break;
    }

    prefix[4] = 0;

    result = strdup(prefix);

    /* switch to upper cases... */
    result=strupper(result,result);

    return result;
}

static
void replace_subscript(expression e)
{
    if( !simd_vector_p(e) && ! expression_constant_p(e) )
    {
        expression e_copy = copy_expression(e);
        if( ! syntax_undefined_p( expression_syntax(e) ) ) free_syntax(expression_syntax(e));
        if( ! normalized_undefined_p( expression_normalized(e) ) ) free_normalized(expression_normalized(e));

        *e=*MakeUnaryCall(CreateIntrinsic(ADDRESS_OF_OPERATOR_NAME), e_copy );
    }

}

statement make_exec_statement_from_name(string ename, list args)
{
    /* SG: ugly patch to make sure fortran's parameter passing and c's are respected */
    if( c_module_p(get_current_module_entity()) )
    {
        if( strstr(ename,SIMD_GEN_LOAD_NAME) )
        {
            replace_subscript( EXPRESSION(CAR(args)));
        }
        else
        {
            FOREACH(EXPRESSION,e,args) replace_subscript(e);
        }
    }
    return call_to_statement(make_call(get_function_entity(ename), args));
}
statement make_exec_statement_from_opcode(opcode oc, list args)
{
    return make_exec_statement_from_name( opcode_name(oc) , args );
}

static statement make_loadsave_statement(int argc, list args, bool isLoad)
{
    enum {
        CONSEC_REFS,
        CONSTANT,
        OTHER
    } argsType;
    const char funcNames[3][2][20] = {
        { SIMD_SAVE_NAME"_",          SIMD_LOAD_NAME"_"          },
        { SIMD_CONS_SAVE_NAME"_", SIMD_CONS_LOAD_NAME"_" },
        { SIMD_GEN_SAVE_NAME"_",  SIMD_GEN_LOAD_NAME"_"  } };
    referenceInfo firstRef = make_empty_referenceInfo();
    referenceInfo cRef = make_empty_referenceInfo();
    int lastOffset = 0;
    char *functionName;

    string lsType = local_name(get_simd_vector_type(args));

    /* the function should not be called with an empty arguments list */
    assert((argc > 1) && (args != NIL));

    /* first, find out if the arguments are:
     *    - consecutive references to the same array
     *    - all constant
     *    - or any other situation
     */

    /* classify according to the second element
     * (first one should be the SIMD vector) */
    expression exp = EXPRESSION(CAR(CDR(args)));
    if (expression_constant_p(exp))
    {
        argsType = CONSTANT;
    }
    // If e is a reference expression, let's analyse this reference
    else if ( (expression_reference_p(exp)) &&
            (analyse_reference(expression_reference(exp), firstRef)) )
    {
        lastOffset = get_consec_offset(firstRef);
        argsType = CONSEC_REFS;
    }
    else
        argsType = OTHER;

    /* now verify the estimation on the first element is correct, and update
     * parameters needed later */
    FOREACH( EXPRESSION, e, CDR(CDR(args)) )
    {
        if (argsType == OTHER)
            break;
        else if (argsType == CONSTANT)
        {
            if (!expression_constant_p(e))
            {
                argsType = OTHER;
                break;
            }
        }
        else if (argsType == CONSEC_REFS)
        {
            // If e is a reference expression, let's analyse this reference
            // and see if e is consecutive to the previous references
            if ( (expression_reference_p(e)) &&
                    (analyse_reference(expression_reference(e), cRef)) &&
                    (consecutive_refs_p(firstRef, lastOffset, cRef)) )
            {
                lastOffset = get_consec_offset(cRef);
            }
            else
            {
                argsType = OTHER;
                break;
            }
        }
    }

    /* Now that the analyze is done, we can generate an "optimized"
     * load instruction.
     */
    switch(argsType)
    {
        case CONSEC_REFS:
            {

                string realVectName = get_vect_name_from_data(argc, EXPRESSION(CAR(CDR(args))));

                if(strcmp(strchr(realVectName, MODULE_SEP)?local_name(realVectName):realVectName, lsType))
                {
                    /*string temp = local_name(lsType);*/

                    lsType = strdup(concatenate(realVectName,
                                "_TO_", /*temp*/lsType, (char *) NULL));
                }
                if(get_bool_property("SIMD_FORTRAN_MEM_ORGANISATION"))
                {
                    args = gen_make_list( expression_domain, 
                            EXPRESSION(CAR(args)),
                            reference_to_expression(referenceInfo_reference(firstRef)),
                            NULL);
                }
                else
                {
                    //expression addr = MakeUnaryCall(CreateIntrinsic("&"), reference_to_expression(referenceInfo_reference(firstRef)));
                    expression addr = reference_to_expression(referenceInfo_reference(firstRef));
                    args = gen_make_list( expression_domain, 
                            EXPRESSION(CAR(args)),
                            addr,
                            NULL);
                }

                gSimdCost += - argc + 1;

                break;
            }

        case CONSTANT:
            {
                gSimdCost += - argc + 1;

                break;
            }

        case OTHER:
        default:
            break;
    }

    free_empty_referenceInfo(firstRef);
    free_empty_referenceInfo(cRef);

    asprintf(&functionName, "%s%s", funcNames[argsType][isLoad], lsType);
    statement es = make_exec_statement_from_name(functionName, args);
    free(functionName);
    return es;

}

static statement make_load_statement(int argc, list args)
{
    return make_loadsave_statement(argc, args, TRUE);
}

static statement make_save_statement(int argc, list args)
{
    return make_loadsave_statement(argc, args, FALSE);
}


/*
   This function creates a simd vector.
   */
static entity make_new_simd_vector(int itemSize, int nbItems, int basicTag)
{
    //extern list integer_entities, real_entities, double_entities;

    basic simdVector;

    entity new_ent, mod_ent;
    char prefix[5]={ 'v', '0', '\0', '\0', '\0' },
         num[1 + sizeof(VECTOR_POSTFIX) + 3 ],
         name[sizeof(prefix)+sizeof(num)+1];
    static int number = 0;

    mod_ent = get_current_module_entity();

    /* build the variable prefix code, which is in fact also the type */
    prefix[1] += nbItems;
    switch(itemSize)
    {
        case 8:  prefix[2] = 'q'; break;
        case 16: prefix[2] = 'h'; break;
        case 32: prefix[2] = 's'; break;
        case 64: prefix[2] = 'd'; break;
    }


    switch(basicTag)
    {
        case is_basic_int:
            simdVector = make_basic_int(itemSize/8);
            prefix[3] = 'i';
            break;

        case is_basic_float:
            simdVector = make_basic_float(itemSize/8);
            prefix[3] = 'f';
            break;

        default:
            simdVector = make_basic_int(itemSize/8);
            prefix[3] = 'i';
            break;
    }

    pips_assert("buffer doesnot overflow",number<1000);
    sprintf(name, "%s%s%u",prefix,VECTOR_POSTFIX,number++);
    list lis=CONS(DIMENSION, make_dimension(int_to_expression(0),int_to_expression(nbItems-1)), NIL);  
    new_ent = make_new_array_variable_with_prefix(name, mod_ent , simdVector, lis);

#if 0
    string type_name = strdup(concatenate(prefix,"_struct", (char *) NULL));
    entity str_type = FindOrCreateEntity(entity_local_name(mod_ent), type_name);
    entity_type(str_type) =make_type_variable(make_variable(simdVector,NIL,NIL)); 

    entity str_dec = FindOrCreateEntity(entity_local_name(mod_ent), name);
    entity_type(str_dec) = entity_type(str_type);
#endif

    AddLocalEntityToDeclarations(new_ent,mod_ent,
            c_module_p(mod_ent)?get_current_module_statement():statement_undefined);

    return new_ent;
}

void reset_argument_info()
{
    int i;

    for(i=0; i<nbArguments; i++)
    {
        argumentInfo_expression(arguments[i]) = expression_undefined;
        gen_free_list(argumentInfo_placesAvailable(arguments[i]));
        argumentInfo_placesAvailable(arguments[i]) = NIL;
        free_argumentInfo(arguments[i]);
    }
    nbArguments = 0;
}

//Get id for this "class" of expression
//If not, add the expression to the mapping.
static int get_argument_id(expression e)
{
    int id;
    int i;

    //See if the expression has already been seen
    for(i=0; i<nbArguments; i++)
        if (same_expression_p(argumentInfo_expression(arguments[i]), e))
            return i;

    //Find an id for the operation
    id = nbArguments++;

    //Make room for the new operation if needed
    if (nbArguments > nbAllocatedArguments)
    {
        nbAllocatedArguments += 10;

        arguments = (argumentInfo*)realloc((void*)arguments, 
                sizeof(argumentInfo)*nbAllocatedArguments);

        if (arguments == NULL)
        {
            pips_user_error("Fatal error: could not allocate memory for arguments.\n");
        }
    }

    //Initialize members
    arguments[id] = make_argumentInfo(e, NIL);

    return id;
}

//Get info on argument with specified id
static argumentInfo get_argument_info(int id)
{
    return ((id>=0) && (id<nbArguments)) ?
        arguments[id] : argumentInfo_undefined;
}

statementInfo make_nonsimd_statement_info(statement s)
{
    statementInfo ssi;
    int i;

    ssi = make_statementInfo_nonsimd(s);

    /* see if some expressions are modified.
     * If that is the case, invalidate the list of their available places 
     */
    for(i=0; i<nbArguments; i++)
    {
        //for now, reset all...
        gen_free_list(argumentInfo_placesAvailable(arguments[i]));
        argumentInfo_placesAvailable(arguments[i]) = NIL;
    }

    return ssi;
}

static vectorElement make_vector_element(simdStatementInfo ssi, int i, int j)
{
    return make_vectorElement(ssi, i, j);
}

static vectorElement copy_vector_element(vectorElement ve)
{
    return make_vectorElement(vectorElement_statement(ve),
            vectorElement_vectorIndex(ve),
            vectorElement_element(ve));
}

static statementInfo make_simd_statement_info(opcodeClass kind, opcode oc, list* args)
{
    statementInfo si;
    simdStatementInfo ssi;
    int i, nbargs;

    /* find out the number of arguments needed */
    nbargs = opcodeClass_nbArgs(kind);

    /* allocate memory */
    ssi = make_simdStatementInfo(oc, 
            nbargs,
            (entity *)malloc(sizeof(entity)*nbargs),
            (statementArgument*)malloc(sizeof(statementArgument) * nbargs * opcode_vectorSize(oc)));
    si = make_statementInfo_simd(ssi);

    /* create the simd vector entities */
    hash_table reference_to_entity = hash_table_make(hash_pointer,HASH_DEFAULT_SIZE);
    int j=nbargs-1;
    FOREACH(EXPRESSION,exp,*args)
    {
        int basicTag = get_basic_from_opcode(oc, j);
        simdStatementInfo_vectors(ssi)[j] = entity_undefined;
#if 1
         HASH_MAP(key,val, {
            if( reference_equal_p(expression_reference(exp),(reference)key ) ) {
                simdStatementInfo_vectors(ssi)[j] = val;
                break;
            }
        },reference_to_entity)
#endif
        if( entity_undefined_p(simdStatementInfo_vectors(ssi)[j]) )
        {
            simdStatementInfo_vectors(ssi)[j] = 
                make_new_simd_vector(get_subwordSize_from_opcode(oc, j),
                        opcode_vectorSize(oc),
                        basicTag);
            hash_put(reference_to_entity,expression_reference(exp),simdStatementInfo_vectors(ssi)[j]);
        }
        --j;
    }
    hash_table_free(reference_to_entity);

    /* Fill the matrix of arguments */
    for(j=0; j<opcode_vectorSize(oc); j++)
    {
        argumentInfo ai;
        expression e;
        statementArgument ssa;

        list l = args[j];

        for(i=nbargs-1; i>=0; i--)
        {
            e = EXPRESSION(CAR(l));

            //Store it in the argument's matrix
            ssa = make_statementArgument(e, NIL);
            simdStatementInfo_arguments(ssi)[j + opcode_vectorSize(oc) * i] = ssa;

            l = CDR(l);
        }

        //Build the dependance tree
        for(i=0; i<nbargs-1; i++)
        {
            //we read this variable
            ssa = simdStatementInfo_arguments(ssi)[j + opcode_vectorSize(oc) * i];
            e = statementArgument_expression(ssa);

            //Get the id of the argumet
            ai = get_argument_info(get_argument_id(e));

            //ssa depends on all the places where the expression was
            //used before
            statementArgument_dependances(ssa) = 
                gen_copy_seq(argumentInfo_placesAvailable(ai));

            //Remember that this variable can be found here too
            argumentInfo_placesAvailable(ai) = 
                CONS(VECTORELEMENT,
                        make_vector_element(ssi, i, j),
                        argumentInfo_placesAvailable(ai));
        }

        //Get the id of the last argument
        ssa = simdStatementInfo_arguments(ssi)[j + opcode_vectorSize(oc) * (nbargs-1)];
        e = statementArgument_expression(ssa);

        ai = get_argument_info(get_argument_id(e));

        //we write to this variable -->
        //Free the list of places available. Those places are
        //not relevant any more
        gen_free_list(argumentInfo_placesAvailable(ai));
        argumentInfo_placesAvailable(ai) = 
            CONS(VECTORELEMENT,
                    make_vector_element(ssi, i, j),
                    NIL);
    }

    return si;
}

static
void free_simd_statement_info(simdStatementInfo s)
{
    free(simdStatementInfo_vectors(s));
    free(simdStatementInfo_arguments(s));
}

list make_simd_statements(list kinds, cons* first, cons* last)
{
    list i;
    int index;
    list args[MAX_PACK];
    opcodeClass type;
    list instr; 
    list all_instr;

    pips_debug(3,"make_simd_statements 1\n");

    if (first == last)
        return gen_statementInfo_cons(
                make_nonsimd_statement_info(STATEMENT(CAR(first))),
                NIL);

    pips_debug(3,"make_simd_statements 2\n");

    i = first;
    all_instr = gen_statementInfo_cons(make_statementInfo(0,NULL), NIL);
    instr = all_instr;

    type = OPCODECLASS(CAR(kinds));
    while(i != CDR(last))
    {
        opcode oc;
        list j;

        /* get the variables */
        for( index = 0, j = i;
                (index < MAX_PACK) && (j != CDR(last));
                index++, j = CDR(j) )
        {
            match m = get_statement_match_of_kind(STATEMENT(CAR(j)), type);
            args[index] = match_args(m);
        }

        /* compute the opcode to use */
        oc = get_optimal_opcode(type, index, args);

        if (opcode_undefined_p(oc))
        {
            /* No optimized opcode found... */
            for( index = 0;
                    (index < MAX_PACK) && (i != CDR(last));
                    index++, i = CDR(i) )
            {
                CDR(instr) = gen_statementInfo_cons(
                        make_nonsimd_statement_info(STATEMENT(CAR(i))),
                        NIL);
                instr = CDR(instr);
            }
        }
        else
        {
            /* update the pointer to the next statement to be processed */
            for(index = 0; 
                    (index<opcode_vectorSize(oc)) && (i!=CDR(last)); 
                    index++)
            {
                i = CDR(i);
            }

            /* generate the statement information */
            CDR(instr) = gen_statementInfo_cons( 
                    make_simd_statement_info(type, oc, args),
                    NIL);
            instr = CDR(instr);
        }
    }

    instr = CDR(all_instr);
    CDR(all_instr) = NIL;
    gen_free_list(all_instr);
    pips_debug(3,"make_simd_statements 3\n");
    return instr;
}

void free_simd_statements(list sil)
{
    FOREACH(STATEMENTINFO,si,sil)
    {
        switch(statementInfo_tag(si))
        {
            case is_statementInfo_nonsimd:
                //free_statementInfo(si);
                break;
            case is_statementInfo_simd:
                free_simd_statement_info(statementInfo_simd(si));
                break;
        };
    }
}

static statement generate_exec_statement(simdStatementInfo si)
{
    list args = NIL;
    int i;

    for(i = 0; i < simdStatementInfo_nbArgs(si); i++)
    {
        args = CONS(EXPRESSION,
                entity_to_expression(simdStatementInfo_vectors(si)[i]),
                args);
    }

    gSimdCost += opcode_cost(simdStatementInfo_opcode(si));

    return make_exec_statement_from_opcode(simdStatementInfo_opcode(si), args);
}

static list merge_available_places(list l1, list l2, int element)
{
    list res = NIL;

    FOREACH(VECTORELEMENT,ei,l1)
    {
        FOREACH(VECTORELEMENT,ej,l2)
        {
            if (vectorElement_vector(ei) == vectorElement_vector(ej))
            {
                if (element < 0)
                {
                    vectorElement ve = copy_vector_element(ej);

                    //quite a hack here: this vectorElement represents in fact an 
                    //aggregate entity x int, where the int is the order param
                    //to be used for shuffle
                    vectorElement_element(ve) = 
                        vectorElement_element(ej) | 
                        (vectorElement_element(ei) << (-2*element));

                    res = CONS(VECTORELEMENT, ve, res);
                }
                else if (vectorElement_element(ei) == element)
                    res = CONS(VECTORELEMENT, ej, res);
            }
        }
    }

    return res;
}

static statement make_shuffle_statement(entity dest, entity src, int order)
{
    list args = gen_make_list(expression_domain,
            entity_to_expression(dest),
            entity_to_expression(src),
            make_integer_constant_expression(order),
            NULL);
    return make_exec_statement_from_name( "PSHUFW",args);
}

static statement generate_load_statement(simdStatementInfo si, int line)
{
    list args = NIL;
    int i;
    int offset = line * opcode_vectorSize(simdStatementInfo_opcode(si));
    list sourcesCopy = NIL;
    list sourcesShuffle = NIL;

    //try to see if the arguments have not already been loaded
    FOREACH(VECTORELEMENT, ve,statementArgument_dependances(simdStatementInfo_arguments(si)[offset]))
    {
        if (vectorElement_element(ve) == 0)
            sourcesCopy = CONS(VECTORELEMENT, ve, sourcesCopy);
        /*
           if ( FALSE)//(vectorElement_subwordSize(ve) == 16) &&
        //(vectorElement_vectorLength(ve) == 4) )
        {
        vectorElement e = copy_vector_element(ve);
        sourcesShuffle = CONS(VECTORELEMENT, e, sourcesShuffle);
        }
        */
    }

    for(i = 1; 
            (i<opcode_vectorSize(simdStatementInfo_opcode(si))) && 
            ((sourcesShuffle!=NIL) || (sourcesCopy!=NIL)); 
            i++)
    {
        list new_sources;

        //update the list of places where the copy can be found
        new_sources = merge_available_places(
                statementArgument_dependances(simdStatementInfo_arguments(si)[i + offset]), 
                sourcesCopy, 
                i);

        gen_free_list(sourcesCopy);
        sourcesCopy = new_sources;

        //update the list of places where shuffled copy can be found
        new_sources = merge_available_places(
                statementArgument_dependances(simdStatementInfo_arguments(si)[i + offset]), 
                sourcesShuffle,
                -i);

        gen_free_list(sourcesShuffle); //we should free the elements too (but not recusively, else we would free the simdStatementInfo...)
        sourcesShuffle = new_sources;
    }

    bool bSameSubwordSize = TRUE;

    if (sourcesCopy != NIL)
    {
        bSameSubwordSize = (get_subwordSize_from_opcode(simdStatementInfo_opcode(si), line+1) 
                == get_subwordSize_from_vector(vectorElement_vector(VECTORELEMENT(CAR(sourcesCopy)))));
    }
    else if(sourcesShuffle != NIL)
    {
        bSameSubwordSize = (get_subwordSize_from_opcode(simdStatementInfo_opcode(si), line+1) 
                == get_subwordSize_from_vector(vectorElement_vector(VECTORELEMENT(CAR(sourcesShuffle)))));
    }

    //Best case is we already have the same thing in another register
    if ((sourcesCopy != NIL) && bSameSubwordSize)
    {
        vectorElement vec = VECTORELEMENT(CAR(sourcesCopy));
        simdStatementInfo_vectors(si)[line] = vectorElement_vector(vec);
        return statement_undefined;
    }
    //Else, maybe we can use a shuffle instruction
    else if ((sourcesShuffle != NIL) && bSameSubwordSize)
    {
        vectorElement ve = VECTORELEMENT(CAR(sourcesShuffle));
        return make_shuffle_statement(simdStatementInfo_vectors(si)[line],
                vectorElement_vector(ve),
                vectorElement_element(ve));
    }
    //Only choice left is to load from memory
    else
    {
        //Build the arguments list
        for(i = opcode_vectorSize(simdStatementInfo_opcode(si))-1; 
                i >= 0; 
                i--)
        {
            args = CONS(EXPRESSION,
                    copy_expression(statementArgument_expression(simdStatementInfo_arguments(si)[i + offset])),
                    args);
        }
        args = CONS(EXPRESSION,
                entity_to_expression(simdStatementInfo_vectors(si)[line]),
                args);

        //Make a load statement
        return make_load_statement(
                opcode_vectorSize(simdStatementInfo_opcode(si)), 
                args);
    }
}

static statement generate_save_statement(simdStatementInfo si)
{
    list args = NIL;
    int i;
    int offset = opcode_vectorSize(simdStatementInfo_opcode(si)) * 
        (simdStatementInfo_nbArgs(si)-1);

    for(i = opcode_vectorSize(simdStatementInfo_opcode(si))-1; 
            i >= 0; 
            i--)
    {
        args = CONS(EXPRESSION,
                copy_expression(
                    statementArgument_expression(simdStatementInfo_arguments(si)[i + offset])),
                args);
    }

    args = CONS(EXPRESSION,
            entity_to_expression(simdStatementInfo_vectors(si)[simdStatementInfo_nbArgs(si)-1]),
            args);

    return make_save_statement(opcode_vectorSize(simdStatementInfo_opcode(si)),
            args);
}

list generate_simd_code(list/* <statementInfo> */ sil, float * simdCost)
{
    list sl_begin; /* <statement> */
    list sl; /* <statement> */

    sl = sl_begin = gen_statementInfo_cons( make_statementInfo(0,NULL), NIL);

    gSimdCost = 0;

    pips_debug(3,"generate_simd_code 1\n");

    /* this is the classical generation process:
     * several load, an exec and a store
     */
    FOREACH(STATEMENTINFO,si,sil)
    {
        if (statementInfo_nonsimd_p(si))
        {
            /* regular (non-SIMD) statement */
            sl = CDR(sl) = CONS(STATEMENT, statementInfo_nonsimd(si), NIL);
        }
        else
        {
            /* SIMD statement (will generate more than one statement) */
            int i;
            simdStatementInfo ssi = statementInfo_simd(si);


            //First, the load statement(s)
            for(i = 0; i < simdStatementInfo_nbArgs(ssi)-1; i++)
            {
                statement s = generate_load_statement(ssi, i);

                if (! statement_undefined_p(s))
                    sl = CDR(sl) = CONS(STATEMENT, s, NIL);
            }

            //Then, the exec statement
            sl = CDR(sl) = CONS(STATEMENT, generate_exec_statement(ssi), NIL);

            //Finally, the save statement (always generated. It is up to 
            //latter phases (USE-DEF elimination....) to remove it, if needed
            sl = CDR(sl) = CONS(STATEMENT, generate_save_statement(ssi), NIL);
        }
    }

    *simdCost = gSimdCost;

    sl = CDR(sl_begin);
    CDR(sl_begin) = NIL;
    gen_free_list(sl_begin);
    pips_debug(3,"generate_simd_code 2\n");
    return sl;
}

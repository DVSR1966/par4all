/*

  $Id$

  Copyright 1989-2010 MINES ParisTech

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
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"

#include "resources.h"

#include "misc.h"
#include "ri-util.h"
#include "effects-util.h"
#include "pipsdbm.h"

#include "effects-generic.h"

#include "reductions.h"
#include "sac.h"

#include "effects-convex.h"
#include "effects-simple.h"

#include "control.h"
#include "properties.h"

bool sac_expression_reduction_p(expression e){
    if(expression_reference_p(e)) {
        const char* refname = entity_user_name(reference_variable(expression_reference(e)));
        if(refname==strstr(refname,get_string_property("SIMD_REMOVE_REDUCTIONS_PREFIX")))
            return true;
    }
    return false;
}

static entity make_reduction_vector_entity(reduction r)
{
    basic base = basic_of_reference(reduction_reference(r));
    entity new_ent, mod_ent;
    static int counter = 0;
    const char *prefix = get_string_property("SIMD_REMOVE_REDUCTIONS_PREFIX") ;
    char buffer[ 1 + 3 + strlen(prefix) ];
    pips_assert("buffer does not overflow",counter < 1000);
    sprintf(buffer,"%s%u",prefix,counter++);

    mod_ent = get_current_module_entity();
    list lis = CONS(DIMENSION,make_dimension(int_to_expression(0),int_to_expression(0)), NIL);
    new_ent = make_new_array_variable_with_prefix(buffer,mod_ent,base,lis);
    AddLocalEntityToDeclarations(new_ent,mod_ent,get_current_module_statement());
#if 0
    /* The new entity is stored in the list of entities of the same type. */
    switch(basic_tag(base))
    {
        case is_basic_int:
            {
                integer_entities = CONS(ENTITY, new_ent, integer_entities);
                break;
            }
        case is_basic_float:
            {
                if(basic_float(base) == DOUBLE_PRECISION_SIZE)
                    double_entities = CONS(ENTITY, new_ent, double_entities);
                else
                    real_entities = CONS(ENTITY, new_ent, real_entities);
                break;
            }
        default:
            break;
    }
    //free_basic(base);
#endif

    return new_ent;
}

/* The first part of the function check that the reduction is allowed. If not,
   the function returns undefined.
   If the reduction is allowed, the function updates the
   reductionInfo list reds or add the reduction to the list.*/
static reductionInfo add_reduction(list* reds, reduction r)
{
    reductionInfo ri = reductionInfo_undefined;

    pips_debug(1, "reduction reference %s\n", entity_local_name(reference_variable(reduction_reference(r))));

    //See if the reduction has already been encountered
    FOREACH(REDUCTIONINFO,ri,*reds)
    {
        if (same_reduction_p(r, reductionInfo_reduction(ri)))
        {
            //The reduction has already been encountered: update the coun
            reductionInfo_count(ri)++;

            free_expression(dimension_upper(DIMENSION(CAR((variable_dimensions(type_variable(entity_type(reductionInfo_vector(ri)))))))));
            dimension_upper(DIMENSION(CAR((variable_dimensions(type_variable(entity_type(reductionInfo_vector(ri)))))))) = int_to_expression(reductionInfo_count(ri)-1);

            return ri; 
        }
    }

    //First time we see this reduction: initialize a reductionInfo structure
    ri = make_reductionInfo(r, 1, make_reduction_vector_entity(r));

    //Add to the list of reductions encountered
    *reds=CONS(REDUCTIONINFO,ri,*reds);

    return ri;
}

static void rename_reduction_ref_walker(expression e, reductionInfo ri)
{
    syntax s = expression_syntax(e);

    if (syntax_reference_p(s) &&
            reference_equal_p(syntax_reference(s), reduction_reference(reductionInfo_reduction(ri))))
    {
        free_reference(syntax_reference(s));
        syntax_reference(s)= make_reference(reductionInfo_vector(ri),
                make_expression_list(int_to_expression(reductionInfo_count(ri)-1)));
    }
}

/* finds out expression with reduction */
typedef struct {
    reduction red;
    bool has_reduction_p;
} reduction_in_statement_param;

static bool reduction_in_statement_walker(reference r, reduction_in_statement_param* p)
{
    p->has_reduction_p|=reference_equal_p(r, reduction_reference(p->red));
    return ! p->has_reduction_p;
}

static bool reduction_in_statement_p(reduction red, statement stat)
{
    reduction_in_statement_param p ={ red, false };
    gen_context_recurse(stat, &p, reference_domain, reduction_in_statement_walker, gen_null);
    return p.has_reduction_p;
}

/* This function gets the possible reduction thanks to load_cumulated_reductions() function. 
   Then, for each possible reduction, the function call add_reduction() to know if 
   the reduction is allowed and if it is, the function calls rename_reduction_ref()
   to do the reduction. */
static void rename_statement_reductions(statement s, list * reductions_info, list reductions)
{
    ifdebug(3) {
        pips_debug(3,"considering statement:\n");
        print_statement(s);
    }
    FOREACH(REDUCTION, r, reductions)
    {
        ifdebug(3) {
            pips_debug(3,"considering reduction:\n");
            print_reference(reduction_reference(r));
            fprintf(stderr,"\n");
        }

        if(reduction_in_statement_p(r,s))
        {
            pips_debug(3,"found in the statement ! rewriting ...\n");
            basic b = basic_of_reference(reduction_reference(r));
            if(!basic_undefined_p(b))
            {
                free_basic(b);
                reductionInfo ri = add_reduction(reductions_info, r);
                if( ! reductionInfo_undefined_p(ri))
                    gen_context_recurse(s,ri,expression_domain,gen_true,rename_reduction_ref_walker);
            }
        }
        else
            pips_debug(3,"not found in the statement ! skipping ...\n");
    }
}

/*
   This function make an expression that represents
   the maximum value
   */
static expression make_maxval_expression(basic b)
{
    switch(basic_tag(b))
    {
        case is_basic_float:
            return expression_undefined;

        case is_basic_int:
            {
                long long max = (2 << (basic_int(b) - 2)) - 1;
                return make_integer_constant_expression(max);
            }

        default:
            return expression_undefined;
    }
}

/*
   This function make an expression that represents
   the minimum value
   */
static expression make_minval_expression(basic b)
{
    switch(basic_tag(b))
    {
        case is_basic_float:
            return expression_undefined;

        case is_basic_int:
            {
                long long min = -(2 << (basic_int(b) - 2));
                return make_integer_constant_expression(min);
            }

        default:
            return expression_undefined;
    }
}

/*
   This function make an expression that represents
   a zero value
   */
static expression make_0val_expression(basic b)
{
    switch(basic_tag(b))
    {
        case is_basic_float:
            return float_to_expression(0);

        case is_basic_int:
            return int_to_expression(0);

        case is_basic_complex:
            return complex_to_expression(0,0);

        default:
            pips_internal_error("function not implemented for this basic \n");
    }
    return expression_undefined;
}

/*
   This function make an expression that represents
   a one value
   */
static expression make_1val_expression(basic b)
{
    switch(basic_tag(b))
    {
        case is_basic_float:
            return float_to_expression(1);

        case is_basic_int:
            return int_to_expression(1);

        case is_basic_complex:
            return complex_to_expression(0,1);

        default:
            return expression_undefined;
    }
}

/*
   This function generate the reduction prelude
   */
static statement generate_prelude(reductionInfo ri)
{
    expression initval;
    list prelude = NIL;
    int i;
    basic bas = basic_of_reference(reduction_reference(reductionInfo_reduction(ri)));

    // According to the operator, get the correct initialization value
    switch(reduction_operator_tag(reduction_op(reductionInfo_reduction(ri))))
    {
        default:
        case is_reduction_operator_none:
            return statement_undefined;
            break;

        case is_reduction_operator_min:
            initval = make_maxval_expression(bas);
            break;

        case is_reduction_operator_max:
            initval = make_minval_expression(bas);
            break;

        case is_reduction_operator_sum:
            initval = make_0val_expression(bas);
            break;

        case is_reduction_operator_prod:
            initval = make_1val_expression(bas);
            break;

        case is_reduction_operator_and:
            initval = bool_to_expression(TRUE);
            break;

        case is_reduction_operator_or:
            initval = bool_to_expression(FALSE);
            break;
    }

    // For each reductionInfo_vector reference, make an initialization
    // assign statement and add it to the prelude
    // do nothing if no init val exist
    for(i=0; i<reductionInfo_count(ri); i++)
    {
        instruction is;

        is = make_assign_instruction(
                reference_to_expression(make_reference(
                        reductionInfo_vector(ri), CONS(EXPRESSION, 
                            int_to_expression(reductionInfo_count(ri)-i-1),
                            NIL))),
                copy_expression(initval));

        prelude = CONS(STATEMENT, 
                instruction_to_statement(is),
                prelude);
    }

    free_expression(initval);

    return instruction_to_statement(make_instruction_sequence(make_sequence(prelude)));
}

/*
   This function generate the reduction postlude
   */
static statement generate_compact(reductionInfo ri)
{
    expression rightExpr;
    entity operator;
    instruction compact;
    int i;

    // According to the operator, get the correct entity
    switch(reduction_operator_tag(reduction_op(reductionInfo_reduction(ri))))
    {
        default:
        case is_reduction_operator_none:
            return statement_undefined;  //nothing to generate
            break;

        case is_reduction_operator_min:
            operator = entity_intrinsic(MIN_OPERATOR_NAME);
            break;

        case is_reduction_operator_max:
            operator = entity_intrinsic(MAX_OPERATOR_NAME);
            break;

        case is_reduction_operator_sum:
            operator = entity_intrinsic(PLUS_OPERATOR_NAME);
            break;

        case is_reduction_operator_csum:
            operator = entity_intrinsic(PLUS_C_OPERATOR_NAME);
            break;

        case is_reduction_operator_prod:
            operator = entity_intrinsic(MULTIPLY_OPERATOR_NAME);
            break;

        case is_reduction_operator_and:
            operator = entity_intrinsic(AND_OPERATOR_NAME);
            break;

        case is_reduction_operator_or:
            operator = entity_intrinsic(OR_OPERATOR_NAME);
            break;
    }

    // Get the reduction variable
    rightExpr = reference_to_expression(copy_reference(reduction_reference(reductionInfo_reduction(ri))));

    // For each reductionInfo_vector reference, add it to the compact statement
    for(i=0; i<reductionInfo_count(ri); i++)
    {
        call c;
        expression e;

        e = reference_to_expression(make_reference(
                    reductionInfo_vector(ri), CONS(EXPRESSION, int_to_expression(i), NIL)));
        c = make_call(operator, CONS(EXPRESSION, e, 
                    CONS(EXPRESSION, rightExpr, NIL)));

        rightExpr = call_to_expression(c);
    }

    // Make the compact assignment statement
    compact = make_assign_instruction(
            reference_to_expression(copy_reference(reduction_reference(reductionInfo_reduction(ri)))),
            rightExpr);

    return instruction_to_statement(compact);
}

/*
   This function attempts to find reductions for each loop
   */
static void reductions_rewrite(statement s)
{
    instruction i = statement_instruction(s);
    statement body;

    //We are only interested in loops
    switch(instruction_tag(i))
    {
        case is_instruction_loop:
            body = loop_body(instruction_loop(i));
            break;

        case is_instruction_whileloop:
            body = whileloop_body(instruction_whileloop(i));
            break;

        case is_instruction_forloop:
            body = forloop_body(instruction_forloop(i));
            break;

        case is_instruction_call:
            if(get_bool_property("SIMD_REMOVE_REDUCTIONS_ONLY_IN_LOOP")) return;
            else body = s;
            break;

        default:
            return;
    }
    {

        list reductions_info = NIL;
        list preludes = NIL;
        list compacts = NIL;

        //Compute the reductions list for the loop
        list reductions = reductions_list(load_cumulated_reductions(s));
        ifdebug(2) {
            if(!ENDP(reductions))
                pips_debug(2,"found reductions for loop:\n");
            else
                pips_debug(2,"no reduction for loop:\n");
            print_statement(s);
        }

        //Lookup the reductions in the loop's body, and change the loop body accordingly
        instruction ibody = statement_instruction(body);
        switch(instruction_tag(ibody))
        {
            case is_instruction_sequence:
                {
                    FOREACH(STATEMENT, curStat,sequence_statements(instruction_sequence(ibody)))
                        rename_statement_reductions(curStat, &reductions_info, reductions);
                } break;

            case is_instruction_call:
                rename_statement_reductions(s, &reductions_info, reductions);
                break;

            default:
                return;
        }

        //Generate prelude and compact code for each of the reductions
        FOREACH(REDUCTIONINFO, ri,reductions_info)
        {
            statement curStat;

            curStat = generate_prelude(ri);
            if (curStat != statement_undefined)
                preludes = CONS(STATEMENT, curStat, preludes);

            curStat = generate_compact(ri);
            if (curStat != statement_undefined)
                compacts = CONS(STATEMENT, curStat, compacts);
        };
        gen_full_free_list(reductions_info);

        // Replace the old statement instruction by the new one
        statement scp = copy_statement(s);
        statement_label(s)=entity_empty_label();
        sequence seq = make_sequence(
                gen_concatenate(
                    preludes,
                    CONS(STATEMENT, scp,compacts)));
        update_statement_instruction(s,make_instruction_sequence(seq));
    }
}

/** 
 * remove reductions by expanding recuced scalar to an array
 * 
 * @param mod_name  module to remove reductions from
 * 
 * @return true
 */
bool simd_remove_reductions(char * mod_name)
{

    /* get the resources */
    set_current_module_statement((statement)db_get_memory_resource(DBR_CODE, mod_name,true));
    set_current_module_entity(module_name_to_entity(mod_name));
    set_cumulated_reductions((pstatement_reductions) db_get_memory_resource(DBR_CUMULATED_REDUCTIONS, mod_name, true));

    debug_on("SIMDREDUCTION_DEBUG_LEVEL");

    /* Now do the job */
    gen_recurse(get_current_module_statement(), statement_domain, gen_true, reductions_rewrite);

    pips_assert("Statement is consistent after remove reductions", statement_consistent_p(get_current_module_statement()));

    /* Reorder the module, because new statements have been added */  
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, mod_name, get_current_module_statement());

    /* update/release resources */
    reset_cumulated_reductions();
    reset_current_module_statement();
    reset_current_module_entity();

    debug_off();

    return true;
}


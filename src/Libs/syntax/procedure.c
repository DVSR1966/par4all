/*
 * $Id$
 *
 * $Log: procedure.c,v $
 * Revision 1.53  1998/11/27 14:54:23  irigoin
 * debug statement added in EndOfProcedure()
 *
 * Revision 1.52  1998/10/07 15:57:20  irigoin
 * Proper substitution of ghost variables to avoid dangling pointers. Profile
 * of remove_ghost_variables() modified and body updated. New function
 * AbortEntries() added to avoid dangling entities in case of a call to
 * ParserError(). Update of MakeEntryCommon() whose call is postponed wrt the
 * previous implementation. Also, it is safer wrt PIPS acceptable
 * inputs. Entry processing has been made safe (or safer) wrt to calls to
 * ParserError(). Also, duplicate DATA statements between entries are avoided.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "string.h"

#include "genC.h"
#include "parser_private.h"
#include "linear.h"
#include "ri.h"
#include "database.h"
#include "resources.h"

#include "misc.h"
#include "properties.h"

#include "ri-util.h"
#include "pipsdbm.h"
#include "syntax.h"

#include "syn_yacc.h"

/* list of called subroutines or functions */
static list called_modules = list_undefined;

/* statement of current function */
static statement function_body = statement_undefined;

/*********************************************************** GHOST VARIABLES */

/* list of potential local or top-level variables that turned out to be useless.
 */
static list ghost_variable_entities = list_undefined;

void
init_ghost_variable_entities()
{
    pips_assert("undefined list", list_undefined_p(ghost_variable_entities));
    ghost_variable_entities = NIL;
}

void
substitute_ghost_variable_in_expression(
    expression expr, 
    entity v,
    entity f)
{
    /* It is assumed that v and f are defined entities and that v is of
       type variable and f of type functional. */
    syntax s = expression_syntax(expr);
    reference ref = reference_undefined;
    range rng = range_undefined;
    call c = call_undefined;

    ifdebug(8) {
	debug(8, "", "Begin for expression: ");
	print_expression(expr);
    }

    switch(syntax_tag(s)) {
    case is_syntax_reference:
	ref = syntax_reference(s);
	if(reference_variable(ref)==v) {
	    debug(1, "substitute_ghost_variable_in_expression",
		  "Reference to formal functional entity %s to be substituted\n",
		  entity_name(f));
	    /* ParserError() is going to request ghost variable
               substitution recursively and we do not want this to happen
               because it is going to fail again. Well, substitution won't be
	       tried from AbortOfProcedure()... */
	    /* ghost_variable_entities = NIL; */
	    user_warning("substitute_ghost_variable_in_expression",
			 "Functional variable %s is used as a functional argument\n",
			 module_local_name(f));
	    ParserError("substitute_ghost_variable_in_expression",
			"Functional parameters are not (yet) supported by PIPS\n");
	}
	MAP(EXPRESSION, e, {
	    substitute_ghost_variable_in_expression(e, v, f);
	}, reference_indices(ref));
	break;
    case is_syntax_range:
	rng = syntax_range(s);
	substitute_ghost_variable_in_expression(range_lower(rng), v, f);
	substitute_ghost_variable_in_expression(range_upper(rng), v, f);
	substitute_ghost_variable_in_expression(range_increment(rng), v, f);
	break;
    case is_syntax_call:
	c = syntax_call(s);
	pips_assert("Called entities are not substituted", call_function(c)!= v);
	MAP(EXPRESSION, e, {
	    substitute_ghost_variable_in_expression(e, v, f);
	}, call_arguments(c));
	break;
    default:
    }

    ifdebug(8) {
	debug(8, "", "End for expression: ");
	print_expression(expr);
    }
}

void
substitute_ghost_variable_in_statement(
    statement stmt, 
    entity v,
    entity f)
{
    /* It is assumed that v and f are defined entities and that v is of
       type variable and f of type functional. */

    /* gen_recurse() is not used to control the context better */

    entity sl = statement_label(stmt);
    instruction i = statement_instruction(stmt);
    loop l = loop_undefined;
    whileloop w = whileloop_undefined;
    test t = test_undefined;
    call c = call_undefined;
    /* unstructured u = unstructured_undefined; */

    pips_assert("Labels are not substituted", sl!= v);

    switch(instruction_tag(i)) {
    case is_instruction_sequence:
	MAP(STATEMENT, s, {
	    substitute_ghost_variable_in_statement(s, v, f);
	}, instruction_block(i));
	break;
    case is_instruction_loop:
	l = instruction_loop(i);
	pips_assert("Loop indices are not substituted", loop_index(l)!= v);
	pips_assert("Loop labels are not substituted", loop_label(l)!= v);
	substitute_ghost_variable_in_expression(range_lower(loop_range(l)), v, f);
	substitute_ghost_variable_in_expression(range_upper(loop_range(l)), v, f);
	substitute_ghost_variable_in_expression(range_increment(loop_range(l)), v, f);
	substitute_ghost_variable_in_statement(loop_body(l), v, f);
	/* Local variables should also be checked */
	break;
    case is_instruction_whileloop:
	w = instruction_whileloop(i);
	pips_assert("WHILE loop labels are not substituted", whileloop_label(w)!= v);
	substitute_ghost_variable_in_expression(whileloop_condition(w), v, f);
	substitute_ghost_variable_in_statement(whileloop_body(w), v, f);
	/* Local variables should also be checked */
	break;
    case is_instruction_test:
	t = instruction_test(i);
	substitute_ghost_variable_in_expression(test_condition(t), v, f);
	substitute_ghost_variable_in_statement(test_true(t), v, f);
	substitute_ghost_variable_in_statement(test_false(t), v, f);
	break;
    case is_instruction_goto:
	/* nothing to do */
	break;
    case is_instruction_call:
	c = instruction_call(i);
	pips_assert("Called entities are not substituted", call_function(c)!= v);
	MAP(EXPRESSION, e, {
	    substitute_ghost_variable_in_expression(e, v, f);
	}, call_arguments(c));
	break;
    case is_instruction_unstructured:
	pips_assert("The parser should not have to know about unstructured\n", FALSE);
	break;
    default:
	FatalError("substitute_ghost_variable_in_statement", "Unexpected instruction tag");
    }
}

void 
remove_ghost_variable_entities(bool substitute_p)
{
    pips_assert("defined list", !list_undefined_p(ghost_variable_entities));
    MAP(ENTITY, e, 
    {
	/* The debugging message must use the variable name before it is freed
	 */
	pips_debug(1, "entity '%s'\n", entity_name(e));
	pips_assert("Entity e is defined and has type \"variable\" if substitution is required\n",
		    !substitute_p 
		    || (!entity_undefined_p(e)
			&& (type_undefined_p(entity_type(e)) || type_variable_p(entity_type(e)))));
	if(entity_in_equivalence_chains_p(e)) {
	    user_warning("remove_ghost_variable_entities",
		     "Entity \"%s\" does not really exist but appears"
		     " in an equivalence chain!\n",
		     entity_name(e));
	    if(!ParserError("remove_ghost_variable_entities",
			    "Cannot remove still accessible ghost variable\n")) {
		/* We already are in ParserError()! Too bad for the memory leak */
		ghost_variable_entities = list_undefined;
		return;
	    }
	}
	else {
	    entity fe = local_name_to_top_level_entity(entity_local_name(e));
	    type t = type_undefined;

	    if(entity_undefined_p(fe)) {
		pips_assert("Entity fe cannot be undefined", FALSE);
	    }
	    else if(type_undefined_p(entity_type(fe))) {
		t = entity_type(fe);
		pips_assert("Type for entity fe cannot be undefined", FALSE);
	    }
	    else if(type_functional_p(entity_type(fe))) {
		statement stmt = function_body;

		t = entity_type(fe);

		/*
		if(intrinsic_entity_p(fe)) {
		    user_warning("remove_ghost_variable_entities",
				 "Intrinsic %s is probably declared in a strange useless way\n",
				 module_local_name(fe));
		}
		*/


		if(substitute_p) {
		    debug(1, "remove_ghost_variable_entities",
			  "Start substitution of variable %s by module %s\n",
			  entity_name(e), entity_name(fe));
		    substitute_ghost_variable_in_statement(stmt, e, fe);
		    debug(1, "remove_ghost_variable_entities",
			  "End for substitution of variable %s by module %s\n",
			  entity_name(e), entity_name(fe));
		}
	    }
	    else {
		t = entity_type(fe);
		pips_assert("Type t for entity fe should be functional", FALSE);
	    }

	    remove_variable_entity(e);
	}
	pips_debug(1, "destroyed\n");
    }, 
	ghost_variable_entities);

    ghost_variable_entities = list_undefined;
}

void
add_ghost_variable_entity(entity e)
{
    pips_assert("defined list",	!list_undefined_p(ghost_variable_entities));
    ghost_variable_entities = arguments_add_entity(ghost_variable_entities, e);
}

/* It is possible to change one's mind and effectively use an entity which was
 * previously assumed useless
 */
void
reify_ghost_variable_entity(entity e)
{
    pips_assert("defined list",	!list_undefined_p(ghost_variable_entities));
    if(entity_is_argument_p(e, ghost_variable_entities))
	ghost_variable_entities = arguments_rm_entity(ghost_variable_entities, e);
}

bool
ghost_variable_entity_p(entity e)
{
    pips_assert("defined list",	!list_undefined_p(ghost_variable_entities));

    return entity_is_argument_p(e, ghost_variable_entities);
}


/* this function is called each time a new procedure is encountered. */
void 
BeginingOfProcedure()
{
    reset_current_module_entity();
    InitImplicit();
    called_modules = NIL;
}

void 
update_called_modules(e)
entity e;
{
    bool already_here = FALSE;
    string n = entity_local_name(e);
    string nom;
    entity cm = get_current_module_entity();

    /* Self recursive calls are not allowed */
    if(e==cm) {
	pips_user_warning("Recursive call from %s to %s\n",
		     entity_local_name(cm), entity_local_name(e));
	ParserError("update_called_modules", 
		    "Recursive call are not supported\n");
    }

    /* do not count intrinsics; user function should not be named
       like intrinsics */
    nom = concatenate(TOP_LEVEL_MODULE_NAME, MODULE_SEP_STRING, n, NULL);
    if ((e = gen_find_tabulated(nom, entity_domain)) != entity_undefined) {
	if(entity_initial(e) == value_undefined) {
	    /* FI, 20/01/92: maybe, initializations of global entities
	       should be more precise (storage, initial value, etc...);
	       for the time being, I choose to ignore the potential
	       problems with other executions of the parser and the linker */
	    /* pips_error("update_called_modules","unexpected case\n"); */
	}
	else if(value_intrinsic_p(entity_initial(e)))
	    return;
    }

    MAPL(ps, {
	if (strcmp(n, STRING(CAR(ps))) == 0) {
	    already_here = TRUE;
	    break;
	}
    }, called_modules);

    if (! already_here) {
	pips_debug(1, "adding %s\n", n);
	called_modules = CONS(STRING, strdup(n), called_modules);
    }
}

/* macros are added, although they should not have been.
 */
void
remove_from_called_modules(entity e)
{
    bool found = FALSE;
    list l = called_modules;
    string name = module_local_name(e);

    if (!called_modules) return;

    if (same_string_p(name, STRING(CAR(called_modules)))) {
	called_modules = CDR(called_modules);
	found = TRUE;
    } else {
	list lp = called_modules;
	l = CDR(called_modules);
	
	for(; !ENDP(l); POP(l), POP(lp)) {
	    if (same_string_p(name, STRING(CAR(l)))) {
		CDR(lp) = CDR(l);
		found = TRUE;
		break;
	    }
	}
    }    

    if (found) {
	pips_debug(3, "removing %s from callees\n", entity_name(e));
	CDR(l) = NIL;
	free(STRING(CAR(l)));
	gen_free_list(l);
    }
}

void 
AbortOfProcedure()
{
    /* get rid of ghost variable entities */
    if (!list_undefined_p(ghost_variable_entities))
	remove_ghost_variable_entities(FALSE);

    (void) ResetBlockStack() ;
}

/* This function is called when the parsing of a procedure is completed.
 * It performs a few calculations which cannot be done on the fly such
 * as address computations.
 *
 * And it writes the internal representation of the CurrentFunction with a
 * call to gen_free (?). */

void 
EndOfProcedure()
{
    entity CurrentFunction = get_current_module_entity();

    debug(8, "EndOfProcedure", "Begin for module %s\n",
	  entity_name(CurrentFunction));
    
    pips_debug(8, "checking code consistency = %d\n",
	       statement_consistent_p( function_body )) ;

    /* get rid of ghost variable entities and substitute them if necessary */
    remove_ghost_variable_entities(TRUE);

    /* we generate the last statement to carry a label or a comment */
    if (strlen(lab_I) != 0 /* || iPrevComm != 0 */ ) {
	LinkInstToCurrentBlock(make_continue_instruction(), FALSE);
    }

    /* we generate statement last+1 to eliminate returns */
    GenerateReturn();

    uses_alternate_return(FALSE);
    ResetReturnCodeVariable();
    SubstituteAlternateReturns("NO");

    /* Check the block stack */
    (void) PopBlock() ;
    if (!IsBlockStackEmpty())
	    ParserError("EndOfProcedure", "bad program structure\n");

    /* are there undefined gotos ? */
    CheckAndInitializeStmt();

    /* The following calls could be located in check_first_statement()
     * which is called when the first executable statement is
     * encountered. At that point, many declaration related
     * problems should be fixed or fixeable. But additional
     * undeclared variables will be added to the dynamic area
     * and their addresses must be computed. At least, ComputeAddresses()
     * must stay here.. so I keep all these calls together.
     */
    UpdateFunctionalType(CurrentFunction,
			 FormalParameters);

    /* Must be performed before equivalence resolution, for user declared
       commons whose declarations are stronger than equivalences */
    update_user_common_layouts(CurrentFunction);

    ComputeEquivalences();
    /* Use equivalence chains to update storages of equivalenced and of
       variables implicitly declared in DynamicArea, or implicitly thru
       DATA or explicitly thru SAVE declared in StaticArea */
    ComputeAddresses();

    /* Initialize the shared field in ram storage */
    SaveChains();

    /* Now that retyping and equivalences have been taken into account: */
    update_common_sizes();

    code_declarations(EntityCode(CurrentFunction)) =
	    gen_nreverse(code_declarations(EntityCode(CurrentFunction))) ;

    if (get_bool_property("PARSER_DUMP_SYMBOL_TABLE"))
	fprint_environment(stderr, CurrentFunction);

    ifdebug(5){
	fprintf(stderr, "Parser: checking callees consistency = %d\n",
		callees_consistent_p( make_callees( called_modules ))) ;
    }

    /*  remove hpfc special routines if required.
     */
    if (get_bool_property("HPFC_FILTER_CALLEES"))
    {
	list l = NIL;
	string s;

	MAPL(cs,
	 {
	     s = STRING(CAR(cs));

	     if (hpf_directive_string_p(s) && !keep_directive_in_code_p(s))
	     {
		 pips_debug(3, "ignoring %s\n", s);
	     }
	     else
		 l = CONS(STRING, s, l);
	 },
	     called_modules);

	gen_free_list(called_modules);
	called_modules = l;
    }

    /* done here. affects callees and code. FC.
     */
    parser_substitute_all_macros(function_body);
    parser_close_macros_support();

    if(!EmptyEntryListsP()) {
	set_current_module_statement(function_body);
	ProcessEntries();
	reset_current_module_statement();
    }

    reset_common_size_map();

    DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, 
			   module_local_name(CurrentFunction), 
			   (char*) make_callees(called_modules));
    
    pips_debug(5, "checking code consistency = %d\n",
		statement_consistent_p( function_body )) ;

    DB_PUT_MEMORY_RESOURCE(DBR_PARSED_CODE, 
			   module_local_name(CurrentFunction), 
			   (char *)function_body);

    /* the current package is re-initialized */
    CurrentPackage = TOP_LEVEL_MODULE_NAME;
    ResetChains();
    DynamicArea = entity_undefined;
    StaticArea = entity_undefined;
    reset_current_module_entity();

    pips_debug(5, "checking code consistency after resettings = %d\n",
		statement_consistent_p( function_body )) ;

    pips_debug(8, "End for module %s\n", entity_name(CurrentFunction));
}



/* This function analyzes the CurrentFunction formal parameter list to
 * determine the CurrentFunction functional type. l is this list.
 *
 * It is called by EndOfProcedure().
 */

void 
UpdateFunctionalType(
		     entity f,
		     list l)
{
    cons *pc;
    parameter p;
    functional ft;
    entity CurrentFunction = f;
    type t = entity_type(CurrentFunction);

    ifdebug(8) {
	debug(8, "UpdateFunctionalType", "Begin for %s with type ",
	      module_local_name(CurrentFunction));
	fprint_functional(stderr, type_functional(t));
	(void) fprintf(stderr, "\n");
    }

    pips_assert("A module type should be functional", type_functional_p(t));

    ft = type_functional(t);

    /* FI: I do not understand this assert... at least now that
     * functions may be typed at call sites. I do not understand why this
     * assert has not made more damage. Only OVL in APSI (Spec-cfp95)
     * generates a core dump. To be studied more!
     *
     * This assert is guaranteed by MakeCurrentFunction() but not by 
     * retype_formal_parameters() which is called in case an intrinsic
     * statement is encountered. It is not guaranteed by MakeExternalFunction()
     * which uses the actual parameter list to estimate a functional type
     */
    pips_assert("Parameter type list should be empty",
		ENDP(functional_parameters(ft)));

    for (pc = l; pc != NULL; pc = CDR(pc)) {
	entity fp = ENTITY(CAR(pc));
	type fpt = entity_type(fp);

	if(type_undefined_p(fpt)) {
	    entity_type(fp) = ImplicitType(fp);
	}

	p = make_parameter((entity_type(fp)), 
			   (MakeModeReference()));
	functional_parameters(ft) = 
		gen_nconc(functional_parameters(ft),
			  CONS(PARAMETER, p, NIL));
    }

    ifdebug(8) {
	debug(8, "UpdateFunctionalType", "End for %s with type ",
	      module_local_name(CurrentFunction));
	fprint_functional(stderr, type_functional(t));
	(void) fprintf(stderr, "\n");
    }
}

void
remove_module_entity(entity m)
{
    /* It is assumed that neither variables nor areas have been declared in m
     * but that m may have been declared by EXTERNAL in other modules.
     */
    gen_array_t modules = db_get_module_list();
    int module_list_length = gen_array_nitems(modules);
    int i = 0;

    for(i = 0; i < module_list_length; i++) {
	entity om = local_name_to_top_level_entity(gen_array_item(modules, i));

	if(!entity_undefined_p(om)) {
	    value v = entity_initial(om);

	    if(!value_undefined_p(v) && !value_unknown_p(v)) {
		code c = value_code(v);

		if(!code_undefined_p(c)) {
		    ifdebug(1) {
			if(gen_in_list_p(m, code_declarations(c))) {
			    debug(1, "remove_module_entity",
				  "Declaration of module %s removed from %s's declarations",
				  entity_name(m), entity_name(om));
			}
		    }
		    gen_remove(&code_declarations(c), m);
		}
	    }
	}
    }
    gen_array_full_free(modules);
    free_entity(m);
}

/* this function creates one entity cf that represents the function f being
analyzed. if f is a Fortran FUNCTION, a second entity is created; this
entity represents the variable that is used in the function body to
return a value.  both entities share the same name and the type of the
result entity is equal to the type of cf's result.

t is the type of the function result if it has been given by the
programmer as in INTEGER FUNCTION F(A,B,C)

msf indicates if f is a main, a subroutine or a function.

cf is the current function

lfp is the list of formal parameters
*/
void 
MakeCurrentFunction(
    type t,
    int msf,
    string cfn,
    list lfp)
{
    entity cf = entity_undefined; /* current function */
    instruction icf; /* the body of the current function */
    entity result; /* the second entity, used to store the function result */
    /* to split the entity name space between mains, commons, blockdatas and regular modules */
    string prefix = string_undefined;
    string fcfn = string_undefined; /* full current function name */
    entity ce = entity_undefined; /* global entity with conflicting name */

    /* Check that there is no such common: This test is obsolete because
     * the standard does not prohibit the use of the same name for a
     * common and a function. However, it is not a good programming practice
     */
    if (gen_find_tabulated(concatenate
	   (TOP_LEVEL_MODULE_NAME, MODULE_SEP_STRING,
	    COMMON_PREFIX, cfn, 0), 
			   entity_domain) != entity_undefined)
    {
	pips_user_warning("global name %s used for a module and for a common\n",
			  cfn);
	/*
	ParserError("MakeCurrentFunction",
		    "Name conflict between a "
		    "subroutine and/or a function and/or a common\n");
		    */
    }

    if(msf==TK_PROGRAM) {
	prefix = MAIN_PREFIX;
    }
    else if(msf==TK_BLOCKDATA) {
	prefix = BLOCKDATA_PREFIX;
    }
    else  {
	prefix = "";
    }
    fcfn = strdup(concatenate(prefix, cfn, NULL));
    cf = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, fcfn);
    free(fcfn);
    ce = global_name_to_entity(TOP_LEVEL_MODULE_NAME, cfn);
    if(!entity_undefined_p(ce) && ce!=cf) {
      if(!value_undefined_p(entity_initial(cf)) || msf!=TK_BLOCKDATA) {
	user_warning("MakeCurrentFunction", "Global name %s used for a function or subroutine"
		     " and for a %s\n", cfn, msf==TK_BLOCKDATA? "blockdata" : "main");
	ParserError("MakeCurrentFunction", "Name conflict\n");
      }
      else {
	/* A block data may be declared in an EXTERNAL statement, see Standard 8-9 */
	debug(1, "MakeCurrentFunction", "Entity \"%s\" does not really exist."
	      " A blockdata is declared in an EXTERNAL statement.");
	/* remove_variable_entity(ce); */
	remove_module_entity(ce);
      }
    }

    /* Let's hope cf is not an intrinsic */
    if( entity_type(cf) != type_undefined
       && intrinsic_entity_p(cf) ) {
	user_warning("MakeCurrentFunction",
		     "Intrinsic %s redefined.\n"
		     "This is not supported by PIPS. Please rename %s\n",
		     entity_local_name(cf), entity_local_name(cf));
	/* Unfortunately, an intrinsics cannot be redefined, just like a user function
	 * or subroutine after editing because intrinsics are not handled like
	 * user functions or subroutines. They are not added to the called_modules
	 * list of other modules, unless the redefining module is parsed FIRST.
	 * There is not mechanism in PIPS to control the parsing order.
	 */
	ParserError("MakeCurrentFunction",
		    "Name conflict between a "
		    "subroutine and/or a function and an intrinsic\n");
    }

    /* set ghost variable entities to NIL */
    /* This procedure is called when the whole module declaration
       statement has been parsed. The formal parameters have already been
       declared and the ghost variables checked. The call was moved in
       gram.y, reduction rule for psf_keyword. */
    /* init_ghost_variable_entities(); */

    /* initialize equivalence chain lists to NIL */
    SetChains();

    if (msf == TK_FUNCTION) {
	if (t == type_undefined) {
	    t = ImplicitType(cf);
	}
    }
    else {
	if (t == type_undefined) {
	    t = make_type(is_type_void, UU);
	}
	else {
	    /* the intended result type t for a main or a subroutine should be undefined */
	    FatalError("MakeCurrentFunction", "bad type\n");
	}
    }

    /* clean up existing local entities in case of a recompilation */
    CleanLocalEntities(cf);

    /* the parameters part of cf's functional type is not created
       because the type of formal parameters is not known. this is done by
       UpdateFunctionalType. */
    entity_type(cf) = make_type(is_type_functional, make_functional(NIL, t));

    /* a function has a rom storage */
    entity_storage(cf) = MakeStorageRom();

    /* a function has an initial value 'code' that contains an empty block */
    icf = MakeEmptyInstructionBlock();

    /* FI: This NULL string is a catastrophy for the strcmp used later
     * to check the content of the stack. Any string, including
     * the empty string "", would be better. icf is used to link new
     * instructions/statement to the current block. Only the first
     * block is not pushed for syntactic reasons. The later blocks
     * will be pushed for DO's and IF's.
     */
    /* PushBlock(icf, (string) NULL); */
    PushBlock(icf, "INITIAL");

    function_body = instruction_to_statement(icf);
    entity_initial(cf) = make_value(is_value_code, make_code(NIL, NULL));

    set_current_module_entity(cf);

    /* No common has yet been declared */
    initialize_common_size_map();

    /* two global areas are created */
    InitAreas();

    /* Formal parameters are created. Alternate returns can be ignored
     * or substituted.
     */
    SubstituteAlternateReturns
	(get_string_property("PARSER_SUBSTITUTE_ALTERNATE_RETURNS"));
    ScanFormalParameters(cf, add_formal_return_code(lfp));

    if (msf == TK_FUNCTION) {
	/* a result entity is created */
	/*result = FindOrCreateEntity(CurrentPackage, entity_local_name(cf));*/
	result = make_entity(strdup(concatenate(CurrentPackage, 
						MODULE_SEP_STRING, 
						module_local_name(cf), 
						NULL)), 
			     type_undefined, 
			     storage_undefined, 
			     value_undefined);
	DeclareVariable(result, t, NIL, make_storage(is_storage_return, cf),
			value_undefined);
	AddEntityToDeclarations(result, cf);
    }
}

/* Processing of entries: when an ENTRY statement is encountered, it is
 * replaced by a labelled CONTINUE and the entry is declared as function
 * or a subroutine, depending on its type. The label and the module entity
 * which are created are stored in two static lists, entry_labels and
 * entry_entities, for later processing. When the current module has been
 * fully parsed, the two entry lists are scanned together. The current
 * module code is duplicated for each entry, a GOTO the proper entry label
 * is added, the code is controlized to get rid of unwanted code, and:
 *
 * - either all references are translated into the entry
 *   reference. The entry declarations are then initialized.
 *
 * - or the controlized code is prettyprinted as SOURCE_FILE and parser
 *   again to avoid the translation issue.
 *
 * The second approach was selected. The current .f file is overwritten
 * when the parser is called for the code of an entry.
 *
 * Further problems are created by entries in fsplit which creates a
 * .f_initial file for each entry and in the parser which may not produce
 * the expected PARSED_CODE when it is called for an ENTRY. A recursive
 * call to the parser is executed to parse the .f file just produced by
 * the first call. This scheme was designed to make entries unvisible from
 * pipsmake.
 *
 */

static list entry_labels = NIL;
static list entry_targets = NIL;
static list entry_entities = NIL;
static list effective_formal_parameters = NIL;

void
ResetEntries()
{
    gen_free_list(entry_labels);
    entry_labels = NIL;

    gen_free_list(entry_targets);
    entry_targets = NIL;

    gen_free_list(entry_entities);
    entry_entities = NIL;

    gen_free_list(effective_formal_parameters);
    effective_formal_parameters = NIL;
}

void
AbortEntries()
{
    /* Useless entities should be reset */

    MAP(ENTITY, el, {
	free_entity(el);
    }, entry_labels);
    gen_free_list(entry_labels);
    entry_labels = NIL;

    MAP(ENTITY, et, {
	free_entity(et);
    }, entry_targets);
    gen_free_list(entry_targets);
    entry_targets = NIL;

    MAP(ENTITY, ee, {
	CleanLocalEntities(ee);
	free_entity(ee);
    }, entry_entities);
    gen_free_list(entry_entities);
    entry_entities = NIL;

    MAP(ENTITY, efp, {
	free_entity(efp);
    }, entry_targets);
    gen_free_list(effective_formal_parameters);
    effective_formal_parameters = NIL;

    /* the current module statement is used when processing entries */
    reset_current_module_statement();
}

bool
EmptyEntryListsP()
{
    bool empty = ((entry_labels==NIL) && (entry_entities==NIL));

    return empty;
}

void
AddEntryLabel(entity l)
{
    entry_labels = arguments_add_entity(entry_labels, l);
}

void
AddEntryTarget(statement s)
{
    entry_targets = gen_nconc(entry_targets, CONS(STATEMENT, s, NIL));
}

void
AddEntryEntity(entity e)
{
    entry_entities = arguments_add_entity(entry_entities, e);
}

/* Keep track of the formal parameters for the current module */
void
AddEffectiveFormalParameter(entity f)
{
    effective_formal_parameters = arguments_add_entity(effective_formal_parameters, f);
}

bool
IsEffectiveFormalParameterP(entity f)
{
    return entity_is_argument_p(f, effective_formal_parameters);
}

static list
TranslateEntryFormals(
    entity e, /* entry e */
    list lfp) /* list of formal parameters wrongly declared in current module */
{
    list lefp = NIL; /* list of effective formal parameters lefp for entry e */

    ifdebug(1) {
	debug(1, "TranslateEntryFormals", "Begin with lfp = ");
	dump_arguments(lfp);
    }

    MAP(ENTITY, fp, {
	entity efp = FindOrCreateEntity(module_local_name(e), entity_local_name(fp));
	entity_type(efp) = copy_type(entity_type(fp));
	/* the storage is not recoverable */
	entity_initial(efp) = copy_value(entity_initial(fp));
	lefp = gen_nconc(lefp, CONS(ENTITY, efp, NIL));
    }, lfp);

    ifdebug(1) {
	debug(1, "TranslateEntryFormals", "\nEnd with lefp = ");
	dump_arguments(lefp);
    }

    return lefp;
}

/* Static variables in a module with entries must be redeclared as stored
 * in a common in order to be accessible from all modules derived from the
 * entries. This may create a problem for variables initialized with a DATA
 * for compilers that do not accept multiple initializations of a common
 * variable.
 */

static void
MakeEntryCommon(
    entity m,
    entity a)
{
    string c_name = strdup(concatenate(COMMON_PREFIX, "_ENTRY_", 
				       module_local_name(m), NULL));
    entity c = local_name_to_top_level_entity(c_name);
    area aa = type_area(entity_type(a));
    area ac = area_undefined;
    list members = list_undefined;

    pips_debug(1, "Begin for static area %s in module %s\n",
	       entity_name(a), entity_name(m));

    if(ENDP(area_layout(aa))) {
	pips_debug(1, "End: no static variables in module %s\n",
		   entity_name(m));
	return;
    }

    members = common_members_of_module(a, m, FALSE);
    if(ENDP(members)) {
	pips_error("MakeEntryCommon", "No local static variables in module %s: impossible!\n",
		   entity_name(m));
    }
    gen_free_list(members);

    ifdebug(1) {
	pips_debug(1, "Static area %s without aliasing in module %s\n",
	       entity_name(a), entity_name(m));
	print_common_layout(stderr, a, TRUE);
	pips_debug(1, "Static area %s with aliasing in module %s\n",
	       entity_name(a), entity_name(m));
	print_common_layout(stderr, a, FALSE);
    }

    /* Make sure that no static variables are aliased because this special
       cases has not been implemented */
    MAP(ENTITY, v, {
	storage vs = entity_storage(v);

	pips_assert("storage is ram", storage_ram_p(vs));
	pips_assert("storage is static", ram_section(storage_ram(vs)) == a);
	if(!ENDP(ram_shared(storage_ram(vs)) )) {
	    pips_user_warning("Static variable %s is aliased with ",
			      entity_local_name(v));
	    print_arguments(ram_shared(storage_ram(vs)));
	    ParserError("MakeEntryCommon",
			"Entries with aliased static variables not yet supported by PIPS\n");
	}
    }, area_layout(aa));

    if(entity_undefined_p(c)) {
	c = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, c_name);
	c = MakeCommon(c);
	ac = type_area(entity_type(c));
    }
    else {
	pips_internal_error("The scheme to generate a new common name %s"
			    " for entries in module %s failed",
			    c_name, module_local_name(m));
    }
    free(c_name);

    /* Process all variables in a's layout and declare them stored in c */
    MAP(ENTITY, v, {
	storage vs = entity_storage(v);

	if(value_constant(entity_initial(v))) {
	    debug(1, "MakeEntryCommon",
		  "Initialized variable %s\n", entity_local_name(v));
	    /* A variable in a common cannot be initialized more than once */
	    /*
	    free_value(entity_initial(v));
	    entity_initial(v) = make_value(is_value_unknown, UU);
	    */
	}

	ram_section(storage_ram(vs)) = c;
    }, area_layout(aa));

    /* Copy a's area in c's area */
    area_layout(ac) = area_layout(aa);
    /* Do not sort by name or the offset increasing implicit rule is
       broken: sort_list_of_entities(area_layout(ac)); */
    area_size(ac) = area_size(aa);

    /* Reset a's area */
    area_layout(aa) = NIL;
    area_size(aa) = 0;

    ifdebug(1) {
	pips_debug(1, "New common %s for static area %s in module %s\n",
	       entity_name(c), entity_name(a), entity_name(m));
	print_common_layout(stderr, c, TRUE);
    }

    pips_debug(1, "End for static area %s in module %s\n",
	       entity_name(a), entity_name(m));
}

/* An ENTRY statement is substituted by a labelled continue. The ENTRY
 * entity is created as in MakeExternalFunction() and MakeCurrentFunction().
 */

instruction
MakeEntry(
    entity e, /* entry, local to retrieve potential explicit typing */
    list lfp) /* list of formal parameters */
{
    entity cm = get_current_module_entity(); /* current module cm */
    string cmn = get_current_module_name(); /* current module name cmn */
    entity l = make_new_label(cmn);
    statement s = make_continue_statement(l);
    /* The parser expects an instruction and not a statement. I use
     * a block wrapping to avoid tampering with lab_I.
     */
    instruction i = make_instruction_block(CONS(STATEMENT, s, NIL));
    /* entity e = FindOrCreateEntity(cmn, en); */
    entity fe = entity_undefined;
    bool is_a_function = entity_function_p(get_current_module_entity());
    type rt = type_undefined; /* result type */
    list cc = list_undefined; /* current chunk (temporary) */
    list lefp = list_undefined; /* list of effective formal parameters */

    debug(1, "MakeEntry", "Begin for entry %s\n", entity_name(e));

    /* Name conflicts could be checked here as in MakeCurrentFunction() */

    /* Keep track of the effective formal parameters of the current module cm
     * at the first call to MakeEntry and reallocate static variables.
     */
    if(EmptyEntryListsP()) {
	MAP(ENTITY, fp, {
	    if(!storage_undefined_p(entity_storage(fp))
	       && storage_formal_p(entity_storage(fp)))
		AddEffectiveFormalParameter(fp);
	}, entity_declarations(cm));

	/* Check if the static area is empty and define a specific common
	 * if not.
	 */
	/* Too early: StaticArea is not defined yet. Postpone to ProcessEntry.
	if(area_size(type_area(entity_type(StaticArea)))!=0) {
	    MakeEntryCommon(cm, StaticArea);
	}
	*/
    }

    /* Compute the result type and make sure a functional entity is being
     * used.
     */
    if(is_a_function) {
	rt = MakeResultType(e, type_undefined);
	/* In case of previous declaration in the current module */
	/* Entity e must not be destroyed if fe is a function because e
	 * must carry the result.
	 */
	fe = SafeLocalToGlobal(e, rt);
    }
    else {
	rt = make_type(is_type_void, UU);
	/* In case of previous declaration in the current module */
	fe = LocalToGlobal(e);
    }

    lefp = TranslateEntryFormals(fe, lfp);
    UpdateFormalStorages(fe, lefp);
    TypeFunctionalEntity(fe, rt);
    UpdateFunctionalType(fe, lefp);

    /* This depends on what has been done in LocalToGlobal and SafeLocalToGlobal */
    if(storage_undefined_p(entity_storage(fe))) {
	entity_storage(fe) = MakeStorageRom();
    }
    else {
	pips_assert("storage must be rom", storage_rom_p(entity_storage(fe)));
    }

    /* This depends on what has been done in LocalToGlobal and SafeLocalToGlobal */
    if(value_undefined_p(entity_initial(fe))) {
	entity_initial(fe) = make_value(is_value_code, make_code(lefp, strdup("")));
    }
    else {
	value val = entity_initial(fe);
	code c = code_undefined;

	pips_assert("value is code", value_code_p(val));
	c = value_code(entity_initial(fe));
	if(code_undefined_p(c)) {
	    value_code(entity_initial(fe)) = make_code(lefp, strdup(""));
	}
	else if(ENDP(code_declarations(c))) {
	    /* Should now be the normal case... */
	    code_declarations(c) = lefp;
	}
	else {
	    pips_error("MakeEntry", "Code should not (yet) be defined for entry fe...");
	}
    }

    /* The entry formal parameters should be removed if they are not
     * formal parameters of the current module... but they are referenced.
     * They cannot be preserved although useless because they may be
     * dimensionned by expressions legal for this entry but not for the
     * current module. They should be removed later when dead code elimination
     * let us know which variables are used by each entry.
     *
     * Temporarily, the formal parameters of entry fe are declared in cm
     * to keep the code consistent but they are supposedly not added to
     * cm's declarations... because FindOrCreateEntity() does not update
     * declarations.
     */
    for(cc = lfp; !ENDP(cc); POP(cc)) {
	entity fp = ENTITY(CAR(cc));
	storage fps = entity_storage(fp);

	if(storage_undefined_p(fps) || !storage_formal_p(fps)) {
	    /* Let's assume it works for undefined storages.. */
	    free_storage(fps);
	    entity_storage(fp) = make_storage(is_storage_formal,
					      make_formal(cm, 0));
	    /* Should it really be officially declared? */
	    if(!IsEffectiveFormalParameterP(fp)) {
		/* Remove it from the declaration list */
		/*
		entity_declarations(cm) = 
		    arguments_rm_entity(entity_declarations(cm), fp);
		    */
		if(entity_is_argument_p(fp,entity_declarations(cm))) {
		    debug(1, "MakeEntry", "Entity %s removed from declarations for %s\n",
			  entity_name(fp), module_local_name(cm));
		}
		gen_remove(&entity_declarations(cm), fp);
	    }
	}
    }

    /* Request some post-processing */
    AddEntryLabel(l);
    AddEntryTarget(s);
    AddEntryEntity(fe);

    ifdebug(2) {
      (void) fprintf(stderr, "Declarations of formal parameters for entry %s:\n",
		     entity_name(fe));
      dump_arguments(entity_declarations(fe));
    }

    debug(1, "MakeEntry", "End for entry %s\n", entity_name(fe));

    return i;
}

/* Build an entry version of the current module statement. */ 

static statement
BuildStatementForEntry(
    entity cm, 
    entity e,
    statement t)
{
    statement s = statement_undefined;
    statement jump = instruction_to_statement(make_instruction(is_instruction_goto, t));
    /* The copy_statement() is not consistent with the use of statement t.
     * You have to free  s in a very careful way
     */
    statement cms = get_current_module_statement(); /* current module statement */
    statement es = statement_undefined; /* statement for entry e */
    list l = NIL; /* temporary statement list */

    debug(1, "BuildStatementForEntry", "Begin for entry %s in module %s\n",
	  entity_name(e), entity_name(cm));

    pips_assert("jump consistent", statement_consistent_p(jump));
    pips_assert("cms consistent", statement_consistent_p(cms));

    s = make_block_statement(
			     CONS(STATEMENT, jump,
				  CONS(STATEMENT, cms, 
				       NIL)));
    es = copy_statement(s);

    pips_assert("s consistent", statement_consistent_p(s));
    pips_assert("es consistent", statement_consistent_p(es));

    /* Let's get rid of s without destroying cms: do not forget the goto t! */
    l = instruction_block(statement_instruction(s));
    pips_assert("cms is the second statement of the block", STATEMENT(CAR(CDR(l))) == cms);
    STATEMENT(CAR(CDR(l))) = statement_undefined;
    instruction_goto(statement_instruction(STATEMENT(CAR(l)))) = statement_undefined;
    free_statement(s);

    pips_assert("es is still consistent", statement_consistent_p(es));
    pips_assert("cms is still consistent", statement_consistent_p(cms));

    debug(1, "BuildStatementForEntry", "End for entry %s in module %s\n",
	  entity_name(e), entity_name(cm));

    return es;
}

static void
ProcessEntry(
    entity cm,
    entity e,
    entity l,
    statement t)
{
    statement es = statement_undefined; /* so as not to compute anything
					   before the debugging message is printed out */
    statement ces = statement_undefined;
    list decls = NIL;
    text txt = text_undefined;
    bool line_numbering_p = FALSE;
    extern void unspaghettify_statement(statement);
    extern unstructured control_graph(statement);
    extern void make_text_resource_and_free(string, string, string, text);

    debug(1, "ProcessEntry", "Begin for entry %s of module %s\n",
	  entity_name(e), module_local_name(cm));

    if(area_size(type_area(entity_type(StaticArea)))!=0) {
	MakeEntryCommon(cm, StaticArea);
    }

    es = BuildStatementForEntry(cm, e, t);

    /* Compute the proper declaration list, without formal parameters from cm
     * and with formal parameters from e
     */

    /* Collect local and global variables of cm that may be visible from entry e */
    MAP(ENTITY, v, {
	if(!storage_formal_p(entity_storage(v))) {
	    decls = arguments_add_entity(decls, v);
	}
	}, entity_declarations(cm));

    ifdebug(2) {
      (void) fprintf(stderr, "Declarations inherited from module %s:\n",
		     module_local_name(cm));
      dump_arguments(entity_declarations(cm));
      (void) fprintf(stderr, "Declarations of formal parameters for entry %s:\n",
		     module_local_name(e));
      dump_arguments(entity_declarations(e));
    }

    /* Try to get rid of unreachable statements which may contain references
     * to formal parameters undeclared in the current entry an obtain a clean
     * entry statement (ces).
     */

    ces = make_statement(entity_empty_label(), 
				 STATEMENT_NUMBER_UNDEFINED,
				 MAKE_ORDERING(0,1),
				 empty_comments,
				 make_instruction(is_instruction_unstructured,
						  control_graph(es)));
    unspaghettify_statement(ces);

    /* Compute an external representation of entry statement es for entry e.
     * Cheat with the declarations because of text_named_module().
     */
    entity_declarations(e) = gen_nconc(entity_declarations(e), decls);

    ifdebug(2) {
      (void) fprintf(stderr, "Declarations of all variables for entry %s:\n",
		     module_local_name(e));
      dump_arguments(entity_declarations(e));
    }

    decls = entity_declarations(cm);
    entity_declarations(cm) = entity_declarations(e);

    ifdebug(1) {
      fprint_environment(stderr, cm);
    }

    line_numbering_p = get_bool_property("PRETTYPRINT_STATEMENT_NUMBER");
    set_bool_property("PRETTYPRINT_STATEMENT_NUMBER", FALSE);
    txt = text_named_module(e, cm, ces);
    set_bool_property("PRETTYPRINT_STATEMENT_NUMBER", line_numbering_p);
    entity_declarations(cm) = decls;

    pips_assert("statement ces is consistent", statement_consistent_p(ces));

    pips_assert("statement for cm is consistent",
		statement_consistent_p(get_current_module_statement()));

    /* */
    make_text_resource_and_free(module_local_name(e), DBR_SOURCE_FILE, ".f", txt);

    pips_assert("statement for cm is consistent",
		statement_consistent_p(get_current_module_statement()));

    free_statement(ces);

    /* give the entry a user file.
     */
    DB_PUT_MEMORY_RESOURCE(DBR_USER_FILE, module_local_name(e), 
	strdup(db_get_memory_resource(DBR_USER_FILE, module_local_name(cm), TRUE)));

    pips_assert("statement for cm is consistent",
		statement_consistent_p(get_current_module_statement()));

    debug(1, "ProcessEntry", "End for entry %s of module %s\n",
	  entity_name(e), module_local_name(cm));

}

void
ProcessEntries()
{
    entity cm = get_current_module_entity();
    code c = entity_code(cm);
    list ce = NIL;
    list cl = NIL;
    list ct = NIL;
    text txt = text_undefined;
    bool line_numbering_p = get_bool_property("PRETTYPRINT_STATEMENT_NUMBER");
    bool data_statements_p = get_bool_property("PRETTYPRINT_DATA_STATEMENTS");

    /* The declarations for cm are likely to be incorrect. They must be
     * synthesized by the prettyprinter.
     */
    free(code_decls_text(c));
    code_decls_text(c) = strdup("");
    /* Regenerate a SOURCE_FILE .f without entries for the module itself */
    /* To avoid warnings about column 73 when the code is parsed again */
    set_bool_property("PRETTYPRINT_STATEMENT_NUMBER", FALSE);
    txt = text_named_module(cm, cm, get_current_module_statement());
    make_text_resource_and_free(module_local_name(cm), DBR_SOURCE_FILE, ".f", txt);

    /* Not ot duplicate DATA statements for static variables and common variables
       in every entry */
    set_bool_property("PRETTYPRINT_DATA_STATEMENTS", FALSE);

    /* Process each entry */
    for(ce = entry_entities, cl = entry_labels, ct = entry_targets;
	!ENDP(ce) && !ENDP(cl) && !ENDP(ct); POP(ce), POP(cl), POP(ct)) {
	entity e = ENTITY(CAR(ce));
	entity l = ENTITY(CAR(cl));
	statement t = STATEMENT(CAR(ct));

	pips_assert("Target and label match", l==statement_label(t));

	ProcessEntry(cm, e, l, t);
    }
    set_bool_property("PRETTYPRINT_STATEMENT_NUMBER", line_numbering_p);
    set_bool_property("PRETTYPRINT_DATA_STATEMENTS", data_statements_p);
    /* Postponed to the_actual_parser() which needs to know entries were
       encountered */
    /* ResetEntries(); */
}

entity
NameToFunctionalEntity(string name)
{
    entity f = gen_find_tabulated
	(concatenate(TOP_LEVEL_MODULE_NAME, MODULE_SEP_STRING,
		     BLOCKDATA_PREFIX, name, 0),
	 entity_domain);

    if(entity_undefined_p(f)) {
	f = gen_find_tabulated
	    (concatenate(CurrentPackage, MODULE_SEP_STRING, name, 0),
	     entity_domain);

	/* Ignore ghost variables, they are *not* in the current scope */
	f = ghost_variable_entity_p(f)? entity_undefined : f;

	if(entity_undefined_p(f)) {
	    f = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, name);
	}
	else if(!storage_undefined_p(entity_storage(f))
		&& storage_formal_p(entity_storage(f))) {
	    /* The functional entity must be a formal parameter */
	    ;
	}
	else if(storage_undefined_p(entity_storage(f))) {
	    /* The current declaration is wrong and should be fixed
	     * later, i.e. by MakeExternalFunction() or MakeCallInst()
	     */
	    ;
	}
	else {
	    pips_assert("Unexpected kind of functional entity!", TRUE);
	}
    }
    else {
	/* It is the name of a blockdata */
	;
    }
    return f;
}

/* The result type of a function may be carried by e, by r or be implicit.
 * A new type structure is allocated, unless r is used as new result type.
 */

type
MakeResultType(
    entity e,
    type r)
{
    type te = entity_type(e);
    type new_r = type_undefined;

    if (te != type_undefined) {
	if (type_variable_p(te)) {
	    /* e is a function that was implicitly declared as a variable. 
	       this may happen in Fortran. */
	    pips_debug(2, "variable --> fonction\n");
	    pips_assert("undefined type", r == type_undefined);
	    new_r = copy_type(te);
	}
	else if (type_functional_p(te)) {
	    /* Well... this should be useless because e is already typed.
	     * FI: I do not believe copy_type() is necessary in spite of
	     * the non orthogonality...
	     */
	    new_r = functional_result(type_functional(te));
	}
	else {
	    pips_error("MakeResultType", "Unexpected type %s for entity %s\n", 
		       type_to_string(te), entity_name(e));
	}
    }
    else {
	if(type_undefined_p(r)) {
	    new_r = ImplicitType(e);
	}
	else {
	    new_r = r;
	}
    }
    pips_assert("type new_r is defined", !type_undefined_p(new_r));
    return new_r;
}

/* A local entity might have been created but found out later to be
 * global, depending on the order of declaration statements (see
 * MakeExternalFunction()). The local entity e is (marked as) destroyed
 * and replaced by functional entity fe.
 */

entity
LocalToGlobal(entity e)
{
    return SafeLocalToGlobal(e, type_undefined);
}

entity
SafeLocalToGlobal(entity e, type r)
{
    entity fe = entity_undefined;

    if(!top_level_entity_p(e)) {
	storage s = entity_storage(e);
	if(s == storage_undefined || storage_ram_p(s)) {
	    extern list arguments_add_entity(list a, entity e);

	    fe = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, 
				    entity_local_name(e));
	    if(storage_undefined_p(entity_storage(fe))) {
		entity_storage(fe) = make_storage(is_storage_rom, UU);
	    }
	    else if(!storage_rom_p(entity_storage(fe))) {
		FatalError("SafeLocalToGlobal",
			   "Unexpected storage class for top level entity\n");
	    }
	    if(value_undefined_p(entity_initial(fe))) {
		entity_initial(fe) = make_value(is_value_code,
						make_code(NIL, strdup("")));
	    }

	    pips_debug(1, "external function %s re-declared as %s\n",
		       entity_name(e), entity_name(fe));
	    /* FI: I need to destroy a virtual entity which does not
	     * appear in the program and wich was temporarily created by
	     * the parser when it recognized a name; however, I've no way
	     * to know if the same entity does not appear for good
	     * somewhere else in the code; does the Fortran standard let
	     * you write: LOG = LOG(3.)  If yes, PIPS will core dump...
	     * PIPS also core dumps with ALOG(ALOG(X))... (8 July 1993) */
	    /* remove_variable_entity(e); */
	    if(type_undefined_p(r)) {
		add_ghost_variable_entity(e);
		pips_debug(1, "entity %s to be destroyed\n", entity_name(e));
	    }
	    else {
		pips_debug(1, "entity %s to be preserved to carry function result\n",
			   entity_name(e));
	    }
	}
	else if(storage_formal_p(s)){
	    pips_user_warning("entity %s is a formal functional parameter\n",
			      entity_name(e));
	    ParserError("LocalToGlobal",
			"Formal functional parameters are not supported "
			"by PIPS.\n");
	    fe = e;
	}
	else {
	    pips_internal_error("entity %s has an unexpected storage %d\n",
				entity_name(e), storage_tag(s));
	}
    }
    else {
	fe = e;
    }
    pips_assert("Entity is global", top_level_entity_p(fe));
    return fe;
}

void
TypeFunctionalEntity(entity fe,
		     type r)
{
    type tfe = entity_type(fe);

    if(tfe == type_undefined) {
	/* this is wrong, because we do not know if we are handling
	   an EXTERNAL declaration, in which case the result type
	   is type_undefined, or a function call appearing somewhere,
	   in which case the ImplicitType should be used;
	   maybe the unknown type should be used? */
	entity_type(fe) = make_type(is_type_functional, 
				   make_functional(NIL, 
						   (r == type_undefined) ?
						   ImplicitType(fe) :
						   r));
    }
    else if (type_functional_p(tfe)) 
    {
	type tr = functional_result(type_functional(tfe));
	if(r != type_undefined && !type_equal_p(tr, r)) {

	    /* a bug is detected here: MakeExternalFunction, as its name
	       implies, always makes a FUNCTION, even when the symbol
	       appears in an EXTERNAL statement; the result type is
	       infered from ImplicitType() - see just above -;
	       let's use implicit_type_p() again, whereas the unknown type
	       should have been used 
	    */
	    if(intrinsic_entity_p(fe)) {
		/* ignore r */
	    } else if (type_void_p(tr)) {
		/* someone used a subroutine as a function.
		 * this happens in hpfc for declaring "pure" routines.
		 * thus I make this case being ignored. warning? FC.
		 */		
	    } else if (implicit_type_p(fe) || overloaded_type_p(tr)) {
		/* memory leak of tr */
		functional_result(type_functional(tfe)) = r;
	    } else  {
		user_warning("TypeFunctionalEntity",
			     "Type redefinition of result for function %s\n", 
			     entity_name(fe));
		if(type_variable_p(tr)) {
		    user_warning("TypeFunctionalEntity",
				 "Currently declared result is %s\n", 
				 basic_to_string(variable_basic(type_variable(tr))));
		}
		if(type_variable_p(r)) {
		    user_warning("TypeFunctionalEntity",
				 "Redeclared result is %s\n", 
				 basic_to_string(variable_basic(type_variable(r))));
		}
		ParserError("TypeFunctionalEntity",
			    "Functional type redefinition.\n");
	    }
	}
    } else if (type_variable_p(tfe)) {
	pips_internal_error("Fortran does not support global variables\n");
    } else {
	pips_internal_error("Unexpected type for a global name %s\n",
			    entity_name(fe));
    }
}

/* 
 * This function creates an external function. It may happen in
 * Fortran that a function is declared as if it were a variable; example:
 *
 * INTEGER*4 F
 * ...
 * I = F(9)
 *
 * or:
 *
 * SUBROUTINE FOO(F)
 * ...
 * CALL F(9)
 *
 * in these cases, the initial declaration must be updated, 
 * ie. the variable declaration must be
 * deleted and replaced by a function declaration. 
 *
 * This function is called when an EXTERNAL or a CALL statement is
 * analyzed.
 *
 * See DeclareVariable for other combination based on EXTERNAL
 *
 * Modifications:
 *  - to perform link edition at parse time, returns a new entity when
 *    e is not a TOP-LEVEL entity; this changes the function a lot;
 *    Francois Irigoin, 9 March 1992;
 *  - introduction of fe and tfe to clean up the relationship between e
 *    and the new TOP-LEVEL entity; formal functional parameters were
 *    no more recognized as a bug because of the previous modification;
 *    Francois Irigoin, 11 July 1992;
 *  - remove_variable_entity() added to avoid problems in semantics analysis
 *    with an inexisting variable, FI, June 1993;
 *  - a BLOCKDATA can be declared EXTERNAL, FI, May 1998
 */

entity 
MakeExternalFunction(
    entity e, /* entity to be turned into external function */
    type r /* type of result */)
{
    entity fe = entity_undefined;
    type new_r = type_undefined;

    debug(8, "MakeExternalFunction", "Begin for %s\n", entity_name(e));

    if(entity_blockdata_p(e)) {
      debug(8, "MakeExternalFunction", "End for blockdata %s\n", entity_name(e));
      return e;
    }

    new_r = MakeResultType(e, r);

    pips_debug(9, "external function %s declared\n", entity_name(e));

    fe = LocalToGlobal(e);

    /* Assertion: fe is a (functional) global entity and the type of its 
       result is new_r */

    TypeFunctionalEntity(fe, new_r);

    /* a function has a rom storage, except for formal functions */

    if (entity_storage(e) == storage_undefined)
	entity_storage(fe) = MakeStorageRom();
    else
	if (! storage_formal_p(entity_storage(e)))
	    entity_storage(fe) = MakeStorageRom();
	else {
	    pips_user_warning("unsupported formal function %s\n", 
			 entity_name(fe));
	    ParserError("MakeExternalFunction",
			"Formal functions are not supported by PIPS.\n");
	}

    /* an external function has an unknown initial value, else code would be temporarily
     * undefined which is avoided (theoretically forbidden) in PIPS.
     */
    if(entity_initial(fe) == value_undefined)
	entity_initial(fe) = MakeValueUnknown();

    /* fe is added to CurrentFunction's entities */
    AddEntityToDeclarations(fe, get_current_module_entity());

    debug(8, "MakeExternalFunction", "End for %s\n", entity_name(fe));

    return fe;
}

entity 
DeclareExternalFunction(
    entity e /* entity to be turned into external function */)
{
    entity fe = MakeExternalFunction(e, type_undefined);

    if(value_intrinsic_p(entity_initial(fe))) {
	pips_user_warning(
	    "Name conflict between user declared module %s and intrinsic %s\n",
	    module_local_name(fe), module_local_name(fe));
	ParserError("DeclareExternalFunction",
		    "Name conflict with intrinsic because PIPS does not support"
		    " a specific name space for intrinsics. "
		    "Please change your function or subroutine name.");
    }

    return fe;
}

/* This function transforms an untyped entity into a formal parameter. 
 * fp is an entity generated by FindOrCreateEntity() for instance,
 * and nfp is its rank in the formal parameter list.
 *
 * A specific type is used for the return code variable which may be
 * adde by the parser to handle alternate returns. See return.c
 */

void 
MakeFormalParameter(
		    entity m, /* module of formal parameter */
		    entity fp, /* formal parameter */
		    int nfp) /* offset (i.e. rank) of formal parameter */
{
    pips_assert("type is undefined", entity_type(fp) == type_undefined);

    if(SubstituteAlternateReturnsP() && ReturnCodeVariableP(fp)) {
	entity_type(fp) = MakeTypeVariable(make_basic(is_basic_int, (void *) 4), NIL);
    }
    else {
	entity_type(fp) = ImplicitType(fp);
    }

    entity_storage(fp) = 
	make_storage(is_storage_formal, make_formal(m, nfp));
    entity_initial(fp) = MakeValueUnknown();
}



/* this function scans the formal parameter list. each formal parameter
is created with an implicit type, and then is added to CurrentFunction's
declarations. */
void 
ScanFormalParameters(entity m, list l)
{
	list pc;
	entity fp; /* le parametre formel */
	int nfp; /* son rang dans la liste */

	FormalParameters = l;

	for (pc = l, nfp = 1; pc != NULL; pc = CDR(pc), nfp += 1) {
		fp = ENTITY(CAR(pc));

		MakeFormalParameter(m, fp, nfp);

		AddEntityToDeclarations(fp, m);
	}
}

/* this function check and set if necessary the storage of formal parameters in lfp. */
void 
UpdateFormalStorages(
		     entity m, 
		     list lfp)
{
    list fpc; /* formal parameter chunk */
    int fpo; /* formal parameter offset */

    for (fpc = lfp, fpo = 1; !ENDP(fpc); POP(fpc), fpo += 1) {
	entity fp = ENTITY(CAR(fpc));
	storage fps = entity_storage(fp);

	pips_assert("Formal parameter fp must be in scope of module m",
		    m==local_name_to_top_level_entity(entity_module_name(fp)));

	if(storage_undefined_p(fps)) {
	    entity_storage(fp) = make_storage(is_storage_formal,
					      make_formal(m, fpo));
	}
	else if(storage_ram_p(fps)){
	    /* Oupss... the associated area should be cleaned up...  but
	     * it should ony occur in EndOfProcedure() when all implictly
	     * declared variables have been encountered...
	     */
	    free_storage(fps);
	    entity_storage(fp) = make_storage(is_storage_formal,
					      make_formal(m, fpo));
	}
	else if(storage_formal_p(fps)){
	    pips_assert("Consistent Offset", 
			fpo==formal_offset(storage_formal(fps)));
	}
	else {
	    pips_internal_error("Unexpected storage for entity %s\n",
				entity_name(fp));
	}
    }
}



/* this function creates an intrinsic function. */

entity 
CreateIntrinsic(string name)
{
    entity e = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, name);
    pips_assert("entity is defined", e!=entity_undefined);
    return(e);
}

#include <stdio.h>
extern int fprintf();
#include "string.h"

#include "genC.h"
#include "parser_private.h"
#include "ri.h"
#include "ri-util.h"
#include "database.h"
#include "pipsdbm.h"
#include "resources.h"

#include "misc.h"
#include "properties.h"

#include "syntax.h"

#include "toklex.h"

/* list of called subroutines or functions */
static list called_modules = list_undefined;

/* list of potential local variables that turned out to be useless */
static list ghost_variable_entities = list_undefined;

/* statement of current function */
static statement function_body = statement_undefined;

/* this function is called each time a new procedure is encountered. */
void BeginingOfProcedure()
{
    reset_current_module_entity();
    InitImplicit();
    called_modules = NIL;
}

void update_called_modules(e)
entity e;
{
    bool already_here = FALSE;
    string n = entity_local_name(e);
    string nom;

    /* do not count intrinsics; user function should not be named
       like intrinsics */
    nom = concatenate(TOP_LEVEL_MODULE_NAME, MODULE_SEP_STRING, n, NULL);
    if ((e = gen_find_tabulated(nom, entity_domain)) != entity_undefined) 
	if(entity_initial(e) == value_undefined) {
	    /* FI, 20/01/92: maybe, initializations of global entities
	       should be more precise (storage, initial value, etc...);
	       for the time being, I choose to ignore the potential
	       problems with other executions of the parser and the linker */
	    /* pips_error("update_called_modules","unexpected case\n"); */
	}
	else if(value_intrinsic_p(entity_initial(e)))
	    return;

    MAPL(ps, {
	if (strcmp(n, STRING(CAR(ps))) == 0) {
	    already_here = TRUE;
	    break;
	}
    }, called_modules);

    if (! already_here) {
	debug(1, "update_called_modules", "addind %s\n", n);
	called_modules = CONS(STRING, n, called_modules);
    }
}


/* this function is called when the parsing of a procedure is done. it
performs a few calculations which cannot be done on the fly and write
the internal representation of the CurrentFunction with a call to
gen_free. */

void EndOfProcedure()
{
    entity CurrentFunction = get_current_module_entity();

    /* get rid of ghost variable entities */
    remove_ghost_variable_entities();

    /* we generate the last statement */
    if (strcmp(lab_I, "") != 0) {
	LinkInstToCurrentBlock(MakeZeroOrOneArgCallInst("CONTINUE", 
							expression_undefined));
    }

    /* we generate statement last+1 to eliminate returns */
    GenerateReturn();

    (void) PopBlock() ;
    if (!IsBlockStackEmpty())
	    ParserError("EndOfProcedure", "bad program structure\n");

    /* are there undefined gotos ? */
    CheckAndInitializeStmt();

    UpdateFunctionalType(FormalParameters);

    check_common_layouts(CurrentFunction);

    ComputeEquivalences();
    ComputeAddresses();
    SaveChains();


    code_declarations(EntityCode(CurrentFunction)) =
	    gen_nreverse(code_declarations(EntityCode(CurrentFunction))) ;

    ifdebug(5){
	fprintf(stderr, "Parser: checking callees consistency = %d\n",
		gen_consistent_p( make_callees( called_modules ))) ;
    }

    /*  remove hpfc special routines if required.
     */
    if (get_bool_property("HPFC_FILTER_CALLEES"))
    {
	extern bool hpf_directive_string_p();
	string s;
	list l = NIL;

	MAPL(cs,
	 {
	     s = STRING(CAR(cs));
	     if (!hpf_directive_string_p(s))
		 l = CONS(STRING, s, l);
	 },
	     called_modules);

	gen_free_list(called_modules);
	called_modules = l;
    }

    DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, 
			   strdup(module_local_name(CurrentFunction)), 
			   (char*) make_callees(called_modules));

    if( get_debug_level() >= 5 ) {
	fprintf(stderr, "Parser: checking code consistency = %d\n",
		gen_consistent_p( function_body )) ;
    }
    DB_PUT_MEMORY_RESOURCE(DBR_PARSED_CODE, 
			   strdup(module_local_name(CurrentFunction)), 
			   (char *)function_body);

    /* the current package is re-initialized */
    CurrentPackage = TOP_LEVEL_MODULE_NAME;
    ResetChains();
    reset_current_module_entity();
}



/* this function analyzes the CurrentFunction formal parameter list to
determine the CurrentFunction functional type. l is this list. */

void UpdateFunctionalType(l)
cons *l;
{
    cons *pc;
    parameter p;
    functional ft;
    entity CurrentFunction = get_current_module_entity();

    ft = type_functional(entity_type(CurrentFunction));
    pips_assert("UpdateFunctionalType", functional_parameters(ft) == NULL);

    for (pc = l; pc != NULL; pc = CDR(pc)) {
	p = make_parameter((entity_type(ENTITY(CAR(pc)))), 
			   (MakeModeReference()));
	functional_parameters(ft) = 
		gen_nconc(functional_parameters(ft),
			  CONS(PARAMETER, p, NIL));
    }
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
void MakeCurrentFunction(t, msf, cf, lfp)
type t;
int msf;
entity cf;
cons *lfp;
{
    instruction icf; /* the body of the current function */
    entity result; /* the second entity */

    /* set ghost variable entities to NIL */
    init_ghost_variable_entities();

    if (msf == TK_FUNCTION) {
	if (t == type_undefined) {
	    t = ImplicitType(cf);
	}
    }
    else {
	if (t == type_undefined) {
	    t = make_type(is_type_void, UU);
	    if(msf == TK_PROGRAM) {
		extern list arguments_add_entity(list a, entity e);
		string main_name = strdup(concatenate(MAIN_PREFIX, entity_local_name(cf),NULL));
		entity fe = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, main_name);

		free(main_name);
		debug(1, "MakeCurrentFunction",
		      "current function %s re-declared as %s\n",
		      entity_name(cf), entity_name(fe));
		ghost_variable_entities = arguments_add_entity(ghost_variable_entities, cf);
		debug(1, "MakeCurrentFunction",
		      "entity %s to be destroyed\n",
		      entity_name(cf));
		cf = fe;
	    }
	}
	else {
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
    PushBlock(icf, (string) NULL);
    function_body = MakeStatement(MakeLabel(""), icf);
    entity_initial(cf) = make_value(is_value_code, make_code(NIL, NULL));

    set_current_module_entity(cf);

    /* two global areas are created */
    InitAreas();

    /* No commons have yet been declared */
    initialize_common_size_map();

    /* formal parameters are created */
    ScanFormalParameters(lfp);

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
 */

entity MakeExternalFunction(e, r)
entity e;
type r; /* type of result */
{
    type te;
    entity fe = entity_undefined;
    type tfe;

    te = entity_type(e);
    if (te != type_undefined) {
	if (type_variable_p(te)) {
	    /* e is a function that was implicitly declared as a variable. 
	       this may happen in Fortran. */
	    debug(2, "MakeExternalFunction", "variable --> fonction\n");
	    pips_assert("MakeExternalFunction", r == type_undefined);
	    r = te;
	}
    }

    debug(9, "MakeExternalFunction", " external function %s declared\n",
	  entity_name(e));

    if(!top_level_entity_p(e)) {
	storage s = entity_storage(e);
	if(s == storage_undefined || storage_ram_p(s)) {
	    extern list arguments_add_entity(list a, entity e);

	    fe = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, entity_local_name(e));

	    debug(1, "MakeExternalFunction",
		  "external function %s re-declared as %s\n",
		  entity_name(e), entity_name(fe));
	    /* FI: I need to destroy a virtual entity which does not
	     * appear in the program and wich was temporarily created by
	     * the parser when it recognized a name; however, I've no way
	     * to know if the same entity does not appear for good
	     * somewhere else in the code; does the Fortran standard let
	     * you write: LOG = LOG(3.)  If yes, PIPS will core dump...
	     * PIPS also core dumps with ALOG(ALOG(X))... (8 July 1993) 
	     */
	    /* remove_variable_entity(e); */
	    ghost_variable_entities = 
		arguments_add_entity(ghost_variable_entities, e);
	    debug(1, "MakeExternalFunction",
		  "entity %s to be destroyed\n",
		  entity_name(e));
	    if(r!=type_undefined) {
		/* r is going to be re-used to build the functional type
		 * which is going to be freed later with e
		 */
		type new_r = type_undefined;
		new_r = gen_copy_tree(r);
		r = new_r;
	    }
	}
	else if(storage_formal_p(s)){
	    user_error("MakeExternalFunction", 
		       "entity %s is a formal functional parameter\n",
		       entity_name(e));
	    fe = e;
	}
	else {
	    pips_error("MakeExternalFunction", 
		       "entity %s has an unexpected storage %d\n",
		       entity_name(e), storage_tag(s));
	}
    }
    else
	fe = e;

    /* Assertion: fe is a (functional) global entity and the type of its 
       result is r */

    tfe = entity_type(fe);
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
	       should have been used */
	    if(implicit_type_p(fe))
		/* memory leak of tr */
		functional_result(type_functional(tfe)) = r;
	    else
		pips_error("MakeExternalFunction",
			   "Type redefinition for %s\n", entity_name(e));
	}
    }
    else if (type_variable_p(tfe)) {
	pips_error("MakeExternalFunction",
		   "Fortran does not support global variables\n");
	}
    else {
	pips_error("MakeExternalFunction",
		   "Unexpected type for a global name %s\n",
		   entity_name(fe));
    }

    /* a function has a rom storage, except for formal functions */
    if (entity_storage(e) == storage_undefined)
	entity_storage(fe) = MakeStorageRom();
    else
	if (! storage_formal_p(entity_storage(e)))
	    entity_storage(fe) = MakeStorageRom();
	else {
	    user_error("MakeExternalFunction",
			 "unsupported formal function %s\n", 
			 entity_name(fe));
	    /*
	    ParserError("MakeExternalFunction",
			"Formal functions are not supported\n");
			*/
	}

    /* an external function has an unknown initial value */
    if(entity_initial(fe) == value_undefined)
	entity_initial(fe) = MakeValueUnknown();

    /* e is added to CurrentFunction's entities */
    AddEntityToDeclarations(fe, get_current_module_entity());

    return fe;
}

/* This function creates a formal parameter. fp is an entity, and nfp is
its rank in the formal parameter list. */

void MakeFormalParameter(fp, nfp)
entity fp;
int nfp;
{
    pips_assert("MakeFormalParameter", entity_type(fp) == type_undefined);

    entity_type(fp) = ImplicitType(fp);
    entity_storage(fp) = make_storage(is_storage_formal, 
				      make_formal(get_current_module_entity(), nfp));
    entity_initial(fp) = MakeValueUnknown();
}



/* this function scans the formal parameter list. each formal parameter
is created with an implicit type, and then is added to CurrentFunction's
declarations. */
void ScanFormalParameters(l)
cons * l;
{
	cons *pc;
	entity fp; /* le parametre formel */
	int nfp; /* son rang dans la liste */

	FormalParameters = l;

	for (pc = l, nfp = 1; pc != NULL; pc = CDR(pc), nfp += 1) {
		fp = ENTITY(CAR(pc));

		MakeFormalParameter(fp, nfp);

		AddEntityToDeclarations(fp, get_current_module_entity());
	}
}



/* this function creates an intrinsic function. */

entity CreateIntrinsic(name)
string name;
{
    /* entity e = FindOrCreateEntity(CurrentPackage, name); */
    entity e = FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, name);

    pips_assert("CreateIntrinsic",
		MakeExternalFunction(e, MakeOverloadedResult()) == e);

    return(e);
}

void init_ghost_variable_entities()
{
    ghost_variable_entities = NIL;
}

void remove_ghost_variable_entities()
{
    MAPL(ce, {
	entity e = ENTITY(CAR(ce));

	remove_variable_entity(e);
	    debug(1, "remove_ghost_variable_entities",
		  "entity %s destroyed\n",
		  entity_name(e));
	}, ghost_variable_entities);

    ghost_variable_entities = list_undefined;
}

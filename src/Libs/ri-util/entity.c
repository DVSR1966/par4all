#include <stdio.h>

#include "genC.h"
#include "misc.h"
#include "ri.h"

#include "ri-util.h"

entity make_empty_program(name)
string name;
{
    string full_name = concatenate(TOP_LEVEL_MODULE_NAME, 
				   MODULE_SEP_STRING, MAIN_PREFIX, name, NULL);
    return make_empty_module(full_name);
}

entity make_empty_subroutine(name)
string name;
{
    string full_name = concatenate(TOP_LEVEL_MODULE_NAME, 
				   MODULE_SEP_STRING, name, NULL);
    return make_empty_module(full_name);
}

entity make_empty_module(full_name)
string full_name;
{
    string name = string_undefined;
    entity e = gen_find_tabulated(full_name, entity_domain);
    entity DynamicArea, StaticArea;

    pips_assert("make_empty_module", e == entity_undefined);

    e = make_entity(strdup(full_name), 
		    make_type(is_type_functional, 
			      make_functional(NIL, 
					      make_type(is_type_void, UU))),
		    MakeStorageRom(),
		    make_value(is_value_code,
			       make_code(NIL,"")));

    name = module_local_name(e);
    DynamicArea = FindOrCreateEntity(name, DYNAMIC_AREA_LOCAL_NAME);
    entity_type(DynamicArea) = make_type(is_type_area, make_area(0, NIL));
    entity_storage(DynamicArea) = MakeStorageRom();
    entity_initial(DynamicArea) = MakeValueUnknown();
    AddEntityToDeclarations(DynamicArea, e);

    StaticArea = FindOrCreateEntity(name, STATIC_AREA_LOCAL_NAME);
    entity_type(StaticArea) = make_type(is_type_area, make_area(0, NIL));
    entity_storage(StaticArea) = MakeStorageRom();
    entity_initial(StaticArea) = MakeValueUnknown();
    AddEntityToDeclarations(StaticArea, e);

    return(e);
}


/* this function checks that e has an initial value code. if yes returns
it, otherwise aborts.  */

code EntityCode(e) 
entity e; 
{
    value ve = entity_initial(e);
    pips_assert("EntityCode", value_tag(ve) == is_value_code);
    return(value_code(ve));
}

 
/* This function returns a new label */
entity make_new_label(module_name)
char * module_name;
{
    /* FI: do labels have to be declared?*/
    /* FI: it's crazy; the name is usually derived from the entity
       by the caller and here the entity is retrieved from its name! */
    entity mod = local_name_to_top_level_entity(module_name); 
    string strg = new_label_name(mod);
    entity l;
  
    l = make_entity(strdup(strg), type_undefined, storage_undefined, 
			value_undefined);
    entity_type(l) = (type) MakeTypeStatement();
    entity_storage(l) = (storage) MakeStorageRom();
    entity_initial(l) = make_value(is_value_constant,
				   MakeConstantLitteral());
 
    return(l);

}

entity make_loop_label(desired_number, module_name)
int desired_number;
char *module_name;
{
    entity e = make_new_label(module_name);

    return (e);
}

/* predicates and functions for entities 
 */

/* entity_local_name modified so that it does not core when used in
 * vect_fprint, since someone thought that it was pertinent to remove the
 * special care of constants there. So I added something here, to deal
 * with the "null" entity which codes the constant. FC 28/11/94.
 */
string entity_local_name(e)
entity e;
{
    static string 
       null_name = "null";
    /* undefined_name = "entity_undefined"; */

    assert(!entity_undefined_p(e));

    return(e==NULL ? null_name : 
	   /* e==entity_undefined ? undefined_name : */
	   local_name(entity_name(e)));
}

string module_local_name(e)
entity e;
{
    string name = local_name(entity_name(e));
    return name+strspn(name, MAIN_PREFIX);
}

string label_local_name(e)
entity e;
{
    string name = local_name(entity_name(e));
    return name+strlen(LABEL_PREFIX);
}

/* See next function! */
/*
string entity_relative_name(e)
entity e;
{
    extern entity get_current_module_entity();
    entity m = get_current_module_entity();
    string s = string_undefined;

    pips_assert("entity_relative_name", !entity_undefined_p(m));

    s = (strcmp(module_local_name(m), entity_module_name(m)) == 0) ? 
	entity_local_name(e) : entity_name(e) ;

    return s;
}
*/

string entity_minimal_name(e)
entity e;
{
    entity 
	m = get_current_module_entity();

    return (strcmp(module_local_name(m), entity_module_name(e)) == 0) ? 
	entity_local_name(e) : entity_name(e) ;
}

bool entity_empty_label_p(e)
entity e;
{
    return(empty_label_p(entity_name(e)));
}

bool entity_label_p(e)
entity e;
{
    return type_statement_p(entity_type(e));
}

bool entity_module_p(e)
entity e;
{
    value v = entity_initial(e);

    return(v != value_undefined && value_code_p(v));
}

bool entity_main_module_p(e)
entity e;
{
    return entity_module_p(e) && strspn(entity_local_name(e), MAIN_PREFIX) == 1;
}

bool entity_function_p(e)
entity e;
{
    type
	t_ent = entity_type(e),
	t = (type_functional_p(t_ent) ? 
	     functional_result(type_functional(t_ent)) : 
	     type_undefined);
    
    return(entity_module_p(e) && 
	   !type_undefined_p(t) &&
	   !type_void_p(t));
}

bool entity_subroutine_p(e)
entity e;
{
    return(entity_module_p(e) && 
	   !entity_main_module_p(e) && 
	   !entity_function_p(e));
}

bool local_entity_of_module_p(e, module)
entity e, module;
{
    bool
	result = same_string_p(entity_module_name(e), 
			       module_local_name(module));

    debug(6, "local_entity_of_module_p",
	  "%s %s %s\n", 
	  entity_name(e), result ? "in" : "not in", entity_name(module));

	  return(result);
}

bool entity_in_common_p(e)
entity e;
{
    storage s = entity_storage(e);

    return(storage_ram_p(s) && 
	   !SPECIAL_COMMON_P(ram_section(storage_ram(s))));
}

string entity_module_name(e)
entity e;
{
    return(module_name(entity_name(e)));
}

code entity_code(e)
entity e;
{
    value ve = entity_initial(e);
    pips_assert("entity_code",value_code_p(ve));
    return(value_code(ve));
}

entity entity_empty_label()
{
    /* FI: it is difficult to memoize entity_empty_label because its value is changed
     * when the symbol table is written and re-read from disk; Remi's memoizing
     * scheme was fine as long as the entity table lasted as long as one run of
     * pips, i.e. is not adequate for wpips
     */
    /* static entity empty = entity_undefined; */
    entity empty;

    empty = gen_find_tabulated(concatenate(TOP_LEVEL_MODULE_NAME,
					   MODULE_SEP_STRING, 
					   LABEL_PREFIX,
					   NULL), entity_domain);
    pips_assert("entity_empty_label", empty != entity_undefined );

    return empty;
}

bool top_level_entity_p(e)
entity e;
{
    return(strncmp(TOP_LEVEL_MODULE_NAME, 
		   entity_name(e),
		   strlen(entity_module_name(e))) == 0);
}

entity entity_intrinsic(name)
string name;
{
    entity e = gen_find_tabulated(concatenate(TOP_LEVEL_MODULE_NAME,
					        MODULE_SEP_STRING,
					        name,
					        NULL),
				  entity_domain);

    pips_assert("entity_intrinsic", e != entity_undefined );
    return(e);
}

/* predicates on entities */

bool same_entity_p(e1, e2)
entity e1, e2;
{
    return(e1 == e2);
}

/*  Comparison function for qsort.
 */
int compare_entities(pe1, pe2)
entity *pe1, *pe2;
{
    int
	null_1 = (*pe1==(entity)NULL),
	null_2 = (*pe2==(entity)NULL);
    
    if (null_1 || null_2) 
	return(null_2-null_1);
    else
	return(strcmp(entity_name(*pe1), entity_name(*pe2)));
}

/*   TRUE if var1 <= var2
 */
bool lexicographic_order_p(var1, var2)
entity var1, var2;
{
    /*   TCST is before anything else
     */
    if ((Variable) var1==TCST) return(TRUE);
    if ((Variable) var2==TCST) return(FALSE);

    /* else there are two entities 
     */

    return(strcmp(entity_local_name(var1), entity_local_name(var2))<=0);
}

/* return the basic associated to entity e if it's a function/variable/constant
 * basic_undefined otherwise */
basic entity_basic(e)
entity e;
{
    if (e != entity_undefined) {
	type t = entity_type(e);
	
	if (type_functional_p(t))
	    t = functional_result(type_functional(t));
	if (type_variable_p(t))
	    return (variable_basic(type_variable(t)));
    }
    return (basic_undefined);
}

/* return TRUE if the basic associated with entity e matchs the passed tag */
bool entity_basic_p(e, basictag)
entity e;
int basictag;
{
    return (basic_tag(entity_basic(e)) == basictag);
}

/*
this function maps a local name, for instance P, to the corresponding
TOP-LEVEL entity, whose name is TOP-LEVEL:P

n is the local name
*/

entity local_name_to_top_level_entity(n)
string n;
{
    entity module =
	gen_find_tabulated(concatenate(TOP_LEVEL_MODULE_NAME,
				       MODULE_SEP_STRING,
				       n,
				       (char *) NULL),
			   entity_domain);

    if(entity_undefined_p(module)) {
	module =
	    gen_find_tabulated(concatenate(TOP_LEVEL_MODULE_NAME,
					   MODULE_SEP_STRING,
					   MAIN_PREFIX,
					   n,
					   (char *) NULL),
			       entity_domain);
    }
    return module;
}

entity global_name_to_entity(m, n)
string m;
string n;
{

    entity global_name;

    global_name = 
	gen_find_tabulated(concatenate(m,
				       MODULE_SEP_STRING,
				       n,
				       (char *) NULL),
			   entity_domain);

    if (entity_undefined_p(global_name)) {
	global_name = 
	    gen_find_tabulated(concatenate(MAIN_PREFIX,
					   m,
					   MODULE_SEP_STRING,
					   n,
					   (char *) NULL),
			       entity_domain);
    }

    return(global_name);
}

/*
 * Cette fonction est appelee chaque fois qu'on rencontre un nom dans le texte
 * du pgm Fortran.  Ce nom est celui d'une entite; si celle-ci existe deja on
 * renvoie son indice dans la table des entites, sinon on cree une entite vide
 * qu'on remplit partiellement.
 *
 * Modifications:
 *  - partial link at parse time for global entities (Remi Triolet?)
 *  - partial link limited to non common variables (Francois Irigoin,
 *    25 October 1990); see below;
 *  - no partial link: no information is available about "name"'s type; it
 *    can be a local variable as well as a global one; if D is a FUNCTION and
 *    D is parsed and put in the database, any future D would be interpreted
 *    as a FUNCTION and an assignment like D = 0. would generate a parser
 *    error message (Francois Irigoin, 6 April 1991)
 *  - partial link limited to intrinsic: it's necessary because there is no
 *    real link; local variables having the same name as an intrinsic will
 *    cause trouble; I did not find a nice way to fix the problem later,
 *    as it should be, in update_called_modules(), MakeAtom() or MakeCallInst()
 *    it would be necessary to rewrite the link phase; (Francois Irigoin,
 *    11 April 1991)
 *  - no partial link at all: this is incompatible with the Perfect Club
 *    benchmarks; variables DIMS conflicts with intrinsics DIMS;
 *    (Francois Irigoin, ?? ???? 1991)
 */
entity FindOrCreateEntity(package, name)
string package; /* le nom du package */
string name; /* le nom de l'entite */
{
    entity e;
    string nom;

    nom = concatenate(package, MODULE_SEP_STRING, name, NULL);
    if ((e = gen_find_tabulated(nom, entity_domain)) != entity_undefined) {
	return(e);
    }

    /* Francois Irigoin (FI): the following optimization, partial link at parse
       time is dangerous for COMMONs; their size is incremented as many
       times as they are encountered in the whole program;

       It also catches variables named like intrinsic, e.g. DIM */

    /*
    nom = concatenate(TOP_LEVEL_MODULE_NAME, MODULE_SEP_STRING, name, NULL);
    if ((e = gen_find_tabulated(nom, entity_domain)) != entity_undefined) {
	-- if(!type_area_p(entity_type(e))) --
	if(type_functional_p(entity_type(e)) && 
	   value_intrinsic_p(entity_initial(e)))
	    return(e);
    }
    */


    nom = concatenate(package, MODULE_SEP_STRING, name, NULL);
    return(make_entity(strdup(nom), type_undefined, storage_undefined, 
			value_undefined));
}

/* FIND_MODULE returns entity. Argument is module_name */
/* This function should be replaced by local_name_to_top_level_entity() */
/*entity find_entity_module(name)
string name;
{
    string full_name = concatenate(TOP_LEVEL_MODULE_NAME, 
				   MODULE_SEP_STRING, name, NULL);
    entity e = gen_find_tabulated(full_name, entity_domain);

    return(e);
}
*/

constant MakeConstantLitteral()
{
    return(make_constant(is_constant_litteral, NIL));
}

storage MakeStorageRom()
{
    return((make_storage(is_storage_rom, UU)));
}

value MakeValueUnknown()
{
    return(make_value(is_value_unknown, NIL));
}

/* returns a range expression containing e's i-th bounds */
expression entity_ith_bounds(e, i)
entity e;
int i;
{
    dimension d = entity_ith_dimension(e, i);
    syntax s = make_syntax(is_syntax_range,
                           make_range(gen_copy_tree(dimension_lower(d)),
                                      gen_copy_tree(dimension_upper(d)),
                                      make_expression_1()));
    return(make_expression(s, normalized_undefined));
}


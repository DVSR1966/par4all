 /* Interface between the untyped database manager and clean code and
  * between pipsmake and clean code.
  *
  * There are other interface routines in prettyprint.c
  *
  * The top routines should be called by the PIPS make utility. They might
  * eventually be integrated into it.
  *
  * The lower level routines are dealing with "statement_mapping"s. They
  * might have to be updated when mappings are integrated into NewGen.
  * At that time, the whole file should disappear in a typed pipsmake.
  *
  * Francois Irigoin, August 1990
  */

 /* the lowest level routines dealing with "statement_mapping"s are now 
  * generated by a macro defined in mapping.h : DEFINE_CURRENT_MAPPING.
  * see this file for more details.
  *
  * Be'atrice Apvrille, August 1993
  */

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "database.h"

#include "resources.h"
#include "ri.h"
#include "ri-util.h"
#include "pipsdbm.h"
#include "effects.h"

#include "misc.h"

#include "transformer.h"
#include "semantics.h"

#include "properties.h"

/* three mappings used throughout semantics analysis:
 *  - transformer_map is computed in a first phase
 *  - precondition_map is computed in a second phase
 *  - cumulated_effects is used and assumed computed before  (defined in effects.c)
 *  - proper_effects might be useless... only DO loop analysis could use it (idem)
 */


/* declaration of the previously described mappings with their access functions :
 *
 * (DEFINE_CURRENT_MAPPING is a macro defined in ~pips/Newgen/mapping.h)
 *
 * BA, August 26, 1993
 */

DEFINE_CURRENT_MAPPING(transformer, transformer)
DEFINE_CURRENT_MAPPING(precondition, transformer)
/* DEFINE_CURRENT_MAPPING(proper_effects, list)         a mettre dans les effects  */
/* DEFINE_CURRENT_MAPPING(cumulated_effects, list)     idem  */


/* Functions to make transformers */

bool transformers_intra_fast(module_name)
char * module_name;
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool transformers_intra_full(module_name)
char * module_name;
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool transformers_inter_fast(module_name)
char * module_name;
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool transformers_inter_full(module_name)
char * module_name;
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool summary_transformer(module_name)
char * module_name;
{
    /* there is a choice: do nothing and leave the effective computation
       in module_name_to_transformers, or move it here */

    return TRUE;
}

bool preconditions_intra(module_name)
char * module_name;
{
    /* nothing to do: transformers are preconditions for this
       intraprocedural option */

    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}

bool preconditions_inter_fast(module_name)
char * module_name;
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}

bool preconditions_inter_full(module_name)
char * module_name;
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}


bool old_summary_precondition(module_name)
char * module_name;
{
    /* do not nothing because it has been computed by side effects;
     * or provide an empty precondition for root modules;
     * maybe a touch to look nicer? 
     */

    transformer t;

    debug_on(SEMANTICS_DEBUG_LEVEL);

    debug(8, "summary_precondition", "begin\n");

    if(db_resource_p(DBR_SUMMARY_PRECONDITION, module_name)) {
	/* touch it */
	t = (transformer) db_get_memory_resource(DBR_SUMMARY_PRECONDITION,
						      module_name,
						      TRUE);
    }
    else {
	t = transformer_identity();
    }

    DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_PRECONDITION, 
			   strdup(module_name), (char * )t);

    ifdebug(8) {
	debug(8, "summary_precondition", 
	      "initial summary precondition %x for %s:\n",
	      t, module_name);
	dump_transformer(t);
	debug(8, "summary_precondition", "end\n");
    }

    debug_off();

    return TRUE;
}

bool summary_precondition(module_name)
char * module_name;
{
    /* Look for all call sites in the callers
     */
    callees callers = (callees) db_get_memory_resource(DBR_CALLERS,
						       module_name,
						       TRUE);
    entity callee = local_name_to_top_level_entity(module_name);
    /* transformer t = transformer_identity(); */
    transformer t = transformer_undefined;

    debug_on(SEMANTICS_DEBUG_LEVEL);

    set_current_module_entity(callee);

    ifdebug(1) {
	debug(1, "summary_precondition", "begin for %s with %d callers\n",
	      module_name,
	      gen_length(callees_callees(callers)));
	MAP(STRING, caller_name, {
	    (void) fprintf(stderr, "%s, ", caller_name);
	}, callees_callees(callers));
	    (void) fprintf(stderr, "\n");
    }

    reset_call_site_number();

    MAP(STRING, caller_name, {
	entity caller = local_name_to_top_level_entity(caller_name);
	t = update_precondition_with_call_site_preconditions(t, caller, callee);
    }, callees_callees(callers));

    if(transformer_undefined_p(t)) {
	t = transformer_identity();
    }
    else {
	/* try to eliminate redundancy */
	t = transformer_normalize(t, 2);
    }

    DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_PRECONDITION, 
			   strdup(module_name), (char * )t);

    ifdebug(1) {
	debug(1, "summary_precondition", 
	      "initial summary precondition %x for %s (%d call sites):\n",
	      t, module_name, get_call_site_number());
	dump_transformer(t);
	debug(1, "summary_precondition", "end\n");
    }

    reset_current_module_entity();
    debug_off();

    return TRUE;
}



/* resource module_name_to_transformers(char * module_name):
 * compute a transformer for each statement of a module with a given
 * name; compute also the global transformer for the module
 */
bool module_name_to_transformers(module_name)
char *module_name;
{
    transformer t_intra;
    transformer t_inter;
    list e_inter;

    debug_on(SEMANTICS_DEBUG_LEVEL);

    set_current_module_entity(local_name_to_top_level_entity(module_name));
    /* could be a gen_find_tabulated as well... */
    set_current_module_statement(
	(statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
    if( get_current_module_statement() == (statement) database_undefined )
	pips_error("module_name_to_transformers",
		   "no statement for module %s\n", module_name);

    set_proper_effects_map( effectsmap_to_listmap((statement_mapping) 
	db_get_memory_resource(DBR_PROPER_EFFECTS, module_name, TRUE)));

    set_cumulated_effects_map( effectsmap_to_listmap((statement_mapping) 
	db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE)));

    /* cumulated_effects_map_print(); */

    e_inter = effects_to_list( (effects)
	db_get_memory_resource(DBR_SUMMARY_EFFECTS, module_name, TRUE));

    set_transformer_map( MAKE_STATEMENT_MAPPING() ); 

    /* compute the basis related to module m */
    module_to_value_mappings( get_current_module_entity() );

    /* compute intraprocedural transformer */
    t_intra = statement_to_transformer( get_current_module_statement() );

    DB_PUT_MEMORY_RESOURCE(DBR_TRANSFORMERS, 
			   strdup(module_name), 
			   (char*) get_transformer_map() );  

    /* FI: side effect; compute and store the summary transformer, because
       every needed piece of data is available... */

    /* filter out local variables from the global intraprocedural effect */
    t_inter = transformer_intra_to_inter(t_intra, e_inter);
    t_inter = transformer_normalize(t_inter, 2);
    if(!transformer_consistency_p(t_inter)) {
	(void) print_transformer(t_inter);
	pips_error("module_name_to_transformers",
		   "Non-consistent summary transformer\n");
    }
    DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_TRANSFORMER, 
			   strdup(module_local_name( get_current_module_entity() )), 
			   (char*) t_inter);
    debug(8,"module_name_to_transformers","t_inter=%x\n", t_inter);

    reset_current_module_entity();
    reset_current_module_statement();
    /* Two auxiliary hash tables allocated by effectsmap_to_listmap() */
    free_proper_effects_map();
    free_cumulated_effects_map();
    reset_transformer_map();

    debug_off();

    return TRUE;
}


/* resource module_name_to_preconditions(char * module_name):
 * compute a transformer for each statement of a module with a given
 * name; compute also the global transformer for the module
 */
bool module_name_to_preconditions(module_name)
char *module_name;
{
    transformer t_inter;
    transformer pre;
    transformer post;

    /* set_debug_level(get_int_property(SEMANTICS_DEBUG_LEVEL)); */
     debug_on(SEMANTICS_DEBUG_LEVEL);

    set_current_module_entity( local_name_to_top_level_entity(module_name) );
    /* could be a gen_find_tabulated as well... */
    set_current_module_statement( 
	(statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
    if(get_current_module_statement() == (statement) database_undefined)
	pips_error("module_name_to_preconditions",
		   "no statement for module %s\n", module_name);

    /* cumulated effects are used to compute the value mappings */
    set_cumulated_effects_map( effectsmap_to_listmap((statement_mapping) 
	db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE)));

    set_transformer_map( (statement_mapping) 
	db_get_memory_resource(DBR_TRANSFORMERS, module_name, TRUE));


    /* p_inter is not used!!! FI, 9 February 1994 */
    /*
    if(get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
	p_inter = (transformer) 
	    db_get_memory_resource(DBR_SUMMARY_PRECONDITION,
				   module_name, TRUE);
    }
    */

    t_inter = (transformer) 
	db_get_memory_resource(DBR_SUMMARY_TRANSFORMER, module_name, TRUE);

    /* debug_on(SEMANTICS_DEBUG_LEVEL); */

    set_precondition_map( MAKE_STATEMENT_MAPPING() );  

    /* compute the mappings related to module m, that is likely to be
       unavailable during interprocedural analysis; a module reference
       should be kept with the mappings to avoid useless recomputation,
       allocation and frees, including those due to the prettyprinter */
    module_to_value_mappings( get_current_module_entity() );

    /* set the list of global values */
    set_module_global_arguments(transformer_arguments(t_inter));

    /* debug_on(SEMANTICS_DEBUG_LEVEL); */

    /* compute the module precondition: it should be passed as an
       additionnal parameter to module_to_preconditions by
       interprocedural analysis; for a main program and for modules called
       only once, data statements are exploited, although data
       statement are not dynamic */
    /* should be the dynamic call site count...*/
    /* if(call_site_count( get_current_module_entity() )<=1) */
    /* Let's assume static initializations (FI, 14 September 1993) */
    if(entity_main_module_p(get_current_module_entity()))
	pre = data_to_precondition( get_current_module_entity() );
    else
	pre = transformer_identity();

    /* interprocedural try for DRET demo - I'm not sure it's correct! 
       especially when there are DATA statements... */
    /* DRET demo: I cancel top-down propagation - missing binding
       take away FALSE in below test later, 23/4/90 */
    if(get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
	transformer ip = load_summary_precondition( get_current_module_entity() );
	if( ip == transformer_undefined) {
	    /* that might be because we are at the call tree root
	       or because no information is available;
	       maybe, every module precondition should be initialized
	       to a neutral value? */
	    user_warning("module_to_postcondition",
			 "no interprocedural module precondition for %s\n",
			 entity_local_name(get_current_module_entity() ));
	    ;
	}
	else {
	    /* convert global variables in the summary precondition in the local frame
	     * as defined by value mappings (FI, 31 January 1994) */
	    translate_global_values(get_current_module_entity(), ip);
	    ifdebug(8) {
		(void) fprintf(stderr,"module_name_to_preconditions\n");
		(void) fprintf(stderr,"\t summary_precondition %x after translation:\n",
			       (unsigned int) ip);
		(void) print_transformer(ip);
	    }
	    /* pre = ip o pre */
	    pre = transformer_combine(transformer_dup(ip), pre);
	    /* memory leak: the old pre should be freed */
	}
    }

    /* debug_on(SEMANTICS_DEBUG_LEVEL); */

    /* propagate the module precondition */
    post = statement_to_postcondition(pre, get_current_module_statement() );

    /* post could be stored in the ri for later interprocedural uses
       but the ri cannot be modified so early before the DRET demo;
       also our current interprocedural algorithm does not propagate
       postconditions upwards in the call tree */

    DB_PUT_MEMORY_RESOURCE(DBR_PRECONDITIONS, 
			   strdup(module_name), 
			   (char*) get_precondition_map() );

    debug(8, "module_name_to_preconditions",
	  "postcondition computed for %s\n", 
	  entity_local_name( get_current_module_entity() ));
    ifdebug(8) (void) print_transformer(post);
    debug(1, "module_to_preconditions", "end\n");

    reset_current_module_entity();
    reset_current_module_statement();
    reset_transformer_map();
    reset_precondition_map();
    /* auxiliary hash table allocated by effectsmap_to_listmap() */
    free_cumulated_effects_map();

    debug_off();

    return TRUE;
}


transformer load_summary_transformer(e)
entity e;
{
    /* FI: I had to add a guard db_resource_p() on Nov. 14, 1995.
     * I do not understand why the problem never occured before,
     * although it should each time the intra-procedural option
     * is selected.
     *
     * This may partially be explained because summary transformers
     * are implicitly computed with transformers instead of using
     * an explicit call to summary_transformer (I guess I'm going
     * to change that).
     *
     * I think it would be better not to call load_summary_transformer()
     * at all when no interprocedural options are selected. I should
     * change that too.
     */

    /* memoization could be used to improve efficiency */
    transformer t = transformer_undefined;

    pips_assert("load_summary_transformer", entity_module_p(e));

    if(db_resource_p(DBR_SUMMARY_TRANSFORMER, module_local_name(e))) {
	t = (transformer) 
	    db_get_memory_resource(DBR_SUMMARY_TRANSFORMER, 
				   entity_local_name(e), 
				   TRUE);

	/* db_get_memory_resource never returns database_undefined or
	   resource_undefined */
	pips_assert("load_summary_transformer", t != transformer_undefined);
    }
    else {
	t = transformer_undefined;
    }

    return t;
}


/* void update_summary_precondition(e, t): t is supposed to be a precondition
 * related to one of e's call sites and translated into e's basis; 
 *
 * the current global precondition for e is replaced by its
 * convex hull with t; 
 *
 * t may be slightly modified by transformer_convex_hull
 * because of bad design (FI)
 */
void update_summary_precondition(e, t)
entity e;
transformer t;
{
    transformer t_old = transformer_undefined;
    transformer t_new = transformer_undefined;

    pips_assert("update_summary_precondition", entity_module_p(e));

    debug(8, "update_summary_precondition", "begin\n");

    t_old = (transformer) 
	db_get_memory_resource(DBR_SUMMARY_PRECONDITION, module_local_name(e),
			       TRUE);

    ifdebug(8) {
	debug(8, "update_summary_precondition", " old precondition for %s:\n",
	      entity_local_name(e));
	print_transformer(t_old);
    }

    if(t_old == transformer_undefined)
	t_new = transformer_dup(t);
    else {
	t_new = transformer_convex_hull(t_old, t);
	transformer_free(t_old);
    }

    DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_PRECONDITION, 
			   strdup(module_local_name(e)), 
			   (char*) t_new );

    ifdebug(8) {
	debug(8, "update_summary_precondition", "new precondition for %s:\n",
	      entity_local_name(e));
	print_transformer(t_new);
	debug(8, "update_summary_precondition", "end\n");
    }
}

/* summary_preconditions are expressed in any possible frame, in fact the frame of
 * the last procedure that used/produced it
 */
transformer load_summary_precondition(e)
entity e;
{
    /* memoization could be used to improve efficiency */
    transformer t;

    pips_assert("load_summary_precondition", entity_module_p(e));

    debug(8, "load_summary_precondition", "begin\n for %s\n", 
	  module_local_name(e));

    t = (transformer) 
	db_get_memory_resource(DBR_SUMMARY_PRECONDITION, module_local_name(e), 
			       TRUE);

    pips_assert("load_summary_precondition", t != transformer_undefined);

    ifdebug(8) {
	debug(8, "load_summary_precondition", " precondition for %s:\n",
	      module_local_name(e));
	dump_transformer(t);
	debug(8, "load_summary_precondition", " end\n");
    }

    return t;
}


list load_summary_effects(e)
entity e;
{
    /* memorization could be used to improve efficiency */
    list t;

    pips_assert("load_summary_effects", entity_module_p(e));

    t = effects_to_list( (effects) 
	db_get_memory_resource(DBR_SUMMARY_EFFECTS, module_local_name(e), 
			       TRUE));

    pips_assert("load_summary_effects", t != list_undefined);

    return t;
}


list load_module_intraprocedural_effects(e)
entity e;
{
    /* memoization could be used to improve efficiency */
    list t;

    pips_assert("load_module_intraprocedural_effects", entity_module_p(e));
    pips_assert("load_module_intraprocedural_effects", 
		e == get_current_module_entity() );

    t = load_statement_cumulated_effects( get_current_module_statement() );

    pips_assert("load_module_intraprocedural_effects", t != list_undefined);

    return t;
}


/* ca n'a rien a` faire ici, et en plus, il serait plus inte'ressant d'avoir 
 * une fonction void statement_map_print(statement_mapping htp)
 */
void cumulated_effects_map_print()
{
    FILE * f =stderr;
    hash_table htp = get_cumulated_effects_map();

    hash_table_print_header (htp,f);

    HASH_MAP(k, v, {
	fprintf(f, "\n\n%d", statement_ordering((statement) k));
	print_effects((list) v);
    },
	     htp);
}

/*
 * $Id$
 *
 * $Log: initial.c,v $
 * Revision 1.10  1997/09/11 13:42:10  coelho
 * check consistency...
 *
 * Revision 1.9  1997/09/11 12:34:16  coelho
 * duplicates instead of relying on pipsmake/pipsdbm...
 *
 * Revision 1.8  1997/09/11 12:00:10  coelho
 * none.
 *
 * Revision 1.7  1997/09/10 09:43:30  irigoin
 * Explicit free_value_mappings() added. Context set for
 * program_precondition() and the prettyprinter modules.
 *
 * Revision 1.6  1997/09/09 11:03:15  coelho
 * initial preconditions should be ok.
 *
 * Revision 1.5  1997/09/08 17:52:05  coelho
 * some version for testing.
 *
 * Revision 1.4  1997/09/08 09:39:11  coelho
 * *** empty log message ***
 *
 * Revision 1.3  1997/09/08 09:35:29  coelho
 * transformer -> precondition.
 *
 * Revision 1.2  1997/09/08 08:51:14  coelho
 * the code is not printed. name fixed.
 *
 * Revision 1.1  1997/09/08 08:45:50  coelho
 * Initial revision
 *
 *
 * Computation of initial transformers that allow to collect
 * global initializations of BLOCK DATA and so.
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"

#include "ri.h"
#include "ri-util.h"

#include "database.h"
#include "pipsdbm.h"
#include "resources.h"

#include "misc.h"

#include "effects-generic.h"
#include "effects-simple.h"

#include "prettyprint.h"
#include "transformer.h"

#include "semantics.h"

/******************************************************************** UTILS */

static entity
get_main_entity(void)
{
    char *module_list[ARGS_LENGTH];
    int nmodules = 0, i;
    
    db_get_module_list(&nmodules, module_list);
    pips_assert("some modules in the program", nmodules>0);

    for (i=0; i<nmodules; i++)
    {
	entity m = local_name_to_top_level_entity(module_list[i]);
	if (entity_main_module_p(m))
	    return m;
    }

    /* ??? some default if there is no main... */
    pips_user_warning("no main found, returning %s instead\n", module_list[0]);
    return local_name_to_top_level_entity(module_list[0]);
}

/******************************************************** PIPSMAKE INTERFACE */

/* Compute an initial transformer.
 */
bool 
initial_precondition(string name)
{
    entity module = local_name_to_top_level_entity(name);
    transformer t;

    debug_on("SEMANTICS_DEBUG_LEVEL");

    set_current_module_entity(module);
    set_current_module_statement( (statement) 
	db_get_memory_resource(DBR_CODE, name, TRUE));
    set_cumulated_rw_effects((statement_effects) 
		   db_get_memory_resource(DBR_CUMULATED_EFFECTS, name, TRUE));
    module_to_value_mappings(module);

    t = all_data_to_precondition(module);

    ifdebug(1) 
	pips_assert("consistent initial precondition", 
		    transformer_consistency_p(t));

    DB_PUT_MEMORY_RESOURCE(DBR_INITIAL_PRECONDITION, strdup(name), (char*) t);

    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();

    free_value_mappings();

    debug_off();
    return TRUE;
}

/* returns t1 inter= t2;
 */
static void
intersect(
    transformer t1,
    transformer t2)
{
    predicate_system_(transformer_relation(t1)) = (char*)
	sc_append((Psysteme) predicate_system(transformer_relation(t1)),
		  (Psysteme) predicate_system(transformer_relation(t2)));
}

#define pred_debug(level, msg, trans) \
  ifdebug(level) { pips_debug(level, msg); dump_transformer(trans);}

/* Compute the union of all initial preconditions.
 */
bool
program_precondition(string name)
{
    transformer t = transformer_identity();
    int nmodules = 0, i;
    char * module_list[ARGS_LENGTH];
    entity the_main = get_main_entity();

    pips_assert("main was found", the_main!=entity_undefined);

    debug_on("SEMANTICS_DEBUG_LEVEL");
    pips_debug(1, "considering program \"%s\" with main \"%s\"\n", name,
	       module_local_name(the_main));

    set_current_module_entity(the_main);
    set_current_module_statement( (statement) 
	db_get_memory_resource(DBR_CODE,
			       module_local_name(the_main),
			       TRUE));
    set_cumulated_rw_effects((statement_effects) 
		   db_get_memory_resource(DBR_CUMULATED_EFFECTS,
					  module_local_name(the_main),
					  TRUE));
    module_to_value_mappings(the_main);
    
    db_get_module_list(&nmodules, module_list);
    pips_assert("some modules in the program", nmodules>0);

    for(i=0; i<nmodules; i++) 
    {
	transformer tm;
	pips_debug(1, "considering module %s\n", module_list[i]);
	
	tm = transformer_dup((transformer) /* no dup & FALSE => core */
	    db_get_memory_resource(DBR_INITIAL_PRECONDITION,  
				   module_list[i], TRUE));

	pred_debug(3, "current: t =\n", t);
	pred_debug(2, "to be added: tm =\n", tm);

	translate_global_values(the_main, tm); /* modifies tm! */

	pred_debug(3, "to be added after translation:\n", tm);

	intersect(t, tm); 

	free_transformer(tm);
    }

    pred_debug(1, "resulting program precondition:\n", t);

    ifdebug(1) 
	pips_assert("consistent program precondition", 
		    transformer_consistency_p(t));

    DB_PUT_MEMORY_RESOURCE(DBR_PROGRAM_PRECONDITION, strdup(name), t);

    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();

    free_value_mappings();

    debug_off();
    return TRUE;
}


/*********************************************************** PRETTY PRINTERS */

bool 
print_initial_precondition(string name)
{
    bool ok;
    entity module = local_name_to_top_level_entity(name);
    transformer t = (transformer) 
	db_get_memory_resource(DBR_INITIAL_PRECONDITION, name, TRUE);
    
    debug_on("SEMANTICS_DEBUG_LEVEL");

    set_current_module_entity(module);
    set_current_module_statement( (statement) 
	db_get_memory_resource(DBR_CODE, name, TRUE));
    set_cumulated_rw_effects((statement_effects) 
		   db_get_memory_resource(DBR_CUMULATED_EFFECTS,
					  name,
					  TRUE));
    module_to_value_mappings(module);
 
    ok = make_text_resource(name,
			    DBR_PRINTED_FILE,
			    ".ipred",
			    text_transformer(t));

    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();

    free_value_mappings();

    debug_off();

    return ok;
}

bool 
print_program_precondition(string name)
{
    bool ok;
    transformer t = (transformer) 
	db_get_memory_resource(DBR_PROGRAM_PRECONDITION, "", TRUE);
    entity m = get_main_entity();
    
    debug_on("SEMANTICS_DEBUG_LEVEL");
    pips_debug(1, "for \"%s\"\n", name);

    set_current_module_entity(m);
    set_current_module_statement( (statement) 
	db_get_memory_resource(DBR_CODE,
			       module_local_name(m),
			       TRUE));
    set_cumulated_rw_effects((statement_effects) 
		   db_get_memory_resource(DBR_CUMULATED_EFFECTS,
					  module_local_name(m),
					  TRUE));
    module_to_value_mappings(m);
 
    ok = make_text_resource(name,
			    DBR_PRINTED_FILE,
			    ".pipred",
			    text_transformer(t));

    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();

    free_value_mappings();

    debug_off();

    return ok;
}


/*
 * $Id$
 *
 * $Log: initial.c,v $
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
    int nmodules, i;
    
    db_get_module_list(&nmodules, module_list);
    pips_assert("some modules in the program", nmodules>0);

    for (i=0; i<nmodules; i++)
    {
	entity m = local_name_to_top_level_entity(module_list[i]);
	if (entity_main_module_p(m))
	    return m;
    }

    return entity_undefined;
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

    DB_PUT_MEMORY_RESOURCE(DBR_INITIAL_PRECONDITION, strdup(name), (char*) t);

    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();

    debug_off();
    return TRUE;
}

/* Compute the union of all initial preconditions.
 */
bool
program_precondition(string name)
{
    transformer t = transformer_identity();
    int nmodules = 0, i;
    char * module_list[ARGS_LENGTH];
    entity the_main = get_main_entity();

    debug_on("SEMANTICS_DEBUG_LEVEL");
    
    db_get_module_list(&nmodules, module_list);
    pips_assert("some modules in the program", nmodules>0);
    pips_assert("main was found", the_main!=entity_undefined);

    for(i=0; i<nmodules; i++) 
    {
	transformer tm, tn;
	pips_debug(1, "considering module %s\n", module_list[i]);
	
	tm = (transformer) 
	    db_get_memory_resource(DBR_INITIAL_PRECONDITION,  
				   module_list[i], FALSE);

	translate_global_values(the_main, tm);

	tn = transformer_convex_hull(t, tm); 

	free_transformer(t);
	free_transformer(tm);
	t = tn;
    }

    DB_PUT_MEMORY_RESOURCE(DBR_PROGRAM_PRECONDITION, strdup(name), t);

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
    
    set_current_module_entity(module);

    ok = make_text_resource(name,
			    DBR_PRINTED_FILE,
			    ".ipred",
			    text_transformer(t));

    reset_current_module_entity();

    return ok;
}

bool 
print_program_precondition(string name)
{
    bool ok;
    transformer t = (transformer) 
	db_get_memory_resource(DBR_PROGRAM_PRECONDITION, name, TRUE);
    entity m = get_main_entity();
    
    set_current_module_entity(m);

    ok = make_text_resource(name,
			    DBR_PRINTED_FILE,
			    ".pipred",
			    text_transformer(t));

    reset_current_module_entity();

    return ok;
}


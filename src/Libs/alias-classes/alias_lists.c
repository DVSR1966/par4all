#include <stdio.h>
#include <string.h>

#include <setjmp.h>

#include "genC.h"
#include "ri.h"
#include "database.h"

#include "ri-util.h"
#include "constants.h"
#include "control.h"
#include "misc.h"
#include "text.h"

#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "sommet.h"
#include "ray_dte.h"
#include "sg.h"
#include "polyedre.h"
#include "union.h"

#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"

#include "semantics.h"
#include "transformer.h"

#include "pipsdbm.h"
#include "resources.h"

#define BACKWARD TRUE
#define FORWARD FALSE

static list l_alias_lists = NIL;

/*
static bool
no_alias_for(region reg)
{
    list rest_alias_lists = l_alias_lists;
    list alias_list;
    region alias_reg;
    Psysteme reg_sys, alias_reg_sys;
    bool result = TRUE;

    if (l_alias_lists != NIL)
	do {
	    alias_list = LIST( CAR(rest_alias_lists) );
	    alias_reg = EFFECT( CAR(alias_list) );
	    if ( effects_same_action_p(reg,alias_reg) )
	    {
		reg_sys = region_system(reg);
		alias_reg_sys = region_system(alias_reg);
		if ( sc_equal_p_ofl(reg_sys,alias_reg_sys) )
		    result = FALSE;
	    }
	    rest_alias_lists = CDR(rest_alias_lists);
	} while (rest_alias_lists != NIL && result == TRUE);
    return result;
}
*/

/*
static void
make_alias_list_sub_region(region reg, string module_name)
{
    list alias_list, l_pairs;
*/
    /* put reg in list of one element for call to alias_pairs */
/*    l_pairs = alias_pairs( module_name, CONS(EFFECT,reg,NIL) );
 */
    /* turn list_pairs into an alias list */
/*    alias_list = CONS(EFFECT,reg,NIL);
    MAP(EFFECTS,alias_pair,
	{
	    alias_list = gen_nconc(alias_list,CDR(alias_pair));
	},
	l_pairs);
    l_alias_lists = gen_nconc(l_alias_lists,CONS(LIST,alias_list,NIL));
}
*/

/*
static void
make_alias_list_if_sub_region(region reg, string module_name)
{
    Psysteme reg_sys, alias_reg_sys;
    region alias_reg;
*/
/* if there is no alias for reg in this module */
/*    if ( no_alias_for(reg) )
    {
    */
/* for each alias_list=[alias_reg|list_trans_alias_reg] of this module,... */
/*	MAP(LIST,alias_list,
	    {
		alias_reg = EFFECT( CAR(alias_list) );
		*/
/* ... except for COMMON region alias_lists, do */
/*		if ( ! storage_ram_p(
		    entity_storage(region_entity(alias_reg))
		    ))
		{
		*/
/* see if reg is properly included in alias_reg */
/*		    if ( effects_same_action_p(alias_reg,reg) )
		    {
			reg_sys = region_system(reg);
			alias_reg_sys = region_system(alias_reg);
			if (sc_inclusion_p_ofl(reg_sys,alias_reg_sys) &&
			    ! sc_inclusion_p_ofl(alias_reg_sys,reg_sys) )
			    */
/* and, if so, add alias list for reg to this module */
/*			    make_alias_list_sub_region(reg,module_name);

		    }
		}
	    },
		l_alias_lists); 
    }
}
*/

/*
static void
make_alias_lists_for_sub_regions(string module_name)
{
    entity module_entity = local_name_to_top_level_entity(module_name);
    callees module_callees;
    list callee_alias_lists;
*/
    /* we need the callees of the current module  */
/*    module_callees = (callees) db_get_memory_resource(DBR_CALLEES,
					       module_name,
					       TRUE);
					       */
    /* for each callee do */
/*    MAP(STRING, callee_name,
	{
	callee_alias_lists = (list) db_get_memory_resource(DBR_ALIAS_LISTS,
					  callee_name,
					  TRUE);
					  */
	/* for each alias list do */
/*	MAP(EFFECTS, alias_list_effects,
	    {
                list callee_alias_list = regions_dup(effects_to_list(alias_list_effects));
*/
/* don't treat COMMON regions */
/*		if ( ! storage_ram_p(
		    entity_storage(region_entity(EFFECT(CAR(callee_alias_list))))
		    ))
		{
*/

/* for any alias in this module do */
/*		    MAP(EFFECT, trans_reg,
			{
			    if ( module_entity == region_entity(trans_reg) )
*/

/* if it is a sub-region of an IN or OUT region of this module */
/* then make an alias list for it in this module */
/*				make_alias_list_if_sub_region(trans_reg, module_name);
			},
			    CDR(callee_alias_list));
		}	    
	    },
	    callee_alias_lists);
	},
	callees_callees(module_callees));
}
*/

/* tests if reg1 and reg2 are the same,
 * ignoring their action_tags (read/write)
 */
static bool
same_reg(region reg1, region reg2)
    {
    Psysteme reg1_sys, reg2_sys;
    bool result = FALSE;

    pips_debug(4,"begin\n");

    if (effect_undefined_p(reg1) || effect_undefined_p(reg2))
	return result;

	if (effect_entity(reg1) == effect_entity(reg2))
	    {
		reg1_sys = region_system(reg1);
		reg2_sys = region_system(reg2);
		if ( sc_equal_p_ofl(reg1_sys,reg2_sys) )
		{
		    result = TRUE;

		    pips_debug(4,"same region\n");
		}
	    }
	pips_debug(4,"end\n");

	return result;
    }


/* tests if reg and any member of reg_list
 * are same_reg
 */
static bool
member(region reg, list reg_list)
    {
	region elem;
	list rest_list;
	bool result = FALSE;

	pips_debug(4,"begin\n");

	rest_list = reg_list;

	if (reg_list != NIL)

			do{
			    elem = EFFECT(CAR(reg_list));
			    if (same_reg(elem,reg))
				{
				    result = TRUE;

				    pips_debug(4,"is member\n");
				}

			    rest_list = CDR(rest_list);
			}while (rest_list != NIL && result == FALSE);

	pips_debug(4,"end\n");

	return result;
    }


static void
add_if_not_present(region reg, list reg_list)
    {
	if (!member(reg,reg_list))
	    reg_list = gen_nconc(reg_list,
				 CONS(EFFECT,region_dup(reg),NIL));
    }


static bool
add_pair_to_existing_list(list alias_pair)
{
    list rest_alias_lists, alias_list;
    region formal_reg_pair, formal_reg_list, actual_reg_pair;
    bool result = FALSE;

    pips_debug(4,"begin\n");

    formal_reg_pair = EFFECT( CAR(alias_pair) );

    ifdebug(9)
	{
	    set_action_interpretation(ACTION_IN,ACTION_OUT);
	    pips_debug(9,"compare:\n\t");
	    print_region(formal_reg_pair);
	}
    
    rest_alias_lists = l_alias_lists;
    if (l_alias_lists != NIL)
	do {
	    alias_list = LIST( CAR(rest_alias_lists) );
	    formal_reg_list = EFFECT( CAR(alias_list) );

	    ifdebug(9)
		{
		    pips_debug(9,"with:\n\t");
		    print_region(formal_reg_list);
		}

	    if ( same_reg(formal_reg_pair,formal_reg_list) )
	    {
		    result = TRUE;
		    actual_reg_pair = EFFECT(CAR(CDR(alias_pair)));
		    ifdebug(9)
			{
			    pips_debug(9,"add region:\n\t");
			    print_region(actual_reg_pair);
			}
		    add_if_not_present(actual_reg_pair,alias_list);
		}
	    rest_alias_lists = CDR(rest_alias_lists);
	} while (rest_alias_lists != NIL && result == FALSE);

    ifdebug(9) 
	{
	    reset_action_interpretation();
	}
    pips_debug(4,"end\n");

    return result;
}


bool
alias_lists( string module_name )
    {
    list in_alias_pairs, out_alias_pairs;
    entity module;

    l_alias_lists = NIL;

    pips_debug(4,"begin for module %s\n",module_name);

    ifdebug(9)
	{
	    /* ATTENTION: we have to do ALL this
	     * just to call print_inout_regions for debug !!
	     */
	    set_current_module_entity(
		local_name_to_top_level_entity(module_name) );
	    module = get_current_module_entity();
	    set_current_module_statement( (statement)
					  db_get_memory_resource(DBR_CODE,
								 module_name,
								 TRUE) );
	    set_cumulated_rw_effects((statement_effects)
				     db_get_memory_resource(
					 DBR_CUMULATED_EFFECTS,
					 module_name,
					 TRUE));
	    module_to_value_mappings(module);
	    /* that's it, but we musn't forget to reset everything below */
	}

    /* make alias lists from the IN_alias_pairs */
    in_alias_pairs = effects_to_list((effects)
				    db_get_memory_resource(DBR_IN_ALIAS_PAIRS,
					    module_name,
					    TRUE));
    MAP(EFFECTS, alias_pair_effects,
	{
	    list alias_pair = regions_dup(effects_to_list(alias_pair_effects));

	    ifdebug(9)
		{
		    pips_debug(9,"IN alias pair: \n");
		    print_inout_regions(alias_pair);
		}

	    if ( ! add_pair_to_existing_list(alias_pair) )
	    l_alias_lists = gen_nconc(l_alias_lists,regions_dup(alias_pair));

	    pips_debug(9,"IN pair added\n");
		},
	in_alias_pairs);

    /* make alias lists from the OUT_alias_pairs */
    out_alias_pairs = effects_to_list((effects)
				    db_get_memory_resource(DBR_OUT_ALIAS_PAIRS,
					    module_name,
					    TRUE));
    MAP(EFFECTS, alias_pair_effects,
	{
	    list alias_pair = regions_dup(effects_to_list(alias_pair_effects));

	    ifdebug(9)
		{
		    pips_debug(9,"OUT alias pair: \n");
		    print_inout_regions(alias_pair);
		}

	    if ( ! add_pair_to_existing_list(alias_pair) )
	    l_alias_lists = gen_nconc(l_alias_lists,CONS(LIST,alias_pair,NIL));

	    pips_debug(9,"OUT pair added\n");
	},
	out_alias_pairs);

    /* check all callees for sub-regions of existing aliases */
/*    make_alias_lists_for_sub_regions(module_name); */

    DB_PUT_MEMORY_RESOURCE(DBR_ALIAS_LISTS, 
			   strdup(module_name),
			   (char*) make_effects_classes(l_alias_lists));    

    ifdebug(9)
	{
	    free_value_mappings();
	    reset_current_module_statement();
	    reset_cumulated_rw_effects();
	    reset_current_module_entity();
	}
    pips_debug(4,"end\n");

    return(TRUE);
}


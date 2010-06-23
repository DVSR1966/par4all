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
/*
 * pipsmake: call by need (make),
 * rule selection (activate),
 * explicit call (apply/capply)
 *
 * Remi Triolet, Francois Irigoin, Pierre Jouvelot, Bruno Baron,
 * Arnauld Leservot, Guillaume Oget, Fabien Coelho.
 *
 * Notes: 
 *  - pismake uses some RI fields explicitly
 *  - see Bruno Baron's DEA thesis for more details
 *  - do not forget the difference between *virtual* resources like 
 *    CALLERS.CODE and *real* resources like FOO.CODE; CALLERS is a 
 *    variable (or a function) whose value depends on the current module; 
 *    it is expanded into a list of real resources;
 *    the variables are CALLEES, CALLERS, ALL and MODULE (the current module
 *    itself);
 *    these variables are used to implement top-down and bottom-up traversals
 *    of the call tree; they make pipsmake different from 
 *
 *  - memoization added to make() to speed-up a sequence of interprocedural 
 *  requests on real applications; a resource r is up-to-date if it already 
 *  has been
 *  proved up-to-date, or if all its arguments have been proved up-to-date and
 *  all its arguments are in the database and all its arguments are
 *  older than the requested resource r; this scheme is correct as soon as 
 *  activate()
 *  destroys the resources produced by the activated (and de-activated) rule
 *  - include of an automatically generated builder_map
 *  - explicit *recursive* destruction of obsolete resources by
 *  activate() but not by apply(); beware! You cannot assume that all
 *  resources in the database are consistent;
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "genC.h"

#include "linear.h"
#include "ri.h"
#include "constants.h"
#include "database.h"

#include "misc.h"

#include "ri-util.h"
#include "pipsdbm.h"
#include "resources.h"
#include "phases.h"
#include "builder_map.h"
#include "properties.h"

#include "pipsmake.h"

static bool catch_user_error(bool (*f)(const char *), const char* rname, const char* oname)
{
    bool success = FALSE;

    CATCH(any_exception_error)
    {
      reset_static_phase_variables();
      success = FALSE;
    }
    TRY
    {
      set_pips_current_computation(rname, oname);
      success = (*f)(oname);
      UNCATCH(any_exception_error);
    }

    reset_pips_current_computation();

    return success;
}

static bool (*get_builder(const char* name))(const char *)
{
    struct builder_map * pbm;
    for (pbm = builder_maps; pbm->builder_name; pbm++)
	if (same_string_p(pbm->builder_name, name))
	    return pbm->builder_func;
    pips_internal_error("no builder for %s\n", name);
    return NULL;
}

/*********************************************** UP TO DATE RESOURCES CACHE */

/* FI: make is very slow when interprocedural analyzes have been selected;
 * some memorization has been added; we need to distinguish betweeen an
 * external make which initializes a set of up-to-date resources and
 * an internal recursive make which updates and exploits that set.
 *
 * This new functionality is extremely useful when old databases
 * are re-opened.
 *
 * apply(), which calls make() many times, does not fully benefit from
 * this memoization scheme.
 */
static set up_to_date_resources = set_undefined;

void reset_make_cache(void)
{
    pips_assert("set is defined", !set_undefined_p(up_to_date_resources));
    set_free(up_to_date_resources);
    up_to_date_resources = set_undefined;
}

void init_make_cache(void)
{
    pips_assert("not set", set_undefined_p(up_to_date_resources));
    up_to_date_resources = set_make(set_pointer);
}

void reinit_make_cache_if_necessary(void)
{
    if (!set_undefined_p(up_to_date_resources))
	reset_make_cache(), init_make_cache();
}

/* Static variables used by phases must be reset on error although
   pipsmake does not know which ones are used. */
void reset_static_phase_variables()
{
#define DECLARE_ERROR_HANDLER(name) extern void \
    name(); name()

    /* From ri-util/static.c */
    DECLARE_ERROR_HANDLER(error_reset_current_module_entity);
    DECLARE_ERROR_HANDLER(error_reset_current_module_statement);

    /* Macro-generated resets */
    DECLARE_ERROR_HANDLER(error_reset_rw_effects);
    DECLARE_ERROR_HANDLER(error_reset_invariant_rw_effects);
    DECLARE_ERROR_HANDLER(error_reset_proper_rw_effects);
    DECLARE_ERROR_HANDLER(error_reset_cumulated_rw_effects);
    DECLARE_ERROR_HANDLER(reset_transformer_map);
    DECLARE_ERROR_HANDLER(icfg_error_handler);

    /* Macro-generated resets in effects-generic/utils.c */
    DECLARE_ERROR_HANDLER(proper_effects_error_handler);

    /* Error handlers for the transformation library */
    DECLARE_ERROR_HANDLER(use_def_elimination_error_handler);
    DECLARE_ERROR_HANDLER(simple_atomize_error_handler);
    DECLARE_ERROR_HANDLER(clone_error_handler);
    DECLARE_ERROR_HANDLER(array_privatization_error_handler);

    DECLARE_ERROR_HANDLER(hpfc_error_handler);

    /* Special cases: Transformers or preconditions are computed or used */
    DECLARE_ERROR_HANDLER(error_reset_value_mappings);
#undef DECLARE_ERROR_HANDLER
}

/* Apply an instantiated rule with a given ressource owner 
 */

/* FI: uncomment if rmake no longer needed in callgraph.c */
/* static bool rmake(string, string); */

#define add_res(vrn, on)					\
  result = CONS(REAL_RESOURCE, \
		make_real_resource(strdup(vrn), strdup(on)), result);

/* Logically, this should be implemented in preprocessor, but the
   preprocessor library is at a upper level than the pipsmake
   library...

   The output is undefined if the module is referenced but not defined in
   the workspace, for instance because its code should be synthetized.

   Fabien Coelho suggests to build a defaut compilation unit where all
   synthesized module codes would be located.

  */
string compilation_unit_of_module(const char* module_name)
{
  /* Should only be called for C modules. */
  string compilation_unit_name = string_undefined;

  /* The guard may not be sufficient and this may crash in db_get_memory_resource() */
  if(db_resource_p(DBR_USER_FILE, module_name)) {
    string source_file_name = db_get_memory_resource(DBR_USER_FILE, module_name, TRUE);
    string simpler_file_name = pips_basename(source_file_name, PP_C_ED);

    /* It is not clear how robust it is going to be when file name conflicts occur. */
    asprintf(&compilation_unit_name,"%s" FILE_SEP_STRING,simpler_file_name);
    free(simpler_file_name);
  }

  return compilation_unit_name;
}

/* Translate and expand a list of virtual resources into a potentially
 * much longer list of real resources
 *
 * this is intrinsically a bad idea: if a new module is created as
 * a side effect of some processing, then the dependency on this new module
 * will never appear and cannot be checked for a redo here (see comments
 * of is_owner_all case).
 *
 * In spite of the name, no resource is actually built.
 */
static list build_real_resources(const char* oname, list lvr)
{
    list pvr, result = NIL;

    for (pvr = lvr; pvr != NIL; pvr = CDR(pvr))
    {
	virtual_resource vr = VIRTUAL_RESOURCE(CAR(pvr));
	string vrn = virtual_resource_name(vr);
	tag vrt = owner_tag(virtual_resource_owner(vr));

	switch (vrt)
	{
	    /* FI: should be is_owner_workspace, but changing Newgen decl... */
	case is_owner_program:
	    /* FI: for  relocation of workspaces */
	    add_res(vrn, "");
	    break;

	case is_owner_module:
	    add_res(vrn, oname);
	    break;

	case is_owner_main:
	{
	    int number_of_main = 0;
	    gen_array_t a = db_get_module_list();

	    GEN_ARRAY_MAP(on,
	    {
		if (entity_main_module_p
		    (local_name_to_top_level_entity(on)) == TRUE)
		{
		    if (number_of_main)
			pips_internal_error("More than one main\n");

		    number_of_main++;
		    pips_debug(8, "Main is %s\n", (string) on);
		    add_res(vrn, on);
		}
	    },
		a);

	    gen_array_full_free(a);
	    break;
	}
	case is_owner_callees:
	{
	    callees called_modules;
	    list lcallees;

	    if (!rmake(DBR_CALLEES, oname)) {
		/* FI: probably missing source code... */
		pips_user_error("unable to build callees for %s\n"
				"Some source code probably is missing!\n",
				 oname);
	    }

	    called_modules = (callees)
		db_get_memory_resource(DBR_CALLEES, oname, TRUE);
	    lcallees = gen_copy_string_list(callees_callees(called_modules));

	    pips_debug(8, "Callees of %s are:\n", oname);
	    MAP(STRING, on,
	    {
		pips_debug(8, "\t%s\n", on);
		add_res(vrn, on);
	    },
		lcallees);
	    gen_free_string_list(lcallees);

	    break;
	}
	case is_owner_callers:
	{
	    /* FI: the keyword callees was badly chosen; anyway, it's just
	       a list of strings... see ri.newgen */
	    callees caller_modules;
	    list lcallers;

	    if (!rmake(DBR_CALLERS, oname)) {
		user_error ("build_real_resources",
			    "unable to build callers for %s\n"
			    "Any missing source code?\n",
			    oname);
	    }

	    caller_modules = (callees)
		db_get_memory_resource(DBR_CALLERS, oname, TRUE);
	    lcallers = gen_copy_string_list(callees_callees(caller_modules));

	    pips_debug(8, "Callers of %s are:\n", oname);

	    MAP(STRING, on,
	    {
		pips_debug(8, "\t%s\n", on);
		add_res(vrn, on);
	    },
		lcallers);
	    gen_free_string_list(lcallers);
	    break;
	}

	case is_owner_all:
	{
	    /* some funny stuff here:
	     * some modules may be added by the phases here...
	     * then we might expect a later coredump if the new resource 
	     * is not found.
	     */
	    gen_array_t modules = db_get_module_list();

	    GEN_ARRAY_MAP(on, 
	    {
		pips_debug(8, "\t%s\n", (string) on);
		add_res(vrn, on);
	    },
		modules);

	    gen_array_full_free(modules);
	    break;
	}

	case is_owner_select:
	{
	    /* do nothing ... */
	    break;
	}

	case is_owner_compilation_unit:
	  {
	    string compilation_unit_name = compilation_unit_of_module(oname);

	    if(string_undefined_p(compilation_unit_name)) {
	      /* Source code for module oname is not available */
	      if(compilation_unit_p(oname)) {
		/* The user can make typos in tpips scripts about
		   compilation unit names */
		/* pips_internal_error("Synthetic compilation units cannot be missing"
				    " because they are synthesized"
				    " with the corresponding file\n",
				    oname);
		*/
		pips_user_error("No source code for compilation unit \"%s\"\n."
				"Compilation units cannot be synthesized.\n",
				oname);
	      }
	      pips_user_warning("No source code for module %s.\n", oname);
	      compilation_unit_name = strdup(concatenate(oname, FILE_SEP_STRING, NULL));
	    }

	    add_res(vrn, compilation_unit_name);
	    free(compilation_unit_name);
	    break;
	  }

	default:
	    pips_internal_error("unknown tag : %d\n", vrt);
	}
    }

    return gen_nreverse(result);
}

static void update_preserved_resources(const char* oname, rule ru)
{
    list reals;

    /* We increment the logical time (kept by pipsdbm) */
    db_inc_logical_time();

    /* we build the list of modified real_resources */
    reals = build_real_resources(oname, rule_modified(ru));

    /* we delete them from the uptodate set */
    MAP(REAL_RESOURCE, rr, {
	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);

	/* is it up to date ? */
	if(set_belong_p(up_to_date_resources, (char *) rr))
	{
	    pips_debug(3, "resource %s(%s) deleted from up_to_date\n",
		       rrrn, rron);
	    set_del_element (up_to_date_resources,
			     up_to_date_resources,
			     (char *) rr);
	    /* GO 11/7/95: we need to del the resource from the data base
	       for a next call of pipsmake to find it unavailable */
	    db_unput_a_resource (rrrn, rron);
	}
    }, reals);

    gen_full_free_list (reals);
}

static bool apply_a_rule(const char* oname, rule ru)
{
    static int number_of_applications_of_a_rule = 0;
    static bool checkpoint_workspace_being_done = FALSE;

    double initial_memory_size = 0.;
    string run = rule_phase(ru), rname, rowner;
    bool first_time = TRUE, success_p = TRUE,
	 print_timing_p = get_bool_property("LOG_TIMINGS"),
	 print_memory_usage_p = get_bool_property("LOG_MEMORY_USAGE"),
	 check_res_use_p = get_bool_property("CHECK_RESOURCE_USAGE");
    bool (*builder) (const char*) = get_builder(run);
    int frequency = get_int_property("PIPSMAKE_CHECKPOINTS");
    list lrp;

    /* periodically checkpoints the workspace if required.
     */
    number_of_applications_of_a_rule++;

    if (!checkpoint_workspace_being_done &&
	frequency>0 && number_of_applications_of_a_rule>=frequency)
    {
      /* ??? FC 05/04/2002 quick fix because of a recursion loop:
	 apply_a_rule -> checkpoint_workspace -> delete_obsolete_resources ->
	 check_physical_resource_up_to_date -> build_real_resources -> rmake ->
	 apply_a_rule ! 
	 * maybe it would be better treater in checkpoint_workspace?
      */
      checkpoint_workspace_being_done = TRUE;
      checkpoint_workspace();
      checkpoint_workspace_being_done = FALSE;
      number_of_applications_of_a_rule = 0;
    }

    /* output the message somewhere...
     */
    lrp = build_real_resources(oname, rule_produced(ru));
    MAP(REAL_RESOURCE, rr, 
    {
	list lr = build_real_resources(oname, rule_required(ru));
	bool is_required = FALSE;
	rname = real_resource_resource_name(rr);
	rowner = real_resource_owner_name(rr);

	MAP(REAL_RESOURCE, rrr, 
	{
	    if (same_string_p(rname, real_resource_resource_name(rrr)) &&
		same_string_p(rowner, real_resource_owner_name(rrr)))
	    {
		is_required = TRUE;
		break;
	    }
	}, 
	    lr);

	gen_full_free_list(lr);

	user_log("  %-30.60s %8s   %s(%s)\n", 
		 first_time == TRUE ? (first_time = FALSE,run) : "",
		 is_required == TRUE ? "updating" : "building",
		 rname, rowner);
    }, 
	lrp);

    gen_full_free_list(lrp);

    if (check_res_use_p)
	init_resource_usage_check();
    
    if (print_timing_p)
	init_log_timers();
    
    if (print_memory_usage_p) 
	initial_memory_size = get_process_gross_heap_size();

    /* DO IT HERE!
     */
    success_p = catch_user_error(builder, run, oname);

    if (print_timing_p) {
	string time_with_io,io_time;
	
	get_string_timers (&time_with_io, &io_time);
	
	user_log ("                                 time       ");
	user_log (time_with_io);
	user_log ("                                 IO time    ");
	user_log (io_time);
    }

    if (print_memory_usage_p) {
	double final_memory_size = get_process_gross_heap_size();
	user_log("\t\t\t\t memory size %10.3f, increase %10.3f\n",
		 final_memory_size,
		 final_memory_size-initial_memory_size);
    }
    
    if (check_res_use_p)
	do_resource_usage_check(oname, ru);
    
    pips_malloc_debug();
    
    update_preserved_resources(oname, ru);

    if (run_pipsmake_callback() == FALSE)
	return FALSE;
    
    if (interrupt_pipsmake_asap_p())
	return FALSE;
    
    return success_p;
}


/* This function returns the active rule to produce resource rname. It
   selects the first active rule in the database which produces the
   resource but does not use/require it.  */
rule find_rule_by_resource(const char* rname)
{
    makefile m = parse_makefile();

    pips_debug(5, "searching rule for resource %s\n", rname);

    /* walking thru rules */
    MAP(RULE, r, {
	bool resource_required_p = FALSE;

	/* walking thru resources required by this rule to eliminate rules
           using and producing this resource, e.g. code transformations
           for the CODE resource. */
	MAP(VIRTUAL_RESOURCE, vr,
	{
	    string vrn = virtual_resource_name(vr);
	    owner vro = virtual_resource_owner(vr);

	    /* We do not check callers and callees */
	    if ( owner_callers_p(vro) || owner_callees_p(vro) ) {}
	    /* Is this resource required ?? */
	    else if (same_string_p(vrn, rname))
		resource_required_p = TRUE;

	}, rule_required(r));

	/* If this particular resource is not required by the current rule. */
	if (!resource_required_p) {
	    /* walking thru resources made by this particular rule */
	    MAP(VIRTUAL_RESOURCE, vr, {
		string vrn = virtual_resource_name(vr);

		if (same_string_p(vrn, rname)) {

		    pips_debug(5, "made by phase %s\n", rule_phase(r));

		    /* Is this phase an active one ? */
		    MAP(STRING, pps, {
			if (same_string_p(pps, rule_phase(r))) {
			    pips_debug(5, "active phase\n");
			    return(r);
			}
		    }, makefile_active_phases(m));

		    pips_debug(5, "inactive phase\n");
		}
	    }, rule_produced(r));
	}
    }, makefile_rules(m));

    return(rule_undefined);
}

/* Always returns a defined rule */
static rule safe_find_rule_by_resource(const char* rname)
{
  rule ru = rule_undefined;

  if ((ru = find_rule_by_resource(rname)) == rule_undefined) {
    /* else */
    pips_internal_error("could not find a rule for %s\n", rname);
  }

  return ru;
}

static bool make_pre_transformation(const char*, rule);
static bool make_required(const char*, rule);

/* Apply do NOT activate the rule applied. 
 * In the case of an interprocedural rule, the rules applied to the
 * callees of the main will be the default rules. For instance,
 * "apply PRINT_CALL_GRAPH_WITH_TRANSFORMERS" applies the rule
 * PRINT_CALL_GRAPH to all callees of the main, leading to a core
 * dump. 
 * Safe apply checks if the rule applied is activated and produces ressources 
 * that it requires (no transitive closure) --DB 8/96
 */
static bool apply_without_reseting_up_to_date_resources(
    const char* pname, 
    const char* oname)
{
    rule ru;

    pips_debug(2, "apply %s on %s\n", pname, oname);

    /* we look for the rule describing this phase 
     */
    if ((ru = find_rule_by_phase(pname)) == rule_undefined) {
	pips_user_warning("could not find rule %s\n", pname);
	return FALSE;
    }

    if (!make_pre_transformation(oname, ru))
	return FALSE;

    if (!make_required(oname, ru))
	return FALSE;

    return apply_a_rule(oname, ru);
}

/* compute all pre-transformations to apply a rule on an object 
 */
static bool make_pre_transformation(const char* oname, rule ru)
{
    list reals;
    bool success_p = TRUE;

    /* we select some resources */
    MAP(VIRTUAL_RESOURCE, vr, 
    {
	string vrn = virtual_resource_name(vr);
	tag vrt = owner_tag(virtual_resource_owner(vr));
	
	if (vrt == is_owner_select) {
	    
	    pips_debug(3, "rule %s : selecting phase %s\n",
		       rule_phase(ru), vrn);
	    
	    if (activate (vrn) == NULL) {
		success_p = FALSE;
		break;
	    }
	}
    }, rule_pre_transformation(ru));
    
    if (success_p) {
	/* we build the list of pre transformation real_resources */
	reals = build_real_resources(oname, rule_pre_transformation(ru));
	
	/* we recursively make the resources */
	MAP(REAL_RESOURCE, rr, {
	    string rron = real_resource_owner_name(rr);
	    /* actually the resource name is a phase name !! */
	    string rrpn = real_resource_resource_name(rr);
	    
	    pips_debug(3, "rule %s : applying %s to %s - recursive call\n",
		       rule_phase(ru), rrpn, rron);
	    
	    if (!apply_without_reseting_up_to_date_resources (rrpn, rron))
		success_p = FALSE;
	    
	    /* now we must drop the up_to_date cache.
	     * maybe not that often? Or one should perform the transforms
	     * Top-down to avoid recomputations, with ALL...
	     */
	    reset_make_cache();
	    init_make_cache();
	}, reals);
    }
    return TRUE;
}

static bool make(const char* rname, const char* oname)
{
    bool success_p = TRUE;

    debug(1, "make", "%s(%s) - requested\n", rname, oname);

    init_make_cache();

    dont_interrupt_pipsmake_asap();
    save_active_phases();
    ifdebug(5)
      db_print_all_required_resources(stderr);

    success_p = rmake(rname, oname);

    reset_make_cache();
    retrieve_active_phases();
    db_clean_all_required_resources();

    pips_debug(1, "%s(%s) - %smade\n", 
	       rname, oname, success_p? "": "could not be ");

    return success_p;
}

/* recursive make resource. Should be static, but FI needs it from callgraph.c */
bool rmake(const char* rname, const char* oname)
{
    rule ru;
    char * res = NULL;

    debug(2, "rmake", "%s(%s) - requested\n", rname, oname);

    /* is it up to date ? */
    if (db_resource_p(rname, oname)) 
    {
	res = db_get_resource_id(rname, oname);
	if(set_belong_p(up_to_date_resources, (char *) res)) 
	{
	  pips_debug(5, "resource %s(%s) found up_to_date, time stamp %d\n",
		     rname, oname, db_time_of_resource(rname, oname));
	  return TRUE; /* YES, IT IS! */
	}
	else
	{
	  /* this resource exists but is maybe up-to-date? */
	  res = NULL; /* NO, IT IS NOT. */
	}
    }
    else if (db_resource_is_required_p(rname, oname))
    {
      /* the resource is already being required... this is bad */
      db_print_all_required_resources(stderr); 
      pips_user_error("recursion on resource %s of %s\n", rname, oname);
    }
    else
    {
      /* well, the resource does not exists, we have to build it */
       db_set_resource_as_required(rname, oname);
    }
    
    /* we look for the active rule to produce this resource */
    if ((ru = find_rule_by_resource(rname)) == rule_undefined)
	pips_internal_error("could not find a rule for %s\n", rname);

    /* we recursively make the pre transformations. */
    if (!make_pre_transformation(oname, ru))
	return FALSE;

    /* we recursively make required resources. */
    if (!make_required(oname, ru))
	return FALSE;

    if (check_resource_up_to_date (rname, oname)) 
    {
      pips_debug(8, 
		 "Resource %s(%s) becomes up-to-date after applying\n"
		 "  pre-transformations and building required resources\n",
		  rname,oname);
    } 
    else 
    {
      bool success = FALSE;
      list lr;
      
      /* we build the resource */
      db_set_resource_as_required(rname, oname);

      success = apply_a_rule(oname, ru);
      if (!success) return FALSE;
      
      lr = build_real_resources(oname, rule_produced(ru));
      
      /* set up-to-date all the produced resources for that rule */
      MAP(REAL_RESOURCE, rr,
      {
	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);
	
	if (db_resource_p(rrrn, rron)) 
	{
	  res = db_get_resource_id(rrrn, rron);
	  pips_debug(5, "resource %s(%s) added to up_to_date "
		     "with time stamp %d\n",
		     rrrn, rron, db_time_of_resource(rrrn, rron));
	  set_add_element(up_to_date_resources, 
			  up_to_date_resources, res);
	}
	else {
	  pips_internal_error("resource %s[%s] just built not found!\n",
			      rrrn, rron);
	}
      }, 
	  lr);
      
      gen_full_free_list(lr);
    }
    
    return TRUE;
}


static bool apply(const char* pname, const char* oname)
{
    bool success_p = TRUE;

    pips_debug(1, "%s.%s - requested\n", oname, pname);

    init_make_cache();
    dont_interrupt_pipsmake_asap();
    save_active_phases();

    success_p = apply_without_reseting_up_to_date_resources(pname, oname);

    reset_make_cache();
    retrieve_active_phases();

    pips_debug(1, "%s.%s - done\n", oname, pname);
    return success_p;
}


static bool concurrent_apply(
    const char* pname,       /* phase to be applied */
    gen_array_t modules /* modules that must be computed */)
{
    bool okay = TRUE;
    rule ru = find_rule_by_phase(pname);

    init_make_cache();
    dont_interrupt_pipsmake_asap();
    save_active_phases();

    GEN_ARRAY_MAP(oname,
		  if (!make_pre_transformation(oname, ru)) {
		    okay = FALSE;
		    break;
		  },
		  modules);

    if (okay) {
	GEN_ARRAY_MAP(oname,
		      if (!make_required(oname, ru)) {
			okay = FALSE;
			break;
		      },
		      modules);
    }

    if (okay) {
	GEN_ARRAY_MAP(oname,
		      if (!apply_a_rule(oname, ru)) {
			okay = FALSE;
			break;
		      },
		      modules);
    }

    reset_make_cache();
    retrieve_active_phases();
    return okay;
}

/* compute all resources needed to apply a rule on an object */
static bool make_required(const char* oname, rule ru)
{
    list reals;
    bool success_p = TRUE;

    /* we build the list of required real_resources */
    reals = build_real_resources(oname, rule_required(ru));

    /* we recursively make required resources */
    MAP(REAL_RESOURCE, rr, 
    {
	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);
	
	pips_debug(3, "rule %s : %s(%s) - recursive call\n",
		   rule_phase(ru), rrrn, rron);
	
	if (!rmake(rrrn, rron)) {
	    success_p = FALSE;
	    /* Want to free the list ... */
	    break;
	}
	
	/* In french:
	   ici nous devons  tester si un des regles modified
	   fait partie des required. Dans ce cas on la fabrique
	   de suite. */
	
    }, reals);

    gen_full_free_list (reals);
    return success_p;
}

/* returns whether resource is up to date.
 */
static bool check_physical_resource_up_to_date(const char* rname, const char* oname)
{
  list real_required_resources = NIL;
  list real_modified_resources = NIL;
  rule ru = rule_undefined;
  bool result = TRUE;
  void * res_id = (void *) db_get_resource_id(rname, oname);

  /* Maybe is has already been proved true */
  if(set_belong_p(up_to_date_resources, res_id))
    return TRUE;

  /* Initial resources by definition are not associated to a rule.
   * FI: and they always are up-to-date?!? Even if somebody touched the file? 
   * You mean you do not propagate modifications performed outside of the workspace?
   */
  if (same_string_p(rname, DBR_USER_FILE))
    return TRUE;

  /* We get the active rule to build this resource */
  ru = safe_find_rule_by_resource(rname);

  /* we build the list of required real_resources */
  /* Here we are sure (thanks to find_rule_by_resource) that the rule does
     not use a resource it produces. FI: OK, this does not rule out
     modified resources which should not be taken into account to avoid
     infinite recursion. */

  real_required_resources = build_real_resources(oname, rule_required(ru));
  real_modified_resources = build_real_resources(oname, rule_modified(ru));

  /* we are going to check if the required resources are 
     - in the database or in the rule_modified list
     - proved up to date (recursively)
     - have timestamps older than the tested one
  */
  MAP(REAL_RESOURCE, rr, {
    string rron = real_resource_owner_name(rr);
    string rrrn = real_resource_resource_name(rr);
      
    bool res_in_modified_list_p = FALSE;
      
    /* we build the list of modified real_resources */
      
    MAP(REAL_RESOURCE, mod_rr, {
      string mod_rron = real_resource_owner_name(mod_rr);
      string mod_rrrn = real_resource_resource_name(mod_rr);
	  
      if ((same_string_p(mod_rron, rron)) &&
	  (same_string_p(mod_rrrn, rrrn))) {
	/* we found it */
	res_in_modified_list_p = TRUE;
	pips_debug(3, "resource %s(%s) is in the rule_modified list",
		   rrrn, rron);
	break;
      }
    }, real_modified_resources);

    /* If the resource is in the modified list, then
       don't check anything */
    if (res_in_modified_list_p == FALSE) {
	if (!db_resource_p(rrrn, rron)) {
	  pips_debug(5, "resource %s(%s) is not there "
		     "and not in the rule_modified list", rrrn, rron);
	  result = FALSE;
	  break;
	} else {
	  /* Check if this resource is up to date */
	  long rest;
	  long respt;
	  if (check_resource_up_to_date(rrrn, rron) == FALSE) {
	    pips_debug(5, "resource %s(%s) is not up to date", rrrn, rron);
	    result = FALSE;
	    break;
	  }
	  rest = db_time_of_resource(rname, oname);
	  respt = db_time_of_resource(rrrn, rron);
	  /* Check if the timestamp is OK */
	  if (rest<respt)
	    {
	      pips_debug(5, "resource %s(%s) with time stamp %ld is newer "
			 "than resource %s(%s) with time stamp %ld\n",
			 rrrn, rron, respt, rname, oname, rest);
	      result = FALSE;
	      break;
	    }
	}
      }
  }, real_required_resources);

  gen_full_free_list (real_required_resources);
  gen_full_free_list (real_modified_resources);

  /* If the resource is up to date then add it in the set, as well as its
     siblings, if they are produced by the same rule. Think of callgraph
     with may produce literaly thousands of resources, three times the
     number of modules! */
  if (result == TRUE) 
    {
      list real_produced_resources =  build_real_resources(oname, rule_produced(ru));
      bool res_found_p = FALSE;

      pips_debug(5, "resource %s(%s) added to up_to_date "
		 "with time stamp %d\n",
		 rname, oname, db_time_of_resource(rname, oname));
      set_add_element(up_to_date_resources, up_to_date_resources, res_id);

      MAP(REAL_RESOURCE, rpr, {
	string srname = real_resource_resource_name(rpr);
	string soname = real_resource_owner_name(rpr);
	void * sres_id = (void *) db_get_resource_id(srname, soname);

	if(sres_id != (real_resource) res_id) {

	  if(same_string_p(rname, srname)) {
	    /* We would retrieve the same rule and the same required
               resources. rpr is up-to-date.*/

	    pips_debug(5, "sibling resource %s(%s) added to up_to_date "
		       "with time stamp %d\n",
		       srname, soname, db_time_of_resource(srname, soname));
	    set_add_element(up_to_date_resources, up_to_date_resources, sres_id);
	  }
	  else {
	    /* Check that the sibling is currently obtained by the same
               rule, because an activate might preempt it for some of the
               produced resources? */
	    rule sru = find_rule_by_resource(srname);
	    if(sru==ru) {
	      /* The rule does not have to be fired again, so its produced
                 resources are up-to-date. */
	    string soname = real_resource_owner_name(rpr);

	    pips_debug(5, "sibling resource %s(%s) added to up_to_date "
		       "with time stamp %d\n",
		       srname, soname, db_time_of_resource(srname, soname));
	    set_add_element(up_to_date_resources, up_to_date_resources, sres_id);
	    }
	  }
	}
	else {
	  res_found_p = TRUE;
	}
      }, real_produced_resources);

      pips_assert("The resources res is among the real resources produced by rule ru",
		  res_found_p);

      gen_full_free_list (real_produced_resources);
    }
  else
    {
      /* well, if it is not okay, let us delete it!???
       * okay, this might be done later, but in some case it is not.
       * I'm not really sure this is the right fix, but at least it avoids 
       * a coredump after touching some internal file (.f_initial) and
       * requesting the PRINTED_FILE for it.
       * FC, 22/07/1998
       *
       * FI: this may be costly and should be avoided on a quit!
       */
      db_delete_resource(rname, oname);
    }
  
  return result;
}

int delete_obsolete_resources(void)
{
    int ndeleted;
    bool cache_off = set_undefined_p(up_to_date_resources);
    if (cache_off) init_make_cache();
    ndeleted =
	db_delete_obsolete_resources(check_physical_resource_up_to_date);
    if (cache_off) reset_make_cache();
    return ndeleted;
}

/* this is quite ugly, but I wanted to put the enumeration down to pipsdbm.
 */
void delete_some_resources(void)
{
    string what = get_string_property("PIPSDBM_RESOURCES_TO_DELETE");
    dont_interrupt_pipsmake_asap();

    user_log("Deletion of %s resources:\n", what);

    if (same_string_p(what, "obsolete")) 
    {
	int ndeleted = delete_obsolete_resources();
	if (ndeleted>0) user_log("%d destroyed.\n", ndeleted);
	else user_log("none destroyed.\n");
    } else if (same_string_p(what, "all")) {
	db_delete_all_resources();
	user_log("done.\n");
    } else
	pips_internal_error("unexpected delete request %s\n", what);
}

/* To be used in a rule. use and update the up_to_dat list
 * created by makeapply 
 */
bool check_resource_up_to_date(const char* rname, const char* oname)
{
    return db_resource_p(rname, oname)?
	check_physical_resource_up_to_date(rname, oname): FALSE;
}

/* Delete from up_to_date all the resources of a given name */
void delete_named_resources (const char* rn)
{
    /* GO 29/6/95: many lines ...
       db_unput_resources_verbose (rn);*/
    db_unput_resources(rn);

    if (up_to_date_resources != set_undefined) {
	/* In this case we are called from a Pips phase 
	user_warning ("delete_named_resources",
		      "called within a phase (i.e. by activate())\n"); */
	SET_MAP(res, {
	    string res_rn = real_resource_resource_name((real_resource) res);
	    string res_on = real_resource_owner_name((real_resource) res);

	    if (same_string_p(rn, res_rn)) {
		pips_debug(5, "resource %s(%s) deleted from up_to_date\n",
			   res_rn, res_on);
		set_del_element (up_to_date_resources,
				 up_to_date_resources,
				 (char *) res);
	    }
	}, up_to_date_resources);
    }
}

void delete_all_resources(void)
{
    db_delete_all_resources();
    set_free(up_to_date_resources);
    up_to_date_resources = set_make(set_pointer);
}

/* Should be able to handle Fortran applications, C applications and
   mixed Fortran/C applications. */
string get_first_main_module(void)
{
    string dir_name = db_get_current_workspace_directory();
    string main_name;
    string name = string_undefined;

    debug_on("PIPSMAKE_DEBUG_LEVEL");

    /* Let's look for a Fortran main */
    main_name = strdup(concatenate(dir_name, "/.fsplit_main_list", NULL));

    if (file_exists_p(main_name)) 
    {
	FILE * tmp_file = safe_fopen(main_name, "r");
	name = safe_readline(tmp_file);
	safe_fclose(tmp_file, main_name);
    }
    free(main_name);

    if(string_undefined_p(name)) {
      /* Let's now look for a C main */
      main_name = strdup(concatenate(dir_name, "/main/main.c", NULL));
      if (file_exists_p(main_name)) 
	name = strdup("main");
      free(main_name);
    }

    free(dir_name);
    debug_off();
    return name;
}

/* check the usage of resources 
 */
void do_resource_usage_check(const char* oname, rule ru)
{
    list reals;
    set res_read = set_undefined;
    set res_write = set_undefined;

    /* Get the dbm sets */
    get_logged_resources (&res_read, &res_write);

    /* build the real required resrouces */
    reals = build_real_resources(oname, rule_required (ru));

    /* Delete then from the set of read resources */
    MAP(REAL_RESOURCE, rr, {
	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);
	string elem_name = strdup(concatenate(rron,".", rrrn, NULL));

	if (set_belong_p (res_read, elem_name)){
	    debug (5, "do_resource_usage_check",
		   "resource %s.%s has been read: ok\n",
		   rron, rrrn);
	    set_del_element(res_read, res_read, elem_name);
	} else
	    user_log ("resource %s.%s has not been read\n",
		      rron, rrrn);
    }, reals);

    /* Try to find an illegally read resrouce ... */
    SET_MAP(re,	user_log("resource %s has been read\n", re), res_read);
    gen_full_free_list(reals);

    /* build the real produced resources */
    reals = build_real_resources(oname, rule_produced (ru));

    /* Delete then from the set of write resources */
    MAP(REAL_RESOURCE, rr, {
	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);
	string elem_name = strdup(concatenate(rron,".", rrrn, NULL));

	if (set_belong_p (res_write, elem_name)){
	    debug (5, "do_resource_usage_check",
		   "resource %s.%s has been written: ok\n",
		   rron, rrrn);
	    set_del_element(res_write, res_write, elem_name);
	} else
	    user_log ("resource %s.%s has not been written\n",
		      rron, rrrn);
    }, reals);

    /* Try to find an illegally written resrouce ... */
    SET_MAP(re,{
	user_log ("resource %s has been written\n", re);
    }, res_write);

    gen_full_free_list(reals);

    set_clear(res_read);
    set_clear(res_write);
}


/******************************************************** EXTERNAL INTERFACE */

static double initial_memory_size;

static void logs_on(void)
{
    if (get_bool_property("LOG_TIMINGS"))
	init_request_timers();

    if (get_bool_property("LOG_MEMORY_USAGE")) 
	initial_memory_size = get_process_gross_heap_size();
}

static void logs_off(void)
{
    if (get_bool_property("LOG_TIMINGS"))
    {
	string request_time, phase_time, dbm_time;
	get_request_string_timers (&request_time, &phase_time, &dbm_time);

	user_log ("                                 stime      ");
	user_log (request_time);
	user_log ("                                 phase time ");
	user_log (phase_time);
	user_log ("                                 IO stime   ");
	user_log (dbm_time);
    }

    if (get_bool_property("LOG_MEMORY_USAGE")) 
    {
	double final_memory_size = get_process_gross_heap_size();
	user_log("\t\t\t\t memory size %10.3f, increase %10.3f\n",
		 final_memory_size,
		 final_memory_size-initial_memory_size);
    }
}

static bool safe_do_something(
    const char* name,
    const char* module_n,
    const char* what_it_is,
    rule (*find_rule)(const char*),
    bool (*doit)(const char*,const char*))
{
    bool success = FALSE;

    debug_on("PIPSMAKE_DEBUG_LEVEL");

    if (find_rule(name) == rule_undefined)
    {
	pips_user_warning("Unknown %s \"%s\"\n", what_it_is, name);
	success = FALSE;
	debug_off();
	return success;
    }

    CATCH(any_exception_error)
    {
	/* global variables that have to be reset after user-error */
	reset_make_cache();
	reset_static_phase_variables();
	retrieve_active_phases();
	pips_user_warning("Request aborted in pipsmake: "
			  "build %s %s for module %s.\n",
			  what_it_is, name, module_n);
	db_clean_all_required_resources();
	success = FALSE;
    }
    TRY
    {
	user_log("Request: build %s %s for module %s.\n",
		 what_it_is, name, module_n);

	logs_on();
	pips_malloc_debug();

	/* DO IT HERE!
	 */
	success = doit(name, module_n);

	if(success)
	{
	    user_log("%s made for %s.\n", name, module_n);
	    logs_off();
	}
	else
	{
	    pips_user_warning("Request aborted under pipsmake: "
			      "build %s %s for module %s.\n",
			      what_it_is, name, module_n);
	}
	UNCATCH(any_exception_error);
    }
    debug_off();
    return success;
}

bool safe_make(const char* res_n, const char* module_n)
{
    return safe_do_something(res_n, module_n, "resource",
			     find_rule_by_resource, make);
}

bool safe_apply(const char* phase_n, const char* module_n)
{
    return safe_do_something(phase_n, module_n, "phase/rule",
			     find_rule_by_phase, apply);
}

bool safe_concurrent_apply(
    const char* phase_n,
    gen_array_t modules)
{
    bool ok = TRUE;
    debug_on("PIPSMAKE_DEBUG_LEVEL");

    /* Get a human being representation of the modules: */
    string module_list = strdup(string_array_join(modules, ","));

    if (find_rule_by_phase(phase_n)==rule_undefined)
    {
	pips_user_warning("Unknown phase \"%s\"\n", phase_n);
	ok = FALSE;
    }
    else
    {
      CATCH(any_exception_error)
      {
	reset_make_cache();
	retrieve_active_phases();
	pips_user_warning("Request aborted in pipsmake\n");
	ok = FALSE;
      }
      TRY
      {
	logs_on();

	user_log("Request: capply %s for module [%s].\n",
		 phase_n, module_list);

	ok = concurrent_apply(phase_n, modules);

	if (ok)	{
	    user_log("capply %s made for [%s].\n", phase_n, module_list);
	    logs_off();
	}
	else {
	  pips_user_warning("Request aborted under pipsmake: "
			    "capply %s for module [%s].\n",
			    phase_n, module_list);
	}

	UNCATCH(any_exception_error);
      }
    }

    free(module_list);
    debug_off();
    return ok;
}

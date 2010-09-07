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
 * package regions :  Beatrice Creusillet, September 1994
 *
 * This File contains the functions that deal with the comparison and
 * combinations of regions.
 */


#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "database.h"

#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "sommet.h"
#include "ray_dte.h"
#include "sg.h"
#include "polyedre.h"
#include "union.h"

#include "constants.h"
#include "ri-util.h"
#include "effects-util.h"
#include "pipsdbm.h"
#include "misc.h"
#include "text.h"

#include "effects-generic.h"
#include "effects-convex.h"

#define COMBINE 1
#define UNUSED 2

#define SUPER TRUE
#define SUB FALSE

/****************************************** STATISTICS FOR BINARY OPERATORS */

static int nb_umust = 0;
static int nb_umust_must_must = 0;
static int nb_umust_must_must_must = 0;
static int nb_umust_must_may = 0;
static int nb_umust_must_may_must = 0;
static int nb_umust_sc_rn = 0;

static int nb_umay = 0;
static int nb_umay_must_must = 0;
static int nb_umay_must = 0;

static int nb_dsup = 0;
static int nb_dsup_pot_must = 0;
static int nb_dsup_must = 0;

static int nb_dinf = 0;
static int nb_dinf_pot_must = 0;
static int nb_dinf_must = 0;


void reset_binary_op_statistics()
{
    nb_umust = 0;
    nb_umust_must_must = 0;
    nb_umust_must_must_must = 0;
    nb_umust_must_may = 0;
    nb_umust_must_may_must = 0;
    nb_umust_sc_rn = 0;

    nb_umay = 0;
    nb_umay_must_must = 0;
    nb_umay_must = 0;

    nb_dsup = 0;
    nb_dsup_pot_must = 0;
    nb_dsup_must = 0;

    nb_dinf = 0;
    nb_dinf_pot_must = 0;
    nb_dinf_must = 0;
}




void print_umust_statistics(char *mod_name, char *prefix)
{
    FILE *fp;
    string filename;

    filename = "umust_op_stat";
    filename = strdup(concatenate(db_get_current_workspace_directory(), "/",
				  mod_name, ".", prefix, filename, NULL));

    fp = safe_fopen(filename, "w");
    fprintf(fp,"%s",mod_name);

    fprintf(fp," %d %d %d %d %d %d\n", nb_umust, nb_umust_must_must,
	    nb_umust_must_must_must, nb_umust_must_may, nb_umust_must_may_must,
	    nb_umust_sc_rn);

    safe_fclose(fp, filename);
    free(filename);
}


void print_umay_statistics(char *mod_name, char *prefix)
{
    FILE *fp;
    string filename;

    filename = "umay_op_stat";
    filename = strdup(concatenate(db_get_current_workspace_directory(), "/",
				  mod_name, ".", prefix, filename, NULL));

    fp = safe_fopen(filename, "w");
    fprintf(fp,"%s",mod_name);

    fprintf(fp," %d %d %d \n", nb_umay, nb_umay_must_must, nb_umay_must);

    safe_fclose(fp, filename);
    free(filename);
}


void print_dsup_statistics(char *mod_name, char *prefix)
{
    FILE *fp;
    string filename;

    filename = "dsup_op_stat";
    filename = strdup(concatenate(db_get_current_workspace_directory(), "/",
				  mod_name, ".", prefix, filename, NULL));

    fp = safe_fopen(filename, "w");
    fprintf(fp,"%s",mod_name);

    fprintf(fp," %d %d %d \n", nb_dsup, nb_dsup_pot_must, nb_dsup_must);

    safe_fclose(fp, filename);
    free(filename);
}


void print_dinf_statistics(char *mod_name, char *prefix)
{
    FILE *fp;
    string filename;

    filename = "dinf_op_stat";
    filename = strdup(concatenate(db_get_current_workspace_directory(), "/",
				  mod_name, ".", prefix, filename, NULL));

    fp = safe_fopen(filename, "w");
    fprintf(fp,"%s",mod_name);

    fprintf(fp," %d %d %d \n", nb_dinf, nb_dinf_pot_must, nb_dinf_must);

    safe_fclose(fp, filename);
    free(filename);
}



/*********************************************************** INTERFACES */


/* list RegionsMayUnion(list l1, list l2, union_combinable_p)
 * input    : two lists of regions
 * output   : a list of regions, may union of the two initial lists
 * modifies : l1 and l2 and their regions. Regions that are not reused in
 *            the output list of regions are freed.nothing
 *            (no sharing introduced).
 */
list RegionsMayUnion(list l1, list l2,
		     boolean (*union_combinable_p)(effect, effect))
{
    list lr;

    debug(3, "RegionsMayUnion", "begin\n");
    lr = list_of_effects_generic_binary_op(l1, l2,
					   union_combinable_p,
					   region_may_union,
					   region_to_may_region_list,
					   region_to_may_region_list);
    debug(3, "RegionsMayUnion", "end\n");
    return(lr);
}


/* list RegionsMustUnion(list l1, list l2, union_combinable_p)
 * input    : two lists of regions
 * output   : a list of regions, must union of the two initial lists
 * modifies : l1 and l2 and their regions. Regions that are not reused in
 *            the output list of regions are freed.
 */
list RegionsMustUnion(list l1, list l2,
		      boolean (*union_combinable_p)(effect, effect))
{
    list lr;

    debug(3, "RegionsMustUnion", "begin\n");
    lr = list_of_effects_generic_binary_op(l1, l2,
					   union_combinable_p,
					   region_must_union,
					   region_to_list,
					   region_to_list);
    debug(3, "RegionsMustUnion", "end\n");
    return(lr);
}


/* list RegionsIntersection(list l1,l2,
        boolean (*intersection_combinable_p)(effect, effect))
 * input    :
 * output   :
 * modifies :
 * comment  :
 */
list RegionsIntersection(list l1, list l2,
			 boolean (*intersection_combinable_p)(effect, effect))
{
    list l_res = NIL;

    debug(3, "RegionsIntersection", "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   intersection_combinable_p,
					   region_intersection,
					   region_to_nil_list,
					   region_to_nil_list);
    debug(3, "RegionsIntersection", "end\n");

    return l_res;
}


/* list RegionsEntitiesIntersection(list l1,l2,
            boolean (*intersection_combinable_p)(effect, effect))
 * input    : two lists of regions
 * output   : a list of regions containing all the regions of l1 that have
 *            a corresponding region (i.e. same entity) in l2.
 * modifies : l1 and l2.
 * comment  :
 */
list RegionsEntitiesIntersection(list l1, list l2,
				 boolean (*intersection_combinable_p)(effect, effect))
{
    list l_res = NIL;

    pips_debug(3, "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   intersection_combinable_p,
					   region_entities_intersection,
					   region_to_nil_list,
					   region_to_nil_list);
    pips_debug(3, "end\n");

    return l_res;
}


/* list RegionsSupDifference(list l1, l2)
 * input    : two lists of regions
 * output   : a list of region, representing the sup_difference of the
 *            initial regions.
 * modifies : the regions of l2 may be freed.
 * comment  : we keep the regions of l1 that are not combinable with those
 *            of l2, but we don't keep the regions of l2 that are not
 *            combinable with those of l_reg1.
 */
list RegionsSupDifference(list l1, list l2,
			  boolean (*difference_combinable_p)(effect, effect))
{
    list l_res = NIL;

    debug(3, "RegionsSupDifference", "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   difference_combinable_p,
					   region_sup_difference,
					   region_to_list,
					   region_to_nil_list);
    debug(3, "RegionsSupDifference", "end\n");

    return l_res;
}

/* list RegionsInfDifference(list l1, l2)
 * input    : two lists of regions
 * output   : a list of region, representing the inf_difference of the
 *            initial regions.
 * modifies : the regions of l2 may be freed.
 * comment  : we keep the regions of l1 that are not combinable with those
 *            of l2, but we don't keep the regions of l2 that are not
 *            combinable with those of l_reg1.
 */
list RegionsInfDifference(list l1, list l2,
			  boolean (*difference_combinable_p)(effect, effect))
{
    list l_res = NIL;

    debug(3, "RegionsInfDifference", "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   difference_combinable_p,
					   region_inf_difference,
					   region_to_list,
					   region_to_nil_list);
    debug(3, "RegionsInfDifference", "end\n");

    return l_res;
}



/* list RegionsEntitiesInfDifference(list l1, l2)
 * input    : two lists of regions
 * output   : a list of regions, such that: if there is a region R concerning
 *            entity A in l1 and in l2, then R is removed from the result;
 *            if there is a region R concerning array A in l1, but not in l2,
 *            then it is kept in l1, and in the result.
 * modifies : the regions of l2 may be freed.
 * comment  : we keep the regions of l1 that are not combinable with those
 *            of l2, but we don't keep the regions of l2 that are not
 *            combinable with those of l_reg1.
 */
list RegionsEntitiesInfDifference(
    list l1, list l2,
    boolean (*difference_combinable_p)(effect, effect))
{
    list l_res = NIL;

    debug(3, "RegionsEntitiesInfDifference", "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   difference_combinable_p,
					   regions_to_nil_list,
					   region_to_list,
					   region_to_nil_list);
    debug(3, "RegionsEntitiesInfDifference", "end\n");

    return l_res;
}


/***************************************** INDIVIDUAL REGIONS COMBINATION */


/* 1- Union :
 */

effect regions_must_convex_hull(region f1, region f2);
static effect regions_may_convex_hull(region f1, region f2);

list region_must_union(region r1, region r2)
{
    list l_res = NIL;
    effect r_res;

    if (anywhere_effect_p(r1) || anywhere_effect_p(r2))
      /* Should be a bit more complicated with typed anywhere */
      /* Should be more generic? */
      r_res = make_anywhere_effect(effect_action(r1));
    else if(store_effect_p(r1)) {
      /* we could check effect_store(r2) */
      pips_assert("r2 is a store effect", store_effect_p(r2));
      r_res = regions_must_convex_hull(r1, r2);
    }
    else {
      /* we could check r2 */
      pips_assert("r2 is not a store effect", !store_effect_p(r2));
      r_res = copy_effect(r1);
    }

    debug_region_consistency(r_res);

    /* FI: does work with anywhere effects? */
    if (store_effect_p(r_res)) {
	if(!region_empty_p(r_res))
	  l_res = region_to_list(r_res);
	else
	  /* no memory leak */
	  region_free(r_res);
    }

    return(l_res);
}

list region_may_union(region r1, region r2)
{
  list l_res = NIL;
  effect r_res;

  ifdebug(1) pips_assert("Effects r1 and r2 are union compatible",
			 union_compatible_effects_p(r1,r2));

  if(store_effect_p(r1) && store_effect_p(r2)) {
    if (anywhere_effect_p(r1) || anywhere_effect_p(r2))
      r_res = make_anywhere_effect(effect_action(r1));
    else
      r_res = regions_may_convex_hull(r1, r2);

    if (!region_empty_p(r_res))
      l_res = region_to_list(r_res);
    else
      /* no memory leak */
      region_free(r_res);
  }
  else {
    r_res = copy_effect(r1);
  }

  return(l_res);
}


static Psysteme region_sc_convex_hull(Psysteme ps1, Psysteme ps2);

/* static effect regions_must_convex_hull(r1,r2)
 * input    : two "effect" representing two regions.
 * output   : an effect, which predicate is the convex hull of the predicates
 *            of the initial regions, and which approximation is function of
 *            of the approximations of the original regions
 *            and, in some cases, of the relations between the two initial
 *            predicates.
 * modifies : the basis of the predicates of r1 and r2
 * comment  : always computes the convex hull, even if it leads to a may
 *            approximation.
 *
 * FI: extension to support non-store effects without predicates
 */
effect regions_must_convex_hull(region r1, region r2)
{
  region reg = r1;  /* result */

  if(!store_effect_p(r1)) {
    /* proper_effects_combine() free the arguments of
       region_sc_convex_hull() */
    reg = copy_effect(r1);
  } else {
    /* Automatic variables read in a CATCH block need to be declared volatile as
     * specified by the documentation*/
    Psysteme volatile s1 = region_system(r1);
    Psysteme s2 = region_system(r2);
    Psysteme sr;
    tag app1 = region_approximation_tag(r1);
    tag app2 = region_approximation_tag(r2);
    tag appr = is_approximation_may;

    debug_region_consistency(r1);
    debug_region_consistency(r2);

    pips_assert("systems not undefined\n",
		!(SC_UNDEFINED_P(s1) || SC_UNDEFINED_P(s2)));

    ifdebug(8)
      {
	pips_debug(8, "syste`mes initiaux\n");
	sc_syst_debug(s1);
	sc_syst_debug(s2);
      }

    /* sc_empty : no part of the array is accessed */
    if (sc_empty_p(s1) && sc_empty_p(s2))
      {
	pips_debug(8, "s1 and s2 sc_empty\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = approximation_or(app1,app2);
	return(reg);
      }

    if (sc_empty_p(s1))
      {
	pips_debug(8, "s1 sc_empty\n");
	reg = region_dup(r1);
	sc_rm(region_system(reg));
	region_system_(reg) = newgen_Psysteme(sc_dup(s2));
	return(reg);
      }

    if (sc_empty_p(s2))
      {
	pips_debug(8, "s2 sc_empty\n");
	reg = region_dup(r1);
	return(reg);
      }


    /* sc_rn : the entire array is accessed */
    if (sc_rn_p(s1) && sc_rn_p(s2))
      {
	pips_debug(8, "s1 and s2 sc_rn\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = approximation_or(app1,app2);
	return(reg);
      }

    if (sc_rn_p(s1))
      {
	pips_debug(8, "s1 sc_rn\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = is_approximation_may;
	return(reg);
      }

    if (sc_rn_p(s2))
      {
	pips_debug(8, "s2 sc_rn\n");
	reg = region_dup(r1);
	region_system_(reg) =  newgen_Psysteme(sc_dup(s2));
	region_approximation_tag(reg) = app2;
	return(reg);
      }


    /* otherwise, we have to compute the convex-hull of the two predicates */
    if (op_statistics_p()) nb_umust++;

    CATCH(overflow_error)
    {
      pips_debug(1, "overflow error\n");
      sr = sc_rn(base_dup(s1->base));
    }
    TRY
      {
	sr = region_sc_convex_hull(s1, s2);
	sc_nredund(&sr);
	UNCATCH(overflow_error);
      }

    /* to avoid problems in case of overflow error */
    appr = is_approximation_may;

    /* if we want to preserve must approximations, then we must find the
     * approximation of the resulting region ;
     * it may depend on the approximations of the initial regions, and on
     * the relations between the two initial systems of constraints */

    if(sc_rn_p(sr))
      {
	reference refr = copy_reference(region_any_reference(r1));
	action acr = region_action(r1);

	pips_debug(1, "sr sc_rn (maybe due to an overflow error)\n");

	if (op_statistics_p())
	  {
	    if ((app1 == is_approximation_must) &&
		(app2 == is_approximation_must))
	      nb_umust_must_must++;
	    else
	      if (!((app1 == is_approximation_may)
		    && (app2 == is_approximation_may)))
		nb_umust_must_may++;
	    nb_umust_sc_rn++;
	  }

	/* we return a whole array region */
	appr = is_approximation_may;
	reg = reference_whole_region(refr, acr);
	effect_to_may_effect(reg);
	ifdebug(8)
	  {
	    pips_debug(8, "final region : \n");
	    print_region(reg);
	  }
	sc_rm(sr);
	sr = NULL;
	return(reg);
      }
    else
      {
	if(!must_regions_p())
	  {
	    pips_debug(8, "may regions\n");
	    appr = is_approximation_may;
	  }
	else
	  {
	    switch (app1)
	      {
	      case is_approximation_must:

		switch (app2)
		  {
		  case is_approximation_must: /* U_must(must, must) */
		    /* si enveloppe convexe (s1, s2) exacte must sinon may */
		    if (sc_convex_hull_equals_union_p_ofl(sr,s1,s2))
		      {
			pips_debug(8,"exact convex hull\n");
			appr = is_approximation_must;
		      }
		    else
		      {
			pips_debug(8,"non exact convex hull\n");
			appr = is_approximation_may;
		      }
		    if (op_statistics_p())
		      {
			nb_umust_must_must++;
			if (appr == is_approximation_must)
			  nb_umust_must_must_must++;
		      }
		    break;

		  case is_approximation_may: /* U_must(must,may) */
		    if(sc_inclusion_p_ofl(s2,s1))
		      appr = is_approximation_must;
		    else
		      appr = is_approximation_may;
		    if (op_statistics_p())
		      {
			nb_umust_must_may++;
			if (appr == is_approximation_must)
			  nb_umust_must_may_must++;
		      }
		    break;
		  }
		break;
	      case is_approximation_may:

		switch (app2)
		  {
		  case is_approximation_must: /* U_must(may,must) */
		    if(sc_inclusion_p_ofl(s1,s2))
		      appr = is_approximation_must;
		    else
		      appr = is_approximation_may;
		    if (op_statistics_p())
		      {
			nb_umust_must_may++;
			if (appr == is_approximation_must)
			  nb_umust_must_may_must++;
		      }
		    break;
		  case is_approximation_may:
		    appr = is_approximation_may;
		    break;
		  }
		break;
	      }
	  }
      }
    reg = region_dup(r1);
    sc_rm(region_system(reg));
    region_system_(reg) = newgen_Psysteme(sr);
    region_approximation_tag(reg) = appr;
  }

  ifdebug(8)
    {
      pips_debug(8, "final region : \n");
      print_region(reg);
      pips_assert("reg is consistent", effect_consistent_p(reg));
    }

  return(reg);

}


/* static effect regions_may_convex_hull(region r1,r2)
 * input    : two "effect" representing two regions.
 * output   : an effect, which predicate is the convex hull of the predicates
 *            of the initial regions, and which approximation is function of
 *            the approximations of the original regions
 *            and, in some cases, of the relations between the two initial
 *            predicates.
 * modifies : the basis of the predicates of r1 and r2
 * comment  : always computes the convex hull, even if it leads to a may
 *            approximation.
 */
static effect regions_may_convex_hull(region r1, region r2)
{
  /* region is a renaming of effect */
    region reg = r1;  /* result */

    if(!store_effect_p(r1) || !store_effect_p(r2)) {
      ifdebug(1) {
	/* Consistency check on regions for action kinds environment
	   and type declaration which should be put in a specific
	   function to enlarge region consistency checks. */
      action a1 = effect_action(r1);
      action_kind ak1 = action_to_action_kind(a1);
      action a2 = effect_action(r2);
      action_kind ak2 = action_to_action_kind(a2);
      descriptor d1 = effect_descriptor(r1);
      descriptor d2 = effect_descriptor(r2);
      entity e1 = effect_variable(r1);
      entity e2 = effect_variable(r2);

      pips_assert("r1 and r2 have the same action",
		  action_tag(a1)==action_tag(a2));
      pips_assert("r1 and r2 have the same action kind",
		  action_kind_tag(ak1)==action_kind_tag(ak2));
      pips_assert("r1 and r2 refers to the same variable",
		  e1==e2);
      pips_assert("r1 and r2 have no descriptor",
		  descriptor_none_p(d1) && descriptor_none_p(d2));
      }

      reg = copy_effect(r1);
    } else {
    Psysteme s1 = region_system(r1);
    Psysteme s2 = region_system(r2);
    Psysteme sr;
    tag app1 = region_approximation_tag(r1);
    tag app2 = region_approximation_tag(r2);
    tag appr = is_approximation_may;

    pips_assert("systems not undefined\n",
		!(SC_UNDEFINED_P(s1) || SC_UNDEFINED_P(s2)));

    ifdebug(8)
    {
	pips_debug(8, "initial systems\n");
	sc_syst_debug(s1);
	sc_syst_debug(s2);
    }

    /* sc_empty : no part of the array is accessed */
    if (sc_empty_p(s1) && sc_empty_p(s2))
    {
	pips_debug(8, "s1 and s2 sc_empty\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = approximation_and(app1,app2);
	return(reg);
    }

    if (sc_empty_p(s1)) {
	pips_debug(8, "s1 sc_empty\n");
	reg = region_dup(r1);
	sc_rm(region_system(reg));
	region_system_(reg) = newgen_Psysteme(sc_dup(s2));
	region_approximation_tag(reg) = is_approximation_may;
	return(reg);
    }

    if (sc_empty_p(s2))
    {
	pips_debug(8, "s2 sc_empty\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = is_approximation_may;
	return(reg);
    }

    /* sc_rn : the entire array is accessed */
     if (sc_rn_p(s1) && sc_rn_p(s2))
     {
	pips_debug(8, "s1 and s2 sc_rn\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = approximation_and(app1,app2);
	return(reg);
    }

    if (sc_rn_p(s1))
    {
	pips_debug(8, "s1 sc_rn\n");
	reg = region_dup(r1);
	region_approximation_tag(reg) = is_approximation_may;
	return(reg);
    }

    if (sc_rn_p(s2))
    {
	pips_debug(8, "s2 sc_rn\n");
	reg = region_dup(r1);
	region_system_(reg) = newgen_Psysteme(sc_dup(s2));
	region_approximation_tag(reg) = is_approximation_may;
	return(reg);
    }

    /* otherwise, we have to compute the convex-hull of the two predicates */
    if (op_statistics_p()) nb_umay++;

    sr = region_sc_convex_hull(s1, s2);
    sc_nredund(&sr);

    /* if we want to preserve must approximations, then we must find the
     * approximation of the resulting region ;
     * it may depend on the approximations of the initial regions, on the
     * precision
     * (must union or may union), and on the relations between the two initial
     * systems of constraints */

    if(sc_rn_p(sr))
    {
	reference refr = copy_reference(region_any_reference(r1));
	action acr = region_action(r1);

	pips_debug(1,"sr sc_rn (maybe due to an overflow error)\n");

	if (op_statistics_p())
	{
	    if (approximation_and(app1,app2) == is_approximation_must)
		nb_umay_must_must++;
	}

	/* we return a whole array region */
	appr = is_approximation_may;
	reg = reference_whole_region(refr, acr);
	effect_to_may_effect(reg);
	ifdebug(8)
	{
	    pips_debug(8, "final region : \n");
	    print_region(reg);
	}
	sc_rm(sr);
	sr = NULL;
	return(reg);
    }
    else
    {
	if(!must_regions_p())
	{
	    pips_debug(8,"may regions\n");
	    appr = is_approximation_may;
	}
	else
	{
	    if ((app1 == is_approximation_must) &&
		(app2 == is_approximation_must))
	    {
		/* U_may(must,must) */
		/* si s1 == s2 (ie repre'sentent le me^me ensemble)
		 * alors must sinon may */
		if (sc_equal_p_ofl(s1,s2))
		    appr = is_approximation_must;
		else
		    appr = is_approximation_may;
	    }
	    else
		appr = is_approximation_may;
	}
    }
    reg = region_dup(r1);
    sc_rm(region_system(reg));
    region_system_(reg) = newgen_Psysteme(sr);
    region_approximation_tag(reg) = appr;

    if (op_statistics_p())
    {
	if (approximation_and(app1,app2) == is_approximation_must)
	{
	    nb_umay_must_must++;
	    if (appr == is_approximation_must) nb_umay_must++;
	}
    }
    }

    ifdebug(8)
    {
	pips_debug(8, "final region : \n");
	print_region(reg);
    }

    return(reg);
}

/*static  Psysteme region_sc_convex_hull(Psysteme ps1, ps2)
 * input    : two systems of constraints
 * output   : another system of constraints representing their convex
 *            hull.
 * modifies : the bases of ps1 and ps2 : they are changed to their union,
 *            and reordered.
 * comment  : same as transformer_convex_hulls, except for the type of the
 *            arguments, and the reordering of the bases.
 */
static Psysteme region_sc_convex_hull(Psysteme ps1, Psysteme ps2)
{
  return cute_convex_union(ps1, ps2);
}


/* 2- Intersection :
 */

/* list region_intersection(region reg1, reg2)
 * input    : two regions
 * output   : a list of regions containing their difference.
 *            the action of the resulting regions is the action
 *            of the first region (reg1).
 * modifies : nothing.
 * comment  : not debugged, because not yet used. (funny, FC;-) (still up_to-date ? BC)
 */
list /* of region */
region_intersection(region reg1, region reg2)
{
  list l_res = NIL;

  if(store_effect_p(reg1) && store_effect_p(reg2)) {
    /* FI: Standard procedure for memory/store regions */
    Psysteme sc1 = region_system(reg1);
    Psysteme sc2 = region_system(reg2);
    tag app1 = region_approximation_tag(reg1);
    tag app2 = region_approximation_tag(reg2);
    tag app_res = approximation_and(app1,app2);

    ifdebug(3){
	pips_debug(3,"initial regions :\n");
	print_region(reg1);
	print_region(reg2);
    }

    if (anywhere_effect_p(reg1))
      {
	region reg = (*effect_dup_func)(reg2);
	/* memory leak? */
	region_action(reg)= copy_action(region_action(reg1));
	region_approximation_tag(reg)= app_res;
	l_res = region_to_list(reg);
      }
    else if (anywhere_effect_p(reg2))
      {
	region reg = (*effect_dup_func)(reg1);
	region_approximation_tag(reg)= app_res;
	l_res = region_to_list(reg);
      }
    else
      {

	/* Automatic variables read in a CATCH block need to be declared volatile as
	 * specified by the documentation*/
	region volatile reg;
	boolean feasible = TRUE;


	/* if one of the regions is unfeasible, return an empty list */
	if (sc_empty_p(sc1) || sc_empty_p(sc2))
	  {
	    pips_debug(8, "reg1 or reg 2 sc_empty");
	    return(l_res);
	  }


	/* else return a list containing a region which predicate is the
	 * intersection of the two initial predicates, and which approximation
	 * is the "logical" and of the initial approximations.
	 */
	reg = region_dup(reg1);
	region_sc_append_and_normalize(reg,sc2,2); /* could be some other level? */

	CATCH(overflow_error)
	{
	  pips_debug(3, "overflow error \n");
	  feasible = TRUE;
	  debug_region_consistency(reg);
	}
	TRY
	  {
	    feasible = sc_integer_feasibility_ofl_ctrl(region_system(reg),
						       FWD_OFL_CTRL, TRUE);
	    UNCATCH(overflow_error);
	  }

	if (feasible)
	  {
	    region_approximation_tag(reg)= app_res;
	    l_res = region_to_list(reg);
	  }
	else
	  {
	    region_free(reg);
	    l_res = NIL;
	  }
      }
  } else {
    /* FI: extensions for the new action kinds on environment and
       store. Since they are combinable, reg1==reg2 and hence their
       intersection is equal to reg1 */
    effect reg = copy_effect(reg1);
    l_res = CONS(EFFECT, reg, NIL);
  }

    ifdebug(3)
      {
	pips_debug(3,"final region :\n");
	print_regions(l_res);
      }
    return(l_res);
}


/* list region_entities_intersection(region reg1, reg2)
 * input    : two regions
 * output   : a mere copy of the first region.
 * modifies : nothing.
 * comment  : We assume that both regions concern the same entity or that
              one of them is an anywhere effect.
 */
list region_entities_intersection(region r1, region r2)
{
  region reg;

  if (anywhere_effect_p(r1))
    {
      reg = (*effect_dup_func)(r2);
      effect_action(reg) = copy_action(effect_action(r1));
    }
  else
    reg = region_dup(r1);

  return(CONS(EFFECT, reg, NIL));
}


/* 3- Difference :
 */

static list disjunction_to_list_of_regions(Pdisjunct disjonction, effect reg,
					  tag app, boolean super_or_sub);

/* list region_sup_difference(effect reg1, reg2)
 * input    : two regions
 * output   : a list of regions containing their sup_difference.
 *            the action of the resulting regions is the action
 *            of the first region (reg1).
 * modifies :
 * comment  :
 */
list region_sup_difference(region reg1, region reg2)
{
  list l_reg = NIL;
  if(store_effect_p(reg1) && store_effect_p(reg2)) {
    /* This is the traditional case, limited to store/memory regions */
    Psysteme sc1, sc2;
    /* Automatic variables read in a CATCH block need to be declared volatile as
     * specified by the documentation*/
    tag volatile app1 = region_approximation_tag(reg1);
    tag app2 = region_approximation_tag(reg2);

    /* approximation of the resulting regions if the difference is exact */
    tag app = -1;
    region reg;


    /* At this point, we are sure that we have array regions */

    ifdebug(6)
      {
	pips_debug(6, "Initial regions : \n");
	fprintf(stderr,"\t reg1 :\n");
	print_region(reg1);
	fprintf(stderr,"\t reg2 :\n");
	print_region(reg2);
      }

    if (anywhere_effect_p(reg1))
      {
	reg = make_anywhere_effect(effect_action(reg1));
	l_reg = region_to_list(reg);
      }
    else if (anywhere_effect_p(reg2))
      {
	reg = (*effect_dup_func)(reg1);
	l_reg = region_to_may_region_list(reg);
      }
    else
      {

	/* particular cases first */
	sc1 = region_system(reg1);
	sc2 = region_system(reg2);

	/* nothing minus (something or nothing)  = nothing */
	if (sc_empty_p(sc1))
	  return l_reg;

	/*  something minus nothing = something */
	if (sc_empty_p(sc2))
	  {
	    l_reg = region_to_list(region_dup(reg1));
	    return l_reg;
	  }


	/* everything minus everything
	 *             = everything (may) for an array and if app2 = may,
	 *             = nothing otherwise */
	if (sc_rn_p(sc1) && sc_rn_p(sc2))
	  {
	    if (app2 == is_approximation_may)
	      l_reg = region_to_may_region_list(region_dup(reg1));
	    return l_reg;
	  }

	/* something minus everything must = nothing */
	/* something minus everything may = something may*/
	if (sc_rn_p(sc2) )
	  {
	    if (app2 == is_approximation_may)
	      l_reg = region_to_may_region_list(region_dup(reg1));
	    return l_reg;
	  }


	/* general case */
	if (op_statistics_p()) nb_dsup++;

	switch (app2)
	  {
	  case is_approximation_must :
	    if (app1 == is_approximation_must)
	      {
		app = is_approximation_must;
		if (op_statistics_p()) nb_dsup_pot_must++;
	      }
	    else
	      app = is_approximation_may;
	    CATCH (overflow_error)
	    {
	      pips_debug(1, "overflow error\n");
	      app = app1;
	      l_reg = region_to_may_region_list(region_dup(reg1));
	    }
	    TRY
	      {
		Pdisjunct disjonction;
		disjonction = sc_difference(sc1,sc2);
		l_reg = disjunction_to_list_of_regions
		  (disjonction, reg1, app, SUPER);
		UNCATCH(overflow_error);
	      }

	    if (op_statistics_p() && app == is_approximation_must &&
		(approximation_tag(effect_approximation(reg1)) ==
		 is_approximation_must))
	      nb_dsup_must++;

	    break;

	  case is_approximation_may :
	    CATCH(overflow_error)
	    {
	      pips_debug(1, "overflow error\n");
	      app = is_approximation_may;
	    }
	    TRY
	      {
		if (app1 == is_approximation_must)
		  {
		    if (op_statistics_p()) nb_dsup_pot_must++;
		    if (sc_intersection_empty_p_ofl(sc1,sc2))
		      {
			app = is_approximation_must;
			if (op_statistics_p()) nb_dsup_must++;
		      }
		    else app = is_approximation_may;
		  }
		else
		  app = is_approximation_may;
		UNCATCH(overflow_error);
	      }
	    reg = region_dup(reg1);
	    approximation_tag(effect_approximation(reg)) = app;
	    l_reg = CONS(EFFECT, reg, NIL);
	    break;
	  }
      }
  } else {
    /* FI: Here we do need to extend the concept to environment and
       type declaration effect. I
       assume that the effect variable is the same, else the
       difference would be meaningless. If the action_kind is the
       same, then the difference must be empty, as I hope is the case
       for scalara variables in the above code. Else r1 is unchanged. */
    action a1 = effect_action(reg1);
    action a2 = effect_action(reg2);
    action_kind ak1 = action_to_action_kind(a1);
    action_kind ak2 = action_to_action_kind(a2);
    if(action_kind_tag(ak1)==action_kind_tag(ak2)) {
      /* The output list is empty and stays empty */
      l_reg = NIL; // redundant, but improves readability
    }
    else if(action_kind_tag(ak1)==is_action_kind_store
	    || action_kind_tag(ak2)==is_action_kind_store)
      pips_internal_error("Unexpected mix of action kinds.\n");
    else {
      /* reg1 is not modified by reg2 */
      /* This also could be considered an internal error */
      effect reg = copy_effect(reg1);
      l_reg = CONS(EFFECT, reg, NIL);
    }
  }

  ifdebug(6)
    {
      pips_debug(6, "Resulting regions : \n");
      fprintf(stderr,"\t l_reg :\n");
      print_regions(l_reg);
    }

  return l_reg;
}


/* list region_inf_difference(region reg1, reg2)
 * input    : two regions
 * output   : a list of regions containing their inf_difference.
 *            the action of the resulting regions is the action
 *            of the first region (reg1).
 * modifies :
 * comment  : not debugged because not yet used.
 */
list region_inf_difference(region reg1, region reg2)
{
    Psysteme sc1, sc2;
    /* approximation of the resulting region if the difference is exact */
    tag app = region_approximation_tag(reg1);

    list l_res = NIL;

    ifdebug(6)
    {
	pips_debug(6, "Initial regions : \n");
	fprintf(stderr,"\t reg1 :\n");
	print_region(reg1);
	fprintf(stderr,"\t reg2 :\n");
	print_region(reg2);
    }

    if (anywhere_effect_p(reg1) || anywhere_effect_p(reg2))
      return NIL;

    /* particular cases first */
    sc1 = region_system(reg1);
    sc2 = region_system(reg2);

    /* nothing minus something (or nothing)  = nothing */
    if (sc_empty_p(sc1))
	return l_res;

    /*  something minus nothing = something */
    if (sc_empty_p(sc2))
    {
	l_res = region_to_list(region_dup(reg1));
	return l_res;
    }

    /* everything minus everything = nothing */
    if (sc_rn_p(sc1) && sc_rn_p(sc2))
	return l_res;

    /* something minus everything = nothing */
    if (sc_rn_p(sc2) )
	return l_res;

    /* general case */
    if (op_statistics_p())
    {
	nb_dinf++;
	if (app == is_approximation_must) nb_dinf_pot_must++;
    }

    CATCH(overflow_error)
    {
	/* We cannot compute the under-approximation of the difference:
	 *  we must assume that it is the empty set
	 */
	pips_debug(1, "overflow error\n");
	app = is_approximation_may;
	l_res = NIL;
    }
    TRY
    {
	Pdisjunct disjonction;
	disjonction = sc_difference(sc1,sc2);
	l_res = disjunction_to_list_of_regions(disjonction, reg1, app, SUPER);
	UNCATCH(overflow_error);
    }

    if (op_statistics_p() && !ENDP(l_res))
	if (app == is_approximation_must) nb_dinf_must++;

    ifdebug(6)
    {
	pips_debug(6, "Resulting regions : \n");
	fprintf(stderr,"\t l_res :\n");
	print_regions(l_res);
    }

    return l_res;
}




/* static Psysteme disjunction_to_region_sc(Pdisjunct disjonction, boolean *b)
 * input    : a disjunction representing the union of several Psystemes
 * output   : a Psysteme representing the convex_hull of the disjunction
 *            of predicates.
 * modifies : frees the input disjuntion.
 * comment  : Exactness is now tested outside the loop.
 */
static Psysteme disjunction_to_region_sc(Pdisjunct disjonction, boolean *p_exact)
{
    Psysteme ps_res = SC_UNDEFINED;

    pips_assert("disjunction not undefined\n", !(disjonction == DJ_UNDEFINED));

    ps_res = disjonction->psys;

    /* no need to compute the hull if there is only one system in the
     * disjunction */
    if (disjonction->succ != NULL)
    {
	Ppath    chemin;
	Pdisjunct disj_tmp;


	for(disj_tmp = disjonction->succ;
	    disj_tmp != DJ_UNDEFINED;
	    disj_tmp = disj_tmp->succ)
	{
	    ps_res = region_sc_convex_hull(ps_res, disj_tmp->psys);
	}
	/* Exactness test */
	chemin = pa_make(ps_res,disjonction);
	*p_exact = !(pa_feasibility_ofl_ctrl(chemin, FWD_OFL_CTRL));
	disjonction = dj_free(disjonction);
	chemin->pcomp = disjonction;
	chemin = pa_free1(chemin);
    }
    else
    {
	*p_exact = TRUE;
	/* do not freee the inner Psysteme: it is the result */
	disjonction = dj_free1(disjonction);
    }
    return ps_res;
}


/* static list disjuction_to_list_of_regions(disjonction, reg, app, super_or_sub)
 * input    : a disjunction and a region
 * output   : a list of regions which contains a region identical
 *            with reg, but for the predicate which is given by the
 *            Psystemes of the disjunction and the approximation which depends
 *            on the exactness of the representation. The latter is either a sub
 *            or a super approximation of the disjonction, depending on the value
 *            of the boolean super_or_sub.
 * modifies : the disjunction is freed by the call to disjunction_to_region_sc.
 * comment  : app is the tag if the disjunction is exact;
 */
static list disjunction_to_list_of_regions(Pdisjunct disjonction, region reg,
					   tag app,
					   boolean super_or_sub __attribute__ ((unused)))
{
    Psysteme ps;
    region reg_ps;
    list l_reg = NIL;
    boolean exact = TRUE;
    Psysteme ps_tmp;

    ps_tmp = disjunction_to_region_sc(disjonction, &exact);

    /* computation of a subapproximation */
    if (!exact && SUB)
    {
	sc_rm(ps_tmp);
    }
    /* computation of the exact representation or of a super approximation */
    else
    {
	/* build_sc_nredund_2pass(&ps_tmp); */
	ps = ps_tmp;

	if (!sc_empty_p(ps))
	{
	    reg_ps = region_dup(reg);
	    sc_rm(region_system(reg_ps));
	    region_system_(reg_ps) = newgen_Psysteme(ps);
	    app = (exact && (app == is_approximation_must))?
		is_approximation_must : is_approximation_may;
	    region_approximation_tag(reg_ps) = app;
	    l_reg = CONS(EFFECT, reg_ps, l_reg);
	}
	else
	    sc_rm(ps);

    }
    return l_reg;
}


/*************************************************** BOOLEAN FUNCTIONS */

/* bool combinable_regions_p(effect r1, r2)
 * input    : two regions
 * output   : TRUE if r1 and r2 affect the same entity, and, if they
 *            have the same action on it, FALSE otherwise.
 * modifies : nothing.
 */
bool combinable_regions_p(region r1, region r2)
{
    boolean same_var, same_act;

    if (region_undefined_p(r1) || region_undefined_p(r2))
	return(TRUE);

    same_var = (region_entity(r1) == region_entity(r2));
    same_act = action_equal_p(region_action(r1), region_action(r2));

    return(same_var && same_act);
}

boolean regions_same_action_p(region r1, region r2)
{
    boolean same_var, same_act;

    if (region_undefined_p(r1) || region_undefined_p(r2))
	return(TRUE);

    same_var = (region_entity(r1) == region_entity(r2));
    same_act = action_equal_p(region_action(r1), region_action(r2));

    return(same_var && same_act);
}

boolean regions_same_variable_p(region r1, region r2)
{
    boolean same_var = (region_entity(r1) == region_entity(r2));
    return(same_var);
}




/*************************************** PROPER REGIONS TO SUMMARY REGIONS */

/* list proper_to_summary_regions(list l_reg)
 * input    : a list of proper regions: there may be several regions for
 *            a given array with a given action.
 * output   : a list of summary regions, with the property that is only
 *            one region for each array per action.
 * modifies : the input list. Some regions are freed.
 * comment  : The computation scans the first list (the "base" list);
 *            From this element, the next elements are scanned (the "current"
 *            list). If the current region is combinable with the base region,
 *            they are combined. Both original regions are freed, and the base
 *            is replaced by the union.
 */
list proper_to_summary_regions(list l_reg)
{
    return proper_regions_combine(l_reg, FALSE);
}

/* list proper_regions_contract(list l_reg)
 * input    : a list of proper regions
 * output   : a list of proper regions in which there is no two identical
 *            scalar regions.
 * modifies : the input list.
 * comment  : This is used to reduce the number of dependence tests.
 */

list proper_regions_contract(list l_reg)
{
    return proper_regions_combine(l_reg, TRUE);
}


/* list proper_regions_combine(list l_reg, bool scalars_only_p)
 * input    : a list of proper regions, and a boolean to know on which
 *            elements to perform the union.
 * output   : a list of regions, in which the selected elements
 *            have been merged.
 * modifies : the input list.
 * comment  :
 */

list proper_regions_combine(list l_reg, bool scalars_only_p)
{
    list base, current, pred;
    ifdebug(6)
    {
	pips_debug(6, "proper regions: \n");
	print_regions(l_reg);
    }

    base = l_reg;
    /* scan the list of regions */
    while(!ENDP(base) )
    {
	/* scan the next elements to find regions combinable
	 * with the region of the base element.
	 */
	current = CDR(base);
	pred = base;
	while (!ENDP(current))
	{
	    region reg_base = REGION(CAR(base));
	    region reg_current = REGION(CAR(current));

	    /* Both regions are about the same sclar variable,
	       with the same action
	       */
	    if ((!scalars_only_p || region_scalar_p(reg_base)) &&
		regions_same_action_p(reg_base, reg_current) )
	    {
		list tmp;
		region new_reg_base;

		/* compute their union */
		new_reg_base = regions_must_convex_hull(reg_base,reg_current);
		/* free the original regions: no memory leak */
		region_free(reg_base);
		region_free(reg_current);

		/* replace the base region by the new region */
		REGION_(CAR(base)) = new_reg_base;

		/* remove the current list element from the global list */
		tmp = current;
		current = CDR(current);
		CDR(pred) = current;
		free(tmp);
	    }
	    else
	    {
		pred = current;
		current = CDR(current);
	    }
	}
	base = CDR(base);
    }

    ifdebug(6)
    {
	pips_debug(6, "summary regions: \n");
	print_regions(l_reg);
    }
    return(l_reg);
}

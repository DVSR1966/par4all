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
/* package generic effects :  Be'atrice Creusillet 5/97
 *
 * File: binary_operators.c
 * ~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This File contains generic binary operators for effects and lists of them.
 *
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"
#include "misc.h"
#include "text.h"
#include "text-util.h"

#include "effects-generic.h"


/************************************************* GENERIC BINARY OPERATORS */

/**
   beware : modifies l1, l2 and their effects

  @param l1 and l2 are two lists of effects.
  @param  r1_r2_combinable_p is a boolean function that takes two
          individual effects as arguments and renders TRUE when they are
          considered as combinable ;
  @param  r1_r2_binary_op is a binary operator that combines two
          individual effects;
  @param  r1_unary_op is a unary operators that deal with the remnants of l1,
          that is those effects that are not combinable with any effect of l2;
  @param  r2_unary_op is a unary operators that deal with the remnants of l2,
          that is those effects that are not combinable with any effect of l1;

  @return a list of effects, combination of l1 and l2.

*/
list
list_of_effects_generic_binary_op(
    list l1,
    list l2,
    bool (*r1_r2_combinable_p)(effect,effect),
    list (*r1_r2_binary_op)(effect,effect),
    list (*r1_unary_op)(effect),
    list (*r2_unary_op)(effect))
{
    list l_res = NIL;
    list cr1 = list_undefined;
    list cr2 = list_undefined;
    list l_clean_res = NIL;

    debug_on("EFFECTS_OPERATORS_DEBUG_LEVEL");

    pips_debug_effects(1, "Initial effects : \n\t l1 :\n", l1);
    ifdebug(8) {
      fprintf(stderr, "Dump of initial effects l1 : \n");
      dump_effects(l1);
    }
    pips_debug_effects(1, "\t l2 :\n", l2);
    ifdebug(8) {
      fprintf(stderr, "Dump of initial effects l2 : \n");
      dump_effects(l2);
    }


    /* we first deal with the effects of l1 : those that are combinable with
     * the effects of l2, and the others, which we call the remnants of l1 */
    for(cr1 = l1; !ENDP(cr1); POP(cr1)) {
      effect r1 = EFFECT(CAR(cr1));
      list lr2 = l2;
      list prec_lr2 = NIL;
      bool combinable = FALSE;

      pips_debug(8, "r1: %s\n", entity_name(effect_variable(r1)));
      ifdebug(8) {
	fprintf(stderr, "Dump r1 when entering the loop body\n");
	dump_effect(r1);
      }

      while(!combinable && !ENDP(lr2)) {
	effect r2 = EFFECT(CAR(lr2));

	pips_debug(8, "r2: %s\n", entity_name(effect_variable(r2)));

	if ( (*r1_r2_combinable_p)(r1,r2) ) {
	  pips_debug(8, "combinable\n");
	  combinable = TRUE;
	  l_res = gen_nconc((*r1_r2_binary_op)(r1,r2), l_res);

	  /* gen_remove(&l2, EFFECT(CAR(lr2))); */
	  if (prec_lr2 != NIL)
	    CDR(prec_lr2) = CDR(lr2);
	  else
	    l2 = CDR(lr2);

	  free(lr2); lr2 = NIL;
	  /* */
	  free_effect(r1); r1=effect_undefined;
	  free_effect(r2); r2=effect_undefined;
	}
	else {
	  prec_lr2 = lr2;
	  lr2 = CDR(lr2);
	}
      }

      pips_debug_effects(2, "intermediate effects 1:\n", l_res);

      if(!combinable) {
	/* r1 belongs to the remnants of l1 : it is combinable
	 * with no effects of l2 */
	if ( (*r1_r2_combinable_p)(r1,effect_undefined) )
	  l_res = gen_nconc(l_res, (*r1_unary_op)(r1));
      }
    }

    pips_debug_effects(2, "intermediate effects 2:\n", l_res);

    /* we must then deal with the remnants of l2 */
    for(cr2 = l2; !ENDP(cr2); POP(cr2)) {
      effect r2 = EFFECT(CAR(cr2));

      if ( (*r1_r2_combinable_p)(effect_undefined,r2) )
	l_res = gen_nconc(l_res, (*r2_unary_op)(r2));
    }

    pips_debug_effects(1, "effects before cleaning:\n", l_res);

    l_clean_res = clean_anywhere_effects(l_res);
    gen_full_free_list(l_res);

    pips_debug_effects(1, "final effects:\n", l_clean_res);

    /* no memory leaks: l1 and l2 won't be used anymore */
    gen_free_list(l1);
    gen_free_list(l2);

    debug_off();

    return l_clean_res;
}

list proper_to_summary_effects(list l_effects)
{
    return proper_effects_combine(l_effects, FALSE);
}


/* list proper_effects_contract(list l_effects)
 * input    : a list of proper effects
 * output   : a list of proper effects in which there is no two identical
 *            scalar effects.
 * modifies : the input list.
 * comment  : This is used to reduce the number of dependence tests.
 */

list proper_effects_contract(list l_effects)
{
    return(proper_effects_combine(l_effects, TRUE));
}


/* list proper_effects_combine(list l_effects, bool scalars_only_p)
 * input    : a list of proper effects, and a boolean to know on which
 *            elements to perform the combination.
 * output   : a list of effects, in which the selected elements have been
 *            merged.
 * modifies : the input list.
 * comment  : the algorithm is in O(n) (was in (n^2)/2)
 *
 * we need "entity/action" -> consp to check for the
 * condition in the second loop directly.
 * or to simplify the hash management, two entity -> consp?
 * a generic multi key combination hash would help.
 * the list is modified IN PLACE, storing on the first effect encountered...
 *
 * Extensions for C make it more complex as an effect on p and on p[1]
 * are not related. It would be possible to eliminate syntactically
 * equal effects by converting them into strings, without information
 * about the approximation. As a first try, I keep the hash tables
 * based on names and actions, and I only check compatibility in
 * reference and addressing mode afterwards. But this is not working
 * as I do not get enough different entries in the hash table since
 * entity_name is the key.
 *
 * The syntactic elimination is implemented, but the scoping
 * information is left out because the function is shard with the
 * effect prettyprinter. Scoping information should be preserved in the key.
 *
 * Also, it might be useful to normalize the effects and to convert
 * the pointer-based effects into an anywhere effect as soon as the
 * corresponding pointer is written before it is
 * dereferenced. However, we can delay the conversion till needed, as
 * long as the concrete semantics of the effect lists is understoof by
 * the user. This gives us a chance to find later the value of the
 * written pointer and to substitue it where needed. Else, the effects
 * to substitute will be gone.
 */
list proper_effects_combine(list l_effects, bool scalars_only_p)
{
  list cur, pred = NIL;
  /* entity name -> consp in effect list. */
  hash_table all_read_effects, all_write_effects;
  /* FI: at this level, it's pretty dangerous to combine effects with
     no constant addresses */

  ifdebug(6) {
    list refl = NIL;
    int i = 0;
    pips_debug(6, "Proper effects to combine: \n");
    (*effects_prettyprint_func)(l_effects);

    /* This is a pretty weak assert because it's performed on the
       addresses */
    pips_assert("The very same effect does not appear twice",
		gen_once_p(l_effects));

    FOREACH(EFFECT, eff, l_effects) {
	cell c = effect_cell(eff);
	/* FI: the cell tag is not checked here but later a posteriori! */
	reference ref = cell_reference(c);

	if(cell_reference_p(c)) {
	  refl = gen_nconc(refl, CONS(REFERENCE, ref, NIL));
	}
	i++;
	fprintf(stderr, "Effect %d: %p\tReference %d: %p (%spersistant)\n",
		i, eff, i, ref, cell_reference_p(c)? "not ":"");
    }

    pips_assert("The very same reference does not appear twice"
		" unless it is persistant", gen_once_p(refl));
  }

  all_read_effects = hash_table_make(hash_string, 10);
  all_write_effects = hash_table_make(hash_string, 10);
  //all_read_pre_effects = hash_table_make(hash_string, 10);
  //all_write_pre_effects = hash_table_make(hash_string, 10);
  //all_read_post_effects = hash_table_make(hash_string, 10);
  //all_write_post_effects = hash_table_make(hash_string, 10);

  cur = l_effects;
  /* scan the list of effects... the list is modified in place */
  while(!ENDP(cur))
  {
    effect lcurrent = EFFECT(CAR(cur));
    effect current = effect_undefined;
    string n;
    tag a;
    action_kind ak;
    bool may_combine, do_combine = FALSE;
    list do_combine_item = NIL;
    list next = CDR(cur); /* now, as 'cur' may be removed... */

    /* Summarization before combination: let's be as store independent as possible */
    current = (*proper_to_summary_effect_func)(lcurrent);

    ak = action_to_action_kind(effect_action(current));
   // n = entity_name(effect_entity(current));
    list w;
    string rn = words_to_string
      (w=effect_words_reference(effect_any_reference(current)));
    gen_map(free,w);gen_free_list(w);
    /* Do not combine effects of different kinds: use the kind in the key */
    asprintf(&n,"%s %s",rn,action_kind_to_string(ak));
    free(rn);
    a = effect_action_tag(current);

    pips_debug(8,"key: \"\%s\"\n", n);

    /* may/do we have to combine ? */
    /* ??? FC this should be no big deal... anyway :
     * in the previous implementation, 'current' was not yet
     * passed thru proper_to_summary_effect_func when tested...
     *
     * FI: effect_scalar_p() should be redefined because the new key
     * used let us deal with complex effects.
     */
    may_combine = (!scalars_only_p || effect_scalar_p(current));
    //&& !store_effect_p(current);


    /* FI: addressing should be checked below against writing of the
       underlying pointer. No time right now. */

    if (may_combine)
    {
      /* did we see it at a previous iteration? */
      switch (a) {
      case is_action_write:
	if (hash_defined_p(all_write_effects, n))
	{
	  do_combine = TRUE;
	  do_combine_item = hash_get(all_write_effects, n);
	}
	break;
      case is_action_read:
	if (hash_defined_p(all_read_effects, n))
	{
	  do_combine = TRUE;
	  do_combine_item = hash_get(all_read_effects, n);
	}
	break;
      default: pips_internal_error("unexpected action tag %d", a);
      }
    }

    if (do_combine)
    {
      /* YES, me must combine */

      effect base = EFFECT(CAR(do_combine_item));
      if(effect_comparable_p(base, current)) {
	/* compute their union */
	effect combined = (*effect_union_op)(base, current);

	/* replace the base effect by the new effect */
	pips_assert("combined is a consistent effect",
		    effect_consistent_p(combined));
	EFFECT_(CAR(do_combine_item)) = combined;

	/* free the original effects: no memory leak */
	free_effect(base);
	free_effect(current);

	/* remove the current list element from the global list */
	/* pred!=NIL as on the first items hash's are empty */
	CDR(pred) = next; /* pred is not changed! */
	free(cur);
      }
      else {
	do_combine = FALSE;
      }
    }
    if(!do_combine)
    {
      /* NO, just store if needed... */
      EFFECT_(CAR(cur)) = current;
      if (may_combine)
      {
	/* if we do not combine. ONLY IF we test, we put... */
	switch (a) {
	case is_action_write:
	  hash_put(all_write_effects, strdup(n), cur);
	  break;
	case is_action_read:
	  hash_put(all_read_effects, strdup(n), cur);
	  break;
	default: pips_internal_error("unexpected action tag %d", a);
	}
      }
      pred = cur;
    }

    cur = next;
    free(n);
  }

  ifdebug(6){
    pips_debug(6, "summary effects: \n");
    (*effects_prettyprint_func)(l_effects);
  }

  /* The keys should be freed as well */
  hash_table_free(all_write_effects);
  hash_table_free(all_read_effects);

  return l_effects;
}



/******************************************************* BOOL(EAN) FUNCTIONS */


/**
   @param eff1 and eff2 are two effects
   @ return true if their entities are the same, and if their access path
            as described by their references are the same, or if one of
	    the effect is an anywhere effect.
	    false otherwise.
 */
bool effects_combinable_p(effect eff1, effect eff2)
{
  bool same_p = false;
  reference r1 = effect_any_reference(eff1);
  reference r2 = effect_any_reference(eff2);
  action a1 = effect_action(eff1);
  action a2 = effect_action(eff2);
  action_kind ak1 = action_to_action_kind(a1);
  action_kind ak2 = action_to_action_kind(a2);

  if(action_kind_tag(ak1)==action_kind_tag(ak2)) {
    if (anywhere_effect_p(eff1))
      {
	pips_debug(8, "eff1 is an anywhere effect\n");
	if (malloc_effect_p(eff2) || io_effect_p(eff2)
	    || (!get_bool_property("USER_EFFECTS_ON_STD_FILES")
		&& std_file_effect_p(eff2)))
	  same_p = false;
	else
	  same_p = true;
	pips_debug(8, "eff2 is %s a malloc or io effect\n",
		   same_p? "not": "");

      }
    else if (anywhere_effect_p(eff2))
      {
	pips_debug(8, "eff2 is an anywhere effect\n");

	if (malloc_effect_p(eff1) || io_effect_p(eff1)
	    || (!get_bool_property("USER_EFFECTS_ON_STD_FILES")
		&& std_file_effect_p(eff1)))
	  same_p = false;
	else
	  same_p = true;
	pips_debug(8, "eff1 is %s a malloc or io effect\n",
		   same_p? "not": "");
      }
    else if (same_entity_p(reference_variable(r1), reference_variable(r2)))
      {
	string n1 = words_to_string(effect_words_reference(r1));
	string n2 = words_to_string(effect_words_reference(r2));

	same_p = same_string_p(n1,n2);
	free(n1);
	free(n2);
      }
    else
      same_p = false;
  }
  else
    same_p = false; // redundant but clearer

  return same_p;
}


/* FI: same action, but also same variable and same indexing
 *
 * Checking the action kind may sometimes be useless or even wrong.
  */
bool effects_same_action_p(effect eff1, effect eff2)
{
  bool same_p = false;

  if (effect_undefined_p(eff1) || effect_undefined_p(eff2))
    same_p = true;
  else if (effect_action_tag(eff1)!=effect_action_tag(eff2))
    {
      same_p = false;
    }
  else {
    action_kind ak1 = action_to_action_kind(effect_action(eff1));
    action_kind ak2 = action_to_action_kind(effect_action(eff2));
    if(action_kind_tag(ak1)!=action_kind_tag(ak2))
      same_p = false;
    else
      same_p = effects_combinable_p(eff1, eff2);
  }

  return same_p;
}

bool effects_same_variable_p(effect eff1, effect eff2)
{
    bool same_var = (effect_entity(eff1) == effect_entity(eff2));
    return(same_var);
}


bool r_r_combinable_p(effect eff1, effect eff2)
{
    bool combinable_p, act_combinable;

    if (effect_undefined_p(eff1))
	return(effect_read_p(eff2));

    if (effect_undefined_p(eff2))
	return(effect_read_p(eff1));

    combinable_p = effects_combinable_p(eff1,eff2);
    act_combinable = (effect_read_p(eff1) && effect_read_p(eff2));

    return(combinable_p && act_combinable);
}

bool w_w_combinable_p(effect eff1, effect eff2)
{
    bool combinable_p, act_combinable;

    if (effect_undefined_p(eff1))
	return(effect_write_p(eff2));

    if (effect_undefined_p(eff2))
	return(effect_write_p(eff1));

    combinable_p = effects_combinable_p(eff1,eff2);
    act_combinable = (effect_write_p(eff1) && effect_write_p(eff2));

    return(combinable_p && act_combinable);
}

bool r_w_combinable_p(effect eff1, effect eff2)
{
    bool combinable_p, act_combinable;

    if (effect_undefined_p(eff1))
	return(effect_write_p(eff2));

    if (effect_undefined_p(eff2))
	return(effect_read_p(eff1));

    combinable_p = effects_combinable_p(eff1,eff2);
    act_combinable = (effect_read_p(eff1) && effect_write_p(eff2));

    return(combinable_p && act_combinable);
}

bool w_r_combinable_p(effect eff1, effect eff2)
{
    bool combinable_p, act_combinable;

    if (effect_undefined_p(eff1))
	return(effect_read_p(eff2));

    if (effect_undefined_p(eff2))
	return(effect_write_p(eff1));

    combinable_p = effects_combinable_p(eff1,eff2);
    act_combinable = (effect_write_p(eff1) && effect_read_p(eff2));

    return(combinable_p && act_combinable);
}

/***********************************************************************/
/* UNDEFINED BINARY OPERATOR                                           */
/***********************************************************************/

list
effects_undefined_binary_operator(list l1, list l2,
				  bool (*effects_combinable_p)(effect, effect))
{
  pips_assert("unused arguments", l1==l1 && l2==l2 &&
	      effects_combinable_p==effects_combinable_p);
  return list_undefined;
}


/***********************************************************************/
/* SOME BINARY OPERATORS which do not depend on the representation     */
/***********************************************************************/

/* list effect_entities_intersection(effect eff1, effect eff2)
 * input    : two effects
 * output   : a mere copy of the first effect.
 * modifies : nothing.
 * comment  : We assume that both effects concern the same entity.
 */
static list
effect_entities_intersection(effect eff1, effect eff2)
{
  pips_assert("unused argument", eff2==eff2);
  return CONS(EFFECT, (*effect_dup_func)(eff1), NIL);
}

/* list effects_entities_intersection(list l1, list l2,
                           bool (*intersection_combinable_p)(effect, effect))
 * input    : two lists of effects.
 * output   : a list of effects containing all the effects of l1 that have
 *            a corresponding effect (i.e. same entity) in l2.
 * modifies : l1 and l2.
 * comment  :
 */
list
effects_entities_intersection(list l1, list l2,
			      bool (*intersection_combinable_p)(effect, effect))
{
    list l_res = NIL;

    pips_debug(3, "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   intersection_combinable_p,
					   effect_entities_intersection,
					   effect_to_nil_list,
					   effect_to_nil_list);
    pips_debug(3, "end\n");

    return l_res;
}


/* list effects_entities_inf_difference(list l1, l2)
 * input    : two lists of effects
 * output   : a list of effects, such that: if there is a effect R concerning
 *            entity A in l1 and in l2, then R is removed from the result;
 *            if there is a effect R concerning array A in l1, but not in l2,
 *            then it is kept in l1, and in the result.
 * modifies : the effects of l2 may be freed.
 * comment  : we keep the effects of l1 that are not combinable with those
 *            of l2, but we don't keep the effects of l2 that are not
 *            combinable with those of l_reg1.
 */
list
effects_entities_inf_difference(
    list l1,
    list l2,
    bool (*difference_combinable_p)(effect, effect))
{
    list l_res = NIL;

    debug(3, "RegionsEntitiesInfDifference", "begin\n");
    l_res = list_of_effects_generic_binary_op(l1, l2,
					   difference_combinable_p,
					   effects_to_nil_list,
					   effect_to_list,
					   effect_to_nil_list);
    debug(3, "RegionsEntitiesInfDifference", "end\n");

    return l_res;
}

/* that is all
 */

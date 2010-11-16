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
/* Created by B. Apvrille, april 11th, 1994 */
/* functions related to types effects and effect, cell, reference and
   gap */

#include <stdio.h>

#include "linear.h"

#include "genC.h"

#include "ri.h"
#include "effects.h"

/* #include"mapping.h" */
#include "misc.h"

#include "ri-util.h"
#include "effects-util.h"
#include "text.h"
#include "text-util.h"


/* functions for entity */
entity effect_entity(effect e)
{
  return(cell_entity(effect_cell(e)));
}

entity cell_entity(cell c)
{
  if (cell_gap_p(c)) return entity_undefined;

  else return(reference_variable(cell_any_reference(c)));
}

/* API for reference */

reference cell_any_reference(cell c)
{
  if (cell_gap_p(c)) return reference_undefined;

  return (cell_reference_p(c)) ? cell_reference(c) :
    preference_reference(cell_preference(c));
}

/* Does the set of locations referenced by r depend on a pointer
   dereferencing?

   Let's hope that all Fortran 77 references will return false...
*/
bool memory_dereferencing_p(reference r)
{
  bool dereferencing_p = FALSE;
  entity v = reference_variable(r);
  type vt = entity_type(v);
  type uvt = ultimate_type(vt);
  list sl = reference_indices(r); // subscript list

  /* Get rid of simple Fortran-like array accesses */
  if(gen_length(sl)==gen_length(variable_dimensions(type_variable(vt)))) {
    /* This is a simple array access */
    dereferencing_p = FALSE;
  }
  else if(!ENDP(sl)) {
    /* cycle with alias-classes library: import explictly */
    bool entity_abstract_location_p(entity);
    if(entity_abstract_location_p(v)) {
      pips_internal_error("Do we want to subscript abstract locations?\n");
    }
    else if(FALSE /* entity_heap_variable_p(v)*/) {
      /* Heap modelization is behind*/
    }
    else if(entity_variable_p(v)) {
    /* Let's walk the subscript list and see if the type associated
       to the nth subscript is a pointer type and if the (n+1)th
       subscript is a zero. */
      if(pointer_type_p(uvt))
	/* Since it is subscripted, there is dereferencing */
	dereferencing_p = TRUE;
      else {
	list csl = sl; // current subscript list
	type ct = uvt; // current type
	while(!dereferencing_p && !ENDP(csl)) {
	  expression se = EXPRESSION(CAR(csl));
	  ct = ultimate_type(subscripted_type_to_type(ct, se));
	  if(pointer_type_p(ct) && !ENDP(csl)) {
	    dereferencing_p = TRUE;
	  }
	  else {
	    POP(csl);
	  }
	}
      }
    }
    else {
      pips_internal_error("Unexpected entity kind \"%s\"", entity_name(v));
    }
  }
  else {
    /* No dereferencing */
    ;
  }

  return dereferencing_p;
}


/* Future API for GAP, Generic Access Path*/

/* ---------------------------------------------------------------------- */
/* list-effects conversion functions                                      */
/* ---------------------------------------------------------------------- */

effects list_to_effects(l_eff)
list l_eff;
{
    effects res = make_effects(l_eff);
    return res;
}

list effects_to_list(efs)
effects efs;
{
    list l_res = effects_effects(efs);
    return l_res;
}

statement_mapping listmap_to_effectsmap(l_map)
statement_mapping l_map;
{
    statement_mapping efs_map = MAKE_STATEMENT_MAPPING();

    STATEMENT_MAPPING_MAP(s,val,{
	hash_put((hash_table) efs_map, (char *) s, (char *) list_to_effects((list) val));
    }, l_map);

    return efs_map;
}

statement_mapping effectsmap_to_listmap(efs_map)
statement_mapping efs_map;
{
    statement_mapping l_map = MAKE_STATEMENT_MAPPING();

    STATEMENT_MAPPING_MAP(s,val,{
	hash_put((hash_table) l_map, (char *) s, (char *) effects_to_list((effects) val));
    }, efs_map);

    return l_map;
}



/* Return TRUE if the statement has a write effect on at least one of
   the argument (formal parameter) of the module. Note that the return
   variable of a function is also considered here as a formal
   parameter. */
bool
statement_has_a_module_formal_argument_write_effect_p(statement s,
                                                      entity module,
                                                      statement_mapping effects_list_map)
{
   bool write_effect_on_a_module_argument_found = FALSE;
   list effects_list = (list) GET_STATEMENT_MAPPING(effects_list_map, s);

   MAP(EFFECT, an_effect,
       {
          entity a_variable = reference_variable(effect_any_reference(an_effect));

          if (action_write_p(effect_action(an_effect))
              && (variable_return_p(a_variable)
		  || variable_is_a_module_formal_parameter_p(a_variable,
							     module))) {
	      write_effect_on_a_module_argument_found = TRUE;
             break;
          }
       },
       effects_list);

   return write_effect_on_a_module_argument_found;

}



/* Anywhere effect: an effect which can be related to any location of any areas */

/* Allocate a new anywhere effect, and the anywhere entity on demand
   which may not be best if we want to express it's aliasing with all
   module areas. In the later case, the anywhere entity should be
   generated by bootstrap and be updated each time new areas are
   declared by the parsers. I do not use a persistant anywhere
   reference to avoid trouble with convex-effect nypassing of the
   persistant pointer.

   Action a is integrated in the new effect (aliasing).
   NOT GENERIC AT ALL. USE make_anywhere_effect INSTEAD (BC).
 */
effect anywhere_effect(action ac)
{
  entity anywhere = entity_all_locations();
  effect any = effect_undefined;

  any = make_effect(make_cell_reference(make_reference(anywhere, NIL)),
		    ac,
		    make_approximation_may(),
		    make_descriptor_none());

  return any;
}

/* Is it an anywhere effect? */
bool anywhere_effect_p(effect e)
{
  bool anywhere_p;
  reference r = effect_any_reference(e);
  entity v = reference_variable(r);

  anywhere_p =  entity_all_locations_p(v);

  return anywhere_p;
}

effect heap_effect(entity m, action ac)
{
  entity heap = global_name_to_entity(entity_local_name(m), HEAP_AREA_LOCAL_NAME );
  effect any = effect_undefined;

  if(entity_undefined_p(heap)) {
    pips_internal_error("Heap for module \"%s\" not found\n", entity_name(m));
  }

  any = make_effect(make_cell_reference(make_reference(heap, NIL)),
		    ac,
		    make_approximation_may(),
		    make_descriptor_none());

  return any;
}

bool heap_effect_p(effect e)
{
  bool heap_p;
  reference r = effect_any_reference(e);
  entity v = reference_variable(r);

  heap_p = same_string_p(entity_local_name(v), HEAP_AREA_LOCAL_NAME);

  return heap_p;
}

bool malloc_effect_p(effect e)
{
  bool heap_p;
  reference r = effect_any_reference(e);
  entity v = reference_variable(r);

  heap_p = same_string_p(entity_local_name(v), MALLOC_EFFECTS_NAME);

  return heap_p;
}

/*************** I/O EFFECTS *****************/
bool io_effect_entity_p(entity e)
{
    return io_entity_p(e) &&
	same_string_p(entity_local_name(e), IO_EFFECTS_ARRAY_NAME);
}

bool io_effect_p(effect e)
{
  return io_effect_entity_p(reference_variable(effect_any_reference(e)));
}

bool std_file_effect_p(effect e)
{
  const char * s = entity_user_name(effect_entity(e));
  return(same_string_p(s, "stdout") || same_string_p(s, "stdin") || same_string_p(s, "stderr"));
}

/* Can we merge these two effects because they are equal or because
   they only differ by their approximations and their descriptors? */
bool effect_comparable_p(effect e1, effect e2)
{
  bool comparable_p = FALSE;
  reference r1 = effect_any_reference(e1);
  reference r2 = effect_any_reference(e2);
  entity v1 = reference_variable(r1);
  entity v2 = reference_variable(r2);

  if(v1==v2) {
    action a1 = effect_action(e1);
    action a2 = effect_action(e2);
    if(action_equal_p(a1, a2))
      {

	/* Check the subscript lists because p and p[0] do not refer
	   the same memory locations at all */
	list sl1 = reference_indices(r1);
	list sl2 = reference_indices(r2);
	if(gen_length(sl1)==gen_length(sl2))
	  {
	    list csl1 = list_undefined;
	    list csl2 = list_undefined;
	    bool equal_p = TRUE;

	    for(csl1=sl1, csl2=sl2; !ENDP(csl1) && equal_p; POP(csl1), POP(csl2))
	      {
		expression e1 = EXPRESSION(CAR(csl1));
		expression e2 = EXPRESSION(CAR(csl2));
		equal_p = expression_equal_p(e1, e2);
	      }
	    comparable_p = equal_p;
	  }

      }
  }

  return comparable_p;
}


/* Does this effect define the same set of memory locations
   regardless of the current (environment and) memory state?
 */
bool store_independent_effect_p(effect eff)
{
  bool independent_p = FALSE;

  ifdebug(1) {
    reference r = effect_any_reference(eff);
    pips_assert("Effect eff is consistent", effect_consistent_p(eff));
    pips_assert("The reference is consistent", reference_consistent_p(r));
  }

  if(anywhere_effect_p(eff))
    independent_p = TRUE;
  else {
    reference r = effect_any_reference(eff);
    entity v = reference_variable(r);
    type t = ultimate_type(entity_type(v));

    if(pointer_type_p(t)) {
      list inds = reference_indices(r);

      independent_p = ENDP(inds);
    }
    else {
      pips_assert("The reference is consistent", reference_consistent_p(r));

      independent_p = reference_with_constant_indices_p(r);
    }
  }

  return independent_p;
}

/* FI: I use this predicate to guard the use-def chains computation */
bool fortran_compatible_effect_p(effect e)
{
  bool compatible_p = FALSE;
  reference r = effect_any_reference(e);
  entity v = reference_variable(r);
  type ut = ultimate_type(entity_type(v));
  list ind = reference_indices(effect_any_reference(e));

  /* Basically, this is about a Fortran effect... */
  if((!pointer_type_p(ut) || ENDP(ind))) {
    compatible_p = TRUE;
  }

  return compatible_p;
}


/* Two effects interfere if one of them modify the set of locations
   defined by the other one. For instance, an index or a pointer may
   be used by one effect and changed by the other one.

   If a subscript expression is changed, the corresponding subscript
   must be replaced by an unbounded expression.

   If a pointer is written, any indirect effect thru this pointer must
   be changed into a read or write anywhere.

   This function is conservative: it is always correct to declare an interference.

   FI: I'm not sure what you can do when you know two effects interfere...
 */
bool effects_interfere_p(effect eff1, effect eff2)
{
  action ac1 = effect_action(eff1);
  action ac2 = effect_action(eff2);
  bool interfere_p = FALSE;

  if(action_write_p(ac1)||action_write_p(ac2)) {
    if(anywhere_effect_p(eff1) && action_write_p(ac1)) {
      interfere_p = !store_independent_effect_p(eff2);
    }
    else if(anywhere_effect_p(eff2) && action_write_p(ac2)) {
      interfere_p = !store_independent_effect_p(eff1);
    }
    else { /* dealing with standard effects */

      /* start with complex cases */
      /* The write effect is a direct effet, the other effect may be
	 direct or indirect, indexed or not. */
      reference wr = reference_undefined;
      entity wv = entity_undefined;
      reference rr = reference_undefined;
      entity rv = entity_undefined;

      list rind = list_undefined;
      list wind = list_undefined;

      if(action_write_p(ac1)) {
	wr = effect_any_reference(eff1);
	rr = effect_any_reference(eff2);
      }
      else {
	wr = effect_any_reference(eff2);
	rr = effect_any_reference(eff1);
      }

      wv = reference_variable(wr);
      wind = reference_indices(wr);
      rv = reference_variable(rr);
      rind = reference_indices(rr);

      /* Does the write impact the indices of the read? */
      MAP(EXPRESSION, s, {
	  list rl = NIL;

	  rl = expression_to_reference_list(s, rl);
	  MAP(REFERENCE, r, {
	      entity v = reference_variable(r);
	      if(wv==v) {
		interfere_p = TRUE;
		break;
	      }
	    }, rl);
	  if(interfere_p)
	    break;
	}, rind);

      //interfere_p = TRUE;
    }
  }
  return interfere_p;
}

effect effect_to_store_independent(effect eff)
{
  reference r = effect_any_reference(eff);
  entity v = reference_variable(r);
  type t = ultimate_type(entity_type(v));

  if(pointer_type_p(t)) {
    effect n_eff = anywhere_effect(copy_action(effect_action(eff)));
    free_effect(eff);
    eff = n_eff;
  }
  else{
    list ind = reference_indices(r);
    list cind = list_undefined;

    for(cind = ind; !ENDP(cind); POP(cind)) {
      expression e = EXPRESSION(CAR(cind));

      if(!unbounded_expression_p(e)) {
	if(!extended_integer_constant_expression_p(e)) {
	  free_expression(e);
	  EXPRESSION_(CAR(cind)) = make_unbounded_expression();
	}
      }
    }
  }

  return eff;
}

/* Modify eff so that the set of memory locations decribed after a
   write to some pointer p is still in the abstract location set of eff.

   \\n_eff
   p = ...;
   \\ eff of stmt s
   s;

   If p is undefined, assumed that any pointer may have been updated.

   As a pointer could be used in indexing, the current implementation
   is not correct/sufficient
 */
effect effect_to_pointer_store_independent_effect(effect eff, entity p)
{
  reference r = effect_any_reference(eff);
  entity v = reference_variable(r);
  //type t = ultimate_type(entity_type(v));

  if(entity_undefined_p(p) || v==p) {
    if(!ENDP(reference_indices(r))) {
      /* p[i][j] cannot be preserved */
      effect n_eff = anywhere_effect(copy_action(effect_action(eff)));
      free_effect(eff);
      eff = n_eff;
    }
    else {
      /* No problem: direct scalar reference */
      ;
    }
  }
  return eff;
}

/* Modify eff so that the set of memory locations decribed after a
   write to some non pointer variable is still in the abstract location set of eff.

   \\n_eff
   i = ...;
   \\ eff of stmt s: p[j], q[i],..
   s;
 */
effect effect_to_non_pointer_store_independent_effect(effect eff)
{
  reference r = effect_any_reference(eff);
  //entity v = reference_variable(r);
  //type t = ultimate_type(entity_type(v));

  r = reference_with_store_independent_indices(r);

  return eff;
}

  /* Modifies effect eff1 to make sure that any memory state
     modification abstracted by eff2 preserves the correctness of
     eff1: all memory locations included in eff1 at input are included
     in the memory locations abstracted by the new eff1 after the
     abstract state transition.

     FI: seems to extend naturally to new kinds of effects...
 */
effect effect_interference(effect eff1, effect eff2)
{
  //action ac1 = effect_action(eff1);
  action ac2 = effect_action(eff2);
  effect n_eff1 = eff1; /* default value */

  ifdebug(1) {
    pips_assert("The new effect is consistent", effect_consistent_p(eff1));
    pips_assert("The new effect is consistent", effect_consistent_p(eff2));
  }

  if(store_independent_effect_p(eff1)) {
    /* nothing to worry about */
    ;
  }
  else if(action_write_p(ac2)) {
    if(anywhere_effect_p(eff2)) {
      // free_effect(eff1);
      n_eff1 = effect_to_store_independent(eff1);
    }
    else {
      reference r2 = effect_any_reference(eff2);
      entity v2 = reference_variable(r2);
      type t2 = ultimate_type(entity_type(v2));

      if(pointer_type_p(t2)) {
	/* pointer-dependence write, indexed or not */
	n_eff1 = effect_to_pointer_store_independent_effect(eff1, v2);
      }
      else {
	/* The base address for the write is constant, the indices should be be checked */
	/* The write effect is a direct effet, the other effect may be
	   direct or indirect, indexed or not. */
	reference r1 = effect_any_reference(eff1);
	list ind1 = reference_indices(r1);
	list cind1 = list_undefined;

	/* FI: should be very similar to reference_with_store_independent_indices()? */

	/* Does the write impact some indices of the read? */
	for(cind1 = ind1; !ENDP(ind1); POP(ind1)) {
	  expression s = EXPRESSION(CAR(cind1));
	  list rl = NIL;
	  list crl = list_undefined;
	  bool interfere_p = FALSE;

	  rl = expression_to_reference_list(s, rl);
	  for(crl=rl; !ENDP(rl); POP(rl)) {
	    reference r = REFERENCE(CAR(crl));
	    entity v = reference_variable(r);
	    if(v2==v) {
	      interfere_p = TRUE;
	      break;
	    }
	  }

	  if(interfere_p) {
	    pips_debug(8, "Interference detected\n");
	    /* May be shared because of persistant references */
	    //free_expression(s);
	    EXPRESSION_(CAR(ind1)) = make_unbounded_expression();
	  }
	}
      }
    }
  }
  ifdebug(1)
    pips_assert("The new effect is consistent", effect_consistent_p(n_eff1));
  return n_eff1;
}

/* Functions dealing with actions */

string action_to_string(action ac)
{
  /* This is correct, but imprecise when action_kinds are taken into
     account */
  return action_read_p(ac)? "read" : "write";
}

string full_action_to_string(action ac)
{
  string s = string_undefined;
  if(action_read_p(ac)) {
    action_kind ak = action_read(ac);

    if(action_kind_store_p(ak))
      s = "read memory";
    else if(action_kind_environment_p(ak))
      s = "read environment";
    else if(action_kind_type_declaration_p(ak))
      s = "read type";
  }
  else {
    action_kind ak = action_write(ac);

    if(action_kind_store_p(ak))
      s = "write memory";
    else if(action_kind_environment_p(ak))
      s = "write environment";
    else if(action_kind_type_declaration_p(ak))
      s = "write type";
  }
  return s;
}

string full_action_to_short_string(action ac)
{
  string s = string_undefined;
  if(action_read_p(ac)) {
    action_kind ak = action_read(ac);

    if(action_kind_store_p(ak))
      s = "R";
    else if(action_kind_environment_p(ak))
      s = "RE";
    else if(action_kind_type_declaration_p(ak))
      s = "RT";
  }
  else {
    action_kind ak = action_write(ac);

    if(action_kind_store_p(ak))
      s = "W";
    else if(action_kind_environment_p(ak))
      s = "WE";
    else if(action_kind_type_declaration_p(ak))
      s = "WT";
  }
  return s;
}

string action_kind_to_string(action_kind ak)
{
  string s = string_undefined;

  if(action_kind_store_p(ak))
    s = "S";
  else if(action_kind_environment_p(ak))
    s = "E";
  else if(action_kind_type_declaration_p(ak))
    s = "T";
  else
    pips_internal_error("Unknown action kind.\n");
  return s;
}

/* To ease the extension of action with action_kind */
action make_action_write_memory(void)
{
  action a = make_action_write(make_action_kind_store());
  return a;
}

action make_action_read_memory(void)
{
  action a = make_action_read(make_action_kind_store());
  return a;
}

bool action_equal_p(action a1, action a2)
{
  bool equal_p = FALSE;

  if(action_tag(a1)==action_tag(a2)) {
    if(action_read_p(a1)) {
      action_kind ak1 = action_read(a1);
      action_kind ak2 = action_read(a2);

      equal_p = action_kind_tag(ak1)==action_kind_tag(ak2);
    }
    else /* action_write_p(a1) */ {
      action_kind ak1 = action_write(a1);
      action_kind ak2 = action_write(a2);

      equal_p = action_kind_tag(ak1)==action_kind_tag(ak2);
    }
  }
  return equal_p;

}

/* Without the consistency test, this function would certainly be
   inlined. Macros are avoided to simplify debugging and
   maintenance. */
action_kind action_to_action_kind(action a)
{
  action_kind ak = action_read_p(a) ? action_read(a): action_write(a);

  if(!action_read_p(a) && !action_write_p(a))
    pips_internal_error("Inconsistent action kind.\n");

  return ak;
}

bool store_effect_p(effect e)
{
  action a = effect_action(e);
  action_kind ak = action_read_p(a)? action_read(a) : action_write(a);
  bool store_p = action_kind_store_p(ak);

  return store_p;
}

bool environment_effect_p(effect e)
{
  action a = effect_action(e);
  action_kind ak = action_read_p(a)? action_read(a) : action_write(a);
  bool store_p = action_kind_environment_p(ak);

  return store_p;
}

bool type_declaration_effect_p(effect e)
{
  action a = effect_action(e);
  action_kind ak = action_read_p(a)? action_read(a) : action_write(a);
  bool store_p = action_kind_type_declaration_p(ak);

  return store_p;
}

bool effects_write_variable_p(list el, entity v)
{
  bool result = FALSE;
  FOREACH(EFFECT, e, el) {
    action a  = effect_action(e);
    entity ev = effect_entity(e);
    if (action_write_p(a) && ev == v) {
      result = TRUE;
      break;
    }
  }
  return result;
}

bool effects_read_variable_p(list el, entity v)
{
  bool result = FALSE;
  FOREACH(EFFECT, e, el) {
    action a  = effect_action(e);
    entity ev = effect_entity(e);
    if (action_read_p(a) && ev == v) {
      result = TRUE;
      break;
    }
  }
  return result;
}

/* Check that all effects in el are read effects */
bool effects_all_read_p(list el)
{
  bool result = TRUE;
  FOREACH(EFFECT, e, el) {
    action a  = effect_action(e);
    //entity ev = effect_entity(e);
    if (action_write_p(a)) {
      result = FALSE;
      break;
    }
  }
  return result;
}

/* Check if some references might be freed with the effects. This may
   lead to disaster if the references are part of another PIPS data
   structure. This information is not fully accurate, but
   conservative. */
bool effect_list_can_be_safely_full_freed_p(list el)
{
  bool safe_p = TRUE;
  FOREACH(EFFECT, e, el) {
    cell c = effect_cell(e);
    if(cell_reference_p(c)) {
      reference r = cell_reference(c);
      list inds = reference_indices(r);

      if(ENDP(inds)) {
	/* The free is very likely to be unsafe */
	//entity v = reference_variable(r);
	safe_p = FALSE;
	//fprintf(stderr, "cell_reference for %s", entity_name(v));
      }
      else {
	/* Is it a possible C reference or is it a synthetic reference
	   generated by the effect analysis? Hard to decide... */
	//entity v = reference_variable(r);
	//fprintf(stderr, "cell_reference for %s", entity_name(v));
	//print_reference(r);
      }
      break;
    }
  }
  return safe_p;
}


/******************************************* COMBINATION OF APPROXIMATIONS */



/* tag approximation_and(tag t1, tag t2)
 * input    : two approximation tags.
 * output   : the tag representing their "logical and", assuming that
 *            must = true and may = false.
 * modifies :  nothing
 */
tag approximation_and(tag t1, tag t2)
{
    if ((t1 == is_approximation_must) && (t2 == is_approximation_must))
	return(is_approximation_must);
    else
	return(is_approximation_may);
}


/* tag approximation_or(tag t1, tag t2)
 * input    : two approximation tags.
 * output   : the tag representing their "logical or", assuming that
 *            must = true and may = false.
 * modifies : nothing
 */
tag approximation_or(tag t1, tag t2)
{
    if ((t1 == is_approximation_must) || (t2 == is_approximation_must))
	return(is_approximation_must);
    else
	return(is_approximation_may);
}


/** CELLS */
/* test if two cells are equal, celles are supposed to be references.*/

bool cell_equal_p(cell c1, cell c2)
{
  /* Has to be extended for GAPs */
  reference r1 = cell_to_reference(c1);
  reference r2 = cell_to_reference(c2);
  return reference_equal_p(r1, r2);
}


/* FI: probably to be moved elsewhere in ri-util */
/* Here, we only know how to cope (for the time being) with
   cell_reference and cell_preference, not with cell_gap and other
   future fields. A bit safer than macro cell_any_reference(). */
reference cell_to_reference(cell c) {
  reference r = reference_undefined;

  if (cell_reference_p(c))
    r = cell_reference(c);
  else if (cell_preference_p(c))
    r = preference_reference(cell_preference(c));
  else
    pips_internal_error("unexpected cell tag\n");

  return r;
}

/* Debugging */
bool effect_list_consistent_p(list el)
{
  bool ok_p = TRUE;

  FOREACH(EFFECT, e, el)
    ok_p = ok_p && effect_consistent_p(e);

  return ok_p;
}

/* Check compatibility conditions for effect union */
bool union_compatible_effects_p(effect ef1, effect ef2)
{
  action a1 = effect_action(ef1);
  tag at1 = action_tag(a1);
  action_kind ak1 = action_to_action_kind(a1);
  tag akt1 = action_kind_tag(ak1);
  entity e1 = effect_variable(ef1);
  descriptor d1 = effect_descriptor(ef1);
  action a2 = effect_action(ef2);
  tag at2 = action_tag(a2);
  action_kind ak2 = action_to_action_kind(a2);
  tag akt2 = action_kind_tag(ak2);
  entity e2 = effect_variable(ef2);
  descriptor d2 = effect_descriptor(ef2);
  bool compatible_p = TRUE;

  pips_assert("effect e1 is consistent", effect_consistent_p(ef1));
  pips_assert("effect e2 is consistent", effect_consistent_p(ef2));

  if(at1!=at2) {
    /* In general, you do not want to union a read and a write, but
       you might want to do so to generate the set of referenced
       elements, for instance to generate communications or to
       allocate memory */
    compatible_p = FALSE;
  }
  else if(akt1!=akt2) {
    /* You do not want to union an effect on store with an effect on
       environment or type declaration */
    compatible_p = FALSE;
  }
  else {
    /* Here we know: at1==at2 and akt1==akt2 */
    /* The code below could be further unified, but it would not make
       it easier to understand */
    if(akt1==is_action_kind_store) {
      if(e1!=e2)
	compatible_p = FALSE;
      else {
	tag dt1 = descriptor_tag(d1);
	tag dt2 = descriptor_tag(d2);

	if(dt1!=dt2)
	  compatible_p = FALSE;
      }
    }
    else {
      /* For environment and type declaration, the descriptor is
	 useless for the time being */
      compatible_p = e1==e2;
    }
  }

  return compatible_p;
}

/* Returns the entity corresponding to the mutation. It could be
   called effect_to_variable(), but effects are sometimes summarized
   with abstract locations, i.e. sets of locations. */
entity effect_to_entity(effect ef)
{
  /* FI unlikely to work with GAPs */
  reference r = effect_any_reference(ef);
  entity e = reference_variable(r);

  return e;
}

/* boolean vect_contains_phi_p(Pvecteur v)
 * input    : a vector
 * output   : TRUE if v contains a PHI variable, FALSE otherwise
 * modifies : nothing
 */
boolean vect_contains_phi_p(Pvecteur v)
{
    for(; !VECTEUR_NUL_P(v); v = v->succ)
	if (variable_phi_p((entity) var_of(v)))
	    return(TRUE);

    return(FALSE);
}

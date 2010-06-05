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
 * File: utils.c
 * ~~~~~~~~~~~~~~~~~
 *
 * This File contains various useful functions, some of which should be moved
 * elsewhere.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"
#include "misc.h"
#include "text-util.h"
#include "properties.h"
#include "preprocessor.h"

#include "effects-generic.h"
#include "effects-convex.h"
#include "alias-classes.h"


/********************************************************************* MISC */

static bool constant_paths_p = FALSE;

void 
set_constant_paths_p(bool b)
{
  constant_paths_p = b;
}

bool
get_constant_paths_p()
{
  return constant_paths_p;
}


/* Statement stack to walk on control flow representation */
DEFINE_GLOBAL_STACK(effects_private_current_stmt, statement)

/* Context stack to keep current context when walking on expressions */
DEFINE_GLOBAL_STACK(effects_private_current_context, transformer)

bool 
effects_private_current_context_stack_initialized_p()
{
    return (effects_private_current_context_stack != stack_undefined);
}

bool 
normalizable_and_linear_loop_p(entity index, range l_range)
{
    Value incr = VALUE_ZERO;
    normalized nub, nlb;
    expression e_incr = range_increment(l_range);
    normalized n;
    bool result = TRUE;
    
    /* Is the loop index an integer variable */
    if (! entity_integer_scalar_p(index))
    {
	pips_user_warning("non integer scalar loop index %s.\n", 
			  entity_local_name(index));
	result = FALSE;	
    }
    else
    {
	/* Is the loop increment numerically known ? */
	n = NORMALIZE_EXPRESSION(e_incr);
	if(normalized_linear_p(n))
	{
	    Pvecteur v_incr = normalized_linear(n);
	    if(vect_constant_p(v_incr)) 
		incr = vect_coeff(TCST, v_incr);	
	}
	
	nub = NORMALIZE_EXPRESSION(range_upper(l_range));
	nlb = NORMALIZE_EXPRESSION(range_lower(l_range));    
	
	result = value_notzero_p(incr) && normalized_linear_p(nub) 
	    && normalized_linear_p(nlb);
    }
    
    return(result);
}

transformer
transformer_remove_variable_and_dup(transformer orig_trans, entity ent)
{
    transformer res_trans = transformer_undefined;

    if (orig_trans != transformer_undefined)
    {
	res_trans = copy_transformer(orig_trans);	
	gen_remove(&transformer_arguments(res_trans), ent);
    }
    return(res_trans);
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

/**************************************** DESCRIPTORS (should not be there) */

static bool descriptor_range_p = FALSE;

void
set_descriptor_range_p(bool b)
{
    descriptor_range_p = b;
}

bool
get_descriptor_range_p(void)
{
    return(descriptor_range_p);
}

descriptor
descriptor_inequality_add(descriptor d, Pvecteur v)
{
    if (!VECTEUR_UNDEFINED_P(v))
    {
	Psysteme sc = descriptor_convex(d);
	Pcontrainte contrainte = contrainte_make(v);
	sc_add_inegalite(sc, contrainte);   
	sc->base = BASE_NULLE;
	sc_creer_base(sc);
	descriptor_convex_(d) = newgen_Psysteme(sc);
    }
    return d;
}

transformer
descriptor_to_context(descriptor d)
{
    transformer context;
    if (descriptor_none_p(d))
	context = transformer_undefined;
    else
    {
	Psysteme sc = sc_dup(descriptor_convex(d));
	context = make_transformer(NIL, make_predicate(sc));
    }
    return(context);
}

void
descriptor_variable_rename(descriptor d, entity old_ent, entity new_ent)
{
    if (descriptor_convex_p(d))
    {
	sc_variable_rename(descriptor_convex(d), 
			   (Variable) old_ent, 
			   (Variable) new_ent);
    }
}

descriptor
descriptor_append(descriptor d1, descriptor d2)
{
    if (descriptor_convex_p(d1) && descriptor_convex_p(d2))
    {
	Psysteme
	    sc1 = descriptor_convex(d1),
	    sc2 = descriptor_convex(d2);
	
	sc1 = sc_safe_append(sc1, sc2);
	descriptor_convex_(d1) = newgen_Psysteme(sc1);
    }	
    else
	d1 = descriptor_undefined;
    return d1;
}

/***********************************************************************/
/*                                                                     */
/***********************************************************************/

transformer
load_undefined_context(statement s __attribute__ ((__unused__)) )
{
    return transformer_undefined;
}

transformer
load_undefined_transformer(statement s __attribute__ ((__unused__)) )
{
    return transformer_undefined;
}

bool
empty_context_test_false(transformer context __attribute__ ((__unused__)) )
{
    return FALSE;
}

void 
effects_computation_no_init(string module_name __attribute__ ((__unused__)) )
{
    return;
}

void 
effects_computation_no_reset(string module_name __attribute__ ((__unused__)) )
{
    return;
}


/***********************************************************************/
/* FORMER effects/utils.c                                              */
/***********************************************************************/


string vect_debug_entity_name(e)
entity e;
{
    return((e == (entity) TCST) ? "TCST" : entity_name(e));
}

/* bool integer_scalar_read_effects_p(cons * effects): checks that *all* read
 * effects in effects are on integer scalar variable
 *
 * Francois Irigoin: I do not see where the "read" type is checked FI
 */
bool integer_scalar_read_effects_p(fx)
cons * fx;
{
    MAPL(ceffect,
     {entity e = 
	  reference_variable(effect_any_reference(EFFECT(CAR(ceffect))));
     if(!integer_scalar_entity_p(e)) return FALSE;},
	 fx);
    return TRUE;
}

/* check that *some* read or write effects are on integer variables
 *
 * FI: this is almost always true because of array subscript expressions
 */
bool some_integer_scalar_read_or_write_effects_p(fx)
cons * fx;
{
    MAPL(ceffect,
     {entity e = 
	  reference_variable(effect_any_reference(EFFECT(CAR(ceffect))));
	  if(integer_scalar_entity_p(e)) return TRUE;},
	 fx);
    return FALSE;
}

/* bool effects_write_entity_p(cons * effects, entity e): check whether e
 * is written by effects "effects" or not
 */
bool effects_write_entity_p(fx, e)
cons * fx;
entity e;
{
    bool write = FALSE;
    MAP(EFFECT, ef, 
    {
	action a = effect_action(ef);
	entity e_used = reference_variable(effect_any_reference(ef));
	
	/* Note: to test aliasing == should be replaced below by
	 * entities_may_conflict_p()
	 */
	if(e==e_used && action_write_p(a)) {
	    write = TRUE;
	    break;
	}
    },
	fx);
    return write;
}

/* bool effects_read_or_write_entity_p(cons * effects, entity e): check whether e
 * is read or written by effects "effects" or not accessed at all
 *
 * In semantics, e can be a functional entity such as constant string
 * or constant float.
 */
bool effects_read_or_write_entity_p(cons * fx, entity e)
{
  bool read_or_write = FALSE;

  if(entity_variable_p(e)) {
    FOREACH(EFFECT, ef, fx) {
      entity e_used = reference_variable(effect_any_reference(ef));
      /* Used to be a simple pointer equality test */
      if(entities_may_conflict_p(e, e_used)) {
	read_or_write = TRUE;
	break;
      }
    }
  }
  return read_or_write;
}

/**
 * @brief Check if an effect may conflict with an entity : i.e. if the entity
 * may be affected (read or write) by the effect.
 */
bool effect_may_conflict_with_entity_p( effect eff, entity e) {
  bool conflict_p = TRUE;
  entity e_used = reference_variable(effect_any_reference(eff));
  if(!entities_may_conflict_p(e, e_used)) {
    conflict_p = FALSE;
  }
  return conflict_p;
}


entity effects_conflict_with_entity(fx, e)
cons * fx;
entity e;
{
  entity conflict_e = entity_undefined;
  MAPL(cef,
      {
        effect ef = EFFECT(CAR(cef));
        if(effect_may_conflict_with_entity_p(ef, e)) {
          entity e_used = reference_variable(effect_any_reference(ef));
          conflict_e = e_used;
          break;
        }
      },
      fx);
  return conflict_e;
}

/* Returns the list of entities used in effect list fx and
   potentially conflicting with e.

   Of course, abstract location entities do conflict with many
   entities, possibly of different types.

   if concrete_p==TRUE, ignore abstract location entities.
 */
list generic_effects_conflict_with_entities(list fx,
					    entity e,
					    bool concrete_p)
{
  list lconflict_e = NIL;

  FOREACH(EFFECT, ef, fx) {
    entity e_used = reference_variable(effect_any_reference(ef));
    if(!(entity_abstract_location_p(e_used) && concrete_p)) {
      if(entities_may_conflict_p(e, e_used)) {
	lconflict_e = gen_nconc(lconflict_e,
				CONS(ENTITY, e_used, NIL));
      }
    }
  }

  return lconflict_e;
}

list effects_conflict_with_entities(list fx, entity e)
{
  return generic_effects_conflict_with_entities(fx, e, FALSE);
}

list concrete_effects_conflict_with_entities(list fx, entity e)
{
  return generic_effects_conflict_with_entities(fx, e, TRUE);
}

/* Return true if a statement has an I/O effect in the effects
   list. */
bool statement_io_effect_p(statement s)
{
   bool io_effect_found = FALSE;
   list effects_list = load_proper_rw_effects_list(s);

   /* If there is an I/O effects, the following entity should
      exist. If it does not exist, statement_io_effect_p() will return
      FALSE anyway. */
   entity private_io_entity =
      global_name_to_entity(IO_EFFECTS_PACKAGE_NAME,
			    IO_EFFECTS_ARRAY_NAME);

   MAP(EFFECT, an_effect,
       {
          reference a_reference = effect_any_reference(an_effect);
          entity a_touched_variable =
             reference_variable(a_reference);

          if (a_touched_variable == private_io_entity) {
             io_effect_found = TRUE;
             break;
          }
       },
       effects_list);

   return io_effect_found;
}

/* Return TRUE if the statement has a write effect on at least one of
   the argument (formal parameter) of the module and if the argument
   passing mode is by reference. Note that the return variable of a
   function is also considered here as a formal parameter. */
bool statement_has_a_formal_argument_write_effect_p(statement s)
{
   bool write_effect_on_a_module_argument_found = FALSE;
   entity module = get_current_module_entity();
   list effects_list = load_proper_rw_effects_list(s);
   /* it might be better to check the parameter passing mode itself,
      via the module type */
   bool fortran_p = fortran_module_p(module);

   FOREACH(EFFECT, an_effect, effects_list) {
     entity a_variable = reference_variable(effect_any_reference(an_effect));
     bool formal_p = variable_is_a_module_formal_parameter_p(a_variable,
							module);
     bool return_variable_p = variable_return_p(a_variable);

     if (action_write_p(effect_action(an_effect))
	 && (return_variable_p
	     || (formal_p && fortran_p)
	     )
	 ) {
       write_effect_on_a_module_argument_found = TRUE;
       break;
     }
   }       ;

   return write_effect_on_a_module_argument_found;

}



list /* of effect */ make_effects_for_array_declarations(list refs)
{     
  list leff = NIL;
  effect eff;
  MAPL(l1,
  {  
    
    reference ref = REFERENCE(CAR(l1));
    /* FI: in this context, I assume that eff is never returned undefined */
    eff = (*reference_to_effect_func)(ref, is_action_read, true);
     if(effect_undefined_p(eff)) {
       pips_debug(8, "Reference to \"%s\" ignored\n", entity_name(reference_variable(ref)));
     }
     else
       leff= CONS(EFFECT,eff,leff);
  },refs);
  
  
  gen_free_list(refs);
  return leff;
}

     

typedef struct { list le, lr; } deux_listes;

static void make_uniq_reference_list(reference r, deux_listes * l)
{
  entity e = reference_variable(r);
  if (! (storage_rom_p(entity_storage(e)) &&
	 !(value_undefined_p(entity_initial(e))) &&
	 value_symbolic_p(entity_initial(e)) &&
	 type_functional_p(entity_type(e)))) {

    /* Add reference r only once */
    if (l->le ==NIL || !gen_in_list_p(e, l->le)) {
      l->le = CONS(ENTITY,e,  l->le);
      l->lr = CONS(REFERENCE,r,l->lr);
    }
  }
}

/* FI: this function has not yet been extended for C types!!! */
list extract_references_from_declarations(list decls)
{
  list arrays = NIL;
  deux_listes lref = { NIL, NIL };

  MAPL(le,{
    entity e= ENTITY(CAR(le));
    type t = entity_type(e);

    if (type_variable_p(t) && !ENDP(variable_dimensions(type_variable(t))))
      arrays = CONS(VARIABLE,type_variable(t), arrays);
  }, decls );

  MAPL(array,
  { variable v = VARIABLE(CAR(array));
  list ldim = variable_dimensions(v);
  while (!ENDP(ldim))
    {
      dimension d = DIMENSION(CAR(ldim));
      gen_context_recurse(d, &lref, reference_domain, make_uniq_reference_list, gen_null);
      ldim=CDR(ldim);

    }
  }, arrays);
  gen_free_list(lref.le);

  return(lref.lr);
}

list summary_effects_from_declaration(string module_name __attribute__ ((unused)))
{
  list sel = NIL;
  //entity mod = module_name_to_entity(module_name);
  //list decls = code_declarations(value_code(entity_initial(mod)));
  extern list current_module_declarations(void);
  list decls = current_module_declarations();
  list refs = list_undefined;

  refs = extract_references_from_declarations(decls);

  ifdebug(8) {
    pips_debug(8, "References from declarations:\n");
    MAP(REFERENCE, r, {
      pips_debug(8, "Reference for variable \"%s\"\n",
		 entity_name(reference_variable(r)));
      print_reference(r);
      fprintf(stderr, "\n");
    }, refs);
  }

  sel = make_effects_for_array_declarations(refs);

  return sel;
}

void dump_cell(cell c)
{
  fprintf(stderr, "Cell %p = (cell_tag=%u, reference=%p)\n", c, cell_tag(c),
	  cell_preference_p(c)? preference_reference(cell_preference(c)):cell_reference(c));
}

void dump_effect(effect e)
{
  cell c = effect_cell(e);
  action ac = effect_action(e);
  approximation ap = effect_approximation(e);
  descriptor d = effect_descriptor(e);

  effect_consistent_p(e);
  fprintf(stderr, "Effect %p = (domain=%td, cell=%p, action=%p,"
	  " approximation=%p, descriptor=%p\n",
	  e, effect_domain_number(e), c, ac, ap, d);
  dump_cell(c);
}

void dump_effects(list le)
{
  int i = 1;
  MAP(EFFECT, e, {
      fprintf(stderr, "%d ", i++);
      dump_effect(e);
    }, le);
}

/* Check if a reference appears more than once in the effect list. If
   persistant_p is true, do not go thru persistant arcs. Else, use all
   references. */
bool effects_reference_sharing_p(list el, bool persistant_p)
{
  bool sharing_p = FALSE;
  list rl = NIL; /* reference list */
  list ce = list_undefined; /* current effect */

  for(ce=el; !ENDP(ce); POP(ce)) {
    effect e = EFFECT(CAR(ce));
    cell c = effect_cell(e);
    reference r = reference_undefined;

    if(persistant_p) {
      if(cell_reference_p(c))
	r = cell_reference(c);
    }
    else
      r = effect_any_reference(e);

    if(!reference_undefined_p(r)) {
      if(gen_in_list_p((void *) r, rl)) {
	extern void print_effect(effect);
	fprintf(stderr, "this effect shares its reference with another effect in the list\n");
	print_effect(e);
	sharing_p = TRUE;
	break;
      }
      else
	rl = CONS(REFERENCE, r, rl);
    }
  }
  return sharing_p;
}

/************************ anywhere effects ********************/

/**
 @return a new anywhere effect.
 @param act is an action tag 

 Allocate a new anywhere effect using generic function 
 reference_to_effect_func, and the anywhere entity on demand
 which may not be best if we want to express it's aliasing with all
 module areas. In the later case, the anywhere entity should be
 generated by bootstrap and be updated each time new areas are
 declared by the parsers. I do not use a persistant anywhere
 reference to avoid trouble with convex-effect nypassing of the
 persistant pointer. (re-used from original non-generic function
 anywhere_effect.)
 
   Action a is integrated in the new effect (aliasing).
 */
effect make_anywhere_effect(tag act)
{
 
  entity anywhere_ent = entity_all_locations();
  effect anywhere_eff = effect_undefined;
 
  anywhere_eff = (*reference_to_effect_func)
    (make_reference(anywhere_ent, NIL),
     act, false);
  effect_to_may_effect(anywhere_eff);
  return anywhere_eff;
}

/**
   remove duplicate anywhere effects and keep anywhere effects and
   effects not combinable with anywhere effects.
   
   @param l_eff is a list of effects
   @return a new list with no sharing with the initial effect list.
   
 */
list clean_anywhere_effects(list l_eff)
{
  list l_tmp;
  list l_res;
  bool anywhere_w_p = false;
  bool anywhere_r_p = false;

  l_tmp = l_eff;
  while ((!anywhere_w_p || !anywhere_r_p) && !ENDP(l_tmp))
    {
      effect eff = EFFECT(CAR(l_tmp));
      if (anywhere_effect_p(eff))
	{
	  anywhere_w_p = anywhere_w_p || effect_write_p(eff);
	  anywhere_r_p = anywhere_r_p || effect_read_p(eff);
	}
      
      POP(l_tmp);
    }

  l_res = NIL;

  if (anywhere_r_p)
    l_res = gen_nconc(l_res, 
		      CONS(EFFECT, make_anywhere_effect(is_action_read), 
			   NIL));
  if (anywhere_w_p)
    l_res = gen_nconc(l_res, 
		      CONS(EFFECT, make_anywhere_effect(is_action_write), 
			   NIL));
  
  
  l_tmp = l_eff;
  while (!ENDP(l_tmp))
    {
      effect eff = EFFECT(CAR(l_tmp));
      
      
      if (malloc_effect_p(eff) || io_effect_p(eff) ||
	  (effect_write_p(eff) && !anywhere_w_p) ||
	  (effect_read_p(eff) && !anywhere_r_p))
	{
	  l_res = gen_nconc(l_res, CONS(EFFECT, (*effect_dup_func)(eff), NIL));
	}     
      POP(l_tmp);
    }
  
  return l_res;
}

/********************** Effects on all accessible paths  ***************/

/**
  @param eff_write a write effect
  @param is the action of the generated effects :
         'r' for read, 'w' for write, and 'x' for read and write.
  @return a list of effects. beware : eff_write is included in the list.

 */
list effect_to_effects_with_given_tag(effect eff, tag act)
{
  list l_res = NIL;
  effect eff_read = effect_undefined;
  effect eff_write = effect_undefined;
  extern void print_effect(effect);
	
  pips_assert("effect is defined \n", !effect_undefined_p(eff));

  if (act == 'x')
    {      
      eff_write = eff;
      effect_action_tag(eff_write) = is_action_write;
      eff_read = (*effect_dup_func)(eff_write);
      effect_action_tag(eff_read) = is_action_read;
    }
  else if (act == 'r')
    {
      
      eff_read = eff;
      effect_action_tag(eff_read) = is_action_read;
      eff_write = effect_undefined;
    }
  else
    {
      eff_read = effect_undefined;
      eff_write = eff;
      effect_action_tag(eff_write) = is_action_write;
    }
  
  ifdebug(8)
    {
      pips_debug(8, "adding effects to l_res : \n");
      if(!effect_undefined_p(eff_write)) 
	print_effect(eff_write);
      if(!effect_undefined_p(eff_read)) 
	print_effect(eff_read);
    }
  
  if(!effect_undefined_p(eff_write)) 
    l_res = gen_nconc(l_res, CONS(EFFECT, eff_write, NIL));
  if(!effect_undefined_p(eff_read)) 
    l_res = gen_nconc(l_res, CONS(EFFECT, eff_read, NIL)); 

  return l_res;
}

/**
   @param eff is an effect whose reference is the beginning access path.
          it is not modified or re-used.
   @param eff_type is the type of the object represented by the effect
          access path. This avoids computing it at each step.
   @param act is the action of the generated effects :
              'r' for read, 'w' for write, and 'x' for read and write.
   @return a list of effects on all the accessible paths from eff reference.
 */
list generic_effect_generate_all_accessible_paths_effects(effect eff, 
							  type eff_type, 
							  tag act)
{
  list l_res = NIL;
  pips_assert("the effect must be defined\n", !effect_undefined_p(eff));
  extern void print_effect(effect);
	
  
  if (anywhere_effect_p(eff))
    {
      /* there is no other accessible path */
      pips_debug(6, "anywhere effect -> returning NIL \n");
      
    }
  else
    {
      reference ref = effect_any_reference(eff);
      entity ent = reference_variable(ref);
      type t = ultimate_type(entity_type(ent));
      int d = effect_type_depth(t);
      effect eff_write = effect_undefined;

      /* this may lead to memory leak if no different access path is 
	 reachable */
      eff_write = (*effect_dup_func)(eff);
      
      ifdebug(6)
	{
	  pips_debug(6, "considering effect : \n");
	  print_effect(eff);
	  pips_debug(6, " with entity effect type depth %d \n",
		     d);
	}
      
           
      switch (type_tag(eff_type))
	{
	case is_type_variable :
	  {
	    variable v = type_variable(eff_type);
	    basic b = variable_basic(v);
	    bool add_effect = false;
	    
	    pips_debug(8, "variable case, of dimension %d\n", 
		       (int) gen_length(variable_dimensions(v))); 

	    /* we first add the array dimensions if any */
	    FOREACH(DIMENSION, c_t_dim, 
		    variable_dimensions(v))
	      {
		(*effect_add_expression_dimension_func)
		  (eff_write, make_unbounded_expression());
		add_effect = true;
	      }
	    /* And add the generated effect */
	    if (add_effect)
	      {
		l_res = gen_nconc
		  (l_res,
		   effect_to_effects_with_given_tag(eff_write,act));
		add_effect = false;
	      }

	    /* If the basic is a pointer type, we must add an effect
	       with a supplementary dimension, and then recurse
               on the pointed type.
	    */
	    if(basic_pointer_p(b))
	      {
		pips_debug(8, "pointer case, \n");
				
		eff_write = (*effect_dup_func)(eff_write);
		(*effect_add_expression_dimension_func)
		  (eff_write, make_unbounded_expression());
		
		l_res = gen_nconc
		  (l_res,
		   effect_to_effects_with_given_tag(eff_write,act));
		
		l_res = gen_nconc
		  (l_res,
		   generic_effect_generate_all_accessible_paths_effects
		   (eff_write,  basic_pointer(b), act));
		
	      }	    	    
	    
	    break;
	  }
	default:
	  {
	    pips_internal_error("case not handled yet\n");
	  }
	} /*switch */
      
    } /* else */
  
  
  return(l_res);
}

/******************************************************************/

list type_fields(type t)
{
  list l_res = NIL;

  switch (type_tag(t))
    {
    case is_type_struct:
      l_res = type_struct(t);
      break;
    case is_type_union:
      l_res = type_union(t);
      break;
    case is_type_enum:
      l_res = type_enum(t);
      break;
    default:
      pips_internal_error("type_fields improperly called\n");
    }
  return l_res;

}

/**
 NOT YET IMPLEMENTED FOR VARARGS AND FUNCTIONAL TYPES.

 @param eff is an effect
 @return true if the effect reference maybe an access path to a pointer
*/
static bool r_effect_pointer_type_p(effect eff, list l_ind, type ct)
{
  bool p = false, finished = false;

  pips_debug(7, "begin with type %s\n and number of indices : %d\n",
	     words_to_string(words_type(ct,NIL)),
	     (int) gen_length(l_ind));
  while (!finished)
    {
      switch (type_tag(ct))
	{
	case is_type_variable :
	  {
	    variable v = type_variable(ct);
	    basic b = variable_basic(v);
	    list l_dim = variable_dimensions(v);

	    pips_debug(8, "variable case, of basic %s, of dimension %d\n",
		       basic_to_string(b),
		       (int) gen_length(variable_dimensions(v)));

	    while (!ENDP(l_dim) && !ENDP(l_ind))
	      {
		POP(l_dim);
		POP(l_ind);
	      }
	    
	    if(ENDP(l_ind) && ENDP(l_dim))
	      {
	      if(basic_pointer_p(b))
		{
		  p = true;		  
		  finished = true;
		}
	      else
		finished = true;
	      }
	    else if (ENDP(l_dim)) /* && !ENDP(l_ind) by construction */
	      {
		pips_assert("the current basic should be a pointer or a derived\n", 
			    basic_pointer_p(b) || basic_derived_p(b));	
		
		if (basic_pointer_p(b))
		  {
		    ct = basic_pointer(b);
		    POP(l_ind);
		  }
		else /* b is a derived */ 
		  {
		    ct = entity_type(basic_derived(b));	
		    p = r_effect_pointer_type_p(eff, l_ind, ct);
		    finished = true;
		  }
		 
	      }
	    else /* ENDP(l_ind) but !ENDP(l_dim) */
	      {
		finished = true;
	      }

	    break;
	  }
	case is_type_struct:
	case is_type_union:
	case is_type_enum:
	  {
	    list l_ent = type_fields(ct);
	    expression field_exp = EXPRESSION(CAR(l_ind));
	    entity field = entity_undefined;

	    pips_debug(7, "field case, with field expression : %s \n",
		       words_to_string(words_expression(field_exp,NIL)));

	    /* If the field is known, we only look at the corresponding type.
	       If not, we have to recursively look at each field
	    */
	    if (!unbounded_expression_p(field_exp))
	      {
		pips_assert("the field expression must be a reference\n", 
			    expression_reference_p(field_exp));
		field = expression_variable(field_exp);
		if (variable_phi_p(field))
		  field = entity_undefined;
	      }
	      
	    if (!entity_undefined_p(field))
	      {
		/* the current type is the type of the field */
		ct = basic_concrete_type(entity_type(field));
		p = r_effect_pointer_type_p(eff, CDR(l_ind), ct);
		free_type(ct);
		ct = type_undefined;
		finished = true;
	      }
	    else
	      /* look at each field until a pointer is found*/
	      {
		while (!ENDP(l_ent) && p)
		  {
		    type new_ct = basic_concrete_type(entity_type(ENTITY(CAR(l_ent))));
		    p = r_effect_pointer_type_p(eff, CDR(l_ind), 
						new_ct);
		    free_type(new_ct);
		    POP(l_ent);
		  }
		finished = true;
	      }	    
	    break;
	  }
	default:
	  {
	    pips_internal_error("case not handled yet\n");
	  }
	} /*switch */

    }/*while */
  pips_debug(8, "end with p = %s\n", p== false ? "false" : "true");
  return p;

}


/** 
 NOT YET IMPLEMENTED FOR VARARGS AND FUNCTIONAL TYPES.

 @param eff is an effect
 @return true if the effect reference maybe an access path to a pointer
*/
bool effect_pointer_type_p(effect eff)
{
  bool p = false;
  reference ref = effect_any_reference(eff);
  list l_ind = reference_indices(ref);
  entity ent = reference_variable(ref);
  type t = basic_concrete_type(entity_type(ent));

  pips_debug(8, "begin with effect reference %s\n",
	     words_to_string(words_reference(ref,NIL)));
  if (entity_abstract_location_p(ent))
    p = true;
  else
    p = r_effect_pointer_type_p(eff, l_ind, t);

  free_type(t);
  pips_debug(8, "end with p = %s\n", p== false ? "false" : "true");
  return p;

}



type simple_effect_reference_type(reference ref)
{
  type bct = basic_concrete_type(entity_type(reference_variable(ref)));
  type ct; /* current_type */

  list l_inds = reference_indices(ref);

  type t = type_undefined; /* result */
  bool finished = false;

  pips_debug(8, "beginning with reference : %s\n", words_to_string(words_reference(ref,NIL)));

  ct = bct;
  while (! finished)
    {
      basic cb = variable_basic(type_variable(ct)); /* current basic */
      list cd = variable_dimensions(type_variable(ct)); /* current type dimensions */

      while(!ENDP(cd) && !ENDP(l_inds))
	{
	  pips_debug(8, "poping one array dimension \n");
	  POP(cd);
	  POP(l_inds);
	}

      if(ENDP(l_inds))
	{
	  pips_debug(8, "end of reference indices, generating type\n");
	  t = make_type(is_type_variable,
			make_variable(copy_basic(cb),
				      gen_full_copy_list(cd),
				      NIL));
	  finished = true;
	}
      else /* ENDP (cd) && ! ENDP(l_inds) */
	{
	  switch (basic_tag(cb))
	    {
	    case is_basic_pointer:
	      /* in an effect reference there is always an index for a pointer */
	      pips_debug(8, "poping pointer dimension\n");
	      POP(l_inds);
	      ct = basic_pointer(cb);
	      break;
	    case is_basic_derived:
	      {
		/* we must know which field it is, else return an undefined type */
		expression field_exp = EXPRESSION(CAR(l_inds));
		entity field = entity_undefined;
		pips_debug(8, "field dimension : %s\n", 
			   words_to_string(words_expression(field_exp,NIL)));
		
		if (!unbounded_expression_p(field_exp))
		  {
		    pips_assert("the field expression must be a reference\n", 
				expression_reference_p(field_exp));
		    field = expression_variable(field_exp);
		    if (variable_phi_p(field))
		      field = entity_undefined;
		  }
	      		
		if (!entity_undefined_p(field))
		  {
		    pips_debug(8, "known field, poping field dimension\n");
		    free_type(bct);
		    bct =  basic_concrete_type(entity_type(field));
		    ct = bct;
		    POP(l_inds);
		  }
		else 
		  {
		    pips_debug(8, "unknown field, returning type_undefined\n");
		    t = type_undefined;
		    finished = true;
		  }
	      }
	      break;
	    case is_basic_int:
	    case is_basic_float:
	    case is_basic_logical:
	    case is_basic_complex:
	    case is_basic_string:
	    case is_basic_bit:
	    case is_basic_overloaded:
	      pips_internal_error("fundamental basic not expected here \n");
	      break;
	    case is_basic_typedef:
	      pips_internal_error("typedef not expected here \n");
	    } /* switch (basic_tag(cb)) */
	}

    } /* while (!finished) */


  free_type(bct);
  pips_debug(6, "returns with %s\n", words_to_string(words_type(t,NIL)));
  return t;

}



bool regions_weakly_consistent_p(list rl)
{
  FOREACH(EFFECT, r, rl) {
    descriptor rd = effect_descriptor(r);

    if(descriptor_convex_p(rd)) {
      Psysteme rsc = descriptor_convex(rd);

      pips_assert("rsc is weakly consistent", sc_weak_consistent_p(rsc));
    }
  }
  return TRUE;
}

bool region_weakly_consistent_p(effect r)
{
  descriptor rd = effect_descriptor(r);

  if(descriptor_convex_p(rd)) {
    Psysteme rsc = descriptor_convex(rd);

    pips_assert("rsc is weakly consistent", sc_weak_consistent_p(rsc));
  }

  return TRUE;
}

/**
   Effects are not copied but a new list is built.
 */
list statement_modified_pointers_effects_list(statement s)
{
  list l_cumu_eff = load_rw_effects_list(s);
  list l_res = NIL;
  bool anywhere_p = false;

  ifdebug(6){
	 pips_debug(6, " effects before selection: \n");
	 (*effects_prettyprint_func)(l_cumu_eff);
       }

  FOREACH(EFFECT, eff, l_cumu_eff)
    {
      if (!anywhere_p && effect_write_p(eff))
	{
	  if (anywhere_effect_p(eff))
	    anywhere_p = true;
	  else if (effect_pointer_type_p(eff))
	    l_res = gen_nconc(l_res, CONS(EFFECT, eff, NIL));
	}
    }
  if (anywhere_p)
    {
      gen_free_list(l_res);
      l_res = CONS(EFFECT, make_anywhere_effect(is_action_write), NIL);
    }

  ifdebug(6){
	 pips_debug(6, " effects after selection: \n");
	 (*effects_prettyprint_func)(l_res);
       }


  return l_res;
}

/******************************************************************/


static bool effects_reference_indices_may_equal_p(expression ind1, expression ind2)
{
  if (unbounded_expression_p(ind1) || unbounded_expression_p(ind2))
    return true;
  else
    return same_expression_p(ind1, ind2);
}

/**
   This function should be instanciated differently for simple and convex 
   effects : much more work should be done for convex effects.

   @return true if the effects have comparable access paths
               in which case result is set to
	          0 if the effects paths may be equal
		  1 if eff1 access path may lead to eff2 access path
		  -1 if eff2 access path may lead to eff1 access path
           false otherwise.
*/
static bool effects_access_paths_comparable_p(effect eff1, effect eff2, 
int *result)
{
  bool comparable_p = true; /* assume they are compable */
  reference ref1 = effect_any_reference(eff1);
  reference ref2 = effect_any_reference(eff2);
  list linds1 = reference_indices(ref1);
  list linds2 = reference_indices(ref2);
  
  pips_debug_effect(8, "begin\neff1 = \n", eff1);
  pips_debug_effect(8, "begin\neff2 = \n", eff2);

  /* to be comparable, they must have the same entity */
  comparable_p = same_entity_p(reference_variable(ref1), 
			       reference_variable(ref2));
  
  while( comparable_p && !ENDP(linds1) && !ENDP(linds2))
    {
      if (!effects_reference_indices_may_equal_p(EXPRESSION(CAR(linds1)),
						EXPRESSION(CAR(linds2))))
	comparable_p = false;

      POP(linds1);
      POP(linds2);
    }
  
  if (comparable_p)
    {
      *result = (int) (gen_length(linds2) - gen_length(linds1)) ;
      if (*result != 0) *result = *result / abs(*result);
    }
 
  pips_debug(8, "end with comparable_p = %s and *result = %d",
	     comparable_p ? "true" : "false", *result);

  return comparable_p;
}


list generic_effects_store_update(list l_eff, statement s, bool backward_p)
{

   transformer t; /* transformer of statement s */
   list l_eff_pointers;
   list l_res = NIL;
   bool anywhere_w_p = false;
   bool anywhere_r_p = false;

   pips_debug(5, "begin\n");
	
   t = (*load_transformer_func)(s);    

   if (l_eff !=NIL)    
     {
       /* first change the store of the descriptor */
       if (backward_p)
	 l_eff = (*effects_transformer_composition_op)(l_eff, t);
       else
	 l_eff =  (*effects_transformer_inverse_composition_op)(l_eff, t);
   
       ifdebug(5){
	 pips_debug(5, " effects after composition with transformer: \n");
	 (*effects_prettyprint_func)(l_eff);
       }

       if (get_bool_property("EFFECTS_POINTER_MODIFICATION_CHECKING"))
	 {
	   /* then change the effects references if some pointer is modified */
	   /* backward_p is not used here because we lack points_to information
	      and we thus generate anywhere effects 
	   */
	   l_eff_pointers = statement_modified_pointers_effects_list(s);
	   
	   while( !ENDP(l_eff) && 
		  ! (anywhere_w_p && anywhere_r_p))
	     {
	       list l_eff_p_tmp = l_eff_pointers;
	       effect eff = EFFECT(CAR(l_eff));
	       bool eff_w_p = effect_write_p(eff);
	       bool found = false;
	       
	       
	       
	       while( !ENDP(l_eff_p_tmp) &&
		      !((eff_w_p && anywhere_w_p) || (!eff_w_p && anywhere_r_p)))
		 {
		   effect eff_p = EFFECT(CAR(l_eff_p_tmp));
		   effect new_eff = effect_undefined;
		   int comp_res = 0;
		   
		   if(effects_access_paths_comparable_p(eff, eff_p, &comp_res)
		      && comp_res <=0 )
		     {
		       new_eff = make_anywhere_effect(effect_action_tag(eff));
		       l_res = gen_nconc(l_res, CONS(EFFECT, new_eff, NIL));
		       found = true;
		       if (eff_w_p)
			 anywhere_w_p = true;
		       else 
			 anywhere_r_p = true;			   
		
		     } /*  if(effects_access_paths_comparable_p) */
		   
		   POP(l_eff_p_tmp);
		 } /* while( !ENDP(l_eff_p_tmp))*/ 
	       
	       /* if we have found no modifiying pointer, we keep the effect */
	       if (!found)
		 {
		   /* is the copy necessary ?*/
		   l_res = gen_nconc(l_res, CONS(EFFECT,(*effect_dup_func)(eff) , NIL));
		   
		 }
	       
	       POP(l_eff);
	       
	     } /* while( !ENDP(l_eff)) */
	   
	   ifdebug(5){
	     pips_debug(5, " effects after composition with pointer effects: \n");
	     (*effects_prettyprint_func)(l_res);
	   }
            
	 } /* if (get_bool_property("EFFECTS_POINTER_MODIFICATION_CHECKING"))*/
       else
	 l_res = l_eff;
     } /* if (l_eff !=NIL) */

   return l_res;
}


/***************************************/

/**
   @param exp is an effect index expression which is either the rank or an entity corresponding to a struct, union or enum field
   @param l_fields is the list of fields of the corresponding struct, union or enum
   @return the entity corresponding to the field.
 */
entity effect_field_dimension_entity(expression exp, list l_fields)
{
  if(expression_constant_p(exp))
    {
      int rank = expression_to_int(exp);
      return ENTITY(gen_nth(rank-1, l_fields));
    }
  else
    {
      return expression_to_entity(exp);
    }
}

/**
   @brief recursively walks thru current_l_ind and current_type in parallel until a pointer dimension is found.
   @param current_l_ind is a list of effect reference indices.
   @param current_type is the corresponding type in the original entity type arborescence 
   @param exact_p is a pointer to a bool, which is set to true if the result is not an approximation.
   @return -1 if no index corresponds to a pointer dimension in current_l_ind, the rank of the least index that may correspond to
      a pointer dimension in current_l_ind otherwise. If this information is exact, *exact_p is set to true.
 */
static int effect_indices_first_pointer_dimension_rank(list current_l_ind, type current_type, bool *exact_p)
{
  int result = -1; /*assume there is no pointer */
  basic current_basic = variable_basic(type_variable(current_type));
  size_t current_nb_dim = gen_length(variable_dimensions(type_variable(current_type)));

  pips_debug(8, "input type : %s\n", type_to_string(current_type));
  pips_debug(8, "current_basic : %s, and number of dimensions %d\n", basic_to_string(current_basic), (int) current_nb_dim);

  pips_assert("there should be no effect on variable names\n", gen_length(current_l_ind) >= current_nb_dim);
  

  switch (basic_tag(current_basic)) 
    {
    case is_basic_pointer:
      {
	// no need to test if gen_length(current_l_ind) >= current_nb_dim because of previous assert
	result = (int) current_nb_dim;
	*exact_p = true;
	break;
      }
    case is_basic_derived:
      {
	int i;
	current_type = entity_type(basic_derived(current_basic));
	
	if (type_enum_p(current_type))
	  result = -1;
	else
	  {
	    
	    /*first skip array dimensions if any*/
	    for(i=0; i< (int) current_nb_dim; i++, POP(current_l_ind));
	    pips_assert("there must be at least one index left for the field\n", gen_length(current_l_ind) > 0);
	    
	    list l_fields = derived_type_fields(current_type);
	    
	    entity current_field_entity = effect_field_dimension_entity(EXPRESSION(CAR(current_l_ind)), l_fields);
	    
	    if (variable_phi_p(current_field_entity) || same_string_p(entity_local_name(current_field_entity), UNBOUNDED_DIMENSION_NAME))
	      {
		while (!ENDP(l_fields))
		  {
		    int tmp_result = -1;
		    entity current_field_entity = ENTITY(CAR(l_fields));
		    type current_type =  basic_concrete_type(entity_type(current_field_entity));
		    size_t current_nb_dim = gen_length(variable_dimensions(type_variable(current_type)));
		    
		    if (gen_length(CDR(current_l_ind)) >= current_nb_dim)
		      // consider this field only if it can be an effect on this field
		      tmp_result = effect_indices_first_pointer_dimension_rank(CDR(current_l_ind), current_type, exact_p);
		    
		    POP(l_fields);
		    if (tmp_result >= 0)
		      result = result < 0 ? tmp_result : (tmp_result <= result ? tmp_result : result);
		    free_type(current_type);
		  }
		
		*exact_p = (result < 0);
		if (result >= 0) result ++; // do not forget the field index !
	      }
	    else
	      {
		
		current_type = basic_concrete_type(entity_type(current_field_entity));
		result = effect_indices_first_pointer_dimension_rank(CDR(current_l_ind), current_type, exact_p);
		if (result >=0) result++; // do not forget the field index ! 
		free_type(current_type);
	      }
	  }
	break;
      }
    default:
      {
	result = -1;
	*exact_p = true;
	break;
      }
    }
  
  pips_debug(8, "returning %d\n", result);
  return result;
  
}


/**
   @brief walks thru ref indices and ref entity type arborescence in parallel until a pointer dimension is found.
   @param ref is an effect reference
   @param exact_p is a pointer to a bool, which is set to true if the result is not an approximation.
   @return -1 if no index corresponds to a pointer dimension, the rank of the least index that may correspond to
      a pointer dimension in current_l_ind otherwise. If this information is exact, *exact_p is set to true.
 */
int effect_reference_first_pointer_dimension_rank(reference ref, bool *exact_p)
{
  entity ent = reference_variable(ref);
  list current_l_ind = reference_indices(ref);
  type ent_type = entity_type(ent);
  int result;

  pips_debug(8, "input reference : %s\n", words_to_string(effect_words_reference(ref)));
  
  if (!type_variable_p(ent_type))
    {
      result = -1;
    }
  else
    {
      type current_type = basic_concrete_type(ent_type);
      result = effect_indices_first_pointer_dimension_rank(current_l_ind, current_type, exact_p);
      free_type(current_type);
    }

  return result;

}

/**
   @param ref is an effect reference
   @param exact_p is a pointer to a bool, which is set to true if the result is not an approximation.
   @return false if no index corresponds to a pointer dimension, false if any index may correspond to
      a pointer dimension. If this information is exact, *exact_p is set to true.
 */
bool effect_reference_contains_pointer_dimension_p(reference ref, bool *exact_p)
{
  int pointer_rank;
  pointer_rank = effect_reference_first_pointer_dimension_rank(ref, exact_p);
  return (pointer_rank >= 0);
}


/**
   @param ref is an effect reference
   @param exact_p is a pointer to a bool, which is set to true if the result is not an approximation.
   @return true if the effect reference may dereference a pointer, false otherwise.
 */
bool effect_reference_dereferencing_p(reference ref, bool * exact_p)
{
  list l_ind = reference_indices(ref);
  bool result;
  int p_rank;

  if (entity_all_locations_p(reference_variable(ref)))
    {
      result = true;
      *exact_p = false;
    }
  else
    {
      p_rank = effect_reference_first_pointer_dimension_rank(ref, exact_p);
  

      if (p_rank == -1)
	result = false;
      else
	result = p_rank < (int) gen_length(l_ind);
    }
  return result;
}


/************ CONVERSION TO CONSTANT PATH EFFECTS ***********/

static bool use_points_to = false;

void set_use_points_to(bool pt_p)
{
  use_points_to = pt_p;
}
void reset_use_points_to()
{
  use_points_to = false;
}
bool get_use_points_to()
{
  return use_points_to;
}

static bool FILE_star_effect_reference(reference ref)
{
  bool res = false;
  type t = basic_concrete_type(entity_type(reference_variable(ref)));
  pips_debug(8, "begin with type %s\n",
	     words_to_string(words_type(t,NIL)));
  if (type_variable_p(t))
    {
      basic b = variable_basic(type_variable(t));
      if (basic_pointer_p(b))
	{
	  t = basic_pointer(b);
	   if (type_variable_p(t))
	     {
		basic b = variable_basic(type_variable(t));
		if (basic_derived_p(b))
		  {
		    entity te = basic_derived(b);
		    if (same_string_p(entity_user_name(te), "_IO_FILE"))
		      {
			res = true;		  
		      }
		  }
	     }
	}
    }
  pips_debug(8, "end with : %s\n", res? "true":"false");
			
  return res;
}


/**
   @param l_pointer_eff is a list of effects that may involve access paths dereferencing pointers.
   @return a list of effects with no access paths dereferencing pointers.

   Two algorithms are currently used, depending on the value returned by get_use_points_to.
   
   If true, when there is an effect reference with a dereferencing dimension, eval_cell_with_points_to is called
   to find an equivalent constant path using points-to.
   If false, effect references with a dereferencing dimension are systematically replaced by anywhere effects.
 */
list pointer_effects_to_constant_path_effects(list l_pointer_eff)
{
  list le = NIL;

  pips_debug_effects(8, "input effects : \n", l_pointer_eff);

  if (get_use_points_to())
    {
      FOREACH(EFFECT, eff, l_pointer_eff)
	{
	  bool exact_p;
	  reference ref = effect_any_reference(eff);
      
	  if (io_effect_p(eff)|| malloc_effect_p(eff) || (!get_bool_property("USER_EFFECTS_ON_STD_FILES") && std_file_effect_p(eff)))
	    {
	      le = CONS(EFFECT, copy_effect(eff), le);
	    }
	  else 
	    {
	      if (effect_reference_dereferencing_p(ref, &exact_p))
		{
		  pips_debug(8, "dereferencing case \n");
		  bool exact_p = false;
		  list l_new_cells = eval_cell_with_points_to(effect_cell(eff), 
							      points_to_list_list(load_pt_to_list(effects_private_current_stmt_head())),
							      &exact_p); 
		  if (ENDP(l_new_cells))
		    {
		      pips_debug(8, "no equivalent constant path found -> anywhere effect\n");
		      /* We have not found any equivalent constant path : it may point anywhere */
		      /* We should maybe contract these effects later. Is it done by the callers ? */
		      le = CONS(EFFECT, make_anywhere_effect(effect_action_tag(eff)), le);
		    }
		  else
		    {
		      FOREACH(CELL, c, l_new_cells)
			{			  
			  le = CONS(EFFECT, 
				    make_effect(c, copy_action(effect_action(eff)), 
						make_approximation(exact_p? is_approximation_must:is_approximation_may,UU), 
						make_descriptor_none()),
				    le);
			}	
		    }
		  
		}
	      else
		le = CONS(EFFECT, copy_effect(eff), le);
	    }
	}
	
      le = gen_nreverse(le);

    }
  else
    {
      bool read_dereferencing_p = false;
      bool write_dereferencing_p = false;
      list l = l_pointer_eff, lkeep = NIL;

      while (!ENDP(l)) 
	{
	  bool exact_p;
	  effect eff = EFFECT(CAR(l));
	  reference ref = effect_any_reference(eff);
      
	  if (io_effect_p(eff)|| malloc_effect_p(eff) 
	      || (!get_bool_property("USER_EFFECTS_ON_STD_FILES") && std_file_effect_p(eff))
	      || (!get_bool_property("ALIASING_ACROSS_IO_STREAMS") && FILE_star_effect_reference(ref)))
	    {
	      lkeep = CONS(EFFECT, eff, lkeep);
	    }
	  else 
	    if (!(read_dereferencing_p && write_dereferencing_p) 
		&& effect_reference_dereferencing_p(ref, &exact_p))
	      {		
		if (effect_read_p(eff)) read_dereferencing_p = true;
		    else write_dereferencing_p = true;
	      }
	  POP(l);      
	}
	
      lkeep = gen_nreverse(lkeep);

      if (write_dereferencing_p)
	le = CONS(EFFECT, make_anywhere_effect(is_action_write), le);
      if (read_dereferencing_p)
	le = CONS(EFFECT, make_anywhere_effect(is_action_read), le); 
      
      if (!write_dereferencing_p)
	if (!read_dereferencing_p)
	  {
	    le = effects_dup(l_pointer_eff);
	  }
	else
	  {
	    list l_write = effects_write_effects(l_pointer_eff);
	    list lkeep_read = effects_read_effects(lkeep);
	    le = gen_nconc(le, effects_dup(lkeep_read));
	    le = gen_nconc(le, effects_dup(l_write));
	    gen_free_list(l_write);
	    gen_free_list(lkeep_read);
	  }
      else
	{
	  if (!read_dereferencing_p)
	    {
	      list l_read = effects_read_effects(l_pointer_eff);
	      list lkeep_write = effects_write_effects(lkeep);
	      le = gen_nconc(le, effects_dup(lkeep_write));
	      le = gen_nconc(le, effects_dup(l_read));
	      gen_free_list(l_read);
	      gen_free_list(lkeep_write);
	    }
	  else
	    {
	      le = gen_nconc(le, effects_dup(lkeep));
	    }
	}
      gen_free_list(lkeep);
	
    }

  pips_debug_effects(8, "ouput effects : \n", le);
	
  return le;
}



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

#include "genC.h"

#include "effects-generic.h"


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
	pips_user_warning("non integer loop index %s.\n", entity_local_name(index));
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


/***********************************************************************/
/* COMBINATION OF APPROXIMATIONS                                       */
/***********************************************************************/



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

/***********************************************************************/
/* DESCRIPTORS (should not be there)                                   */
/***********************************************************************/

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
load_undefined_context(statement s)
{
    return transformer_undefined;
}

transformer
load_undefined_transformer(statement s)
{
    return transformer_undefined;
}

bool
empty_context_test_false(transformer context)
{
    return FALSE;
}

void 
effects_computation_no_init(string module_name)
{
    return;
}

void 
effects_computation_no_reset(string module_name)
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
	  reference_variable(effect_reference(ENTITY(CAR(ceffect))));
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
	  reference_variable(effect_reference(ENTITY(CAR(ceffect))));
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
	entity e_used = reference_variable(effect_reference(ef));
	
	/* Note: to test aliasing == should be replaced below by
	 * entity_conflict_p()
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
 */
bool effects_read_or_write_entity_p(fx, e)
cons * fx;
entity e;
{
    bool read_or_write = FALSE;
    MAPL(cef, 
     {
	 effect ef = EFFECT(CAR(cef));
	 entity e_used = reference_variable(effect_reference(ef));
	 /* Used to be a simple pointer equality test */
	 if(entity_conflict_p(e, e_used)) {
	     read_or_write = TRUE;
	     break;
	 }
     },
	 fx);
    return read_or_write;
}

entity effects_conflict_with_entity(fx, e)
cons * fx;
entity e;
{
    entity conflict_e = entity_undefined;
    MAPL(cef, 
     {
	 effect ef = EFFECT(CAR(cef));
	 entity e_used = reference_variable(effect_reference(ef));
	 if(entity_conflict_p(e, e_used)) {
	     conflict_e = e_used;
	     break;
	 }
     },
	 fx);
    return conflict_e;
}

/*************** I/O EFFECTS *****************/
bool io_effect_entity_p(entity e)
{
    return io_entity_p(e) && 
	same_string_p(entity_local_name(e), IO_EFFECTS_ARRAY_NAME);
}

/* Return true if a statement has an I/O effect in the effects
   list. */
bool
statement_io_effect_p(statement s)
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
          reference a_reference = effect_reference(an_effect);
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
   the argument (formal parameter) of the module. Note that the return
   variable of a function is also considered here as a formal
   parameter. */
bool
statement_has_a_formal_argument_write_effect_p(statement s)
{
   bool write_effect_on_a_module_argument_found = FALSE;
   entity module = get_current_module_entity();
   list effects_list = load_proper_rw_effects_list(s);

   MAP(EFFECT, an_effect,
       {
          entity a_variable = reference_variable(effect_reference(an_effect));
          
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

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
/* package scalarization
 *
 * Substitute array references by scalar references when legal and
 * profitable in loop bodies. The key function is loop_scalarization().
 *
 * The legality condition is based on a linear algebra function to
 * decide if a region function defined a function from any one
 * iteration towards an array element.
 *
 * Because the C3 linear library cannot create new entities, i.e. new
 * dimensions, the c_functional_graph_p() cannot have a clean
 * interface.
 *
 * Nevertheless, some functions should be moved in linear.
 */

#include <stdlib.h>
#include <stdio.h>

// FI: I did not include accel-util.h initialement
// parser_private.h is surprising as is top-level.h
//
// svn blame shed some light on this includes...

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "database.h"
#include "ri-util.h"
#include "effects-util.h"
#include "control.h"
#include "constants.h"
#include "misc.h"
#include "parser_private.h"
#include "top-level.h"
#include "text-util.h"
#include "text.h"
#include "transformer.h"
#include "semantics.h"
#include "effects-generic.h"
#include "effects-convex.h"
#include "pipsdbm.h"
#include "properties.h"
#include "resources.h"
#include "accel-util.h"
#include "transformations.h"


////////////////////

/*
 * WARNING ! This function introduces side-effects on g
 */

static Psysteme sc_add_offset_variables(Psysteme g, Pbase b, Pbase db)
{
  Pbase gb = sc_base(g);

  /* check validity conditions:
     - b must appear in g's basis but db must not, and
     - b and db must have the same dimension.
   */
  if (base_included_p(b,gb)
      && !base_included_p(db,gb)
      && base_dimension(b)==base_dimension(db)
      ) {

    Pcontrainte eqs   = sc_egalites(g);
    Pcontrainte ineqs = sc_inegalites(g);
    Pcontrainte c     = CONTRAINTE_UNDEFINED;

    /* update g's basis */
    Pbase tb = sc_base(g);
    sc_base(g) = base_union(tb, db);
    sc_dimension(g) += base_dimension(db);
    base_rm(tb);

    for(c = eqs ; !CONTRAINTE_UNDEFINED_P(c) ; c = contrainte_succ(c)) {
      Pbase cb  = BASE_NULLE;
      Pbase cdb = BASE_NULLE;
      for (cb=b,cdb=db ; !BASE_NULLE_P(cb) ; cb = vecteur_succ(cb), cdb = vecteur_succ(cdb)) {
        Variable v     = vecteur_var(cb);
        Variable dv    = vecteur_var(cdb);
        Value    coeff = vect_coeff(v, contrainte_vecteur(c));
        vect_add_elem(&(contrainte_vecteur(c)), dv, coeff);
      }
    }

    for(c = ineqs ; !CONTRAINTE_UNDEFINED_P(c) ; c= contrainte_succ(c)) {
      Pbase cb  = BASE_NULLE;
      Pbase cdb = BASE_NULLE;
      for (cb=b,cdb=db ; !BASE_NULLE_P(cb) ; cb = vecteur_succ(cb), cdb = vecteur_succ(cdb)) {
        Variable v     = vecteur_var(cb);
        Variable dv    = vecteur_var(cdb);
        Value    coeff = vect_coeff(v, contrainte_vecteur(c));
        vect_add_elem(&(contrainte_vecteur(c)), dv, coeff);
      }
    }
  }
  return g;
}


/*
   This function checks that graph g is a function graph from domain d
   to range r.

   Return value :
   - TRUE if the graph is certainly functional,
   - FALSE if it *might* be not functional.

   NOTE: parameter dr could be derived from r within this function,
   but we do not know how to create generic variables in Linear. The
   initial implementation used a PIPS function to generate the new
   variables.
 */

static bool sc_functional_graph_p(Psysteme g, Pbase d, Pbase r, Pbase dr)
{
  bool functional_p = TRUE;

  Psysteme g1, g2, g3, g4;

  // check validity conditions: d and r should be included in g's basis.
  Pbase gb = sc_base(g);
  if (!(base_included_p(d, gb) && base_included_p(r, gb))) {
    // illegal arguments
    functional_p = FALSE; //TODO
  }
  else {

    Pbase cr = BASE_UNDEFINED;

    // Create two copies of g
    g1 = sc_copy(g);
    g2 = sc_copy(g);

    // Substitute r by r + dr in g2 (indexed by dimension)
    g2 = sc_add_offset_variables(g2, r, dr);

    // Merge g1 and g2 into a unique system g3, eliminating redundencies.
    g3 = sc_intersection(SC_EMPTY, g1, g2);
    g3 = sc_elim_redond(g3);

    // Project g3 on dr space -> result: g4. If projection fails, return FALSE.
    Pbase dr4 = BASE_NULLE;
    // dr4 := list of variables of g3's basis which are not in dr
    for ( cr = sc_base(g3) ; !BASE_UNDEFINED_P(cr) ; cr = vecteur_succ(cr) ) {
      Variable vr = vecteur_var(cr);
      if (!base_find_variable(dr, vr))
        dr4 = base_add_variable(dr4, vr);
    }
    g4 = sc_copy(g3);
    sc_projection_along_variables_ofl_ctrl(&g4, dr4, OFL_CTRL);
    base_rm(dr4);
    /*
      ifdebug(1) {
      pips_debug(1, "g4 =\n");
      sc_print(g4, (get_variable_name_t)entity_local_name);
      }
    */
    if (SC_EMPTY_P(g4)) {
      functional_p = FALSE;
    }
    else {
      // Check that all r_b_i variables are null, using sc_minmax_of_variables()
      for ( cr = dr ; !BASE_UNDEFINED_P(cr) ; cr = vecteur_succ(cr) ) {
        Psysteme g4b = sc_copy(g4);
        Variable dv  = vecteur_var(cr);
        Value pmin, pmax;
        bool feasible_p = sc_minmax_of_variable(g4b,dv, &pmin, &pmax);
        if (!(feasible_p && value_eq(VALUE_ZERO, pmin) && value_eq(VALUE_ZERO, pmax))) {
          functional_p = FALSE;
          break;
        }
      }
    }
    // Cleanup
    sc_rm(g1);
    sc_rm(g2);
    sc_rm(g3);
    sc_rm(g4);
  }
  return functional_p;
}


/*
   This function checks that graph g is a total function graph, i.e. a
   mapping graph,from domain d to range r.
*/
static bool sc_totally_functional_graph_p( Psysteme g, // function graph
                                    Pbase d,    // domain's basis
                                    Psysteme D, // membership predicate for functional domain
                                    Pbase r,    // range's predicate
                                    Pbase dr    // difference variable
                                    )
{
  bool totally_functional_p = FALSE;

  if (sc_functional_graph_p(g, d, r, dr)) {

    // Check args coherence : d should be included in D's basis.
    if (base_included_p(d, sc_base(D))) {

      // Project g on d along r.
      Psysteme g1 = sc_copy(g);
      sc_projection_along_variables_ofl_ctrl(&g1, r, OFL_CTRL);

      // By definition of a total function, D must be included in g1.
      totally_functional_p = sc_inclusion_p_ofl_ctrl(D, g1, OFL_CTRL);
    }
  }
  return totally_functional_p;
}


// We need a Pbase to accumulate loop indices during loop traversal
static Pbase loop_indices_b = BASE_NULLE;

// These are needed for callback function reference_substitute
static reference scalarized_reference = reference_undefined;
static entity scalarized_replacement_variable = entity_undefined;

// gen_recurse callback function for
// statement_substitute_scalarized_array_references
static bool reference_substitute(reference r) {
  bool result = FALSE;
  entity v = reference_variable(r);
  if(reference_equal_p(r,scalarized_reference)) {
    // Scalarize only if r refers to an array element and not to a slice
    list inds = reference_indices(r);
    size_t d = type_depth(ultimate_type(entity_type(v)));
    if (gen_length(inds) == d) {
      reference_variable(r) = scalarized_replacement_variable;
      reference_indices(r) = NIL; // TODO: add missing gen_full_free_list(reference_indices(r))
      result = TRUE;
    }
  }
  return result;
}


static void statement_substitute_scalarized_array_references(statement st, reference pvr, entity s)
{
  //TODO: create context ansd use gen_multi_recurse_with_context
  scalarized_reference = pvr;
  scalarized_replacement_variable = s;
  gen_recurse (st, reference_domain, reference_substitute, gen_null);
  scalarized_reference = reference_undefined;
  scalarized_replacement_variable = entity_undefined;
}


static void * car_effect_to_variable(gen_chunk car) {
  return effect_variable(EFFECT(car)); // type 'entity'
}


static Pbase make_phi_base(int phi_min, int phi_max)
{
  Pbase phi_b = BASE_NULLE;
  int i;
  for(i=phi_min; i<=phi_max; i++)
    phi_b = base_add_variable(phi_b, (Variable) make_phi_entity(i));
  return(phi_b);
}


static effect  effect_write_or_read_on_variable(list el, entity v, boolean write_p)
{
  effect result = effect_undefined;
  if(v) {
    FOREACH(EFFECT, e, el) {
      if (store_effect_p(e)) {
	action a  = effect_action(e);
	entity ev = effect_entity(e);
	if (same_entity_p(ev,v) && (write_p ? action_write_p(a):action_read_p(a)))
	  return(e);
      }
    }
  }
  return result;
}



// To keep track of already scalarized vars
static list scalarized_variables = list_undefined;

static bool loop_scalarization(loop l)
{
  entity i    = loop_index(l);
  statement s = loop_body(l);
  ifdebug(1) {
    pips_debug(1, "Statement:\n");
    print_statement(s);
  }

  transformer prec = load_statement_precondition(s);
  Psysteme D       = predicate_system(transformer_relation(transformer_range(prec)));

  effects ie   = load_in_effects(s);
  effects oe   = load_out_effects(s);
  effects crwe = load_cumulated_rw_effects(s);

  list irl  = effects_store_effects(effects_effects(ie));
  list orl  = effects_store_effects(effects_effects(oe));
  list crwl = effects_store_effects(effects_effects(crwe));

  ifdebug(1) {
    pips_debug(1, "Entering function...\n");
    pips_debug(1, "Entering level-%d loop, index=%s\n", base_dimension(loop_indices_b), entity_name(i));
    pips_debug(1, "OUT regions:\n");
    print_regions(orl);
    pips_debug(1, "CUMULATED RW regions:\n");
    print_regions(crwl);
  }

  Pvecteur var_already_seen = VECTEUR_NUL;
  // Now we determine which read/write effects are not copied out.
  FOREACH (EFFECT, pr, crwl) {
    if (!store_effect_p(pr))
      continue;

    entity pv  = effect_variable(pr);
    // Does the current variable appear in the in effect?
    entity iv  = (entity) gen_find(pv, irl, (bool (*)())gen_eq, car_effect_to_variable);
    // Does the current variable appear in the out effect?
    entity ov  = (entity) gen_find(pv, orl, (bool (*)())gen_eq, car_effect_to_variable);

    descriptor d = effect_descriptor(pr);
    if (descriptor_convex_p(d) &&
	!entity_is_argument_p(pv, scalarized_variables) && !vect_coeff(pv,var_already_seen)) {

      Psysteme sc = descriptor_convex(d);
      int nd = type_depth(entity_type(pv));
      Psysteme sc_union = SC_UNDEFINED;
      Psysteme sc2 = SC_UNDEFINED;
      effect pr2 = effect_undefined;

      vect_add_elem(&var_already_seen,pv,1);
      /* Search Effect with the opposite action */
      if ((pr2 = effect_write_or_read_on_variable(crwl,pv,!(action_write_p(effect_action(pr)))))
	  != effect_undefined) {
	descriptor d2= effect_descriptor(pr2);
	if (descriptor_convex_p(d2))
	  sc2 = descriptor_convex(d2);
      }
      /*  Merge Read and Write Effects constraints on pv */
      if (!SC_UNDEFINED_P(sc) && sc_dimension(sc) !=0) {
	if (!SC_UNDEFINED_P(sc2) && sc_dimension(sc2) !=0)
	  sc_union=sc_cute_convex_hull(sc2,sc);
	else
	  sc_union=sc_dup(sc);
      }
      if (!SC_UNDEFINED_P(sc_union)) {
	// Estimate the dynamic number of *element* and *variable*
	// occurrences in the loop body
	int neo = count_references_to_variable_element(s, pv);
	int nvo = count_references_to_variable(s, pv);

	/* Legality criterion:

	   if nvo is greater than neo, there must be hidden references
	   to the array, due for instance to a function call, and the
	   substitution might break dependence arcs.

	   So, we go on only if the two are equal.
	*/

	if (nvo == neo) {

	  bool read_pv    = effects_read_variable_p(crwl, pv);
	  bool written_pv = effects_write_variable_p(crwl, pv);
	  bool read_and_written_pv = read_pv && written_pv;

	  /* Profitability criterion:

	     - nd > 0: it's an array (do not scalarize scalar variables)

	     - neo > 2: if the number of references if greater than 2, the
	     copy-in and copy-out code overhead is assumed to be small
	     enough to make scalarization profitable.

	     - neo > 1: if the number of references is 2, the copy-in *xor*
	     the copy-out overhead meets the above criterion.

	     - else: if there is neither copy-in nor copy-out,
	     privatization is always useful.

	     FI: we should also check if the reference is loop invariant
	     and then decide that it is always profitable to scalarize
	     it... See scalarization34 in Transformations. We should
	     check if the region is a constant function.
	  */

	  if (nd > 0
	      && (neo > 2
		  || (neo > 1 && !read_and_written_pv)
		  || (entity_undefined_p(iv) && entity_undefined_p(ov))
		  )
	      ) {

	    /* Check that a unique element of pv is used. In other
	       words, the relationship between the iterations i and the
	       array element phi is a function, by definition of a
	       function.

	       This cannot be checked without auxiliary variables. It is
	       not possible to create PIPS compatible variables in
	       Linear. Hence, they are created here.

	       The proof is based on r(i)=phi and r(i)=phi+dphi and r is
	       a function implies dphi=0. If dphi!=0, then r cannot be a
	       function.

	       But we also want that each iteration uses at least one
	       element. The function must be a total function. FI: I do
	       not remember why we need a total function.
	    */

	    Pbase phi_b = make_phi_base(1, nd);
	    Pbase d_phi_b = BASE_NULLE;
	    Pbase cr = BASE_NULLE;

	    /* Build base d_phi_b using
	       make_local_temporary_integer_value_entity(void) */
	    for ( cr = phi_b ; !BASE_UNDEFINED_P(cr) ; cr = vecteur_succ(cr) ) {
	      entity e_d_phi_b = make_local_temporary_integer_value_entity();
	      d_phi_b = base_add_variable(d_phi_b, (Variable) e_d_phi_b);
	    }

	    if (sc_totally_functional_graph_p(sc_union, loop_indices_b,
					      D, phi_b, d_phi_b)) {
	      /* The array references can be replaced a references to a
		 scalar */

	      // Create new temp var of same type as the array element pv
	      type pvt      = ultimate_type(entity_type(pv));
	      variable pvtv = type_variable(pvt);
	      basic pvb     = variable_basic(pvtv);
	      basic svb     = copy_basic(pvb);

	      // Copy the a reference to pv, just in case we need it later
	      reference pvr = copy_reference(find_reference_to_variable(s, pv));

	      // Create a new variable and add
	      // its declaration to the current module
	      // If no default prefix is defined, use the variable name
	      // as prefix
	      string dpref = get_string_property("SCALARIZATION_PREFIX");
	      string epref = strlen(dpref)==0?
		concatenate(entity_user_name(pv), "_", NULL)
		: dpref;
	      entity sv = make_new_scalar_variable_with_prefix(epref, get_current_module_entity(), svb);
	      AddEntityToCurrentModule(sv);
	      scalarized_variables =
		arguments_add_entity(scalarized_variables, pv);

	      pips_debug(1,"Creating variable %s for variable %s\n",
			 entity_name(sv), entity_name(pv));

	      // Substitute all references to pv with references to new variable
	      statement_substitute_scalarized_array_references(s, pvr, sv);

	      // Take care of copy-in and copy-out code if necessary
	      if (get_bool_property("SCALARIZATION_FORCE_OUT")
		  || !entity_undefined_p(ov)) {
		// Generate copy-out code
		statement co_s =
		  make_assign_statement(reference_to_expression(pvr),
					entity_to_expression(sv));
		insert_statement(s, co_s, false);
	      }
	      else {
		//free_reference(pvr);
	      }

	      if (!entity_undefined_p(iv)) {
		// Generate copy-in code
		statement ci_s =
		  make_assign_statement(entity_to_expression(sv),
					reference_to_expression(copy_reference(pvr)));
		insert_statement(s, ci_s,true);
	      }
	      else {
		//free_reference(pvr);
	      }
	    }
	    // Clean-up
	    base_rm(phi_b);
	    base_rm(d_phi_b);
	    reset_temporary_value_counter();

	  }
	}
      }
      sc_rm(sc_union);
    }
  }
  vect_rm(var_already_seen);
  return TRUE;
}


/* gen_recurse callback on entering a statement. If it's a loop,
 * process it. Stack the loop index to build the iteration
 * space. Perform loop scalarization.
 */
static bool scalarization_statement_in(statement ls)
{
  bool result = TRUE;
  if (statement_loop_p(ls)) {
    loop l = statement_loop(ls);

    /* Insert a marker to keep track of the privatized variables
       inside loop l, lest they are privatized a second time in an inner
       loop.
    */
    entity i = loop_index(l);
    scalarized_variables = arguments_add_entity(scalarized_variables, i);

    // Accumulate new index in "domain" basis
    loop_indices_b = base_add_variable(loop_indices_b, (Variable) i);

    result = loop_scalarization(l);
  }
  return result;
}


/* gen_recurse callback on exiting a statement. If it's a loop, process it.
 * unstack all variables related to loops internal wrt the current
 * loop to restore the current iteration space.
 */
static void scalarization_statement_out(statement s)
{
  if (statement_loop_p(s)) {
    loop l   = statement_loop(s);
    entity i = loop_index(l);
    list nl  = NIL;

    pips_debug(1, "Exiting loop with index %s, size=%d\n",
               entity_name(i), base_dimension(loop_indices_b));

    /* Remove variables privatized in the current loop, so that
       successive loops don't interfere with each other.
    */
    for (list el=scalarized_variables; !ENDP(el); POP(el)) {
      entity e = ENTITY(CAR(el));
      if (e == i)
        break;
      else
        nl = CONS(ENTITY, e, nl);
    }
    gen_free_list(scalarized_variables);
    scalarized_variables = gen_nreverse(nl);

    loop_indices_b = base_remove_variable(loop_indices_b, (Variable) i);
  }
}


bool scalarization (char * module_name)
{
  entity module;
  statement module_stat;

  set_current_module_entity(module_name_to_entity(module_name));
  module = get_current_module_entity();

  set_current_module_statement( (statement)
                                db_get_memory_resource(DBR_CODE, module_name, TRUE) );
  module_stat = get_current_module_statement();


  set_cumulated_rw_effects((statement_effects)
                           db_get_memory_resource(DBR_REGIONS, module_name, TRUE));

  module_to_value_mappings(module);

  set_precondition_map((statement_mapping)
                       db_get_memory_resource(DBR_PRECONDITIONS, module_name, TRUE));
  set_in_effects((statement_effects)
                 db_get_memory_resource(DBR_IN_REGIONS, module_name, TRUE));
  set_out_effects((statement_effects)
                  db_get_memory_resource(DBR_OUT_REGIONS, module_name, TRUE));


  debug_on("SCALARIZATION_DEBUG_LEVEL");
  pips_debug(1, "begin\n");

  /* We now traverse our module's statements. */
  loop_indices_b = BASE_NULLE;
  scalarized_variables = NIL;
  gen_recurse(module_stat, statement_domain, scalarization_statement_in,
              scalarization_statement_out);
  scalarized_variables = list_undefined;

  pips_debug(1, "end\n");
  debug_off();

  /* Save modified code to database */
  module_reorder(module_stat);
  DB_PUT_MEMORY_RESOURCE(DBR_CODE, strdup(module_name), module_stat);

  /* TODO: Cleanup after scalarization */
  pips_assert("Loop index Pbase is empty", BASE_NULLE_P(loop_indices_b));

  reset_current_module_entity();
  reset_current_module_statement();

  reset_cumulated_rw_effects();
  reset_precondition_map();
  reset_in_effects();
  reset_out_effects();

  free_value_mappings();

  /* Return value */
  bool good_result_p = TRUE;

  return (good_result_p);

}

typedef struct {
    entity array;
    bool constant_p;
} references_constant_param;



static void all_array_references_constant_walker(reference ref, references_constant_param* p)
{
    if(same_entity_p(p->array,reference_variable(ref)))
    {
        expression offset = reference_offset(ref);
        p->constant_p&=extended_expression_constant_p(offset) && (!ENDP(reference_indices(ref)));
        free_expression(offset);
    }
}

static bool all_array_references_constant_p(statement in, entity array)
{
    references_constant_param p = { array,true};
    gen_context_recurse(in, &p, reference_domain, gen_true, all_array_references_constant_walker);
    return p.constant_p;
}

typedef struct {
    entity array;
    hash_table mapping;
} replace_references_constant_param;

static void replace_constant_array_references_walker(reference ref, replace_references_constant_param *p)
{
    if(same_entity_p(p->array,reference_variable(ref)))
    {
        /* we know for sure all indices are constant */
        expression offset = reference_offset(ref);
        intptr_t value;
        if(!expression_integer_value(offset,&value))
           pips_internal_error("reference index should be constants");
        /* add one to the value, because 0 seems reserved */
        entity var = (entity)hash_get(p->mapping,(void*)(1+value));
        if(var == HASH_UNDEFINED_VALUE)
        {
           var = make_new_scalar_variable_with_prefix(entity_user_name(p->array),get_current_module_entity(),basic_of_reference(ref));
           hash_put(p->mapping,(void*)(1+value),var);
           AddEntityToCurrentModule(var);
        }
        gen_full_free_list(reference_indices(ref));
        free_expression(offset);
        reference_indices(ref)=NIL;
        reference_variable(ref)=var;
    }
}

static void replace_constant_array_references(statement in, entity array)
{
    replace_references_constant_param p = { array , hash_table_make(hash_int, HASH_DEFAULT_SIZE) };
    gen_context_recurse(in,&p,reference_domain,gen_true, replace_constant_array_references_walker);
}

bool constant_array_scalarization(const char * module_name)
{
    set_current_module_entity(module_name_to_entity(module_name));
    set_current_module_statement( (statement)        db_get_memory_resource(DBR_CODE, module_name, true) );
    set sreferenced_entities = get_referenced_entities(get_current_module_statement());
    SET_FOREACH(entity,e,sreferenced_entities)
    {
        if((entity_array_p(e)||entity_pointer_p(e)) && all_array_references_constant_p(get_current_module_statement(),e))
        {
           replace_constant_array_references(get_current_module_statement(),e);
           if(!same_string_p(entity_module_name(e),module_name))
               pips_user_warning("changing entity %s from other module, result may be wrong\n",entity_user_name(e));
        }
    }
    set_free(sreferenced_entities);
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name, get_current_module_statement());
    reset_current_module_entity();
    reset_current_module_statement();
    return true;
}

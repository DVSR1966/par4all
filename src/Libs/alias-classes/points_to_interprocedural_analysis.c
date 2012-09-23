#include <stdlib.h>
#include <stdio.h>
/* For strdup: */
// FI: already defined elsewhere
// #define _GNU_SOURCE
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "database.h"
#include "ri-util.h"
#include "effects-util.h"
#include "constants.h"
#include "misc.h"
#include "parser_private.h"
#include "top-level.h"
#include "text-util.h"
#include "text.h"
#include "properties.h"
#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"
#include "pipsdbm.h"
#include "resources.h"
#include "newgen_set.h"
#include "points_to_private.h"
#include "alias-classes.h"
/* Points-to interprocedural analysis is inspired by the work of Wilson[95]. The documentation 
   of this phase is a chapter of Amira Mensi thesis. */




/*  Foreach real argument r and its corresponding formal one f  create the assignment f = r
    then compute points-to set s generated by the assignment.
    The result is the union of pt_caller and s
*/
set compute_points_to_binded_set(entity called_func, list real_args, set pt_caller)
{ 
  set s = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set pt_binded = set_generic_make(set_private, points_to_equal_p,
				   points_to_rank);

  cons *pc;
  int ipc;
  s = set_assign(s, pt_caller);
  for (ipc = 1, pc = real_args; pc != NIL; pc = CDR(pc), ipc++) {
    expression rhs = EXPRESSION(CAR(pc));
    entity pf = find_ith_parameter(called_func, ipc);
    expression lhs = entity_to_expression(pf);
    //statement stmt = make_assign_statement(lhs, rhs);
    //s = set_assign(s, points_to_assignment(stmt, lhs, rhs, s));	
    points_to_graph s_g = make_points_to_graph(false, s);
      points_to_graph a_g = assignment_to_points_to(lhs, rhs, s_g);
      s = set_assign(s, points_to_graph_set(a_g));
  }

  SET_FOREACH(points_to, pt, s) {
    reference r = cell_any_reference(points_to_sink(pt));
    entity e = reference_variable(r);
    if(entity_stub_sink_p(e))
      s = set_del_element(s,s,(void*)pt);
  }
  pt_binded = set_union(pt_binded, s, pt_caller);
  return pt_binded;
}


/* Filter out written effects on pointers */
list written_pointers_set(list eff) {
  list written_l = NIL;
  list wr_eff = effects_write_effects(eff);
  FOREACH(effect, ef, wr_eff) {
    if(effect_pointer_type_p(ef)){
      cell c = effect_cell(ef);
      written_l = gen_nconc(CONS(CELL, c, NIL), written_l);
    }
  }

  return written_l; 

}


/* obsolete function*/
/* For each stub cell e  find its formal corresponding parameter and add to it a dereferencing dimension */
cell formal_access_paths(cell e, list args, __attribute__ ((__unused__)) set pt_in)
{
  int i;
  cell f_a = cell_undefined;
  reference r1 = cell_any_reference(e);
  reference r = reference_undefined;
  entity v1 = reference_variable(r1);
  const char * en1 = entity_local_name(v1);
  list sl = NIL; 
  bool change_p = false ;
  while(!ENDP(args) && !change_p) {
    cell c = CELL(CAR(args));
    reference r2 = cell_to_reference(c);
    entity v2 = reference_variable(r2);
    const char * en2 = entity_local_name(v2);
    char *cmp = strstr(en1, en2);
    r = copy_reference(r2);
    for(i = 0; cmp != NULL && cmp[i]!= '\0' ; i++) {
      if(cmp[i]== '_') 	{
	expression s =  int_to_expression(0);
	sl = CONS(EXPRESSION, s, sl);
	change_p = true;
      }
    }
    if(change_p) {
      reference_indices(r) = gen_nconc( sl, reference_indices(r));
      f_a = make_cell_reference(r);  
      break ;
    }
    args = CDR(args);
  }
 
  if(cell_undefined_p(f_a)) 
    pips_user_error("Formal acces paths undefined \n");

  return f_a;
}


/* obsolete function*/
/* returns the prefix parameter of the stub c */
cell points_to_stub_prefix(cell s, list params)
{
  cell c = cell_undefined;
  reference r1 = cell_any_reference(s);
  entity v1 = reference_variable(r1);
  const char * en1 = entity_local_name(v1);
  while(!ENDP(params)) {
    c = CELL(CAR(params));
    reference r2 = cell_to_reference(c);
    entity v2 = reference_variable(r2);
    const char * en2 = entity_local_name(v2);
    char *cmp = strstr(en1, en2);
    if(cmp!=NULL)
      break ;
    params = CDR(params);
  }
  return c;

}

/* obsolete function*/
list points_to_backword_cell_translation(cell c, list params, set pt_in, set pt_binded)
{
  int i;
  list inds1 = NIL, inds2 = NIL;
  list inds = reference_indices(cell_any_reference(c));
  list tl = sink_to_sources(c, pt_in, true);
  // FI: impedance problem and memory leak
  points_to_graph pt_binded_g = make_points_to_graph(false, pt_binded);

  FOREACH(cell, s , tl) {
    if(stub_points_to_cell_p(s)) {
      cell f_a = formal_access_paths(s, params, pt_in);
      inds1 = reference_indices(cell_any_reference(f_a));
      inds2 = reference_indices(cell_any_reference(s));
      }
      cell pr = points_to_stub_prefix(s, params);
      tl = source_to_sinks(pr, pt_binded_g, true);
      for(i = 1; i <= (int) gen_length(inds1); i++) {
	list tmp = gen_full_copy_list(tl);
	FOREACH(cell, sk, tmp) {
	  tl = source_to_sinks(sk, pt_binded_g, true);
	}
	gen_free_list(tmp);
      }
      FOREACH(cell, st, tl) {
	reference r = cell_to_reference(st);
	reference_indices_(r) = gen_nconc(reference_indices_(r), inds2);
      }
  }
 
  if(!ENDP(inds)) {
    FOREACH(cell, t, tl) {
      reference r = cell_to_reference(t);
      reference_indices(r) = gen_nconc(reference_indices(r), inds);
    }
  }
  return tl;
}

/* obsolete function*/
/* Evalute c using points-to relation already computed */ 
list actual_access_paths(cell c, set pt_binded)
{
  bool exact_p = false;
  set_methods_for_proper_simple_effects();
  list l_in = set_to_sorted_list(pt_binded,
				 (int(*)(const void*, const void*))
				 points_to_compare_location);
  list l = eval_cell_with_points_to(c, l_in, &exact_p);
  generic_effects_reset_all_methods();
  return l;
}


/* obsolete function*/
/* Translate each element of E into the caller's scope */
list caller_addresses(cell c, list args, set pt_in, set pt_binded)
{
  
  cell c_f = formal_access_paths(c, args, pt_in);
  list a_p = actual_access_paths(c_f, pt_binded);
  if(ENDP(a_p))
    a_p = points_to_cell_translation(c,args, pt_in, pt_binded);
  return a_p;
}

/* obsolete function*/
list points_to_cell_translation(cell sink, list args, set pt_in, set pt_binded)
{
  list l = NIL, ca = NIL;
  list sources =  sink_to_sources(sink, pt_binded, true);
  FOREACH(cell, c, sources) {
    ca = gen_nconc(caller_addresses(c, args, pt_in, pt_binded), ca);
  }
  FOREACH(cell, s, ca) {
    l = gen_nconc(sink_to_sources(s, pt_binded, true), l);
  }
  return l;
}


/* Add cells referencing a points-to stub found in parameter "s" are
 * copied and added to list "osl".
 *
 * The stubs are returned as cells not as entities.
 *
 * New cells are allocated. No sharing is created between parameter
 * "s" and result "sl".
 */
list points_to_set_to_stub_cell_list(set s, list osl)
{
  list sl = osl;
  SET_FOREACH(points_to, pt, s) {
    cell sink = points_to_sink(pt);
    reference r1 = cell_any_reference(sink);
    entity e1 = reference_variable(r1);
    if(entity_stub_sink_p(e1) && !points_to_cell_in_list_p(sink, sl))
      sl = CONS(CELL, copy_cell(sink), sl);
      
    cell source = points_to_source(pt);
    reference r2 = cell_any_reference(source);
    entity e2 = reference_variable(r2);
    if(entity_stub_sink_p(e2) && !points_to_cell_in_list_p(source, sl))
      sl = CONS(CELL, copy_cell(source), sl);
  }
  
  gen_sort_list(sl, (gen_cmp_func_t) points_to_compare_ptr_cell );
  ifdebug(1) print_points_to_cells(sl);
  return sl;
}


/* returns all the element of E, the set of stubs created when the callee is analyzed.
 *
 * E = {e in pt_in U pt_out|entity_stub_sink_p(e)}
 *
 * FI->AM: a[*][*] or p[next] really are elements of set E?
 */
list stubs_list(set pt_in, set pt_out)
{
  list sli = points_to_set_to_stub_cell_list(pt_in, NIL);
  list slo = points_to_set_to_stub_cell_list(pt_out, sli);
  return slo;
}

/* Check compatibility of points-to set pt_in of the callee and
 * pt_binded of the call site in the caller.
 *
 * Parameter "stubs" is the set E in the intraprocedural and
 * interprocedural analysis chapters of Amira Mensi's PhD
 * dissertation. The list "stubs" contains all the stub references
 * generated when the callee is analyzed.
 *
 * Parameter "args" is a list of cells. Each cell is a reference to a
 * formal parameter of the callee.
 */
bool sets_binded_and_in_compatibles_p(list stubs,
				      list args,
				      set pt_binded,
				      set pt_in,
				      set pt_out __attribute__ ((unused)))
{
  bool compatible_p = true;
  // FI->AM: this set does not seem to be used
  // set io = new_simple_pt_map();
  set bm = new_simple_pt_map();
  list tmp = gen_full_copy_list(stubs);
  gen_sort_list(args, (gen_cmp_func_t) points_to_compare_ptr_cell );
  
  // io = set_union(io, pt_in, pt_out);

  /* "bm" is a mapping from the formal context to the calling
   * context. It includes the mapping from formal arguments to the
   * calling context.
   */
  bm = points_to_binding(args, pt_in, pt_binded);

  while(!ENDP(stubs) && compatible_p) {
    cell st = CELL(CAR(stubs));
    bool to_be_freed;
    type st_t = points_to_cell_to_type(st, &to_be_freed);
    if(pointer_type_p(st_t)) {
      bool found_p = false; // the comparison is symetrical
      FOREACH(CELL, c, tmp) {
	if(points_to_cell_equal_p(c, st))
	  found_p = true;
	if(found_p && !points_to_cell_equal_p(c, st)) {
	  bool to_be_freed_2;
	  type c_t = points_to_cell_to_type(c, &to_be_freed_2);
	  if(pointer_type_p(c_t)) {
	    // FI: impedance issue + memory leak
	    points_to_graph bm_g = make_points_to_graph(false, bm);
	    list act1 = source_to_sinks(c, bm_g, false);
	    list act2 = source_to_sinks(st, bm_g, false);
	    /* Compute the intersection of the abstract value lists of
	       the two stubs in the actual context. */
	    points_to_cell_list_and(&act1, act2);

	    if(!ENDP(act1)) {
	      /* We are still OK if the common element(s) are NULL,
		 NOWHERE or even maybe a general heap abstract
		 location. */
	      //entity ne = entity_null_locations();
	      //reference nr = make_reference(ne, NIL);
	      //cell nc = make_cell_reference(nr);
	      //cell nc = make_null_pointer_value_cell();
	      //entity he =  entity_all_heap_locations();
	      //reference hr = make_reference(he, NIL);
	      //cell hc = make_cell_reference(hr);
	  
	      //if((int) gen_length(act1) == 1
	      // && (!points_to_cell_in_list_p(nc, act1)
	      //     || !points_to_cell_in_list_p(hc, act1)))
	      // FI: I'm not convinced this test is correct
	      // I guess we should remove all possibly shared targets
	      // and then test for emptiness
	      if((int) gen_length(act1) == 1) {
		cell act1_c = CELL(CAR(act1));
		if(!null_cell_p(act1_c)
		   && !all_heap_locations_cell_p(act1_c)) {
		  compatible_p = false;
		  break;
		}
	      }
	    }
	    gen_free_list(act1);
	    gen_free_list(act2);
	  }
	  if(to_be_freed_2) free_type(c_t);
	}
      }
    }
    if(to_be_freed) free_type(st_t);
    stubs = CDR(stubs);
  }
  gen_full_free_list(tmp);
  // set_free(io);

  return compatible_p;
}

/* Translate the "out" set into the scope of the caller
 *
 * Shouldn't it be the "written" list that needs to be translated?
 */ 
set compute_points_to_kill_set(list written,
			       set pt_caller,
			       list args,
			       set pt_in,
			       set pt_binded)
{
  set kill = new_simple_pt_map(); 	
  list written_cs = NIL;
  set bm = new_simple_pt_map();

  // FI->AM: do you recompute "bm" several times?
  bm = points_to_binding(args, pt_in, pt_binded); 

  FOREACH(CELL, c, written) {
    cell new_sr = cell_undefined; 
    reference r_1 = cell_any_reference(c);
    entity v_1 = reference_variable(r_1);
    list ind1 = reference_indices(r_1);
    SET_FOREACH(points_to, pp, bm) {
      cell sr2 = points_to_source(pp); 
      cell sk2 = points_to_sink(pp); 
      reference r22 = copy_reference(cell_to_reference(sk2));
      /* FI->AM: this test is loop invariant... and performed within the loop */
      if(!source_in_set_p(c, bm)) {
	reference r_12 = cell_any_reference(sr2);
	entity v_12 = reference_variable( r_12 );
	if(same_string_p(entity_local_name(v_1),entity_local_name(v_12))) { 
	  reference_indices(r22) = gen_nconc(reference_indices(r22),
					     gen_full_copy_list(ind1));
	  new_sr = make_cell_reference(r22);
	  // FI->AM: I guess this statement was forgotten
	  written_cs = CONS(CELL, new_sr, written_cs);
	  break;
	}
	else if (heap_cell_p(c)) {
	  written_cs = CONS(CELL, c, written_cs);
	}
      }
      else if(points_to_compare_cell(c,sr2)) {
	written_cs = CONS(CELL, points_to_sink(pp), written_cs);
	break;
      }
    }
  }
  
  FOREACH(CELL, c, written_cs) {
    SET_FOREACH(points_to, pt, pt_caller) {
      if(points_to_cell_equal_p(c, points_to_source(pt)))
	set_add_element(kill, kill, (void*)pt);
    }
  }

  return kill;
}



/* Translate the out set in the scope of the caller using the binding information */
set compute_points_to_gen_set(list args, set pt_out, set pt_in, set pt_binded)
{
  set gen = new_simple_pt_map(); 		
  set bm = new_simple_pt_map();
  bm = points_to_binding(args, pt_in, pt_binded);  

  SET_FOREACH(points_to, p, pt_out) {
    cell new_sr = cell_undefined; 
    cell new_sk = cell_undefined;
    approximation a = points_to_approximation(p);
    cell sr1 = points_to_source(p); 
    cell sk1 = points_to_sink(p); 
    reference r_1 = cell_any_reference(sr1);
    reference r_2 = cell_any_reference(sk1);
    entity v_1 = reference_variable(r_1);
    list ind1 = reference_indices(r_1);
    entity v_2 = reference_variable(r_2);
    SET_FOREACH(points_to, pp, bm) {
      cell sr2 = points_to_source(pp); 
      cell sk2 = points_to_sink(pp); 
      reference r22 = copy_reference(cell_to_reference(sk2));
      if(!source_in_set_p(sr1, bm)) {
	reference r_12 = cell_any_reference(sr2);
	entity v_12 = reference_variable( r_12 );
	if(same_string_p(entity_local_name(v_1),entity_local_name(v_12))) { 
	  reference_indices_(r22) = gen_nconc(reference_indices(r22),ind1);
	  new_sr = make_cell_reference(r22);
	  break;
	}
	else if (heap_cell_p(sr1)) {
	  new_sr = sr1;
	}
      }
      else if(points_to_compare_cell(sr1,sr2)) {
	new_sr = points_to_sink(pp);
	break;
      }
    }
    SET_FOREACH(points_to, pp1, bm) {
      cell sr2 = points_to_source(pp1); 
      cell sk2 = points_to_sink(pp1); 
      reference r22 = copy_reference(cell_to_reference(sk2));
      if(!source_in_set_p(sk1, bm)) {
	reference r_12 = cell_any_reference(sr2);
	entity v_12 = reference_variable( r_12 );
	if(same_string_p(entity_local_name(v_2),entity_local_name(v_12))) { 
	  reference_indices_(r22) = gen_nconc(reference_indices(r22),ind1);
	  new_sk = make_cell_reference(r22);
	  break;
	}
	else if (null_cell_p(sk1) || nowhere_cell_p(sk1) || heap_cell_p(sk1) || anywhere_cell_p(sk1)) {
	  new_sk = sk1;
	}
      }
      else if(points_to_compare_cell(sk1,sr2)) {
	new_sk = points_to_sink(pp1);
	break;
      }
    }
    
    if(!cell_undefined_p(new_sr) && !cell_undefined_p(new_sk)) { 
      points_to new_pt = make_points_to(new_sr, new_sk, a, make_descriptor_none());
      set_add_element(gen, gen, (void*)new_pt);
    }
  }

  ifdebug(1)   print_points_to_set("gen", gen);
  return gen;  
}


/* find all the arcs, ai, starting from the argument c1 using in, 
   find all the arcs, aj, starting from the parameter c2 using pt_binded,
   map each node ai to its corresponding aj.
*/
set points_to_binding_arguments(cell c1, cell c2 , set in, set pt_binded)
{
  set bm = new_simple_pt_map();
  if(!source_in_set_p(c1, in)) {
    points_to pt = make_points_to(c1, c2, make_approximation_exact(), make_descriptor_none());
    add_arc_to_simple_pt_map(pt, bm);
  } else {
    // FI: impedance problem... and memory leak
    points_to_graph in_g = make_points_to_graph(false, in);
    points_to_graph pt_binded_g = make_points_to_graph(false, pt_binded);
    list sinks1 = source_to_sinks(c1, in_g, true);
    list sinks2 = source_to_sinks(c2, pt_binded_g, true);
    list tmp1 = gen_full_copy_list(sinks1);
    list tmp2 = gen_full_copy_list(sinks2);

    FOREACH(CELL, s1, sinks1) {
      FOREACH(CELL, s2, sinks2) {
	approximation a = approximation_undefined;
	if((size_t)gen_length(sinks2)>1)
	  a = make_approximation_may();
	else
	  a = make_approximation_exact();
	cell sink1 = copy_cell(s1);
	cell sink2 = copy_cell(s2);
	points_to pt = make_points_to(sink1, sink2, a, make_descriptor_none());
	add_arc_to_simple_pt_map(pt, bm);
	gen_remove(&sinks2, (void*)s2);
      }
      gen_remove(&sinks1, (void*)s1);
    }

    FOREACH(CELL, sr1, tmp1) {
      FOREACH(CELL, sr2, tmp2) {
	bm = set_union(bm,points_to_binding_arguments(sr1, sr2, in, pt_binded), bm);
      }
    }
  }
  return bm;
}

/* find for the source of p its corresponding alias, which means
   finding another source that points to the same location.  */
cell points_to_source_alias(points_to pt, set pts)
{
  cell source = cell_undefined;
  cell sink1 = points_to_sink(pt);
  SET_FOREACH(points_to, p, pts) {
    cell sink2 =  points_to_sink(p);
    if(cell_equal_p(sink1, sink2)) {
      source = points_to_source(p);
      break ;
      }
  }
  if(cell_undefined_p(source))
    pips_user_error("");

  return source;

}

/* Using the result of points_to_binding_arguments complete the process of binding each element of in to its corresponding at the call site.
   Necessery to translate fields structure.
 */
set points_to_binding(list args, set in, set pt_binded)
{

  set bm = new_simple_pt_map();
  set bm1 = new_simple_pt_map();
 
 SET_FOREACH(points_to, pt, pt_binded) {
    FOREACH(CELL, c1, args) {
      cell source = points_to_source(pt);
      if(cell_equal_p(c1, source)) {
	cell c2 = points_to_source_alias(pt, pt_binded);
	approximation a = make_approximation_exact();
	points_to pt = make_points_to(c1, c2, a, make_descriptor_none());
	add_arc_to_simple_pt_map(pt, bm);
	bm = set_union(bm, bm, points_to_binding_arguments(c1 , c2,  in, pt_binded));
      }
    }
  }

  set_assign(bm1, bm);
  SET_FOREACH(points_to, p1,in) {
    cell s = points_to_source(p1);
    if(!source_in_set_p(s,bm)) {
      reference r = cell_any_reference(s);
      entity e = reference_variable(r);
      list ind = reference_indices(r);
      SET_FOREACH(points_to, pt, bm1) {
	cell source = points_to_source(pt);
	reference rr = cell_any_reference(source);
	entity ee = reference_variable(rr);
	if(same_string_p(entity_local_name(e),entity_local_name(ee))) {
	  cell sink = points_to_sink(pt);
	  reference r1 = copy_reference(cell_any_reference(sink));
	  reference_indices_(r1) = gen_nconc(reference_indices(r1),ind);
	  cell new_sk = make_cell_reference(r1);
	  approximation a = points_to_approximation(pt);
	  points_to pt = make_points_to(s, new_sk, a, make_descriptor_none());
	  add_arc_to_simple_pt_map(pt, bm);
	  bm = set_union(bm, points_to_binding_arguments(s, new_sk, in, pt_binded), bm);
	  break;
	}
      }
    }
  }

  return bm;
}

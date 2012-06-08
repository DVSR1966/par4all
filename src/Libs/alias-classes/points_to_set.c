/* private implementation of points_to set.

   points_to_equal_p to determine if two points_to relations are equal (same
   source, same sink, same relation)

   points_to_rank   how to compute rank for a points_to element

*/
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
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
#include "syntax.h"
#include "top-level.h"
#include "text-util.h"
#include "text.h"
#include "properties.h"
#include "pipsmake.h"
#include "semantics.h"
#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"
#include "transformations.h"
#include "preprocessor.h"
#include "pipsdbm.h"
#include "resources.h"
#include "prettyprint.h"
#include "newgen_set.h"
#include "points_to_private.h"
#include "alias-classes.h"
#include "genC.h"

#define INITIAL_SET_SIZE 10

int compare_entities_without_scope(const entity *pe1, const entity *pe2)
{
  int
    null_1 = (*pe1==(entity)NULL),
    null_2 = (*pe2==(entity)NULL);

  if (null_1 || null_2)
    return(null_2-null_1);
  else {
    /* FI: Which sorting do you want? */

    string s1 = entity_name(*pe1);
    string s2 = entity_name(*pe2);

    return strcmp(s1,s2);
  }
}

entity location_entity(cell c)
{
  reference r = cell_to_reference(c);
  entity e = reference_variable(r);

  return e;
}



/*return true if two acces_path are equals*/
bool locations_equal_p(cell acc1, cell acc2)
{
  return cell_equal_p(acc1, acc2);
}

/* returns true if two points-to arcs "vpt1" and "vpt2" are equal.
 *
 * Used to build sets of points-to using the set library of Newgen
 */
int points_to_equal_p( const void * vpt1, const void*  vpt2)
{
  points_to pt1 = (points_to) vpt1;
  points_to pt2 = (points_to) vpt2;

 //same source
  cell c1 = points_to_source(pt1);
  cell c2 = points_to_source(pt2);
  bool cmp1 = locations_equal_p(c1,c2);

 // same sink
  cell c3 = points_to_sink(pt1);
  cell c4 = points_to_sink(pt2);
  bool cmp2 = locations_equal_p(c3,c4);

 // same approximation
  approximation a1 = points_to_approximation(pt1);
  approximation a2 = points_to_approximation(pt2);
  // FI: must is forgotten...
  //bool cmp3 = (approximation_exact_p(a1) && approximation_exact_p(a2))
  //  || ( approximation_may_p(a1) && approximation_may_p(a2));
  // FI: should we identify "exact" and "must"?
  bool cmp3 = (approximation_tag(a1)==approximation_tag(a2));
  bool cmp = cmp1 && cmp2 && cmp3;

  ifdebug(8) {
    printf("%s for pt1=%p and pt2=%p\n", bool_to_string(cmp), pt1, pt2);
    print_points_to(pt1);
    print_points_to(pt2);
  }

  return cmp;
}


/* create a key which is a concatenation of the source's name, the
  sink's name and the approximation of their relation(may or exact)*/
_uint points_to_rank( const void *  vpt, size_t size)
{
  points_to pt= (points_to)vpt;
   cell source = points_to_source(pt);
  cell sink = points_to_sink(pt);
  approximation rel = points_to_approximation(pt);
  tag rel_tag = approximation_tag(rel);
  string s = strdup(i2a(rel_tag));
  reference sro = cell_to_reference(source);
  reference sri = cell_to_reference(sink);
  string s1 = strdup(words_to_string(words_reference(sro, NIL)));
  string s2 = strdup(words_to_string(words_reference(sri, NIL)));
  string key = strdup(concatenate(s1,
				  " ",
				  s2,
				  s,
				  NULL));
  return hash_string_rank(key,size);
}

/* Remove from "pts" arcs based on at least one local entity in list
 * "l" and preserve those based on static and global entities. This
 * function is called when exiting a statement block.
 *
 * Detection of dangling pointers.
 *
 * Detection of memory leaks. Could be skipped when dealing with the
 * "main" module, but the information would have to be passed down
 * thru an extra parameter.
 *
 * Side-effects on argument "pts".
 */
set points_to_block_projection(set pts, list  l)
{
  list pls = NIL; // Possibly lost sinks
  FOREACH(ENTITY, e, l) {
    type uet = ultimate_type(entity_type(e));
    if(pointer_type_p(uet)) {
      SET_FOREACH(points_to, pt, pts){
	cell source = points_to_source(pt);
	cell sink = points_to_sink(pt);
	entity e_sr = reference_variable(cell_to_reference(source));
	entity e_sk = reference_variable(cell_to_reference(sink));

	if(e == e_sr && (!(variable_static_p(e_sr) || top_level_entity_p(e_sr) || heap_cell_p(source)))) {
	  set_del_element(pts, pts, (void*)pt);
	  if(heap_cell_p(sink)) {
	    /* Check for memory leaks */
	    pls = CONS(CELL, sink, pls);
	  }
	}
	else if(e == e_sk
		&& (!(variable_static_p(e_sk)
		      || top_level_entity_p(e_sk)
		      || heap_cell_p(sink)))) {
	  if(gen_in_list_p(e_sr, l)) {
	    /* Both the sink and the source disappear: the arc is removed */
	    set_del_element(pts, pts, (void*)pt);
	  }
	  else {
	    pips_user_warning("Dangling pointer %s \n", entity_user_name(e_sr));
	    list lhs = CONS(CELL, source, NIL);
	    pts = points_to_nowhere_typed(lhs, pts);
	  }
	}
      }
    }
  }
  /* Any memory leak? */
  FOREACH(CELL, c, pls) {
    if(!sink_in_set_p(c, pts)) {
      reference r = cell_any_reference(c);
      entity b = reference_variable(r);
      pips_user_warning("Memory leak for bucket \"%s\".\n",
			entity_name(b));
    }
  }
  return pts;
}

/* Remove all arcs starting from e because e has been assigned a new value */
set points_to_source_projection(set pts, entity e)
{
  list pls = NIL; // Possibly lost sinks

  SET_FOREACH(points_to, pt, pts) {
    cell source = points_to_source(pt);
    cell sink = points_to_sink(pt);
    entity e_sr = reference_variable(cell_to_reference(source));
    //entity e_sk = reference_variable(cell_to_reference(sink));

    if(e == e_sr) {
      set_del_element(pts, pts, (void*)pt);
      if(heap_cell_p(sink)) {
	/* Check for memory leaks */
	pls = CONS(CELL, sink, pls);
      }
    }
  }

  /* Any memory leak? */
  FOREACH(CELL, c, pls) {
    if(!sink_in_set_p(c, pts)) {
      reference r = cell_any_reference(c);
      entity b = reference_variable(r);
      pips_user_warning("Memory leak for bucket \"%s\".\n",
			entity_name(b));
    }
  }
  return pts;
}

/* FI: side-effects to be used in this function */
set points_to_function_projection(set pts)
{
  set res = set_generic_make(set_private, points_to_equal_p,
			     points_to_rank);
  set_assign(res, pts);
  /* Do we have a useful return value? */
  type ft = ultimate_type(entity_type(get_current_module_entity()));
  type rt = ultimate_type(functional_result(type_functional(ft)));
  entity rv = entity_undefined;
  if(pointer_type_p(rt))
    rv = function_to_return_value(get_current_module_entity());

  SET_FOREACH(points_to, pt, pts) {
    if(cell_out_of_scope_p(points_to_source(pt))) {
      /* Preserve the return value */
      reference r = cell_any_reference(points_to_source(pt));
      entity v = reference_variable(r);
      if(rv!=v)
	set_del_element(res, res, (void*)pt);
    }
  }
  return res;
}

/* Return true if a cell is out of scope */
bool cell_out_of_scope_p(cell c)
{
  reference r = cell_to_reference(c);
  entity e = reference_variable(r);
  return !(variable_static_p(e) ||  entity_stub_sink_p(e) || top_level_entity_p(e) || entity_heap_location_p(e));
}

/* print a points-to arc for debug */
void print_points_to(const points_to pt)
{
  if(points_to_undefined_p(pt))
    (void) fprintf(stderr,"POINTS_TO UNDEFINED\n");
  // For debugging with gdb, dynamic type checking
  else if(points_to_domain_number(pt)!=points_to_domain)
    (void) fprintf(stderr,"Arg. \"pt\"is not a points_to.\n");
  else {
    cell source = points_to_source(pt);
    cell sink = points_to_sink(pt);
    approximation app = points_to_approximation(pt);
    reference r1 = cell_to_reference(source);
    reference r2 = cell_to_reference(sink);

    fprintf(stderr,"%p ", pt);
    print_reference(r1);
    fprintf(stderr,"->");
    print_reference(r2);
    fprintf(stderr," (%s)\n", approximation_to_string(app));
  }
}

/* Print a set of points-to for debug */
void print_points_to_set(string what,  set s)
{
  fprintf(stderr,"points-to set %s:\n", what);
  if(set_undefined_p(s))
    fprintf(stderr, "undefined set\n");
  else if(s==NULL)
    fprintf(stderr, "uninitialized set\n");
  else if(set_size(s)==0)
    fprintf(stderr, "empty set\n");
  else
    SET_MAP(elt, print_points_to((points_to) elt), s);
  fprintf(stderr, "\n");
}

/* test if a cell appear as a source in a set of points-to */
bool source_in_set_p(cell source, set s)
{
  bool in_p = false;
  SET_FOREACH ( points_to, pt, s ) {
    /* if( opkill_may_vreference(source, points_to_source(pt) )) */
    if(cell_equal_p(source, points_to_source(pt)))
      return true;
  }
  return in_p;
}

/* test if a cell appear as a sink in a set of points-to */
bool sink_in_set_p(cell sink, set s)
{
  bool in_p = false;
  SET_FOREACH ( points_to, pt, s ) {
    if( cell_equal_p(points_to_sink(pt),sink) )
      in_p = true;
  }
  return in_p;
}

/* The approximation is not taken into account
 *
 * It might be faster to look up the different points-to arcs that can
 * be made with source, sink and any approximation.
 */
points_to find_arc_in_points_to_set(cell source, cell sink, pt_map s)
{
  points_to fpt = points_to_undefined;
  SET_FOREACH(points_to, pt, s) {
    if(cell_equal_p(points_to_source(pt), source)
       && cell_equal_p(points_to_sink(pt), sink) ) {
      fpt = pt;
      break;
    }
  }
  return fpt;
}

/* source is assumed to be either nowhere/undefined or anywhere, it
 * may be typed or not.
 *
 * Shouldn't we create NULL pointers if the corresponding property is set?
 * Does it matter for anywhere/nowhere?
 *
 * pts must be updated with the new arc(s).
 */
list anywhere_to_sinks(cell source, pt_map pts)
{
  list sinks = NIL;
  reference r = cell_any_reference(source);
  entity v = reference_variable(r);
  // FI: it would be better to print the reference... words_reference()...
  // FI: sinks==NIL would be interpreted as a bug...
  // If we dereference a nowhere/undefined, we should end up
  // anywhere to please gcc and Fabien Coelho
  type vt = ultimate_type(entity_type(v));
  if(type_variable_p(vt)) {
    if(pointer_type_p(vt)) {
      // FI: should nt be freed?
      type nt = type_to_pointed_type(vt);
      cell c = make_anywhere_points_to_cell(nt);
      sinks = CONS(CELL, c, NIL);
      points_to pt = make_points_to(copy_cell(source), c,
				    make_approximation_may(),
				    make_descriptor_none());
      pts = add_arc_to_pt_map(pt, pts);
    }
    else if(struct_type_p(vt)) {
      variable vrt = type_variable(vt);
      basic b = variable_basic(vrt);
      entity se = basic_derived(b);
      type st = entity_type(se);
      pips_assert("se is an internal struct", type_struct_p(st));
      list fl = type_struct(st);
      FOREACH(ENTITY, f, fl) {
	type ft = entity_type(f);
	type uft = compute_basic_concrete_type(ft); // FI: to be freed...
	if(pointer_type_p(uft)) {
	  cell nsource = copy_cell(source); // FI: memory leak?
	  nsource = points_to_cell_add_field_dimension(nsource, f);
	  type nt = type_to_pointed_type(uft);
	  cell nsink = make_anywhere_points_to_cell(nt);
	  points_to pt = make_points_to(nsource, nsink, 
					make_approximation_may(),
					make_descriptor_none());
	  pts = add_arc_to_pt_map(pt, pts);
	  //sinks = source_to_sinks(source, pts, false);
	  sinks = CONS(CELL, nsink, NIL);
	}
	else if(struct_type_p(uft)) {
	  cell nsource = copy_cell(source); // FI: memory leak?
	  nsource = points_to_cell_add_field_dimension(nsource, f);
	  sinks = anywhere_to_sinks(nsource, pts);
	  //pips_internal_error("Not implemented yet.\n");
	}
	else if(array_type_p(uft)) {
	  variable uftv = type_variable(uft);
	  basic uftb = variable_basic(uftv);
	  if(basic_pointer_p(uftb)) {
	    cell nsource = copy_cell(source); // FI: memory leak?
	    reference r = cell_any_reference(nsource);
	    reference_add_zero_subscripts(r, uft);
	    type nt = ultimate_type(uft); // FI: get rid of the dimensions
	    cell nsink = make_anywhere_points_to_cell(nt);
	    points_to pt = make_points_to(nsource, nsink, 
					  make_approximation_may(),
					  make_descriptor_none());
	    pts = add_arc_to_pt_map(pt, pts);
	    sinks = CONS(CELL, nsink, NIL);
	  }
	}
      }
    }
    else if(array_type_p(vt)) {
      variable uftv = type_variable(vt);
      basic uftb = variable_basic(uftv);
      if(basic_pointer_p(uftb)) {
	cell nsource = copy_cell(source); // FI: memory leak?
	reference r = cell_any_reference(nsource);
	reference_add_zero_subscripts(r, vt);
      }
    }
    else
      // FI: struct might be dereferenced?
      // FI: should this be tested when entering this function rather
      // than expecting that the caller is safe
      pips_internal_error("Unexpected dereferenced type.\n");
  }
  else if(type_area_p(vt)) {
    // FI: this should be removed using type_overloaded for the
    // "untyped" anywhere
    entity e = entity_anywhere_locations();
    reference r = make_reference(e, NIL);
    cell c = make_cell_reference(r);
    sinks = CONS(CELL, c, NIL);
    points_to pt = make_points_to(copy_cell(source), c,
				  make_approximation_may(),
				  make_descriptor_none());
    pts = add_arc_to_pt_map(pt, pts);
  }
  else {
    pips_internal_error("Unexpected dereferenced type.\n");
  }
  return sinks;
}

/* For debugging */
void print_points_to_path(list p)
{
  if(ENDP(p))
    fprintf(stderr, "p is empty.\n");
  else {
    FOREACH(CELL, c, p) {
      if(c!=CELL(CAR(p)))
	fprintf(stderr, "->");
      print_points_to_cell(c);
    }
    fprintf(stderr, "\n");
  }
}

/* A type "t" is compatible with a cell "c" if any of the enclosing
 * cell "c'" of "c", including "c", is of type "t".
 *
 * For instance, "a.next" is included in "a". It is compatible with
 * both the type of "a" and the type of "a.next".
 */
bool type_compatible_with_points_to_cell_p(type t, cell c)
{
  cell nc = copy_cell(c);
  reference nr = cell_any_reference(nc);
  //list sl = reference_indices(nr);
  bool compatible_p = false;

  do {
    bool to_be_freed;
    type nct = cell_to_type(nc, &to_be_freed);
    if(concrete_array_pointer_type_equal_p(t, nct)) {
      compatible_p = true;
      if(to_be_freed) free_type(nct);
      break;
    }
    else if(ENDP(reference_indices(nr))) {
      if(to_be_freed) free_type(nct);
      break;
    }
    else {
      if(to_be_freed) free_type(nct);
      /* Remove the last subscript */
      expression l = EXPRESSION(CAR(gen_last(reference_indices(nr))));
      gen_remove(&reference_indices(nr), l);
    }
  } while(true);

  ifdebug(8) {
    bool to_be_freed;
    type ct = cell_to_type(nc, &to_be_freed);
    pips_debug(8, "Cell of type \"%s\" is %s included in cell of type \"%s\"\n",
	       type_to_full_string_definition(t),
	       type_to_full_string_definition(ct),
	       compatible_p? "":"not");
      if(to_be_freed) free_type(ct);
  }

  free_cell(nc);

  return compatible_p;
}

/* See if a super-cell of "c" exists witf type "t". A supercell is a
 * cell "nc" equals to cell "c" but with a shorter subscript list.
 *
 * This function is almost identical to the previous one.
 *
 * A new cell is allocated and returned.
 */
cell type_compatible_super_cell(type t, cell c)
{
  cell nc = copy_cell(c);
  reference nr = cell_any_reference(nc);
  //list sl = reference_indices(nr);
  bool compatible_p = false;

  do {
    bool to_be_freed;
    type nct = cell_to_type(nc, &to_be_freed);
    if(type_equal_p(t, nct)) {
      compatible_p = true;
      if(to_be_freed) free_type(nct);
      break;
    }
    else if(ENDP(reference_indices(nr))) {
      if(to_be_freed) free_type(nct);
      break;
    }
    else {
      if(to_be_freed) free_type(nct);
      /* Remove the last subscript */
      expression l = EXPRESSION(CAR(gen_last(reference_indices(nr))));
      gen_remove(&reference_indices(nr), l);
    }
  } while(true);

  ifdebug(8) {
    bool to_be_freed;
    type ct = cell_to_type(nc, &to_be_freed);
    pips_debug(8, "Cell of type \"%s\" is %s included in cell of type \"%s\"\n",
	       type_to_full_string_definition(t),
	       type_to_full_string_definition(ct),
	       compatible_p? "":"not");
    if(to_be_freed) free_type(ct);
    if(compatible_p) {
      pips_debug(8, "Type compatible cell \"");
      print_points_to_cell(nc);
      fprintf(stderr, "\"\n.");
    }
  }

  return nc;
}


/* Find the "k"-th node of type "t" in list "p". Beware of cycles? No
 * reason since "p" is bounded... The problem must be addressed when
 * "p" is built.
 *
 * An issue with "t": the nodes are references and they carry multiple
 * types, one for each number of subscripts or fields they have. So
 * for instance, s1 and s1.next denote the same location.
 */
cell find_kth_points_to_node_in_points_to_path(list p, type t, int k)
{
  int count = 0;
  cell kc = cell_undefined;
  FOREACH(CELL, c, p) {
    // bool to_be_freed;
    // type ct = cell_to_type(c, &to_be_freed);
    // if(type_equal_p(t, ct)) {
    if(type_compatible_with_points_to_cell_p(t, c)) {
      count++;
      if(count==k) {
	kc = type_compatible_super_cell(t,c);
	break;
      }
    }
  }
  ifdebug(8) {
    pips_debug(8, "Could not find %d nodes of type \"%s\" in path \"", k,
	       type_to_full_string_definition(t));
    print_points_to_path(p);
    fprintf(stderr, "\"\n");
  }
  return kc;
}

bool node_in_points_to_path_p(cell n, list p)
{
  bool in_path_p = false;
  FOREACH(CELL, c, p) {
    if(cell_equal_p(c, n)) {
      in_path_p = true;
      break;
    }
  }
  return in_path_p;
}

/* "p" is a points-to path ending with a cell that points towards a
 * new cell ot type "t". To avoid creating infinite/unbounded path, no
 * more than k nodes of type "t" can be present in path "p". If k are
 * found, a cycle is created to represent longer paths. The
 * corresponding arc is returned. If the creation condition is not
 * met, do not create a new arc.
 */
points_to points_to_path_to_k_limited_points_to_path(list p,
						     int k,
						     type t,
						     bool array_p,
						     pt_map in)
{
  pips_assert("p contains at least one element", !ENDP(p));

  points_to pt = points_to_undefined;
  cell c = CELL(CAR(p));
  list sources = sink_to_sources(c, in, false); // No duplication of cells

  if(ENDP(sources)) {
    /* The current path cannot be made any longer */

    /* Find the k-th node of type "t" if it exists */
    cell kc = find_kth_points_to_node_in_points_to_path(p, t, k);
    if(!cell_undefined_p(kc)) {
      // cell nkc = copy_cell(kc);
      // The above function should return a freshly allocated cell or
      // a cell_undefined
      cell nkc = kc;
      cell source = copy_cell(CELL(CAR(gen_last(p))));
      pt = make_points_to(source, nkc, make_approximation_may(),
			  make_descriptor_none());
    }
  }
  else {
    FOREACH(CELL, source, sources) {
      /* Skip sources that are already in the path "p" so as to avoid
	 infinite path due to cycles in points-to graph "in". */
      if(node_in_points_to_path_p(source, p)) {
	; // Could be useful for debugging
      }
      else {
	list np = CONS(CELL, source, p); // make the path longer
	// And recurse
	pt = points_to_path_to_k_limited_points_to_path(np, k, t, array_p, in);
	// And restore p
	CDR(np) = NIL;
	gen_free_list(np);
	if(!points_to_undefined_p(pt)) // Stop as soon as an arc has been created; FI->AM/FC: may not be correct...
	  break;
      }
    }
  }
  return pt;
}


/* Create a new node "sink" of type "t" and a new arc "pt" starting
 * from node "source", if no path starting from any node and ending in
 * "source", built with arcs in the points-to set "in", contains more
 * than k nodes of type "t" (the type of the sink). If k nodes of type
 * "t" are already in the path, create a new arc "pt" between the
 * "source" and the k-th node in the path.
 *
 * Parameter "array_p" indicates if the source is an array or a
 * scalar. Different models can be chosen. For instance, Beatrice
 * Creusillet wants to have an array as target and obtain something
 * like argv[*]->_argv_1[*] although argv[*]->_argv-1 might also be a
 * correct model if _argv_1 is an abstract location representing lots
 * of different physical locations.
 *
 * Parameter k is defined by a property.
 *
 * FI: not to clear about what is going to happen when "source" is the
 * final node of several paths.
 *
 * Also, beware of circular paths.
 *
 * Efficiency is not yet a goal...
 */
points_to create_k_limited_stub_points_to(cell source, type t, bool array_p, pt_map in)
{
  int k = get_int_property("POINTS_TO_PATH_LIMIT");
  pips_assert("k is greater than one", k>=1);
  points_to pt = points_to_undefined;
  list p = CONS(CELL, source, NIL); // points-to path...

  // FI: not to sure about he possible memory leaks...
  pt = points_to_path_to_k_limited_points_to_path(p, k, t, array_p, in);

  /* No cycle could be created, the paths can safely be made longer. */
  if(points_to_undefined_p(pt))
    // FI: I do not know how to use the third argument...
    pt = create_stub_points_to(source, t, basic_undefined);

  gen_free_list(p);

  return pt;
}

/* Build a list of possible cell sources for cell "sink" in points-to
 * graph "pts". If fresh_p is set, allocate new cells, if not just
 * build the spine of the list.
 */
list sink_to_sources(cell sink, set pts, bool fresh_p)
{
  list sources = NIL;

  // FI: This is a short-term short cut
  // The & operator can be applied to anything
  // We should start with all subscripts and then get rid of subscript
  // one by one and each time add a new source

  /* Get rid of the constant subscripts since they are not direclty
     part of the points-to scheme on the sink side */
  entity v = reference_variable(cell_any_reference(sink));
  reference nr = make_reference(v, NIL);
  cell nsink = make_cell_reference(nr);

  /* 1. Try to find the source in the points-to information */
  SET_FOREACH(points_to, pt, pts) {
    // FI: a more flexible test is needed as the sink cell may be
    // either a, or a[0] or a[*] or a[*][*] or...
    if(cell_equal_p(nsink, points_to_sink(pt))) {
      cell sc = fresh_p? copy_cell(points_to_source(pt))
	: points_to_source(pt);
      sources = CONS(CELL, sc, sources);
    }
  }
  free_cell(nsink);
  return sources;
}

list stub_source_to_sinks(cell source, pt_map pts)
{
  list sinks = NIL;
  reference r = cell_any_reference(source);
  list sl = reference_indices(r);
  if(ENDP(sl))
    sinks = scalar_stub_source_to_sinks(source, pts);
  else
    sinks = array_stub_source_to_sinks(source, pts);
  return sinks;
}

list scalar_stub_source_to_sinks(cell source, pt_map pts)
{
  list sinks = generic_stub_source_to_sinks(source, pts, false);
  return sinks;
}

list array_stub_source_to_sinks(cell source, pt_map pts)
{
  list sinks = generic_stub_source_to_sinks(source, pts, true);
  return sinks;
}

list generic_stub_source_to_sinks(cell source, pt_map pts, bool array_p)
{
  list sinks = NIL;
  bool null_initialization_p =
    get_bool_property("POINTS_TO_NULL_POINTER_INITIALIZATION");
  //type ost = ultimate_type(entity_type(v));
  bool to_be_freed; // FI: memory leak for the time being
  type rt = cell_to_type(source, &to_be_freed); // reference type
  if(pointer_type_p(rt)) {
    // bool array_p = array_type_p(rt); FI: always false if pointer_type_P(rt)
    type nst = type_to_pointed_type(rt);
    points_to pt = create_k_limited_stub_points_to(source, nst, array_p, pts);
    pts = add_arc_to_pt_map(pt, pts);
    add_arc_to_points_to_context(copy_points_to(pt));
    sinks = source_to_sinks(source, pts, false);
    if(null_initialization_p) {
      // FI: I'm lost here... both with the meaning of null
      // initialization_p and with the insertion of a
      // corresponding arc in "pts"
      list ls = null_to_sinks(source, pts);
      sinks = gen_nconc(ls, sinks);	 
    }
  }
  else if(struct_type_p(rt)) {
    // FI FI FI - to be really programmed with the field type
    // FI->AM: I am really confused about what I am doing here...
    variable vrt = type_variable(rt);
    basic b = variable_basic(vrt);
    entity se = basic_derived(b);
    type st = entity_type(se);
    pips_assert("se is an internal struct", type_struct_p(st));
    list fl = type_struct(st);
    FOREACH(ENTITY, f, fl) {
      type ft = entity_type(f);
      type uft = ultimate_type(ft);
      bool array_p = array_type_p(ft); // not consistent with ultimate_type()
      points_to pt = create_k_limited_stub_points_to(source, uft, array_p, pts);
      pts = add_arc_to_pt_map(pt, pts);
      add_arc_to_points_to_context(copy_points_to(pt));
      sinks = source_to_sinks(source, pts, false);
    }
  }
  else if(array_type_p(rt)) {
    reference r = cell_any_reference(source);
    entity v = reference_variable(r);
    printf("Entity \"%s\"\n", entity_local_name(v));
    pips_internal_error("Not implemented yet.\n");
  }
  return sinks;
}


/* Return a list of cells, "sinks", that are sink for some arc whose
 * source is "source" in set "s". If "fresh_p" is set to true, no
 * sharing is created between list "sinks" and reference "source" or
 * points-to set "s". Else, the cells in list "sinks" are the cells in
 * arcs of the points-to set.
 *
 * Additional functionality: add new arcs in s when global, formal or
 * virtual variables are reached.
 *
 * Function added by FI.
 */
list source_to_sinks(cell source, pt_map pts, bool fresh_p)
{
  list sinks = NIL;
  // AM: Get the property POINTS_TO_NULL_POINTER_INITIALIZATION
  bool null_initialization_p =
    get_bool_property("POINTS_TO_NULL_POINTER_INITIALIZATION");

  /* Can we expect a sink? */
  if(nowhere_cell_p(source)) {
    bool uninitialized_dereferencing_p =
      get_bool_property("POINTS_TO_UNINITIALIZED_POINTER_DEREFERENCING");
    if(uninitialized_dereferencing_p) {
      reference r = cell_any_reference(source);
      entity v = reference_variable(r);
      pips_user_warning("Possibly undefined pointer \"%s\" is dereferenced.\n",
			entity_local_name(v));
      sinks = anywhere_to_sinks(source, pts);
    }
  }
  else if(anywhere_cell_p(source) || cell_typed_anywhere_locations_p(source)) {
    /* FI: we should return an anywhere cell with the proper type */
    /* FI: should we add the corresponding arcs in pts? */
    /* FI: should we take care of typed anywhere as well? */
    sinks = anywhere_to_sinks(source, pts);
  }
  // FI: the null pointer should also be checked here!
  else if(null_pointer_value_cell_p(source)) {
    bool null_dereferencing_p =
      get_bool_property("POINTS_TO_NULL_POINTER_DEREFERENCING");
    if(null_dereferencing_p) {
      reference r = cell_any_reference(source);
      entity v = reference_variable(r);
      pips_user_warning("Possibly undefined pointer \"%s\" is dereferenced.\n",
			entity_local_name(v));
      sinks = anywhere_to_sinks(source, pts);
    }
  }
  else {
    /* 0. Is the source a pointer? You would expect a yes, but C
       pointer arithmetics requires some strange typing. We assume it
       is an array of pointers. */
    bool to_be_freed;
    type ct = points_to_cell_to_type(source, &to_be_freed);
    if(array_type_p(ct)) {
      basic ctb = variable_basic(type_variable(ct));
      // FI->AM: I am not happy at all with his
      if(basic_pointer_p(ctb))
	points_to_cell_add_unbounded_subscripts(source);
      else {
	// Pointers/dependence11.c: an array is a kind of constant pointer
	// No need to eval more
	cell sc = copy_cell(source);
	sinks = CONS(CELL, sc, sinks);
      }
    }
    if(to_be_freed) free_type(ct);

    /* 1. Try to find the source in the points-to information */
    if(ENDP(sinks)) {
      SET_FOREACH( points_to, pt, pts) {
	if(cell_equal_p(source, points_to_source(pt))) {
	  cell sc = fresh_p? copy_cell(points_to_sink(pt)) : points_to_sink(pt);
	  sinks = CONS(CELL, sc, sinks);
	}
      }
    }

    /* 2. Much harder... See if source is contained in one of the many
       abstract sources. Step 1 is subsumed by Step 2... but much faster.  */
    if(ENDP(sinks)) {
      SET_FOREACH(points_to, pt, pts) {
	if(cell_included_p(source, points_to_source(pt))) {
	  cell sc = fresh_p? copy_cell(points_to_sink(pt)) : points_to_sink(pt);
	  sinks = CONS(CELL, sc, sinks);
	}
      }
    }

    /* The source may be an array field */
    /* FI: I think this is all wrong */
    if(false && ENDP(sinks)) {
      bool to_be_freed = false;
      type st = points_to_cell_to_type(source, &to_be_freed);
      if(array_type_p(st)) {
	// FI: I'm not too sure I cannot copy the reference because it
	// has to ne modified
	reference nr = copy_reference(cell_any_reference(source));
	expression zero = int_to_expression(0);
	reference_indices(nr) = gen_nconc(reference_indices(nr),
					  CONS(EXPRESSION,zero, NIL));
	cell nc = make_cell_reference(nr);
	sinks = CONS(CELL, nc, NIL);
      }
      if(to_be_freed)
	free_type(st);
    }

    /* 3. If the previous steps have failed, build a new sink if the
       source is a formal parameter, a global variable, a C file local
       global variable (static) or a stub or an anywhere abstract location. */
    if(ENDP(sinks)) {
      reference r = cell_any_reference(source);
      entity v = reference_variable(r);
      if(formal_parameter_p(v)) {
	// Find stub type
	type st = type_to_pointed_type(ultimate_type(entity_type(v)));
	// FI: the type retrieval must be improved for arrays & Co
	// FI: This is not going to work with typedefs...
	bool array_p = array_type_p(entity_type(v));
	points_to pt = create_k_limited_stub_points_to(source, st, array_p, pts);
	pts = add_arc_to_pt_map(pt, pts);
	add_arc_to_points_to_context(copy_points_to(pt));
	sinks = source_to_sinks(source, pts, false);
	if(null_initialization_p){
	  list ls = null_to_sinks(source, pts);
	  sinks = gen_nconc(ls, sinks);
	}
      }
      else if(top_level_entity_p(v) || static_global_variable_p(v)) {
	type st = type_to_pointed_type(ultimate_type(entity_type(v)));
	// FI: the type retrieval must be improved for arrays & Co
	//points_to pt = create_stub_points_to(source, st, basic_undefined);
	points_to pt = points_to_undefined;
	if(const_variable_p(v)) {
	  expression init = variable_initial_expression(v);
	  sinks = expression_to_points_to_sinks(init, pts);
	  free_expression(init);
	  /* Add these new arcs to the context */
	  bool exact_p = gen_length(sinks)==1;
	  FOREACH(CELL, sink, sinks) {
	    cell nsource = copy_cell(source);
	    cell nsink = copy_cell(sink);
	    approximation na = exact_p? make_approximation_exact():
	      make_approximation_may();
	    points_to npt = make_points_to(nsource, nsink, na,
					   make_descriptor_none());
	    pts = add_arc_to_pt_map(npt, pts);
	    add_arc_to_points_to_context(copy_points_to(npt));
	  }
	}
	else {
	  /* Do we have to ignore an initialization? */
	  value val = entity_initial(v);
	  if(!value_unknown_p(val))
	    pips_user_warning("Initialization of global variable \"%s\" is ignored "
			      "because the \"const\" qualifier is not used.\n",

			      entity_user_name(v));
	  bool array_p = array_type_p(st);
	  pt = create_k_limited_stub_points_to(source, st, array_p, pts);

	  pts = add_arc_to_pt_map(pt, pts);
	  add_arc_to_points_to_context(copy_points_to(pt));
	  sinks = source_to_sinks(source, pts, false);
	  if(null_initialization_p){
	    list ls = null_to_sinks(source, pts);
	    sinks = gen_nconc(ls, sinks);
	  }
	}
      }
      else if(entity_stub_sink_p(v)) {
	sinks = stub_source_to_sinks(source, pts);
      }
      else if(entity_typed_anywhere_locations_p(v)) {
	pips_internal_error("This case should have been handled above.\n");
	type t = entity_type(v);
	type ut = ultimate_type(t);
	if(pointer_type_p(ut)) {
	  type upt = ultimate_type(type_to_pointed_type(t));
	  cell c = make_anywhere_points_to_cell(upt);
	  sinks = CONS(CELL, c, NIL);
	  points_to pt = make_points_to(copy_cell(source), c,
					make_approximation_may(),
					make_descriptor_none());
	  pts = add_arc_to_pt_map(pt, pts);
	}
	else {
	  pips_internal_error("Should be a pointer type.\n");
	}
      }
      if(ENDP(sinks)) {
	/* We must be analyzing dead code... */
	reference r = cell_any_reference(source);
	print_reference(r);
	pips_user_warning("\nUninitialized or null pointer dereferenced: "
			  "Sink missing for a source based on \"%s\".\n"
			  "Update points-to property POINTS_TO_UNINITIALIZED_POINTER_DEREFERENCING and/or POINTS_TO_UNINITIALIZED_NULL_DEREFERENCING according to needs.\n",
			  entity_user_name(v));
      }
    }
  }
  // FI: use gen_nreverse() to simplify debbugging? Not meaningful
  // with SET_FOREACH
  return sinks;
}

/* Return all cells in points-to set "pts" who source is based on entity "e". 
 *
 * Similar to source_to_sinks, but much easier and shorter.
 */
list variable_to_sinks(entity e, pt_map pts, bool fresh_p)
{
  list sinks = NIL;
  SET_FOREACH( points_to, pt, pts) {
    cell source = points_to_source(pt);
    reference sr = cell_any_reference(source);
    entity v = reference_variable(sr);
    if(e==v) {
      cell sc = fresh_p? copy_cell(points_to_sink(pt)) : points_to_sink(pt);
      sinks = CONS(CELL, sc, sinks);
    }
  }
  return sinks;

}

/* Create a list of null sinks and add a new null points-to relation to pts.
   pts is modified by side effect.
*/
list null_to_sinks(cell source, set pts)
{
  cell nsource = copy_cell(source);
  cell nsink = make_null_pointer_value_cell();
  points_to npt = make_points_to(nsource, nsink,
				 make_approximation_may(),
				 make_descriptor_none());
  pts = add_arc_to_pt_map(npt, pts);
  add_arc_to_points_to_context(copy_points_to(npt));
  list sinks = CONS(CELL, copy_cell(nsink), NIL);
  return sinks;
}

/* Same as source_to_sinks, but for a list of cells. */
list sources_to_sinks(list sources, set pts, bool fresh_p)
{
  list sinks = NIL;
  FOREACH(CELL, c, sources) {
    list cl =  source_to_sinks(c, pts, fresh_p);
    sinks = gen_nconc(sinks, cl);
  }
  return sinks;
}

list reference_to_sinks(reference r, pt_map in, bool fresh_p)
{
  cell source = make_cell_reference(copy_reference(r));
  list sinks = source_to_sinks(source, in, fresh_p);
  free_cell(source);
  return sinks;
}

/* Merge two points-to sets
 *
 * This function is required to compute the points-to set resulting of
 * an if control statements.
 *
 * A new set is allocated but it reuses the elements of "s1" and "s2".
 */
set merge_points_to_set(set s1, set s2) {
  set Merge_set = set_generic_make(set_private, points_to_equal_p,
				   points_to_rank);
  if(set_empty_p(s1))
    set_assign(Merge_set, s2);
  else if(set_empty_p(s2)) 
    set_assign(Merge_set, s1);
  else {
    set Definite_set = set_generic_make(set_private, points_to_equal_p,
					points_to_rank);
    set Possible_set = set_generic_make(set_private, points_to_equal_p,
					points_to_rank);
    set Intersection_set = set_generic_make(set_private, points_to_equal_p,
					    points_to_rank);
    set Union_set = set_generic_make(set_private, points_to_equal_p,
				     points_to_rank);

    Intersection_set = set_intersection(Intersection_set, s1, s2);
    Union_set = set_union(Union_set, s1, s2);

    SET_FOREACH ( points_to, i, Intersection_set ) {
      if ( approximation_exact_p(points_to_approximation(i)) 
	   || approximation_must_p(points_to_approximation(i)) )
	Definite_set = set_add_element(Definite_set,Definite_set,
				       (void*) i );
    }

    SET_FOREACH ( points_to, j, Union_set ) {
      if ( ! set_belong_p(Definite_set, (void*)j) ) {
	points_to pt = make_points_to(points_to_source(j), points_to_sink(j),
				      make_approximation_may(),
				      make_descriptor_none());
	Possible_set = set_add_element(Possible_set, Possible_set,(void*) pt);
      }
    }

    Merge_set = set_clear(Merge_set);
    Merge_set = set_union(Merge_set, Possible_set, Definite_set);

    set_clear(Intersection_set);
    set_clear(Union_set);
    set_clear(Possible_set);
    set_clear(Definite_set);
    set_free(Definite_set);
    set_free(Possible_set);
    set_free(Intersection_set);
    set_free(Union_set);
  }
  return Merge_set;
}

/* Change the all the exact points-to relations to may relations */
set exact_to_may_points_to_set(set s)
{
  SET_FOREACH ( points_to, pt, s ) {
    if(approximation_exact_p(points_to_approximation(pt)))
      points_to_approximation(pt) = make_approximation_may();
  }
  return s;
}


bool cell_in_list_p(cell c, const list lx)
{
  list l = (list) lx;
  for (; !ENDP(l); POP(l))
    if (points_to_compare_cell(CELL(CAR(l)), c)) return true; /* found! */

  return false; /* else no found */
}

bool points_to_in_list_p(points_to pt, const list lx)
{
  list l = (list) lx;
  for (; !ENDP(l); POP(l))
    if (points_to_equal_p(POINTS_TO(CAR(l)), pt)) return true; /* found! */

  return false; /* else no found */
}

/*  */
bool points_to_compare_cell(cell c1, cell c2)
{
  if(c1==c2)
    return true;

  int i = 0;
  reference r1 = cell_any_reference(c1);
  reference r2 = cell_any_reference(c2);
  entity v1 = reference_variable(r1);
  entity v2 = reference_variable(r2);
  list sl1 = NIL, sl2 = NIL;
  extern const char* entity_minimal_user_name(entity);

  i = strcmp(entity_minimal_user_name(v1), entity_minimal_user_name(v2));
  if(i==0) {
    sl1 = reference_indices(r1);
    sl2 = reference_indices(r2);
    int i1 = gen_length(sl1);
    int i2 = gen_length(sl2);

    i = i2>i1? 1 : (i2<i1? -1 : 0);

    for(;i==0 && !ENDP(sl1); POP(sl1), POP(sl2)){
      expression se1 = EXPRESSION(CAR(sl1));
      expression se2 = EXPRESSION(CAR(sl2));
      if(expression_constant_p(se1) && expression_constant_p(se2)){
	int i1 = expression_to_int(se1);
	int i2 = expression_to_int(se2);
	i = i2>i1? 1 : (i2<i1? -1 : 0);
      }else{
	string s1 = words_to_string(words_expression(se1, NIL));
	string s2 = words_to_string(words_expression(se2, NIL));
	i = strcmp(s1, s2);
      }
    }
  }

  return (i== 0 ? true: false) ;
}

/* Order the two points-to relations according to the alphabetical
 * order of the underlying variables. Return -1, 0, or 1.
 */
int points_to_compare_location(void * vpt1, void * vpt2) {
  int i = 0;
  points_to pt1 = *((points_to *) vpt1);
  points_to pt2 = *((points_to *) vpt2);

  cell c1so = points_to_source(pt1);
  cell c2so = points_to_source(pt2);
  cell c1si = points_to_sink(pt1);
  cell c2si = points_to_sink(pt2);

  // FI: bypass of GAP case
  reference r1so = cell_to_reference(c1so);
  reference r2so = cell_to_reference(c2so);
  reference r1si = cell_to_reference(c1si);
  reference r2si = cell_to_reference(c2si);

  entity v1so = reference_variable(r1so);
  entity v2so = reference_variable(r2so);
  entity v1si = reference_variable(r1si);
  entity v2si = reference_variable(r2si);
  list sl1 = NIL, sl2 = NIL;
  // FI: memory leak? generation of a new string?
  extern const char* entity_minimal_user_name(entity);

  i = strcmp(entity_minimal_user_name(v1so), entity_minimal_user_name(v2so));
  if(i==0) {
    i = strcmp(entity_minimal_user_name(v1si), entity_minimal_user_name(v2si));
    if(i==0) {
      sl1 = reference_indices(r1so);
      sl2 = reference_indices(r2so);
      int i1 = gen_length(sl1);
      int i2 = gen_length(sl2);

      i = i2>i1? 1 : (i2<i1? -1 : 0);

      if(i==0) {
	list sli1 = reference_indices(r1si);
	list sli2 = reference_indices(r2si);
	int i1 = gen_length(sli1);
	int i2 = gen_length(sli2);

	i = i2>i1? 1 : (i2<i1? -1 : 0);
	if(i==0) {
	  for(;i==0 && !ENDP(sl1); POP(sl1), POP(sl2)){
	    expression se1 = EXPRESSION(CAR(sl1));
	    expression se2 = EXPRESSION(CAR(sl2));
	    if(expression_constant_p(se1) && expression_constant_p(se2)){
	      int i1 = expression_to_int(se1);
	      int i2 = expression_to_int(se2);
	      i = i2>i1? 1 : (i2<i1? -1 : 0);
	      if(i==0){
		for(;i==0 && !ENDP(sli1); POP(sli1), POP(sli2)){
		  expression sei1 = EXPRESSION(CAR(sli1));
		  expression sei2 = EXPRESSION(CAR(sli2));
		  if(expression_constant_p(sei1) && expression_constant_p(sei2)){
		    int i1 = expression_to_int(sei1);
		    int i2 = expression_to_int(sei2);
		    i = i2>i1? 1 : (i2<i1? -1 : 0);
		  }else{
		    string s1 = words_to_string(words_expression(se1, NIL));
		    string s2 = words_to_string(words_expression(se2, NIL));
		    i = strcmp(s1, s2);
		  }
		}
	      }
	    }else{
	      string s1 = words_to_string(words_expression(se1, NIL));
	      string s2 = words_to_string(words_expression(se2, NIL));
	      i = strcmp(s1, s2);
	    }
	  }
	}
      }
    }
  }
  return i;
}

/* make sure that set "s" does not contain redundant or contradictory
   elements */
bool consistent_points_to_set(set s)
{
  bool consistent_p = true;

  SET_FOREACH(points_to, a, s) {
    consistent_p = consistent_p && points_to_consistent_p(a);
  }

  SET_FOREACH(points_to, pt1, s) {
    SET_FOREACH(points_to, pt2, s) {
      if(pt1!=pt2) {
	//same source
	cell c1 = points_to_source(pt1);
	cell c2 = points_to_source(pt2);
	bool cmp1 = locations_equal_p(c1,c2);

	// same sink
	cell c3 = points_to_sink(pt1);
	cell c4 = points_to_sink(pt2);
	bool cmp2 = locations_equal_p(c3,c4);
	if(cmp1&&cmp2) {
	  // same approximation
	  approximation a1 = points_to_approximation(pt1);
	  approximation a2 = points_to_approximation(pt2);
	  // FI: must is forgotten...
	  //bool cmp3 = (approximation_exact_p(a1) && approximation_exact_p(a2))
	  //  || ( approximation_may_p(a1) && approximation_may_p(a2));
	  // FI: should we identify "exact" and "must"?
	  bool cmp3 = (approximation_tag(a1)==approximation_tag(a2));
	  if(cmp3) {
	    fprintf(stderr, "Redundant points-to arc:\n");
	    print_points_to(pt1);
	    consistent_p = false;
	  }
	  else {
	    fprintf(stderr, "Contradictory points-to arcs: incompatible approximation\n");
	    print_points_to(pt1);
	    print_points_to(pt2);
	    consistent_p = false;
	  }
	}
      }
    }
  }

  /* Make sure that the element of set "s" belong to "s" (issue with
   * side effects performed on subscript expressions).
   */
  SET_FOREACH(points_to, pt, s) {
    if(!set_belong_p(s,pt)) {
      fprintf(stderr, "Points-to %p ", pt);
      print_points_to(pt);
      fprintf(stderr, " is in set s but does not belong to it!\n");
      consistent_p = false;
    }
  }

  return consistent_p;
}

/* because of points-to set implementation, you cannot change
 * approximations by side effects.
 */
void upgrade_approximations_in_points_to_set(pt_map pts)
{
  SET_FOREACH(points_to, pt, pts) {
    approximation a = points_to_approximation(pt);
    if(!approximation_exact_p(a)) {
      cell source = points_to_source(pt);
      if(!cell_abstract_location_p(source) // Represents may locations
	 && !stub_points_to_cell_p(source)) { // May not exist...
	list sinks = source_to_sinks(source, pts, false);
	if(gen_length(sinks)==1) {
	  cell sink = points_to_sink(pt);
	  if(!cell_abstract_location_p(sink)) {
	    points_to npt = make_points_to(copy_cell(source),
					   copy_cell(sink),
					   make_approximation_exact(),
					   make_descriptor_none());
	    remove_arc_from_pt_map(pt, pts);
	    add_arc_to_pt_map(npt, pts);
	  }
	}
	gen_free_list(sinks);
      }
    }
  }
}

void remove_points_to_arcs(cell source, cell sink, pt_map pt)
{
  points_to a = make_points_to(copy_cell(source), copy_cell(sink),
			       make_approximation_may(),
			       make_descriptor_none());
  remove_arc_from_pt_map(a, pt);
  free_points_to(a);

  a = make_points_to(copy_cell(source), copy_cell(sink),
			       make_approximation_exact(),
			       make_descriptor_none());
  remove_arc_from_pt_map(a, pt);
  free_points_to(a);
}


bool points_to_cell_equal_p(cell c1, cell c2)
{
  reference r1 = cell_any_reference(c1);
  reference r2 = cell_any_reference(c2);
  return reference_equal_p(r1,r2);
}

/*

  $Id: prettyprint.c 14802 2009-08-12 12:27:53Z mensi $

  Copyright 1989-2009 MINES ParisTech

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
/*
 * (prettyg)print of POINTS TO.
 *
 * AM, August 2009.
 */
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "genC.h"

#include "text.h"
#include "text-util.h"

#include "top-level.h"

#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"


#include "database.h"
#include "pipsdbm.h"
#include "resources.h"

#include "misc.h"
#include "properties.h"

#include "prettyprint.h"
#include "semantics.h"
#include "effects-generic.h"
#include "effects-simple.h"


#include "alias-classes.h"




#define PT_TO_SUFFIX ".points_to"
#define PT_TO_DECO "points to = "
#define SUMMARY_PT_TO_SUFFIX ".summary_points_to"
#define SUMMARY_PT_TO_DECO "summary points to = "

/****************************************************** STATIC INFORMATION */
GENERIC_GLOBAL_FUNCTION(printed_points_to_list, statement_points_to)

/************************************************************* BASIC WORDS */
/*Already exist in cprettyprint but in mode static. To be removed later.*/
static bool variable_p(entity e)
{
	return type_undefined_p(entity_type(e));
}

/* To modelize the heap locations we manufacture fictious reference,
 * that triggered a bug when it appears as an argument of entity_user_name(). */
list
words_fictious_reference(reference obj)
{
  list pc = NIL;
  entity e = reference_variable(obj);
  pc = CHAIN_SWORD(pc, entity_name(e));
  return(pc);
}

#if 0
/* For debugging points-to: use this function in points_to_words_reference() */
static string entity_full_name(entity e)
{
  return entity_name(e);
}
#endif

/* Specific handling of references appearing in points_to */
list points_to_words_reference(reference r)
{
  extern const char* entity_minimal_user_name(entity);

  // Normal implementation, used for validation:
  return words_any_reference(r,NIL, entity_minimal_user_name);
  // To ease debugging, use:
  //return words_any_reference(r,NIL, entity_full_name);
}

list word_points_to(points_to pt)
{
  list l2 = NIL, l1 = NIL, rlt1 = NIL;

  pips_assert("pt is defined", !points_to_undefined_p(pt));
  points_to_consistent_p(pt);
  cell c1 = points_to_source(pt);
  cell c2 = points_to_sink(pt);
  // FI->AM: check all your copy_xxxx(); you often copy a large object
  // to obtain a small part of it
  reference r1 = cell_to_reference(c1);
  reference r2 = cell_to_reference(c2);
  approximation rel = points_to_approximation(pt);
  string l3 = "-MAY-";

  if (approximation_exact_p(rel))
    l3 = "-Exact-";
  if(variable_p(reference_variable(r2)))
    l2 = words_fictious_reference(r2);
  else
    l2 = points_to_words_reference(r2);

  l1 = points_to_words_reference(r1);

  rlt1 = gen_nconc((CONS(STRING,strdup("("), NIL)),l1);
  rlt1 = gen_nconc(rlt1,(CONS(STRING,strdup(","), NIL)));

  rlt1 = gen_nconc(rlt1, l2);
  rlt1 = gen_nconc(rlt1,(CONS(STRING,strdup(","), NIL)));
  rlt1 = gen_nconc(rlt1,(CONS(STRING,strdup(l3), NIL)));
  rlt1 = gen_nconc(rlt1,(CONS(STRING,strdup(")"), NIL)));
  return rlt1;
}

/* must return -1, 0 or 1. Should avoid 0 because we want a total
   order to avoid validation problems. */
int points_to_compare_cells(const void * vpt1, const void * vpt2)
{
  int i = 0;

  points_to pt1 = *((points_to *) vpt1);
  points_to pt2 = *((points_to *) vpt2);

  cell c1so = points_to_source(pt1);
  cell c2so = points_to_source(pt2);
  cell c1si = points_to_sink(pt1);
  cell c2si = points_to_sink(pt2);

  //cell c1 = CELL(CAR(vc1));
  //cell c2 = CELL(CAR(vc2));
  // FI: bypass of GAP case
  reference r1so = cell_to_reference(c1so);
  reference r2so = cell_to_reference(c2so);
  reference r1si = cell_to_reference(c1si);
  reference r2si = cell_to_reference(c2si);

  entity v1so = reference_variable(r1so);
  entity v2so = reference_variable(r2so);
  entity v1si = reference_variable(r1si);
  entity v2si = reference_variable(r2si);

  // FI: memory leak? generation of a new string?
  extern const char* entity_minimal_user_name(entity);

  i = strcmp(entity_minimal_user_name(v1so), entity_minimal_user_name(v2so));
  if(i==0) {
    i = strcmp(entity_minimal_user_name(v1si), entity_minimal_user_name(v2si));
    if(i==0) {
      list sl1 = reference_indices(r1so);
      list sl2 = reference_indices(r2so);
      int i1 = gen_length(sl1);
      int i2 = gen_length(sl2);

      i = i2>i1? 1 : (i2<i1? -1 : 0);
      if(i==0) {
	list sl1 = reference_indices(r1si);
	list sl2 = reference_indices(r2si);
	int i1 = gen_length(sl1);
	int i2 = gen_length(sl2);

	i = i2>i1? 1 : (i2<i1? -1 : 0);
	if(i==0) {
	  pips_internal_error("Further reference comparison not implemented...");
	}
      }
    }
  }

  return i;
}

/* Allocate a copy of ptl and sort it. It might be better to admit a
   side effect on ptl and to let the caller copy the liste before
   sorting. */
list points_to_list_sort(list ptl)
{
  list sptl = gen_full_copy_list(ptl);

  gen_sort_list(sptl, /* (gen_cmp_func_t) */ points_to_compare_cells);

  return sptl;
}

//extern void print_points_to(
/* Make sure that points-to are fully ordered before prettyprinting
   them or validation will be in trouble sooner or later. The sort
   could occur before storing the points-to information into the hash
   table or just before prettypriting it. */
list words_points_to_list(string note, points_to_list s)
{
  list l = NIL;
  int i = 0;
  list ptl = points_to_list_list(s);
  list sptl = points_to_list_sort(ptl);

  FOREACH(POINTS_TO, j, sptl) {
    if(i>0)
      l = gen_nconc(l, (CONS(STRING,strdup(";"), NIL)));
    else
      i++;
    l = gen_nconc(l,word_points_to(j));
  }
  l = CONS(STRING,strdup("{"), l);
  l = gen_nconc(l,(CONS(STRING,strdup("}"), NIL)));

  gen_full_free_list(sptl);

  return l;
}

text text_points_to(entity module,int margin, statement s)
{

  text t;
  t = bound_printed_points_to_list_p(s)?
    words_predicate_to_commentary
    (words_points_to_list(PT_TO_DECO,
			  load_printed_points_to_list(s)),
			      get_comment_sentinel())
    :words_predicate_to_commentary
    (CONS(STRING,PT_TO_DECO, CONS(STRING, strdup("{}"), NIL)),
        get_comment_sentinel());
  return t;
}


 text text_code_points_to(statement s)
{
  text t;
  debug_on("PRETTYPRINT_DEBUG_LEVEL");
  init_prettyprint(text_points_to);
  t = text_module(get_current_module_entity(), s);
  close_prettyprint();
  debug_off();
  return t;
}

bool print_code_points_to(string module_name,
		      string resource_name,
		      string file_suffix)
{
  
  list wl = list_undefined;
  text t, st;
  bool res;
  debug_on("POINTS_TO_DEBUG_LEVEL");
  //init_printed_points_to_list();
  set_current_module_entity(local_name_to_top_level_entity(module_name));
  points_to_list summary_pts_to = (points_to_list)
    db_get_memory_resource(DBR_SUMMARY_POINTS_TO_LIST, module_name, TRUE);
  wl = words_points_to_list(SUMMARY_PT_TO_SUFFIX, summary_pts_to);
  pips_debug(1, "considering module %s \n",
	     module_name);

  /*  FI: just for debugging */
  // check_abstract_locations();

  //init_printed_points_to_list();
  set_printed_points_to_list((statement_points_to)
			     db_get_memory_resource(DBR_POINTS_TO_LIST, module_name, TRUE));
  // statement_points_to_consistent_p(get_printed_points_to_list());
  statement_points_to_consistent_p(get_printed_points_to_list());
  set_current_module_statement((statement)
			       db_get_memory_resource(DBR_CODE,
						      module_name,
						      TRUE));
  // FI: should be language neutral...
  st = words_predicate_to_commentary(wl, get_comment_sentinel());
  t = text_code_points_to(get_current_module_statement());
  //print_text(stderr,t);
  //st = text_code_summary_points_to(get_current_module_statement());
  MERGE_TEXTS(st, t);
  //print_text(stderr,t);
  res= make_text_resource_and_free(module_name,DBR_PRINTED_FILE,file_suffix, st);
  reset_current_module_entity();
  reset_current_module_statement();
  reset_printed_points_to_list();
  
 
  

  //reset_printed_points_to_list();
  
 
  //free(t);
  debug_off();
  return TRUE;
}

//Handlers for PIPSMAKE
bool print_code_points_to_list(string module_name)
{
	return print_code_points_to(module_name,
				    DBR_POINTS_TO_LIST,
				    PT_TO_SUFFIX);
}

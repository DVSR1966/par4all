/*

  $Id$

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
#include <stdio.h>

#include "linear.h"

#include "genC.h"
#include "ri.h"

#include "misc.h"
#include "ri-util.h"

extern int Nbrdo;

DEFINE_CURRENT_MAPPING(enclosing_loops, list)

void clean_enclosing_loops(void)
{
    /* warning: there are shared lists...
     */
    hash_table seen = hash_table_make(hash_pointer, 0);
    
    STATEMENT_MAPPING_MAP(s, l, 
    {
	if (l && !hash_defined_p(seen, l))
	{
	    gen_free_list((list)l);
	    hash_put(seen, l, (char*) 1);
	}
    },
	get_enclosing_loops_map());

    hash_table_free(seen);
    free_enclosing_loops_map();
}

static void rloops_mapping_of_statement();

static void 
rloops_mapping_of_unstructured(
    statement_mapping m,
    list loops,
    unstructured u)
{
    list blocs = NIL ;
	  
    CONTROL_MAP(c, rloops_mapping_of_statement(m, loops, control_statement(c)),
		unstructured_control(u), blocs) ;

    gen_free_list(blocs) ;
}

static void 
rloops_mapping_of_statement(
    statement_mapping m,
    list loops,
    statement s)
{
    instruction i = statement_instruction(s);

    SET_STATEMENT_MAPPING(m, s, gen_copy_seq(loops));

    switch(instruction_tag(i)) {

      case is_instruction_block:
	MAP(STATEMENT, s, rloops_mapping_of_statement(m, loops, s),
	    instruction_block(i));
	break;

      case is_instruction_loop: 
      {
	  list nl = gen_nconc(gen_copy_seq(loops), CONS(STATEMENT, s, NIL));
	  Nbrdo++;
	  rloops_mapping_of_statement(m, nl, loop_body(instruction_loop(i)));
	  gen_free_list(nl);
	  break;
      }

      case is_instruction_test:
	rloops_mapping_of_statement(m, loops, test_true(instruction_test(i)));
	rloops_mapping_of_statement(m, loops, test_false(instruction_test(i)));
	break;

      case is_instruction_whileloop:
	rloops_mapping_of_statement(m, loops, whileloop_body(instruction_whileloop(i)));
	break;

      case is_instruction_call:
	break;
      case is_instruction_goto:
	pips_internal_error("Go to instruction in CODE internal representation\n");
	break;

      case is_instruction_unstructured: {
	  rloops_mapping_of_unstructured(m, loops,instruction_unstructured(i));
	  break ;
      }
	
      case is_instruction_forloop: {
	/*
	  pips_user_error("Use property FOR_TO_WHILE_LOOP_IN_CONTROLIZER or "
			  "FOR_TO_DO_LOOP_IN_CONTROLIZER to convert for loops into while loops\n");
	*/
	rloops_mapping_of_statement(m, loops, forloop_body(instruction_forloop(i)));
	break ;
      }
	
      default:
	pips_internal_error("unexpected tag %d\n", instruction_tag(i));
    }
}



statement_mapping 
loops_mapping_of_statement(statement stat)
{   
    statement_mapping loops_map;
    loops_map = MAKE_STATEMENT_MAPPING();
    Nbrdo = 0;
    rloops_mapping_of_statement(loops_map, NIL, stat);

    if (get_debug_level() >= 7) {
	STATEMENT_MAPPING_MAP(stat, loops, {
	    fprintf(stderr, "statement %td in loops ", 
		    statement_number((statement) stat));
	    MAP(STATEMENT, s, 
		fprintf(stderr, "%td ", statement_number(s)),
		(list) loops);
	    fprintf(stderr, "\n");
	}, loops_map)
    }
    return(loops_map);
}

static bool 
distributable_statement_p(statement stat, set region)
{
    instruction i = statement_instruction(stat);

    switch(instruction_tag(i)) 
    {
    case is_instruction_block:
	MAPL(ps, {
	    if (distributable_statement_p(STATEMENT(CAR(ps)), 
					  region) == FALSE) {
		return(FALSE);
	    }
	}, instruction_block(i));
	return(TRUE);
	
    case is_instruction_loop:
	region = set_add_element(region, region, (char *) stat);
	return(distributable_statement_p(loop_body(instruction_loop(i)), 
					 region));
	
    case is_instruction_call:
	region = set_add_element(region, region, (char *) stat);
	return(TRUE);

    case is_instruction_whileloop:
    case is_instruction_goto:
    case is_instruction_unstructured:
    case is_instruction_test:
	return(FALSE);
    default:
	pips_error("distributable_statement_p", 
		   "unexpected tag %d\n", instruction_tag(i));
    }

    return((bool) NULL); /* just to avoid a gcc warning */
}

/* this functions checks if Kennedy's algorithm can be applied on the
loop passed as argument. If yes, it returns a set containing all
statements belonging to this loop including the initial loop itself.
otherwise, it returns an undefined set.

Our version of Kennedy's algorithm can only be applied on loops
containing no test, goto or unstructured control structures. */

set distributable_loop(l)
statement l;
{
    set r;

    pips_assert("distributable_loop", statement_loop_p(l));

    r = set_make(set_pointer);

    if (distributable_statement_p(l, r)) {
	return(r);
    }

    set_free(r);
    return(set_undefined);
}

/* returns TRUE if loop lo's index is private for this loop */
bool index_private_p(lo)
loop lo;
{
    if( lo == loop_undefined ) {
	pips_error("index_private_p", "Loop undefined\n");
    }

    return((entity) gen_find_eq(loop_index(lo), loop_locals(lo)) != 
	   entity_undefined);
}

/* this function returns the set of all statements belonging to the loop given
   even if the loop contains test, goto or unstructured control structures */
set region_of_loop(l)
statement l;
{
    set r;

    pips_assert("distributable_loop", statement_loop_p(l));

    r = set_make(set_pointer);
    region_of_statement(l,r);
    return(r);
}

void region_of_statement(stat, region)
statement stat;
set region;
{
  instruction i = statement_instruction(stat);  
    
  switch(instruction_tag(i)) {

  case is_instruction_block:
      MAPL(ps, {
	  region_of_statement(STATEMENT(CAR(ps)),region);
      }, instruction_block(i));
      break;

  case is_instruction_loop:{
      region = set_add_element(region, region, (char *) stat);
      region_of_statement(loop_body(instruction_loop(i)),region);
      break;
  }

  case is_instruction_call:
      region = set_add_element(region, region, (char *) stat);
      break;
      
  case is_instruction_goto:
      region = set_add_element(region, region, (char *) stat);
      break;
    
  case is_instruction_test:
      /* The next statement is added by Y.Q. 12/9.*/
      region = set_add_element(region, region, (char *) stat);
      region_of_statement(test_true(instruction_test(i)), region);
      region_of_statement(test_false(instruction_test(i)), region);
      break;

  case is_instruction_unstructured:{
      unstructured u = instruction_unstructured(i);
      cons *blocs = NIL;

      CONTROL_MAP(c, {
	  region_of_statement(control_statement(c), region);
      }, unstructured_control(u), blocs) ;
      
      gen_free_list(blocs) ;
      break;
  }

  default:
      pips_error("region_of_statement", 
		 "unexpected tag %d\n", instruction_tag(i));    
      
  }
}

/* @return a list of entities that are private in the current
 * context. The function can also remove from that list all the
 * variables that are localy declared in the loop body and the loop
 * indices using the apropriate flags.
 * @param obj, the loop to look at.
 * @param local, set to TRUE to remove the  the variables that
 * are localy declared.
 * @param indice, set to TRUE to remove the loop indice variable
 */
list loop_private_variables_as_entites (loop obj, bool local, bool indice) {
  // list of entity that are private to the loop according to the previous
  // phases. For historical reasons private variables are stored in the
  // locals field of the loop.
  list result = gen_copy_seq (loop_locals(obj));

  if (local == TRUE) {
    // list of localy declared entity that are stored in loop body
    list decl_var = statement_declarations (loop_body (obj));
    gen_list_and_not (&result, decl_var);
  }
  if (indice == TRUE) {
    gen_remove (&result, loop_index(obj));
  }

  return result;
}



/************************************** SORT ALL LOCALS AFTER PRIVATIZATION */

static void loop_rwt(loop l)
{
    list /* of entity */ le = loop_locals(l);
    if (le) sort_list_of_entities(le);
}

void sort_all_loop_locals(statement s)
{
    gen_multi_recurse(s, loop_domain, gen_true, loop_rwt, NULL);
}


/* Test if a loop is parallel.

   It tests the parallel status of the loop but should test extensions
   such as OpenMP pragma and so on. TODO...

   @param s is the statement that owns the loop. We need this statement to
   get the pragma for the loop.

   @return TRUE if the loop is parallel.
*/
bool parallel_loop_statement_p(statement s) {
  instruction i = statement_instruction(s);
  loop l = instruction_loop(i);

  return execution_parallel_p(loop_execution(l));
}

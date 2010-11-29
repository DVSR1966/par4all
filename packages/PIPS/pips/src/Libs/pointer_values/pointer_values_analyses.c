/*

  $Id$

  Copyright 1989-2010 MINES ParisTech
  Copyright 2010 HPC Project

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

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "effects.h"
#include "effects-util.h"
#include "text-util.h"
#include "effects-simple.h"
#include "effects-generic.h"
#include "pipsdbm.h"
#include "misc.h"

#include "pointer_values.h"

/******************** MAPPINGS */

/* I don't know how to deal with these mappings if we have to analyse several modules
   at the same time when performing an interprocedural analysis.
   We may need a stack of mappings, or a global mapping for the whole program,
   and a temporary mapping to store the resources one module at a time
*/
GENERIC_GLOBAL_FUNCTION(pv, statement_cell_relations)
GENERIC_GLOBAL_FUNCTION(gen_pv, statement_cell_relations)
GENERIC_GLOBAL_FUNCTION(kill_pv, statement_effects)



/******************** PIPSDBM INTERFACES */

static statement_cell_relations db_get_simple_pv(char * module_name)
{
  return (statement_cell_relations) db_get_memory_resource(DBR_SIMPLE_POINTER_VALUES, module_name, TRUE);
}

static void db_put_simple_pv(char * module_name, statement_cell_relations scr)
{
   DB_PUT_MEMORY_RESOURCE(DBR_SIMPLE_POINTER_VALUES, module_name, (char*) scr);
}

/* static statement_cell_relations db_get_simple_gen_pv(char * module_name) */
/* { */
/*   return (statement_cell_relations) db_get_memory_resource(DBR_SIMPLE_GEN_POINTER_VALUES, module_name, TRUE); */
/* } */

/* static void db_put_simple_gen_pv(char * module_name, statement_cell_relations scr) */
/* { */
/*    DB_PUT_MEMORY_RESOURCE(DBR_SIMPLE_GEN_POINTER_VALUES, module_name, (char*) scr); */
/* } */

/* static statement_effects db_get_simple_kill_pv(char * module_name) */
/* { */
/*   return (statement_effects) db_get_memory_resource(DBR_SIMPLE_KILL_POINTER_VALUES, module_name, TRUE); */
/* } */

/* static void db_put_simple_kill_pv(char * module_name, statement_effects se) */
/* { */
/*    DB_PUT_MEMORY_RESOURCE(DBR_SIMPLE_KILL_POINTER_VALUES, module_name, (char*) se); */
/* } */


/******************** ANALYSIS CONTEXT */


pv_context make_simple_pv_context()
{
  pv_context ctxt;

  ctxt.db_get_pv_func = db_get_simple_pv;
  ctxt.db_put_pv_func = db_put_simple_pv;
/*   ctxt.db_get_gen_pv_func = db_get_simple_gen_pv; */
/*   ctxt.db_put_gen_pv_func = db_put_simple_gen_pv; */
/*   ctxt.db_get_kill_pv_func = db_get_simple_kill_pv; */
/*   ctxt.db_put_kill_pv_func = db_put_simple_kill_pv; */
  ctxt.make_pv_from_effects_func = make_simple_pv_from_simple_effects;
  ctxt.cell_reference_with_value_of_cell_reference_translation_func =
    simple_cell_reference_with_value_of_cell_reference_translation;
  ctxt.cell_reference_with_address_of_cell_reference_translation_func =
    simple_cell_reference_with_address_of_cell_reference_translation;
  ctxt.pv_composition_with_transformer_func = simple_pv_composition_with_transformer;
  ctxt.pvs_must_union_func = simple_pvs_must_union;
  ctxt.pvs_may_union_func = simple_pvs_may_union;
  ctxt.pvs_equal_p_func = simple_pvs_syntactically_equal_p;
  ctxt.stmt_stack = stack_make(statement_domain, 0, 0);
  return ctxt;
}

#define UNDEF abort

typedef void (*void_function)();
typedef gen_chunk* (*chunks_function)();
typedef list (*list_function)();
typedef bool (*bool_function)();
typedef descriptor (*descriptor_function)();
typedef statement_cell_relations (*statement_cell_relations_function)();
typedef statement_effects (*statement_effects_function)();
typedef cell_relation (*cell_relation_function)();

void reset_pv_context(pv_context *p_ctxt)
{
  p_ctxt->db_get_pv_func = (statement_cell_relations_function) UNDEF;
  p_ctxt->db_put_pv_func = (void_function) UNDEF;
/*   p_ctxt->db_get_gen_pv_func =(statement_cell_relations_function) UNDEF ; */
/*   p_ctxt->db_put_gen_pv_func = (void_function) UNDEF; */
/*   p_ctxt->db_get_kill_pv_func = (statement_effects_function) UNDEF; */
/*   p_ctxt->db_put_kill_pv_func = (void_function) UNDEF; */
  p_ctxt->make_pv_from_effects_func = (list_function) UNDEF;
  p_ctxt->pv_composition_with_transformer_func = (cell_relation_function) UNDEF;
  p_ctxt->pvs_must_union_func = (list_function) UNDEF;
  p_ctxt->pvs_may_union_func = (list_function) UNDEF;
  p_ctxt->pvs_equal_p_func = (bool_function) UNDEF;
}

void pv_context_statement_push(statement s, pv_context * ctxt)
{
  stack_push((void *) s, ctxt->stmt_stack);
}

void pv_context_statement_pop(pv_context * ctxt)
{
  (void) stack_pop( ctxt->stmt_stack);
}

statement pv_context_statement_head(pv_context * ctxt)
{
  return ((statement) stack_head(ctxt->stmt_stack));
}

/************* RESULTS HOOK */

pv_results make_pv_results()
{
  pv_results pv_res;
  pv_res.l_out = NIL;
  pv_res.result_paths = NIL;
  pv_res.result_paths_interpretations = NIL;
  return pv_res;
}

void free_pv_results_paths(pv_results *pv_res)
{
  gen_full_free_list(pv_res->result_paths);
  pv_res->result_paths = NIL;
  gen_full_free_list(pv_res->result_paths_interpretations);
  pv_res->result_paths_interpretations = NIL;
}

void print_pv_results(pv_results pv_res)
{
  fprintf(stderr, "l_out =");
  print_pointer_values(pv_res.l_out);
  list l_rp = pv_res.result_paths;
  list l_rpi = pv_res.result_paths_interpretations;

  if (!ENDP(l_rp))
    {
      fprintf(stderr, "result_paths are:\n");
      for(; !ENDP(l_rp); POP(l_rp), POP(l_rpi))
	{
	  effect eff = EFFECT(CAR(l_rp));
	  cell_interpretation ci = CELL_INTERPRETATION(CAR(l_rpi));
	  fprintf(stderr, "%s:",
		  cell_interpretation_value_of_p(ci)
		  ? "value of" : "address of");
	  (*effect_prettyprint_func)(eff);
	}
    }
  else
    fprintf(stderr, "result_path is undefined\n");
}




/******************** LOCAL FUNCTIONS DECLARATIONS */

static
list sequence_to_post_pv(sequence seq, list l_in, pv_context *ctxt);

static
list statement_to_post_pv(statement stmt, list l_in, pv_context *ctxt);

static
list declarations_to_post_pv(list l_decl, list l_in, pv_context *ctxt);

static
list declaration_to_post_pv(entity e, list l_in, pv_context *ctxt);

static
list instruction_to_post_pv(instruction inst, list l_in, pv_context *ctxt);

static
list test_to_post_pv(test t, list l_in, pv_context *ctxt);

static
list loop_to_post_pv(loop l, list l_in, pv_context *ctxt);

static
list whileloop_to_post_pv(whileloop l, list l_in, pv_context *ctxt);

static
list forloop_to_post_pv(forloop l, list l_in, pv_context *ctxt);

static
list unstructured_to_post_pv(unstructured u, list l_in, pv_context *ctxt);


static
void call_to_post_pv(call c, list l_in, pv_results *pv_res, pv_context *ctxt);




/**************** MODULE ANALYSIS *************/

static
list sequence_to_post_pv(sequence seq, list l_in, pv_context *ctxt)
{
  list l_cur = l_in;
  list l_locals = NIL;
  pips_debug(1, "begin\n");
  FOREACH(STATEMENT, stmt, sequence_statements(seq))
    {
      ifdebug(2){

	pips_debug(2, "dealing with statement");
	print_statement(stmt);
	pips_debug_pvs(2, "l_cur =", l_cur);
      }
      /* keep local variables in declaration reverse order */
      if (declaration_statement_p(stmt))
	{
	  pv_context_statement_push(stmt, ctxt);
	  if(bound_pv_p(stmt))
	    update_pv(stmt, make_cell_relations(gen_full_copy_list(l_cur)));
	  else
	    store_pv(stmt, make_cell_relations(gen_full_copy_list(l_cur)));
	  FOREACH(ENTITY, e, statement_declarations(stmt))
	    {
	      type e_type = basic_concrete_type(entity_type(e));
	      /* beware don't push static variables and non pointer variables. */
	      if (! static_area_p(ram_section(storage_ram(entity_storage(e))))
		  &&!type_fundamental_basic_p(e_type)
		  && type_leads_to_pointer_p(e_type))
		l_locals = CONS(ENTITY, e, l_locals);
	      l_cur = declaration_to_post_pv(e, l_cur, ctxt);
	      free_type(e_type);
	    }
	  //store_gen_pv(stmt, make_cell_relations(NIL));
	  //store_kill_pv(stmt, make_effects(NIL));
	  pv_context_statement_pop(ctxt);
	}
      else
	l_cur = statement_to_post_pv(stmt, l_cur, ctxt);

    }

  /* don't forget to eliminate local declarations on exit */
  /* ... */
  if (!ENDP(l_locals))
    {
      pips_debug(5, "eliminating local variables\n");
      expression rhs_exp =
	entity_to_expression(undefined_pointer_value_entity());

      FOREACH(ENTITY, e, l_locals)
	{
	  pips_debug(5, "dealing with %s\n", entity_name(e));
	  pv_results pv_res = make_pv_results();
	  pointer_values_remove_var(e, false, l_cur, &pv_res, ctxt);
	  l_cur= pv_res.l_out;
	  free_pv_results_paths(&pv_res);
	}
      free_expression(rhs_exp);
    }

  pips_debug_pvs(2, "returning:", l_cur);
  pips_debug(1, "end\n");
  return (l_cur);
}

static
list statement_to_post_pv(statement stmt, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");
  pv_context_statement_push(stmt, ctxt);
  pips_debug_pvs(2, "input pvs:", l_in);

  if(bound_pv_p(stmt))
    update_pv(stmt, make_cell_relations(gen_full_copy_list(l_in)));
  else
    store_pv(stmt, make_cell_relations(gen_full_copy_list(l_in)));

  if (declaration_statement_p(stmt))
    {
      list l_decl = statement_declarations(stmt);
      l_out = declarations_to_post_pv(l_decl, l_in, ctxt);
    }
  else
    {
      l_out = instruction_to_post_pv(statement_instruction(stmt), l_in, ctxt);
    }

  pips_debug_pvs(2, "before composition_with_transformer:", l_out);
  l_out = pvs_composition_with_transformer(l_out, transformer_undefined, ctxt);

  //store_gen_pv(stmt, make_cell_relations(NIL));
  //store_kill_pv(stmt, make_effects(NIL));
  pips_debug_pvs(2, "returning:", l_out);
  pips_debug(1, "end\n");
  pv_context_statement_pop(ctxt);

  return (l_out);
}

static
list declarations_to_post_pv(list l_decl, list l_in, pv_context *ctxt)
{
  list l_out = l_in;
  pips_debug(1, "begin\n");

  FOREACH(ENTITY, e, l_decl)
    {
      /* well, not exactly, we must take kills into account */
      l_out = gen_nconc(l_out, declaration_to_post_pv(e, l_out, ctxt));
    }
  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}


static
list declaration_to_post_pv(entity e, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  type e_type = basic_concrete_type(entity_type(e));
  bool static_p = static_area_p(ram_section(storage_ram(entity_storage(e))));

  pips_debug(1, "begin\n");

  expression lhs_exp = entity_to_expression(e);
  expression rhs_exp = expression_undefined;
  bool free_rhs_exp = false; /* turned to true if rhs_exp is built and
				must be freed */

  value v_init = entity_initial(e);
  if (v_init == value_undefined)
    {
      pips_debug(2, "undefined inital value\n");

      if (static_p
	  && !type_fundamental_basic_p(e_type)
	  && type_leads_to_pointer_p(e_type))
	/* when no initialization is provided for pointer static variables,
	   or aggregate variables which recursively have pointer fields,
	   then all pointers are initialized to NULL previous to program
	   execution.
	*/
	rhs_exp = entity_to_expression(null_pointer_value_entity());
      else
	rhs_exp = entity_to_expression(undefined_pointer_value_entity());
      free_rhs_exp = true;
    }
  else
    {
      switch (value_tag(v_init))
	{
	case is_value_expression:
	  rhs_exp = value_expression(v_init);
	  break;
	case is_value_unknown:
	  pips_debug(2, "unknown inital value\n");
	  rhs_exp = expression_undefined;
	  break;
	case is_value_code:
	case is_value_symbolic:
	case is_value_constant:
	case is_value_intrinsic:
	default:
	  pips_internal_error("unexpected tag");
	}
    }

  pv_results pv_res = make_pv_results();
  assignment_to_post_pv(lhs_exp, static_p, rhs_exp, true, l_in, &pv_res, ctxt);
  l_out = pv_res.l_out;
  free_pv_results_paths(&pv_res);

  free_expression(lhs_exp);
  if (free_rhs_exp) free_expression(rhs_exp);

  pips_debug_pvs(2, "returning:", l_out);
  pips_debug(1, "end\n");
  free_type(e_type);
  return (l_out);
}

static
list instruction_to_post_pv(instruction inst, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");

  switch(instruction_tag(inst))
    {
    case is_instruction_sequence:
      l_out = sequence_to_post_pv(instruction_sequence(inst), l_in, ctxt);
      break;
    case is_instruction_test:
      l_out = test_to_post_pv(instruction_test(inst), l_in, ctxt);
      break;
    case is_instruction_loop:
      l_out = loop_to_post_pv(instruction_loop(inst), l_in, ctxt);
      break;
    case is_instruction_whileloop:
      l_out = whileloop_to_post_pv(instruction_whileloop(inst), l_in, ctxt);
      break;
    case is_instruction_forloop:
      l_out = forloop_to_post_pv(instruction_forloop(inst), l_in, ctxt);
      break;
    case is_instruction_unstructured:
      l_out = unstructured_to_post_pv(instruction_unstructured(inst), l_in, ctxt);
      break;
    case is_instruction_expression:
      {
	pv_results pv_res = make_pv_results();
	expression_to_post_pv(instruction_expression(inst), l_in, &pv_res, ctxt);
	l_out = pv_res.l_out;
	free_pv_results_paths(&pv_res);
      }
      break;
    case is_instruction_call:
      {
	pv_results pv_res = make_pv_results();
	call_to_post_pv(instruction_call(inst), l_in, &pv_res, ctxt);
	l_out = pv_res.l_out;
	free_pv_results_paths(&pv_res);
      }
      break;
    case is_instruction_goto:
      pips_internal_error("unexpected goto in pointer values analyses");
      break;
    case is_instruction_multitest:
      pips_internal_error("unexpected multitest in pointer values analyses");
      break;
    default:
      pips_internal_error("unknown instruction tag");
    }
  pips_debug_pvs(2, "returning:", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

static
list test_to_post_pv(test t, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");

  expression t_cond = test_condition(t);
  statement t_true = test_true(t);
  statement t_false = test_false(t);

  pv_results pv_res = make_pv_results();
  expression_to_post_pv(t_cond, l_in, &pv_res, ctxt);

  list l_in_branches = pv_res.l_out;

  list l_out_true = statement_to_post_pv(t_true, gen_full_copy_list(l_in_branches), ctxt);
  list l_out_false = statement_to_post_pv(t_false, l_in_branches, ctxt);

  l_out = (*ctxt->pvs_may_union_func)(l_out_true, l_out_false);

  free_pv_results_paths(&pv_res);

  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

#define PV_NB_MAX_ITER_FIX_POINT 3

static
list loop_to_post_pv(loop l, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  range r = loop_range(l);
  list l_in_cur = l_in;
  statement body = loop_body(l);
  pips_debug(1, "begin\n");

  /* first loop range is evaluated */
  pv_results pv_res = make_pv_results();
  range_to_post_pv(r, l_in_cur, &pv_res, ctxt);
  free_pv_results_paths(&pv_res);
  l_in_cur = pv_res.l_out;
  list l_saved = gen_full_copy_list(l_in_cur);

  /* then, the loop body is executed if and only if the upper bound
     is greater than the lower bound, else the loop body is only possibly
     executed.
  */

  /* as a first approximation, we perform no test on the loop bounds,
     and thus assume that the loop body is only possibly executed
  */
  int i = 0;
  bool fix_point_reached = false;
  l_out = pv_res.l_out;
  do
    {
      pips_debug(3, "fix point iteration number %d.\n", i+1);
      list l_iter_in = gen_full_copy_list(l_out);
      l_out = statement_to_post_pv(body, l_out, ctxt);

      /* this iteration may never be executed :*/
      l_out = (*ctxt->pvs_may_union_func)(l_out, gen_full_copy_list(l_iter_in));
      i++;
      fix_point_reached = (l_out == l_iter_in)
	|| (*ctxt->pvs_equal_p_func)(l_iter_in, l_out);
      pips_debug(3, "fix point %s reached\n", fix_point_reached? "":"not");
      gen_full_free_list(l_iter_in);
    }
  while(i<PV_NB_MAX_ITER_FIX_POINT && !fix_point_reached);

  if (!fix_point_reached)
    {
      pv_results pv_res_failed = make_pv_results();
      effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
      list l_anywhere = CONS(EFFECT, anywhere_eff, NIL);
      list l_kind = CONS(CELL_INTERPRETATION,
			 make_cell_interpretation_address_of(), NIL);
      single_pointer_assignment_to_post_pv(anywhere_eff, l_anywhere, l_kind,
					   false, l_saved,
					   &pv_res_failed, ctxt);
      l_out =  pv_res_failed.l_out;
      free_pv_results_paths(&pv_res_failed);
      free_effect(anywhere_eff);
      gen_free_list(l_anywhere);
      gen_full_free_list(l_kind);

      /* now update input pointer values of inner loop statements */
      l_in_cur = gen_full_copy_list(l_out);
      list l_tmp = statement_to_post_pv(body, l_in_cur, ctxt);
      gen_full_free_list(l_tmp);
    }
  else
    gen_full_free_list(l_saved);

  pips_debug_pvs(1, "end with l_out =\n", l_out);
  return (l_out);
}

static
list whileloop_to_post_pv(whileloop l, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  list l_in_cur = NIL;
  list l_saved = gen_full_copy_list(l_in); /* in case fix point is not reached */
  expression cond = whileloop_condition(l);
  bool before_p = evaluation_before_p(whileloop_evaluation(l));
  statement body = whileloop_body(l);
  pips_debug(1, "begin\n");

  int i = 1;
  bool fix_point_reached = false;
  l_out = l_in;
  do
    {
      pips_debug(3, "fix point iteration number %d.\n", i);
      list l_iter_in = gen_full_copy_list(l_out);
      l_in_cur = l_out;

      if(before_p)
	{
	  pv_results pv_res_cond = make_pv_results();
	  expression_to_post_pv(cond, l_in_cur, &pv_res_cond, ctxt);
	  l_in_cur = pv_res_cond.l_out;
	  free_pv_results_paths(&pv_res_cond);
	}

      l_out = statement_to_post_pv(body, l_in_cur, ctxt);

      if(!before_p)
	{
	  pv_results pv_res_cond = make_pv_results();
	  expression_to_post_pv(cond, l_out, &pv_res_cond, ctxt);
	  l_out = pv_res_cond.l_out;
	  free_pv_results_paths(&pv_res_cond);
	}

      /* this iteration may never be executed :*/
      if (i!=1 || before_p)
	  l_out = (*ctxt->pvs_may_union_func)(l_out,
					      gen_full_copy_list(l_iter_in));
      else
	{
	  l_in = gen_full_copy_list(l_out);
	}

      i++;
      fix_point_reached = (l_out == l_iter_in)
	|| (*ctxt->pvs_equal_p_func)(l_iter_in, l_out);
      pips_debug(3, "fix point %s reached\n", fix_point_reached? "":"not");
      gen_full_free_list(l_iter_in);
    }
  while(i<=PV_NB_MAX_ITER_FIX_POINT && !fix_point_reached);

  if (!fix_point_reached)
    {
      pv_results pv_res_failed = make_pv_results();
      effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
      list l_anywhere = CONS(EFFECT, anywhere_eff, NIL);
      list l_kind = CONS(CELL_INTERPRETATION,
			 make_cell_interpretation_address_of(), NIL);
      single_pointer_assignment_to_post_pv(anywhere_eff, l_anywhere, l_kind,
					   false, l_saved,
					   &pv_res_failed, ctxt);
      l_out =  pv_res_failed.l_out;
      free_pv_results_paths(&pv_res_failed);
      free_effect(anywhere_eff);
      gen_free_list(l_anywhere);
      gen_full_free_list(l_kind);

      /* now update input pointer values of inner loop statements */
      l_in_cur = gen_full_copy_list(l_out);
      if(before_p)
	{
	  pv_results pv_res_cond = make_pv_results();
	  expression_to_post_pv(cond, l_in_cur, &pv_res_cond, ctxt);
	  l_in_cur = pv_res_cond.l_out;
	  free_pv_results_paths(&pv_res_cond);
	}

      list l_tmp = statement_to_post_pv(body, l_in_cur, ctxt);

      if(!before_p)
	{
	  pv_results pv_res_cond = make_pv_results();
	  expression_to_post_pv(cond, l_tmp, &pv_res_cond, ctxt);
	  free_pv_results_paths(&pv_res_cond);
	  gen_full_free_list(pv_res_cond.l_out);
	}
      else
	gen_full_free_list(l_tmp);
    }
  else
    gen_full_free_list(l_saved);

  pips_debug_pvs(2, "returning:", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

static
list forloop_to_post_pv(forloop l, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  list l_in_cur = NIL;
  pips_debug(1, "begin\n");

  expression init = forloop_initialization(l);
  expression cond = forloop_condition(l);
  expression incr = forloop_increment(l);
  statement body = forloop_body(l);

  /* First, the initialization is always evaluatated */
  pv_results pv_res_init = make_pv_results();
  expression_to_post_pv(init, l_in, &pv_res_init, ctxt);
  l_in_cur = pv_res_init.l_out;
  l_in = gen_full_copy_list(l_in_cur); /* saved in case fix point is not reached */
  free_pv_results_paths(&pv_res_init);

  int i = 1;
  bool fix_point_reached = false;
  l_out = l_in_cur;
  do
    {
      pips_debug(3, "fix point iteration number %d.\n", i);
      list l_iter_in = gen_full_copy_list(l_out);
      l_in_cur = l_out;

      /* condition is evaluated before each iteration */
      pv_results pv_res_cond = make_pv_results();
      expression_to_post_pv(cond, l_in_cur, &pv_res_cond, ctxt);
      l_in_cur = pv_res_cond.l_out;
      free_pv_results_paths(&pv_res_cond);

      l_in_cur = statement_to_post_pv(body, l_in_cur, ctxt);

      /* increment expression is evaluated at the end of each iteration */
      pv_results pv_res_incr = make_pv_results();
      expression_to_post_pv(incr, l_in_cur, &pv_res_incr, ctxt);
      l_in_cur = pv_res_incr.l_out;
      free_pv_results_paths(&pv_res_incr);

      /* this iteration may never be executed :*/
      l_out = (*ctxt->pvs_may_union_func)(l_in_cur,
					  gen_full_copy_list(l_iter_in));
      i++;
      fix_point_reached = (l_out == l_iter_in)
	|| (*ctxt->pvs_equal_p_func)(l_iter_in, l_out);
      pips_debug(3, "fix point %s reached\n", fix_point_reached? "":"not");
      gen_full_free_list(l_iter_in);
    }
  while(i<=PV_NB_MAX_ITER_FIX_POINT && !fix_point_reached);

  if (!fix_point_reached)
    {
      pv_results pv_res_failed = make_pv_results();
      effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
      list l_anywhere = CONS(EFFECT, anywhere_eff, NIL);
      list l_kind = CONS(CELL_INTERPRETATION,
			 make_cell_interpretation_address_of(), NIL);
      single_pointer_assignment_to_post_pv(anywhere_eff, l_anywhere, l_kind,
					   false, l_in,
					   &pv_res_failed, ctxt);
      l_out =  pv_res_failed.l_out;
      free_pv_results_paths(&pv_res_failed);
      free_effect(anywhere_eff);
      gen_free_list(l_anywhere);
      gen_full_free_list(l_kind);

      /* now update input pointer values of inner loop statements */
      l_in_cur = gen_full_copy_list(l_out);
      pv_results pv_res_cond = make_pv_results();
      expression_to_post_pv(cond, l_in_cur, &pv_res_cond, ctxt);
      l_in_cur = pv_res_cond.l_out;
      free_pv_results_paths(&pv_res_cond);

      list l_tmp = statement_to_post_pv(body, l_in_cur, ctxt);

      gen_full_free_list(l_tmp);
    }


  pips_debug_pvs(2, "returning:", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}


static
list unstructured_to_post_pv(unstructured __attribute__ ((unused))u, list __attribute__ ((unused))l_in, pv_context __attribute__ ((unused)) *ctxt)
{
  list l_out = NIL;
  pips_internal_error("not yet implemented");
   pips_debug_pvs(2, "returning: ", l_out);
 pips_debug(1, "end\n");
  return (l_out);
}


void range_to_post_pv(range r, list l_in, pv_results * pv_res, pv_context *ctxt)
{
    expression el = range_lower(r);
    expression eu = range_upper(r);
    expression ei = range_increment(r);

    pips_debug(1, "begin\n");

    expression_to_post_pv(el, l_in, pv_res, ctxt);
    expression_to_post_pv(eu, pv_res->l_out, pv_res, ctxt);
    expression_to_post_pv(ei, pv_res->l_out, pv_res, ctxt);

    free_pv_results_paths(pv_res);

    pips_debug_pvs(1, "end with pv_res->l_out:\n", pv_res->l_out);
}

void expression_to_post_pv(expression exp, list l_in, pv_results * pv_res, pv_context *ctxt)
{
  if (expression_undefined_p(exp))
    {
      pips_debug(1, "begin for undefined expression, returning undefined pointer_value\n");
      pv_res->l_out = l_in;
      pv_res->result_paths = CONS(EFFECT, make_effect(make_undefined_pointer_value_cell(),
						      make_action_write_memory(),
						      make_approximation_exact(),
						      make_descriptor_none()),NIL);
      pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
						  make_cell_interpretation_value_of(), NIL);
    }
  else
    {
      pips_debug(1, "begin for expression : %s\n",
		 words_to_string(words_expression(exp,NIL)));

      syntax exp_syntax = expression_syntax(exp);

      switch(syntax_tag(exp_syntax))
	{
	case is_syntax_reference:
	  pips_debug(5, "reference case\n");
	  reference exp_ref = syntax_reference(exp_syntax);
	  if (same_string_p(entity_local_name(reference_variable(exp_ref)), "NULL"))
	    {
	      pv_res->result_paths = CONS(EFFECT, make_effect(make_null_pointer_value_cell(),
							     make_action_read_memory(),
							     make_approximation_exact(),
							     make_descriptor_none()), NIL);
	      pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
							  make_cell_interpretation_value_of(),
							  NIL);
	    }
	  else
	    {
	      pv_res->result_paths = CONS(EFFECT,
					 (*reference_to_effect_func)
					 (copy_reference(exp_ref),
					  make_action_write_memory(),
					  false), NIL);
	      pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
							  make_cell_interpretation_value_of(),
							  NIL);
	    }
	  /* we assume no effects on aliases due to subscripts evaluations for the moment */
	  pv_res->l_out = l_in;
	  break;
	case is_syntax_range:
	  pips_internal_error("not yet implemented");
	  break;
	case is_syntax_call:
	  {
	    call_to_post_pv(syntax_call(exp_syntax), l_in, pv_res, ctxt);
	    break;
	  }
	case is_syntax_cast:
	  {
	    expression_to_post_pv(cast_expression(syntax_cast(exp_syntax)),
				  l_in, pv_res, ctxt);
	    pips_debug(5, "cast case\n");
	  break;
	  }
	case is_syntax_sizeofexpression:
	  {
	    /* we assume no effects on aliases due to sizeof argument expression
	       for the moment */
	    pv_res->l_out = l_in;
	    break;
	  }
	case is_syntax_subscript:
	  {
	    pips_debug(5, "subscript case\n");
	    effect eff;
	    /* aborts if there are calls in subscript expressions */
	    list l_tmp = generic_proper_effects_of_complex_address_expression(exp, &eff, true);
	    gen_full_free_list(l_tmp);
	    pv_res->result_paths = CONS(EFFECT, eff, NIL);
	    pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
							make_cell_interpretation_value_of(),
							NIL);
	    /* we assume no effects on aliases due to subscripts evaluations for the moment */
	    pv_res->l_out = l_in;
	    break;
	  }
	case is_syntax_application:
	  pips_internal_error("not yet implemented");
	  break;
	case is_syntax_va_arg:
	  {
	    pips_internal_error("not yet implemented");
	    break;
	  }
	default:
	  pips_internal_error("unexpected tag %d", syntax_tag(exp_syntax));
	}
    }

  pips_debug_pv_results(1, "end with pv_results =\n", *pv_res);
  pips_debug(1, "end\n");
  return;
}

static
void call_to_post_pv(call c, list l_in, pv_results *pv_res, pv_context *ctxt)
{
  entity func = call_function(c);
  value func_init = entity_initial(func);
  tag t = value_tag(func_init);
  list func_args = call_arguments(c);
  type func_type = ultimate_type(entity_type(func));

  pips_debug(1, "begin for %s\n", entity_local_name(func));

  if(type_functional_p(func_type))
    {
      switch (t)
	{
	case is_value_code:
	  pips_debug(5, "external function \n");
	  /* the expression that denotes the called function and the arguments
	     are evaluated; then there is a sequence point, and the function is called
	  */
	  pips_user_warning("external call, not handled yet, returning all locations effect "
			    "and assuming no effects on pointer_values \n");
	  pv_res->l_out = l_in;
	  pv_res->result_paths = CONS(EFFECT, make_anywhere_effect(make_action_write_memory()),
				      NIL);
	  pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
						      make_cell_interpretation_address_of(),
						      NIL);
	  break;

	case is_value_intrinsic:
	  pips_debug(5, "intrinsic function\n");
	  intrinsic_to_post_pv(func, func_args, l_in, pv_res, ctxt);
	  break;

	case is_value_symbolic:
	  pips_debug(5, "symbolic\n");
	  pv_res->l_out = l_in;
	  break;

	case is_value_constant:
	  pips_debug(5, "constant\n");
	  pv_res->l_out = l_in;

	  constant func_const = value_constant(func_init);
	  /* We should be here only in case of a pointer value rhs, and the value should be 0 */
	  if (constant_int_p(func_const) && (constant_int(func_const) == 0))
	    {
	      /* use approximation_exact to be consistent with effects,
		 should be approximation_exact */
	      pv_res->result_paths = CONS(EFFECT,
					  make_effect(make_null_pointer_value_cell(),
						      make_action_read_memory(),
						      make_approximation_exact(),
						      make_descriptor_none()),
					  NIL);
	      pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
							  make_cell_interpretation_value_of(),
							  NIL);
	    }
	  else
	    {
	      type tt = functional_result(type_functional(func_type));
	      if (type_variable_p(tt))
		{
		  variable v = type_variable(tt);
		  basic b = variable_basic(v);
		  if (basic_string_p(b))/* constant strings */
		    {
		      /* not generic here */
		      effect eff = make_effect(make_cell_reference(make_reference(func, NIL)),
					       make_action_read_memory(),
					       make_approximation_exact(),
					       make_descriptor_none());
		      effect_add_dereferencing_dimension(eff);
		      pv_res->result_paths = CONS(EFFECT, eff, NIL);
		      pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
							  make_cell_interpretation_address_of(),
							  NIL);
		    }
		}
	    }
	  pv_res->l_out = l_in;
	  break;

	case is_value_unknown:
	  pips_internal_error("unknown function ");
	  break;

	default:
	  pips_internal_error("unknown tag %d", t);
	  break;
	}
    }
  else if(type_variable_p(func_type))
    {
      pips_internal_error("not yet implemented");
    }
  else if(type_statement_p(func_type))
    {
      pips_internal_error("not yet implemented");
    }
  else
    {
      pips_internal_error("Unexpected case");
    }

  pips_debug_pvs(2, "returning pv_res->l_out:", pv_res->l_out);
  pips_debug(1, "end\n");
  return;
}



/*
  @brief returns in pv_res the effects of a single pointer assignment on pointer values
  @param lhs_eff is the left hand side path of the assignment
  @param l_rhs_eff is a list of rhs paths corresponding to the rhs
  @param l_rhs_kind is the list of rhs paths interpretations corresponding to elements of l_rhs_eff
  @param l_in is a list of the input pointer values
  @param pv_res is the struture holding the output result
  @param ctxt gives the functions specific to the kind of pointer values to be
          computed.
 */
void single_pointer_assignment_to_post_pv(effect lhs_eff,
					  list l_rhs_eff, list l_rhs_kind,
					  bool declaration_p, list l_in,
					  pv_results *pv_res, pv_context *ctxt)
{
  list l_out = NIL;
  list l_aliased = NIL;
  list l_kill = NIL;
  list l_gen = NIL;

  pips_debug_effect(1, "begin for lhs_eff =", lhs_eff);
  pips_debug(1, "and l_rhs_eff:\n");
  ifdebug(1)
    {
      list l_tmp = l_rhs_kind;
      FOREACH(EFFECT, rhs_eff, l_rhs_eff)
	{
	  cell_interpretation ci = CELL_INTERPRETATION(CAR(l_tmp));
	  pips_debug(1, "%s of:\n", cell_interpretation_value_of_p(ci)?"value": "address");
	  pips_debug_effect(1, "\t", rhs_eff);
	  POP(l_tmp);
	}
    }
  pips_debug_pvs(1, "and l_in =", l_in);
  bool anywhere_lhs_p = false;

  /* First search for all killed paths */
  /* we could be more precise/generic on abstract locations */
  if (anywhere_effect_p(lhs_eff))
    {
      pips_assert("we cannot have an anywhere lhs for a declaration\n", !declaration_p);
      pips_debug(3, "anywhere lhs\n");
      anywhere_lhs_p = true;
      l_kill = CONS(EFFECT, copy_effect(lhs_eff), NIL);
      l_aliased = l_kill;
    }
  else
    {
      if (!declaration_p) /* no aliases for a newly declared entity */
	{
	  l_aliased = effect_find_aliased_paths_with_pointer_values(lhs_eff, l_in, ctxt);
	  if (!ENDP(l_aliased) && anywhere_effect_p(EFFECT(CAR(l_aliased))))
	    {
	      pips_debug(3, "anywhere lhs (from aliases)\n");
	      anywhere_lhs_p = true;
	      l_kill = l_aliased;
	    }
	  else
	    {
	      /* if lhs_eff is a may-be-killed, then all aliased effects are also
		 may-be-killed effects */
	      if (effect_may_p(lhs_eff))
		{
		  pips_debug(3, "may lhs effect, changing all aliased effects to may\n");
		  effects_to_may_effects(l_aliased);
		}
	      l_kill = CONS(EFFECT, copy_effect(lhs_eff), l_aliased);
	    }
	}
      else
	{
	  l_kill = CONS(EFFECT, copy_effect(lhs_eff), NIL);
	}
    }

  pips_debug_effects(2, "l_kill =", l_kill);

  if (anywhere_lhs_p)
    {
      free_pv_results_paths(pv_res);
      pv_res->result_paths = l_aliased;
      pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION, make_cell_interpretation_address_of(), NIL);

      /* we must find in l_in all pointers p and generate p == rhs for all rhs if p is
	 of a type compatible with rhs, and p == &*anywhere* otherwise.
	 in fact, for the moment we generate p == &*anywhere* in all cases
      */
      effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
      cell_interpretation rhs_kind = make_cell_interpretation_address_of();
      FOREACH(CELL_RELATION, pv_in, l_in)
	{
	  /* dealing with first cell */
	  /* not generic */
	  effect eff_alias = make_effect(copy_cell(cell_relation_first_cell(pv_in)),
					 make_action_write_memory(),
					 make_approximation_may(),
					 make_descriptor_none());

	  list l_gen_pv = (* ctxt->make_pv_from_effects_func)
	    (eff_alias, anywhere_eff, rhs_kind, l_in);
	  l_gen = (*ctxt->pvs_must_union_func)(l_gen_pv, l_gen);
	  free_effect(eff_alias);

	  if (cell_relation_second_value_of_p(pv_in)
	      && !undefined_pointer_value_cell_p(cell_relation_second_cell(pv_in))
	      && !null_pointer_value_cell_p(cell_relation_second_cell(pv_in)) )
	    {
	      /* not generic */
	      effect eff_alias = make_effect(copy_cell(cell_relation_second_cell(pv_in)),
					     make_action_write_memory(),
					     make_approximation_may(),
					     make_descriptor_none());

	      list l_gen_pv = (* ctxt->make_pv_from_effects_func)
		(eff_alias, anywhere_eff, rhs_kind, l_in);
	      l_gen = (*ctxt->pvs_must_union_func)(l_gen_pv, l_gen);
	      free_effect(eff_alias);
	    }
	}
      free_effect(anywhere_eff);
      free_cell_interpretation(rhs_kind);
    }
  else
    {
      /* generate for all alias p in l_kill p == rhs_eff */
      FOREACH(EFFECT, eff_alias, l_kill)
	{
	  list l_rhs_kind_tmp = l_rhs_kind;
	  FOREACH(EFFECT, rhs_eff, l_rhs_eff)
	    {
	      cell_interpretation rhs_kind =
		CELL_INTERPRETATION(CAR(l_rhs_kind_tmp));
	      //bool exact_preceding_p = true;
	      list l_gen_pv = (* ctxt->make_pv_from_effects_func)
		(eff_alias, rhs_eff, rhs_kind, l_in);
	      l_gen = gen_nconc(l_gen_pv, l_gen);
	      POP(l_rhs_kind_tmp);
	    }
	}
      if (declaration_p)
	{
	  gen_full_free_list(l_kill);
	  l_kill = NIL;
	}
      pips_debug_pvs(2, "l_gen =", l_gen);
    }

  /* now take kills into account */
  l_out = kill_pointer_values(l_in, l_kill, ctxt);
  pips_debug_pvs(2, "l_out_after kill:", l_out);

  /* and add gen */
  l_out = (*ctxt->pvs_must_union_func)(l_out, l_gen);

  pv_res->l_out = l_out;

  return;
}


/*
  @brief returns in pv_res the effects of a multiple pointer assignment (through nested strutures or arrays) on pointer values
  @param lhs_base_eff is the left hand side path of the assignment
  @param l_rhs_base_eff is a list of rhs paths corresponding to the rhs
  @param l_rhs_base_kind is the list of rhs paths interpretations corresponding to elements of l_rhs_eff
  @param l_in is a list of the input pointer values
  @param pv_res is the struture holding the output result
  @param ctxt gives the functions specific to the kind of pointer values to be
          computed.
 */
void multiple_pointer_assignment_to_post_pv(effect lhs_base_eff, type lhs_type,
					    list l_rhs_base_eff, list l_rhs_base_kind,
					    bool declaration_p, list l_in,
					    pv_results *pv_res, pv_context *ctxt)
{
  list l_in_cur = l_in;
  cell rhs_base_cell = gen_length(l_rhs_base_eff) > 0
    ? effect_cell(EFFECT(CAR(l_rhs_base_eff))): cell_undefined;
  bool anywhere_lhs_p = false;

  pips_debug(1, "begin\n");

  pips_assert("assigning NULL to several pointers"
	      " at the same time is forbidden!\n", !(!cell_undefined_p(rhs_base_cell) && null_pointer_value_cell_p(rhs_base_cell)));

  /* we could be more precise here on abstract locations */
  if (anywhere_effect_p(lhs_base_eff))
    {
      pips_assert("we cannot have an anywhere lhs for a declaration\n", !declaration_p);
      pips_debug(3, "anywhere lhs\n");
      anywhere_lhs_p = true;

      list l_rhs_eff = CONS(EFFECT, make_anywhere_effect(make_action_write_memory()), NIL);
      list l_rhs_kind = CONS(CELL_INTERPRETATION, make_cell_interpretation_address_of(), NIL);

      single_pointer_assignment_to_post_pv(lhs_base_eff, l_rhs_eff, l_rhs_kind, declaration_p, l_in_cur, pv_res, ctxt);

      gen_full_free_list(l_rhs_eff);
      gen_full_free_list(l_rhs_kind);
    }
  else /* if (!anywhere_lhs_p) */
    {
      /* lhs is not a pointer, but it is an array of pointers or an aggregate type
	 with pointers.... */
      /* In this case, it cannot be an address_of case */

      /* First, search for all accessible pointers */
      list l_lhs = generic_effect_generate_all_accessible_paths_effects_with_level
	(lhs_base_eff, lhs_type, is_action_write, false, 0, true);
      if(effect_exact_p(lhs_base_eff))
	effects_to_must_effects(l_lhs); /* to work around the fact that exact effects are must effects */
      pips_debug_effects(2, "l_lhs = ", l_lhs);

      /* Then for each found pointer, do as if there were an assignment by translating
         the rhs path accordingly 
      */
      if (!ENDP(l_lhs))
	{
	  list l_lhs_tmp = l_lhs;
	  while (!anywhere_lhs_p && !ENDP(l_lhs_tmp))
	    {
	      /* build the list of corresponding rhs */
	      effect lhs_eff = EFFECT(CAR(l_lhs_tmp));
	      reference lhs_ref = effect_any_reference(lhs_eff);
	      list lhs_dims = reference_indices(lhs_ref);

	      reference lhs_base_ref = effect_any_reference(lhs_base_eff);
	      size_t lhs_base_nb_dim = gen_length(reference_indices(lhs_base_ref));
	      list l_rhs_eff = NIL;
	      list l_rhs_kind = NIL;

	      bool free_rhs_kind = false;
	      list l_rhs_base_kind_tmp = l_rhs_base_kind;
	      FOREACH(EFFECT, rhs_base_eff, l_rhs_base_eff)
		{
		  effect rhs_eff = copy_effect(rhs_base_eff);
		  cell_interpretation rhs_kind = CELL_INTERPRETATION(CAR(l_rhs_base_kind));

		  if (!undefined_pointer_value_cell_p(rhs_base_cell)
		      && !anywhere_effect_p(rhs_base_eff))
		    {
		      reference rhs_ref = effect_any_reference(rhs_eff);
		      type rhs_type = cell_reference_to_type(rhs_ref);

		      if (!type_equal_p(lhs_type, rhs_type))
			{
			  pips_debug(5, "not same lhs and rhs types generating anywhere rhs\n");
			  rhs_eff = make_anywhere_effect(make_action_write_memory()); /* should be refined */
			  rhs_kind = make_cell_interpretation_address_of();
			  free_rhs_kind = true;
			}
		      else /* general case at least :-) */
			{
			  /* This is not generic, I should use a translation algorithm here I guess */
			  /* first skip dimensions of kill_ref similar to lhs_base_ref */
			  list lhs_dims_tmp = lhs_dims;
			  for(size_t i = 0; i < lhs_base_nb_dim; i++, POP(lhs_dims_tmp));
			  /* add the remaining dimensions to the copy of rhs_base_eff */
			  FOREACH(EXPRESSION, dim, lhs_dims_tmp)
			    {
			      (*effect_add_expression_dimension_func)(rhs_eff, dim);
			    }
			}
		      free_type(rhs_type);
		    }
		  l_rhs_eff = CONS(EFFECT, rhs_eff, l_rhs_eff);
		  l_rhs_kind = CONS(CELL_INTERPRETATION, rhs_kind, l_rhs_kind);
		  POP(l_rhs_base_kind_tmp);
		}

	      single_pointer_assignment_to_post_pv(lhs_eff, l_rhs_eff, l_rhs_kind, declaration_p, l_in_cur, pv_res, ctxt);

	      gen_full_free_list(l_rhs_eff);
	      if (free_rhs_kind) gen_full_free_list(l_rhs_kind); else gen_free_list(l_rhs_kind);

	      list l_out = pv_res->l_out;
	      if (l_out != l_in_cur)
		gen_full_free_list(l_in_cur);
	      l_in_cur = l_out;

	      list l_result_paths = pv_res->result_paths;

	      if (gen_length(l_result_paths) > 0 && anywhere_effect_p(EFFECT(CAR(l_result_paths))))
		anywhere_lhs_p = true;

	      POP(l_lhs_tmp);
	    } /* while */
	} /* if (!ENDP(l_lhs)) */
      gen_full_free_list(l_lhs);
    } /* if (!anywhere_lhs_p) */

  return;
}


/*
  @brief computes the gen, post and kill pointer values of an assignment
  @param lhs is the left hand side expression of the assignment*
  @param may_lhs_p is true if it's only a possible assignment
  @param rhs is the right hand side of the assignement
  @param l_in is a list of the input pointer values
  @param ctxt gives the functions specific to the kind of pointer values to be
          computed.
 */
void assignment_to_post_pv(expression lhs, bool may_lhs_p,
			   expression rhs, bool declaration_p,
			   list l_in, pv_results *pv_res, pv_context *ctxt)
{
  list l_in_cur = NIL;
  effect lhs_eff = effect_undefined;

  pips_debug(1, "begin\n");

  type lhs_type = expression_to_type(lhs);

  /* first convert the rhs and lhs into memory paths, rhs is evaluated first */
  /* this is done even if this is a non-pointer assignment, becasue there
     maybe side effects on alising hidden in sub-expressions, function calls...
  */
  pv_results lhs_pv_res = make_pv_results();
  pv_results rhs_pv_res = make_pv_results();

  expression_to_post_pv(rhs, l_in, &rhs_pv_res, ctxt);
  list l_rhs_eff = rhs_pv_res.result_paths;
  list l_rhs_kind = rhs_pv_res.result_paths_interpretations;
  l_in_cur = rhs_pv_res.l_out;

  expression_to_post_pv(lhs, l_in_cur, &lhs_pv_res, ctxt);
  l_in_cur = lhs_pv_res.l_out;
  /* we should test here that lhs_pv_res.result_paths has only one element.
     well is it correct? can a relational operator expression be a lhs ?
  */
  lhs_eff = EFFECT(CAR(lhs_pv_res.result_paths));
  if (may_lhs_p) effect_to_may_effect(lhs_eff);
  pv_res->result_paths = CONS(EFFECT, copy_effect(lhs_eff), NIL);
  pv_res->result_paths_interpretations = CONS(CELL_INTERPRETATION,
					      make_cell_interpretation_value_of(), NIL);

  if (type_fundamental_basic_p(lhs_type) || !type_leads_to_pointer_p(lhs_type))
    {
      pips_debug(2, "non-pointer assignment\n");
      /* l_gen = NIL; l_kill = NIL; */
      pv_res->l_out = l_in_cur;
    }
  else
    {
      if(type_variable_p(lhs_type))
	{
	  if (pointer_type_p(lhs_type)) /* simple case first: lhs is a pointer */
	    {
	      single_pointer_assignment_to_post_pv(lhs_eff, l_rhs_eff,
						       l_rhs_kind,
						       declaration_p, l_in_cur,
						       pv_res, ctxt);
	    }
	  else /* hidden pointer assignments */
	    {
	      multiple_pointer_assignment_to_post_pv(lhs_eff, lhs_type, l_rhs_eff,
							 l_rhs_kind,
							 declaration_p, l_in_cur,
							 pv_res, ctxt);
	    }
	} /* if (type_variable_p(lhs_type) */
      else if(type_functional_p(lhs_type))
	{
	  pips_internal_error("not yet implemented");
	}
      else
	pips_internal_error("unexpected_type");
    }


  free_pv_results_paths(&lhs_pv_res);
  free_pv_results_paths(&rhs_pv_res);

  pips_debug_pv_results(1, "end with pv_res =\n", *pv_res);
  return;
}


/**
   @brief generic interface to compute the pointer values of a given module
   @param module_name is the name of the module
   @param ctxt gives the functions specific to the kind of pointer values to be
          computed.
 */
static void generic_module_pointer_values(char * module_name, pv_context *ctxt)
{
  list l_out;

  /* temporary settings : in an interprocedural context we need to keep track
     of visited modules */
  /* Get the code of the module. */
  set_current_module_statement( (statement)
				db_get_memory_resource(DBR_CODE, module_name, TRUE));
  set_current_module_entity(module_name_to_entity(module_name));
  init_pv();
  //  init_gen_pv();
  //  init_kill_pv();

  debug_on("POINTER_VALUES_DEBUG_LEVEL");
  pips_debug(1, "begin\n");

  l_out = statement_to_post_pv(get_current_module_statement(), NIL, ctxt);

  (*ctxt->db_put_pv_func)(module_name, get_pv());
  //(*ctxt->db_put_gen_pv_func)(module_name, get_gen_pv());
  //(*ctxt->db_put_kill_pv_func)(module_name, get_kill_pv());

  pips_debug(1, "end\n");
  debug_off();
  reset_current_module_entity();
  reset_current_module_statement();

  reset_pv();
  //  reset_gen_pv();
  //  reset_kill_pv();

  return;
}

/**************** INTERFACE *************/

/**
   @brief interface to compute the simple pointer values of a given module
 */
bool simple_pointer_values(char * module_name)
{
  pv_context ctxt = make_simple_pv_context();
  set_methods_for_simple_pointer_effects();
  generic_module_pointer_values(module_name, &ctxt);
  reset_pv_context(&ctxt);
  generic_effects_reset_all_methods();
  return(TRUE);
}


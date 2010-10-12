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

static statement_cell_relations db_get_simple_gen_pv(char * module_name)
{
  return (statement_cell_relations) db_get_memory_resource(DBR_SIMPLE_GEN_POINTER_VALUES, module_name, TRUE);
}

static void db_put_simple_gen_pv(char * module_name, statement_cell_relations scr)
{
   DB_PUT_MEMORY_RESOURCE(DBR_SIMPLE_GEN_POINTER_VALUES, module_name, (char*) scr);
}

static statement_effects db_get_simple_kill_pv(char * module_name)
{
  return (statement_effects) db_get_memory_resource(DBR_SIMPLE_KILL_POINTER_VALUES, module_name, TRUE);
}

static void db_put_simple_kill_pv(char * module_name, statement_effects se)
{
   DB_PUT_MEMORY_RESOURCE(DBR_SIMPLE_KILL_POINTER_VALUES, module_name, (char*) se);
}


/******************** ANALYSIS CONTEXT */


pv_context make_simple_pv_context()
{
  pv_context ctxt;
  
  ctxt.db_get_pv_func = db_get_simple_pv;
  ctxt.db_put_pv_func = db_put_simple_pv;
  ctxt.db_get_gen_pv_func = db_get_simple_gen_pv;
  ctxt.db_put_gen_pv_func = db_put_simple_gen_pv;
  ctxt.db_get_kill_pv_func = db_get_simple_kill_pv;
  ctxt.db_put_kill_pv_func = db_put_simple_kill_pv;
  ctxt.make_pv_from_effects_func = make_simple_pv_from_simple_effects;
  ctxt.cell_reference_with_value_of_cell_reference_translation_func = 
    simple_cell_reference_with_value_of_cell_reference_translation;
  ctxt.cell_reference_with_address_of_cell_reference_translation_func = 
    simple_cell_reference_with_address_of_cell_reference_translation;
  ctxt.pv_composition_with_transformer_func = simple_pv_composition_with_transformer;
  ctxt.pvs_must_union_func = simple_pvs_must_union;
  ctxt.pvs_may_union_func = simple_pvs_may_union;
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
  p_ctxt->db_get_gen_pv_func =(statement_cell_relations_function) UNDEF ;
  p_ctxt->db_put_gen_pv_func = (void_function) UNDEF;
  p_ctxt->db_get_kill_pv_func = (statement_effects_function) UNDEF;
  p_ctxt->db_put_kill_pv_func = (void_function) UNDEF;
  p_ctxt->make_pv_from_effects_func = (cell_relation_function) UNDEF;
  p_ctxt->pv_composition_with_transformer_func = (cell_relation_function) UNDEF;
  p_ctxt->pvs_must_union_func = (list_function) UNDEF;
  p_ctxt->pvs_may_union_func = (list_function) UNDEF;
}


/******************** CONVERSION FROM EXPRESSIONS TO PATHS AND CELL_INTERPRETATION */

static effect intrinsic_to_interpreted_path(entity func, list args, cell_interpretation * ci, pv_context * ctxt);
static effect call_to_interpreted_path(call c, cell_interpretation * ci, pv_context * ctxt);
static 
effect expression_to_interpreted_path(expression exp, cell_interpretation * ci, pv_context * ctxt);


static effect intrinsic_to_interpreted_path(entity func, list args, cell_interpretation * ci, pv_context * ctxt)
{
  effect eff = effect_undefined;
  
  if(ENTITY_ASSIGN_P(func))
    /* if the call is an assignement, return the lhs path because the relation
       between the lhs and the rhs has been treated elsewhere
    */
    return(expression_to_interpreted_path(EXPRESSION(CAR(args)), ci, ctxt));

  if(ENTITY_DEREFERENCING_P(func))
    {
      eff = expression_to_interpreted_path(EXPRESSION(CAR(args)), ci, ctxt);
      effect_add_dereferencing_dimension(eff);
      *ci = make_cell_interpretation_value_of();
      return eff;
    }

  if(ENTITY_FIELD_P(func))
    {
      expression e2 = EXPRESSION(CAR(CDR(args)));
      syntax s2 = expression_syntax(e2);
      reference r2 = syntax_reference(s2);
      entity f = reference_variable(r2);

      pips_assert("e2 is a reference", syntax_reference_p(s2));
      pips_debug(4, "It's a field operator\n");

      eff = expression_to_interpreted_path(EXPRESSION(CAR(args)), ci, ctxt);
      effect_add_field_dimension(eff,f);
      *ci = make_cell_interpretation_value_of();
      return eff;
    }

  if(ENTITY_POINT_TO_P(func))
    {
      expression e2 = EXPRESSION(CAR(CDR(args)));
      syntax s2 = expression_syntax(e2);
      entity f;

      pips_assert("e2 is a reference", syntax_reference_p(s2));
      f = reference_variable(syntax_reference(s2));

      pips_debug(4, "It's a point to operator\n");
      eff = expression_to_interpreted_path(EXPRESSION(CAR(args)), ci, ctxt);

      /* We add a dereferencing */
      effect_add_dereferencing_dimension(eff);

      /* we add the field dimension */
      effect_add_field_dimension(eff,f);
      *ci = make_cell_interpretation_value_of();
      return eff;
    }

  if(ENTITY_ADDRESS_OF_P(func))
    {
      eff = expression_to_interpreted_path(EXPRESSION(CAR(args)), ci, ctxt);
      *ci = make_cell_interpretation_address_of();
      return eff;      
    }
  
  return eff;
}

static effect call_to_interpreted_path(call c, cell_interpretation * ci, pv_context * ctxt)
{
  effect eff = effect_undefined;

  entity func = call_function(c);
  value func_init = entity_initial(func);
  tag t = value_tag(func_init);
  string n = module_local_name(func);
  list func_args = call_arguments(c);
  type uet = ultimate_type(entity_type(func));
  
  if(type_functional_p(uet)) 
    {
      switch (t) 
	{
	case is_value_code:
	  pips_debug(5, "external function %s\n", n);
	  /* If it's an external call, we should try to retrieve the returned value 
	     if it's a pointer. 
	     for the moment, just return an anywhere effect....
	  */
	  pips_user_warning("external call, not handled yet, returning all locations effect\n");
	  eff = make_anywhere_effect(make_action_write_memory());
	  *ci = make_cell_interpretation_address_of();		  
	  break;
      
	case is_value_intrinsic:
	  pips_debug(5, "intrinsic function %s\n", n);
	  eff = intrinsic_to_interpreted_path(func, func_args, ci, ctxt);
	  break;
      
	case is_value_symbolic:
	  pips_debug(5, "symbolic\n");
	  pips_internal_error("symbolic case, not yet implemented\n", entity_name(func));
	  break;
	    
	case is_value_constant:
	  pips_debug(5, "constant\n");
	  constant func_const = value_constant(func_init);
	  /* We should be here only in case of a pointer value rhs, and the value should be 0 */
	  if (constant_int_p(func_const))
	    {
	      if (constant_int(func_const) == 0)
		{
		  *ci = make_cell_interpretation_value_of();
		  cell path = make_null_pointer_value_cell();
		  /* use approximation_must to be consistent with effects, should be approximation_exact */
		  eff = make_effect(path, make_action_read_memory(), make_approximation_must(), make_descriptor_none());
		}
	      else
		pips_internal_error("unexpected integer constant value\n", entity_name(func));
	    }
	  else
	    pips_internal_error("unexpected constant case\n", entity_name(func));
	  break;
	    
	case is_value_unknown:
	  pips_internal_error("unknown function %s\n", entity_name(func));
	    
	default:
	  pips_internal_error("unknown tag %d\n", t);
	  break;
	}
    }
  return eff;
}

static 
effect expression_to_interpreted_path(expression exp, cell_interpretation * ci, 
				      pv_context * ctxt)
{
  effect eff = effect_undefined;
  *ci = cell_interpretation_undefined;

  if (expression_undefined_p(exp))
    {
      eff = make_effect(make_undefined_pointer_value_cell(), 
			make_action_write_memory(), 
			make_approximation_must(), 
			make_descriptor_none());  
      *ci = make_cell_interpretation_value_of();
    }
  else
    {
      pips_debug(1, "begin with_expression : %s\n", 
		 words_to_string(words_expression(exp,NIL)));

      syntax exp_syntax = expression_syntax(exp);

      switch(syntax_tag(exp_syntax))
	{
	case is_syntax_reference:
	  pips_debug(5, "reference case\n");
	  reference exp_ref = syntax_reference(exp_syntax);
	  if (same_string_p(entity_local_name(reference_variable(exp_ref)), "NULL"))
	    {
	      *ci = make_cell_interpretation_value_of();
	      cell path = make_null_pointer_value_cell();
	      /* use approximation_must to be consistent with effects, should be approximation_exact */
	      eff = make_effect(path, make_action_read_memory(), make_approximation_must(), make_descriptor_none());
	    }
	  else
	    {
	      /* this function name should be stored in ctxt*/
	      eff = (*reference_to_effect_func)(copy_reference(exp_ref), 
						make_action_write_memory(), false);
	      *ci = make_cell_interpretation_value_of();
	    }
	  break;

	case is_syntax_range:
	  pips_debug(5, "range case\n");
	  pips_internal_error("not yet implemented\n");
	  break;

	case is_syntax_call:
	  pips_debug(5, "call case\n");
	  eff = call_to_interpreted_path(syntax_call(exp_syntax), ci, ctxt);
	  break;

	case is_syntax_cast:
	  pips_debug(5, "cast case\n");
	  eff = expression_to_interpreted_path(cast_expression(syntax_cast(exp_syntax)), 
					       ci, ctxt);
	  break;

	case is_syntax_sizeofexpression:
	  pips_debug(5, "sizeof case\n");
	  pips_internal_error("sizeof not expected\n");
	  break;
    
	case is_syntax_subscript:
	  pips_debug(5, "subscript case\n");
	  subscript sub = syntax_subscript(exp_syntax);
	  eff = expression_to_interpreted_path(subscript_array(sub), ci, ctxt);

	  FOREACH(EXPRESSION, sub_ind_exp, subscript_indices(sub))
	    {
	      (*effect_add_expression_dimension_func)(eff, sub_ind_exp);
	    }
	  list l_tmp = generic_proper_effects_of_complex_address_expression(exp, &eff, true);
	  *ci = make_cell_interpretation_value_of();
	  gen_full_free_list(l_tmp);	
	  break;

	case is_syntax_application:
	  pips_debug(5, "application case\n");		
	  pips_internal_error("not yet implemented\n");
	  break;

	case is_syntax_va_arg:
	  pips_debug(5, "va_arg case\n");		
	  pips_internal_error("va_arg not expected\n");
	  break;
      
	default:
	  pips_internal_error("unexpected tag %d\n", syntax_tag(exp_syntax));
	}
    }
  pips_debug_effect(2, "returning path :",eff);
  pips_debug(2, "with %s interpretation\n", 
	     cell_interpretation_value_of_p(*ci)? "value_of": "address_of");
  pips_debug(1,"end\n");
  return eff;
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
list expression_to_post_pv(expression exp, list l_in, pv_context *ctxt);

static
list call_to_post_pv(call c, list l_in, pv_context *ctxt);

static
list intrinsic_to_post_pv(entity func, list func_args, list l_in, pv_context *ctxt);

static 
list assignment_to_post_pv(expression lhs, expression rhs, list l_in, pv_context *ctxt);



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

	pips_debug(2, "dealing with statement ");
	print_statement(stmt);
	pips_debug_pvs(2, "l_cur = \n", l_cur);
      }
      /* keep local variables in declaration reverse order */
      if (declaration_statement_p(stmt))
	{
	  store_pv(stmt, make_cell_relations(gen_full_copy_list(l_cur)));
	  FOREACH(ENTITY, e, statement_declarations(stmt))
	    {
	      /* beware don't push static variables */
	      l_locals = CONS(ENTITY, e, l_locals);
	      l_cur = declaration_to_post_pv(e, l_cur, ctxt);
	    }
	  store_gen_pv(stmt, make_cell_relations(NIL));
	  store_kill_pv(stmt, make_effects(NIL));
	  
	} 
      else
	l_cur = statement_to_post_pv(stmt, l_cur, ctxt);

    }
  
  /* don't forget to eliminate local declarations on exit */
  /* ... */
  pips_debug_pvs(2, "returning: ", l_cur);
  pips_debug(1, "end\n");
  return (l_cur);
}

static 
list statement_to_post_pv(statement stmt, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");
  pips_debug_pvs(2, "input pvs: ", l_in);

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

  pips_debug_pvs(2, "before composition_with_transformer: ", l_out);
  l_out = pvs_composition_with_transformer(l_out, transformer_undefined, ctxt);

  store_gen_pv(stmt, make_cell_relations(NIL));
  store_kill_pv(stmt, make_effects(NIL));
  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");

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
  pips_debug(1, "begin\n");

  expression lhs_exp = entity_to_expression(e);
  expression rhs_exp = expression_undefined;
  bool free_rhs_exp = false;

  value v_init = entity_initial(e);
  if (v_init == value_undefined)
    {
      pips_debug(2, "undefined inital value\n");
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
	  pips_internal_error("unexpected tag\n");
	}
    }

  l_out = assignment_to_post_pv(lhs_exp, rhs_exp, l_in, ctxt);
  
  free_expression(lhs_exp);
  if (free_rhs_exp) free_expression(rhs_exp);
  


  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
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
      l_out = expression_to_post_pv(instruction_expression(inst), l_in, ctxt);
      break;
    case is_instruction_call:
      l_out = call_to_post_pv(instruction_call(inst), l_in, ctxt);
      break;
    case is_instruction_goto:
      pips_internal_error("unexpected goto in pointer values analyses\n");
      break;
    case is_instruction_multitest:
      pips_internal_error("unexpected multitest in pointer values analyses\n");
      break;
    default:
      pips_internal_error("unknown instruction tag\n");
    }
  pips_debug_pvs(2, "returning: ", l_out);
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

  list l_in_branches = expression_to_post_pv(t_cond, l_in, ctxt);
  
  list l_out_true = statement_to_post_pv(t_true, l_in_branches, ctxt);
  list l_out_false = statement_to_post_pv(t_false, l_in_branches, ctxt);

  l_out = (*ctxt->pvs_may_union_func)(l_out_true, l_out_false);

  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

static
list loop_to_post_pv(loop l, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");
  pips_internal_error("not yet implemented\n");
  pips_debug(1, "end\n");
  return (l_out);
}

static
list whileloop_to_post_pv(whileloop l, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");
  pips_internal_error("not yet implemented\n");
  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

static
list forloop_to_post_pv(forloop l, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");
  pips_internal_error("not yet implemented\n");
  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}


static
list unstructured_to_post_pv(unstructured u, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_internal_error("not yet implemented\n");
   pips_debug_pvs(2, "returning: ", l_out);
 pips_debug(1, "end\n");
  return (l_out);
}

static
list expression_to_post_pv(expression exp, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin\n");
  syntax s = expression_syntax(exp);

  switch(syntax_tag(s))
    {
    case is_syntax_reference:
      pips_internal_error("not yet implemented\n");
      break;
    case is_syntax_range:
      pips_internal_error("not yet implemented\n");
      break;
    case is_syntax_call:
      {
	l_out = call_to_post_pv(syntax_call(s), l_in, ctxt);
	break;
      }
    case is_syntax_cast:
      {
	pips_internal_error("not yet implemented\n");
	break;
      }
    case is_syntax_sizeofexpression:
      {
	pips_internal_error("not yet implemented\n");
	break;
      }
    case is_syntax_subscript:
      {
	pips_internal_error("not yet implemented\n");
	break;
      }
    case is_syntax_application:
      pips_internal_error("not yet implemented\n");
      break;
    case is_syntax_va_arg:
      {
	pips_internal_error("not yet implemented\n");
	break;
      }
    default:
      pips_internal_error("unexpected tag %d\n", syntax_tag(s));
    }

  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

static
list call_to_post_pv(call c, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  entity func = call_function(c);
  tag t = value_tag(entity_initial(func));
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
	  
	  pips_internal_error("not yet implemented\n");
	  break;
	  
	case is_value_intrinsic:
	  pips_debug(5, "intrinsic function\n");
	  l_out = intrinsic_to_post_pv(func, func_args, l_in, ctxt);
	  break;
	  
	case is_value_symbolic:
	  pips_debug(5, "symbolic\n");
	  l_out = l_in;
	  break;
	  
	case is_value_constant:
	  pips_debug(5, "constant\n");
	  l_out = l_in;
	  break;
	  
	case is_value_unknown:
	  pips_internal_error("unknown function \n");
	  break;
	  
	default:
	  pips_internal_error("unknown tag %d\n", t);
	  break;
	}
    }
  else if(type_variable_p(func_type)) 
    {
      pips_internal_error("not yet implemented\n");
    }
  else if(type_statement_p(func_type)) 
    {
      pips_internal_error("not yet implemented\n");
    }
  else 
    {
      pips_internal_error("Unexpected case\n");
    }

  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}

static
list intrinsic_to_post_pv(entity func, list func_args, list l_in, pv_context *ctxt)
{
  list l_out = NIL;
  pips_debug(1, "begin for %s\n", entity_local_name(func));

  /* only few intrinsics are currently handled : we should have a way to 
     describe the effects on aliasing of all intrinsics.
  */

  if (ENTITY_ASSIGN_P(func)) 
    {
      expression lhs = EXPRESSION(CAR(func_args));
      expression rhs = EXPRESSION(CAR(CDR(func_args)));
      l_out = assignment_to_post_pv(lhs, rhs, l_in, ctxt);
    }
  else if((ENTITY_STOP_P(func) || ENTITY_ABORT_SYSTEM_P(func)
	   || ENTITY_EXIT_SYSTEM_P(func))) 
    {
      /* The call is never returned from. No information is available
	 for the dead code that follows.
      */
      l_out = NIL;
    }
  else if (ENTITY_C_RETURN_P(func))
    {
      /* but we have to evaluate the impact
	 of the argument evaluation on pointer values
	 eliminate local variables, retrieve the value of the returned pointer if any...
      */
      l_out = expression_to_post_pv(EXPRESSION(CAR(func_args)), l_in, ctxt);
    }
  else
    pips_internal_error("not yet implemented\n");

  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
}



/*
  @brief computes the gen, post and kill pointer values of an assignment 
  @param lhs is the left hand side expression of the assignment
  @param rhs is the right hand side of the assignement
  @param l_in is a list of the input pointer values
  @param ctxt gives the functions specific to the kind of pointer values to be
          computed. 
 */
static 
list assignment_to_post_pv(expression lhs, expression rhs, list l_in, pv_context *ctxt) 
{
  list l_out = NIL;
  list l_aliased = NIL;
  list l_kill = NIL;
  list l_gen = NIL;
  effect lhs_eff = effect_undefined;
  cell_interpretation lhs_kind;
  effect rhs_eff = effect_undefined;
  cell_interpretation rhs_kind;
  pips_debug(1, "begin\n");

  type lhs_type = expression_to_type(lhs);

  if (type_fundamental_basic_p(lhs_type) || !type_leads_to_pointer_p(lhs_type))
    {
      pips_debug(2, "non-pointer assignment \n");
    }
  else
    {
		    

      if(type_variable_p(lhs_type))
	{
	  bool anywhere_lhs_p = false;

	  /* first convert the rhs and lhs into memory paths */
	  rhs_eff = expression_to_interpreted_path(rhs, &rhs_kind, ctxt);
	  lhs_eff = expression_to_interpreted_path(lhs, &lhs_kind, ctxt);
	  free_cell_interpretation(lhs_kind);
      
	  /* we could be more precise here on abstract locations */
	  if (anywhere_effect_p(lhs_eff))
	    {
	      pips_debug(3, "anywhere lhs\n");
	      anywhere_lhs_p = true;
	      l_kill = CONS(EFFECT, copy_effect(lhs_eff), NIL);
	    }
	  
	  /* find all pointers that are or may be defined through this assignment */
	  if (pointer_type_p(lhs_type))
	    {
	      /* simple case first: lhs is a pointer */
	      if (!anywhere_lhs_p)
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
		      if (effect_may_p(lhs_eff)) effects_to_may_effects(l_aliased);
		      l_kill = CONS(EFFECT, lhs_eff, l_aliased);
		      pips_debug_effects(2, "pointer case, l_kill = ", l_kill);		      
		    }
		}

	      if (anywhere_lhs_p)
		{
		  /* we must find in l_in all pointers p and generate p == rhs */
		  FOREACH(CELL_RELATION, pv_in, l_in)
		    {
		      if (cell_relation_second_address_of_p(pv_in) 
			  || undefined_pointer_value_cell_p(cell_relation_second_cell(pv_in))
			  || null_pointer_value_cell_p(cell_relation_second_cell(pv_in)) )
			{
			  /* not generic */
			  effect eff_alias = make_effect(copy_cell(cell_relation_first_cell(pv_in)),
							 make_action_write_memory(),
							 make_approximation_may(),
							 make_descriptor_none());
			  cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
			    (eff_alias, rhs_eff, rhs_kind);
			  l_gen = CONS(CELL_RELATION, gen_pv, l_gen);
			  free_effect(eff_alias);
			}
		    }
		}
	      else
		{
		  FOREACH(EFFECT, eff_alias, l_kill)
		    {
		      cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
			(eff_alias, rhs_eff, rhs_kind);
		      l_gen = CONS(CELL_RELATION, gen_pv, l_gen);	      
		    }
		  pips_debug_pvs(2, "l_gen = ", l_gen);
		  free_effect(rhs_eff);
		  free_cell_interpretation(rhs_kind);
		}
	      
	    }
	  else 
	    {
	      if (!anywhere_lhs_p)
		{
		  /* lhs is not a pointer, but it is an array of pointers or an aggregate type 
		     with pointers.... */
		  /* In this case, it cannot be an address_of case */
		  list l_lhs = generic_effect_generate_all_accessible_paths_effects_with_level
		    (lhs_eff, lhs_type, is_action_write, false, 0, true);
		  if(effect_must_p(lhs_eff))
		    effects_to_must_effects(l_lhs);
		  pips_debug_effects(2, "l_lhs = ", l_lhs);
		  
		  if (!ENDP(l_lhs))
		    {	      
		      pips_assert("pointer assignement through arrays or aggregate types"
				  " requires a value_of rhs kind\n", 
				  cell_interpretation_value_of_p(rhs_kind));
		      cell rhs_cell = effect_cell(rhs_eff);
		      
		      if(null_pointer_value_cell_p(rhs_cell))
			{
			  pips_internal_error("assigning NULL to several pointers"
					      " at the same time!\n");
			}
		      else if (undefined_pointer_value_cell_p(rhs_cell) || 
			       (anywhere_effect_p(rhs_eff)))
			{
			  list l_lhs_tmp = l_lhs;
			  while (!anywhere_lhs_p && !ENDP(l_lhs_tmp))
			    {
			      effect eff = EFFECT(CAR(l_lhs_tmp));
			      l_aliased = effect_find_aliased_paths_with_pointer_values(eff, l_in, ctxt);
			      if (!ENDP(l_aliased) && anywhere_effect_p(EFFECT(CAR(l_aliased))))
				{
				  pips_debug(3, "anywhere lhs (from aliases)\n");
				  anywhere_lhs_p = true;
				  gen_full_free_list(l_kill);
				  l_kill = l_aliased;
				  gen_full_free_list(l_gen);
				  /* we must find in l_in all pointers p and generate p == rhs */
				  effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
				  cell_interpretation ci = make_cell_interpretation_address_of();
				  FOREACH(CELL_RELATION, pv_in, l_in)
				    {
				      if (cell_relation_second_address_of_p(pv_in) 
					  || undefined_pointer_value_cell_p(cell_relation_second_cell(pv_in))
					  || null_pointer_value_cell_p(cell_relation_second_cell(pv_in)) )
					{
					  /* not generic */
					  effect eff_alias = make_effect
					    (copy_cell(cell_relation_first_cell(pv_in)),
					     make_action_write_memory(),
					     make_approximation_may(),
					     make_descriptor_none());
					  cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
					    (eff_alias, anywhere_eff, ci);
					  l_gen = CONS(CELL_RELATION, gen_pv, l_gen);
					  free_effect(eff_alias);
					}
				    }
				  free_effect(anywhere_eff);
				  free_cell_interpretation(ci);
				}
			      else
				{
				  /* if eff is a may-be-killed, then all aliased effects are also 
				     may-be-killed effects */
				  if (effect_may_p(eff))
				    effects_to_may_effects(l_aliased);
				  l_aliased = CONS(EFFECT, eff, l_aliased);
				  l_kill = gen_nconc(l_kill, l_aliased);
				  pips_debug_effects(2, "pointer case, l_aliased = ", l_aliased);
				  FOREACH(EFFECT, eff_alias, l_aliased)
				    {
				      cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
					(eff_alias, rhs_eff, rhs_kind);
				      l_gen = CONS(CELL_RELATION, gen_pv, l_gen);	      
				    }
				}
			      POP(l_lhs_tmp);
			    } /* while */
			}
		      else
			{
			  reference rhs_ref = effect_any_reference(rhs_eff);
			  type rhs_type = cell_reference_to_type(rhs_ref);
			  
			  if (type_equal_p(lhs_type, rhs_type))
			    {
			      reference lhs_ref = effect_any_reference(lhs_eff);
			      size_t lhs_nb_dim = gen_length(reference_indices(lhs_ref));
			      
			      list l_lhs_tmp = l_lhs;
			      while (!anywhere_lhs_p && !ENDP(l_lhs_tmp))
				{
				  effect eff = EFFECT(CAR(l_lhs_tmp));
				  reference ref = effect_any_reference(eff);
				  list dims = reference_indices(ref);
				  effect new_rhs_eff = copy_effect(rhs_eff);
				  
				  /* first skip dimensions of kill_ref similar to lhs_ref */
				  
				  for(size_t i = 0; i < lhs_nb_dim; i++, POP(dims));
				  
				  /* add the remaining dimensions to the copy of rhs_eff */
				  
				  for(; !ENDP(dims); POP(dims))
				    {
				      expression dim = EXPRESSION(CAR(dims));
				      (*effect_add_expression_dimension_func)(new_rhs_eff, dim);
				      
				    }
				  
				  /* find aliases */
				  l_aliased = effect_find_aliased_paths_with_pointer_values(eff, l_in, ctxt);
				  if (!ENDP(l_aliased) && anywhere_effect_p(EFFECT(CAR(l_aliased))))
				    {
				      pips_debug(3, "anywhere lhs (from aliases)\n");
				      anywhere_lhs_p = true;
				      gen_full_free_list(l_kill);
				      l_kill = l_aliased;
				      gen_full_free_list(l_gen);
				      /* we must find in l_in all pointers p and generate p == rhs */
				      effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
				      cell_interpretation ci = make_cell_interpretation_address_of();
				      FOREACH(CELL_RELATION, pv_in, l_in)
					{
					  if (cell_relation_second_address_of_p(pv_in) 
					      || undefined_pointer_value_cell_p(cell_relation_second_cell(pv_in))
					      || null_pointer_value_cell_p(cell_relation_second_cell(pv_in)) )
					    {
					      /* not generic */
					      effect eff_alias = make_effect
						(copy_cell(cell_relation_first_cell(pv_in)),
						 make_action_write_memory(),
						 make_approximation_may(),
						 make_descriptor_none());
					      cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
						(eff_alias, anywhere_eff, ci);
					      l_gen = CONS(CELL_RELATION, gen_pv, l_gen);
					      free_effect(eff_alias);
					    }
					}
				      free_effect(anywhere_eff);
				      free_cell_interpretation(ci);
				    }
				  else
				    {
				      /* if eff is a may-be-killed, then all aliased effects 
					 are also may-be-killed effects */
				      if (effect_may_p(eff))
					effects_to_may_effects(l_aliased);
				      l_aliased = CONS(EFFECT, eff, l_aliased);
				      l_kill = gen_nconc(l_kill, l_aliased);
				      pips_debug_effects(2, "pointer case, l_aliased = ", l_aliased);
				      
				      /* Then build the pointer_value relation */
				      FOREACH(EFFECT, eff_alias, l_aliased)
					{
					  cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
					    (eff_alias, new_rhs_eff, rhs_kind);
					  l_gen = CONS(CELL_RELATION, gen_pv, l_gen);
					}
				      free_effect(new_rhs_eff);
				    }
				  POP(l_lhs_tmp);
				} /* while */
			    }
			  else
			    {
			      pips_internal_error("not same lhs and rhs types, not yet supported."
						  " Please report.\n");
			    }
			  
			  free_type(rhs_type);
			}
		      
		      free_effect(rhs_eff); 
		      free_cell_interpretation(rhs_kind);

		    } /* if (!ENDP(l_lhs)) */
		}
	      else
		{
		  /* we must find in l_in all pointers p and generate p == &*anywhere* */
		  effect anywhere_eff = make_anywhere_effect(make_action_write_memory());
		  cell_interpretation ci = make_cell_interpretation_address_of();
		  FOREACH(CELL_RELATION, pv_in, l_in)
		    {
		      if (cell_relation_second_address_of_p(pv_in) 
			  || undefined_pointer_value_cell_p(cell_relation_second_cell(pv_in))
			  || null_pointer_value_cell_p(cell_relation_second_cell(pv_in)) )
			{
			  /* not generic */
			  effect eff_alias = make_effect(copy_cell(cell_relation_first_cell(pv_in)),
							 make_action_write_memory(),
							 make_approximation_may(),
							 make_descriptor_none());
			  cell_relation gen_pv = (* ctxt->make_pv_from_effects_func)
			    (eff_alias, anywhere_eff, ci);
			  l_gen = CONS(CELL_RELATION, gen_pv, l_gen);
			  free_effect(eff_alias);
			}
		      free_effect(anywhere_eff);
		    }
		}	
	    }

	  
	  
	} /* if (type_variable_p(lhs_type) */
      else if(type_functional_p(lhs_type))
	{
	  pips_internal_error("not yet implemented\n");
	}
      else
	pips_internal_error("unexpected_type\n");
    }

  /* now take kills into account */
  l_out = kill_pointer_values(l_in, l_kill, ctxt);
  pips_debug_pvs(2, "l_out_after kill: ", l_out);

  /* and add gen */
  list l_tmp = (*ctxt->pvs_must_union_func)(l_out, l_gen);
  //gen_full_free_list(l_out);
  //gen_full_free_list(l_gen);
  l_out = l_tmp;
  free_type(lhs_type);
  pips_debug_pvs(2, "returning: ", l_out);
  pips_debug(1, "end\n");
  return (l_out);
  
}

/* 
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
  init_gen_pv();
  init_kill_pv();

  debug_on("POINTER_VALUES_DEBUG_LEVEL");
  pips_debug(1, "begin\n");
  
  l_out = statement_to_post_pv(get_current_module_statement(), NIL, ctxt);

  (*ctxt->db_put_pv_func)(module_name, get_pv());
  (*ctxt->db_put_gen_pv_func)(module_name, get_gen_pv());
  (*ctxt->db_put_kill_pv_func)(module_name, get_kill_pv());

  pips_debug(1, "end\n");
  debug_off();
  reset_current_module_entity();
  reset_current_module_statement();

  reset_pv();
  reset_gen_pv();
  reset_kill_pv();

  return;
}

/**************** INTERFACE *************/

/* 
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


/**
 * General computation for PHRASE distribution
 * COMMUNICATION STUFFs
 */

#include <stdio.h>
#include <ctype.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"

#include "resources.h"

#include "misc.h"
#include "ri-util.h"
#include "pipsdbm.h"

#include "text-util.h"
#include "properties.h"
#include "prettyprint.h"


#include "dg.h"
#include "transformations.h"
#include "transformer.h"

typedef dg_arc_label arc_label;
typedef dg_vertex_label vertex_label;

#include "graph.h"

#include "ray_dte.h"
#include "sommet.h"
#include "sg.h"
#include "polyedre.h"
#include "ricedg.h"
#include "semantics.h"
#include "control.h"

#include "phrase_tools.h"

#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"

#include "sc.h"
#include "conversion.h"

#include "phrase_distribution.h"

/**
 * Build and store new module START_RU.
 * Create statement module_statement
 */
entity make_start_ru_module (hash_table ht_params, 
			     statement* module_statement, 
			     int number_of_deployment_units,
			     entity global_common,
			     list l_commons) 
{
  entity start_ru_module;
  entity func_id, unit_id;
  string function_name;
  entity called_module;
  statement called_module_stat;
  list stat_seq = NIL;
  sequence new_sequence;
  instruction sequence_instruction;
  entity set_entity = get_current_module_entity();
  
  start_ru_module = make_empty_subroutine(strdup(START_RU_MODULE_NAME));
  pips_debug(2, "Creating module %s\n", entity_global_name(start_ru_module));
  reset_current_module_entity();
  set_current_module_entity(start_ru_module);
  func_id = create_integer_parameter_for_new_module (START_RU_PARAM1_NAME,
						     START_RU_MODULE_NAME,
						     start_ru_module,
						     1);
  unit_id = create_integer_parameter_for_new_module (START_RU_PARAM2_NAME,
						     START_RU_MODULE_NAME,
						     start_ru_module,
						     2);
  
  /* Declare CONTROL_DATA common to be visible here */
  declare_common_variables_in_module (global_common, start_ru_module);

  /* Declare commons for all externalized functions to be visible here */
  MAP (ENTITY, com, {
    declare_common_variables_in_module (com, start_ru_module);
  },l_commons);

  ifdebug(7) {
    pips_debug(7, "Declarations for START_RU module: \n");
    fprint_environment(stderr, start_ru_module);
  }

  HASH_MAP (externalized_function, l_params, {
    
    entity function = (entity)externalized_function;
    expression test_condition;
    test new_test;
    instruction test_instruction;
    statement test_statement;
    statement call_statement;
    list call_params ;
      
    function_name = entity_local_name(function);
        
    called_module_stat = (statement) db_get_memory_resource(DBR_CODE, 
							    function_name, 
							    TRUE);
    
    called_module = local_name_to_top_level_entity(function_name);
  
    /* Compute the parameters of call function */
    call_params = NIL;

    /* Processing PARAMS regions */
    MAP (REGION, reg, {
      expression new_param;
      reference ref = region_reference(reg);
      entity local_variable 
	= entity_in_module (get_common_param_name(reference_variable(ref), function), start_ru_module);
      list indices = NIL;
      if (number_of_deployment_units > 1) {
	list primary_indices = variable_dimensions(type_variable(entity_type(local_variable)));
	expression to_be_replaced = NULL;
	MAP (DIMENSION, dim, {
	  indices = CONS (EXPRESSION, 
			  dimension_lower(dim),
			  indices);
	  to_be_replaced=dimension_lower(dim);
	}, primary_indices);
	gen_list_patch (indices,to_be_replaced,make_expression_from_entity(unit_id));
	indices = gen_nreverse(indices);
      }  
      new_param = make_entity_expression (local_variable, indices);
      call_params = CONS(EXPRESSION, new_param, call_params);
    }, (list)l_params);

    call_params = gen_nreverse(call_params);
    
    /* Make the CALL statement */
    call_statement = make_statement(entity_empty_label(),
				    STATEMENT_NUMBER_UNDEFINED,
				    STATEMENT_ORDERING_UNDEFINED,
				    empty_comments,
				    make_instruction(is_instruction_call,
						     make_call(called_module,call_params)),
				    NIL,NULL);

    test_condition 
      = MakeBinaryCall (entity_intrinsic(EQUAL_OPERATOR_NAME),
			make_expression_from_entity (func_id),
			make_expression_from_entity (entity_in_module (get_function_id_name(function), start_ru_module)));
    
    new_test = make_test (test_condition, call_statement, 
			  make_continue_statement(entity_undefined));
    
    test_instruction = make_instruction (is_instruction_test,new_test);

    test_statement = make_statement (entity_empty_label(),
				     STATEMENT_NUMBER_UNDEFINED,
				     STATEMENT_ORDERING_UNDEFINED,
				     empty_comments,
				     test_instruction,NIL,NULL);

    stat_seq = CONS (STATEMENT, test_statement, stat_seq);

  }, ht_params);
  
  stat_seq =  gen_nreverse(CONS(STATEMENT, make_return_statement(start_ru_module), stat_seq));

  new_sequence 
    = make_sequence (stat_seq);
  
  sequence_instruction 
    = make_instruction(is_instruction_sequence,
		       new_sequence);

  *module_statement = make_statement(entity_empty_label(),
				     STATEMENT_NUMBER_UNDEFINED,
				     STATEMENT_ORDERING_UNDEFINED,
				     empty_comments,
				     sequence_instruction,NIL,NULL);
  
  store_new_module (strdup(START_RU_MODULE_NAME), start_ru_module, *module_statement);

  reset_current_module_entity();
  set_current_module_entity(set_entity);
  return start_ru_module;
}

/**
 * Build and store new module WAIT_RU.
 * Create statement module_statement
 */
entity make_wait_ru_module (hash_table ht_params, 
			    statement* module_statement, 
			    int number_of_deployment_units,
			    entity global_common,
			    list l_commons) 
{
  entity wait_ru_module;
  entity func_id, unit_id;
  string function_name;
  entity set_entity = get_current_module_entity();

  wait_ru_module = make_empty_subroutine(strdup(WAIT_RU_MODULE_NAME));
  pips_debug(2, "Creating module %s\n", entity_global_name(wait_ru_module));
  reset_current_module_entity();
  set_current_module_entity(wait_ru_module);
  func_id = create_integer_parameter_for_new_module (WAIT_RU_PARAM1_NAME,
						     WAIT_RU_MODULE_NAME,
						     wait_ru_module,
						     1);
  unit_id = create_integer_parameter_for_new_module (WAIT_RU_PARAM2_NAME,
						     WAIT_RU_MODULE_NAME,
						     wait_ru_module,
						     2);
  
  /* Declare CONTROL_DATA common to be visible here */
  declare_common_variables_in_module (global_common, wait_ru_module);

  /* Declare commons for all externalized functions to be visible here */
  MAP (ENTITY, com, {
    declare_common_variables_in_module (com, wait_ru_module);
  },l_commons);

  ifdebug(7) {
    pips_debug(7, "Declarations for WAIT_RU module: \n");
    fprint_environment(stderr, wait_ru_module);
  }
  
  *module_statement = make_return_statement(wait_ru_module);
  
  store_new_module (strdup(WAIT_RU_MODULE_NAME), wait_ru_module, *module_statement);
  
  reset_current_module_entity();
  set_current_module_entity(set_entity);

  return wait_ru_module;
}

/**
 * Make communication statement (SEND or RECEIVE data), for a
 * given fonction and region
 */
static statement make_communication_statement (entity function,
					       entity module,
					       region reg,
					       entity unit_id,
					       entity param,
					       int number_of_deployment_units,
					       boolean is_receiving)
{
  entity local_entity = entity_in_module (get_common_param_name (region_entity(reg), function), module);
  
  ifdebug(2) {
    pips_debug(2, "BEGIN make_communication_statement\n");
    pips_debug(2, "Function: [%s]\n",entity_local_name(function));
    pips_debug(2, "Module: [%s]\n",entity_local_name(module));
    pips_debug(2, "Region: \n");
    print_region(reg);
    pips_debug(2, "Local entity: [%s]\n",entity_local_name(local_entity));
  }

  if (region_scalar_p(reg)) {
    if (number_of_deployment_units > 1) {
      list inds = CONS(EXPRESSION, make_expression_from_entity(unit_id),NIL);
      if (is_receiving) {
	return make_binary_call_statement (ASSIGN_OPERATOR_NAME,
					   make_entity_expression(local_entity, inds),
					   make_expression_from_entity(param),
					   NULL);
      }
      else {
	return make_binary_call_statement (ASSIGN_OPERATOR_NAME,
					   make_expression_from_entity(param),
					   make_entity_expression(local_entity, inds),
					   NULL);
      }
    }
    else {
      if (is_receiving) {
	return make_binary_call_statement (ASSIGN_OPERATOR_NAME,
					   make_expression_from_entity(local_entity),
					   make_expression_from_entity(param),
					   NULL);
      }
      else {
	return make_binary_call_statement (ASSIGN_OPERATOR_NAME,
					   make_expression_from_entity(param),
					   make_expression_from_entity(local_entity),
					   NULL);
      }
    }
  }
  
  return make_continue_statement(entity_empty_label());
}

/**
 * Build and store new module used for communication (SEND or RECEIVE scalar params)
 * Create statement module_statement
 */
static entity make_scalar_communication_module (variable var,
						string module_name,
						hash_table ht_communications, 
						statement* module_statement, 
						int number_of_deployment_units,
						entity global_common,
						list l_commons,
						boolean is_receiving) 
{
  entity new_module;
  entity func_id, param_id, param;
  entity unit_id = NULL;
  string function_name;
  entity set_entity = get_current_module_entity();

  expression test_condition;
  test new_test;
  instruction test_instruction;
  statement test_statement;
  list stat_seq = NIL;
  statement function_statement;
  sequence new_sequence;
  instruction sequence_instruction;

  new_module = make_empty_subroutine(module_name);
  pips_debug(2, "Creating module %s\n", entity_global_name(new_module));
  reset_current_module_entity();
  set_current_module_entity(new_module);
  func_id = create_integer_parameter_for_new_module (COM_MODULE_PARAM1_NAME,
						     module_name,
						     new_module,
						     1);
  if (number_of_deployment_units > 1) {
    unit_id = create_integer_parameter_for_new_module (COM_MODULE_PARAM2_NAME,
						       module_name,
						       new_module,
						       2);
  }
  param_id = create_integer_parameter_for_new_module (COM_MODULE_PARAM3_NAME,
						      module_name,
						      new_module,
						      3);

  param = create_parameter_for_new_module (var,
					   COM_MODULE_PARAM4_NAME,
					   module_name,
					   new_module,
					   4);
  
  /* Declare CONTROL_DATA common to be visible here */
  declare_common_variables_in_module (global_common, new_module);

  /* Declare commons for all externalized functions to be visible here */
  MAP (ENTITY, com, {
    declare_common_variables_in_module (com, new_module);
  },l_commons);

  ifdebug(7) {
    pips_debug(7, "Declarations for %s module: \n", module_name);
    fprint_environment(stderr, new_module);
  }

  HASH_MAP (function, l_reg, {

    list function_proc_l_stats = NIL;
    expression test_condition2;
    test new_test2;
    instruction test_instruction2;
    statement test_statement2;
    sequence new_sequence2;
    instruction sequence_instruction2;

    MAP (REGION, reg, {
      
      statement communication_stat 
	= make_communication_statement(function,
				       new_module,
				       reg,
				       unit_id,
				       param,
				       number_of_deployment_units,
				       is_receiving);
      
      entity param_id_value = entity_in_module (is_receiving?get_out_param_id_name(region_entity(reg),function):get_in_param_id_name(region_entity(reg),function), new_module);
      
      test_condition2
      = MakeBinaryCall (entity_intrinsic(EQUAL_OPERATOR_NAME),
			make_expression_from_entity (param_id),
			make_expression_from_entity (param_id_value));

      new_test2 = make_test (test_condition2, communication_stat, 
			     make_continue_statement(entity_undefined));
    
      test_instruction2 = make_instruction (is_instruction_test,new_test2);
    
      test_statement2 = make_statement (entity_empty_label(),
					STATEMENT_NUMBER_UNDEFINED,
					STATEMENT_ORDERING_UNDEFINED,
					empty_comments,
					test_instruction2,NIL,NULL);
    
      function_proc_l_stats = CONS (STATEMENT, test_statement2, function_proc_l_stats);
 
     /*if (scalar_region_p(reg)) {
	}*/
    },l_reg);

    function_proc_l_stats = gen_nreverse(function_proc_l_stats);

  new_sequence2
    = make_sequence (function_proc_l_stats);
  
  sequence_instruction2 
    = make_instruction(is_instruction_sequence,
		       new_sequence2);
  
  function_statement = make_statement(entity_empty_label(),
				      STATEMENT_NUMBER_UNDEFINED,
				      STATEMENT_ORDERING_UNDEFINED,
				      empty_comments,
				      sequence_instruction2,NIL,NULL);
  
  test_condition 
    = MakeBinaryCall (entity_intrinsic(EQUAL_OPERATOR_NAME),
		      make_expression_from_entity (func_id),
		      make_expression_from_entity (entity_in_module (get_function_id_name(function), new_module)));
  
    new_test = make_test (test_condition, function_statement, 
			  make_continue_statement(entity_undefined));
    
    test_instruction = make_instruction (is_instruction_test,new_test);
    
    test_statement = make_statement (entity_empty_label(),
				     STATEMENT_NUMBER_UNDEFINED,
				     STATEMENT_ORDERING_UNDEFINED,
				     empty_comments,
				     test_instruction,NIL,NULL);
    
    stat_seq = CONS (STATEMENT, test_statement, stat_seq);
  }, ht_communications);

  stat_seq =  gen_nreverse(CONS(STATEMENT, make_return_statement(new_module), stat_seq));

  new_sequence 
    = make_sequence (stat_seq);
  
  sequence_instruction 
    = make_instruction(is_instruction_sequence,
		       new_sequence);

  *module_statement = make_statement(entity_empty_label(),
				     STATEMENT_NUMBER_UNDEFINED,
				     STATEMENT_ORDERING_UNDEFINED,
				     empty_comments,
				     sequence_instruction,NIL,NULL);
  
  store_new_module (module_name, new_module, *module_statement);
  
  reset_current_module_entity();
  set_current_module_entity(set_entity);

  return new_module;
}

/**
 * Return SEND_PARAM module name for function and region
 */
string get_send_param_module_name(entity function, region reg)
{
  if (region_scalar_p(reg)) {
    return get_send_parameter_module_name(type_variable(entity_type(region_entity(reg))));
  }
  else {
    char buffer[256];
    sprintf(buffer,
	    SEND_ARRAY_PARAM_MODULE_NAME,
	    entity_local_name(function),
	    entity_local_name(region_entity(reg)));
    return strdup(buffer);
  }
}

/**
 * Return RECEIVE_PARAM module name for function and region
 */
string get_receive_param_module_name(entity function, region reg)
{
  if (region_scalar_p(reg)) {
    return get_receive_parameter_module_name(type_variable(entity_type(region_entity(reg))));
  }
  else {
    char buffer[256];
    sprintf(buffer,
	    RECEIVE_ARRAY_PARAM_MODULE_NAME,
	    entity_local_name(function),
	    entity_local_name(region_entity(reg)));
    return strdup(buffer);
  }
}

/**
 * Internally used for building communication modules
 */ 
static list make_scalar_communication_modules (hash_table ht_communications,
					       int number_of_deployment_units,
					       entity global_common,
					       list l_commons,
					       boolean is_receiving) 
{
  list l_modules = NIL;
  
  HASH_MAP (var, ht_regions_for_functions, {
    string module_name = is_receiving?get_receive_parameter_module_name(var):get_send_parameter_module_name (var);
    statement module_statement;
    pips_debug(2, "Creating module [%s]\n", module_name);
    l_modules 
      = CONS (ENTITY,
	      make_scalar_communication_module (var,
						module_name, 
						ht_regions_for_functions,
						&module_statement,
						number_of_deployment_units,
						global_common,
						l_commons,
						is_receiving),
	      l_modules);
  },ht_communications);

  return l_modules;
}

/**
 * Build and return list of modules used for INPUT communications
 * (SEND_PARAMETERS...)
 */
list make_send_scalar_params_modules (hash_table ht_in_communications,
					     int number_of_deployment_units,
					     entity global_common,
					     list l_commons) 
{
  return make_scalar_communication_modules (ht_in_communications,
					    number_of_deployment_units,
					    global_common,
					    l_commons,
					    FALSE); 
}

/**
 * Build and return list of modules used for OUTPUT communications
 * (RECEIVE_PARAMETERS...)
 */
list make_receive_scalar_params_modules (hash_table ht_out_communications,
					 int number_of_deployment_units,
					 entity global_common,
					 list l_commons) 
{
  return make_scalar_communication_modules (ht_out_communications,
					    number_of_deployment_units,
					    global_common,
					    l_commons,
					    TRUE); 
}

static void compute_region_variables (region reg,
				      list* l_reg_params,
				      list* l_reg_variables) 
{
  Psysteme ps_reg;
  Pbase ps_base;
  
  ps_reg = region_system(reg);
  ps_base = ps_reg->base;

  *l_reg_params = NIL;
  *l_reg_variables = NIL;
  
  pips_debug(3, "BEGIN compute_region_variables: \n");

  pips_assert("compute_region_variables", ! SC_UNDEFINED_P(ps_reg));
  
  for (; ! VECTEUR_NUL_P(ps_base); ps_base = ps_base->succ) {

    entity e = (entity) ps_base->var;
    if (e != NULL) {
      storage s = entity_storage(e);
      pips_debug(7, "Variable: %s\n", entity_global_name(e));
      /* An entity in a system that has an undefined storage is
	 necesseraly a PHI entity, not dynamic !! */
      if (s != storage_undefined) {
	if (storage_tag(s) == is_storage_ram) {
	  ram r = storage_ram(s);
	  if (dynamic_area_p(ram_section(r))) {
	    *l_reg_variables = CONS(ENTITY, e, *l_reg_variables);
	  }
	}
	else {
	  *l_reg_params = CONS(ENTITY, e, *l_reg_params);
	}
      }
    }
  }
  pips_debug(3, "END compute_region_variables: \n");
}    

/**
 * Internally used for making communication module for non-scalar region
 * and function
 */
static entity compute_region_communications (entity function,
					     region reg) 

{
  list l_reg_params;
  list l_reg_variables;
  Psysteme ps_reg;
  Pbase ps_base;
  Pcontrainte ps_eg, ps_ineg;
  entity equ, leq;

  ps_reg = region_system(reg);
  ps_base = ps_reg->base;
  ps_eg = ps_reg->egalites;
  ps_ineg = ps_reg->inegalites;

  equ = entity_intrinsic(EQUAL_OPERATOR_NAME);
  leq = entity_intrinsic(LESS_OR_EQUAL_OPERATOR_NAME);

  compute_region_variables(reg,&l_reg_params,&l_reg_variables);
  
  ifdebug(3) {
    string variables_string = "Variables: ";
    MAP (ENTITY, e, {variables_string=strdup(concatenate(variables_string, " ", entity_local_name(e), NULL));}, l_reg_variables);
    variables_string=strdup(concatenate(variables_string, "\nParameters: ", NULL));
    MAP (ENTITY, e, {variables_string=strdup(concatenate(variables_string, " ", entity_local_name(e), NULL));}, l_reg_params);
    pips_debug(2, "Region variables:\n%s\n",variables_string);
  }
  
  pips_debug(2, "Psysteme to expression:\n");
  print_expression(Psysteme_to_expression(ps_reg));
  
  pips_debug(2, "Egalites:\n");
  MAP(EXPRESSION, exp, {
    print_expression(exp);
  }, Pcontrainte_to_expression_list(ps_eg, equ));
  
  pips_debug(2, "Inegalites:\n");
  MAP(EXPRESSION, exp, {
    print_expression(exp);
  }, Pcontrainte_to_expression_list(ps_ineg, leq));
  
  pips_debug(2, "loop nest:\n");
  {
    /* !!! WARNING !!!  This divide function has to be redefined here to
     * have a positive remainder ! Use an other custom integer division
     * operation ! */
    entity divide = entity_intrinsic(DIVIDE_OPERATOR_NAME);
    statement stat = systeme_to_loop_nest(ps_reg, l_reg_params, make_continue_statement(entity_empty_label()), divide);
    print_statement(stat);
  }

  /* pips_debug(2, "elements_loop:\n");
  {
    list l_ind = CONS(ENTITY, make_scalar_integer_entity("IND",entity_local_name(get_current_module_entity())), NIL);
    list l_scal = NIL;
    statement stat = elements_loop (ps_reg, l_ind, l_scal, make_continue_statement(entity_empty_label()));
    print_statement(stat);
    }*/

  /*  pips_debug(2, "Region AFTER region_dynamic_var_elim: ");
      {
      region reg2 = copy_region(reg);
      region_dynamic_var_elim(reg2);
      print_region(reg2);
      }
      pips_debug(2, "Region AFTER region_remove_phi_variables: ");
      {
      region reg2 = copy_region(reg);
      region_remove_phi_variables(reg2);
      print_region(reg2);
      }  */
  
}

/**
 * Internally used for making communication module for non-scalar region
 * and function
 */
static entity make_array_communication_module (entity function,
					       region reg,
					       entity global_common,
					       entity externalized_fonction_common,
					       int number_of_deployment_units,
					       boolean is_receiving)
{
  entity new_module;
  entity unit_id, param;
  string module_name = is_receiving?get_receive_param_module_name(function,reg):get_send_param_module_name(function,reg);
  entity set_entity = get_current_module_entity();
  variable var = type_variable(entity_type(region_entity(reg)));
  statement module_statement;
  
  new_module = make_empty_subroutine(module_name);
  pips_debug(2, "Creating module %s\n", entity_local_name(new_module));
  pips_debug(2, "Function [%s]\n", entity_local_name(function));
  pips_debug(2, "Region: ");
  print_region(reg);

  compute_region_communications (function, reg);

  reset_current_module_entity();
  set_current_module_entity(new_module);
  if (number_of_deployment_units > 1) {
    unit_id = create_integer_parameter_for_new_module (COM_MODULE_PARAM2_NAME,
						       module_name,
						       new_module,
						       1);
  }
  param = create_parameter_for_new_module (var,
					   COM_MODULE_PARAM4_NAME,
					   module_name,
					   new_module,
					   2);
  
  /* Declare CONTROL_DATA common to be visible here */
  declare_common_variables_in_module (global_common, new_module);

  /* Declare common for externalized function to be visible here */
  declare_common_variables_in_module (externalized_fonction_common, new_module);
  
  ifdebug(7) {
    pips_debug(7, "Declarations for %s module: \n", module_name);
    fprint_environment(stderr, new_module);
  }
  
  module_statement = make_continue_statement(entity_empty_label());
  
  store_new_module (module_name, new_module, module_statement);
  
  reset_current_module_entity();
  set_current_module_entity(set_entity);
  
  return new_module;
}

/**
 * Internally used for making all communication modules for non-scalar IN
 * or OUT regions for a given function
 */
static list make_array_communication_modules (entity function,
					      list l_regions,
					      entity global_common,
					      entity externalized_fonction_common,
					      int number_of_deployment_units,
					      boolean is_receiving)
{
  list returned = NIL;

  MAP (REGION, reg, {
    if (!region_scalar_p(reg)) {
      returned = CONS (ENTITY, 
		       make_array_communication_module (function,
							reg,
							global_common,
							externalized_fonction_common,
							number_of_deployment_units,
							is_receiving),
		       returned);
    }
  },l_regions);

  return returned;
}
					    
/**
 * Make all SEND_PARAM communication modules for non-scalar regions for a
 * given function
 */
list make_send_array_params_modules (entity function,
				     list l_regions,
				     entity global_common,
				     entity externalized_fonction_common,
				     int number_of_deployment_units)
{
  return make_array_communication_modules (function,
					   l_regions,
					   global_common,
					   externalized_fonction_common,
					   number_of_deployment_units,
					   FALSE);
}
					    
/**
 * Make all RECEIVE_PARAM communication modules for non-scalar regions for a
 * given function
 */
list make_receive_array_params_modules (entity function,
					list l_regions,
					entity global_common,
					entity externalized_fonction_common,
					int number_of_deployment_units)
{
  return make_array_communication_modules (function,
					   l_regions,
					   global_common,
					   externalized_fonction_common,
					   number_of_deployment_units,
					   TRUE);
}

		

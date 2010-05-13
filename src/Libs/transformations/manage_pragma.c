/*
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

/**
 * @file manage_pragma.c
 * @brief This file holds transformations on OpenMP pragmas
 * store in the RI as extension expression.
 * Here is the list of transformations:
 *   1- add an OpenMP if clause
 */

#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include "genC.h"
#include "misc.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "database.h"
#include "pipsdbm.h"
#include "resources.h"
#include "properties.h"

/// @brief add a if condition to the omp pragma
/// @return void
/// @param pr, the pragma to process
static void add_loop_parallel_threshold (pragma pr) {
  // only the pragma as expressions are managed
  if (pragma_expression_p (pr) == TRUE) {
    // we need to get the loop index
    statement stmt = (statement) gen_get_ancestor(statement_domain, pr);
    instruction inst = statement_instruction (stmt);
    if (instruction_tag (inst) == is_instruction_loop) {
      loop l = instruction_loop (inst);
      entity index = loop_index (l);
      entity op = entity_undefined;
      switch(get_prettyprint_language_tag()) {
        case is_language_fortran:
          op = CreateIntrinsic(GREATER_OR_EQUAL_OPERATOR_NAME);
          break;
        case is_language_c:
          op = CreateIntrinsic(C_GREATER_OR_EQUAL_OPERATOR_NAME);
          break;
        case is_language_fortran95:
          pips_assert ("Need to update F95 case", FALSE);
          break;
        default:
          pips_assert ("This case should have been handled before", FALSE);
          break;
      }
      int threshold = get_int_property ("OMP_LOOP_PARALLEL_THRESHOLD_VALUE");
      list args_if =  gen_expression_cons (int_expr (threshold), NIL);
      args_if = gen_expression_cons (entity_to_expression (index), args_if);
      call c = make_call (op, args_if);
      expression cond = call_to_expression (c);
      expression expr_if = pragma_if_as_expr (cond);
      add_expr_to_pragma_expr_list (pr, expr_if);
    }
  }
  return;
}

//////////////////////////////////////////////////////////////
// the phase function name

bool omp_loop_parallel_threshold_set (char mod_name[]) {
  debug_on("OPMIFY_CODE_DEBUG_LEVEL");

  statement mod_stmt = statement_undefined;

  // generate pragma string or expression using the correct language:
  value mv = entity_initial(module_name_to_entity(mod_name));
  if(value_code_p(mv)) {
    code c = value_code(mv);
    set_prettyprint_language_from_property(language_tag(code_language(c)));
  } else {
    /* Should never arise */
    set_prettyprint_language_from_property(is_language_fortran);
  }

  // Get the code and tell PIPS_DBM we do want to modify it
  mod_stmt = (statement) db_get_memory_resource(DBR_CODE, mod_name, TRUE);

  // Add the parallel threshold to all the omp for pragmas
  gen_recurse(mod_stmt, pragma_domain, gen_true,
	      add_loop_parallel_threshold);

  pips_debug(2, "done for %s\n", mod_name);
  debug_off();

  return TRUE;
}

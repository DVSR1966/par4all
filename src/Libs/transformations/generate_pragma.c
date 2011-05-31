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
 * @file generate_pragma.c
 * @brief This file holds transformations on sequentiel code
 * that generates pragmas according to the information available
 * in the pips RI.
 * The generated code is sequential in order to allow
 * further pips transformations the user might want to apply later on.
 * The type of pragma generated are:
 * 1- OpenMP pragma: parallel, for and private clauses
 *
 * @author pierre villalon <pierre.villalon@hpc-project.com>
 * @date 2009-05-24
 */
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif

#include "genC.h"
#include "misc.h"
#include "linear.h"
#include "text.h"
#include "text-util.h"
#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"
#include "control.h"
#include "database.h"
#include "pipsdbm.h"
#include "resources.h"
#include "reductions.h"
#include "properties.h"


/////////////////////////////////////////////////////PRAGMA AS EXPRESSION

///@return the new pragma
///@param l, the loop to analyze for omp reduction
///@param exprs, the pragma as a list of expression
static list append_private_clause (loop l, list exprs) {
  // the private variables as a list of entites
  list private = loop_private_variables_as_entites (l, TRUE, TRUE);
  // add private clause if needed
  if (gen_length (private) != 0) {
    expression expr_private  = pragma_private_as_expr (private);
    exprs = gen_expression_cons (expr_private, exprs);
  }
  return exprs;
}

/// @brief generate pragma for a reduction as a list of expressions
/// @return true if a pragma has been generated
/// @param l, the loop to analyze for omp reduction
/// @param stmt, the statament where the pragma should be attached
static bool pragma_expr_for_reduction (loop l, statement stmt) {
  // the list of expression to generate
  list exprs = NULL;
  exprs = reductions_get_omp_pragma_expr (l, stmt);
  // insert the pragma (if any) as an expression to the current statement
  if (exprs != NULL) {
    exprs = append_private_clause (l, exprs);
    add_pragma_expr_to_statement (stmt, exprs);
    pips_debug (5, "new reduction pragma as an extension added\n");
  }
  return (exprs != NULL);
}

/// @brief generate "pragma omp for" as a list of expressions
/// @return true if a pragma has been generated
/// @param l, the loop to analyze for omp for
/// @param stmt, the statament where the pragma should be attached
static bool pragma_expr_for (loop l, statement stmt) {
  list exprs = NULL;
  if (execution_parallel_p(loop_execution(l))) {
    // the list of expression to generate initialized with
    // pragma "omp parallel for"
    list exprs = pragma_omp_parallel_for_as_exprs ();
    exprs = append_private_clause (l, exprs);
    // insert the pragma as an expression to the current statement
    add_pragma_expr_to_statement (stmt, exprs);
    pips_debug (5, "new for pragma as an extension added\n");
  }
  return (exprs != NULL);
}

/// @brief generate pragma as a list of expressions for a loop
/// @return void
/// @param l, the loop to decorate with pragma
static void generate_expr_omp_pragma_loop (loop l) {

  statement stmt = (statement) gen_get_ancestor(statement_domain, l);

  if (pragma_expr_for_reduction (l, stmt) == false)
    // A reduction pragma has alredy been found no need to look for a simple
    // parallel for loop. Note that founding such a simple paralle for loop
    // might show some problems in the code. For example dead code or the usage
    // of unitialized variables. In such a case PIPS follows the principle :
    // If the code is false or dead then, do it in parallel, it will still be
    // false or dead.
    pragma_expr_for (l, stmt);

  return;
}

/////////////////////////////////////////////////////PRAGMA AS STRING

/// @brief generate pragma for a reduction as a string
/// @return true if a pragma has been generated
/// @param l, the loop to analyze for omp reduction
/// @param stmt, the statament where the pragma should be attached
static bool pragma_str_for_reduction (loop l, statement stmt) {
  string str = string_undefined;
  str = reductions_get_omp_pragma_str (l, stmt);
  // insert the pragma (if any) as a string to the current statement
  if ((str !=string_undefined) && (str != NULL) && (strcmp (str, "") != 0)) {
    add_pragma_str_to_statement (stmt, str, FALSE);
    pips_debug (5, "new reduction pragma as an extension added: %s \n", str);
    return true;
  }
  return false;
}

/// @brief generate pragma for as a string
/// @return true if a pragma has been generated
/// @param l, the loop to analyze for omp for
/// @param stmt, the statament where the pragma should be attached
static bool pragma_str_for (loop l, statement stmt) {
  text        t    = text_undefined;
  string      str  = string_undefined;
  // get the pragma as text and convert to string
  t = text_omp_directive (l, 0);
  str = text_to_string (t);
  // text appends one uselless \n at the end of the string so remove it
  chop_newline (str, FALSE);
  if ((str !=string_undefined) && (str != NULL) && (strcmp (str, "") != 0)) {
    string tmp = string_undefined;
    switch(get_prettyprint_language_tag()) {
      case is_language_fortran:
        // for fortran case we need to look at the O of OMP and skip !$
        tmp = strchr(str, 'O');
        break;
      case is_language_c:
        // for C case we need to look at the o of omp and skip #pragma"
        tmp = strchr(str, 'o');
        break;
      case is_language_fortran95:
        pips_internal_error("Need to update F95 case");
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
    // insert the pragma as a string to the current statement
    if ((tmp !=string_undefined) && (tmp != NULL) && (strcmp (tmp, "") != 0)) {
      add_pragma_str_to_statement (stmt, tmp, TRUE);
      pips_debug (5, "new for pragma as an extension added: %s \n", str);
      return true;
    }
  }
  return false;
}

/// @brief generate pragma as a string for a loop
/// @return void
/// @param l, the loop to decorate with pragma
static void generate_str_omp_pragma_loop (loop l) {

  statement stmt = (statement) gen_get_ancestor(statement_domain, l);

  if (pragma_str_for_reduction (l, stmt) == false)
    // A reduction pragma has alredy been found no need to look for a simple
    // parallel for loop. Note that founding such a simple paralle for loop
    // might show some problems in the code. For example dead code or the usage
    // of unitialized variables. In such a case PIPS follows the principle :
    // If the code is false or dead then, do it in parallel, it will still be
    // false or dead.
    pragma_str_for (l, stmt);

  return;
}

//////////////////////////////////////////////////////////////
// the phases function name

bool ompify_code (const string module_name) {
  // Use this module name and this environment variable to set
  statement module_statement = PIPS_PHASE_PRELUDE(module_name,
						  "OMPIFY_CODE_DEBUG_LEVEL");
  // we want omp syntax so save and change the current PRETTYPRINT_PARALLEL
  // property
  string previous = strdup(get_string_property("PRETTYPRINT_PARALLEL"));
  set_string_property("PRETTYPRINT_PARALLEL", "omp");
  // we need to know which type of pragma need to be generated
  string type = get_string_property("PRAGMA_TYPE");

  // we need to iniatlize few things to generate reduction
  reductions_pragma_omp_init (module_name);

  // generate pragma string or expression using the correct language:
  value mv = entity_initial(module_name_to_entity(module_name));
  if(value_code_p(mv)) {
    code c = value_code(mv);
    set_prettyprint_language_from_property(language_tag(code_language(c)));
  } else {
    /* Should never arise */
    set_prettyprint_language_from_property(is_language_fortran);
  }

  // generate omp pragma for parallel loops
  // We need to access to the statement containing the current loop, forloop
  // so ask NewGen gen_recurse to keep this informations for us
  // Iterate on all the loop
  if (strcmp (type, "str") == 0)
    gen_recurse(module_statement, loop_domain, gen_true,
		generate_str_omp_pragma_loop);
  else  if (strcmp (type, "expr") == 0)
    gen_recurse(module_statement, loop_domain, gen_true,
		generate_expr_omp_pragma_loop);
  else pips_assert ("not expected property", FALSE);

  // Restore the previous PRETTYPRINT_PARALLEL property for the next
  set_string_property("PRETTYPRINT_PARALLEL", previous);
  free(previous);

  // no more reductions to generate
  reductions_pragma_omp_end ();

  // Put back the new statement module
  PIPS_PHASE_POSTLUDE(module_statement);

  return TRUE;
}
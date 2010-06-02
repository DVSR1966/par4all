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

#ifndef lint
char lib_ri_util_prettyprint_c_rcsid[] = "$Id$";
#endif /* lint */

 /*
  * Prettyprint all kinds of ri related data structures
  *
  *  Modifications:
  * - In order to remove the extra parentheses, I made the several changes:
  * (1) At the intrinsic_handler, the third term is added to indicate the
  *     precendence, and accordingly words_intrinsic_precedence(obj) is built
  *     to get the precedence of the call "obj".
  * (2) words_subexpression is created to distinguish the
  *     words_expression.  It has two arguments, expression and
  *     precedence. where precedence is newly added. In case of failure
  *     of words_subexpression , that is, when
  *     syntax_call_p is FALSE, we use words_expression instead.
  * (3) When words_call is firstly called , we give it the lowest precedence,
  *        that is 0.
  *    Lei ZHOU           Nov. 4, 1991
  *
  * - Addition of CMF and CRAFT prettyprints. Only text_loop() has been
  * modified.
  *    Alexis Platonoff, Nov. 18, 1994

  * - Modifications of sentence_area to deal with  the fact that
  *   " only one appearance of a symbolic name as an array name in an
  *     array declarator in a program unit is permitted."
  *     (Fortran standard, number 8.1, line 40)
  *   array declarators now only appear with the type declaration, not with the
  *   area. - BC - october 196.
  *
  * - Modification of text_entity_declaration to ensure that the OUTPUT of PIPS
  *   can also be used as INPUT; in particular, variable declarations must
  *   appear
  *   before common declarations. BC.
  *
  * - neither are DATA statements for non integers (FI/FC)
  *
  * - Also, EQUIVALENCE statements are not generated for the moment. BC.
  *     Thay are now??? FC?
  *
  * - variable pdl added in most signature to handle derived type
  *   declarations in C; it is the parser declaration list; if a derived
  *   type must be prettyprinted, it must be prettyprinted with all
  *   information if in pdl, and else it must be prettyprinted with no
  *   information. For instance, "struct m {int l; int m}" is the
  *   definition of m. Other references to the type must be
  *   prettyprinted "struct m". The PIPS internal representation does
  *   record derived type declarations. The parser declaration list is
  *   used to desambiguate between the two cases. The problem occurs
  *   in both declarations.c and prettyprint.c because types can
  *   appear in expressions thanks to the sizeof and cast operators.
  *
  * Data structures used:
  *
  * text: to produce output with multiple lines (a.k.a. "sentence")
  * and proper indenting; this is a Newgen managed data structure
  *
  * words: a list of strings to produce output without any specific
  * formatting, but text's sentences can be built with words.
  *
  * Call graph structure (a slice of it, for C prettyprint):
  *
  * text_module
  *   text_named_module
  *     text_statement
  *       text_statement_enclosed: to manage braces
  *         text_instruction: to print a command
  *         c_text_related_entities: to print the declarations
  *                                  all variables declared share some type
  *           c_text_entities:  to declare a list of variables
  *             c_text_entity: to declare a variable; may call
  *                            recursively c_text_related_entities to
  *                            print out, for instance, a set of membres
  *               words_variable_or_function(): words level
  *                 c_words_simplified_entity()
  *                   generic_c_words_simplified_entity()
  */

// To have asprintf:
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "linear.h"

#include "genC.h"
#include "text.h"
#include "text-util.h"
#include "ri.h"
#include "ri-util.h"

#include "pipsdbm.h"

#include "misc.h"
#include "properties.h"
#include "prettyprint.h"
#include "preprocessor.h"

#include "STEP_name.h"
extern string compilation_unit_of_module(string);

/* operator precedences are in the [0,100] range */

#define MAXIMAL_PRECEDENCE 100
#define MINIMAL_ARITHMETIC_PRECEDENCE 19

/* Define the markers used in the raw unstructured output when the
   PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH property is true: */
#define PRETTYPRINT_UNSTRUCTURED_BEGIN_MARKER "\200Unstructured"
#define PRETTYPRINT_UNSTRUCTURED_END_MARKER "\201Unstructured End"
#define PRETTYPRINT_UNSTRUCTURED_ITEM_MARKER "\202Unstructured Item"
#define PRETTYPRINT_UNSTRUCTURED_SUCC_MARKER "\203Unstructured Successor ->"
#define PRETTYPRINT_UNREACHABLE_EXIT_MARKER "\204Unstructured Unreachable"

/*===================== Language management ===========*/

/* The prettyprint language */
static language prettyprint_language = language_undefined;


/**
 * @brief please avoid using this function directly, use predicate instead
 * (see below)
 * @return the prettyprint language as a newgen language object
 */
language get_prettyprint_language () {
  if (prettyprint_language == language_undefined)
    prettyprint_language = make_language_fortran ();
  return prettyprint_language;
}


/**
 * @return the prettyprint language as a language_utype
 **/
enum language_utype get_prettyprint_language_tag () {
  return language_tag (get_prettyprint_language ());
}


/**
 * @return true if the language is f77
 **/
bool prettyprint_language_is_fortran_p () {
  return (get_prettyprint_language_tag () == is_language_fortran?TRUE:FALSE);
}


/**
 * @return true if the language is f95
 **/
bool prettyprint_language_is_fortran95_p () {
  return (get_prettyprint_language_tag () == is_language_fortran95?TRUE:FALSE);
}


/**
 * @return true if the language is C
 **/
bool prettyprint_language_is_c_p () {
  return (get_prettyprint_language_tag () == is_language_c?TRUE:FALSE);
}


/**
 * @brief set the prettyprint language according to the property
 * PRETTYPRINT_LANGUAGE
 * @description If the property PRETTYPRINT_LANGUAGE is set to the special
 * value "native" then the language passed in arg is used, usually it's the
 * module native language. The user can set "F77", "F95", or "C" to force the
 * prettyprint of a language.
 */
void set_prettyprint_language_from_property( enum language_utype native ) {
  if (prettyprint_language == language_undefined) {
    prettyprint_language = make_language_fortran ();
  }
  string lang = get_string_property ("PRETTYPRINT_LANGUAGE");
  if (strcmp (lang, "F77") == 0) {
    language_tag (prettyprint_language) = is_language_fortran;
  }
  else if (strcmp (lang, "C") == 0) {
    language_tag (prettyprint_language) = is_language_c;
  }
  else if (strcmp (lang, "F95") == 0) {
    language_tag (prettyprint_language) = is_language_fortran95;
  }
  else if (strcmp (lang, "native") == 0) {
    language_tag (prettyprint_language) = native;
  } else {
    pips_internal_error("bad property value for language");
  }
}


/**
   @brief set the prettyprint language from a newgen language object
   @param lang, the language to be used to set the prettyprint_language
   variable, content is copied so caller may free if it was malloced
 **/
void set_prettyprint_language (language lang) {
  if (prettyprint_language == language_undefined)
    prettyprint_language = make_language_fortran ();
  *prettyprint_language = *lang;
}


/**
   @brief set the prettyprint language from a language_utype argument
   @param lang, the language to be used to set the prettyprint_language
   variable
 **/

void set_prettyprint_language_tag (enum language_utype lang) {
  if (prettyprint_language == language_undefined)
    prettyprint_language = make_language_fortran ();
  language_tag (prettyprint_language) = lang;
}


/* @brief Start a single line comment
 * @return a string containing the begin of a comment line, language dependent
 */
string get_comment_sentinel() {
  switch(get_prettyprint_language_tag()) {
    case is_language_c: return "//";
    case is_language_fortran: return "C";
    case is_language_fortran95: return "!";
  }
}


/* @brief Start a single line comment with continuation (blank spaces)
 * @return a string containing the begin of a comment line, language dependent
 */
string get_comment_continuation() {
  switch(get_prettyprint_language_tag()) {
    case is_language_c: return "//    ";
    case is_language_fortran: return "C    ";
    case is_language_fortran95: return "!    ";
  }
}


unsigned int get_prettyprint_indentation() {
  if(prettyprint_language_is_fortran_p()) {
    return 0;
  } else {
    return INDENTATION;
  }
}

static list words_cast(cast obj, int precedence, list pdl);
static list words_sizeofexpression(sizeofexpression obj, bool in_type_declaration, list pdl);
static list words_subscript(subscript s, list pdl);
static list words_application(application a, list pdl);
static text text_forloop(entity module,string label,int margin,forloop obj,int n, list pdl);

/* This variable is used to disable the precedence system and hence to
   prettyprint all parentheses, which let the prettyprint reflect the
   AST. */
static bool precedence_p = TRUE;

/******************************************************************* STYLES */

static bool pp_style_p(string s) {
  return same_string_p(get_string_property(PRETTYPRINT_PARALLEL), s);
}

#define pp_hpf_style_p() 	pp_style_p("hpf")
#define pp_f90_style_p() 	pp_style_p("f90")
#define pp_craft_style_p() 	pp_style_p("craft")
#define pp_cray_style_p() 	pp_style_p("cray")
#define pp_cmf_style_p()	pp_style_p("cmf")
#define pp_doall_style_p()	pp_style_p("doall")
#define pp_do_style_p()		pp_style_p("do")
#define pp_omp_style_p()	pp_style_p("omp")

/********************************************************************* MISC */

text empty_text(entity __attribute__ ((unused)) e,
                int __attribute__ ((unused)) m,
                statement __attribute__ ((unused)) s) {
  return make_text(NIL);
}

static text (*text_statement_hook)(entity, int, statement) = empty_text;


/**
 * @brief checks that the prettyprint hook was actually reset...
 */
void init_prettyprint(text(*hook)(entity, int, statement)) {
  pips_assert("prettyprint hook not set", text_statement_hook==empty_text);
  text_statement_hook = hook;
}


/**
 * @brief because some prettyprint functions may be used for debug, so
 * the last hook set by somebody may have stayed there although
 * being non sense...
 */
void close_prettyprint() {
  text_statement_hook = empty_text;
}


/**
 * @brief Can this statement be printed on one line, without enclosing braces?
 */
bool one_liner_p(statement s)
{
  instruction i = statement_instruction(s);
  bool yes = (instruction_test_p(i) || instruction_loop_p(i) || instruction_whileloop_p(i)
	      || instruction_call_p(i) || instruction_forloop_p(i) || instruction_goto_p(i)
	      || return_instruction_p(i));

  yes = yes  && ENDP(statement_declarations(s));

  if(!yes && instruction_sequence_p(i)) {
    list sl = sequence_statements(instruction_sequence(i));
    int sc = gen_length(sl);

    if(sc==1) {
      /* There may be many lines hidden behind another block construct
	 when code is generated in a non canonical way as for
	 {{x=1;y=2;}} */
      instruction ii = statement_instruction(STATEMENT(CAR(sl)));

      if(instruction_sequence_p(ii)) {
	/* OK, we could check deeper, but this is only useful for
	   redundant internal representations. Let's forget about
	   niceties such as skipping useless braces. */
	yes = FALSE;
      }
      else
	yes = ENDP(statement_declarations(s));
    }
    else
      yes = (sc < 1) && ENDP(statement_declarations(s));
  }

  return yes;
}



/***************************************************local variables handling */

static text local_var;
static bool local_flg = false;

/**
 *  @brief This function either appends the declaration to the text given as a
 *  parameter or return a new text with the declaration
*/
static text insert_locals (text r) {
  if (local_flg == true) {
    if ((r != text_undefined) && (r != NULL)){
      MERGE_TEXTS (r, local_var);
    }
    else {
      r = local_var;
    }
    local_flg = false;
  }
  return r;
}


/**
 * @brief This function returns true if BLOCK boundary markers are required.
 * The function also creates the maker when needed.
 */
static bool mark_block(unformatted *t_beg,
                       unformatted *t_end,
                       int n,
                       int margin) {
  bool result = false;
  if(!get_bool_property("PRETTYPRINT_FOR_FORESYS")
      && (get_bool_property("PRETTYPRINT_ALL_EFFECTS")
          || get_bool_property("PRETTYPRINT_BLOCKS")))
    result = true;
  if(result == true) {
    list pbeg = NIL;
    list pend = NIL;
    // Here we need to generate block markers for later use:
    switch(get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        // Fortran case: comments at the begin of the line
        pbeg = CHAIN_SWORD (NIL, "BEGIN BLOCK");
        pend = CHAIN_SWORD (NIL, "END BLOCK");
        *t_beg = make_unformatted(strdup(get_comment_sentinel()),
                                  n,
                                  margin,
                                  pbeg);
        *t_end = make_unformatted(strdup(get_comment_sentinel()),
                                  n,
                                  margin,
                                  pend);
        break;
      case is_language_c:
        // C case: comments alligned with blocks:
        pbeg = CHAIN_SWORD(NIL, strdup(get_comment_sentinel()));
        pend = CHAIN_SWORD(NIL, strdup(get_comment_sentinel()));
        pbeg = CHAIN_SWORD (pbeg, " BEGIN BLOCK");
        pend = CHAIN_SWORD (pend, " END BLOCK");
        *t_beg = make_unformatted(NULL, n, margin, pbeg);
        *t_end = make_unformatted(NULL, n, margin, pend);
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }
  return result;
}

/********************************************************************* WORDS */

static int words_intrinsic_precedence(call);
static int intrinsic_precedence(string);

/**
 * @brief exported for craft
 */
list words_loop_range(range obj, list pdl) {
    list pc;
    call c = syntax_call(expression_syntax(range_increment(obj)));

    pc = words_subexpression(range_lower(obj), 0, TRUE, pdl);
    pc = CHAIN_SWORD(pc,", ");
    pc = gen_nconc(pc, words_subexpression(range_upper(obj), 0, TRUE, pdl));
    if (/*  expression_constant_p(range_increment(obj)) && */
	 strcmp( entity_local_name(call_function(c)), "1") == 0 )
	return(pc);
    pc = CHAIN_SWORD(pc,", ");
    pc = gen_nconc(pc, words_expression(range_increment(obj), pdl));

    return(pc);
}


/**
 * @brief Output a Fortan-like do-loop range as a C-like for-loop index part.
 * @description Assume that the increment is an integer so we can generate the
 * good condition. Since the do-loops are recognized in C program part only
 * with this assumptions, it is a good assumption.
 */
list C_loop_range(range obj, entity i, list pdl)
{
    list pc;
    /* call c = syntax_call(expression_syntax(range_increment(obj))); */

    /* Complete the initialization assignment */
    pc = words_subexpression(range_lower(obj), 0, TRUE, pdl);
    pc = CHAIN_SWORD(pc,"; ");

    /* Check the final bound */
    pc = CHAIN_SWORD(pc, entity_user_name(i));

    /* Increasing or decreasing index? */
    expression inc = range_increment(obj);
    // Assume the increment has an integer value with a known sign
    if (expression_negative_integer_value_p(inc))
      /* The increment is negative, that means the index is tested against
	 a lower bound: */
      pc = CHAIN_SWORD(pc," >= ");
    else
      /* Else we assume to test against an upper bound: */
      pc = CHAIN_SWORD(pc," <= ");

    pc = gen_nconc(pc, words_subexpression(range_upper(obj), 0, TRUE, pdl));
    pc = CHAIN_SWORD(pc,"; ");

    /* Increment the loop index */
    pc = CHAIN_SWORD(pc, entity_user_name(i));
    pc = CHAIN_SWORD(pc," += ");
    pc = gen_nconc(pc, words_expression(inc, pdl));
    pc = CHAIN_SWORD(pc,")");

    return(pc);
}


/**
 * @return a list of string
 */
list words_range(range obj, list pdl) {
  list pc = NIL;

  /* if undefined I print a star, why not!? */
  if(expression_undefined_p(range_lower(obj))) {
    pc = CONS(STRING, MAKE_SWORD("*"), NIL);
  } else {
    switch(get_prettyprint_language_tag()) {
      case is_language_fortran: {
        call c = syntax_call(expression_syntax(range_increment(obj)));

        pc = CHAIN_SWORD(pc,"(/ (I,I=");
        pc = gen_nconc(pc, words_expression(range_lower(obj), pdl));
        pc = CHAIN_SWORD(pc,",");
        pc = gen_nconc(pc, words_expression(range_upper(obj), pdl));
        if(strcmp(entity_local_name(call_function(c)), "1") != 0) {
          pc = CHAIN_SWORD(pc,",");
          pc = gen_nconc(pc, words_expression(range_increment(obj), pdl));
        }
        break;
      }
      case is_language_fortran95: {
        // Print the lower bound if != *
        if(!unbounded_expression_p(range_lower(obj))) {
          pc = gen_nconc(pc, words_expression(range_lower(obj), pdl));
        }

        // Print the upper bound if != *
        pc = CHAIN_SWORD(pc,":");
        if(!unbounded_expression_p(range_upper(obj))) {
          pc = gen_nconc(pc, words_expression(range_upper(obj), pdl));
        }

        // Print the increment if != 1
        call c = syntax_call(expression_syntax(range_increment(obj)));
        if(strcmp(entity_local_name(call_function(c)), "1") != 0) {
          pc = CHAIN_SWORD(pc,":");
          pc = gen_nconc(pc, words_expression(range_increment(obj), pdl));
        }
        break;
      }
      case is_language_c:
        pips_internal_error("I don't know how to print a range in C !");
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }
  return pc;
}


/**
 * @description FI: array constructor R433, p. 37 in Fortran 90 standard, can be
 * used anywhere in arithmetic expressions whereas the triplet notation is
 * restricted to subscript expressions. The triplet notation is used to define
 * array sections (see R619, p. 64).
 *
 * @return a list of string corresponding to the range
*/
list words_subscript_range(range obj, list pdl) {
  list pc = NIL;

  /* if undefined I print a star, why not!? */
  if(expression_undefined_p(range_lower(obj))) {
    pc = CONS(STRING, MAKE_SWORD("*"), NIL);
  } else {
    switch(get_prettyprint_language_tag()) {
      case is_language_fortran: {
        call c = syntax_call(expression_syntax(range_increment(obj)));

        pc = gen_nconc(pc, words_expression(range_lower(obj), pdl));
        pc = CHAIN_SWORD(pc,":");
        pc = gen_nconc(pc, words_expression(range_upper(obj), pdl));
        if(strcmp(entity_local_name(call_function(c)), "1") != 0) {
          pc = CHAIN_SWORD(pc,":");
          pc = gen_nconc(pc, words_expression(range_increment(obj), pdl));
        }
        break;
      }
      case is_language_fortran95: {
        // Print the lower bound if != *
        if(!unbounded_expression_p(range_lower(obj))) {
          pc = gen_nconc(pc, words_expression(range_lower(obj), pdl));
        }

        // Print the upper bound if != *
        pc = CHAIN_SWORD(pc,":");
        if(!unbounded_expression_p(range_upper(obj))) {
          pc = gen_nconc(pc, words_expression(range_upper(obj), pdl));
        }

        // Print the increment if != 1
        call c = syntax_call(expression_syntax(range_increment(obj)));
        if(strcmp(entity_local_name(call_function(c)), "1") != 0) {
          pc = CHAIN_SWORD(pc,":");
          pc = gen_nconc(pc, words_expression(range_increment(obj), pdl));
        }
        break;
      }
      case is_language_c:
        pips_internal_error("I don't know how to print a range in C !");
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }
  return pc;
}

/* exported for expression.c
 *
 * Should only be used to prettyprint proper C references.
 */
list words_any_reference(reference obj, list pdl, const char* (*enf)(entity))
{
  list pc = NIL;
  string begin_attachment;
  entity e = reference_variable(obj);


  if(!ENTITY_ALLOCATABLE_BOUND_P(e)) {
    /* We don't want to print these special entity, they are there for
     * internal purpose only
     */

    /* Print the entity first */
    pc = CHAIN_SWORD(pc, (*enf)(e));

    begin_attachment = STRING(CAR(pc));

    /* Let's print the indices now */
    if(reference_indices(obj) != NIL) {
      switch(get_prettyprint_language_tag()) {
        case is_language_fortran95:
        case is_language_fortran: {
          int count = 0;
          pc = CHAIN_SWORD(pc,"(");
          FOREACH(EXPRESSION, subscript, reference_indices(obj)) {
            syntax ssubscript = expression_syntax(subscript);
            if(count > 0)
              pc = CHAIN_SWORD(pc,",");
            else
              count++;
            if(syntax_range_p(ssubscript)) {
              pc = gen_nconc(pc,
                             words_subscript_range(syntax_range(ssubscript),
                                                   pdl));
            } else {
              pc = gen_nconc(pc, words_subexpression(subscript, 0, TRUE, pdl));
            }
          }
          pc = CHAIN_SWORD(pc,")");
          break;
        }
        case is_language_c: {
          FOREACH(EXPRESSION, subscript, reference_indices(obj)) {
            syntax ssubscript = expression_syntax(subscript);
            pc = CHAIN_SWORD(pc, "[");
            if(syntax_range_p(ssubscript)) {
              pc = gen_nconc(pc,
                             words_subscript_range(syntax_range(ssubscript),
                                                   pdl));
            } else {
              pc = gen_nconc(pc, words_subexpression(subscript, 0, TRUE, pdl));
            }
            pc = CHAIN_SWORD(pc, "]");
          }
          break;
        }
        default:
          pips_internal_error("Language unknown !");
      }
    }
    attach_reference_to_word_list(begin_attachment,
                                  STRING(CAR(gen_last(pc))),
                                  obj);
  }


  return(pc);
}

list words_reference(reference obj, list pdl)
{
  return words_any_reference(obj, pdl, entity_user_name);
}

/* Management of alternate returns */

static list set_of_labels_required_for_alternate_returns = list_undefined;

void set_alternate_return_set()
{
  ifdebug(1) {
  pips_assert("The target list is undefined",
	      list_undefined_p(set_of_labels_required_for_alternate_returns));
  }
  set_of_labels_required_for_alternate_returns = NIL;
}

void reset_alternate_return_set()
{
  ifdebug(1) {
  pips_assert("The target list is initialized",
	      !list_undefined_p(set_of_labels_required_for_alternate_returns));
  }
  gen_free_list(set_of_labels_required_for_alternate_returns);
  set_of_labels_required_for_alternate_returns = list_undefined;
}

void add_target_to_alternate_return_set(entity l)
{
  ifdebug(1) {
  pips_assert("The target list is initialized",
	      !list_undefined_p(set_of_labels_required_for_alternate_returns));
  }
  set_of_labels_required_for_alternate_returns =
    gen_once(l, set_of_labels_required_for_alternate_returns);
}

text generate_alternate_return_targets()
{
  text ral = text_undefined;

  if(!ENDP(set_of_labels_required_for_alternate_returns)) {
    list sl = NIL;
    FOREACH(entity, le, set_of_labels_required_for_alternate_returns) {
      sentence s1 = sentence_undefined;
      string str_continue = string_undefined;
      switch (get_prettyprint_language_tag()) {
        case is_language_fortran95:
        case is_language_fortran:
          str_continue = CONTINUE_FUNCTION_NAME;
          break;
        case is_language_c:
          str_continue = C_CONTINUE_FUNCTION_NAME;
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
      unformatted u1 =
          make_unformatted( strdup( label_local_name( le ) ),
                            STATEMENT_NUMBER_UNDEFINED,
                            0,
                            CONS(STRING, strdup(str_continue), NIL));
      s1 = make_sentence(is_sentence_unformatted, u1);
      sl = gen_nconc(sl, CONS(SENTENCE, s1, NIL));
    }
    ral = make_text(sl);
  }
  else {
    ral = make_text(NIL);
  }
  return ral;
}


/* words_regular_call used for user subroutine and user function and
   intrinsics called like user function such as MOD().

   used also by library static_controlize
 */

list words_regular_call(call obj, bool is_a_subroutine, list pdl)
{
  list pc = NIL;

  entity f = call_function(obj);
  value i = entity_initial(f);
  type t = entity_type(f);
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  if (call_arguments(obj) == NIL) {
    if (type_statement_p(t))
      return (CHAIN_SWORD(pc, entity_local_name(f)+strlen(LABEL_PREFIX)));
    if (value_constant_p(i) || value_symbolic_p(i)) {
      switch (get_prettyprint_language_tag()) {
        case is_language_fortran:
        case is_language_fortran95:
          return (CHAIN_SWORD(pc, entity_user_name(f)));
          break;
        case is_language_c:
          if (ENTITY_TRUE_P(f))
            return (CHAIN_SWORD(pc, "true"));
          if (ENTITY_FALSE_P(f))
            return (CHAIN_SWORD(pc, "false"));
          return (CHAIN_SWORD(pc, entity_user_name(f)));
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    }
  }

  type calltype = call_compatible_type(entity_type(call_function(obj)));
  bool function_p = type_void_p(functional_result(type_functional(calltype)));

  if (function_p) {
    if (is_a_subroutine) {
      switch (get_prettyprint_language_tag()) {
        case is_language_fortran:
        case is_language_fortran95:
          pc = CHAIN_SWORD(pc, "CALL ");
          break;
        case is_language_c:
          pc = CHAIN_SWORD(pc, "");
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    } else {
      switch (get_prettyprint_language_tag()) {
        case is_language_fortran:
          pips_user_warning("subroutine '%s' used as a function.\n",
              entity_name(f));
          break;
        case is_language_c:
          // no warning in C
          break;
        case is_language_fortran95:
          pips_internal_error("Need to update F95 case");
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    }
  } else if (is_a_subroutine) {
    switch (get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        pips_user_warning("function '%s' used as a subroutine.\n",
            entity_name(f));
        pc = CHAIN_SWORD(pc, "CALL ");
        break;
      case is_language_c:
        // no warning in C
        pc = CHAIN_SWORD(pc, "");
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }

  /* special cases for stdarg builtin macros */
  if (ENTITY_VA_END_P(f))
    pc = CHAIN_SWORD(pc, "va_end");
  else if (ENTITY_VA_START_P(f))
    pc = CHAIN_SWORD(pc, "va_start");
  else if (ENTITY_VA_COPY_P(f))
    pc = CHAIN_SWORD(pc, "va_copy");

  /* Special cases for stdio.h */
  /* else if (ENTITY__IO_GETC_P(f)) */
/*     pc = CHAIN_SWORD(pc, "getc"); */
/*   else if (ENTITY__IO_PUTC_P(f)) */
/*     pc = CHAIN_SWORD(pc, "putc"); */
  else if (ENTITY_ISOC99_SCANF_P(f))
    pc = CHAIN_SWORD(pc, ISOC99_SCANF_USER_FUNCTION_NAME);
  else if (ENTITY_ISOC99_FSCANF_P(f))
    pc = CHAIN_SWORD(pc, ISOC99_FSCANF_USER_FUNCTION_NAME);
  else if (ENTITY_ISOC99_SSCANF_P(f))
    pc = CHAIN_SWORD(pc, ISOC99_SSCANF_USER_FUNCTION_NAME);
  else if (ENTITY_ISOC99_VFSCANF_P(f))
    pc = CHAIN_SWORD(pc, ISOC99_VFSCANF_USER_FUNCTION_NAME);
  else if (ENTITY_ISOC99_VSCANF_P(f))
    pc = CHAIN_SWORD(pc, ISOC99_VSCANF_USER_FUNCTION_NAME);
  else if (ENTITY_ISOC99_VSSCANF_P(f))
    pc = CHAIN_SWORD(pc, ISOC99_VSSCANF_USER_FUNCTION_NAME);


  /* the implied complex operator is hidden... [D]CMPLX_(x,y) -> (x,y)
   */
  else if(!ENTITY_IMPLIED_CMPLX_P(f) && !ENTITY_IMPLIED_DCMPLX_P(f))
    pc = CHAIN_SWORD(pc, entity_user_name(f));

  /* The corresponding formal parameter cannot be checked by
     formal_label_replacement_p() because the called modules may not have
     been parsed yet. */

  if(!ENDP(call_arguments(obj))) {
    list pa = list_undefined;
    pc = CHAIN_SWORD(pc, "(");

    for(pa = call_arguments(obj); !ENDP(pa); POP(pa)) {
      expression eap = EXPRESSION(CAR(pa));
      if(get_bool_property("PRETTYPRINT_REGENERATE_ALTERNATE_RETURNS")
	 && expression_call_p(eap) && actual_label_replacement_p(eap)) {
	/* Alternate return actual argument have been replaced by
           character strings by the parser. */
	entity cf = call_function(syntax_call(expression_syntax(eap)));
	string ls = entity_local_name(cf);
	string ls1 = malloc(strlen(ls));
	/* pips_assert("ls has at least four characters", strlen(ls)>=4); */

	/* Get rid of initial and final quotes */
	ls1 = strncpy(ls1, ls+1, strlen(ls)-2);
	*(ls1+strlen(ls)-2) = '\000';
	pips_assert("eap must be a call to a constant string", expression_call_p(eap));
	if(strcmp(get_string_property("PARSER_SUBSTITUTE_ALTERNATE_RETURNS"), "STOP")!=0) {
	  pc = CHAIN_SWORD(pc, ls1);
	  /* free(ls1); */
	}
	else {
	  /* The actual label cannot always be used because it might have been
             eliminated as part of dead code by PIPS since it is not used
             with the STOP option. */
	  if(label_string_defined_in_current_module_p(ls1+1)) {
	    pc = CHAIN_SWORD(pc, ls1);
	  }
	  else {
	    entity els1 = find_label_entity(get_current_module_name(), ls1+1);

	    /* The assertion may be wrong if this piece of code is used to
               print intermediate statements */
	    pips_assert("Label els1 has been defined although it is not used anymore",
			!entity_undefined_p(els1));

	    pc = CHAIN_SWORD(pc, ls1);
	    add_target_to_alternate_return_set(els1);
	  }
	}
      }
      else {
	/* words_expression cannot be called because of the C comma
	   operator which require surrounding parentheses in this
	   context. Be careful with unary minus. */
	pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(pa)),
					       ASSIGN_OPERATOR_PRECEDENCE,
					       TRUE/*FALSE*/,
					       pdl));
      }
      if (CDR(pa) != NIL)
	pc = CHAIN_SWORD(pc, space_p? ", ": ",");
    }

    pc = CHAIN_SWORD(pc, ")");
  }
  else if(!type_void_p(functional_result(type_functional(t))) ||
	  !is_a_subroutine || prettyprint_language_is_c_p()) {
    pc = CHAIN_SWORD(pc, "()");
  }

  return pc;
}


/* To deal with attachment on user module usage. */
static list words_genuine_regular_call(call obj, bool is_a_subroutine, list pdl)
{
  list pc = words_regular_call(obj, is_a_subroutine, pdl);

  if (call_arguments(obj) != NIL) {
    /* The call is not used to code a constant: */
    //entity f = call_function(obj);
    //type t = entity_type(f);
    /* The module name is the first one except if it is a procedure CALL. */
    if (type_void_p(functional_result(type_functional(call_compatible_type(entity_type(call_function(obj)))))))
      attach_regular_call_to_word(STRING(CAR(CDR(pc))), obj);
    else
      attach_regular_call_to_word(STRING(CAR(pc)), obj);
  }

  return pc;
}

static list
words_call_intrinsic(call obj,
		     int __attribute__ ((unused)) precedence,
		     bool __attribute__ ((unused)) leftmost,
		     list pdl)
{
  return words_regular_call(obj, TRUE, pdl);
}

static list
words_assign_op(call obj,
		int precedence,
		bool __attribute__ ((unused)) leftmost,
		list pdl)
{
  list pc = NIL, args = call_arguments(obj);
  int prec = words_intrinsic_precedence(obj);
  string fun = entity_local_name(call_function(obj));

  pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(args)), prec, TRUE, pdl));

  if (strcmp(fun, MODULO_UPDATE_OPERATOR_NAME) == 0)
    fun = "%=";
  else if (strcmp(fun, BITWISE_AND_UPDATE_OPERATOR_NAME) == 0)
    fun = "&=";
  else if (strcmp(fun, BITWISE_XOR_UPDATE_OPERATOR_NAME) == 0)
    fun = "^=";

  /* FI: space_p could be used here to control spacing around assignment */
  pc = CHAIN_SWORD(pc," ");
  pc = CHAIN_SWORD(pc, fun);
  pc = CHAIN_SWORD(pc," ");
  expression exp = expression_undefined;
  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      exp = EXPRESSION(CAR(CDR(args)));
      if (expression_call_p(exp)) {
        /* = is not a Fortran operator. No need for parentheses ever,
         even with the parenthesis option */
        /*
         call c = syntax_call(expression_syntax(e));
         pc = gen_nconc(pc, words_call(c, 0, TRUE, TRUE, pdl));
         */
        pc = gen_nconc(pc, words_syntax(expression_syntax(exp), pdl));
      } else
        pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(CDR(args))),
                                               prec,
                                               TRUE,
                                               pdl));
      break;
    case is_language_c:
      /* Brace expressions are not allowed in standard assignments */
      exp = EXPRESSION(CAR(CDR(args)));
      if (ENTITY_ASSIGN_P(call_function(obj))) {
        if (brace_expression_p(exp))
          //pc = gen_nconc(pc,words_brace_expression(exp));
          pips_user_error("Brace expressions are not allowed in assignments\n");
        else {
          /* Be careful with expression lists, they may require
           surrounding parentheses. */
          pc = gen_nconc(pc, words_subexpression(exp, prec, TRUE, pdl));
        }
      } else {
        pc = gen_nconc(pc, words_subexpression(exp, prec, TRUE, pdl));
      }
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }
  if (prec < precedence || (!precedence_p && precedence > 0)) {
    pc = CONS(STRING, MAKE_SWORD("("), pc);
    pc = CHAIN_SWORD(pc, ")");
  }
  return (pc);
}
static list
words_substring_op(call obj,
		   int __attribute__ ((unused)) precedence,
		   bool __attribute__ ((unused)) leftmost,
		   list pdl) {
  /* The substring function call is reduced to a syntactic construct */
    list pc = NIL;
    expression r = expression_undefined;
    expression l = expression_undefined;
    expression u = expression_undefined;
    /* expression e = EXPRESSION(CAR(CDR(CDR(CDR(call_arguments(obj)))))); */
    int prec = words_intrinsic_precedence(obj);

    pips_assert("words_substring_op", gen_length(call_arguments(obj)) == 3 ||
		gen_length(call_arguments(obj)) == 4);

    r = EXPRESSION(CAR(call_arguments(obj)));
    l = EXPRESSION(CAR(CDR(call_arguments(obj))));
    u = EXPRESSION(CAR(CDR(CDR(call_arguments(obj)))));

    pc = gen_nconc(pc, words_subexpression(r,  prec, TRUE, pdl));
    pc = CHAIN_SWORD(pc, "(");
    pc = gen_nconc(pc, words_subexpression(l, prec, TRUE, pdl));
    pc = CHAIN_SWORD(pc, ":");

    /* An unknown upper bound is encoded as a call to
       UNBOUNDED_DIMENSION_NAME and nothing must be printed */
    if(syntax_call_p(expression_syntax(u))) {
	entity star = call_function(syntax_call(expression_syntax(u)));
	if(star!=CreateIntrinsic(UNBOUNDED_DIMENSION_NAME))
	  pc = gen_nconc(pc, words_subexpression(u, prec, TRUE, pdl));
    }
    else {
      pc = gen_nconc(pc, words_subexpression(u, prec, TRUE, pdl));
    }
    pc = CHAIN_SWORD(pc, ")");

    return(pc);
}

static list
words_assign_substring_op(call obj,
			  int __attribute__ ((unused)) precedence,
			  bool __attribute__ ((unused)) leftmost,
			  list pdl)
{
  /* The assign substring function call is reduced to a syntactic construct */
    list pc = NIL;
    expression e = expression_undefined;
    int prec = words_intrinsic_precedence(obj);

    pips_assert("words_substring_op", gen_length(call_arguments(obj)) == 4);

    e = EXPRESSION(CAR(CDR(CDR(CDR(call_arguments(obj))))));

    pc = gen_nconc(pc, words_substring_op(obj,  prec, TRUE, pdl));
    pc = CHAIN_SWORD(pc, " = ");
    pc = gen_nconc(pc, words_subexpression(e, prec, TRUE, pdl));

    return(pc);
}

/**
 * @return the external string representation of the operator
 * @param name, the pips internal representation of the operator
 */
static string renamed_op_handling (string name) {
  string result = name;

  if ( strcmp(result,PLUS_C_OPERATOR_NAME) == 0 )
    result = "+";
  else  if ( strcmp(result, MINUS_C_OPERATOR_NAME) == 0 )
    result = "-";
  else  if ( strcmp(result,BITWISE_AND_OPERATOR_NAME) == 0 )
    result = "&";
  else  if ( strcmp(result,BITWISE_XOR_OPERATOR_NAME) == 0 )
    result = "^";
  else  if ( strcmp(result,C_AND_OPERATOR_NAME) == 0 )
    result = "&&";
  else  if ( strcmp(result,C_NON_EQUAL_OPERATOR_NAME) == 0 )
    result = "!=";
  else  if ( strcmp(result,C_MODULO_OPERATOR_NAME) == 0 )
    result = "%";
  else if (prettyprint_language_is_c_p()) {
    if(strcasecmp(result, GREATER_THAN_OPERATOR_NAME)==0)
      result=C_GREATER_THAN_OPERATOR_NAME;
    else if(strcasecmp(result, LESS_THAN_OPERATOR_NAME)==0)
      result=C_LESS_THAN_OPERATOR_NAME;
    else if(strcasecmp(result,GREATER_OR_EQUAL_OPERATOR_NAME)==0)
      result=C_GREATER_OR_EQUAL_OPERATOR_NAME;
    else if(strcasecmp(result,LESS_OR_EQUAL_OPERATOR_NAME)==0)
      result=C_LESS_OR_EQUAL_OPERATOR_NAME;
    else if(strcasecmp(result, EQUAL_OPERATOR_NAME) ==0)
      result=C_EQUAL_OPERATOR_NAME;
    else if(strcasecmp(result,NON_EQUAL_OPERATOR_NAME)==0)
      result= "!=";
    else if(strcasecmp(result,AND_OPERATOR_NAME)==0)
      result="&&";
    else if(strcasecmp(result, OR_OPERATOR_NAME)==0)
      result=C_OR_OPERATOR_NAME;
  }
  return result;
}

/** @return a list of string with the prettyprint of a omp reduction clause
 */
static list
words_omp_red(call obj,
	      int precedence __attribute__ ((unused)),
	      bool leftmost __attribute__ ((unused)),
	      list pdl)
{
  list result = NIL;
  entity fct = call_function(obj);
  result = CHAIN_SWORD(result, entity_user_name(fct));
  result = CHAIN_SWORD(result, "(");
  // the reduction arguments as an expression list
  list args = call_arguments (obj);
  pips_assert ("no arguments for reduction clause", args != NIL);
  int nb_arg = 0;
  FOREACH (EXPRESSION, arg, args) {
    if (nb_arg == 0) {
      // the first argument is an operator and need to be handle separately
      // because of the intenal management of operator
      string op;
      syntax syn = expression_syntax (arg);
      pips_assert ("should be a reference", syntax_tag (syn) == is_syntax_reference);
      op = entity_local_name (reference_variable (syntax_reference (syn)));
      op = renamed_op_handling (op);
      CHAIN_SWORD(result, op);
    }
    else { // (nb_arg != 0)
      result = (nb_arg == 1)? CHAIN_SWORD(result,":") : CHAIN_SWORD(result,",");
      result = gen_nconc (result, words_expression(arg, pdl));
    }
    nb_arg++;
  }
  pips_assert ("reduction clause has at least two arguments", nb_arg > 1);
  result = CHAIN_SWORD(result, ")");
  return result;
}

// Function written by C.A. Mensi to prettyprint C or Fortran code as C code
static list
words_nullary_op_c(call obj,
		   int precedence __attribute__ ((unused)),
		   bool leftmost __attribute__ ((unused)),
		   list pdl)
{
  list pc = NIL;
  list args = call_arguments(obj);
  entity func = call_function(obj);
  string fname = entity_local_name(func);
  int nargs = gen_length(args);
  bool parentheses_p=TRUE;

  /* STOP and PAUSE and RETURN in Fortran may have 0 or 1 argument.
     STOP and PAUSE are prettyprinted in C using PIPS specific C functions. */

  if(nargs==0){
    if(same_string_p(fname,STOP_FUNCTION_NAME))
      pc = CHAIN_SWORD(pc, "exit(0)");
    else if(same_string_p(fname,RETURN_FUNCTION_NAME)
	    ||same_string_p(fname,C_RETURN_FUNCTION_NAME))
      pc = CHAIN_SWORD(pc, "return");
    else if(same_string_p(fname,PAUSE_FUNCTION_NAME))
      pc = CHAIN_SWORD(pc, "_f77_intrinsics_pause_(0)");
    else if(same_string_p(fname,CONTINUE_FUNCTION_NAME))
      pc = CHAIN_SWORD(pc, "");
    else if ((same_string_p(fname,OMP_OMP_FUNCTION_NAME)) ||
	     (same_string_p(fname,OMP_FOR_FUNCTION_NAME)) ||
	     (same_string_p(fname,OMP_PARALLEL_FUNCTION_NAME)))
      pc = CHAIN_SWORD(pc, fname);
    else
      pips_internal_error("Unknown nullary operator");
  }
  else if(nargs==1){
    expression e = EXPRESSION(CAR(args));

    if(same_string_p(fname,STOP_FUNCTION_NAME)){
      basic b=expression_basic(e);
      if(basic_int_p(b)){
	// Missing: declaration of exit() if Fortran code handled
	pc = CHAIN_SWORD(pc, "exit");
      }
      else if(basic_string_p(b)){
	pc = CHAIN_SWORD(pc, "_f77_intrinsics_stop_");
      }
    }
    else if(same_string_p(fname,RETURN_FUNCTION_NAME)
	    ||same_string_p(fname,C_RETURN_FUNCTION_NAME)){
      pc = CHAIN_SWORD(pc, "return");
      parentheses_p = FALSE;
      //pips_user_error("alternate returns are not supported in C\n");
    }
    else if(same_string_p(fname, PAUSE_FUNCTION_NAME)){
      pc = CHAIN_SWORD(pc, "_f77_intrinsics_pause_");
    }
    else {
      pips_internal_error("unexpected one argument");
    }
    pc = CHAIN_SWORD(pc, parentheses_p?"(":" ");
    pc = gen_nconc(pc, words_subexpression(e, precedence, TRUE, pdl));
    pc = CHAIN_SWORD(pc, parentheses_p?")":"");
  }
  else {
    pips_internal_error("unexpected arguments");
  }
  return(pc);
}

// function added for fortran  by A. Mensi
static list words_nullary_op_fortran(call obj,
				     int precedence,
				     bool __attribute__ ((unused)) leftmost,
				     list pdl)
{
  list pc = NIL;
  list args = call_arguments(obj);
  entity func = call_function(obj);
  string fname = entity_local_name(func);

  if(same_string_p(fname,RETURN_FUNCTION_NAME)
     ||same_string_p(fname,C_RETURN_FUNCTION_NAME))
    pc = CHAIN_SWORD(pc, RETURN_FUNCTION_NAME);
  else if (same_string_p(fname,OMP_FOR_FUNCTION_NAME))
    pc = CHAIN_SWORD(pc, "do");
  else
    pc = CHAIN_SWORD(pc, fname);

  // STOP and PAUSE and RETURN in fortran may have 0 or 1 argument.A Mensi
  if(gen_length(args)==1) {
    if(same_string_p(fname,STOP_FUNCTION_NAME)
       || same_string_p(fname,PAUSE_FUNCTION_NAME)
       || same_string_p(fname,RETURN_FUNCTION_NAME)
       || same_string_p(fname, C_RETURN_FUNCTION_NAME)) {
      expression e = EXPRESSION(CAR(args));
      pc = CHAIN_SWORD(pc, " ");
      pc = gen_nconc(pc, words_subexpression(e, precedence, TRUE, pdl));
    }
    else {
      pips_internal_error("unexpected arguments");
    }
  }
  else if(gen_length(args)>1) {
    pips_internal_error("unexpected arguments");
  }

  return(pc);
}


static list words_nullary_op(call obj,
                             int precedence,
                             bool __attribute__ ((unused)) leftmost,
                             list pdl) {
  list result = NIL;
  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      result = words_nullary_op_fortran(obj, precedence, leftmost, pdl);
      break;
    case is_language_c:
      result = words_nullary_op_c(obj, precedence, leftmost, pdl);
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }
  return result;
}


static list
words_io_control(list *iol,
		 int __attribute__ ((unused)) precedence,
		 bool __attribute__ ((unused)) leftmost,
		 list pdl)
{
    list pc = NIL;
    list pio = *iol;

    while (pio != NIL) {
	syntax s = expression_syntax(EXPRESSION(CAR(pio)));
	call c;

	if (! syntax_call_p(s)) {
	    pips_error("words_io_control", "call expected");
	}

	c = syntax_call(s);

	if (strcmp(entity_local_name(call_function(c)), IO_LIST_STRING_NAME) == 0) {
	    *iol = CDR(pio);
	    return(pc);
	}

	if (pc != NIL)
	    pc = CHAIN_SWORD(pc, ",");

	pc = CHAIN_SWORD(pc, entity_local_name(call_function(c)));
	pc = gen_nconc(pc, words_expression(EXPRESSION(CAR(CDR(pio))), pdl));

	pio = CDR(CDR(pio));
    }

    if (pio != NIL)
	    pips_error("words_io_control", "bad format");

    *iol = NIL;

    return(pc);
}

static list
words_implied_do(call obj,
		 int __attribute__ ((unused)) precedence,
		 bool __attribute__ ((unused)) leftmost,
		 list pdl)
{
    list pc = NIL;
    list pcc;
    expression index;
    syntax s;
    range r;
    bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

    pcc = call_arguments(obj);
    index = EXPRESSION(CAR(pcc));

    pcc = CDR(pcc);
    s = expression_syntax(EXPRESSION(CAR(pcc)));
    if (! syntax_range_p(s)) {
	pips_error("words_implied_do", "range expected");
    }
    r = syntax_range(s);

    pc = CHAIN_SWORD(pc, "(");
    MAPL(pcp, {
	pc = gen_nconc(pc, words_expression(EXPRESSION(CAR(pcp)), pdl));
	if (CDR(pcp) != NIL)
	    pc = CHAIN_SWORD(pc, space_p? ", " : ",");
    }, CDR(pcc));
    pc = CHAIN_SWORD(pc, space_p? ", " : ",");

    pc = gen_nconc(pc, words_expression(index, pdl));
    pc = CHAIN_SWORD(pc, " = ");
    pc = gen_nconc(pc, words_loop_range(r, pdl));
    pc = CHAIN_SWORD(pc, ")");

    return(pc);
}

static list
words_unbounded_dimension(call __attribute__ ((unused)) obj,
			  int __attribute__ ((unused)) precedence,
			  bool __attribute__ ((unused)) leftmost)
{
    list pc = NIL;

    pc = CHAIN_SWORD(pc, "*");

    return(pc);
}

static list
words_list_directed(call __attribute__ ((unused)) obj,
		    int __attribute__ ((unused)) precedence,
		    bool __attribute__ ((unused)) leftmost)
{
    list pc = NIL;

    pc = CHAIN_SWORD(pc, "*");

    return(pc);
}

static list
words_io_inst(call obj,
	      int precedence, bool leftmost, list pdl)
{
  list pc = NIL;
  list pcio = call_arguments(obj);
  list pio_write = pcio;
  boolean good_fmt = FALSE;
  bool good_unit = FALSE;
  bool iolist_reached = FALSE;
  bool complex_io_control_list = FALSE;
  expression fmt_arg = expression_undefined;
  expression unit_arg = expression_undefined;
  string called = entity_local_name(call_function(obj));
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  /* AP: I try to convert WRITE to PRINT. Three conditions must be
   fullfilled. The first, and obvious, one, is that the function has
   to be WRITE. Secondly, "FMT" has to be equal to "*". Finally,
   "UNIT" has to be equal either to "*" or "6".  In such case,
   "WRITE(*,*)" is replaced by "PRINT *,". */
  /* GO: Not anymore for UNIT=6 leave it ... */
  while((pio_write != NIL) && (!iolist_reached)) {
    syntax s = expression_syntax(EXPRESSION(CAR(pio_write)));
    call c;
    expression arg = EXPRESSION(CAR(CDR(pio_write)));

    if(!syntax_call_p(s)) {
      pips_internal_error("call expected");
    }

    c = syntax_call(s);
    if(strcmp(entity_local_name(call_function(c)), "FMT=") == 0) {
      /* Avoid to use words_expression(arg) because it set some
       attachments and unit_words may not be used
       later... RK. */
      entity f;
      /* The * format is coded as a call to "LIST_DIRECTED_FORMAT_NAME" function: */
      good_fmt = syntax_call_p(expression_syntax(arg))
          && value_intrinsic_p(entity_initial(f =
                  call_function(syntax_call(expression_syntax(arg)))))
          && (strcmp(entity_local_name(f), LIST_DIRECTED_FORMAT_NAME) == 0);
      pio_write = CDR(CDR(pio_write));
      /* To display the format later: */
      fmt_arg = arg;
    } else if(strcmp(entity_local_name(call_function(c)), "UNIT=") == 0) {
      /* Avoid to use words_expression(arg) because it set some
       attachments and unit_words may not be used
       later... RK. */
      entity f;
      /* The * format is coded as a call to "LIST_DIRECTED_FORMAT_NAME" function: */
      good_unit = syntax_call_p(expression_syntax(arg))
          && value_intrinsic_p(entity_initial(f =
                  call_function(syntax_call(expression_syntax(arg)))))
          && (strcmp(entity_local_name(f), LIST_DIRECTED_FORMAT_NAME) == 0);
      /* To display the unit later: */
      unit_arg = arg;
      pio_write = CDR(CDR(pio_write));
    } else if(strcmp(entity_local_name(call_function(c)), IO_LIST_STRING_NAME)
        == 0) {
      iolist_reached = TRUE;
      pio_write = CDR(pio_write);
    } else {
      complex_io_control_list = TRUE;
      pio_write = CDR(CDR(pio_write));
    }
  }

  if(good_fmt && good_unit && same_string_p(called, "WRITE")) {
    /* WRITE (*,*) -> PRINT * */

    if(pio_write != NIL) /* WRITE (*,*) pio -> PRINT *, pio */
    {
      pc = CHAIN_SWORD(pc, "PRINT *, ");
    } else /* WRITE (*,*)  -> PRINT *  */
    {
      pc = CHAIN_SWORD(pc, "PRINT * ");
    }

    pcio = pio_write;
  } else if(good_fmt && good_unit && same_string_p(called, "READ")) {
    /* READ (*,*) -> READ * */

    if(pio_write != NIL) /* READ (*,*) pio -> READ *, pio */
    {
      switch(get_prettyprint_language_tag()) {
        case is_language_fortran:
        case is_language_fortran95:
          pc = CHAIN_SWORD(pc, "READ *, ");
          break;
        case is_language_c:
          pc = CHAIN_SWORD(pc, "_f77_intrinsics_read_(");
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    } else /* READ (*,*)  -> READ *  */
    {
      pc = CHAIN_SWORD(pc, "READ * ");
    }
    pcio = pio_write;
  } else if(!complex_io_control_list) {
    list unit_words = words_expression(unit_arg, pdl);
    pips_assert("A unit must be defined", !ENDP(unit_words));
    pc = CHAIN_SWORD(pc, entity_local_name(call_function(obj)));
    pc = CHAIN_SWORD(pc, " (");
    pc = gen_nconc(pc, unit_words);

    if(!expression_undefined_p(fmt_arg)) {
      /* There is a FORMAT: */
      pc = CHAIN_SWORD(pc, space_p? ", " : ",");
      pc = gen_nconc(pc, words_expression(fmt_arg, pdl));
    }

    pc = CHAIN_SWORD(pc, ") ");
    pcio = pio_write;
  } else {
    pc = CHAIN_SWORD(pc, entity_local_name(call_function(obj)));
    pc = CHAIN_SWORD(pc, " (");
    /* FI: missing argument; I use "precedence" because I've no clue;
     see LZ */
    pc = gen_nconc(pc, words_io_control(&pcio, precedence, leftmost, pdl));
    pc = CHAIN_SWORD(pc, ") ");
    /*
     free_words(fmt_words);
     */
  }

  /* because the "IOLIST=" keyword is embedded in the list
   and because the first IOLIST= has already been skipped,
   only odd elements are printed */
  MAPL(pp, {
        pc = gen_nconc(pc, words_expression(EXPRESSION(CAR(pp)), pdl));
        if (CDR(pp) != NIL) {
          POP(pp);
          if(pp==NIL)
          pips_internal_error("missing element in IO list");
          pc = CHAIN_SWORD(pc, space_p? ", " : ",");
        }
      }, pcio);

  if(prettyprint_language_is_c_p())
    pc = CHAIN_SWORD(pc, ") ");

  return (pc);
}


/**
 *  Implemented for ALLOCATE(), but is applicable for every call to
 *  function that take STAT= parameter
 */
static list words_stat_io_inst(call obj,
                               int __attribute__((unused)) precedence,
                               bool __attribute__((unused)) leftmost,
                               list pdl) {
  list pc = NIL;
  list pcio = call_arguments(obj);
  list pio_write = pcio;
  string called = entity_local_name(call_function(obj));
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  /* Write call function */
  pc = CHAIN_SWORD(pc, called);
  pc = CHAIN_SWORD(pc, " (");

  while ( ( pio_write != NIL ) ) {
    expression expr = EXPRESSION(CAR(pio_write));
    syntax s = expression_syntax(expr);
    call c;

    if ( syntax_call_p(s) ) { /* STAT= is a call */
      c = syntax_call(s);
      if ( strcmp( entity_local_name( call_function(c) ), "STAT=" ) == 0 ) {
        /* We got it ! */
        pc = CHAIN_SWORD(pc, strdup("STAT=")); /* FIXME : strdup ? */
        /* get argument */
        pio_write = CDR(pio_write);
        expression arg = EXPRESSION(CAR(pio_write));
        pc = gen_nconc( pc, words_expression( arg, pdl ) );
      }
    } else { /* It's not a call */
      pc = gen_nconc( pc, words_expression( expr, pdl ) );
    }
    pio_write = CDR(pio_write);
    if(pio_write) {
      pc = CHAIN_SWORD(pc, space_p? ", " : ",");
    }
  }

  pc = CHAIN_SWORD(pc, ") ");

  return ( pc );
}


static list
null(call __attribute__ ((unused)) obj,
     int __attribute__ ((unused)) precedence,
     bool __attribute__ ((unused)) leftmost)
{
    return(NIL);
}

static list
words_prefix_unary_op(call obj,
		      int  precedence,
		      bool __attribute__ ((unused)) leftmost,
		      list pdl)
{
  list pc = NIL;
  expression e = EXPRESSION(CAR(call_arguments(obj)));
  int prec = words_intrinsic_precedence(obj);
  string fun = entity_local_name(call_function(obj));
  if (strcmp(fun,PRE_INCREMENT_OPERATOR_NAME) == 0)
    fun = "++";
  else if (strcmp(fun,PRE_DECREMENT_OPERATOR_NAME) == 0)
    fun = "--";
  else if (strcmp(fun,ADDRESS_OF_OPERATOR_NAME) == 0)
    fun = "&";
  else if (strcmp(fun,C_NOT_OPERATOR_NAME) == 0)
    fun = "!";
  else if (strcmp(fun,BITWISE_NOT_OPERATOR_NAME) == 0)
    fun = "~";
  else if (strcmp(fun,DEREFERENCING_OPERATOR_NAME) == 0)
      /* Since we put no spaces around an operator (to not change Fortran), the blank
	 before '*' is used to avoid the confusion in the case of divide operator, i.e
	 d1 = 1.0 / *det  in function inv_j, SPEC2000 quake benchmark.

	 But we do not want this in a lhs and espcially with a double dereferencing. */
    fun = "*";
  else if(prettyprint_language_is_c_p()){
	if(strcasecmp(fun, NOT_OPERATOR_NAME)==0)
	  fun="!";
	if(strcasecmp(fun, UNARY_PLUS_OPERATOR_NAME)==0) {
	  /* You do not want to transform +1 + +1 into +1++ 1 */
	  /* Maybe the precedence could be useful to avoid adding a
	     useless SPACE, but unary plus is rare enough to reduce
	     the ROI of such anoptimization to zero. */
	  fun=" +";
	}
      }

  pc = CHAIN_SWORD(pc,fun);
  pc = gen_nconc(pc, words_subexpression(e, prec, FALSE, pdl));

  if(prec < precedence ||  (!precedence_p && precedence>0)) {
    pc = CONS(STRING, MAKE_SWORD("("), pc);
    pc = CHAIN_SWORD(pc, ")");
  }

  return(pc);
}

static list
words_postfix_unary_op(call obj,
		       int  precedence,
		       bool __attribute__ ((unused)) leftmost,
		       list pdl)
{
    list pc = NIL;
    expression e = EXPRESSION(CAR(call_arguments(obj)));
    int prec = words_intrinsic_precedence(obj);
    string fun = entity_local_name(call_function(obj));

    pc = gen_nconc(pc, words_subexpression(e, prec, FALSE, pdl));

    if (strcmp(fun,POST_INCREMENT_OPERATOR_NAME) == 0)
      fun = "++";
    else if (strcmp(fun,POST_DECREMENT_OPERATOR_NAME) == 0)
     fun = "--";

    pc = CHAIN_SWORD(pc,fun);

    if(prec < precedence ||  (!precedence_p && precedence>0)) {
      pc = CONS(STRING, MAKE_SWORD("("), pc);
      pc = CHAIN_SWORD(pc, ")");
    }

    return(pc);
}


static list
words_unary_minus(call obj, int precedence, bool leftmost, list pdl)
{
    list pc = NIL;
    expression e = EXPRESSION(CAR(call_arguments(obj)));
    int prec = words_intrinsic_precedence(obj);

    if ( prec < precedence || !leftmost ||  !precedence_p)
	pc = CHAIN_SWORD(pc, "(");
    pc = CHAIN_SWORD(pc, "-");
    pc = gen_nconc(pc, words_subexpression(e, prec, FALSE, pdl));
    if ( prec < precedence || !leftmost ||  !precedence_p)
	pc = CHAIN_SWORD(pc, ")");

    return(pc);
}

/*
   The precedence of (1/x) is the same as the multiply operator
   (e.g. a*1/b without parentheses). Moreover, the MAXIMAL precedence is
   used for the (x) subterm (e.g. 1/(a*b) 1/(-2) ...). However, 1/x**2 may
   be a correct prettyprint in Fortran (?) */
/* WARNING : the floating point division is used wether b is an int or not
   ! (1.0/b) -- in fact b should not be an int ! */
static list /* of string */
words_inverse_op(call obj,
		 int precedence,
		 bool __attribute__ ((unused)) leftmost,
		 list pdl)
{
  list /* of string */ pc = NIL;

  expression e = EXPRESSION(CAR(call_arguments(obj)));
  int prec = words_intrinsic_precedence(obj);

  if ( prec < precedence)
    pc = CHAIN_SWORD(pc, "(");
  pc = CHAIN_SWORD(pc, "1./");
  pc = gen_nconc(pc, words_subexpression(e, MAXIMAL_PRECEDENCE ,
					 FALSE, pdl));

  if ( prec < precedence)
    pc = CHAIN_SWORD(pc, ")");

  return(pc);
}

list /* of string */
words_goto_label(string tlabel)
{
    list pc = NIL;
  if (strcmp(tlabel, RETURN_LABEL_NAME) == 0) {
    pc = CHAIN_SWORD(pc, RETURN_FUNCTION_NAME);
  } else {
    switch (get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        pc = CHAIN_SWORD(pc, strdup("GOTO "));
        pc = CHAIN_SWORD(pc, tlabel);
        break;
      case is_language_c:
        /* In C, a label cannot begin with a number so "l" is added for this case*/
        pc = CHAIN_SWORD(pc, strdup((isdigit(tlabel[0])?"goto l":"goto ")));
        pc = CHAIN_SWORD(pc, tlabel);
        pc = CHAIN_SWORD(pc, C_CONTINUE_FUNCTION_NAME);
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }
  return pc;
}

static list
eole_fmx_specific_op(call obj,
		     int __attribute__ ((unused)) precedence,
		     bool __attribute__ ((unused)) leftmost,
		     bool isadd,
		     list pdl)
{
  list /* of strings */ pc = NIL;
  list /* of expressions */ args = call_arguments(obj);

  int prec ;

  /* open parenthese one  */
  pc = CHAIN_SWORD(pc, "(");

  /* open parenthese two */
  pc = CHAIN_SWORD(pc, "(");

  /* get precedence for mult operator */
  prec = intrinsic_precedence("*");

  /* first argument */
  pc = gen_nconc(pc,words_subexpression(EXPRESSION(CAR(args)), prec, TRUE, pdl));

  /* mult operator */
  pc = CHAIN_SWORD(pc,"*");

  /* second argument */
  args = CDR(args);
  pc = gen_nconc(pc,words_subexpression(EXPRESSION(CAR(args)),prec,TRUE, pdl));

  /* close parenthese two */
  pc = CHAIN_SWORD(pc, ")");

  /* get precedence for add operator */
  prec = intrinsic_precedence("+");

  /* add/sub operator */
  pc = CHAIN_SWORD(pc, isadd? "+": "-");

  /* third argument */
  args = CDR(args);
  pc = gen_nconc(pc,words_subexpression(EXPRESSION(CAR(args)),prec,FALSE, pdl));

  /* close parenthese one  */
  pc = CHAIN_SWORD(pc,")");

  return pc;
}

/* EOLE : The multiply-add operator is used within the optimize
   transformation ( JZ - sept 98) - fma(a,b,c) -> ((a*b)+c)
 */
list /* of string */
eole_fma_specific_op(call obj, int precedence, bool leftmost, list pdl)
{
  return eole_fmx_specific_op(obj, precedence, leftmost, TRUE, pdl);
}

/* MULTIPLY-SUB operator */
list /* of string */
eole_fms_specific_op(call obj, int precedence, bool leftmost, list pdl)
{
  return eole_fmx_specific_op(obj, precedence, leftmost, FALSE, pdl);
}

/* Check if the given operator is associated with a special
    prettyprint. For instance, n-ary add and multiply operators which are
    used in the EOLE project use "+" and "*" prettyprints instead of the
    entity_local_name (JZ - sept 98) */
static string
get_special_prettyprint_for_operator(call obj){

  static struct special_operator_prettyprint {
    char * name;
    char * op_prettyprint;
  } tab_operator_prettyprint[] = {
    {EOLE_SUM_OPERATOR_NAME,"+"},
    {EOLE_PROD_OPERATOR_NAME,"*"},
    {NULL,NULL}
  };
  int i = 0;
  string op_name;

  /* get the entity name */
  op_name = entity_local_name(call_function(obj));

  while (tab_operator_prettyprint[i].name) {
    if (!strcmp(tab_operator_prettyprint[i].name,op_name))
      return tab_operator_prettyprint[i].op_prettyprint;
    else i++;
  }

  return op_name;
}

/* Extension of "words_infix_binary_op" function for nary operators used
   in the EOLE project - (since "nary" assumes operators with at least 2
   op)  - JZ (Oct. 98)*/

static list /* of string */
words_infix_nary_op(call obj, int precedence, bool leftmost, list pdl)
{
  list /*of string*/ pc = NIL;
  list /* of expressions */ args = call_arguments(obj);

  /* get current operator precedence */
  int prec = words_intrinsic_precedence(obj);

  expression exp1 = EXPRESSION(CAR(args));
  expression exp2;

  list we1 = words_subexpression(exp1, prec,
				 prec>=MINIMAL_ARITHMETIC_PRECEDENCE? leftmost: TRUE, pdl);
  list we2;

  /* open parenthese if necessary */
  if ( prec < precedence )
    pc = CHAIN_SWORD(pc, "(");
  pc = gen_nconc(pc, we1);

  /* reach the second arg */
  args = CDR(args);

  for(; args; args=CDR(args)) { /* for all args */
    exp2 = EXPRESSION(CAR(args));

    /*
     * If the infix operator is either "-" or "/", I prefer not to delete
     * the parentheses of the second expression.
     * Ex: T = X - ( Y - Z ) and T = X / (Y*Z)
     *
     * Lei ZHOU       Nov. 4 , 1991
     */
    if ( strcmp(entity_local_name(call_function(obj)), "/") == 0 )  /* divide operator */
      we2 = words_subexpression(exp2, MAXIMAL_PRECEDENCE, FALSE, pdl);
    else if ( strcmp(entity_local_name(call_function(obj)), "-") == 0 ) { /* minus operator */
      if ( expression_call_p(exp2) &&
	   words_intrinsic_precedence(syntax_call(expression_syntax(exp2))) >=
	   intrinsic_precedence("*") )
	/* precedence is greater than * or / */
	we2 = words_subexpression(exp2, prec, FALSE, pdl);
      else
	we2 = words_subexpression(exp2, MAXIMAL_PRECEDENCE, FALSE, pdl);
    }
    else {
      we2 = words_subexpression(exp2, prec,
				prec<MINIMAL_ARITHMETIC_PRECEDENCE, pdl);
    }

    /* operator prettyprint */
    pc = CHAIN_SWORD(pc, get_special_prettyprint_for_operator(obj));

    pc = gen_nconc(pc, we2);
  }
  /* close parenthese if necessary */
  if ( prec < precedence )
    pc = CHAIN_SWORD(pc, ")");

  return(pc);
}

/*
 * If the infix operator is either "-" or "/", I prefer not to delete
 * the parentheses of the second expression.
 * Ex: T = X - ( Y - Z ) and T = X / (Y*Z)
 *
 * Lei ZHOU       Nov. 4 , 1991
 */
static list
words_infix_binary_op(call obj, int precedence, bool leftmost, list pdl)
{
  list pc = NIL;
  list args = call_arguments(obj);
  int prec = words_intrinsic_precedence(obj);
  list we1 = words_subexpression(EXPRESSION(CAR(args)), prec,
				 prec>=MINIMAL_ARITHMETIC_PRECEDENCE? leftmost: TRUE, pdl);
  list we2;
  string fun = entity_local_name(call_function(obj));

  /* handling of internally renamed operators */
  fun = renamed_op_handling (fun);

  if(strcmp(fun, DIVIDE_OPERATOR_NAME) == 0) {
    /* Do we want to add a space in case we2 starts with a dereferencing operator "*"?
     Nga suggests to look at the quake benchmark of SPEC2000. */
    we2 = words_subexpression(EXPRESSION(CAR(CDR(args))), MAXIMAL_PRECEDENCE, FALSE, pdl);
  }
  else if (strcmp(fun, MINUS_OPERATOR_NAME) == 0 ) {
    expression exp = EXPRESSION(CAR(CDR(args)));
    if(expression_call_p(exp) &&
       words_intrinsic_precedence(syntax_call(expression_syntax(exp))) >=
       intrinsic_precedence(MULTIPLY_OPERATOR_NAME) )
      /* precedence is greater than * or / */
      we2 = words_subexpression(exp, prec, FALSE, pdl);
    else
      we2 = words_subexpression(exp, MAXIMAL_PRECEDENCE, FALSE, pdl);
  }
  else if(strcmp(fun, MULTIPLY_OPERATOR_NAME) == 0) {
    expression exp = EXPRESSION(CAR(CDR(args)));
    if(expression_call_p(exp) &&
       ENTITY_DIVIDE_P(call_function(syntax_call(expression_syntax(exp))))) {
      basic bexp = basic_of_expression(exp);

      if(basic_int_p(bexp)) {
	we2 = words_subexpression(exp, MAXIMAL_PRECEDENCE, FALSE, pdl);
      }
      else
	we2 = words_subexpression(exp, prec, FALSE, pdl);
      free_basic(bexp);
    }
    else
      we2 = words_subexpression(exp, prec, FALSE, pdl);
  }
  else {
    we2 = words_subexpression(EXPRESSION(CAR(CDR(args))), prec,
			      prec<MINIMAL_ARITHMETIC_PRECEDENCE, pdl);
  }

  /* Use precedence to generate or not parentheses,
   * unless parentheses are always required */
  if(prec < precedence ||  !precedence_p) {
    pc = CHAIN_SWORD(pc, "(");
  }

  if(prettyprint_language_is_fortran95_p()
      && strcmp(fun, FIELD_OPERATOR_NAME) == 0) {
    pc = gen_nconc(pc, we1);
  } else {
    pc = gen_nconc(pc, we1);
    pc = CHAIN_SWORD(pc, strdup(fun));
    pc = gen_nconc(pc, we2);
  }

  if(prec < precedence || !precedence_p) {
    pc = CHAIN_SWORD(pc, ")");
  }

  return(pc);
}

/* Nga Nguyen : this case is added for comma expression in C, but I am
   not sure about its precedence => to be looked at more carefully */

static list words_comma_op(call obj,
			   int precedence,
			   bool __attribute__ ((unused)) leftmost,
			   list pdl)
{
  list pc = NIL, args = call_arguments(obj);
  int prec = words_intrinsic_precedence(obj);
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  if(prec < precedence || !precedence_p)
    pc = CHAIN_SWORD(pc,"(");
  pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(args)), prec, TRUE, pdl));
  while (!ENDP(CDR(args)))
  {
    pc = CHAIN_SWORD(pc,space_p?", " : ",");
    pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(CDR(args))), prec, TRUE, pdl));
    args = CDR(args);
  }
  if(prec < precedence || !precedence_p)
    pc = CHAIN_SWORD(pc,")");
  return(pc);
}

static list words_conditional_op(call obj,
				 int precedence,
				 bool __attribute__ ((unused)) leftmost,
				 list pdl)
{
  list pc = NIL, args = call_arguments(obj);
  int prec = words_intrinsic_precedence(obj);

  if(prec < precedence || !precedence_p)
    pc = CHAIN_SWORD(pc,"(");
  pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(args)), prec, TRUE, pdl));
  pc = CHAIN_SWORD(pc,"?");
  pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(CDR(args))), prec, TRUE, pdl));
  pc = CHAIN_SWORD(pc,":");
  pc = gen_nconc(pc, words_subexpression(EXPRESSION(CAR(CDR(CDR(args)))), prec, TRUE, pdl));
  if(prec < precedence || !precedence_p)
    pc = CHAIN_SWORD(pc,")");
  return(pc);
}


/* precedence needed here
 * According to the Precedence of Operators
 * Arithmetic > Character > Relational > Logical
 * Added by Lei ZHOU    Nov. 4,91
 *
 * A precedence is a integer in [0..MAXIMAL_PRECEDENCE]
 */

static struct intrinsic_handler {
    char * name;
    list (*f)();
    int prec;
} tab_intrinsic_handler[] = {
    {POWER_OPERATOR_NAME, words_infix_binary_op, 30},

    {CONCATENATION_FUNCTION_NAME, words_infix_binary_op, 30},

    /* The Fortran 77 standard does not allow x*-3 or x+-3, but this is dealt
    * with by argument leftmost, not by prorities.
    */
    {UNARY_MINUS_OPERATOR_NAME, words_unary_minus, 25},
    /* {"--", words_unary_minus, 19}, */

    {INVERSE_OPERATOR_NAME, words_inverse_op, 21},

    {PLUS_OPERATOR_NAME, words_infix_binary_op, 20},
    {MINUS_OPERATOR_NAME, words_infix_binary_op, 20},

    /* Non-arithemtic operators have priorities lesser than MINIMAL_ARITHMETIC_PRECEDENCE
     * leftmost is restaured to true for unary minus.
     */

    {LESS_THAN_OPERATOR_NAME, words_infix_binary_op, 15},
    {GREATER_THAN_OPERATOR_NAME, words_infix_binary_op, 15},
    {LESS_OR_EQUAL_OPERATOR_NAME, words_infix_binary_op, 15},
    {GREATER_OR_EQUAL_OPERATOR_NAME, words_infix_binary_op, 15},
    {EQUAL_OPERATOR_NAME, words_infix_binary_op, 15},
    {NON_EQUAL_OPERATOR_NAME, words_infix_binary_op, 15},

    {NOT_OPERATOR_NAME, words_prefix_unary_op, 9},

    {AND_OPERATOR_NAME, words_infix_binary_op, 8},

    {OR_OPERATOR_NAME, words_infix_binary_op, 6},

    {EQUIV_OPERATOR_NAME, words_infix_binary_op, 3},
    {NON_EQUIV_OPERATOR_NAME, words_infix_binary_op, 3},

    {ASSIGN_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},

    {ALLOCATE_FUNCTION_NAME, words_stat_io_inst, 0},
    {DEALLOCATE_FUNCTION_NAME, words_stat_io_inst, 0},
    {WRITE_FUNCTION_NAME, words_io_inst, 0},
    {READ_FUNCTION_NAME, words_io_inst, 0},
    {PRINT_FUNCTION_NAME, words_io_inst, 0},
    {OPEN_FUNCTION_NAME, words_io_inst, 0},
    {CLOSE_FUNCTION_NAME, words_io_inst, 0},
    {INQUIRE_FUNCTION_NAME, words_io_inst, 0},
    {BACKSPACE_FUNCTION_NAME, words_io_inst, 0},
    {REWIND_FUNCTION_NAME, words_io_inst, 0},
    {ENDFILE_FUNCTION_NAME, words_io_inst, 0},
    {IMPLIED_DO_FUNCTION_NAME, words_implied_do, 0},

    {RETURN_FUNCTION_NAME, words_nullary_op,0},
    {C_RETURN_FUNCTION_NAME, words_nullary_op,0},
    {PAUSE_FUNCTION_NAME, words_nullary_op,0 },
    {STOP_FUNCTION_NAME, words_nullary_op, 0},
    {CONTINUE_FUNCTION_NAME, words_nullary_op,0},
    {END_FUNCTION_NAME, words_nullary_op, 0},


    {FORMAT_FUNCTION_NAME, words_prefix_unary_op, 0},
    {UNBOUNDED_DIMENSION_NAME, words_unbounded_dimension, 0},
    {LIST_DIRECTED_FORMAT_NAME, words_list_directed, 0},

    {SUBSTRING_FUNCTION_NAME, words_substring_op, 0},
    {ASSIGN_SUBSTRING_FUNCTION_NAME, words_assign_substring_op, 0},

    /* These operators are used within the optimize transformation in
order to manipulate operators such as n-ary add and multiply or
multiply-add operators ( JZ - sept 98) */
    {EOLE_FMA_OPERATOR_NAME, eole_fma_specific_op,
                             MINIMAL_ARITHMETIC_PRECEDENCE },
    {EOLE_FMS_OPERATOR_NAME, eole_fms_specific_op,
                             MINIMAL_ARITHMETIC_PRECEDENCE },
    {EOLE_SUM_OPERATOR_NAME, words_infix_nary_op, 20},
    {EOLE_PROD_OPERATOR_NAME, words_infix_nary_op, 21},

    /* show IMA/IMS */
    {IMA_OPERATOR_NAME, eole_fma_specific_op,
	                         MINIMAL_ARITHMETIC_PRECEDENCE },
	{IMS_OPERATOR_NAME, eole_fms_specific_op,
	                         MINIMAL_ARITHMETIC_PRECEDENCE },

    /* 05/08/2003 - Nga Nguyen - Here are C intrinsics.
       The precedence is computed by using Table xx, page 49, book
       "The C programming language" of Kernighan and Ritchie, and by
       taking into account the precedence value of Fortran intrinsics. */

    {FIELD_OPERATOR_NAME, words_infix_binary_op, 30},
    {POINT_TO_OPERATOR_NAME, words_infix_binary_op, 30},
    {POST_INCREMENT_OPERATOR_NAME, words_postfix_unary_op, 30},
    {POST_DECREMENT_OPERATOR_NAME, words_postfix_unary_op, 30},

    {PRE_INCREMENT_OPERATOR_NAME,  words_prefix_unary_op, 25},
    {PRE_DECREMENT_OPERATOR_NAME,  words_prefix_unary_op, 25},
    {ADDRESS_OF_OPERATOR_NAME,     words_prefix_unary_op,25},
    {DEREFERENCING_OPERATOR_NAME,  words_prefix_unary_op, 25},
    {UNARY_PLUS_OPERATOR_NAME, words_prefix_unary_op, 25},
    /*{"-unary", words_prefix_unary_op, 25},*/
    {BITWISE_NOT_OPERATOR_NAME, words_prefix_unary_op, 25},
    {C_NOT_OPERATOR_NAME, words_prefix_unary_op, 25},

    /* What is the priority for CAST? 22? */

#define CAST_OPERATOR_PRECEDENCE (22)

    {C_MODULO_OPERATOR_NAME,  words_infix_binary_op, 21},
    {MULTIPLY_OPERATOR_NAME, words_infix_binary_op, 21},
    {DIVIDE_OPERATOR_NAME, words_infix_binary_op, 21},

    {PLUS_C_OPERATOR_NAME, words_infix_binary_op, 20},
    {MINUS_C_OPERATOR_NAME, words_infix_binary_op, 20},

    {LEFT_SHIFT_OPERATOR_NAME, words_infix_binary_op, 19},
    {RIGHT_SHIFT_OPERATOR_NAME, words_infix_binary_op, 19},

    {C_LESS_THAN_OPERATOR_NAME, words_infix_binary_op, 15 },
    {C_GREATER_THAN_OPERATOR_NAME, words_infix_binary_op, 15},
    {C_LESS_OR_EQUAL_OPERATOR_NAME, words_infix_binary_op, 15},
    {C_GREATER_OR_EQUAL_OPERATOR_NAME, words_infix_binary_op, 15},

    {C_EQUAL_OPERATOR_NAME, words_infix_binary_op, 14},
    {C_NON_EQUAL_OPERATOR_NAME, words_infix_binary_op, 14},

    {BITWISE_AND_OPERATOR_NAME, words_infix_binary_op, 13},
    {BITWISE_XOR_OPERATOR_NAME, words_infix_binary_op, 12},
    {BITWISE_OR_OPERATOR_NAME, words_infix_binary_op, 11},

    {C_AND_OPERATOR_NAME, words_infix_binary_op, 8},
    {C_OR_OPERATOR_NAME, words_infix_binary_op, 6},

    {MULTIPLY_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {DIVIDE_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {MODULO_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {PLUS_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {MINUS_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {LEFT_SHIFT_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {RIGHT_SHIFT_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {BITWISE_AND_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {BITWISE_XOR_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},
    {BITWISE_OR_UPDATE_OPERATOR_NAME, words_assign_op, ASSIGN_OPERATOR_PRECEDENCE},

    /* which precedence ? You are safe within an assignment. */
    {CONDITIONAL_OPERATOR_NAME, words_conditional_op, ASSIGN_OPERATOR_PRECEDENCE+1},

    /* which precedence ? You need parentheses within an assignment. */
    {COMMA_OPERATOR_NAME, words_comma_op, ASSIGN_OPERATOR_PRECEDENCE-1},

    /* OMP pragma function part */
    {OMP_OMP_FUNCTION_NAME,       words_nullary_op, 0},
    {OMP_FOR_FUNCTION_NAME,       words_nullary_op, 0},
    {OMP_PARALLEL_FUNCTION_NAME,  words_nullary_op, 0},
    {OMP_REDUCTION_FUNCTION_NAME, words_omp_red,    0},

#include "STEP_RT_intrinsic.h"

    {NULL, null, 0}
};

static list
words_intrinsic_call(call obj, int precedence, bool leftmost, list pdl)
{
    struct intrinsic_handler *p = tab_intrinsic_handler;
    char *n = entity_local_name(call_function(obj));

    while (p->name != NULL) {
	if (strcmp(p->name, n) == 0) {
	  return((*(p->f))(obj, precedence, leftmost, pdl));
	}
	p++;
    }

    return words_regular_call(obj, FALSE, pdl);
}

static int
intrinsic_precedence(string n)
{
    struct intrinsic_handler *p = tab_intrinsic_handler;

    while (p->name != NULL) {
	if (strcmp(p->name, n) == 0)
	    return(p->prec);
	p++;
    }

    return 0;
}

static int
words_intrinsic_precedence(call obj)
{
    char *n = entity_local_name(call_function(obj));
    return intrinsic_precedence(n);
}

static list words_va_arg(list obj, list pdl)
{
  list pc = NIL;
  expression e1 = sizeofexpression_expression(SIZEOFEXPRESSION(CAR(obj)));
  type t2 = sizeofexpression_type(SIZEOFEXPRESSION(CAR(CDR(obj))));
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  pc = CHAIN_SWORD(pc,"va_arg(");
  pc = gen_nconc(pc, words_expression(e1, pdl));
  pc = CHAIN_SWORD(pc, space_p? ", " : ",");
  pc = gen_nconc(pc, words_type(t2, pdl));
  pc = CHAIN_SWORD(pc,")");
  return pc;
}

/* exported for cmfortran.c
 */
list words_call(
    call obj,
    int precedence,
    bool leftmost,
    bool is_a_subroutine,
    list pdl)
{
  list pc;
  entity f = call_function(obj);
  value i = entity_initial(f);

  if(value_intrinsic_p(i)) {
    int effective_precedence = (precedence_p||precedence<=1)? precedence : MAXIMAL_PRECEDENCE;

    pc = words_intrinsic_call(obj, effective_precedence, leftmost, pdl);
  }
  else
    pc = words_genuine_regular_call(obj, is_a_subroutine, pdl);
  return pc;
}

/* exported for expression.c
 */
list
words_syntax(syntax obj, list pdl)
{
    list pc = NIL;

    switch (syntax_tag(obj)) {
    case is_syntax_reference :
      pc = words_reference(syntax_reference(obj), pdl);
      break;
    case is_syntax_range:
      pc = words_range(syntax_range(obj), pdl);
      break;
    case is_syntax_call:
      pc = words_call(syntax_call(obj), 0, TRUE, FALSE, pdl);
      break;
    case is_syntax_cast:
      pc = words_cast(syntax_cast(obj), 0, pdl);
      break;
    case is_syntax_sizeofexpression: {
      /* FI->SG: I do not know if in_type_declaration is TRUE, FALSE
	 or a formal parameter */
      bool in_type_declaration = TRUE;
      pc = words_sizeofexpression(syntax_sizeofexpression(obj),
				  in_type_declaration, pdl);
      break;
    }
    case is_syntax_subscript:
      pc = words_subscript(syntax_subscript(obj), pdl);
      break;
    case is_syntax_application:
      pc = words_application(syntax_application(obj), pdl);
      break;
    case is_syntax_va_arg:
      pc = words_va_arg(syntax_va_arg(obj), pdl);
      break;
    default:
      pips_internal_error("unexpected tag\n");
    }

    return(pc);
}

/* This one is exported. Outer parentheses are never useful. pdl can
   point to an empty list, but it must be free on return*/
list /* of string */ words_expression(expression obj, list pdl)
{
  return words_syntax(expression_syntax(obj), pdl);
}

/* exported for cmfortran.c
 */
list words_subexpression(
    expression obj,
    int precedence,
    bool leftmost,
    list pdl)
{
    list pc;

    if ( expression_call_p(obj) )
      pc = words_call(syntax_call(expression_syntax(obj)),
		      precedence, leftmost, FALSE, pdl);
    else if(expression_cast_p(obj)) {
      cast c = expression_cast(obj);
      pc = words_cast(c, precedence, pdl);
    }
    else
      pc = words_syntax(expression_syntax(obj), pdl);

    return pc;
}


/**************************************************************** SENTENCE */

static sentence
sentence_tail(entity e)
{
  sentence result = sentence_undefined;
  switch(get_prettyprint_language_tag()) {
    case is_language_fortran:
      result = MAKE_ONE_WORD_SENTENCE(0, strdup("END"));
      break;
    case is_language_c:
      result = MAKE_ONE_WORD_SENTENCE(0, strdup("}"));
      break;
    case is_language_fortran95: {
      /* In fortran 95, we want the end to be followed by the type of construct
       * and its name.
       */
      list pc = NIL;
      type te = entity_type(e);
      functional fe;
      type tr;

      pc = CHAIN_SWORD(pc,"END ");

      pips_assert("is functionnal", type_functional_p(te));

      if (static_module_p(e))
        pc = CHAIN_SWORD(pc,"static ");

      fe = type_functional(te);
      tr = functional_result(fe);

      switch(type_tag(tr)) {
        case is_type_void:
          if (entity_main_module_p(e))
            pc = CHAIN_SWORD(pc,"PROGRAM ");
          else {
            if (entity_blockdata_p(e))
              pc = CHAIN_SWORD(pc, "BLOCKDATA ");
            else if (entity_f95module_p(e))
              pc = CHAIN_SWORD(pc, "MODULE ");
            else
              pc = CHAIN_SWORD(pc,"SUBROUTINE ");
          }
          break;
        case is_type_variable: {
          list pdl = NIL;
          pc = CHAIN_SWORD(pc,"FUNCTION ");
          break;
        }
        case is_type_unknown:
          /*
           * For C functions with no return type.
           * It can be treated as of type int, but we keep it unknown
           * for the moment, to make the differences and to regenerate initial code
           */
          break;
        default:
          pips_internal_error("unexpected type for result\n");
      }

      pc = CHAIN_SWORD(pc, entity_user_name(e));
      result = make_sentence(is_sentence_unformatted, make_unformatted(NULL,
                                                                       0,
                                                                       0,
                                                                       pc));
      break;
    }
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  return result;
}

/* exported for unstructured.c */
sentence
sentence_goto_label(
    entity __attribute__ ((unused)) module,
    string label,
    int margin,
    string tlabel,
    int n)
{
    list pc = words_goto_label(tlabel);

    return(make_sentence(is_sentence_unformatted,
	    make_unformatted(label?strdup(label):NULL, n, margin, pc)));
}

static sentence sentence_goto(entity module,
                                string label,
                                int margin,
                                statement obj,
                                int n) {
    string tlabel = entity_local_name(statement_label(obj)) +
      strlen(LABEL_PREFIX);
    pips_assert("Legal label required", strlen(tlabel)!=0);
    return sentence_goto_label(module, label, margin, tlabel, n);
}

/* Build the text of a code block (a list of statements)

   @module is the module entity the code to display belong to

   @label is the label associated to the block

   @param margin is the indentation level

   @param objs is the list of statements in the sequence to display

   @param n is the statement number of the sequence

   @pdl is the parser declaration list to track type declaration display
   in C

   @return the text of the block
*/
static text
text_block(entity module,
	   string label,
	   int margin,
	   list objs,
	   int n,
	   list pdl)
{
  text r = make_text(NIL);

  if (ENDP(objs)
      && ! (get_bool_property("PRETTYPRINT_EMPTY_BLOCKS")
	    || get_bool_property("PRETTYPRINT_ALL_C_BLOCKS")))
    return(r);

  if(!empty_string_p(label)) {
    pips_user_warning("Illegal label \"%s\". "
		      "Blocks cannot carry a label\n",
		      label);
  }

  /* "Unformatted" to be added at the beginning and at the end of a block: */
  unformatted bm_beg = NULL;
  unformatted bm_end = NULL;
  // Test if block markers are required and set them:
  bool flg_marker = mark_block(&bm_beg, &bm_end, n, margin);

  // Print the begin block marker(s) if needed:
  if (flg_marker == true)
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, bm_beg));
  else if ((get_bool_property("PRETTYPRINT_ALL_EFFECTS")
	    || get_bool_property("PRETTYPRINT_BLOCKS"))
	   && get_bool_property("PRETTYPRINT_FOR_FORESYS"))
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
					  strdup("C$BB\n")));

  if (get_bool_property("PRETTYPRINT_ALL_C_BLOCKS")) {
    /* Since we generate new { }, we increment the margin for the nested
       statements: */
    margin -= INDENTATION;
    if (margin < 0)
      margin = 0;
    ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin, "{{"));
    margin += INDENTATION;
  }

  // Append local variables if any:
  r = insert_locals (r);

  /* Now begin block markers and declarations have been printed, so print
     the block instructions: */
  for (; objs != NIL; objs = CDR(objs)) {
    statement s = STATEMENT(CAR(objs));

    text t = text_statement_enclosed(module, margin, s, FALSE, TRUE, pdl);
    text_sentences(r) = gen_nconc(text_sentences(r), text_sentences(t));
    text_sentences(t) = NIL;
    free_text(t);
  }

  if (get_bool_property("PRETTYPRINT_ALL_C_BLOCKS")) {
    /* Get back to previous indentation: */
    margin -= INDENTATION;
    ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin, "}}"));
    margin += INDENTATION;
  }

  // Print the end block marker(s) if needed:
  if (flg_marker == true)
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, bm_end));

  return r;
}

/* @return a list of string with the variable that need to be private in the
 * current context. The context takes care of the kind of output. For example
 * in the case of open mp the variables would be encapsulated into
 * the private() clause like this: private (a,b).
 * @param obj the loop to look at.
 */
static list /* of string */
loop_private_variables(loop obj, list pdl)
{
    bool all_private = get_bool_property("PRETTYPRINT_ALL_PRIVATE_VARIABLES"),
      hpf_private = pp_hpf_style_p(), omp_private = pp_omp_style_p(),
      some_before = FALSE;
  list l = NIL;

  // list of local entities
  // In case of openmp the variable declared in the loop body should
  // not be made private, so ask for removing them from the list of locals.
  // If all_private is FALSE -> remove loop indice from the list of locals.
  list locals = loop_private_variables_as_entites(obj,
                                                  omp_private,
                                                  !all_private);
  /* comma-separated list of private variables.
   * built in reverse order to avoid adding at the end...
   */
  FOREACH (ENTITY, p, locals) {
    if (some_before)
      l = CHAIN_SWORD(l, ",");
    else
      some_before = TRUE; /* from now on commas, triggered... */
    l = gen_nconc(l, words_declaration(p, TRUE, pdl));
  }

  gen_free_list(locals);

  pips_debug(5, "#printed %zd/%zd\n", gen_length(l),
      gen_length(loop_locals(obj)));

  /* stuff around if not empty
   */
  if (l) {
    string private;
    if (hpf_private) {
    private = "NEW(";
    } else if (omp_private) {
      switch (get_prettyprint_language_tag()) {
        case is_language_fortran:
        private = "PRIVATE(";
          break;
        case is_language_c:
        private = "private(";
          break;
        case is_language_fortran95:
          pips_internal_error("Need to update F95 case");
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    } else
    private = "PRIVATE ";
    l = CONS(STRING, MAKE_SWORD(private), l);
    if (hpf_private || omp_private)
      CHAIN_SWORD(l, ")");
  }

  return l;
}

/* returns a formatted text for the HPF independent and new directive
 * well, no continuations and so, but the directives do not fit the
 * unformatted domain, because the directive prolog would not be well
 * managed there.
 */
static string
marged(
    string prefix,
    int margin)
{
    int len = strlen(prefix), i;
    string result = (string) malloc(strlen(prefix)+margin+1);
    strcpy(result, prefix);
    if(prettyprint_language_is_fortran_p()) {
      for (i=len; margin-->0;) {
        result[i++] = ' '; result[i]='\0';
      }
    }
    return result;
}


static text text_directive(loop obj, /* the loop we're interested in */
                           int margin,
                           string basic_directive,
                           string basic_continuation,
                           string parallel,
                           list pdl) {
  string dir = marged(basic_directive, margin), cont =
      marged(basic_continuation, margin);
  text t = make_text(NIL);
  char buffer[100]; /* ??? */
  list /* of string */l = NIL;
  bool is_hpf = pp_hpf_style_p(), is_omp = pp_omp_style_p();
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  /* start buffer */
  buffer[0] = '\0';

  if (execution_parallel_p(loop_execution(obj))) {
    add_to_current_line(buffer, dir, cont, t);
    add_to_current_line(buffer, parallel, cont, t);
    l = loop_private_variables(obj, pdl);
    if (l && is_hpf)
      add_to_current_line(buffer, space_p ? ", " : ",", cont, t);
  } else if (get_bool_property("PRETTYPRINT_ALL_PRIVATE_VARIABLES")) {
    l = loop_private_variables(obj, pdl);
    if (l) {
      add_to_current_line(buffer, dir, cont, t);
      if (is_omp) {
        switch (get_prettyprint_language_tag()) {
          case is_language_fortran:
          case is_language_fortran95:
            add_to_current_line(buffer, "DO ", cont, t);
            break;
          case is_language_c:
            add_to_current_line(buffer, "for ", cont, t);
            break;
          default:
            pips_internal_error("Language unknown !");
            break;
        }
      }
    }
  }

  if (strlen(buffer) > 0)
    MAP(STRING, s, add_to_current_line(buffer, s, cont, t), l);

  /* what about reductions? should be associated to the ri somewhere.
   */

  close_current_line(buffer, t, cont);
  free(dir);
  free(cont);
  return t;
}

#define HPF_SENTINEL 		"!HPF$"
#define HPF_DIRECTIVE 		HPF_SENTINEL " "
#define HPF_CONTINUATION 	HPF_SENTINEL "x"
#define HPF_INDEPENDENT 	"INDEPENDENT"

static text text_hpf_directive(loop l, int m)
{
  list pdl = NIL; // pdl is useless in Fortran
  text t = text_directive(l, m, "\n" HPF_DIRECTIVE, HPF_CONTINUATION,
			  HPF_INDEPENDENT, pdl);
  return t;
}

#define OMP_SENTINEL 		"!$OMP"
#define OMP_DIRECTIVE 		OMP_SENTINEL " "
#define OMP_CONTINUATION 	OMP_SENTINEL "x"
#define OMP_PARALLELDO		"PARALLEL DO "
#define OMP_C_SENTINEL 		"#pragma omp"
#define OMP_C_DIRECTIVE 	OMP_C_SENTINEL " "
#define OMP_C_CONTINUATION 	OMP_C_SENTINEL "x"
#define OMP_C_PARALLELDO	"parallel for "

text
text_omp_directive(loop l, int m)
{
  list pdl = NIL; // pdl is useless in Fortran
  text t = text_undefined;

  switch(get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      t = text_directive(l,
                         m,
                         "\n" OMP_DIRECTIVE,
                         OMP_CONTINUATION,
                         OMP_PARALLELDO,
                         pdl);
      break;
    case is_language_c:
      // text_directive function takes care of private variables
      // More should be done to take care of shared variables, reductions
      // and other specific omp clause like lastprivate, copyin ...
      t = text_directive(l,
                         m,
                         OMP_C_DIRECTIVE,
                         OMP_C_CONTINUATION,
                         OMP_C_PARALLELDO,
                         pdl);
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }
  return t;
}

/* exported for fortran90.c */
text text_loop_default(entity module,
                       string label,
                       int margin,
                       loop obj,
                       int n,
                       list pdl) {
  list pc = NIL;
  sentence first_sentence = sentence_undefined;
  unformatted u;
  text r = make_text(NIL);
  statement body = loop_body( obj );
  entity the_label = loop_label(obj);
  string do_label = entity_local_name(the_label) + strlen(LABEL_PREFIX);
  bool structured_do = entity_empty_label_p(the_label);
  bool doall_loop_p = FALSE;
  bool hpf_prettyprint = pp_hpf_style_p();
  bool do_enddo_p = get_bool_property("PRETTYPRINT_DO_LABEL_AS_COMMENT");
  bool all_private = get_bool_property("PRETTYPRINT_ALL_PRIVATE_VARIABLES");

  if (execution_sequential_p(loop_execution(obj))) {
    doall_loop_p = FALSE;
  } else {
    doall_loop_p = pp_doall_style_p();
  }

  /* HPF directives before the loop if required (INDEPENDENT and NEW) */
  if (hpf_prettyprint)
    MERGE_TEXTS(r, text_hpf_directive(obj, margin));
  /* idem if Open MP directives are required */
  if (pp_omp_style_p())
    MERGE_TEXTS(r, text_omp_directive(obj, margin));

  /* LOOP prologue.
   */
  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      pc = CHAIN_SWORD(NIL, (doall_loop_p) ? "DOALL " : "DO " );
      if (!structured_do && !doall_loop_p && !do_enddo_p) {
        pc = CHAIN_SWORD(pc, concatenate(do_label, " ", NULL));
      }
      break;
    case is_language_c:
      pc = CHAIN_SWORD(NIL, (doall_loop_p) ? "forall(" : "for(" );
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  //pc = CHAIN_SWORD(pc, entity_local_name(loop_index(obj)));
  pc = CHAIN_SWORD(pc, entity_user_name(loop_index(obj)));
  pc = CHAIN_SWORD(pc, " = ");

  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      pc = gen_nconc(pc, words_loop_range(loop_range(obj), pdl));
      u = make_unformatted(strdup(label), n, margin, pc);
      ADD_SENTENCE_TO_TEXT(r, first_sentence =
          make_sentence(is_sentence_unformatted, u));
      break;
    case is_language_c:
      pc = gen_nconc(pc, C_loop_range(loop_range(obj), loop_index(obj), pdl));
      if (!one_liner_p(body))
        pc = CHAIN_SWORD(pc," {");
      if ((label != NULL) && (label[0] != '\0')) {
        pips_debug(9, "the label %s need to be print for a for C loop", label);
        u = make_unformatted(strdup(label), 0, 0, NULL);
        ADD_SENTENCE_TO_TEXT(r, first_sentence =
            make_sentence(is_sentence_unformatted, u));
      }
      u = make_unformatted(NULL, n, margin, pc);
      ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, u));
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }
  /* builds the PRIVATE scalar declaration if required
   */
  if (!ENDP(loop_locals(obj)) && (doall_loop_p || all_private)
      && !hpf_prettyprint) {
    list /* of string */lp = loop_private_variables(obj, pdl);

    // initialize the local variable text if needed
    if ((local_flg == false) && (lp)) {
      local_flg = true;
      local_var = make_text(NIL);
    }

    if (lp)
      /* local_var is a global variable which is exploited
       later... */
      /* FI: I do not understand why the local declarations were
       not added right away. I hope my change (simplification)
       does not break something else that is not tested by our
       non-regression suite. */
      if (!pp_omp_style_p()) {
        ADD_SENTENCE_TO_TEXT
        //	    ( local_var,
        ( r,
            make_sentence(is_sentence_unformatted,
                make_unformatted(NULL, 0, margin+INDENTATION, lp)));
      }
  }

  /* loop BODY
   */
  MERGE_TEXTS(r, text_statement_enclosed(module,
          margin+INDENTATION,
          body,
          !one_liner_p(body),
          !one_liner_p(body),
          pdl));

  /* LOOP postlogue
   */
  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      if (structured_do || doall_loop_p || do_enddo_p || pp_cray_style_p()
          || pp_craft_style_p() || pp_cmf_style_p()) {
        ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"ENDDO"));
      }
      break;
    case is_language_c:
      if (!one_liner_p(body))
        ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  attach_loop_to_sentence_up_to_end_of_text(first_sentence, r, obj);

  return r;
}

/* exported for conversion/look_for_nested_loops.c */
text text_loop(
    entity module,
    string label,
    int margin,
    loop obj,
    int n,
    list pdl)
{
  text r = make_text(NIL);
  statement body = loop_body( obj ) ;
  entity the_label = loop_label(obj);
  string do_label = entity_local_name(the_label)+strlen(LABEL_PREFIX) ;
  bool structured_do = entity_empty_label_p(the_label);
  bool do_enddo_p = get_bool_property("PRETTYPRINT_DO_LABEL_AS_COMMENT");

  /* small hack to show the initial label of the loop to name it...
   */
  if(!structured_do && do_enddo_p)
    {
      ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
					    strdup(concatenate("!     INITIALLY: DO ", do_label, "\n", NULL))));
    }

  /* quite ugly management of other prettyprints...
   */
  switch(execution_tag(loop_execution(obj)) ) {
  case is_execution_sequential:
    MERGE_TEXTS(r, text_loop_default(module, label, margin, obj, n, pdl));
    break ;
  case is_execution_parallel:
    if (pp_cmf_style_p()) {
      text aux_r;
      if((aux_r = text_loop_cmf(module, label, margin, obj, n, NIL, NIL))
	 != text_undefined) {
	MERGE_TEXTS(r, aux_r);
      }
    }
    else if (pp_craft_style_p()) {
      text aux_r;
      if((aux_r = text_loop_craft(module, label, margin, obj, n, NIL, NIL))
	 != text_undefined) {
	MERGE_TEXTS(r, aux_r);
      }
    }
    else if (pp_f90_style_p()) {
      instruction bi = statement_instruction(body); // body instruction
      bool success_p = FALSE;
      if(instruction_assign_p(bi) ) {
	MERGE_TEXTS(r, text_loop_90(module, label, margin, obj, n));
	success_p = TRUE;
      }
      else if(instruction_sequence_p(bi)) {
	list sl = sequence_statements(instruction_sequence(bi));
	if(gen_length(sl)==1) {
	  statement ibs = STATEMENT(CAR(sl));
	  instruction ibi = statement_instruction(ibs);
	  if(instruction_assign_p(ibi) ) {
	    MERGE_TEXTS(r, text_loop_90(module, label, margin, obj, n));
	    success_p = TRUE;
	  }
	}
      }
      if(!success_p) {
	MERGE_TEXTS(r, text_loop_default(module, label, margin, obj, n, pdl));
      }
    }
    else {
      MERGE_TEXTS(r, text_loop_default(module, label, margin, obj, n, pdl));
    }
    break ;
  default:
    pips_internal_error("Unknown tag\n") ;
  }
  return r;
}

static text text_whileloop(entity module,
                           string label,
                           int margin,
                           whileloop obj,
                           int n,
                           list pdl) {
  list pc = NIL;
  sentence first_sentence;
  unformatted u;
  text r = make_text(NIL);
  statement body = whileloop_body( obj );
  entity the_label = whileloop_label(obj);
  string do_label = entity_local_name(the_label) + strlen(LABEL_PREFIX);
  bool structured_do = entity_empty_label_p(the_label);
  bool do_enddo_p = get_bool_property("PRETTYPRINT_DO_LABEL_AS_COMMENT");

  evaluation eval = whileloop_evaluation(obj);

  /* Show the initial label of the loop to name it...
   * FI: I believe this is useless for while loops since they cannot
   * be parallelized.
   */
  if(!structured_do && do_enddo_p) {
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
            strdup(concatenate("!     INITIALLY: DO ", do_label, "\n", NULL))));
  }

  if(evaluation_before_p(eval)) {
    switch(get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        /* LOOP prologue.
         */
        pc = CHAIN_SWORD(NIL, "DO " );

        if(!structured_do && !do_enddo_p) {
          pc = CHAIN_SWORD(pc, concatenate(do_label, " ", NULL));
        }
        pc = CHAIN_SWORD(pc, "WHILE (");
        pc = gen_nconc(pc, words_expression(whileloop_condition(obj), pdl));
        pc = CHAIN_SWORD(pc, ")");
        u = make_unformatted(strdup(label), n, margin, pc);
        ADD_SENTENCE_TO_TEXT(r, first_sentence =
            make_sentence(is_sentence_unformatted, u));

        /* loop BODY
         */
        MERGE_TEXTS(r, text_statement(module, margin+INDENTATION, body, pdl));

        /* LOOP postlogue
         */
        if(structured_do) {
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"ENDDO"));
        }
        break;
      case is_language_c:
        if(one_liner_p(body)) {
          pc = CHAIN_SWORD(NIL,"while (");
          pc = gen_nconc(pc, words_expression(whileloop_condition(obj), pdl));
          pc = CHAIN_SWORD(pc,") ");
          u = make_unformatted(strdup(label), n, margin, pc);
          ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, u));
          MERGE_TEXTS(r, text_statement_enclosed(module,
                  margin+INDENTATION,
                  body,
                  !one_liner_p(body),
                  !one_liner_p(body),
                  pdl));

          //if (structured_do)
          //ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
        } else {
          pc = CHAIN_SWORD(NIL,"while (");
          pc = gen_nconc(pc, words_expression(whileloop_condition(obj), pdl));
          pc = CHAIN_SWORD(pc,") {");
          u = make_unformatted(strdup(label), n, margin, pc);
          ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, u));
          MERGE_TEXTS(r, text_statement(module, margin+INDENTATION, body, pdl));
          if(structured_do)
            ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
        }
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  } else {
    pips_assert ("Only C language is managed here",
                 prettyprint_language_is_c_p());
    /* C do { s; } while (cond); loop*/
    pc = CHAIN_SWORD(NIL,"do {");
    u = make_unformatted(strdup(label), n, margin, pc);
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, u));
    MERGE_TEXTS(r, text_statement(module, margin+INDENTATION, body, pdl));
    ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
    pc = CHAIN_SWORD(NIL,"while (");
    pc = gen_nconc(pc, words_expression(whileloop_condition(obj), pdl));
    pc = CHAIN_SWORD(pc, ");");
    u = make_unformatted(NULL, n, margin, pc);
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, u));
  }

  /* attach_loop_to_sentence_up_to_end_of_text(first_sentence, r, obj); */
  return r;
}

/* exported for unstructured.c
 */
text
init_text_statement(
    entity module,
    int margin,
    statement obj)
{
    instruction i = statement_instruction(obj);
    text r;

    if (get_bool_property("PRETTYPRINT_ALL_EFFECTS")
	|| !((instruction_block_p(i) &&
	      !get_bool_property("PRETTYPRINT_BLOCKS")) ||
	     (instruction_unstructured_p(i) &&
	      !get_bool_property("PRETTYPRINT_UNSTRUCTURED")))) {
      /* FI: before calling the hook,
       * statement_ordering(obj) should be checked */
	r = (*text_statement_hook)( module, margin, obj );
	if (text_statement_hook != empty_text)
	    attach_decoration_to_text(r);
    }
    else
	r  = make_text( NIL ) ;

    if (get_bool_property("PRETTYPRINT_ALL_EFFECTS") ||
	get_bool_property("PRETTYPRINT_STATEMENT_ORDERING")) {
	char *buffer;
	int so = statement_ordering(obj) ;

	if (!(instruction_block_p(statement_instruction(obj)) &&
	      (! get_bool_property("PRETTYPRINT_BLOCKS")))) {
	  if (so != STATEMENT_ORDERING_UNDEFINED)
	    asprintf(&buffer, "%s (%d,%d)\n", get_comment_sentinel(),
		     ORDERING_NUMBER(so), ORDERING_STATEMENT(so));
	  else
	    asprintf(&buffer, "%s (statement ordering unavailable)\n",
	             get_comment_sentinel());
	  ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
						buffer));
	}
    }
    return( r ) ;
}

static text text_logical_if(entity __attribute__ ((unused)) module,
                            string label,
                            int margin,
                            test obj,
                            int n,
                            list pdl) {
  text r = make_text(NIL);
  list pc = NIL;
  statement tb = test_true(obj);

  switch(get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      pc = CHAIN_SWORD(pc, strdup("IF ("));
      break;
    case is_language_c:
      pc = CHAIN_SWORD(pc, strdup("if ("));
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
  pc = CHAIN_SWORD(pc, ") ");
  instruction ti = instruction_undefined;
  call c = call_undefined;
  text t = text_undefined;
  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      ti = statement_instruction(tb);
      c = instruction_call(ti);
      pc = gen_nconc(pc, words_call(c, 0, TRUE, TRUE, pdl));
      ADD_SENTENCE_TO_TEXT(r,
          make_sentence(is_sentence_unformatted,
              make_unformatted(strdup(label), n,
                  margin, pc)));
      break;
    case is_language_c:
      t = text_statement(module, margin + INDENTATION, tb, pdl);
      ADD_SENTENCE_TO_TEXT(r,
          make_sentence(is_sentence_unformatted,
              make_unformatted(strdup(label), n,
                  margin, pc)));
      text_sentences(r) = gen_nconc(text_sentences(r), text_sentences(t));
      text_sentences(t) = NIL;
      free_text(t);
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  ifdebug(8) {
    fprintf(stderr, "logical_if=================================\n");
    print_text(stderr, r);
    fprintf(stderr, "==============================\n");
  }
  return (r);
}


static text text_block_if(entity module,
                          string label,
                          int margin,
                          test obj,
                          int n,
                          list pdl) {
  text r = make_text(NIL);
  list pc = NIL;
  statement test_false_obj;
  bool one_liner_true_statement = one_liner_p(test_true(obj));
  bool one_liner_false_statement = one_liner_p(test_false(obj));
  bool else_branch_p = FALSE; /* The else branch must be printed */

  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      pc = CHAIN_SWORD(pc, "IF (");
      pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
      pc = CHAIN_SWORD(pc, ") THEN");
      break;
    case is_language_c:
      pc = CHAIN_SWORD(pc, "if (");
      pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
      if(one_liner_true_statement) {
        pc = CHAIN_SWORD(pc, ")");
      } else
        pc = CHAIN_SWORD(pc, ") {");
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  ADD_SENTENCE_TO_TEXT(r,
      make_sentence(is_sentence_unformatted,
          make_unformatted(strdup(label), n,
              margin, pc)));
  MERGE_TEXTS(r, text_statement_enclosed(module,
          margin+INDENTATION,
          test_true(obj),
          !one_liner_true_statement,
          !one_liner_true_statement,
          pdl));

  test_false_obj = test_false(obj);
  if(statement_undefined_p(test_false_obj)) {
    pips_error("text_test", "undefined statement\n");
  }
  if(!statement_with_empty_comment_p(test_false_obj)
      || (!empty_statement_p(test_false_obj)
          && !continue_statement_p(test_false_obj))
      || (empty_statement_p(test_false_obj)
          && (get_bool_property("PRETTYPRINT_EMPTY_BLOCKS")))
      || (continue_statement_p(test_false_obj)
          && (get_bool_property("PRETTYPRINT_ALL_LABELS")))) {
    else_branch_p = TRUE;
    switch (get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"ELSE"));
        break;
      case is_language_c:
        if(!one_liner_true_statement)
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
        if(one_liner_false_statement) {
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"else"));
        } else {
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"else {"));
        }
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
    MERGE_TEXTS(r, text_statement(module, margin+INDENTATION,
            test_false_obj, pdl));
  }
  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,strdup("ENDIF")));
      break;
    case is_language_c:
      if((!else_branch_p && !one_liner_true_statement) || (else_branch_p
          && !one_liner_false_statement))
        ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,strdup("}")));
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  ifdebug(8) {
    fprintf(stderr, "text_block_if=================================\n");
    print_text(stderr, r);
    fprintf(stderr, "==============================\n");
  }

  return (r);
}


static text text_io_block_if(entity module,
                             string label,
                             int margin,
                             test obj,
                             int n,
                             list pdl) {
  text r = make_text(NIL);
  list pc = NIL;
  string strglab = local_name(new_label_name(module)) + 1;

  if (!empty_statement_p(test_true(obj))) {

    r = make_text(CONS(SENTENCE,
        sentence_goto_label(module, label, margin,
            strglab, n),
        NIL));

    ADD_SENTENCE_TO_TEXT(r,
        make_sentence(is_sentence_unformatted,
            make_unformatted(strdup(label), n,
                margin, pc)));
    MERGE_TEXTS(r, text_statement(module, margin,
            test_true(obj), pdl));
    string str = string_undefined;
    switch (get_prettyprint_language_tag()) {
      case is_language_fortran:
        str = strdup(CONTINUE_FUNCTION_NAME);
        break;
      case is_language_c:
        str = strdup(C_CONTINUE_FUNCTION_NAME);
        break;
      case is_language_fortran95:
        pips_internal_error("Need to update F95 case");
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }

    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted,
            make_unformatted(strdup(strglab), n, margin,
                CONS(STRING, str, NIL))));
  }

  if (!empty_statement_p(test_false(obj)))
    MERGE_TEXTS(r, text_statement(module, margin,
            test_false(obj), pdl));

  return (r);
}


static text text_block_ifthen(entity module,
                              string label,
                              int margin,
                              test obj,
                              int n,
                              list pdl) {
  text r = make_text(NIL);
  list pc = NIL;
  statement tb = test_true(obj);

  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      pc = CHAIN_SWORD(pc, "IF (");
      pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
      pc = CHAIN_SWORD(pc, ") THEN");
      break;
    case is_language_c:
      pc = CHAIN_SWORD(pc, "if (");
      pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
      pc = CHAIN_SWORD(pc, (one_liner_p(tb)?")":") {"));
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  ADD_SENTENCE_TO_TEXT(r,
      make_sentence(is_sentence_unformatted,
          make_unformatted(strdup(label), n,
              margin, pc)));
  MERGE_TEXTS(r, text_statement_enclosed(module,
          margin+INDENTATION,
          tb,
          !one_liner_p(tb),
          !one_liner_p(tb),
          pdl));
  if (prettyprint_language_is_c_p()
      && !one_liner_p(tb))
    ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
  return (r);
}


static text text_block_else(entity module,
                            string __attribute__ ((unused)) label,
                            int margin,
                            statement stmt,
                            int __attribute__ ((unused)) n,
                            list pdl) {
  text r = make_text(NIL);

  if (!statement_with_empty_comment_p(stmt) || (!empty_statement_p(stmt)
      && !continue_statement_p(stmt)) || (empty_statement_p(stmt)
      && (get_bool_property("PRETTYPRINT_EMPTY_BLOCKS")))
      || (continue_statement_p(stmt)
          && (get_bool_property("PRETTYPRINT_ALL_LABELS")))) {
    switch (get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin, "ELSE"));
        MERGE_TEXTS(r, text_statement(module, margin+INDENTATION, stmt, pdl));
        break;
      case is_language_c:
        if (one_liner_p(stmt)) {
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"else"));
          MERGE_TEXTS(r, text_statement_enclosed(module,
                  margin+INDENTATION,
                  stmt,
                  FALSE,
                  FALSE,
                  pdl));
        } else {
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin, "else {"));
          MERGE_TEXTS(r, text_statement(module, margin+INDENTATION, stmt, pdl));
          ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin, "}"));
        }
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }

  return r;
}


static text text_block_elseif(entity module,
                              string label,
                              int margin,
                              test obj,
                              int n,
                              list pdl) {
  text r = make_text(NIL);
  list pc = NIL;
  statement tb = test_true(obj);
  statement fb = test_false(obj);

  switch (get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      pc = CHAIN_SWORD(pc, strdup("ELSEIF ("));
      pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
      pc = CHAIN_SWORD(pc, strdup(") THEN"));
      break;
    case is_language_c:
      pc = CHAIN_SWORD(pc, strdup("else if ("));
      pc = gen_nconc(pc, words_expression(test_condition(obj), pdl));
      pc = CHAIN_SWORD(pc, strdup((one_liner_p(tb)?")":") {")));
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  ADD_SENTENCE_TO_TEXT(r,
      make_sentence(is_sentence_unformatted,
          make_unformatted(strdup(label), n,
              margin, pc)));

  MERGE_TEXTS(r, text_statement_enclosed(module,
          margin+INDENTATION,
          tb,
          !one_liner_p(tb),
          !one_liner_p(tb),
          pdl));

  if (prettyprint_language_is_c_p()
      && !one_liner_p(tb)) {
    ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin, strdup("}")));
  }

  if (statement_test_p(fb) && empty_comments_p(statement_comments(fb))
      && entity_empty_label_p(statement_label(fb))) {
    MERGE_TEXTS(r, text_block_elseif(module,
            label_local_name(statement_label(fb)),
            margin,
            statement_test(fb), n, pdl));

  } else {
    MERGE_TEXTS(r, text_block_else(module, label, margin, fb, n, pdl));
  }
  ifdebug(8) {
    fprintf(stderr, "elseif=================================\n");
    print_text(stderr, r);
    fprintf(stderr, "==============================\n");
  }
  return (r);
}


static text text_test(entity module,
                      string label,
                      int margin,
                      test obj,
                      int n,
                      list pdl) {
  text r = text_undefined;
  statement tb = test_true(obj);
  statement fb = test_false(obj);

  /* 1st case: one statement in the true branch => Fortran logical IF */
  if (nop_statement_p(fb) && statement_call_p(tb)
      && entity_empty_label_p(statement_label(tb))
      && empty_comments_p(statement_comments(tb)) && !continue_statement_p(tb)
      && !get_bool_property("PRETTYPRINT_BLOCK_IF_ONLY")
      && !(call_contains_alternate_returns_p(statement_call(tb))
          && get_bool_property("PRETTYPRINT_REGENERATE_ALTERNATE_RETURNS"))) {
    r = text_logical_if(module, label, margin, obj, n, pdl);
  }
  /* 2nd case: one test in the false branch => "ELSEIF" Fortran block or "else if" C construct */
  else if (statement_test_p(fb) && empty_comments_p(statement_comments(fb))
      && entity_empty_label_p(statement_label(fb))
      && !get_bool_property("PRETTYPRINT_BLOCK_IF_ONLY")) {

    r = text_block_ifthen(module, label, margin, obj, n, pdl);

    MERGE_TEXTS(r, text_block_elseif
        (module,
            label_local_name(statement_label(fb)),
            margin, statement_test(fb), n, pdl));

    switch (get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"ENDIF"));
        break;
      case is_language_c:
        //nothing to do in C
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  } else {
    syntax c = expression_syntax(test_condition(obj));

    if (syntax_reference_p(c)
        && io_entity_p(reference_variable(syntax_reference(c)))
        && !get_bool_property("PRETTYPRINT_CHECK_IO_STATEMENTS"))
      r = text_io_block_if(module, label, margin, obj, n, pdl);
    else
      r = text_block_if(module, label, margin, obj, n, pdl);
  }
  ifdebug(8) {
    fprintf(stderr, "text_test=================================\n");
    print_text(stderr, r);
    fprintf(stderr, "==============================\n");
  }
  return r;
}


/* hook for adding something in the head. used by hpfc.
 * done so to avoid hpfc->prettyprint dependence in the libs.
 * FC. 29/12/95.
 */
static string (*head_hook)(entity) = NULL;
void set_prettyprinter_head_hook(string(*f)(entity)){ head_hook=f;}
void reset_prettyprinter_head_hook(){ head_hook=NULL;}

static text text_instruction(entity module,
                             string label,
                             int margin,
                             instruction obj,
                             int n,
                             list pdl) {
  text r = text_undefined;

  switch(instruction_tag(obj)) {
    case is_instruction_block: {
      r = text_block(module, label, margin, instruction_block(obj), n, pdl);
      break;
    }
    case is_instruction_test: {
      r = text_test(module, label, margin, instruction_test(obj), n, pdl);
      break;
    }
    case is_instruction_loop: {
      r = text_loop(module, label, margin, instruction_loop(obj), n, pdl);
      break;
    }
    case is_instruction_whileloop: {
      r = text_whileloop(module,
                         label,
                         margin,
                         instruction_whileloop(obj),
                         n,
                         pdl);
      break;
    }
    case is_instruction_goto: {
      r = make_text(CONS(SENTENCE,
          sentence_goto(module, label, margin,
              instruction_goto(obj), n), NIL));
      break;
    }
    case is_instruction_call: {
      unformatted u;
      sentence s;
      /* FI: in C at least, this has already been decided by the
       caller, text_statement_enclosed(); but apparently not in
       Fortran. Also, the source code may be in Fortran, but the
       user wants it prettyprinted as C. */
      if (prettyprint_language_is_fortran_p()
      && instruction_continue_p(obj) && empty_string_p(label)
          && !get_bool_property("PRETTYPRINT_ALL_LABELS")) {
        pips_debug(5, "useless Fortran CONTINUE not printed\n");
        r = make_text(NIL);
      } else {
        switch (get_prettyprint_language_tag()) {
          case is_language_fortran:
          case is_language_fortran95:
            u = make_unformatted(strdup(label),
                                 n,
                                 margin,
                                 words_call(instruction_call(obj),
                                            0,
                                            TRUE,
                                            TRUE,
                                            pdl));
            break;
          case is_language_c:
            u = make_unformatted(strdup(label),
                                 n,
                                 margin,
                                 CHAIN_SWORD(words_call(instruction_call(obj),
                                         0, TRUE, TRUE, pdl),
                                     strdup(C_STATEMENT_END_STRING)));
            break;
          default:
            pips_internal_error("Language unknown !");
            break;
        }
        s = make_sentence(is_sentence_unformatted, u);
        r = make_text(CONS(SENTENCE, s, NIL));
      }
      break;
    }
    case is_instruction_unstructured: {
      // append local variables if there is some.
      // local variable need to be inserted before diging the
      // unstructured graph.
      r = insert_locals(r);

      text tmp = text_undefined;
      tmp = text_unstructured(module,
                              label,
                              margin,
                              instruction_unstructured(obj),
                              n);

      // append the unstructured to the current text if it exists
      if ((r != text_undefined) && (r != NULL)) {
        MERGE_TEXTS (r, tmp);
      } else {
        r = tmp;
      }

      break;
    }
    case is_instruction_forloop: {
      r = text_forloop(module, label, margin, instruction_forloop(obj), n, pdl);
      break;
    }
    case is_instruction_expression: {
      list pc = words_expression(instruction_expression(obj), pdl);
      unformatted u;
      pc = CHAIN_SWORD(pc,C_CONTINUE_FUNCTION_NAME);
      u = make_unformatted(strdup(label), n, margin, pc);
      r
          = make_text(CONS(SENTENCE,make_sentence(is_sentence_unformatted, u),NIL));
      break;
    }
    default: {
      pips_internal_error("unexpected tag");
    }
  }
  return (r);
}


/* In case the input code is not C code, non-standard comments have to be detected */
bool  C_comment_p(string c){
  bool is_C_comment=TRUE;
  char * ccp=c;
  char cc=' ';

 init:
  cc=*ccp++;
  if(cc==' '|| cc=='\t' || cc=='\n')
   goto init;
 else if( cc=='/')
   goto slash;
 else if(cc=='\000')
   goto end;
 else {
   is_C_comment=FALSE;
   goto end;
 }

 slash:
  cc=*ccp++;
  if(cc=='*')
   goto slash_star;
 else if(cc=='/')
   goto slash_slash;
 else{
   is_C_comment=FALSE;
   goto end;
 }

 slash_star:
   cc=*ccp++;
 if(cc=='*')
   goto slash_star_star;
  else if(cc=='\0'){
    is_C_comment=FALSE;
    goto end;
  }
 else
   goto slash_star;

 slash_slash:
  cc=*ccp++;
  if(cc=='\n')
   goto init;
  if(cc=='\0') // The comment may not end first with a '\n'
     goto end;
 else
   goto slash_slash;

 slash_star_star:
  cc=*ccp++;
  if(cc=='/')
   goto init;
  else if(cc=='*')
    goto slash_star_star;
  else if(cc=='\0'){
    is_C_comment=FALSE;
    goto end;
  }
  else
   goto slash_star;

 end : return is_C_comment;
}

/* In case comments are not formatted according to C rules, e.g. when
   prettyprinting Fortran code as C code, add // at beginning of lines   */
text C_any_comment_to_text(int r_margin, string c)
{
  string lb = c; /* line beginning */
  string le = c; /* line end */
  string cp = c; /* current position, pointer in comments */
  text ct = make_text(NIL);
  bool is_C_comment = C_comment_p(c);
  int e_margin = r_margin;

  /* We do not need spaces before a line feed */
  if(strcmp(c, "\n")==0)
    e_margin = 0;

  if(strlen(c)>0) {
    for(;*cp!='\0';cp++) {
      if(*cp=='\n') {
	if(cp!=c || TRUE){ // Do not skip \n
	  string cl = gen_strndup0(lb, le-lb);
	  sentence s = sentence_undefined;
	  if(is_C_comment)
	    s = MAKE_ONE_WORD_SENTENCE(e_margin, cl);
	  else if(strlen(cl)>0){
	    list pc = CHAIN_SWORD(NIL, cl); // cl is uselessly duplicated
	    pc = CONS(STRING, MAKE_SWORD("//"), pc);
	    s= make_sentence(is_sentence_unformatted,
			     make_unformatted((char *) NULL, 0, e_margin, pc));
	  }
	  else {
	    s = MAKE_ONE_WORD_SENTENCE(0, cl);
	  }
	  ADD_SENTENCE_TO_TEXT(ct, s);
	  free(cl);
	}
	lb = cp+1;
	le = cp+1;
      }
      else
	le++;
    }
    // Final \n has been removed in the parser presumably by Ronan
    // But this is also useful when non-standard comments are added,
    // for instance by phase "comment_prepend"
    if(lb<cp){
      sentence s = sentence_undefined;
      string sl = gen_strndup0(lb,le-lb);
      if(is_C_comment) {
	s = MAKE_ONE_WORD_SENTENCE(e_margin,sl);
      }
      else {
	list pc = CHAIN_SWORD(NIL, sl); // sl is uselessly duplicated
	pc = CONS(STRING, MAKE_SWORD("//"), pc);
	s = make_sentence(is_sentence_unformatted,
			  make_unformatted((char *) NULL, 0, e_margin, pc));
      }
      ADD_SENTENCE_TO_TEXT(ct,s);
      free(sl);
    } else{
      //ADD_SENTENCE_TO_TEXT(ct,MAKE_ONE_WORD_SENTENCE(0,""));
      ;
    }
  }
  else{// Final \n has been removed by Ronan
    //ADD_SENTENCE_TO_TEXT(ct,MAKE_ONE_WORD_SENTENCE(0,""));
    ;
  }

  return ct;
}

// Ronan's improved version is bugged. It returns many lines for a
// unique \n because le is not updated before looping. Has this code
// been validated?
text C_standard_comment_to_text(int margin, string comment)
{
  string line;
  string le = comment; /* position of a line end */
  text ct = make_text(NIL);

  do {
    /* Find the first end of line: */
    le = strchr(comment, '\n');
    if (le == NULL)
      /* No end-of-line, so use all the rest of the comment: */
      line = strdup(comment);
    else {
      /* Skip the '\n' at the end since the line concept is the notion of
	 sentence */
      line = gen_strndup0(comment, le - comment);
      /* Analyze the next line: */
      comment = le + 1;
    }
    /* Do not indent if the line is empty */
    ADD_SENTENCE_TO_TEXT(ct,
			 MAKE_ONE_WORD_SENTENCE(line[0] == '\0' ? 0 : margin,
						line));
  } while (le != NULL);
  return ct;
}

/* Special handling for C comments  with each line indented according to
   the context.

   I do not see the interest if the user code is already indented... RK
   OK, since the blanks outside the comments are removed by the parser.
*/
text C_comment_to_text(int margin, string comment)
{
  text ct = text_undefined;

  if(C_comment_p(comment))
    //ct = C_standard_comment_to_text(margin, comment);
    ct = C_any_comment_to_text(margin, comment);
  else
    ct = C_any_comment_to_text(margin, comment);
  return ct;
}

/* See text_statement for most parameters
 *
 * braces_p: the statement is within a block; this has an impact of
 * the print-out of continue statements in C, ";"
 *
 * drop_continue_p: another condition to control the print-out of ";"
 * or not;
 *
 * Notes:
 *
 * in simple tests, the statement ";" may be mandatory or not.
 *
 * continue may be used to preserve comments and then the ";" may be
 * dropped
 *
 * Source fidelity would be easier if a new NOP statement that is
 * never printed out were used.
 */
text text_statement_enclosed(entity module,
			     int imargin,
			     statement stmt,
			     bool braces_p,
			     bool drop_continue_p,
			     list pdl)
{
  instruction i = statement_instruction(stmt);
  text r= make_text(NIL);
  text temp;
  string label =
    entity_local_name(statement_label(stmt)) + strlen(LABEL_PREFIX);
  string i_comments = statement_comments(stmt);
  string comments = string_undefined;
  bool braces_added = FALSE;
  int nmargin = imargin;

  // To ease breakpoint setting
  //pips_assert("Blocks have no comments", !instruction_block_p(i)||empty_comments_p(comments));
  if(instruction_block_p(i) && !empty_comments_p(i_comments)) {
    pips_internal_error("Blocks should have no comments\n");
  }

  /* Special handling of comments linked to declarations and to the
     poor job of the lexical analyzer as regards C comments:
     failure. */
  if(empty_comments_p(i_comments)) {
    comments = strdup("");
  }
  else if(declaration_statement_p(stmt)) {
      /* LF interspersed within C struct or union or initialization
         declarations may damage the user comment. However, there is no
         way no know if the LF are valid because thay are located
         between two statements or invalid because they are located
         within one statement. The information is lost by the lexer and
         the parser. */
      //comments = string_strip_final_linefeeds(strdup(i_comments));
      //comments = string_fuse_final_linefeeds(strdup(i_comments));
      comments = strdup(i_comments);
  }
  else {
    comments = strdup(i_comments);
  }

  // the first thing to do is to print the statement extensions
  string ext =  extensions_to_string (statement_extensions (stmt), TRUE);
  if (ext != string_undefined) {
    ADD_SENTENCE_TO_TEXT(r,make_sentence(is_sentence_formatted, ext));
  }

  /* Generate text for local declarations
   *
   * 31/07/2003 Nga Nguyen : This code is added for C, because a
   * statement can have its own declarations
   */
  list dl = statement_declarations(stmt);

  if (!ENDP(dl) && prettyprint_language_is_c_p()) {
    if(statement_block_p(stmt)) {
      if(!braces_p) {
	braces_added = TRUE;
	ADD_SENTENCE_TO_TEXT(r,
			     MAKE_ONE_WORD_SENTENCE(imargin, "{"));
	nmargin += INDENTATION;
      }
    }
    else {
      pips_assert("declarations are carried by continue statements",
		  continue_statement_p(stmt));
    }
    // initialize the local variable text if needed
    if (local_flg == false) {
      local_flg = true;
      local_var =  make_text(NIL);
    }
    if(declaration_statement_p(stmt)) {
      int sn = statement_number(stmt);
      MERGE_TEXTS(local_var,
		  c_text_related_entities(module,dl,nmargin,sn,dl));
    }
    else {
      //MERGE_TEXTS(local_var, c_text_entities(module,l,nmargin));
      // Do nothing and rely on CONTINUE statements...
      ;
    }
  }

  pips_debug(2, "Begin for statement %s with braces_p=%d\n",
	     statement_identification(stmt),braces_p);
  pips_debug(9, "statement_comments: --%s--\n",
	     string_undefined_p(comments)? "<undef>": comments);

  if(statement_number(stmt)!=STATEMENT_NUMBER_UNDEFINED &&
     statement_ordering(stmt)==STATEMENT_ORDERING_UNDEFINED) {
    /* we are in trouble with some kind of dead (?) code...
       but we might as well be dealing with some parsed_code */
    pips_debug(1, "I unexpectedly bumped into dead code?\n");
  }

  if (same_string_p(label, RETURN_LABEL_NAME))
    {
      pips_assert("Statement with return label must be a return statement",
		  return_statement_p(stmt));

      /* do not add a redundant RETURN before an END, unless requested */
      if(get_bool_property("PRETTYPRINT_FINAL_RETURN")
	 || !last_statement_p(stmt))
	{
	  sentence s = MAKE_ONE_WORD_SENTENCE(nmargin, RETURN_FUNCTION_NAME);
	  temp = make_text(CONS(SENTENCE, s ,NIL));
	}
      else {
	temp = make_text(NIL);
      }
    }
  else
    {
      entity m = entity_undefined_p(module)?
	get_current_module_entity()
	: module;
      if(TRUE || !compilation_unit_p(entity_name(m))) {
	/* Do we need to print this CONTINUE statement in C? */
	string cs = statement_comments(stmt);
	entity l = statement_label(stmt);

	if(prettyprint_language_is_c_p()
	   && (braces_p || drop_continue_p)
	   && empty_label_p(entity_local_name(l))
	   && instruction_continue_p(i)) {
	  if(!ENDP(statement_declarations(stmt))) {
	    /* The declarations will be printed, no need for anything else */
	    temp = make_text(NIL);
	  }
	  else if(string_undefined_p(cs) || cs == NULL || strcmp(cs, "")==0) {
	    sentence s = MAKE_ONE_WORD_SENTENCE(0, "");
	    temp = make_text(CONS(SENTENCE, s ,NIL));
	    //temp = make_text(NIL);
	  }
	  else if(strcmp(cs, "\n")==0) {
	    // MAKE_ONE_WORD_SENTENCE already implies a '\n'
	    sentence s = MAKE_ONE_WORD_SENTENCE(0, "");
	    temp = make_text(CONS(SENTENCE, s ,NIL));
	  }
	  else
	    temp = text_instruction(module, label, nmargin, i,
				    statement_number(stmt), pdl);
	}
	else
	  temp = text_instruction(module, label, nmargin, i,
				  statement_number(stmt), pdl);
      }
      else
	temp = make_text(NIL);
    }

  /* Take care of comments and of analysis results printed as comments
   *
   * Note about comments: they are duplicated here, but I'm pretty
   * sure that the free is NEVER performed as it should. FC.
   */
  if(!ENDP(text_sentences(temp))) {
    MERGE_TEXTS(r, init_text_statement(module, nmargin, stmt));
    if (! empty_comments_p(comments)) {
      text ct = text_undefined;
      switch(get_prettyprint_language_tag()) {
        case is_language_fortran:
        case is_language_fortran95:
          ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
                  strdup(comments)));
          break;
        case is_language_c:
          ct = C_comment_to_text(nmargin, comments);
          MERGE_TEXTS(r, ct);
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    }
    MERGE_TEXTS(r, temp);
  }
  else {
    /* Preserve comments and empty C instruction */
    if (! empty_comments_p(comments)) {
      text ct = text_undefined;
      switch (get_prettyprint_language_tag()) {
        case is_language_fortran:
        case is_language_fortran95:
          ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
                  strdup(comments)));
          break;
        case is_language_c:
          ct = C_comment_to_text(nmargin, comments);
          MERGE_TEXTS(r, ct);
          MERGE_TEXTS(r, init_text_statement(module, nmargin, stmt));
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    }
    else if(prettyprint_language_is_c_p() &&
	    !braces_p && !braces_added &&ENDP(dl)) {
      // Because C braces can be eliminated and hence semi-colon
      // may be mandatory in a test branch or in a loop body.
      // A. Mensi
      sentence s = MAKE_ONE_WORD_SENTENCE(nmargin,
					  strdup(C_CONTINUE_FUNCTION_NAME));
      ADD_SENTENCE_TO_TEXT(r, s);
    }
    else if(!ENDP(dl)) {
      MERGE_TEXTS(r, init_text_statement(module, nmargin, stmt));
    }
    free_text(temp);
  }

  /* append local variables  that might have not been inserted
     previously

     FI: this seems to be quite late and might explain the problem
     with local variables of Fortran do loops. Might, because I've
     never managed to figure out exactly what happens...
  */
  r = insert_locals (r);

  if (braces_added) {
    ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(imargin, "}"));
  }
  attach_statement_information_to_text(r, stmt);

  // the last thing to do is to close the extension
  string close =  close_extensions (statement_extensions (stmt), TRUE);
  if (close != string_undefined) {
    ADD_SENTENCE_TO_TEXT(r,make_sentence(is_sentence_formatted, close));
  }

  ifdebug(1) {
    if (instruction_sequence_p(i)) {
      if(!(statement_with_empty_comment_p(stmt)
	   && statement_number(stmt) == STATEMENT_NUMBER_UNDEFINED
	   && unlabelled_statement_p(stmt))) {
	user_log("Block statement %s\n"
		 "Block number=%d, Block label=\"%s\", block comment=\"%s\"\n",
		 statement_identification(stmt),
		 statement_number(stmt), label_local_name(statement_label(stmt)),
		 statement_comments(stmt));
	pips_internal_error("This block statement should be labelless,"
			    " numberless and commentless.\n");
      }
    }
  }
  ifdebug(8){
    fprintf(stderr,"text_statement_enclosed=================================\n");
    print_text(stderr,r);
    fprintf(stderr,"==============================\n");
  }

  free(comments);

  pips_debug(2, "End for statement %s\n", statement_identification(stmt));

  return(r);
}

/* Handles all statements but tests that are nodes of an unstructured.
 * Those are handled by text_control.
 *
 * module: the module containing the statement
 *
 * margin: current tabulation
 *
 * stat: the statement to print
 *
 * pdl: previous declaration list; list of entities that have already
 * been declared and should not be redeclared; this is required for
 * struct and union which may be declared independently or in a nested
 * way. See C_syntax/struct03, 04, 05, etc...
 */
text text_statement(
    entity module,
    int margin,
    statement stmt,
    list pdl)
{
  return text_statement_enclosed(module, margin, stmt, TRUE, TRUE, pdl);
}

/* Keep track of the last statement to decide if a final return can be omitted
 * or not. If no last statement can be found for sure, for instance because it
 * depends on the prettyprinter, last_statement is set to statement_undefined
 * which is safe.
 */
static statement last_statement = statement_undefined;

statement
find_last_statement(statement s)
{
    statement last = statement_undefined;

    pips_assert("statement is defined", !statement_undefined_p(s));

    if(statement_sequence_p(s)) {
	list ls = instruction_block(statement_instruction(s));

	last = (ENDP(ls)? statement_undefined : STATEMENT(CAR(gen_last(ls))));
    }
    else if(statement_unstructured_p(s)) {
	unstructured u = statement_unstructured(s);
	list trail = unstructured_to_trail(u);

	last = control_statement(CONTROL(CAR(trail)));

	gen_free_list(trail);
    }
    else if(statement_call_p(s)) {
	/* Hopefully it is a return statement.
	 * Since the semantics of STOP is ignored by the parser, a
	 * final STOp should be followed by a RETURN.
	 */
	last = s;
    }
    else {
	/* loop or test cannot be last statements of a module */
	last = statement_undefined;
    }

    /* recursive call */
    if(!statement_undefined_p(last)
       && (statement_sequence_p(last) || statement_unstructured_p(last))) {
	last = find_last_statement(last);
    }

    /* Too many program transformations and syntheses violate the following assert */
    if(!(statement_undefined_p(last)
	 || !statement_sequence_p(s)
	 || return_statement_p(last))) {
      switch(get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        pips_user_warning("Last statement is not a RETURN!\n");
        break;
      case is_language_c:
        /* No warning needed for C, is it right for C ?*/
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
      last = statement_undefined;
    }

    /* I had a lot of trouble writing the condition for this assert... */
    pips_assert("Last statement is either undefined or a call to return",
	 statement_undefined_p(last) /* let's give up: it's always safe */
     || !statement_sequence_p(s) /* not a block: any kind of statement... */
		|| return_statement_p(last)); /* if a block, then a return */

    return last;
}

void
set_last_statement(statement s)
{
    statement ls = statement_undefined;
    pips_assert("last statement is undefined",
		statement_undefined_p(last_statement));
    ls = find_last_statement(s);
    last_statement = ls;
}

void reset_last_statement()
{
    last_statement = statement_undefined;
}

bool last_statement_p(statement s) {
    pips_assert("statement is defined\n", !statement_undefined_p(s));
    return s == last_statement;
}

/* Build the text of a module.

   The original text of the declarations is used if possible in
   Fortran. Otherwise, the function text_declaration is called.
 */
text text_named_module(
    entity name, /**< the name of the module */
    entity module,
    statement stat)
{
  text r = make_text(NIL);
  code c = entity_code(module);
  string s = code_decls_text(c);
  text ral = text_undefined;

  debug_on("PRETTYPRINT_DEBUG_LEVEL");

  /* Set the prettyprint language */
  set_prettyprint_language_from_property(language_tag(code_language(c)));

  /* This guard is correct but could be removed if find_last_statement()
   * were robust and/or if the internal representations were always "correct".
   * See also the guard for reset_last_statement()
   */
  if(!get_bool_property("PRETTYPRINT_FINAL_RETURN"))
    set_last_statement(stat);

  precedence_p = !get_bool_property("PRETTYPRINT_ALL_PARENTHESES");
  list l = NIL;
  switch(get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      if(strcmp(s, "") == 0
          || get_bool_property("PRETTYPRINT_ALL_DECLARATIONS")) {
        if(get_bool_property("PRETTYPRINT_HEADER_COMMENTS"))
          /* Add the original header comments if any: */
          ADD_SENTENCE_TO_TEXT(r, get_header_comments(module));

        ADD_SENTENCE_TO_TEXT(r,
            attach_head_to_sentence(sentence_head(name, NIL), module));
        if(head_hook)
          ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_formatted,
                  head_hook(module)));

        if(get_bool_property("PRETTYPRINT_HEADER_COMMENTS"))
          /* Add the original header comments if any: */
          ADD_SENTENCE_TO_TEXT(r, get_declaration_comments(module));

        MERGE_TEXTS(r, text_declaration(module));
        MERGE_TEXTS(r, text_initializations(module));
      } else {
        ADD_SENTENCE_TO_TEXT(r,
            attach_head_to_sentence(make_sentence(is_sentence_formatted,
                    strdup(s)),
                module));
      }
      break;
    case is_language_c:
      /* C prettyprinter */
      pips_debug(3,"Prettyprint function %s\n",entity_name(name));
      if(!compilation_unit_p(entity_name(name))) {
        //entity cu = module_entity_to_compilation_unit_entity(module);
        //list pdl = code_declarations(value_code(entity_initial(cu))));
        /* Print function header if the current module is not a compilation unit*/
        ADD_SENTENCE_TO_TEXT(r,attach_head_to_sentence(sentence_head(name, NIL), module));
        ADD_SENTENCE_TO_TEXT(r,MAKE_ONE_WORD_SENTENCE(0,"{"));
        /* get the declarations for Fortran codes prettyrinted as C,
         as the declarations are not located in the module
         statement. A.Mensi */
        if(ENDP(statement_declarations(stat)) && fortran_module_p(module)) {
          l = code_declarations(value_code(entity_initial(module)));
          MERGE_TEXTS(r,c_text_entities(module, l, INDENTATION, NIL));
        }
      }
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  set_alternate_return_set();
  reset_label_counter();

  if (stat != statement_undefined) {
    /* FI: This function should not be used here because it is part of
       the preprocessor library... */
    //entity cu = module_entity_to_compilation_unit_entity(module);
    switch(get_prettyprint_language_tag()) {
      case is_language_fortran:
      case is_language_fortran95:
        MERGE_TEXTS(r, text_statement(module,
                                      get_prettyprint_indentation(),
                                      stat,
                                      NIL));
        break;
      case is_language_c:
        MERGE_TEXTS(r,
            text_statement(module,
                (compilation_unit_p(entity_name(name)))?0:INDENTATION,
                stat, NIL));
        break;
      default:
        pips_internal_error("Language unknown !");
        break;
    }
  }

  ral = generate_alternate_return_targets();
  reset_alternate_return_set();
  MERGE_TEXTS(r, ral);

  if(!compilation_unit_p(entity_name(name))
      || prettyprint_language_is_fortran_p()) {
    /* No need to print TAIL (}) if the current module is a C compilation unit*/
    ADD_SENTENCE_TO_TEXT(r, sentence_tail(module));
  }

  if(!get_bool_property("PRETTYPRINT_FINAL_RETURN"))
    reset_last_statement();

  debug_off();
  return(r);
}


text text_module(entity module, statement stat) {
  return text_named_module(module, module, stat);
}

text text_graph(), text_control() ;
string control_slabel() ;


/* The node itentifiers are generated from the ordering, more stable than
   the control node address: */
void
add_control_node_identifier_to_text(text r, control c) {
  _int so = statement_ordering(control_statement(c));
  add_one_unformated_printf_to_text(r, "c_%d_%d",
				    ORDERING_NUMBER(so),
				    ORDERING_STATEMENT(so));
}

void output_a_graph_view_of_the_unstructured_successors(text r,
							entity module,
							int margin,
							control c)
{
  _int so = statement_ordering(control_statement(c));
  list pdl = NIL; // FI: I have no idea how to initialize it in this context...

  add_one_unformated_printf_to_text(r, "%s ",
				    PRETTYPRINT_UNSTRUCTURED_ITEM_MARKER);
  add_control_node_identifier_to_text(r, c);
  add_one_unformated_printf_to_text(r, "\n");

  if (get_bool_property("PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH_VERBOSE")) {
    add_one_unformated_printf_to_text(r, "C Unstructured node %p ->", c);
    MAP(CONTROL, a_successor,
	so = statement_ordering(control_statement(a_successor));
	add_one_unformated_printf_to_text(r, " %p", a_successor),
	control_successors(c));
    add_one_unformated_printf_to_text(r,"\n");
  }

  MERGE_TEXTS(r, text_statement(module,
				margin,
				control_statement(c),
				pdl));


  add_one_unformated_printf_to_text(r,
				    PRETTYPRINT_UNSTRUCTURED_SUCC_MARKER);
  MAP(CONTROL, a_successor,
      {
	add_one_unformated_printf_to_text(r, " ");
	add_control_node_identifier_to_text(r, a_successor);
      },
      control_successors(c));
  add_one_unformated_printf_to_text(r,"\n");
}


bool output_a_graph_view_of_the_unstructured_from_a_control(text r,
							    entity module,
							    int margin,
							    control begin_control,
							    control exit_control)
{
  bool exit_node_has_been_displayed = FALSE;
  list blocs = NIL;

  CONTROL_MAP(c,
	      {
		/* Display the statements of each node followed by
		   the list of its successors if any: */
		output_a_graph_view_of_the_unstructured_successors(r,
								   module,
								   margin,
								   c);
		if (c == exit_control)
		  exit_node_has_been_displayed = TRUE;
	      },
	      begin_control,
	      blocs);
  gen_free_list(blocs);

  return exit_node_has_been_displayed;
}

void output_a_graph_view_of_the_unstructured(text r,
					     entity module,
					     string __attribute__ ((unused)) label,
					     int margin,
					     unstructured u,
					     int __attribute__ ((unused)) num)
{
  bool exit_node_has_been_displayed = FALSE;
  control begin_control = unstructured_control(u);
  control end_control = unstructured_exit(u);

  add_one_unformated_printf_to_text(r, "%s ",
				    PRETTYPRINT_UNSTRUCTURED_BEGIN_MARKER);
  add_control_node_identifier_to_text(r, begin_control);
  add_one_unformated_printf_to_text(r, " end: ");
  add_control_node_identifier_to_text(r, end_control);
  add_one_unformated_printf_to_text(r, "\n");

  exit_node_has_been_displayed =
    output_a_graph_view_of_the_unstructured_from_a_control(r,
							   module,
							   margin,
							   begin_control,
							   end_control);

  /* If we have not displayed the exit node, that mean that it is not
     connex with the entry node and so the code is
     unreachable. Anyway, it has to be displayed as for the classical
     Sequential View: */
  if (! exit_node_has_been_displayed) {
    /* Note that since the controlizer adds a dummy successor to the
       exit node, use
       output_a_graph_view_of_the_unstructured_from_a_control()
       instead of
       output_a_graph_view_of_the_unstructured_successors(): */
    output_a_graph_view_of_the_unstructured_from_a_control(r,
							   module,
							   margin,
							   end_control,
							   end_control);
    /* Even if the code is unreachable, add the fact that the
       control above is semantically related to the entry node. Add
       a dash arrow from the entry node to the exit node in daVinci,
       for example: */
    add_one_unformated_printf_to_text(r, "%s ",
				      PRETTYPRINT_UNREACHABLE_EXIT_MARKER);
    add_control_node_identifier_to_text(r, begin_control);
    add_one_unformated_printf_to_text(r, " -> ");
    add_control_node_identifier_to_text(r, end_control);
    add_one_unformated_printf_to_text(r, "\n");
    if (get_bool_property("PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH_VERBOSE"))
      add_one_unformated_printf_to_text(r, "C Unreachable exit node (%p -> %p)\n",
					begin_control,
					end_control);
  }

  add_one_unformated_printf_to_text(r, "%s ",
				    PRETTYPRINT_UNSTRUCTURED_END_MARKER);
  add_control_node_identifier_to_text(r, begin_control);
  add_one_unformated_printf_to_text(r, " end: ");
  add_control_node_identifier_to_text(r, end_control);
  add_one_unformated_printf_to_text(r, "\n");
}


/* ================C prettyprinter functions================= */

static list words_cast(cast obj, int precedence, list pdl)
{
  list pc = NIL;
  type t = cast_type(obj);
  expression exp = cast_expression(obj);
  bool space_p = get_bool_property("PRETTYPRINT_LISTS_WITH_SPACES");

  pc = CHAIN_SWORD(pc,"(");
  pc = gen_nconc(pc, c_words_entity(t, NIL, pdl));
  pc = CHAIN_SWORD(pc, space_p? ") " : ")");
  pc = gen_nconc(pc, words_subexpression(exp, CAST_OPERATOR_PRECEDENCE, TRUE, pdl));
  if(get_bool_property("PRETTYPRINT_ALL_PARENTHESES")  || precedence >= 25) {
    pc = CONS(STRING, strdup("("),
	      gen_nconc(pc,CONS(STRING, strdup(")"), NIL)));
  }
  return pc;
}

static list words_sizeofexpression(sizeofexpression obj,
				   bool in_type_declaration,
				   list pdl)
{
  list pc = NIL;
  pc = CHAIN_SWORD(pc,"sizeof(");
  if (sizeofexpression_type_p(obj)) {
    type t = sizeofexpression_type(obj);
    /* FI: the test used below is probably too strict I believe, because
       dimensions are not allowed, but I may be wrong*/
    if(derived_type_p(t)) {
      entity te = basic_derived(variable_basic(type_variable(t)));
      if(!gen_in_list_p((void *) te, pdl)) {
	list pca = words_type(sizeofexpression_type(obj), pdl);
	pc = gen_nconc(pc, pca);
      }
      else {
	/* The type must be fully declared: see struct15.c */
	list pct = c_words_simplified_entity(t, NIL, TRUE, in_type_declaration, pdl);
	pc = gen_nconc(pc, pct);
      }
    }
    else {
      list pca = words_type(sizeofexpression_type(obj), pdl);
      pc = gen_nconc(pc, pca);
    }
  }
  else
    pc = gen_nconc(pc, words_expression(sizeofexpression_expression(obj), pdl));
  pc = CHAIN_SWORD(pc,")");
  return pc;
}

static list words_subscript(subscript s, list pdl)
{
  list pc = NIL;
  expression a = subscript_array(s);
  list lexp = subscript_indices(s);
  bool first = TRUE;

  /* Parentheses must be added for array expression
   * like __ctype+1 in (__ctype+1)[*np]
   */

  /* Here we differentiate the indices parenthesis syntax */
  switch(get_prettyprint_language_tag()) {
    case is_language_fortran:
      pips_internal_error("We don't know how to prettyprint a subscript in "
          "Fortran, aborting");
    case is_language_fortran95: {
      bool allocatable_p = expression_allocatable_data_access_p(a);
      pips_assert("We don't know how to prettyprint a subscript in Fortran95 "
          "and it's not an allocatable",
          allocatable_p );
      pc = gen_nconc(pc, words_expression(a, pdl));
      if(!ENDP(lexp)) {
        pc = CHAIN_SWORD(pc,"(");
      }
      break;
    }
    case is_language_c:
      pc = CHAIN_SWORD(pc,"(");
      pc = gen_nconc(pc, words_expression(a, pdl));
      pc = CHAIN_SWORD(pc,")");
      if(!ENDP(lexp)) {
        pc = CHAIN_SWORD(pc,"[");
      }
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }

  /* Print now the indices list */
  FOREACH(expression,exp,lexp) {
    if(!first) {
      switch(get_prettyprint_language_tag()) {
        case is_language_fortran:
        case is_language_fortran95:
          pc = CHAIN_SWORD(pc, ",");
          break;
        case is_language_c:
          pc = CHAIN_SWORD(pc,"][");
          break;
        default:
          pips_internal_error("Language unknown !");
          break;
      }
    }
    pc = gen_nconc(pc, words_expression(exp, pdl));
    first = FALSE;
  }

  /* Here we differentiate the indices syntax */
  switch(get_prettyprint_language_tag()) {
    case is_language_fortran:
    case is_language_fortran95:
      if(!ENDP(lexp)) {
        pc = CHAIN_SWORD(pc,")");
      }
      break;
    case is_language_c:
      if(!ENDP(lexp)) {
        pc = CHAIN_SWORD(pc,"]");
      }
      break;
    default:
      pips_internal_error("Language unknown !");
      break;
  }


  return pc;
}

static list words_application(application a, list pdl)
{
  list pc = NIL;
  expression f = application_function(a);
  list lexp = application_arguments(a);
  bool first = TRUE;
  /* Parentheses must be added for function expression */
  pc = CHAIN_SWORD(pc,"(");
  pc = gen_nconc(pc, words_expression(f, pdl));
  pc = CHAIN_SWORD(pc,")(");
  MAP(EXPRESSION,exp,
  {
    if (!first)
      pc = CHAIN_SWORD(pc,",");
    pc = gen_nconc(pc, words_expression(exp, pdl));
    first = FALSE;
  },lexp);
  pc = CHAIN_SWORD(pc,")");
  return pc;
}

static text text_forloop(entity module,
			 string label,
			 int margin,
			 forloop obj,
			 int n,
			 list pdl)
{
    list pc = NIL;
    unformatted u;
    text r = make_text(NIL);
    statement body = forloop_body(obj) ;
    //instruction i = statement_instruction(body);

    pc = CHAIN_SWORD(pc,"for (");
    if (!expression_undefined_p(forloop_initialization(obj)))
      pc = gen_nconc(pc, words_expression(forloop_initialization(obj), pdl));
    pc = CHAIN_SWORD(pc,C_STATEMENT_END_STRING);
    if (!expression_undefined_p(forloop_condition(obj))) {
      /* To restitute for(;;) */
      expression cond = forloop_condition(obj);
      if(!expression_one_p(cond))
	pc = gen_nconc(pc, words_expression(forloop_condition(obj), pdl));
    }
    pc = CHAIN_SWORD(pc,C_STATEMENT_END_STRING);
    if (!expression_undefined_p(forloop_increment(obj)))
      pc = gen_nconc(pc, words_expression(forloop_increment(obj), pdl));
    pc = CHAIN_SWORD(pc,one_liner_p(body)?")":") {");
    u = make_unformatted(strdup(label), n, margin, pc) ;
    ADD_SENTENCE_TO_TEXT(r, make_sentence(is_sentence_unformatted, u));

    if(one_liner_p(body)) {
      MERGE_TEXTS(r, text_statement_enclosed(module,
					     margin+INDENTATION,
					     body,
					     !one_liner_p(body),
					     !one_liner_p(body),
					     pdl));
    }
    else {
      // ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"{"));
      MERGE_TEXTS(r, text_statement(module, margin+INDENTATION, body, pdl));
      ADD_SENTENCE_TO_TEXT(r, MAKE_ONE_WORD_SENTENCE(margin,"}"));
    }

    return r;
}

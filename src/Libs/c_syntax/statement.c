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

/*******************  STATEMENTS *******************/

/* Attention, the null statement in C is represented as the continue
   statement in Fortran (make_continue_statement means make_null_statement)*/

// To have strndup(), asprintf()...:
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "parser_private.h"
#include "c_syntax.h"


#include "resources.h"
#include "database.h"

#include "misc.h"
#include "text-util.h"
#include "properties.h"
#include "alias_private.h"

stack BlockStack; /* BlockStack is used to handle block scope */

list LabeledStatements; /* list of labeled statements of the current module*/

stack SwitchGotoStack = stack_undefined;
stack SwitchControllerStack = stack_undefined;
stack LoopStack = stack_undefined; /* is used for switch statements also, because we do not
					distinguish a break in a loop or a switch */

void MakeCurrentModule(entity e)
{
  /* This must be changed later, the storage is of type return and we
     have to create a new entity*/
  entity_storage(e) = make_storage_rom() /* make_storage_return(e) */;
  if (value_undefined_p(entity_initial(e)))
    entity_initial(e) = make_value(is_value_code,
				   make_code(NIL,strdup(""),
					     make_sequence(NIL),
					     NIL,
					     make_language_c()));
  /* code_declaration to be updated : only need formal parameters, because the others are added in
     block statement declaration ? */
  pips_debug(4,"Set current module entity %s\n",entity_user_name(e));

  /* The next two tests are replicated from the Fortran parser,
     Syntax/procedure.c, MakeCurrentFunction() */

  /* In case the module is parsed a second time, clean up the symbol
     table to avoid variable redefinition warnings and errors */
  if(!value_undefined_p(entity_initial(e))) {
    if(value_code_p(entity_initial(e))) {
      code c = value_code(entity_initial(e));
      if(!code_undefined_p(c) && !ENDP(code_declarations(c))) {
	/* Clean up existing local entities in case of a recompilation. */
	CCleanLocalEntities(e);
      }
    }
  }

  /* Let's hope cf is not an intrinsic: name conflict (the problem may
     have been detected earlier in UpdateEntity() if there are
     arguments) */
  if( entity_type(e) != type_undefined
      && intrinsic_entity_p(e) ) {
    pips_user_warning("Intrinsic %s redefined.\n"
		      "This is not supported by PIPS. Please rename %s\n",
		      entity_local_name(e), entity_local_name(e));
    /* Unfortunately, an intrinsics cannot be redefined, just like a user function
     * or subroutine after editing because intrinsics are not handled like
     * user functions or subroutines. They are not added to the called_modules
     * list of other modules, unless the redefining module is parsed FIRST.
     * There is not mechanism in PIPS to control the parsing order.
     */
    CParserError("Name conflict between a "
		 "function and an intrinsic\n");
  }


  set_current_module_entity(e);
  init_c_areas();
  init_c_implicit_variables(e);
  LabeledStatements = NIL;
  SwitchGotoStack = stack_make(sequence_domain,0,0);
  SwitchControllerStack = stack_make(expression_domain,0,0);
  LoopStack = stack_make(basic_domain,0,0);
}

void ResetCurrentModule()
{
  CModuleMemoryAllocation(get_current_module_entity());
  if (get_bool_property("PARSER_DUMP_SYMBOL_TABLE"))
    fprint_C_environment(stderr, get_current_module_entity());
  pips_debug(4,"Reset current module entity %s\n",get_current_module_name());
  reset_current_module_entity();
  stack_free(&SwitchGotoStack);
  stack_free(&SwitchControllerStack);
  stack_free(&LoopStack);
  stack_free(&BlockStack);
  /* Reset them to stack_undefined_p instead of STACK_NULL */
  SwitchGotoStack = stack_undefined;
  SwitchControllerStack = stack_undefined;
  LoopStack = stack_undefined;
  BlockStack = stack_undefined;
}

void InitializeBlock()
{
  BlockStack = stack_make(statement_domain,0,0);
}

statement MakeBlock(list decls, list stmts)
{
  /* To please to current RI choices about Fortran, blocks cannot carry
     line numbers nor comments */
  /* Anyway, it might be much too late to retrieve the comment
     associated to the beginning of the block. The lost comment
     appears after the last statement of the block. To save it, as is
     done in Fortran, an empty statement should be added at the end of
     the sequence. */
  /* Because of old C standard, the initial list of declaration
     statements is separated from the executable statements. However,
     C99 allows declarations anywhere in the block. So some
     declaration statements may be located in stms. */
  list sl = gen_nconc(decls, stmts);
  list dl = statements_to_direct_declarations(sl);

  statement s = make_statement(entity_empty_label(),
			       STATEMENT_NUMBER_UNDEFINED /* get_current_C_line_number() */,
			       STATEMENT_ORDERING_UNDEFINED,
			       empty_comments /* get_current_C_comment() */,
			       make_instruction_sequence(make_sequence(sl)),
			       dl, string_undefined, empty_extensions ());

  discard_C_comment();

  ifdebug(1) {
      fprintf(stderr, "Declaration list: ");
      if(ENDP(statement_declarations(s)))
	fprintf(stderr, "NONE\n");
      else {
	print_entities(statement_declarations(s));
	fprintf(stderr, "\n");
      }
    }

  pips_assert("Block statement is consistent",statement_consistent_p(s));
  return s;
}


statement FindStatementFromLabel(entity l)
{
  MAP(STATEMENT,s,
  {
    if (statement_label(s) == l)
      return s;
  },LabeledStatements);
  return statement_undefined;
}


/* Construct a new statement from \param s by adding a \param label and a
   \param comment. If it is possible to do it without creating a new
   statement, retun the old one modified according to.
*/
statement MakeLabeledStatement(string label, statement s, string comment) {
  entity l = MakeCLabel(label);
  statement labeled_statement;
  // Get the statement with the l label, if any:
  statement smt = FindStatementFromLabel(l);
  if (smt == statement_undefined) {
    /* There is no other statement already associated with this
       label... */
    /* Add a label and deal all the gory details about the PIPS internal
     representation: */
    s = add_label_to_statement(l, s, &labeled_statement);
    // Keep a track of this statement associated to a label:
    LabeledStatements = CONS(STATEMENT, labeled_statement, LabeledStatements);
  }
  else {
    /* There is already a statement stub smt associated to this label with
       some gotos pointing to it, so keep it as the labeled target. */
    if (!instruction_sequence_p(statement_instruction(s))
	&& unlabelled_statement_p(s)) {
      /* The statement does not have a label and can accept one, so
	 patch in place smt: */
      statement_instruction(smt) =  statement_instruction(s);
      statement_comments(smt) = statement_comments(s);
      // Discard the old statement:
      statement_instruction(s) = instruction_undefined;
      statement_comments(s) = string_undefined;
      free_statement(s);
      // And keep the old one:
      s = smt;
      labeled_statement = s;
    }
    else {
      /* The statement can not accept a label or another one, just keep
	 the previous label in front: */
      list stmts = gen_statement_cons(s, NIL);
      stmts = gen_statement_cons(smt, stmts);
      statement seq = make_block_statement(stmts);
      labeled_statement = smt;
      s = seq;
    }
  }
  // Associate the current comment to the statement with the label:
  if (comment != string_undefined) {
    insert_comments_to_statement(labeled_statement, comment);
    free(comment);
  }
  return s;
}


statement MakeGotoStatement(string label)
{
  entity l = MakeCLabel(label);
  statement gts = statement_undefined;

  /* Find the corresponding statement from its label,
     if not found, create a pseudo one, which will be replaced lately when
     we see the statement (label: statement) */

  statement s = FindStatementFromLabel(l);
  if (s == statement_undefined) {
    s = make_continue_statement(l);
    LabeledStatements = CONS(STATEMENT,s,LabeledStatements);
  }
  gts = make_statement(entity_empty_label(),
		       get_current_C_line_number(),
		       STATEMENT_ORDERING_UNDEFINED,
		       get_current_C_comment(),
		       make_instruction(is_instruction_goto,s),NIL,NULL,
		       empty_extensions ());

  return gts;
}

/* The labels in C have function scope. */

entity MakeCLabel(string s)
{
  string ename = strdup(concatenate(LABEL_PREFIX,s,NULL));
  entity l = FindOrCreateEntity(get_current_module_name(),ename);
  free(ename);
  if (entity_type(l) == type_undefined)
    {
      pips_debug(7,"Label %s\n", s);
      entity_type(l) = MakeTypeStatement();
      entity_storage(l) = make_storage_rom();
      entity_initial(l) = make_value(is_value_constant,
				     make_constant_litteral());
    }
  else
    pips_debug(7, "Label %s already exists\n", s);
  return(l);
}


statement MakeWhileLoop(list lexp, statement s, bool before)
{
  statement smt;
  int i = basic_int((basic) stack_head(LoopStack));
  string lab1;
  asprintf(&lab1,"%s%d","loop_end_",i);
  statement s1 = FindStatementFromLabel(MakeCLabel(lab1));
  free(lab1);
  string lab2;
  asprintf(&lab2,"%s%d","break_",i);
  statement s2 = FindStatementFromLabel(MakeCLabel(lab2));
  free(lab2);

  if (!statement_undefined_p(s1))
    {
      /* This loop has a continue statement which has been transformed to goto
	 Add the labeled statement at the end of loop body*/
      insert_statement(s,s1,FALSE);
    }

  smt = make_whileloop_statement(MakeCommaExpression(lexp),
				 s,
				 get_current_C_line_number(),
				 before);
  if (!statement_undefined_p(s2))
    {
      /* This loop has a break statement which has been transformed to goto
	 Add the labeled statement after the loop */
      insert_statement(smt,s2,FALSE);
    }

  pips_assert("While loop is consistent",statement_consistent_p(smt));
  ifdebug(5)
    {
      printf("While loop statement: \n");
      print_statement(smt);
    }
  return smt;
}


/* Create a for-loop statement with some parser-specific characteristics.

   A more generic implementation would have been in ri-util instead.

   @param[in] e1 is the init part of the for

   @param[in] e2 is the conditional part of the for

   @param[in] e3 is the increment part of the for

   @param[in] body is the loop body statement
*/
statement MakeForloop(expression e1,
		      expression e2,
		      expression e3,
		      statement body) {
  forloop f;
  statement smt;
  string sc = pop_current_C_comment();
  int sn = pop_current_C_line_number();
  expression init = e1;
  expression cond = e2;
  expression inc = e3;

  pips_assert("For loop body consistent",statement_consistent_p(body));

  if(expression_undefined_p(init))
    init = make_call_expression(entity_intrinsic(CONTINUE_FUNCTION_NAME),
				NIL);
  else
    simplify_C_expression(init);

  if(expression_undefined_p(cond))
    /* A bool C constant cannot be used
       because stdbool.h may not be
       included */
    /* cond = make_call_expression(MakeConstant(TRUE_OPERATOR_NAME, */
    /* is_basic_logical), */
    /* NIL); */
    cond = int_to_expression(1);
  else
    simplify_C_expression(cond);

  if(expression_undefined_p(inc))
    inc = make_call_expression(entity_intrinsic(CONTINUE_FUNCTION_NAME),
			       NIL);
  else
    simplify_C_expression(inc);


  int i = basic_int((basic) stack_head(LoopStack));
  /* Create some land-pad labels to deal with break and continue.

     Looks like some memory leaks if no break or continue... */
  string lab1;
  asprintf(&lab1, "%s%d","loop_end_", i);
  statement s1 = FindStatementFromLabel(MakeCLabel(lab1));
  free(lab1);
  string lab2;
  asprintf(&lab2, "%s%d", "break_", i);
  statement s2 = FindStatementFromLabel(MakeCLabel(lab2));
  free(lab2);

  if (!statement_undefined_p(s1))
    /* This loop has a continue statement which has been transformed to goto.

       Add the labeled statement at the end of loop body*/
    insert_statement(body, s1, FALSE);

  /*  The for clause may contain declarations*/
  f = make_forloop(init, cond, inc, body);
  smt = make_statement(entity_empty_label(),
		       get_current_C_line_number(),
		       STATEMENT_ORDERING_UNDEFINED,
		       string_undefined,
		       make_instruction_forloop(f),
		       NIL, string_undefined,
		       empty_extensions());

  if (!statement_undefined_p(s2))
    /* This loop has a break statement which has been transformed to goto
       Add the labeled statement after the loop */
    insert_statement(smt, s2, FALSE);

  smt = add_comment_and_line_number(smt, sc, sn);
  stack_pop(LoopStack);
  pips_assert("For loop consistent", statement_consistent_p(smt));

  pips_assert("For loop is consistent", forloop_consistent_p(f));
  ifdebug(5) {
    printf("For loop statement: \n");
    print_statement(smt);
  }
  return smt;
}


/* Create a C99 for-loop statement with an initializer as first parameter,
   with some parser-specific characteristics.

   @param[in,out] s1 is the init part of the for. It is a list with one
   statement

   @param[in] e2 is the conditional part of the for

   @param[in] e3 is the increment part of the for

   @param[in] body is the loop body statement
*/
statement MakeForloopWithIndexDeclaration(list s1,
					  expression e2,
					  expression e3,
					  statement body) {
  //split_initializations_in_statement(statement s)
  pips_assert("s1 is a list of one element", gen_length(s1) == 1);
  // Get the initializer statement:
  statement init = STATEMENT(CAR(s1));
  gen_free_list(s1);
  statement for_s = MakeForloop(expression_undefined, e2, e3, body);
  insert_statement(init, for_s, FALSE);
  return init;
}

statement MakeSwitchStatement(statement s)
{
  /* Transform a switch statement to if - else - goto. Example:

     switch (c) {
     case 1:
     s1;
     case 2:
     s2;
     default:
     sd;
     }

     if (c==1) goto switch_xxx_case_1;
     if (c==2) goto switch_xxx_case_2;
     goto switch_xxx_default;
     switch_xxx_case_1: ;
     s1;
     switch_xxx_case_2: ;
     s2;
     switch_xxx_default: ;
     sd;

     In si, we can have goto break_xxx; (which was a break)

     and break_xxx: ; is inserted at the end of the switch statement

     The statement s corresponds to the body

     switch_xxx_case_1: ;
     s1;
     switch_xxx_case_2: ;
     s2;
     switch_xxx_default: ;
     sd;

     we have to insert

     if (c==1) goto switch_xxx_case_1;
     if (c==2) goto switch_xxx_case_2;
     goto switch_xxx_default;


     before s and return the inserted statement.  */
  int i = basic_int((basic) stack_head(LoopStack));
  string lab ;
  asprintf(&lab,"break_%d",i);
  statement smt = statement_undefined;
  statement seq = statement_undefined;
  sequence oseq = (sequence)stack_head(SwitchGotoStack);
  list tl = sequence_statements(oseq);
  list ct = list_undefined;
  list ntl = NIL;
  statement ds = statement_undefined;

  ifdebug(8) {
    pips_debug(8, "tl=%p\n", tl);
  }

  /* For the time being, the switch comment is lost. It should already
     be included in the argument,s  */
  /* pop_current_C_comment(); */

  /* Make sure the default case is the last one in the test sequence */
  for(ct=tl;!ENDP(ct); POP(ct)) {
    statement s = STATEMENT(CAR(ct));

    if(instruction_goto_p(statement_instruction(s))) {
      ds = s;
    }
    else {
      /* Keep the cases in the user order. Tobe checked at the
	 pARSED_PRINTED_FILE level. */
      ntl = gen_nconc(ntl, CONS(STATEMENT,s,NIL));
      //ntl = CONS(STATEMENT,s,ntl);
    }
  }
  if(statement_undefined_p(ds)) {
    /* no default case, jump out of the switch control structure */
    ds = MakeBreakStatement(string_undefined /*strdup("")*/);
  }
  ntl = gen_nconc(ntl, CONS(STATEMENT,ds,NIL));
  gen_free_list(tl);
  sequence_statements(oseq)=NIL;
  free_sequence(oseq);
  seq = instruction_to_statement(make_instruction_sequence(make_sequence(ntl)));
  //seq = instruction_to_statement(make_instruction_sequence(make_sequence(tl)));

  insert_statement(s,seq,TRUE);

  smt = FindStatementFromLabel(MakeCLabel(lab));
  free(lab);
  if (!statement_undefined_p(smt))
    {
      /* This switch has a break statement which has been transformed to goto
	 Add the labeled statement after the switch */
      insert_statement(s,smt,FALSE);
    }
  pips_assert("Switch is consistent",statement_consistent_p(s));
  ifdebug(5)
    {
      printf("Switch statement: \n");
      print_statement(s);
    }
  return s;
}

statement MakeCaseStatement(expression e)
{
  /* Transform
         case e:
     to
         switch_xxx_case_e: ;
     and generate
        if (c == e) goto switch_xxx_case_e
     where c is retrieved from SwitchControllerStack
           xxx is unique from LoopStack */
  int i = basic_int((basic) stack_head(LoopStack));
  string lab ;
  /* It might be easier to evaluate e since e must be evaluable at
     compile time... */
  string estr = words_to_string(words_expression(e, NIL));
  string restr = estr;

  /* The expression may be a character */
  if(*estr=='\'') {
    /* remove the quotes */
    restr++;
    *(estr+strlen(estr)-1) = '\000';
  }

  /* Make sure restr only contains C characters valid for a label if
     a character constant is used: is_letter || is_digit || '_'. */
  if(strspn(restr,
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_abcdefghijklmnopqrstuvwxyz")!=strlen(restr) && *estr=='\'') {
    /* illegal characters such as '?' or ',' or '.' must be converted
       as well as octal constant such as '\001' and special
       characters such as '\n' */
    if(strlen(restr)==1) {
      /* Must be an illegal character for a label */
      /* FI: not too safe to make it octal among decimal because it
	 can generate a label conflict. */
      asprintf(&lab,"switch_%d_case_%hhd",i,*restr);
    }
    else if(*restr=='\\') {
      if(*(restr+1)=='0'||*(restr+1)=='1'||*(restr+1)=='2'||*(restr+1)=='3')
	/* octal character */
	asprintf(&lab,"switch_%d_case_%s",i,restr+1);
      else {
	/* FI: let's deal with special cases such as \n, \r, \t,...*/
	char labc; // A string would carry more ASCII information
	if(*(restr+1)=='a') // bell
	  labc = '\a'; // "BEL"
	else if(*(restr+1)=='b') // backspace
	  labc = '\b'; // "BS"
	else if(*(restr+1)=='f') // form feed
	  labc = '\f'; // "FF"
	else if(*(restr+1)=='n') // new line
	  labc = '\n'; // "LF"
	else if(*(restr+1)=='t') // horizontal tab
	  labc = '\t'; // "HT"
	else if(*(restr+1)=='r') // carriage return
	  labc = '\r'; // "CR"
	else if(*(restr+1)=='v') // vertical tab
	  labc = '\v'; // "VT"
	else
	  pips_internal_error("Unexpected case.");
	asprintf(&lab,"switch_%d_case_%hhd",i,labc);
      }
    }
  }
  else
    asprintf(&lab,"switch_%d_case_%s",i,restr);

  free(estr);
  statement s = MakeLabeledStatement(lab,
				     make_continue_statement(entity_empty_label()),
				     get_current_C_comment());
  expression cond = call_to_expression(make_call(entity_intrinsic("=="),
						 CONS(EXPRESSION, stack_head(SwitchControllerStack),
						      CONS(EXPRESSION, e, NIL))));
  test t = make_test(cond,MakeGotoStatement(lab),make_continue_statement(entity_undefined));
  sequence CurrentSwitchGotoStack = stack_head(SwitchGotoStack);
  sequence_statements(CurrentSwitchGotoStack) = gen_nconc(sequence_statements(CurrentSwitchGotoStack),
							       CONS(STATEMENT,test_to_statement(t),NULL));
  return s;
}

statement MakeDefaultStatement()
{
  /* Return the labeled statement
       switch_xxx_default: ;
     and add
       goto switch_xxx_default;
     to the switch header */
  int i = basic_int((basic) stack_head(LoopStack));
  string lab;
 asprintf(&lab,"switch_%d_default",i);
  statement s = MakeLabeledStatement(lab,
				     make_continue_statement(entity_empty_label()),
				     get_current_C_comment());
  sequence CurrentSwitchGoto = stack_head(SwitchGotoStack);
  /* If the default case is not last, it must be moved later in the
     sequence_statements(CurrentSwitchGoto) */
  sequence_statements(CurrentSwitchGoto) = gen_nconc(sequence_statements(CurrentSwitchGoto),
							       CONS(STATEMENT,MakeGotoStatement(lab),NULL));
  free(lab);
  return s;
}

statement MakeBreakStatement(string cmt)
{
  /* NN : I did not add a boolean variable to distinguish between loop and switch statements :-(*/
  int i = basic_int((basic) stack_head(LoopStack));
  string lab;
  asprintf(&lab,"break_%d",i);
  statement bs = MakeGotoStatement(lab);
  free(lab);

  statement_comments(bs) = cmt;

  return bs;
}

statement MakeContinueStatement(string cmt)
{
  /* Unique label with the LoopStack */
  int i = basic_int((basic) stack_head(LoopStack));
  string lab;
  asprintf(&lab,"loop_end_%d",i);
  statement cs = MakeGotoStatement(lab);
  free(lab);

  statement_comments(cs) = cmt;

  return cs;
}

/* e is now owned by returned statement */
statement ExpressionToStatement(expression e)
{
  syntax s = expression_syntax(e);
  statement st = statement_undefined;
  string c = get_current_C_comment();

  if (syntax_call_p(s)) {
    st = call_to_statement(syntax_call(s));
    syntax_call(s)=call_undefined;
    free_expression(e);
  }
  else
    st = instruction_to_statement(make_instruction_expression(e));

  statement_number(st) = get_current_C_line_number();
  if(!string_undefined_p(c)) {
    statement_comments(st) = c;
  }

  return st;
}






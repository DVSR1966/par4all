
/*******************  STATEMENTS ******************
 $Id$
 */

/* Attention, the null statement in C is represented as the continue 
   statement in Fortran (make_continue_statement means make_null_statement)*/

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "parser_private.h"

#include "c_syntax.h"
#include "syntax.h"

#include "resources.h"
#include "database.h"
#include "makefile.h"

#include "misc.h"
#include "pipsdbm.h"
#include "text-util.h"
#include "properties.h"
#include "alias_private.h"
#include "instrumentation.h"

extern statement ModuleStatement;

stack BlockStack; /* BlockStack is used to handle block scope */

list LabeledStatements; /* list of labeled statements of the current module*/

stack SwitchGotoStack = stack_undefined;
stack SwitchControllerStack = stack_undefined; 
stack LoopStack = stack_undefined; /* is used for switch statements also, because we do not 
					distinguish a break in a loop or a switch */

void MakeCurrentModule(entity e)
{
  /* This must be changed later, the storage is of type return and we have to create a new entity*/
  entity_storage(e) = make_storage_return(e); 
  if (value_undefined_p(entity_initial(e)))
    entity_initial(e) = make_value(is_value_code, make_code(NIL,strdup(""), make_sequence(NIL),NIL));
  /* code_declaration to be updated : only need formal parameters, because the others are added in
     block statement declaration ? */
  pips_debug(4,"Set current module entity %s\n",entity_user_name(e));
  set_current_module_entity(e);
  init_c_areas(); 
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

statement MakeBlock(list decls, list stms)
{ 
  /* To please the controlizer, blocks cannot carry line numbers nor comments */
  /* Anyway, it might be much too late to retrieve the comment
     associated to the beginning of the block. The lost comment
     appears after the last statement of the block. To save it, as is
     done in Fortran, an empty statement should be added at the end of
     the sequence. */

  statement s = make_statement(entity_empty_label(), 
			       STATEMENT_NUMBER_UNDEFINED /* get_current_C_line_number() */, 
			       STATEMENT_ORDERING_UNDEFINED, 
			       empty_comments /* get_current_C_comment() */,
			       make_instruction_sequence(make_sequence(stms)),
			       decls, string_undefined);

  discard_C_comment();

  ifdebug(1) 
    {
      printf("Declaration list: ");
      print_entities(statement_declarations(s));
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

statement MakeLabeledStatement(string label, statement s)
{
  entity l = MakeCLabel(label);
  statement smt = FindStatementFromLabel(l);
  statement st;
  if (smt == statement_undefined) 
    {
      st = make_statement(l, STATEMENT_NUMBER_UNDEFINED, 
			 STATEMENT_ORDERING_UNDEFINED, 
			 get_current_C_comment(), 
			 statement_instruction(s),
			 NIL,string_undefined);
      LabeledStatements = CONS(STATEMENT,st,LabeledStatements);
   }
  else 
    {
      /* The statement is already created pseudoly, replace it by the real one*/
      statement_instruction(smt) =  statement_instruction(s);
      statement_comments(smt) = statement_comments(s);
      st = smt;
    }
  return st;
}

statement MakeGotoStatement(string label)
{
  entity l = MakeCLabel(label);
  statement gts = statement_undefined;

  /* Find the corresponding statement from its label, 
     if not found, create a pseudo one, which will be replaced lately when
     we see the statement (label: statement) */
 
  statement s = FindStatementFromLabel(l);
  if (s == statement_undefined) 
    {
      s = make_statement(l,STATEMENT_NUMBER_UNDEFINED,
			 STATEMENT_ORDERING_UNDEFINED,
			 string_undefined, 
			 make_continue_instruction(),NIL,NULL);
      LabeledStatements = CONS(STATEMENT,s,LabeledStatements);

    }
  gts = make_statement(entity_empty_label(),
		       get_current_C_line_number(),
		       STATEMENT_ORDERING_UNDEFINED,
		       get_current_C_comment(), 
		       make_instruction(is_instruction_goto,s),NIL,NULL);

 return gts;
}

/* The labels in C have function scope. */

entity MakeCLabel(string s)
{
  entity l = FindOrCreateEntity(get_current_module_name(),strdup(concatenate(LABEL_PREFIX,s,NULL)));
  if (entity_type(l) == type_undefined) 
    {
      pips_debug(7,"Label %s\n", s);
      entity_type(l) = MakeTypeStatement();
      entity_storage(l) = MakeStorageRom();
      entity_initial(l) = make_value(is_value_constant,
				     MakeConstantLitteral());
    }
  else 
    pips_debug(7, "Label %s already exists\n", s);
  return(l);
}


statement MakeWhileLoop(list lexp, statement s, bool before)
{  
  whileloop w; 
  statement smt;
  int i = basic_int((basic) stack_head(LoopStack));
  string lab1 = strdup(concatenate("loop_end_",int_to_string(i),NULL));
  statement s1 = FindStatementFromLabel(MakeCLabel(lab1));
  string lab2 = strdup(concatenate("break_",int_to_string(i),NULL));
  statement s2 = FindStatementFromLabel(MakeCLabel(lab2));

  if (!statement_undefined_p(s1))
    {
      /* This loop has a continue statement which has been transformed to goto 
	 Add the labeled statement at the end of loop body*/
      insert_statement(s,s1,FALSE);
    }
  w  = make_whileloop(MakeCommaExpression(lexp),
		      s,entity_empty_label(),
		      before ? make_evaluation_before(): make_evaluation_after());
  smt = make_statement(entity_empty_label(), 
		       get_current_C_line_number(), 
		       STATEMENT_ORDERING_UNDEFINED, 
		       //pop_current_C_comment(),
		       string_undefined,
		       make_instruction_whileloop(w),
		       NIL, string_undefined);

  if (!statement_undefined_p(s2))
    {
      /* This loop has a break statement which has been transformed to goto 
	 Add the labeled statement after the loop */
      insert_statement(smt,s2,FALSE);
    }
  pips_assert("While loop is consistent",whileloop_consistent_p(w));
  ifdebug(5) 
    {
      printf("While loop statement: \n");
      print_statement(smt);
    }
  return smt;
}

statement MakeForloop(expression e1, expression e2, expression e3, statement s)
{								 
  forloop f;
  statement smt;
  int i = basic_int((basic) stack_head(LoopStack));
  string lab1 = strdup(concatenate("loop_end_",int_to_string(i),NULL));
  statement s1 = FindStatementFromLabel(MakeCLabel(lab1));
  string lab2 = strdup(concatenate("break_",int_to_string(i),NULL));
  statement s2 = FindStatementFromLabel(MakeCLabel(lab2));

 if (!statement_undefined_p(s1))
    {
      /* This loop has a continue statement which has been transformed to goto 
	 Add the labeled statement at the end of loop body*/
      insert_statement(s,s1,FALSE);
    }
  f = make_forloop(e1,e2,e3,s);
  smt = make_statement(entity_empty_label(), 
		       get_current_C_line_number(), 
		       STATEMENT_ORDERING_UNDEFINED, 
		       string_undefined,
		       make_instruction_forloop(f),
		       NIL, string_undefined);

  if (!statement_undefined_p(s2))
    {
      /* This loop has a break statement which has been transformed to goto 
	 Add the labeled statement after the loop */
      insert_statement(smt,s2,FALSE);
    }
  pips_assert("For loop is consistent",forloop_consistent_p(f));
  ifdebug(5) 
    {
      printf("For loop statement: \n");
      print_statement(smt);
    }
  return smt;
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
  string lab = strdup(concatenate("break_",int_to_string(i),NULL));
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

  /* For the time being, the switch comment is lost. It should already be included in the argument,s  */
  /* pop_current_C_comment(); */

  /* Make sure the default case is the last one in the test sequence */
  for(ct=tl;!ENDP(ct); POP(ct)) {
    statement s = STATEMENT(CAR(ct));

    if(instruction_goto_p(statement_instruction(s))) {
      ds = s;
    }
    else {
      ntl = gen_nconc(ntl, CONS(STATEMENT,s,NIL));
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
  string lab = strdup(concatenate("switch_",int_to_string(i),
				  "_case_",words_to_string(words_expression(e)),NULL));
  statement s = MakeLabeledStatement(lab,make_continue_statement(entity_empty_label()));
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
  string lab = strdup(concatenate("switch_",int_to_string(i),"_default",NULL));
  statement s = MakeLabeledStatement(lab,make_continue_statement(entity_empty_label()));
  sequence CurrentSwitchGoto = stack_head(SwitchGotoStack);
  /* If the default case is not last, it must be moved later in the
     sequence_statements(CurrentSwitchGoto) */
  sequence_statements(CurrentSwitchGoto) = gen_nconc(sequence_statements(CurrentSwitchGoto),
							       CONS(STATEMENT,MakeGotoStatement(lab),NULL));
  return s;
}

statement MakeBreakStatement(string cmt)
{
  /* NN : I did not add a boolean variable to distinguish between loop and switch statements :-(*/
  int i = basic_int((basic) stack_head(LoopStack));
  string lab = strdup(concatenate("break_",int_to_string(i),NULL));
  statement bs = MakeGotoStatement(lab);

  statement_comments(bs) = cmt;

  return bs;
}

statement MakeContinueStatement(string cmt)
{
  /* Unique label with the LoopStack */
  int i = basic_int((basic) stack_head(LoopStack));
  string lab = strdup(concatenate("loop_end_",int_to_string(i),NULL));
  statement cs = MakeGotoStatement(lab);

  statement_comments(cs) = cmt;

  return cs;
}

statement ExpressionToStatement(expression e)
{
  syntax s = expression_syntax(e);
  statement st = statement_undefined;
  string c = get_current_C_comment();

  if (syntax_call_p(s))
    st = call_to_statement(syntax_call(s));
  else
    st = instruction_to_statement(make_instruction(is_instruction_expression,e));

  statement_number(st) = get_current_C_line_number();
  if(!string_undefined_p(c)) {
    statement_comments(st) = c;
  }

  return st;
}






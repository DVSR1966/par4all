 /* 
    Function for statement, and its subtypes:
     - instruction

    Lei ZHOU         12 Sep. 1991
    Francois IRIGOIN
  */

#include <stdio.h>
#include <string.h>
#include <varargs.h>

#include "genC.h"
#include "misc.h"

#include "text.h"
#include "text-util.h"
#include "ri.h"

#include "ri-util.h"

entity CreateIntrinsic(string name); /* in syntax.h */

/* PREDICATES ON STATEMENTS */

bool empty_statement_p(st)
statement st;
{
    instruction i;

    return(entity_empty_label_p(statement_label(st)) &&
	    instruction_block_p(i=statement_instruction(st)) &&
	    ENDP(instruction_block(i)));
}

bool assignment_statement_p(s)
statement s;
{
    instruction i = statement_instruction(s);

    return (fortran_instruction_p(i, ASSIGN_OPERATOR_NAME));
}

bool return_statement_p(s)
statement s;
{
    instruction i = statement_instruction(s);

    return (fortran_instruction_p(i, RETURN_FUNCTION_NAME));
}

bool continue_statement_p(s)
statement s;
{
    instruction i = statement_instruction(s);

    return (fortran_instruction_p(i, CONTINUE_FUNCTION_NAME));
}

bool stop_statement_p(s)
statement s;
{
    instruction i = statement_instruction(s);

    return (fortran_instruction_p(i, STOP_FUNCTION_NAME));
}

bool statement_less_p(st1, st2)
statement st1, st2;
{
    int o1 = statement_ordering( st1 ) ;
    int o2 = statement_ordering( st2 ) ;

    if (ORDERING_NUMBER( o1 ) != ORDERING_NUMBER( o2 )) {
	fprintf(stderr, "cannot compare %d (%d,%d) and %d (%d,%d)\n",
		statement_number(st1), 
		ORDERING_NUMBER( o1 ), ORDERING_STATEMENT( o1 ),
		statement_number(st2), 
		ORDERING_NUMBER( o2 ), ORDERING_STATEMENT( o2 ));

	abort();
    }

    return( ORDERING_STATEMENT(o1) < ORDERING_STATEMENT(o2)) ;
}

bool statement_possible_less_p(st1, st2)
statement st1, st2;
{
    int o1 = statement_ordering( st1 ) ;
    int o2 = statement_ordering( st2 ) ;

    if (ORDERING_NUMBER( o1 ) != ORDERING_NUMBER( o2 )) {
	return(TRUE);
    }
    else
	return( ORDERING_STATEMENT(o1) < ORDERING_STATEMENT(o2));
}

bool statement_loop_p(s)
statement s;
{
    return(instruction_loop_p(statement_instruction(s)));
}

bool statement_test_p(statement s)
{
    return(instruction_test_p(statement_instruction(s)));
}

bool statement_continue_p(s)
statement s;
{
    return(instruction_continue_p(statement_instruction(s)));
}

bool unlabelled_statement_p(st)
statement st;
{
    return(entity_empty_label_p(statement_label(st)));
}

/* Return true if the statement is an empty instruction block without
   label or a continue without label or a recursive combination of
   above. */
bool
empty_statement_or_labelless_continue_p(statement st)
{
   instruction i;

   if (!entity_empty_label_p(statement_label(st)))
      return FALSE;
   if (continue_statement_p(st))
      return TRUE;
   i = statement_instruction(st);
   if (instruction_block_p(i)) {
      bool useless = TRUE;
      MAPL(sts,
           {
              statement st = STATEMENT(CAR(sts)) ;
              if (!empty_statement_p(st))
                 if (!empty_statement_or_labelless_continue_p(st)) {
                    /* Well there is at least one possibly usefull thing... */
                    useless = FALSE;
                    break;
                 }
           },
              instruction_block(i));
      if (useless)
         return TRUE;
   }
   return FALSE;
}


/* Return true if the statement is an empty instruction block or a
   continue or a recursive combination of above. */
bool
empty_statement_or_continue_p(statement st)
{
   instruction i;

   if (continue_statement_p(st))
      return TRUE;
   i = statement_instruction(st);
   if (instruction_block_p(i)) {
      bool useless = TRUE;
      MAPL(sts,
           {
              statement st = STATEMENT(CAR(sts)) ;
              if (!empty_statement_p(st))
                 if (!empty_statement_or_continue_p(st)) {
                    /* Well there is at least one possibly usefull thing... */
                    useless = FALSE;
                    break;
                 }
           },
              instruction_block(i));
      if (useless)
         return TRUE;
   }
   return FALSE;
}


bool statement_call_p(s)
statement s;
{
    return(instruction_call_p(statement_instruction(s)));
}

bool perfectly_nested_loop_p(stat)
statement stat;
{
    instruction ins = statement_instruction(stat);
    tag t = instruction_tag(ins);

    switch( t ) {
    case is_instruction_block: {
	list lb = instruction_block(ins);

	if ( lb != NIL && (lb->cdr) != NIL && (lb->cdr)->cdr == NIL 
	    && ( statement_continue_p(STATEMENT(CAR(lb->cdr))) ) ) {
	    if ( assignment_statement_p(STATEMENT(CAR(lb))) )
		return TRUE;
	    else
		return(perfectly_nested_loop_p(STATEMENT(CAR(lb))));
	}
	else if ( lb != NIL && (lb->cdr) == NIL )
	    return(perfectly_nested_loop_p(STATEMENT(CAR(lb))));
	else if ( lb != NIL ) {
	    /* biased for WP65 */
	    return assignment_block_p(ins);
	}
	else
	    /* extreme case: empty loop nest */
	    return TRUE;
	break;
    }
    case is_instruction_loop: {
	loop lo = instruction_loop(ins);
	statement sbody = loop_body(lo);
	    
	if ( assignment_statement_p(sbody) ) 
	    return TRUE;
	else
	    return(perfectly_nested_loop_p(sbody));
	break;
    }
    default:
	break;
    }

    return FALSE;
}

/* checks that a block is a list of assignments, possibly followed by
   a continue */
bool assignment_block_p(i)
instruction i;
{
    MAPL(cs,
     {
	 statement s = STATEMENT(CAR(cs));

	 if(!assignment_statement_p(s))
	     if(!(continue_statement_p(s) && ENDP(CDR(cs)) ))
		 return FALSE;
     },
	 instruction_block(i));
    return TRUE;
}

/* functions to generate statements */

statement make_empty_statement()
{
    return(make_statement(entity_empty_label(), 
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED, 
			  string_undefined,
			  make_instruction(is_instruction_block, NIL)));
}

statement make_stmt_of_instr(instr)
instruction instr;
{
    return(make_statement(entity_empty_label(), 
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED, 
			  string_undefined,
			  instr));
}

statement make_assign_statement(l, r)
expression l, r;
{
    call c = make_call(entity_intrinsic(ASSIGN_OPERATOR_NAME),
		       CONS(EXPRESSION, l, CONS(EXPRESSION, r, NIL)));

    pips_assert("make_assign_statement",
		syntax_reference_p(expression_syntax(l)));

    return(make_statement(entity_empty_label(), 
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED,
			  string_undefined,
			  make_instruction(is_instruction_call, c)));
}

/* FI: make_block_statement_with_stop is obsolete, do not use */

statement make_block_statement_with_stop()
{
    statement b;
    statement stop;
    entity stop_function;

    stop_function = gen_find_tabulated(concatenate(TOP_LEVEL_MODULE_NAME, 
						   MODULE_SEP_STRING,
						   "STOP",
						   NULL),
				       entity_domain);

    pips_assert("make_block_statement_with_stop", 
		stop_function != entity_undefined);

    stop = make_statement(entity_empty_label(),
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED,
			  string_undefined,
			  make_instruction(is_instruction_call,
					   make_call(stop_function,NIL)));

    b = make_statement(entity_empty_label(),
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED,
			  string_undefined,
			  make_instruction(is_instruction_block,
					   CONS(STATEMENT, stop, NIL)));

    ifdebug(8) {
	fputs("make_block_statement_with_stop",stderr);
	print_text(stderr, text_statement(entity_undefined,0,b));
    }

    return b;
}

statement make_empty_block_statement()
{
    statement b;

    b = make_block_statement(NIL);

    return b;
}

statement make_block_statement(body)
list body;
{
    statement b;

    b = make_statement(entity_empty_label(),
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED,
			  string_undefined,
			  make_instruction(is_instruction_block, body));

    return b;
}


instruction make_instruction_block(list statements)
{
    return make_instruction(is_instruction_block, statements);
}

statement make_return_statement()
{
    return make_call_statement(RETURN_FUNCTION_NAME, NIL, 
			       entity_undefined, string_undefined);
}


instruction make_continue_instruction()
{
    entity called_function;

    called_function = entity_intrinsic(CONTINUE_FUNCTION_NAME);
    return make_instruction(is_instruction_call,
			    make_call(called_function,NIL));
}


statement make_continue_statement(l)
entity l;
{
    return make_call_statement(CONTINUE_FUNCTION_NAME, NIL, l, 
			       string_undefined);
}



instruction MakeUnaryCallInst(f,e)
entity f;
expression e;
{
    return(make_instruction(is_instruction_call,
			    make_call(f, CONS(EXPRESSION, e, NIL))));
}

/* this function creates a call to a function with zero arguments.  */

expression MakeNullaryCall(f)
entity f;
{
    return(make_expression(make_syntax(is_syntax_call, make_call(f, NIL)),
			   normalized_undefined));
}


/* this function creates a call to a function with one argument. */

expression MakeUnaryCall(f, a)
entity f;
expression a;
{
    call c =  make_call(f, CONS(EXPRESSION, a, NIL));

    return(make_expression(make_syntax(is_syntax_call, c),
			   normalized_undefined));
}


statement make_call_statement(function_name, args, l, c)
string function_name;
list args;
entity l; /* label, default entity_undefined */
string c; /* comments, default string_undefined */
{
    entity called_function;
    statement cs;

    called_function = entity_intrinsic(function_name);

    l = (l==entity_undefined)? entity_empty_label() : l;
    cs = make_statement(l,
			  STATEMENT_NUMBER_UNDEFINED,
			  STATEMENT_ORDERING_UNDEFINED,
			  c,
			  make_instruction(is_instruction_call,
					   make_call(called_function,NIL)));

    ifdebug(8) {
	fputs("[make_call_statement] ", stderr);
	print_text(stderr, text_statement(entity_undefined, 0, cs));
    }

    return cs;
}

/* */

statement perfectly_nested_loop_to_body(loop_nest)
statement loop_nest;
{
    instruction ins = statement_instruction(loop_nest);

    switch(instruction_tag(ins)) {
    case is_instruction_block: {
	list lb = instruction_block(ins);
	statement first_s = STATEMENT(CAR(lb));
	instruction first_i = statement_instruction(first_s);

	if(instruction_call_p(first_i))
	    return loop_nest;
	else {
	    if(instruction_block_p(first_i)) 
		return perfectly_nested_loop_to_body(STATEMENT(CAR(instruction_block(first_i))));
	    else {
		pips_assert("perfectly_nested_loop_to_body",
			    instruction_loop_p(first_i));
		return perfectly_nested_loop_to_body( first_s);
	    }
	}
	break;
    } 
    case is_instruction_loop: {
	statement sbody = loop_body(instruction_loop(ins));
	return (perfectly_nested_loop_to_body(sbody));
	break;
    }
    default:
	pips_error("perfectly_nested_loop_to_body","illegal tag\n");
	break;
    }
    return(statement_undefined); /* just to avoid a warning */
}

/* Direct accesses to second level fields */

loop statement_loop(s)
statement s;
{
    pips_assert("statement_loop", statement_loop_p(s));

    return(instruction_loop(statement_instruction(s)));
}

call statement_call(s)
statement s;
{
    pips_assert("statement_call", statement_call_p(s));

    return(instruction_call(statement_instruction(s)));
}

/* predicates on instructions */

bool instruction_assign_p(i)
instruction i;
{
    return fortran_instruction_p(i, ASSIGN_OPERATOR_NAME);
}

bool instruction_continue_p(i)
instruction i;
{
    return fortran_instruction_p(i, CONTINUE_FUNCTION_NAME);
}

bool fortran_instruction_p(i, s)
instruction i;
string s;
{
    if (instruction_call_p(i)) {
	call c = instruction_call(i);
	entity f = call_function(c);

	if (strcmp(entity_local_name(f), s) == 0)
	    return(TRUE);
    }

    return(FALSE);
}

/*
  returns the numerical value of loop l increment expression.
  aborts if this expression is not an integral constant expression.
  modification : returns the zero value when it isn't constant
  Y.Q. 19/05/92
*/

int loop_increment_value(l)
loop l;
{
    range r = loop_range(l);
    expression ic = range_increment(r);
    normalized ni;
    int inc;

    ni = NORMALIZE_EXPRESSION(ic);

    if (! EvalNormalized(ni, &inc)){
	/*user_error("loop_increment_value", "increment is not constant");*/
	debug(8,"loop_increment_value", "increment is not constant");
	return(0);
    }
    return(inc);
}

bool assignment_block_or_statement_p(s)
statement s;
{
    instruction i = statement_instruction(s);

    switch(instruction_tag(i)) {
    case is_instruction_block:
	return assignment_block_p(statement_instruction(s));
	break;
    case is_instruction_test:
	break;
    case is_instruction_loop:
	break;
    case is_instruction_goto:
	pips_error("assignment_block_p", "unexpected GO TO\n");
    case is_instruction_call:
	return assignment_statement_p(s);
    case is_instruction_unstructured:
	break;
    default: pips_error("assignment_block_or_statement_p",
			"ill. instruction tag %d\n", instruction_tag(i));
    }
    return FALSE;
}

void print_statement_set(fd, r)
FILE *fd;
set r;
{
    fprintf(fd, "Set contains statements");

    SET_MAP(s, {
	fprintf(fd, " %02d", statement_number((statement) s));
    }, r);

    fprintf(fd, "\n");
}

void print_statement(s)
statement s;
{
    print_text(stderr, text_statement(entity_undefined,0,s));
}

static hash_table number_to_statement = hash_table_undefined;

static void update_number_to_statement(s)
statement s;
{
    if(statement_number(s)!=STATEMENT_NUMBER_UNDEFINED) {
	hash_put(number_to_statement, (char *) statement_number(s), (char *) s);
    }
}

hash_table build_number_to_statement(nts, s)
hash_table nts;
statement s;
{
    pips_assert("build_number_to_statement", nts!=hash_table_undefined
		&& !statement_undefined_p(s));

    number_to_statement = nts;

    gen_recurse(s, statement_domain, gen_true, update_number_to_statement);

    /* nts is updated by side effect on number_to_statement */
    number_to_statement = hash_table_undefined;

    return nts;
}

void print_number_to_statement(nts)
hash_table nts;
{
    HASH_MAP(number, stmt, {
	fprintf(stderr,"%d\t", (int) number);
	print_statement((statement) stmt);
    }, nts);
}

hash_table allocate_number_to_statement()
{
    hash_table nts = hash_table_undefined;

    /* let's assume that 50 statements is a good approximation of a module
     * size 
     */
    nts = hash_table_make(hash_int, 50);

    return nts;
}

/* get rid of all labels in controlized code before duplication: all
 * labels have become useless and they cannot be freely duplicated
 */
statement clear_labels(s)
statement s;
{
    gen_recurse (s, statement_domain, gen_true, clear_label);
    return s;
}

void clear_label(s)
statement s;
{
    statement_label(s) = entity_empty_label();

    if(instruction_loop_p(statement_instruction(s))) 
	loop_label(instruction_loop(statement_instruction(s))) = 
	    entity_empty_label();
}

/*
 *   moved from HPFC by FC, 15 May 94
 *
 */

statement list_to_statement(l)
list l;
{
    switch (gen_length(l))
    {
    case 0:
	return(statement_undefined);
    case 1:
    {
	statement stat=STATEMENT(CAR(l));
	gen_free_list(l);
	return(stat);
    }
    default:
	return(make_block_statement(l));
    }

    return(statement_undefined);
}

statement st_make_nice_test(condition, ltrue, lfalse)
expression condition;
list ltrue,lfalse;
{
    statement
	stattrue = list_to_statement(ltrue),
	statfalse = list_to_statement(lfalse);
    bool
	notrue=(stattrue==statement_undefined),
	nofalse=(statfalse==statement_undefined);
    
    if ((notrue) && (nofalse)) 
	return(make_continue_statement(entity_undefined)); 

    if (nofalse) 
    {
	return
	    (make_stmt_of_instr
	     (make_instruction
	      (is_instruction_test,
	       make_test(condition,
			 stattrue,
			 make_continue_statement(entity_undefined)))));
    }

    if (notrue) 
    {
	expression
	    newcond = MakeUnaryCall(CreateIntrinsic(NOT_OPERATOR_NAME),
				    condition);

	return
	    (make_stmt_of_instr
	     (make_instruction
	      (is_instruction_test,
	       make_test(newcond,
			 statfalse,
			 make_continue_statement(entity_undefined)))));
    }

    return(make_stmt_of_instr(make_instruction(is_instruction_test,
					       make_test(condition,
							 stattrue,
							 statfalse))));
}


/* statement makeloopbody(l) 
 * make statement of a  loop body
 *
 * FI: the name of this function is not very good; the function should be put
 * in ri-util/statement.c, if it is really useful. The include list is
 * a joke!
 *
 * move here from generation... FC 16/05/94
 */
statement makeloopbody(l,s_old)
loop l;
statement s_old;
{
    statement state_l;
    instruction instr_l;
    statement l_body;

    instr_l = make_instruction(is_instruction_loop,l);
    state_l = make_statement(statement_label(s_old),
			     statement_number(s_old),
			     statement_ordering(s_old),
			     statement_comments(s_old),
			     instr_l);
    l_body = make_statement(entity_empty_label(), -1,  -1,
			    string_undefined,
			    make_instruction(is_instruction_block,
					     CONS(STATEMENT,state_l,NIL)));

    return(l_body);
}


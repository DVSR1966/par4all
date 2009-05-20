/* 
 * $Id$
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"

#include "linear.h"
#include "ri.h"

#include "ri-util.h"
#include "misc.h"
#include "control.h" /* for CONTROL_MAP() */
#include "transformations.h"

/* void atomize_as_required(stat, ref_decide, call_decide, test_decide, range_decide, while_decide, new)
 * statement stat;
 * bool (*ref_decide)(ref r, expression e);
 * bool (*call_decide)(call r, expression e);
 * bool (*test_decide)(test t, expression e);
 * bool (*range_decide)(range r, expression e), 
 * bool (*while_decide)(whileloop w, expression e), 
 * entity (*new)(entity m, basic b);
 *
 * atomizes the given statement as driven by the given functions.
 * ??? does not care of any side effect and so.
 * 
 * - ref_decide tells whether or not to atomize for reference r
 *   and expression indice e of this reference. If yes, expression e
 *   is computed outside.
 * - call_decide tells whether or not to atomize for call c
 *   and argument expression e...
 * - test_decide tells whether expression condition e of a test
 *   should be atomized or not.
 * - new creates a new entity in the current module, the basic type
 *   of which should be t.
 */

static void atomize_object(gen_chunk*);

/* static functions used
 */
static bool (*ref_atomize_decision)(/* ref r, expression e */) = NULL;
static bool (*call_atomize_decision)(/* call c, expression e */)= NULL;
static bool (*test_atomize_decision)(/* expression e */) = NULL;
static bool (*range_atomize_decision)(/* range r, expression e */)= NULL;
static bool (*while_atomize_decision)(/* whileloop w, expression e */)= NULL;

static entity (*create_new_variable)(/* entity m, tag t */) = NULL;


/* the stack of the encoutered statements is maintained
 * to be able to insert the needed computations just before the
 * very last statement encountered.
 *
 * The use of a stack is not necessary, since there are no effective rewrite.
 * but it could have been... It enables some recursive calls that would have
 * required to save the current statement explicitely otherwise.
 */
DEFINE_LOCAL_STACK(current_statement, statement)
DEFINE_LOCAL_STACK(current_control, control)

void simple_atomize_error_handler()
{
    error_reset_current_statement_stack();
    error_reset_current_control_stack();
}

/* s is inserted before the current statement. 
 * if it is a block, it is added just before the last statement of the block,
 * else a block statement is created in place of the current statement.
 */
static void insert_before_current_statement(s)
statement s;
{
    statement   cs = current_statement_head();
    instruction  i = statement_instruction(cs);
    control     cc = current_control_empty_p() ? 
	control_undefined : current_control_head() ;

    if (!control_undefined_p(cc) && control_statement(cc)==cs)
    {
      /* it is in an unstructured, and s is to be inserted properly
       */
	bool seen;
	control newc;

	pips_debug(8, "unstructured\n");

	newc = make_control(s, control_predecessors(cc), 
			    CONS(CONTROL, cc, NIL));
	control_predecessors(cc) = CONS(CONTROL, newc, NIL);

	/*   update the other lists
	 */
	MAPL(c1,
	 {
	     seen=FALSE;
	     
	     MAPL(c2,
	      {
		  if (CONTROL(CAR(c2))==cc) 
		  {
		      CONTROL_(CAR(c2))=newc;
		      seen=TRUE;
		  }
	      },
		  control_successors(CONTROL(CAR(c1))));

	     assert(seen);
	 },
	     control_predecessors(newc));

	/*   new current statement, to avoid other control insertions
	 */
	current_statement_replace(s);
	gen_recurse_stop(newc); 
    }
    else
    {
	pips_debug(7, "statement\n");
	
	if (instruction_block_p(i)) {
	  list /* of statement */ block = instruction_block(i);
	  /* insert statement before the last one */
	  block = gen_insert_before(s, 
				    STATEMENT(CAR(gen_last(block))), 
				    block);
	}
	else {
	  statement_instruction(cs) =
	    make_instruction_block(CONS(STATEMENT, s,
				   CONS(STATEMENT,
					make_statement(statement_label(cs), 
						       statement_number(cs),
						       statement_ordering(cs),
						       statement_comments(cs),
						       statement_instruction(cs),
						       NIL, NULL,
						       extensions_undefined),
					NIL)));
	  statement_label(cs) = entity_empty_label();
	  statement_number(cs) = STATEMENT_NUMBER_UNDEFINED;
	  statement_ordering(cs) = STATEMENT_ORDERING_UNDEFINED;
	  statement_comments(cs) = empty_comments;
	}
    }
}

/* returns the assignment statement is moved, or NULL if not.
 */
statement atomize_this_expression(entity (*create)(entity, basic),
				  expression e)
{
  basic bofe;

  /* it does not make sense to atomize a range...
   */
  if (syntax_range_p(expression_syntax(e))) return NULL;
  
  bofe = please_give_me_a_basic_for_an_expression(e);
  if(!basic_undefined_p(bofe)) {
    if (!basic_overloaded_p(bofe))
      {
	entity newvar; 
	expression rhs;
	statement assign;
	syntax ref;

	newvar = (*create)(get_current_module_entity(), bofe);
	rhs = make_expression(expression_syntax(e), normalized_undefined);
	normalize_all_expressions_of(rhs);

	ref = make_syntax(is_syntax_reference, make_reference(newvar, NIL));

	assign = make_assign_statement(make_expression(copy_syntax(ref), 
						       normalized_undefined), 
				       rhs);
	expression_syntax(e) = ref;

	/*
	// TEST
	if (syntax_reference_p(ref))
	{
	  fprintf(stderr,"Ref: %s is reference!\n", entity_local_name(newvar));
	}
	else if (syntax_call_p(ref))
	{
	  fprintf(stderr,"Ref: %s is call '%s'!\n", entity_local_name(newvar),
		  entity_local_name(call_function(syntax_call(ref))));
	}
	print_expression(e);
	print_statement(assign);
	*/

	return assign;
      }
  
    free_basic(bofe);
  }
  return NULL;
}

static void compute_before_current_statement(expression *pe)
{
    statement stat = atomize_this_expression(create_new_variable,*pe);
    if (stat) insert_before_current_statement(stat);
}

static void ref_rwt(r)
reference r;
{
    MAPL(ce, 
     {
	 expression *pe = (expression*) REFCAR(ce);
	 
	 if ((*ref_atomize_decision)(r, *pe))
	 {
	   /* syntax saved = expression_syntax(*pe);*/
	   compute_before_current_statement(pe);
	   /* atomize_object(saved); */
	 }
     },
	 reference_indices(r));

    /* return(FALSE);*/
}

static void call_rwt(call c)
{
  if (same_string_p(entity_local_name(call_function(c)), IMPLIED_DO_NAME))
    return; /* (panic mode:-) */
  
  MAPL(ce, 
       {
	 expression *pe = (expression*) REFCAR(ce);

	 if ((*call_atomize_decision)(c, *pe))
	   {
	     /* syntax saved = expression_syntax(*pe);*/
	     
	     compute_before_current_statement(pe);
	     /* atomize_object(saved); */
	   }
       },
	 call_arguments(c));
}


static void exp_range_rwt (range r, expression * pe)			     
{
  
  if ((*range_atomize_decision)(r, *pe))
    {
      /* syntax saved = expression_syntax(*pe); */
	     
      compute_before_current_statement(pe);
      /* atomize_object(saved);*/
    }
}

static void range_rwt(range r)
{
  /* lower */
  exp_range_rwt(r, &range_lower(r));

  /* upper */
  exp_range_rwt(r, &range_upper(r));

  /* increment */
  exp_range_rwt(r, &range_increment(r));

  /* return(TRUE);*/
}


static void test_rwt(test t)
{
    expression *pe = &test_condition(t);

    if ((*test_atomize_decision)(t, *pe)) 
      {
	/* syntax saved = expression_syntax(*pe);*/

	/*   else I have to break the condition
	 *   and to complete the recursion.
	 */
	compute_before_current_statement(pe);
	/* atomize_object(saved); */
    }

    /* return(TRUE);*/
}

static void whileloop_rwt(whileloop w)
{
  expression *pe = &whileloop_condition(w);

  if ((*while_atomize_decision)(w, *pe)) 
      {
	/* syntax saved = expression_syntax(*pe); */

	/*   else I have to break the condition
	 *   and to complete the recursion.
	 */
	compute_before_current_statement(pe);
	/* atomize_object(saved);*/
    }

  /* return(TRUE);*/
}

static void atomize_object(obj)
gen_chunk *obj;
{
    gen_multi_recurse
	(obj,
	 control_domain, current_control_filter, 
	                 current_control_rewrite,     /* CONTROL */
	 statement_domain, current_statement_filter, 
                         current_statement_rewrite,   /* STATEMENT */
	 reference_domain, gen_true, ref_rwt,      /* REFERENCE */
	 test_domain, gen_true, test_rwt,          /* TEST */
         call_domain, gen_true, call_rwt,          /* CALL */
	 range_domain, gen_true, range_rwt,        /* RANGE */
	 whileloop_domain, gen_true, whileloop_rwt,/* WHILELOOP */
	 NULL);
}

void atomize_as_required(
  statement stat, 
  bool (*ref_decide)(reference, expression), /* reference */
  bool (*call_decide)(call, expression), /* call */
  bool (*test_decide)(test, expression), /* test */
  bool (*range_decide)(range, expression), /* range */
  bool (*while_decide)(whileloop, expression), /* whileloop */
  entity (*new)(entity, basic))
{
    ifdebug(5) {
      pips_debug(5,
		 "Statement at entry:\n");
      print_statement(stat);
    }
    make_current_statement_stack();
    make_current_control_stack();
    ref_atomize_decision = ref_decide;
    call_atomize_decision = call_decide;
    test_atomize_decision = test_decide;
    range_atomize_decision = range_decide;
    while_atomize_decision = while_decide;
    create_new_variable = new;
    
    atomize_object(stat);

    ref_atomize_decision = NULL;
    call_atomize_decision = NULL;
    test_atomize_decision = NULL;
    range_atomize_decision = NULL;
    while_atomize_decision = NULL;
    create_new_variable = NULL;
    free_current_statement_stack();
    free_current_control_stack();

    ifdebug(5) {
      pips_debug(5,
		 "Statement at exit:\n");
      print_statement(stat);
    }
}

/*  that is all
 */





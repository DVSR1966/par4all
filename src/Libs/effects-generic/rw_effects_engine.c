/*

  $Id$

  Copyright 1989-2009 MINES ParisTech

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
/* package generic effects :  Be'atrice Creusillet 5/97
 *
 * File: rw_effects_engine.c
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This File contains the generic functions necessary for the computation of 
 * all types of read and write effects and cumulated references.
 *
 * $Id$
 */
#include <stdio.h>
#include <string.h>

#include "genC.h"

#include "linear.h"
#include "ri.h"
#include "database.h"

#include "ri-util.h"
#include "control.h"
#include "constants.h"
#include "misc.h"
#include "text-util.h"
#include "text.h"
#include "makefile.h"

#include "properties.h"
#include "pipsmake.h"

#include "transformer.h"
#include "semantics.h"
#include "pipsdbm.h"
#include "resources.h"

#include "effects-generic.h"

/************************************************ TO CONTRACT PROPER EFFECTS */

static bool contract_p = TRUE;

void set_contracted_rw_effects(bool b)
{
    contract_p = b;
}


/*********************************************** INTERPROCEDURAL COMPUTATION */

bool summary_rw_effects_engine(string module_name)
{

    list l_glob = NIL, l_loc = NIL,l_loc2 = NIL, l_dec=NIL; 
    statement module_stat;

    set_current_module_entity(module_name_to_entity(module_name)); 
    set_current_module_statement( (statement)
	db_get_memory_resource(DBR_CODE, module_name, TRUE) );
    module_stat = get_current_module_statement();

    (*effects_computation_init_func)(module_name);

    debug_on("SUMMARY_EFFECTS_DEBUG_LEVEL");

    set_rw_effects((*db_get_rw_effects_func)(module_name));

    l_loc = load_rw_effects_list(module_stat);
    ifdebug(2){
	pips_debug(2, "local regions, before translation to global scope:\n");
	(*effects_prettyprint_func)(l_loc);
    }

    l_dec = summary_effects_from_declaration(module_name);
    ifdebug(8) {
      int nb_param;
      pips_debug(8, "Summary effects from declarations:\n");
      (*effects_prettyprint_func)(l_dec);
      nb_param = gen_length(functional_parameters(type_functional(ultimate_type(entity_type(get_current_module_entity())))));
      pips_debug(8, "number of declared formal parameters:%d\n", nb_param);
	
    }
    
    l_loc2 = gen_append(l_loc,l_dec);
    
    // MAP(EFFECT, e, fprintf(stderr, "=%s=", entity_name(reference_variable(effect_any_reference(e)))) ,l_loc2);
    l_glob = (*effects_local_to_global_translation_op)(l_loc2);


    ifdebug(4)
      {
	/* Check that summary effects are not corrupted */
	if(!check_sdfi_effects_p(get_current_module_entity(), l_glob))
	  pips_internal_error("SDFI effects for \"%s\" are corrupted \n",
			      entity_name(get_current_module_entity()));
      }

    /* Different effects may have been reduced to the same one */
    /* FI: I'm not to sure the parameter TRUE is generic */
    l_glob = proper_effects_combine(l_glob, TRUE);

    ifdebug(2){
	pips_debug(2, "local regions, after translation to global scope:\n");
	(*effects_prettyprint_func)(l_loc2);
	pips_debug(2, "global regions, after translation to global scope:\n");
	(*effects_prettyprint_func)(l_glob);
    }

    ifdebug(4)
      {
	/* Check that summary effects are not corrupted */
	if(!check_sdfi_effects_p(get_current_module_entity(), l_glob))
	  pips_internal_error("SDFI effects for \"%s\" are corrupted\n",
			      entity_name(get_current_module_entity()));
      }

    (*db_put_summary_rw_effects_func)(module_name, l_glob);

    reset_current_module_entity();
    reset_current_module_statement();
    reset_rw_effects();

    debug_off();
    (*effects_computation_reset_func)(module_name);
    
    return(TRUE);
}

/*********************************************** INTRAPROCEDURAL COMPUTATION */

static void rw_effects_of_unstructured(unstructured unst)
{
    statement current_stat = effects_private_current_stmt_head();
    list blocs = NIL ;
    list le = NIL ;
    control ct;

    pips_debug(2, "begin\n");

    ct = unstructured_control(unst);

    if(control_predecessors(ct) == NIL && control_successors(ct) == NIL)
    {
	/* there is only one statement in u; no need for a fixed point */
	pips_debug(3, "unique node\n");
	le = effects_dup(load_rw_effects_list(control_statement(ct)));
    }
    else
    {	
	transformer t_unst = (*load_transformer_func)(current_stat);
	list l_node;

	CONTROL_MAP(c, {
	    l_node = effects_dup(load_rw_effects_list(control_statement(c)));
	    le = (*effects_test_union_op) (l_node, le, effects_same_action_p) ;
	},
	    ct, blocs) ;	
	le = (*effects_transformer_composition_op)(le, t_unst);
	effects_to_may_effects(le);	
	gen_free_list(blocs) ;
    }    
    
    (*effects_descriptor_normalize_func)(le);

    ifdebug(2){
	pips_debug(2, "R/W effects: \n");
	(*effects_prettyprint_func)(le);
    }
    store_rw_effects_list(current_stat, le);

    pips_debug(2, "end\n");
}

/*
 * From BC's PhD:
 *
 *   R[while(C)S] = R[C] U ( R[S] o E[C] ) U ( R[while(C)S] o T[S] o E[C] )
 *
 * However we do not have the lpf available to solve the recursive equation...
 * Ok, let's try something else, with a few transformations:
 *
 *   R[while(C)S]  = Rc[while(C)S] u Rs[while(C)S] ;
 *
 *   Rc[while(C)S] = R[C] u R[C] o T[S] o E[C] u ...
 *                 = U_i=0^inf R[C] o (T[S] o E[C])^i
 *                 = R[C] O U_i=0^inf (T[S] o E[C])^i
 *                 = R[C] O T*[while(C)S] ;
 *
 *   Rs[while(C)S] = R[S] o E[C] u R[S] o E[C] o T[S] o E[C] u ...
 *                 = U_i=0^inf R[S] o E[C] o (T[S] o E[C])^i
 *                 = R[S] o E[C] O U_i=0^inf (T[S] o E[C])^i
 *                 = R[S] o E[C] O T*[while(C)S] ;
 *
 * Thus
 *
 *   R[while(C)S]  = (R[C] u R[S] o E[C]) O T*[while(C)S] ;
 *
 *
 * I assume that someone (FI) is to provide:
 *
 *   T*[while(C)S] = U_i=0^inf (T[S] o E[C])^i ;
 *
 * Note that T* can be computed as a fixpoint from the recursice equation:
 *
 *   T*[while(C)S] = T*[while(C)S] o T[S] o E[C] u Id
 *
 * That is the resolution of the fixpoint for R is expressed as a fixpoint
 * on transformers only, and a direct computation on R. 
 *
 * Also we know that the output state is the one which makes C false.
 *
 *   T[while(C)S] = E[.not.C] O T*[while(C)S] ;
 *
 * note that T[] Sigma -> Sigma, 
 * but T*[] Sigma -> P(Sigma) 
 * that is it describes exactly intermediate stores reached by the while.
 *
 * FC, 04/06/1998
 */
static void rw_effects_of_while(whileloop w)
{
    statement current_stat = effects_private_current_stmt_head();
    list l_prop, l_body;
    statement b = whileloop_body(w);
    transformer trans;
    
    l_prop = effects_dup(load_proper_rw_effects_list(current_stat)); /* R[C] */
    l_body = effects_dup(load_rw_effects_list(b)); /* R[S] */
    /* I use the famous over-approximation of E[C]: Id */
    trans = (*load_transformer_func)(current_stat); /* T*[while(C)S] */

    l_body = (*effects_union_op)(l_body, l_prop, effects_same_action_p);
    l_body = (*effects_transformer_composition_op)(l_body, trans);

    (*effects_descriptor_normalize_func)(l_body);

    store_rw_effects_list(current_stat, l_body);
}

/* Amira: To be refined later... */
static void rw_effects_of_forloop(forloop w)
{
    statement current_stat = effects_private_current_stmt_head();
    list l_prop, l_body;
    statement b = forloop_body(w);
    transformer trans;
    
    pips_user_warning("Ongoing implementation! Amira mensi\n");

    l_prop = effects_dup(load_proper_rw_effects_list(current_stat)); /* R[C] */
    l_body = effects_dup(load_rw_effects_list(b)); /* R[S] */
    /* I use the famous over-approximation of E[C]: Id */
    trans = (*load_transformer_func)(current_stat); /* T*[while(C)S] */

    l_body = (*effects_union_op)(l_body, l_prop, effects_same_action_p);
    l_body = (*effects_transformer_composition_op)(l_body, trans);

    (*effects_descriptor_normalize_func)(l_body);

    store_rw_effects_list(current_stat, l_body);
}

static void rw_effects_of_loop(loop l)
{
    statement current_stat = effects_private_current_stmt_head();
    list l_prop, l_body, l_loop = NIL;
    statement b = loop_body(l);
    entity i = loop_index(l);
    range r = loop_range(l);
    transformer loop_trans;

    pips_debug(2, "begin\n");

    /* proper effects of loop */
    l_prop = effects_dup(load_proper_rw_effects_list(current_stat));
    if (contract_p)
	l_prop = proper_to_summary_effects(l_prop);    
   
    /* rw effects of loop body */
    l_body = load_rw_effects_list(b);

    ifdebug(4){
	pips_debug(4, "rw effects of loop body:\n");
	(*effects_prettyprint_func)(l_body);
    }
    /* Loop body must not have a write effect on the loop index */
    MAP(EFFECT, ef, {
      if(effect_entity(ef)==i && action_write_p(effect_action(ef)))
	pips_user_error("Index %s of loop %s defined in loop body. "
			"Fortran 77 standard violation, see Section 11.10.5.\n",
			entity_local_name(i),
			label_local_name(loop_label(l)));
    }, l_body);

    /* effects on locals are unconditionnaly masked */
    l_body = effects_dup_without_variables(l_body, loop_locals(l));
    l_body = effects_dup_without_variables(l_body, statement_declarations(b));
    
    /* COMPUTATION OF INVARIANT RW EFFECTS */
        
    /* We get the loop transformer, which gives the loop invariant */
    /* We must remove the loop index from the list of modified variables */
    loop_trans = (*load_transformer_func)(current_stat);
    loop_trans = transformer_remove_variable_and_dup(loop_trans, i);
    
    /* And we compute the invariant RW effects. */
    l_body = (*effects_transformer_composition_op)(l_body, loop_trans);
    update_invariant_rw_effects_list(b, effects_dup(l_body));

    ifdebug(4){
	pips_debug(4, "invariant rw effects of loop body:\n");
	(*effects_prettyprint_func)(l_body);
    }    

    /* COMPUTATION OF RW EFFECTS OF LOOP FROM INVARIANT RW EFFECTS */
    if (!ENDP(l_body))
    {

	l_loop = l_body;
	/* We eliminate the loop index */
	l_loop = (*effects_union_over_range_op)(l_loop, i, r, 
						descriptor_undefined);	  
	
    }

    ifdebug(4){
	pips_debug(4, "rw effects of loop before adding proper effects:\n");
	(*effects_prettyprint_func)(l_loop);
    }

    /* We finally add the loop proper effects */
    l_loop = (*effects_union_op)(l_loop, l_prop, effects_same_action_p);

    (*effects_descriptor_normalize_func)(l_loop);

    ifdebug(2){
	pips_debug(2, "R/W effects: \n");
	(*effects_prettyprint_func)(l_loop);
    }
    store_rw_effects_list(current_stat, l_loop);
    pips_debug(2, "end\n");

}

static void rw_effects_of_call(call c)
{
    statement current_stat = effects_private_current_stmt_head();
    transformer context = (*load_context_func)(current_stat);
    list le = NIL;

    pips_debug(2, "begin call to %s\n", entity_name(call_function(c)));

    if (!(*empty_context_test)(context))
    {
      list sel = load_proper_rw_effects_list(current_stat);
	le = effects_dup(sel);
	ifdebug(2){
	    pips_debug(2, "proper effects before summarization: \n");
	    (*effects_prettyprint_func)(le);
	}
	if (contract_p)
	    le = proper_to_summary_effects(le);
    }
    else
	pips_debug(2, "empty context\n");

    (*effects_descriptor_normalize_func)(le);

    ifdebug(2){
	pips_debug(2, "R/W effects: \n");
	(*effects_prettyprint_func)(le);
    }
    store_rw_effects_list(current_stat, le);

    pips_debug(2, "end\n");
}

/* For the time being, just a duplicate of rw_effects_of_call() */
static void rw_effects_of_application(application a __attribute__ ((__unused__)))
{
    statement current_stat = effects_private_current_stmt_head();
    transformer context = (*load_context_func)(current_stat);
    list le = NIL;

    pips_debug(2, "begin application\n");

    if (!(*empty_context_test)(context))
    {
      list sel = load_proper_rw_effects_list(current_stat);
	le = effects_dup(sel);
	ifdebug(2){
	    pips_debug(2, "proper effects before summarization: \n");
	    (*effects_prettyprint_func)(le);
	}
	if (contract_p)
	    le = proper_to_summary_effects(le);
    }
    else
	pips_debug(2, "empty context\n");

    (*effects_descriptor_normalize_func)(le);

    ifdebug(2){
	pips_debug(2, "R/W effects: \n");
	(*effects_prettyprint_func)(le);
    }
    store_rw_effects_list(current_stat, le);

    pips_debug(2, "end\n");
}

/* Just to handle one kind of instruction, expressions which are not
   calls.  As we do not distinguish between Fortran and C, this
   function is called for Fortran module although it does not have any
   effect.
 */
static void rw_effects_of_expression_instruction(instruction i)
{
  //list l_proper = NIL;
  statement current_stat = effects_private_current_stmt_head();
  //instruction inst = statement_instruction(current_stat);

  /* Is the call an instruction, or a sub-expression? */
  if (instruction_expression_p(i)) {
    expression ie = instruction_expression(i);
    syntax is = expression_syntax(ie);
    call c = call_undefined;

    if(syntax_cast_p(is)) {
      expression ce = cast_expression(syntax_cast(is));
      syntax sc = expression_syntax(ce);

      if(syntax_call_p(sc)) {
	c = syntax_call(sc);
	rw_effects_of_call(c);
      }
      else {
	pips_internal_error("Cast case not implemented\n");
      }
    }
    else if(syntax_call_p(is)) {
      /* This may happen when a loop is desugared into an unstructured. */
      c = syntax_call(is);
      rw_effects_of_call(c);
    }
    else if(syntax_application_p(is)) {
      application a = syntax_application(is);
      //expression fe = application_function(a);

      pips_user_warning("Cumulated effects of call site using function "
			"pointers in data structures are ignored for the time being\n");
      rw_effects_of_application(a);
    }
    else {
      pips_internal_error("Instruction expression case not implemented\n");
    }

    pips_debug(2, "Effects for expression instruction in statement%03zd\n",
	       statement_ordering(current_stat)); 

  }
}

static void rw_effects_of_test(test t)
{
  statement current_stat = effects_private_current_stmt_head();
  list le, lt, lf, lc, lr;
  statement true_s = test_true(t);
  statement false_s = test_false(t);
  extern effect reference_to_convex_region(reference, action);

  pips_debug(2, "begin\n");

  /* FI: when regions are computed the test condition should be
     evaluated wrt the current precondition to see if it evaluates
     to true or false. This would preserve must effects. 

     dead_test_filter() could be used, but it returns an enum
     defined in transformations-local.h */

  if(reference_to_effect_func == reference_to_convex_region
     && !statement_strongly_feasible_p(true_s)) {
    /* the true branch is dead */
    le = effects_dup(load_rw_effects_list(false_s));
  }
  else if(reference_to_effect_func == reference_to_convex_region
	  && !statement_strongly_feasible_p(false_s)) {
    /* the false branch is dead */
    le = effects_dup(load_rw_effects_list(true_s));
  }
  else {
    /* effects of the true branch */
    lt = effects_dup(load_rw_effects_list(test_true(t)));
    /* effects of the false branch */
    lf = effects_dup(load_rw_effects_list(test_false(t)));
    /* effects of the combination of both */
    le = (*effects_test_union_op)(lt, lf, effects_same_action_p);
  }

  /* proper_effects of the condition */
  lc = effects_dup(load_proper_rw_effects_list(current_stat));
  if (contract_p)
    lc = proper_to_summary_effects(lc);
  /* effect of the test */
  lr = (*effects_union_op)(le, lc, effects_same_action_p);

  (*effects_descriptor_normalize_func)(lr);

  ifdebug(2){
    pips_debug(2, "R/W effects: \n");
    (*effects_prettyprint_func)(lr);
  }
    
  store_rw_effects_list(current_stat, lr);
  pips_debug(2, "end\n");
}

static list r_rw_effects_of_sequence(list l_inst)
{
    statement first_statement;
    list remaining_block = NIL;
    
    list s1_lrw; /* rw effects of first statement */
    list rb_lrw; /* rw effects of remaining block */
    list l_rw = NIL; /* resulting rw effects */
    transformer t1; /* transformer of first statement */
 
    first_statement = STATEMENT(CAR(l_inst));
    remaining_block = CDR(l_inst);
	    
    s1_lrw = effects_dup(load_rw_effects_list(first_statement));
	
    /* Is it the last instruction of the block */
    if (!ENDP(remaining_block))
    {	
	t1 = (*load_transformer_func)(first_statement);    
	rb_lrw = r_rw_effects_of_sequence(remaining_block);

	ifdebug(3){
	    pips_debug(3, "R/W effects of first statement: \n");
	    (*effects_prettyprint_func)(s1_lrw);
	    pips_debug(3, "R/W effects of remaining sequence: \n");
	    (*effects_prettyprint_func)(rb_lrw);
	    /* if (!transformer_undefined_p(t1))
	    {
		pips_debug(3, "transformer of first statement: %s\n",
			   transformer_to_string(t1));		
	    }*/
	}
    	if (rb_lrw !=NIL)    
	rb_lrw = (*effects_transformer_composition_op)(rb_lrw, t1); 
	else {
	  ifdebug(3){
	    pips_debug(3, "warning - no effect on  remaining block\n");
	   
	  }
	}
	ifdebug(5){
	    pips_debug(5, "R/W effects of remaining sequence "
		       "after composition: \n");
	    (*effects_prettyprint_func)(rb_lrw);
	}

	/* RW(block) = RW(rest_of_block) U RW(S1) */
	l_rw = (*effects_union_op)(rb_lrw, s1_lrw, effects_same_action_p);
    }	
    else 
    {
	l_rw = s1_lrw;
    }
    

    return(l_rw);
}

static void rw_effects_of_sequence(sequence seq)
{
    statement current_stat = effects_private_current_stmt_head();
    list le = NIL;
    list l_inst = sequence_statements(seq);

    pips_debug(2, "begin\n");

    if (ENDP(l_inst))
    {
	if (get_bool_property("WARN_ABOUT_EMPTY_SEQUENCES"))
	    pips_user_warning("empty sequence\n");
    }
    else
    {
	le = r_rw_effects_of_sequence(l_inst);
    }

    ifdebug(2){
	pips_debug(2, "R/W effects: \n");
	(*effects_prettyprint_func)(le);
    }

    (*effects_descriptor_normalize_func)(le);
    
    store_rw_effects_list(current_stat, le);
    pips_debug(2, "end\n");
}

static bool rw_effects_stmt_filter(statement s)
{
    pips_debug(1, "Entering statement %03zd :\n", statement_ordering(s));
    ifdebug(4) {
      
      print_statement(s);
    }
    effects_private_current_stmt_push(s);
    return(TRUE);
}

static void rw_effects_of_statement(statement s)
{
    store_invariant_rw_effects_list(s, NIL);
    effects_private_current_stmt_pop();
    pips_debug(1, "End statement %03zd :\n", statement_ordering(s));
}


void rw_effects_of_module_statement(statement module_stat)
{

    make_effects_private_current_stmt_stack();
    pips_debug(1,"begin\n");
    
    gen_multi_recurse
	(module_stat, 
	 statement_domain, rw_effects_stmt_filter, rw_effects_of_statement,
	 sequence_domain, gen_true, rw_effects_of_sequence,
	 test_domain, gen_true, rw_effects_of_test,
	 call_domain, gen_true, rw_effects_of_call,
	 loop_domain, gen_true, rw_effects_of_loop,
	 whileloop_domain, gen_true, rw_effects_of_while,
	 forloop_domain, gen_true, rw_effects_of_forloop,
	 unstructured_domain, gen_true, rw_effects_of_unstructured,
	 instruction_domain, gen_true, rw_effects_of_expression_instruction,
	 expression_domain, gen_false, gen_null, /* NOT THESE CALLS */
	 NULL); 

    pips_debug(1,"end\n");
    free_effects_private_current_stmt_stack();
}

bool rw_effects_engine(char * module_name)
{
    /* Get the code of the module. */
    set_current_module_statement( (statement)
		      db_get_memory_resource(DBR_CODE, module_name, TRUE));

    set_current_module_entity(module_name_to_entity(module_name));

    (*effects_computation_init_func)(module_name);

    /* We also need the proper effects of the module */
    set_proper_rw_effects((*db_get_proper_rw_effects_func)(module_name));

    /* Compute the rw effects or references of the module. */
    init_rw_effects();
    init_invariant_rw_effects();
  
    debug_on("EFFECTS_DEBUG_LEVEL");
    pips_debug(1, "begin\n");

    rw_effects_of_module_statement(get_current_module_statement()); 

    pips_debug(1, "end\n");
    debug_off();

    (*db_put_rw_effects_func) 
	(module_name, get_rw_effects());
    (*db_put_invariant_rw_effects_func)
	(module_name, get_invariant_rw_effects());

    reset_current_module_entity();
    reset_current_module_statement();
    reset_proper_rw_effects();
    reset_rw_effects();
    reset_invariant_rw_effects();

    (*effects_computation_reset_func)(module_name);
    
    return(TRUE);
}

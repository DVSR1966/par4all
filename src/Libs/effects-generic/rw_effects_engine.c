/* package generic effects :  Be'atrice Creusillet 5/97
 *
 * File: rw_effects_engine.c
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This File contains the generic functions necessary for the computation of 
 * all types of read and write effects and cumulated references.
 *
 */
#include <stdio.h>
#include <string.h>

#include "genC.h"

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

/*********************************************************************************/
/* TO CONTRACT PROPER EFFECTS                                                    */
/*********************************************************************************/

static contract_p = TRUE;

void
set_contracted_rw_effects(bool b)
{
    contract_p = b;
}


/*********************************************************************************/
/* INTERPROCEDURAL COMPUTATION                                                   */
/*********************************************************************************/

bool 
summary_rw_effects_engine(string module_name)
{

    list l_glob = NIL, l_loc = NIL; 
    statement module_stat;

    set_current_module_entity(local_name_to_top_level_entity(module_name)); 
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

    l_glob = (*effects_local_to_global_translation_op)(l_loc);
    

    ifdebug(2){
	pips_debug(2, "local regions, after translation to global scope:\n");
	(*effects_prettyprint_func)(l_loc);
	pips_debug(2, "global regions, after translation to global scope:\n");
	(*effects_prettyprint_func)(l_glob);
    }

    
    (*db_put_summary_rw_effects_func)(module_name, l_glob);

    reset_current_module_entity();
    reset_current_module_statement();
    reset_rw_effects();

    debug_off();
    (*effects_computation_reset_func)(module_name);
    
    return(TRUE);
}

/*********************************************************************************/
/* INTRAPROCEDURAL COMPUTATION                                                   */
/*********************************************************************************/

static void
rw_effects_of_unstructured(unstructured unst)
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

static void 
rw_effects_of_loop(loop l)
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
    l_body = load_rw_effects_list(b);;
    ifdebug(4){
	pips_debug(4, "rw effects of loop body:\n");
	(*effects_prettyprint_func)(l_body);
    }   

    /* effects on locals are unconditionnaly masked */
    l_body = effects_dup_without_variables(l_body, loop_locals(l));
    
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

static void
rw_effects_of_call(call c)
{
    statement current_stat = effects_private_current_stmt_head();
    transformer context = (*load_context_func)(current_stat);
    list le = NIL;

    if (transformer_undefined_p(context))
    {
	/* this happens to the CONTINUE statement of the exit node
	 * even if unreachable. Thus transformer are not computed,
	 * orderings are not set... however gen_multi_recurse goes there.
	 * I just store NIL, what seems reasonnable an answer.
	 * It seems to be sufficient for other passes. 
	 * I should check that it is indeed the exit node?
	 * FC.
	 */
	pips_debug(2, "call with undefined context... exit node?\n");
	store_rw_effects_list(current_stat, NIL);
	return;
    }

    pips_debug(2, "begin\n");

    if (!(*empty_context_test)(context))
    {
	le = effects_dup(load_proper_rw_effects_list(current_stat));
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

static void 
rw_effects_of_test(test t)
{
    statement current_stat = effects_private_current_stmt_head();
    list le, lt, lf, lc, lr;

    pips_debug(2, "begin\n");

      /* effects of the true branch */
    lt = effects_dup(load_rw_effects_list(test_true(t)));
      /* effects of the false branch */
    lf = effects_dup(load_rw_effects_list(test_false(t)));
      /* effects of the combination of both */
    le = (*effects_test_union_op)(lt, lf, effects_same_action_p);

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

static list
r_rw_effects_of_sequence(list l_inst)
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

	ifdebug(5){
	    pips_debug(5, "R/W effects of first statement: \n");
	    (*effects_prettyprint_func)(s1_lrw);
	    pips_debug(5, "R/W effects of remaining sequence: \n");
	    (*effects_prettyprint_func)(rb_lrw);
	    if (!transformer_undefined_p(t1))
	    {
		pips_debug(5, "transformer of first statement: %s\n",
			   transformer_to_string(t1));		
	    }
	}
    	    
	rb_lrw = (*effects_transformer_composition_op)(rb_lrw, t1); 
	    
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

static void
rw_effects_of_sequence(sequence seq)
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

static bool 
rw_effects_stmt_filter(statement s)
{
    pips_debug(1, "Entering statement %03d :\n", statement_ordering(s));
    effects_private_current_stmt_push(s);
    return(TRUE);
}

static void
rw_effects_of_statement(statement s)
{
    store_invariant_rw_effects_list(s, NIL);
    effects_private_current_stmt_pop();
    pips_debug(1, "End statement %03d :\n", statement_ordering(s));
}


void
rw_effects_of_module_statement(statement module_stat)
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
	 unstructured_domain, gen_true, rw_effects_of_unstructured,
	 expression_domain, gen_false, gen_null, /* NOT THESE CALLS */
	 NULL); 

    pips_debug(1,"end\n");
    free_effects_private_current_stmt_stack();
}

bool
rw_effects_engine(char *module_name)
{
    /* Get the code of the module. */
    set_current_module_statement( (statement)
		      db_get_memory_resource(DBR_CODE, module_name, TRUE) );
    
    set_current_module_entity( local_name_to_top_level_entity(module_name) );

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



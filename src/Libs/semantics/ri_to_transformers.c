 /* semantical analysis
  *
  * phasis 1: compute transformers from statements and statements effects
  *
  * For (simple) interprocedural analysis, this phasis should be performed
  * bottom-up on the call tree.
  *
  * Francois Irigoin, April 1990
  *
  */

#include <stdio.h>
#include <string.h>
/* #include <stdlib.h> */

#include "genC.h"
#include "database.h"
#include "ri.h"
#include "text.h"
#include "text-util.h"
#include "ri-util.h"
#include "constants.h"
#include "control.h"
#include "effects.h"

#include "misc.h"

#include "properties.h"

#include "vecteur.h"
#include "contrainte.h"
#include "ray_dte.h"
#include "sommet.h"
#include "sg.h"
#include "polyedre.h"

#include "transformer.h"

#include "semantics.h"

extern Psysteme sc_projection_by_eq(Psysteme sc, Pcontrainte eq, Variable v);



/* Recursive Descent in Data Structure Statement */


/* SHARING : returns the transformer stored in the database. Make a copy 
 * before using it. The copy is not made here because the result is not always used
 * after a call to this function, and itwould create non reachable structures. Another
 * solution would be to store a copy and free the unused result in the calling function
 * but transformer_free does not really free the transformer. Not very clean. 
 * BC, oct. 94 */
transformer statement_to_transformer(s)
statement s;
{
    instruction i = statement_instruction(s);
    cons * e = load_statement_cumulated_effects(s);
    transformer t;

    debug(8,"statement_to_transformer","begin for statement %03d (%d,%d)\n",
	  statement_number(s), ORDERING_NUMBER(statement_ordering(s)), 
	  ORDERING_STATEMENT(statement_ordering(s)));

    t = load_statement_transformer(s);

    /* it would be nicer to control warning_on_redefinition */
    if (t == transformer_undefined) {
	t = instruction_to_transformer(i, e);
	if(!transformer_consistency_p(t)) {
	    int so = statement_ordering(s);
	    (void) fprintf(stderr, "statement %03d (%d,%d):\n",
			   statement_number(s),
			   ORDERING_NUMBER(so), ORDERING_STATEMENT(so));
	    (void) print_transformer(load_statement_transformer(s));
	    pips_error("statement_to_transformer",
		       "Inconsistent transformer detected\n");
	}
	store_statement_transformer(s, t);
    }
    else 
	pips_error("statement_to_transformer","transformer redefinition");

    ifdebug(1) {
	int so = statement_ordering(s);
	(void) fprintf(stderr, "statement %03d (%d,%d):\n", statement_number(s),
		       ORDERING_NUMBER(so), ORDERING_STATEMENT(so));
	(void) print_transformer(load_statement_transformer(s));
    }

    debug(8,"statement_to_transformer","end with t=%x\n",
	  (unsigned int) t);

    return(t);
}

transformer instruction_to_transformer(i, e)
instruction i;
cons * e; /* effects associated to instruction i */
{
    transformer tf = transformer_undefined;
    test t;
    loop l;
    call c;

    debug(8,"instruction_to_transformer","begin\n");

    switch(instruction_tag(i)) {
      case is_instruction_block:
	tf = block_to_transformer(instruction_block(i));
	break;
      case is_instruction_test:
	t = instruction_test(i);
	tf = test_to_transformer(t, e);
	break;
      case is_instruction_loop:
	l = instruction_loop(i);
	tf = loop_to_transformer(l, e);
	break;
      case is_instruction_goto:
	pips_error("instruction_to_transformer",
		   "unexpected goto in semantics analysis");
	tf = transformer_identity();
	break;
      case is_instruction_call:
	c = instruction_call(i);
	tf = call_to_transformer(c, e);
	break;
      case is_instruction_unstructured:
	tf = unstructured_to_transformer(instruction_unstructured(i), e);
	  break ;
      default:
	pips_error("instruction_to_transformer","unexpected tag %d\n",
	      instruction_tag(i));
    }
    debug(9,"instruction_to_transformer","resultat:\n");
    ifdebug(9) (void) print_transformer(tf);
    debug(8,"instruction_to_transformer","end\n");
    return tf;
}

transformer block_to_transformer(b)
cons * b;
{
    statement s;
    transformer tf;

    debug(8,"block_to_transformer","begin\n");

    if(ENDP(b))
	tf = transformer_identity();
    else {
	s = STATEMENT(CAR(b));
	tf = transformer_dup(statement_to_transformer(s));
	for (POP(b) ; !ENDP(b); POP(b)) {
	    s = STATEMENT(CAR(b));
	    tf = transformer_combine(tf, statement_to_transformer(s));
	    ifdebug(1) {
		(void) transformer_consistency_p(tf);
	    }
	}
    }

    debug(8,"block_to_transformer","end\n");
    return tf;
}

transformer unstructured_to_transformer(u, e)
unstructured u;
cons * e; /* effects */
{
    transformer tf;
    control c;

    debug(8,"unstructured_to_transformer","begin\n");

    pips_assert("unstructured_to_transformer", u!=unstructured_undefined);

    c = unstructured_control(u);
    if(control_predecessors(c) == NIL && control_successors(c) == NIL) {
	/* there is only one statement in u; no need for a fix-point */
	debug(8,"unstructured_to_transformer","unique node\n");
	tf = statement_to_transformer(control_statement(c));
    }
    else {
	/* Do not try anything clever! God knows what may happen in
	   unstructured code. Transformer tf is not computed recursively
	   from its components but directly derived from effects e.
	   Transformers associated to its components are then computed
	   independently, hence the name unstructured_to_transformerS
	   instead of unstructured_to_transformer */
	debug(8,"unstructured_to_transformer","complex: based on effects\n");
	(void) unstructured_to_transformers(u) ;
	tf = effects_to_transformer(e);
    }

    debug(8,"unstructured_to_transformer","end\n");

    return tf;
}

cons * effects_to_arguments(fx)
cons * fx; /* list of effects */
{
    /* algorithm: keep only write effects on integer scalar variable */
    cons * args = NIL;

    MAPL(cef, { effect ef = EFFECT(CAR(cef));
		reference r = effect_reference(ef);
		action a = effect_action(ef);
		entity e = reference_variable(r);

		if(action_write_p(a) && entity_integer_scalar_p(e)) {
		    args = arguments_add_entity(args, e);}},
 	 fx);

    return args;
}

transformer effects_to_transformer(e)
cons * e; /* list of effects */
{
    /* algorithm: keep only write effects on integer scalar variable */
    transformer tf = transformer_identity();
    cons * args = transformer_arguments(tf);
    Pbase b = VECTEUR_NUL;
    Psysteme s = sc_new();

    MAPL(cef, { effect ef = EFFECT(CAR(cef));
		reference r = effect_reference(ef);
		action a = effect_action(ef);
		entity v = reference_variable(r);

		if(action_write_p(a) && entity_has_values_p(v)) {
		    entity new_val = entity_to_new_value(v);
		    args = arguments_add_entity(args, new_val);
		    b = vect_add_variable(b, (Variable) new_val);}},
 	 e);

    transformer_arguments(tf) = args;
    s->base = b;
    s->dimension = vect_size(b);
    /* NewGen should generate proper cast for external types */
    /* predicate_system(transformer_relation(tf)) = (Psysteme) s; */
    predicate_system(transformer_relation(tf)) = (char *) s;
    return tf;
}

void unstructured_to_transformers(u)
unstructured u ;
{
    cons *blocs = NIL ;
    control ct = unstructured_control(u) ;

    debug(8,"unstructured_to_transformers","begin\n");

    CONTROL_MAP(c, {
	statement st = control_statement(c) ;
	(void) statement_to_transformer(st) ;
    }, ct, blocs) ;

    gen_free_list(blocs) ;

    debug(8,"unstructured_to_transformers","end\n");
}

/* The transformer associated to a DO loop does not include the exit 
 * condition because it is used to compute the precondition for any 
 * loop iteration.
 *
 * There is only one attachment for the unbounded transformer and
 * for the bounded one.
 */

transformer loop_to_transformer(l, e)
loop l;
cons * e; /* effects of loop l */
{
    /* loop transformer tf = tfb* or tf = tfb+ or ... */
    transformer tf;
    /* loop body transformer */
    transformer tfb;

    entity i = loop_index(l);
    range r = loop_range(l);
    expression incr = range_increment(r);
    Pvecteur v_incr = VECTEUR_UNDEFINED;
    statement s = loop_body(l);

    debug(8,"loop_to_transformer","begin\n");

    if(pips_flag_p(SEMANTICS_FIX_POINT)) {
	/* compute the loop body transformer */
	tfb = transformer_dup(statement_to_transformer(s));
	/* it does not contain the loop index update
	   the loop increment expression must be linear to find inductive 
	   variables related to the loop index */
	if(!VECTEUR_UNDEFINED_P(v_incr = expression_to_affine(incr))) {
	    if(entity_has_values_p(i))
		tfb = transformer_add_loop_index(tfb, i, v_incr);
	    else
		user_warning("loop_to_transformer", 
			     "non-integer loop index %s?\n",
			     entity_local_name(i));
	}

	/* compute tfb's fix point according to pips flags */
	if(pips_flag_p(SEMANTICS_INEQUALITY_INVARIANT)) {
	    tf = transformer_halbwachs_fix_point(tfb);
	}
	else {
	    /* FI: it might have been easier to pass tf as an argument to
	     * transformer_equality_fix_point() and to update it with
	     * new equations...
	     */
	    transformer ftf = transformer_equality_fix_point(tfb);
	    Psysteme fsc = (Psysteme) predicate_system(transformer_relation(ftf));
	    Psysteme sc = SC_UNDEFINED;
	    Pcontrainte eq = CONTRAINTE_UNDEFINED;
	    normalized nlb = NORMALIZE_EXPRESSION(range_lower(r));
	    Pbase new_b = BASE_UNDEFINED;
	    
	    tf = effects_to_transformer(e);
	    sc = (Psysteme) predicate_system(transformer_relation(tf));

	    /* compute the basis for tf and ftf */

	    /* FI: just in case. I do not understand why sc_base(fsc) is not enough.
	     * I do not understand why I used effects_to_transformer() instead
	     * of transformer_indentity()...
	     */
	    new_b = base_union(sc_base(fsc), sc_base(sc));
	    base_rm(sc_base(sc));
	    sc_base(sc) = new_b;
	    sc_dimension(sc) = base_dimension(new_b);

	    /* add equations from ftf to tf */
	    for(eq = sc_egalites(fsc); !CONTRAINTE_UNDEFINED_P(eq); ) {
		Pcontrainte neq;

		neq = eq->succ;
		sc_add_egalite(sc, eq);
		eq = neq;
	    }

	    /* FI" I hope that inequalities will be taken care of some day! */

	    sc_egalites(fsc) = CONTRAINTE_UNDEFINED;
	    free_transformer(ftf);

	    ifdebug(8) {
		debug(8, "loop_to_transformer", "intermediate fix-point tf=\n");
		fprint_transformer(stderr, tf, external_value_name);
	    }

	    /* add initialization for the loop index variable */
	    /* FI: this seems to be all wrong because a transformer cannot
	     * state anything about its initial state...
	     *
	     * Also, sc basis should be updated!
	     *
	     * I change my mind: let's use the lower bound anyway since it
	     * make sense as soon as i_init is eliminated in the transformer
	     */
	    if(entity_has_values_p(i) && normalized_linear_p(nlb)) {
		Pvecteur v_lb = vect_dup((Pvecteur) normalized_linear(nlb));
		entity i_init = entity_to_old_value(i);

		vect_add_elem(&v_lb, (Variable) i_init, -1);
		eq = contrainte_make(v_lb);
		/* sc_add_egalite(sc, eq); */
		sc = sc_projection_by_eq(sc, eq, (Variable) i_init);
		if(SC_RN_P(sc)) {
		    /* FI: a NULL is not acceptable; I assume that we cannot end up
		     * with a SC_EMPTY...
		     */
		    predicate_system(transformer_relation(tf)) =
			sc_make(CONTRAINTE_UNDEFINED, CONTRAINTE_UNDEFINED);
		}
	    }

	    ifdebug(8) {
		debug(8, "loop_to_transformer", "full fix-point tf=\n");
		fprint_transformer(stderr, tf, external_value_name);
		debug(8, "loop_to_transformer", "end\n");
	    }

	}
	/* we have a problem here: to compute preconditions within the
	   loop body we need a tf using private variables; to return
	   the loop transformer, we need a filtered out tf; only
	   one hook is available in the ri..; let'a assume there
	   are no private variables and that if they are privatizable
	   they are not going to get in our way */
    }
    else {
	/* basic cheap version: do not use the loop body transformer and
	   avoid fix-points; local variables do not have to be filtered out
	   because this was already done while computing effects */

	(void) statement_to_transformer(s);
	tf = effects_to_transformer(e);
    }

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer",
		       "resultat tf =");
	(void) (void) print_transformer(tf);
    }
    debug(8,"loop_to_transformer","end\n");
    return tf;
}

transformer test_to_transformer(t, ef)
test t;
cons * ef; /* effects of t */
{
    statement st = test_true(t);
    statement sf = test_false(t);
    transformer tf;

    debug(8,"test_to_transformer","begin\n");

    /* the test condition cannot be used to improve transformers
       it may be used later when propagating preconditions 
       Francois Irigoin, 15 April 1990 

       Why?
       Francois Irigoin, 31 July 1992

       Well, you can benefit from STOP statements.
       But you do not know if the variable values in
       the condition are the new or the old values...
       Francois Irigoin, 8 November 1995

       */

    if(pips_flag_p(SEMANTICS_FLOW_SENSITIVE)) {
	/*
	tft = statement_to_transformer(st);
	tff = statement_to_transformer(sf);
	tf = transformer_convex_hull(tft, tff);
	*/
	expression e = test_condition(t);
	transformer tftwc;
	transformer tffwc;

	tftwc = transformer_dup(statement_to_transformer(st));
	tffwc = transformer_dup(statement_to_transformer(sf));

	tftwc = transformer_add_condition_information(tftwc,e,TRUE);
	tffwc = transformer_add_condition_information(tffwc,e,FALSE);

	tf = transformer_convex_hull(tftwc, tffwc);
	transformer_free(tftwc);
	transformer_free(tffwc);
    }
    else {
	(void) statement_to_transformer(st);
	(void) statement_to_transformer(sf);
	tf = effects_to_transformer(ef);
    }

    debug(8,"test_to_transformer","end\n");
    return tf;
}

transformer call_to_transformer(c, ef)
call c;
cons * ef; /* effects of call c */
{
    transformer tf = transformer_undefined;
    entity e = call_function(c);
    cons *pc = call_arguments(c);
    tag tt;

    debug(8,"call_to_transformer","begin\n");

    switch (tt = value_tag(entity_initial(e))) {
      case is_value_code:
	/* call to an external function; preliminary version:
	   rely on effects */
	debug(5, "call_to_transformer", "external function %s\n",
	      entity_name(e));
	if(get_bool_property(SEMANTICS_INTERPROCEDURAL))
	    tf = user_call_to_transformer(e, pc, ef);
	else
	    tf = effects_to_transformer(ef);
	break;
      case is_value_symbolic:
      case is_value_constant:
	tf = transformer_identity();
	break;
      case is_value_unknown:
	pips_error("call_to_transformer", "unknown function %s\n",
		   entity_name(e));
	break;
      case is_value_intrinsic:
	debug(5, "call_to_transformer", "intrinsic function %s\n",
	      entity_name(e));
	tf = intrinsic_to_transformer(e, pc, ef);
	break;
      default:
	pips_error("call_to_transformer", "unknown tag %d\n", tt);
    }

    debug(8,"call_to_transformer","end\n");

    return(tf);
}

transformer intrinsic_to_transformer(e, pc, ef)
entity e;
cons * pc;
cons * ef; /* effects of intrinsic call */
{
    transformer tf;

    debug(8,"intrinsic_to_transformer","begin\n");

    if(ENTITY_ASSIGN_P(e))
	tf = assign_to_transformer(pc, ef);
    else if(ENTITY_STOP_P(e))
	tf = transformer_empty();
    else
	tf = effects_to_transformer(ef);

    debug(8,"intrinsic_to_transformer","end\n");

    return tf;
}

transformer assign_to_transformer(args, ef)
cons *args; /* arguments for assign */
cons * ef; /* effects of assign */
{
    /* algorithm: if lhs and rhs are linear expressions on scalar integer
       variables, build the corresponding equation; else, use effects ef
       
       should be extended to cope with constant integer division as in
       N2 = N/2
       because it is used in real program; inequalities should be
       generated in that case 2*N2 <= N <= 2*N2+1
       
       same remark for MOD operator
       
       implementation: part of this function should be moved into
       transformer.c
       */

    expression lhs = EXPRESSION(CAR(args));
    expression rhs = EXPRESSION(CAR(CDR(args)));
    transformer tf = transformer_undefined;
    normalized n = NORMALIZE_EXPRESSION(lhs);

    debug(8,"assign_to_transformer","begin\n");

    pips_assert("assign_to_transformer", CDR(CDR(args))==NIL);

    if(normalized_linear_p(n)) {
	Pvecteur vlhs =
	    (Pvecteur) normalized_linear(n);
	entity e = (entity) vecteur_var(vlhs);

	if(entity_has_values_p(e) && integer_scalar_entity_p(e)) {
	    /* FI: the initial version was conservative because
	     * only affine scalar integer assignments were processed
	     * precisely. But non-affine operators and calls to user defined
	     * functions can also bring some information as soon as
	     * *some* integer read or write effect exists
	     */
	    /* check that *all* read effects are on integer scalar entities */
	    /*
	    if(integer_scalar_read_effects_p(ef)) {
		tf = expression_to_transformer(e, rhs, ef);
	    }
	    */
	    /* Check that *some* read or write effects are on integer 
	     * scalar entities. This is almost always true... Let's hope
	     * expression_to_transformer() returns quickly for array
	     * expressions used to initialize a scalar integer entity.
	     */
	    if(some_integer_scalar_read_or_write_effects_p(ef)) {
		tf = expression_to_transformer(e, rhs, ef);
	    }
	}
    }
    /* if any condition was not met and transformer derivation failed */
    if(tf==transformer_undefined)
	tf = effects_to_transformer(ef);

    debug(9,"assign_to_transformer","return tf=%x\n",tf);
    ifdebug(9) (void) print_transformer(tf);
    debug(8,"assign_to_transformer","end\n");
    return tf;
}

/* transformer expression_to_transformer(entity e, expression expr, list ef):
 * returns a transformer abstracting the effect of assignment e = expr
 * if entity e and entities referenced in expr are accepted for
 * semantics analysis anf if expr is affine; else returns
 * transformer_undefined
 *
 * Note: it might be better to distinguish further between e and expr
 * and to return a transformer stating that e is modified when e
 * is accepted for semantics analysis.
 *
 * Bugs:
 *  - core dumps if entities referenced in expr are not accepted for
 *    semantics analysis
 *
 * Modifications:
 *  - MOD and / added for very special cases (but not as general as it should be)
 *    FI, 25/05/93
 *  - MIN, MAX and use function call added (for simple cases)
 *    FI, 16/11/95
 */
transformer expression_to_transformer(e, expr, ef)
entity e;
expression expr;
list ef;
{
    transformer tf = transformer_undefined;

    debug(8, "expression_to_transformer", "begin\n");

    if(entity_has_values_p(e)) {
	Pvecteur ve = vect_new((Variable) e, 1);
	normalized n = NORMALIZE_EXPRESSION(expr);

	if(normalized_linear_p(n)) {
	    entity e_new = entity_to_new_value(e);
	    entity e_old = entity_to_old_value(e);
	    cons * tf_args = CONS(ENTITY, e_new, NIL);
	    /* must be duplicated right now  because it will be
	       renamed and checked at the same time by
	       value_mappings_compatible_vector_p() */
	    Pvecteur vexpr = vect_dup((Pvecteur) normalized_linear(n));
	    Pcontrainte c;
	    Pvecteur eq = VECTEUR_NUL;

	    ifdebug(9) {
		(void) fprintf(stderr,"Expression to linearize:\n");
		print_words(stderr,words_expression(expr));
		(void) fprintf(stderr,"\nLinearized expression:\n");
		vect_dump(vexpr);
	    }

	    if(value_mappings_compatible_vector_p(ve) &&
	       value_mappings_compatible_vector_p(vexpr)) {
		ve = vect_variable_rename(ve,
					  (Variable) e,
					  (Variable) e_new);
		(void) vect_variable_rename(vexpr,
					    (Variable) e_new,
					    (Variable) e_old);
		eq = vect_substract(ve, vexpr);
		vect_rm(ve);
		vect_rm(vexpr);
		c = contrainte_make(eq);
		tf = make_transformer(tf_args,
				      make_predicate(sc_make(c, CONTRAINTE_UNDEFINED)));
	    }
	    else {
		vect_rm(eq);
		vect_rm(ve);
		vect_rm(vexpr);
		tf = transformer_undefined;
	    }
	}
	else if(modulo_expression_p(expr)) {
	    tf = modulo_to_transformer(e, expr);
	}
	else if(divide_expression_p(expr)) {
	    tf = integer_divide_to_transformer(e, expr);
	}
	else if(min0_expression_p(expr)) {
	    tf = min0_to_transformer(e, expr);
	}
	else if(max0_expression_p(expr)) {
	    tf = max0_to_transformer(e, expr);
	}
	else if(user_function_call_p(expr) 
		&& get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
	    /* FI: I need effects ef here to use user_call_to_transformer()
	     * although the general idea was to return an undefined transformer
	     * on failure rather than a transformer derived from effects
	     */
	    tf = user_function_call_to_transformer(e, expr, ef);
	}
	else {
	    vect_rm(ve);
	    tf = transformer_undefined;
	}
    }

    debug(8, "expression_to_transformer",
	  "end with tf=%x\n", (unsigned int) tf);

    return tf;
}

transformer user_function_call_to_transformer(entity e, expression expr,
					      list ef)
{
    syntax s = expression_syntax(expr);
    call c = syntax_call(s);
    entity f = call_function(c);
    list pc = call_arguments(c);
    transformer t_caller = transformer_undefined;
    basic rbt = basic_of_call(c);

    debug(8, "user_function_call_to_transformer", "begin\n");

    pips_assert("user_function_call_to_transformer", syntax_call_p(s));

    if(basic_int_p(rbt)) {
	string fn = module_local_name(f);
	entity rv = global_name_to_entity(fn, fn);
	entity orv = entity_undefined;
	entity e_new = entity_to_new_value(e);
	cons * tf_args = CONS(ENTITY, e_new, NIL);
	Psysteme sc = SC_UNDEFINED;
	Pcontrainte c = CONTRAINTE_UNDEFINED;
	Pvecteur eq = VECTEUR_NUL;
	transformer t_assign = transformer_undefined;

	pips_assert("user_function_call_to_transformer",
		    !entity_undefined_p(rv));

	/* Build a transformer reflecting the call site */
	t_caller = user_call_to_transformer(f, pc, ef);

	ifdebug(8) {
	    debug(8, "user_function_call_to_transformer", 
		  "Transformer 0x%x for callee %s:\n",
		  t_caller, entity_local_name(f));
	    dump_transformer(t_caller);
	}

	/* Build a transformer representing the assignment of
	 * the function value to e
	 */
	eq = vect_make(eq, (Variable) e, 1, (Variable) rv, -1, 0, 0);
	c = contrainte_make(eq);
	sc = sc_make(c, CONTRAINTE_UNDEFINED);
	/* FI: I do not understand why this is useful since the basis
	 * does not have to have old values that do not appear in
	 * predicates... See transformer_consistency_p()
	 *
	 * But it proved useful for the call to foo3 in funcside.f
	 */
	sc_base_add_variable(sc, (Variable) entity_to_old_value(e));
	t_assign = make_transformer(tf_args,
				    make_predicate(sc));

	/* Consistency cannot be checked on a non-local transformer */
	/* pips_assert("user_function_call_to_transformer",
	   transformer_consistency_p(t_assign)); */

	ifdebug(8) {
	    debug(8, "user_function_call_to_transformer", 
		  "Transformer 0x%x for assignment of %s with %s:\n",
		  (unsigned int) t_assign, entity_local_name(e), entity_name (rv));
	    dump_transformer(t_assign);
	}

	/* Combine the effect of the function call and of the assignment */
	t_caller = transformer_combine(t_caller, t_assign);
	free_transformer(t_assign);

	/* Get rid of the temporary representing the function's value */
	orv = global_new_value_to_global_old_value(rv);
	t_caller = transformer_filter(t_caller,
				      CONS(ENTITY, rv, CONS(ENTITY, orv, NIL)));


	ifdebug(8) {
	    debug(8, "user_function_call_to_transformer", 
		  "Final transformer 0x%x for assignment of %s with %s:\n",
		  (unsigned int) t_caller, entity_local_name(e), entity_name (rv));
	    dump_transformer(t_caller);
	}

	/* FI: e is added in arguments because user_call_to_transformer()
	 * uses effects to make sure arrays and non scalar integer variables
	 * impact is taken into account
	 */
	/*
	transformer_arguments(t_caller) =
	    arguments_rm_entity(transformer_arguments(t_caller), e);
	    */

	/* FI, FI: il vaudrait mieux ne pas eliminer e d'abord1 */
	/* J'ai aussi des free a decommenter */
	/*
	if(ENDP(transformer_arguments(t_caller))) {
	    transformer_arguments(t_caller) = 
		gen_nconc(transformer_arguments(t_caller), CONS(ENTITY, e, NIL));
	}
	else {
	    t_caller = transformer_value_substitute(t_caller, rv, e);
	}
	*/
    }

    debug(8, "user_function_call_to_transformer",
	  "end with t_caller=%x\n", (unsigned int) t_caller);

    return t_caller;
}

transformer transformer_intra_to_inter(tf, le)
transformer tf;
cons * le;
{
    cons * lost_args = NIL;
    /* Filtered TransFormer ftf */
    transformer ftf = transformer_dup(tf);
    cons * old_args = transformer_arguments(ftf);
    Psysteme sc = SC_UNDEFINED;
    Pbase b = BASE_UNDEFINED;
    Pbase eb = BASE_UNDEFINED;

    debug(8,"transformer_intra_to_inter","begin\n");
    debug(8,"transformer_intra_to_inter","argument tf=%x\n",ftf);
    ifdebug(8) (void) dump_transformer(ftf);

    /* get rid of tf's arguments that do not appear in effects le */

    /* build a list of arguments to suppress */
    /* FI: I do not understand anymore why corresponding old values do not have
     * to be suppressed too (6 July 1993)
     *
     * FI: because only read arguments are eliminated, non? (12 November 1995)
     */
    MAPL(ca, 
     {entity e = ENTITY(CAR(ca));
      if(!effects_write_entity_p(le, e) &&
	       !storage_return_p(entity_storage(e))) 
	  lost_args = arguments_add_entity(lost_args, e);
     },
    old_args);

    /* get rid of them */
    ftf = transformer_projection(ftf, lost_args);

    /* free the temporary list of entities */
    gen_free_list(lost_args);
    lost_args = NIL;

    debug(8,"transformer_intra_to_inter","after first filtering ftf=%x\n",ftf);
    ifdebug(8) (void) dump_transformer(ftf);

    /* get rid of local read variables */

    /* FI: why not use this loop to get rid of *all* local variables, read or written? */

    sc = (Psysteme) predicate_system(transformer_relation(ftf));
    b = sc_base(sc);
    for(eb=b; !BASE_UNDEFINED_P(eb); eb = eb->succ) {
	entity e = (entity) vecteur_var(eb);

	if(e != (entity) TCST) {
	    entity v = value_to_variable(e);

	    /* Variables with no impact on the caller world are eliminated.
	     * However, the return value associated to a function is preserved.
	     */
	    if( ! effects_read_or_write_entity_p(le, v) &&
	       !storage_return_p(entity_storage(v))) {
		lost_args = arguments_add_entity(lost_args, e);
	    }
	}
    }

    /* get rid of them */
    ftf = transformer_projection(ftf, lost_args);

    /* free the temporary list of entities */
    gen_free_list(lost_args);
    lost_args = NIL;

    debug(8,"transformer_intra_to_inter","return ftf=%x\n",ftf);
    ifdebug(8) (void) dump_transformer(ftf);
    debug(8,"transformer_intra_to_inter","end\n");

    return ftf;
}

transformer user_call_to_transformer(f, pc, ef)
entity f;
list pc;
list ef;
{
    transformer t_callee = transformer_undefined;
    transformer t_caller = transformer_undefined;
    transformer t_effects = transformer_undefined;
    entity caller = entity_undefined;
    list all_args = list_undefined;

    debug(8, "user_call_to_transformer", "begin\n");

    pips_assert("user_call_to_transformer", entity_module_p(f));

    if(!get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
	/*
	user_warning("user_call_to_transformer",
		     "unknown interprocedural transformer for %s\n",
		     entity_local_name(f));
		     */
	t_caller = effects_to_transformer(ef);
    }
    else {
	/* add equations linking formal parameters to argument expressions
	   to transformer t_callee and project along the formal parameters */
	/* for performance, it  would be better to avoid building formals
	   and to inline entity_to_formal_parameters */
	/* it wouls also be useful to distinguish between in and out
	   parameters; I'm not sure the information is really available
	   in a field ??? */
	list formals = entity_to_formal_integer_parameters(f);
	list formals_new = NIL;
	cons * ce;

	t_callee = load_summary_transformer(f);

	ifdebug(8) {
	    Psysteme s = 
		(Psysteme) predicate_system(transformer_relation(t_callee));
	    debug(8, "user_call_to_transformer", 
		  "Transformer for callee %s:\n", entity_local_name(f));
	    dump_transformer(t_callee);
	    sc_fprint(stderr, s, dump_value_name);
	}

	t_caller = transformer_dup(t_callee);

	/* take care of formal parameters */
	/* let's start a long, long, long MAPL, so long that MAPL is a pain */
	for( ce = formals; !ENDP(ce); POP(ce)) {
	    entity e = ENTITY(CAR(ce));
	    int r = formal_offset(storage_formal(entity_storage(e)));
	    expression expr;
	    normalized n;

	    if((expr = find_ith_argument(pc, r)) == expression_undefined)
		user_error("user_call_to_transformer",
			   "not enough args for %d formal parm. %s in call to %s from %s\n",
			   r, entity_local_name(e), entity_local_name(f),
			   get_current_module_entity());

	    n = NORMALIZE_EXPRESSION(expr);
	    if(normalized_linear_p(n)) {
		Pvecteur v = vect_dup((Pvecteur) normalized_linear(n));
		if(value_mappings_compatible_vector_p(v)) {
		    entity e_new = external_entity_to_new_value(e);
		    entity a_new = entity_undefined;
		    entity a_old = entity_undefined;
		    
		    if(entity_is_argument_p(e_new, 
					    transformer_arguments(t_caller))) {
			/* e_new and e_old must be replaced by the
			   actual entity argument */
			entity e_old = external_entity_to_old_value(e);

			if(vect_size(v) != 1 || vecteur_var(v) == TCST)
			    /* should be a user error! */
			    user_error("user_call_to_transformer",
				       "value (!) modifed by call to %s\n",
				       entity_local_name(f));
			a_new = entity_to_new_value((entity)
						    vecteur_var(v));
			a_old = entity_to_old_value((entity)
						    vecteur_var(v));
			t_caller = transformer_value_substitute(t_caller,
								e_new,
								a_new);
			t_caller = transformer_value_substitute(t_caller,
								e_old,
								a_old);
		    }
		    else {
			/* simple case: formal parameter e is not
			   modified and can be replaced by actual
			   argument expression */
			vect_add_elem(&v, (Variable) e_new, -1);
			t_caller = transformer_equality_add(t_caller,
							    v);
		    }
		}
		else {
		    /* formal parameter e has to be eliminated:
		       e_new and e_old will be eliminated */
		    vect_rm(v);
		}
	    }
	}
	/* formal new values left over are eliminated */
	MAPL(ce,{entity e = ENTITY(CAR(ce));
		 entity e_new = external_entity_to_new_value(e);
		     
		 formals_new = CONS(ENTITY, e_new, formals_new);},
	     formals);
		 
	t_caller = transformer_filter(t_caller, formals_new);
		 
	free_arguments(formals_new);
	free_arguments(formals);
    }

    ifdebug(8) {
	Psysteme s = 
	    (Psysteme) predicate_system(transformer_relation(t_caller));
	debug(8, "user_call_to_transformer", 
	      "After binding formal/real parameters\n");
	dump_transformer(t_caller);
	sc_fprint(stderr, s, dump_value_name);
    }

    /* take care of global variables */
    caller = get_current_module_entity();
    translate_global_values(caller, t_caller);

    /* FI: are invisible variables taken care of by translate_global_values()? 
     * Yes, now...
     * A variable may be invisible because its location is reached
     * thru an array or thru a non-integer scalar variable in the
     * current module, for instance because a COMMON is defined
     * differently. A variable whose location is not reachable
     * in the current module environment is considered visible.
     */

    ifdebug(8) {
	debug(8, "user_call_to_transformer", 
	      "After replacing global variables\n");
	dump_transformer(t_caller);
    }

    /* Callee f may have read/write effects on caller's scalar
     * integer variables thru an array and/or non-integer variables.
     */
    t_effects = effects_to_transformer(ef);
    all_args = arguments_union(transformer_arguments(t_caller),
			       transformer_arguments(t_effects));
    /*
    free_transformer(t_effects);
    gen_free_list(transformer_arguments(t_caller));
    */
    transformer_arguments(t_caller) = all_args;
    

    ifdebug(8) {
	debug(8, "user_call_to_transformer", 
	      "End: after taking non-integer scalar effects %x\n",
	      (unsigned int) t_caller);
	dump_transformer(t_caller);
    }

    return t_caller;
}


transformer modulo_to_transformer(e, expr)
entity e;
expression expr;
{
    transformer tf = transformer_undefined;
    expression arg2 = expression_undefined;
    call c = syntax_call(expression_syntax(expr));

    debug(8, "modulo_to_transformer", "begin\n");
    
    arg2 = find_ith_argument(call_arguments(c), 2);

    if(integer_constant_expression_p(arg2)) {
	int d = integer_constant_expression_value(arg2);
	entity e_new = entity_to_new_value(e);
	Pvecteur ub = vect_new((Variable) e_new, 1);
	Pvecteur lb = vect_new((Variable) e_new, -1);
	Pcontrainte clb = contrainte_make(lb);
	Pcontrainte cub = CONTRAINTE_UNDEFINED;
	cons * tf_args = CONS(ENTITY, e_new, NIL);

	vect_add_elem(&ub, TCST, -(d-1));
	vect_add_elem(&lb, TCST, (d-1));
	cub = contrainte_make(ub);
	clb->succ = cub;
	tf = make_transformer(tf_args,
			      make_predicate(sc_make(CONTRAINTE_UNDEFINED, clb)));
    }

    ifdebug(8) {
	debug(8, "modulo_to_transformer", "result:\n");
	print_transformer(tf);
	debug(8, "modulo_to_transformer", "end\n");
    }

   return tf;
}

transformer integer_divide_to_transformer(e, expr)
entity e;
expression expr;
{
    transformer tf = transformer_undefined;
    call c = syntax_call(expression_syntax(expr));
    expression arg1 = expression_undefined;
    normalized n1 = normalized_undefined;
    expression arg2 = expression_undefined;

    debug(8, "integer_divide_to_transformer", "begin\n");
    
    arg1 = find_ith_argument(call_arguments(c), 1);
    n1 = NORMALIZE_EXPRESSION(arg1);
    arg2 = find_ith_argument(call_arguments(c), 2);

    if(integer_constant_expression_p(arg2) && normalized_linear_p(n1)) {
	int d = integer_constant_expression_value(arg2);
	entity e_new = entity_to_new_value(e);
	entity e_old = entity_to_old_value(e);
	cons * tf_args = CONS(ENTITY, e, NIL);
	/* must be duplicated right now  because it will be
	   renamed and checked at the same time by
	   value_mappings_compatible_vector_p() */
	Pvecteur vlb = vect_multiply(vect_dup((Pvecteur) normalized_linear(n1)), -1);
	Pvecteur vub = vect_dup((Pvecteur) normalized_linear(n1));
	Pcontrainte clb = CONTRAINTE_UNDEFINED;
	Pcontrainte cub = CONTRAINTE_UNDEFINED;

	(void) vect_variable_rename(vlb,
				    (Variable) e_new,
				    (Variable) e_old);
	(void) vect_variable_rename(vub,
				    (Variable) e_new,
				    (Variable) e_old);

	vect_add_elem(&vlb, (Variable) e_new, (Value) d);
	vect_add_elem(&vub, (Variable) e_new, (Value) -d);
	vect_add_elem(&vub, TCST, (Value) -(d-1));
	clb = contrainte_make(vlb);
	cub = contrainte_make(vub);
	clb->succ = cub;
	tf = make_transformer(tf_args,
			      make_predicate(sc_make(CONTRAINTE_UNDEFINED, clb)));
    }

    ifdebug(8) {
	debug(8, "integer_divide_to_transformer", "result:\n");
	print_transformer(tf);
	debug(8, "integer_divide_to_transformer", "end\n");
    }

    return tf;
}

transformer min0_to_transformer(e, expr)
entity e;
expression expr;
{
    return minmax_to_transformer(e, expr, TRUE);
}

transformer max0_to_transformer(e, expr)
entity e;
expression expr;
{
    return minmax_to_transformer(e, expr, FALSE);
}

transformer minmax_to_transformer(e, expr, minmax)
entity e;
expression expr;
bool minmax;
{
    transformer tf = transformer_undefined;
    call c = syntax_call(expression_syntax(expr));
    expression arg = expression_undefined;
    normalized n = normalized_undefined;
    list cexpr;
    cons * tf_args = CONS(ENTITY, e, NIL);
    Pcontrainte cl = CONTRAINTE_UNDEFINED;

    debug(8, "minmax_to_transformer", "begin\n");

    for(cexpr = call_arguments(c); !ENDP(cexpr); POP(cexpr)) {
	arg = EXPRESSION(CAR(cexpr));
	n = NORMALIZE_EXPRESSION(arg);

	if(normalized_linear_p(n)) {
	    Pvecteur v = vect_dup((Pvecteur) normalized_linear(n));
	    Pcontrainte cv = CONTRAINTE_UNDEFINED;
	    entity e_new = entity_to_new_value(e);
	    entity e_old = entity_to_old_value(e);

	    (void) vect_variable_rename(v,
					(Variable) e,
					(Variable) e_old);
	    vect_add_elem(&v, (Variable) e_new, (Value) -1);

	    if(minmax) {
		v = vect_multiply(v, -1);
	    }

	    cv = contrainte_make(v);
	    cv->succ = cl;
	    cl = cv;

	}
    }

    tf = make_transformer(tf_args,
			  make_predicate(sc_make(CONTRAINTE_UNDEFINED, cl)));


    ifdebug(8) {
	debug(8, "minmax_to_transformer", "result:\n");
	print_transformer(tf);
	debug(8, "minmax_to_transformer", "end\n");
    }

    return tf;
}

/* comp_expr_to_pnome.c */
/* scan a ri expression and try to make a polynomial of it */

/* Modif:
  -- entity_local_name is replaced by module_local_name. LZ 230993
  -- MAXINT replaced by INT_MAX, -MAXINT by INT_MIN FI 1/12/95
*/

#include <stdio.h>
/* #include <values.h>   */ 
#include <limits.h>
#include <string.h>

#include "genC.h"
#include "ri.h"
#include "complexity_ri.h"
#include "ri-util.h"

#include "misc.h"                 /* useful, pips_error is defined there */

#include "properties.h"

#include "complexity.h"

extern hash_table hash_callee_to_complexity;
extern hash_table hash_complexity_parameters;

char *noms_var(e)
entity e;
{
    bool entity_has_values_p();
    string external_value_name();

    if(e == (entity) TCST)
	return "";
    else
	return entity_has_values_p(e) ? (char *)module_local_name(e) :
	    (char *)external_value_name(e);
}

/* 
 * This file contains routines named "xxx_to_polynome".
 * They don't care about the complexity of the expression they scan,
 * however care about their value. They return a complete complexity,
 * not only a polynomial but also statistics: guessed, unknown variables, ...
 */


/* Entry point routine of this file:
 *
 * complexity expression_to_polynome(expression expr,
 *                                   transformer precond,
 *                                   list effects_list,
 *                                   boolean keep_symbols,
 *                                   int maximize)
 * return the polynomial associated to the expression expr,
 * or POLYNOME_UNDEFINED if it's not a polynome. 
 * or POLYNOME_NULL if it's a 0 complexity.
 * If keep_symbols is FALSE, we force evaluation of variables. 
 * If they can't be evaluated, they enter the polynomial,
 * but they are replaced by the pseudo-variable UNKNOWN_VARIABLE,
 * except when they appear in a loop range:
 * in that case, the whole range is replaced by UNKNOWN_RANGE.
 * The int maximize lets us use the mins and maxs of
 * preconditions system, to compute a WORST-CASE complexity
 * for the upper bound , maximize is 1
 * for the lower bound and increment, maximize is -1
 * (when the precondition doesn't give an exact value)
 *
 * effects_list is added on 10 Nov 92 to pass the effects
 * which can be used to determine the "must-be-written"
 * effects to delay the variable evaluation. LZ 10 Nov 92
 *
 * FI:
 * - better symbol generation for unknown complexities
 */
complexity expression_to_polynome(expr, precond, effects_list, keep_symbols, maximize)
expression expr;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    normalized no = NORMALIZE_EXPRESSION(expr);
    syntax sy = expression_syntax(expr);
    complexity comp = make_zero_complexity();

    trace_on("expression -> pnome");

    if ( expr == expression_undefined ) {
	pips_error("expression_to_polynome", "undefined expression\n");
    }
    if ( sy == syntax_undefined ) {
	pips_error("expression_to_polynome","wrong expression\n");
    }

    if ( normalized_linear_p(no) ) {
	comp =  normalized_to_polynome(no, precond, effects_list, 
				       keep_symbols, maximize);
    }
    else {
	comp = syntax_to_polynome(sy, precond, effects_list, keep_symbols, maximize);
    }

    if ( complexity_unknown_p(comp) ) {
	pips_error("complexity expression_to_polynome",
		   "Better unknown value name generation required!\n");
	/*
	return(make_single_var_complexity(1.0,UNKNOWN_RANGE));
	*/
	return complexity_undefined;
    }

    /* The following line is merely for debugging */
    
    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	fprintf(stderr, "expr->pnome ");
	complexity_fprint(stderr, comp, TRUE, TRUE);
    }

    trace_off();
    return (comp);
}

/* 1st element of expression */   
complexity syntax_to_polynome(synt, precond, effects_list, keep_symbols, maximize)
syntax synt;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity comp = complexity_undefined;

    trace_on("syntax -> pnome");
   
    switch (syntax_tag(synt)) {
    case is_syntax_reference: 
	comp = reference_to_polynome(syntax_reference(synt),
				     precond, effects_list, keep_symbols, maximize);
	break;
    case is_syntax_range:
	comp = range_to_polynome(syntax_range(synt),
				 precond, effects_list, keep_symbols, maximize);
	break;
    case is_syntax_call:
	comp = call_to_polynome(syntax_call(synt),
				precond, effects_list, keep_symbols, maximize);
	break;
    default:
	pips_error("syntax_to_polynome",
		   "This tag:%d is not in 28->30\n", syntax_tag(synt));
    }

    trace_off();
    return (comp);
}

/* 2nd element of expression */
complexity normalized_to_polynome(no, precond, effects_list, keep_symbols, maximize)
normalized no;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity comp = make_zero_complexity();

    trace_on("normalized -> pnome");

    if (normalized_linear_p(no) ) {
	Pvecteur pvect = (Pvecteur)normalized_linear(no);
	comp =  pvecteur_to_polynome(pvect, precond, effects_list, keep_symbols, maximize);
    }
    else
	pips_error("normalized_to_polynome", "vecteur undefined\n");

    trace_off();
    return(comp);
}

/* The only element available of normalized */
complexity pvecteur_to_polynome(pvect, precond, effects_list, keep_symbols, maximize)
Pvecteur pvect;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity comp = make_zero_complexity();
    Ppolynome ppvar;
    Pvecteur pv;
    Variable var;
    Value val;
    boolean we_must_evaluate;
/*    boolean must_be_written = FALSE; */

    trace_on("pvecteur  -> pnome maximize is %d", maximize);

    if ( get_bool_property("COMPLEXITY_INTERMEDIATES") ) {
	fprintf(stderr, "expr->pnome: pvecteur = ");
	vect_fprint(stderr, pvect, variable_name);
    }

    for(pv=pvect; !VECTEUR_NUL_P(pv); pv = pv->succ) {
	var = vect_first_var(pv);
	val = vect_coeff(var, pv);

	we_must_evaluate = 
	    (var != TCST) &&
	    (keep_symbols ? (!hash_contains_p(hash_complexity_parameters,
					      (char *)module_local_name((entity)var)))
	                  : TRUE);
/*
	must_be_written = is_must_be_written_var(effects_list, 
						 variable_local_name(var));
 
	if ( get_debug_level() >= 3 ) {
	    fprintf(stderr, "Must be written var %s\n", variable_local_name(var) );
	}
*/
/*	    
	if (we_must_evaluate && must_be_written) {
*/
	if (we_must_evaluate && get_bool_property("COMPLEXITY_EARLY_EVALUATION")) {
	    complexity ctmp;
	    ctmp = evaluate_var_to_complexity((entity)var, precond, effects_list, maximize);
	    /* should be multiplied by "val" here */
	    complexity_scalar_mult(&ctmp, VALUE_TO_FLOAT(val));
	    complexity_add(&comp, ctmp);
        }
	else { 
	    /* We keep this symbol (including TCST) in the polynome */
	    ppvar = make_polynome(VALUE_TO_FLOAT(val), var, VALUE_ONE);
	    if (complexity_zero_p(comp))
		comp = polynome_to_new_complexity(ppvar);
	    else
		complexity_polynome_add(&comp, ppvar);
	    varcount_symbolic(complexity_varcount(comp)) ++;
	}
    }

    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	fprintf(stderr, "Pvecteur evaluation = ");
	prc(comp);
    }

    trace_off();
    return (comp);
}

/* 1st element of syntax */
complexity reference_to_polynome(ref, precond, effects_list, keep_symbols, maximize)
reference ref;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity comp = make_zero_complexity();
    boolean we_must_evaluate;
/*    boolean must_be_written; */
    entity var = reference_variable(ref);

    trace_on("reference -> pnome");

    if ( reference_indices(ref) == NIL) {  
	/* if it's an array: let fall */
	we_must_evaluate = (keep_symbols ?
			    !hash_contains_p(hash_complexity_parameters,
					     (char *)module_local_name((entity)var) ):
			    TRUE);
/*
	must_be_written = is_must_be_written_var(effects_list, var);
 
	if ( get_debug_level() >= 3 ) {
	    fprintf(stderr, "Must be written var %s\n", noms_var(var) );
	}
*/
/*
	if (we_must_evaluate && must_be_written) {
*/
	if (we_must_evaluate && get_bool_property("COMPLEXITY_EARLY_EVALUATION")) {
	    comp = evaluate_var_to_complexity((entity)var, precond, effects_list, maximize);
	}
	else {         /* We keep this symbol in the polynome */
	    Ppolynome pp = make_polynome((float) 1, (Variable) var, VALUE_ONE);
	    comp = polynome_to_new_complexity(pp);
	    varcount_symbolic(complexity_varcount(comp)) ++;
	    polynome_rm(&pp);
	}
    }

    trace_off();
    return(comp);
}

/* 2nd element of syntax */
complexity range_to_polynome(rg, precond, effects_list, keep_symbols, maximize)
range rg;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity comp = make_zero_complexity();
    
    trace_on("range -> pnome");

    pips_error("range_to_polynome", "Don't you know\n");    

    trace_off();
    return(comp);
}

/* 3rd element of syntax */
complexity call_to_polynome(call_instr, precond, effects_list, keep_symbols, maximize)
call call_instr;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    entity f = call_function(call_instr);
    char *name = module_local_name(f);
    list args = call_arguments(call_instr);
    type t = entity_type(f);
    value v = entity_initial(f);
    complexity comp = make_zero_complexity();

    trace_on("CALL '%s' -> pnome", name);

    if (!type_functional_p(t) ||
	(value_intrinsic_p(v) && value_code_p(v) && value_constant_p(v)))
	pips_error("call_to_polynome",
		   "'%s' isn't an expected entity (type %d, value %d)\n",
		   type_tag(t), value_tag(v), name);

    switch (value_tag(v)) {
    case is_value_code:              
	/* For the moment, we don't want to evaluate the complexity of the   */
	break;                       /* function defined by the users    */
    case is_value_constant:
	complexity_float_add(&comp, constant_entity_to_float(f));
	break;
    case is_value_intrinsic:
	if (streq(name, PLUS_OP))
	    comp = plus_op_handler(args, precond, effects_list, keep_symbols, maximize);
	else if (streq(name, MINUS_OP))
	    comp = minus_op_handler(args, precond, effects_list, keep_symbols, maximize);
	else if (streq(name, MULTIPLY_OP))
	    comp = multiply_op_handler(args, precond, effects_list, keep_symbols, maximize);
	else if (streq(name, DIVIDE_OP))
	    comp = divide_op_handler(args, precond, effects_list, keep_symbols, maximize);
	else if (streq(name, POWER_OP))
	    comp = power_op_handler(args, precond, effects_list, keep_symbols, maximize);
	else if (streq(name, UNARY_MINUS_OP))
	    comp = unary_minus_op_handler(args, precond, effects_list, keep_symbols, maximize);
	break;
    }
    
    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	fprintf(stderr, "call->pnome '%s': ", name);
	complexity_fprint(stderr, comp, TRUE, TRUE);
    }

    trace_off();
    return (comp);
}

complexity plus_op_handler(args, precond, effects_list, keep_symbols, maximize)
list args;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity c1 = expression_to_polynome(EXPRESSION(CAR(args)),
					   precond, effects_list, keep_symbols,
					   maximize);
    complexity c2 = expression_to_polynome(EXPRESSION(CAR(CDR(args))),
					   precond, effects_list, keep_symbols,
					   maximize);

    complexity_add(&c1, c2);
    complexity_rm(&c2);

    return (c1);
}

complexity minus_op_handler(args, precond, effects_list, keep_symbols, maximize)
list args;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity c1 = expression_to_polynome(EXPRESSION(CAR(args)),
					   precond, effects_list, keep_symbols,
					   maximize);
    complexity c2 = expression_to_polynome(EXPRESSION(CAR(CDR(args))),
					   precond, effects_list, keep_symbols,
					   -maximize);

    complexity_sub(&c1, c2);
    complexity_rm(&c2);

    return (c1);
}

complexity multiply_op_handler(args, precond, effects_list, keep_symbols, maximize)
list args;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity c1 = expression_to_polynome(EXPRESSION(CAR(args)),
					   precond, effects_list, keep_symbols,
					   maximize);
    complexity c2 = expression_to_polynome(EXPRESSION(CAR(CDR(args))),
					   precond, effects_list, keep_symbols,
					   maximize);

    complexity_mult(&c1, c2);
    complexity_rm(&c2);

    return (c1);
}

complexity unary_minus_op_handler(args, precond, effects_list, keep_symbols, maximize)
list args;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    complexity c2 = expression_to_polynome(EXPRESSION(CAR(args)),
					   precond, effects_list, keep_symbols,
					   -maximize);
    complexity c1 = make_zero_complexity();

    complexity_sub(&c1, c2);
    complexity_rm(&c2);

    return (c1);
}

complexity divide_op_handler(args, precond, effects_list, keep_symbols, maximize)
list args;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    float denominateur;
    complexity c1 = expression_to_polynome(EXPRESSION(CAR(args)),
					   precond, effects_list, keep_symbols,
					   maximize);
    complexity c2 = expression_to_polynome(EXPRESSION(CAR(CDR(args))),
					   precond, effects_list, DONT_KEEP_SYMBOLS,
					   -maximize);
    if (complexity_constant_p(c2)) {
	denominateur = complexity_TCST(c2);
	if (denominateur == 0)
	    user_error("divide_op_handler", "division by zero\n");
	else 
	    complexity_mult(&c1, make_constant_complexity(1.0/denominateur));
    }
    else {
	complexity_rm(&c1);
    }

    complexity_rm(&c2);
    return (c1);
}

complexity power_op_handler(args, precond, effects_list, keep_symbols, maximize)
list args;
transformer precond;
list effects_list;
boolean keep_symbols;
int maximize;
{
    float power;
    complexity c1 = expression_to_polynome(EXPRESSION(CAR(args)),
					   precond, effects_list, keep_symbols,
					   maximize);
    complexity c2 = expression_to_polynome(EXPRESSION(CAR(CDR(args))),
					   precond, effects_list, DONT_KEEP_SYMBOLS,
					   maximize);

    if (complexity_constant_p(c2)) {
	power = complexity_TCST(c2);
	complexity_rm(&c2);
	if (power == (int) power) {
	    Ppolynome pp = polynome_power_n(complexity_polynome(c1),
					    (int) power);
	    Ppolynome pt = complexity_eval(c1);

	    /* polynome_rm(&(complexity_eval(c1))); */
	    polynome_rm(&pt);
	    /* (Ppolynome) complexity_eval(c1) = pp; */
	    complexity_eval_(c1) = newgen_Ppolynome(pp);
	    return (c1);
	}
    }
    else 
	complexity_rm(&c2);

    complexity_rm(&c1);
    return (make_zero_complexity());
}

/* complexity evaluate_var_to_complexity(entity var,
 *                                       transformer precond,
 *                                       list effects_list,
 *                                       int maximize)
 * Return, packed in a complexity, the exact value of variable var
 * or its max if (maximize==MAXIMUM_VALUE), or its min, according to 
 * preconditions passed in precond. 
 * If nothing is found,
 * the complexity returned is UNKNOWN_VARIABLE, with a
 * varcount_unknown set to 1. The variable statistics are up to
 * date in the new complexity returned, in any case.
 */
complexity evaluate_var_to_complexity(var, precond, effects_list, maximize)
entity var;
transformer precond;
list effects_list;
int maximize;
{
    predicate pred = transformer_relation(precond);
    Psysteme psyst = (Psysteme) predicate_system(pred);
    Value min = VALUE_MAX; 
    Value max = VALUE_MIN;
    boolean faisable;
    complexity comp = make_zero_complexity();

#define maxint_p(i) ((i) == INT_MAX)
#define minint_p(i) ((i) == (INT_MIN))
    
    trace_on("variable %s -> pnome, maximize %d ", entity_name(var), maximize);

    /* This is the only case that we use the precondition */
    if (entity_integer_scalar_p(var) &&
	precond != transformer_undefined &&
	pred != predicate_undefined && 
	!SC_UNDEFINED_P(psyst) ) {
	Psysteme ps = sc_dup(psyst);
	Psysteme ps1;
	Pvecteur pv;
	char *precondition_to_string();

	if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	    sc_fprint(stderr,ps,noms_var);
	}

	for ( pv=ps->base; !VECTEUR_NUL_P(pv); pv=pv->succ) {
	    if ( var_of(pv) != (Variable)var ) {
		boolean b = hash_contains_p(hash_complexity_parameters,
					(char *)module_local_name((entity)var_of(pv))); 

		if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
		    fprintf(stderr,"var_of is %s -- variable is %s --bool is %d\n",
		       module_local_name((entity)(var_of(pv)) ), 
		       module_local_name((entity)(var) ), b);
		}

		if (!b) {
		    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
			fprintf(stderr, "in projection %s\n",noms_var((entity)var_of(pv)));
		    }
		    ps = sc_projection(ps,var_of(pv));
		}
	    }
	}

	if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	    fprintf(stderr, "Prec=%s", precondition_to_string(precond));
	    sc_fprint(stderr,ps,noms_var);

	    if ( !SC_UNDEFINED_P(ps) )
		fprintf(stderr,"ps OK\n");
	    else 
		fprintf(stderr,"ps is NULL\n");
	}

	/* added by LZ 18/02/92 */
	if ( hash_contains_user_var_p(hash_complexity_parameters,
				      (char *) var) ) {
	    if (get_bool_property("COMPLEXITY_INTERMEDIATES"))
		fprintf(stderr,"Don't evaluate %s------\n", noms_var(var));
	    return(make_single_var_complexity(1.0, (Variable)var));
	}

	ps1 = sc_dup(ps);
	faisable = sc_minmax_of_variable(ps1, (Variable) var,
					 &min, &max);

	if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	    fprintf(stderr," faisable is %d\n",faisable);
	    fprintf(stderr,"Variable %s -- ", noms_var(var));
	    fprint_string_Value(stderr, "min is ", min);
	    fprint_string_Value(stderr, ", max is ", max);
	    fprintf(stderr,", maximize is %d\n", maximize);
	}

	if ( faisable && (value_notmin_p(min) || value_notmax_p(max)) )
	    {

	    if ( value_notmax_p(max) && TAKE_MAX(maximize) ) {
		comp = simplify_sc_to_complexity(ps,(Variable)var);
		if ( complexity_zero_p(comp) )
		    comp = make_constant_complexity( VALUE_TO_FLOAT(max) );
		varcount_bounded(complexity_varcount(comp)) ++;
	    }
	    else if ( value_notmin_p(min) && TAKE_MIN(maximize) ) {
		comp = simplify_sc_to_complexity(ps,(Variable)var);
		if ( complexity_zero_p(comp) )
		    comp = make_constant_complexity( VALUE_TO_FLOAT(min) );
		varcount_bounded(complexity_varcount(comp)) ++;
	    }

	    /* for the inner loop 28/06/91 example p.f */
	    else if ( value_max_p(max) && TAKE_MAX(maximize) ) {
		comp = simplify_sc_to_complexity(ps,(Variable)var);
		if ( complexity_zero_p(comp) )
		    comp = make_constant_complexity( VALUE_TO_FLOAT(max) );
		varcount_bounded(complexity_varcount(comp)) ++;
	    }
	    else if ( value_min_p(min) && TAKE_MIN(maximize) ) {
		comp = simplify_sc_to_complexity(ps,(Variable)var);
		if ( complexity_zero_p(comp) )
		    comp = make_constant_complexity( VALUE_TO_FLOAT(min) );
		varcount_bounded(complexity_varcount(comp)) ++;
	    }

	    else if (min == max) {
		comp = make_constant_complexity( VALUE_TO_FLOAT(max) );
		varcount_guessed(complexity_varcount(comp)) ++;
		if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
		    fprintf(stderr,"value is ");
		    fprint_Value(stderr, val_of((ps1->egalites)->vecteur));
		    fprintf(stderr, "\n");
		    fprint_string_Value(stderr,"max = min = ",max);
		    fprintf(stderr, "\n");
		}
	    }
	    else if ( value_notmin_p(min) && value_notmax_p(max) ) {
		comp = simplify_sc_to_complexity(ps,(Variable)var);
	    }
	    else {
		comp = make_single_var_complexity( 1.0, (Variable) var);
		varcount_bounded(complexity_varcount(comp)) ++;
	    }
	}
	else if ( faisable ) {
	    comp = simplify_sc_to_complexity(ps,(Variable)var);
	}
    }
    else {
	comp = make_single_var_complexity(1.0, (Variable) var);
	varcount_unknown(complexity_varcount(comp)) ++;
    }

    trace_off();
    return(comp);
}

/* This function is recently added by L.Zhou   June 5, 91
 * simplify_sc_to_complexity(Psysteme ps, Variable var)
 * It looks for the egality formula containing (Variable)var 
 * in the system (Psysteme)ps. 
 * If ps->egalites is NULL, zero complexity is returned.
 * The rest of the variable in that formula should be the known variable
 * for example: formal parameter(s), inductible variable, etc.
 * EX: M1 - M2 = 1   where M1 is formal parameter
 *                   where M2 is an inductible variable
 * This function returns M2 = M1 - 1 packed in the polynomial of the complexity
 * the statistics of this complexity should be all zero.
 */
complexity simplify_sc_to_complexity(ps, var)
Psysteme ps;
Variable var;
{
    complexity comp = make_zero_complexity();
    Value var_coeff=VALUE_ONE; 

    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	sc_fprint(stderr,ps,noms_var);
    }

    if ( !SC_UNDEFINED_P(ps) && !CONTRAINTE_UNDEFINED_P(ps->egalites) ) {
	Pvecteur v = (ps->egalites)->vecteur;

	for  ( ; !VECTEUR_NUL_P(v); v=v->succ ) {
	    if ( v->var != TCST ) {
		if ( v->var != (Variable)var ) {
		    complexity c = make_single_var_complexity
			(VALUE_TO_FLOAT(v->val),v->var);
		    complexity_add(&comp, c);
		    complexity_rm(&c);
		}
		else {
		    var_coeff = value_uminus(v->val);
		}
	    }
	    else {
		complexity c = make_constant_complexity
		    (VALUE_TO_FLOAT(v->val));
		complexity_add(&comp, c);
		complexity_rm(&c);
	    }
	}
    }
    else {
	boolean b = hash_contains_p(hash_complexity_parameters,
		           (char *)module_local_name((entity)var)); 

	if ( b )
	    comp = make_single_var_complexity(1.0, (Variable)var);
	else {
	    string v_prefix = strdup
		(concatenate(UNKNOWN_VARIABLE_VALUE_PREFIX,
			     entity_local_name((entity) var),
			     "_", 0));
	    Variable v = (Variable)
		make_new_scalar_variable_with_prefix
		    (v_prefix,
		     get_current_module_entity(),
		     MakeBasic (is_basic_int));
	    free(v_prefix);
	    comp = make_single_var_complexity(1.0, v); 
	}
    }

    complexity_scalar_mult(&comp,1.0/VALUE_TO_FLOAT(var_coeff));
    varcount_unknown(complexity_varcount(comp)) ++;

    return (comp);
}

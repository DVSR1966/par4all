/* $RCSfile: utils.c,v $ (version $Revision$)
 * $Date: 1997/04/15 17:27:32 $, 
 *
 * utilities for reductions.
 *
 * FC, June 1996.
 */

#include "local-header.h"
#include "semantics.h"

/******************************************************************** MISC */
/* ??? some arrangements with words_range to print a star in this case:-)
 */
static syntax make_star_syntax()
{
    return make_syntax(is_syntax_range, 
		       make_range(expression_undefined, 
				  expression_undefined, 
				  expression_undefined));
}

/***************************************************** REFERENCED ENTITIES */
/* returns the list of referenced variables
 */
static list /* of entity */ referenced;
static void ref_rwt(reference r)
{ referenced = gen_once(reference_variable(r), referenced);}

static list /* of entity */
referenced_variables(reference r)
{
    referenced = NIL;
    gen_recurse(r, reference_domain, gen_true, ref_rwt);
    return referenced;
}

/************************************** REMOVE A VARIABLE FROM A REDUCTION */
/* must be able to remove a modified variable from a reduction:
 * . A(I) / I -> A(*)
 * . A(B(C(I))) / C -> A(B(*))
 * . A(I+J) / I -> A(*)
 * . A(B(I)+C(I)) / I -> A(*) or A(B(*)+C(*)) ? former looks better
 */

/* the variable that must be removed is stored here
 */
static entity variable_to_remove;
/* list of expressions to be deleted (at rwt)
 */
static list /* of expression */ dead_expressions;
/* the first expression encountered which is a function call, so as 
 * to avoid "*+J" results
 */
static expression first_encountered_call;
/* stack of reference expressions (if there is no call)
 */
DEFINE_LOCAL_STACK(ref_exprs, expression)

static bool expr_flt(expression e)
{ 
    if (expression_reference_p(e)) /* REFERENCE */
	ref_exprs_push(e);
    else if (!first_encountered_call && expression_call_p(e)) /* CALL */
	first_encountered_call = e;
    return TRUE;
}

static void expr_rwt(expression e)
{
    if (expression_reference_p(e)) 
	ref_exprs_pop();
    else if (first_encountered_call==e)
	first_encountered_call = NULL;

    if (gen_in_list_p(e, dead_expressions))
    {
	free_syntax(expression_syntax(e));
	expression_syntax(e) = make_star_syntax();
    }
}

static bool ref_flt(reference r)
{
    if (reference_variable(r)==variable_to_remove)
    {
	dead_expressions = 
	    gen_once(first_encountered_call? first_encountered_call:
		     ref_exprs_head(), dead_expressions);
	return FALSE;
    }
    return TRUE;
}

void 
remove_variable_from_reduction(
    reduction red,
    entity var)
{
    variable_to_remove = var;
    dead_expressions = NIL;
    first_encountered_call = NULL;
    make_ref_exprs_stack();

    pips_debug(8, "removing %s from %s[%s]\n",
	       entity_name(var), 
	       reduction_operator_tag_name
	          (reduction_operator_tag(reduction_op(red))),
	       entity_name(reduction_variable(red)));

    gen_multi_recurse(reduction_reference(red), 
		      expression_domain, expr_flt, expr_rwt,
		      reference_domain, ref_flt, gen_null,
		      NULL);

    gen_free_list(dead_expressions);
    free_ref_exprs_stack();
}

bool
update_reduction_under_effect(
    reduction red,
    effect eff)
{
    entity var = effect_variable(eff);
    bool updated = FALSE;
    
    pips_debug(7, "reduction %s[%s] under effect %s on %s\n",
	       reduction_operator_tag_name
	          (reduction_operator_tag(reduction_op(red))),
	       entity_name(reduction_variable(red)),
	       effect_write_p(eff)? "W": "R",
	       entity_name(effect_variable(eff)));

    /* REDUCTION is dead if the reduction variable is affected
     */
    if (entity_conflict_p(reduction_variable(red),var))
    {
	reduction_operator_tag(reduction_op(red)) = 
	    is_reduction_operator_none;
	return FALSE;
    }
    /* else */

    if (effect_read_p(eff)) return TRUE;

    /* now var is written */
    MAP(ENTITY, e, 
    {
	if (entity_conflict_p(var, e)) 
	{ 
	    updated = TRUE; 
	    remove_variable_from_reduction(red, e);
	}
    },
	reduction_dependences(red));

    if (updated)
    {
	gen_free_list(reduction_dependences(red));
	reduction_dependences(red) =
	     referenced_variables(reduction_reference(red));
    }

    return TRUE;
}

/* looks for a reduction about var in reds, and returns it.
 * tells whether it worths keeping on. It does not if there may be some 
 * conflicts with other reduced variables...
 */
static bool 
find_reduction_of_var(
    entity var,
    reductions reds,
    reduction *pr)
{
    MAP(REDUCTION, r,
    {
	entity red_var = reduction_variable(r);
	if (red_var==var)
	{
	    *pr = copy_reduction(r);
	    return TRUE;
	}
	else if (entity_conflict_p(red_var, var))
	    return FALSE; /* I will not combine them... */
    },
	reductions_list(reds));
    return TRUE;
}

/* merge two reductions into first so as to be compatible with both.
 * deletes the second. tells whether they where compatibles
 * quite basic at the time
 */
static bool 
merge_two_reductions(reduction first, reduction second)
{
    pips_assert("same variable", 
		reduction_variable(first)==reduction_variable(second));

    if (reduction_operator_tag(reduction_op(first))!=
	reduction_operator_tag(reduction_op(second)))
    {
	free_reduction(second);
	return FALSE;
    }

    if (!reference_equal_p(reduction_reference(first), 
			   reduction_reference(second)))
    {
	/* actually merges, very simple at the time 
	 */
	free_reference(reduction_reference(first));
	reduction_reference(first) = 
	    make_reference(reduction_variable(second), NIL);
    }

    free_reduction(second);
    return TRUE;
}

/* update *pr according to r for variable var
 * r is not touched.
 */
bool 
update_compatible_reduction_with(
    reduction *pr,
    entity var,
    reduction r)
{
    if (reduction_variable(r)!=var)
	return !entity_conflict_p(var, reduction_variable(r));

    /* else same var and no conflict */
    if (reduction_none_p(*pr))
    {
	free_reduction(*pr);
	*pr = copy_reduction(r);
	return TRUE;
    }
    /* else are they compatible? 
     */
    if (reduction_tag(*pr)!=reduction_tag(r))
	return FALSE;
    /* ok, let us merge them
     */
    return merge_two_reductions(*pr, copy_reduction(r));
}

/* what to do with reduction *pr for variable var 
 * under effects le and reductions reds.
 * returns whether worth to go on.
 * conditions:
 */   
bool 
update_compatible_reduction(
    reduction *pr,
    entity var,
    list /* of effect */ le,
    reductions reds)
{
    reduction found = NULL;

    if (!find_reduction_of_var(var, reds, &found)) 
	return FALSE;

    if (found)
    {
	if (!reduction_none_p(*pr)) /* some reduction already available */
	    return merge_two_reductions(*pr, found);
	else /* must update the reduction with the encountered effects */
	{
	    MAP(ENTITY, e,
		remove_variable_from_reduction(found, e),
		reduction_dependences(*pr));

	    free_reduction(*pr); *pr = found;
	    return TRUE;
	}
    }
    /* else 
     * now no new reduction waas found, must check *pr against effects 
     */
    if (!reduction_none_p(*pr)) /* some reduction */
    {
	MAP(EFFECT, e,
	    if (!update_reduction_under_effect(*pr, e))
	    {
		DEBUG_REDUCTION(8, "kill of ", *pr);
		pips_debug(8, "under effect to %s\n", 
			   entity_name(effect_variable(e)));

		return FALSE;
	    },
	    le);
    }
    else
    {
	MAP(EFFECT, e,
	{
	    if (entity_conflict_p(effect_variable(e), var))
	        return FALSE;
	    else if (effect_write_p(e)) /* stores for latter cleaning */
		reduction_dependences(*pr) = 
		    gen_once(effect_variable(e), reduction_dependences(*pr));
	},
	    le);
    }
    
    return TRUE;
}

/*************************************************** CALL PROPER REDUCTION */
/* extract the proper reduction of a call (instruction) if any.
 */

/* I trust intrinsics (operations?) and summary effects...
 */
bool pure_function_p(entity f)
{
    value v = entity_initial(f);

    if (value_symbolic_p(v) || value_constant_p(v) || value_intrinsic_p(v))
	return TRUE;
    /* else */

    if (entity_module_p(f))
    {
	MAP(EFFECT, e,
	    {
		if (effect_write_p(e)) /* a side effect!? */
		    return FALSE; 
		if (io_effect_entity_p(effect_variable(e))) /* LUNS */
		    return FALSE; 
	    },
	load_summary_effects(f));
    }

    return TRUE;
}

/* tells whether r is a functional reference...
 * actually I would need to recompute somehow the proper effects of obj?
 */
static bool is_functional;
static bool call_flt(call c)
{
    if (pure_function_p(call_function(c))) 
	return TRUE;
    /* else */
    is_functional = FALSE;
    gen_recurse_stop(NULL);
    return FALSE;
}
static bool
functional_object_p(gen_chunk* obj)
{
    is_functional = TRUE;
    gen_recurse(obj, call_domain, call_flt, gen_null);
    return is_functional;
}

/****************************************************** REDUCTION OPERATOR */

/* tells whether entity f is a reduction operator function
 * also returns the corresponding tag, and if commutative
 */
#define OKAY(op,com) { *op_tag=op; *commutative=com; return TRUE;}
static bool 
function_reduction_operator_p(
    entity f,
    tag *op_tag,
    bool *commutative)
{
    if (ENTITY_PLUS_P(f)) 
	OKAY(is_reduction_operator_sum, TRUE)
    else if (ENTITY_MINUS_P(f))
	OKAY(is_reduction_operator_sum, FALSE)
    else if (ENTITY_MULTIPLY_P(f))
	OKAY(is_reduction_operator_prod, TRUE)
    else if (ENTITY_DIVIDE_P(f))
	OKAY(is_reduction_operator_prod, FALSE)
    else if (ENTITY_MIN_P(f) || ENTITY_MIN0_P(f))
	OKAY(is_reduction_operator_min, TRUE)
    else if (ENTITY_MAX_P(f) || ENTITY_MAX0_P(f))
	OKAY(is_reduction_operator_max, TRUE)
    else if (ENTITY_AND_P(f))
	OKAY(is_reduction_operator_and, TRUE)
    else if (ENTITY_OR_P(f))
	OKAY(is_reduction_operator_or, TRUE)
    else
	return FALSE;
}

/* returns the possible operator of expression e if it is a reduction, 
 * and the operation commutative. (- and / are not)
 */
static bool 
extract_reduction_operator(
    expression e,
    tag *op_tag,
    bool *commutative)
{
    syntax s = expression_syntax(e);
    call c;
    entity f;

    if (!syntax_call_p(s)) return FALSE;
    c = syntax_call(s);
    f = call_function(c);
    return function_reduction_operator_p(f, op_tag, commutative);
}

/* tells whether call to f is compatible with tag op.
 * if so, also returns whether f is commutative
 */
static bool
reduction_function_compatible_p(
    entity f,
    tag op,
    bool *pcomm)
{
    tag nop;
    if (!function_reduction_operator_p(f, &nop, pcomm))
	return FALSE;
    return nop==op;
}

/***************************************************** FIND SAME REFERENCE */

/* looks for an equal reference in e, for reduction rop.
 * the reference found is also returned.
 * caution: 
 * - for integers, / is *not* a valid reduction operator:-(
 *   this case is detected here...
 * static variables: 
 * - refererence looked for fsr_ref, 
 * - reduction operator tag fsr_op,
 * - returned reference fsr_found,
 * - boolean fsr_okay.
 */
static reference fsr_ref, fsr_found;
static tag fsr_op;
static bool fsr_okay;
static bool fsr_reference_flt(reference r)
{
    if (reference_equal_p(r, fsr_ref))
    {
	fsr_found = r;
	/* stop the recursion if does not need to check int div
	 */
	if (!basic_int_p(entity_basic(reference_variable(fsr_ref))) ||
	    (fsr_op!=is_reduction_operator_prod))
	    gen_recurse_stop(NULL);
    }

    return FALSE; /* no candidate refs within a ref! */
}
static bool fsr_call_flt(call c)
{
    bool comm;
    if (!reduction_function_compatible_p(call_function(c), fsr_op, &comm))
	return FALSE;
    /* else */
    if (!comm)
    {
	list /* of expression */ le = call_arguments(c);
	pips_assert("length is two", gen_length(le)==2);
	gen_recurse_stop(EXPRESSION(CAR(CDR(le))));

	/* int div */
	if (basic_int_p(entity_basic(reference_variable(fsr_ref))) &&
	    (fsr_op==is_reduction_operator_prod))
	    fsr_okay = FALSE;
    }

    return TRUE;
}

static bool 
equal_reference_in_expression_p(
    reference r,       /* looked for */
    expression e,      /* visited object */
    tag rop,           /* assumed reduction */
    reference *pfound) /* returned */
{
    fsr_ref = r;
    fsr_op  = rop;
    fsr_found = NULL;
    fsr_okay = TRUE; /* no int / */

    gen_multi_recurse(e,
		      call_domain, fsr_call_flt, gen_null,
		      reference_domain, fsr_reference_flt, gen_null,
		      NULL);

    *pfound = fsr_found;
    return (bool)fsr_found && fsr_okay;
}

/******************************************************** NO OTHER EFFECTS */

/* checks that the references are the only touched within this statement.
 * I trust the proper effects to store all references...
 */
bool
no_other_effects_on_references(
    statement s,
    list /* of reference on the same variable */ lr)
{
    list /* of effect */ le; 
    entity var;
    if (ENDP(lr)) return TRUE;

    le = load_statement_proper_references(s);
    var = reference_variable(REFERENCE(CAR(lr)));

    MAP(EFFECT, e,
    {
	reference r = effect_reference(e);
	if (!gen_in_list_p(r, lr) && 
	    entity_conflict_p(reference_variable(r), var))
	    return FALSE;
    },
	le);

    return TRUE;
}

/************************************************ EXTRACT PROPER REDUCTION */
/* is the call in s a reduction? returns the reduction if so.
 * mallocs are avoided if nothing is found...
 * looks for v = v OP y, where y is independent of v.
 */
bool 
call_proper_reduction_p(
    statement s,    /* needed to query about proper effects */
    call c,         /* the call of interest */
    reduction *red) /* the returned reduction (if any) */
{
    list /* of expression */ le, 
         /* of reference */ lr, 
         /* of Preference */ lp;
    expression elhs, erhs;
    reference lhs, other;
    tag op;
    bool comm;

    pips_debug(7, "call to %s (0x%x)\n", 
	       entity_name(call_function(c)), (unsigned int) s);

    if (!ENTITY_ASSIGN_P(call_function(c))) 
	return FALSE;

    le = call_arguments(c);
    pips_assert("two arguments", gen_length(le)==2);
    elhs = EXPRESSION(CAR(le));
    erhs = EXPRESSION(CAR(CDR(le)));

    pips_assert("lhs reference", syntax_reference_p(expression_syntax(elhs)));
    lhs = syntax_reference(expression_syntax(elhs));

    /* the lhs and rhs must be functionnal 
     * (same location on different evaluations)
     */
    if (!functional_object_p(lhs) || !functional_object_p(erhs))
	return FALSE;
    pips_debug(8, "lsh and rhs are functional\n");

    /* the operation performed must be valid for a reduction
     */
    if (!extract_reduction_operator(erhs, &op, &comm))
	return FALSE;
    pips_debug(8, "reduction operator %s\n", reduction_operator_tag_name(op));

    /* there should be another direct reference as lhs
     * !!! syntax is a call if extract_reduction_operator returned TRUE
     */
    if (!equal_reference_in_expression_p(lhs, erhs, op, &other))
	return FALSE;
    pips_debug(8, "matching reference found (0x%x)\n", (unsigned int) other);
    
    /* there should be no extract effects on the reduced variable
     */
    lr = CONS(REFERENCE, lhs, CONS(REFERENCE, other, NIL));
    if (!no_other_effects_on_references(s, lr))
    {
	gen_free_list(lr);
	return FALSE;
    }
    /* lr is used latter on */
    pips_debug(8, "no other effects\n");

    lp = NIL;
    MAP(REFERENCE, r, lp = CONS(PREFERENCE, make_preference(r), lp), lr);
    gen_free_list(lr), lr = NIL;

    /* well, it is ok for a reduction now! 
     */
    *red = make_reduction(copy_reference(lhs),
			  make_reduction_operator(op, UU), 
			  referenced_variables(lhs),
			  lp);

    DEBUG_REDUCTION(7, "returning\n", *red);
    return TRUE;
}

/* end of $RCSfile: utils.c,v $
 */

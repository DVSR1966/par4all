#include <stdio.h>
#include <string.h>
#include <values.h>

#include "genC.h"
#include "ri.h"
#include "misc.h"

#include "ri-util.h"

extern int fprintf();

/*
 * this function shouldn't be called. 
 * Use macro NORMALIZE_EXPRESSION(e) instead.
 */
normalized NormalizeExpression(e)
expression e;
{
    normalized n;

    if (expression_normalized(e) != normalized_undefined)
	    pips_error("NormalizeExpression", 
		       "expression is already normalized\n");

    n = NormalizeSyntax(expression_syntax(e));

    return(n);
}

normalized NormalizeSyntax(s)
syntax s;
{
    normalized n=normalized_undefined;

    switch (syntax_tag(s)) {
      case is_syntax_reference:
	n = NormalizeReference(syntax_reference(s));
	break;
      case is_syntax_range:
	n = make_normalized(is_normalized_complex, UU);
	break;
      case is_syntax_call:
	n = NormalizeCall((syntax_call(s)));
	break;
      default:
	pips_error("NormalizeSyntax", "cas default");
    }

    return(n);
}

normalized NormalizeCall(c)
call c;
{
    normalized n=normalized_undefined;
    entity f = call_function(c);
    value v = entity_initial(f);

    switch (value_tag(v)) {
      case is_value_intrinsic:
	n = NormalizeIntrinsic(f, call_arguments(c));
	break;
      case is_value_constant:
	n = NormalizeConstant((value_constant(v)));
	break;
      case is_value_symbolic:
	n = NormalizeConstant((symbolic_constant(value_symbolic(v))));
	break;
      case is_value_unknown:
      case is_value_code:
	n = make_normalized(is_normalized_complex, UU);
	break;
      default:
	pips_error("NormalizeCall", "case default");
    }

    return(n);
}

normalized NormalizeConstant(c) 
constant c;
{
    return((constant_int_p(c)) ?
	   make_normalized(is_normalized_linear, 
			   vect_new(TCST, constant_int(c))) :
	   make_normalized(is_normalized_complex, UU));
}

normalized NormalizeReference(r)
reference r;
{
    normalized n=normalized_undefined;
    entity e = reference_variable(r);
    type t = entity_type(e);

    if (! type_variable_p(t)) {
	pips_error("NormalizeReference", "should be a variable !");
    }
    else {
	n = (entity_integer_scalar_p(e)) ?
	    make_normalized(is_normalized_linear, 
			    vect_new((Variable) e, 1)) :
		make_normalized(is_normalized_complex, UU);
    }

    return(n);
}

normalized NormalizeIntrinsic(e, la)
entity e;
cons *la;
{
    normalized n;

    if (! top_level_entity_p(e))
	return(make_normalized(is_normalized_complex, UU));

    if (ENTITY_UNARY_MINUS_P(e)) {
	n = NormalizeExpression(EXPRESSION(CAR(la)));

	if (normalized_complex_p(n))
	    return(n);

	vect_chg_sgn(normalized_linear(n));
    }
    else if (ENTITY_MINUS_P(e) || ENTITY_PLUS_P(e)) {
	normalized ng, nd;

	ng = NormalizeExpression(EXPRESSION(CAR(la)));
	if (normalized_complex_p(ng))
	    return(ng);

	nd = NormalizeExpression(EXPRESSION(CAR(CDR(la))));
	if (normalized_complex_p(nd)) {
	    FreeNormalized(ng);
	    return(nd);
	}

	n = ENTITY_PLUS_P(e) ? 
	    make_normalized(is_normalized_linear, 
			    vect_add(normalized_linear(ng), 
					   normalized_linear(nd))) : 
	    make_normalized(is_normalized_linear, 
			    vect_substract(normalized_linear(ng), 
				     normalized_linear(nd)));

	FreeNormalized(ng);
	FreeNormalized(nd);
    }
    else if (ENTITY_MULTIPLY_P(e)) {
	normalized ng, nd;
	int val;

	ng = NormalizeExpression(EXPRESSION(CAR(la)));
	if (normalized_complex_p(ng))
	    return(ng);

	nd = NormalizeExpression(EXPRESSION(CAR(CDR(la))));
	if (normalized_complex_p(nd)) {
	    FreeNormalized(ng);
	    return(nd);
	}

	if (EvalNormalized(nd, &val)) {
	    FreeNormalized(nd);
	    normalized_linear(n = ng) = 
		(char *) vect_multiply(normalized_linear(ng), val);
	}
	else if (EvalNormalized(ng, &val)) {
	    FreeNormalized(ng);
	    normalized_linear(n = nd) = 
		(char *) vect_multiply(normalized_linear(nd), val);
	}
	else {
	    FreeNormalized(ng);
	    FreeNormalized(nd);
	    return(make_normalized(is_normalized_complex, UU));
	}
    }
    else {
	return(make_normalized(is_normalized_complex, UU));
    }

    return(n);
}

bool EvalNormalized(n, pv)
normalized n;
int *pv;
{
    if (normalized_linear_p(n)) {
	Pvecteur v = (Pvecteur) normalized_linear(n);
	if (vect_size(v) == 1 && var_of(v) == TCST) {
	    *pv = val_of(v);
	    return(TRUE);
	}
    }
    
    return(FALSE);		
}

void FreeNormalized(n)
normalized n;
{
    /* FI: theoretically, free_normalized() performs the next three lines... */
    if (normalized_linear_p(n)) {
	vect_rm(normalized_linear(n));
	normalized_linear(n) = NULL;
    }

    free_normalized(n);
    /* gen_free(n); */
}

void free_expression_normalized(e)
expression e;
{
    normalized n = normalized_undefined;

    if((n = expression_normalized(e)) != normalized_undefined) {
	FreeNormalized(n);
	expression_normalized(e) = normalized_undefined;
    }
}

/* FI: any chunk* can be passed; this function could be applied to an expression */
void recursive_free_normalized(st)
statement st ;
{
    gen_recurse(st, 
		expression_domain,
		gen_true,
		free_expression_normalized);
}

Pvecteur expression_to_affine(e)
expression e;
{
    normalized n;
    Pvecteur v;

    /* As long as VECTEUR_UNDEFINED is not different from VECTEUR_NUL
       this routine cannot be used safely; either 0 will be considered
       not linear or every non linear expression will be equal to 0 */

    /* should it also perform the conversion from variable entities
       to new value entities? */

    if(expression_normalized(e)==normalized_undefined)
	n = NormalizeExpression(e);
    else
	n = expression_normalized(e);

    if(normalized_linear_p(n))
	v = vect_dup((Pvecteur) normalized_linear(n));
    else 
	v = VECTEUR_UNDEFINED;

    return v;
}


/* -----------------------------------------------------------
 *
 *    New Stuff to normalize all expressions - bottom-up 
 *
 *    FC 09/06/94
 */

static bool normalized_constant_p(n, pv)
normalized n;
int *pv;
{
    if (normalized_linear_p(n))
    {
	Pvecteur v = normalized_linear(n);
	int length = vect_size(v);
	
	if (var_of(v)==TCST && length==1) 
	{
	    *pv=val_of(v); 
	    return(TRUE);
	}
	
	if (length==0) 
	{
	    *pv=0;
	    return(TRUE);
	}
    }
    
    return(FALSE);
}    

static normalized normalize_intrinsic(f, la)
entity f;
list la;
{
    bool minus;
	
    if (! top_level_entity_p(f))
	return(make_normalized(is_normalized_complex, UU));

    if (ENTITY_UNARY_MINUS_P(f))
    {
	Pvecteur
	    tmp = VECTEUR_NUL;
	normalized 
	    n = expression_normalized(EXPRESSION(CAR(la)));

	return(normalized_complex_p(n) ?
	       make_normalized(is_normalized_complex, UU) :
	       make_normalized(is_normalized_linear, 
		   (vect_chg_sgn(tmp=vect_dup(normalized_linear(n))), tmp)));
    }
    /* else */
    if ((minus=ENTITY_MINUS_P(f)) || (ENTITY_PLUS_P(f)))
    {
	normalized
	    n1 = expression_normalized(EXPRESSION(CAR(la))),
	    n2 = expression_normalized(EXPRESSION(CAR(CDR(la))));
	Pvecteur (*operation)() = minus ? vect_substract : vect_add;

	assert(!normalized_undefined_p(n1) && !normalized_undefined_p(n2));

	if (normalized_complex_p(n1) || normalized_complex_p(n2))
	    return(make_normalized(is_normalized_complex, UU));
	/* else */
	return(make_normalized(is_normalized_linear, 
			       operation(normalized_linear(n1), 
					 normalized_linear(n2))));
    }
    /* else */
    if (ENTITY_MULTIPLY_P(f))
    {
	normalized
	    n1 = expression_normalized(EXPRESSION(CAR(la))),
	    n2 = expression_normalized(EXPRESSION(CAR(CDR(la))));
	bool
	    b1, b2;
	int
	    i1, i2;

	if (normalized_complex_p(n1) || normalized_complex_p(n2))
	    return(make_normalized(is_normalized_complex, UU));
	/* else */
	b1 = normalized_constant_p(n1, &i1),
	b2 = normalized_constant_p(n2, &i2);

	if (! (b1 || b2)) return(make_normalized(is_normalized_complex, UU));

	return(make_normalized(is_normalized_linear,
	    vect_multiply(b1 ? normalized_linear(n2) : normalized_linear(n1), 
			  b1 ? i1 : i2)));
    }
    /* else */
    return(make_normalized(is_normalized_complex, UU));
}

normalized normalize_constant(c)
constant c;
{
    return((constant_int_p(c)) ? 
	   make_normalized(is_normalized_linear, 
			   vect_new(TCST, constant_int(c))) :
	   make_normalized(is_normalized_complex, UU));
}

normalized normalize_reference(r)
reference r;
{
    entity
	var = reference_variable(r);

    pips_assert("normalize_reference", entity_variable_p(var));

    return(entity_integer_scalar_p(var) ? 
	   make_normalized(is_normalized_linear, vect_new((Variable) var, 1)) :
	   make_normalized(is_normalized_complex, UU));
}

normalized normalize_call(c)
call c;
{
    normalized
	n = normalized_undefined;
    entity
	f = call_function(c);
    value 
	v = entity_initial(f);
    tag
	t = value_tag(v);

    switch (t) 
    {
    case is_value_intrinsic:
	n = normalize_intrinsic(f, call_arguments(c));
	break;
      case is_value_constant:
	n = normalize_constant(value_constant(v));
	break;
      case is_value_symbolic:
	n = normalize_constant(symbolic_constant(value_symbolic(v)));
	break;
      case is_value_unknown:
      case is_value_code:
	n = make_normalized(is_normalized_complex, UU);
	break;
      default:
	pips_error("normalize_call", "unexpected value tag (%d)\n", t);
    }

    return(n);
}

/*
static bool normalize_all_expressions_filter(e)
expression e;
{
    return(normalized_undefined_p(expression_normalized(e)));
}
*/

static void normalize_all_expressions_rewrite(e)
expression e;
{
    syntax 
	s = expression_syntax(e);
    tag 
	t = syntax_tag(s);

    if (!normalized_undefined_p(expression_normalized(e))) return;

    expression_normalized(e) = 
	t==is_syntax_range ? make_normalized(is_normalized_complex, UU) :
	t==is_syntax_reference ? normalize_reference(syntax_reference(s)) :
	t==is_syntax_call ? normalize_call(syntax_call(s)) :
	    (pips_error("normalize_all_expressions_rewrite",
			"undefined syntax tag (%d)\n", t), 
	     normalized_undefined);
}

void normalize_all_expressions_of(obj)
chunk *obj;
{
    gen_recurse(obj,
		expression_domain,
		gen_true,
		normalize_all_expressions_rewrite);
}

/* --------------------------------------------------------------------- 
 *
 * the first expressions encountered are normalized
 */

static bool normalize_first_expressions_filter(e)
expression e;
{
    if (normalized_undefined_p(expression_normalized(e)))
    {
	normalized
	    n = NORMALIZE_EXPRESSION(e);

	ifdebug(8)
	    fprintf(stderr, "[NormExpr] result (0x%x)\n", (int) n),
	    print_expression(e);
    }

    return(FALSE);
}

static void normalize_first_expressions_rewrite(x)
chunk *x;
{
    return;
}

void normalize_first_expressions_of(obj)
chunk *obj;
{
    gen_recurse(obj,
		expression_domain,
		normalize_first_expressions_filter,
		normalize_first_expressions_rewrite);
}

/*
 *   that is all
 */

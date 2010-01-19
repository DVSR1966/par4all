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
/* 

This file contains a set of functions to evaluate integer constant
expressions. The algorithm is built on a recursive analysis of the
expression structure. Lower level functions are called until basic atoms
are reached. The succes of basic atom evaluation depend on the atom
type:

reference: right now, the evaluation fails because we do not compute
predicates on variables.

call: a call to a user function is not evaluated. a call to an intrinsic
function is successfully evaluated if arguments can be evaluated. a call
to a constant function is evaluated if its basic type is integer.

range: a range is not evaluated. 
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "linear.h"

#include "genC.h"
#include "ri.h"
#include "misc.h"

#include "ri-util.h"

#include "operator.h"

value 
EvalExpression(expression e)
{
    return EvalSyntax(expression_syntax(e));
}

value 
EvalSyntax(syntax s)
{
  value v;
  
  switch (syntax_tag(s)) {
  case is_syntax_reference:
  case is_syntax_range:
    v = make_value(is_value_unknown, NIL);
    break;
  case is_syntax_call:
    v = EvalCall((syntax_call(s)));
    break;
  case is_syntax_cast:
    v = make_value(is_value_unknown, NIL);
    break;
  case is_syntax_sizeofexpression:
    v = EvalSizeofexpression((syntax_sizeofexpression(s)));
    break;
  case is_syntax_subscript:
  case is_syntax_application:
  case is_syntax_va_arg:
    v = MakeValueUnknown();
    break;
  default:
    fprintf(stderr, "[EvalExpression] Unexpected default case %d\n",
	    syntax_tag(s));
    abort();
  }

  return v;
}

/* only calls to constant, symbolic or intrinsic functions might be
 * evaluated. recall that intrinsic functions are not known.
 */
value
EvalCall(call c)
{
    value vout, vin;
    entity f;

    f = call_function(c);
    vin = entity_initial(f);

    if (value_undefined_p(vin))
	pips_internal_error("undefined value for %s\n", entity_name(f));

    switch (value_tag(vin)) {
      case is_value_intrinsic:
	vout = EvalIntrinsic(f, call_arguments(c));
	break;
      case is_value_constant:
	vout = EvalConstant((value_constant(vin)));
	break;
      case is_value_symbolic:
	vout = EvalConstant((symbolic_constant(value_symbolic(vin))));
	break;
      case is_value_unknown:
	/* it might be an intrinsic function */
	vout = EvalIntrinsic(f, call_arguments(c));
	break;
      case is_value_code:
	vout = make_value(is_value_unknown, NIL);
	break;
      default:
	fprintf(stderr, "[EvalCall] case default\n");
	abort();
    }

    return(vout);
}

value EvalSizeofexpression(sizeofexpression soe)
{
  type t = type_undefined;
  value v = value_undefined;
  _int i;

  if(sizeofexpression_expression_p(soe)) {
    expression e = sizeofexpression_expression(soe);

    t = expression_to_type(e);
  }
  else {
    t = sizeofexpression_type(soe);
  }

  i = type_memory_size(t);
  v = make_value_constant(make_constant_int(i));

  if(sizeofexpression_expression_p(soe))
    free_type(t);

  return v;
}

value 
EvalConstant(constant c) 
{
  //return make_value(is_value_constant, copy_constant(c));

    return((constant_int_p(c)) ?
	   make_value(is_value_constant, make_constant(is_constant_int,
						   (void*) constant_int(c))) :
	   make_value(is_value_constant, 
		      make_constant(is_constant_litteral, NIL)));
}

/* this function tries to evaluate a call to an intrinsic function.
right now, we only try to evaluate unary and binary intrinsic functions,
ie. fortran operators.

e is the intrinsic function.

la is the list of arguments.
*/

value 
EvalIntrinsic(entity e, list la)
{
  value v;
  int token;

  if ((token = IsUnaryOperator(e)) > 0)
    v = EvalUnaryOp(token, la);
  else if ((token = IsBinaryOperator(e)) > 0)
    v = EvalBinaryOp(token, la);
  else if ((token = IsNaryOperator(e)) > 0)
    v = EvalNaryOp(token, la);
  else if (ENTITY_CONDITIONAL_P(e))
    v = EvalConditionalOp(la);
  else
    v = make_value(is_value_unknown, NIL);

  return(v);
}

value EvalConditionalOp(list la)
{
  value vout, v1, v2, v3;
  _int arg1 = 0, arg2 = 0, arg3 = 0;
  bool failed = FALSE;

  pips_assert("Three arguments", gen_length(la)==3);

  v1 = EvalExpression(EXPRESSION(CAR(la)));
  if (value_constant_p(v1) && constant_int_p(value_constant(v1)))
    arg1 = constant_int(value_constant(v1));
  else
    failed = TRUE;

  v2 = EvalExpression(EXPRESSION(CAR(CDR(la))));
  if (value_constant_p(v2) && constant_int_p(value_constant(v2)))
    arg2 = constant_int(value_constant(v2));
  else
    failed = TRUE;

  v3 = EvalExpression(EXPRESSION(CAR(CDR(CDR(la)))));
  if (value_constant_p(v3) && constant_int_p(value_constant(v3)))
    arg3 = constant_int(value_constant(v3));
  else
    failed = TRUE;

  if(failed)
    vout = make_value(is_value_unknown, NIL);
  else
    vout = make_value(is_value_constant,
		      make_constant(is_constant_int, (void *) (arg1? arg2: arg3)));

  free_value(v1);
  free_value(v2);
  free_value(v3);

  return vout;
}


value 
EvalUnaryOp(int t, list la)
{
  value vout, v;
  int arg;

  assert(la != NIL);
  v = EvalExpression(EXPRESSION(CAR(la)));
  if (value_constant_p(v) && constant_int_p(value_constant(v)))
    arg = constant_int(value_constant(v));
  else
    return(v);

  if (t == MINUS) {
    constant_int(value_constant(v)) = -arg;
    vout = v;
  }
  else if (t == PLUS) {
    constant_int(value_constant(v)) = arg;
    vout = v;
  }
  else if (t == NOT) {
    constant_int(value_constant(v)) = arg!=0;
    vout = v;
  }
  else {
    free_value(v);
    vout = make_value(is_value_unknown, NIL);
  }

  return(vout);
}

value 
EvalBinaryOp(int t, list la)
{
  value v;
  int argl, argr;

  pips_assert("non empty list", la != NIL);

  v = EvalExpression(EXPRESSION(CAR(la)));
  if (value_constant_p(v) && constant_int_p(value_constant(v))) {
    argl = constant_int(value_constant(v));
    free_value(v);
  }
  else
    return(v);

  la = CDR(la);

  pips_assert("non empty list", la != NIL);
  v = EvalExpression(EXPRESSION(CAR(la)));

  if (value_constant_p(v) && constant_int_p(value_constant(v))) {
    argr = constant_int(value_constant(v));
  }
  else
    return(v);

  switch (t) {
  case MINUS:
    constant_int(value_constant(v)) = argl-argr;
    break;
  case PLUS:
    constant_int(value_constant(v)) = argl+argr;
    break;
  case STAR:
    constant_int(value_constant(v)) = argl*argr;
    break;
  case SLASH:
    if (argr != 0)
      constant_int(value_constant(v)) = argl/argr;
    else {
      fprintf(stderr, "[EvalBinaryOp] zero divide\n");
      abort();
    }	
    break;
  case MOD:
    if (argr != 0)
      constant_int(value_constant(v)) = argl%argr;
    else {
      fprintf(stderr, "[EvalBinaryOp] zero divide in modulo\n");
      abort();
    }
    break;
  case POWER:
    if (argr >= 0)
      constant_int(value_constant(v)) = ipow(argl,argr);
    else {
      free_value(v);
      v = make_value(is_value_unknown, NIL);
    }
    break;
  case EQ:
    constant_int(value_constant(v)) = argl==argr;
    break;
  case NE:
    constant_int(value_constant(v)) = argl!=argr;
    break;
  case EQV:
    constant_int(value_constant(v)) = argl==argr;
    break;
  case NEQV:
       constant_int(value_constant(v)) = argl!=argr;
   break;
  case GT:
       constant_int(value_constant(v)) = argl>argr;
   break;
  case LT:
       constant_int(value_constant(v)) = argl<argr;
   break;
  case GE:
       constant_int(value_constant(v)) = argl>=argr;
   break;
   /* OK for Fortran Logical? */
  case OR:
       constant_int(value_constant(v)) = (argl!=0)||(argr!=0);
   break;
  case AND:
       constant_int(value_constant(v)) = (argl!=0)&&(argr!=0);
   break;
  case BITWISE_OR:
       constant_int(value_constant(v)) = argl|argr;
   break;
  case BITWISE_AND:
       constant_int(value_constant(v)) = argl&argr;
   break;
  case BITWISE_XOR:
       constant_int(value_constant(v)) = argl^argr;
   break;
  case LEFT_SHIFT:
       constant_int(value_constant(v)) = argl<<argr;
   break;
   case RIGHT_SHIFT:
       constant_int(value_constant(v)) = argl>>argr;
   break;
 default:
    free_value(v);
    v = make_value(is_value_unknown, NIL);
  }

  return(v);
}

value 
EvalNaryOp(int t, list la)
{
    value v = value_undefined;
    value w = value_undefined;
    int new_arg = 0;
    bool first_arg_p = TRUE;

    /* 2 operands at least are needed */
    assert(la != NIL && CDR(la) != NIL);

    MAP(EXPRESSION, e, {
	v = EvalExpression(e);
	if (value_constant_p(v) && constant_int_p(value_constant(v))) {
	    new_arg = constant_int(value_constant(v));
	    if (first_arg_p) {
		first_arg_p = FALSE;
		w = v;
	    }
	    else {
		switch(t) {
		case MAXIMUM:
		    constant_int(value_constant(w))= MAX(constant_int(value_constant(w)),
							 new_arg);
		    break;
		case MINIMUM:
		    constant_int(value_constant(w))= MIN(constant_int(value_constant(w)),
							 new_arg);
		    break;
		default:
		    return v;
		}
		free_value(v);
	    }
	}
	else
	    return(v);
    }, la);

    return(w);
}

int 
IsUnaryOperator(entity e)
{
	int token;
	string n = entity_local_name(e);

	if (same_string_p(n, UNARY_MINUS_OPERATOR_NAME))
		token = MINUS;
	else if (same_string_p(n, UNARY_PLUS_OPERATOR_NAME))
		token = PLUS;
	else if (same_string_p(n, NOT_OPERATOR_NAME)
		 || same_string_p(n, C_NOT_OPERATOR_NAME))
		token = NOT;
	else
		token = -1;

	return(token);
}

/* FI: These string constants are defined in ri-util.h */
int 
IsBinaryOperator(entity e)
{
	int token;
	string n = entity_local_name(e);

	if      (same_string_p(n, MINUS_OPERATOR_NAME)
		 || same_string_p(n, MINUS_C_OPERATOR_NAME))
		token = MINUS;
	else if (same_string_p(n, PLUS_OPERATOR_NAME)
		 || same_string_p(n, PLUS_C_OPERATOR_NAME))
		token = PLUS;
	else if (same_string_p(n, MULTIPLY_OPERATOR_NAME))
		token = STAR;
	else if (same_string_p(n, DIVIDE_OPERATOR_NAME))
		token = SLASH;
	else if (same_string_p(n, POWER_OPERATOR_NAME))
		token = POWER;
	else if (same_string_p(n, MODULO_OPERATOR_NAME)
		 || same_string_p(n, C_MODULO_OPERATOR_NAME))
		token = MOD;
	else if (same_string_p(n, EQUAL_OPERATOR_NAME)
		 || same_string_p(n, C_EQUAL_OPERATOR_NAME))
		token = EQ;
	else if (same_string_p(n, NON_EQUAL_OPERATOR_NAME)
		 || same_string_p(n, C_NON_EQUAL_OPERATOR_NAME))
		token = NE;
	else if (same_string_p(n, MODULO_OPERATOR_NAME)
		 || same_string_p(n, C_MODULO_OPERATOR_NAME))
		token = EQV;
	else if (same_string_p(n, EQUIV_OPERATOR_NAME))
		token = NEQV;
	else if (same_string_p(n, GREATER_THAN_OPERATOR_NAME)
		 || same_string_p(n, C_MODULO_OPERATOR_NAME))
		token = GT;
	else if (same_string_p(n, LESS_THAN_OPERATOR_NAME)
		 || same_string_p(n, C_LESS_THAN_OPERATOR_NAME))
		token = LT;
	else if (same_string_p(n, GREATER_OR_EQUAL_OPERATOR_NAME)
		 || same_string_p(n, C_GREATER_OR_EQUAL_OPERATOR_NAME))
		token = GE;
	else if (same_string_p(n, LESS_OR_EQUAL_OPERATOR_NAME)
		 || same_string_p(n, C_LESS_OR_EQUAL_OPERATOR_NAME))
		token = LE;
	else if (same_string_p(n, OR_OPERATOR_NAME)
		 || same_string_p(n, C_OR_OPERATOR_NAME))
		token = OR;
	else if (same_string_p(n, AND_OPERATOR_NAME)
		 || same_string_p(n, C_AND_OPERATOR_NAME))
		token = AND;
	else if (same_string_p(n, BITWISE_AND_OPERATOR_NAME))
		token = BITWISE_AND;
	else if (same_string_p(n, BITWISE_OR_OPERATOR_NAME))
		token = BITWISE_OR;
	else if (same_string_p(n, BITWISE_XOR_OPERATOR_NAME))
		token = BITWISE_XOR;
	else if (same_string_p(n, LEFT_SHIFT_OPERATOR_NAME))
		token = LEFT_SHIFT;
	else if (same_string_p(n, RIGHT_SHIFT_OPERATOR_NAME))
		token = RIGHT_SHIFT;
	else if (same_string_p(entity_local_name(e), IMPLIED_COMPLEX_NAME) ||
		 same_string_p(entity_local_name(e), IMPLIED_DCOMPLEX_NAME))
	        token = CAST_OP;
	else
		token = -1;

	return(token);
}

int 
IsNaryOperator(entity e)
{
	int token;

	if (strcmp(entity_local_name(e), MIN0_OPERATOR_NAME) == 0)
		token = MINIMUM;
	else if (strcmp(entity_local_name(e), MAX0_OPERATOR_NAME) == 0)
		token = MAXIMUM;
	else if (strcmp(entity_local_name(e), MIN_OPERATOR_NAME) == 0)
		token = MINIMUM;
	else if (strcmp(entity_local_name(e), MAX_OPERATOR_NAME) == 0)
		token = MAXIMUM;
	else
		token = -1;

	return token;
}

/* FI: such a function should exist in Linear/arithmetique */
int 
ipow(int vg, int vd)
{
	int i = 1;

	assert(vd >= 0);

	while (vd-- > 0)
		i *= vg;

	return(i);
}


/*
  computes the value of an integer expression.
  returns TRUE if an integer value has been found and placed in pval.
  returns FALSE otherwise.
*/
bool
expression_integer_value(expression e, int * pval)
{
    bool is_int = FALSE;
    value v = EvalExpression(e);

    if (value_constant_p(v) && constant_int_p(value_constant(v))) {
        *pval = constant_int(value_constant(v));
        is_int = TRUE;
    }

    free_value(v);
    return is_int;
}


/* Return TRUE iff the expression has an integer value and this value is
   negative.
*/
bool
expression_negative_integer_value_p(expression e) {
  int v;
  return expression_integer_value(e, &v) && (v < 0);
}


/* The range count only can be evaluated if the three range expressions
 * are constant and if the increment is non zero. On failure, a zero
 * count is returned. See also SizeOfRange().
 */
bool
range_count(range r, int * pcount)
{
    bool success = FALSE;
    int l, u, inc;

    if(expression_integer_value(range_lower(r), &l)
       && expression_integer_value(range_upper(r), &u)
       && expression_integer_value(range_increment(r), &inc)
       && inc != 0 ) {

	if(inc<0) {
	    * pcount = ((l-u)/(-inc))+1;
	}
	else /* inc>0 */ {
	    * pcount = ((u-l)/inc)+1;
	}

	if(* pcount < 0)
	    *pcount = 0;

	success = TRUE;
    }
    else {
	* pcount = 0;
	success = FALSE;
    }

    return success;
}


/* returns TRUE if v is not NULL and is constant */
/* I make it "static" because it conflicts with a Linear library function.
 * Both functions have the same name but a slightly different behavior.
 * The Linear version returns 0 when a null vector is passed as argument.
 * Francois Irigoin, 16 April 1990
 */
static bool
vect_const_p(Pvecteur v)
{
    pips_assert("vect_const_p", v != NULL);
    return vect_size(v) == 1 && value_notzero_p(vect_coeff(TCST, v));
}

/* 
  returns a Pvecteur equal to (*pv1) * (*pv2) if this product is 
  linear or NULL otherwise. 
  the result is built from pv1 or pv2 and the other vector is removed.
*/
Pvecteur 
vect_product(Pvecteur * pv1, Pvecteur * pv2)
{
    Pvecteur vr;

    if (vect_const_p(*pv1)) {
        vr = vect_multiply(*pv2, vect_coeff(TCST, *pv1));
        vect_rm(*pv1);
    }
    else if (vect_const_p(*pv2)) {
        vr = vect_multiply(*pv1, vect_coeff(TCST, *pv2));
        vect_rm(*pv2);
    }
    else {
        vr = NULL;
        vect_rm(*pv1);
        vect_rm(*pv2);
    }

    *pv1 = NULL;
    *pv2 = NULL;

    return(vr);
}

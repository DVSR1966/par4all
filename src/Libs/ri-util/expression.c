 /* 
  * $Id$
  *
  * Functions for the expressions
  *
  * Yi-Qing YANG, Lei ZHOU, Francois IRIGOIN, Fabien COELHO
  *
  * 12, Sep, 1991
  *
  * Alexis PLATONOFF, Sep. 25, 1995 : I have added some usefull functions from
  * static_controlize/utils.c
  */

#include <stdio.h>
#include <string.h>

#include "linear.h"

#include "genC.h"
#include "misc.h"
#include "ri.h"
#include "text.h"
#include "text-util.h"

#include "arithmetique.h"

#include "ri-util.h"

/**************************************************** FORTRAN STRING COMPARE */

/* quite lazy... 
 */
static string actual_fortran_string_to_compare(string fs, int * plength)
{
  string s = fs;
  int len;

  /* skip TOP-LEVEL header */
  if (strncmp(s, TOP_LEVEL_MODULE_NAME, strlen(TOP_LEVEL_MODULE_NAME))==0)
    s += strlen(TOP_LEVEL_MODULE_NAME);

  /* skip : header */
  if (strncmp(s, MODULE_SEP_STRING, strlen(MODULE_SEP_STRING))==0)
    s += strlen(MODULE_SEP_STRING);

  len = strlen(s);

  /* skip surrounding quotes */
  if (len>=2 &&
      ((s[0]=='\'' && s[len-1]=='\'') || (s[0]=='"' && s[len-1]=='"')))
  {
    s++; 
    len -= 2;
  }

  /* skip trailing *spaces* (are these blanks?) if any. */
  while (len>0 && s[len-1]==' ') 
    len--;

  *plength = len;
  return s;
}

/* compare pips fortran string constants from the fortran point of view.
 *
 * as of 3.1 and 6.3.5 of the Fortran 77 standard, the character order 
 * is not fully specified. It states:
 *  - A < B < C ... < Z
 *  - 0 < 1 < 2 ... < 9
 *  - blank < 0 
 *  - blank < A
 *  - 9 < A  *OR* Z < 0
 * since these rules are ascii compatible, we'll take ascii.
 * in practice, this may be implementation dependent?
 * 
 * @param fs1 constant fortran string (entity name is fine)
 * @param fs2 constant fortran string (entity name is fine)
 * @return -n 0 +n depending on < == >, n first differing char.
 */
int fortran_string_compare(string fs1, string fs2)
{
  int l1, l2, i, c = 0;
  string s1, s2;

  /* skip headers, trailers... */
  s1 = actual_fortran_string_to_compare(fs1, &l1);
  s2 = actual_fortran_string_to_compare(fs2, &l2);

  /* collating sequence comparison. */
  for (i=0; c==0 && i<l1 && i<l2; i++)
  {
    if (s1[i] < s2[i]) c = -i-1;
    if (s1[i] > s2[i]) c = i+1;
  }

  /* equal string header case. */
  if (c==0 && l1!=l2)
    c = (l1<l2)? -l1-1: l2+1;

  return c;
}

/********************************************************************* BASIC */

/*  a BASIC tag is returned for the expression
 *  this is a preliminary version. should be improved.
 *  was in HPFC.
 */
tag suggest_basic_for_expression(e)
expression e;
{
    tag	result = basic_tag(expression_basic(e));

    if (result==is_basic_overloaded)
    {
	syntax s = expression_syntax(e);

	/*  must be a call
	 */
	assert(syntax_call_p(s));

	if (ENTITY_RELATIONAL_OPERATOR_P(call_function(syntax_call(s))))
	    result = is_basic_logical;
	else
	{
	    /* else some clever analysis could be done
	     */
	    pips_user_warning("an overloaded is turned into an int...\n");
	    result = is_basic_int;
	}
    }

    return result;
}

expression expression_dup(expression ex)
{
    return copy_expression(ex);
}

syntax syntax_dup(syntax s)
{
    return copy_syntax(s);
}

normalized normalized_dup(normalized n)
{
    return copy_normalized(n);
}

reference reference_dup(reference r)
{
    return copy_reference(r);
}

range range_dup(range r)
{
    return copy_range(r);
}

call call_dup(call c)
{
    return copy_call(c);
}

expression expression_mult(ex)
expression ex;
{
    pips_error("expression_mult", "not implemented\n");
    return ex;
}

/* if v is a constant, returns a constant call.
 * if v is a variable, returns a reference to v.
 */
expression entity_to_expression(entity e)
{
 if(entity_symbolic_p(e))
   return call_to_expression(make_call(e, NIL));
 else if (entity_constant_p(e))
   return call_to_expression(make_call(e, NIL));
 else /* should be a scalar variable! */
   return reference_to_expression(make_reference(e, NIL));
}

/*
 * remarks: why is the default to normalized_complex~? 
 * should be undefined, OR normalized if possible.
 * I put normalize_reference... FC 27/09/93 and june 94
 */
expression reference_to_expression(r)
reference r;
{
    expression e;
    syntax s = make_syntax(is_syntax_reference, r);

    e = make_expression(s, normalize_reference(r));

    return e;
}

expression MakeBinaryCall(f, eg, ed)
entity f;
expression eg, ed;
{
  call c =  make_call(f, CONS(EXPRESSION, eg, CONS(EXPRESSION, ed, NIL)));

  return(make_expression(make_syntax(is_syntax_call, c),
			 normalized_undefined));
}

expression call_to_expression(c)
call c;
{
    return make_expression(make_syntax(is_syntax_call, c),
			   normalized_undefined);
}

expression make_call_expression(e, l)
entity e;
list l;
{
    return(call_to_expression(make_call(e, l)));
}

expression MakeTernaryCallExpr(f, e1, e2, e3)
entity f;
expression e1,e2,e3;
{
    return(make_call_expression(f,
	       CONS(EXPRESSION, e1,
	       CONS(EXPRESSION, e2,
	       CONS(EXPRESSION, e3,
		    NULL)))));
}

/* predicates on expressions */

bool expression_call_p(e)
expression e;
{
    return(syntax_call_p(expression_syntax(e)));
}

bool expression_reference_p(e)
expression e;
{
    return(syntax_reference_p(expression_syntax(e)));
}

bool is_expression_reference_to_entity_p(expression e, entity v)
{
  bool is_e_reference_to_v = FALSE;

  if(expression_reference_p(e)) {
    reference r = syntax_reference(expression_syntax(e));

    is_e_reference_to_v = (reference_variable(r)==v);
  }
  return is_e_reference_to_v;
}

/* This function returns TRUE, if there exists an equal expression in the list
 *                       FALSE, otherwise 
*/

bool same_expression_in_list_p(expression e, list le)
{
  MAP(EXPRESSION, f, if (same_expression_p(e,f)) return TRUE, le);
  return FALSE;
}

bool logical_operator_expression_p(expression e)
{
  /* Logical operators are : .NOT.,.AND.,.OR.,.EQV.,.NEQV.*/
  if (expression_call_p(e))
    {
      entity op = call_function(syntax_call(expression_syntax(e)));
      if (ENTITY_AND_P(op) || ENTITY_OR_P(op) || ENTITY_NOT_P(op) || 
	  ENTITY_EQUIV_P(op) ||ENTITY_NON_EQUIV_P(op))
	return TRUE;
      return FALSE;
    }
  return FALSE;
}

bool relational_expression_p(expression e)
{
  /* A relational expression is a call whose function is either one of the following :
   * .LT.,.LE.,.EQ.,.NE.,.GT.,.GE. */
  if (expression_call_p(e))
    {
      entity op = call_function(syntax_call(expression_syntax(e)));
      if (ENTITY_RELATIONAL_OPERATOR_P(op)) 
	return TRUE;
      return FALSE;
    }
  return FALSE;
}

bool integer_expression_p(expression e)
{
  basic b = basic_of_expression(e);
  bool integer_p = basic_int_p(b);

  free_basic(b);
  return integer_p;
}

bool logical_expression_p(expression e)
{  
  /* A logical expression is either one of the following:
   * - a logical constant
   * - the symbolic name of a logical constant
   * - a logical variable name 
   * - a logical array element name
   * - a logical function reference
   * - a relational expression (.LT.,.LE.,.EQ.,.NE.,.GT.,.GE.)
   * - is formed by combining together one or more of the above 
   *   entities using parentheses and the logical operators 
   *   .NOT.,.AND.,.OR.,.EQV.,.NEQV. */

  /* NN:  In fact, I didn't use the PIPS function : basic_of_expression because of 2 reasons : 
   * - the function basic_of_intrinsic use the macro : ENTITY_LOGICAL_OPERATOR_P 
   *   which is not like the Fortran Standard definition (the above comments)
   * - the case where an expression is a range is not considered here for a 
   *   logical expression */
  
  syntax s = expression_syntax(e);
  basic b;
  entity func;

  debug(2, "logical expression", "\n");
  
  switch(syntax_tag(s)) {
  case is_syntax_reference:
    {        
      b = variable_basic(type_variable(entity_type(reference_variable(syntax_reference(s)))));
      if (basic_logical_p(b))
	return TRUE;
      return FALSE;     
    }
  case is_syntax_call:
    {
      if (operator_expression_p(e,TRUE_OPERATOR_NAME) || 
	  operator_expression_p(e,FALSE_OPERATOR_NAME) ||
	  relational_expression_p(e)|| 
	  logical_operator_expression_p(e) )
	return TRUE;
      func = call_function(syntax_call(expression_syntax(e)));
      b = variable_basic(type_variable(functional_result(type_functional(entity_type(func)))));
      if (basic_logical_p(b)) return TRUE;   

      /* The case of symbolic name of a logical constant is not treated here */

      return FALSE;
    }
  case is_syntax_range: 
    return FALSE;
  default: pips_error("basic_of_expression", "Bad syntax tag");
    return FALSE;
  }
  
  debug(2, "logical expression", " ends\n");  
}


/* This function returns:
 *                        1 , if e is a relational expression that is always TRUE
 *                       -1 , if e is a relational expression that is always FALSE
 *                        0 , otherwise */

int trivial_expression_p(expression e)
{  
  if (relational_expression_p(e))
    {
      /* If e is a relational expression*/
      list args = call_arguments(syntax_call(expression_syntax(e)));
      expression e1 =  EXPRESSION(CAR(args));
      expression e2 = EXPRESSION(CAR(CDR(args)));
      normalized n1,n2;
      entity op;
      if (expression_undefined_p(e1) ||expression_undefined_p(e2) ) return 0;
      n1 = NORMALIZE_EXPRESSION(e1);
      n2 = NORMALIZE_EXPRESSION(e2);
      op = call_function(syntax_call(expression_syntax(e)));
     
      ifdebug(3) {
	fprintf(stderr, "Normalizes of  expression:");
	print_expression(e);
	print_normalized(n1);
	print_normalized(n2);
      }
 
      if (normalized_linear_p(n1) && normalized_linear_p(n2))
	{
	  Pvecteur v1 = normalized_linear(n1);
	  Pvecteur v2 = normalized_linear(n2);
	  Pvecteur v = vect_substract(v1,v2);
	   
	  /* The test if an expression is trivial (always TRUE or FALSE) or not 
	   * depends on the operator of the expression : 
	   * (op= {<=,<,>=,>,==,!=}) so we have to treat each different case */
	  
	  if (vect_constant_p(v))
	    {
	      if (ENTITY_NON_EQUAL_P(op))
		{
		  /* Expression :  v != 0 */	
		  if (VECTEUR_NUL_P(v)) return -1;
		  if (value_zero_p(val_of(v))) return -1;
		  if (value_notzero_p(val_of(v))) return 1;	
		}
	      if (ENTITY_EQUAL_P(op))
		{
		  /* Expression :  v == 0 */
		  if (VECTEUR_NUL_P(v)) return 1;
		  if (value_zero_p(val_of(v))) return 1;
		  if (value_notzero_p(val_of(v))) return -1;	
		}	
	      if (ENTITY_GREATER_OR_EQUAL_P(op))
		{
		  /* Expression :  v >= 0 */
		  if (VECTEUR_NUL_P(v)) return 1;
		  if (value_posz_p(val_of(v))) return 1;
		  if (value_neg_p(val_of(v))) return -1;	
		}	
	      if (ENTITY_LESS_OR_EQUAL_P(op))
		{
		  /* Expression :  v <= 0 */
		  if (VECTEUR_NUL_P(v)) return 1;
		  if (value_negz_p(val_of(v))) return 1;
		  if (value_pos_p(val_of(v))) return -1;	
		}	
	      if (ENTITY_LESS_THAN_P(op))
		{
		  /* Expression :  v < 0 */
		  if (VECTEUR_NUL_P(v)) return -1;
		  if (value_neg_p(val_of(v))) return 1;
		  if (value_posz_p(val_of(v))) return -1;	
		}	
	      if (ENTITY_GREATER_THAN_P(op))
		{
		  /* Expression :  v > 0 */
		  if (VECTEUR_NUL_P(v)) return -1;
		  if (value_pos_p(val_of(v))) return 1;
		  if (value_negz_p(val_of(v))) return -1;	
		}	
	    }	
	  return 0;
	}
      return 0;
    }
  return 0;
}



bool expression_implied_do_p(e)
expression e ;
{
    if (expression_call_p(e)) {
	call c = syntax_call(expression_syntax(e));
	entity e = call_function(c);

	return(strcmp(entity_local_name(e), IMPLIED_DO_NAME) == 0);
    }

    return(FALSE);
}

bool expression_list_directed_p(e)
expression e ;
{
    if (expression_call_p(e)) {
	call c = syntax_call(expression_syntax(e));
	entity e = call_function(c);

	return(strcmp(entity_local_name(e), LIST_DIRECTED_FORMAT_NAME) == 0);
    }

    return(FALSE);
}

/* More extensive than next function */
bool extended_integer_constant_expression_p(e)
expression e;
{
  value v = EvalExpression(e);
  bool ice_p = FALSE;

  if(value_constant_p(v)) {
    constant c = value_constant(v);

    ice_p = constant_int_p(c);
  }
  return ice_p;
}

/* positive integer constant expression: call to a positive constant
   or to a sum of positive integer constant expressions (much too
   restrictive, but enough for the source codes submitted to PIPS up
   to now).

   Likely to fail and need further extension if subtraction and
   multiplication are used as probably allowed by C standard.

   NormalizeExpression() could be used instead, as it is in fact to compute
   the value of the expression.

   Use previous function instead of this one, and -1 will be a constant...
*/
bool integer_constant_expression_p(e)
expression e;
{
  syntax s = expression_syntax(e);
  bool ice = FALSE;

  if(syntax_call_p(s)) {
    call c = syntax_call(s);
    entity cst = call_function(c);
    list args = call_arguments(c);
    int i;

    if(integer_constant_p(cst, &i)) {
      ice = TRUE;
    }
    else if(integer_symbolic_constant_p(cst, &i)) {
      ice = TRUE;
    }
    else if(ENTITY_PLUS_P(cst)||ENTITY_PLUS_C_P(cst)) {
      expression e1 = EXPRESSION(CAR(args));
      expression e2 = EXPRESSION(CAR(CDR(args)));

      ice = integer_constant_expression_p(e1) && integer_constant_expression_p(e2);
    }
  }

  return ice;
}

bool signed_integer_constant_expression_p(e)
expression e;
{
  if(!integer_constant_expression_p(e)) {
    syntax s = expression_syntax(e);

    if(syntax_call_p(s)) {
	call c = syntax_call(s);
	entity um = call_function(c);

	if(um == gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
							 UNARY_MINUS_OPERATOR_NAME),
				    entity_domain)) {
	  expression e2 = EXPRESSION(CAR(call_arguments(c)));

	  return integer_constant_expression_p(e2);
	}
    }
    return FALSE;
  }
  else {
    return TRUE;
  }
}

/* The expression may be complicated but all its leaves are constants or
   para,eters. It evaluates to a signed integer constant. I am too lazy to
   fully implement it as I should and I only take care of affine
   expressions (Francois). */
bool expression_with_constant_signed_integer_value_p(e)
expression e;
{
  normalized ne = NORMALIZE_EXPRESSION(e);
  bool constant_p = FALSE;

  if(normalized_linear_p(ne)) {
    Pvecteur ve = normalized_linear(ne);
    /* No vecteur_constant_p() in linear library? */
    constant_p = VECTEUR_NUL_P(ve)
      || (term_cst(ve) && VECTEUR_UNDEFINED_P(vecteur_succ(ve)));
  }

  return constant_p;
}

bool modulo_expression_p(e)
expression e;
{
    return operator_expression_p(e, MODULO_OPERATOR_NAME);
}

bool divide_expression_p(e)
expression e;
{
    return operator_expression_p(e, DIVIDE_OPERATOR_NAME);
}

bool power_expression_p(e)
expression e;
{
    return operator_expression_p(e, POWER_OPERATOR_NAME);
}

bool abs_expression_p(e)
expression e;
{
    return operator_expression_p(e, ABS_OPERATOR_NAME);
}

bool iabs_expression_p(e)
expression e;
{
    return operator_expression_p(e, IABS_OPERATOR_NAME);
}

bool dabs_expression_p(e)
expression e;
{
    return operator_expression_p(e, DABS_OPERATOR_NAME);
}

bool cabs_expression_p(e)
expression e;
{
    return operator_expression_p(e, CABS_OPERATOR_NAME);
}

bool min0_expression_p(e)
expression e;
{
    return operator_expression_p(e, MIN0_OPERATOR_NAME) ||
	operator_expression_p(e, MIN_OPERATOR_NAME);
}

bool max0_expression_p(e)
expression e;
{
    return operator_expression_p(e, MAX0_OPERATOR_NAME) ||
	operator_expression_p(e, MAX_OPERATOR_NAME);
}

bool user_function_call_p(e)
expression e;
{
    syntax s = expression_syntax(e);
    bool user_function_call_p = FALSE;

    if(syntax_call_p(s)) {
	call c = syntax_call(s);
	entity f = call_function(c);
	value v = entity_initial(f);
	user_function_call_p = value_code_p(v);
    }
    else {
	user_function_call_p = FALSE;
    }

    return user_function_call_p;
}

bool operator_expression_p(e, op_name)
expression e;
string op_name;
{
    syntax s = expression_syntax(e);

    if(syntax_call_p(s)) {
	call c = syntax_call(s);
	entity op = call_function(c);

	return strcmp(op_name, entity_local_name(op)) == 0;
    }
    else
	return FALSE;
}

expression  make_true_expression()
{ 
  return make_call_expression(MakeConstant(TRUE_OPERATOR_NAME,is_basic_logical),NIL);
}

expression make_false_expression() 
{
  return make_call_expression(MakeConstant(FALSE_OPERATOR_NAME,is_basic_logical),NIL);
}

bool true_expression_p(expression e) 
{
  return operator_expression_p(e,TRUE_OPERATOR_NAME);
}

bool false_expression_p(expression e) 
{
  return operator_expression_p(e,FALSE_OPERATOR_NAME);
}

/* boolean unbounded_dimension_p(dim)
 * input    : a dimension of an array entity.
 * output   : TRUE if the last dimension is unbounded (*),
 *            FALSE otherwise.
 * modifies : nothing
 * comment  : 
 */
boolean unbounded_dimension_p(dim)
dimension dim;
{
    syntax dim_synt = expression_syntax(dimension_upper(dim));
    boolean res = FALSE;
    
    if (syntax_call_p(dim_synt)) {
	string dim_nom = entity_local_name(call_function(syntax_call(dim_synt)));
	
	if (same_string_p(dim_nom, UNBOUNDED_DIMENSION_NAME))
	    res = TRUE;
    }
	
    return(res);
	
}


expression find_ith_argument(args, n)
list args;
int n;
{
    int i;
    pips_assert("find_ith_argument", n > 0);

    for(i=1; i<n && !ENDP(args); i++, POP(args))
	;
    if(i==n && !ENDP(args))
	return EXPRESSION(CAR(args));
    else
	return expression_undefined;
}

/* find_ith_expression() is obsolet; use find_ith_argument() instead */
expression find_ith_expression(le, r)
list le;
int r;
{
    /* the first element is one */
    /* two local variables, useless but for debugging */
    list cle;
    int i;

    pips_assert("find_ith_expression", r > 0);

    for(i=r, cle=le ; i>1 && !ENDP(cle); i--, POP(cle))
	;

    if(ENDP(cle))
	pips_error("find_ith_expression", 
		   "not enough elements in expresion list\n");

    return EXPRESSION(CAR(cle));
}

/* transform an int into an expression and generate the corresponding
   entity if necessary; it is not clear if strdup() is always/sometimes
   necessary and if a memory leak occurs; wait till syntax/expression.c
   is merged with ri-util/expression.c 

   Negative constants do not seem to be included in PIPS internal
   representation.
  */
expression
int_to_expression(intptr_t i)
{
  char constant_name[3*sizeof(i)];
    expression e;

    (void) sprintf(constant_name,"%td", i >= 0 ? i : -i);
    e = MakeIntegerConstantExpression(strdup(constant_name));
    if(i<0) {
	entity um = entity_intrinsic(UNARY_MINUS_OPERATOR_NAME);
	e = MakeUnaryCall(um, e);
    }
    return e;
}

/* added interface for linear stuff.
 * it is not ok if Value is not an int, but if Value is changed
 * sometime, I guess code that use this function will not need
 * any change.
 * FC.
 */
expression Value_to_expression(v)
Value v;
{
    return(int_to_expression(VALUE_TO_INT(v)));
}

/* conversion of an expression into a list of references; references are
   appended to list lr as they are encountered; array references are
   added before their index expressions are scanned;

   references to functions and constants (which are encoded as null-ary
   functions) are not recorded 
*/
list expression_to_reference_list(e, lr)
expression e;
list lr;
{
    syntax s = expression_syntax(e);
    lr = syntax_to_reference_list(s, lr);
    return lr;
}

list syntax_to_reference_list(s, lr)
syntax s;
list lr;
{
    switch(syntax_tag(s)) {
    case is_syntax_reference:
	lr = gen_nconc(lr, CONS(REFERENCE, syntax_reference(s), NIL));
	MAPL(ce, {
	    expression e = EXPRESSION(CAR(ce));
	    lr = expression_to_reference_list(e, lr);
	    },
	     reference_indices(syntax_reference(s)));
	break;
    case is_syntax_range:
	lr = expression_to_reference_list(range_lower(syntax_range(s)), lr);
	lr = expression_to_reference_list(range_upper(syntax_range(s)), lr);
	lr = expression_to_reference_list(range_increment(syntax_range(s)),
					  lr);
	break;
    case is_syntax_call:
	MAPL(ce, {
	    expression e = EXPRESSION(CAR(ce));
	    lr = expression_to_reference_list(e, lr);
	    },
	     call_arguments(syntax_call(s)));
	break;
    default:
	pips_error("syntax_to_reference_list","illegal tag %d\n", 
		   syntax_tag(s));

    }
    return lr;
}
       
/* no file descriptor is passed to make is easier to use in a debugging
   stage.
   Do not make macros of those printing functions */

void print_expression(e)
expression e;
{
    normalized n;

    if(e==expression_undefined)
	(void) fprintf(stderr,"EXPRESSION UNDEFINED\n");
    else {
	(void) fprintf(stderr,"syntax = ");
	print_syntax(expression_syntax(e));
	(void) fprintf(stderr,"\nnormalized = ");
	if((n=expression_normalized(e))!=normalized_undefined)
	    print_normalized(n);
	else
	    (void) fprintf(stderr,"NORMALIZED UNDEFINED\n");
    }
}

void print_expressions(list le)
{

  MAP(EXPRESSION, e , {
    print_expression(e);
      },
    le);

}

void print_syntax_expressions(list le)
{

  MAP(EXPRESSION, e , {
    print_syntax(expression_syntax(e));
    if(!ENDP(CDR(le))) {
	(void) fprintf(stderr, ", ");
    }
      },
    le);

}

void print_syntax(s)
syntax s;
{
    print_words(stderr,words_syntax(s));
}

void print_reference(r)
reference r;
{
    print_words(stderr,words_reference(r));
}

void print_reference_list(lr)
list lr;
{
    if(ENDP(lr))
	fputs("NIL", stderr);
    else
	MAPL(cr,
	 {
	     reference r = REFERENCE(CAR(cr));
	     entity e = reference_variable(r);
	     (void) fprintf(stderr,"%s, ", entity_local_name(e));
	 },
	     lr);

    (void) putc('\n', stderr);
}

void print_references(list rl)
{
  print_reference_list(rl);
}

void print_normalized(n)
normalized n;
{
    if(normalized_complex_p(n))
	(void) fprintf(stderr,"COMPLEX\n");
    else
	/* should be replaced by a call to expression_fprint() if it's
	   ever added to linear library */
	vect_debug((Pvecteur)normalized_linear(n));
}

bool expression_equal_p(e1, e2)
expression e1;
expression e2;
{
  syntax s1, s2;
  
  /* Add expression_undefined tests to avoid segmentation fault */

  if (expression_undefined_p(e1) && expression_undefined_p(e2))
    return TRUE;
  if (expression_undefined_p(e1) || expression_undefined_p(e2))
    return FALSE;

  /* let's assume that every expression has a correct syntax component */
  s1 = expression_syntax(e1);
  s2 = expression_syntax(e2);

  return syntax_equal_p(s1, s2);
}

bool 
same_expression_p(expression e1, expression e2)
{
    normalized n1, n2;

    n1 = expression_normalized(e1);
    n2 = expression_normalized(e2);

    /* lazy normalization.
     */
    if (normalized_undefined_p(n1)) {
	normalize_all_expressions_of(e1);
        n1 = expression_normalized(e1);
    }

    if (normalized_undefined_p(n2)) {
	normalize_all_expressions_of(e2);
        n2 = expression_normalized(e2);
    }

    if (normalized_linear_p(n1) && normalized_linear_p(n2))
	return vect_equal(normalized_linear(n1), normalized_linear(n2));
    else
	return expression_equal_p(e1, e2);
}

bool 
syntax_equal_p(syntax s1, syntax s2)
{
    tag t1 = syntax_tag(s1);
    tag t2 = syntax_tag(s2);

    if(t1!=t2)
	return FALSE;

    switch(t1) {
    case is_syntax_reference:
	return reference_equal_p(syntax_reference(s1), syntax_reference(s2));
	break;
    case is_syntax_range:
	return range_equal_p(syntax_range(s1), syntax_range(s2));
	break;
    case is_syntax_call:
	return call_equal_p(syntax_call(s1), syntax_call(s2));
	break;
    default:
	break;
    }

    pips_error("syntax_equal_p", "ill. tag\n");
    return FALSE;
}

bool 
reference_equal_p(reference r1, reference r2)
{
    entity v1 = reference_variable(r1);
    entity v2 = reference_variable(r2);

    list dims1 = reference_indices(r1);
    list dims2 = reference_indices(r2);

    if(v1 != v2)
	return FALSE;

    if(gen_length(dims1) != gen_length(dims2))
	return FALSE;
    /*
       pips_internal_error("Different dimensions for %s: %d and %d\n",
       entity_local_name(v1), gen_length(dims1), gen_length(dims2));
       */

    for(; !ENDP(dims1); POP(dims1), POP(dims2))
	if(!expression_equal_p(EXPRESSION(CAR(dims1)), EXPRESSION(CAR(dims2))))
	    return FALSE;

    return TRUE;
}

bool 
range_equal_p(range r1, range r2)
{
    return expression_equal_p(range_lower(r1), range_lower(r2))
	&& expression_equal_p(range_upper(r1), range_upper(r2))
	    && expression_equal_p(range_increment(r1), range_increment(r2));
}

bool 
call_equal_p(call c1, call c2)
{
    entity f1 = call_function(c1);
    entity f2 = call_function(c2);
    list args1 = call_arguments(c1);
    list args2 = call_arguments(c2);

    if(f1 != f2)
	return FALSE;

    if(gen_length(args1) != gen_length(args2)) /* this should be a bug */
	return FALSE;

    for(; !ENDP(args1); POP(args1), POP(args2))
	if(!expression_equal_p(EXPRESSION(CAR(args1)), EXPRESSION(CAR(args2))))
	    return FALSE;

    return TRUE;
}

/* expression make_integer_constant_expression(int c)
 *  make expression for integer
 */
expression 
make_integer_constant_expression(int c)
{
    expression ex_cons;
    entity ce;   
   
    ce = make_integer_constant_entity(c);
    /* make expression for the constant c*/
    ex_cons = make_expression(
			      make_syntax(is_syntax_call,
					  make_call(ce,NIL)), 
			      normalized_undefined);
    return (ex_cons);
}

int 
integer_constant_expression_value(expression e)
{
  pips_assert("is constant", integer_constant_expression_p(e));
  return signed_integer_constant_expression_value(e);
}

int 
signed_integer_constant_expression_value(expression e)
{
    /* could be coded by geting directly the value of the constant entity... */
    /* also available as integer_constant_p() which has *two* arguments */

    normalized n = normalized_undefined;
    int val = 0;

    pips_assert("is signed constant", signed_integer_constant_expression_p(e));

    n = NORMALIZE_EXPRESSION(e);
    if(normalized_linear_p(n)) {
	Pvecteur v = (Pvecteur) normalized_linear(n);

	if(vect_constant_p(v)) {
	    Value x = vect_coeff(TCST, v);
	    val = VALUE_TO_INT(x);
	    }
	else
	    pips_internal_error("non constant expression\n");
    }
    else
	pips_internal_error("non affine expression\n");

    return val;
}

/* expression make_factor_expression(int coeff, entity vari)
 * make the expression "coeff*vari"  where vari is an entity.
 */
expression 
make_factor_expression(int coeff, entity vari)
{
    expression e1, e2, e3;
    entity operateur_multi;

    e1 = make_integer_constant_expression(coeff);
    if (vari==NULL)
	return(e1);			/* a constant only */
    else {
	e2 = make_expression(make_syntax(is_syntax_reference,
					 make_reference(vari, NIL)),
			     normalized_undefined);
	if (coeff == 1) return(e2);
	else {
	    operateur_multi = gen_find_tabulated("TOP-LEVEL:*",entity_domain);
	    e3 = make_expression(make_syntax(is_syntax_call,
					     make_call(operateur_multi,
				       CONS(EXPRESSION, e1,
				       CONS(EXPRESSION, e2,
					    NIL)))),
				 normalized_undefined);
	    return (e3);
	}
    }
}

/* make expression for vector (Pvecteur)
 */
expression 
make_vecteur_expression(Pvecteur pv)
{
    /* sort: to insure a deterministic generation of the expression.
     * note: the initial system is *NOT* touched.
     * ??? Sometimes the vectors are shared, so you cant modify them
     *     that easily. Many cores in Hpfc (deducables), Wp65, and so.
     * ok, I'm responsible for some of them:-)
     *
     *  (c) FC 24/11/94
     */
    Pvecteur
	v_sorted = vect_sort(pv, compare_Pvecteur),
	v = v_sorted;
    expression 	factor1, factor2;
    entity op_add, op_sub;
    int coef;

    op_add = entity_intrinsic(PLUS_OPERATOR_NAME);
    op_sub = entity_intrinsic(MINUS_OPERATOR_NAME);

    if (VECTEUR_NUL_P(v)) 
	return make_integer_constant_expression(0);

    coef = VALUE_TO_INT(vecteur_val(v));

    if (coef==-1) /* let us avoid -1*var, we prefer -var */
    {
	entity op_ums = entity_intrinsic(UNARY_MINUS_OPERATOR_NAME);
	factor1 = make_factor_expression(1, (entity) vecteur_var(v));
	factor1 = call_to_expression
	    (make_call(op_ums, CONS(EXPRESSION, factor1, NIL)));
    }
    else
	factor1 = make_factor_expression(coef, (entity) vecteur_var(v));

    for (v=v->succ; v!=NULL; v=v->succ)
    {
	coef = VALUE_TO_INT(vecteur_val(v));
	pips_assert("some coefficient", coef!=0);
	factor2 = make_factor_expression(ABS(coef), (entity) vecteur_var(v));
	factor1 = call_to_expression
	    (make_call(coef>0? op_add: op_sub,
		       CONS(EXPRESSION, factor1,
		       CONS(EXPRESSION, factor2,
			    NIL))));
    }

    vect_rm(v_sorted);

    return factor1;
}

/* generates var = linear expression 
 * from the Pvecteur. var is removed if necessary.
 * ??? should manage an (positive remainder) integer divide ?
 */
statement
Pvecteur_to_assign_statement(
    entity var,
    Pvecteur v)
{
    statement result;
    Pvecteur vcopy;
    Value coef;
    
    coef = vect_coeff((Variable) var, v);
    assert(value_le(value_abs(coef),VALUE_ONE));

    vcopy = vect_dup(v);
    
    if (value_notzero_p(coef)) vect_erase_var(&vcopy, (Variable) var);
    if (value_one_p(coef)) vect_chg_sgn(vcopy);
	
    result = make_assign_statement(entity_to_expression(var),
				   make_vecteur_expression(vcopy));
    vect_rm(vcopy);

    return result;
}

reference expression_reference(e)
expression e;
{
    return syntax_reference(expression_syntax(e));
}

/* predicates on references */

bool 
array_reference_p(r)
reference r;
{
    /* two possible meanings:
     * - the referenced variable is declared as an array
     * - the reference is to an array element
     *
     * This makes a difference in procedure calls and IO statements
     *
     * The second interpretation is chosen.
     */

    return reference_indices(r) != NIL;
}

/* If TRUE is returned, the two references cannot conflict unless array 
 * bound declarations are violated. If FALSE is returned, the two references
 * may conflict.
 *
 * TRUE is returned if the two references are array references and if
 * the two references entities are equal and if at least one dimension
 * can be used to desambiguate the two references using constant subscript
 * expressions. This test is store independent and certainly does not
 * replace a dependence test. It may beused to compute ude-def chains.
 *
 * If needed, an extra effort could be made for aliased arrays.
 */

bool
references_do_not_conflict_p(reference r1, reference r2)
{
    bool do_not_conflict = FALSE;
    entity v1 = reference_variable(r1);
    entity v2 = reference_variable(r2);

    if(v1==v2) {
	list s1 = reference_indices(r1);
	list s2 = reference_indices(r2);
	if(!ENDP(s1) && gen_length(s1)==gen_length(s2)) {
	    list cs1, cs2;
	    for(cs1=s1, cs2=s2; !ENDP(cs1) && !do_not_conflict; POP(cs1), POP(cs2)) {
		expression sub1 = EXPRESSION(CAR(cs1));
		expression sub2 = EXPRESSION(CAR(cs2));
		if(expression_constant_p(sub1) && expression_constant_p(sub2)) {
		    /* FI: OK, it would be better to use their normalized forms */
		    do_not_conflict = (expression_to_int(sub1)!=expression_to_int(sub2));
		}
	    }
	}
    }

    return do_not_conflict;
}

/*
 *    Utils from hpfc on 15 May 94, FC
 *
 */
expression 
expression_list_to_binary_operator_call(list l, entity op)
{
    int
	len = gen_length(l);
    expression
	result = expression_undefined;

    pips_assert("list_to_binary_operator_call", len!=0);

    result = EXPRESSION(CAR(l));

    MAPL(ce,
     {
	 result = MakeBinaryCall(op, EXPRESSION(CAR(ce)), result);
     },
	 CDR(l));

    return(result);
}


expression 
expression_list_to_conjonction(list l)
{
    int	len = gen_length(l);
    entity and = entity_intrinsic(AND_OPERATOR_NAME);
    return(len==0? 
	   MakeNullaryCall(entity_intrinsic(".TRUE.")):
	   expression_list_to_binary_operator_call(l, and));
}

/* bool expression_intrinsic_operation_p(expression exp): Returns TRUE
 * if "exp" is an expression with a call to an intrinsic operation.
 */
bool 
expression_intrinsic_operation_p(exp)
expression exp;
{
    entity e;
    syntax syn = expression_syntax(exp);

    if (syntax_tag(syn) != is_syntax_call)
	return (FALSE);

    e = call_function(syntax_call(syn));

    return(value_tag(entity_initial(e)) == is_value_intrinsic);
}

/* bool call_constant_p(call c): Returns TRUE if "c" is a call to a constant,
 * that is, a constant number or a symbolic constant.
 */
bool 
call_constant_p(call c)
{
    value cv = entity_initial(call_function(c));
    return( (value_tag(cv) == is_value_constant) ||
	   (value_tag(cv) == is_value_symbolic)   );
}


/* AP, sep 25th 95 : some usefull functions moved from
   static_controlize/utils.c */

/*=================================================================*/
/* expression Pvecteur_to_expression(Pvecteur vect): returns an
 * expression equivalent to "vect".
 *
 * A Pvecteur is a list of variables, each with an associated value.
 * Only one term of the list may have an undefined variables, it is the
 * constant term of the vector :
 *     (var1,val1) , (var2,val2) , (var3,val3) , ...
 *
 * An equivalent expression is the addition of all the variables, each
 * multiplied by its associated value :
 *     (...((val1*var1 + val2*var2) + val3*var3) +...)
 *
 * Two special cases are treated in order to get a more simple expression :
 *       _ if the sign of the value associated to the variable is
 *         negative, the addition is replaced by a substraction and we
 *         change the sign of the value (e.g. 2*Y + (-3)*X == 2*Y - 3*X).
 *         This optimization is of course not done for the first variable.
 *       _ the values equal to one are eliminated (e.g. 1*X == X).
 *
 * Note (IMPORTANT): if the vector is equal to zero, then it returns an
 * "expression_undefined", not an expression equal to zero.
 *
 */
/* rather use make_vecteur_expression which was already there */
expression 
Pvecteur_to_expression(Pvecteur vect)
{
    Pvecteur Vs;
    expression aux_exp, new_exp;
    entity plus_ent, mult_ent, minus_ent, unary_minus_ent, op_ent;
    
    new_exp = expression_undefined;
    Vs = vect;
    
    debug( 7, "Pvecteur_to_expression", "doing\n");
    if(!VECTEUR_NUL_P(Vs))
    {
	entity var = (entity) Vs->var;
	int val = VALUE_TO_INT(Vs->val);
	
  /* We get the entities corresponding to the three operations +, - and *. */
  plus_ent = gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
                                                     PLUS_OPERATOR_NAME),
                                entity_domain);
  minus_ent = gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
                                                      MINUS_OPERATOR_NAME),
                                 entity_domain);
  mult_ent = gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
                                                     MULTIPLY_OPERATOR_NAME),
                                entity_domain);
  unary_minus_ent =
         gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
                                                 UNARY_MINUS_OPERATOR_NAME),
		 	    entity_domain);

  /* Computation of the first variable of the vector. */
  if(term_cst(Vs))
    /* Special case of the constant term. */
    new_exp = make_integer_constant_expression(val);
  else if( (val != 1) && (val != -1) )
    new_exp = MakeBinaryCall(mult_ent,
                             make_integer_constant_expression(val),
                             make_entity_expression(var, NIL));
  else if (val == 1)
    /* We eliminate this value equal to one. */
    new_exp = make_entity_expression(var, NIL);
  else /* val == -1 */
    new_exp = MakeUnaryCall(unary_minus_ent, make_entity_expression(var, NIL));

  /* Computation of the rest of the vector. */
  for(Vs = vect->succ; !VECTEUR_NUL_P(Vs); Vs = Vs->succ)
    {
    var = (entity) Vs->var;
    val = VALUE_TO_INT(Vs->val);

    if (val < 0)
      {
      op_ent = minus_ent;
      val = -val;
      }
    else
      op_ent = plus_ent;

    if(term_cst(Vs))
      /* Special case of the constant term. */
      aux_exp = make_integer_constant_expression(val);
    else if(val != 1)
      aux_exp = MakeBinaryCall(mult_ent,
                               make_integer_constant_expression(val),
                               make_entity_expression(var, NIL));
    else
      /* We eliminate this value equal to one. */
      aux_exp = make_entity_expression(var, NIL);

    new_exp = MakeBinaryCall(op_ent, new_exp, aux_exp);
    }
    }
    return(new_exp);
}

/*=================================================================*/
/* bool expression_equal_integer_p(expression exp, int i): returns TRUE if
 * "exp" is a constant value equal to "i".
 */
bool 
expression_equal_integer_p(expression exp, int i)
{
    debug( 7, "expression_equal_integer", "doing\n");
    if(expression_constant_p(exp))
	return(expression_to_int(exp) == i);
    return(FALSE);
}

/*=================================================================*/
/* expression make_op_exp(char *op_name, expression exp1 exp2):
 * Returns an expression containing the operation "op_name" between "exp1" and
 * "exp2".
 * "op_name" must be one of the four classic operations : +, -, * or /.
 *
 * If both expressions are integer constant values and the operation
 * result is an integer then the returned expression contained the
 * calculated result.
 *
 * Else, we treat five special cases :
 *       _ exp1 and exp2 are integer linear and op_name is + or -.
 *         This case is resolved by make_lin_op_exp().
 *       _ exp1 = 0
 *       _ exp1 = 1
 *       _ exp2 = 0
 *       _ exp2 = 1
 *
 * Else, we create a new expression with a binary call.
 *
 * Note: The function MakeBinaryCall() comes from Pips/.../syntax/expression.c
 *       The function make_integer_constant_expression() comes from ri-util.
 */
expression 
make_op_exp(op_name, exp1, exp2)
char *op_name;
expression exp1, exp2;
{
    expression result_exp = expression_undefined;
    entity op_ent, unary_minus_ent;
    
    debug( 7, "make_op_exp", "doing\n");
    op_ent = gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
						     op_name), entity_domain);
    unary_minus_ent =
	gen_find_tabulated(make_entity_fullname(TOP_LEVEL_MODULE_NAME,
						UNARY_MINUS_OPERATOR_NAME),
			   entity_domain);
    
    debug(5, "make_op_exp", "begin OP EXP : %s  %s  %s\n",
	  words_to_string(words_expression(exp1)),
                        op_name,
	  words_to_string(words_expression(exp2)));
    
    if( ! ENTITY_FOUR_OPERATION_P(op_ent) )
	user_error("make_op_exp", "operation must be : +, -, * or /");
    
    if( expression_constant_p(exp1) && expression_constant_p(exp2) )
    {
	int val1, val2;
	
	debug(6, "make_op_exp", "Constant expressions\n");

	val1 = expression_to_int(exp1);
	val2 = expression_to_int(exp2);
	
	if (ENTITY_PLUS_P(op_ent))
	    result_exp = make_integer_constant_expression(val1 + val2);
	else if(ENTITY_MINUS_P(op_ent))
	    result_exp = make_integer_constant_expression(val1 - val2);
	else if(ENTITY_MULTIPLY_P(op_ent))
	    result_exp = make_integer_constant_expression(val1 * val2);
	else /* ENTITY_DIVIDE_P(op_ent) */
	    /* we compute here as FORTRAN would do */
	    result_exp = make_integer_constant_expression((int) (val1 / val2));
    }
    else
    {
	/* We need to know the integer linearity of both expressions. */
	normalized nor1 = NORMALIZE_EXPRESSION(exp1);
	normalized nor2 = NORMALIZE_EXPRESSION(exp2);
	
	if((normalized_tag(nor1) == is_normalized_linear) &&
	   (normalized_tag(nor2) == is_normalized_linear) &&
	   (ENTITY_PLUS_P(op_ent) || ENTITY_MINUS_P(op_ent)) )
	{
	    debug(6, "make_op_exp", "Linear operation\n");
	    
	    result_exp = make_lin_op_exp(op_ent, exp1, exp2);
	}
	else if(expression_equal_integer_p(exp1, 0))
	{
	    if (ENTITY_PLUS_P(op_ent))
		result_exp = exp2;
	    else if(ENTITY_MINUS_P(op_ent))
		result_exp = MakeUnaryCall(unary_minus_ent, exp2);
	    else /* ENTITY_MULTIPLY_P(op_ent) || ENTITY_DIVIDE_P(op_ent) */
		result_exp = make_integer_constant_expression(0);
	}
	else if(expression_equal_integer_p(exp1, 1))
	{
	    if(ENTITY_MULTIPLY_P(op_ent))
		result_exp = exp2;
	}
	else if(expression_equal_integer_p(exp2, 0))
	{
	    if (ENTITY_PLUS_P(op_ent) || ENTITY_MINUS_P(op_ent))
		result_exp = exp1;
	    else if (ENTITY_MULTIPLY_P(op_ent))
		result_exp = make_integer_constant_expression(0);
	    else /* ENTITY_DIVIDE_P(op_ent) */
		user_error("make_op_exp", "division by zero");
	}
	else if(expression_equal_integer_p(exp2, 1))
	{
	    if(ENTITY_MULTIPLY_P(op_ent) || ENTITY_DIVIDE_P(op_ent))
		result_exp = exp1;
	}
	
	/* Both expressions are unnormalized because they might be reused in
	 * an unnormalized expression. */
	unnormalize_expression(exp1);
	unnormalize_expression(exp2);
    }
    
    if(result_exp == expression_undefined)
	result_exp = MakeBinaryCall(op_ent, exp1, exp2);
    
    debug(5, "make_op_exp", "end   OP EXP : %s\n",
	  words_to_string(words_expression(result_exp)));
    
    return (result_exp);
}


/*=================================================================*/
/* expression make_lin_op_exp(entity op_ent, expression exp1 exp2): returns
 * the expression resulting of the linear operation (ie. + or -) "op_ent"
 * between two integer linear expressions "exp1" and "exp2".
 *
 * This function uses the linear library for manipulating Pvecteurs.
 *
 * Pvecteur_to_expression() is a function that rebuilds an expression
 * from a Pvecteur.
 */
expression 
make_lin_op_exp(op_ent, exp1, exp2)
entity op_ent;
expression exp1, exp2;
{
    Pvecteur V1, V2, newV = VECTEUR_NUL;

    debug( 7, "make_lin_op_exp", "doing\n");
    if((normalized_tag(expression_normalized(exp1)) == is_normalized_complex) ||
       (normalized_tag(expression_normalized(exp2)) == is_normalized_complex))
	user_error("make_lin_op_exp",
		   "expressions MUST be linear and normalized");
    
    V1 = (Pvecteur) normalized_linear(expression_normalized(exp1));
    V2 = (Pvecteur) normalized_linear(expression_normalized(exp2));
    
    if (ENTITY_PLUS_P(op_ent))
	newV = vect_add(V1, V2);
    else if (ENTITY_MINUS_P(op_ent))
	newV = vect_substract(V1, V2);
    else
	pips_error("make_lin_op_exp", "operation must be : + or -");
    
    return(Pvecteur_to_expression(newV));
}

/*=================================================================*/
/* void unnormalize_expression(expression exp): puts the normalized
 * field of "exp" to undefined and does the unnormalization recursively
 * if this expression is a call to an intrinsic function.
 *
 * This is very useful when you combine expressions. It prohibits
 * unnormalized expressions with normalized sub-expressions.
 */
void 
unnormalize_expression(exp)
expression exp;
{
    syntax sy;
    
    debug( 9, "unnormalize_expression", "doing\n");
    /* FI: memory leak... */
    expression_normalized(exp) = normalized_undefined;
    sy = expression_syntax(exp);
    
    if(syntax_tag(sy) == is_syntax_call)
    {
	call c = syntax_call(sy);
	value v = entity_initial(call_function(c));
	
	/* We unnormalize the arguments of the intrinsic functions. */
	if(value_tag(v) == is_value_intrinsic)
	{
	    list la = call_arguments(c);
	    for(; la != NIL; la = CDR(la))
		unnormalize_expression(EXPRESSION(CAR(la)));
	}
    }
}


/*=================================================================*/
/* int expression_to_int(expression exp): returns the integer value of "exp".
 *
 * Note: "exp" is supposed to contain an integer value which means that the
 *       function expression_constant_p() has returned TRUE with "exp" in
 *       argument.
 *       This implies that if "exp" is not a "value_constant", it must be
 *       a "value_intrinsic". In that case it is an unary minus operation
 *       upon an expression for which the function expression_constant_p()
 *       returns TRUE (See the commentary given for it).
 */
int 
expression_to_int(exp)
expression exp;
{
    int rv = 0;

    debug( 7, "expression_to_int", "doing\n");
    if(expression_constant_p(exp)) {
	call c = syntax_call(expression_syntax(exp));
	switch(value_tag(entity_initial(call_function(c)))) {
	case is_value_constant:	{
	    rv = constant_int(value_constant(entity_initial(call_function(c))));
	    break;
	}
	case is_value_intrinsic: {
	    rv = 0 - expression_to_int(EXPRESSION(CAR(call_arguments(c))));
	    break;
	}
	}
    }
    else if(expression_call_p(exp)) {
      /* Is it an integer parameter? */
      entity p = call_function(syntax_call(expression_syntax(exp)));
      value v = entity_initial(p);

      if(value_symbolic_p(v) && constant_int_p(symbolic_constant(value_symbolic(v)))) {
	rv = constant_int(symbolic_constant(value_symbolic(v)));
      }
      else {
	pips_error("expression_to_int",
		   "expression is not an integer constant");
      }
    }
    else
	pips_error("expression_to_int",
		   "expression is not an integer constant");
    return(rv);
}

/*=================================================================*/
/* bool expression_constant_p(expression exp)
 * Returns TRUE if "exp" contains an integer constant value.
 *
 * Note : A negative constant can be represented with a call to the unary
 *        minus intrinsic function upon a positive value.
 */
bool 
expression_constant_p(exp)
expression exp;
{
    if(syntax_call_p(expression_syntax(exp)))
    {
	call c = syntax_call(expression_syntax(exp));
	switch(value_tag(entity_initial(call_function(c))))
	{
	case is_value_constant:
	    return(TRUE);
	case is_value_intrinsic:
	    if(ENTITY_UNARY_MINUS_P(call_function(c)))
		return expression_constant_p
		    (EXPRESSION(CAR(call_arguments(c))));
	}
    }
    return(FALSE);
}
bool 
expression_one_p(expression exp)
{
  bool one_p = FALSE;

  if(syntax_call_p(expression_syntax(exp))) {
    call c = syntax_call(expression_syntax(exp));
    value v = entity_initial(call_function(c));

    if(value_constant_p(v)) {
      constant c = value_constant(v);
      one_p = constant_int_p(c) && (constant_int(c)==1);
    }
  }
  return one_p;
}


/****************************************************** SAME EXPRESSION NAME */

/* compare two entities for their appearance point of view.
 * used for putting common in includes.
 */

bool same_expression_name_p(expression, expression);

bool 
same_lexpr_name_p(list l1, list l2)
{
    if (gen_length(l1)!=gen_length(l2))
	return FALSE;
    /* else */
    for(; l1 && l2; POP(l1), POP(l2))
	if (!same_expression_name_p(EXPRESSION(CAR(l1)), EXPRESSION(CAR(l2))))
	    return FALSE;
    return TRUE;
}

bool
same_entity_lname_p(entity e1, entity e2)
{
    return same_string_p(entity_local_name(e1), entity_local_name(e2));
}

bool
same_call_name_p(call c1, call c2)
{
    return same_entity_lname_p(call_function(c1), call_function(c2)) &&
	same_lexpr_name_p(call_arguments(c1), call_arguments(c2));
}

bool
same_ref_name_p(reference r1, reference r2)
{
    return same_entity_lname_p(reference_variable(r1), reference_variable(r2)) 
	&& same_lexpr_name_p(reference_indices(r1), reference_indices(r2));
}

bool 
same_range_name_p(range r1, range r2)
{
    return same_expression_name_p(range_lower(r1), range_lower(r2)) &&
	same_expression_name_p(range_upper(r1), range_upper(r2)) &&
	same_expression_name_p(range_increment(r1), range_increment(r2));
}

bool
same_syntax_name_p(syntax s1, syntax s2)
{
    if (syntax_tag(s1)!=syntax_tag(s2)) 
	return FALSE;
    /* else */
    switch (syntax_tag(s1))
    {
    case is_syntax_call: 
	return same_call_name_p(syntax_call(s1), syntax_call(s2));
    case is_syntax_reference:
	return same_ref_name_p(syntax_reference(s1), syntax_reference(s2));
    case is_syntax_range:
	return same_range_name_p(syntax_range(s1), syntax_range(s2));
    default:
	pips_internal_error("unexpected syntax tag\n");
    }
    return FALSE;
}

bool 
same_expression_name_p(expression e1, expression e2)
{
    return same_syntax_name_p(expression_syntax(e1), expression_syntax(e2));
}

/************************************************************* DAVINCI GRAPH */

#define ALREADY_SEEN(node) (hash_defined_p(seen, (char*)node))
#define SEEN(node) (hash_put(seen, (char*) node, (char*) 1))

#define DV_CIRCLE ",a(\"_GO\",\"circle\")"
#define DV_YELLOW ",a(\"COLOR\",\"yellow\")"

static bool  davinci_dump_expression_rc(
    FILE * out, expression e, hash_table seen)
{
  syntax s;
  string name, shape, color;
  list sons = NIL;
  bool first = TRUE, something = TRUE;

  if (ALREADY_SEEN(e)) return FALSE;
  SEEN(e);

  s = expression_syntax(e);
  switch (syntax_tag(s))
  {
  case is_syntax_call: 
    {
      call c = syntax_call(s);
      name = entity_local_name(call_function(c));
      sons = call_arguments(c);
      shape = "";
      color = "";
      break;
    }
  case is_syntax_range:
    name = "::";
    shape = "";
    color = "";
    break;
  case is_syntax_reference:
    name = entity_local_name(reference_variable(syntax_reference(s)));
    shape = DV_CIRCLE;
    color = DV_YELLOW;
    break;
  default:
    name = "";
    shape = "";
    color = "";
    pips_internal_error("unexpected syntax tag (%d)\n", syntax_tag(s));
  }
  
    /* daVinci node prolog. */
  fprintf(out, "l(\"%zx\",n(\"\",[a(\"OBJECT\",\"%s\")%s%s],[", 
	  (uintptr_t) e, name, color, shape);
  
  MAP(EXPRESSION, son, 
  {      
    if (!first) fprintf(out, ",\n");
    else { fprintf(out, "\n"); first=FALSE; }
    fprintf(out, " l(\"%zx->%zx\",e(\"\",[],r(\"%zx\")))", 
	    (uintptr_t) e, (uintptr_t) son, (uintptr_t) son);
  },
    sons);

    /* node epilog */
  fprintf(out, "]))\n");
  
  MAP(EXPRESSION, son, 
  { 
    if (something) fprintf(out, ","); 
    something = davinci_dump_expression_rc(out, son, seen);
  }, sons);

  return TRUE;
}

/* dump expression e in file out as a davinci graph.
 */
void davinci_dump_expression(FILE * out, expression e)
{
  hash_table seen = hash_table_make(hash_pointer, 0);
  fprintf(out, "[\n");
  davinci_dump_expression_rc(out, e, seen);
  fprintf(out, "]\n\n");
  hash_table_free(seen);
}

static FILE * out_flt = NULL;
static bool expr_flt(expression e)
{
  davinci_dump_expression(out_flt, e);
  return FALSE;
}

/* dump all expressions in s to out.
 */
void davinci_dump_all_expressions(FILE * out, statement s)
{
  out_flt = out;
  gen_recurse(s, expression_domain, expr_flt, gen_null);
  out_flt = NULL;
}

/* This function replaces all the occurences of an old entity in the 
 * expression exp by the new entity. It returns the expression modified.  
 * I think we  can write this function by using gen_context_multi_recurse  ... * To do .... NN */
expression substitute_entity_in_expression(entity old, entity new, expression e)
{
  syntax s;
  tag t;
  call c;
  range ra;
  reference re;
  list args,tempargs= NIL;
  expression retour = copy_expression(e), exp,temp,low,up,inc;

  s = expression_syntax(e);
  t = syntax_tag(s);
  switch (t){ 
  case is_syntax_call:  
    {
      c = syntax_call(s);      
      args = call_arguments(c);
      while (!ENDP(args))
	{
	  exp = EXPRESSION(CAR(args));
	  temp = substitute_entity_in_expression(old,new,exp);
	  tempargs = gen_nconc(tempargs,CONS(EXPRESSION,temp,NIL));
	  args = CDR(args);
	}
      
      call_arguments(syntax_call(expression_syntax(retour))) = tempargs;

      if (same_entity_p(call_function(c),old))
	call_function(syntax_call(expression_syntax(retour))) = new;
      else 
	call_function(syntax_call(expression_syntax(retour))) = call_function(c);

      break;
    }
  case is_syntax_reference:
    { 
      re = syntax_reference(s);      
      args = reference_indices(re);
      while (!ENDP(args))
	{
	  exp = EXPRESSION(CAR(args));
	  temp = substitute_entity_in_expression(old,new,exp);
	  tempargs = gen_nconc(tempargs,CONS(EXPRESSION,temp,NIL));
	  args = CDR(args);
	}
      
      reference_indices(syntax_reference(expression_syntax(retour))) = tempargs;

      if (same_entity_p(reference_variable(re),old))
	reference_variable(syntax_reference(expression_syntax(retour))) = new;
      else 
	reference_variable(syntax_reference(expression_syntax(retour))) = reference_variable(re);

      break;
    }
  case is_syntax_range:
    {
      ra = syntax_range(s);  
      low = range_lower(ra);
      range_lower(syntax_range(expression_syntax(retour))) = substitute_entity_in_expression(old,new,low);
  
      up = range_upper(ra);
      range_upper(syntax_range(expression_syntax(retour))) = substitute_entity_in_expression(old,new,up);

   
      inc = range_increment(ra);
      range_increment(syntax_range(expression_syntax(retour))) = substitute_entity_in_expression(old,new,inc);
  
      break;
	    
    }
  }

  return retour;
}

/* Replace C operators "+C" and "-C" which can handle pointers by
   arithmetic operators "+" and "-" when it is safe to do so, i.e. when no
   pointer arithmetic is involved. */
bool simplify_C_expression(expression e)
{
  syntax s = expression_syntax(e);
  bool   can_be_substituted_p = FALSE;

  pips_debug(9, "Begin\n");
  
  switch(syntax_tag(s)) {
  case is_syntax_reference:
    {
      entity re = reference_variable(syntax_reference(s));
      type rt = entity_type(re);
      basic bt = basic_undefined;

      if(type_variable_p(rt)) { /* FI: What if not? core dump? */
	bt = variable_basic(type_variable(rt));

	can_be_substituted_p =
	  basic_int_p(bt)
	  || basic_float_p(bt) 
	  || basic_overloaded_p(bt) /* Might be wrong, but necessary */
	  || basic_complex_p(bt); /* Should not occur in C */

	pips_debug(9, "Variable %s is an arithmetic variable: %s\n",
		   entity_local_name(re), bool_to_string(can_be_substituted_p));
      }
      break; /* FI: The index expressions should be simplified too... */
    }
  case is_syntax_call:
    {
      call c = syntax_call(s);

      if(expression_constant_p(e)) {
	/* What is the type of the constant? */
	entity cste = call_function(c);
	basic rb = variable_basic(type_variable(functional_result(type_functional(entity_type(cste)))));

	can_be_substituted_p =
	  basic_int_p(rb)
	  || basic_float_p(rb) 
	  || basic_complex_p(rb); /* Should not occur in C, before C99 */
      }
      else if(gen_length(call_arguments(c))==2) {
	/* Check "+C" and "-C" */
	expression e1 = EXPRESSION(CAR(call_arguments(c)));
	expression e2 = EXPRESSION(CAR(call_arguments(c)));
	can_be_substituted_p = simplify_C_expression(e1)
	  && simplify_C_expression(e2);
	if(can_be_substituted_p) {
	  entity op = call_function(c);
	  if(ENTITY_PLUS_C_P(op)) {
	    call_function(c) = entity_intrinsic(PLUS_OPERATOR_NAME);
	  }
	  else if(ENTITY_MINUS_C_P(op)) {
	    call_function(c) = entity_intrinsic(MINUS_OPERATOR_NAME);
	  }
	}
      }
      else {
	/* Try to simplify the arguments, do not hope much from the result
           type because of overloading. */
	type ft = call_to_functional_type(c, TRUE);
	type rt = ultimate_type(functional_result(type_functional(ft)));

	//pips_assert("The function type is functional", type_functional_p(entity_type(f)));

	MAP(EXPRESSION, se, {
	  (void) simplify_C_expression(se);
	}, call_arguments(c));

	if(type_variable_p(rt)) {
	  basic rb = variable_basic(type_variable(rt));

	  /* FI: I guess, typedef equivalent to those could also be declared substituable */
	  can_be_substituted_p =
	    basic_int_p(rb)
	    || basic_float_p(rb) 
	    || basic_complex_p(rb); /* Should not occur in C */
	}
	else {
	  can_be_substituted_p = FALSE;
	}
      }
      break;
    }
  case is_syntax_range: 
    {
      range r = syntax_range(s);
      expression le = range_lower(r);
      expression ue = range_upper(r);
      expression ince = range_increment(r);
      (void) simplify_C_expression(le);
      (void) simplify_C_expression(ue);
      (void) simplify_C_expression(ince);
      can_be_substituted_p = FALSE;
      break;
      }
  case is_syntax_cast:
  case is_syntax_sizeofexpression:
  case is_syntax_subscript: 
  case is_syntax_application:
  case is_syntax_va_arg:
      can_be_substituted_p = FALSE;
      break;
  default: pips_internal_error("Bad syntax tag");
    can_be_substituted_p = FALSE; /* for gcc */
  }

  pips_debug(9, "End: %s\n", bool_to_string(can_be_substituted_p));
  return can_be_substituted_p;
}

/* Replace a C expression used as FOR bound by a Fortran DO bound
expression, taking into account the C comparison operator used. */
expression convert_bound_expression(expression e, bool upper_p, bool non_strict_p)
{
  expression b = expression_undefined;

  if(non_strict_p) {
    b = copy_expression(e);
  }
  else {
    /* */
    int ib = 0;
    int nb = 0;

    if(expression_integer_value(e, &ib)) {
      /* The offset might not be plus or minus one, unless we know the
	 index is an integer? Does it depend on the step value? More
	 thought needed than available tonight (FI) */
      nb = upper_p? ib-1 : ib+1;
      b = int_to_expression(nb);
    }
    else {
      expression offset = int_to_expression(1);
      entity op = entity_intrinsic(upper_p? MINUS_OPERATOR_NAME : PLUS_OPERATOR_NAME);

      b = MakeBinaryCall(op, copy_expression(e), offset);
    }
  }
  return b;
}

bool reference_with_constant_indices_p(reference r)
{
  list sel = reference_indices(r);
  bool constant_p = TRUE;

  MAP(EXPRESSION, se, {
      if(!extended_integer_constant_expression_p(se)) {
	constant_p = FALSE;
	break;
      }
    }, sel);
  return constant_p;
}

/* Return by side effect a reference whose memory locations includes
   the memory locations of r in case the subcript expressions are
   changed by a store change.

   Constant subscript expressions are preserved.

   Store varying subscript expressions are replaced by unbounded expressions.
 */
reference reference_with_store_independent_indices(reference r)
{
  list sel = reference_indices(r);
  list sec = list_undefined;

  for(sec = sel; !ENDP(sec); POP(sec)) {
    expression se = EXPRESSION(CAR(sec));

    if(!extended_integer_constant_expression_p(se)) {
      free_expression(se);
      EXPRESSION_(CAR(sec)) = make_unbounded_expression();
    }
  }

  return r;
}

/* indices can be constant or unbounded: they are store independent. */
bool reference_with_unbounded_indices_p(reference r)
{
  list sel = reference_indices(r);
  bool unbounded_p = TRUE;

  MAP(EXPRESSION, se, {
      if(!extended_integer_constant_expression_p(se)
	 && !unbounded_expression_p(se)) {
	unbounded_p = FALSE;
	break;
      }
    }, sel);
  return unbounded_p;
}

/* Does this reference define the same set of memory locations
   regardless of the current (environment and) memory state?
 */
bool store_independent_reference_p(reference r)
{
  bool independent_p = TRUE;
  //list ind = reference_indices(r);
  entity v = reference_variable(r);
  type t = ultimate_type(entity_type(v));

  if(pointer_type_p(t)) {
    independent_p = FALSE;
  }
  else {
    independent_p = reference_with_constant_indices_p(r);
  }

  return independent_p;
}

void check_user_call_site(entity func, list args)
{
  /* check the number of parameters */
  list l_formals = module_formal_parameters(func);
  int n_formals = (int) gen_length(l_formals);
   
  if ((int) gen_length(args) < n_formals)
    {
      /* this is really a user error.
       * if you move this as a user warning, the pips would drop
       * effects about unbounded formals... why not? FC.
       */
      fprintf(stderr,"%d formal arguments for module %s:\n",
	      n_formals,module_local_name(func));
      dump_arguments(l_formals);
      fprintf(stderr,"%zd actual arguments:\n",gen_length(args));
      print_expressions(args);
      pips_user_error("\nCall to module %s: "
		      "insufficient number of actual arguments.\n",
		      module_local_name(func));
    }
  else if ((int) gen_length(args) > n_formals)
    {
      /* This can be survived... */        
      fprintf(stderr,"%d formal arguments for module%s:\n",
	      n_formals,module_local_name(func));
      dump_arguments(l_formals);
      fprintf(stderr,"%zd actual arguments:\n",gen_length(args));
      print_expressions(args);

      pips_user_warning("\nCall to module %s: "
			"too many actual arguments.\n",
			module_local_name(func));
    }
  gen_free_list(l_formals), l_formals=NIL;
}

/* just returns the entity of an expression... 
 * SG: moved here from hpfc
 */
entity expression_to_entity(e)
expression e;
{
    syntax s = expression_syntax(e);
    
    switch (syntax_tag(s))
    {
    case is_syntax_call:
	return call_function(syntax_call(s));
    case is_syntax_reference:
	return reference_variable(syntax_reference(s));
    case is_syntax_range:
    default: 
	pips_internal_error("unexpected syntax tag: %d\n", syntax_tag(s));
	return entity_undefined; 
    }
}

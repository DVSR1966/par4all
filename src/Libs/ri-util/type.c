#include <stdio.h>

#include "genC.h"
#include "ri.h"
#include "misc.h"

#include "ri-util.h"

/* generation of types */

basic 
MakeBasicOverloaded()
{
    return(make_basic(is_basic_overloaded, NIL));
}

mode 
MakeModeReference()
{
    return(make_mode(is_mode_reference, NIL));
}

mode 
MakeModeValue()
{
    return(make_mode(is_mode_value, NIL));
}

type 
MakeTypeStatement()
{
    return(make_type(is_type_statement, NIL));
}

type 
MakeTypeUnknown()
{
    return(make_type(is_type_unknown, NIL));
}

type 
MakeTypeVoid()
{
    return(make_type(is_type_void, NIL));
}

type 
MakeTypeVariable(b, ld)
basic b;
cons * ld;
{
    return(make_type(is_type_variable, make_variable(b, ld)));
}

/*
 *
 */
basic 
MakeBasic(the_tag)
int the_tag;
{
    switch(the_tag)
    {
    case is_basic_int: 
	return(make_basic(is_basic_int, 4));
	break;
    case is_basic_float: 
	return(make_basic(is_basic_float, 4));
	break;
    case is_basic_logical: 
	return(make_basic(is_basic_logical, 4));
	break;
    case is_basic_complex: 
	return(make_basic(is_basic_complex, 8));
	break;
    case is_basic_overloaded: 
	return(make_basic(is_basic_overloaded, UU));
	break;
    case is_basic_string: 
	return(make_basic(is_basic_string, string_undefined));
	break;
    default:
	pips_error("MakeBasic", "unexpected basic tag: %d\n",
                   the_tag);
	break;
    }
    
    return(basic_undefined);
}

/* functions on types */

type 
MakeTypeArray(b, ld)
basic b;
cons * ld;
{
    return(make_type(is_type_variable, make_variable(b, ld)));
}

parameter 
MakeOverloadedParameter()
{
  return MakeAnyScalarParameter(is_basic_overloaded, UU);
}

parameter 
MakeIntegerParameter()
{
  return MakeAnyScalarParameter(is_basic_int, DEFAULT_REAL_TYPE_SIZE);
}

parameter 
MakeRealParameter()
{
  return MakeAnyScalarParameter(is_basic_float, DEFAULT_REAL_TYPE_SIZE);
}

parameter 
MakeDoubleprecisionParameter()
{
  return MakeAnyScalarParameter(is_basic_float, DEFAULT_DOUBLEPRECISION_TYPE_SIZE);
}

parameter 
MakeLogicalParameter()
{
  return MakeAnyScalarParameter(is_basic_logical, DEFAULT_LOGICAL_TYPE_SIZE);
}

parameter 
MakeComplexParameter()
{
  return MakeAnyScalarParameter(is_basic_complex, DEFAULT_COMPLEX_TYPE_SIZE);
}

parameter 
MakeDoublecomplexParameter()
{
  return MakeAnyScalarParameter(is_basic_complex, DEFAULT_DOUBLECOMPLEX_TYPE_SIZE);
}

parameter 
MakeCharacterParameter()
{
  return make_parameter(MakeTypeArray(make_basic(is_basic_string, 
						 make_value(is_value_constant,
							    make_constant(is_constant_int,
									  DEFAULT_CHARACTER_TYPE_SIZE))),
				      NIL),
			make_mode(is_mode_value, UU));
}

parameter 
MakeAnyScalarParameter(tag t, int size)
{
    return(make_parameter((MakeTypeArray(make_basic(t, 
						    size), NIL)),
			  make_mode(is_mode_value, UU)));
}

/* this function creates a default fortran operator result, i.e. a zero
 * dimension variable with an overloaded basic type.
 */
type 
MakeOverloadedResult()
{
    return MakeAnyScalarResult(is_basic_overloaded, UU);
}

type 
MakeIntegerResult()
{
    return MakeAnyScalarResult(is_basic_int, DEFAULT_INTEGER_TYPE_SIZE);
}

type 
MakeRealResult()
{
    return MakeAnyScalarResult(is_basic_float, DEFAULT_REAL_TYPE_SIZE);
}

type 
MakeDoubleprecisionResult()
{
    return MakeAnyScalarResult(is_basic_float, DEFAULT_DOUBLEPRECISION_TYPE_SIZE);
}

type 
MakeLogicalResult()
{
    return MakeAnyScalarResult(is_basic_logical, DEFAULT_LOGICAL_TYPE_SIZE);
}

type 
MakeComplexResult()
{
    return MakeAnyScalarResult(is_basic_complex, DEFAULT_COMPLEX_TYPE_SIZE);
}

type 
MakeDoublecomplexResult()
{
    return MakeAnyScalarResult(is_basic_complex, DEFAULT_DOUBLECOMPLEX_TYPE_SIZE);
}

type 
MakeCharacterResult()
{
  return MakeTypeArray(make_basic(is_basic_string, 
				  make_value(is_value_constant,
					     make_constant(is_constant_int,
							   DEFAULT_CHARACTER_TYPE_SIZE))),
		       NIL);
}

type 
MakeAnyScalarResult(tag t, int size)
{
    return(MakeTypeArray(make_basic(t, size), NIL));
}


bool 
type_equal_p(t1, t2)
type t1;
type t2;
{
    /* the unknown type is not handled in a satisfactory way: in some
       sense, it should be declared equal to any other type but the
       undefined type;

       undefined types could also be seen in a different way, as really
       undefined values; so if t1 or t2 is undefined, the procedure
       should abort 

       Francois Irigoin, 10 March 1992
       */
  bool tequal = FALSE;

    if(t1 == t2)
	return TRUE;
    else if (t1 == type_undefined && t2 != type_undefined)
	return FALSE;
    else if (t1 != type_undefined && t2 == type_undefined)
	return FALSE;
    else if (type_tag(t1) != type_tag(t2))
	return FALSE;

    /* assertion: t1 and t2 are defined and have the same tag */
    switch(type_tag(t1)) {
    case is_type_statement:
	return TRUE;
    case is_type_area:
	return area_equal_p(type_area(t1), type_area(t2));
    case is_type_variable:
      tequal = variable_equal_p(type_variable(t1), type_variable(t2));
      return tequal;
    case is_type_functional:
	return functional_equal_p(type_functional(t1), type_functional(t2));
    case is_type_unknown:
	return TRUE;
    case is_type_void:
	return TRUE;
    default: 
	pips_error("type_equal_p", "unexpected tag %d\n", type_tag(t1));
    }

    return FALSE; /* just to avoid a warning */
}

type 
make_scalar_integer_type(n)
int n;
{
    type t = make_type(is_type_variable,
		       make_variable(make_basic(is_basic_int, n), NIL));
    return t;
}

bool 
area_equal_p(a1, a2)
area a1;
area a2;
{
    if(a1 == a2)
	return TRUE;
    else if (a1 == area_undefined && a2 != area_undefined)
	return FALSE;
    else if (a1 != area_undefined && a2 == area_undefined)
	return FALSE;
    else
	/* layouts are independent ? */
	return (area_size(a1) == area_size(a2));
}

bool 
variable_equal_p(v1, v2)
variable v1;
variable v2;
{
  bool vtequal = FALSE;

    if(v1 == v2)
	return TRUE;
    else if (v1 == variable_undefined && v2 != variable_undefined)
	return FALSE;
    else if (v1 != variable_undefined && v2 == variable_undefined)
	return FALSE;
    else if (!(vtequal = basic_equal_p(variable_basic(v1), variable_basic(v2))))
	return FALSE;
    else {
	list ld1 = variable_dimensions(v1);
	list ld2 = variable_dimensions(v2);

	if(ld1==NIL && ld2==NIL)
	    return vtequal;
	else {
	    /* dimensions should be checked, but it's hard: the only
	       Fortran requirement is that the space allocated in
	       the callers is bigger than the space used in the
	       callee; stars represent any strictly positive integer;
	       we do not know if v1 is the caller type or the callee type;
	       I do not know what should be done; FI */
	    /* FI: I return FALSE because the exact test should never be useful
	       in the parser; 1 February 1994 */
	    return FALSE;
	    pips_error("variable_equal_p", "dimension check not implemented\n");
	}
    }
  /* Should never be executed */
  return vtequal;
}

bool 
basic_equal_p(b1, b2)
basic b1;
basic b2;
{
    if(b1 == b2)
	return TRUE;
    else if (b1 == basic_undefined && b2 != basic_undefined)
	return FALSE;
    else if (b1 != basic_undefined && b2 == basic_undefined)
	return FALSE;
    else if (basic_tag(b1) != basic_tag(b2))
	return FALSE;

    /* assertion: b1 and b2 are defined and have the same tag
       (see previous tests) */

    switch(basic_tag(b1)) {
    case is_basic_int:
	return basic_int(b1) == basic_int(b2);
    case is_basic_float:
	return basic_float(b1) == basic_float(b2);
    case is_basic_logical:
	return basic_logical(b1) == basic_logical(b2);
    case is_basic_overloaded:
	return TRUE;
    case is_basic_complex:
	return basic_complex(b1) == basic_complex(b2);
    case is_basic_string:
      /* Do we want string types to be equal only if lengths are equal?
       * I do not think so
       */
      /*
	pips_error("basic_equal_p",
		   "string type comparison not implemented\n");
		   */
	/* could be a star or an expression; a value_equal_p() is needed! */
	return TRUE;
    default: pips_error("basic_equal_p", "unexpected tag %d\n", basic_tag(b1));
    }
    return FALSE; /* just to avoid a warning */
}

bool 
functional_equal_p(f1, f2)
functional f1;
functional f2;
{
    if(f1 == f2)
	return TRUE;
    else if (f1 == functional_undefined && f2 != functional_undefined)
	return FALSE;
    else if (f1 != functional_undefined && f2 == functional_undefined)
	return FALSE;
    else {
	list lp1 = functional_parameters(f1);
	list lp2 = functional_parameters(f2);

	if(gen_length(lp1) != gen_length(lp2))
	    return FALSE;

	for( ; !ENDP(lp1); POP(lp1), POP(lp2)) {
	    parameter p1 = PARAMETER(CAR(lp1));
	    parameter p2 = PARAMETER(CAR(lp2));

	    if(!parameter_equal_p(p1, p2))
		return FALSE;
	}

	return type_equal_p(functional_result(f1), functional_result(f2));
    }
}

bool 
parameter_equal_p(p1, p2)
parameter p1;
parameter p2;
{
    if(p1 == p2)
	return TRUE;
    else if (p1 == parameter_undefined && p2 != parameter_undefined)
	return FALSE;
    else if (p1 != parameter_undefined && p2 == parameter_undefined)
	return FALSE;
    else
	return type_equal_p(parameter_type(p1), parameter_type(p2))
	    && mode_equal_p(parameter_mode(p1), parameter_mode(p2));
}

bool 
mode_equal_p(m1, m2)
mode m1;
mode m2;
{
    if(m1 == m2)
	return TRUE;
    else if (m1 == mode_undefined && m2 != mode_undefined)
	return FALSE;
    else if (m1 != mode_undefined && m2 == mode_undefined)
	return FALSE;
    else 
	return mode_tag(m1) == mode_tag(m2);
}

int 
string_type_size(b)
basic b;
{
    int size = -1;
    value v = basic_string(b);
    constant c = constant_undefined;

    switch(value_tag(v)) {
    case is_value_constant:
      c = value_constant(v);
      if(constant_int_p(c))
	size = constant_int(c);
      else
	pips_error("string_size_type", "Non-integer constant to size a string");
      break;
    default:
	pips_error("string_size_type", "Non-constant value to size a string");
    }

    return size;
}

/* See also SizeOfElements() */
int 
basic_type_size(b)
basic b;
{
    int size = -1;

    switch(basic_tag(b)) {
    case is_basic_int: size = basic_int(b);
	break;
    case is_basic_float: size = basic_float(b);
	break;
    case is_basic_logical: size = basic_logical(b);
	break;
    case is_basic_overloaded: 
	pips_error("basic_type_size", "undefined for type overloaded\n");
	break;
    case is_basic_complex: size = basic_complex(b);
	break;
    case is_basic_string: 
      /* pips_error("basic_type_size", "undefined for type string\n"); */
      size = string_type_size(b);
	break;
    default: size = basic_int(b);
	pips_error("basic_type_size", "ill. tag %d\n", basic_tag(b));
	break;
    }

    return size;
}


/*
 * See basic_of_expression() which is much more comprehensive
 * Intrinsic overloading is not resolved!
 *
 * WARNING: a pointer to an existing data structure is returned.
 */
basic 
expression_basic(expr)
expression expr;
{
    syntax the_syntax=expression_syntax(expr);
    basic b = basic_undefined;

    switch(syntax_tag(the_syntax))
    {
    case is_syntax_reference:
	b = entity_basic(reference_variable(syntax_reference(the_syntax)));
	break;
    case is_syntax_range:
	/* should be int */
	b = expression_basic(range_lower(syntax_range(the_syntax)));
	break;
    case is_syntax_call:
	/*
	 * here is a little problem with pips...
	 * every intrinsics are overloaded, what is not 
	 * exactly what is desired...
	 */
      	return(entity_basic(call_function(syntax_call(the_syntax))));
	break;
    default:
	pips_error("expression_basic", "unexpected syntax tag\n");
	break;
    }

    return b;
}

dimension 
dimension_dup(d)
dimension d;
{
    return(make_dimension(expression_dup(dimension_lower(d)),
			  expression_dup(dimension_upper(d))));
}

list 
ldimensions_dup(l)
list l;
{
    list 
	result = NIL ;

    MAPL(cd,
     {
	 result = CONS(DIMENSION, dimension_dup(DIMENSION(CAR(cd))), 
		       result);
     },
	 l);

    return(gen_nreverse(result));
}

dimension 
FindIthDimension(e, i)
entity e;
int i;
{
    cons * pc;

    if (!type_variable_p(entity_type(e))) 
	pips_error("FindIthDimension", "not a variable\n");

    if (i <= 0)
	pips_error("FindIthDimension", "invalid dimension\n");

    pc = variable_dimensions(type_variable(entity_type(e)));

    while (pc != NULL && --i > 0)
	pc = CDR(pc);

    if (pc == NULL) 
	pips_error("FindIthDimension", "not enough dimensions\n");

    return(DIMENSION(CAR(pc)));
}

/*
 * returns a string defining a type.
 */
string 
type_to_string(type t)
{
    switch (type_tag(t))
    {
    case is_type_statement:
	return "statement";
    case is_type_area:
	return "area";
    case is_type_variable:
	return "variable";
    case is_type_functional:
	return "functional";
    case is_type_unknown:
	return "unknow";
    case is_type_void:
	return "void";
    default: break;
    }

    pips_error("type_to_string", 
	       "unexpected type: 0x%x (tag=%d)",
	       t,
	       type_tag(t));

    return(string_undefined); /* just to avoid a gcc warning */
}

/*
 * returns the string to declare a basic type.
 */
string 
basic_to_string(b)
basic b;
{
    static char char_decl[20]; /* ??? hummm... */
    int lng = 0;

    switch (basic_tag(b))
    {
    case is_basic_int:
	switch(basic_int(b))
	{
	case 2: return("INTEGER*2") ;
	case 4: return("INTEGER*4") ;
	case 8: return("INTEGER*8") ;
	default: break;
	}
    case is_basic_float:
	switch(basic_float(b))
	{
	case 4: return("REAL*4") ;
	case 8: return("REAL*8") ;
	default: break;
	}
    case is_basic_logical:
	switch(basic_logical(b))
	{
	case 2: return("LOGICAL*2") ;
	case 4: return("LOGICAL*4") ;
	case 8: return("LOGICAL*8") ;
	default: break;
	}
    case is_basic_complex:
	switch(basic_complex(b))
	{
	case 8: return("COMPLEX*8") ;
	case 16: return("COMPLEX*16") ;
	default: break;
	}
    case is_basic_string:
	if(value_constant_p(basic_string(b))
	   && constant_int_p(value_constant(basic_string(b)))) {
	    lng = constant_int(value_constant(basic_string(b)));
	    sprintf(&char_decl[0],"CHARACTER*%d", lng);
	}
	else if(value_symbolic_p(basic_string(b))
      && constant_int_p(symbolic_constant(value_symbolic(basic_string(b))))) {
       lng = constant_int(symbolic_constant(value_constant(basic_string(b))));
	sprintf(&char_decl[0],"CHARACTER*%d", lng);
	}
	else {
	    lng = -1;
	    sprintf(&char_decl[0],"CHARACTER**");
	}
	pips_assert("basic_to_string", strlen(char_decl)<20);
	return(char_decl);
    case is_basic_overloaded:
	return("OVERLOADED");
    default: break;
    }

    pips_error("basic_to_string", 
	       "unexpected basic: 0x%x (tag=%d)",
	       b,
	       basic_tag(b));

    return(string_undefined); /* just to avoid a gcc warning */
}

/* basic basic_of_expression(expression exp): Makes a basic of the same
 * basic as the expression "exp". Indeed, "exp" will be assigned to
 * a temporary variable, which will have the same declaration as "exp".
 *
 * WARNING: a new basic object is allocated
 */
basic 
basic_of_expression(exp)
expression exp;
{
  syntax sy = expression_syntax(exp);
  basic b = basic_undefined;

  debug(6, "basic_of_expression", "\n");

  switch(syntax_tag(sy)) {
  case is_syntax_reference:
    {
      type exp_type = entity_type(reference_variable(syntax_reference(sy)));

      if (type_tag(exp_type) != is_type_variable)
	pips_error("basic_of_expression", "Bad reference type tag");
      b = copy_basic(variable_basic(type_variable(exp_type)));
      break;
    }
  case is_syntax_call: 
    b = basic_of_call(syntax_call(sy));
    break;
  case is_syntax_range: 
    /* Well, let's assume range are well formed... */
    b = basic_of_expression(range_lower(syntax_range(sy)));
    break;
  default: pips_error("basic_of_expression", "Bad syntax tag");
    /* Never go there... */
    b = make_basic(is_basic_overloaded, 4);
  }

  return b;
}

/* basic basic_of_call(call c): returns the basic of the result given by the
 * call "c".
 *
 * WARNING: a new basic is allocated
 */
basic 
basic_of_call(c)
call c;
{
  entity e = call_function(c);
  tag t = value_tag(entity_initial(e));
  basic b = basic_undefined;

  switch (t)
    {
    case is_value_code:
      b = copy_basic(basic_of_external(c));
      break;
    case is_value_intrinsic: 
      b = basic_of_intrinsic(c);
      break;
    case is_value_symbolic: 
      b = make_basic(is_basic_overloaded, UU);
      break;
    case is_value_constant:
      b = copy_basic(basic_of_constant(c));
      break;
    case is_value_unknown:
      debug(1, "basic_of_call", "function %s has no initial value.\n"
	    " Maybe it has not been parsed yet.\n",
	    entity_name(e));
      b = copy_basic(basic_of_external(c));
      break;
    default: pips_error("basic_of_call", "unknown tag %d\n", t);
      /* Never go there... */
    }
  return b;
}



/* basic basic_of_external(call c): returns the basic of the result given by
 * the call to an external function.
 *
 * WARNING: returns a pointer
 */
basic 
basic_of_external(c)
call c;
{
  type call_type, return_type;

  debug(7, "basic_of_call", "External call\n");

  call_type = entity_type(call_function(c));
  if (type_tag(call_type) != is_type_functional)
    pips_error("basic_of_external", "Bad call type tag");

  return_type = functional_result(type_functional(call_type));
  if (type_tag(return_type) != is_type_variable)
    pips_error("basic_of_external", "Bad return call type tag");

  return(variable_basic(type_variable(return_type)));
}

/* basic basic_of_intrinsic(call c): returns the basic of the result given by
 * call to an intrinsic function. This basic must be computed with the
 * basic of the arguments of the intrinsic.
 *
 * WARNING: returns a newly allocated basic object
 */
basic 
basic_of_intrinsic(c)
call c;
{
  basic rb;
  entity call_func;

  debug(7, "basic_of_call", "Intrinsic call\n");

  call_func = call_function(c);

  if(ENTITY_LOGICAL_OPERATOR_P(call_func))
    rb = make_basic(is_basic_logical, 4);
  else
    {
      list call_args = call_arguments(c);

      if (call_args == NIL) {
	/* I don't know the type since there is no arguments !
	   Bug encountered with a FMT=* in a PRINT.
	   RK, 21/02/1994 : */
	rb = make_basic(is_basic_overloaded, UU);
	/* rb = make_basic(is_basic_int, 4); */
      }
      else {
	expression arg1, arg2;

	arg1 = EXPRESSION(CAR(call_args));
	if(CDR(call_args) == NIL)
	  rb = basic_of_expression(arg1);
	else
	  {
	    arg2 = EXPRESSION(CAR(CDR(call_args)));
	    rb = basic_union(arg1, arg2);
	  }

      }
    }

  return rb;
}

/* basic basic_of_constant(call c): returns the basic of the call to a
 * constant.
 *
 * WARNING: returns a pointer towards an existing data structure
 */
basic 
basic_of_constant(c)
call c;
{
  type call_type, return_type;

  debug(7, "basic_of_constant", "Constant call\n");

  call_type = entity_type(call_function(c));

  if (type_tag(call_type) != is_type_functional)
    pips_error("basic_of_constant", "Bad call type tag");

  return_type = functional_result(type_functional(call_type));

  if (type_tag(return_type) != is_type_variable)
    pips_error("basic_of_constant", "Bad return call type tag");

  return(variable_basic(type_variable(return_type)));
}

/* basic basic_union(expression exp1 exp2): returns the basic of the
 * expression which has the most global basic. Then, between "int" and
 * "float", the most global is "float".
 *
 * Note: there are two different "float" : DOUBLE PRECISION and REAL.
 *
 * WARNING: a new basic data structure is allocated (because you cannot
 * always find a proper data structure to return simply a pointer
 */
basic 
basic_union(exp1, exp2)
expression exp1, exp2;
{
  basic b1 = basic_of_expression(exp1);
  basic b2 = basic_of_expression(exp2);
  basic b = basic_undefined;

  /* FI: I do not believe this is correct for all intrinsics! */

  debug(7, "basic_union", "Tags: tag exp1 = %d, tag exp2 = %d\n",
	basic_tag(b1), basic_tag(b2));


  if(basic_overloaded_p(b2)) {
    b = copy_basic(b2);
  }
  else {
    switch(basic_tag(b1)) {

    case is_basic_overloaded:
      b = copy_basic(b1);
      break;

    case is_basic_string: 
      if(basic_string_p(b2)) {
	int s1 = SizeOfElements(b1);
	int s2 = SizeOfElements(b2);

	/* Type checking problem for ? : with gcc... */
	if(s1>s2)
	  b = copy_basic(b1);
	else
	  b = copy_basic(b2);
      }
      else
	b = make_basic(is_basic_overloaded, UU);
      break;

    case is_basic_logical:
      if(basic_logical_p(b2)) {
	int s1 = basic_logical(b1);
	int s2 = basic_logical(b2);

	b = make_basic(is_basic_logical,s1>s2?s1:s2);
      }
      else
	b = make_basic(is_basic_overloaded, UU);
      break;

    case is_basic_complex:
      if(basic_complex_p(b2) || basic_float_p(b2) || basic_int_p(b2)) {
	int s1 = SizeOfElements(b1);
	int s2 = SizeOfElements(b2);

	b = make_basic(is_basic_complex, s1>s2?s1:s2);
      }
      else
	b = make_basic(is_basic_overloaded, UU);
      break;

    case is_basic_float:
      if(basic_complex_p(b2)) {
	int s1 = SizeOfElements(b1);
	int s2 = SizeOfElements(b2);

	b = make_basic(is_basic_complex, s1>s2?s1:s2);
      }
      else if(basic_float_p(b2) || basic_int_p(b2)) {
	int s1 = SizeOfElements(b1);
	int s2 = SizeOfElements(b2);

	b = make_basic(is_basic_float, s1>s2?s1:s2);
      }
      else
	b = make_basic(is_basic_overloaded, UU);
      break;

    case is_basic_int:
      if(basic_complex_p(b2) || basic_float_p(b2)) {
	int s1 = SizeOfElements(b1);
	int s2 = SizeOfElements(b2);

	b = make_basic(basic_tag(b2), s1>s2?s1:s2);
      }
      else if(basic_int_p(b2)) {
	int s1 = SizeOfElements(b1);
	int s2 = SizeOfElements(b2);

	b = make_basic(is_basic_int, s1>s2?s1:s2);
      }
      else
	b = make_basic(is_basic_overloaded, UU);
      break;

    default: pips_error("basic_union", "Ill. basic tag %d\n", basic_tag(b1));
    }
  }

  return b;

  /*
    if( (t1 != is_basic_complex) && (t1 != is_basic_float) &&
    (t1 != is_basic_int) && (t2 != is_basic_complex) &&
    (t2 != is_basic_float) && (t2 != is_basic_int) )
    pips_error("basic_union",
    "Bad basic tag for expression in numerical function");

    if(t1 == is_basic_complex)
    return(b1);
    if(t2 == is_basic_complex)
    return(b2);
    if(t1 == is_basic_float) {
    if( (t2 != is_basic_float) ||
    (basic_float(b1) == DOUBLE_PRECISION_SIZE) )
    return(b1);
    return(b2);
    }
    if(t2 == is_basic_float)
    return(b2);
    return(b1);
    */
}

bool 
overloaded_type_p(type t)
{
  pips_assert("overloaded_type_p", type_variable_p(t));

  return basic_overloaded_p(variable_basic(type_variable(t)));
}

/* bool is_inferior_basic(basic1, basic2)
 * return TRUE if basic1 is less complex than basic2
 * ex:  int is less complex than float*4,
 *      float*4 is less complex than float*8, ...
 * - overloaded is inferior to any basic.
 * - logical is inferior to any other but overloaded.
 * - string is inferior to any other but overloaded and logical.
 * Used to decide that the sum of an int and a float
 * is a floating-point addition (for ex.)
 */
bool
is_inferior_basic(b1, b2)
basic b1, b2;
{
    if ( b1 == basic_undefined ) 
	pips_error("is_inferior_basic", "first  basic_undefined\n");
    else if ( b2 == basic_undefined )
	pips_error("is_inferior_basic", "second basic_undefined\n");

    if (basic_overloaded_p(b1))
	return (TRUE);
    else if (basic_overloaded_p(b2))
	return (FALSE);
    else if (basic_logical_p(b1))
	return (TRUE);
    else if (basic_logical_p(b2))
	return (FALSE);
    else if (basic_string_p(b1))
	return (TRUE);
    else if (basic_string_p(b2))
	return (FALSE);
    else if (basic_int_p(b1)) {
	if (basic_int_p(b2))
	    return (basic_int(b1) <= basic_int(b2));
	else
	    return (TRUE);
    }
    else if (basic_float(b1)) {
	if (basic_int_p(b2))
	    return (FALSE);
	else if (basic_float_p(b2))
	    return (basic_float(b1) <= basic_float(b2));
	else
	    return (TRUE);
    }
    else if (basic_complex_p(b1)) {
	if (basic_int_p(b2) || basic_float_p(b2))
	    return (FALSE);
	else if (basic_complex(b2))
	    return (basic_complex(b1) <= basic_complex(b2));
	else
	    return (TRUE);
    }
    else
	pips_error("is_inferior_basic", "Case never occurs.\n");
    return (TRUE);
}

basic 
simple_basic_dup(b)
basic b;
{
    /* basic_int, basic_float, basic_logical, basic_complex are all int's */
    /* so we duplicate them the same manner: with basic_int. */
    if (basic_int_p(b)     || basic_float_p(b) || 
	basic_logical_p(b) || basic_complex_p(b))
	return(make_basic(basic_tag(b), basic_int(b)));
    else if (basic_overloaded_p(b))
	return(make_basic(is_basic_overloaded, UU));
    else {
	user_warning("simple_basic_dup",
		     "(tag %d) isn't that simple\n", basic_tag(b));
	if (basic_string_p(b))
	    fprintf(stderr, "string: value tag = %d\n", 
		             value_tag(basic_string(b)));
	return (make_basic(basic_tag(b), basic_int(b))); 
    }
}

/*
 *  that is all
 */

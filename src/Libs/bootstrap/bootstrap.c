/* Symbol table initialization with Fortran operators, commands and intrinsics
   
   More information is provided in effects/effects.c
   
   Remi Triolet
   
   Modifications:
   - add intrinsics according to Fortran standard Table 5, pp. 15.22-15-25,
   Francois Irigoin, 02/06/90
   - add .SEQ. to handle ranges outside of arrays [pj]
   - add intrinsic DFLOAT. bc. 13/1/96.
   - add pseudo-intrinsics SUBSTR and ASSIGN_SUBSTR to handle strings,
   fi, 25/12/96
   Bugs:
   - intrinsics are not properly typed
*/

#include <stdio.h>
#include <string.h>
/* #include <values.h> */
#include <limits.h>
#include <stdlib.h>

#include "linear.h"

#include "genC.h"
#include "ri.h"
#include "ri-util.h"
#include "makefile.h"
#include "database.h"

#include "bootstrap.h"

#include "misc.h"
#include "pipsdbm.h"
#include "parser_private.h"
#include "syntax.h"
#include "constants.h"
#include "resources.h"

#define LOCAL static

/* CLASSIFICATION OF BASIC */
#define basic_numeric_simple_p(b) (basic_int_p(b) || basic_float_p(b))
#define basic_numeric_p(b) (basic_numeric_simple_p(b) || basic_complex_p(b))
#define basic_compatible_simple_p(b1, b2) (\
                (basic_numeric_simple_p(b1) && basic_numeric_simple_p(b2)) ||\
                (basic_string_p(b1) && basic_string_p(b2)) ||\
                (basic_logical_p(b1) && basic_logical_p(b2)) ||\
                (basic_overloaded_p(b1) && basic_overloaded_p(b2)) ||\
                (basic_undefined_p(b1) && basic_undefined_p(b2)))
#define basic_compatible_p(b1, b2) (\
                (basic_numeric_p(b1) && basic_numeric_p(b2)) ||\
                (basic_string_p(b1) && basic_string_p(b2)) ||\
                (basic_logical_p(b1) && basic_logical_p(b2)) ||\
                (basic_overloaded_p(b1) && basic_overloaded_p(b2)) ||\
                (basic_undefined_p(b1) && basic_undefined_p(b2)))

/* Working with hash_table of basic
 */
#define GET_TYPE(h, e) ((basic)hash_get(h, (char*)(e)))
#define PUT_TYPE(h, e, b) hash_put(h, (char*)(e), (char*)(b))

extern expression 
insert_cast(basic cast, basic from, expression exp, type_context_p);

void 
CreateAreas()
{
  make_entity(AddPackageToName(TOP_LEVEL_MODULE_NAME, 
			       DYNAMIC_AREA_LOCAL_NAME),
	      make_type(is_type_area, make_area(0, NIL)),
	      make_storage(is_storage_rom, UU),
	      make_value(is_value_unknown, UU));
  
  
  make_entity(AddPackageToName(TOP_LEVEL_MODULE_NAME, 
			       STATIC_AREA_LOCAL_NAME),
	      make_type(is_type_area, make_area(0, NIL)),
	      make_storage(is_storage_rom, UU),
	      make_value(is_value_unknown, UU));
}

void 
CreateArrays()
{
  /* First a dummy function - close to C one "crt0()" - in order to
     - link the next entity to its ram
     - make an unbounded dimension for this entity
  */
  
  entity ent;
  
  ent = make_entity(AddPackageToName(TOP_LEVEL_MODULE_NAME,
				     IO_EFFECTS_PACKAGE_NAME),
		    make_type(is_type_functional,
			      make_functional(NIL,make_type(is_type_void,
							    NIL))),
		    make_storage(is_storage_rom, UU),
		    make_value(is_value_code,make_code(NIL, strdup(""))));
  
  set_current_module_entity(ent);
  
  make_entity(AddPackageToName(IO_EFFECTS_PACKAGE_NAME, 
			       STATIC_AREA_LOCAL_NAME),
	      make_type(is_type_area, make_area(0, NIL)),
	      make_storage(is_storage_rom, UU),
	      make_value(is_value_unknown, UU));
  
  /* GO: entity for io logical units: It is an array which*/
  make_entity(AddPackageToName(IO_EFFECTS_PACKAGE_NAME,
			       IO_EFFECTS_ARRAY_NAME),
	      MakeTypeArray(make_basic(is_basic_int,
				     UUINT(IO_EFFECTS_UNIT_SPECIFIER_LENGTH)),
			    CONS(DIMENSION,
				 make_dimension
				 (MakeIntegerConstantExpression("0"),
				  /*
				    MakeNullaryCall
				    (CreateIntrinsic(UNBOUNDED_DIMENSION_NAME))
				  */
				  MakeIntegerConstantExpression("2000")
				  ),
				 NIL)),
	      /* make_storage(is_storage_ram,
		 make_ram(entity_undefined, DynamicArea, 0, NIL))
	      */
	      make_storage(is_storage_ram,
			   make_ram(ent,
			       global_name_to_entity(IO_EFFECTS_PACKAGE_NAME, 
						     STATIC_AREA_LOCAL_NAME),
				    0, NIL)),
	      make_value(is_value_unknown, UU));
  
  /* GO: entity for io logical units: It is an array which*/
  make_entity(AddPackageToName(IO_EFFECTS_PACKAGE_NAME,
			       IO_EOF_ARRAY_NAME),
	      MakeTypeArray(make_basic(is_basic_logical,
				     UUINT(IO_EFFECTS_UNIT_SPECIFIER_LENGTH)),
			    CONS(DIMENSION,
				 make_dimension
				 (MakeIntegerConstantExpression("0"),
				  /*
				    MakeNullaryCall
				    (CreateIntrinsic(UNBOUNDED_DIMENSION_NAME))
				  */
				  MakeIntegerConstantExpression("2000")
				  ),
				 NIL)),
	      /* make_storage(is_storage_ram,
		 make_ram(entity_undefined, DynamicArea, 0, NIL))
	      */
	      make_storage(is_storage_ram,
			   make_ram(ent,
				global_name_to_entity(IO_EFFECTS_PACKAGE_NAME, 
						      STATIC_AREA_LOCAL_NAME),
				    0, NIL)),
	      make_value(is_value_unknown, UU));
  
  /* GO: entity for io logical units: It is an array which*/
  make_entity(AddPackageToName(IO_EFFECTS_PACKAGE_NAME,
			       IO_ERROR_ARRAY_NAME),
	      MakeTypeArray(make_basic(is_basic_logical,
				     UUINT(IO_EFFECTS_UNIT_SPECIFIER_LENGTH)),
			    CONS(DIMENSION,
				 make_dimension
				 (MakeIntegerConstantExpression("0"),
				  /*
				    MakeNullaryCall
				    (CreateIntrinsic(UNBOUNDED_DIMENSION_NAME))
				  */
				  MakeIntegerConstantExpression("2000")
				  ),
				 NIL)),
	      /* make_storage(is_storage_ram,
		 make_ram(entity_undefined, DynamicArea, 0, NIL))
	      */
	      make_storage(is_storage_ram,
			   make_ram(ent,
				global_name_to_entity(IO_EFFECTS_PACKAGE_NAME, 
						      STATIC_AREA_LOCAL_NAME),
				    0, NIL)),
	      make_value(is_value_unknown, UU));
  
  reset_current_module_entity();
}

static list
make_parameter_list(int n, parameter (* mkprm)(void))
{
  list l = NIL;
  
  if (n < (INT_MAX)) 
  {
    int i = n;
    while (i-- > 0) 
    {
      l = CONS(PARAMETER, mkprm(), l);
    }
  }
  else 
  {
    /* varargs */
    parameter p = mkprm();
    type pt = copy_type(parameter_type(p));
    type v = make_type(is_type_varargs, pt);
    parameter vp = make_parameter(v, make_mode(is_mode_reference, UU));
    
    l = CONS(PARAMETER, vp, l);
    free_parameter(p);
  }
  return l;
}

/* The default intrinsic type is a functional type with n overloaded
 * arguments returning an overloaded result if the arity is known.
 * If the arity is unknown, the default intrinsic type is a 0-ary
 * functional type returning an overloaded result.
 */

static type 
default_intrinsic_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeOverloadedResult());
  t = make_type(is_type_functional, ft);
  
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  return t;
}

static type 
overloaded_to_integer_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeIntegerResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
overloaded_to_real_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeRealResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
overloaded_to_double_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoubleprecisionResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
overloaded_to_complex_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeComplexResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
overloaded_to_doublecomplex_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoublecomplexResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
overloaded_to_logical_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeLogicalResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeOverloadedParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
integer_to_integer_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeIntegerResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeIntegerParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
integer_to_real_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeRealResult());
  functional_parameters(ft) = make_parameter_list(n, MakeIntegerParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
integer_to_double_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoubleprecisionResult());
  functional_parameters(ft) = make_parameter_list(n, MakeIntegerParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
real_to_integer_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeIntegerResult());
  functional_parameters(ft) = make_parameter_list(n, MakeRealParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
real_to_real_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeRealResult());
  functional_parameters(ft) = make_parameter_list(n, MakeRealParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
real_to_double_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoubleprecisionResult());
  functional_parameters(ft) = make_parameter_list(n, MakeRealParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
double_to_integer_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeIntegerResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeDoubleprecisionParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
double_to_real_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeRealResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeDoubleprecisionParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
double_to_double_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoubleprecisionResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeDoubleprecisionParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
complex_to_real_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeRealResult());
  functional_parameters(ft) = make_parameter_list(n, MakeComplexParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
doublecomplex_to_double_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoubleprecisionResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeDoublecomplexParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
complex_to_complex_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeComplexResult());
  functional_parameters(ft) = make_parameter_list(n, MakeComplexParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
doublecomplex_to_doublecomplex_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeDoublecomplexResult());
  functional_parameters(ft) = 
    make_parameter_list(n, MakeDoublecomplexParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
character_to_integer_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeIntegerResult());
  functional_parameters(ft) = make_parameter_list(n, MakeCharacterParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
character_to_logical_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeLogicalResult());
  functional_parameters(ft) = make_parameter_list(n, MakeCharacterParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
character_to_character_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeCharacterResult());
  functional_parameters(ft) = make_parameter_list(n, MakeCharacterParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
substring_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeCharacterResult());
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeIntegerParameter(), NIL);
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeIntegerParameter(),
	 functional_parameters(ft));
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeCharacterParameter(),
	 functional_parameters(ft));
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
assign_substring_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeCharacterResult());
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeCharacterParameter(), NIL);
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeIntegerParameter(),
	 functional_parameters(ft));
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeIntegerParameter(),
	 functional_parameters(ft));
  functional_parameters(ft) = 
    CONS(PARAMETER, MakeCharacterParameter(),
	 functional_parameters(ft));
  t = make_type(is_type_functional, ft);
  
  return t;
}

static type 
logical_to_logical_type(int n)
{
  type t = type_undefined;
  functional ft = functional_undefined;
  
  ft = make_functional(NIL, MakeLogicalResult());
  functional_parameters(ft) = make_parameter_list(n, MakeCharacterParameter);
  t = make_type(is_type_functional, ft);
  
  return t;
}

/***************************************************** TYPE A CALL FUNCTIONS */

/* Determine the longest basic among the arguments of c
 */
static basic 
basic_union_arguments(call c, hash_table types)
{
  basic b2, b1 = basic_undefined;
  
  MAP(EXPRESSION, e,
  {
    if (b1==basic_undefined)
    {
      /* first time */
      b1 = GET_TYPE(types, e);
    }
    else 
    {
      /* after first argument */
      b2 = GET_TYPE(types, e);
      if (is_inferior_basic(b1, b2))
	b1 = b2;
    }
  },
      call_arguments(c));
  
  return b1==basic_undefined? b1: copy_basic(b1);
}

/**************** CHECK THE VALIDE OF ARGUMENTS BASIC OF FUNCTION ************/
/* Verify if all the arguments basic of function C are INTEGER
 * If there is no argument, I return TRUE
 */
static bool 
check_if_basics_ok(list le, hash_table types, bool(*basic_ok)(basic))
{
  MAP(EXPRESSION, e, 
  {
    if (!basic_ok(GET_TYPE(types, e))) 
    {
      return FALSE;
    }
  }
      , le);
  
  return TRUE;
}

static bool 
is_basic_int_p(basic b) 
{ 
  return basic_int_p(b); 
}
static bool 
is_basic_real_p(basic b) 
{ 
  return basic_float_p(b) && basic_float(b)==4; 
}
static bool 
is_basic_double_p(basic b) 
{ 
  return basic_float_p(b) && basic_float(b)==8; 
}
static bool 
is_basic_complex_p(basic b) 
{ 
  return basic_complex_p(b) && basic_complex(b)==8; 
}
static bool 
is_basic_dcomplex_p(basic b) 
{ 
  return basic_complex_p(b) && basic_complex(b)==16; 
}

static bool 
arguments_are_integer(call c, hash_table types)
{
  return check_if_basics_ok(call_arguments(c), types, is_basic_int_p);
}

static bool 
arguments_are_real(call c, hash_table types)
{
  return check_if_basics_ok(call_arguments(c), types, is_basic_real_p);
}
static bool 
arguments_are_double(call c, hash_table types)
{
  return check_if_basics_ok(call_arguments(c), types, is_basic_double_p);
}
static bool 
arguments_are_complex(call c, hash_table types)
{
  return check_if_basics_ok(call_arguments(c), types, is_basic_complex_p);
}
static bool 
arguments_are_dcomplex(call c, hash_table types)
{
  return check_if_basics_ok(call_arguments(c), types, is_basic_dcomplex_p);
}

/***************************************************************************** 
 * Verify if all the arguments basic of function C are REAL and DOUBLE
 * If there is no argument, I return TRUE
 *
 * Note: I - Integer; R - Real; D - Double; C - Complex
 */
static bool 
arguments_are_RD(call c, hash_table types)
{
  basic b;
  list args = call_arguments(c);
  
  while (args != NIL)
  {
    b = GET_TYPE(types, EXPRESSION(CAR(args)));
    if ( !basic_float_p(b) )
    {
      return FALSE;
    }
    args = CDR(args);
  }
  return TRUE;
}

static bool 
arguments_are_IR(call c, hash_table types)
{
  basic b;
  list args = call_arguments(c);
  
  while (args != NIL)
  {
    b = GET_TYPE(types, EXPRESSION(CAR(args)));
    if ( !basic_int_p(b) && !(basic_float_p(b) && basic_float(b)==4))
    {
      return FALSE;
    }
    args = CDR(args);
  }
  return TRUE;
}
static bool 
arguments_are_IRD(call c, hash_table types)
{
  basic b;
  list args = call_arguments(c);
  
  while (args != NIL)
  {
    b = GET_TYPE(types, EXPRESSION(CAR(args)));
    if ( !basic_int_p(b) && !basic_float_p(b))
    {
      return FALSE;
    }
    args = CDR(args);
  }
  return TRUE;
}
/***************************************************************************** 
 * Verify if all the arguments basic of function C are REAL, DOUBLE and COMPLEX
 * According to (ANSI X3.9-1978 FORTRAN 77, Table 2 & 3, Page 6-5 & 6-6),
 * it is prohibited an arithetic operator operaters on 
 * a pair of DOUBLE and COMPLEX, so that I return FALSE in that case.
 *
 * PDSon: If there is no argument, I return TRUE
 */
static bool 
arguments_are_RDC(call c, hash_table types)
{
  basic b;
  bool arg_double, arg_cmplx;    
  list args = call_arguments(c);
  
  arg_double = FALSE;
  arg_cmplx = FALSE;
  
  MAP(EXPRESSION, e,
  {
    b = GET_TYPE(types, e);
    if ( !basic_float_p(b) &&
	 !(basic_complex_p(b) && basic_complex(b) == 8))
    {
      return FALSE;
    }
    else if ((arg_cmplx && basic_float_p(b) && basic_float(b) == 8)||
	     (arg_double && basic_complex_p(b) && basic_complex(b) == 8))
    {
      return FALSE;	    
    }
    else if (!arg_double && basic_float_p(b) && basic_float(b) == 8)
    {
      arg_double = TRUE;
    }
    else if (!arg_cmplx && basic_complex_p(b) && basic_complex(b) == 8)
    {
      arg_cmplx = TRUE;
    }
  }
      , args);
  
  return TRUE;
}
static bool 
arguments_are_IRDC(call c, hash_table types)
{
  basic b;
  bool arg_double, arg_cmplx;    
  list args = call_arguments(c);
  
  arg_double = FALSE;
  arg_cmplx = FALSE;
  
  MAP(EXPRESSION, e,
  {
    b = GET_TYPE(types, e);
    if ( !basic_int_p(b) && 
	 !basic_float_p(b) &&
	 !(basic_complex_p(b) && basic_complex(b) == 8))
    {
      return FALSE;
    }
    else if ((arg_cmplx && basic_float_p(b) && basic_float(b) == 8)||
	     (arg_double && basic_complex_p(b) && basic_complex(b) == 8))
    {
      return FALSE;	    
    }
    else if (!arg_double && basic_float_p(b) && basic_float(b) == 8)
    {
      arg_double = TRUE;
    }
    else if (!arg_cmplx && basic_complex_p(b) && basic_complex(b) == 8)
    {
      arg_cmplx = TRUE;
    }
  }
      , args);
  
  return TRUE;
}

static bool
arguments_are_character(call c, hash_table types)
{
  basic b;
  list args = call_arguments(c);
  
  while (args != NIL)
  {
    b = GET_TYPE(types, EXPRESSION(CAR(args)));
    if (!basic_string_p(b))
    {
      return FALSE;
    }
    args = CDR(args);
  }
  return TRUE;
}
static bool
arguments_are_logical(call c, hash_table types)
{
  basic b;
  list args = call_arguments(c);
  
  while (args != NIL)
  {
    b = GET_TYPE(types, EXPRESSION(CAR(args)));
    if (!basic_logical_p(b))
    {
      return FALSE;
    }
    args = CDR(args);
  }
  return TRUE;
}
/***************************************************************************** 
 * Verification if all the arguments are compatible
 * PDSon: If #arguments <=1, I return true
 */
static bool
arguments_are_compatible(call c, hash_table types)
{
  basic b1, b2;
  b1 = basic_undefined;
  
  MAP(EXPRESSION, e,
  { 
    /* First item */
    if(basic_undefined_p(b1))
    {
      b1 = GET_TYPE(types, e);
    }
    /* Next item */
    else
    {
      b2 = GET_TYPE(types, e);
      if(!basic_compatible_p(b1, b2))
      {
	return FALSE;
      }
    }
  }
      , call_arguments(c));
  
  return TRUE;
}

/***************************************************************************** 
 * Typing all the arguments of c to basic b if their basic <> b
 */
static void 
typing_arguments(call c, type_context_p context, basic b)
{
  basic b1;
  list args = call_arguments(c);
  
  while (args != NIL)
  {
    b1 = GET_TYPE(context->types, EXPRESSION(CAR(args)));
    if (!basic_equal_p(b, b1))
    {
      EXPRESSION(CAR(args)) =
	insert_cast(b, b1, EXPRESSION(CAR(args)), context);
      /* Update hash table */
      PUT_TYPE(context->types, EXPRESSION(CAR(args)), b);
    }
    args = CDR(args);
  }
}

/***************************************************************************** 
 *                           TYPING THE INTRINSIC FUNCTIONS
 * Typing assignment statement (=)
 */
static basic
typing_assignment(call c, type_context_p context)
{
  basic b1, b2;
  list args = call_arguments(c);
  
  if(!arguments_are_compatible(c, context->types))
  {
    add_one_line_of_comment((statement) stack_head(context->stats), 
			    "Arguments of assignement '%s' are not compatible", 
			    entity_local_name(call_function(c))); 
    /* Count the number of errors */
    context->number_of_error++;
    /* Just for return a result */
    return make_basic_float(4); 
  }
  
  b1 = GET_TYPE(context->types, EXPRESSION(CAR(args)));
  b2 = GET_TYPE(context->types, EXPRESSION(CAR(CDR(args))));
  if (!basic_equal_p(b1, b2))
  {
    EXPRESSION(CAR(CDR(args))) = 
      insert_cast(b1, b2, EXPRESSION(CAR(CDR(args))), context);
  }
  
  return copy_basic(b1);    
}
/***************************************************************************** 
 * Typing arithmetic operator (+, -, --, *, /), except **
 */
static basic
typing_arithmetic_operator(call c, type_context_p context)
{
  basic b;
  
  if(!arguments_are_IRDC(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats), 
		  "Argument(s) of '%s' must be INT, REAL, DOUBLE or COMPLEX" \
			    " and not DOUBLE & COMPLEX",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    /* Just for return a result */
    return make_basic_float(4); 
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);
  
  /* Typing all arguments to b if necessary */
  typing_arguments(c, context, b);
  
  return copy_basic(b);    
}
/***************************************************************************** 
 * Typing power operator (**)
 */
static basic
typing_power_operator(call c, type_context_p context)
{
  basic b, b1, b2;
  list /* of expression */ args = call_arguments(c);
  b = basic_undefined;
  
  if(!arguments_are_IRDC(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats),
		   "Argument(s) of '%s' must be INT, REAL, DOUBLE or COMPLEX" \
			    " and not DOUBLE & COMPLEX",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    /* Just for return a result */
    return make_basic_float(4); 
  }
  
  b1 = GET_TYPE(context->types, EXPRESSION(CAR(args)));
  b2 = GET_TYPE(context->types, EXPRESSION(CAR(CDR(args))));
  
  if (is_inferior_basic(b1, b2))
  {
    b = b2;
  }
  else
  {
    b = b1;
  }
  
  if (!basic_equal_p(b, b1))
  {
    EXPRESSION(CAR(args)) = 
      insert_cast(b, b1, EXPRESSION(CAR(args)), context);
  }
  /* Fortran prefers: (ANSI X3.9-1978, FORTRAN 77, PAGE 6-6, TABLE 3)
   * "var_double = var_double ** var_int" instead of
   * "var_double = var_double ** DBLE(var_int)"
   */
  if (!basic_equal_p(b, b2) && !basic_int_p(b2))
  {
    EXPRESSION(CAR(CDR(args))) = 
      insert_cast(b, b2, EXPRESSION(CAR(CDR(args))), context);
  }
  return copy_basic(b);
}
/***************************************************************************** 
 * Typing relational operator (LT, LE, EQ, GT, GE) 
 */
static basic
typing_relational_operator(call c, type_context_p context)
{
  basic b;
  
  if(!arguments_are_IRDC(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats), 
		   "Argument(s) of '%s' must be INT, REAL, DOUBLE or COMPLEX" \
			    " and not DOUBLE & COMPLEX",
			    entity_local_name(call_function(c))); 
    /* Count the number of errors */
    context->number_of_error++;
    
    /* Just for return a result */
    return make_basic(is_basic_logical, UUINT(4));
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);
  
  /* Typing all arguments to b if necessary */
  typing_arguments(c, context, b);
  
  return make_basic(is_basic_logical, UUINT(4));
}
/***************************************************************************** 
 * Typing logical operator (NOT, AND, OR, EQV, NEQV)
 */
static basic
typing_logical_operator(call c, type_context_p context)
{
  if(!arguments_are_logical(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats),
			    "Argument(s) of '%s' must be LOGICAL",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    /* Just for return a result */
    return make_basic(is_basic_logical, UUINT(4));
  }
  return make_basic(is_basic_logical, UUINT(4));
}
/***************************************************************************** 
 * Typing concatenate operator (//)
 */
static basic
typing_concat_operator(call c, type_context_p context)
{
  if(!arguments_are_character(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats), 
			    "Argument(s) of '%s' must be CHARACTER",
			    entity_local_name(call_function(c))); 
    /* Count the number of errors */
    context->number_of_error++;
    
    /* Just for return a result */
    return make_basic(is_basic_string, value_undefined);
  }
  return make_basic(is_basic_string, value_undefined);
}

/***************************************************************************** 
 * Typing function C whose argument type is from_type and
 * whose return type is to_type
 */
static basic 
typing_function_argument_type_to_return_type(call c, type_context_p context,
					     basic from_type, basic to_type)
{
  bool check_arg = FALSE;
  
  /* INT */
  if(basic_int_p(from_type))
  {
    check_arg = arguments_are_integer(c, context->types);
  }
  /* REAL */
  else if(basic_float_p(from_type) && basic_float(from_type) == 4)
  {
    check_arg = arguments_are_real(c, context->types);
  }
  /* DOUBLE */
  else if(basic_float_p(from_type) && basic_float(from_type) == 8)
  {
    check_arg = arguments_are_double(c, context->types);
  }
  /* COMPLEX */
  else if(basic_complex_p(from_type) && basic_complex(from_type) == 8)
  {
    check_arg = arguments_are_complex(c, context->types);
  }
  /* DOUBLE COMPLEX */
  else if(basic_complex_p(from_type) && basic_complex(from_type) == 16)
  {
    check_arg = arguments_are_dcomplex(c, context->types);
  }
  /* CHAR */
  else if(basic_string_p(from_type))
  {
    check_arg = arguments_are_character(c, context->types);
  }
  /* LOGICAL */
  else if(basic_logical_p(from_type))
  {
    check_arg = arguments_are_logical(c, context->types);
  }
  /* UNEXPECTED */
  else
  {
    pips_internal_error("Unexpected basic: %s \n", basic_to_string(from_type));
  }

  /* ERROR: Invalide of argument type */
  if(check_arg == FALSE)
  {
    add_one_line_of_comment((statement) stack_head(context->stats), 
			    "Invalid argument(s) of '%s'!",
			    entity_local_name(call_function(c))); 
    
    /* Count the number of errors */
    context->number_of_error++;
  }
  
  return copy_basic(to_type);
}

static basic
typing_function_int_to_int(call c, type_context_p context)
{
  basic type_INT = make_basic_int(4);
  return typing_function_argument_type_to_return_type(c, context, 
						      type_INT, type_INT);
}
static basic
typing_function_real_to_real(call c, type_context_p context)
{
  basic type_REAL = make_basic_float(4);
  return typing_function_argument_type_to_return_type(c, context, 
						      type_REAL, type_REAL);
}
static basic
typing_function_double_to_double(call c, type_context_p context)
{
  basic type_DBLE = make_basic_float(8);
  return typing_function_argument_type_to_return_type(c, context, 
						      type_DBLE, type_DBLE);
}
static basic
typing_function_complex_to_complex(call c, type_context_p context)
{
  basic type_CMPLX = make_basic_complex(8);
  return typing_function_argument_type_to_return_type(c, context, 
						      type_CMPLX, type_CMPLX);
}
static basic
typing_function_dcomplex_to_dcomplex(call c, type_context_p context)
{
  basic type_DCMPLX = make_basic_complex(16);
  return typing_function_argument_type_to_return_type(c, context, type_DCMPLX,
						      type_DCMPLX);
}
static basic
typing_function_char_to_int(call c, type_context_p context)
{
  basic type_INT = make_basic_int(4);
  basic type_CHAR = make_basic(is_basic_string, value_undefined);
  return typing_function_argument_type_to_return_type(c, context, type_CHAR, 
						      type_INT);
}
static basic
typing_function_int_to_char(call c, type_context_p context)
{
  basic type_INT = make_basic_int(4);
  basic type_CHAR = make_basic(is_basic_string, value_undefined);
  return typing_function_argument_type_to_return_type(c, context, type_INT, 
						      type_CHAR);
}
static basic
typing_function_real_to_int(call c, type_context_p context)
{
  basic type_INT = make_basic_int(4);
  basic type_REAL = make_basic_float(4);
  return typing_function_argument_type_to_return_type(c, context, type_REAL,
						      type_INT);
}
static basic
typing_function_int_to_real(call c, type_context_p context)
{
  basic type_INT = make_basic_int(4);
  basic type_REAL = make_basic_float(4);
  return typing_function_argument_type_to_return_type(c, context, type_INT, 
						      type_REAL);
}
static basic
typing_function_double_to_int(call c, type_context_p context)
{
  basic type_INT = make_basic_int(4);
  basic type_DBLE = make_basic_float(8);
  return typing_function_argument_type_to_return_type(c, context, type_DBLE, 
						      type_INT);
}
static basic
typing_function_real_to_double(call c, type_context_p context)
{
  basic type_REAL = make_basic_float(4);
  basic type_DBLE = make_basic_float(8);
  return typing_function_argument_type_to_return_type(c, context, type_REAL, 
						      type_DBLE);
}
static basic
typing_function_complex_to_real(call c, type_context_p context)
{
  basic type_REAL = make_basic_float(4);
  basic type_CMPLX = make_basic_complex(8);
  return typing_function_argument_type_to_return_type(c, context, type_CMPLX, 
						      type_REAL);
}
static basic
typing_function_dcomplex_to_double(call c, type_context_p context)
{
  basic type_DBLE = make_basic_float(8);
  basic type_DCMPLX = make_basic_complex(16);
  return typing_function_argument_type_to_return_type(c, context, type_DCMPLX,
						      type_DBLE);
}
static basic
typing_function_char_to_logical(call c, type_context_p context)
{
  basic type_LOGICAL = make_basic_logical(4);
  basic type_CHAR = make_basic(is_basic_string, value_undefined);
  return typing_function_argument_type_to_return_type(c, context, type_CHAR, 
						      type_LOGICAL);
}

/***************************************************************************** 
 * Arguments are REAL (or DOUBLE); and the return is the same with argument
 */
static basic
typing_function_RealDouble_to_RealDouble(call c, type_context_p context)
{
  basic b;
  
  if(!arguments_are_RD(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats), 
			    "Argument(s) of '%s' must be REAL or DOUBLE",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return a result */
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);
  
  /* Typing all arguments to b if necessary */
  typing_arguments(c, context, b);
  
  return copy_basic(b);    
}
static basic
typing_function_RealDouble_to_Integer(call c, type_context_p context)
{
  basic b;
  
  if(!arguments_are_RD(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats), 
			    "Argument(s) of '%s' must be REAL or DOUBLE",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return a result */
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);

  /* Typing all arguments to b if necessary */
  typing_arguments(c, context, b);

  return make_basic_int(4);
}
static basic
typing_function_RealDoubleComplex_to_RealDoubleComplex(call c, 
						       type_context_p context)
{
  basic b;
  
  if(!arguments_are_RDC(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats),
		       "Argument(s) of '%s' must be REAL, DOUBLE or COMPLEX",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return a result  */
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);
  
  /* Typing all arguments to b if necessary */
  typing_arguments(c, context, b);
  
  return copy_basic(b);
}
static basic
typing_function_IntegerRealDouble_to_IntegerRealDouble(call c, 
						       type_context_p context)
{
  basic b;
  
  if(!arguments_are_IRD(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats),
			    "Argument(s) of '%s' must be INT, REAL or DOUBLE",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return a result */
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);

  // Typing all arguments to b if necessary
  typing_arguments(c, context, b);
  
  return copy_basic(b);    
}
/***************************************************************************** 
 * The arguments are INT, REAL, DOUBLE or COMPLEX. The return is the same 
 * with the argument except case argument are COMPLEX, return is REAL
 *
 * Note: Only for Intrinsic ABS(): ABS(CMPLX(x)) --> REAL
 */
static basic
typing_function_IntegerRealDoubleComplex_to_IntegerRealDoubleReal(call c, 
						       type_context_p context)
{
  basic b;
  
  if(!arguments_are_IRDC(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats),
		    "Argument(s) of '%s' must be INT, REAL, DOUBLE or COMPLEX",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return result */
  }
  /* Find the longest type amongs all arguments */
  b = basic_union_arguments(c, context->types);
  
  /* Typing all arguments to b if necessary */
  typing_arguments(c, context, b);

  if (basic_complex_p(b))
  {
    b = make_basic_float(4); /* CMPLX --> REAL */
  }
  return copy_basic(b);
}

/***************************************************************************** 
 * Intrinsic conversion to a numeric
 *
 * Note: argument must be numeric
 */
static basic
typing_function_conversion_to_numeric(call c, type_context_p context, 
				      basic to_type)
{
  if(!arguments_are_IRDC(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats), 
		   "Argument(s) of '%s' must be INT, REAL, DOUBLE or COMPLEX",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
  }
  return copy_basic(to_type);
}
static basic
typing_function_conversion_to_integer(call c, type_context_p context)
{
  return typing_function_conversion_to_numeric(c, context, make_basic_int(4));
}
static basic
typing_function_conversion_to_real(call c, type_context_p context)
{
  return typing_function_conversion_to_numeric(c, context, 
					       make_basic_float(4));
}
static basic
typing_function_conversion_to_double(call c, type_context_p context)
{
  return typing_function_conversion_to_numeric(c, context, 
					       make_basic_float(8));
}
static basic
typing_function_conversion_to_complex(call c, type_context_p context)
{
    return typing_function_conversion_to_numeric(c, context, 
						 make_basic_complex(8));
}
static basic
typing_function_conversion_to_dcomplex(call c, type_context_p context)
{
  return typing_function_conversion_to_numeric(c, context, 
					       make_basic_complex(16));
}
/* CMPLX_ */
static basic
typing_function_constant_complex(call c, type_context_p context)
{
  if(!arguments_are_IR(c, context->types))
  {
    /* ERROR: Invalide of type  */
    add_one_line_of_comment((statement) stack_head(context->stats), 
			    "Argument(s) of '%s' must be INT, REAL",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return result */
  }
  /* Typing all arguments to REAL if necessary */
  typing_arguments(c, context, make_basic_float(4));
  
  return make_basic_complex(8);
}
/* DCMPLX_ */
static basic
typing_function_constant_dcomplex(call c, type_context_p context)
{
  if(!arguments_are_IRD(c, context->types))
  {
    /* ERROR: Invalide of type */
    add_one_line_of_comment((statement) stack_head(context->stats),
			    "Argument(s) of '%s' must be INT, REAL or DOUBLE",
			    entity_local_name(call_function(c)));
    /* Count the number of errors */
    context->number_of_error++;
    
    return make_basic_float(4); /* Just for return result */
  }
  /* Typing all arguments to DOUBLE if necessary */
  typing_arguments(c, context, make_basic_float(8));
  
  return make_basic_complex(16);
}

/***************************************** SIMPLIFICATION DES EXPRESSIONS */
/* Find the specific name from the specific argument
 */

/************************************************************************ 
 * Each intrinsic of name generic have a function for switching to the
 * specific name correspondent with the argument
 */
static void
switch_generic_to_specific(call c, type_context_p context,
			   string arg_int_name,
			   string arg_real_name,
			   string arg_double_name,
			   string arg_complex_name,
			   string arg_dcomplex_name)
{
  string specific_name = NULL;
  list args = call_arguments(c);
  basic arg_basic = GET_TYPE(context->types, EXPRESSION(CAR(args)));
  
  if (basic_int_p(arg_basic))
  {
    specific_name = arg_int_name;
  }
  else if (basic_float_p(arg_basic) && basic_float(arg_basic) == 4)
  {
    specific_name = arg_real_name;
  }
  else if (basic_float_p(arg_basic) && basic_float(arg_basic) == 8)
  {
    specific_name = arg_double_name;
  }
  else if (basic_complex_p(arg_basic) && basic_complex(arg_basic) == 8)
  {
    specific_name = arg_complex_name;
  }
  else if (basic_complex_p(arg_basic) && basic_complex(arg_basic) == 16)
  {
    specific_name = arg_dcomplex_name;
  }
  
  /* Modify the (function:entity) of the call c if necessary
   * NOTE: If specific_name == NULL: Invalid argument or argument basic unknown
   */
  if(specific_name != NULL && 
     strcmp(specific_name, entity_local_name(call_function(c))) != 0)
  {
    call_function(c) = CreateIntrinsic(specific_name);
    
    /* Count number of simplifications */
    context->number_of_simplication++;
  }
}

/* AINT */
static void
switch_specific_aint(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "AINT", "DINT", NULL, NULL);
}
/* ANINT */
static void
switch_specific_anint(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ANINT", "DNINT", NULL, NULL);
}
/* NINT */
static void
switch_specific_nint(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "NINT", "IDNINT", NULL, NULL);
}
/* ABS */
static void
switch_specific_abs(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     "IABS", "ABS", "DABS", "CABS", NULL);
}
/* MOD */
static void
switch_specific_mod(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     "MOD", "AMOD", "DMOD", NULL, NULL);
}
/* SIGN */
static void
switch_specific_sign(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     "ISIGN", "SIGN", "DSIGN", NULL, NULL);
}
/* DIM */
static void
switch_specific_dim(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     "IDIM", "DIM", "DDIM", NULL, NULL);
}
/* MAX */
static void
switch_specific_max(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     "MAX0", "AMAX1", "DMAX1", NULL, NULL);
}
/* MIN */
static void
switch_specific_min(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     "MIN0", "AMIN1", "DMIN1", NULL, NULL);
}
/* SQRT */
static void
switch_specific_sqrt(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "SQRT", "DSQRT", "CSQRT", NULL);
}
/* EXP */
static void
switch_specific_exp(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "EXP", "DEXP", "CEXP", NULL);
}
/* LOG */
static void
switch_specific_log(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ALOG", "DLOG", "CLOG", NULL);
}
/* LOG10 */
static void
switch_specific_log10(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ALOG10", "DLOG10", NULL, NULL);
}
/* SIN */
static void
switch_specific_sin(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL,"SIN","DSIN", "CSIN", NULL);
}
/* COS */
static void
switch_specific_cos(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "COS", "DCOS", "CCOS", NULL);
}
/* TAN */
static void
switch_specific_tan(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "TAN", "DTAN", NULL, NULL);
}
/* ASIN */
static void
switch_specific_asin(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ASIN", "DASIN", NULL, NULL);
}
/* ACOS */
static void
switch_specific_acos(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ACOS", "DACOS", NULL, NULL);
}
/* ATAN */
static void
switch_specific_atan(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ATAN", "DATAN", NULL, NULL);
}
/* ATAN2 */
static void
switch_specific_atan2(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "ATAN2", "DATAN2", NULL, NULL);
}
/* SINH */
static void
switch_specific_sinh(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "SINH", "DSINH", NULL, NULL);
}
/* COSH */
static void
switch_specific_cosh(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "COSH", "DCOSH", NULL, NULL);
}
/* TANH */
static void
switch_specific_tanh(call c, type_context_p context)
{
  switch_generic_to_specific(c, context,
			     NULL, "TANH", "DTANH", NULL, NULL);
}

/********************************* SIMPLIFICATION THE CONVERSION *************
 * e.g: INT(INT(R)) -> INT(R)
 *      INT(2.9) -> 2
 */

/******************************************************** INTRINSICS LIST */

/* The following data structure describes an intrinsic function: its
   name and its arity and its type. */

typedef struct IntrinsicDescriptor 
{
  string name;
  int nbargs;
  type (*intrinsic_type)(int);
  typing_function_t type_function;
  switch_name_function name_function;
} IntrinsicDescriptor;

/* The table of intrinsic functions. this table is used at the begining
   of linking to create Fortran operators, commands and intrinsic functions. 

   Functions with a variable number of arguments are declared with INT_MAX
   arguments.
*/

static IntrinsicDescriptor IntrinsicDescriptorTable[] = {
  {"+", 2, default_intrinsic_type, typing_arithmetic_operator, 0},
  {"-", 2, default_intrinsic_type, typing_arithmetic_operator, 0},
  {"/", 2, default_intrinsic_type, typing_arithmetic_operator, 0},
  {"INV", 1, real_to_real_type, 0, 0},
  {"*", 2, default_intrinsic_type, typing_arithmetic_operator, 0},
  {"--", 1, default_intrinsic_type, typing_arithmetic_operator, 0},
  {"**", 2, default_intrinsic_type, typing_power_operator, 0},
  
  {"=", 2, default_intrinsic_type, typing_assignment, 0},
  
  {".EQV.", 2, overloaded_to_logical_type, typing_logical_operator, 0},
  {".NEQV.", 2, overloaded_to_logical_type, typing_logical_operator, 0},
  
  {".OR.", 2, logical_to_logical_type, typing_logical_operator, 0},
  {".AND.", 2, logical_to_logical_type, typing_logical_operator, 0},
  {".NOT.", 1, logical_to_logical_type, typing_logical_operator, 0},
  
  {".LT.", 2, overloaded_to_logical_type, typing_relational_operator, 0},
  {".GT.", 2, overloaded_to_logical_type, typing_relational_operator, 0},
  {".LE.", 2, overloaded_to_logical_type, typing_relational_operator, 0},
  {".GE.", 2, overloaded_to_logical_type, typing_relational_operator, 0},
  {".EQ.", 2, overloaded_to_logical_type, typing_relational_operator, 0},
  {".NE.", 2, overloaded_to_logical_type, typing_relational_operator, 0},
  
  {"//", 2, character_to_character_type, typing_concat_operator, 0},
  
  {"WRITE", (INT_MAX), default_intrinsic_type, 0, 0}, /* ERROR ? */
  {"REWIND", (INT_MAX), default_intrinsic_type, 0, 0},
  {"BACKSPACE", (INT_MAX), default_intrinsic_type, 0, 0},
  {"OPEN", (INT_MAX), default_intrinsic_type, 0, 0},
  {"CLOSE", (INT_MAX), default_intrinsic_type, 0, 0},
  {"READ", (INT_MAX), default_intrinsic_type, 0, 0},
  {"BUFFERIN", (INT_MAX), default_intrinsic_type, 0, 0},
  {"BUFFEROUT", (INT_MAX), default_intrinsic_type, 0, 0},
  {"ENDFILE", (INT_MAX), default_intrinsic_type, 0, 0},
  {"IMPLIED-DO", (INT_MAX), default_intrinsic_type, 0, 0},
  {FORMAT_FUNCTION_NAME, 1, default_intrinsic_type, 0, 0},
  {"INQUIRE", (INT_MAX), default_intrinsic_type, 0, 0},
  
  {SUBSTRING_FUNCTION_NAME, 3, substring_type, 0, 0},
  {ASSIGN_SUBSTRING_FUNCTION_NAME, 4, assign_substring_type, 0, 0},
  
  {"CONTINUE", 0, default_intrinsic_type, 0, 0},
  {"ENDDO", 0, default_intrinsic_type, 0, 0},
  {"PAUSE", 1, default_intrinsic_type, 0, 0},
  {"RETURN", 0, default_intrinsic_type, 0, 0},
  {"STOP", 0, default_intrinsic_type, 0, 0},
  {"END", 0, default_intrinsic_type, 0, 0},
  
  {"INT", 1, overloaded_to_integer_type, 
   typing_function_conversion_to_integer, 0},
  {"IFIX", 1, real_to_integer_type, typing_function_real_to_int, 0},
  {"IDINT", 1, double_to_integer_type, typing_function_double_to_int, 0},
  {"REAL", 1, overloaded_to_real_type, typing_function_conversion_to_real, 0},
  {"FLOAT", 1, overloaded_to_real_type, typing_function_conversion_to_real, 0},
  {"DFLOAT", 1, overloaded_to_double_type, 
   typing_function_conversion_to_real, 0},
  {"SNGL", 1, overloaded_to_real_type, typing_function_conversion_to_real, 0},
  {"DBLE", 1, overloaded_to_double_type, 
   typing_function_conversion_to_double, 0},
  {"DREAL", 1, overloaded_to_double_type, 
   typing_function_conversion_to_double, 0}, /* Arnauld Leservot, code CEA */
  {"CMPLX", (INT_MAX), overloaded_to_complex_type, 
   typing_function_conversion_to_complex, 0},
  
  {"DCMPLX", (INT_MAX), overloaded_to_doublecomplex_type, 
   typing_function_conversion_to_dcomplex, 0},
  
  /* (0.,1.) -> switched to a function call...
   */
  { IMPLIED_COMPLEX_NAME, 2, overloaded_to_complex_type, 
    typing_function_constant_complex, 0},
  { IMPLIED_DCOMPLEX_NAME, 2, overloaded_to_doublecomplex_type, 
    typing_function_constant_dcomplex, 0},
  
  {"ICHAR", 1, default_intrinsic_type, typing_function_char_to_int, 0},
  {"CHAR", 1, default_intrinsic_type, typing_function_int_to_char, 0},
  {"AINT", 1, real_to_real_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_aint},
  {"DINT", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"ANINT", 1, real_to_real_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_anint},
  {"DNINT", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"NINT", 1, real_to_integer_type, 
   typing_function_RealDouble_to_Integer, switch_specific_nint},
  {"IDNINT", 1, double_to_integer_type, typing_function_double_to_int, 0},
  {"IABS", 1, integer_to_integer_type, typing_function_int_to_int, 0},
  {"ABS", 1, real_to_real_type, 
   typing_function_IntegerRealDoubleComplex_to_IntegerRealDoubleReal, 
   switch_specific_abs},
  {"DABS", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"CABS", 1, complex_to_real_type, typing_function_complex_to_real, 0},
  {"CDABS", 1, doublecomplex_to_double_type, 
   typing_function_dcomplex_to_double, 0},
  
  {"MOD", 2, default_intrinsic_type, 
   typing_function_IntegerRealDouble_to_IntegerRealDouble, 
   switch_specific_mod},
  {"AMOD", 2, real_to_real_type, typing_function_real_to_real, 0},
  {"DMOD", 2, double_to_double_type, typing_function_double_to_double, 0},
  {"ISIGN", 2, integer_to_integer_type, typing_function_int_to_int, 0},
  {"SIGN", 2, default_intrinsic_type, 
   typing_function_IntegerRealDouble_to_IntegerRealDouble, 
   switch_specific_sign},
  {"DSIGN", 2, double_to_double_type, typing_function_double_to_double, 0},
  {"IDIM", 2, integer_to_integer_type, typing_function_int_to_int, 0},
  {"DIM", 2, default_intrinsic_type, 
   typing_function_IntegerRealDouble_to_IntegerRealDouble, 
   switch_specific_dim},
  {"DDIM", 2, double_to_double_type, typing_function_double_to_double, 0},
  {"DPROD", 2, real_to_double_type, typing_function_real_to_double, 0},
  {"MAX", (INT_MAX), default_intrinsic_type, 
   typing_function_IntegerRealDouble_to_IntegerRealDouble, 
   switch_specific_max},
  {"MAX0", (INT_MAX), integer_to_integer_type, typing_function_int_to_int, 0},
  {"AMAX1", (INT_MAX), real_to_real_type, typing_function_real_to_real, 0},
  {"DMAX1", (INT_MAX), double_to_double_type, 
   typing_function_double_to_double, 0},
  {"AMAX0", (INT_MAX), integer_to_real_type, typing_function_int_to_real, 0},
  {"MAX1", (INT_MAX), real_to_integer_type, typing_function_real_to_int, 0},
  {"MIN", (INT_MAX), default_intrinsic_type, 
   typing_function_IntegerRealDouble_to_IntegerRealDouble, 
   switch_specific_min},
  {"MIN0", (INT_MAX), integer_to_integer_type, typing_function_int_to_int, 0},
  {"AMIN1", (INT_MAX), real_to_real_type, typing_function_real_to_real, 0},
  {"DMIN1", (INT_MAX), double_to_double_type, 
   typing_function_double_to_double, 0},
  {"AMIN0", (INT_MAX), integer_to_real_type, typing_function_int_to_real, 0},
  {"MIN1", (INT_MAX), real_to_integer_type, typing_function_real_to_int, 0},
  {"LEN", 1, character_to_integer_type, typing_function_char_to_int, 0},
  {"INDEX", 2, character_to_integer_type, typing_function_char_to_int, 0},
  {"AIMAG", 1, complex_to_real_type, typing_function_complex_to_real, 0},
  {"DIMAG", 1, doublecomplex_to_double_type, 
   typing_function_dcomplex_to_double, 0},
  {"CONJG", 1, complex_to_complex_type, typing_function_complex_to_complex, 0},
  {"DCONJG", 1, doublecomplex_to_doublecomplex_type, 
   typing_function_dcomplex_to_dcomplex, 0},
  {"SQRT", 1, default_intrinsic_type, 
   typing_function_RealDoubleComplex_to_RealDoubleComplex, 
   switch_specific_sqrt},
  {"DSQRT", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"CSQRT", 1, complex_to_complex_type, typing_function_complex_to_complex, 0},
  
  {"EXP", 1, default_intrinsic_type, 
   typing_function_RealDoubleComplex_to_RealDoubleComplex, 
   switch_specific_exp},
  {"DEXP", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"CEXP", 1, complex_to_complex_type, typing_function_complex_to_complex, 0},
  {"LOG", 1, default_intrinsic_type, 
   typing_function_RealDoubleComplex_to_RealDoubleComplex, 
   switch_specific_log},
  {"ALOG", 1, real_to_real_type, typing_function_real_to_real, 0},
  {"DLOG", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"CLOG", 1, complex_to_complex_type, typing_function_complex_to_complex, 0},
  {"LOG10", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_log10},
  {"ALOG10", 1, real_to_real_type, typing_function_real_to_real, 0},
  {"DLOG10", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"SIN", 1, default_intrinsic_type, 
   typing_function_RealDoubleComplex_to_RealDoubleComplex, 
   switch_specific_sin},
  {"DSIN", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"CSIN", 1, complex_to_complex_type, typing_function_complex_to_complex, 0},
  {"COS", 1, default_intrinsic_type, 
   typing_function_RealDoubleComplex_to_RealDoubleComplex, 
   switch_specific_cos},
  {"DCOS", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"CCOS", 1, complex_to_complex_type, typing_function_complex_to_complex, 0},
  {"TAN", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_tan},
  {"DTAN", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"ASIN", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_asin},
  {"DASIN", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"ACOS", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_acos},
  {"DACOS", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"ATAN", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_atan},
  {"DATAN", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"ATAN2", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_atan2},
  {"DATAN2", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"SINH", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_sinh},
  {"DSINH", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"COSH", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_cosh},
  {"DCOSH", 1, double_to_double_type, typing_function_double_to_double, 0},
  {"TANH", 1, default_intrinsic_type, 
   typing_function_RealDouble_to_RealDouble, switch_specific_tanh},
  {"DTANH", 1, double_to_double_type, typing_function_double_to_double, 0},
  
  {"LGE", 2, character_to_logical_type, typing_function_char_to_logical, 0},
  {"LGT", 2, character_to_logical_type, typing_function_char_to_logical, 0},
  {"LLE", 2, character_to_logical_type, typing_function_char_to_logical, 0},
  {"LLT", 2, character_to_logical_type, typing_function_char_to_logical, 0},
  
  {LIST_DIRECTED_FORMAT_NAME, 0, default_intrinsic_type, 0, 0},
  {UNBOUNDED_DIMENSION_NAME, 0, default_intrinsic_type, 0, 0},
  
  /* These operators are used within the OPTIMIZE transformation in
     order to manipulate operators such as n-ary add and multiply or
     multiply-add operators ( JZ - sept 98) */
  {EOLE_SUM_OPERATOR_NAME, (INT_MAX), default_intrinsic_type , 
   typing_arithmetic_operator, 0},
  {EOLE_PROD_OPERATOR_NAME, (INT_MAX), default_intrinsic_type , 
   typing_arithmetic_operator, 0},
  {EOLE_FMA_OPERATOR_NAME, 3, default_intrinsic_type , 
   typing_arithmetic_operator, 0},
  
  {NULL, 0, 0, 0, 0}
};

/***************************************************************************** 
 * Get the function for typing the specified intrinsic
 *
 */
typing_function_t get_typing_function_for_intrinsic(string name)
{
  static hash_table name_to_type_function = NULL;
  
  /* Initialize first time */
  if (!name_to_type_function) 
  {
    IntrinsicDescriptor * pdt = IntrinsicDescriptorTable;
    
    name_to_type_function = hash_table_make(hash_string, 0);
    
    for(; pdt->name; pdt++)
    {
      hash_put(name_to_type_function, 
	       (char*)pdt->name, (char*)pdt->type_function);
    }
  }
  
  if (!hash_defined_p(name_to_type_function, name))
  {
    pips_internal_error("No type function for intrinsics %s\n", name);
  }
  
  return (typing_function_t) hash_get(name_to_type_function, name);
}
/***************************************************************************** 
 * Get the function for switching to specific name from generic name 
 *
 */
switch_name_function get_switch_name_function_for_intrinsic(string name)
{
  static hash_table name_to_switch_function = NULL;
  
  /* Initialize first time */
  if (!name_to_switch_function) 
  {
    IntrinsicDescriptor * pdt = IntrinsicDescriptorTable;
    
    name_to_switch_function = hash_table_make(hash_string, 0);
    
    for(; pdt->name; pdt++)
    {
      hash_put(name_to_switch_function, 
	       (char*)pdt->name, (char*)pdt->name_function);
    }
  }
  
  if (!hash_defined_p(name_to_switch_function, name))
  {
    pips_internal_error("No switch name function for intrinsics %s\n", name);
  }
  
  return (switch_name_function) hash_get(name_to_switch_function, name);
}

/* This function creates an entity that represents an intrinsic
   function. Fortran operators and basic statements are intrinsic
   functions.
   
   An intrinsic function has a rom storage, an unknown initial value and a
   functional type whose result and arguments have an overloaded basic
   type. The number of arguments is given by the IntrinsicDescriptorTable
   data structure. */

void 
MakeIntrinsic(name, n, intrinsic_type)
     string name;
     int n;
     type (*intrinsic_type)(int);
{
  entity e;
  
  e = make_entity(AddPackageToName(TOP_LEVEL_MODULE_NAME, name),
		  intrinsic_type(n),
		  make_storage(is_storage_rom, UU),
		  make_value(is_value_intrinsic, NIL));
  
}

/* This function is called one time (at the very beginning) to create
   all intrinsic functions. */

void 
CreateIntrinsics()
{
  IntrinsicDescriptor *pid;
  
  for (pid = IntrinsicDescriptorTable; pid->name != NULL; pid++) {
    MakeIntrinsic(pid->name, pid->nbargs, pid->intrinsic_type);
  }
}

bool 
bootstrap(string workspace)
{
  if (db_resource_p(DBR_ENTITIES, "")) 
    pips_internal_error("entities already initialized");
  
  CreateIntrinsics();
  
  /* Creates the dynamic and static areas for the super global
   * arrays such as the logical unit array (see below).
   */
  CreateAreas();
  
  /* The current entity is unknown, but for a TOP-LEVEL:TOP-LEVEL
   * which is used to create the logical unit array for IO effects
   */
  CreateArrays();
  
  /* Create the empty label */
  (void) make_entity(strdup(concatenate(TOP_LEVEL_MODULE_NAME,
					MODULE_SEP_STRING, 
					LABEL_PREFIX,
					NULL)),
		     MakeTypeStatement(),
		     MakeStorageRom(),
		     make_value(is_value_constant,
				MakeConstantLitteral()));
  
  /* FI: I suppress the owner filed to make the database moveable */
  /* FC: the content must be consistent with pipsdbm/methods.h */
  DB_PUT_MEMORY_RESOURCE(DBR_ENTITIES, "", (char*) entity_domain);
  return TRUE;
}

value 
MakeValueLitteral()
{
  return(make_value(is_value_constant, 
		    make_constant(is_constant_litteral, UU)));
}

string 
MakeFileName(prefix, base, suffix)
     char *prefix, *base, *suffix;
{
  char *s;
  
  s = (char*) malloc(strlen(prefix)+strlen(base)+strlen(suffix)+1);
  
  strcpy(s, prefix);
  strcat(s, base);
  strcat(s, suffix);
  
  return(s);
}

/* This function creates a fortran operator parameter, i.e. a zero
   dimension variable with an overloaded basic type. */

char *
AddPackageToName(p, n)
     string p, n;
{
  string ps;
  int l;
  
  l = strlen(p);
  ps = strndup(l + 1 + strlen(n) +1, p);
  
  *(ps+l) = MODULE_SEP;
  *(ps+l+1) = '\0';
  strcat(ps, n);
  
  return(ps);
}


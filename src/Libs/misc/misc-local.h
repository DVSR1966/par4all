/* $Id$
 */

#ifndef _STDARG_H
#include <stdarg.h>
#endif
#ifndef _STDLIB_H
#include <stdlib.h>
#endif
#include <setjmp.h>

/* To generate a string from a macro: */
#define STRINGIFY_SECOND_STAGE(symbol) #symbol
/* If not using this 2-stage macro evaluation, the generated string is not
   the value of the macro but the name of the macro... Who said C was a
   simple language? :-/ */
#define STRINGIFY(symbol) STRINGIFY_SECOND_STAGE(symbol)


/* Measurement type for mem_spy.c */
typedef enum {SBRK_MEASURE, NET_MEASURE, GROSS_MEASURE} measurement_type;

#define ifdebug(l) if(the_current_debug_level>=(l))

#define pips_unknown_function "Unknown Function Name"

/* these macros use the GNU extensions that allow to know the
 * function name thru the special __FUNCTION__ macro. Also variable
 * number of arguments macros are used.
 * if not available, other macros or function calls are generated.
 * Fabien.
 */
#if (defined(__GNUC__))
#define pips_where(out) \
  fprintf(out, "[%s] (%s:%d) ", __FUNCTION__, __FILE__, __LINE__)
#define debug_on(env) debug_on_function(env, __FUNCTION__, __FILE__, __LINE__)
#define debug_off() debug_off_function(__FUNCTION__, __FILE__, __LINE__)
/* Use the old "do {...} while(0)" C hack to allow a macro with arbitrary
   code to be used everywhere: */
#define pips_debug(level, format, args...)				\
  do {									\
    ifdebug(level) fprintf(stderr, "[%s] " format, __FUNCTION__ ,	\
			   ##args);					\
  } while(0)
#define pips_user_warning(format, args...)\
  user_warning(__FUNCTION__, format, ##args)
#define pips_user_error(format, args...)\
  user_error(__FUNCTION__, format, ##args)
#define pips_user_irrecoverable_error(format, args...)\
  user_irrecoverable_error(__FUNCTION__, format, ##args)
#define pips_internal_error(format, args...)\
  pips_error(__FUNCTION__, "(%s:%d) " format, __FILE__ , __LINE__ , ##args)
#define pips_assert(what, predicate)					\
  do {									\
    if(!(predicate)) {							\
      (void) fprintf(stderr,						\
		     "[%s] (%s:%d) assertion failed\n\n '%s' not verified\n\n", \
		     __FUNCTION__ , __FILE__ , __LINE__ , what);	\
      abort();								\
    }									\
  } while(0)
#define pips_user_assert(what, predicate)				\
  do {									\
    if(!(predicate)) {							\
      (void) fprintf(stderr,						\
		     "[%s] (%s:%d) assertion failed\n\n '%s' not verified\n\n", \
		     __FUNCTION__ , __FILE__ , __LINE__ , what);	\
      pips_user_error("this is a USER ERROR, I guess\n");		\
    };									\
  } while(0)
#define pips_exit(code, format, args...)\
   pips_user_warning(format, ##args), exit(code)
#else
#define pips_where(out) \
  fprintf(out, "[%s] (%s:%d) ", pips_unknown_function, __FILE__, __LINE__)
#define debug_on(env) \
  debug_on_function(env, pips_unknown_function, __FILE__, __LINE__)
#define debug_off() \
  debug_off_function(pips_unknown_function, __FILE__, __LINE__)
#define pips_debug pips_debug_function
#define pips_user_warning pips_user_warning_function
#define pips_user_error pips_user_error_function
#define pips_user_irrecoverable_error user_irrecoverable_error_function
#define pips_internal_error pips_internal_error_function
#define pips_assert(what, predicate)					\
  do {									\
    if(!(predicate)) {							\
      (void) fprintf(stderr,						\
		     "(%s:%d) assertion failed\n\n '%s' not verified\n\n", \
		     __FILE__ , __LINE__ , what);			\
      abort();								\
    }									\
  } while(0)
#define pips_user_assert(what, predicate)				\
  do {									\
    if(!(predicate)) {							\
      (void) fprintf(stderr,						\
		     "(%s:%d) assertion failed\n\n '%s' not verified\n\n", \
		     __FILE__ , __LINE__ , what);			\
      pips_user_error("this is a USER ERROR, I guess\n");		\
    }									\
  } while(0)
#define pips_exit pips_where(stderr), pips_exit_function
#endif

#define same_string_p(s1, s2) (strcmp((s1), (s2)) == 0)

/* MAXPATHLEN is defined in <sys/param.h> for SunOS... but not for all OS! */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#define PIPS_CATCH(what) \
   if (push_debug_status(), \
       setjmp(*push_exception_on_stack(what, __CURRENT_FUNCTION_NAME__, \
      	                    __FILE__, __LINE__, pop_debug_status)))

#include <varargs.h>

#define ifdebug(l) if(the_current_debug_level>=(l))

/* a debug macro that generates automatically the function name if available.
 */
#ifdef __GNUC__
#define pips_debug(level, format, args...)\
 ifdebug(level) fprintf(stderr, "[" __FUNCTION__  "] " format, ##args);
#else
#define pips_debug pips_debug_function
#endif

#define same_string_p(s1, s2) (strcmp((s1), (s2)) == 0)

/* Constant used to dimension arrays in wpips and pipsmake */
#define ARGS_LENGTH 512

/* MAXPATHLEN is defined in <sys/param.h> for SunOS... but not for all OS! */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

extern char *re_comp();
extern int re_exec();

/*VARARGS2*/
extern void debug();
/* The next function should not be used. Use the pips_debug macro. */
extern void pips_debug_function();
extern void pips_error();
extern void user_warning();
extern void user_log();
extern void user_error();

/*VARARGS0*/
extern char * concatenate();

extern void (* pips_log_handler)(char * fmt, va_list args);
extern void (* pips_warning_handler)();
extern void (* pips_error_handler)();

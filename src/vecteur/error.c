/* package vecteur */

/*LINTLIBRARY*/

#include <stdio.h>
#include <varargs.h>
	

/* void vect_error(va_dcl va_list): 
 * should be called to terminate execution and to core dump
 * when data structures are corrupted or when an undefined operation is
 * requested (zero divide for instance). VECT_ERROR should be called as:
 * 
 *   VECT_ERROR(function_name, format, expression-list)
 * 
 * where function_name is a string containing the name of the function
 * calling VECT_ERROR, and where format and expression-list are passed as
 * arguments to vprintf. VECT_ERROR terminates execution with abort.
 * 
 */
/*VARARGS0*/
void vect_error(va_alist)
va_dcl
{
    va_list args;
    char *fmt;

    va_start(args);

    /* print name of function causing error */
    (void) fprintf(stderr, "vecteur error in %s: ", va_arg(args, char *));
    fmt = va_arg(args, char *);

    /* print out remainder of message */
    (void) vfprintf(stderr, fmt, args);
    va_end(args);

    /* create a core file for debug */
    (void) abort();
}

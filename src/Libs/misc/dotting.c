 /* Sorry to keep you waiting... 
  *
  * Emit a message and then a waiting character every second till it is
  * notified to shut up.
  *
  * Francois Irigoin, 18 April 1990
  *
  * Modification:
  *  - installed in Lib/misc/dotting.c, 21 April 1990
  *  - no more dotting: in wpips, the window io associated to each
  * dotting requires a malloc(); as it may occur at any time, malloc()
  * (or free()) may be already active and wpips core dumps;
  * (Francois Irigoin, Bruno Baron, ?? ???? 1990)
  */

#include <stdio.h>
extern int fflush();
extern int _flsbuf();
extern int vfprintf(FILE *stream, char *format, ...);
extern unsigned int alarm(unsigned int seconds);
#include <varargs.h>
#include <signal.h>

#include "genC.h"

#include "misc.h"

static FILE * dotting_file;
static char dotting_character;

static void print_dot()
{
    if (dotting_file == stdout) 
	user_log("%c", dotting_character);
    else {
	(void) putc(dotting_character, dotting_file);
	if(dotting_file!=stderr) fflush(dotting_file);
    }
    alarm(1);
}

/* start_dotting: the profile should be changed and varargs used so as
 * to let user emit the initial message as with a fprintf
 * the dotting character could be a constant (e.g. '.') or be passed
 * as first argument
 * start_dotting(dotting_file, dotting_character, format, args)
 */

/*VARARGS3*/
void start_dotting(va_alist)
va_dcl
{
    char *fmt;
    va_list args;

    va_start(args);
    dotting_file = va_arg(args, FILE *);
    dotting_character = (char) va_arg(args, int);
    fmt= va_arg(args, char *);

    if (dotting_file == stdout) 
	(* pips_log_handler)(fmt, args);
    else {
	vfprintf(dotting_file, fmt, args);
	if(dotting_file!=stderr) fflush(dotting_file);
    }
    va_end(args);
/*
    if((int) signal(SIGALRM, print_dot)==-1) {
	pips_error("start_dotting", "signal error\n");
	exit(1);
    }
    alarm(1);
*/
}

void stop_dotting()
{
/*    alarm(0);
    (void) signal(SIGALRM, SIG_DFL);
*/
    if (dotting_file == stdout) 
	user_log("\n");
    else {
	(void) putc('\n', dotting_file);
	if(dotting_file!=stderr) fflush(dotting_file);
    }
}

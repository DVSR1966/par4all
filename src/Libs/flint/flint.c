/*
 * This File contains the routine of verification launching
 * 
 * Laurent Aniort & Fabien COELHO 1992
 * 
 * Modified      92 08 31 
 * Author        Arnauld Leservot 
 * Old Version   flint.c.old 
 * Object        Include this as a pips phase.
 */

/*************************************************************/
/* system includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/* PiPs specific headers */
#include "genC.h"
#include "ri.h"
#include "database.h"

#include "misc.h"
#include "ri-util.h"
#include "flint.h"
#include "pipsdbm.h"
#include "resources.h"
#include "flint-local.h"

/* internal variables */
static FILE    *flint_messages_file;
static string   filename;
static bool     no_message = TRUE;
static int      number_of_messages = 0;
statement       flint_current_statement;

/* name of module being flinted */
static char *flint_current_module_name;

/*************************************************************/
/* Routine of  global module verification                    */

bool flinter(module_name)
char *module_name;
{
    entity module = local_name_to_top_level_entity(module_name);
    statement module_stat;
    
    debug_on("FLINT_DEBUG_LEVEL");

    flint_current_module_name = module_name;
    flint_current_statement = statement_undefined;

    debug(1, "flinter", "flinting module %s\n", module_name);
    
    /* Getting parsed code of module */
    /* the last parameter should be pure=TRUE; the code should not be modified! */
    module_stat = (statement)
	db_get_memory_resource(DBR_CODE, module_name, TRUE);

    filename = 
	strdup(concatenate(db_get_current_workspace_directory(),
			   "/", module_name, ".flinted", NULL));
    flint_messages_file = 
	(FILE *) safe_fopen(filename, "w");

    /* what is  done */
    debug(3, "flinter", "checking commons\n");
    check_commons(module);	         /* checking commons */

    debug(3, "flinter", "checking statements\n");
    flint_statement(module_stat);	 /* checking syntax  */

    if (no_message)                      /* final message */
      fprintf(flint_messages_file,
	      "%s has been flinted : everything is ok.\n",
	      module_name);
    else
      fprintf(flint_messages_file,
	      "number of messages from flint for %s : %d\n",
	      module_name,
	      number_of_messages);
    
    safe_fclose(flint_messages_file, filename);
    DB_PUT_FILE_RESOURCE(DBR_FLINTED, strdup(module_name),
			 filename);
    debug_off();

    return TRUE;
}

/*************************************************************/

/*
 * FLINT_MESSAGE(fonction, format [, arg] ... ) string fonction, format;
 */

void flint_message(char *fun, char *fmt, ...) 
{
    va_list         args;
    int             order;

    va_start(args, fmt);

    /*
     * print name of function causing message, and in which module it
     * occured.
     */

    no_message = FALSE;
    number_of_messages++;

    order = statement_ordering(flint_current_statement);

    (void) fprintf(flint_messages_file,
		   "flint message from %s, in module %s, (%d.%d), line %d\n",
		   fun, flint_current_module_name,
		   ORDERING_NUMBER(order), ORDERING_STATEMENT(order),
		   statement_number(flint_current_statement));


    /* print out remainder of message */
    (void) vfprintf(flint_messages_file, fmt, args);

    va_end(args);

}
/*************************************************************/
/* Same as flint_message but without the function name       */

void flint_message_2(char *fun, char *fmt, ...) 
{
    va_list         args;

    va_start(args, fmt);

    no_message = FALSE;
    number_of_messages++;

    (void) fprintf(flint_messages_file,
		   "flint message from %s, in module %s\n",
		   fun, flint_current_module_name);

    /* print out remainder of message */
    (void) vfprintf(flint_messages_file, fmt, args);

    va_end(args);

}
/*************************************************************/
/* End of File */

/* $RCSfile: hpfc_interactive.c,v $ (version $Revision$)
 * $Date: 1995/08/11 12:52:53 $, 
 *
 * interactive interface to hpfc,
 * based on the GNU READLINE library for interaction,
 * and the associated HISTORY library for history.
 * taken from bash. it's definitely great.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "readline.h"
#include "history.h"

#define HPFC_PROMPT "hpfc> " 		/* prompt for readline  */
#define HPFC_SHELL "hpfc"   		/* forked shell script  */
#define HPFC_HISTENV "HPFC_HISTORY"	/* history file env variable */
#define HPFC_HISTORY_LENGTH 100		/* max length of history file */
#define HIST ".hpfc.history" 		/* default history file */

#define SHELL_ESCAPE "\\" 		/* ! used for history reference */
#define CHANGE_DIR   "cd "
#define QUIT         "quit"

/*  returns the full hpfc history file name, i.e.
 *  - $HPFC_HISTORY (if any)
 *  - $HOME/"HIST"
 */
static char *default_hist_file_name()
{
    char *home, *hist = getenv(HPFC_HISTENV);

    if (hist) return hist;

    /* else builds the default name. memory leak.
     */
    home = getenv("HOME");
    return sprintf((char*) malloc(sizeof(char)*(strlen(home)+strlen(HIST)+2)),
		   "%s/%s", home, HIST);
}

static char * initialize_hpfc_history()
{
    HIST_ENTRY * last_entry;
    char *file_name = default_hist_file_name();
    
    /*  initialize history: 
     *  read the history file, then point to the last entry.
     */
    using_history();
    read_history(file_name);
    history_set_pos(history_length);
    last_entry = previous_history();

    /* last points to the last history line of any.
     * used to avoid to put twice the same line.
     */
    return last_entry ? last_entry->line : NULL ;
}


/* Handlers
 */
void cdir_handler(char * line)
{
    if (chdir(line+strlen(CHANGE_DIR)))
	fprintf(stderr, "error while changing directory\n");
}

void shell_handler(char * line)
{
    system(line+strlen(SHELL_ESCAPE));
}

void quit_handler(char * line)
{
    char *file_name = default_hist_file_name();

    /*   close history: truncate list and write history file
     */
    stifle_history(HPFC_HISTORY_LENGTH);
    write_history(file_name);
    
    exit(0);
}

void default_handler(char * line)
{
    char *shll = (char*)
	malloc(sizeof(char)*(strlen(HPFC_SHELL)+strlen(line)+2));
    
    system(sprintf(shll, "%s %s", HPFC_SHELL, line));

    free(shll);
}

struct t_handler 
{
    char * name;
    void (*function)(char *);
} ;

static struct t_handler handlers[] =
{
  { QUIT,		quit_handler },
  { CHANGE_DIR, 	cdir_handler },
  { SHELL_ESCAPE, 	shell_handler },
  { (char *) NULL, 	default_handler}
};

/* the lexer is quite simple:-)
 */
#define PREFIX_EQUAL_P(str, prf) (strncmp(str, prf, strlen(prf))==0)

static void (*find_handler(char* line))(char *)
{
    struct t_handler * x = handlers;
    while ((x->name) && !PREFIX_EQUAL_P(line, x->name)) x++;
    return x->function;
}

/* MAIN: interactive loop and history management.
 */
int main()
{
    char *last, *line;

    last = initialize_hpfc_history();

    /*  interactive loop
     */
    while ((line = readline(HPFC_PROMPT)))
    {
	/*   calls the appropriate handler.
	 */
	(find_handler(line))(line);

	/*   add to history if not the same as the last one.
	 */
	if (line && *line && ((last && strcmp(last, line)!=0) || (!last)))
	    add_history(line), last = line; 
	else
	    free(line);
    }

    fprintf(stdout, "\n"); /* for Ctrl-D terminations */
    quit_handler(line);
    return 0; 
}

/*   that is all
 */

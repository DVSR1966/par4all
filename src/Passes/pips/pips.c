#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>

#include "genC.h"
#include "ri.h"
#include "graph.h"
#include "makefile.h"
#include "properties.h"

#include "misc.h"
#include "ri-util.h"
#include "complexity_ri.h"

#include "constants.h"
#include "resources.h"

#include "database.h"
#include "pipsdbm.h"

#include "pipsmake.h"

#include "top-level.h"

jmp_buf pips_top_level;

extern void (*pips_error_handler)();
extern void (*pips_log_handler)();
extern void (*pips_warning_handler)();
extern void set_bool_property();

char *usage = 
	"Usage: %s [-f F]* [-m M] [-s S]* [-p P] [-b B] [-(0|1) T]* wspace\n"
	"\t-f F: source file F\n"
	"\t-m M: module M\n"
	"\t-s S: select rule S\n"
	"\t-p P: perform rule P\n"
	"\t-b B: build resource B\n"
	"\t-(0|1) T: set boolean property T to FALSE or TRUE\n" ;

static char *wspace = NULL;
static char *module = NULL;
static char *performed_rule = NULL;
static list build_resource_names = NIL;
static list source_files = NIL;
static list selected_rules = NIL;

static void pips_parse_arguments(argc, argv)
int argc;
char * argv[];
{
    int c;
    extern char *optarg;
    extern int optind;

    if (argc < 2) {
	fprintf(stderr, usage, argv[0]);
	exit(1);
    }

    while ((c = getopt(argc, argv, "f:m:s:p:b:1:0:")) != -1)
	switch (c) {
	case 'f':
	    source_files= gen_nconc(source_files, 
				    CONS(STRING, optarg, NIL));
	    break;
	case 'm':
	    module= optarg;
	    break;
	case 's':
	    selected_rules= gen_nconc(selected_rules, 
				      CONS(STRING, optarg, NIL));
	    break;
	case 'p':
	    performed_rule= optarg;
	    break;
	case 'b':
	    build_resource_names = gen_nconc(build_resource_names,
					     CONS(STRING, optarg, NIL));
	    break;
	/*
	 * next two added to deal with boolean properties directly
	 * FC, 27/03/95
	 */
	case '1':
	    set_bool_property(optarg, TRUE);
	    break;
	case '0':
	    set_bool_property(optarg, FALSE);
	    break;
	case '?':
	    fprintf(stderr, usage, argv[0]);
	    exit(1);
	    ;
	}
    
    if (argc != optind + 1) {
	user_warning("pips_parse_argument", 
		     ((argc < (optind + 1)) ?
		     "Too few arguments\n" : "illegal argument: %s\n"),
		     argv[optind + 1]);
	fprintf(stderr, usage, argv[0]);
	exit(1);
    }
    wspace= argv[argc - 1];
}

void
select_rule(rule_name)
char *rule_name;
{
    user_log("Selecting rule: %s\n", rule_name);

    activate(rule_name);

    if(get_debug_level()>5)
	fprint_activated(stderr);
}

/* Pips user log */

void pips_user_log(char *fmt, va_list args)
{
    FILE * log_file = get_log_file();

    if(log_file!=NULL) {
	if (vfprintf(log_file, fmt, args) <= 0) {
	    perror("tpips_user_log");
	    abort();
	}
	else
	    fflush(log_file);
    }

    if(get_bool_property("USER_LOG_P")==FALSE)
	return;

    /* It goes to stderr to have only displayed files on stdout */
    (void) vfprintf(stderr, fmt, args);
    fflush(stderr);
}

void main(argc, argv)
int argc;
char * argv[];
{
    bool success = TRUE;

    initialize_newgen();
    initialize_sc((char*(*)(Variable)) entity_local_name); 

    pips_parse_arguments(argc, argv);

    debug_on("PIPS_DEBUG_LEVEL");
    initialize_signal_catcher();
    pips_log_handler = pips_user_log;

    if(setjmp(pips_top_level)) {
	/* no need to pop_pips_context() at top-level */
	/* FI: are you sure make_close_program() cannot call user_error() ? */
	close_workspace();
	success = FALSE;
    }
    else 
    {
	push_pips_context(&pips_top_level);
	/* push_performance_spy(); */

	/* Initialize workspace
	 */

	if (source_files != NIL) {
	    /* Workspace must be created */
	    db_create_workspace(wspace);

	    /* FI: The next lines should be replaced by a call to 
	     * create_workspace()
	     */

	    open_log_file();
	    set_entity_to_size();

	    MAPL(f_cp, {
		debug(1, "main", "processing file %s\n", STRING(CAR(f_cp)));
		process_user_file( STRING(CAR(f_cp)) );
	    }, source_files);

	    wspace = db_get_current_workspace_name();
	    user_log("Workspace %s created and opened\n", wspace);
	}
	else {
	    /* Workspace must be opened */
	    if (open_workspace(wspace) == NULL) {
		user_log("Cannot open workspace %s\n", wspace);
		exit(1);
	    }
	    else {
		/* user_log("Workspace %s opened\n", wspace); */
		;
	    }
	}

	/* Open module
	 */

	if (module != NULL) {
	    /* CA - le 040293- remplacement de db_open_module(module) par */
	    open_module(module);
	}
	else {
	    open_module_if_unique();
	}

	/* Activate rules
	 */

	if (success && selected_rules != NIL) {
	    /* Select rules */
	    MAPL(r_cp, {
		select_rule(STRING(CAR(r_cp)));
	    }, selected_rules);
	}

	/* Perform applies
	 */

	if (success && performed_rule != NULL) {
	    /* Perform rule */
	    /*
	    user_log("Request: perform rule %s for module %s.\n", 
		     performed_rule, module);
		     */
	    success = safe_apply(performed_rule, module);
		if (success) {
		    user_log("%s performed for %s.\n", 
			     performed_rule, module);
		}
		else {
		    user_log("Cannot perform %s for %s.\n", 
			     performed_rule, module);
		}
	}

	/* Build resources
	 */

	if (success && build_resource_names != NIL) {
	    /* Build resource */
	    MAPL(crn, {
		string build_resource_name = STRING(CAR(crn));
		/* user_log("Request: build resource %s for module %s.\n", 
			 build_resource_name, module); */
		success = safe_make(build_resource_name, module);
		if (success) {
		    /* user_log("%s built for %s.\n", 
			     build_resource_name, module); */
		    ;
		}
		else {
		    user_log("Cannot build %s for %s.\n", 
			     build_resource_name, module);
		    break;
		}
	    }, build_resource_names);
	}
	
	/* whether success or not... */
	close_workspace();
	/* pop_performance_spy(stderr, "pips"); */
    }

    debug_off();
    exit(!success);
}

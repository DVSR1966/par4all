 /* procedures used in both PIPS top-level, wpips and tpips */
 /* problems to use those procedures with wpips: show_message() and 
    update_props() .
  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dirent.h>

#include <sys/stat.h>
#include <ctype.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>

#include "genC.h"
#include "ri.h"
#include "database.h"
#include "makefile.h"

#include "misc.h"

#include "ri-util.h"
#include "pipsdbm.h"

#include "constants.h"
#include "resources.h"
#include "phases.h"

#include "property.h"
#include "pipsmake.h"

#include "top-level.h"

#define LINE_LENGTH 128


/* Return a sorted arg list of workspace names. (For each name, there
   is a name.database directory in the current directory): */
void
pips_get_workspace_list(
    int * pargc,
    char * argv[])
{
   int i;

   /* Find all directories with name ending with ".database": */

   list_files_in_directory(pargc, argv, ".",
                           "^.*\\.database$", directory_exists_p);

   /* Remove the ".database": */
   for (i = 0; i < *pargc; i++) {
      *strchr(argv[i], '.') = '\0';
   }
}


/* Select the true file with names ending in ".f" and return a sorted
   arg list: */
void
pips_get_fortran_list(int * pargc,
                      char * argv[])
{
    list_files_in_directory(pargc, argv, ".", "^.*\\.f$", file_exists_p);
}


/* Return the path of an HPFC file name relative to the current PIPS
   directory. Can be freed by the caller. */
char *
hpfc_generate_path_name_of_file_name(char * file_name)
{
   return concatenate(build_pgmwd(db_get_current_workspace_name()),
                      "/",
                      HPFC_COMPILED_FILE_DIR,
                      "/",
                      file_name,
                      NULL);
}


int
hpfc_get_file_list(int * file_number,
                   char * file_names[],
                   char ** hpfc_directory_name)
{
   static char hpfc_dir[MAXNAMLEN + 1];
   int return_code;
        
   /* Get the HPFC file name list: */
   sprintf(hpfc_dir, "%s/%s",
           build_pgmwd(db_get_current_workspace_name()),
           HPFC_COMPILED_FILE_DIR);
   
   return_code = safe_list_files_in_directory(file_number,
                                              file_names,
                                /* Where is the output of HPFC: */
                                              hpfc_dir,
                                /* generated files start with upercases */
                                              "^[A-Z].*\\.[fh]$",
                                /* Plain files only: */
                                              file_exists_p);
   *hpfc_directory_name = hpfc_dir;

   return return_code;
}


char *pips_change_directory(dir)
char *dir;
{
    if (directory_exists_p(dir)) {
	chdir(dir);
	/*
	  log_execl("pwd", NULL);
	  log_execl("/bin/echo Available Fortran Files:", NULL);
	  log_execl("/bin/ls -C *.f", NULL);
	  log_execl("/bin/echo", NULL);
	  log_execl("/bin/echo Available Workspaces:", NULL);
	  log_execl("/bin/ls -C *.DATABASE | sed s/.DATABASE//g", 
	  NULL);
	  */

	return(get_cwd());	
    }

    return(NULL);
}

char *
build_view_file(char * print_type)
{
   char * module_name = db_get_current_module_name();

   if(!unloadable_file_p(print_type)) {
       user_error("build_view_file", "resource %s cannot be displayed\n",
		   print_type);
   }

   if(module_name != NULL) {
      if ( safe_make(print_type, module_name) ) {
         static char file_name_in_database[MAXPATHLEN];
           
         char * file_name = db_get_file_resource(print_type, module_name, TRUE);
         sprintf(file_name_in_database, "%s/%s",
                 build_pgmwd(db_get_current_workspace_name()),
                 file_name);
            
         return file_name_in_database;
      }
   }
   else {
      /* should be show_message */
      user_log("No current module; select a module\n");
   }
   return NULL;
}

char *
get_dont_build_view_file(char * print_type)
{
   char *module_name = db_get_current_module_name();

   if(module_name != NULL) {
      /* Allow some place for "/./" and other useless stuff: */
       static char file_name_in_database[MAXPATHLEN];
           
      char * file_name = db_get_file_resource(print_type, module_name, TRUE);

      sprintf(file_name_in_database, "%s/%s",
              build_pgmwd(db_get_current_workspace_name()),
              file_name);
            
      return file_name_in_database;
   }
   else {
      /* should be show_message */
      user_log("No current module; select a module\n");
   }
   return NULL;
}

char *read_line(fd)
FILE *fd;
{
    static char line[LINE_LENGTH];
    if (fgets(line, LINE_LENGTH, fd) != NULL) {
	int l = strlen(line);

	if (l > 0)
	    line[l-1] = '\0';

	return(line);
    }

    return(NULL);
}

/*************************** MODULE PROCESSING (INCLUDES and IMPLICIT NONE) */

static string real_file_dir = NULL;
static void reset_real_file_dir(void)
{
    free(real_file_dir);
    real_file_dir = NULL;
}
static void set_real_file_dir(string path)
{
    int l = strlen(path);
    if (real_file_dir) reset_real_file_dir();
    real_file_dir = strdup(path);
    while (l>0 && real_file_dir[l]!='/') l--;
    real_file_dir[l]= '\0';
}

#ifdef NO_RX_LIBRARY

static bool
pips_process_file(string file_name)
{
    int err = safe_system_no_abort(concatenate
       ("trap 'exit 123' 2; pips-process-module ", file_name, NULL));

    if(err==123) {
	user_warning("process_user_file",
		     "pips-process-module interrupted by control-C\n");
	return FALSE;
    }
    else if(err!=0) 
	pips_internal_error
	    ("Unexpected return code from pips-process-module: %d\n", err);

    return TRUE;
}

#else

#include "rxposix.h"

#define IMPLICIT_NONE_RX "^[ \t]*implicit[ \t]*none"
#define INCLUDE_FILE_RX "^[ \t]*include[ \t]*['\"]\\([^'\"]*\\)['\"]"

static regex_t implicit_none_rx, include_file_rx;
static FILE *output_file;

/* tries several path for a file to include...
 */
static string find_file(string name)
{
    string other;

    other = strdup(concatenate(real_file_dir, "/", name, NULL));
    if (file_exists_p(other))
	return other;
    free(other);

    if (file_exists_p(name)) return strdup(name);

    other = strdup(concatenate("../", name, NULL));
    if (file_exists_p(other))
	return other;
    free(other);
    
    return NULL;
}

static void handle_file(FILE*);
static void handle_file_name(char * file_name, bool comment)
{
    FILE * f;
    string found = find_file(file_name);

    if (!found)
    {
	pips_user_error("include file %s not found\n", file_name);
	fprintf(output_file, "! include \"%s\" not found\n", file_name);
    }

    pips_debug(2, "including file \"%s\"\n", found);

    if (comment)
	fprintf(output_file, "! include \"%s\"\n", file_name);

    f=safe_fopen(found, "r");
    handle_file(f);
    safe_fclose(f, found);

    if (comment)
	fprintf(output_file, "! end include \"%s\"\n", file_name);

    free(found);
}

static void handle_file(FILE * f) /* process f for includes and nones */
{
    char line[LINE_LENGTH];
    regmatch_t matches[2]; /* matched strings */
    int nline = 0;

    while (!fgets(line, LINE_LENGTH, f))
    {
	nline++;
	
	if (!regexec(&include_file_rx, line, 2, matches, 0))
	{
	    line[matches[1].rm_eo]='\0';
	    handle_file_name(&line[matches[1].rm_so], TRUE);
	}
	else 
	{
	    if (!regexec(&implicit_none_rx, line, 0, matches, 0))
		fprintf(output_file, "! MIL-STD-1553 Fortran not in PIPS\n! ");
	    
	    fprintf(output_file, "%s", line);
	}
    }
    
    pips_debug(5, "NULL out after %d lines\n", nline);
}

static void init_rx(void)
{
    static bool done=FALSE;
    if (done) return;
    done=TRUE;
    if (regcomp(&implicit_none_rx, IMPLICIT_NONE_RX, REG_ICASE) ||
	regcomp(&include_file_rx, INCLUDE_FILE_RX, REG_ICASE))
	abort();
}

static bool pips_process_file(string file_name)
{
    string origin = strdup(concatenate(file_name, ".origin", NULL));
    
    if (rename(file_name, origin))
	pips_internal_error("error while renaming %s as %s\n",
			    file_name, origin);

    pips_debug(2, "processing file %s\n", file_name);

    output_file = safe_fopen(file_name, "w");
    init_rx();
    handle_file_name(origin, FALSE);
    safe_fclose(output_file, origin);
    free(origin);

    return TRUE;
}

#endif

static bool zzz_file_p(string s) /* zzz???.f */
{ return strlen(s)==9 && s[0]=='z' && s[1]=='z' && s[2]=='z' &&
      s[6]=='.' && s[7]=='f' && s[8]=='\n'; }
#define MAX_NLINES 1000
#define MAX_LENGTH 100
static int cmp(char**x1, char**x2)
{ return strcmp(*x1, *x2);}
static void sort_file(string name)
{
    FILE *f;
    char * lines[MAX_NLINES];
    char line[MAX_LENGTH];
    int i=0;

    f=safe_fopen(name, "r");
    while (!feof(f))
    {
	string sg = fgets(line, MAX_LENGTH, f);
	pips_assert("not too many lines", i<MAX_NLINES);
	if (sg && !zzz_file_p(sg)) /* drop zzz* files */
            lines[i++]=strdup(sg);
    }
    safe_fclose(f, name);

    qsort(lines, i, sizeof(char*), &cmp);

    f=safe_fopen(name, "w");
    while (i>0) {
	fprintf(f, "%s", lines[--i]);
	free(lines[i]);
    }
    safe_fclose(f, name);
}

static bool pips_split_file(string name, string tempfile)
{
#ifdef NO_INTERNAL_FSPLIT
    int err = safe_system_no_abort
	(concatenate("trap 'exit 123' 2;",
		     "pips-split ", abspath,
		     "| sed -e /zzz[0-9][0-9][0-9].f/d | sort -r > ",
		     tempfile, "; /bin/rm -f zzz???.f", NULL));

    if(err==123)
	user_warning("process_user_file",
		     "File splitting interrupted by control-C\n");
    else if(err!=0)
	pips_error("process_user_file",
		   "Unexpected return code from pips-split: %d\n", err);

    return err;
#else
    int err;
    FILE * out = safe_fopen(tempfile, "w");
    err = fsplit(name, out);
    safe_fclose(out, tempfile);

    sort_file(tempfile);

    return err;
#endif
}

bool process_user_file(file)
string file;
{
    bool success_p = FALSE;
    database pgm;
    FILE *fd;
    char *cwd;
    static char buffer[MAXNAMLEN];
    string abspath = NULL;
    /* string relpath = NULL; */
    static char *tempfile = NULL;
    static int number_of_files = 0;
    static int number_of_modules = 0;
    int err;

    number_of_files++;
    pips_debug(1, "file %s (number %d)\n", file, number_of_files);

    if (! file_exists_p(file)) {
	pips_user_warning("Cannot open file : %s\n", file);
	return FALSE;
    }

    if (tempfile == NULL) {
	tempfile = tmpnam(NULL);
    }

    pgm = db_get_current_workspace();

    /* the absolute path of file is calculated */
    abspath = strdup((*file == '/') ? file : 
		     concatenate(get_cwd(), "/", file, NULL));

    set_real_file_dir(abspath);

    /* the new file is registered in the database
     */
    user_log("Registering file %s\n", file);

    /* FI: two problems here
       - the successive calls to DB_PUT_FILE_RESOURCE erase each other...
       - the wiring of the database_name prevents mv of the database (fixed)
       */
    /* DB_PUT_FILE_RESOURCE(DBR_USER_FILE, database_name(pgm), abspath); */
    DB_PUT_FILE_RESOURCE(DBR_USER_FILE, "", abspath);

    /* the new file is splitted according to Fortran standard */

    user_log("Splitting file    %s\n", file);

    cwd = strdup(get_cwd());
    chdir(db_get_current_workspace_directory());

    /* reverse sort because the list of modules is reversed later */
    /* if two modules have the same name, the first splitted wins
       and the other one is hidden by the call to "sed" since
       fsplit gives it a zzz00n.f name */
    /* Let's hope no user module is called zzz???.f */

    err = pips_split_file(abspath, tempfile);

    /* Go back unconditionnally to regular directory for execution
     * or you are heading for trouble when the database is closed
     */
    chdir(cwd);
    free(cwd);

    if (err) return FALSE;

    /* the newly created module files are registered in the database */
    fd = safe_fopen(tempfile, "r");
    while (fscanf(fd, "%s", buffer) != EOF) {
	char *modname;
	char * modrelfilename = NULL;

	number_of_modules++;
	pips_debug(2, "module %s (number %d)\n", buffer, number_of_modules);

	success_p = TRUE;

	modrelfilename = strdup(buffer);

	*strchr(buffer, '.') = '\0';
	(void) strupper(buffer, buffer);
	modname = strdup(buffer);

	user_log("  Module         %s\n", modname);

        /* Apply a cleaning procedure on each module: */
        cwd = strdup(get_cwd());
        chdir(db_get_current_workspace_directory());
        if (!pips_process_file(modrelfilename))
	    return FALSE;
        chdir(cwd);
        free(cwd);

	if(DB_PUT_NEW_FILE_RESOURCE(DBR_SOURCE_FILE, 
				    modname, modrelfilename)
	   == resource_undefined) {
	    user_warning("process_user_file", 
			 "Two source codes for module %s."
			 "The second occurence in file %s is ignored\n",
			 modname, file);
	}
    }
    safe_fclose(fd, tempfile);

    unlink(tempfile);
    tempfile = NULL;

    if(!success_p) {
	user_warning("", "No module was found when splitting file %s.\n",
		     abspath);
    }

    reset_real_file_dir();

    return success_p;
}

#include <stdio.h>

#include "genC.h"

#include "ri.h"
#include "misc.h"
#include "database.h"
#include "ri-util.h"
#include "pipsdbm.h"
#include "properties.h"

#define LOG_FILE "LOGFILE"
/* The log file is closed by default */
static FILE *log_file = NULL;
/* Used in information messages */
char log_file_name[MAXPATHLEN];



void
close_log_file()
{
   if (log_file != NULL && get_bool_property("USER_LOG_P") == TRUE)
      if (fclose(log_file) != 0) {
	  pips_error("close_log_file", "Could not close\n");
         perror("close_log_file");
         abort();
      }
   log_file = NULL;
}


void
open_log_file()
{
   if (log_file != NULL)
      close_log_file();

   if (get_bool_property("USER_LOG_P") == TRUE) {
      sprintf(log_file_name, "%s/%s",
              db_get_current_workspace_directory(),
              LOG_FILE);
      
      if ((log_file = fopen(log_file_name, "a")) == NULL) {
         perror("open_log_file");
         abort();
      }
   }
}


FILE * get_log_file()
{
    return log_file;
}

void
log_on_file(char chaine[])
{
   if (log_file != NULL /* && get_bool_property("USER_LOG_P") == TRUE */) {
      if (fprintf(log_file, "%s", chaine) <= 0) {
         perror("log_on_file");
         abort();
      }
      else
         fflush(log_file);
   }
}

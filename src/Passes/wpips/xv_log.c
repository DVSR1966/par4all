/* 	%A% ($Date: 1995/09/15 15:58:26 $, ) version $Revision$, got on %D%, %T% [%P%].\n Copyright (c) �cole des Mines de Paris Proprietary.	 */

#ifndef lint
static char vcid[] = "%A% ($Date: 1995/09/15 15:58:26 $, ) version $Revision$, got on %D%, %T% [%P%].\n Copyright (c) �cole des Mines de Paris Proprietary.";
#endif /* lint */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/text.h>
#include <xview/textsw.h>
#include <xview/notice.h> 
#include <xview/xv_error.h>
#include <setjmp.h>

#include "genC.h"
#include "misc.h"
#include "ri.h"
#include "database.h"
#include "pipsdbm.h"

#include "wpips.h"

static Textsw log_textsw;
static Menu_item open_front, clear, close;

#define LOG_FILE "LOGFILE"
/* Par de'faut, le fichier est ferme' : */
static FILE *log_file = NULL;
static char log_file_name[MAXPATHLEN];



void
close_log_file()
{
   if (log_file != NULL && get_bool_property("USER_LOG_P") == TRUE)
      if (fclose(log_file) != 0) {
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
              database_directory(db_get_current_workspace()),
              LOG_FILE);
      
      if ((log_file = fopen(log_file_name, "a")) == NULL) {
         perror("open_log_file");
         abort();
      }
   }
}


static void
log_on_file(char chaine[])
{
   if (log_file != NULL && get_bool_property("USER_LOG_P") == TRUE) {
      if (fprintf(log_file, "%s", chaine) <= 0) {
         perror("log_on_file");
         abort();
      }
      else
         fflush(log_file);
   }
}


void
prompt_user(string a_printf_format, ...)
{
   Event e;
   va_list some_arguments;
   static char message_buffer[SMALL_BUFFER_LENGTH];

   va_start(some_arguments, a_printf_format);

   (void) vsprintf(message_buffer, a_printf_format, some_arguments);

   if (wpips_emacs_mode) 
      send_prompt_user_to_emacs(message_buffer);
   (void) notice_prompt(xv_find(main_frame, WINDOW, 0), 
                        &e,
                        NOTICE_MESSAGE_STRINGS,
                        message_buffer,
                        0,
                        NOTICE_BUTTON_YES,	"Press Here",
                        0);
}


static void
insert_something_in_the_wpips_log_window(char * a_message)
{
   int new_length;
   int message_length = strlen(a_message);
   int old_length = (int) xv_get(log_textsw, TEXTSW_LENGTH);

   /* Try to insert at the end: */
   xv_set(log_textsw,
          TEXTSW_INSERTION_POINT, old_length,
          NULL);
   textsw_insert(log_textsw, a_message, message_length);
   /* Verify it fitted: */
   new_length = (int) xv_get(log_textsw, TEXTSW_LENGTH);
   if (new_length != old_length + message_length) {
      /* It ran out of space! */
      /* recreate_log_window(); */
      /* Discard the content without keeping the undo buffer: */
      textsw_reset(log_textsw, 0, 0);
      
      xv_set(log_textsw,
             TEXTSW_INSERTION_POINT, 0,
             NULL);
      /* Hope there is no use of recursion... */
      textsw_insert(log_textsw, a_message, message_length);

      prompt_user("As you have just clicked, "
                  "you saw the log window was full :-) ...\n"
                  "It is now cut down to 0 byte again...\n\n"
                  "Anyway, you can retrieve all the log content "
                  "in the file \"%s\"\n",
                  log_file_name);
   }   

   textsw_possibly_normalize(log_textsw, 
                             (Textsw_index) xv_get(log_textsw, TEXTSW_INSERTION_POINT));

   XFlush((Display *) xv_get(main_frame, XV_DISPLAY));
   XFlush((Display *) xv_get(log_frame, XV_DISPLAY));

   xv_set(clear, MENU_INACTIVE, FALSE, 0);
}


void
wpips_user_error_message(char error_buffer[])
{
   extern jmp_buf pips_top_level;

   log_on_file(error_buffer);

   if (wpips_emacs_mode) 
      send_user_error_to_emacs(error_buffer);
   else
      insert_something_in_the_wpips_log_window(error_buffer);

   show_message(error_buffer);

   prompt_user("Something went wrong. Check the log window");
      
   /* terminate PIPS request */
   if(get_bool_property("ABORT_ON_USER_ERROR"))
      abort();

   longjmp(pips_top_level, 1);
      
   (void) exit(1);
}

void 
wpips_user_warning_message(char warning_buffer[])
{
   log_on_file(warning_buffer);

   if (wpips_emacs_mode) 
      send_user_warning_to_emacs(warning_buffer);
   else
      insert_something_in_the_wpips_log_window(warning_buffer);

   show_message(warning_buffer);
}


#define MAXARGS     100

void
wpips_user_log(string fmt, va_list args)
{
   static char log_buffer[SMALL_BUFFER_LENGTH];

   if(get_bool_property("USER_LOG_P")==FALSE)
      return;

   (void) vsprintf(log_buffer, fmt, args);

   log_on_file(log_buffer);

   if (wpips_emacs_mode) 
      send_user_log_to_emacs(log_buffer);
   else
      insert_something_in_the_wpips_log_window(log_buffer);
   /* Display the "Message:" line in the main window also in the emacs
      mode: */
   show_message(log_buffer);
}


void open_log_subwindow(menu, menu_item)
Menu menu;
Menu_item menu_item;
{
    xv_set(open_front, MENU_STRING, "Front", 0);
    xv_set(close, MENU_INACTIVE, FALSE, 0);
    unhide_window(log_frame);
}


void clear_log_subwindow(menu, menu_item)
Menu menu;
Menu_item menu_item;
{
    int l = (int) xv_get(log_textsw, TEXTSW_LENGTH);
    textsw_delete(log_textsw, 0, l);
    xv_set(clear, MENU_INACTIVE, TRUE, 0);
}



void close_log_subwindow(menu, menu_item)
Menu menu;
Menu_item menu_item;
{
    xv_set(open_front, MENU_STRING, "Open", 0); /*MENU_INACTIVE, FALSE, 0);*/
    xv_set(close, MENU_INACTIVE, TRUE, 0);
    hide_window(log_frame);
}


void create_log_menu()
{
    Menu menu;
    Panel_item log_button;

    open_front = xv_create(NULL, MENUITEM, 
		     MENU_STRING, "Open",
		     MENU_NOTIFY_PROC, open_log_subwindow,
		     MENU_RELEASE,
		     NULL);

    clear = xv_create(NULL, MENUITEM, 
		      MENU_STRING, "Clear",
		      MENU_NOTIFY_PROC, clear_log_subwindow,
		      MENU_INACTIVE, TRUE,
		      MENU_RELEASE,
		      NULL);

    close = xv_create(NULL, MENUITEM, 
		      MENU_STRING, "Close",
		      MENU_NOTIFY_PROC, close_log_subwindow,
		      MENU_INACTIVE, TRUE,
		      MENU_RELEASE,
		      NULL);

    menu = xv_create(XV_NULL, MENU_COMMAND_MENU, 
		     MENU_APPEND_ITEM, open_front,
		     MENU_APPEND_ITEM, clear,
		     MENU_APPEND_ITEM, close,
		     NULL);

    log_button = xv_create(main_panel, PANEL_BUTTON,
		     PANEL_LABEL_STRING, "Log  ",
		     PANEL_ITEM_MENU, menu,
		     0);

    if (wpips_emacs_mode)
       /* In fact, create it but disabled to keep the same frame
          layout in the Emacs mode: */
       xv_set(log_button, PANEL_INACTIVE, TRUE,
              NULL);
}


/* This works but it is cleaner to use textsw_reset() instead...
void
recreate_log_window()
{
   xv_destroy(log_textsw);
   log_textsw = (Xv_Window) xv_create(log_frame, TEXTSW, 0);
}
*/


void create_log_window()
{
    /* Xv_Window window; */


    log_textsw = (Xv_Window) xv_create(log_frame, TEXTSW, 0);
/* recuperation d'event ne fonctionne pas -> installer TEXTSW_NOTIFY_PROC, */
/* autre suggestion: mettre un masque X */

/*    window = (Xv_Window) xv_find(log_frame, WINDOW, 0);

    xv_set(window, 
	   WIN_CONSUME_X_EVENT_MASK, EnterWindowMask, 
	   WIN_EVENT_PROC, default_win_interpose, 
	   NULL);
*/
}

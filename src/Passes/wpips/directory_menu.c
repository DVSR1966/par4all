/* Generate a menu from the current directory to serve as a directory
   chooser. */


/* 	%A% ($Date: 1996/12/03 17:21:38 $, ) version $Revision$, got on %D%, %T% [%P%].\n Copyright (c) �cole des Mines de Paris Proprietary.	 */

#ifndef lint
char vcid_directory_menu[] = "%A% ($Date: 1996/12/03 17:21:38 $, ) version $Revision$, got on %D%, %T% [%P%].\n Copyright (c) �cole des Mines de Paris Proprietary.";
#endif /* lint */

#include <xview/xview.h>
#include <xview/panel.h>
/* To have SunOS 5.5 happy about MAXNAMELEN (in SunOS 4, it is already
   defined in sys/dirent.h): */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dirent.h>
/* To have SunOS4 still working: */
#ifndef MAXNAMELEN
#define MAXNAMELEN MAXNAMLEN
#endif
#include "genC.h"
#include "misc.h"
#include "database.h"
#include "ri.h"
#include "ri-util.h"
#include "pipsdbm.h"
#include "wpips.h"


enum {MENU_PATH_DATA_HANDLER = 54829,
/* Maximum size of the directory menu of the main frame: */
         WPIPS_MAX_DIRECTORY_MENU_SIZE = 80
};


static bool
accept_all_file_names(char * file_name)
{
   return TRUE;
}


static Menu
directory_gen_pullright(Menu_item menu_item,
                        Menu_generate op)
{
   Menu menu;
   
   if (op == MENU_DISPLAY) {
      char directory[MAXNAMELEN + 1];
      char * parent_directory;
      Menu parent = xv_get(menu_item, MENU_PARENT);

      debug(2, "directory_gen_pullright", "menu_item = %#x (%s), parent = %#x\n",
            menu_item, (char *) xv_get(menu_item, MENU_STRING), parent);
      
      /* First get the parent directory name that is the title: */
      parent_directory = (char *) xv_get(parent,
                                         XV_KEY_DATA, MENU_PATH_DATA_HANDLER);
      debug(2, "directory_gen_pullright", " parent_directory = %s\n",
            parent_directory);
      
      /* Build the new directory name: */
      (void) sprintf(directory, "%s/%s",
                     parent_directory,
                     (char *) xv_get(menu_item, MENU_STRING));
      
      if ((menu = (Menu) xv_get(menu_item, MENU_PULLRIGHT)) != NULL) {
         /* Well, there is already a menu... we've been already here.
            Free it first: */
         /* Free the associated directory name: */
         free((char *) xv_get(menu, XV_KEY_DATA, MENU_PATH_DATA_HANDLER));
         xv_destroy(menu);
      }
      
      /* Then initialize it with a new directory menu: */
      menu = generate_a_directory_menu(directory);
   }
   else
      /* A Menu-Generating procedure has to return the menu in any case: */
      menu = (Menu)xv_get(menu_item, MENU_PULLRIGHT);

   return menu;
}


static void
generate_a_directory_menu_notify(Menu menu, Menu_item menu_item)
{
   char full_directory_name[MAXNAMELEN + 1];
   char * directory_name = (char *) xv_get(menu_item, MENU_STRING);
   char * parent_path_name =
      (char *) xv_get(menu, XV_KEY_DATA, MENU_PATH_DATA_HANDLER);
   
   (void) sprintf(full_directory_name, "%s/%s",
                  parent_path_name, directory_name);
   (void) end_directory_notify(full_directory_name);
}


Menu
generate_a_directory_menu(char * directory)
{
   char *file_list[ARGS_LENGTH];
   int file_list_length;
   int return_code;
   int i;
   Menu menu;
   
   menu = xv_create(NULL, MENU,
                    /* The string is *not* copied in MENU_TITLE_ITEM: */
                    MENU_TITLE_ITEM, strdup(directory),
                    /* and furthermore MENU_TITLE_ITEM is write
                       only, so add the info somewhere else: */
                    XV_KEY_DATA, MENU_PATH_DATA_HANDLER, strdup(directory),
                    /* Add its own notifying procedure: */
                    MENU_NOTIFY_PROC, generate_a_directory_menu_notify,
                    NULL);
   debug(2, "generate_a_directory_menu", "menu = %#x (%s)\n",
         menu, directory);

   if (db_get_current_workspace() != database_undefined) {
      xv_set(menu, MENU_APPEND_ITEM,
             xv_create(XV_NULL, MENUITEM,
                       MENU_STRING,
                       "* Close the current workspace before changing directory *",
                       MENU_RELEASE,
                       MENU_INACTIVE, TRUE,
                       NULL),
             NULL);
      user_warning("generate_a_directory_menu",
                   "Close the current workspace before changing directory.\n");
   }
   else {
      /* Get all the files in the directory: */
      return_code = safe_list_files_in_directory(&file_list_length, file_list,
                                                 directory,
                                                 ".*", accept_all_file_names);
      if (return_code == -1 || file_list_length == 0)
         xv_set(menu, MENU_APPEND_ITEM,
                xv_create(XV_NULL, MENUITEM,
                          MENU_STRING,
                          "* No file in this directory or cannot be open *",
                          MENU_RELEASE,
                          MENU_INACTIVE, TRUE,
                          NULL),
                NULL);
      else if (file_list_length > WPIPS_MAX_DIRECTORY_MENU_SIZE) {
         xv_set(menu, MENU_APPEND_ITEM,
                xv_create(XV_NULL, MENUITEM,
                          MENU_STRING,
                          "* Too many files. Type directly in the Directory line of the main panel *",
                          MENU_RELEASE,
                          MENU_INACTIVE, TRUE,
                          NULL),
                NULL);
         user_warning("generate_a_directory_menu",
                      "Too many files in the \"%s\" directory.\n", directory);
      }
      else
         /* Generate a corresponding entry for each file: */
         for(i = 0; i < file_list_length; i++)
            /* Skip the "." directory: */
            if (strcmp(file_list[i], ".") != 0) {
               struct stat buf;
               char complete_file_name[MAXNAMELEN + 1];

               Menu_item menu_item =
                  xv_create(XV_NULL, MENUITEM,
                            MENU_STRING, strdup(file_list[i]),
                            MENU_RELEASE,
                            /* The strdup'ed string will also be
                               freed when the menu is discarded: */
                            MENU_RELEASE_IMAGE,
                            NULL);

               (void) sprintf(complete_file_name, "%s/%s", directory, file_list[i]);
               if (((stat(complete_file_name, &buf) == 0) 
                    && (buf.st_mode & S_IFDIR))) {
                  /* Since a menu item cannot be selected as an item, add an
                     plain item with the same name. Not beautiful
                     hack... :-( */
                  xv_set(menu,
                         MENU_APPEND_ITEM, menu_item,
                         NULL);
                  /* Now recreate another item that will be the submenu: */
                  menu_item =
                     xv_create(XV_NULL, MENUITEM,
                               MENU_STRING, strdup(file_list[i]),
                               MENU_RELEASE,
                               /* The strdup'ed string will also be
                                  freed when the menu is discarded: */
                               MENU_RELEASE_IMAGE,
                               /* Put a right menu on each directory
                                  entry: */
                               MENU_GEN_PULLRIGHT, directory_gen_pullright,
                               NULL);
                  debug(2, "generate_a_directory_menu", " menu_item = %#x (%s)\n",
                        menu_item, file_list[i]);
               }
               else
                  /* And disable non-subdirectory entry: */
                  xv_set(menu_item, MENU_INACTIVE, TRUE, NULL);
         
               xv_set(menu, MENU_APPEND_ITEM, menu_item, NULL);    
            }
   }
   return menu;
}

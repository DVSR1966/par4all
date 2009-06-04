/*

  $Id$

  Copyright 1989-2009 MINES ParisTech

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/
/* $Id: xv_select.c 12801 2007-10-19 07:08:13Z coelho $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <gtk/gtk.h>

#if (defined(TEXT))
#undef TEXT
#endif

#if (defined(TEXT_TYPE))
#undef TEXT_TYPE
#endif

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "makefile.h"
#include "database.h"

#include "misc.h"
#include "pipsdbm.h"
#include "pipsmake.h"
#include "top-level.h"
#include "gpips.h"

#include "resources.h"

/* Maximum size of the module menu of the main frame: */
enum {
	GPIPS_MAX_MODULE_MENU_SIZE = 50
};

GtkWidget * directory_menu_item;

static GtkWidget *create_menu_item, *open_menu_item, *close_menu_item,
		*module_menu_item;

success close_workspace_notify(GtkWidget * widget, gpointer data);

/* To enable or disable the menu items generated by generate_module_menu(): */
bool gpips_close_workspace_menu_inactive,
		gpips_close_workspace_menu_inactive_old,
		gpips_create_workspace_menu_inactive,
		gpips_open_workspace_menu_inactive,
		gpips_delete_workspace_menu_inactive, gpips_change_directory_inactive,
		gpips_change_directory_inactive_old;

/* Try to select a main module (that is the PROGRAM in the Fortran
 stuff) if no one is selected: */
void select_a_module_by_default() {
	char *module_name = db_get_current_module_name();

	if (module_name == NULL) {
		/* Ok, no current module, then find a main module (PROGRAM): */
		string main_module_name = get_first_main_module();

		if (!string_undefined_p(main_module_name)) {
			/* Ok, we got it ! Now we select it: */
			module_name = main_module_name;
			user_log("Main module PROGRAM \"%s\" found.\n", module_name);
			end_select_module_callback(module_name);
			/* GO: show_module() has already been called so return now */
			return;
		}
	}
	/* Refresh the module name on the status window: */
	show_module();
}

success end_directory_notify(char * dir) {
	char *s;

	/*   if (dir != NULL) {*/
	if ((s = pips_change_directory(dir)) == NULL) {
		user_log("Directory \"%s\" does not exist\n", dir);
		prompt_user("Directory \"%s\" does not exist\n", dir);
		show_directory();
		return FALSE;
	} else {
		user_log("Directory \"%s\" selected\n", dir);
		show_directory();
		return TRUE;
	}
}

void end_directory_text_notify(GtkWidget * text_item, gpointer data) {
	(void) end_directory_notify(gtk_entry_get_text(GTK_ENTRY(text_item)));
}

/* FC: uses an external wish script
 * I do not know how to activate this function safely...
 */
void direct_change_directory() {
	char newdir[MAXPATHLEN];
	char * tmp = strdup("/tmp/gpips.dir.XXXXXX");
	int i = 0, c;
	FILE * tmph;

	(void) mkstemp(tmp);

	if (gpips_change_directory_inactive)
		return; /* no cd in this state! */

	safe_system(concatenate("gpips-changedir -- ", get_cwd(), " > ", tmp, NULL));

	tmph = safe_fopen(tmp, "r");
	while ((c = getc(tmph)) != EOF && i < MAXPATHLEN)
		newdir[i++] = c;
	newdir[i - 1] = '\0'; /* last was \n */
	safe_fclose(tmph, tmp);

	end_directory_notify(newdir);

	unlink(tmp);
	free(tmp);

	return /* generate_workspace_menu(); */;
}

GtkWidget * generate_directory_menu() {
	return generate_a_directory_menu(get_cwd());
}

void prompt_user_not_allowed_to_change_directory(GtkWidget * widget,
		gpointer data) {
	/* First untype whatever the user typed: */
	show_directory();
	prompt_user("You have to close the current workspace"
		" before changing directory.");
}

void start_directory_notify(GtkWidget * widget, gpointer data) {
	if (db_get_current_workspace_name())
		prompt_user_not_allowed_to_change_directory(NULL, NULL);
	else
		start_query("Change Directory", "Enter directory path: ",
				"ChangeDirectory", end_directory_notify, cancel_query_notify);
}

void enable_change_directory() {
	gtk_widget_set_sensitive(GTK_WIDGET(directory_menu_item), TRUE);

	/* Enable the normal notify mode: notify when return and so is
	 typed: */
	gtk_widget_set_sensitive(GTK_WIDGET(directory_name_entry), TRUE);
	g_signal_connect(G_OBJECT(directory_name_entry), "activate", G_CALLBACK(
			end_directory_text_notify), NULL);

	gpips_change_directory_inactive_old = gpips_change_directory_inactive;
	gpips_change_directory_inactive = FALSE;
}

void disable_change_directory() {
	gtk_widget_set_sensitive(GTK_WIDGET(directory_menu_item), FALSE);

	/* In order to warn the user as soon as possible that (s)he can't
	 change the directory, notify for whatever character: */
	gtk_widget_set_sensitive(GTK_WIDGET(directory_name_entry), FALSE);
	g_signal_connect(G_OBJECT(directory_name_entry), "activate", G_CALLBACK(
			prompt_user_not_allowed_to_change_directory), NULL);

	gpips_change_directory_inactive_old = gpips_change_directory_inactive;
	gpips_change_directory_inactive = TRUE;
}

void static restore_enable_change_directory_state() {
	if (gpips_change_directory_inactive_old)
		disable_change_directory();
	else
		enable_change_directory();
}

void disable_workspace_create_or_open() {
	gtk_widget_set_sensitive(GTK_WIDGET(create_menu_item), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(open_menu_item), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(workspace_name_entry), FALSE);
	gpips_create_workspace_menu_inactive = TRUE;
}

void enable_workspace_create_or_open() {
	gtk_widget_set_sensitive(GTK_WIDGET(create_menu_item), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(open_menu_item), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(workspace_name_entry), TRUE);
	gpips_create_workspace_menu_inactive = FALSE;
}

void disable_workspace_delete_or_open() {
	/* Avoid also to delete a workspace during a creation in case the
	 name of the workspace is the same... */
	gpips_delete_workspace_menu_inactive = TRUE;
	gpips_open_workspace_menu_inactive = TRUE;
	gtk_widget_set_sensitive(GTK_WIDGET(workspace_name_entry), FALSE);
}

void enable_workspace_delete_or_open() {
	gpips_delete_workspace_menu_inactive = FALSE;
	gpips_open_workspace_menu_inactive = FALSE;
	gtk_widget_set_sensitive(GTK_WIDGET(workspace_name_entry), TRUE);
}

void disable_workspace_close() {
	gtk_widget_set_sensitive(GTK_WIDGET(close_menu_item), FALSE);
	gpips_close_workspace_menu_inactive_old
			= gpips_close_workspace_menu_inactive;
	/* For generate_module_menu(): */
	gpips_close_workspace_menu_inactive = TRUE;
}

void enable_workspace_close() {
	gtk_widget_set_sensitive(GTK_WIDGET(close_menu_item), TRUE);
	/* For generate_module_menu(): */
	gpips_close_workspace_menu_inactive = FALSE;
}

void restore_enable_workspace_close_state() {
	gpips_close_workspace_menu_inactive
			= gpips_close_workspace_menu_inactive_old;
}

void disable_module_selection() {
	gtk_widget_set_sensitive(GTK_WIDGET(module_menu_item), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(module_name_entry), FALSE);
	disable_view_selection();
	disable_transform_selection();
	disable_compile_selection();
	disable_option_selection();
}

void enable_module_selection() {
	gtk_widget_set_sensitive(GTK_WIDGET(module_menu_item), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(module_name_entry), TRUE);
	/* Well, after a workspace creation without automatic module
	 selection successful, there is no module selected and thus the
	 following menus are not very interestiong, except for the option
	 part of the option selection. Anyway, there is a guard in each
	 of these menu, so, just do nothing... :-) */
	enable_view_selection();
	enable_transform_selection();
	enable_compile_selection();
	enable_option_selection();
}

void end_delete_workspace_notify(char * name) {
	schoose_close();

	if (db_get_current_workspace_name() != NULL && strcmp(
			db_get_current_workspace_name(), name) == 0) {
		int result;

		GtkWidget * dialog = gtk_message_dialog_new(GTK_WINDOW(log_window),
				GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,
				GTK_BUTTONS_YES_NO, concatenate("The workspace ", name,
						" is currently opened!\n",
						"Do you really want to close and remove it ?", NULL));
		result = gtk_dialog_run(dialog);
		gtk_widget_destroy(dialog);

		if (result == GTK_RESPONSE_NO || !close_workspace_notify(NULL, NULL))
			goto do_not_delete_the_workspace;
	}

	(void) delete_workspace(name);

	do_not_delete_the_workspace: enable_workspace_delete_or_open();
	enable_workspace_create_or_open();
	restore_enable_workspace_close_state();
	restore_enable_change_directory_state();

	display_memory_usage();
}

void cancel_delete_workspace_notify() {
	/* Nothing to do. */
	enable_workspace_delete_or_open();
	enable_workspace_create_or_open();
	restore_enable_workspace_close_state();
	restore_enable_change_directory_state();
}

void start_delete_workspace_notify(GtkWidget * widget, gpointer data) {
	gen_array_t workspace_list;
	int workspace_list_length = 0;

	workspace_list = gen_array_make(0);
	pips_get_workspace_list(workspace_list);
	workspace_list_length = gen_array_nitems(workspace_list);

	if (workspace_list_length == 0) {
		prompt_user("No workspace available in this directory.");
	} else {
		disable_workspace_delete_or_open();
		disable_workspace_create_or_open();
		disable_workspace_close();
		disable_change_directory();

		schoose("Select the Workspace to Delete", workspace_list,
		/* current workspace as default choice : */
		db_get_current_workspace_name(), end_delete_workspace_notify,
				cancel_delete_workspace_notify);
	}
	gen_array_full_free(workspace_list);
}

void start_create_workspace_notify(GtkWidget * widget, gpointer data) {
	disable_workspace_delete_or_open();
	disable_workspace_create_or_open();
	disable_change_directory();

	if (db_get_current_workspace_name() != NULL)
		/* There is an open workspace: close it first: */
		if (!close_workspace_notify(NULL, NULL)) {
			/* If it fails: */
			cancel_create_workspace_notify(NULL, NULL);
			return;
		}

	start_query("Create Workspace", "Enter workspace name: ",
			"CreateWorkspace", continue_create_workspace_notify,
			/* Pas la peine de faire quelque chose si on appuie
			 sur cancel : */
			cancel_create_workspace_notify);
}

void cancel_create_workspace_notify(GtkWidget * widget, gpointer data) {
	/* enable the rights (in the gui) to create or open a workspace : */
	enable_workspace_create_or_open();
	enable_workspace_delete_or_open();
	enable_change_directory();
	cancel_query_notify(widget, data);
	show_workspace();
}

success continue_create_workspace_notify(char * name) {
	gen_array_t fortran_list;
	int fortran_list_length = 0;

	/* Is the name a valid workspace name? */
	if (!workspace_name_p(name)) {
		user_prompt_not_a_valid_workspace_name(name);
	} else {
		fortran_list = gen_array_make(0);
		pips_get_fortran_list(fortran_list);
		fortran_list_length = gen_array_nitems(fortran_list);

		if (fortran_list_length == 0) {
			prompt_user("No Fortran files in this directory");
		} else {
			/* Code added to confirm for a database destruction before
			 opening a database with the same name.
			 RK 18/05/1993. */
			if (workspace_exists_p(name)) {
				int result;

				GtkWidget * dialog = gtk_message_dialog_new(GTK_WINDOW(
						log_window), GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO, concatenate(
								"The database ", name, " already exists!\n",
								"Do you really want to remove it ?", NULL));
				result = gtk_dialog_run(dialog);
				gtk_widget_destroy(dialog);

				if (result == GTK_RESPONSE_NO) {
					goto continue_create_workspace_notify_failed;
				}
			}

			disable_workspace_create_or_open();

			// On enregistre le nom du workspace qui va être créé dans l'entry associée
			gtk_entry_set_text(GTK_ENTRY(workspace_name_entry), name);

			if (fortran_list_length == 1) {
				/* Only one Fortran program: use it without user
				 confirmation. */
				user_log(
						"There is only one Fortran program in the current directory.\n"
							"\tCreating the workspace \"%s\" from the file \"%s\"...\n",
						name, gen_array_item(fortran_list, 0));
				end_create_workspace_notify(fortran_list);
			} else {
				mchoose("Create Workspace", fortran_list,
						end_create_workspace_notify,
						(void(*)(void)) cancel_create_workspace_notify);
			}
			/* Memory leak if mchoose exit... */
			gen_array_full_free(fortran_list);

			return (TRUE);
		}
	}

	/* If it failed, cancel the creation: */
	continue_create_workspace_notify_failed: cancel_create_workspace_notify(
			NULL, NULL);

	return FALSE;
}

void user_prompt_not_a_valid_workspace_name(char * workspace_name) {
	prompt_user("The name \"%s\" is not a valid workspace name!\n",
			workspace_name);
	enable_workspace_create_or_open();
	disable_workspace_close();
	enable_change_directory();
}

void end_create_workspace_notify(gen_array_t files) {
	/* If the user click quickly on OK, be sure
	 end_create_workspace_notify() is not reentrant by verifying
	 something as not been opened already: */
	char* workspace_name_to_create = gtk_entry_get_text(GTK_ENTRY(
			workspace_name_entry));
	if (db_get_current_workspace_name() == NULL) {
		/* Is the name a valid workspace name? */
		if (workspace_name_p(workspace_name_to_create)) {
			if (db_create_workspace(workspace_name_to_create)) {
				/* The create workspace has been successful: */
				/* open_log_file(); */
				display_memory_usage();

				if (create_workspace(files)) {
					/* The processing of user files has been successful: */
					enable_workspace_close();

					show_workspace();
					select_a_module_by_default();
					enable_module_selection();
					disable_change_directory();

					enable_workspace_create_or_open();
					enable_workspace_delete_or_open();

					display_memory_usage();

					return;
				} else
					/* close_log_file(); */
					;
			}
		} else
			user_prompt_not_a_valid_workspace_name(workspace_name_to_create);

		/* The creation failed: */
		enable_change_directory();
		enable_workspace_create_or_open();
		enable_workspace_delete_or_open();

		display_memory_usage();
	}
}

void end_open_workspace_notify(string name) {
	schoose_close();

	if (open_workspace(name)) {
		/* open_log_file(); */
		enable_workspace_close();
		show_workspace();
		select_a_module_by_default();
		enable_module_selection();
		disable_change_directory();
	}

	enable_workspace_create_or_open();

	display_memory_usage();
}

void cancel_open_workspace_notify() {
	enable_workspace_create_or_open();
	enable_change_directory();
	show_workspace();
}

void open_workspace_notify(GtkWidget * widget, gpointer data) {
	gen_array_t workspace_list;
	int workspace_list_length = 0;

	workspace_list = gen_array_make(0);
	pips_get_workspace_list(workspace_list);
	workspace_list_length = gen_array_nitems(workspace_list);

	if (workspace_list_length == 0) {
		prompt_user("No workspace available in this directory.");
	} else if (workspace_list_length == 1) {
		/* There is only workspace: open it without asking confirmation
		 to the user: */
		user_log("There is only one workspace in the current directory.\n"
			"\tOpening the workspace \"%s\"...\n", gen_array_item(
				workspace_list, 0));
		end_open_workspace_notify(gen_array_item(workspace_list, 0));
	} else {
		disable_workspace_create_or_open();

		schoose("Select Workspace", workspace_list,
		/* Choix initial sur le workspace courant si
		 possible : */
		db_get_current_workspace_name(), end_open_workspace_notify,
				cancel_open_workspace_notify);
	}
	gen_array_full_free(workspace_list);
}

success close_workspace_notify(GtkWidget * widget, gpointer data) {
	success return_value;

	return_value = close_workspace(FALSE);
	debug(1, "close_workspace_notify", "return_value = %d\n", return_value);

	if (return_value) {
		/* The close has been successful: */
		/* close_log_file(); */
		initialize_gpips_hpfc_hack_for_fabien_and_from_fabien();

		edit_close_notify(NULL, NULL);

		enable_workspace_create_or_open();
		disable_workspace_close();

		/* It is the only place to enable a directory change, after a close
		 workspace: */
		enable_change_directory();

		disable_module_selection();
	}

	hide_window(schoose_window, NULL, NULL);
	show_workspace();
	show_module();
	display_memory_usage();

	return return_value;
}

/* To be used with schoose_create_abbrev_menu_with_text from the main
 panel: */
void open_or_create_workspace(char * workspace_name_original) {
	int i;
	gen_array_t workspace_list;
	int workspace_list_length = 0;
	char workspace_name[SMALL_BUFFER_LENGTH];

	/* If close_workspace_notify() is called below, show_workspace() will
	 set the name to "(* none *)" in the panel and
	 workspace_name_original is directly a pointer to it ! */
	(void) strncpy(workspace_name, workspace_name_original,
			sizeof(workspace_name) - 1);

	if (!workspace_name_p(workspace_name)) {
		/* Prompt the warning and restore the menu enable state: */
		user_prompt_not_a_valid_workspace_name(workspace_name);
		show_workspace();
		return;
	}

	if (db_get_current_workspace_name() != NULL)
		/* There is an open workspace: close it first: */
		if (!close_workspace_notify(NULL, NULL))
			return;

	/* To choose between open or create, look for the an existing
	 workspace with the same name: */
	workspace_list = gen_array_make(0);
	pips_get_workspace_list(workspace_list);
	workspace_list_length = gen_array_nitems(workspace_list);

	for (i = 0; i < workspace_list_length; i++) {
		string name = gen_array_item(workspace_list, i);
		if (strcmp(workspace_name, name) == 0) {
			/* OK, the workspace exists, open it: */
			end_open_workspace_notify(workspace_name);
			return;
		}
	}
	/* The workspace does not exist, create it: */
	disable_change_directory();
	gen_array_full_free(workspace_list);
	(void) continue_create_workspace_notify(workspace_name);
}

// "workspace name entry" is set to the value chosen in the choice menu
// and then we calls the callback associated to the activation of
// the "workspace name entry" thanks to the "activate" signal
// (opens or creates a workspace)
void select_workspace_notify(GtkWidget * w, gpointer data __attribute__((unused))) {
	gtk_entry_set_text(GTK_ENTRY(workspace_name_entry),
			gpips_gtk_menu_item_get_label(w));
	g_signal_emit_by_name(workspace_name_entry, "activate");
}

/* To use with schoose_create_abbrev_menu_with_text: */
GtkWidget * generate_workspace_menu() {
	GtkWidget * menu;

	GtkWidget * delete_menu_item;
	int i;
	gen_array_t workspace_list;
	int workspace_list_length = 0;

	workspace_list = gen_array_make(0);
	pips_get_workspace_list(workspace_list);
	workspace_list_length = gen_array_nitems(workspace_list);

	menu = gtk_menu_new();

	/* Replace the Select Workspace menu from the status window with
	 the following items: */
	GtkWidget * temp_item;

	temp_item = gtk_menu_item_new_with_label("Create Workspace");
	g_signal_connect(G_OBJECT(temp_item), "activate",
			start_create_workspace_notify, NULL);
	gtk_menu_append(GTK_MENU(menu), temp_item);
	gtk_widget_set_sensitive(temp_item, !gpips_create_workspace_menu_inactive);

	temp_item = gtk_menu_item_new_with_label("Close Workspace");
	g_signal_connect(G_OBJECT(temp_item), "activate", close_workspace_notify,
			NULL);
	gtk_menu_append(GTK_MENU(menu), temp_item);
	gtk_widget_set_sensitive(temp_item, !gpips_close_workspace_menu_inactive);

	delete_menu_item = gtk_menu_item_new_with_label("Delete Workspace");
	g_signal_connect(G_OBJECT(delete_menu_item), "activate",
			start_delete_workspace_notify, NULL);
	gtk_menu_append(GTK_MENU(menu), delete_menu_item);
	gtk_widget_set_sensitive(delete_menu_item,
			!gpips_delete_workspace_menu_inactive);

	/* Now complete with the list of the workspaces: */
	if (workspace_list_length == 0) {
		temp_item = gtk_menu_item_new_with_label(
				"* No workspace available in this directory *");
		gtk_menu_append(GTK_MENU(menu), temp_item);
		gtk_widget_set_sensitive(temp_item, FALSE);
		/* Well, since there is no workspace, there is nothing to
		 delete... */
		gtk_widget_set_sensitive(delete_menu_item, FALSE);
	} else {
		gtk_menu_append(GTK_MENU(menu), gtk_separator_menu_item_new());
		for (i = 0; i < workspace_list_length; i++) {
			string name = gen_array_item(workspace_list, i);
			temp_item = gtk_menu_item_new_with_label(name);
			gtk_menu_append(GTK_MENU(menu), temp_item);
			g_signal_connect(G_OBJECT(temp_item), "activate",
					select_workspace_notify, NULL);
			gtk_widget_set_sensitive(temp_item,
					!gpips_open_workspace_menu_inactive);
		}
	}

	gen_array_full_free(workspace_list);

	gtk_widget_show_all(menu);

	return menu;
}

void end_select_module_callback(string name) {
	gen_array_t module_list = db_get_module_list();
	int module_list_length = gen_array_nitems(module_list);

	if (module_list_length == 0) {
		prompt_user("No module available in this workspace.");
	} else {
		bool module_found = FALSE;
		int i;

		for (i = 0; i < module_list_length; i++) {
			string mn = gen_array_item(module_list, i);
			if (strcmp(name, mn) == 0) {
				module_found = TRUE;
				break;
			}
		}
		if (module_found)
			lazy_open_module(name);
		else
			prompt_user("The module \"%s\" does not exist in this workspace.",
					name);
	}

	show_module();
	display_memory_usage();
	gen_array_full_free(module_list);
}

void cancel_select_module_notify() {
}

void select_module_from_menubar_callback(GtkWidget * widget, gpointer data) {
	gen_array_t module_list = db_get_module_list();
	int module_list_length = gen_array_nitems(module_list);

	if (module_list_length == 0) {
		/* If there is no module... RK, 23/1/1993. */
		prompt_user("No module available in this workspace.");
	} else
		schoose("Select Module", module_list,
		/* Affiche comme choix courant le module
		 courant (c'est utile si on ferme la fen�tre
		 module entre temps) : */
		db_get_current_module_name(), end_select_module_callback,
				cancel_select_module_notify);

	gen_array_full_free(module_list);
}

static void select_module_from_status_menu_callback(GtkWidget * widget, gpointer data __attribute__((unused)))
{
	char * module_name = gpips_gtk_menu_item_get_label(widget);
	gtk_entry_set_text(GTK_ENTRY(module_name_entry), module_name);
	end_select_module_callback(module_name);
}

/* participe à la génération du menu associés aux modules dans la fenetre principale */
GtkWidget * generate_module_menu() {
	GtkWidget * menu;
	int i;

	menu = gtk_menu_new();

	if (db_get_current_workspace_name() == NULL) {
		GtkWidget * no_workspace_menu_item;
		no_workspace_menu_item = gtk_menu_item_new_with_label(
				"* No workspace yet! *");
		gtk_widget_set_sensitive(no_workspace_menu_item, FALSE);
		gtk_menu_append(GTK_MENU(menu), no_workspace_menu_item);
	} else {
		gen_array_t module_list = db_get_module_list();
		int module_list_length = gen_array_nitems(module_list);

		if (module_list_length == 0) {
			GtkWidget * no_workspace_menu_item;
			no_workspace_menu_item = gtk_menu_item_new_with_label(
					"* No module available in this workspace *");
			gtk_widget_set_sensitive(no_workspace_menu_item, FALSE);
			gtk_menu_append(GTK_MENU(menu), no_workspace_menu_item);
		} else {
			GtkWidget * module_menu_item;
			for (i = 0; i < module_list_length; i++) {
				string mn = gen_array_item(module_list, i);
				module_menu_item = gtk_menu_item_new_with_label(mn);
				gtk_menu_append(GTK_MENU(menu), module_menu_item);
				g_signal_connect(G_OBJECT(module_menu_item), "activate",
									select_module_from_status_menu_callback, NULL);
			}
		}
		gen_array_full_free(module_list);
	}

	gtk_widget_show_all(menu);
	return menu;
}

void create_select_menu() {
	GtkWidget *menu, *menu_item, *pmenu, *pmenu_item;

	create_menu_item = gtk_menu_item_new_with_label("Create");
	g_signal_connect(G_OBJECT(create_menu_item), "activate", G_CALLBACK(
			start_create_workspace_notify), NULL);

	open_menu_item = gtk_menu_item_new_with_label("Open");
	g_signal_connect(G_OBJECT(open_menu_item), "activate", G_CALLBACK(
			open_workspace_notify), NULL);

	close_menu_item = gtk_menu_item_new_with_label("Close");
	g_signal_connect(G_OBJECT(close_menu_item), "activate", G_CALLBACK(
			close_workspace_notify), NULL);
	gtk_widget_set_sensitive(GTK_WIDGET(close_menu_item), FALSE);

	pmenu = gtk_menu_new();
	gtk_menu_append(GTK_MENU(pmenu), open_menu_item);
	gtk_menu_append(GTK_MENU(pmenu), create_menu_item);
	gtk_menu_append(GTK_MENU(pmenu), close_menu_item);

	pmenu_item = gtk_menu_item_new_with_label("Deal with workspaces");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(pmenu_item), pmenu);

	module_menu_item = gtk_menu_item_new_with_label("Module");
	g_signal_connect(G_OBJECT(module_menu_item), "activate", G_CALLBACK(
			select_module_from_menubar_callback), NULL);
	gtk_widget_set_sensitive(GTK_WIDGET(module_menu_item), FALSE);

	directory_menu_item = gtk_menu_item_new_with_label("Directory");
	g_signal_connect(G_OBJECT(directory_menu_item), "activate", G_CALLBACK(
			start_directory_notify), NULL);

	menu = gtk_menu_new();
	gtk_menu_append(GTK_MENU(menu), module_menu_item);
	gtk_menu_append(GTK_MENU(menu), pmenu_item);
	gtk_menu_append(GTK_MENU(menu), directory_menu_item);

	menu_item = gtk_menu_item_new_with_label("Selection Menu");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);

	gtk_widget_show(menu_item);
	gtk_widget_show_all(menu);

	gtk_menu_bar_append(GTK_MENU_BAR(main_window_menu_bar), menu_item);
}

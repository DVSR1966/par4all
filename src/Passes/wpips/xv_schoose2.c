/* Single choice handling */

/* Difference with previous release: 
   1/ schoose_close() must be called in order to close the schoose window.
   2/ schoose() has one more argument, because cancel button is created.
   bb 04.06.91
 */

#include <stdio.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/text.h>
#include <types.h>

#include "genC.h"
#include "misc.h"

#include "wpips.h"

static Panel_item choice, choices, ok, help, cancel;
static void (*apply_on_choice)();
static void (*apply_on_cancel)();


static void schoose_help_notify(item, event)
Panel_item item;
Event *event;
{
    display_help("SingleChoice");
}

/* Cette routine est appel� d'une part lorsqu'on a cliqu� sur OK pour
   valider un nom tap� textuellement, d'autre part lorsqu'on clique sur
   un choix. */
static void schoose_ok_notify(item, event)
Panel_item item;
Event *event;
{
  /* Suppose qu'item et event sont folklo car on peut �tre appel� par
     schoose_choice_notify */
    char *curchoice;
	int i, nchoices;
	int item_is_in_the_list;

    curchoice = strdup((char *) xv_get(choice, PANEL_VALUE, 0));
    if (strlen(curchoice)==0)
		prompt_user("Choose one item or cancel");
	else {
			/* Modified to verify that an correct item is selected.
				RK, 21/05/1993. */
		nchoices = (int) xv_get(choices, PANEL_LIST_NROWS, 0);
		item_is_in_the_list = FALSE;
		for(i = 0; i < nchoices; i++)
			if (strcmp((char *)xv_get(choices, PANEL_LIST_STRING, i),
				curchoice) == 0) {
				item_is_in_the_list = TRUE;
				break;
			}
		if (item_is_in_the_list == FALSE)
			prompt_user("You have to choose one item of the list!");
		else
				/* Normal case : */
    		(*apply_on_choice)(curchoice);
	}

    free(curchoice);
}

/* schoose_close() can be called even when schoose window is already closed.
 */
void schoose_close()
{
    int i, nchoices;

    hide_window(schoose_frame);

    nchoices = (int) xv_get(choices, PANEL_LIST_NROWS, NULL);

    for (i = 0; i < nchoices; i++) {
	xv_set(choices, PANEL_LIST_DELETE, 0, NULL);
    }

    xv_set(choice, PANEL_VALUE, "", NULL);
}

void schoose_cancel_notify(item, event)
Panel_item item;
Event *event;
{
    schoose_close();

    (*apply_on_cancel)();
}

/* This function was rewritten bellow
static void choice_notify(item, op, event)
Panel_item item;
Panel_list_op op;
Event *event;
{
    if (op == PANEL_LIST_OP_SELECT) {
	char *s  = (char *) xv_get(item, PANEL_LABEL_STRING, NULL);

	xv_set(choice, PANEL_VALUE, s, NULL);
    }
}
*/

/* replaced previous implementation on 92.04.22, as we shifted to xview.3 */
static void schoose_choice_notify(item, op, event)
Panel_item item;
Panel_list_op op;
Event *event;
{
    int nchoices = (int) xv_get(choices, PANEL_LIST_NROWS, NULL);

    while (nchoices--) {
	if ((int) xv_get(choices, PANEL_LIST_SELECTED, nchoices) == TRUE) {
	    xv_set(choice, PANEL_VALUE, 
		   (char *)xv_get(choices, PANEL_LIST_STRING, nchoices),
		   NULL);
	}
    }

    /* �a y est, les choix sont faits ! */
    schoose_ok_notify((Panel_item) NULL, (Event *) NULL);
    /* De toute mani�re les arguments sont inutilis�s. */
}

void schoose(title, argc, argv, initial_choice, f, g)
char *title;
int argc;
char *argv[];
char *initial_choice;
void (*f)(), (*g)();
{
  int i;
  int nchoices;

  apply_on_choice = f;
  apply_on_cancel = g;

  xv_set(schoose_frame, FRAME_LABEL, title, NULL);

  /* reset the choice set to empty */
  nchoices = (int) xv_get(choices, PANEL_LIST_NROWS, 0);

  for (i = 0; i < nchoices; i++) {
    xv_set(choices, PANEL_LIST_DELETE, 0, NULL);
  }

  for (i = 0; i < argc; i++) {
    xv_set(choices, PANEL_LIST_STRING, i, argv[i], NULL);
  }

  /* Initialise au choix initial ou � d�faut le premier : */
  xv_set(choice, PANEL_VALUE, argv[0], NULL);
  if (initial_choice != NULL)
    for (i = 0; i < argc; i++)
      if (strcmp(initial_choice, argv[i]) == 0) {
	xv_set(choice, PANEL_VALUE, argv[i], NULL);
	xv_set(choices, PANEL_LIST_SELECT, i, TRUE, NULL);
	break;
      }
  
  unhide_window(schoose_frame);
  /* move the pointer to the center of the query window */
  pointer_in_center_of_frame(schoose_frame);
}


void create_schoose_window()
{
    schoose_frame = xv_create(main_frame, FRAME,
			      XV_SHOW, FALSE,
			      FRAME_DONE_PROC, hide_window,
			      NULL);

    schoose_panel = xv_create(schoose_frame, PANEL, NULL);

    choice = xv_create(schoose_panel, PANEL_TEXT,
		       PANEL_LABEL_STRING, "Current choice",
		       PANEL_VALUE_DISPLAY_LENGTH, 8,
		       PANEL_NOTIFY_PROC, schoose_ok_notify,
		       XV_X, 10,
		       XV_Y, 10,
		       NULL);

    choices = xv_create(schoose_panel, PANEL_LIST,
			PANEL_LABEL_STRING, "Available choices",
			PANEL_LIST_DISPLAY_ROWS, 5,
			PANEL_NOTIFY_PROC, schoose_choice_notify,
			PANEL_CHOOSE_ONE, TRUE,
			XV_X, 10,
			XV_Y, 40,
			NULL);

    help = xv_create(schoose_panel, PANEL_BUTTON,
		     PANEL_LABEL_STRING, "Help",
		     PANEL_NOTIFY_PROC, schoose_help_notify,
		     XV_X, 250,
		     XV_Y, 40,
		     NULL);

    cancel = xv_create(schoose_panel, PANEL_BUTTON,
		     PANEL_LABEL_STRING, "Cancel",
		   PANEL_NOTIFY_PROC, schoose_cancel_notify,
		   XV_X, 250,
		   XV_Y, 70,
		   NULL);

    ok = xv_create(schoose_panel, PANEL_BUTTON,
		   PANEL_LABEL_STRING, "OK",
		   PANEL_NOTIFY_PROC, schoose_ok_notify,
		   XV_X, 250,
		   XV_Y, 100,
		   NULL);

	(void) xv_set(schoose_panel, PANEL_DEFAULT_ITEM, ok, NULL);

    window_fit(schoose_panel);
    window_fit(schoose_frame);
}

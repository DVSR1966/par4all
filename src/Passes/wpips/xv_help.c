#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/text.h>

#include <types.h>

#include "genC.h"
#include "top-level.h"
#include "wpips.h"
#include "xv_sizes.h"

#include "genC.h"
#include "misc.h"

#define min(a,b) ((a) < (b) ? (a) : (b))
#define HELP_LINES 8

/* The URL of the PIPS documentation at the �cole des Mines de Paris: */
#define PIPS_DOCUMENTATION_URL "http://www.cri.ensmp.fr/~pips"

static Panel_item lines[HELP_LINES];
static char *help_list[ARGS_LENGTH];
static int help_list_length;

void
display_help(char * topic)
{
   int i;

   if (help_list_length != 0)
      /* help window is already opened */
      args_free(&help_list_length, help_list);

   get_help_topic(topic, &help_list_length, help_list);

   for (i = 0; i < min(HELP_LINES, help_list_length); i++) {
      xv_set(lines[i], PANEL_LABEL_STRING, help_list[i], 0);
   }

   for (i = min(HELP_LINES, help_list_length); i < HELP_LINES; i++) {
      xv_set(lines[i], PANEL_LABEL_STRING, "", 0);
   }

   unhide_window(help_frame);
}



static void
close_help_notify(Panel_item item,
                  Event * event)
{
   args_free(&help_list_length, help_list);

   hide_window(help_frame);
}



void
create_help_window()
{
   int i;
   Panel help_panel;

   help_panel = xv_create(help_frame, PANEL,
                          NULL);

   for (i = 0; i < HELP_LINES; i++) {
      lines[i] = xv_create(help_panel, PANEL_MESSAGE, 
                           XV_X, 15,
                           XV_Y, 15*(i+1),
                           0);
   }

   (void) xv_create(help_panel, PANEL_BUTTON,
                    PANEL_LABEL_STRING, "CLOSE",
                    XV_X, HELP_WIDTH/2-28,
                    XV_Y, 15*(HELP_LINES+1),
                    PANEL_NOTIFY_PROC, close_help_notify,
                    0);


}

static void
help_notify(Menu menu,
            Menu_item menu_item)
{
   display_help((char *) xv_get(menu_item, MENU_CLIENT_DATA));
}


static void
help_launch_pips_netscape(Menu menu,
                          Menu_item menu_item)
{
   system("netscape " PIPS_DOCUMENTATION_URL " &");
}


static void
help_launch_pips_xmosaic(Menu menu,
                          Menu_item menu_item)
{
   system("xmosaic " PIPS_DOCUMENTATION_URL " &");
}


void
create_help_menu()
{
   Menu menu;

   menu = xv_create(XV_NULL, MENU_COMMAND_MENU, 
                    MENU_TITLE_ITEM,
                    "The PIPS documentation",
                    MENU_GEN_PIN_WINDOW, main_frame, "Help Menu",
                    MENU_ITEM,
                    MENU_STRING, "Few words about \"Workspace\"...",
                    MENU_CLIENT_DATA, strdup("Workspace"),
                    MENU_NOTIFY_PROC, help_notify,
                    NULL,
                    MENU_ITEM,
                    MENU_STRING, "Few words about \"Module\"...",
                    MENU_CLIENT_DATA, strdup("Workspace"),
                    MENU_NOTIFY_PROC, help_notify,
                    NULL,
                    /* Just a separator: */
                    MENU_ITEM, MENU_STRING, "--------", MENU_INACTIVE, TRUE,
                    NULL,
                    MENU_ACTION_ITEM, "The PIPS documentation on Internet with Netscape...",
                    help_launch_pips_netscape,
                    MENU_ACTION_ITEM, "The PIPS documentation on Internet with XMosaic...",
                    help_launch_pips_xmosaic,
                    NULL);

   (void) xv_create(main_panel, PANEL_BUTTON,
                    PANEL_LABEL_STRING, "Help ",
                    PANEL_ITEM_MENU, menu,
                    0);
}

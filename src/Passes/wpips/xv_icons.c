#include <stdio.h>
#include <malloc.h>

#include <types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/text.h>
#include <xview/svrimage.h>
#include <xview/icon.h>
#include <X11/Xlib.h>

#include "genC.h"
#include "misc.h"

#include "wpips.h"

enum {ICON_TEXT_HEIGHT = 20};

/* To store the negative image for a blinking icon in the interrupt
   button: */
Server_image wpips_positive_server_image,
wpips_negative_server_image;

static Server_image pips_icon_server_image[LAST_ICON];

static unsigned short int pips_icons_data[LAST_ICON][256] = {
  {
#include "pips.icon"
  }, {
#include "ICFG.icon"
  }, {
#include "WP65_PE.icon"
  }, {
#include "WP65_bank.icon"
  }, {
#include "callgraph.icon"
  }, {
#include "parallel.icon"
  }, {
#include "sequential.icon"
  }, {
#include "user.icon"
  }
};


/*#include "logo_pips_small.xpm"*/
#include "logo_pips_small.xbm"


void create_icons()
{
    int i;

    for(i = 0; i < LAST_ICON; i++) {
      pips_icon_server_image[i] =
	(Server_image) xv_create(NULL, SERVER_IMAGE, 
				 XV_WIDTH, 64,
				 XV_HEIGHT, 64,
				 SERVER_IMAGE_BITS, &pips_icons_data[i][0],
				 NULL);
    }

    set_pips_icon(main_frame, PIPS_ICON, "main window");
}


Server_image
create_status_window_pips_image()
{
   /* To store the negative image for a blinking icon in the interrupt
      button: */
   char * inverted_pips_icon;
   int i;
   /*  Pixmap logo_pips_small_pixmap;
   
       logo_pips_small_pixmap = XCreatePixmap((Display *) xv_get(main_frame,
       XV_DISPLAY),
       (Window) xv_get(main_frame,
       XV_ID),
       */                                     
   wpips_positive_server_image =
      (Server_image) xv_create(NULL, SERVER_IMAGE,
                                /*SERVER_IMAGE_PIXMAP,
                                  logo_pips_small,
                                  SERVER_IMAGE_X_BITS,
                                  logo_pips_small,
                                  SERVER_IMAGE_DEPTH, 8,
                                  XV_WIDTH, 56,
                                  XV_HEIGHT, 51,*/
                               SERVER_IMAGE_X_BITS,
                               logo_pips_small_bits,
                               XV_WIDTH, logo_pips_small_width,
                               XV_HEIGHT, logo_pips_small_height,
                               NULL);

   inverted_pips_icon = (char *) malloc(sizeof(logo_pips_small_bits));
   for (i = 0; i < sizeof(logo_pips_small_bits); i++)
      inverted_pips_icon[i] = ~logo_pips_small_bits[i];
   wpips_negative_server_image =
      (Server_image) xv_create(NULL, SERVER_IMAGE,
                               SERVER_IMAGE_X_BITS,
                               inverted_pips_icon,
                               XV_WIDTH, logo_pips_small_width,
                               XV_HEIGHT, logo_pips_small_height,
                               NULL); 
   free(inverted_pips_icon);
   
   return wpips_positive_server_image;
}


void set_pips_icon(Frame frame, int icon_number, char *icon_text)
{
  /*if (icon_number != PIPS_ICON) return;*/
  if (icon_number >= 0) {
    Icon icon;
    Rect image_rect, label_rect;
    Server_image image;
    int height, width;

    image = pips_icon_server_image[icon_number];
    image = create_status_window_pips_image();
    
    width = xv_get(image, XV_WIDTH);
    height = xv_get(image, XV_HEIGHT);

    rect_construct(&image_rect, 0, 0, width, height);
    rect_construct(&label_rect, 0, height + 5, width, ICON_TEXT_HEIGHT);

    /* Hum... Is there a need to free the old icon ? */
    /* Bug if we don't reuse an already existing frame's icon... */
    icon = (Icon) xv_get(frame, FRAME_ICON);
    /* fprintf(stderr, "0x%x\n", icon); */
    if (icon != NULL) {
      /* If the owner of the icon is not NULL, the behaviour is crazy !
	 RK, 16/06/94. */
      xv_set(icon, 
	     ICON_IMAGE, image,
	     XV_WIDTH, width,
	     XV_HEIGHT, height + ICON_TEXT_HEIGHT,
	     ICON_IMAGE_RECT, &image_rect,
	     ICON_LABEL, icon_text,
	     ICON_LABEL_RECT, &label_rect,
	     NULL);
    }
    else {
      icon = (Icon) xv_create(NULL, ICON, 
			      ICON_IMAGE, image,
			      XV_WIDTH, width,
			      XV_HEIGHT, height + ICON_TEXT_HEIGHT,
			      ICON_IMAGE_RECT, &image_rect,
			      ICON_LABEL, icon_text,
			      ICON_LABEL_RECT, &label_rect,
			      NULL);

      xv_set(frame, FRAME_ICON, icon, 
	     NULL);
    }
    /* If we want to place the icon on the screen : 
       rect.r_width= (int)xv_get(icon, XV_WIDTH);
       rect.r_height= (int)xv_get(icon, XV_HEIGHT);
       rect.r_left= 0;
       rect.r_top= 0;
       
       xv_set(frame, FRAME_ICON, icon, 
       FRAME_CLOSED_RECT, &rect,
       NULL);
       */
  }
}

/* Find out a legal hyperplane direction. In a first phase, trust the user blindly!
 *
 * $Id$
 */

#include <stdio.h>
#include <strings.h>

#include "boolean.h"
#include "arithmetique.h"
#include "matrice.h"
#include "genC.h"
#include "misc.h"

bool
interactive_hyperplane_direction(Value * h, int n)
{
    int i;
    int n_read;
    string resp = string_undefined;
    string cn = string_undefined;
    bool return_status = FALSE;

    /* Query the user for h's coordinates */
    pips_assert("hyperplane_direction", n>=1);
    debug(8, "interactive_hyperplane_direction", "Reading h\n");
    resp = user_request("Hyperplane direction vector?\n"
			"(give all its integer coordinates on one line): ");
    if (resp[0] == '\0') {
	user_log("Hyperplane loop transformation has been cancelled.\n");
	return_status = FALSE;
    }
    else {    
	cn = strtok(resp, " \t");

	return_status = TRUE;
	for( i = 0; i<n; i++) {
	    if(cn==NULL) {
		user_log("Not enough coordinates. "
			 "Hyperplane loop transformation has been cancelled.\n");
		return_status = FALSE;
		break;
	    }
	    n_read = sscanf(cn," " VALUE_FMT, h+i);
	    if(n_read!=1) {
		user_log("Not enough coordinates. "
			 "Hyperplane loop transformation has been cancelled.\n");
		return_status = FALSE;
		break;
	    }
	    cn = strtok(NULL, " \t");
	}
    }

    if(cn!=NULL) {
	user_log("Too many coordinates. "
		 "Hyperplane loop transformation has been cancelled.\n");
	return_status = FALSE;
    }

    ifdebug(8) {
	if(return_status) {
	    pips_debug(8, "Hyperplane direction vector:\n");
	    for( i = 0; i<n; i++) {
		(void) fprintf(stderr," " VALUE_FMT, *(h+i));
	    }
	    (void) fprintf(stderr,"\n");
	    pips_debug(8, "End\n");
	}
	else {
	    pips_debug(8, "Ends with failure\n");
	}
    }

    return return_status;
}

/* package sc : $RCSfile: sc_debug.c,v $ version $Revision$
 * date: $Date: 1996/02/15 14:18:54 $, 
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

#include "sc-private.h"

#define SC_DEBUG_LEVEL "SC_DEBUG_LEVEL"

char* (*default_variable_to_string)(Variable) = variable_default_name;

int sc_debug_level = 0;

void set_sc_debug_level(l)
int l;
{ 
    sc_debug_level = l ;
}

void initialize_sc(char *(*var_to_string)(Variable))
{
    char * l = getenv(SC_DEBUG_LEVEL);
    if (l) set_sc_debug_level(atoi(l));
    default_variable_to_string = var_to_string;
}

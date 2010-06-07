/*

  $Id$

  Copyright 1989-2010 MINES ParisTech

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

 /* package vecteur - routines sur les variables
  *
  * Francois Irigoin
  *
  * Notes:
  *  - variable_equal() and variable_default_name() should be overriden
  *    by application specific routines
  *  - the same holds for variable_make()
  *  - variable are identified by opaque void * (Variable)
  *
  * Modifications:
  */

/*LINTLIBRARY*/
#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"

/* boolean variable_equal(Variable v1, Variable v2): returns TRUE if
 * variables v1 and v2 have the same VALUE
 *
 * Type Variable is assumed here to be char *
 *
 * Modifications:
 *  - no assumptions are made on type Variable; v1 and v2 may be of
 *    any type; all computations in vecteur.dir are based on pointer
 *    comparisons (FI, 28/12/89)
 */
boolean variable_equal(v1, v2)
Variable v1;
Variable v2;
{
    /*
     * if(v1==NULL&&v2==NULL)
     *     return(TRUE);
     * else if (v1==NULL||v2==NULL)
     *	   return(FALSE);
     *
     * return(!strcmp(v1,v2));
     */
    return v1==v2;
}

/* char * variable_default_name(Variable v): returns the name of variable v
 *
 * Type variable is assumed here to be char *
 */
char * variable_default_name(v)
Variable v;
{
    return((char *)v);
}

/* variable_dump_name() returns an unambiguous name for variable v, based
 * on the pointer used to really identify variables in the vecteur
 * package; the name starts with the letter X and contains the hexadecimal
 * representation of v
 *
 * Bugs:
 *  - the name is build in a local buffer; so a call to this function
 *    overwrite the previous returned value
 */
char * variable_dump_name(Variable v) {
  /* Room for X0x1234567812345678\0 for example on 64 bit address
     architecture since Variable is a pointer to something: */
  static char buffer[sizeof(void *)*2+4]; 

  buffer[0] = 'X';
  (void) sprintf(&buffer[1],"%p", v);
  return(buffer);
}

/* Variable variable_make(char * name): defines a new variable of a given
 * name
 */
Variable variable_make(name)
char * name;
{
    return((Variable) strdup(name));
}

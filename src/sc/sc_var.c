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

 /* package sc */

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

#define MALLOC(s,t,f) malloc(s)
#define FREE(p,t,f) free(p)

/* char * noms_var(int i):
 * cette fonction convertit un numero de variable en chaine de caracteres
 *
 *  resultat retourne par la fonction :
 *
 *  char *	: chaine de caracteres associee au numero de variable
 *
 *  Les parametres de la fonction :
 *
 *  int  i    : numero de variable du systeme   
 *
 * FI: a regarder et a commenter et a transferer dans base.dir; pourrait-etre
 * un variable_default_name()? Ou integer_default_name()?
 *
 * FI: suppression avec le passage a char * du type Variable
 */
/*
 * char *noms_var(i)
 * int i;
 * {
 *     char *ch1,*ch2,*ch3;
 *     char c;
 * 
 *     ch1 = (char *) MALLOC (sizeof(char),CHAINE,"noms_var");
 *     ch1[0] = '\0';
 *     while (i>=1) {
 * 	ch3=(char *) MALLOC(2*sizeof(char),CHAINE,"noms_var");
 * 	c = ((i-1)%27)+ 'a';
 * 	ch3[0]=c;
 * 	ch3[1]='\0';
 * 	ch2 = (char *) MALLOC((unsigned int) ((strlen(ch1)+2) * sizeof(char)),
 * 			      CHAINE,"noms_var");
 * 	(void) strcpy(ch2,ch3);
 * 	(void) strcat(ch2,ch1);
 * 	ch1 = (char *) MALLOC((unsigned int) ((strlen(ch2) +1)*sizeof(char)),
 * 			      CHAINE,"noms_var");
 * 	(void) strcpy(ch1,ch2);
 * 	i = (i - (i%27))/27;
 * 
 *     }
 *     return (ch1);
 * }
 */

/* Variable creat_new_var(Psysteme ps):
 * creation d'une nouvelle variable v pour le systeme ps; les champs "base"
 * et "dimension" sont mis a jour et la nouvelle variable est renvoyee.
 *
 * Le nom de la nouvelle variable v est de la forme "Xnnn" ou nnn represente
 * son rang dans la base "ps->base"
 *
 * Auteur: Corinne Ancourt
 *
 * Modifications:
 *  - remplacement du champ num_var par le champ base
 */
Variable creat_new_var(ps)
Psysteme ps;
{
    Variable v;
    int d;
    static char name[4];

    assert(ps != NULL);

    d = ps->dimension++;
    assert(d<=99);
    name[0] = 'X';
    (void) sprintf(&name[1],"%d",d);
    v = variable_make(name);
    return v;
}


boolean var_in_lcontrainte_p(pc,var)
Pcontrainte pc;
Variable var;
{
    boolean find = FALSE;
    Pcontrainte pc1;

    for (pc1 = pc; !CONTRAINTE_UNDEFINED_P(pc1) && !find; pc1=pc1->succ)
	find = find || vect_coeff(var,pc1->vecteur);

    return (find);
}

/* boolean var_in_sc_p(Psysteme sc, Variable var)
 * Cette fonction teste si la variable est contrainte par les egalites
 * ou les inegalites
*/
boolean var_in_sc_p(sc,var)
Psysteme sc;
Variable var;
{

    boolean find = FALSE;

    find = find ||  var_in_lcontrainte_p(sc->egalites,var) 
	||  var_in_lcontrainte_p(sc->inegalites,var); 
 
    return(find);
}

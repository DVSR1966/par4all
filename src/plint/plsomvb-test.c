

#include <stdio.h>
#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "sommet.h"
#include "matrix.h"

#include "plint.h"

#define MALLOC(s,t,f) malloc(s)
char * malloc();








/*
 * Cette fonction teste si l'un des termes constants des contraintes est negatif
 *
 *  resultat retourne par la fonction :
 *
 *  boolean          : == TRUE si l'un des termes constants est negatif
 *                  == FALSE sinon
 *
 *  Les parametres de la fonction :
 *
 *  Psommet  som  : systeme lineaire
 *
 */

boolean const_negative(som)
Psommet som;
{
    Psommet ps;
    boolean result = FALSE;

    for (ps = som;
	 ps!= NULL && (- vect_coeff(TCST,ps->vecteur) >= 0);
	 ps= ps->succ);
    result = (ps == NULL) ? FALSE : TRUE;
    return (result);
}



/*
 * Si la partie constante d'une contrainte est negative, il faut que l'un des
 * coefficients des variables de la contrainte le soit aussi pour que le systeme 
 * reste borne.
 *
 *  resultat retourne par la fonction :
 *
 *  boolean           : FALSE si la contrainte montre que le systeme est non borne
 *		     TRUE  sinon
 *
 *  Les parametres de la fonction :
 *
 *  Psommet  eq    : contrainte du systeme  
 */

boolean test_borne(eq)
Psommet eq;
{
    Pvecteur pv = NULL;
    boolean result= FALSE;

    if (eq) {
	pv = eq->vecteur;
	if (-vect_coeff(TCST,pv) < 0)
	{
	    for (pv= eq->vecteur;pv!= NULL 
		 && ((pv->var ==NULL) || (pv->val >0))
		 ;pv= pv->succ);
	    result = (pv==NULL) ? FALSE : TRUE;
	}
	else result = TRUE;
    }
    return (result);

}





/*
 * Cette fonction teste s'il existe une variable hors base de cout nul
 * dans le systeme
 *
 *  resultat retourne par la fonction :
 *
 *  boolean           : TRUE s'il existe une variable hors base de cout 
 *    		      nul
 *
 *  Les parametres de la fonction :
 *
 *  Psommet fonct  : fonction economique du  programme lineaire
 *  Pvecteur lvbase: liste des variables de base du systeme
 */

boolean cout_nul(fonct,lvbase,nbvars,b)
Psommet fonct;
Pvecteur lvbase;
int nbvars;
Pbase b;
{
    Pvecteur liste1 = NULL;		/* liste des variables h.base de cout nul */
    Pvecteur pv=NULL;
    Pvecteur pv2=VECTEUR_NUL;
    register int i;
    boolean result= FALSE;

#ifdef TRACE
    printf(" ** Gomory - existe-t-il une var. h.base de cout  nul  \n");
#endif

    liste1 = vect_new(vecteur_var(b),1);
    for (i = 1 ,pv2 = b->succ;
	 i< nbvars && !VECTEUR_NUL_P(pv2); 
	 i++, pv2=pv2->succ)
	vect_add_elem (&(liste1),vecteur_var(pv2),1);
    if (fonct != NULL)
	for (pv = fonct->vecteur;pv != NULL;pv=pv->succ)
	    if (pv->val != 0)
		vect_chg_coeff(&liste1,pv->var,0);
		
    for (pv = lvbase;pv != NULL;pv=pv->succ)
	if (pv->val != 0)
	    vect_chg_coeff(&liste1,pv->var,0);
    result = (liste1 != NULL) ? TRUE : FALSE;

    vect_rm(liste1);
    return (result);

}


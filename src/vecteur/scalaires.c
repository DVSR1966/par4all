/* package vecteur - operations vecteur x scalaire */

/*LINTLIBRARY*/

#include <stdio.h>
#include <stdlib.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"

#define FREE(p,t,f) free(p)

/* Pvecteur vect_div(Pvecteur v, Value x): division du vecteur v
 * par le scalaire x, si x est different de 0.
 * 
 * ->   ->
 * v := v / x;
 *                                          ->   ->
 * Si x vaut 0, la procedure aborte meme si v == 0
 *                                                           ->
 * Attention, si x ne divise pas le pgcd des coefficients de v, la valeur
 * retournee n'est pas colineaire a la valeur initiale
 * 
 * 
 */
Pvecteur vect_div(v,x)
Pvecteur v;
Value x;
{
    if(x==0) {
	vect_error("vect_div","vector zero divide\n");
    }
    else if (x==1)
	;
    else if(x==-1)
	vect_chg_sgn(v);
    else {
	Pvecteur coord;

	for (coord = v ;coord!=NULL;coord=coord->succ) {
	    val_of(coord) = DIVIDE(val_of(coord),x);
	}
    }
return (vect_clean(v));
}

/* Pvecteur vect_clean(Pvecteur v): elimination de tous les couples dont le
 * coefficient vaut 0 dans le vecteur v et renvoie de v
 *
 * Ne devrait JAMAIS etre utilise en dehors de la bibliotheque vecteur. Ne sert
 * qu'a corriger le resultat de vect_div quand la division entiere fait apparaitre
 * un 0. Dans ces cas, vect_div n'est pas lineaire.
 */
Pvecteur vect_clean(v)
Pvecteur v;
{
    Pvecteur v1,v2;
    Pvecteur pred = v;
    Pvecteur result=v;

    for (v1 = v; ((v1!= NULL) && (v1->val != 0)); pred = v1,v1=v1->succ);

    for (v2 = v1;v2 != NULL; v2 = v2->succ)
    {
	if (v2->val == 0)
	{
	    if (v2 == v)
	    {
		result = v2->succ;
		pred = v;

	    }
	    else {
		pred->succ = v2->succ;
		v2->succ = NULL;
		FREE((char*)v2,VECTEUR,"vect_clean");
		v2 = pred;
	    }
	}
	else
	    pred = v2;

    }
    return (result);
}

/* Pvecteur vect_multiply(Pvecteur v, Value x): multiplication du vecteur v
 * par le scalaire x, si x est different de 0.
 * 
 * ->     ->
 * v := x v;
 *
 * Ancien nom: vect_mult()
 * Ancien profil: void vect_mult(); ne permettait pas de renvoyer un vecteur
 * nul en cas de multiplication par zero d'un vecteur non nul
 */
Pvecteur vect_multiply(v,x)
Pvecteur v;
Value x;
{
    Pvecteur coord;

    switch(x) {
    case 0:
	vect_rm(v);
	v=NULL;
	break;
    case 1:
	break;
    case -1:
	vect_chg_sgn(v);
	break;
    default:
	for(coord = v; coord!=NULL; coord=coord->succ) {
	    val_of(coord) = x * val_of(coord);
	}
    }
    return v;
}

/* void vect_chg_sgn(Pvecteur v): multiplie v par -1
 * 
 * ->       ->
 * v  :=  - v
 *
 */
void vect_chg_sgn(v)
Pvecteur v;
{
    for( ;v != NULL; v = v->succ)
	val_of(v) = -val_of(v);
}

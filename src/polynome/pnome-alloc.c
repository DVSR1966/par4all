/***************************************************************** pnome-alloc.c
 *
 * CREATING, DUPLICATING AND FREEING A POLYNOME
 *
 */

/*LINTLIBRARY*/

#include <stdio.h>
#include <sys/stdtypes.h>  /* for debug with dbmalloc */
#include <malloc.h>

#include "boolean.h"
#include "vecteur.h"
#include "polynome.h"


/* Pmonome make_monome(float coeff, Variable var, Value exp)
 *  PRIVATE
 *  allocates space for, and creates, the monome "coeff*var^exp" 
 */
Pmonome make_monome(coeff, var, exp)
float coeff;
Variable var;
Value exp;
{
    if (coeff == 0)
	return (MONOME_NUL);
    else {
	Pmonome pm = (Pmonome) malloc(sizeof(Smonome));
	monome_coeff(pm) = coeff;
	monome_term(pm) = vect_new((exp == 0 ? TCST : var),
				   (exp == 0 ?    1 : exp));
	return(pm);
    }
}

/* Ppolynome make_polynome(float coeff, Variable var, Value exp)
 *  PRIVATE
 *  allocates space for, and creates, the polynome "coeff*var^exp"
 */
Ppolynome make_polynome(coeff, var, exp)
float coeff;
Variable var;
Value exp;
{
    return (monome_to_new_polynome(make_monome(coeff, var, exp)));
}


/* Ppolynome monome_to_new_polynome(Pmonome pm)
 *  PRIVATE
 *  allocates space for, and creates the polynomial containing
 *  the monomial pointed by pm, which is NOT duplicated
 *  but attached to the polynomial.
 */
Ppolynome monome_to_new_polynome(pm)
Pmonome pm;
{
    if (MONOME_NUL_P(pm)) 
	return (POLYNOME_NUL);
    else if (MONOME_UNDEFINED_P(pm)) 
	return (POLYNOME_UNDEFINED);
    else {
	Ppolynome pp = (Ppolynome) malloc(sizeof(Spolynome));
	polynome_monome(pp) = pm;
	polynome_succ(pp) = NIL;
	return (pp);
    }
}

/* Pmonome monome_dup(Pmonome pm)
 *  PRIVATE
 *  creates and returns a copy of pm
 */
Pmonome monome_dup(pm)
Pmonome pm;
{
    if (MONOME_NUL_P(pm)) 
	return (MONOME_NUL);
    else if (MONOME_UNDEFINED_P(pm)) 
	return (MONOME_UNDEFINED);
    else {
	Pmonome pmd = (Pmonome) malloc(sizeof(Smonome));
	monome_coeff(pmd) = monome_coeff(pm);
	monome_term(pmd) = vect_dup(monome_term(pm));
	return(pmd);
    }
}


/* void monome_rm(Pmonome* ppm)
 *  PRIVATE
 *  frees space occupied by monomial *ppm
 *  returns *ppm pointing to MONOME_NUL
 *  !usage: monome_rm(&pm);
 */
void monome_rm(ppm)
Pmonome *ppm;
{
    if ((!MONOME_NUL_P(*ppm)) && (!MONOME_UNDEFINED_P(*ppm))) {
	vect_rm((Pvecteur) monome_term(*ppm));
	free((char *) *ppm);
    }
    *ppm = MONOME_NUL;
}


/* void polynome_rm(Ppolynome* ppp)
 *  frees space occupied by polynomial *ppp
 *  returns *ppp pointing to POLYNOME_NUL
 *  !usage: polynome_rm(&pp);
 */
void polynome_rm(ppp)
Ppolynome *ppp;
{
    Ppolynome pp1 = *ppp, pp2;

    if (!POLYNOME_UNDEFINED_P(*ppp)) {
	while (pp1 != NIL) {
	    pp2 = polynome_succ(pp1);
	    monome_rm(&polynome_monome(pp1));
	    free((char *) pp1);               /* correct? */
	    pp1 = pp2;
	}
	*ppp = POLYNOME_NUL;
    }
}

/* Ppolynome polynome_free(Ppolynome pp)
 *  frees space occupied by polynomial pp
 *  returns pp == POLYNOME_NUL
 *  !usage: polynome_rm(pp);
 */
Ppolynome polynome_free(pp)
Ppolynome pp;
{
    Ppolynome pp1 = pp, pp2;

    if (!POLYNOME_UNDEFINED_P(pp)) {
	while (pp1 != NIL) {
	    pp2 = polynome_succ(pp1);
	    monome_rm(&polynome_monome(pp1));
	    free((char *) pp1);               /* correct? */
	    pp1 = pp2;
	}
    }
    return POLYNOME_NUL;
}


/* Ppolynome polynome_dup(Ppolynome pp)
 *  creates and returns a copy of pp
 */
Ppolynome polynome_dup(pp)
Ppolynome pp;
{
    Ppolynome ppdup, curpp;

    if (POLYNOME_NUL_P(pp)) 
	return (POLYNOME_NUL);
    else if (POLYNOME_UNDEFINED_P(pp)) 
	return (POLYNOME_UNDEFINED);
    else {
	ppdup = monome_to_new_polynome(monome_dup(polynome_monome(pp)));
	curpp = ppdup;
	while ((pp = polynome_succ(pp)) != NIL) {
	    polynome_succ(curpp) =
		monome_to_new_polynome(monome_dup(polynome_monome(pp)));
	    curpp = polynome_succ(curpp);
	}
	return (ppdup);
    }
}

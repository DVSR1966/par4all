/*************************************************************** pnome-private.c
 *
 * PRIVATE ROUTINES  (MONOMIAL MANIPULATIONS)
 *
 */

#include <stdio.h>
#include <strings.h>

#include "boolean.h"
#include "vecteur.h"
#include "polynome.h"


/* Pmonome monome_del_var(Pmonome pm, Variable var)
 *  PRIVATE
 *  returns a copy of monomial pm, where variable var is deleted
 */
Pmonome monome_del_var(pm, var)
Pmonome pm;
Variable var;
{
    if (MONOME_UNDEFINED_P(pm)) 
	return (MONOME_UNDEFINED);
    else if (MONOME_NUL_P(pm)) 
	return (MONOME_NUL);
    else {
	Pmonome newpm = (Pmonome) malloc(sizeof(Smonome));

	monome_coeff(newpm) = monome_coeff(pm);
	monome_term(newpm)  = vect_del_var(monome_term(pm), var);
	if (VECTEUR_NUL_P(monome_term(newpm))) {
	    /* is it the only variable */
	    monome_term(newpm) = vect_new(TCST, 1); 
	    /* now it is a constant term   */
	}
    
	return(newpm);
    }
}


/* boolean monome_colin(Pmonome pm1, Pmonome pm2)
 *  PRIVATE
 *  returns TRUE if the two monomials are "colinear":
 *  same variables, same exponents.
 *  We consider that MONOME_UNDEFINED is only colinear to MONOME_UNDEFINED. [???]
 */
boolean monome_colin(pm1, pm2)
Pmonome pm1, pm2;
{
    if (MONOME_UNDEFINED_P(pm1) || MONOME_UNDEFINED_P(pm2)
	||    MONOME_NUL_P(pm1) || MONOME_NUL_P(pm2))
	return(pm1 == pm2);    
    else 
	return(vect_equal(monome_term(pm1), monome_term(pm2)));
}


/* boolean monome_equal(Pmonome pm1, Pmonome pm2)
 *  PRIVATE
 *  returns TRUE if the two monomials are equal
 *  same coeff., same variables, same exponents.
 */
boolean monome_equal(pm1, pm2)
Pmonome pm1, pm2;
{
    if (MONOME_UNDEFINED_P(pm1) || MONOME_UNDEFINED_P(pm2)
	||    MONOME_NUL_P(pm1) || MONOME_NUL_P(pm2))
	return(pm1 == pm2);    
    else 
	return(vect_equal(monome_term(pm1), monome_term(pm2))
	       && ((monome_coeff(pm1) == monome_coeff(pm2))));
}

/* float Bernouilli(int i)
 *  PRIVATE
 *  returns Bi = i-th Bernouilli number
 */
float Bernouilli(i)
int i;
{
    switch (i) {
    case  1: return((float) 1/6);
    case  2: return((float) 1/30);
    case  3: return((float) 1/42);
    case  4: return((float) 1/30);
    case  5: return((float) 5/66);
    case  6: return((float) 691/2730);
    case  7: return((float) 7/6); 
    case  8: return((float) 3617/510);
    case  9: return((float) 43867/798);
    case 10: return((float) 174611/330);
    case 11: return((float) 854513/138);
    case 12: return((float) 236364091/2730);
    default: polynome_error("Bernouilli(i)", "i=%d illegal\n", i);
	/* later, we could compute bigger Bernouilli(i) with the recurrence */
    }
}


/* int factorielle (int n)
 *  PRIVATE
 *  returns n!
 */
int factorielle(n)
int n;
{
    if (n<0) 
	polynome_error("factorielle", "n=%d", n);
    else if (n<2) 
	return(1);
    else 
	return(factorielle(n-1) * n);
}


/* double intpower(double d, int n)
 *  returns d^n for all integers n
 */
double intpower(d, n)
double d;
int n;
{
    if (n>0) 
	return (intpower(d, n-1) * d);
    else if (n==0) 
	return((double) 1);
    else 
	return (intpower(d, n+1) / d);
}


/* boolean is_inferior_monome(Pmonome pm1, pm2, boolean (*is_inferior_var)())
 *  returns the boolean (pm1<pm2)
 *  we follow the "lexicographic" order: decreasing powers of the main variable,
 *  (according to the variable order relation passed in is_inferior_var)
 *  the decreasing powers of the next main variable, ...
 *  When pm1=pm2 we return FALSE.
 *  When pm1 or pm2 is MONOME_NUL or MONOME_UNDEFINED we return FALSE.
 */
boolean is_inferior_monome(pm1, pm2, is_inferior_var)
Pmonome pm1, pm2;
boolean (*is_inferior_var)();
{
    if (MONOME_UNDEFINED_P(pm1) || MONOME_UNDEFINED_P(pm2)
	||    MONOME_NUL_P(pm1) || MONOME_NUL_P(pm2))
	return (FALSE);
    else {
	Pvecteur pv1 = vect_sort(monome_term(pm1), vect_compare);
	Pvecteur pv2 = vect_sort(monome_term(pm2), vect_compare);
	/* Pvecteur pv1 = vect_tri(monome_term(pm1), is_inferior_var);
	   Pvecteur pv2 = vect_tri(monome_term(pm2), is_inferior_var);*/
	Pbase pb = base_union((Pbase) pv1, (Pbase) pv2);
	/* Pbase pbsorted = (Pbase) vect_tri(pb, is_inferior_var); */
	Pbase pbsorted = (Pbase) vect_sort(pb, vect_compare);
	boolean result = FALSE;

	/* The following test is added by L.Zhou .    
	   We want the constant term at the end . Jul.15, 91 */
	if ( term_cst(pv1) )
	    return(TRUE);
	else if ( term_cst(pv2) )
	    return(FALSE);

	/* The following test is added by L.Zhou .    Mar.26, 91 */
	if ( vect_coeff_sum(pv1) < vect_coeff_sum(pv2) )
	    result = TRUE;
	else if ( vect_coeff_sum(pv1) > vect_coeff_sum(pv2) )
	    ;
	else {
	    while (!BASE_NULLE_P(pbsorted)) {
		Variable var = vect_first_var((Pvecteur) pbsorted);
		Value power1 = vect_coeff(var, pv1);
		Value power2 = vect_coeff(var, pv2);

		if ( power1 < power2 ) {
		    result = TRUE;
		    break;
		}
		else if ( power2 < power1 ) 
		    break;
		vect_erase_var((Pvecteur) &pbsorted, var);
	    }
	}

	vect_rm((Pvecteur) pbsorted);
	vect_rm((Pvecteur) pb);
	vect_rm(pv2);
	vect_rm(pv1);

	return (result);
    }
}

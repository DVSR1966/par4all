 /* PACKAGE CONTRAINTE - OPERATIONS UNAIRES
  */

#include <stdio.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"

/* norm_eq: normalisation d'une contrainte par le pgcd de TOUS les
 * coefficients, i.e. y compris le terme constant
 */
void norm_eq(nr)
Pcontrainte nr;
{
    vect_normalize(nr->vecteur);
}

/* void contrainte_chg_sgn(Pcontrainte eq): changement de signe d'une 
 * contrainte, i.e. multiplication par -1. Les equations ne sont pas
 * modifiees mais les inequations sont transformees.
 *
 * Ancien nom: ch_sgn
 */
void contrainte_chg_sgn(c)
Pcontrainte c;
{
    vect_chg_sgn(c->vecteur);
}


/* void contrainte_reverse(Pcontrainte eq): changement de signe d'une 
 * contrainte, i.e. multiplication par -1, et ajout de la constante 1.
 *
 */
void contrainte_reverse(c)
Pcontrainte c;
{
    contrainte_chg_sgn(c);
    vect_add_elem(&(c->vecteur),TCST,1);
}

/* void_eq_set_vect_nul(Pcontrainte c): transformation d'une contrainte
 * en une contrainte triviale 0 == 0
 *
 * cette fonction est utile lorsque l'on veut eliminer plusieurs         
 * contraintes du systeme sans avoir a le restructurer apres chaque        
 * elimination.                                                          
 *
 * Pour eliminer toutes ces "fausses" contraintes on utilise a la fin la   
 * fonction "syst_elim_eq" (ou "sc_rm_empty_constraints"...)                                              
 */
void eq_set_vect_nul(c)
Pcontrainte c;
{
    if(!CONTRAINTE_UNDEFINED_P(c)) {
	vect_rm(contrainte_vecteur(c));
	contrainte_vecteur(c) = VECTEUR_NUL;
    }
}

/* Pcontrainte contrainte_translate(Pcontrainte c, Pbase b, 
 *                                  char * (*variable_name)()):
 * normalisation des vecteurs de base utilises dans c par rapport
 * a la base b utilisant les "noms" des vecteurs de base; en sortie
 * tous les vecteurs de base utilises dans c appartiennent a b;
 */
Pcontrainte contrainte_translate(c, b, variable_name)
Pcontrainte c;
Pbase b;
char * (*variable_name)();
{
    if(!CONTRAINTE_UNDEFINED_P(c))
	contrainte_vecteur(c) = vect_translate(contrainte_vecteur(c), b,
					       variable_name);

    return c;
}

/* Pcontrainte contrainte_variable_rename(Pcontrainte c, Variable v_old,
 *                                        Variable v_new):
 * rename the potential coordinate v_old in c as v_new
 */
Pcontrainte contrainte_variable_rename(c, v_old, v_new)
Pcontrainte c;
Variable v_old;
Variable v_new;
{
    if(!CONTRAINTE_UNDEFINED_P(c))
	contrainte_vecteur(c) = vect_variable_rename(contrainte_vecteur(c), 
						     v_old, v_new);

    return c;
}

/* void Pcontrainte_separate_on_vars(initial, vars, pwith, pwithout)
 * Pcontrainte initial;
 * Pbase vars;
 * Pcontrainte *pwith, *pwithout;
 *
 *     IN: initial, vars
 *    OUT: pwith, pwithout
 *
 * builds two Pcontraintes from the one given, using the
 * constraint_without_vars criterium.
 * 
 * (c) FC 16/05/94
 */
void Pcontrainte_separate_on_vars(initial, vars, pwith, pwithout)
Pcontrainte initial;
Pbase vars;
Pcontrainte *pwith, *pwithout;
{
    Pcontrainte
	c = (Pcontrainte) NULL,
	new = CONTRAINTE_UNDEFINED;

    for(c=initial, 
	*pwith=(Pcontrainte)NULL,
	*pwithout=(Pcontrainte)NULL; 
	c!=(Pcontrainte) NULL;
	c=c->succ)
	if (constraint_without_vars(c, vars))
	    new = contrainte_make(vect_dup(c->vecteur)),
	    new->succ = *pwithout, 
	    *pwithout = new;
	else
	    new = contrainte_make(vect_dup(c->vecteur)),
	    new->succ = *pwith, 
	    *pwith = new;
}

/* void constraints_for_bounds(var, pinit, plower, pupper)
 * Variable var;
 * Pcontrainte *pinit, *plower, *pupper;
 * IN: var, *pinit;
 * OUT: *pinit, *plower, *pupper;
 *
 * separate the constraints involving var for upper and lower bounds
 * The constraints are removed from the original system. 
 * everything is touched. Should be fast because there is no allocation.
 *
 * FC 28/11/94
 */
void constraints_for_bounds(var, pinit, plower, pupper)
Variable var;
Pcontrainte *pinit, *plower, *pupper;
{
    Value 
	v;
    Pcontrainte
        c, next,
	remain = NULL,
        lower = NULL,
	upper = NULL;

    for(c = *pinit, next=(c==NULL ? NULL : c->succ); 
	c!=NULL; 
        c=next, next=(c==NULL ? NULL : c->succ))
    {
	v = vect_coeff(var, c->vecteur);

	if (v>0)
	    c->succ = upper, upper = c;
	else if (v<0)
	    c->succ = lower, lower = c;
	else /* v==0 */
	    c->succ = remain, remain = c;
    }

    *pinit = remain,
    *plower = lower,
    *pupper = upper;
}

/* Pcontrainte contrainte_dup_extract(c, var)
 * Pcontrainte c;
 * Variable var;
 *
 * returns a copy of the constraints of c which contain var.
 *
 * FC 27/09/94
 */
Pcontrainte contrainte_dup_extract(c, var)
Pcontrainte c;
Variable var;
{
    Pcontrainte
	result = NULL,
	pc, ctmp;

    for (pc=c; pc!=NULL; pc=pc->succ)
	if ((var==NULL) || vect_coeff(var, pc->vecteur)!=0)
	    ctmp = contrainte_dup(pc),
	    ctmp->succ = result,
	    result = ctmp;
    
    return(result);
}

/* Pcontrainte contrainte_extract(pc, base, var)
 * Pcontrainte *pc;
 * Pbase base;
 * Variable var;
 *
 * returns the constraints of *pc of which the higher rank variable from base 
 * is var. These constraints are removed from *pc.
 *
 * FC 27/09/94
 */
Pcontrainte contrainte_extract(pc, base, var)
Pcontrainte *pc;
Pbase base;
Variable var;
{
    int
	rank = rank_of_variable(base, var);
    Pcontrainte
	ctmp = NULL,
	result = NULL,
	cprev = NULL,
	c = *pc;

    while (c!=NULL)
    {
	if (search_higher_rank(c->vecteur, base)==rank)
	{
	    /*
	     * c must be extracted
	     */
	    ctmp = c->succ,
	    c->succ = result,
	    result = c,
	    c = ctmp;
	    
	    if (cprev==NULL)
		*pc = ctmp;
	    else
		cprev->succ=ctmp;
	}
	else
	    c=c->succ, 
	    cprev=(cprev==NULL) ? *pc : cprev->succ;
    }

    return(result);
}

/* int level_contrainte(Pcontrainte pc, Pbase base_index)
 * compute the level (rank) of the constraint pc in the nested loops.
 * base_index is the index basis in the good order
 * The result corresponds to the rank of the greatest index in the constraint, 
 * and the sign of the result  corresponds to the sign of the coefficient of 
 * this  index  
 * 
 * For instance:
 * base_index :I->J->K ,
 *                 I - J <=0 ==> level -2
 *             I + J + K <=0 ==> level +3
 */
int level_contrainte(pc, base_index)
Pcontrainte pc;
Pbase base_index;
{
    Pvecteur pv;
    Pbase pb;
    int level = 0;
    int i;
    int sign=1;
    boolean trouve = FALSE;

    for (pv = pc->vecteur;
	 pv!=NULL;
	 pv = pv->succ)
    {
	for (i=1, trouve=FALSE, pb=base_index;
	     pb!=NULL && !trouve;
	     i++, pb=pb->succ)
	    if (pv->var == pb->var)
	    {
		trouve = TRUE;
		if (i>level)
		    level = i, sign = (pv->val<0) ? -1 : 1 ;
	    }
    }
    return(sign*level);
}

/* it sorts the vectors as expected. FC 24/11/94
 */
void contrainte_vect_sort(c, compare)
Pcontrainte c;
int (*compare)();
{
    for (; c!=NULL; c=c->succ)
	vect_sort_in_place(&c->vecteur, compare);
}


/* Pcontrainte contrainte_var_min_coeff(Pcontrainte contraintes, Variable v,
 *                           int *coeff)
 * input    : a list of constraints (euqalities or inequalities), 
 *            a variable, and the location of an integer.
 * output   : the constraint in "contraintes" where the coefficient of
 *            "v" is the smallest (but non-zero).
 * modifies : nothing.
 * comment  : the returned constraint is not removed from the list if 
 *            rm_if_not_first_p is FALSE.
 *            if rm_if_not_first_p is TRUE, the returned contraint is
 *            remove only if it is not the first constraint.
 */
Pcontrainte contrainte_var_min_coeff(contraintes, v, coeff, rm_if_not_first_p) 
Pcontrainte contraintes;
Variable v;
int *coeff;
boolean rm_if_not_first_p;
{
    int sc = 0,
        c,
        cv = 0;
    Pcontrainte result, eq, pred, eq1;

    if (contraintes == NULL) 
	return(NULL);

    result = pred = eq1 = NULL;
    
    for (eq = contraintes; eq != NULL; eq = eq->succ) {
	c = vect_coeff(v, eq->vecteur);
	if ((ABS(c) < cv && ABS(c) > 0) || (cv == 0 && c != 0)) {
	    cv = ABS(c);
	    sc = c;
	    result = eq;
	    pred = eq1;
	}
    }

    if (sc < 0) 
	contrainte_chg_sgn(result);
    
    if (rm_if_not_first_p && pred != NULL) {
	pred->succ = result->succ;
	result->succ = NULL;
    }
  
    *coeff = cv;
    return(result);
}



/*    that is all
 */

/* package sc 
 * $RCSfile: sc_elim_simple_redund.c,v $ (version $Revision$)
 * $Date: 1996/07/18 19:15:54 $, 
 */

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "arithmetique.h"
#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

jmp_buf overflow_error;

/* boolean sc_elim_simple_redund_with_eq(Psysteme ps, Pcontrainte eg):
 * elimination en place des contraintes d'un systeme ps, qui sont redondantes
 * avec une egalite eg
 *                                                                       
 *  * si une autre egalite a le meme membre gauche (equation sans le terme 
 *     constant) :
 *                                                                       
 *    - si les termes constants sont egaux ==> elimination d'une egalite 
 *    - sinon ==> systeme infaisable                                     
 *                                                                       
 *  * si une inegalite  a le meme membre gauche que l'egalite :               
 *                                                                       
 *     - si l'egalite est compatible avec inegalite                      
 *               ==> elimination de l'inegalite                          
 *     - sinon  ==> systeme infaisable                                   
 *                                                                       
 *
 *  resultat retourne par la fonction :
 *
 *  boolean: FALSE si l'equation a permis de montrer que le systeme
 *                 etait non faisable
 *           TRUE sinon
 *
 *  Les parametres de la fonction :
 *
 * !Psysteme ps	  : systeme
 *  Pcontrainte eg : equation du systeme 
 */
boolean sc_elim_simple_redund_with_eq(ps,eg)
Psysteme ps;
Pcontrainte eg;
{
    Pcontrainte eq = NULL;

    if (SC_UNDEFINED_P(ps) || sc_rn_p(ps))
	return(TRUE);

    /* cas des egalites    */
    for (eq = ps->egalites; eq != NULL; eq = eq->succ) {
	if (eq != eg && eq->vecteur != NULL && eq_smg(eq,eg)) {
	    if (eq_diff_const(eq,eg) == 0)
		/* les deux egalites sont redondantes 
		   ==> elimination de eq */
		eq_set_vect_nul(eq);
	    else 
		return(FALSE);
	}
    }

    /* cas des inegalites     */
    for (eq = ps->inegalites; eq != NULL; eq = eq->succ) {
	if (eq_smg(eq,eg) && eq->vecteur!= NULL) {
	    if (value_negz_p(eq_diff_const(eq, eg)))
		eq_set_vect_nul(eq);
	    else
		return(FALSE);
	}
    }
    return(TRUE);
}

/* boolean sc_elim_simple_redund_with_ineq(Psysteme ps, Pcontrainte ineg):
 * elimination des contraintes redondantes de ps avec une inegalite ineg
 * (FI: qui doit appartenir a ps; verifier qu'on ne fait pas de comparaisons
 * inutiles; apparemment pas parce qu'on modifie froidement ineg)
 *                                                                       
 *  * si deux inegalites ont le meme membre gauche (contrainte sans le     
 *    terme constant) :                                                  
 *                                                                       
 *      ==> elimination de l'inegalite ayant le terme constant le plus   
 *          grand                                                        
 *                                                                       
 *  * si une egalite  a le meme membre gauche que l'inegalite :               
 *                                                                       
 *     - si l'egalite est compatible avec inegalite                      
 *              ==> elimination de l'inegalite                          
 *     - sinon  ==> systeme infaisable                                   
 *                                                                       
 *  resultat retourne par la fonction :
 *
 *  boolean: FALSE si l'inequation a permis de montrer que le systeme
 *                 etait non faisable
 *           TRUE sinon
 *
 *  Les parametres de la fonction :
 *
 *  Psysteme ps      : systeme
 *  Pcontrainte ineg : inequation du systeme
 */
boolean sc_elim_simple_redund_with_ineq(ps,ineg)
Psysteme ps;
Pcontrainte ineg;
{
    Pcontrainte eq=NULL;
    boolean result = TRUE;
    Value b = VALUE_ZERO;
 
    if (SC_UNDEFINED_P(ps) || sc_rn_p(ps))
	return(TRUE);

    /* cas des egalites     */
    for (eq = ps->egalites; eq != NULL && ineg->vecteur != NULL; 
	 eq = eq->succ)
	if (eq_smg(eq,ineg)) {
	    b = eq_diff_const(ineg,eq);
	    if (value_negz_p(b))
		/* inegalite redondante avec l'egalite  
		   ==> elimination de inegalite */
		eq_set_vect_nul(ineg);
	    else result = FALSE;
	}

    /* cas des inegalites          */
    for (eq = ps->inegalites;eq !=NULL && ineg->vecteur != NULL; 
	 eq = eq->succ) {
	if (eq != ineg)
	    if (eq_smg(eq,ineg)) {
		b = eq_diff_const(eq,ineg);
		if(value_negz_p(b))
		    eq_set_vect_nul(eq);
		else
		    eq_set_vect_nul(ineg);
	    }
    }
    return (result);
}

/* int sc_check_inequality_redundancy(Pcontrainte ineq, Psysteme ps)
 * Check if an inequality ineq, possibly part of ps, is trivially
 * infeasible (return 2), redundant (return 1), potentially useful
 * (return 0) with respect to inequalities in ps.
 *
 * Neither ps nor ineq are modified. ineq may be one of ps constraints.
 */
int sc_check_inequality_redundancy(Pcontrainte ineq, Psysteme ps)
{
    Pcontrainte c = CONTRAINTE_UNDEFINED;
    int code = 0;

    for(c = sc_inegalites(ps);
	code ==0 && !CONTRAINTE_UNDEFINED_P(c);
	c = contrainte_succ(c)) {

	if(c!=ineq) {
	    Value b;

	    if(eq_smg(c, ineq)) {
		b = eq_diff_const(c, ineq);
		if(value_neg_p(b)) {
		    /* c is redundant with ineq */
		    ;
		}
		else {
		    /* ineq is redundant with c */
		    code = 1;
		}
	    }
	    else if (inequalities_opposite_p(c, ineq)) {
		b = eq_sum_const(c, ineq);
		if(value_negz_p(b)) {
		    /* c and ineq define a non-empty interval */
		    ;
		}
		else {
		    /* ineq and c are incompatible */
		    code = 2;
		}
	    }
	}
    }
    return code;
}


/* void sc_elim_empty_constraints(Psysteme ps, boolean process_equalities):
 * elimination des "fausses" contraintes du systeme ps, i.e. les contraintes ne
 * comportant plus de couple (variable,valeur), i.e. les contraintes qui
 * ont ete eliminees par la fonction 'eq_set_vect_nul', i.e. 0 = 0 ou
 * 0 <= 0
 * 
 * resultat retourne par la fonction: le systeme initial ps est modifie.
 * 
 * parametres de la fonction:
 *   !Psysteme ps: systeme lineaire 
 *   boolean egalite: TRUE s'il faut traiter la liste des egalites 
 *                    FALSE s'il faut traiter la liste des inegalites
 *
 * Modifications:
 *  - the number of equalities was always decremented, regardless
 *    of the process_equalities parameter; Francois Irigoin, 30 October 1991
 */
void sc_elim_empty_constraints(ps, process_equalities)
Psysteme ps;
boolean process_equalities;
{
    Pcontrainte pc, ppc;

    if (!SC_UNDEFINED_P(ps)) {
	if  (process_equalities) {
	    pc = ps->egalites; 
	    ppc = NULL;
	    while (pc != NULL) {
		if  (contrainte_vecteur(pc) == NULL) {
		    Pcontrainte p = pc;
		    if (ppc == NULL) ps->egalites = pc = pc->succ;
		    else 
			ppc->succ = pc = pc->succ;
		    contrainte_free(p);
		    ps->nb_eq--; 
		}
		else {
		    ppc = pc;
		    pc = pc->succ;
		}
	    }
	}
	else {
	    pc = ps->inegalites; 
	    ppc = NULL; 
	    while (pc != NULL) {
		if  (contrainte_vecteur(pc) == NULL) {
		    Pcontrainte p = pc;
		    if (ppc == NULL)  ps->inegalites = pc = pc->succ;
		    else
			ppc->succ = pc = pc->succ;
		    contrainte_free(p);
		    ps->nb_ineq--;
		}
		else {
		    ppc = pc;
		    pc = pc->succ;
		}
	    }
	}
    }
}

/* Psysteme sc_elim_db_constraints(Psysteme ps):
 * elimination des egalites et des inegalites identiques ou inutiles dans
 * le systeme; plus precisemment:
 *
 * Pour les egalites, on elimine une equation si on a un systeme d'egalites 
 * de la forme :
 *
 *   a1/    Ax - b == 0,            ou  b1/        Ax - b == 0,              
 *          Ax - b == 0,                           b - Ax == 0,              
 *
 * ou   c1/    0 == 0
 *
 * Pour les inegalites, on elimine une inequation si on a un systeme 
 * d'inegalites de la forme :
 *  
 *   a2/    Ax - b <= c,             ou   b2/     0 <= const  (avec const >=0)
 *          Ax - b <= c             
 *
 *  resultat retourne par la fonction :
 *
 *  Psysteme   	    : Le systeme initial est modifie (si necessaire) et renvoye
 *       	      Si le systeme est non faisable (0 <= const <0 ou
 *                    0 = b), il est desalloue et NULL est
 *                    renvoye.
 *
 * Attention, on ne teste pas les proportionalites: 2*i=2 est different
 * de i = 1. Il faut reduire le systeme par gcd avant d'appeler cette
 * fonction sc_elim_db_constraints()
 *
 * Notes:
 *  - le temps d'execution doit pouvoir etre divise par deux en prenant en
 * compte la symetrie des comparaisons et en modifiant l'initialisation
 * des boucles internes pour les "triangulariser superieurement".
 *  - la representation interne des vecteurs est utilisee pour les tests;
 * il faudrait tester la colinearite au vecteur de base representatif du
 * terme constant
 *
 * - so called triangular version, FC 28/09/94
 */
Psysteme sc_elim_db_constraints(ps)
Psysteme ps;
{
    Pcontrainte
	eq1 = NULL,
	eq2 = NULL;

    if (SC_UNDEFINED_P(ps)) 
	return(NULL);

    for (eq1 = ps->egalites; eq1 != NULL; eq1 = eq1->succ) 
    {
	if ((vect_size(eq1->vecteur) == 1) && 
	    (eq1->vecteur->var == 0) && (eq1->vecteur->val != 0)) 
	{
	    /* b = 0 */
	    sc_rm(ps);
	    return(NULL);
	}

	for (eq2 = eq1->succ; eq2 != NULL;eq2 = eq2->succ)
	    if (egalite_equal(eq1, eq2))
		eq_set_vect_nul(eq2);
    }

    for (eq1 = ps->inegalites; eq1 != NULL;eq1 = eq1->succ)
    {
	if ((vect_size(eq1->vecteur) == 1) && (eq1->vecteur->var == 0))
	    if (value_negz_p(val_of(eq1->vecteur)))
		vect_rm(eq1->vecteur),
		eq1->vecteur = NULL;
	    else
	    {
		/* 0 <= b < 0 */
		sc_rm(ps);
		return(NULL);
	    }
	
	for (eq2 = eq1->succ;eq2 != NULL;eq2 = eq2->succ)
	    if (contrainte_equal(eq1,eq2))
		eq_set_vect_nul(eq2);
    }

    sc_elim_empty_constraints(ps, TRUE);
    sc_elim_empty_constraints(ps, FALSE);

    return (ps);
}


 #include <stdio.h>
#include <setjmp.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

jmp_buf overflow_error;

/* This function returns TRUE if the inequation ineq is redundant for 
*  the system  ps and FALSE otherwise.
*  sc and ineq are not modified by the function.
*/
 
boolean ineq_redund_with_sc_p(sc,ineq)
Psysteme sc;
Pcontrainte ineq;
{
    Psysteme ps= sc_dup(sc);
    Pcontrainte ineg = contrainte_dup(ineq);
    boolean result = FALSE;

    contrainte_reverse(ineg);
    sc_add_inegalite(ps,ineg);

    /* test de sc_faisabilite avec la nouvelle inegalite      */
     if (!sc_rational_feasibility_ofl_ctrl(ps,OFL_CTRL,TRUE))
	result = TRUE;
    sc_rm(ps);
    return(result);
}


/* boolean eq_redund_with_sc_p(sc, eq)
 * Psysteme sc;
 * Pcontrainte eq;
 *
 *     IN: sc, eq
 *    OUT: returned boolean
 *
 * true if eq is redundant with sc
 *
 * (c) FC 16/05/94
 */
boolean eq_redund_with_sc_p(sc, eq)
Psysteme sc;
Pcontrainte eq;
{
    if (!ineq_redund_with_sc_p(sc, eq)) /* eq considered as an inequality */
	return(FALSE);
    else
    {
	Pcontrainte
	    c = contrainte_dup(eq);
	boolean
	    res = ineq_redund_with_sc_p(sc, (contrainte_chg_sgn(c), c));
	contrainte_free(c);
	return(res);
    }
}


/* Psysteme extract_nredund_subsystem(s1, s2)
 * Psysteme s1, s2;
 *
 *       IN: s1, s2
 *      OUT: returned Psysteme
 *
 * returns the constraints of s1 that are not redundant with s2
 *
 * (c) FC 16/05/94
 */
Psysteme extract_nredund_subsystem(s1, s2)
Psysteme s1, s2;
{
    Psysteme
	new = SC_UNDEFINED;
    Pcontrainte
	c = CONTRAINTE_UNDEFINED,
	eq = CONTRAINTE_UNDEFINED,
	in = CONTRAINTE_UNDEFINED,
	cnew = CONTRAINTE_UNDEFINED; /* temporary */
	
    /* inequalities 
     */
    for(c=sc_inegalites(s1);
	c!=CONTRAINTE_UNDEFINED;
	c=c->succ)
	if (!ineq_redund_with_sc_p(s2, c))
	    cnew = contrainte_dup(c),
	    cnew->succ = in,
	    in = cnew;
    
    /* equalities
     */
    for(c=sc_egalites(s1);
	c!=CONTRAINTE_UNDEFINED;
	c=c->succ)
	if (!eq_redund_with_sc_p(s2, c))
	    cnew = contrainte_dup(c),
	    cnew->succ = eq,
	    eq = cnew;

    new = sc_make(eq, in);
    return(sc_nredund(&new), new);
}


/* Psysteme build_sc_nredund_1pass_ofl_ctrl(Psysteme ps, int ofl_check_level)
 * input    : a system in which redundancies must be eliminated, and an
 *            integer indicating how overflow errors must be handled.
 * output   : Computes a new system sc from the system ps, where each 
 *            constraint of the system ps is added to the new system sc, 
 *            if the constraint is not redundant with the system sc 
 *            previously computed. 
 * modifies : 
 * comment  : 
 *            The set of equalities is copied as such and ignored by 
 *            redundancy checks.
 *
 *            if ofl_check_level == 0 overflow errors are trapped in the 
 *               routine which checks if the system is feasible.
 *            if ofl_check_level == 2 overflow errors are trapped in the 
 *               current routine, and the remaining constraints are simply 
 *               added, as if they were not redundant.
 *            if ofl_check_level == 1 overflow errors are forwarded to the
 *               calling routine.
 */
void build_sc_nredund_1pass_ofl_ctrl(psc, ofl_check_level)
Psysteme *psc;
int ofl_check_level;
{

    Psysteme sc;
    Psysteme ps = *psc;
    Pcontrainte ineq, ineg;

    if (SC_UNDEFINED_P(ps) || sc_rn_p(ps) || sc_empty_p(ps)) 
	return;

    sc = sc_init_with_sc(ps);
    if (!sc_rational_feasibility_ofl_ctrl(ps,OFL_CTRL,TRUE)) { 
	Pvecteur v = vect_new(TCST, 1);
	Pcontrainte eq = contrainte_make(v);
	sc->egalites = eq;
	sc->nb_eq =1;
	sc_rm(ps);
	*psc =sc;
	return;
    }

    sc->egalites = contraintes_dup(ps->egalites);
    sc->nb_eq = ps->nb_eq;
 
    for (ineq = ps->inegalites;
	 !CONTRAINTE_UNDEFINED_P(ineq);
	 ineq=ineq->succ) 
    {
	ineg = contrainte_dup(ineq);
	contrainte_reverse(ineg);
	
	sc_add_inegalite(sc,ineg);
	if (sc_rational_feasibility_ofl_ctrl(sc,ofl_check_level,TRUE))
	    contrainte_reverse(ineg);
	else {
	    sc->inegalites = sc->inegalites->succ;
	    ineg->succ = NULL;
	    contrainte_rm(ineg);
	    sc->nb_ineq--;
	}
    }
    sc_rm(ps);
    *psc = sc;
} 


/* Computation of a new system sc from the system ps, where each 
 * constraint of the system ps is added to the new system sc, 
 * if the constraint is not redundant with the system sc previously 
 * computed.
 * 
 * The set of equalities is copied as such and ignored by redundancy checks.
 */
void build_sc_nredund_1pass(ps)
Psysteme *ps;
{
    return(build_sc_nredund_1pass_ofl_ctrl(ps,OFL_CTRL));
} 



void build_sc_nredund_2pass_ofl_ctrl(psc,ofl_ctrl)
Psysteme *psc;
int ofl_ctrl;
{
    Psysteme ps = *psc;
       Pbase psbase = base_dup(sc_base(ps));

    if (SC_UNDEFINED_P(ps) || sc_rn_p(ps) || sc_empty_p(ps)) {
	base_rm(psbase);
	return;
    }
    ps = sc_normalize(ps);
    if (SC_UNDEFINED_P(ps)) 
         *psc =  sc_empty(psbase);
    else {
	 build_sc_nredund_1pass_ofl_ctrl(psc, ofl_ctrl);
	 build_sc_nredund_1pass_ofl_ctrl(psc, ofl_ctrl);
    }
}


/* void  build_sc_nredund_2pass
 * Psysteme *psc;
 *
 */

void build_sc_nredund_2pass(Psysteme *psc)
{
    if (SC_UNDEFINED_P(*psc)) 
	return;
    else
	return(build_sc_nredund_2pass_ofl_ctrl(psc,OFL_CTRL));
}


/* This function returns TRUE if the constraint ineq can be eliminated 
 * from the system sc and FALSE oterwise. 
 * It assumes that two constraints at least must be kept for constraining
 * the variable "var_hr" in the system.
 * the array "tab_info" contains the useful informations allowing to know
 * the number of constraints constraining  each variable as upper or 
 * lower bounds.
*/

static boolean sc_elim_triang_integer_redund_constraint_p(pc2,index_base,ineq,var_hr,tab_info,rank_max)
Pcontrainte pc2;
Pbase index_base;
Pcontrainte ineq;
Variable var_hr;
int tab_info[][3];
int *rank_max;
{

    int rank_hr = rank_of_variable(index_base,var_hr);
    int coeff=vect_coeff(var_hr,ineq->vecteur);
    int sign = (coeff >0) ? 1:-1;
    boolean result=FALSE;
    boolean trouve=FALSE;
    *rank_max=rank_hr;

    if (tab_info[rank_hr][1]) {

	/* This condition is TRUE if the variable is a loop index. 
	   As the constraint constrains directly the variable, 
	   this constraint must be kept if there is not enough 
	   remainding constraints  
	   */

	if (((sign >0) && (tab_info[rank_hr][2]>1))
	    || ((sign <0) && (tab_info[rank_hr][3]>1)))
	    result = TRUE;
    }
    else {
	register Pcontrainte pc;

	for (pc = pc2;
	     !CONTRAINTE_UNDEFINED_P(pc) && !trouve;
	     pc = pc->succ) {

	    int coeff2 = vect_coeff(var_hr,pc->vecteur);
	    int sign2 = (coeff2 >0) ? 1:-1;
	    int right_coeff,right_rank,left_coeff,left_rank;
	    Variable right_var,left_var;
				    
	    if (coeff2 !=0 && sign == -sign2) {
		constraint_integer_combination(index_base,ineq,pc,rank_hr,
			       &right_var,&right_rank,&right_coeff,
			       &left_var,&left_rank,&left_coeff);
		*rank_max = (right_rank > left_rank) ? right_rank : left_rank;
		if (((right_rank>left_rank) 
		     && (((right_coeff >0) && (tab_info[right_rank][2] <=1))
			 || ((right_coeff <0) && (tab_info[right_rank][3] <=1)))) 
		    || ((right_rank<left_rank) 
			&& (((left_coeff>0) && (tab_info[left_rank][2]<=1)) 
			    || ((left_coeff<0) && (tab_info[left_rank][3] <=1)))))
		    trouve = TRUE;
	    }   
	}
	if (!trouve) result = TRUE;
    } 
    return(result);

}


/* Computation of a new system sc from the system ps, where each 
 * constraint of the system ps is added to the new system sc, 
 * if the constraint is not redundant with the system sc previously 
 * computed. 
 *
 * The difference with the function build_sc_nredund is that at least 
 * 2 constraints are kept for each variable: one for the upper bound 
 * and the other for the lower bound. 
 *
 */ 

Psysteme build_integer_sc_nredund(ps,index_base,tab_info,loop_level,dim_h,n)
Psysteme ps;
Pbase index_base;
int tab_info[][3];
int loop_level;
int dim_h;
int n;
{

    Psysteme sc = sc_new();
    Pcontrainte ineq,pred,eq;
    int rank_hr,rank_max = 0;
    Variable var_hr;
    int coeff,sign;

    sc->base = base_dup(ps->base);
    sc->dimension = ps->dimension; 
    sc->inegalites = contrainte_dup(ps->inegalites);
    sc->nb_ineq +=1;

    for (eq = ps->egalites;
	 !CONTRAINTE_UNDEFINED_P(eq); eq=eq->succ) {
	Pcontrainte pc=contrainte_dup(eq);
	sc_add_eg(sc,pc);
    }

    if (!CONTRAINTE_UNDEFINED_P(ps->inegalites))  {

	for (pred = ps->inegalites,ineq = (ps->inegalites)->succ;
	     !CONTRAINTE_UNDEFINED_P(ineq); ineq=ineq->succ) {

	    Pcontrainte ineg = contrainte_dup(ineq);
	    sc_add_inegalite(sc,ineg);	    

	    /* search the characteristics of the variable of higher rank in 
	       the constraint ineq */
	    if (( rank_hr= search_higher_rank(ineq->vecteur,index_base)) >0) {
		var_hr=variable_of_rank(index_base,rank_hr);
		coeff=vect_coeff(var_hr,ineq->vecteur);
		sign = (coeff >0) ? 1:-1;

		if (sc_elim_triang_integer_redund_constraint_p(ps->inegalites,index_base,ineq,
					   var_hr,tab_info, &rank_max) 
		    && (rank_max >= loop_level)) {

		    /* this condition is TRUE if the constraint can be
		       eliminated from the system, that means if this is
		       not the last constraint on the variable or if all
		       the constraints on this variable can be
		       eliminated (the rank of variable is greater the
		       number of loops) */

		    contrainte_reverse(ineg);
		    if (setjmp(overflow_error)) {
			pred = pred->succ;
			contrainte_reverse(ineg);
		    }
		    else {
			/* test de sc_faisabilite avec la nouvelle 
			   inegalite */
			if (!sc_rational_feasibility_ofl_ctrl(sc,OFL_CTRL,TRUE)) { 
			
			    /* si le systeme est non faisable ==>
			       inegalite redondante ==> elimination de
			       cette inegalite */
			    sc->inegalites = sc->inegalites->succ;
			    ineg->succ = NULL;
			    contrainte_rm(ineg);
			    sc->nb_ineq -=1;
			    pred->succ = ineq->succ;

			    /* mise a jour du nombre de contraintes restantes 
			       contraingnant la variable  de rang rank_hr */
			    if (sign >0) tab_info[rank_hr][2] --;
			    else if (sign <0) tab_info[rank_hr][3]--; 
			}
			else { pred = pred->succ;
			       contrainte_reverse(ineg);
			   }
		    }
		}
	    }
	}
    }
    return(sc);
} 


/* This  function returns TRUE if the constraint C (resulting of the
 *  combination of the two constraints ineq1 and ineq2) is redundant 
 * for the system sc, and FALSE otherwise.
 *
 * Assume that ineq1 = coeff1 (positive) * var + E1 <=0
 *             ineq2 = coeff2 (negative) * var +E2 <=0
 *             C = coeff1 * E2 - coeff2 * E1 - coeff1*coeff2-coeff1 <=0
 *
*/

boolean bound_redund_with_sc_p(sc,ineq1,ineq2,var)
Psysteme sc;
Pcontrainte ineq1,ineq2;
Variable var;
{

    Pcontrainte posit, negat;
    Pcontrainte ineg = CONTRAINTE_UNDEFINED;
    boolean result = FALSE;

    if (!CONTRAINTE_UNDEFINED_P(ineq1) && !CONTRAINTE_UNDEFINED_P(ineq2)) {
	
	if (vect_coeff(var,ineq1->vecteur) > 0) {	    
	    posit = contrainte_dup(ineq1);
	    negat = contrainte_dup(ineq2);
	}
	else  {
	    posit = contrainte_dup(ineq2);
	    negat = contrainte_dup(ineq1);
	}
	
	if (setjmp(overflow_error)) 
	    result = FALSE;
	else {
	    ineg = sc_integer_inequalities_combination_ofl_ctrl(sc,
								posit,
								negat,
								var,
								&result,
								FWD_OFL_CTRL);
	    contrainte_rm(ineg);
	}

	contrainte_rm(posit);
	contrainte_rm(negat);
    }    
    return(result);

}

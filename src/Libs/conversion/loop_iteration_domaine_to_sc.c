
 /* package conversion
  */
#include <stdio.h>

#include "genC.h"
#include "ri.h"
#include "ri-util.h"
#include "misc.h"

#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "conversion.h"

/* Psysteme loop_iteration_domaine_to_sc(cons *lls , Pbase *baseindex)
 * transform the   iteration domain of the nested loops (contained  lls)
 * in a  system of constraints (sc)
 * New loop indices are added at the head of the list  baseindex.
 * The field base of sc contains all the system  variables. Variables are 
 * not in a specific order.
 *
 * CA: Ajout du cas ou l'increment de boucle est different de 1 le 1/9/91
 */  

Psysteme loop_iteration_domaine_to_sc(lls, baseindex)
cons *lls;
Pbase *baseindex;
{
    Psysteme sc;  

    debug_on("CONVERSION_DEBUG_LEVEL");

    debug(8,"loop_iteration_domaine_to_sc", "begin\n");

    sc = sc_new();
    for (; lls != NIL; lls = CDR(lls)){
	loop l = instruction_loop(statement_instruction(STATEMENT(CAR(lls))));
	entity ind = loop_index(l);
	range r = loop_range(l);

	/* add the current index "ind" to the index basis */	
	base_add_dimension(baseindex, (Variable) ind);

	/*add the constraints to sc*/
	loop_index_domaine_to_contrainte(r, ind, sc);
    }  
    sc->nb_ineq = nb_elems_list(sc->inegalites);
    sc_creer_base(sc);

    ifdebug(8) {
	vect_fprint(stderr,*baseindex,entity_local_name);
    }

    debug(8,"loop_iteration_domaine_to_sc", "end\n");

    debug_off();
    return sc;
}

void loop_index_domaine_to_contrainte(r, ind, sc)
range r;
entity ind;
Psysteme sc;
{
    Pvecteur pv;
    Pcontrainte pc;
    Pvecteur 
	vlow = VECTEUR_NUL,
	vup = VECTEUR_NUL,
	pv_incr = VECTEUR_NUL;

    Value incr= VALUE_ONE;

    expression low = range_lower(r);
    expression up = range_upper(r);
    normalized low_norm = NORMALIZE_EXPRESSION(low);
    normalized up_norm = NORMALIZE_EXPRESSION(up);
    normalized incr_norm = NORMALIZE_EXPRESSION(range_increment(r));


    debug_on("CONVERSION_DEBUG_LEVEL");

    debug(8,"loop_index_domaine_to_contrainte", "begin\n");
   
    if (normalized_linear_p(low_norm) && normalized_linear_p(up_norm)){
	vlow = (Pvecteur) normalized_linear(low_norm);	
	vup = (Pvecteur) normalized_linear(up_norm);

    }
    else
	user_error("loop_iteration_domaine_to_sc","untractable loop bound\n");

    if (normalized_linear_p(incr_norm)) 
	pv_incr = (Pvecteur) normalized_linear(incr_norm);
    if (vect_constant_p(pv_incr)) {
	if (value_one_p(incr = vecteur_val(pv_incr))) {
	    /*make two constraints for the current index: 
	      I-vup<=0, -I+vlow<=0 */
	    pv = vect_dup(vup);
	    vect_chg_sgn(pv);
	    vect_add_elem(&pv, (Variable) ind, VALUE_ONE);
	    pc = contrainte_make(pv);
	    sc_add_ineg(sc,pc);
	    pv = vect_dup(vlow);
	    vect_add_elem(&pv, (Variable) ind, VALUE_MONE);
	    pc = contrainte_make(pv);
	    sc_add_ineg(sc,pc);
	}
	else {
	    Variable new_var = creat_new_var(sc);
	    sc->dimension++;
	    sc->base = vect_add_variable(sc->base, (Variable) new_var);
	    /* build   - new_var <= 0 */
	    pv = vect_new((Variable) ind, VALUE_MONE);
	    pc = contrainte_make(pv);
	    sc_add_ineg(sc,pc);
	    /* build     incr * new_var - vup + vlow <= 0 */   
	    pv = vect_dup(vup);
	    vect_chg_sgn(pv);
	    pv = vect_add(pv,vect_dup(vlow));
	    vect_add_elem(&pv, (Variable) new_var, incr);
	    pc = contrainte_make(pv);
	    sc_add_ineg(sc,pc);
	    /* build   ind == vlow + incr * new_var */
	    pv = vect_dup(vlow);
	    vect_chg_sgn(pv);
	    pv = vect_add(pv,vect_new((Variable) ind, VALUE_ONE));
	    pv = vect_add(pv,vect_new((Variable) new_var, value_uminus(incr)));
	    pc = contrainte_make(pv);
	    sc_add_eg(sc,pc);
	}
    }

    debug(8,"loop_index_domaine_to_contrainte", "end\n");

    debug_off();
}

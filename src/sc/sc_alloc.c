 /* package sc */

#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "arithmetique.h"
#include "assert.h"
#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

#define MALLOC(s,t,f) malloc(s)

/* Psysteme sc_new():
 * alloue un systeme vide, initialise tous les champs avec des
 * valeurs nulles, puis retourne ce systeme en resultat.
 * 
 * Attention, sc_new ne fabrique pas un systeme coherent comportant
 * une base. Un tel systeme s'obtient par appel a la fonction sc_creer_base,
 * apres avoir ajoute des equations et des inequations au systeme. La base
 * n'est pas entretenue lorsque le systeme est modifie.
 *
 * Ancien nom: init_systeme()
 */
Psysteme sc_new()
{
    Psysteme p;

    p = (Psysteme) malloc(sizeof(Ssysteme));
    if (p == NULL) {
	(void) fprintf(stderr,"sc_new: Out of memory space\n");
	exit(-1);
    }
    p->nb_eq = p->nb_ineq = p->dimension = 0;
	
    p->egalites = (Pcontrainte ) NULL;
    p->inegalites = (Pcontrainte) NULL;
    p->base = (Pbase) NULL;

    return(p);
}

/* creation d'une base contenant toutes les variables
 * apparaissant avec des coefficients non-nuls
 *  dans les egalites ou les inegalites de ps
 */
Pbase sc_to_minimal_basis(Psysteme ps)
{
    Pbase b = BASE_NULLE;
    Pcontrainte eq;
    Pvecteur pv;

	for(eq = ps->egalites; eq!= NULL; eq=eq->succ) {
	    for (pv = eq->vecteur;pv!= NULL;pv=pv->succ)
		if (pv->var != TCST)
		    vect_chg_coeff(&b,pv->var, VALUE_ONE);
	}

	for(eq = ps->inegalites; eq!= NULL; eq=eq->succ) {
	    for (pv = eq->vecteur;pv!= NULL;pv=pv->succ)
		if (pv->var != TCST)
		    vect_chg_coeff(&b,pv->var, VALUE_ONE);
	}

    return b;
}

/* void sc_creer_base(Psysteme ps): initialisation des parametres dimension
 * et base d'un systeme lineaire en nombres entiers ps, i.e. de la
 * base implicite correspondant aux egalites et inegalites du systeme;
 *
 * Attention, cette base ne reste pas coherente apres un ajout de nouvelles
 * egalites ou inegalites (risque d'ajout de nouvelles variables), ni apres
 * des suppressions (certaines variables de la base risque de n'apparaitre
 * dans aucune contrainte).
 *
 * dimension : nombre de variables du systeme (i.e. differentes de TCST, le
 *          terme constant)
 *       
 * Modifications:
 *  - passage de num_var a base (FI, 13/12/89)
 */
void sc_creer_base(ps)
Psysteme ps;
{
    if (ps) {
	assert(ps->base == (Pbase) NULL);
	ps->base = sc_to_minimal_basis(ps);
	ps->dimension = vect_size(ps->base);
    }
}

/* Variable * sc_base_dup(int nbv, Variable * b):
 * duplication de la table des variables base, qui contient nbv elements
 *
 * Modifications:
 *  - on renvoie un pointeur NULL si le nombre de variables nbv est nul
 *  - changement de num_var en base; cette fonction perd tout interet
 *    et n'est conservee que pour faciliter la mise a jour des modules
 *    de plint (FI, 19/12/89)
 *
 * ancien nom: tab_dup
 */
Pbase sc_base_dup(nbv,b)
int nbv;
Pbase b;
{
    assert(nbv==base_dimension(b));

    return((Pbase) base_dup(b));
}

/* Psystem sc_dup(Psysteme ps): duplication d'un systeme (allocation et copie
 * complete des champs sans sharing)
 *
 * allocate cp;
 * cp := ps;
 *
 * Ancien nom (obsolete): cp_sc()
 *
 * Modification: la base est maintenant recopiee dans le meme ordre que la
 * base initiale. (CA, 28/08/91) 
 */
Psysteme sc_dup(ps)
Psysteme ps;
{
    Psysteme cp = SC_UNDEFINED;
    Pcontrainte eq, eq_cp;

    if (!SC_UNDEFINED_P(ps)) {
	cp = sc_new();

	for (eq = ps->egalites; eq != NULL; eq = eq->succ) {
	    eq_cp = contrainte_new();
	    contrainte_vecteur(eq_cp) = vect_dup(contrainte_vecteur(eq));
	    sc_add_egalite(cp, eq_cp);
	}

	for(eq=ps->inegalites;eq!=NULL;eq=eq->succ) {
	    eq_cp = contrainte_new();
	    contrainte_vecteur(eq_cp) = vect_dup(contrainte_vecteur(eq));
	    sc_add_inegalite(cp, eq_cp);
	}

	if(ps->dimension==0) {
	    assert(VECTEUR_UNDEFINED_P(ps->base));
	    cp->dimension = 0;
	    cp->base = VECTEUR_UNDEFINED;
	}
	else {
	    assert(ps->dimension==vect_size(ps->base));
	    cp->dimension = ps->dimension;
	    cp->base = base_dup(ps->base);
	    
	}
    }
    return(cp);
}

/* void sc_rm(Psysteme ps): liberation de l'espace memoire occupe par le
 * systeme de contraintes ps;
 * 
 * utilisation standard:
 *    sc_rm(s);
 *    s = NULL;
 * 
 * comme toujours, les champs pointeurs sont remis a NULL avant la
 * desallocation pour detecter au plus tot les erreurs dues a l'allocation
 * dynamique de memoire
 * 
 */
void sc_rm(ps)
Psysteme ps;
{
    if (ps != NULL) {
	if (ps->inegalites != NULL) {
	    contraintes_free(ps->inegalites);
	    ps->inegalites = NULL;
	}

	if (ps->egalites != NULL) {
	    contraintes_free(ps->egalites);
	    ps->egalites = NULL;
	}

	if (!VECTEUR_NUL_P(ps->base)) {
	    vect_rm(ps->base);
	    ps->base = VECTEUR_UNDEFINED;
	}

	free((char *) ps);
    }
}

/* This function returns a new empty system which has been initialized with 
 * the same dimension and base than sc.
 */
Psysteme sc_init_with_sc(sc)
Psysteme sc;
{

    Psysteme sc1= sc_new();
    sc1->dimension = sc->dimension;
    sc1->base =base_dup(sc->base); 
    return(sc1);
} 

/* Psysteme sc_empty(Pbase b): build a Psysteme with one unfeasible constraint to
 * define the empty subspace in a space  R^n, where
 * n is b's dimension. b is shared by sc.
 *
 * The unfeasible constraint is the equations 0 == 1
 */
Psysteme sc_empty(b)
Pbase b;
{
    Psysteme sc = SC_UNDEFINED;
    Pvecteur v = vect_new(TCST, VALUE_ONE);
    Pcontrainte eq = contrainte_make(v);
    sc = sc_make(eq, CONTRAINTE_UNDEFINED);

    sc_base(sc) = b;
    sc_dimension(sc) = base_dimension(b);

    return sc;
}

/* Psysteme sc_rn(Pbase b): build a Psysteme without constraints to define R^n, where
 * n is b's dimension. b is shared by sc.
 */
Psysteme sc_rn(b)
Pbase b;
{
    Psysteme sc = sc_new();

    sc_base(sc) = b;
    sc_dimension(sc) = base_dimension(b);

    return sc;
}
/* boolean sc_empty_p(Psysteme sc): check if the set associated to sc is the constant 
 * sc_empty or not. More expensive tests like sc_faisabilite() are necessary to handle 
 * the general case.
 */
boolean sc_empty_p(sc)
Psysteme sc;
{
    boolean empty = FALSE;

    assert(!SC_UNDEFINED_P(sc));
    if(sc_nbre_inegalites(sc)==0 && sc_nbre_egalites(sc)==1) {
	Pvecteur eq = contrainte_vecteur(sc_egalites(sc));

	empty = vect_size(eq) == 1 && vecteur_var(eq) == TCST;
	if(empty)
	    assert(vecteur_val(eq)!=0);
    }
    return empty;
}

/* boolean sc_rn_p(Psysteme sc): check if the set associated to sc is the whole space, rn
 */
boolean sc_rn_p(sc)
Psysteme sc;
{
    assert(!SC_UNDEFINED_P(sc));
    return sc_nbre_inegalites(sc)==0 && sc_nbre_egalites(sc)==0;
}

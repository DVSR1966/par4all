 /* transformer package - fix point computation
  *
  * Francois Irigoin, 21 April 1990
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
#include "ray_dte.h"
#include "sommet.h"
#include "sg.h"
#include "polyedre.h"

#include "matrice.h"

#include "transformer.h"
/*
transformer transformer_fix_point(t1, t2)
transformer t1;
transformer t2;
{
    Psysteme r1;
    Psysteme r2;
    Psysteme r;
    transformer t = transformer_identity();

    debug(8,"transformer_fix_point","begin\n");

    pips_assert("transformer_convex_hull",
		arguments_equal_p(transformer_arguments(t1),
				  transformer_arguments(t2)));
    transformer_arguments(t) = gen_copy_seq(transformer_arguments(t1));

    r1 = (Psysteme) predicate_system(transformer_relation(t1));
    r2 = (Psysteme) predicate_system(transformer_relation(t2));
    r = sc_elarg(r1, r2);

    predicate_system(transformer_relation(t)) = (char *) r;

    debug(8,"transformer_fix_point","end\n");

    return t;
}
*/

transformer transformer_halbwachs_fix_point(tf)
transformer tf;
{
    /* THIS FUNCTION WAS NEVER CORRECT AND HAS NOT BEEN REWRITTEN FOR
       THE NEW VALUE PACKAGE */

    /* simple fix-point for inductive variables: loop bounds are
       ignored */

    /* cons * args = effects_to_arguments(e); */
    /* tf1: transformer for one loop iteration */
    transformer tf1;
    /* tf2: transformer for two loop iterations */
    transformer tf2;
    /* tf12: transformer for one or two loop iterations */
    transformer tf12;
    /* tf23: transformer for two or three loop iterations */
    transformer tf23;
    /* tf_star: transformer for one, two, three,... loop iterations
       should be called tf_plus by I do not use the one_trip flag */
    transformer tf_star = transformer_undefined;

    pips_error("transformer_halbwachs_fix_point","not implemented\n");

    /* preserve transformer of loop body s */
    /* temporarily commented out */
    /*
      tf1 = transformer_rename(transformer_dup(tf), args);
      */
    tf1 = transformer_dup(tf);

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer",
		"tf1 after renaming =");
	(void) print_transformer(tf1);
    }

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer", "tf1 =");
	(void) print_transformer(tf1);
    }

    /* duplicate tf1 because transformer_combine might not be able
       to use twice the same argument */
    tf2 = transformer_dup(tf1);
    tf2 = transformer_combine(tf2, tf1);

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer", "tf2 =");
	(void) print_transformer(tf2);
    }

    /* input/output relation for one or two iteration of the loop */
    tf12 = transformer_convex_hull(tf1, tf2);

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer", "tf12 =");
	(void) print_transformer(tf12);
    }

    /* apply one iteration on tf12 */
    tf23 = transformer_combine(tf12, tf1);

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer", "tf23 =");
	(void) print_transformer(tf23);
    }

    /* fix-point */
    /* should be done again based on Chenikova */
    /* tf_star = transformer_fix_point(tf12, tf23); */

    ifdebug(8) {
	(void) fprintf(stderr,"%s: %s\n","loop_to_transformer", "tf_star =");
	(void) print_transformer(tf_star);
    }

    /* free useless transformers */
    transformer_free(tf1);
    transformer_free(tf2);
    transformer_free(tf12);
    transformer_free(tf23);

    tf = tf_star;

    return tf;
}



static void build_transfer_matrix(pa, lteq, n_eq, b_new)
matrice * pa;
Pcontrainte lteq;
int n_eq;
Pbase b_new;
{
    matrice a = matrice_new(n_eq, n_eq);
    Pcontrainte eq = CONTRAINTE_UNDEFINED;

    matrice_nulle(a, n_eq, n_eq);

    for(eq = lteq; !CONTRAINTE_UNDEFINED_P(eq); eq = eq->succ) {
	Pvecteur t = contrainte_vecteur(eq);
	entity nv = new_value_in_transfer_equation(t);
	int nv_rank = rank_of_variable(b_new, (Variable) nv);

	for( ; !VECTEUR_UNDEFINED_P(t); t = t->succ) {
	    entity e = (entity) vecteur_var(t);

	    if( e != (entity) TCST )
		if(new_value_entity_p(e)) {
		    pips_assert("build_transfer_matrix", 
				value_one_p(vecteur_val(t)));
		}
		else {
		    entity ov = old_value_to_new_value(e);
		    int ov_rank = rank_of_variable(b_new, (Variable) ov);
		    debug(8,"build_transfer_matrix", "nv_rank=%d, ov_rank=%d\n",
			  nv_rank, ov_rank);
		    ACCESS(a, n_eq, nv_rank, ov_rank) = 
			value_uminus(vecteur_val(t));
		}
	    else {
		ACCESS(a, n_eq, nv_rank, n_eq) = 
		    value_uminus(vecteur_val(t));
	    }
	}
    }
    /* add the homogeneous coordinate */
    ACCESS(a, n_eq, n_eq, n_eq) = VALUE_ONE;

    *pa = a;
}


/* Let A be the affine loop transfert function. The affine transfer fix-point T
 * is such that T(A-I) = 0
 *
 * T A = 0 also gives a fix-point when merged with the initial state. It can only be
 * used to compute preconditions.
 *
 * Algorithm (let H be A's Smith normal form):
 *
 * A = A - I (if necessary)
 * H = P A Q
 * A = P^-1 H Q^-1
 * T A = T P^-1 H Q^-1 = 0
 * T P^-1 H = 0 (by multipliying by Q)
 * 
 * Soit X t.q. X H = 0
 * A basis for all solutions is given by X chosen such that
 * X is a rectangular matrix (0 I) selecting the zero submatrix of H
 *
 * T P^-1 = X
 * T = X P
 */

transformer transformer_equality_fix_point(tf)
transformer tf;
{
    /* result */
    transformer fix_tf = transformer_identity();

    /* do not modify an input argument */
    transformer tf_copy = transformer_dup(tf);

    /* number of transfer equalities plus the homogeneous constraint which
       states that 1 == 1 */
    int n_eq = 1;
    /* use a matrix a to store these equalities in a dense form */
    matrice a = MATRICE_UNDEFINED;
    matrice h = MATRICE_UNDEFINED;
    matrice p = MATRICE_UNDEFINED;
    matrice q = MATRICE_UNDEFINED;
    Pbase b_new = BASE_NULLE;
    Pcontrainte lteq = CONTRAINTE_UNDEFINED;
    Pcontrainte leq = sc_egalites((Psysteme) predicate_system(transformer_relation(tf_copy)));

    Psysteme sc_inv = SC_UNDEFINED;
    int n_inv = 0;

    int i = 0;
    Pbase t = BASE_UNDEFINED;

    debug(8, "transformer_equality_fix_point", "begin\n");

    /* find or build explicit transfer equations: v#new = f(v1#old, v2#old,...)
     * and the corresponding sub-basis
     *
     * FI: for each constant variable, whose constance is implictly implied by not having it
     * in the argument field, an equation should be added...
     *
     * lieq = build_implicit_equalities(tf);
     * lieq->succ = leq;
     * leq = lieq;
     */
    build_transfer_equations(leq, &lteq, &b_new);

    /* build matrix a from lteq and b_new: this should be moved in a function */
    n_eq = base_dimension(b_new)+1;
    build_transfer_matrix(&a , lteq, n_eq, b_new);
    ifdebug(8) {
	debug(8, "transformer_equality_fix_point", "transfer matrix:\n");
	matrice_fprint(stderr, a, n_eq, n_eq);
    }

    /* Subtract the identity matrix */
    for(i=1; i<= n_eq; i++) {
	value_substract(ACCESS(a, n_eq, i, i),VALUE_ONE);
    }

    /* Compute the Smith normal form: H = P A Q */
    h = matrice_new(n_eq, n_eq);
    p = matrice_new(n_eq, n_eq);
    q = matrice_new(n_eq, n_eq);
    DENOMINATOR(h) = VALUE_ONE;
    DENOMINATOR(p) = VALUE_ONE;
    DENOMINATOR(q) = VALUE_ONE;
    matrice_smith(a, n_eq, n_eq, p, h, q);
 
    ifdebug(8) {
	debug(8, "transformer_equality_fix_point", "smith matrix h=p.a.q:\n");
	matrice_fprint(stderr, h, n_eq, n_eq);
	debug(8, "transformer_equality_fix_point", "p  matrix:\n");
	matrice_fprint(stderr, p, n_eq, n_eq);
	debug(8, "transformer_equality_fix_point", "q  matrix:\n");
	matrice_fprint(stderr, q, n_eq, n_eq);
    }

    /* Find out the number of invariants n_inv */
    for(i=1; i <= n_eq && 
	    value_notzero_p(ACCESS(h, n_eq, i, i)) ; i++)
	;
    n_inv = n_eq - i + 1;
    debug(8, "transformer_equality_fix_point", "number of useful invariants: %d\n", n_inv-1);
    pips_assert("transformer_equality_fix_point", n_inv >= 1);

    /* Convert p's last n_inv rows into constraints */
    /* FI: maybe I should retrieve fix_tf system... */
    sc_inv = sc_new();

    for(i = n_eq - n_inv + 1 ; i <= n_eq; i++) {
	int j;
	/* is it a non-trivial invariant? */
	for(j=1; j<= n_eq-1 && value_zero_p(ACCESS(p, n_eq, i, j)); j++)
	    ;
	if( j != n_eq) {
	    Pvecteur v_inv = VECTEUR_NUL;
	    Pcontrainte c_inv = CONTRAINTE_UNDEFINED;

	    for(j = 1; j<= n_eq; j++) {
		Value coeff = ACCESS(p, n_eq, i, j);

		if(value_notzero_p(coeff)) {
		    entity n_eb = (entity) variable_of_rank(b_new, j);
		    entity o_eb = new_value_to_old_value(n_eb);

		    vect_add_elem(&v_inv, (Variable)n_eb, coeff);
		    vect_add_elem(&v_inv, (Variable)o_eb, value_uminus(coeff));
		}
	    }
	    c_inv = contrainte_make(v_inv);
	    sc_add_egalite(sc_inv, c_inv);
	}
    }

    sc_creer_base(sc_inv);

    ifdebug(8) {
	debug(8, "transformer_equality_fix_point", "fix-point sc_inv=\n");
	sc_fprint(stderr, sc_inv, external_value_name);
	debug(8, "transformer_equality_fix_point", "end\n");
    }

    /* Set fix_tf's argument list */
    for(t = b_new; !BASE_NULLE_P(t); t = t->succ) {
	/* I should use a conversion function from value_to_variable() */
	entity arg = (entity) vecteur_var(t);

	transformer_arguments(fix_tf) = arguments_add_entity(transformer_arguments(fix_tf), arg);
    }
    transformer_relation(fix_tf) = make_predicate(sc_inv);
 
    /* get rid of dense matrices */
    matrice_free(a);
    matrice_free(h);
    matrice_free(p);
    matrice_free(q);

    free_transformer(tf_copy);

    ifdebug(8) {
	debug(8, "transformer_equality_fix_point", "fix-point fix_tf=\n");
	fprint_transformer(stderr, fix_tf, external_value_name);
	debug(8, "transformer_equality_fix_point", "end\n");
    }

    return fix_tf;
}

void build_transfer_equations(leq, plteq, pb_new)
Pcontrainte leq;
Pcontrainte *plteq;
Pbase * pb_new;
{
    Pcontrainte lteq = CONTRAINTE_UNDEFINED;
    Pbase b_new = BASE_NULLE;
    Pbase b_old = BASE_NULLE;
    Pbase b_diff = BASE_NULLE;
    Pcontrainte eq = CONTRAINTE_UNDEFINED;

    ifdebug(8) {
	debug(8, "build_transfer_equations", "begin\ninput equations:\n");
	egalites_fprint(stderr, leq, external_value_name);
    }

    /* FI: this is the simplest version;
     * It would be possible to build a larger set of transfer equations
     * by adding indirect transfer equations using a new basis
     * till the set is stable, and then by removing equations containing
     * old values which are not defined, again till the result is stable
     *
     * I guess that these new equations would not change the fix-point
     * and/or the preconditions.
     */
    for(eq = leq; !CONTRAINTE_UNDEFINED_P(eq); eq = eq->succ) {
	if(transfer_equation_p(contrainte_vecteur(eq))) {
	    Pcontrainte neq = contrainte_dup(eq);

	    neq->succ = lteq;
	    lteq = neq;
	}
    }

    ifdebug(8) {
	debug(8, "build_transfer_equations", "preliminary transfer equations:\n");
	egalites_fprint(stderr, lteq, external_value_name);
    }

    /* derive the new basis and the old basis */
    equations_to_bases(lteq, &b_new, &b_old);

    /* check that the old basis is included in the new basis (else no fix-point!) */
    ifdebug(8) {
	debug(8, "build_transfer_equations", "old basis:\n");
	base_fprint(stderr, b_old, external_value_name);
	debug(8, "build_transfer_equations", "new basis:\n");
	base_fprint(stderr, b_new, external_value_name);
    }

    /* Remove equations as long b_old is not included in b_new */
    b_diff = base_difference(b_old, b_new);
    while(!BASE_NULLE_P(b_diff)) {
	Pcontrainte n_lteq = CONTRAINTE_UNDEFINED;
	Pcontrainte next_eq = CONTRAINTE_UNDEFINED;

	for(eq = lteq; !CONTRAINTE_UNDEFINED_P(eq); eq = next_eq) {
	    bool preserve = TRUE;
	    Pvecteur t = VECTEUR_UNDEFINED;
	    for(t = contrainte_vecteur(eq); !VECTEUR_UNDEFINED_P(t) && preserve; t = t->succ) {
		entity e = (entity) vecteur_var(t);

		preserve = base_contains_variable_p(b_diff, (Variable) e);
	    }
	    next_eq = eq->succ;
	    if(preserve) {
		eq->succ = n_lteq;
		n_lteq = eq;
	    }
	    else {
		contrainte_rm(eq);
	    }
	}
	lteq = n_lteq; /* could be avoided by using only lteq... */
	equations_to_bases(lteq, &b_new, &b_old);
	b_diff = base_difference(b_old, b_new);
    }

    ifdebug(8) {
	debug(8, "build_transfer_equations", "final transfer equations:\n");
	egalites_fprint(stderr, lteq, external_value_name);
	debug(8, "build_transfer_equations", "old basis:\n");
	base_fprint(stderr, b_old, external_value_name);
	debug(8, "build_transfer_equations", "new basis:\n");
	base_fprint(stderr, b_new, external_value_name);
    }

    if(!sub_basis_p(b_old, b_new)) {
	pips_error("build_transfer_equations", "Old basis is too large\n");
    }
    base_rm(b_old);
    b_old = BASE_UNDEFINED;
    *plteq = lteq;
    *pb_new = b_new;

    ifdebug(8) {
	debug(8, "build_transfer_equations", "results\ntransfer equations:\n");
	egalites_fprint(stderr, lteq, external_value_name);
	debug(8, "build_transfer_equations", "results\ntransfer basis:\n");
	base_fprint(stderr, b_new, external_value_name);
	debug(8, "build_transfer_equations", "end\n");
    }
}

/* A transfer equation is an explicit equation giving one new value
 * as a function of old values and a constant term. Because we are
 * using diophantine equations, the coefficient of the new value must
 * one or minus one. We assume that the equation is reduced (i.e.
 * gcd(coeffs) == 1).
 *
 * FI: Non-unary coefficients may appear because equations were combined, for instance
 * by a previous internal fix-point as in KINETI of mdg-fi.f (Perfect Club). Maybe, something
 * could be done for these nested fix-points.
 */
bool transfer_equation_p(eq)
Pvecteur eq;
{
    Pvecteur t;
    int n_new = 0;
    Value coeff = VALUE_ZERO;

    for(t=eq; !VECTEUR_UNDEFINED_P(t) && n_new <= 1; t = t->succ) {
	entity e = (entity) vecteur_var(t);

	if( e != (entity) TCST && new_value_entity_p(e)) {
	    coeff = vecteur_val(t);
	    n_new++;
	}
    }

    return (n_new==1) && (value_one_p(coeff) || value_mone_p(coeff));
}

entity new_value_in_transfer_equation(eq)
Pvecteur eq;
{
    Pvecteur t;
    int n_new = 0;
    Value coeff = VALUE_ZERO; /* for gcc */
    entity new_value = entity_undefined;

    for(t=eq; !VECTEUR_UNDEFINED_P(t) && n_new <= 1; t = t->succ) {
	entity e = (entity) vecteur_var(t);

	if( e != (entity) TCST && new_value_entity_p(e) && 
	   (value_one_p(vecteur_val(t)) || value_mone_p(vecteur_val(t)))) {
	    new_value = e;
	    coeff = vecteur_val(t);
	    n_new++;
	}
    }

    if(value_mone_p(coeff)) {
	for(t=eq; !VECTEUR_UNDEFINED_P(t) && n_new <= 1; t = t->succ) {
	    value_oppose(vecteur_val(t));
	}
    }

    pips_assert("new_value_in_transfer_equation", n_new==1);

    return new_value;
}

/* FI: should be moved in base.c */

/* sub_basis_p(Pbase b1, Pbase b2): check if b1 is included in b2 */
bool sub_basis_p(b1, b2)
Pbase b1;
Pbase b2;
{
    bool is_a_sub_basis = TRUE;
    Pbase t;

    for(t=b1; !VECTEUR_UNDEFINED_P(t) && is_a_sub_basis; t = t->succ) {
	is_a_sub_basis = base_contains_variable_p(b2, vecteur_var(t));
    }

    return is_a_sub_basis;
}

void equations_to_bases(lteq, pb_new, pb_old)
Pcontrainte lteq;
Pbase * pb_new;
Pbase * pb_old;
{
    Pbase b_new = BASE_UNDEFINED;
    Pbase b_old = BASE_UNDEFINED;
    Pcontrainte eq = CONTRAINTE_UNDEFINED;

    for(eq = lteq; !CONTRAINTE_UNDEFINED_P(eq); eq = eq->succ) {
	Pvecteur t;

	for(t=contrainte_vecteur(eq); !VECTEUR_UNDEFINED_P(t); t = t->succ) {
	    entity e = (entity) vecteur_var(t);

	    if( e != (entity) TCST )
		if(new_value_entity_p(e)) {
		    b_new = vect_add_variable(b_new, (Variable) e);
		}
		else {
		    b_old = vect_add_variable(b_old, (Variable) old_value_to_new_value(e));
		}
	}
    }

    *pb_new = b_new;
    *pb_old = b_old;
}


 /* package loops_interchange
 */

#include <stdio.h>

#include "genC.h"
#include "ri.h"
#include "misc.h"
#include "text.h"
#include "prettyprint.h"

#include "ri-util.h"
#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "matrice.h"
#include "matrix.h"
#include "sparse_sc.h"
#include "conversion.h"
/* #include "generation.h" */

/* #include "loops_interchange.h" */
#define INTERCHANGE_OPTIONS "X"
extern void global_parallelization();
extern statement gener_DOSEQ();
extern statement interchange();
extern int set_interchange_parameters();

/* statement gener_DOSEQ(cons *lls,Pvecteur pvg[], Pbase base_oldindex,
 * Pbase base_newindex)
 * generation of loops interchange code for the nested loops  (cons *lls).
 * the new nested loops will be ;
 *  DOSEQ Ip = ...
 *    DOSEQ Jp = ...
 *      DOSEQ Kp = ...
 *        ...
 *  ENDDO
 */
statement gener_DOSEQ(cons *lls, Pvecteur *pvg, Pbase base_oldindex, Pbase base_newindex, Psysteme sc_newbase)
{
    statement state_lhyp = statement_undefined;
    instruction instr_lhyp = instruction_undefined;
    loop l_old = loop_undefined;
    loop l_hyp = loop_undefined;
    range rl = range_undefined;
    expression lower = expression_undefined;
    expression upper = expression_undefined;
    statement bl = statement_undefined; 
    statement s_loop = statement_undefined;
    Pbase pb = BASE_NULLE;

    bl = loop_body(instruction_loop(statement_instruction(STATEMENT(CAR(lls)))));
    statement_newbase(bl,pvg,base_oldindex);
    /* make the parallel loops from inner loop to upper loop*/
   
    for(pb=base_reversal(base_newindex);lls!=NIL; lls=CDR(lls)) {
	/* traitement of current loop */
	s_loop = STATEMENT(CAR(lls));
	l_old = instruction_loop(statement_instruction(s_loop));

	/*new bounds de new index correspondant a old index de cet loop*/
	make_bound_expression(pb->var,base_newindex,sc_newbase,&lower,&upper);
	rl = make_range(lower,upper,make_integer_constant_expression(1));


	if (CDR(lls)!=NULL) {		/* make  the inner sequential loops
					   they will be the inner parallel loops after 
					   integration the phase  of parallelization*/
	    l_hyp = make_loop(pb->var,
			      rl,
			      bl,
			      loop_label(l_old),
			      make_execution(is_execution_sequential,UU),
			      loop_locals(l_old));
	    bl = makeloopbody(l_hyp,s_loop);
	    pb=pb->succ;
	}
    }

    /*make the last loop which is sequential*/
   
    l_hyp = make_loop(pb->var,rl,bl,loop_label(l_old),
		      make_execution(is_execution_sequential,UU),
		      loop_locals(l_old));
    instr_lhyp = make_instruction(is_instruction_loop,l_hyp);
    state_lhyp = make_statement(statement_label(s_loop),
				statement_number(s_loop),
				statement_ordering(s_loop),
				statement_comments(s_loop),
				instr_lhyp);
    return(state_lhyp);
}

/* void interchange(cons *lls)
 *  Implementation of the loops interchange method
 * "lls" is the list of nested loops
 *
 * FI: should be replaced by interchange_two_loops(lls, 1, n)
 */

statement interchange(cons *lls)
{
    Psysteme sci;			/* sc initial */
    Psysteme scn;			/* sc nouveau */
    Psysteme sc_row_echelon;
    Psysteme sc_newbase;
    Pbase base_oldindex = NULL;
    Pbase base_newindex = NULL;
    matrice A;
    matrice G;
    matrice AG;
    int n ;				/* number of index */
    int m ;				/* number of constraints */
    statement s_lhyp;
    Pvecteur *pvg;
    Pbase pb;  
    expression lower, upper;
    Pvecteur pv1, pv2;
    loop l;


    /* make the  system "sc" of constraints of iteration space */
    sci = loop_iteration_domaine_to_sc(lls, &base_oldindex);
    debug(8," interchange","\n begin :");
    
    /* create the  matrix A of coefficients of  index in (Psysteme)sci */
    n = base_dimension(base_oldindex);
    m = sci->nb_ineq;
    A = matrice_new(m,n);
    sys_matrice_index(sci, base_oldindex, A, n, m);

    /* computation of the matrix of basis change  for loops interchange */
    G = matrice_new(n,n);
    matrice_identite(G,n,0);
    matrice_swap_columns(G,n,n,1,n);

    /* the new matrice of constraints AG = A * G */
    AG = matrice_new(m,n);
    matrice_multiply(A,G,AG,m,n,n);


    /* create the new system of constraintes (Psysteme scn) with  
       AG and sci */
    scn = sc_dup(sci);
    matrice_index_sys(scn,base_oldindex,AG,n,m);

    /* computation of the new iteration space in the new basis G */
    sc_row_echelon = new_loop_bound(scn,base_oldindex);

    /* changeof basis for index */
    change_of_base_index(base_oldindex,&base_newindex);
    sc_newbase=sc_change_baseindex(sc_dup(sc_row_echelon),base_oldindex,base_newindex);
    
    /* generation of interchange  code */
    /*  generation of bounds */
    for (pb=base_newindex; pb!=NULL; pb=pb->succ) {
	make_bound_expression(pb->var,base_newindex,sc_newbase,&lower,&upper);
    }
  
    /* loop body generation */
    pvg = (Pvecteur *)malloc((unsigned)n*sizeof(Svecteur));
    scanning_base_to_vect(G,n,base_newindex,pvg);
    pv1 = sc_row_echelon->inegalites->succ->vecteur;
    pv2 = vect_change_base(pv1,base_oldindex,pvg);    

    l = instruction_loop(statement_instruction(STATEMENT(CAR(lls))));
    lower = range_upper(loop_range(l));
    upper= expression_to_expression_newbase(lower, pvg, base_oldindex);


    s_lhyp = gener_DOSEQ(lls,pvg,base_oldindex,base_newindex,sc_newbase);

    return(s_lhyp);
}

statement interchange_two_loops(cons *lls, int n1, int n2)
{
    Psysteme sci;			/* sc initial */
    Psysteme scn;			/* sc nouveau */
    Psysteme sc_row_echelon;
    Psysteme sc_newbase;
    Pbase base_oldindex = NULL;
    Pbase base_newindex = NULL;
    matrice A;
    matrice G;
    matrice AG;
    int n ;				/* number of index */
    int m ;				/* number of constraints */
    statement s_lhyp;
    Pvecteur *pvg;
    Pbase pb;  
    expression lower, upper;
    Pvecteur pv1, pv2;
    loop l;

    debug(8,"interchange_two_loops","\n begin: n1=%d, n2=%d\n", n1, n2);

    /* make the  system "sc" of constraints of iteration space */
    sci = loop_iteration_domaine_to_sc(lls, &base_oldindex);
    
    /* create the  matrix A of coefficients of  index in (Psysteme)sci */
    n = base_dimension(base_oldindex);
    m = sci->nb_ineq;
    A = matrice_new(m,n);
    sys_matrice_index(sci, base_oldindex, A, n, m);

    /* computation of the matrix of basis change  for loops interchange */
    G = matrice_new(n,n);
    matrice_identite(G,n,0);
    matrice_swap_columns(G,n,n, n1, n2);

    /* the new matrice of constraints AG = A * G */
    AG = matrice_new(m,n);
    matrice_multiply(A,G,AG,m,n,n);


    /* create the new system of constraintes (Psysteme scn) with  
       AG and sci */
    scn = sc_dup(sci);
    matrice_index_sys(scn,base_oldindex,AG,n,m);

    /* computation of the new iteration space in the new basis G */
    sc_row_echelon = new_loop_bound(scn,base_oldindex);

    /* changeof basis for index */
    change_of_base_index(base_oldindex,&base_newindex);
    sc_newbase=sc_change_baseindex(sc_dup(sc_row_echelon),base_oldindex,base_newindex);
    
    /* generation of interchange  code */
    /*  generation of bounds */
    for (pb=base_newindex; pb!=NULL; pb=pb->succ) {
	make_bound_expression(pb->var,base_newindex,sc_newbase,&lower,&upper);
    }
  
    /* loop body generation */
    pvg = (Pvecteur *)malloc((unsigned)n*sizeof(Svecteur));
    scanning_base_to_vect(G,n,base_newindex,pvg);
    pv1 = sc_row_echelon->inegalites->succ->vecteur;
    pv2 = vect_change_base(pv1,base_oldindex,pvg);    

    l = instruction_loop(statement_instruction(STATEMENT(CAR(lls))));
    lower = range_upper(loop_range(l));
    upper= expression_to_expression_newbase(lower, pvg, base_oldindex);


    s_lhyp = gener_DOSEQ(lls,pvg,base_oldindex,base_newindex,sc_newbase);

    debug(8, "interchange_two_loops", "end\n");

    return(s_lhyp);
}

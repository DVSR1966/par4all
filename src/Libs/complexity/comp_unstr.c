/* comp_unstr.c */
/* evaluate the complexity of unstructured graphs of statements */
#define _POSIX_SOURCE

#include <stdio.h>

#include <math.h>

#include "genC.h"
#include "ri.h"
#include "complexity_ri.h"
#include "ri-util.h"
#include "properties.h"  /* used by get_bool_property   */
#include "misc.h"
#include "control.h"
#include "matrice.h"
#include "complexity.h"

/* Added by AP, March 15th 93: allows the simulation of a two-dimensional array
 * with a mono-dimensional memory allocation.
 */
#define FA(i,j) fa[(i)*n_controls + (j)]

extern hash_table hash_callee_to_complexity;
extern hash_table hash_complexity_parameters;

/* 6th element of instruction */
/* complexity unstructured_to_complexity(unstructured unstr;
 *                                       transformer precond;
 *                                       list effects_list;
 *
 * Return the complexity of the unstructured graph unstr
 * whose nodes are controls.
 *
 * First runs through the graph to compute nodes' complexities,
 * the total number of nodes, and give a number to each node.
 * This is done in "controls_to_hash_table()".
 *
 * Then runs again through it to fill the probability matrix [Pij]
 * ( Pij = probability to go to node j when you're in node i)
 * This is done in "build_probability_matrix()"
 *
 * Then computes A = I-P, then inv_A = 1/A with the matrix package.
 *
 * MAX_CONTROLS_IN_UNSTRUCTURED = 100
 */
complexity unstructured_to_complexity(unstr, precond, effects_list)
unstructured unstr;
transformer precond;
list effects_list;
{
    complexity comp = make_zero_complexity(); 
    hash_table hash_control_to_complexity = hash_table_make(hash_pointer, 
					    MAX_CONTROLS_IN_UNSTRUCTURED);
    int i, j, n_controls = 0;
    control control_array[MAX_CONTROLS_IN_UNSTRUCTURED];
    matrice P, A, B;

    trace_on("unstructured");

    controls_to_hash_table(unstructured_control(unstr),
			   &n_controls,
			   control_array,
			   hash_control_to_complexity,
			   precond,
			   effects_list);
    
     
    P = average_probability_matrix(unstr, n_controls, control_array);
    A = matrice_new(n_controls, n_controls);
    B = matrice_new(n_controls, n_controls);

    /* A is identity matrix I */
    matrice_identite(A, n_controls, 0);   
    /* B = A - P =  I - P */
    matrice_substract(B, A, P, n_controls, n_controls);  

    if (get_debug_level() >= 5) {
	fprintf(stderr, "I - P =");
	matrice_fprint(stderr, B, n_controls, n_controls);
    }

    /* matrice_general_inversion(B, A, n_controls);  */    
    /* A = 1/(B) */
    /* This region will call C routines. LZ 20/10/92  */

    /* 
       the "global" complexity is:
       comp = a11 * C1 + a12 * C2 + ... + a1n * Cn
       where Ci = complexity of control #i
     */

    if ( n_controls < MAX_CONTROLS_IN_UNSTRUCTURED ) {

	/* Modif by AP, March 15th 93: old version had non constant int in dim decl
	float fa[n_controls][n_controls];
	int indx[n_controls];
	int d;
	*/
	float *fa = (float *) malloc(n_controls*n_controls*sizeof(float));
	int *indx = (int *) malloc(sizeof(int) * n_controls);
	int d;

	for (i=1; i<=n_controls; i++) {
	    for (j=1; j<=n_controls; j++ ) {
		FA(i-1,j-1) = (float)(ACCESS(B, n_controls, i, j))/DENOMINATOR(B);
	    }
	}

	if ( get_debug_level() >= 5 ) {
	    fprintf(stderr, "Before float matrice inversion\n\n");
	}

	if ( get_debug_level() >= 9 ) {
	    fprintf(stderr, "(I - P) =\n");
	    for (i=0;i<n_controls;i++) {
		for (j=0;j<n_controls;j++)
		   fprintf(stderr, "%4.2f ",FA(i,j) );
		fprintf(stderr, "\n");
	    }
	}

	float_matrice_inversion(fa, n_controls, indx, &d);

	if ( get_debug_level() >= 5 ) {
	    fprintf(stderr, "After  float matrice inversion\n\n");
	}

	if ( get_debug_level() >= 9 ) {
	    fprintf(stderr, "(I - P)^(-1) =\n");
	    for (i=0;i<n_controls;i++) {
		for (j=0;j<n_controls;j++)
		   fprintf(stderr, "%4.2f ",FA(i,j) );
		fprintf(stderr, "\n");
	    }
	}

	for (i=1; i<=n_controls; i++) {
	    control conti = control_array[i];
	    complexity compi = (complexity) 
		hash_get(hash_control_to_complexity, (char *) conti);
	    float f = FA(0,i-1);

	    if ( get_debug_level() >= 5 ) {
		fprintf(stderr, "control $%X:f=%f, compl.=",(int)conti, f);
		complexity_fprint(stderr, compi, TRUE, FALSE);
	    }

	    complexity_scalar_mult(&compi, f);
	    complexity_add(&comp, compi);
	}
    }
    else
	pips_error("unstructured_to_complexity", "Too large to compute\n");

    if (get_debug_level() >= 5) {
	fprintf(stderr, "cumulated complexity=");
	complexity_fprint(stderr, comp, TRUE, FALSE);
    }

    matrice_free(B);
    matrice_free(A);
    matrice_free(P);

    hash_table_free(hash_control_to_complexity);

    complexity_check_and_warn("unstructured_to_complexity", comp);    

    trace_off();
    return(comp);
}


/* Returns the hash table hash_control_to_complexity filled in
 * with the complexities of the successors of control cont.
 * each control of the graph is also stored in the array
 * control_array (beginning at 1).
 * also returns the total number of controls in the unstructured
 */
void controls_to_hash_table(cont, pn_controls, control_array,
			    hash_control_to_complexity, precond, effects_list)
control cont;
int *pn_controls;
control control_array[];
hash_table hash_control_to_complexity;
transformer precond;
list effects_list;
{
    statement s = control_statement(cont);
    complexity comp;

    comp = statement_to_complexity(s, precond, effects_list);
    hash_put(hash_control_to_complexity, (char *) cont, (char *) comp);
    control_array[++(*pn_controls)] = cont;
    complexity_check_and_warn("control_to_complexity", comp);

    if (get_debug_level() >= 5) {
	fprintf(stderr, "this control($%X) has:", (int) cont);
	complexity_fprint(stderr, comp, TRUE, TRUE);
	MAPL(pc, {
	    fprintf(stderr, "successor: $%X\n", (int) CONTROL(CAR(pc)));
	    }, control_successors(cont));
	if ( control_successors(cont) == NIL )
	    fprintf(stderr, "NO successor at all!\n");
	fprintf(stderr, ". . . . . . . . . . . . . . . . . . . . . .\n");
    }

    MAPL(pc, {
	control c = CONTROL(CAR(pc));
	if ( hash_get(hash_control_to_complexity, c)==HASH_UNDEFINED_VALUE )
	    controls_to_hash_table(c, pn_controls, control_array,
				   hash_control_to_complexity, precond, effects_list);
    }, control_successors(cont));
}

/* return the number i, that is i'th element of the control array
 * Note that i begins from 1 instead of 0 
 */
int control_element_position_in_control_array(cont, control_array, n_controls)
control cont;
control control_array[];
int n_controls;
{
    int i;
    
    for (i=1; i<=n_controls; i++)
	if (cont == control_array[i]) 
	    return (i);
    pips_error("control_element_position_in_control_array", 
	       "this control isn't in control_array[]!\n");

    /* In order to satisfy compiler. LZ 3 Feb. 93 */
    return (i);
}


matrice average_probability_matrix(unstr, n_controls, control_array)
unstructured unstr;
int n_controls;
control control_array[];
{
    control cont = unstructured_control(unstr);
    boolean already_examined[MAX_CONTROLS_IN_UNSTRUCTURED];
    int i, j , n_succs, max_n_succs = 0;
    matrice P = matrice_new(n_controls, n_controls);

    if ( n_controls > MAX_CONTROLS_IN_UNSTRUCTURED ) {
	pips_error("average_probability_matrix", 
		   "control number is larger than %d\n", 
		   MAX_CONTROLS_IN_UNSTRUCTURED );
    }

    matrice_nulle(P, n_controls, n_controls);

    /* initilize the already_examined to FALSE */
    for (i=1; i<=n_controls; i++) 
	already_examined[i] = FALSE;

    /* make in P the matrix "is_successor_of" */
    node_successors_to_matrix(cont, P, n_controls,
			      control_array, already_examined);

    /* we'll attribute equitable probabilities to the n succ. of a node */
    for (i=1; i<=n_controls; i++) {
	n_succs = 0;
	for (j=1; j<=n_controls; j++)
	    n_succs += ACCESS(P, n_controls, i, j);
	if (n_succs > max_n_succs) 
	    max_n_succs = n_succs;
    }

    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	fprintf(stderr, "n_controls=%d, and max_n_succs=%d\n",
		n_controls,max_n_succs);
    }

    /* computes probabilities (0<=p<1) out of the matrix "is_successor_of" */

    
    DENOMINATOR(P) = factorielle(max_n_succs);

    for (i=1; i<=n_controls; i++) {
	n_succs = 0;
	for (j=1; j<=n_controls; j++)
	    n_succs += ACCESS(P, n_controls, i, j);
	if (n_succs>0)
	    for (j=1; j<=n_controls; j++)
		ACCESS(P, n_controls, i, j) *= DENOMINATOR(P)/n_succs; 
    }

    matrice_normalize(P, n_controls, n_controls);

    if (get_debug_level() > 0) {
	fprintf(stderr, "n_controls is %d\n", n_controls);
	fprintf(stderr, "average_probability_matrix:  P =\n");
	matrice_fprint(stderr, P, n_controls, n_controls);
    }

    return (P);
}

/* 
 * On return, Pij = 1  <=>  there's an edge from control #i to #j 
 * It means that every succeccor has the same possibility to be reached.
 *
 * Modification:
 *  - put already_examined[i] = TRUE out of MAPL.
 *    If control i has several successors, there is no need to set it several
 *    times in MAPL. LZ 13/04/92
 */
void node_successors_to_matrix(cont, P, n_controls,
			       control_array, already_examined)
control cont;
matrice P;
int n_controls;
control control_array[];
boolean already_examined[];
{
    int i = control_element_position_in_control_array(cont, control_array, n_controls);

    already_examined[i] = TRUE;

    if (get_bool_property("COMPLEXITY_INTERMEDIATES")) {
	fprintf(stderr, "CONTROL ($%X)  CONTROL_NUMBER %d\n", (int) cont, i);
    }

    MAPL(pc, {
	control succ = CONTROL(CAR(pc));
	int j = control_element_position_in_control_array(succ, control_array, 
							  n_controls);

	if ( get_debug_level() >= 5 ) {
	    fprintf(stderr,"Control ($%X) %d  -->  Control ($%X) %d\n",
		    (int)cont, i, (int)succ, j);
	}

	/* Here, we give equal possibility , 1 for each one */
	ACCESS(P, n_controls, i, j) = 1;
	if (!already_examined[j])
	    node_successors_to_matrix(succ, P, n_controls,
				      control_array, already_examined);
	else {
	    if ( get_debug_level() >= 5 ) {
		fprintf(stderr, "Control Number %d already examined!\n",j);
	    }
	}
    }, control_successors(cont));
}

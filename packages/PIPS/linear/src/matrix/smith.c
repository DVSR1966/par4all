/*

  $Id$

  Copyright 1989-2012 MINES ParisTech

  This file is part of Linear/C3 Library.

  Linear/C3 Library is free software: you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  Linear/C3 Library is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Linear/C3 Library.  If not, see <http://www.gnu.org/licenses/>.

*/

 /* package matrix */

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

#include "assert.h"

#include "boolean.h"
#include "arithmetique.h"

#include "matrix.h"

#define MALLOC(s,t,f) malloc((unsigned)(s))
#define FREE(s,t,f) free((char *)(s))

/* void matrix_smith(matrice MAT,  matrix P, matrix D, 
 *                    matrix Q):
 * Calcul de la forme reduite de Smith D d'une matrice entiere MAT et des deux
 * matrices de changement de base unimodulaires associees, P et Q.
 *
 * D est la forme reduite de Smith, P et Q sont des matrices unimodulaires;
 * telles que D == P x MAT x Q
 *
 * (c.f. Programmation Lineaire. M.MINOUX. (83))
 *
 * int 	MAT[n,m]	:  matrice
 * int  n	: nombre de lignes de la matrice MAT
 * int  m 	: nombre de colonnes de la matrice MAT
 * int 	P[n,n]	: matrice
 * int 	D[n,m]	: matrice (quasi-diagonale) reduite de Smith
 * int 	Q[m,m]	: matrice
 *  
 * Les 3 matrices P(nxn), Q(mxm) et D(nxm) doivent etre allouees avant le 
 * calcul. 
 *
 * Note: les determinants des matrices MAT, P, Q et D ne sont pas utilises.
 *
 */
void matrix_smith(MAT,P,D,Q)
Pmatrix MAT;
Pmatrix P;
Pmatrix D;
Pmatrix Q;
{
    int n_min,m_min;
    int level = 0;
    Value ALL;        /* le plus petit element sur la diagonale */
    Value x=VALUE_ZERO;          /* le rest de la division par ALL */
    int i;
    
    bool stop = false;
    bool next = true;

    /* precondition sur les parametres */
    int n = MATRIX_NB_LINES(MAT);
    int m = MATRIX_NB_COLUMNS(MAT);
    assert(m > 0 && n >0);
    matrix_assign(MAT,D);
    assert(value_one_p(MATRIX_DENOMINATOR(D)));

    matrix_identity(P,0);
    matrix_identity(Q,0);
 
    while (!stop) {
	matrix_min(D,&n_min,&m_min,level);
		
	if ((n_min == 0) && (m_min == 0))
	    stop = true;
	else {
	    /* le transformation n'est pas fini. */
	    if (n_min > level +1) {
		matrix_swap_rows(D,level+1,n_min);
		matrix_swap_rows(P,level+1,n_min);
	    }
#ifdef TRACE
	    (void) printf (" apres alignement du plus petit element a la premiere ligne \n");
	    matrix_print(D);
#endif
	    if (m_min >1+level) {
		matrix_swap_columns(D,level+1,m_min);
		matrix_swap_columns(Q,level+1,m_min);
	    }
#ifdef TRACE
	    (void) printf (" apres alignement du plus petit element a la premiere colonne\n");
	    matrix_print(D);
#endif
	   
	    ALL = SUB_MATRIX_ELEM(D,1,1,level);
	    if (matrix_line_el(D,level) != 0) 
		for (i=level+2; i<=m; i++) {
		    x = value_div(MATRIX_ELEM(D,level+1,i),ALL);
		    matrix_subtraction_column(D,i,level+1,x);
		    matrix_subtraction_column(Q,i,level+1,x);
		    next = false;
		}
	    if (matrix_col_el(D,level) != 0) 
		for(i=level+2;i<=n;i++) {
		    x = value_div(MATRIX_ELEM(D,i,level+1),ALL);
		    matrix_subtraction_line(D,i,level+1,x);
		    matrix_subtraction_line(P,i,level+1,x);
		    next = false;
		}
#ifdef TRACE
		(void) printf("apres division par A(%d,%d) des termes de la %d-ieme ligne et de la %d-em colonne \n",level+1,level+1,level+1,level+1);
		matrix_print(D);
#endif
	    if (next) level++;
	    next = true;
	}
    }

#ifdef TRACE
    (void) printf (" la  matrice D apres transformation est la suivante :");
    matrix_print(D);

    (void) printf (" la matrice P est \n");
    matrix_print(P);

    (void) printf (" la matrice Q est \n");
    matrix_print(Q);
#endif
}
	


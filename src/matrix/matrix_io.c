  /* package matrice */

#include <stdio.h>
#include <sys/stdtypes.h> /*for debug with dbmalloc */
#include <malloc.h>

#include "assert.h"

#include "boolean.h"
#include "arithmetique.h"

#include "matrix.h"

/* void matrix_fprint(File * f, matrice a,n,m): print an (nxm) rational matrix
 *
 * Note: the output format is incompatible with matrix_fscan()
 */
void matrix_fprint(f,a)
FILE * f;
Pmatrix	a;

{
    int	loop1,loop2;
    int n =MATRIX_NB_LINES(a);
    int m = MATRIX_NB_COLUMNS(a);
    assert(MATRIX_DENOMINATOR(a)!=0);

    (void) fprintf(f,"\n\n");

    for(loop1=1; loop1<=n; loop1++) {
	for(loop2=1; loop2<=m; loop2++)
	    matrix_pr_quot(f, MATRIX_ELEM(a,loop1,loop2), MATRIX_DENOMINATOR(a));
	(void) fprintf(f,"\n\n");
    }
    (void) fprintf(f," ......denominator = %d\n",MATRIX_DENOMINATOR(a));
}

/* void matrix_print(matrice a): print an (nxm) rational matrix
 *
 * Note: the output format is incompatible with matrix_fscan()
 *       this should be implemented as a macro, but it's a function for
 *       dbx's sake
 */
void matrix_print(a)
Pmatrix	a;
{
    matrix_fprint(stdout,a);
}

/* void matrix_fscan(FILE * f, matrice * a, int * n, int * m): 
 * read an (nxm) rational matrix in an ASCII file accessible 
 * via file descriptor f; a is allocated as a function of its number
 * of columns and rows, n and m.
 *
 * Format: 
 *
 * n m denominator a[1,1] a[1,2] ... a[1,m] a[2,1] ... a[2,m] ... a[n,m]
 *
 * After the two dimensions and the global denominator, the matrix as
 * usually displayed, line by line. Line feed can be used anywhere.
 * Example for a (2x3) integer matrix:
 *  2 3 
 *  1
 *  1 2 3
 *  4 5 6
 */
void matrix_fscan(f,a,n,m)
FILE * f;
Pmatrix  *a;
int * n;			/* column size */
int * m;			/* row size */
{
    int	r;
    int c;

    /* number of read elements */
    int n_read;

    /* read dimensions */
    n_read = fscanf(f,"%d%d", n, m);
    assert(n_read==2);
    assert(1 <= *n && 1 <= *m);

    /* allocate a */
    *a = matrix_new(*n,*m); 

    /* read denominator */
    n_read = fscanf(f," %d",&(MATRIX_DENOMINATOR(*a)));
    assert(n_read == 1);
    /* necessaires pour eviter les divisions par zero */
    assert(MATRIX_DENOMINATOR(*a)!=0);
    /* pour "normaliser" un peu les representations */
    assert(MATRIX_DENOMINATOR(*a) > 0);

    for(r = 1; r <= *n; r++) 
	for(c = 1; c <= *m; c++) {
	    n_read = fscanf(f," %d",&MATRIX_ELEM(*a,r,c));
	    assert(n_read == 1);
	}
}

/* void matrix_pr_quot(FILE * f, int a, int b): print quotient a/b in a sensible way,
 * i.e. add a space in front of positive number to compensate for the
 * minus sign of negative number
 *
 * FI: a quoi sert le parametre b? A quoi sert la variable d? =>ARGSUSED
 */
/*ARGSUSED*/
void matrix_pr_quot(f,a,b)
FILE * f;
int a;
int b;
{	
    if (a < 0) 
	(void) fprintf(f," %d ",a);
    else 
	(void) fprintf(f,"  %d ",a);
}

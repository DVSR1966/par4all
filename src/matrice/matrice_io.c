  /* package matrice */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include "assert.h"

#include "boolean.h"
#include "arithmetique.h"

#include "matrice.h"

/* void pr_quot(FILE * f, int a, int b): print quotient a/b in a sensible way,
 * i.e. add a space in front of positive number to compensate for the
 * minus sign of negative number
 *
 * FI: a quoi sert le parametre b? A quoi sert la variable d? =>ARGSUSED
 */
static void pr_quot(f,a,b)
FILE * f;
Value a;
Value b;
{
    if (value_pos_p(a))
	fprintf(f, " ");
    fprintf(f, " ");
    fprint_Value(f, a);
    fprintf(f, " ");
}

/* void matrice_fprint(File * f, matrice a,n,m): print an (nxm) rational matrix
 *
 * Note: the output format is incompatible with matrice_fscan()
 */
void matrice_fprint(f,a,n,m)
FILE * f;
matrice	a;
int	n;			/* column size */
int	m;			/* row size */
{
    int	loop1,loop2;

    assert(value_notzero_p(DENOMINATOR(a)));

    (void) fprintf(f,"\n\n");

    for(loop1=1; loop1<=n; loop1++) {
	for(loop2=1; loop2<=m; loop2++)
	    pr_quot(f, ACCESS(a,n,loop1,loop2), DENOMINATOR(a));
	(void) fprintf(f,"\n\n");
    }
    (void) fprintf(f," ......denominator = ");
    fprint_Value(f,DENOMINATOR(a));
    fprintf(f, "\n");
}

/* void matrice_print(matrice a, int n, int m): print an (nxm) rational matrix
 *
 * Note: the output format is incompatible with matrice_fscan()
 *       this should be implemented as a macro, but it's a function for
 *       dbx's sake
 */
void matrice_print(a,n,m)
matrice	a;
int	n;			/* column size */
int	m;			/* row size */
{
    matrice_fprint(stdout,a,n,m);
}

/* void matrice_fscan(FILE * f, matrice * a, int * n, int * m): 
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
void matrice_fscan(f,a,n,m)
FILE * f;
matrice	* a;
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
    *a = matrice_new(*n,*m); 

    /* read denominator */
    n_read = fscan_Value(f,&(DENOMINATOR(*a)));
    assert(n_read == 1);
    /* necessaires pour eviter les divisions par zero */
    assert(value_notzero_p(DENOMINATOR(*a)));
    /* pour "normaliser" un peu les representations */
    assert(value_pos_p(DENOMINATOR(*a)));

    for(r = 1; r <= *n; r++) 
	for(c = 1; c <= *m; c++) {
	    n_read = fscan_Value(f,&ACCESS(*a,*n,r,c));
	    assert(n_read == 1);
	}
}

/* package matrice */
#include <stdio.h>
/* #include <sys/stdtypes.h> */ /* for debug with dbmalloc */
/* #include "malloc.h" */

#include "genC.h"
#include "ri.h"
#include "misc.h"


#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "matrice.h"

#include "hyperplane.h"

/* void scanning_base_hyperplane(int h[],int n,matrice U)
 * compute the  matrix U :
 *   ->   ->  ->
 *   U1 = h  (U1  the first column of U)
 *   determinant(U) = +(/-)1.
 * 
 * Let G the scanning basis, the relation between U and T is
 *          -1 T   
 *    G = (U  )
 * the parameters of the  function are:
 *
 * int h[]   :  hyperplan direction  ---- input
 * int n     :  iteration dimension  ---- input
 * matrice G :  matrix  ---- output
 *             
 */
void scanning_base_hyperplane(h,n,G)
int h[];
int n;
matrice G;
{
    int k=1;

   
    if (h[0] == 0){ 
	/* search the first k / h[k]!=0 */
	while(k<n && h[k]==0)
	    k++;
	if (k==n) pips_error("scanning_base_hyperplane","h null");
	else{ /* permution de h[0] et h[k] */
	    h[0] = h[k];
	    h[k] = 0;
	    base_G_h1_unnull(h,n,G);
	    matrice_swap_rows(G,n,n,1,k+1);
	}
    }
    else 
	base_G_h1_unnull(h,n,G);    
}
 	    
	    
/*void base_G_h1_unnull(int h[],int n,matrice G)	
 */
void base_G_h1_unnull(h, n, G)	
int h[];
int n;
matrice G;
{
    matrice U = matrice_new(n,n);
    matrice G1 = matrice_new(n,n);
    int det_Ui = 0;
    int det_Ui1 = 0;			/* determinant of Ui and Ui-1*/
    int Xi,Yi;
    int i,j,r;
   
    /* computation of matrix U */
    assert(n>0);
    assert(h[0]!=0);
    /* initialisation */
    matrice_nulle(U,n,n);
    ACCESS(U,n,1,1) = h[0];
    det_Ui1 = h[0];

    for(i=2; i<=n; i++){
	/* computation of Xi,Yi / Xi.det(Ui-1) - Yi.hi  = GCD(det(Ui-1),hi) */
	det_Ui = bezout_grl(det_Ui1,h[i-1],&Xi,&Yi);
	if (i ==2 || i%2 != 0)
	    Yi = -Yi;
	/* make  Ui - the i-th line: U[i,1]=h[i-1],U[i,2..n-1] =0,U[i,n] = Xi */
	ACCESS(U,n,i,1) = h[i-1];
	ACCESS(U,n,i,i) = Xi;
        /*                                            ->i-1 */
	/* the i-th column:U[1..n-1] = Yi/det(Ui-1) . h      */
	for (j=1; j<=i-1; j++){
	    r = h[j-1]/det_Ui1;
	    ACCESS(U,n,j,i) = Yi * r;
	}
	det_Ui1 = det_Ui;
    }
    for (i=1; i<=n; i++)
        /*         ->         ->*/
	/* divide U1 par GCD(h) */
	ACCESS(U,n,i,1) /= det_Ui;

    /* computation of  matrix G */
    matrice_general_inversion(U,G1,n);
    matrice_transpose(G1,G,n,n);
}

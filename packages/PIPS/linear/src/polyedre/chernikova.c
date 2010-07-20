/*

  $Id$

  Copyright 1989-2010 MINES ParisTech

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "assert.h"
#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "sommet.h"
#include "ray_dte.h"
#include "sg.h"
#include "polyedre.h"


/* IRISA/POLYLIB data structures.
 */
#include "polylib/polylib.h"

/* maximum number of rays allowed in chernikova... (was 20000)
 * it does not look a good idea to move the limit up, as it
 * makes both time and memory consumption to grow a lot.
 */
#define MAX_NB_RAYS (20000)

/* Irisa is based on int. We would like to change this to 
 * some other type, say "long long" if desired, as VALUE may
 * also be changed. It is currently an int. Let us assume
 * that the future type will be be called "IRINT" (Irisa Int)
 */
/*
#define VALUE_TO_IRINT(val) VALUE_TO_INT(val)
#define IRINT_TO_VALUE(i) ((Value)i)
*/

#define VALUE_TO_IRINT(val) (val)
#define IRINT_TO_VALUE(i) (i)

/*****duong - set timeout with signal and alarm*****/
#include <signal.h>
#define SC_CONVEX_HULL_TIMEOUT 180 //3 minutes

boolean SC_CONVEX_HULL_timeout = FALSE;

void 
catch_alarm_SC_CONVEX_HULL (int sig  __attribute__ ((unused)))
{  
  fprintf(stderr,"CATCH ALARM sc_convex_hull !!!\n");
  //put inside CATCH(any_exception_error) alarm(0); //clear the alarm
  SC_CONVEX_HULL_timeout = TRUE;

  THROW(timeout_error);
}


static void my_Matrix_Free(Matrix * volatile * m)
{
  ifscdebug(9) {
    fprintf(stderr, "[my_Matrix_Free] in %p\n", *m);
  }

  Matrix_Free(*m);
  *m = NULL;

  ifscdebug(9) {
    fprintf(stderr, "[my_Matrix_Free] out %p\n", *m);
  }
}

static void my_Polyhedron_Free(Polyhedron * volatile * p)
{
  ifscdebug(9) {
    fprintf(stderr, "[my_Polyhedron_Free] in %p\n", *p);
  }

  Polyhedron_Free(*p);
  *p = NULL;

  ifscdebug(9) {
    fprintf(stderr, "[my_Polyhedron_Free] out %p\n", *p);
  }  
}

/* Fonctions de conversion traduisant une ligne de la structure 
 * Matrix de l'IRISA en un Pvecteur
 */
static Pvecteur matrix_ligne_to_vecteur(mat,i,base,dim)
Matrix *mat;
int i;
Pbase base;
int dim;
{
    int j;
    Pvecteur pv,pvnew = NULL;
    boolean NEWPV = TRUE;

    for (j=1,pv=base ;j<dim;j++,pv=pv->succ) {
      if (mat->p[i][j]) {
	if (NEWPV) { 
	  pvnew= vect_new(vecteur_var(pv),
			  IRINT_TO_VALUE(mat->p[i][j]));
	  NEWPV =FALSE; 
	}		
	else 
	  vect_add_elem(&pvnew,vecteur_var(pv),
			IRINT_TO_VALUE(mat->p[i][j]));
      }
    }
    return pvnew;
}

/* Fonctions de conversion traduisant une ligne de la structure 
 * Matrix de l'IRISA en une Pcontrainte
 */
static Pcontrainte matrix_ligne_to_contrainte(mat,i,base)
Matrix * mat;
int i;
Pbase base;
{
    Pcontrainte pc=NULL;
    int dim = vect_size(base) +1;

    Pvecteur pvnew = matrix_ligne_to_vecteur(mat,i,base,dim);
    vect_add_elem(&pvnew,TCST,IRINT_TO_VALUE(mat->p[i][dim]));
    vect_chg_sgn(pvnew);
    pc = contrainte_make(pvnew);
    return (pc);
}


/*  Fonctions de conversion traduisant une ligne de la structure 
 *  Polyhedron de l'IRISA en un Pvecteur
 */
static Pvecteur polyhedron_ligne_to_vecteur(pol,i,base,dim)
Polyhedron *pol;
int i;
Pbase base;
int dim;
{
    int j;
    Pvecteur pv,pvnew = NULL;
    boolean NEWPV = TRUE;

    for (j=1,pv=base ;j<dim;j++,pv=pv->succ) {
      if (pol->Ray[i][j]) {
	    if (NEWPV) { pvnew= vect_new(vecteur_var(pv),
					 IRINT_TO_VALUE(pol->Ray[i][j]));
			 NEWPV =FALSE;  }		
	    else vect_add_elem(&pvnew,vecteur_var(pv),
			       IRINT_TO_VALUE(pol->Ray[i][j]));
      }
    }
    return pvnew;
}

/* Fonctions de conversion traduisant une Pray_dte en une 
 * ligne de la structure matrix de l'IRISA 
 */ 
static void ray_to_matrix_ligne(pr,mat,i,base)
Pray_dte pr;
Matrix *mat;
int i;
Pbase base;
{
    Pvecteur pb;
    unsigned int j;
    Value v;

    for (pb = base, j=1; 
	 !VECTEUR_NUL_P(pb) && j<mat->NbColumns-1; 
	 pb = pb->succ,j++)
    {
	v = vect_coeff(vecteur_var(pb),pr->vecteur);
	mat->p[i][j] = VALUE_TO_IRINT(v);
    }
}


/* Fonctions de conversion traduisant une Pcontrainte en une 
 * ligne de la structure matrix de l'IRISA 
 */
static void contrainte_to_matrix_ligne(pc,mat,i,base)
Pcontrainte pc;
Matrix *mat;
int i;
Pbase base;
{
    Pvecteur pv;
    int j;
    Value v;

    for (pv=base,j=1; !VECTEUR_NUL_P(pv); pv=pv->succ,j++)
    {
	v = value_uminus(vect_coeff(vecteur_var(pv),pc->vecteur));
	mat->p[i][j] = VALUE_TO_IRINT(v);
    }

    v = value_uminus(vect_coeff(TCST,pc->vecteur));
    mat->p[i][j]= VALUE_TO_IRINT(v);
}

/* Passage du systeme lineaire sc a une matrice matrix (structure Irisa)
 * Cette fonction de conversion est utilisee par la fonction 
 * sc_to_sg_chernikova
 */ 
static void sc_to_matrix(Psysteme sc, Matrix *mat)
{
    int nbrows, nbcolumns, i, j;
    Pcontrainte peq;
    Pvecteur pv;

    nbrows = mat->NbRows;
    nbcolumns = mat->NbColumns;

    /* Differentiation equations and inequations */
    for (i=0; i<=sc->nb_eq-1;i++)
	mat->p[i][0] =0;
    for (; i<=nbrows-1;i++)
	mat->p[i][0] =1;

    /* Matrix initialisation */
    for (peq = sc->egalites,i=0;
	 !CONTRAINTE_UNDEFINED_P(peq);
	 contrainte_to_matrix_ligne(peq,mat,i,sc->base),
	 peq=peq->succ,i++);
    for (peq =sc->inegalites;
	 !CONTRAINTE_UNDEFINED_P(peq);
	 contrainte_to_matrix_ligne(peq,mat,i,sc->base),
	 peq=peq->succ,i++);

    for (pv=sc->base,j=1;!VECTEUR_NUL_P(pv);
	 mat->p[i][j] = 0,pv=pv->succ,j++);
    mat->p[i][j]=1;
}

/* Fonction de conversion traduisant un systeme generateur sg 
 * en une matrice de droites, rayons et sommets utilise'e par la 
 * structure  de Polyhedron de l'Irisa.
 * Cette fonction de conversion est utilisee par la fonction 
 * sg_to_sc_chernikova
 */ 
static void sg_to_polyhedron(Ptsg sg, Matrix * mat)
{
    Pray_dte pr;
    Psommet ps;
    int  nbC=0;
    int nbcolumns = mat->NbColumns;

    /* Traitement des droites */
    if (sg_nbre_droites(sg)) {
	pr = sg->dtes_sg.vsg;
	for (pr = sg->dtes_sg.vsg; pr!= NULL; 
	     mat->p[nbC][0] = 0, 
	     mat->p[nbC][nbcolumns-1] =0,
	     ray_to_matrix_ligne(pr,mat,nbC,sg->base),
	     nbC++,
	     pr = pr->succ); 
    } 
    /* Traitement des rayons */
    if (sg_nbre_rayons(sg)) {
	pr =sg->rays_sg.vsg;
	for (pr = sg->rays_sg.vsg; pr!= NULL; 
	     mat->p[nbC][0] = 1, 
	     mat->p[nbC][nbcolumns-1] =0,
	     ray_to_matrix_ligne(pr,mat,nbC,sg->base),
	     nbC++,
	     pr = pr->succ);
    } 
    /* Traitement des sommets */
    if (sg_nbre_sommets(sg)) {
	for (ps = sg->soms_sg.ssg; ps!= NULL; 
	     mat->p[nbC][0] = 1, 
	     mat->p[nbC][nbcolumns-1] = VALUE_TO_IRINT(ps->denominateur),
	     ray_to_matrix_ligne(ps,mat,nbC,sg->base),
	     nbC++,
	     ps = ps->succ);
    }
}

/* Fonction de conversion traduisant une matrix structure Irisa 
 * sous forme d'un syste`me lineaire sc. Cette fonction est 
 * utilisee paar la fonction sg_to_sc_chernikova 
 */ 
static void matrix_to_sc(Matrix *mat, Psysteme sc)
{
    Pcontrainte pce=NULL;
    Pcontrainte pci=NULL;
    Pcontrainte pc_tmp=NULL;
    boolean neweq = TRUE;
    boolean newineq = TRUE;
    int i,nbrows;
    int nbeq=0;
    int nbineq=0;

    /* Premiere droite */
    if ((nbrows= mat->NbRows)) {
	for (i=0; i<nbrows; i++) {
	    switch (mat->p[i][0]) {
	    case 0:
		nbeq ++;
		if (neweq) { 
		    pce= pc_tmp  = 
			matrix_ligne_to_contrainte(mat, i, sc->base);
		    neweq = FALSE;} 
		else {
		    pc_tmp->succ = 
			matrix_ligne_to_contrainte(mat, i, sc->base);
		    pc_tmp = pc_tmp->succ;	}
		break;
	    case 1:
		nbineq++;
		if (newineq) {pci = pc_tmp =
				  matrix_ligne_to_contrainte(mat,
							     i,sc->base); 
				  newineq = FALSE; } 
		else {
		    pc_tmp->succ = matrix_ligne_to_contrainte(mat,
							      i,sc->base);
		    pc_tmp = pc_tmp->succ;}
		break;
	    default:
		printf("error in matrix interpretation in Matrix_to_sc\n");
		break;
	    }
	}
	sc->nb_eq = nbeq;
	sc->egalites = pce;
	sc->nb_ineq = nbineq;
	sc->inegalites = pci;
    }
}



/* Fonction de conversion traduisant un polyhedron structure Irisa 
 * sous forme d'un syste`me ge'ne'rateur. Cette fonction est 
 * utilisee paar la fonction sc_to_sg_chernikova 
 */ 
static void polyhedron_to_sg(Polyhedron  *pol, Ptsg sg)
{
    Pray_dte ldtes_tmp=NULL,ldtes = NULL;
    Pray_dte lray_tmp=NULL,lray = NULL;
    Psommet lsommet_tmp=NULL,lsommet=NULL;
    Stsg_vects dtes,rays;
    Stsg_soms sommets;
    Pvecteur pvnew;
    unsigned int i;
    int nbsommets =0;
    int  nbrays=0;
    int dim = vect_size(sg->base) +1;
    boolean newsommet = TRUE;
    boolean newray = TRUE;
    boolean newdte = TRUE;

    for (i=0; i< pol->NbRays; i++) {
	switch (pol->Ray[i][0]) {
	case 0:
	    /* Premiere droite */
	    pvnew = polyhedron_ligne_to_vecteur(pol,i,sg->base,dim);
	    if (newdte) {
		ldtes_tmp= ldtes = ray_dte_make(pvnew);
		newdte = FALSE; 
	    } else {
		/* Pour chaque droite suivante */
		ldtes_tmp->succ = ray_dte_make(pvnew);
		ldtes_tmp =ldtes_tmp->succ;
	    }
	    break;
	case 1:
	    switch (pol->Ray[i][dim]) {
	    case 0:
		nbrays++;
		/* premier rayon */
		pvnew = polyhedron_ligne_to_vecteur(pol,i,sg->base,dim);
		if (newray) {
		    lray_tmp = lray = ray_dte_make(pvnew);
		    newray = FALSE;
		} else {
		    lray_tmp->succ= ray_dte_make(pvnew);
		    lray_tmp =lray_tmp->succ;    }
		break;
	    default:
		nbsommets ++;
		pvnew = polyhedron_ligne_to_vecteur(pol,i,sg->base,dim);
		if (newsommet) {
		    lsommet_tmp=lsommet=
			sommet_make(IRINT_TO_VALUE(pol->Ray[i][dim]),
				    pvnew);
		    newsommet = FALSE;
		} else {
		    lsommet_tmp->succ=
			sommet_make(IRINT_TO_VALUE(pol->Ray[i][dim]),
				    pvnew);
		    lsommet_tmp = lsommet_tmp->succ;
		}
		break;
	    }
	    break;

	default: printf("error in rays elements \n");
	    break;
	}
    }
    if (nbsommets) {
	sommets.nb_s = nbsommets;
	sommets.ssg = lsommet;
	sg->soms_sg = sommets;  }
    if (nbrays) {
	rays.nb_v = nbrays;
	rays.vsg=lray;
	sg->rays_sg = rays;    }
    if (pol->NbBid) {
	dtes.vsg=ldtes;
	dtes.nb_v=pol->NbBid;
	sg->dtes_sg = dtes;
    }
}

/* Fonction de conversion d'un systeme lineaire a un systeme generateur
 * par chenikova
 */
Ptsg  sc_to_sg_chernikova(Psysteme sc)
{
    /* Automatic variables read in a CATCH block need to be declared volatile as
     * specified by the documentation*/
    Polyhedron * volatile A = NULL;
    Matrix * volatile a = NULL;
    Ptsg volatile sg = NULL;

    int nbrows = 0;
    int nbcolumns = 0;

    CATCH(any_exception_error)
    {
      if (A) my_Polyhedron_Free(&A);
      if (a) my_Matrix_Free(&a);
      if (sg) sg_rm(sg);

      RETHROW();
    }
    TRY 
    {
      assert(!SC_UNDEFINED_P(sc) && (sc_dimension(sc) != 0));

      sg = sg_new();
      nbrows = sc->nb_eq + sc->nb_ineq + 1;
      nbcolumns = sc->dimension +2;
      sg->base = base_dup(sc->base);
      //replace base_dup, which changes the pointer given as parameter
      a = Matrix_Alloc(nbrows, nbcolumns);
      sc_to_matrix(sc,a);
      
      A = Constraints2Polyhedron(a, MAX_NB_RAYS);
      my_Matrix_Free(&a);
      
      polyhedron_to_sg(A,sg);
      my_Polyhedron_Free(&A);
    } /* end TRY */

    UNCATCH(any_exception_error);

    return sg;
}

/* Fonction de conversion d'un systeme generateur a un systeme lineaire.
 * par chernikova
 */
Psysteme sg_to_sc_chernikova(Ptsg sg)
{
    int nbrows = sg_nbre_droites(sg)+ sg_nbre_rayons(sg)+sg_nbre_sommets(sg);
    int nbcolumns = base_dimension(sg->base)+2;
    /* Automatic variables read in a CATCH block need to be declared volatile as
     * specified by the documentation*/
    Matrix * volatile a = NULL;
    Psysteme volatile sc = NULL;
    Polyhedron * volatile A = NULL;

    CATCH(any_exception_error)
    {
      if (sc) sc_rm(sc);
      if (a) my_Matrix_Free(&a);
      if (A) my_Polyhedron_Free(&A);

      RETHROW();
    }
    TRY
    {

    sc = sc_new();
    sc->base = base_dup(sg->base);
    //replace base_dup, which changes the pointer given as parameter
    sc->dimension = vect_size(sc->base);

    if (sg_nbre_droites(sg)+sg_nbre_rayons(sg)+sg_nbre_sommets(sg)) {

	a = Matrix_Alloc(nbrows, nbcolumns);
	sg_to_polyhedron(sg,a);	 
   
	A = Rays2Polyhedron(a, MAX_NB_RAYS);
	my_Matrix_Free(&a); 

	a = Polyhedron2Constraints(A);
	my_Polyhedron_Free(&A);

	matrix_to_sc(a,sc);
	my_Matrix_Free(&a);

	sc=sc_normalize(sc);

	if (sc == NULL) {
	    Pcontrainte pc = contrainte_make(vect_new(TCST, VALUE_ONE));
	    sc = sc_make(pc, CONTRAINTE_UNDEFINED);
	    sc->base = base_dup(sg->base);
	    //replace base_dup, which changes the pointer given as parameter
	    sc->dimension = vect_size(sc->base);	}

    }
    else {
	sc->egalites = contrainte_make(vect_new(TCST,VALUE_ONE));
	sc->nb_eq ++;
    }	
    } /* end TRY */

    UNCATCH(any_exception_error);

    return sc;
}


Psysteme sc_convex_hull(Psysteme sc1, Psysteme sc2)
{
    int nbrows1 = 0;
    int nbcolumns1 = 0;
    int nbrows2 = 0;
    int nbcolumns2 = 0;

    /* Automatic variables read in a CATCH block need to be declared volatile as
     * specified by the documentation*/
    Matrix * volatile a1 = NULL;
    Matrix * volatile a2 = NULL;
    Polyhedron * volatile A1 = NULL;
    Polyhedron * volatile A2 = NULL;
    Matrix * volatile a = NULL;
    Psysteme volatile sc = NULL;
    Polyhedron * volatile A = NULL;

    unsigned int i1, i2;
    int j;
    int Dimension,cp;

    CATCH(any_exception_error)
    {

      if (a) my_Matrix_Free(&a);
      if (a1) my_Matrix_Free(&a1);
      if (a2) my_Matrix_Free(&a2);
      if (A) my_Polyhedron_Free(&A);
      if (A1) my_Polyhedron_Free(&A1);
      if (A2) my_Polyhedron_Free(&A2);
      if (sc) sc_rm(sc);
      
      alarm(0);//clear the alarm. 
      //There's maybe exceptions rethrown by polylib. 
      //So clear alarm in catch_alarm_sc_convex_hull is not enough
      
      if (SC_CONVEX_HULL_timeout) {

	SC_CONVEX_HULL_timeout = FALSE;

	//fprintf(stderr,"\n *** * *** Timeout from polyedre/chernikova : sc_convex_hull !!! \n");
	
	//We can change to print to stderr by using sc_default_dump(sc). duong

	/* need to install sc before to run, because of Production/Include/sc.h
        ifscprint(4) {
	char * filename;
	char * label;
	  // if  print to stderr
	  //fprintf(stderr, "Timeout [sc_convex_hull] considering:\n");
	  //sc_default_dump(sc1) 
	  //sc_default_dump(sc2)	  
	  filename = "convex_hull_fail_sc_dump.out";
	  label = "LABEL - Timeout with sc_convex_hull considering: *** * *** SC ";
	  sc_default_dump_to_file(sc1,label,1,filename);
	  label = "                                                 *** * *** SC ";
	  sc_default_dump_to_file(sc2,label,2,filename);	
	}
	*/
      }

      //fprintf(stderr,"\nThis is an exception rethrown from sc_convex_hull(): \n ");
      RETHROW();
    }
    TRY 
    {

    assert(!SC_UNDEFINED_P(sc1) && (sc_dimension(sc1) != 0));
    assert(!SC_UNDEFINED_P(sc2) && (sc_dimension(sc2) != 0));

   

    //start the alarm
    signal(SIGALRM, catch_alarm_SC_CONVEX_HULL);   
    alarm(SC_CONVEX_HULL_TIMEOUT);

    ifscdebug(7) {
	fprintf(stderr, "[sc_convex_hull] considering:\n");
	sc_default_dump(sc1);
	sc_default_dump(sc2);
    }

    /* comme on il ne faut pas que les structures irisa 
       apparaissent dans le fichier polyedre.h, une sous-routine 
       renvoyant un polyhedron n'est pas envisageable.
       Le code est duplique */

    nbrows1 = sc1->nb_eq + sc1->nb_ineq + 1;
    nbcolumns1 = sc1->dimension +2;
    a1 = Matrix_Alloc(nbrows1, nbcolumns1);
    sc_to_matrix(sc1,a1);

    nbrows2 = sc2->nb_eq + sc2->nb_ineq + 1;
    nbcolumns2 = sc2->dimension +2;
    a2 = Matrix_Alloc(nbrows2, nbcolumns2);
    sc_to_matrix(sc2,a2);
   
    ifscdebug(8) {
	fprintf(stderr, "[sc_convex_hull]\na1 =");
	Matrix_Print(stderr, "%4d",a1);
	fprintf(stderr, "\na2 =");
	Matrix_Print(stderr, "%4d",a2);
    }

    A1 = Constraints2Polyhedron(a1, MAX_NB_RAYS);
   
    my_Matrix_Free(&a1); 

    A2 = Constraints2Polyhedron(a2, MAX_NB_RAYS);
      
    my_Matrix_Free(&a2);

    ifscdebug(8) {
	fprintf(stderr, "[sc_convex_hull]\nA1 (%p %p)=", A1, a1);
	Polyhedron_Print(stderr, "%4d",A1);
	fprintf(stderr, "\nA2 (%p %p) =", A2, a2);
	Polyhedron_Print(stderr, "%4d",A2);
    }
    
    sc = sc_new();
    sc->base = base_dup(sc1->base);
    //replace base_dup, which changes the pointer given as parameter
    sc->dimension = vect_size(sc->base);

    if (A1->NbRays == 0) {
	a= Polyhedron2Constraints(A2); 
    } else  if (A2->NbRays == 0) {
	a= Polyhedron2Constraints(A1); 
    } else {
	int i1p;
	int cpp;

	Dimension = A1->Dimension+2;
	a = Matrix_Alloc(A1->NbRays + A2->NbRays,Dimension);

	/* Tri des contraintes de A1->Ray et A2->Ray, pour former 
	   l'union de ces contraintes dans un meme format 
	   Line , Ray , vertex */
	cp = 0;
	i1 = 0;
	i2 = 0;
	while (i1 < A1->NbRays && A1->Ray[i1][0] ==0) {
	    for (j=0; j < Dimension ; j++)
		a->p[cp][j] = A1->Ray[i1][j]; 
	    cp++; i1++; 
	}

	while (i2 < A2->NbRays && A2->Ray[i2][0] ==0) {
	    for (j=0 ; j < Dimension ; j++) 
		a->p[cp][j] = A2->Ray[i2][j];
	    cp++; i2++;
	}

	i1p = i1;
	cpp = cp;
	while (i1 < A1->NbRays && A1->Ray[i1][0] ==1 
	       && A1->Ray[i1][Dimension-1]==0) {
	    for (j=0; j < Dimension ; j++) 
		a->p[cp][j] = A1->Ray[i1][j]; 
	    cp++; i1++; 
	}

	while (i2 < A2->NbRays && A2->Ray[i2][0] == 1 
	       && A2->Ray[i2][Dimension-1]==0) {
	    for (j=0; j < Dimension ; j++) 
		a->p[cp][j] = A2->Ray[i2][j]; 
	    cp++; i2++;
	}

	i1p = i1;
	cpp = cp;
	while (i1 < A1->NbRays && A1->Ray[i1][0] == 1 
	       && A1->Ray[i1][Dimension-1]!= 0) {
	    for (j=0; j < Dimension ; j++)  
		a->p[cp][j] = A1->Ray[i1][j]; 
	    cp++; i1++; 
	}

	while (i2 < A2->NbRays && A2->Ray[i2][0] == 1 
	       && A2->Ray[i2][Dimension-1]!=0) {
	    for (j=0; j < Dimension ; j++)  
		a->p[cp][j] = A2->Ray[i2][j]; 
	    cp++;  i2++;
	}
  
	my_Polyhedron_Free(&A1);
	my_Polyhedron_Free(&A2);
	
	A = Rays2Polyhedron(a, MAX_NB_RAYS);
	my_Matrix_Free(&a);

	a = Polyhedron2Constraints(A);    
	my_Polyhedron_Free(&A);
    }

    matrix_to_sc(a,sc);
    my_Matrix_Free(&a);

    sc = sc_normalize(sc);

    if (sc == NULL) {
	Pcontrainte pc = contrainte_make(vect_new(TCST, VALUE_ONE));
	sc = sc_make(pc, CONTRAINTE_UNDEFINED);
	sc->base = base_dup(sc1->base);
	//replace base_dup, which changes the pointer given as parameter
	sc->dimension = vect_size(sc->base);
    }

    } /* end TRY */
    
    alarm(0); //clear the alarm

    UNCATCH(any_exception_error);

    return sc;
}

struct pehrhart {
    Enumeration *e;
};
/* enumerate the systeme sc using base pb
 * pb contains the unknow variables
 * and sc all the constraints
 * ordered_sc must be order as follows:
 * elements from ordered_base appear last
 * variable_names can be provided for debugging purpose (name of bases) or set to NULL
 */ 
Pehrhart sc_enumerate(Psysteme ordered_sc, Pbase ordered_base, const char* variable_names[])
{
    /* Automatic variables read in a CATCH block need to be declared volatile as
     * specified by the documentation*/
    Polyhedron * volatile A = NULL;
    Matrix * volatile a = NULL;
    Polyhedron * volatile C = NULL;
    Enumeration * volatile ehrhart = NULL;

    int nbrows = 0;
    int nbcolumns = 0;

    CATCH(any_exception_error)
    {
      if (A) my_Polyhedron_Free(&A);
      if (a) my_Matrix_Free(&a);
      if (C) my_Polyhedron_Free(&C);

      RETHROW();
    }
    TRY 
    {
        assert(!SC_UNDEFINED_P(ordered_sc) && (sc_dimension(ordered_sc) != 0));
        nbrows = ordered_sc->nb_eq + ordered_sc->nb_ineq+1;
        nbcolumns = ordered_sc->dimension +2;
        a = Matrix_Alloc(nbrows, nbcolumns);
        sc_to_matrix(ordered_sc,a);

        A = Constraints2Polyhedron(a, MAX_NB_RAYS);
        my_Matrix_Free(&a);

        C = Universe_Polyhedron(base_dimension(ordered_base));

        ehrhart = Polyhedron_Enumerate(A,C, MAX_NB_RAYS,variable_names);
        /*
        Value vals[2]= {0,0};
        Value *val = compute_poly(ehrhart,&vals[0]);
        printf("%lld\n",*val);*/
        my_Polyhedron_Free(&A);
        my_Polyhedron_Free(&C);
    } /* end TRY */

    UNCATCH(any_exception_error);

    {
        Pehrhart p;
        p=malloc(sizeof(*p));
        p->e=ehrhart;
        return p;
    }
}

/* evaluate the ehrhart polynomial p using values as context
 * returned value is allocated
 */
Value* Pehrhart_evaluate(Pehrhart p, Value values[])
{
    return compute_poly(p->e,values);
}

/* generate a pretty printed version of the polynomial p,
 * suitable for interaction with third party tools
 * pname is the name of each basis
 *
 * the implementation could use open_memstream,
 * but it is not portable and not handled by gnulib ...
 */
char ** Pehrhart_string(Pehrhart p, const char *pname[])
{
    Enumeration * curr = p->e,*iter=p->e;
    size_t nb_enum = 0,i;
    char ** enum_strings;
    /* count the number of domains */
    while(iter) {
        nb_enum++;
        iter=iter->next;
    }

    if(!nb_enum || !(enum_strings=malloc(sizeof(*enum_strings)*(1+nb_enum))))
        return NULL;
    else {
        char seed[]="pipsXXXXXX";
        int tmpfd;
        FILE * tmpf;
        tmpfd=mkstemp(seed);
        if(tmpfd==-1) {
            perror(strerror(errno));
            return NULL;
        }
        tmpf=fdopen(tmpfd,"r+");
        if(!tmpf) {
            close(tmpfd);
            perror(strerror(errno));
            return NULL;
        }

        for(i=0;i<nb_enum;i++)
        {
            char * generated_string,*iter;
            long pos;
            size_t generated_string_size;
            rewind(tmpf);
            print_evalue(tmpf,&curr->EP,pname);
            pos=ftell(tmpf);
            rewind(tmpf);
            generated_string=calloc(1+pos,sizeof(*generated_string));
            generated_string_size=fread(generated_string,sizeof(*generated_string),1+pos,tmpf);
            for(iter=generated_string;*iter;iter++)
                if(iter[0]=='\n' )
                    iter[0]=' ';
            enum_strings[i]=generated_string;

        }
        enum_strings[i]=NULL;
        if(fclose(tmpf)!=0)
            perror(strerror(errno));
        return enum_strings;
    }
}


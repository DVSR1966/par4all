#include <stdio.h>
#include "type.h"
#include "tab.h"
extern long int cross_product, limit;

extern Entier sol_pgcd();
extern void sol_new();
extern void sol_div();
extern void sol_forme();
extern void sol_val();
extern int llog();

Entier sol_mod(x, y)
Entier x, y;
{Entier r;
 r = x % y;
 if(r<0) r += y;
 return(r);
}

int non_borne(tp, nvar, D, bigparm)
Tableau *tp;
int nvar, bigparm;
Entier D;
{int i, ff;
 for(i = 0; i<nvar; i++)
     {ff = Flag(tp, i);
      if(bigparm > 0)
	 {if(ff & Unit)return(True);
          if(Index(tp, i, bigparm) != D) return(True);
	 }
      }
 return(False);
}

Tableau *expanser();

int integrer(ptp, pcontext, D, pnvar, pnparm, pni, pnc)
Tableau **ptp, **pcontext;
int *pnvar, *pnparm, *pni, *pnc;
int D;
{int ncol = *pnvar+*pnparm+1;
 int nligne = *pnvar + *pni;
 Entier coupure[MAXCOL];
 int i, j, k, ff;
 Entier x, d;
 int ok_var, ok_const, ok_parm;
 if(D == 1) return(-1); /* attention, en cas de succes, nligne augmente
                           l'echec ne peut donc etre marque par une valeur
			   superieure a nligne */
 if(ncol >= MAXCOL) {
      fprintf(stderr, "Trop de colonnes : %d\n", ncol);
      exit(3);
      }
 for(i = 0; i<*pnvar; i++)
     {
      ff = Flag(*ptp, i);
      if(ff & Unit)continue;
      ok_var = False;
      for(j = 0; j<*pnvar; j++)
          {x = coupure[j] = sol_mod(Index(*ptp, i, j), D);
	   if(x > 0) ok_var = True;
	  }
      if(! ok_var) continue;
      x = coupure[*pnvar] = - sol_mod(-Index(*ptp, i, *pnvar), D);
      ok_const = (x != 0);
      d = x;
      ok_parm = False;
      for(j = (*pnvar)+1; j<ncol; j++)
          {
	   x = coupure[j] = - sol_mod(- Index(*ptp, i, j), D);
	   d = sol_pgcd(d, x);
	   if(x != 0) ok_parm = True;
	  }
#if (DEBUG > 2)
      Q{printf("%ld/cut : %d %d #[", cross_product, *pnvar, *pnparm);
        for(j = 0; j<ncol; j++)printf(" %d",coupure[j]);
        printf("]/ %d\n", D);
       }
#endif
      if(ok_parm)
          {
           Entier discrp[MAXPARM], discrm[MAXPARM];
	   if(*pnparm >= MAXPARM) {
	       fprintf(stderr, "Trop de parametres : %d\n", *pnparm);
	       exit(4);
	       }
           d = sol_pgcd(d, D);
	   sol_new(*pnparm);
	   sol_div();
	   sol_forme(*pnparm+1);
	   for(j = 0; j<*pnparm; j++)sol_val(-coupure[j+*pnvar+1]/d, 1);
	   sol_val(-coupure[*pnvar]/d, 1);
	   sol_val(D/d, 1);
	   for(j = 0; j<*pnparm; j++)
	       {x = coupure[j+*pnvar+1];
		discrp[j] = -x;
		discrm[j] = x;
	       }
	   discrp[*pnparm] = -D;
	   discrm[*pnparm] = D;
	   x = coupure[*pnvar];
	   discrp[(*pnparm)+1] = -x;
	   discrm[(*pnparm)+1] = -x + D -1;
	   if((*pnc) +2 > (*pcontext)->height ||
	      (*pnparm) + 2 > (*pcontext)->width)
	       {int dcw, dch;
		dcw = llog(D);
		dch = 2 * dcw + *pni;
		Flag(*pcontext, *pnc) = 0;
		*pcontext = expanser(*pcontext, 0, *pnc, *pnparm+1, 0,
				     dch, dcw);
	       }
           (*pnparm)++;
	   for(k = 0; k < *pnc; k++)
	       {Index(*pcontext, k, *pnparm) =
			     Index(*pcontext, k, (*pnparm)-1);
		Index(*pcontext, k, (*pnparm)-1) = 0;
	       }
	   for(j = 0; j <= *pnparm; j++)
	       {Index(*pcontext, *pnc, j) = discrp[j];
		Index(*pcontext, *pnc + 1, j) = discrm[j];
	       }
	   Flag(*pcontext, *pnc) = Unknown;
	   Flag(*pcontext, *pnc+1) = Unknown;
	   (*pnc) += 2;
           coupure[ncol] = D;
	   break;
	  }
       else if(ok_const)break;
      }
 if(i >= *pnvar)return(-1);
 if(i < *pnvar)
      {/* y a t'il de la place ? */
       if(nligne >= (*ptp)->height || ncol >= (*ptp)->width)
           {int d, dth, dtw;
	    int llog();
            d = llog(D);
	    dth = d + (*pnparm? *pni : 0);
	    dtw = (*pnparm ? d : 0);
	    *ptp = expanser(*ptp, *pnvar, *pni, ncol, 0, dth, dtw);
	   }
       ncol = *pnvar + *pnparm + 1;
       Flag(*ptp, nligne) = Minus;
       for(j = 0; j<ncol; j++) Index(*ptp, nligne, j) = coupure[j];
       for(j = ncol; j < (*ptp)->width; j++) Index(*ptp, nligne, j) =0;
       (*pni)++;
      }
 return(nligne);
}


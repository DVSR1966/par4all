%{

#include <stdio.h>
#include <string.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include <sys/stdtypes.h>  /* for debug with dbmalloc */
#include "malloc.h"

extern char yytext[]; /* dialogue avec l'analyseur lexical */

Psysteme ps_yacc;

boolean syntax_error;

Value valcst;

short int fac;        /* facteur multiplicatif suivant qu'on analyse un terme*/
                      /* introduit par un moins (-1) ou par un plus (1) */

int sens;       /* indique le sens de l'inegalite
                         sens = -1  ==> l'operateur est soit > ,soit >=,
                         sens = 1   ==> l'operateur est soit <, soit <=   */
short int cote; /* booleen indiquant quel membre est en cours d'analyse*/

long int b1, b2; /* element du vecteur colonne du systeme donne par l'analyse*/
	               /* d'une contrainte */


Pcontrainte eq;   /* pointeur sur l'egalite ou l'inegalite
                                courante */
   
Pvecteur cp ;   /* pointeur sur le membre courant             */ 


short int operat;    /* dernier operateur rencontre                 */


/*code des operateurs de comparaison */

#define OPINF 1
#define OPINFEGAL 2
#define OPEGAL 3
#define OPSUPEGAL 4
#define OPSUP 5
#define DROIT 1
#define GAUCHE 2
#define NULL 0
%}

%token ACCFERM		/* accolade fermante */ 1
%token ACCOUVR		/* accolade ouvrante */ 2
%token CONSTANTE	/* constante entiere sans signe a recuperer dans yytext */ 3
%token EGAL		/* signe == */ 4
%token IDENT		/* identificateur de variable a recuperer dans yytext */ 5
%token INF		/* signe < */ 6
%token INFEGAL		/* signe <= */ 7
%token MOINS		/* signe - */ 8
%token PLUS		/* signe + */ 9
%token SUP		/* signe > */ 10
%token SUPEGAL		/* signe >= */ 11
%token VAR		/* mot reserve VAR introduisant la liste de variables */ 12
%token VIRG		/* signe , */ 13


%%
system	: inisys defvar ACCOUVR l_eq virg_opt ACCFERM
	; 

inisys	:
		{   /* initialisation des parametres du systeme */
                    /* et initialisation des variables */
                   
                       ps_yacc = sc_new();
		       init_globals();
		       syntax_error = FALSE;
                }
	;

defvar	: VAR l_var
                 {   /* remise en ordre des vecteurs de base */
		     Pbase b;
		     
		     b = ps_yacc->base;
		     ps_yacc->base = base_reversal(b);
		     vect_rm(b);
		 }
	;

l_var	: newid
	| l_var VIRG newid
	;

l_eq	: eq
	| l_eq VIRG eq
	|
	;

eq	: debeq multi_membre op membre fin_mult_membre feq
	;

debeq	:
		{       fac = 1;
                        sens = 1;
			cote = GAUCHE;
			b1 = 0;
                        b2 = 0;
                        operat = 0;
                        cp = NULL;
                        eq = contrainte_new();
        		}
	;

feq     :{ 

           contrainte_free(eq); 
        }
        ;

membre	: addop terme 
	| { fac = 1;} terme
	| membre addop terme
	;

terme	: const ident 
		{
			fac *=((cote == GAUCHE) ? 1 : -1);
                        /* ajout du couple (ident,const) a la contrainte 
                           courante                                     */
                        vect_add_elem(&(eq->vecteur),(Variable) $2,fac*$1);
                        /* duplication du couple (ident,const)
                           de la combinaison lineaire traitee           */ 
                        if (operat)
                            vect_add_elem(&cp,(Variable) $2,-fac*$1);
                }
	| const
		{
			b1 += ((cote == DROIT) ? fac*$1 : -fac*$1);
                        b2 += ((cote == DROIT) ? -fac*$1 : fac*$1);
                        }     
	| ident
		{
		        fac *= ((cote == GAUCHE) ? 1 :-1 );
                        /* ajout du couple (ident,1) a la contrainte courante */
                        vect_add_elem (&(eq->vecteur),(Variable) $1,fac);
                        /* duplication du couple (ident,1) de la
                           combinaison lineaire traitee                    */
                        if (operat)
				vect_add_elem(&cp,(Variable) $1,-fac);
		}
	;


ident	: IDENT
		{
		    $$ = (int) rec_ident(ps_yacc,yytext);
		}
	;

newid	: IDENT
		{
			new_ident(ps_yacc,yytext);
		}
	;

/* I'm pessimistic for long long here... 
 * should rather return a pointer to a Value stored somewhere...
 */
const	: CONSTANTE
		{ 
		    sscan_Value(yytext,&valcst);
		    $$ = valcst;
		}
	;

op	: INF
		{ cote = DROIT; 
                  sens = 1;
                  operat = OPINF;
                  cp = NULL;
                  b2 = 0; }
	| INFEGAL
		{ cote = DROIT; 
                  sens = 1;
                  operat = OPINFEGAL;
                  cp = NULL;
                  b2 = 0;}
	| EGAL
		{ cote = DROIT; 
                  sens = 1;
                  operat = OPEGAL; 
                  cp = NULL;
                  b2 = 0;
                 }
	| SUP
		{ cote = DROIT; 
                  sens = -1;
                  operat = OPSUP; 
                  cp = NULL;
                  b2 = 0;}
	| SUPEGAL
		{ cote = DROIT;	
                  sens = -1;
                  operat = OPSUPEGAL;
                  cp = NULL;
                  b2 = 0;}
	;

addop	: PLUS
		{ fac = 1; }
	| MOINS
		{ fac = -1; }
	;

multi_membre : membre
             | multi_membre op membre fin_mult_membre
             ;

fin_mult_membre :
                  {
                       vect_add_elem(&(eq->vecteur),TCST,-b1);
			switch (operat) 
                        {
			case OPINF:
                                creer_ineg(ps_yacc,eq,sens);
                                vect_add_elem(&(eq->vecteur),TCST,1);
				break;
			case OPINFEGAL:
                                creer_ineg(ps_yacc,eq,sens);
				break;
			case OPSUPEGAL:
                                creer_ineg(ps_yacc,eq,sens);
				break;
			case OPSUP:
				creer_ineg(ps_yacc,eq,sens);
                                vect_add_elem (&(eq->vecteur),TCST,1);
                                break;
			case OPEGAL:
                                creer_eg(ps_yacc,eq);
				break;
			}

                    eq = contrainte_new();
                    eq->vecteur = cp;
                    b1 = b2;

                  }
                ;

virg_opt : VIRG
         |
         ; 
%%

void yyerror(s)
char *s;
{
	/* procedure minimun de recouvrement d'erreurs */
	int c;

	(void) fprintf(stderr,"%s near %s\n",s,yytext);
	while ((c = getchar()) != EOF)
		putchar(c);

	syntax_error = TRUE;
}

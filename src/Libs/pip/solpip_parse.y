/* Name     : solpip_parse.y
 * Package  : paf-util
 * Author   : F. Lamour, F. Dumontet
 * Date     : 25 march 1993
 * Historic : 2 august 93, moved into (package) paf-util, AP
 * Documents:
 *
 * Comments :
 * Grammaire Yacc pour interpreter une solution fournie par PIP en un "quast
 * newgen".
 */
 
%{
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Newgen includes      */
#include "genC.h"

/* C3 includes 		*/
#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "ray_dte.h"
#include "sommet.h"
#include "sg.h"
#include "sc.h"
#include "polyedre.h"
#include "matrix.h"

/* Pips includes        */
#include "ri.h"
#include "constants.h"
#include "ri-util.h"
#include "misc.h"
#include "bootstrap.h"
#include "graph.h"
#include "paf_ri.h"
#include "paf-util.h"

extern void retire_par_de_pile(void);
extern void init_quast(void);
extern void creer_quast_value(void);
extern void creer_true_quast(void);
extern void creer_predicat(void);
extern void fait_quast(void);
extern void fait_quast_value(void);
extern void init_liste_vecteur(void);
extern void init_vecteur(void);
extern void ecrit_coeff1(int /*ent*/);
extern void creer_Psysteme(void);
extern void init_new_base(void);
extern void ajoute_new_var(int /*ent*/, int /*rang*/);
extern void ecrit_liste_vecteur(void);
extern void ecrit_une_var(int /*ent*/);
extern void ecrit_une_var_neg(int /*ent*/);
extern void ecrit_coeff_neg2(int /*ent*/);

%}

%union{
int valeur;
char identificateur;
char *blabla;
}

%start quast_sol
%token <valeur>  ENTIER
%token <blabla>  TEXTE
%token  LIST LPAR RPAR LCRO RCRO DIES IF NEWPARM DIV DIV_OP MOINS_OP
%type <valeur> coefficient

%left DIV_OP 
%right MOINS_OP


%% 


/* Les regles de grammaire definissant une solution de PIP */
 
 
quast_sol          : LPAR LPAR commentaire RPAR  
		   {
		   init_new_base ();
		   }
		    super_quast RPAR
                  ;

commentaire       : 
		   |
		    TEXTE
                  |  
		    commentaire TEXTE
		  |
		   commentaire LPAR commentaire RPAR commentaire
                  ;


super_quast       : LPAR 
		   {
		   init_quast();
		   }
		    quast RPAR
		   {
		   fait_quast();
		   }

                  | nouveau_parametre super_quast
		   {
		   retire_par_de_pile();
		   }
                  ;


quast             : forme
		   {
		   creer_quast_value ();
		   } 
                  | IF vecteur1 
		   {
		   creer_predicat();
		   }
		    super_quast 
		   {
		   creer_true_quast ();
		   }
		    super_quast
		   {
		   fait_quast_value ();
		   }
                  ;


forme             :     
                  | LIST 
		   {
		   init_liste_vecteur ();
		   }
		    liste_vecteur
                  ;
    

nouveau_parametre : NEWPARM ENTIER LPAR DIV vecteur2 ENTIER  RPAR RPAR
		   {
		   printf("nouveau_parametre1");
		   ajoute_new_var( $6 , $2 ); 
		   }
                  ;


liste_vecteur     : vecteur  
                  | liste_vecteur vecteur
                  ;


vecteur           : DIES LCRO 
		   {
		   init_vecteur ();
		   }
		    liste_coefficient RCRO 
		   {
		   ecrit_liste_vecteur();
		   }
                  ;
         
liste_coefficient : coefficient              
                  | liste_coefficient coefficient 
                  ;


coefficient       : MOINS_OP ENTIER    
		   {
		    ecrit_une_var_neg( $2 );
		   }
                  | ENTIER                            
		   {
		   ecrit_une_var( $1 ); 
		   }
                  ;

vecteur2          : DIES LCRO
                   {
		   init_vecteur ();
                   }
                    liste_coefficient2 RCRO
                  ;

liste_coefficient2: coefficient2
                  | liste_coefficient2 coefficient2
                  ;


coefficient2      : MOINS_OP ENTIER
                   {
		   ecrit_coeff_neg2 ( $2 );
                   }
                  | ENTIER
                  ;

vecteur1          : DIES LCRO 
		   {
		   creer_Psysteme();
		   }
		    liste_coefficient1 RCRO
                  ;

liste_coefficient1: coefficient1
                  | liste_coefficient1 coefficient1
                  ;


coefficient1      : MOINS_OP ENTIER
		   {
		   ecrit_coeff1 ( -$2 );
		   }	
                  | ENTIER
                   {
		   ecrit_coeff1 ( $1 );
                   }

		  ;

%% 
void yyerror(s)

	char*	s;
{
	fputs(s,stderr);
	putc('\n',stderr);
}

/*#include "lex.yy.c"*/   /* Le repertoire est au 25 / 03 / 93 : ~lamour/C/yac */
    

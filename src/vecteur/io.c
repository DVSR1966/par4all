 /*
  * LECTURE ET AFFICHAGE  ET DIMENSION D'UN VECTEUR
  *
  * Malik Imadache, Corinne Ancourt, Neil Butler, Francois Irigoin
  *
  * Modifications:
  *  - suppression de l'ancienne version de vect_print() qui faisait
  *    l'hypothese que le type Variable etait egal au type int; remplacement
  *    par un appel a vect_fprint(); (FI, 27/11/89)
  *  - ajout de vect_dump() qui utilise implicitement la fonction 
  *    variable_dump_name()
  */

/*LINTLIBRARY*/

#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>

#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"

/* Pvecteur vect_read(Pbase * b): lecture interactive d'un vecteur sur stdin;
 * la base b est modifiee de maniere a ce que chaque composante y figure bien;
 *
 * La representation d'un vecteur dont la troisieme composante vaut 2,
 * la dixieme 6 et les autres 0 peut etre:
 *   3 2 1 10 6 0
 * ou
 *   x3 2 1 x10 6 0
 * Le 1 veut dire qu'il faut continuer les lectures, le zero, qu'il faut
 * arreter
 *
 * Ce format est malheureusement incompatible avec celui de vect_fprint()
 */
Pvecteur vect_read(b)
Pbase * b;
{
    Pvecteur v1;
    Pvecteur v = VECTEUR_NUL;
    /* Let's assume that no variable name will be longer than 9 characters */
    static char buffer[10];
    /* flag de continuation */
    int c;

    c = 1;
    while (c == 1)  {
	Variable var;
	Value val;

	/* vect_chain() n'est pas utilise pour conserver l'ordre des couples */
	(void) printf ("valeur de variable :");
	(void) scanf("%9s",buffer);
	(void) printf ("valeur du coefficient de la variable :");
	(void) scan_Value(&val);
	if(!base_contains_variable_p(*b, (Variable) buffer)) {
	    var = variable_make(buffer);
	    *b = vect_add_variable(*b,var);
	}
	else {
	    var = base_find_variable(*b, (Variable) buffer);
	}
	v1 = vect_new(var, val);
	v1->succ = v;
	v = v1;
	(void) printf ("'1' -->rentrer d'autres valeurs,(0-2..9)"
		       "sinon. votre choix:");
	(void) scanf("%d",&c);
    }
    return(v);
}

/* void vect_fprint(FILE * f, Pvecteur v, char * (*variable_name)()):
 * impression d'un vecteur creux v sur le fichier f; le nom de chaque
 * coordonnee est donne par la fonction variable_name()
 *
 * Par exemple, le vecteur v
 *  ->     ->    ->  ->
 *  v == 2 i - 3 j + k
 * est imprime sous la forme
 *  2 * i - 3 * j + k LF
 * ou les symboles i, j et k sont obtenus via la fonction pointee par
 * variable_name()
 *
 * Le vecteur nul est represente par
 *      vecteur nul LF
 *
 * Note: attention au linefeed final
 *       il n'existe pas de fonction relisant des vecteurs sous cette forme
 *       (pour le moment...)
 *
 * Modifications:
 *  - suppression du cas special du terme constant
 *    Resultat : core dump (FC, 28/11/94)
 *    Fixed, (BC, 6/12/94)
 */
void vect_fprint(f, v, variable_name)
FILE * f;
Pvecteur v;
char * (*variable_name)();
{
    Pvecteur p;

    if(v==NULL) 
	(void) fprintf(f,"nul vector\n");
    else
	for (p = v; p != NULL; p = p->succ)
	{
	    if (p->var != TCST) {
		fprint_Value(f, p->val);
		(void) fprintf(f," * %s ", variable_name(p->var));
	    }
	    else {
		(void) fprint_Value(f, p->val);
		fprintf(f, " ");
	    }

	    if (p->succ != NULL) {
		(void) fprintf(f,"+ ");}
	    else {
		(void) fprintf(f,"\n");}
	}
}

/* void vect_fprint_as_dense(FILE * f, Pvecteur v, Pbase b):
 *
 * Par exemple, le vecteur v
 *  ->     ->    ->  ->
 *  v == 2 i - 3 j + k
 * est imprime sous la forme
 * ( 2 -3 1)
 * dans la base (i j k)
 *
 * No constant term TCST is expected.
 */
void vect_fprint_as_dense(f, v, b)
FILE * f;
Pvecteur v;
Pbase b;
{
    if(vect_in_basis_p(v, b)) {
	Pvecteur coord;

	fputc('(', f);

	for(coord = b; !VECTEUR_NUL_P(coord); coord = coord->succ) {
	    Variable var = vecteur_var(coord);

	    if(VARIABLE_DEFINED_P(var)) {
		fprint_Value(f, vect_coeff(var, v));
		if(VECTEUR_NUL_P(coord->succ)) {
		    fputc(')', f);
		}
		else {
		    fputc(',', f);
		}
	    }
	    else {
		/* I do not know what should be done for constant terms... */
		abort();
	    }
	}
    }
    else
	abort();
}

/* void vect_fprint_as_monome(FILE * f, Pvecteur v, Pbase b,
 *                            char * (*variable_name)(), char *mult_symbol):
 * impression d'un vecteur creux considere comme un monome sans coefficient.
 * Par ex.: le vecteur 2 * i - 3 * j + k est ecrit: "i^2 * j^(-3) * k".
 * Le nom de chaque variable est donne par la fonction variable_name().
 * L'ordre dans lequel sont ecrites les inconnues du monome est fixe par la base b.
 * Le symbole "multiplication" est passe dans (char *) mult_symbol: " * ", ".", "x", "", ...
 * le vecteur de base special TCST n'est pas affiche: seulement son coefficient.
 * Pas de \n a la fin de l'affichage.
 */
void vect_fprint_as_monome(f, v, b, variable_name, mult_symbol)
FILE * f;
Pvecteur v;
Pbase b;
char * (*variable_name)();
char *mult_symbol;
{
    char *s = vect_sprint_as_monome(v, b, variable_name, mult_symbol);

    fprintf(f, "%s", s);
    free(s);
}

/* char *vect_sprint_as_monome(Pvecteur v, Pbase b,
 *                              char * (*variable_name)(), char *mult_symbol):
 * Retourne dans une chaine le Pvecteur considere comme un monome sans coefficient.
 * (voir ci-dessus)
 */
char *vect_sprint_as_monome(v, b, variable_name, mult_symbol)
Pvecteur v;
Pbase b;
char * (*variable_name)();
char *mult_symbol;
{
    Pvecteur p;
    char t[99];
    char *r = t;
    char *s = NULL;

    if (VECTEUR_NUL_P(v))
	strcpy (&t[0], "0");
    else if (VECTEUR_NUL_P(b)) {
	/* si la base est vide: affiche comme ca vient */
	for (p = v; p != NULL; p = p->succ) {
	    if (value_one_p(p->val)) {
		(void) sprintf(r, "%s", variable_name(p->var));
		r = strchr(r, '\0');
	    }
	    else {
		(void) sprintf(r, "%s^", variable_name(p->var));
		r = strchr(r, '\0');
		sprint_Value(r, p->val);
		r = strchr(r, '\0');
	    }
	    if (p->succ != NULL) {
		(void) sprintf(r, "%s", mult_symbol);
		r = strchr(r, '\0');
	    }
	}
    }
    else {
	/* si la base n'est pas vide, affiche selon l'ordre 
	 * qu'elle definit */
	boolean first_var = TRUE;
	Value exp;
	for ( ; !VECTEUR_NUL_P(b); b = b->succ) {
	    exp = vect_coeff(b->var, v);
	    if(exp!=0) {
	    
		if (!first_var) {
		    (void) sprintf(r, "%s", mult_symbol);
		    r = strchr(r, '\0');
		}
		else 
		    first_var = FALSE;
	    
		if (value_pos_p(exp)) {
		    if (value_one_p(exp)) {
			(void) sprintf(r, "%s", variable_name(b->var));
			r = strchr(r, '\0');
		    }
		    else {
			(void) sprintf(r,"%s^", variable_name(b->var));
			r = strchr(r, '\0');
			sprint_Value(r, exp);
			r = strchr(r, '\0');
		    }
		}
		else /* exp < 0 */ {
		    /* inutile pour les polynomes */ 
		    first_var = FALSE;

		    (void) sprintf(r, "%s^(", variable_name(b->var));
		    r = strchr(r, '\0');
		    sprint_Value(r, exp);
		    r = strchr(r, '\0');
		    (void) sprintf(r, ")");
		    r = strchr(r, '\0');
		}
	    }
	}
    }
    s = (char*) strdup(t);
    assert(strlen(s)<99); /* (un peu tard:-) */
    return s;
}


/* void vect_dump(Pvecteur v): impression "physique" d'un vecteur creux 
 * sur stderr
 */
void vect_dump(v)
Pvecteur v;
{
    vect_fprint(stderr, v, variable_dump_name);
}

/* void vect_print(Pvecteur v, char * (*variable_name)()):
 * impression d'un vecteur creux v sur stdout; le nom de chaque
 * coordonnee est donne par la fonction variable_name(); voir vect_fprint()
 */
void vect_print(v,variable_name)
Pvecteur v;
char * (*variable_name)();
{
    vect_fprint(stdout, v, variable_name);
}

/* void vect_fdump(FILE * f, Pvecteur v): impression d'un vecteur creux
 * par vect_fprint() avec passage de la fonction variable_dump_name()
 */
void vect_fdump(f, v)
FILE * f;
Pvecteur v;
{
    vect_fprint(f, v, variable_dump_name);
}

/* void base_fprint(FILE * f, Pbase b, char * (*variable_name)()):
 * impression d'une base sur le fichier f; le nom de chaque
 * coordonnee est donne par la fonction variable_name()
 *
 * Par exemple, la base b
 *  ->     -> ->  ->
 *  b == ( i  j   k )
 * est imprime sous la forme
 *  i j k LF
 * ou les symboles i, j et k sont obtenus via la fonction pointee par
 * variable_name()
 *
 * Le base vide est represente par
 *      base vide LF
 *
 * Note: attention au linefeed final
 *       il n'existe pas de fonction relisant une base sous cette forme
 */
void base_fprint(f, b, variable_name)
FILE * f;
Pbase b;
char * (*variable_name)();
{
    if(VECTEUR_NUL_P(b)) 
	(void) fprintf(f,"base vide\n");
    else
	for ( ; !VECTEUR_NUL_P(b); b = b->succ) {
	    (void) fprintf(f,"%s ", variable_name(b->var));
	}
		(void) fprintf(f,"\n");
}

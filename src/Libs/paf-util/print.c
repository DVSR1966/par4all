
/* Name     : print.c
 * Package  : paf-util
 * Author   : Alexis Platonoff
 * Date     : july 1993
 *
 * Historic :
 * - 16 july 93, changes in paf_ri, AP
 * - 23 sept 93, change the name of this file, AP
 * - 17 nov 93, add of fprintf_indent() for a prettier imprime_quast(). AP
 *
 * Documents:
 * Comments : This file contains the functions used for printing the data
 * structures of paf_ri.
 */

/* Ansi includes 	*/
#include <stdio.h>

/* Newgen includes 	*/
#include "genC.h"
#include "list.h"

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
#include "matrice.h"
#include "matrix.h"

/* Pips includes 	*/
#include "boolean.h"
#include "ri.h"
#include "ri-util.h"
#include "arithmetique.h"
#include "constants.h"
#include "misc.h"
#include "graph.h"
#include "paf_ri.h"
#include "text.h"
#include "text-util.h"
#include "misc.h"
#include "paf-util.h"

extern int adg_number_to_ordering(int /*in_nb*/);

/* Macro functions  	*/
#define IS_INEG 0
#define IS_EG 1
#define IS_VEC 2

/* Global variables 	*/

/* Internal variables 	*/
static int quast_depth = 0;	/* Used in imprime_quast() for correct indentation */

/* Local defines */
typedef dfg_vertex_label vertex_label;
typedef dfg_arc_label arc_label;

/*============================================================================*/
/* static void pu_contrainte_fprint(FILE *fp, Pcontrainte c, int is_what,
 *				     char *(*variable_name)()):
 * 
 * prints in the file "fp" the constraint "c", of type equality, inequality or
 * vector according to the value of the integer argument "is_what", using the
 * function "variable_name" for the name of the variables.
 *
 * The function contrainte_fprint() exists, it is defined in contrainte.c (C3
 * library). We redefine this function because:
 *
 * 	1. we want the constant term in the left hand side
 * 	2. we want a third type of contrainte (vector) which does not print the
 *         inequality or equality symbol.
 *	3. we do not want a line feed at the end of the constraint
 *
 * We consider that CONTRAINTE_UNDEFINED => CONTRAINTE_NULLE
 *
 * Results for a constraint containing the following Pvecteur (2*I) (-J) (-4):
 *
 *   equality:		2 * I - J - 4 = 0
 *   inequality:	2 * I - J - 4 <= 0
 *   vector:		2 * I - J - 4
 *
 */
static void pu_contrainte_fprint(fp,c,is_what,variable_name)
FILE *fp;
Pcontrainte c;
int is_what;
char * (*variable_name)();
{
    Pvecteur v;
    short int debut = 1;
    long int constante = 0;

    if (!CONTRAINTE_UNDEFINED_P(c))
        v = contrainte_vecteur(c);
    else
        v = VECTEUR_NUL;

    if(!vect_check(v))
       pips_error("pu_contrainte_fprint", "Non coherent vector");

    while (!VECTEUR_NUL_P(v)) {
        if (v->var!=TCST) {
            char signe;
            long int coeff = v->val;

            if (coeff != 0) {
                if (coeff > 0)
                    signe = (debut) ? ' ' : '+';
                else {
                    signe = '-';
                    coeff = -coeff;
                };
                debut = 0;
                if (coeff == 1)
                    (void) fprintf(fp,"%c %s ", signe, variable_name(v->var));
		else
                    (void) fprintf(fp,"%c %ld %s ", signe, coeff,
				   variable_name(v->var));
            }
        }
        else
            /* on admet plusieurs occurences du terme constant!?! */
            constante += v->val;

        v = v->succ;
    }
    if (is_what == IS_INEG)
      {
       if (constante > 0)
          fprintf (fp,"+ %ld <= 0 ,", constante);
       else if (constante < 0)
          fprintf (fp,"- %ld <= 0 ,", -constante);
       else 
          fprintf (fp,"<= 0 ,");
      }
    else if(is_what == IS_EG)
      {
       if (constante > 0)
          fprintf (fp,"+ %ld == 0 ,", constante);
       else if (constante < 0)
          fprintf (fp,"- %ld == 0 ,", -constante);
       else 
          fprintf (fp,"== 0 ,");
      }
    else /* IS_VEC */
      {
       if (constante > 0)
          fprintf (fp,"+ %ld ,", constante);
       else if (constante < 0)
          fprintf (fp,"- %ld ,", -constante);
       else 
          fprintf (fp," ,");
      }
}

/*============================================================================*/
/* void pu_inegalite_fprint(FILE *fp, Pcontraint ineg,
 *			     char *(*variable_name)()):
 * Redefinition of inegalite_fprint(). See pu_contrainte_fprint() for details.
 */
void pu_inegalite_fprint(fp,ineg,variable_name)
FILE *fp;
Pcontrainte ineg;
char * (*variable_name)();
{
 pu_contrainte_fprint(fp,ineg,IS_INEG,variable_name);
}

/*============================================================================*/
/* void pu_egalite_fprint(FILE *fp, Pcontraint eg, char *(*variable_name)()):
 * Redefinition of egalite_fprint(). See pu_contrainte_fprint() for details.
 */
void pu_egalite_fprint(fp,eg,variable_name)
FILE *fp;
Pcontrainte eg;
char * (*variable_name)();
{
 pu_contrainte_fprint(fp,eg,IS_EG,variable_name);
}

/*============================================================================*/
/* void vecteur_fprint(FILE *fp, Pcontraint vec char *(*variable_name)()):
 * See pu_contrainte_fprint() for details.
 */
void vecteur_fprint(fp,vec,variable_name)
FILE *fp;
Pcontrainte vec;
char * (*variable_name)();
{
 pu_contrainte_fprint(fp,vec,IS_VEC,variable_name);
}

/*============================================================================*/
/* void fprint_dataflow(FILE *fp, int stmt, dataflow df, int count)
 *
 * Prints in the file "fp" the dataflow "df" with a sink statement "stmt".
 *
 * AP, oct 4th 1995: add the 4th arg "count" which is used for labelling
 * the instructions that are duplicated in the DFG due to a
 * disjunction. If count=0 then it is not printed.
 */
void fprint_dataflow(fp, stmt, df, count)
FILE *fp;
int stmt;
dataflow df;
int count;
{
 list trans_l = dataflow_transformation(df);
 reference ref = dataflow_reference(df);
 predicate gov_pred = dataflow_governing_pred(df);
 communication comm = dataflow_communication(df);

 if(count == 0)
   fprintf(fp
	   " ---Def-Use---> ins_%d:\n  Reference: %s\n  Transformation: [",
	   stmt, words_to_string(words_reference(ref)));
 else
   fprintf(fp
	   " ---Def-Use---> ins_%d_%d:\n  Reference: %s\n  Transformation: [",
	   stmt, count, words_to_string(words_reference(ref)));

 fprint_list_of_exp(fp, trans_l);
 fprintf(fp,"]\n");
 fprintf(fp,"  Governing predicate:\n");
 if(gov_pred != predicate_undefined)
    fprint_pred(fp, gov_pred);
 else
    fprintf(fp, " {nil} \n");

 if(comm == communication_undefined)
   {
    /*fprintf(fp, "\t\t Communication general \n");*/
   }
 else
   {
    predicate pred;

    pred = communication_broadcast(comm);
    if(pred != predicate_undefined)
      {
       fprintf(fp,"\t\t Broadcast vector(s):");
       fprint_sc_pvecteur(fp, (Psysteme) predicate_system(pred));
      }

    pred = communication_reduction(comm);
    if(pred != predicate_undefined)
      {
       fprintf(fp,"\t\t Reduction vector(s):");
       fprint_sc_pvecteur(fp, (Psysteme) predicate_system(pred));
      }

    pred = communication_shift(comm);
    if(pred != predicate_undefined)
      {
       fprintf(fp,"\t\t Shift vector(s):");
       fprint_sc_pvecteur(fp, (Psysteme) predicate_system(pred));
      }
   }
}


/*============================================================================*/
/* void fprint_list_of_exp(FILE *fp, list exp_l): prints in the file "fp"
 * the list of expression "exp_l". We separate the expressions with a colon
 * (","). We do not end the print with a line feed.
 */
void fprint_list_of_exp(fp, exp_l)
FILE *fp;
list exp_l;
{
 list aux_l;
 expression exp;

 for(aux_l = exp_l; aux_l != NIL; aux_l = CDR(aux_l))
   {
    exp = EXPRESSION(CAR(aux_l));
    fprintf(fp,"%s", words_to_string(words_expression(exp)));
    if(CDR(aux_l) != NIL)
       fprintf(fp,",");
   }
}

/*============================================================================*/
/* void fprint_pred(FILE *fp, predicate pred): prints in the file "fp" the
 * predicate "pred".
 */
void fprint_pred(fp, pred)
FILE *fp;
predicate pred;
{
 Psysteme ps = (Psysteme) predicate_system(pred);

 fprint_psysteme(fp, ps);
}

/*============================================================================*/
/* void fprint_psysteme(FILE *fp, Psysteme ps): prints in the file "fp" the
 * Psysteme "ps". Each constraint is printed either with pu_inegalite_fprint()
 * or pu_egalite_fprint(), both redefined above. See pu_contrainte_fprint()
 * for details.
 */
void fprint_psysteme(fp, ps)
FILE *fp;
Psysteme ps;
{
 Pcontrainte peq;

 if (ps != NULL) {
    fprintf(fp,"{\n");
    for (peq = ps->inegalites; peq!=NULL; peq=peq->succ) {
       	 pu_inegalite_fprint(fp,peq,entity_local_name);
	 fprintf(fp, "\n");
    }
    for (peq = ps->egalites; peq!=NULL; peq=peq->succ) {
	 pu_egalite_fprint(fp,peq,entity_local_name);
	 fprintf(fp, "\n");
    }
    fprintf(fp,"} \n");
   }
 else
    fprintf(fp," { nil }\n");
}

/*============================================================================*/
/* void fprint_sc_pvecteur(FILE *fp, Psysteme ps): prints in the file "fp"
 * the Psysteme "ps" as a list of vectors. Each constraint is printed with
 * vecteur_fprint() defined above. See pu_contrainte_fprint() for details.
 */
void fprint_sc_pvecteur(fp, ps)
FILE *fp;
Psysteme ps;
{
 Pcontrainte peq;

 if (ps != NULL) {
    fprintf(fp,"{ ");

    for (peq = ps->inegalites; peq!=NULL;
       	 vecteur_fprint(fp,peq,entity_local_name),peq=peq->succ);

    for (peq = ps->egalites; peq!=NULL;
	 vecteur_fprint(fp,peq,entity_local_name),peq=peq->succ);

    fprintf(fp," } \n");
   }
 else
    fprintf(fp,"(nil)\n");
}


/*============================================================================*/
void fprint_bdt(fp, obj)
FILE *fp;
bdt obj;
{
  list sched_l;

  fprintf(fp,"\n Scheduling:\n");
  fprintf(fp,"============\n");

  if(obj == bdt_undefined) {
    fprintf(fp, "\tNot computed\n\n");
    return;
  }

  sched_l = bdt_schedules(obj);

  for(; sched_l != NIL; sched_l = CDR(sched_l)) {
    schedule crt_sched;
    int crt_stmt;
    list dim_l;
    predicate crt_pred;

    crt_sched = SCHEDULE(CAR(sched_l));
    crt_stmt = schedule_statement(crt_sched);
    dim_l = schedule_dims(crt_sched);
    crt_pred = schedule_predicate(crt_sched);

    /* PRINT */
    /* Mod by AP, sep 13th 95: the number of the instruction is the
       statement_number. */
    fprintf(fp,"ins_%d:\n",
	    statement_number(ordering_to_statement(adg_number_to_ordering(crt_stmt))));

    if(crt_pred != predicate_undefined) {
      Psysteme ps = (Psysteme) predicate_system(crt_pred);
      Pcontrainte peq;

      if (ps != NULL) {
        fprintf(fp,"\t pred: ");

        for (peq = ps->inegalites; peq!=NULL;
             pu_inegalite_fprint(fp,peq,entity_local_name),peq=peq->succ);

        for (peq = ps->egalites; peq!=NULL;
             pu_egalite_fprint(fp,peq,entity_local_name),peq=peq->succ);

        fprintf(fp,"\n");
      }
      else
        fprintf(fp, "\t pred: TRUE\n");
    }
    else
      fprintf(fp, "\t pred: TRUE\n");

    fprintf(fp, "\t dims: ");
    for(; dim_l != NIL; dim_l = CDR(dim_l)) {
      expression exp = EXPRESSION(CAR(dim_l));
      fprintf(fp,"%s", words_to_string(words_expression(exp)));
      if(CDR(dim_l) != NIL)
        fprintf(fp," , ");
    }
    fprintf(fp,"\n");
  }
}


 /* package mapping : Alexis Platonoff, april 1993*/


/*============================================================================*/
string pu_variable_name(v)
Variable v;
{
 if(v == TCST)
    return("TCST");
 else
    return(entity_local_name((entity) v));
}


/*============================================================================*/
boolean pu_is_inferior_var(v1, v2)
Variable v1, v2;
{
 return(FALSE);
}


/*============================================================================*/
/* void pu_vect_fprint(FILE *fp, Pvecteur v): impression d'un vecteur creux v
 * sur le fichier fp.
 *
 * There exist a function "vect_fprint" in C3 which takes a third argument.
 */
void pu_vect_fprint(fp, v)
FILE *fp;
Pvecteur v;
{
    short int debut = 1;
    long int constante = 0;
    char signe;
    while (!VECTEUR_NUL_P(v)) {
	if (v->var!=TCST) {
	    long int coeff = v->val;

	    if (coeff != 0) {
		if (coeff > 0)
		    signe = (debut) ? ' ' : '+';
		else {
		    signe = '-';
		    coeff = -coeff;
		}
		debut = 0;
		if (coeff == 1)
		    fprintf(fp,"%c %s ", signe,
			    entity_local_name((entity) v->var));
		else
		    fprintf(fp,"%c %ld %s ", signe, coeff,
			    entity_local_name((entity) v->var));
	    }
	}
	else
	    /* on admet plusieurs occurences du terme constant!?! */
	    constante += v->val;
	v = v->succ;
    }
    if(debut)
	(void) fprintf(fp,"%ld\n", constante);
    else if(constante != 0) {
	if(constante > 0)
	    signe = (debut) ? ' ' : '+';
	else {
	    signe = '-';
	    constante = -constante;
	}
	(void) fprintf(fp,"%c %ld\n", signe, constante);
    }
    else
	(void) fprintf(fp, "\n");
}


/*============================================================================*/
/* void fprint_entity_list(FILE *fp,list l): prints a list of entities on
 * file fp.
 */
void fprint_entity_list(fp,l)
FILE *fp;
list l;
{
  for( ; l != NIL; l = CDR(l))
    fprintf(fp, "%s, ", entity_local_name(ENTITY(CAR(l))));
}


#define INDENT_FACTOR 2

/*============================================================================*/
void fprint_indent(fp, indent)
FILE *fp;
int indent;
{
  int i;

  fprintf(fp, "\n");
  for(i = 0; i < (indent * INDENT_FACTOR); i++) fprintf(fp, " ");
}


/*============================================================================*/
void imprime_quast (fp, qu)
FILE *fp;
quast qu;
{
  extern int quast_depth;

  Psysteme        paux  = SC_UNDEFINED;
  predicate       pred_aux = predicate_undefined;
  conditional     cond_aux;
  quast_value     quv;
  quast_leaf	  qul;
  leaf_label      ll;
  list            sol;

  if( (qu == quast_undefined) || (qu == NULL) ) {
    fprint_indent(fp, quast_depth);
    fprintf(fp, "Empty Quast ");
    return;
  }
  quv = quast_quast_value(qu);
  if( quv == quast_value_undefined ) {
    fprint_indent(fp, quast_depth);
    fprintf(fp, "Empty Quast ");
    return;
  }
  quv = quast_quast_value(qu);

  switch( quast_value_tag(quv)) {
    case is_quast_value_conditional:
      cond_aux = quast_value_conditional(quv);
      pred_aux = conditional_predicate(cond_aux);
      if (pred_aux != predicate_undefined)
      	  paux = (Psysteme)  predicate_system(pred_aux);
 
      fprint_indent(fp, quast_depth);
      fprintf(fp, "IF ");
      fprint_psysteme(fp, paux);

      fprint_indent(fp, quast_depth);
      fprintf(fp, "THEN");
      quast_depth++;
      imprime_quast(fp, conditional_true_quast (cond_aux) );
      quast_depth--;

      fprint_indent(fp, quast_depth);
      fprintf(fp, "ELSE");
      quast_depth++;
      imprime_quast(fp, conditional_false_quast (cond_aux) );
      quast_depth--;

      fprint_indent(fp, quast_depth);
      fprintf(fp, "FI");
    break;

    case is_quast_value_quast_leaf:
      qul = quast_value_quast_leaf( quv );
      if (qul == quast_leaf_undefined) {fprintf(fp,"Empty Quast Leaf\n");break;} 
      sol = quast_leaf_solution( qul );
      ll  = quast_leaf_leaf_label( qul );
      if (ll != leaf_label_undefined) {
        fprint_indent(fp, quast_depth);
	fprintf(fp, "Statement source number : %d",
		statement_number(ordering_to_statement(leaf_label_statement(ll))));

        fprint_indent(fp, quast_depth);
	fprintf(fp, "Depth : %d", leaf_label_depth(ll));	
      }
      fprint_indent(fp, quast_depth);
      while (sol != NIL) {
        fprintf(fp, "%s, ", words_to_string(words_expression(EXPRESSION(CAR(sol)))));
        sol = CDR(sol);
      }
    break;
  }
}

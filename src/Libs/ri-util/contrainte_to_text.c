/*
 * help-to-debug functions; 
 * previously in /rice/debug.c
 *
 * BA, september 3, 1993
 */
#include <stdio.h>
#include <string.h>

#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"

#include "genC.h"

#include "text.h"
#include "text-util.h"

#include "ri.h"
#include "ri-util.h"

void inegalite_debug(c)
Pcontrainte c;
{
    inegalite_fprint(stderr, c, entity_local_name);
}

void egalite_debug(c)
Pcontrainte c;
{
    egalite_fprint(stderr, c, entity_local_name);
}

int
contrainte_gen_allocated_memory(
    Pcontrainte pc)
{
    int result = 0;
    for(; pc; pc=pc->succ)
	result += sizeof(Scontrainte) + 
	    vect_gen_allocated_memory(pc->vecteur);
    return result;
}

/******************************************** FOR PRETTYPRINTING CONSTRAINTS */



void 
constante_to_textline(
    char * operation_line,
    Value constante,
    boolean is_inegalite, 
    boolean a_la_fortran)
{
    operation_line[0]='\0';
    (void) sprint_operator(operation_line+strlen(operation_line), 
			   is_inegalite, a_la_fortran);
    (void) sprint_Value(operation_line+strlen(operation_line), 
			constante);
}

void
unsigned_operation_to_textline(
    char * operation_line,
    Value coeff,
    Variable var,
    char * (*variable_name)(Variable))
{
    if (value_notone_p(ABS(coeff)) || var==TCST)
	(void) sprint_Value(operation_line+strlen(operation_line), coeff);
    (void) sprintf(operation_line+strlen(operation_line),"%s", 
		   variable_name(var));

}

void
signed_operation_to_textline(
    char * operation_line,
    char signe,
    Value coeff,
    Variable var,
    char * (*variable_name)(Variable))
{
   
    (void) sprintf(operation_line+strlen(operation_line),"%c",signe);
    unsigned_operation_to_textline(operation_line,coeff,var,variable_name);

}

static char * 
contrainte_to_text_1(
    string aux_line,
    string str_prefix,
    text txt,
    Pvecteur v,
    boolean is_inegalite,
    char * (*variable_name)(Variable),
    boolean a_la_fortran,
    boolean first_line)
{
    short int debut = 1;
    Value constante = VALUE_ZERO;
    char operation_line[MAX_LINE_LENGTH];
	
    while (!VECTEUR_NUL_P(v)) {
	Variable var = var_of(v);
	Value coeff = val_of(v);
	operation_line[0]='\0';

	if (var!=TCST) {
	    char signe;

	    if (value_notzero_p(coeff)) {
		if (value_pos_p(coeff))
		    signe =  '+';
		else {
		    signe = '-';
		    coeff = value_uminus(coeff);
		};
		if (value_pos_p(coeff) && debut)
		    unsigned_operation_to_textline(operation_line,coeff,var,
						   variable_name);
		else 
		    signed_operation_to_textline(operation_line,signe,coeff,
						 var, variable_name);
		debut = 0;
	    }
	    first_line = add_to_current_line(aux_line,operation_line,
					     str_prefix,txt,first_line);
	}
	else
	    /* on admet plusieurs occurences du terme constant!?! */
	    value_addto(constante, coeff);

	v = v->succ;
    }
    constante_to_textline(operation_line,value_uminus(constante),is_inegalite,
			  a_la_fortran);
    first_line = add_to_current_line(aux_line, operation_line,
				     str_prefix,txt,first_line);
    return aux_line;
}

static char * 
contrainte_to_text_2(
    string aux_line,
    string str_prefix,
    text txt,
    Pvecteur v,
    boolean is_inegalite,
    char * (*variable_name)(Variable),
    boolean a_la_fortran,
    boolean first_line)
{
    Pvecteur coord;
    short int debut = TRUE;
    int positive_terms = 0;
    int negative_terms = 0;
    Value const_coeff = 0;
    boolean const_coeff_p = FALSE;
    char signe;
    char operation_line[MAX_LINE_LENGTH];
   
    if(!is_inegalite) {
	for(coord = v; !VECTEUR_NUL_P(coord); coord = coord->succ) {
	    if(vecteur_var(coord)!= TCST) 
		(value_pos_p(vecteur_val(coord))) ? 
		    positive_terms++ :  negative_terms++;   
	}

	if(negative_terms > positive_terms) 
	    vect_chg_sgn(v);
    }

    positive_terms = 0;
    negative_terms = 0;

    for(coord = v; !VECTEUR_NUL_P(coord); coord = coord->succ) {
	Value coeff = vecteur_val(coord);
	Variable var = vecteur_var(coord);
	operation_line[0]='\0';

	if (value_pos_p(coeff)) {
	    positive_terms++;
	     if(!term_cst(coord)|| is_inegalite) {
		 signe =  '+';
		 if (debut)
		     unsigned_operation_to_textline(operation_line,coeff,var,
				       variable_name);
		 else 
		     signed_operation_to_textline(operation_line,signe,
						  coeff,var,variable_name);
		 debut=FALSE;

	    }
	     else  positive_terms--;
	       
	     first_line = add_to_current_line(aux_line,operation_line,
					     str_prefix,txt,first_line);
	}
    }

    operation_line[0]='\0';
    if(positive_terms == 0) 	
	(void) sprintf(operation_line+strlen(operation_line), "0"); 
    
    (void) sprint_operator(operation_line+strlen(operation_line), 
			   is_inegalite, a_la_fortran);
    
    first_line = add_to_current_line(aux_line,operation_line,
					     str_prefix,txt,first_line);

    debut = TRUE;
    for(coord = v; !VECTEUR_NUL_P(coord); coord = coord->succ) {
	Value coeff = vecteur_val(coord);
	Variable var = var_of(coord);
	operation_line[0]='\0';

	if(term_cst(coord) && !is_inegalite) {
	    /* Save the constant term for future use */
	    const_coeff_p = TRUE;
	    const_coeff = coeff;
	    /* And now, a lie... In fact, rhs_terms++ */
	    negative_terms++;
	}
	else if (value_neg_p(coeff)) {
	    negative_terms++;
	    signe = '+';
	    if (debut) {
		unsigned_operation_to_textline(operation_line, 
					       value_uminus(coeff),var,
					       variable_name);
		debut=FALSE;
	    }
	    else 
		signed_operation_to_textline(operation_line,signe,
					     value_uminus(coeff),var,
					     variable_name);
	    
	}
	first_line = add_to_current_line(aux_line, operation_line,
					 str_prefix,txt,first_line); 
    }
    operation_line[0]='\0';
    if(negative_terms == 0) {
	(void) sprintf(operation_line+strlen(operation_line), "0"); 
        first_line = add_to_current_line(aux_line,operation_line,
					     str_prefix,txt,first_line);
    }
      else if(const_coeff_p) {
	assert(value_notzero_p(const_coeff));
	
	if  (!debut && value_neg_p(const_coeff))
	    (void) sprintf(operation_line+strlen(operation_line), "+"); 
	(void) sprint_Value(operation_line+strlen(operation_line), 
			    value_uminus(const_coeff));

	first_line = add_to_current_line(aux_line,operation_line,
					 str_prefix,txt,first_line);
    }
   

    return aux_line;
}


char * 
contrainte_text_format(
    char * aux_line,
    char * str_prefix,
    text txt,
    Pcontrainte c,
    boolean is_inegalite,
    char * (*variable_name)(Variable),
    boolean a_la_fortran,
    boolean first_line)
{
    Pvecteur v;
    int heuristique = 2;

    if (!CONTRAINTE_UNDEFINED_P(c))
	v = contrainte_vecteur(c);
    else
	v = VECTEUR_NUL;

    assert(vect_check(v));

    switch(heuristique) {
    case 1: aux_line = contrainte_to_text_1(aux_line,str_prefix,txt,
					    v,is_inegalite, variable_name, 
					    a_la_fortran, first_line);
	break;
    case 2:aux_line = contrainte_to_text_2(aux_line,str_prefix,txt,v,
					   is_inegalite, variable_name, 
					    a_la_fortran, first_line);
	
	break;
    default: contrainte_error("contrainte_sprint", "unknown heuristics\n");
    }

    return aux_line;
}

char  * 
egalite_text_format(
    char * aux_line,
    char * str_prefix,
    text txt,
    Pcontrainte eg,
    char * (*variable_name)(),
    bool  a_la_fortran, 
    bool first_line)
{
    return contrainte_text_format(aux_line,str_prefix,txt,eg,FALSE,
				  variable_name,a_la_fortran,first_line);
}

char * 
inegalite_text_format(
    char *aux_line,
    char * str_prefix,
    text txt,
    Pcontrainte ineg,
    char * (*variable_name)(),
    boolean a_la_fortran,
    boolean first_line)
{
    return contrainte_text_format(aux_line,str_prefix,txt,ineg,TRUE, 
				  variable_name,a_la_fortran,first_line);
}


static void 
add_separation(string line, string prefix, text txt, bool a_la_fortran)
{
    add_to_current_line(line, a_la_fortran? ".AND.": ", ", prefix, txt, FALSE);
}

static bool
contraintes_text_format(
    string line,   		/* current buffer */
    string prefix, 		/* for continuations */
    text txt,      		/* formed text */
    Pcontrainte cs, 		/* contraintes to be printed */
    string (*variable_name)(),	/* hook for naming a variable */
    bool invert_put_first,      /* whether to invert put_first */
    int (*put_first)(Pvecteur), /* whether to put first some constraints */
    bool some_previous,         /* whether a separator is needed */
    bool is_inegalites,		/* egalites or inegalites */
    bool a_la_fortran		/* fortran look? */)
{
    for (; cs; cs=cs->succ)
    {
	if (put_first? (invert_put_first ^ put_first(cs->vecteur)): TRUE)
	{
	    if (some_previous) add_separation(line, prefix, txt, a_la_fortran);
	    else some_previous = TRUE;

	    if (a_la_fortran)
		add_to_current_line(line, "(", prefix, txt, FALSE);
	    
	    contrainte_text_format(line, prefix, txt, cs, is_inegalites,
				   variable_name, FALSE, a_la_fortran);
	    
	    if (a_la_fortran) 
		add_to_current_line(line, ")", prefix, txt, FALSE);
	}
    }
    return some_previous;
}

/* lower level hook for regions.
 */
void
system_sorted_text_format(
    string line,
    string prefix,
    text txt,
    Psysteme ps,
    string (*variable_name)(),
    bool (*put_first)(Pvecteur), /* whether to put a constraints ahead */
    bool a_la_fortran)
{
    bool invert, stop, some_previous = FALSE;

    /* { 
     */
    if (!a_la_fortran) add_to_current_line(line, "{", prefix, txt, FALSE);

    /* repeat twice: once for first, once for not first.
     */
    for(invert = FALSE, stop = FALSE; !stop; )
    {
	/* , / .AND.
	 */
	if (some_previous) add_separation(line, prefix, txt, a_la_fortran);
	
	/* == / .EQ.
	 */
	some_previous = 
	    contraintes_text_format(line, prefix, txt, sc_egalites(ps), 
				    variable_name, invert, put_first,
				    some_previous, FALSE, a_la_fortran);
	/* , / .AND.
	 */
	if (some_previous) add_separation(line, prefix, txt, a_la_fortran);
	
	/* <= / .LE.
	 */
	some_previous = 
	    contraintes_text_format(line, prefix, txt, sc_inegalites(ps), 
				    variable_name, invert, put_first,
				    some_previous, TRUE, a_la_fortran);

	if (invert || !put_first) stop = TRUE;
	invert = TRUE;	
    }

    /* }
     */
    if (!a_la_fortran) add_to_current_line(line, "}", prefix, txt, FALSE);
}

/* appends ps to line/txt with prefix continuations. 
 */
void 
system_text_format(
    string line,
    string prefix,
    text txt,
    Psysteme ps,
    string (*variable_name)(),
    bool a_la_fortran)
{
    system_sorted_text_format
	(line, prefix, txt, ps, variable_name, NULL, a_la_fortran);
}

/*   That is all
 */

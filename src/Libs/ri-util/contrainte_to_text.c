/*
 * help-to-debug functions; 
 * previously in /rice/debug.c
 *
 * BA, september 3, 1993
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "linear.h"

#include "genC.h"

#include "text.h"
#include "text-util.h"

#include "ri.h"
#include "ri-util.h"

/************************************************************* VECTEUR STUFF */

void vect_debug(Pvecteur v)
{
    vect_fprint(stderr, v, entity_local_name);
}

/* comparison function for Pvecteur in pips
 */
int compare_Pvecteur(pv1, pv2)
Pvecteur *pv1, *pv2;
{
    return compare_entities((entity*)&var_of(*pv1),
			    (entity*)&var_of(*pv2));
}

/******************************************************************* SYSTEME */

void sc_syst_debug(Psysteme s)
{
    sc_fprint(stderr, s, entity_local_name);
}

/******************************************** FOR PRETTYPRINTING CONSTRAINTS */

void 
inegalite_debug(Pcontrainte c)
{
    inegalite_fprint(stderr, c, entity_local_name);
}

void 
egalite_debug(Pcontrainte c)
{
    egalite_fprint(stderr, c, entity_local_name);
}

static string
the_operator(
    boolean is_inegalite, 
    boolean a_la_fortran)
{
    return is_inegalite? (a_la_fortran? ".LE.": "<="): 
	(a_la_fortran? ".EQ.": "==");
}

static void
add_Value_to_current_line(
    string buffer,
    Value v,
    string continuation,
    text t)
{
    add_to_current_line(buffer, Value_to_string(v), continuation, t);
}

static void 
constante_to_textline(
    string buffer,
    Value constante,
    boolean is_inegalite, 
    boolean a_la_fortran,
    string continuation,
    text t)
{
    add_to_current_line(buffer, the_operator(is_inegalite, a_la_fortran),
			continuation, t);
    add_Value_to_current_line(buffer, constante, continuation, t);
}

static void
unsigned_operation_to_textline(
    string buffer,
    Value coeff,
    Variable var,
    char * (*variable_name)(Variable),
    string continuation,
    text t)
{
    if (value_notone_p(ABS(coeff)) || var==TCST)
	add_Value_to_current_line(buffer, coeff, continuation, t);
    add_to_current_line(buffer, variable_name(var), continuation, t);
}

static void
signed_operation_to_textline(
    string buffer,
    string signe,
    Value coeff,
    Variable var,
    char * (*variable_name)(Variable),
    string continuation,
    text t)
{
    add_to_current_line(buffer, signe, continuation, t);
    unsigned_operation_to_textline(buffer,coeff,var,variable_name,
				   continuation, t);
}

static char * 
contrainte_to_text_1(
    string buffer,
    string continuation,
    text txt,
    Pvecteur v,
    boolean is_inegalite,
    char * (*variable_name)(Variable),
    boolean a_la_fortran,
    boolean first_line)
{
    short int debut = 1;
    Value constante = VALUE_ZERO;
	
    while (!VECTEUR_NUL_P(v)) 
    {
	Variable var = var_of(v);
	Value coeff = val_of(v);

	if (var!=TCST) {
	    string signe;

	    if (value_notzero_p(coeff)) {
		if (value_pos_p(coeff))
		    signe =  "+";
		else {
		    signe = "-";
		    coeff = value_uminus(coeff);
		};
		if (value_pos_p(coeff) && debut)
		    unsigned_operation_to_textline(buffer,coeff, var,
						   variable_name,
						   continuation, txt);
		else 
		    signed_operation_to_textline(buffer, signe, coeff,
						 var, variable_name,
						 continuation, txt);
		debut = 0;
	    }
	}
	else
	    /* on admet plusieurs occurences du terme constant!?! */
	    value_addto(constante, coeff);

	v = v->succ;
    }
    constante_to_textline(buffer,value_uminus(constante),is_inegalite,
			  a_la_fortran, continuation, txt);
    return buffer;
}

static char * 
contrainte_to_text_2(
    string buffer,
    string continuation,
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
    string signe;
   
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

	if (value_pos_p(coeff)) {
	    positive_terms++;
	     if(!term_cst(coord)|| is_inegalite) 
	     {
		 signe =  "+";
		 if (debut)
		     unsigned_operation_to_textline(buffer,coeff,var,
						    variable_name,
						    continuation, txt);
		 else 
		     signed_operation_to_textline(buffer,signe,
						  coeff,var,variable_name,
						  continuation, txt);
		 debut=FALSE;

	    }
	     else  positive_terms--;
	}
    }

    if(positive_terms == 0)
	add_to_current_line(buffer, "0", continuation, txt);

    add_to_current_line(buffer, the_operator(is_inegalite, a_la_fortran),
			continuation, txt);

    debut = TRUE;
    for(coord = v; !VECTEUR_NUL_P(coord); coord = coord->succ) 
    {
	Value coeff = vecteur_val(coord);
	Variable var = var_of(coord);

	if(term_cst(coord) && !is_inegalite) {
	    /* Save the constant term for future use */
	    const_coeff_p = TRUE;
	    const_coeff = coeff;
	    /* And now, a lie... In fact, rhs_terms++ */
	    negative_terms++;
	}
	else if (value_neg_p(coeff))
	{
	    negative_terms++;
	    signe = "+";
	    if (debut) {
		unsigned_operation_to_textline
		    (buffer, value_uminus(coeff), var, variable_name,
		     continuation, txt);
		debut=FALSE;
	    }
	    else 
		signed_operation_to_textline
		    (buffer, signe, value_uminus(coeff),var, variable_name,
		     continuation, txt);
	    
	}
    }

    if(negative_terms == 0)
        add_to_current_line(buffer, "0", continuation, txt);

    else if(const_coeff_p) 
    {
	assert(value_notzero_p(const_coeff));
	if  (!debut && value_neg_p(const_coeff))
	    add_to_current_line(buffer, "+", continuation, txt);
	add_Value_to_current_line(buffer, value_uminus(const_coeff),
				  continuation, txt);
    }

    return buffer;
}


char * 
contrainte_text_format(
    char * aux_line,
    char * continuation,
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
    case 1: aux_line = contrainte_to_text_1(aux_line,continuation,txt,
					    v,is_inegalite, variable_name, 
					    a_la_fortran, first_line);
	break;
    case 2:aux_line = contrainte_to_text_2(aux_line,continuation,txt,v,
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
    char * continuation,
    text txt,
    Pcontrainte eg,
    char * (*variable_name)(),
    bool  a_la_fortran, 
    bool first_line)
{
    return contrainte_text_format(aux_line,continuation,txt,eg,FALSE,
				  variable_name,a_la_fortran,first_line);
}

char * 
inegalite_text_format(
    char *aux_line,
    char * continuation,
    text txt,
    Pcontrainte ineg,
    char * (*variable_name)(),
    boolean a_la_fortran,
    boolean first_line)
{
    return contrainte_text_format(aux_line,continuation,txt,ineg,TRUE, 
				  variable_name,a_la_fortran,first_line);
}


static void 
add_separation(string line, string prefix, text txt, bool a_la_fortran)
{
    add_to_current_line(line, a_la_fortran? ".AND.": ", ", prefix, txt);
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
		add_to_current_line(line, "(", prefix, txt);
	    
	    contrainte_text_format(line, prefix, txt, cs, is_inegalites,
				   variable_name, FALSE, a_la_fortran);
	    
	    if (a_la_fortran) 
		add_to_current_line(line, ")", prefix, txt);
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

    if (ps==NULL)
    {
	add_to_current_line(line, "<null system>", prefix, txt);
	return;
    }
    else if (SC_UNDEFINED_P(ps))
    {
	add_to_current_line(line, "<undefined system>", prefix, txt);
	return;
    }
    else if (sc_empty_p(ps))
    {
	add_to_current_line(line, "{0==-1}", prefix, txt);
	return;
    }

    /* { 
     */
    if (!a_la_fortran) add_to_current_line(line, "{", prefix, txt);

    /* repeat twice: once for first, once for not first.
     */
    for(invert = FALSE, stop = FALSE; !stop; )
    {
	/* == / .EQ.
	 */
	some_previous = 
	    contraintes_text_format(line, prefix, txt, sc_egalites(ps), 
				    variable_name, invert, put_first,
				    some_previous, FALSE, a_la_fortran);
	
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
    if (!a_la_fortran) add_to_current_line(line, "}", prefix, txt);
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

static int 
wordcmp(const void * s1, const void * s2)
{
    return strcmp(*(char**)s1,*(char**)s2);
}

/* appends the list of entity...
 */
void
entity_list_text_format(
    string line,
    string continuation,
    text t,
    list /* of entity */ le,
    string (*var_name)(entity))
{
    if (le)
    {
	int j=0, len = gen_length(le);
	char ** provi = (char **) malloc(sizeof(char *) * len);
	bool some_previous = FALSE;
	
	MAP(ENTITY, e, 
	    provi[j++] = entity_undefined_p(e)? "undefined...": var_name(e),
	    le);
	
	qsort(provi, len, sizeof(char**), wordcmp);
	
	for(j=0; j<len; j++)
	{
	    if (some_previous) add_to_current_line(line, ",", continuation, t);
	    else some_previous = TRUE;
	    add_to_current_line(line, provi[j], continuation, t);
	}
	
	free(provi);
    }
}

/*   That is all
 */

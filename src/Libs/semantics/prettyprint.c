/* 	%A% ($Date: 1996/06/12 23:34:51 $, ) version $Revision$, got on %D%, %T% [%P%].\n Copyright (c) �cole des Mines de Paris Proprietary.	 */

#ifndef lint
char vcid_semantics_prettyprint[] = "%A% ($Date: 1996/06/12 23:34:51 $, ) version $Revision$, got on %D%, %T% [%P%].\n Copyright (c) �cole des Mines de Paris Proprietary.";
#endif /* lint */

 /* package semantics - prettyprint interface */

#include <stdio.h>
#include <string.h>
/* #include <stdlib.h> */

#include "types.h"

#include "genC.h"

#include "text.h"
#include "text-util.h"

#include "top-level.h"

#include "ri.h"
#include "ri-util.h"

#include "database.h"
#include "pipsdbm.h"
#include "resources.h"

#include "misc.h"
#include "properties.h"

#include "prettyprint.h"

#include "transformer.h"
#include "effects.h"

#include "semantics.h"
extern void qsort();

#define PREC_FORESYS_PREFIX "C$PREC"
#define TRAN_FORESYS_PREFIX "C$TRAN"
#define FORESYS_CONTINUATION_PREFIX "C$&"
#define PIPS_NORMAL_PREFIX "C"

DEFINE_CURRENT_MAPPING(semantic, transformer)

static bool is_transformer;
static bool is_user_view;
static hash_table nts = hash_table_undefined;

static bool print_code_semantics();
static text get_semantic_text();

bool print_code_transformers(module_name)
char *module_name;
{
    is_user_view = FALSE;
    is_transformer = TRUE;
    return print_code_semantics(module_name);
}

bool print_code_preconditions(module_name)
char *module_name;
{
    is_user_view = FALSE;
    is_transformer = FALSE;
    return print_code_semantics(module_name);
}

bool print_source_transformers(module_name)
char *module_name;
{
    is_user_view = TRUE;
    is_transformer = TRUE;
    return print_code_semantics(module_name);
}

bool print_source_preconditions(module_name)
char *module_name;
{
    is_user_view = TRUE;
    is_transformer = FALSE;
    return print_code_semantics(module_name);
}

text get_text_transformers(module_name)
char *module_name;
{
    is_user_view = FALSE;
    is_transformer = TRUE;
    return get_semantic_text(module_name,FALSE);
}

text get_text_preconditions(module_name)
char *module_name;
{
    is_user_view = FALSE;
    is_transformer = FALSE;
    return get_semantic_text(module_name,FALSE);
}

static bool print_code_semantics(module_name)
char *module_name;
{
    bool success = TRUE;

    char * file_ext = strdup(concatenate(is_transformer?
		     (is_user_view? USER_TRANSFORMER_SUFFIX :
		      SEQUENTIAL_TRANSFORMER_SUFFIX ) :
		     (is_user_view? USER_PRECONDITION_SUFFIX :
		      SEQUENTIAL_PRECONDITION_SUFFIX),
		     get_bool_property
		     ("PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH") ?
		     GRAPH_FILE_EXT : "",
		     NULL));

    char * resource_name =
	get_bool_property("PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH") ?
	    DBR_GRAPH_PRINTED_FILE :
		(is_user_view? DBR_PARSED_PRINTED_FILE :
		 DBR_PRINTED_FILE);

    begin_attachment_prettyprint();
    
    success = make_text_resource(module_name,
				 resource_name,
				 file_ext,
				 get_semantic_text(module_name,TRUE));

    end_attachment_prettyprint();
 
    free(file_ext);
    return success;
}

static text get_semantic_text(module_name,give_code_p)
char *module_name;
bool give_code_p;
{
    text r = make_text(NIL);
    entity mod;
    statement mod_stat;
    transformer summary = transformer_undefined;
    statement user_stat = statement_undefined;

    set_current_module_entity( local_name_to_top_level_entity(module_name) );
    mod = get_current_module_entity();

    set_current_module_statement
	((statement)db_get_memory_resource(DBR_CODE, module_name, TRUE) );
    mod_stat = get_current_module_statement();

    /* To set up the hash table to translate value into value names */
    set_cumulated_effects_map( effectsmap_to_listmap
			      ((statement_mapping)
			       db_get_memory_resource
			       (DBR_CUMULATED_EFFECTS, module_name, TRUE)));

    debug_on("SEMANTICS_DEBUG_LEVEL");

    module_to_value_mappings(mod);

    if(is_user_view) {
	user_stat =  (statement)
	    db_get_memory_resource(DBR_PARSED_CODE, module_name, TRUE);

	nts = allocate_number_to_statement();
	nts = build_number_to_statement(nts, mod_stat);

	ifdebug(1) {
	    print_number_to_statement(nts);
	}
	/* debug_off(); */
    }


    /* semantic information to print */
    set_semantic_map( (statement_mapping)
		     db_get_memory_resource(
					    is_transformer? DBR_TRANSFORMERS 
					    : DBR_PRECONDITIONS,
					    module_name,
					    TRUE) );
    summary = (transformer)
	db_get_memory_resource(
			       is_transformer? DBR_SUMMARY_TRANSFORMER
			       : DBR_SUMMARY_PRECONDITION,
			       module_name,
			       TRUE);
    /* The summary precondition may be in another module's frame */
    translate_global_values(mod, summary);

    init_prettyprint(semantic_to_text);

    /* initial version; to be used again when prettyprint really prettyprints*/
    /* print_text(fd, text_statement(mod, 0, mod_stat)); */

    /* summary information first */
    MERGE_TEXTS(r,text_transformer(summary)); 
    attach_decoration_to_text(r);
    if (is_transformer)
	attach_transformers_decoration_to_text(r);
    else
	attach_preconditions_decoration_to_text(r);
 
    if (give_code_p == TRUE) {
	/* then code with the corresponding information */
	ADD_SENTENCE_TO_TEXT(r, 
			     make_sentence(is_sentence_formatted, 
					   code_decls_text(entity_code(mod))));
	MERGE_TEXTS(r, text_statement(mod, 0, is_user_view? user_stat:mod_stat));
	ADD_SENTENCE_TO_TEXT(r, sentence_tail());
    }

    debug_off();

    if(is_user_view) {
	hash_table_free(nts);
	nts = hash_table_undefined;
    }

    close_prettyprint();

    reset_semantic_map();
    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_effects_map();

    return r;
}

/* this function name is VERY misleading - it should be changed, sometime FI */
text semantic_to_text(module, margin, stmt)
entity module;
int margin;
statement stmt;
{
    transformer t;
    text txt;

    if(is_user_view) {
	statement i = (statement) hash_get(nts, (char *) statement_number(stmt));

	if(i!=(statement) HASH_UNDEFINED_VALUE) {
	    t = load_statement_semantic(i);
	}
	else
	    t = (transformer) HASH_UNDEFINED_VALUE;
    }
    else
	t = load_statement_semantic(stmt);

    txt = text_transformer(t);

    if (is_transformer)
	attach_transformers_decoration_to_text(txt);
    else
	attach_preconditions_decoration_to_text(txt);
	
    return txt; 
}


/* It is used to sort arguments preconditions in text_transformer(). */
static int wordcmp(s1,s2)
char **s1, **s2;
{
    return strcmp(*s1,*s2);
}

/* The strange argument type is required by qsort(), deep down in the calls */
static int is_inferior_pvarval(Pvecteur * pvarval1, Pvecteur * pvarval2)
{
    /* The constant term is given the highest weight to push constant
       terms at the end of the constraints and to make those easy
       to compare. If not, constant 0 will be handled differently from
       other constants. However, it would be nice to give constant terms
       the lowest weight to print simple constraints first...

       Either I define two comparison functions, or I cheat somewhere else.
       Let's cheat? */
    int is_equal = 0;
    
    if (term_cst(*pvarval1) && !term_cst(*pvarval2))
	is_equal = 1;
    else if (term_cst(*pvarval1) && term_cst(*pvarval2))
	is_equal = 0;
    else if(term_cst(*pvarval2))
	is_equal = -1;
    else
	is_equal = 
	    strcmp(pips_user_value_name((entity) vecteur_var(*pvarval1)),
		   pips_user_value_name((entity) vecteur_var(*pvarval2)));


    return is_equal; 
}


#define LINE_SUFFIX "\n"
#define MAX_LINE_LENGTH 70


boolean add_to_current_line(crt_line, add_string, str_prefix, txt, first_line)
string crt_line, add_string, str_prefix;
text txt;
boolean first_line;
{
  boolean foresys = get_bool_property("PRETTYPRINT_FOR_FORESYS");
  
  if(strlen(crt_line) + strlen(add_string) > MAX_LINE_LENGTH-2) {
    (void) strcat(crt_line, LINE_SUFFIX);
    ADD_SENTENCE_TO_TEXT(txt, make_sentence(is_sentence_formatted,
					    strdup(crt_line)));
    
    if(first_line) {
      first_line = FALSE;
      if(foresys) {
	str_prefix = strdup(str_prefix);
	str_prefix[0] = '\0';
	(void) strcat(str_prefix, FORESYS_CONTINUATION_PREFIX);
      }
    }
    
    crt_line[0] = '\0'; (void) strcat(crt_line, str_prefix); 
    (void) strcat(crt_line, "    ");
    if(strlen(crt_line) + strlen(add_string) > MAX_LINE_LENGTH-2)
      pips_error("text_transformer", "line buffer too small");
  }
  (void) strcat(crt_line, add_string);

  return(first_line);
}

/* text text_transformer(transformer tran) 
 * input    : a transformer representing a transformer or a precondition 
 * output   : a text containing commentaries representing the transformer
 * modifies : nothing.
 *
 * Modification: AP, Nov 10th, 1995. Instead of building a (very long)
 * string, I directly use the transformer to build the prettyprint in text
 * format. This is to avoid the problem occuring when the buffer used in
 * transformer[precondition]_to_string() is too small. I also use a static
 * buffer to build each constraint; we are restricted to constraints of
 * lengths smaller than the line length.
 */
text text_transformer(transformer tran)
{
  text txt = make_text(NIL);
  boolean foresys = get_bool_property("PRETTYPRINT_FOR_FORESYS");
  string str_prefix;
  static char crt_line[MAX_LINE_LENGTH];
  static char aux_line[MAX_LINE_LENGTH];
  Pcontrainte peq;
  Psysteme ps;
  boolean first_line = TRUE;

  if (is_transformer) {
    if (foresys) 
      str_prefix = TRAN_FORESYS_PREFIX;
    else 
      str_prefix = PIPS_NORMAL_PREFIX;
  }
  else {
    if (foresys) 
      str_prefix = PREC_FORESYS_PREFIX;
    else 
      str_prefix = PIPS_NORMAL_PREFIX;
  }

  /* If in EMACS mode, does not add any separator line: */
  if (!get_bool_property("PRETTYPRINT_ADD_EMACS_PROPERTIES"))
      ADD_SENTENCE_TO_TEXT(txt, make_sentence(is_sentence_formatted,
					      strdup("\n")));

  crt_line[0] = '\0'; (void) strcat(crt_line, str_prefix);
  (void) strcat(crt_line, " ");

  if(tran != (transformer) HASH_UNDEFINED_VALUE && 
     tran != (transformer) list_undefined) {
    if(tran==transformer_undefined) {
      if (is_transformer)
	(void) strcat(crt_line, " TRANSFORMER: TRANSFORMER_UNDEFINED");
      else
	(void) strcat(crt_line, " PRECONDITION: TRANSFORMER_UNDEFINED");
    }
    else {
      list args;
      int j=0, provi_length = 1;
      char *provi[100];

      aux_line[0] = '\0';
      if (is_transformer)
	(void) strcat(aux_line, " T(");
      else
	(void) strcat(aux_line, " P(");
      if(strlen(crt_line) + strlen(aux_line) > MAX_LINE_LENGTH - 2)
	pips_error("text_transformer", "line buffer too small");

      (void) strcat(crt_line, aux_line);

      args = transformer_arguments(tran);
      if(!ENDP(args)) {
	MAPL(c, {entity e = ENTITY(CAR(c));
		 if (e==entity_undefined) 
		   provi[j] = (char*) "entity_undefined";
		 else
		   provi[j] = (char*) entity_local_name(e);
		 j++;
	       },
	     args);
	provi_length = j;
    
	qsort(provi, provi_length, sizeof provi[0], wordcmp);
	if ( provi_length > 1 ) {
	  for (j=0; j < provi_length-1; j++) {
	    aux_line[0] = '\0';
	    (void) strcat(aux_line, provi[j]);
	    strcat(aux_line,",");
	    first_line = add_to_current_line(crt_line, aux_line,
					     str_prefix, txt, first_line);
	  }
	}
	aux_line[0] = '\0';
	(void) strcat(aux_line, provi[provi_length-1]);
	strcat(aux_line, ")");
	if (foresys)
	  (void) strcat(aux_line, ",");
	first_line = add_to_current_line(crt_line, aux_line,
					 str_prefix, txt, first_line);
      }
      else
	strcat(crt_line, ")");

      if(strlen(crt_line)+1 > MAX_LINE_LENGTH-2) {
	(void) strcat(crt_line, LINE_SUFFIX);
	ADD_SENTENCE_TO_TEXT(txt, make_sentence(is_sentence_formatted,
						strdup(crt_line)));

	if(first_line) {
	  first_line = FALSE;
	  if(foresys) {
	    str_prefix = strdup(str_prefix);
	    str_prefix[0] = '\0';
	    (void) strcat(str_prefix, FORESYS_CONTINUATION_PREFIX);
	  }
	}

	crt_line[0] = '\0'; (void) strcat(crt_line, str_prefix); 
	(void) strcat(crt_line, "    ");
      }
      else 
	(void) strcat(crt_line, " ");

      ps = (Psysteme) predicate_system(transformer_relation(tran));

      if (ps != NULL) {
	boolean first_constraint = TRUE, last_constraint = FALSE;
	
	sc_lexicographic_sort(ps, is_inferior_pvarval);

	for (peq = ps->egalites; peq!=NULL; peq=peq->succ) {
	  last_constraint = ((peq->succ == NULL) &&
			     (ps->inegalites == NULL));
	  aux_line[0] = '\0';
	  if (foresys) {
	    (void) strcat(aux_line, "(");
	    (void) egalite_sprint_format(aux_line, peq,
					 pips_user_value_name, foresys);
	    (void) strcat(aux_line, ")");
	    if(! last_constraint)
	      (void) strcat(aux_line, ".AND."); 
	  }
	  else {
	    if(first_constraint) {
	      (void) strcat(aux_line, "{");
	      first_constraint = FALSE;
	    }
	    (void) egalite_sprint_format(aux_line, peq,
					 pips_user_value_name, foresys);
	    if(! last_constraint)
	      (void) strcat(aux_line, ", ");
	    else
	      (void) strcat(aux_line, "}");
	  }

	  first_line = add_to_current_line(crt_line, aux_line, str_prefix,
					   txt, first_line);
	}

	for (peq = ps->inegalites; peq!=NULL; peq=peq->succ) {
	  last_constraint = (peq->succ == NULL);
	  aux_line[0] = '\0';
	  if (foresys) {
	    (void) strcat(aux_line, "(");
	    (void) inegalite_sprint_format(aux_line, peq,
					   pips_user_value_name, foresys);
	    (void) strcat(aux_line, ")");
	    if(! last_constraint)
	      (void) strcat(aux_line, ".AND."); 
	  }
	  else {
	    if(first_constraint) {
	      (void) strcat(aux_line, "{");
	      first_constraint = FALSE;
	    }
	    (void) inegalite_sprint_format(aux_line, peq,
					   pips_user_value_name, foresys);
	    if(! last_constraint)
	      (void) strcat(aux_line, ", ");
	    else
	      (void) strcat(aux_line, "}");
	  }

	  first_line = add_to_current_line(crt_line, aux_line, str_prefix,
					   txt, first_line);
	}

	/* If there is no constraint */
	if((ps->egalites == NULL) && (ps->inegalites == NULL)) {
	  aux_line[0] = '\0';
	  (void) strcat(aux_line, "{}");
	  first_line = add_to_current_line(crt_line, aux_line, str_prefix,
					   txt, first_line);
	}
      }
      else {
	aux_line[0] = '\0';
	(void) strcat(aux_line, "SC_UNDEFINED");
	first_line = add_to_current_line(crt_line, aux_line, str_prefix,
					 txt, first_line);
      }
    }

    /* Save last line */
    (void) strcat(crt_line, LINE_SUFFIX);
    ADD_SENTENCE_TO_TEXT(txt, make_sentence(is_sentence_formatted,
					    strdup(crt_line)));
  }
  
  if (!get_bool_property("PRETTYPRINT_ADD_EMACS_PROPERTIES"))
      ADD_SENTENCE_TO_TEXT(txt, make_sentence(is_sentence_formatted,
					      strdup("\n")));

  return txt; 
}


/* ---------------------------------------------------------------- */
/* to convert strings containing predicates to text of commentaries */
/* BA, april 1994                                                   */
/* ---------------------------------------------------------------- */

#define MAX_PRED_COMMENTARY_STRLEN 70


/* text string_predicate_to_commentary(string str_pred, string comment_prefix) 
 * input    : a string, part of which represents a predicate.
 * output   : a text consisting of several lines of commentaries,
 *            containing the string str_pred, and beginning with 
 *            comment_prefix.
 * modifies : str_pred;
 */
text string_predicate_to_commentary(str_pred, comment_prefix)
string str_pred;
string comment_prefix;
{
    text t_pred = make_text(NIL);
    string str_suiv = NULL;
    string str_prefix = comment_prefix;
    char str_tmp[MAX_PRED_COMMENTARY_STRLEN];
    int len, new_str_pred_len, longueur_max;
    boolean premiere_ligne = TRUE;
    boolean foresys = get_bool_property("PRETTYPRINT_FOR_FORESYS");
    longueur_max = MAX_PRED_COMMENTARY_STRLEN - strlen(str_prefix) - 2;

    /* if str_pred is too long, it must be splitted in several lines; 
     * the hyphenation must be done only between the constraints of the
     * predicate, when there is a "," or a ")". A space is added at the beginning
     * of extra lines, for indentation. */
    while((len = strlen(str_pred)) > 0) {
	if (len > longueur_max) {

	    /* search the maximal substring which length 
	     * is less than longueur_max */
	    str_tmp[0] = '\0';
	    (void) strncat(str_tmp, str_pred, longueur_max);

	    switch (foresys) {
	    case FALSE : 
		str_suiv = strrchr(str_tmp, ',');
		break;
	    case TRUE : 
		str_suiv = strrchr(str_tmp, ')');
		break;
	    }
	    new_str_pred_len = (strlen(str_tmp) - strlen(str_suiv)) + 1;
	    str_suiv = strdup(&(str_pred[new_str_pred_len]));

	    str_tmp[0] = '\0';
	    if (!premiere_ligne)
		(void) strcat(str_tmp, " "); 
	    (void) strncat(str_tmp, str_pred, new_str_pred_len);

	    /* add it to the text */
	    ADD_SENTENCE_TO_TEXT(t_pred, 
				 make_pred_commentary_sentence(strdup(str_tmp),
							       str_prefix));
	    str_pred =  str_suiv;
	}
	else {
	    /* if the remaining string fits in one line */
	    str_tmp[0] = '\0';
	    if (!premiere_ligne)
		(void) strcat(str_tmp, " "); 
	    (void) strcat(str_tmp, str_pred);

	    ADD_SENTENCE_TO_TEXT(t_pred, 
				 make_pred_commentary_sentence(str_tmp,
							       str_prefix));
	    str_pred[0] = '\0';
	}
	
	if (premiere_ligne) {
	    premiere_ligne = FALSE;
	    longueur_max = longueur_max - 1;
	    if (foresys){
		int i;
		int nb_espaces = strlen(str_prefix) -
		    strlen(FORESYS_CONTINUATION_PREFIX);

		str_prefix = strdup(str_prefix);
		str_prefix[0] = '\0';
		(void) strcat(str_prefix, FORESYS_CONTINUATION_PREFIX);
		for (i=1; i <= nb_espaces; i++)
		    (void) strcat(str_prefix, " ");
	      }
	  }
    }
    
    return(t_pred);
}
    


/* text words_predicate_to_commentary(list w_pred, string comment_prefix)
 * input    : a list of strings, one of them representing a predicate.
 * output   : a text of several lines of commentaries containing 
 *            this list of strings, and beginning with comment_prefix.
 * modifies : nothing.
 */
text words_predicate_to_commentary(w_pred, comment_prefix)
list w_pred;
string comment_prefix;
{
    string str_pred;
    text t_pred;

    /* str_pred is the string corresponding to the concatenation
     * of the strings in w_pred */
    str_pred = words_to_string(w_pred);

    t_pred = string_predicate_to_commentary(str_pred, comment_prefix);

    return(t_pred);
}


/* sentence make_pred_commentary_sentence(string str_pred, string comment_prefix) 
 * input    : a substring formatted to be a commentary
 * output   : a sentence, containing the commentary form of this string,
 *            beginning with the comment_prefix.
 * modifies : nothing
 */
sentence make_pred_commentary_sentence(str_pred, comment_prefix)
string str_pred;
string comment_prefix;
{
    char str_tmp[MAX_PRED_COMMENTARY_STRLEN + 1];
    sentence sent_pred;

    str_tmp[0] = '\0';
    (void) strcat(str_tmp, comment_prefix); 
    (void) strcat(str_tmp, "  ");
    (void) strcat(str_tmp, str_pred);
    (void) strcat(str_tmp, "\n"); 

    sent_pred = make_sentence(is_sentence_formatted, strdup(str_tmp));
    free(str_tmp);
    return(sent_pred);
}


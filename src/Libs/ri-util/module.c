 /*
  * $Id$
  */
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "linear.h"

#include "genC.h"
#include "text.h"
#include "text-util.h"
#include "ri.h"
#include "ri-util.h"

#include "misc.h"
#include "properties.h"

static bool module_coherent_p=TRUE;
static entity checked_module = entity_undefined;


/* Check if the given name is a plain module (and not a compilation unit)
   internal name
 */
bool module_name_p(string name) {
  return (!compilation_unit_p(name) && strstr(name, MODULE_SEP_STRING) == NULL);
}


/* Check if the given name is a static module name.
 */
bool static_module_name_p(string name)
{
  /* An entity is a static module if its name contains the FILE_SEP_STRING
     but the last one is not the last character of the name string */
  /* FI: I doubt this is true. Maybe if you're sure name is the name of a module? */
  return (!compilation_unit_p(name) && strstr(name, FILE_SEP_STRING) != NULL);
}


/* Check if the given module entity is a static module.
 */
bool static_module_p(entity e) {
  return static_module_name_p(entity_name(e));
}


/* Check if the given name is a compilation unit internal name.
 */
bool compilation_unit_p(string module_name) {
  /* A module name is a compilation unit if and only if its last character is
     FILE_SEP */
  if (module_name[strlen(module_name)-1]==FILE_SEP)
    return TRUE;
  return FALSE;
}


/* Check if the given module entity is a compilation unit.
 */
bool compilation_unit_entity_p(entity e) {
  return compilation_unit_p(entity_name(e));
}


bool
variable_in_module_p2(v,m)
entity v;
entity m;
{
    bool in_module_1 = 
	same_string_p(module_local_name(m), entity_module_name(v));
    list decl =  code_declarations (value_code(entity_initial (m)));

    bool in_module_2 = entity_is_argument_p(v,decl);

    ifdebug(8) {
	if (!in_module_1 || !in_module_2) {
	    pips_debug(8, "variable %s not in declarations of %s\n", 
		       entity_name(v),entity_name(m));
	    dump_arguments(decl);
	}
    }
    return (in_module_1 && in_module_2);
}


void 
variable_declaration_verify(reference ref)
{

    if (!variable_in_module_p2(reference_variable(ref),checked_module))
    { 	module_coherent_p = FALSE;
	fprintf(stderr,
			   "variable non declaree %s\n",
			   entity_name(reference_variable(ref)));
    }
}

void 
symbolic_constant_declaration_verify(call c)
{
    
    if (value_symbolic_p(entity_initial(call_function(c)))) {
	if (!variable_in_module_p2(call_function(c),checked_module))
	{    module_coherent_p = FALSE;
	     ifdebug(4) fprintf(stderr,
				"variable non declaree %s\n",
				entity_name(call_function(c)));
	 }
    }
}

void 
add_non_declared_reference_to_declaration(reference ref)
{
    entity var = reference_variable(ref);
    /* type t = entity_type(var); */

    if (!variable_in_module_p2(var,checked_module))
    { 
	value val = entity_initial(checked_module);
	code ce = value_code(val);
	list decl =  code_declarations (ce);
	entity ent= entity_undefined;
	string name=strdup(concatenate(
	    module_local_name(checked_module), 
	    MODULE_SEP_STRING,
	    entity_local_name(var), 
	    NULL));
	if ((ent = gen_find_tabulated(name,entity_domain)) ==entity_undefined)
	{
	    pips_debug(8, "ajout de la variable symbolique %s au module %s\n",
		       entity_name(var), entity_name(checked_module)); 

	    ent =  make_entity(name,
			       copy_type(entity_type(var)),
			       copy_storage(entity_storage(var)),
			       copy_value(entity_initial(var)));

	    code_declarations(ce) = gen_nconc(decl,
					      CONS(ENTITY, ent, NIL));
	}
    }
}

void 
add_symbolic_constant_to_declaration(call c)
{    
    if (value_symbolic_p(entity_initial(call_function(c)))) {
	value val = entity_initial(checked_module);
	code ce = value_code(val);
	list decl =  code_declarations (ce);
	entity ent= entity_undefined;
	string name=strdup(concatenate(module_local_name(checked_module), 
				       MODULE_SEP_STRING,
				       entity_local_name(call_function(c)), NULL));
	if ((ent = gen_find_tabulated(name,entity_domain)) == entity_undefined) {
	    ifdebug(8) 
		(void) fprintf(stderr,"ajout de la variable symbolique %s au module %s\n",
			       entity_name(call_function(c)), 
			       entity_name(checked_module)); 

	    ent =  make_entity( name,
				copy_type(entity_type(call_function(c))),
			       copy_storage(entity_storage(call_function(c))),
			       copy_value(entity_initial(call_function(c))));

	    code_declarations(ce) = gen_nconc(decl,
					      CONS(ENTITY, ent, NIL));
	}
    }
}

bool 
variable_declaration_coherency_p(entity module, statement st)
{
    module_coherent_p = TRUE;
    checked_module = module;
    gen_multi_recurse(st,
		reference_domain,
		gen_true,
		add_non_declared_reference_to_declaration, NULL);
    module_coherent_p = TRUE;
    gen_multi_recurse(st,
		call_domain,
		gen_true,
		add_symbolic_constant_to_declaration, NULL);
    checked_module = entity_undefined;
    return module_coherent_p;
}

/********************************************************* CLEAN DECLARATION */

/* new and as efficient as possible version. FC 08/06/94
 */

/* should be in Newgen? */
#ifndef bool_undefined
#define bool_undefined ((bool) (-1))
#define bool_undefined_p(b) ((b)==bool_undefined)
#endif

/* global summary of referenced variables.
 * used to clean commons on the whole by hpfc...
 */
GENERIC_GLOBAL_FUNCTION(referenced_variables, entity_int)
GENERIC_LOCAL_FUNCTION(declared_variables, entity_int)

/* the list is needed to insure a deterministic scanning, what would be
 * rather random over installations thru hash_table_map for instance.
 */
GENERIC_LOCAL_FUNCTION(referenced_variables_in_list, entity_int)
static list /* of entity */ referenced_variables_list = NIL;

static void
store_this_entity(entity var)
{
    message_assert("defined entity", !entity_undefined_p(var));
    pips_debug(5, "ref to %s\n", entity_name(var));
    if (!bound_referenced_variables_in_list_p(var))
    {
	storage s = entity_storage(var);
	pips_debug(4, "new reference to %s (storage: %d)\n",
		   entity_name(var), storage_tag(s));
	referenced_variables_list =
	    CONS(ENTITY, var, referenced_variables_list);
	store_or_update_referenced_variables(var, TRUE);
	store_referenced_variables_in_list(var, TRUE);

	if (storage_ram_p(s) &&
           !bound_referenced_variables_in_list_p(ram_section(storage_ram(s))))
	{
	    /* the COMMON is also marqued as referenced...
	     * as well as its variables: one => all primary members...
	     */
	    entity
		common = ram_section(storage_ram(s)),
		module = get_current_module_entity();
	    store_this_entity(common);
	    if (!SPECIAL_COMMON_P(common))
            {
                list /* of entity */ lc =
                    common_members_of_module(common, module, TRUE);
		MAP(ENTITY, e, store_this_entity(e), lc);
                gen_free_list(lc);
	    }
	}
    }
}

/* it is an EXTERNAL or a PARAMETER */
static bool
declarable_function_p(entity f)
{
    type t = entity_type(f);
    value v = entity_initial(f);
    storage s = entity_storage(f);

    return type_functional_p(t) && 
	(((value_code_p(v) || value_unknown_p(v) /* not parsed callee */) && 
	 storage_rom_p(s) && 
	 !type_void_p(functional_result(type_functional(t)))) ||
	 value_symbolic_p(v));
}

static void
store_a_call_function(call c)
{
    entity called = call_function(c);
    pips_debug(5, "call to %s\n", entity_name(called));
    if (declarable_function_p(called)) store_this_entity(called);
}

static void 
store_a_referenced_variable(reference ref)
{
    store_this_entity(reference_variable(ref));
}

static void 
store_the_loop_index(loop l)
{
    store_this_entity(loop_index(l));
}

static void
mark_referenced_entities(void * p)
{
    gen_multi_recurse(p,
	call_domain, gen_true, store_a_call_function,
	reference_domain, gen_true, store_a_referenced_variable,
	loop_domain, gen_true, store_the_loop_index,
	NULL);    
}

/*************************************************** UPDATE DECLARATION LIST */
/* this part is not really convincing. it is just a fix to add
 * dependent parameters and perform a transitive closure...
 */
static void update_new_declaration_list(entity, list *);
static list * current_new_declaration = NULL;
static void new_decl_call(call c)
{
    entity called = call_function(c);
    if (declarable_function_p(called))
	update_new_declaration_list(called, current_new_declaration);
}
static void new_decl_ref(reference r)
{
    update_new_declaration_list(reference_variable(r), 
				current_new_declaration);
}

static void
recursive_update_new_decls(void * p)
{
    gen_multi_recurse(p,
		      call_domain, gen_true, new_decl_call,
		      reference_domain, gen_true, new_decl_ref,
		      NULL);
}

static void
update_new_declaration_list(
    entity var,
    list /* of entity */ *pl)
{
    value v = entity_initial(var);
    if (bound_declared_variables_p(var)) return; /* already there */
    /* else let us add it.
     */
    *pl = CONS(ENTITY, var, *pl);
    store_or_update_declared_variables(var, TRUE);

    if (value_symbolic_p(v))
    {
	/* recursively add dependent parameters... 
	 * maybe the transitive closure could be computed more cleanly?
	 */
	current_new_declaration = pl;
	recursive_update_new_decls(symbolic_expression(value_symbolic(v)));
    }
}

static int 
gen_find_occurence(list l, void * c)
{
    int i;
    for (i=0; l; i++, POP(l))
	if (CHUNK(CAR(l))==c) return i;
    return -1;
}

static list initial_order = NIL;
static int 
cmp(const void * x1, const void * x2)
{
    entity e1 = * (entity*) x1, e2 = * (entity*) x2;
    int o1 = gen_find_occurence(initial_order, e1),
	o2 = gen_find_occurence(initial_order, e2);

    if (o1==-1)
    {
	if (o2==-1)
	    return strcmp(entity_name(e1), entity_name(e2));
	else
	    return -1;
    }
    else 
    {
	if (o2==-1)
	    return 1;
	else
	    return o1-o2;
    }
    
}
static void 
sort_with_initial_order(list l, list old)
{
    initial_order = old;
    gen_sort_list(l, cmp);
    initial_order = NIL;
}

/* similar to gen_list_and but specialized for
 * keeping reference of otherwise deleted expression
 */
static list
gen_list_and_keep_expression(list * a,
	     list b,list statements)
{
    if (ENDP(*a))
        return statements;

    if (!gen_in_list_p(CHUNK(CAR(*a)), b)) {
        /* This element of a is not in list b: delete it
         * but first check that assignment have no side effect !
         */

        entity e = ENTITY(CAR(*a));
        value v = entity_initial(e);
        /* look for operations with poosible side-effect
         */
        if( v != NULL && value_expression_p(v) )
        {
            /* build simple statement
             */
            expression expr = copy_expression(value_expression(v));
            statement s = instruction_to_statement( make_instruction_expression(expr) );
            statements=CONS(STATEMENT, s , statements );
            *a = CDR(*a);
        }
        else
        {
            cons *aux = *a;
            *a = CDR(*a);
            CAR(aux).p = NULL;
            CDR(aux) = NULL;
            free(aux);
        }
        return gen_list_and_keep_expression(a, b, statements);
    }
    else {
        return gen_list_and_keep_expression(&CDR(*a), b, statements);
    }
}

/* declare those statically in order to get the same behavior between fortran and c parsing
 */
static list current_included_entities = NIL;

/* real implementation of insure_global_declaration_coherency
 * with ugly switches to handle both fortran and C
 */
static void
insure_global_declaration_coherency_helper(statement stat)
{
    list decl, new_decl = NIL;
    decl=(prettyprint_is_fortran?
            entity_declarations(get_current_module_entity()):
            statement_declarations(stat));

    init_declared_variables();
    init_referenced_variables_in_list();
    referenced_variables_list = NIL;
    pips_assert("ref var initialized", !referenced_variables_undefined_p());

    if (current_included_entities) MAP(ENTITY, e, store_this_entity(e), current_included_entities);

    pips_debug(3, "tagging references\n");
    mark_referenced_entities(stat);

    /* formal arguments are considered as referenced!
     */
    MAP(ENTITY, var, 
	if (storage_formal_p(entity_storage(var)))
	    store_this_entity(var),
	decl);

    /* common variables with associated data are considered referenced,
     * because the DATA must not be dropped.
     */
    MAP(ENTITY, var,
    { 
	value v = entity_initial(var);
	storage s = entity_storage(var);
	if (type_variable_p(entity_type(var)) &&
	    !value_undefined_p(v) && value_constant_p(v) &&
	    !storage_undefined_p(s) && storage_ram_p(s) &&
	    !SPECIAL_COMMON_P(ram_section(storage_ram(s))))
	    store_this_entity(var);
    },
	decl);

    /* checks each declared variable for a reference.
     */
    MAP(ENTITY, var,
    {
	if (bound_referenced_variables_in_list_p(var) &&
	    !bound_declared_variables_p(var))
	{
	    pips_debug(3, "declared %s referenced, kept\n",
		       entity_name(var));
	    update_new_declaration_list(var, &new_decl);
	    if (entity_variable_p(var))
		mark_referenced_entities(entity_type(var));
	}
	else
	{
	    pips_debug(7, "declared %s not referenced, dropped\n",
		       entity_name(var));
	}
    },
	 decl);

    /* checks each referenced variable for a declaration.
     */
    MAP(ENTITY, var,
    {
	if (!bound_declared_variables_p(var))
	{
	    pips_debug(3, "referenced %s added\n", entity_name(var));
	    update_new_declaration_list(var, &new_decl);
	}
    },
	referenced_variables_list);


    /* re-sorted as specified by the initial order...
     */
    sort_with_initial_order(new_decl, decl);

    /* commit changes
     */
    if( prettyprint_is_fortran) {
        entity_declarations(get_current_module_entity()) = new_decl;
    }
    else {
        /* only remove elements, never add !
         * new_decl may contain entities declared in deeper statements
         */
        list head = gen_list_and_keep_expression(&decl,new_decl,NIL);
        statement_declarations(stat) = decl;
        /* add extra statements if needed
         */
        if( head != NIL )
        {
            head=gen_nreverse(head);
            instruction i = statement_instruction(stat);
            switch( instruction_tag(i) )
            {
                case is_instruction_sequence:
                    {
                        /* insert new statements in front of thoses
                        */
                        sequence s = instruction_sequence(i);
                        head = gen_nconc(head, sequence_statements(s) );
                        sequence_statements(s) = head ;
                    } break;
                default:
                    pips_assert("this instruction should not provide extra declaration ?!",false);

            };

        }

    }

    ifdebug(2) {
	pips_debug(2, "resulting declarations for %s: ", entity_name(get_current_module_entity()));
	MAP(ENTITY, e, fprintf(stderr, "%s, ", entity_name(e)), new_decl);
	fprintf(stderr, "\n");
    }

    /* the temporaries are cleaned
     */
    gen_free_list(prettyprint_is_fortran?decl:new_decl);
    close_declared_variables();
    close_referenced_variables_in_list();
    gen_free_list(referenced_variables_list), referenced_variables_list = NIL;

}


/******************************************************** EXTERNAL INTERFACE */


/* remove unused definition from codes
 * use legacy implementation for fortran
 * c implementation should work
 * *but* it does *not* handle the following case
 * int a =y++; //a is unused and will be removed
 * int b=y; // b is used but will be declared before y++ is applied
 */
void 
insure_global_declaration_coherency(
    entity module,          /* the module whose declarations are considered */
    statement stat,         /* the statement where to find references */
    list /* of entity */ le /* added entities, for includes... */)
{
    debug_on("RI_UTIL_DEBUG_LEVEL");
    pips_debug(2, "Processing module %s\n", entity_name(module));

    /* set globals prior to processing, unset them after processing to be safe*/
    current_included_entities=le;
    
    entity saved_module = get_current_module_entity(); /* SG: why is this not set to module ? */
    reset_current_module_entity();
    set_current_module_entity(module);
    if( prettyprint_is_fortran ) insure_global_declaration_coherency_helper(stat);
    else gen_recurse( stat, statement_domain, gen_true, insure_global_declaration_coherency_helper);
    current_included_entities=NIL;
    reset_current_module_entity();
    set_current_module_entity(saved_module);

    debug_off();
}

void
insure_declaration_coherency(
    entity module,          /* the module whose declarations are considered */
    statement stat,         /* the statement where to find references */
    list /* of entity */ le /* added entities, for includes... */)
{
    init_referenced_variables();
    insure_global_declaration_coherency(module, stat, le);
    close_referenced_variables();
}

/*  to be called if the referenced map need not be shared between modules
 *  otherwise the next function is okay.
 */
void 
insure_declaration_coherency_of_module(
    entity module,
    statement stat)
{
    insure_declaration_coherency(module, stat, NIL);
}


/****************************************************** DECLARATION COMMENTS */

/*  Look for the end of header comments: */
static string
get_end_of_header_comments(string the_comments)
{
    string end_of_comment = the_comments;
    
    for(;;) {
	string next_line;
	/* If we are at the end of string or the line is not a comment
           (it must be a PROGRAM, FUNCTION,... line), it is the end of
           the header comment: */
	if (!comment_string_p(end_of_comment))
	    break;

	if ((next_line = strchr(end_of_comment, '\n')) != NULL
	    || *end_of_comment !=  '\0')
	    /* Look for next line */
	    end_of_comment = next_line + 1;
	else
	    /* No return char: it is the last line and it is a
               comment... Should never happend in a real program
               without any useful header! */
	    pips_assert("get_end_of_header_comments found only comments!",
			FALSE);
    }
    return end_of_comment;
}


/* Get the header comments (before PROGRAM, FUNCTION,...) from the
   text declaration: */
sentence
get_header_comments(entity module)
{
    int length;
    string end_of_comment;
    string extracted_comment;
    /* Get the textual header: */
    string the_comments = code_decls_text(entity_code(module));

    end_of_comment = get_end_of_header_comments(the_comments);
    /* Keep room for the trailing '\0': */
    length = end_of_comment - the_comments;
    extracted_comment = (string) malloc(length + 1);
    (void) strncpy(extracted_comment, the_comments, length);
    extracted_comment[length] = '\0';
    return make_sentence(is_sentence_formatted, extracted_comment);  
}


/* Get all the declaration comments, that are comments from the
   PROGRAM, FUNCTION,... stuff up to the end of the declarations: */
sentence
get_declaration_comments(entity module)
{
    string comment;
    string old_extracted_comments;
    string extracted_comments = strdup("");
    /* Get the textual header: */
    string the_comments = code_decls_text(entity_code(module));
    
    comment = get_end_of_header_comments(the_comments);
    /* Now, gather all the comments: */
    for(;;) {
	string next_line;
	if (*comment == '\0')
	    break;
	next_line = strchr(comment, '\n');
	if (comment_string_p(comment)) {
	    string the_comment;
	    /* Add it to the extracted_comments: */
	    if (next_line == NULL)
		/* There is no newline, so this is the last
		   comment line: */
		the_comment = strdup(comment);
	    else {
		/* Build a string that holds the comment: */
		the_comment = (string) malloc(next_line - comment + 2);
		(void) strncpy(the_comment, comment, next_line - comment + 1);
		the_comment[next_line - comment + 1] = '\0';
	    }
	    old_extracted_comments = extracted_comments;
	    extracted_comments = strdup(concatenate(old_extracted_comments,
						    the_comment,
						    NULL));
	    free(old_extracted_comments);
	    free(the_comment);
	}
	/* Have a look to next line if any... */
	if (next_line == NULL)
	    break;
	comment = next_line + 1;
    }
    
    return make_sentence(is_sentence_formatted, extracted_comments);
}

/* list module_formal_parameters(entity func)
 * input    : an entity representing a function.
 * output   : the unsorted list (of entities) of parameters of the function "func".
 * modifies : nothing.
 * comment  : Made from "entity_to_formal_integer_parameters()" that considers 
 *            only integer variables.
 */
list 
module_formal_parameters(entity func)
{
    list formals = NIL;
    list decl = list_undefined;

    pips_assert("func must be a module",entity_module_p(func));

    decl = code_declarations(entity_code(func));
    MAP(ENTITY, e, 
     {
       /* Dummy parameters should have been filtered out of the
	  declarations by the parser, but let's be careful here. As a
	  consequence, dummy parameters cannot be retrieved, except
	  thru the type. */
       if(!dummy_parameter_entity_p(e) && storage_formal_p(entity_storage(e)))
	     formals = CONS(ENTITY, e, formals);
     },
	 decl);
    
    return (formals);
}

/* Number of user declaration lines for a module */
int
module_to_declaration_length(entity func)
{
    value v = entity_initial(func);
    code c = code_undefined;
    int length = 0;

    if(!value_undefined_p(v)) {
	if(value_code_p(v)) {
	    string s = string_undefined;

	    c = value_code(v);
	    s = code_decls_text(c);
	    if(string_undefined_p(s)) {
		/* Is it allowed? The declaration text may be destroyed or
		 * may not exist when a module is synthesized or heavily
		 * transformed.
		 */
		length = 0;
	    }
	    else {
		char l;
		while((l=*s++)!= '\0')
		    if(l=='\n')
			length++;
	    }
	}
	else {
	    pips_internal_error("Entity %s is not a module", 
				entity_name(func));
	}
    }
    else {
	/* Entity func has not been parsed yet */
	length = -1;
    }

    return length;
}

/* Find all references in the declaration list */
list declaration_supporting_references(list dl)
{
  list srl = NIL;

  /* FI: for efficiency, the type cache used in
     type_supporting_references() should be moved up here */
  MAP(ENTITY, v,
  {
    if( ! SPECIAL_AREA_P(v))
    {
        type t = entity_type(v);
        /* FI: we should also look up the initial values */
        srl = type_supporting_references(srl, t);
    }
  }, dl);

  return srl;
}

/* The function itself is not in its declarations. Maybe, it should be changed in the parser?

   A new list is allocated to avoid sharing with code_declarations.
 */
list module_all_declarations(entity m)
{
  list dl = CONS(ENTITY, m, gen_copy_seq(code_declarations(value_code(entity_initial(m)))));

  return dl;
}

/*
 *  that is all
 */

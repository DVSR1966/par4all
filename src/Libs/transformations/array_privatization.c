/* regions package :  Be'atrice Creusillet, october 1995
 *
 * array_privatization.c
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * This File contains the functions computing the private regions.
 *
 * $RCSfile: array_privatization.c,v $ (version $Revision$)
 * $Date: 1997/07/21 13:09:02 $, 
 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "genC.h"
#include "ri.h"
#include "database.h"
#include "makefile.h"
#include "ri-util.h"
#include "control.h"
#include "constants.h"
#include "misc.h"
#include "parser_private.h"
#include "syntax.h"
#include "top-level.h"
#include "text-util.h"
#include "text.h"
#include "properties.h"
#include "pipsmake.h"
#include "transformer.h"
#include "semantics.h"
#include "effects.h"
#include "regions.h"
#include "pipsdbm.h"
#include "resources.h"
#include "prettyprint.h"

/*********************************************************************************/
/*********************************************************************************/
/* 1-  PRIVATIZABILITY DETECTION  && LOOP PRIVATISATION                          */
/*********************************************************************************/
/*********************************************************************************/

/*********************************************************************************/
/* USEFUL VARIABLES AND ACCESS FUNCTIONS                                         */
/*********************************************************************************/

/* global static variable local_regions_map, and its access functions */
GENERIC_GLOBAL_FUNCTION(private_effects, statement_effects)
GENERIC_GLOBAL_FUNCTION(copy_out_effects, statement_effects)

/* statement stack */
DEFINE_LOCAL_STACK(current_stmt, statement)

/* what sort of privatization? */
static bool store_as_regions = FALSE;
static bool store_as_loop_locals = TRUE;
static bool privatize_sections = FALSE;
static bool copy_in = FALSE;
static bool copy_out = FALSE;


/* =============================================================================== 
 *
 * INTRAPROCEDURAL PRIVATE REGIONS ANALYSIS
 *
 * =============================================================================== */

static void private_regions_of_module_statement(statement module_stat);
static void private_regions_of_module_loops(statement module_stat);
static bool privatizer(char *module_name);

/* void array_privatizer(char *module_name) 
 * input    : the name of the current module
 * output   : nothing.
 * modifies : computes the local regions of a module.
 * comment  : local regions can contain local variables.
 */
bool array_privatizer(char *module_name)
{
    store_as_regions = FALSE;
    store_as_loop_locals = TRUE;
    privatize_sections = FALSE;
    copy_in = FALSE;
    copy_out = FALSE;
    return( privatizer(module_name) );
}


/* void array_section_privatizer(char *module_name) 
 * input    : the name of the current module
 * output   : nothing.
 * modifies : computes the local regions of a module.
 * comment  : local regions can contain local variables.
 */
bool array_section_privatizer(char *module_name)
{
    store_as_regions = TRUE;
    store_as_loop_locals = TRUE;
    privatize_sections = TRUE;
    copy_in = FALSE;
    copy_out = get_bool_property("ARRAY_SECTION_PRIV_COPY_OUT");
    return( privatizer(module_name) );    
}

/* void privatizer(char *module_name) 
 * input    : the name of the current module
 * output   : nothing.
 * modifies : computes the local regions of a module.
 * comment  : local regions can contain local variables.
 */
static bool privatizer(char *module_name)
{
    entity module;
    statement module_stat;

    pips_assert("Coyp-in not implemented.\n", !copy_in);
    pips_assert("No array section privatization if we do not store as regions.\n", 
		store_as_regions || ! privatize_sections);
    pips_assert("No copy-in or copy-out if we do not privatize array sections.\n",
		privatize_sections || (!copy_in && !copy_out) );


    if (!same_string_p(rule_phase(find_rule_by_resource("REGIONS")),
		       "MUST_REGIONS"))
	pips_user_warning("\nMUST REGIONS not selected - "
			  "Do not expect wonderful results\n");


    /* set and get the current properties concerning regions */
    set_bool_property("MUST_REGIONS", TRUE);
    set_bool_property("EXACT_REGIONS", TRUE);
    get_regions_properties();

    /* Get the code of the module. */
    set_current_module_statement( (statement)
	db_get_memory_resource(DBR_CODE, module_name, TRUE) );
    module_stat = get_current_module_statement();
    
    /* Get the transformers and preconditions of the module. (Necessary ?) */
    set_transformer_map( (statement_mapping) 
	db_get_memory_resource(DBR_TRANSFORMERS, module_name, TRUE) );
    set_precondition_map( (statement_mapping) 
	db_get_memory_resource(DBR_PRECONDITIONS, module_name, TRUE) );

    /* Get the READ, WRITE, IN and OUT regions of the module */
    set_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_REGIONS, module_name, TRUE));
    set_invariant_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_INV_REGIONS, module_name, TRUE));
    set_in_effects((statement_effects) 
	db_get_memory_resource(DBR_IN_REGIONS, module_name, TRUE));
    set_invariant_in_effects((statement_effects) 
	db_get_memory_resource(DBR_INV_IN_REGIONS, module_name, TRUE));
    set_out_effects((statement_effects) 
	db_get_memory_resource(DBR_OUT_REGIONS, module_name, TRUE));

    /* predicates defining summary regions from callees have to be 
       translated into variables local to module */
    set_current_module_entity( local_name_to_top_level_entity(module_name) );
    module = get_current_module_entity();

    set_cumulated_rw_effects((statement_effects)
	   db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE) );
    module_to_value_mappings(module);


    /* initialisation of private maps */
    if (store_as_regions)
    {
	init_private_effects();
	init_copy_out_effects();
    }

    debug_on("ARRAY_PRIVATIZATION_DEBUG_LEVEL");
    pips_debug(1, "begin\n");

    /* Compute the private regions of the module and of its loops. */
    private_regions_of_module_statement(module_stat); 
    private_regions_of_module_loops(module_stat);

    pips_debug(1, "end\n");
    debug_off();

    if (store_as_regions)
    {
	DB_PUT_MEMORY_RESOURCE(DBR_PRIVATIZED_REGIONS, 
			       strdup(module_name),
			       (char*) get_private_effects());
	
	DB_PUT_MEMORY_RESOURCE(DBR_COPY_OUT_REGIONS, 
			       strdup(module_name),
			       (char*) get_copy_out_effects());
	
    }

    DB_PUT_MEMORY_RESOURCE(DBR_CODE, strdup(module_name), module_stat);

    reset_current_module_entity();
    reset_current_module_statement();
    reset_transformer_map();
    reset_precondition_map();
    reset_cumulated_rw_effects();
    reset_rw_effects();
    reset_invariant_rw_effects();
    reset_in_effects();
    reset_invariant_in_effects();
    reset_out_effects();
    if (store_as_regions)
    {
	reset_private_effects();
	reset_copy_out_effects();
    }

    return(TRUE);
}



static void private_regions_of_module_statement(module_stat)
statement module_stat;
{
    list l_priv, l_cand, l_out_cand;
    list l_write = regions_dup
	(regions_write_regions(load_statement_local_regions(module_stat))); 
    list l_in = regions_dup(load_statement_in_regions(module_stat));
    list l_out = regions_dup(load_statement_out_regions(module_stat));
    

    pips_debug(1,"begin\n");

    pips_debug(2, "CAND = W -inf IN \n");       
    l_cand = RegionsInfDifference(l_write, l_in, w_r_combinable_p);

    pips_debug(2, "OUT_CAND = CAND inter OUT\n");
    l_out_cand = RegionsIntersection(regions_dup(l_cand), l_out, w_w_combinable_p);

    pips_debug(2, "PRIV = CAND -inf OUT_CAND\n");
    l_priv = RegionsInfDifference(l_cand, l_out_cand, w_w_combinable_p);
    
    if (store_as_regions)
    {
	store_private_effects(module_stat, make_effects(l_priv));
	store_copy_out_effects(module_stat, make_effects(NIL));
    }
    
    pips_debug(1,"end\n");
}


static bool stmt_filter(statement s);
static void private_regions_of_statement(statement s);
static bool private_regions_of_loop(loop l);


/* static void private_regions_of_module_loops(statement module_stat)
 * input    : the current statement.
 * output   : 
 * modifies : 
 * comment  : 
 */
static void private_regions_of_module_loops(statement module_stat)
{

    make_current_stmt_stack();

    pips_debug(1,"begin\n");
    
    gen_multi_recurse(module_stat, 
		      statement_domain, stmt_filter, private_regions_of_statement,
		      loop_domain, private_regions_of_loop, gen_null,
		      NULL); 
    

    pips_debug(1,"end\n");

    free_current_stmt_stack();

}
 

static bool stmt_filter(s)
statement s;
{
    pips_debug(1, "statement %03d\n", statement_number(s));
    
    current_stmt_push(s);
    pips_debug(1, "end\n");
    return(TRUE);
}

static void private_regions_of_statement(s)
statement s;
{
    pips_debug(1, "statement %03d\n", statement_number(s));

    if (store_as_regions && !bound_private_effects_p(s))
    {
	pips_debug(6, "non-loop body statement, storing NIL regions.\n");
	store_private_effects(s, make_effects(NIL));
	store_copy_out_effects(s, make_effects(NIL));
    }

    current_stmt_pop();

    pips_debug(1, "end\n");

}


static bool private_regions_of_loop(l)
loop l;
{
    statement b = loop_body(l);
    transformer loop_trans = load_statement_transformer(current_stmt_head());
    list l_cand, l_loc, l_out_priv, l_out_priv_tmp, l_locals, 
         l_loc_i, l_loc_i_prime;
    list l_write, l_in, l_out, l_tmp, l_cand_tmp;
    entity i = loop_index(l);
    entity i_prime = entity_to_intermediate_value(i);
    Psysteme sc_loop_prec;
    Pcontrainte contrainte;

    pips_debug(1, "begin, statement %03d\n", 
	       statement_number(current_stmt_head()));


    /* first get the write, in and out regions, invariant if available */
    l_write = regions_dup(regions_write_regions(load_statement_inv_regions(b)));
    l_in = regions_to_write_regions(regions_dup(load_statement_inv_in_regions(b)));
    l_out = regions_dup(load_statement_out_regions(b));

    project_regions_with_transformer_inverse(l_out, 
				     loop_trans, 
				     CONS(ENTITY, i, NIL));
    ifdebug(2)
    {
	pips_debug(3, "W(i) before: \n");
	print_regions(l_write);
	pips_debug(3, "IN(i) before: \n");
	print_regions(l_in);
	pips_debug(3, "OUT(i) before: \n");
	print_regions(l_out);
    }


    if (privatize_sections)
    {
	/* LOC(i) = W(i) -inf IN(i) */
	l_tmp = regions_dup(l_in);
	l_loc = RegionsInfDifference(l_write, l_tmp, w_w_combinable_p);
	
	ifdebug(3)
	{
	    pips_debug(3, "LOC(i) = W(i) -inf IN(i)\n");
	    print_regions(l_loc);
	}
    }
    else
    {
	/* LOC(i) = W(i) -
	   {les re'gions correspondant a` des tableaux dans IN(i)}*/
	l_tmp = regions_dup(l_in);
	l_loc = RegionsEntitiesInfDifference(l_write, l_tmp, w_w_combinable_p);

	ifdebug(3)
	{
	    pips_debug(3, "LOC(i) = W(i) -entities_inf IN(i)\n");
	    print_regions(l_loc);
	}
    }
    
    if (get_bool_property("ARRAY_PRIV_FALSE_DEP_ONLY"))
    {
	/* Keep only arrays that induce false dependences between iterations   
	 * that is to say arrays such that LOC(i) inter LOC(i', i'<i) != empty_set 
	 */
	sc_loop_prec = sc_loop_proper_precondition(l);
	
	/* we make and LOC(i, i'<i) and LOC(i', i'<i) */
	l_loc_i_prime = regions_dup(l_loc);
	l_loc_i = regions_dup(l_loc);
	array_regions_variable_rename(l_loc_i_prime, i, i_prime);
	contrainte = contrainte_make(vect_make(VECTEUR_NUL, 
					       (Variable) i_prime, VALUE_ONE,
					       (Variable) i, VALUE_MONE,
					       TCST, VALUE_ONE));
	sc_add_inegalite(sc_loop_prec, contrainte_dup(contrainte));
	sc_loop_prec->base = BASE_NULLE;
	sc_creer_base(sc_loop_prec);
	l_loc_i_prime = array_regions_sc_append(l_loc_i_prime, sc_loop_prec, FALSE);
	l_loc_i = array_regions_sc_append(l_loc_i, sc_loop_prec, FALSE);
	sc_rm(sc_loop_prec);
	
	/* LOC_I(i) = LOC(i, i'<i) inter LOC(i', i'<i) */
	l_loc_i = RegionsIntersection(l_loc_i, l_loc_i_prime, w_w_combinable_p);
	
	/* We keep in Loc(i) only the regions that correspond to arrays in l_loc_i,
	 * that is to say arrays that induce false dependences
	 */
	l_loc = RegionsEntitiesIntersection(l_loc, l_loc_i, w_w_combinable_p);

	ifdebug(3)
	{
	    pips_debug(3,
		       "regions on arrays that really induce false dependences:\n");
	    print_regions(l_loc);
	}
    }
    
    if (privatize_sections)
    {
	/* CAND(i) = LOC(i) -inf proj_i'[IN(i')] */
	
	/* first proj_i'[IN(i')] */
	sc_loop_prec = sc_loop_proper_precondition(l);
	array_regions_variable_rename(l_in, i, i_prime);	
	l_in = array_regions_sc_append(l_in, sc_loop_prec, FALSE);
	sc_rm(sc_loop_prec);
	project_regions_along_loop_index(l_in, i_prime, loop_range(l));
	
	/* Then the difference */
	l_cand = RegionsInfDifference(l_loc, l_in, w_w_combinable_p);
	
	ifdebug(3)
	{
	    pips_debug(3, "CAND(i) = LOC(i) -inf proj_i'[IN(i')]\n");
	    print_regions(l_cand);
	}
	
    }
    else
    {
	/* CAND(i) = LOC(i), because LOC(i) inter IN(i') = empty */
	l_cand = l_loc;
	pips_debug(3, "CAND(i) = LOC(i)\n");
    }


    if (copy_out)
    {
	/* OUT_PRIV(i) = CAND(i) inter OUT(i) */
	l_out_priv = RegionsIntersection(regions_dup(l_cand),
					 l_out, w_w_combinable_p);
	
	ifdebug(3)
	{
	    pips_debug(3, "OUT_PRIV(i) = CAND(i) inter OUT(i)\n");
	    print_regions(l_out_priv);
	}

	/* keep only good candidates (those for which the copy-out is either empty
	 * or exactly determined). */
	
	l_out_priv_tmp = l_out_priv;
	while(!ENDP(l_out_priv_tmp))
	{
	    list l_cand_tmp;
	    boolean found;
	    
	    region reg = EFFECT(CAR(l_out_priv_tmp));
	    l_out_priv_tmp = CDR(l_out_priv_tmp);
	    if (!region_must_p(reg)) 
	    { 
		l_cand_tmp = l_cand;
		found = FALSE;
		/* remove the corresponding candidate from l_cand */
		while(!found && !ENDP(l_cand_tmp))
		{
		    region reg_cand = EFFECT(CAR(l_cand_tmp));
		    l_cand_tmp = CDR(l_cand_tmp);
		    if (same_entity_p(region_entity(reg_cand) , region_entity(reg)))
		    {
			/* a` optimiser */
			gen_remove(&l_cand, reg_cand);
			region_free(reg_cand);
			found = TRUE;
		    }
		}
		gen_remove(&l_out_priv, reg);
		region_free(reg);
	    } 
	}		
    }
    else
    {
	/* OUT_PRIV(i) = NIL */
	l_out_priv = NIL;
	/* CAND(i) = CAND(i) -entities_inf OUT(i) */
	l_cand = RegionsEntitiesInfDifference(l_cand, l_out, w_w_combinable_p);

	ifdebug(3)
	{
	    pips_debug(3, "No copy-out: OUT_PRIV(i) = NIL\n");
	    pips_debug(3, "l_cand: \n");
	    print_regions(l_cand);
	}
    }


    /* remove variables in commons not declared in current module */
    l_cand_tmp = l_cand;
    while(!ENDP(l_cand_tmp))
    {
	region reg = REGION(CAR(l_cand_tmp));
	entity reg_ent = region_entity(reg);
	entity ccommon;
	list l_tmp;
	list l_com_ent;
	bool found= FALSE; 

	l_cand_tmp = CDR(l_cand_tmp);
	    
	/* First, we search if the common is declared in the current module;
	 * if not, the variable cannot be privatized, and a warning is issued.
	 */
	if(storage_ram_p(entity_storage(reg_ent)))
	{
	    ccommon = ram_section(storage_ram(entity_storage(reg_ent)));
	    l_com_ent = area_layout(type_area(entity_type(ccommon)));
	    
	    for( l_tmp = l_com_ent; !ENDP(l_tmp) && !found; l_tmp = CDR(l_tmp) )
	    {
		entity com_ent = ENTITY(CAR(l_tmp));
		if (strcmp(entity_module_name(com_ent),
			   module_local_name(get_current_module_entity())) == 0)
		{
		    found = TRUE;
		}
	    }
	    if (!found)	    
	    {
		pips_user_warning("\nCOMMON %s not declared in subroutine %s:\n\t"
				  "variable %s cannot be privatized in loop "
				  "number %03d\n",
				  entity_name(ccommon),
				  module_local_name(get_current_module_entity()),
				  entity_name(reg_ent),
			      statement_number(current_stmt_head()));
		gen_remove(&l_cand, reg);
		
	    }
	}
    }

    l_out_priv = RegionsEntitiesIntersection(l_out_priv, regions_dup(l_cand),
					     w_w_combinable_p);
        
    /* compute loop_locals from l_cand */
    if (store_as_loop_locals)
    {
	gen_free_list(loop_locals(l));
	l_locals = NIL;
	MAP(EFFECT, reg,
	    {		
		l_locals = CONS(ENTITY, region_entity(reg), l_locals);		
	    },
	    l_cand);
	loop_locals(l) = l_locals;   
	
	/* add the loop index */
	loop_locals(l) = CONS(ENTITY, loop_index(l), loop_locals(l));
	
	ifdebug(2)
	{
	    pips_debug(2, "candidate entities: ");
	    print_arguments(loop_locals(l));  
	}
    }

    ifdebug(2)
    {	
	pips_debug(2, "candidate regions:\n");
	print_regions(l_cand);
	/* No copy-in for the moment */
	/* pips_debug(2, "copy-in:\n"); */
	/* print_regions(l_out_priv); */   
	pips_debug(2, "copy-out:\n");
	print_regions(l_out_priv);
    }

    if (store_as_regions)
    {
	store_private_effects(b, make_effects(l_cand));
	store_copy_out_effects(b,make_effects(l_out_priv));
    }
    
    pips_debug(1, "end\n");
    
    return(TRUE);
}


/*********************************************************************************/
/* PRETTYPRINT OF PRIVATIZED REGIONS (AND COPY-OUT)                              */
/*********************************************************************************/
static boolean is_user_view_p = FALSE;
static hash_table nts = hash_table_undefined;
static text text_statement_privatized_array_regions(entity module,
						    int margin,
						    statement stat);
static text get_privatized_regions_text(string module_name,
					bool give_code_p);
static text text_privatized_array_regions(list l_priv, list l_out);

/* bool print_code_privatized_sections(string module_name, list summary_regions)
 * input    : the name of the current module, the name of the region and
 *            summary region resources and the file suffix
 *            the regions are in the global variable local_regions_map.
 * modifies : nothing.
 * comment  : prints the source code with the corresponding privatized and
 *            copy-out regions.	
 */
bool print_code_privatized_regions(string module_name)
{
    char *file_name, *file_resource_name;
    bool success = TRUE;
    /* we print all private variables, even when attached to blocks */
    bool blocks_tmp = get_bool_property("PRETTYPRINT_BLOCKS");
    bool priv_tmp = get_bool_property("PRETTYPRINT_ALL_PRIVATE_VARIABLES");

    file_name = strdup(concatenate(".priv_reg",
                                  get_bool_property
				  ("PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH") ? 
				  GRAPH_FILE_EXT : "",
                                  NULL));
    file_resource_name = get_bool_property("PRETTYPRINT_UNSTRUCTURED_AS_A_GRAPH") ?
	DBR_GRAPH_PRINTED_FILE : 
	    (is_user_view_p ? DBR_PARSED_PRINTED_FILE : DBR_PRINTED_FILE);

    set_bool_property("PRETTYPRINT_BLOCKS", TRUE);
    set_bool_property("PRETTYPRINT_ALL_PRIVATE_VARIABLES", TRUE);
    success = make_text_resource(module_name, file_resource_name,
				 file_name,
				 get_privatized_regions_text(module_name, TRUE));

    set_bool_property("PRETTYPRINT_BLOCKS", blocks_tmp);
    set_bool_property("PRETTYPRINT_ALL_PRIVATE_VARIABLES", priv_tmp);
    free(file_name);
    return(TRUE);
}

static text get_privatized_regions_text(string module_name,
					bool give_code_p)
{
    entity module;
    statement module_stat, user_stat = statement_undefined;
    text txt = make_text(NIL);

    debug_on("ARRAY_PRIVATIZATION_DEBUG_LEVEL");

    set_private_effects((statement_effects) 
	db_get_memory_resource(DBR_PRIVATIZED_REGIONS, module_name, TRUE));

    set_copy_out_effects( (statement_effects) 
	db_get_memory_resource(DBR_COPY_OUT_REGIONS, module_name, TRUE) );


    set_current_module_entity( local_name_to_top_level_entity(module_name));
    module = get_current_module_entity();

    set_current_module_statement((statement) db_get_memory_resource
				 (DBR_CODE, module_name, TRUE));

    module_stat = get_current_module_statement();

    /* To set up the hash table to translate value into value names */       
    set_cumulated_rw_effects((statement_effects)
	  db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));
    module_to_value_mappings(module);


    if(is_user_view_p) 
    {
	user_stat =  (statement)
	    db_get_memory_resource(DBR_PARSED_CODE, module_name, TRUE);

	nts = allocate_number_to_statement();
	nts = build_number_to_statement(nts, module_stat);

	ifdebug(1)
	{
	    print_number_to_statement(nts);
	}
    }

    /* prepare the prettyprinting */
    init_prettyprint(text_statement_privatized_array_regions);

    if (give_code_p)
	/* then code with regions, using text_array_regions */
	MERGE_TEXTS(txt, text_module(module,
				     is_user_view_p? user_stat : module_stat));

    if(is_user_view_p)
    {
	hash_table_free(nts);
	nts = hash_table_undefined;
    }

    close_prettyprint();

    debug_off();

    reset_private_effects();
    reset_copy_out_effects();
    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();

    return txt;
}


/* static text text_statement_privatized_array_regions(entity module, int margin, 
 *                                                     statement stat)
 * output   : a text representing the list of array regions associated with the
 *            statement stat.
 * comment  : if the number of array regions is not nul, then empty lines are
 *            added before and after the text of the list of regions.
 */
static text text_statement_privatized_array_regions(module, margin, stat)
entity module;
int margin;
statement stat;
{
    list l_priv = NIL, l_out = NIL;
    statement s;

    s = is_user_view_p? 
	(statement) hash_get(nts, (char *) statement_number(stat)) :
	stat;
    
    if (s != (statement) HASH_UNDEFINED_VALUE)
    {
	l_priv = effects_effects(load_private_effects(s));
	l_out = effects_effects(load_copy_out_effects(s));
    }
    else
    {
	l_priv = (list) HASH_UNDEFINED_VALUE;
	l_out = (list) HASH_UNDEFINED_VALUE;
    }
    
    return text_privatized_array_regions(l_priv, l_out);
}


/* static text text_privatized_array_regions(list l_priv, list l_out)
 * input    : a list of regions
 * output   : a text representing this list of regions.
 * comment  : if the number of array regions is not nul, and if 
 *            PRETTYPRINT_LOOSE is TRUE, then empty lines are
 *            added before and after the text of the list of regions.
 */
static text text_privatized_array_regions(list l_priv, list l_out)
{
#define PIPS_NORMAL_PREFIX "C"

    text reg_text = make_text(NIL);
    /* in case of loose_prettyprint, at least one region to print? */
    boolean loose_p = get_bool_property("PRETTYPRINT_LOOSE");

    /* First: private sections */
    if (l_priv !=(list) HASH_UNDEFINED_VALUE &&  !ENDP(l_priv))
    {
	if (loose_p)
	{
	    ADD_SENTENCE_TO_TEXT(reg_text, 
				 make_sentence(is_sentence_formatted, 
					       strdup("\n")));
	}
	ADD_SENTENCE_TO_TEXT(reg_text, 
			     make_pred_commentary_sentence
			     (strdup("PRIVATE REGIONS:"),PIPS_NORMAL_PREFIX));
	MERGE_TEXTS(reg_text,text_all_regions(l_priv));

	if (loose_p)
	    ADD_SENTENCE_TO_TEXT(reg_text, 
				 make_sentence(is_sentence_formatted, 
					       strdup("\n")));
    }

    /* Then: copy-out sections */
    if (l_out !=(list) HASH_UNDEFINED_VALUE &&  !ENDP(l_out))
    {
	if (loose_p)
	{
	    ADD_SENTENCE_TO_TEXT(reg_text, 
				 make_sentence(is_sentence_formatted, 
					       strdup("\n")));
	}
	ADD_SENTENCE_TO_TEXT(reg_text, 
			     make_pred_commentary_sentence
			     (strdup("COPY-OUT REGIONS:"),PIPS_NORMAL_PREFIX));
	MERGE_TEXTS(reg_text,text_all_regions(l_out));

	if (loose_p)
	    ADD_SENTENCE_TO_TEXT(reg_text, 
				 make_sentence(is_sentence_formatted, 
					       strdup("\n")));
    }

    return(reg_text);
}

/*********************************************************************************/
/*********************************************************************************/
/* 2-  MODULE PRIVATISATION (VARIABLE REDECLARATION)                             */
/*********************************************************************************/
/*********************************************************************************/

static entity dynamic_area = entity_undefined;

#define PRIVATE_VARIABLE_SUFFIX "_P"
static entity current_old_entity = entity_undefined;
static entity current_new_entity = entity_undefined;

static void
set_current_entities(entity e_old, entity e_new)
{
  current_old_entity = e_old;
  current_new_entity = e_new; 
}

static entity
get_current_old_entity()
{
    return current_old_entity;
}

static entity
get_current_new_entity()
{
    return current_new_entity;
}

static void
reset_current_entities()
{
  current_old_entity = entity_undefined;
  current_new_entity = entity_undefined;
}


static bool
reference_filter(reference ref)
{
    entity e_old = get_current_old_entity();
    entity e_new = get_current_new_entity();

    if (reference_variable(ref) == e_old)
	reference_variable(ref) = e_new;

    return(TRUE);
}

static void 
privatize_entity(entity ent)
{
    entity new_ent;
    string new_ent_name;
    storage ent_storage = entity_storage(ent);
    storage new_ent_storage;
    entity module = get_current_module_entity();

    /* We do not need to privatize local (dynamic) or formal variables */
    if (storage_ram_p(ent_storage) &&
	!dynamic_area_p(ram_section(storage_ram(ent_storage))))
    {
	pips_user_warning("privatizing variable: %s\n",
			  entity_local_name(ent));

	/* Make a new ram entity, similar to the previous one, 
	   but with a new name */
	new_ent_name = copy_string(concatenate(entity_name(ent),
					       PRIVATE_VARIABLE_SUFFIX, NULL));
	
	
	new_ent = make_entity(new_ent_name, 
			      copy_type(entity_type(ent)),
			      storage_undefined,
			      copy_value(entity_initial(ent)));
			      
	
	new_ent_storage = make_storage(is_storage_ram,
				       make_ram(module,
						dynamic_area,
						CurrentOffsetOfArea(dynamic_area,
								    new_ent),
						NIL));
	entity_storage(new_ent) = new_ent_storage;

	/* add this entity to the declarations of the module */
	AddEntityToDeclarations(new_ent, module);
	
	/* replace all references to this entity by a reference to the new entity 
	 */
	set_current_entities(ent, new_ent);
	gen_multi_recurse(get_current_module_statement(),
			  reference_domain, reference_filter, gen_null, NULL);
	reset_current_entities();
    }
}

bool 
declarations_privatizer(char *mod_name)
{
    list l_priv = NIL, l_in, l_out, l_write; 
    statement module_stat;
    entity module;

    if (get_bool_property("ARRAY_SECTION_PRIV_COPY_OUT"))
    {
	pips_user_warning("property ARRAY_SECTION_PRIV_COPY_OUT set to TRUE ; " 
			" COPY OUT not implemented.\n" ); 
    }

    if (!same_string_p(rule_phase(find_rule_by_resource("REGIONS")),
		       "MUST_REGIONS"))
	pips_user_warning("\nMUST REGIONS not selected - "
			  "Do not expect wonderful results\n");


    /* set and get the current properties concerning regions */
    set_bool_property("MUST_REGIONS", TRUE);
    set_bool_property("EXACT_REGIONS", TRUE);
    get_regions_properties();

    /* Get the code of the module. */
    set_current_module_statement( (statement)
	db_get_memory_resource(DBR_CODE, mod_name, TRUE) );
    module_stat = get_current_module_statement();
    set_current_module_entity( local_name_to_top_level_entity(mod_name) );
    module = get_current_module_entity();
    set_cumulated_rw_effects((statement_effects)
	   db_get_memory_resource(DBR_CUMULATED_EFFECTS, mod_name, TRUE));
    module_to_value_mappings(module);
    
    /* sets dynamic_area */
    if (entity_undefined_p(dynamic_area))
    {
       	
	dynamic_area = FindOrCreateEntity(module_local_name(module),
					  DYNAMIC_AREA_LOCAL_NAME); 
    }

    debug_on("ARRAY_PRIVATIZATION_DEBUG_LEVEL");

    /* Privatizable array regions */
    /* For the moment, we only want to privatize whole variables;
     * Thus we only keep those variables which are absolutely not imported 
     * nor exported. 
     * If we want to privatize regions, we can use the ressource PRIVATE_REGIONS.
     */
     /* Get the READ, WRITE, IN and OUT regions of the module */
    set_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_REGIONS, mod_name, TRUE));
    set_in_effects((statement_effects) 
	db_get_memory_resource(DBR_IN_REGIONS, mod_name, TRUE));
    set_out_effects((statement_effects) 
	db_get_memory_resource(DBR_OUT_REGIONS, mod_name, TRUE));
   
    l_write = regions_dup
	(regions_write_regions(load_statement_local_regions(module_stat))); 
    l_in = regions_dup(load_statement_in_regions(module_stat));
    l_out = regions_dup(load_statement_out_regions(module_stat));

    ifdebug(2)
    {
	pips_debug(3, "WRITE regions: \n");
	print_regions(l_write);
	pips_debug(3, "IN regions: \n");
	print_regions(l_in);
	pips_debug(3, "OUT regions: \n");
	print_regions(l_out);
    }

    
    l_priv = RegionsEntitiesInfDifference(l_write, l_in, w_r_combinable_p);
    l_priv = RegionsEntitiesInfDifference(l_priv, l_out, w_w_combinable_p);

    ifdebug(2)
    {
	pips_debug(3, "Private regions: \n");
	print_regions(l_priv);
    }

    /* We effectively perform the privatization */
    MAP(REGION, reg,
	{
	    privatize_entity(region_entity(reg));
	},
	l_priv);

    /* Then we need to clean the declarations */
    /* to be done later or in another phase */

    debug_off();

    DB_PUT_MEMORY_RESOURCE(DBR_CODE, strdup(mod_name), module_stat);

    dynamic_area = entity_undefined;
    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();
    reset_rw_effects();
    reset_in_effects();
    reset_out_effects();
    return( TRUE );
}


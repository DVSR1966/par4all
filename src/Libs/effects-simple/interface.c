/* package simple effects :  Be'atrice Creusillet 5/97
 *
 * File: interface.c
 * ~~~~~~~~~~~~~~~~~
 *
 * This File contains.
 *
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "ri.h"

#include "misc.h"
#include "ri-util.h"
#include "database.h"
#include "resources.h"
#include "pipsdbm.h"
#include "effects-generic.h"
#include "effects-simple.h"

/****************************************************** PIPSMAKE INTERFACES */

/* SPECIFIC INTERFACES */

bool 
cumulated_references(string module_name)
{
    bool ok;
    set_methods_for_cumulated_references();
    ok = rw_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool 
proper_references(string module_name)
{
    bool ok;
    set_methods_for_proper_references();
    ok = proper_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool 
proper_effects(string module_name)
{
    bool ok;
    set_methods_for_proper_simple_effects();
    ok = proper_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool 
summary_effects(string module_name)
{
    bool ok;
    set_methods_for_simple_effects();
    ok = summary_rw_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool 
cumulated_effects(string module_name)
{
    bool ok;
    set_methods_for_simple_effects();
    ok = rw_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool
in_summary_effects(string module_name)
{
    bool ok;
    set_methods_for_simple_effects();
    ok = summary_in_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool
out_summary_effects(string module_name)
{
    return TRUE;
}


bool
in_effects(string module_name)
{
    bool ok;
    set_methods_for_simple_effects();
    ok = in_effects_engine(module_name);
    generic_effects_reset_all_methods();
    return ok;
}

bool
out_effects(string module_name)
{
    return TRUE;
}

/************************************************************* PRETTYPRINTS */

static bool 
print_code_effects(
    string module_name,
    bool is_rw,
    bool is_user_view,
    bool is_attached,
    string resource_name,
    string summary_resource_name,
    string suffix)
{
    bool ok;

    if (is_rw) set_methods_for_rw_effects_prettyprint(module_name);
    else       set_methods_for_inout_effects_prettyprint(module_name);

    set_is_user_view_p(is_user_view);
    set_prettyprint_with_attachments(is_attached);

    ok = print_source_or_code_with_any_effects_engine
	(module_name, resource_name, summary_resource_name, suffix);

    generic_effects_reset_all_methods();
    return ok;
}

bool
print_code_proper_effects(string module_name)
{
    return print_code_effects(module_name, TRUE, FALSE, TRUE, 
			      DBR_PROPER_EFFECTS, string_undefined, ".prop");
}

bool
print_code_cumulated_effects(string module_name)
{
    return print_code_effects(module_name, TRUE, FALSE, TRUE, 
		      DBR_CUMULATED_EFFECTS, DBR_SUMMARY_EFFECTS, ".cumu");
}

bool 
print_code_proper_references(string module_name)
{
    return print_code_effects(module_name, TRUE, FALSE, TRUE, 
		      DBR_PROPER_REFERENCES, string_undefined, ".propref");
}

bool 
print_code_cumulated_references(string module_name)
{
    return print_code_effects(module_name, TRUE, FALSE, TRUE, 
		      DBR_CUMULATED_REFERENCES, string_undefined, ".cumuref");
}

bool
print_code_in_effects(string module_name)
{
    return print_code_effects(module_name, FALSE, FALSE, FALSE, 
		      DBR_IN_EFFECTS, DBR_IN_SUMMARY_EFFECTS, ".ineff");
}

bool
print_code_out_effects(string module_name)
{
    return print_code_effects(module_name, FALSE, FALSE, FALSE, 
		      DBR_OUT_EFFECTS, DBR_OUT_SUMMARY_EFFECTS, ".outeff");
}

bool
print_source_proper_effects(string module_name)
{
    return print_code_effects(module_name, TRUE, TRUE, TRUE, 
		      DBR_PROPER_EFFECTS, string_undefined, ".uprop");
}

bool
print_source_cumulated_effects(string module_name)
{
    return print_code_effects(module_name, TRUE, TRUE, TRUE, 
		      DBR_CUMULATED_EFFECTS, DBR_SUMMARY_EFFECTS, ".ucumu");
}


bool
print_source_in_effects(string module_name)
{
    return print_code_effects(module_name, FALSE, TRUE, FALSE, 
		      DBR_IN_EFFECTS, DBR_IN_SUMMARY_EFFECTS, ".uineff");
}

bool
print_source_out_effects(string module_name)
{
    return print_code_effects(module_name, FALSE, TRUE, FALSE, 
		      DBR_OUT_EFFECTS, DBR_OUT_SUMMARY_EFFECTS, ".uouteff");
}


/********************************************************** OTHER FUNCTIONS */

text
get_text_proper_effects(string module_name)
{
    text t;
    set_is_user_view_p(FALSE);

    t = get_any_effect_type_text(module_name,
				 DBR_PROPER_EFFECTS,
				 string_undefined,
				 FALSE);
    return t;
}


text
get_text_cumulated_effects(string module_name)
{
    text t;
    set_is_user_view_p(FALSE);
    t = get_any_effect_type_text(module_name,
				 DBR_CUMULATED_EFFECTS,
				 DBR_SUMMARY_EFFECTS,
				 FALSE);
    return t;
}


/*********** INTERFACES TO COMPUTE SIMPLE PROPER EFFECTS FROM OTHER PHASES */

/* list proper_effects_of_expression(expression e)
 * input    : an expression and the current context
 * output   : the correpsonding list of effects.
 * modifies : nothing.
 * comment  :	
 */
list 
proper_effects_of_expression(expression e)
{
    list le;
    bool context_stack_defined_p = 
	effects_private_current_context_stack_initialized_p();

    if (!context_stack_defined_p)
    {
	set_methods_for_simple_effects();
	make_effects_private_current_context_stack();
    }

    effects_private_current_context_push(transformer_undefined);
    le = generic_proper_effects_of_expression(e);
    effects_private_current_context_pop();
    if (!context_stack_defined_p)
    {
	free_effects_private_current_context_stack();
    }

    return(le);
}

/* list proper_effects_of_range(range r)
 * input    : an expression and the current context
 * output   : the correpsonding list of effects.
 * modifies : nothing.
 * comment  :	
 */
list 
proper_effects_of_range(range r)
{
    list le;
    bool context_stack_defined_p = 
	effects_private_current_context_stack_initialized_p();

    if (!context_stack_defined_p)
    {
	    set_methods_for_simple_effects();
	    make_effects_private_current_context_stack();
    }
    effects_private_current_context_push(transformer_undefined);

    le = generic_proper_effects_of_range(r);
    effects_private_current_context_pop();

    if (!context_stack_defined_p)
    {
	free_effects_private_current_context_stack();
    }

    return(le);
}

/*************************************************** BACKWARD COMPATIBILITY */

/* called from prettyprint CRAY */
void 
rproper_effects_of_statement(statement s)
{
    set_methods_for_proper_simple_effects();
    proper_effects_of_module_statement(s); 
    return;
}


void 
rcumulated_effects_of_statement(statement s)
{
    init_invariant_rw_effects();
    set_methods_for_simple_effects();
    rw_effects_of_module_statement(s); 
    reset_invariant_rw_effects();
}



/* called from rice */
list 
statement_to_effects(s)
statement s;
{
    list l_eff;

    init_proper_rw_effects();
    init_rw_effects();
    init_invariant_rw_effects();

    debug_on("EFFECTS_DEBUG_LEVEL");
    pips_debug(1, "begin\n");

    set_methods_for_proper_simple_effects();
    proper_effects_of_module_statement(s); 
    set_methods_for_simple_effects();
    rw_effects_of_module_statement(s); 
    l_eff = effects_dup(load_rw_effects_list(s));

    pips_debug(1, "end\n");
    debug_off();

    /* Faudrait faire les free, mais je ne sais pas comment. mail a` fc */
    reset_proper_rw_effects();
    reset_rw_effects();
    reset_invariant_rw_effects();
    generic_effects_reset_all_methods();

    return l_eff;
}

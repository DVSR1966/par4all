/*

  $Id$

  Copyright 1989-2010 MINES ParisTech

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
/* An atomizer that uses the one made by Fabien Coelho for HPFC,
 * and is in fact just a hacked version of the one made by Ronan
 * Keryell...
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"

#include "dg.h"

typedef dg_arc_label arc_label;
typedef dg_vertex_label vertex_label;

#include "graph.h"
#include "ri-util.h"
#include "effects-util.h"
#include "text-util.h"
#include "database.h"
#include "misc.h"
#include "pipsdbm.h"
#include "resources.h"
#include "transformer.h"
#include "semantics.h"
#include "control.h"
#include "transformations.h"
#include "arithmetique.h"

#include "effects-generic.h"
#include "effects-simple.h"
#include "properties.h"
#include "atomizer.h"
#include "preprocessor.h"
#include "properties.h"

#include "expressions.h"

#include "sac-local.h" 

#include "sac.h"

static statement orginal_statement = NULL;


static 
void patch_constant_size(syntax s, bool* patch_all)
{
    switch(syntax_tag(s))
    {
        case is_syntax_call:
            {
                call c = syntax_call(s);
                if(entity_constant_p(call_function(c)) )
                {
                    basic b = variable_basic(type_variable(functional_result(type_functional(entity_type(call_function(c))))));
                       if( basic_int_p(b) && (basic_int(b) == DEFAULT_INTEGER_TYPE_SIZE) )
                            basic_int(b)= 1;
                }
            } break;
        case is_syntax_reference:
            *patch_all = false;
            break;
        default:
            break;
    };
}
static expression current_lhs = expression_undefined;

/* returns the assignment statement if moved, or NULL if not.
 */
statement simd_atomize_this_expression(entity (*create)(entity, basic),
        expression e)
{
    basic bofe = basic_undefined;

    /* it does not make sense to atomize a range...
    */
    if (syntax_range_p(expression_syntax(e))) return NULL;

    /* SG: in case of expression similar to (a+2), if a is a short (or a char ...),
     * the user may expect the result is a short too 
     * the C syntax expect 2 is an int
     * set the property to false if you want to override this behavior
     */
    if( get_bool_property("SIMD_OVERRIDE_CONSTANT_TYPE_INFERENCE") )
    {
        expression etemp = copy_expression(e);
        bool patch_all=true;
        /* force all integer to char, then compute the basic
         * validate only if we can guess the size from something not a constant integer
         */
        gen_context_recurse(etemp,&patch_all,syntax_domain,gen_true,patch_constant_size);
        if( !patch_all )
            bofe=basic_of_expression(etemp);
        free_expression(etemp);
    }

    if(basic_undefined_p(bofe) )
        bofe = basic_of_expression(e);

    if(!basic_undefined_p(bofe)) {
        if (!basic_overloaded_p(bofe))
        {
            entity newvar = (*create)(get_current_module_entity(), bofe);
            expression rhs = make_expression(expression_syntax(e), normalized_undefined);
            normalize_all_expressions_of(rhs);
            statement assign = make_assign_statement(entity_to_expression(newvar),rhs);
            expression_syntax(e) = make_syntax_reference( make_reference(newvar, NIL));

            return assign;
        }

        free_basic(bofe);
    }
    return NULL;
}


/* This function computes the maximum width of all the variables used in a call
 */
static void get_type_max_width(call ca, int* maxWidth)
{
    FOREACH(EXPRESSION, arg,call_arguments(ca))
    {
        syntax s = expression_syntax(arg);

        switch(syntax_tag(s))
        {
            case is_syntax_call:
                {
                    call c = syntax_call(s);

                    entity op = call_function(c);
                    if ( ENTITY_FIELD_P(op) || ENTITY_POINT_TO_P(op))
                    {
                        basic bas = basic_of_expression(binary_call_rhs(c));
                        *maxWidth=MAX(*maxWidth , basic_type_size(bas));
                        free_basic(bas);
                    }
                    else if (!call_constant_p(c))
                        get_type_max_width(c, maxWidth);
                } break;

            case is_syntax_reference:
                {
                    basic bas = basic_of_reference(syntax_reference(s));
                    *maxWidth=MAX(*maxWidth , basic_type_size(bas));
                    free_basic(bas);
                } break;
            default:pips_internal_error("synatx_tag %u not supported yet",syntax_tag(s));

        }
    }

}

/* This function aims at changing the basic size of the left expression of
 * the newly created assign statement
 */
static void change_basic_if_needed(statement stat)
{
	if( statement_call_p(stat) && ENTITY_ASSIGN_P(call_function(statement_call(stat))))
	{
		int maxWidth = -1;
		expression lExp = EXPRESSION(CAR(call_arguments(statement_call(stat))));
		expression rExp = EXPRESSION(CAR(CDR(call_arguments(statement_call(stat)))));

		// Check that the right expression is a call statement
		if(expression_call_p(rExp))
		{

			// Check that the statement can be potentially integrated in a 
			// SIMD statement
			if(match_statement(stat) != NIL)
			{
				get_type_max_width(syntax_call(expression_syntax(rExp)), &maxWidth);
			}

			// If the maxWidth of the right expression is smaller than the width 
			// of the current left expression, then replace the left expression width 
			// by maxWidth
			if(maxWidth > 0)
			{
				basic lExpBasic = expression_basic(lExp);
                maxWidth=MIN(maxWidth,basic_type_size(lExpBasic));
			}
		}
	}
}

/* This function insert stat before orginal_statement in the code
 */
static void simd_insert_statement(statement cs, statement stat)
{
	change_basic_if_needed(stat);
	// If cs is already a sequence, we just need to insert stat in cs
	if(instruction_sequence_p(statement_instruction(cs)))
	{
		instruction_block(statement_instruction(cs)) = gen_insert_before(stat,
				orginal_statement,
				instruction_block(statement_instruction(cs)));
	}
	// If cs is not a sequence, we have to create one sequence composed of
	// cs then orginal_statement
	else
	{
		statement_label(stat) = statement_label(cs);

		orginal_statement = make_statement(entity_empty_label(), 
				statement_number(cs),
				statement_ordering(cs),
				statement_comments(cs),
				statement_instruction(cs),
				statement_declarations(cs),
				NULL,
				statement_extensions(cs));

		statement_instruction(cs) =
			make_instruction_block(CONS(STATEMENT, stat,
						CONS(STATEMENT,
							orginal_statement,
							NIL)));

		statement_label(cs) = entity_empty_label();
		statement_number(cs) = STATEMENT_NUMBER_UNDEFINED;
		statement_ordering(cs) = STATEMENT_ORDERING_UNDEFINED;
		statement_comments(cs) = empty_comments;
		statement_extensions(cs) = empty_extensions ();
	}
}

static
entity sac_make_new_variable(entity module, basic b)
{
    entity e = make_new_scalar_variable(module, copy_basic(b));
    AddLocalEntityToDeclarations(e,module,
            c_module_p(module)?get_current_module_statement():statement_undefined);
    return e;
}
static entity sac_get_current_lhs(entity module, basic b)
{
    entity current_entity = expression_to_entity(current_lhs);
    return entity_undefined_p(current_entity)? sac_make_new_variable(module,b): current_entity;
}

static
void simd_do_atomize(expression ce, statement cs)
{
	syntax s = expression_syntax(ce);
	statement stat =statement_undefined;

	// Atomize expression only if this is a call expression
	if(syntax_call_p(s))
	{
		call cc = syntax_call(s);

		// Atomize expression only if the call is not a constant
		if(FUNC_TO_ATOMIZE_P(cc))
		{
			/* If the current call is not an assign call,
			 * let's atomize the current argument
             * sg: also try to be smart and make reduction appear if any
             */
            bool potential_reduction_p = false;
            FOREACH(EXPRESSION,e,call_arguments(cc))
                if(same_expression_p(e,current_lhs)) potential_reduction_p = true;

			if( (stat = simd_atomize_this_expression(potential_reduction_p?sac_get_current_lhs:sac_make_new_variable, ce)) )
				simd_insert_statement(cs, stat);
		}
	}
}
static bool reference_filter(expression exp, __attribute__((unused)) statement cs)
{
    if( expression_reference_p(exp) ) return get_bool_property("SIMD_ATOMIZER_ATOMIZE_REFERENCE");
    if(expression_call_p(exp))
    {
        call c = expression_call(exp);
        entity op = call_function(c);
        return !ENTITY_POINT_TO_P(op) && !ENTITY_FIELD_P(op);
    }
    return true;
}

/* This function is called for each call statement and atomize it
*/
static void atomize_call_statement(statement cs)
{
    call c = instruction_call(statement_instruction(cs));

    // Initialize orginal_statement if this is the first argument
    orginal_statement=cs;

    // For each call argument, the argument is atomized if needed
	if( ENTITY_ASSIGN_P(call_function(c)) )
	{
        current_lhs = binary_call_lhs(c);
		expression rhs = binary_call_rhs(c);
		if( expression_call_p(rhs) )
		{
			FOREACH(EXPRESSION, arg,call_arguments(expression_call(rhs)))
				gen_context_recurse(arg,cs,expression_domain,reference_filter,simd_do_atomize);
		}
        if(get_bool_property("SIMD_ATOMIZER_ATOMIZE_LHS"))
        {
            expression lhs = EXPRESSION(CAR(call_arguments(c)));
            gen_context_recurse(lhs,cs,expression_domain,reference_filter,simd_do_atomize);
        }
	}
}

/* This function is called for all statements in the code
*/
static void atomize_statements(statement cs)
{
    // Only a call statement can be atomized
    if (instruction_call_p(statement_instruction(cs)))
    {
        atomize_call_statement(cs);
    }
}

boolean simd_atomizer(char * mod_name)
{
    /* get the resources */
    statement mod_stmt = (statement)
        db_get_memory_resource(DBR_CODE, mod_name, TRUE);

    set_current_module_statement(mod_stmt);
    set_current_module_entity(module_name_to_entity(mod_name));
    set_simd_treematch((matchTree)db_get_memory_resource(DBR_SIMD_TREEMATCH,"",TRUE));
    set_simd_operator_mappings(db_get_memory_resource(DBR_SIMD_OPERATOR_MAPPINGS,"",TRUE));

    debug_on("SIMD_ATOMIZER_DEBUG_LEVEL");


    /* Now do the job */
    gen_recurse(mod_stmt, statement_domain, gen_true, atomize_statements);

    /* Reorder the module, because new statements have been added */  
    module_reorder(mod_stmt);
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, mod_name, mod_stmt);

    /* update/release resources */
    reset_current_module_statement();
    reset_current_module_entity();
    reset_simd_operator_mappings();
    reset_simd_treematch();

    debug_off();

    return TRUE;
}

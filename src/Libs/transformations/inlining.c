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
/**
 * @file inlining.c
 * @brief add inlining support to pips, with two flavors
 *  - inlining(char* module) to inline all calls to a module
 *  - unfolding(char* module) to inline all call in a module
 *  - outlining(char* module) to outline statements from a module
 *
 * @author Serge Guelton <serge.guelton@enst-bretagne.fr>
 * @date 2009-01-07
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "text.h"
#include "pipsdbm.h"
#include "resources.h"
#include "properties.h"
#include "misc.h"
#include "control.h"
#include "callgraph.h"
#include "effects-generic.h"
#include "effects-convex.h"
#include "preprocessor.h"
#include "text-util.h"
#include "transformations.h"
#include "parser_private.h"
#include "syntax.h"
#include "c_syntax.h"
#include "alias-classes.h"
#include "locality.h"
#include "conversion.h"

extern string compilation_unit_of_module(string);

/**
 * @name inlining
 * @{ */

/**
 * structure containing all the parameters needed by inlining.
 * It avoids using globals
 * newgen like macros are defined
 */
typedef struct {
    entity _inlined_module_;
    statement _inlined_module_statement_;
    statement _new_statements_;
    bool _has_static_declaration_;
    bool _has_inlinable_calls_;
    statement _laststmt_;
    instruction _tail_ins_;
    entity _returned_entity_;
    bool _use_effects_;
} iparam, * inlining_parameters;
#define IPARAM_INIT { \
    ._inlined_module_=entity_undefined,\
    ._inlined_module_statement_=statement_undefined,\
    ._new_statements_=statement_undefined,\
    ._has_static_declaration_=false,\
    ._has_inlinable_calls_=false,\
    ._laststmt_=statement_undefined,\
    ._tail_ins_=instruction_undefined,\
    ._returned_entity_=entity_undefined,\
    ._use_effects_=true\
}
#define inlined_module(p)           (p)->_inlined_module_
#define inlined_module_statement(p) (p)->_inlined_module_statement_
#define new_statements(p)           (p)->_new_statements_
#define has_static_declaration(p)   (p)->_has_static_declaration_
#define has_inlinable_calls(p)      (p)->_has_inlinable_calls_
#define laststmt(p)                 (p)->_laststmt_
#define tail_ins(p)                 (p)->_tail_ins_
#define returned_entity(p)          (p)->_returned_entity_
#define use_effects(p)              (p)->_use_effects_


/* replace return instruction by a goto
 */
static
void inline_return_remover(statement s,inlining_parameters p)
{
    if( return_statement_p( s ) )
        update_statement_instruction(s,make_instruction_goto(copy_statement(laststmt(p))));
}

/* replace return instruction by an assignment and a goto
 */
static
void inline_return_crawler(statement s,inlining_parameters p)
{
    if( return_statement_p( s ) )
    {
        // create the goto
        list l= (statement_instruction(s) == tail_ins(p)) ?
            NIL :
            make_statement_list( instruction_to_statement( make_instruction_goto( copy_statement(laststmt(p)) ) ) ) ;
        // create the assign and push it if needed
        call ic = statement_call(s);
        if( !ENDP(call_arguments(ic)) )
        {
            pips_assert("return is called with one argument",ENDP(CDR(call_arguments(ic))));
            statement assign = make_assign_statement(
                    entity_to_expression( returned_entity(p) ),
                    copy_expression(EXPRESSION(CAR(call_arguments(ic)))));
            l = CONS( STATEMENT, assign , l );
        }
        update_statement_instruction(s,make_instruction_sequence(make_sequence(l)));
    }
}

/* helper function to check if a call is a call to the inlined function
 */
static
bool inline_should_inline(entity inlined_module,call callee)
{
    return same_entity_lname_p(call_function(callee),inlined_module) ;
}

/* find effects on entity `e' in statement `s'
 * cumulated effects for these statements must have been loaded
 */
bool find_write_effect_on_entity(statement s, entity e)
{
	list cummulated_effects = load_cumulated_rw_effects_list( s );
	FOREACH(EFFECT, eff,cummulated_effects)
	{
		reference r = effect_any_reference(eff);
		entity re = reference_variable(r);
		if( entities_may_conflict_p(e,re) )
		{
			if( ENDP( reference_indices(r) ) && action_write_p(effect_action(eff) ) )
                return true;
		}
	}
	return false;
}

static bool has_similar_entity(entity e,set se)
{
    SET_FOREACH(entity,ee,se)
        if( same_string_p(entity_user_name(e),entity_user_name(ee)))
            return true;
    return false;
}



/* look for entity locally named has `new' in statements `s'
 * when found, find a new name and perform substitution
 */
static void
solve_name_clashes(statement s, entity new)
{
    list l = statement_declarations(s);
    set re = get_referenced_entities(s);
    for(;!ENDP(l);POP(l))
    {
        entity decl_ent = ENTITY(CAR(l));
        if( same_string_p(entity_user_name(decl_ent), entity_user_name(new)))
        {
            entity solve_clash = copy_entity(decl_ent);
            string ename = strdup(entity_name(solve_clash));
            do {
                string tmp;
                asprintf(&tmp,"%s_",ename);
                free(ename);
                ename=tmp;
                entity_name(solve_clash)=ename;
            } while( has_similar_entity(solve_clash,re));
            CAR(l).p = (void*)solve_clash;
            replace_entity(s,decl_ent,solve_clash);
        }
    }
    set_free(re);
}

/* return true if an entity declared in `iter' is static to `module'
 */
static
bool inline_has_static_declaration(list iter)
{
    FOREACH(ENTITY, e ,iter)
    {
        if ( variable_static_p(e))
            return true;
    }
    return false;
}

/* return true if an entity declared in the statement `s' from
   `p->inlined_module'
 */
static
void statement_with_static_declarations_p(statement s,inlining_parameters p )
{
    has_static_declaration(p)|=inline_has_static_declaration(statement_declarations(s) );
}

/* create a scalar similar to `efrom' initialized with expression `from'
 */
static
entity make_temporary_scalar_entity(expression from,statement * assign)
{
    pips_assert("expression is valid",expression_consistent_p(from)&&!expression_undefined_p(from));
    /* create the scalar */
	entity new = make_new_scalar_variable(
			get_current_module_entity(),
			basic_of_expression(from)
			);
    /* set intial */
    if(!expression_undefined_p(from))
    {
        *assign=make_assign_statement(entity_to_expression(new),copy_expression(from));
    }
	return new;
}

/* regenerate the label of each statement with a label.
 * it avoid duplications due to copy_statement
 */
static
void inlining_regenerate_labels(statement s, string new_module)
{
    entity lbl = statement_label(s);
    if(!entity_empty_label_p(lbl))
    {
        if( !entity_undefined_p(find_label_entity(new_module,label_local_name(lbl))))
        {
            statement_label(s)=lbl=make_new_label(new_module);
        }
        else
            FindOrCreateEntity(new_module,entity_local_name(lbl));
        if( statement_loop_p(s) )
            loop_label(statement_loop(s))=lbl;
    }
    else if( statement_loop_p(s) ) {
        if( !entity_undefined_p( find_label_entity(new_module,label_local_name(lbl)) ) )
            loop_label(statement_loop(s))=make_new_label(new_module);
    }
}

static
bool has_entity_with_same_name(entity e, list l) {
    FOREACH(ENTITY,ent,l)
        if(same_entity_name_p(e,ent)) return true;
    return false;
}


/* this should inline the call callee
 * calling module inlined_module
 */
static
statement inline_expression_call(inlining_parameters p, expression modified_expression, call callee)
{
    /* only inline the right call */
    pips_assert("inline the right call",inline_should_inline(inlined_module(p),callee));

    string modified_module_name = entity_local_name(get_current_module_entity());

    value inlined_value = entity_initial(inlined_module(p));
    pips_assert("is a code", value_code_p(inlined_value));
    code inlined_code = value_code(inlined_value);

    /* stop if the function has static declaration */
    {
        has_static_declaration(p)=false;
        if( c_module_p(inlined_module(p)) )
            gen_context_recurse(inlined_module_statement(p),p, statement_domain, gen_true, statement_with_static_declarations_p);
        else
            has_static_declaration(p)= inline_has_static_declaration( code_declarations(inlined_code) );

        if( has_static_declaration(p))
        {
            pips_user_warning("cannot inline function with static declarations\n");
            return statement_undefined;
        }
    }

    /* create the new instruction sequence
     * no need to change all entities in the new statements, because we build a new text ressource latter
     */
    statement expanded = copy_statement(inlined_module_statement(p));
    statement declaration_holder = make_empty_block_statement();
    //statement_declarations(expanded) = gen_full_copy_list( statement_declarations(expanded) ); // simple copy != deep copy

    /* add external declartions for all extern referenced entities it
     * is needed because inlined module and current module may not
     * share the same compilation unit.
     * Not relevant for fortran
     *
     * FI: However, it would be nice to check first if the entity is not
     * already in the scope for the function or in the scope of its
     * compilation unit (OK, the later is difficult because the order
     * of declarations has to be taken into account).
     */
    if(c_module_p(get_current_module_entity()))
    {
        string cu_name = compilation_unit_of_module(get_current_module_name());
        set inlined_referenced_entities = get_referenced_entities(expanded);
        list lire = set_to_sorted_list(inlined_referenced_entities,(gen_cmp_func_t)compare_entities);
        set_free(inlined_referenced_entities);
        //list new_externs = NIL;
        FOREACH(ENTITY,ref_ent,lire)
        {
            if( entity_field_p(ref_ent) ) /* special hook for struct member : consider their structure instead of the field */
            {
                ref_ent=entity_field_to_entity_struct(ref_ent);
            }

            if(!entity_enum_member_p(ref_ent) && /* enum member cannot be added to declarations */
                    !entity_formal_p(ref_ent) ) /* formal parameters are not considered */
            {
                string emn = entity_module_name(ref_ent);
                if(extern_entity_p(get_current_module_entity(),ref_ent) &&
                        !has_entity_with_same_name(ref_ent,entity_declarations(module_name_to_entity(cu_name)) ) )
                {
                    AddEntityToModuleCompilationUnit(ref_ent,get_current_module_entity());
                    gen_append(code_externs(entity_code(module_name_to_entity(cu_name))),CONS(ENTITY,ref_ent,NIL));
                }
                else if(variable_static_p(ref_ent) &&
                        !has_entity_with_same_name(ref_ent,entity_declarations(module_name_to_entity(cu_name)) ) )
                {
                    pips_user_warning("replacing static variable \"%s\" by a global one, this may lead to incorrect code\n", entity_user_name(ref_ent));
                    entity add = make_global_entity_from_local(ref_ent);
                    replace_entity(expanded,ref_ent,add);
                    replace_entity(inlined_module_statement(p),ref_ent,add);
                    AddEntityToModuleCompilationUnit(add,get_current_module_entity());
                    gen_append(code_externs(entity_code(module_name_to_entity(cu_name))),CONS(ENTITY,add,NIL));
                }
                else if(!variable_entity_p(ref_ent) && !same_string_p(emn,cu_name) &&
                        !has_entity_with_same_name(ref_ent,entity_declarations(module_name_to_entity(cu_name)) ))
                {
                    AddEntityToModuleCompilationUnit(ref_ent,get_current_module_entity());
                }
            }
        }
        gen_free_list(lire);
    }
    /* we have anothe rproblem with fortran, where declaration are  not handled like in C
     * another side effect of 'declarations as statements' */
    else
    {
        bool did_something = false;
        FOREACH(ENTITY,e,entity_declarations(inlined_module(p)))
        {
            if(!entity_area_p(e))
            {
                entity new;
                if(entity_variable_p(e)) {
                    if(entity_scalar_p(e)) {
                        new = make_new_scalar_variable_with_prefix(entity_user_name(e),get_current_module_entity(),copy_basic(entity_basic(e)));
                    }
                    else {
                        new = make_new_array_variable_with_prefix(entity_user_name(e),get_current_module_entity(),
                                copy_basic(entity_basic(e)), gen_full_copy_list(variable_dimensions(type_variable(entity_type(e)))));
                    }
                }
                else
                {
                    /*sg: unsafe*/
                    bool regenerate = entity_undefined_p(FindEntity(get_current_module_name(),entity_local_name(e)));
                    new=FindOrCreateEntity(get_current_module_name(),entity_local_name(e));
                    if(regenerate)
                    {
                        entity_storage(new)=copy_storage(entity_storage(e));
                        entity_initial(new)=copy_value(entity_initial(e));
                        entity_type(new)=copy_type(entity_type(e));
                    }
                }
                gen_context_recurse(expanded, new, statement_domain, gen_true, &solve_name_clashes);
                AddEntityToDeclarations(new,get_current_module_entity());
                replace_entity(expanded,e,new);
                did_something=true;
            }
        }
        if(did_something)
        {
            string decls = code_decls_text(entity_code(get_current_module_entity()));
            if(decls && !empty_string_p(decls)){
                free(decls);
                code_decls_text(entity_code(get_current_module_entity()))=strdup("");
            }
        }
    }


    /* ensure block status */
    if( ! statement_block_p( expanded ) )
    {
        instruction i = make_instruction_sequence( make_sequence( CONS(STATEMENT,expanded,NIL) ) );
        expanded = instruction_to_statement( i );
    }


    /* avoid duplicated label due to copy_statement */
    gen_context_recurse(expanded,modified_module_name,statement_domain,gen_true,inlining_regenerate_labels);

    /* add label at the end of the statement */
    laststmt(p)=make_continue_statement(make_new_label( modified_module_name ) );
    insert_statement(expanded,laststmt(p),false);

    /* fix `return' calls
     * in case a goto is immediatly followed by its targeted label
     * the goto is not needed (SG: seems difficult to do in the previous gen_recurse)
     */
    {
        list tail = sequence_statements(instruction_sequence(statement_instruction(expanded)));
        pips_assert("inlinined statement is not empty",!ENDP(tail) && !ENDP(CDR(tail)));
        {
            tail_ins(p)= statement_instruction(STATEMENT(CAR(gen_last(tail))));

            type treturn = ultimate_type(functional_result(type_functional(entity_type(inlined_module(p)))));
            if( type_void_p(treturn) ) /* only replace return statement by gotos */
            {
                gen_context_recurse(expanded, p,statement_domain, gen_true, &inline_return_remover);
            }
            else /* replace by affectation + goto */
            {
                /* create new variable to receive computation result */
                pips_assert("returned value is a variable", type_variable_p(treturn));
                do {
                    returned_entity(p)= make_new_scalar_variable_with_prefix(
                            "_return",
                            get_current_module_entity(),
                            copy_basic(variable_basic(type_variable(treturn)))
                            );
                    /* make_new_scalar_variable does not ensure the entity is not defined in enclosing statement, we check this */
                    FOREACH(ENTITY,ent,statement_declarations(expanded))
                    {
                        if(same_string_p(entity_user_name(ent),entity_user_name(returned_entity(p))))
                        {
                            returned_entity(p)=entity_undefined;
                            break;
                        }
                    }
                } while(entity_undefined_p(returned_entity(p)));

                AddEntityToCurrentModule(returned_entity(p));

                /* do the replacement */
                gen_context_recurse(expanded, p, statement_domain, gen_true, &inline_return_crawler);

                /* change the caller from an expression call to a call to a constant */
                if( entity_constant_p(returned_entity(p)) )
                {
                    expression_syntax(modified_expression) = make_syntax_call(make_call(returned_entity(p),NIL));
                }
                /* ... or to a reference */
                else
                {
                    reference r = make_reference( returned_entity(p), NIL);
                    expression_syntax(modified_expression) = make_syntax_reference(r);
                }
            }
        }
    }

    /* fix declarations */
    {
        /* retreive formal parameters*/
        list formal_parameters = NIL;
        FOREACH(ENTITY,cd,code_declarations(inlined_code))
            if( entity_formal_p(cd)) formal_parameters=CONS(ENTITY,cd,formal_parameters);
        formal_parameters = gen_nreverse(formal_parameters);

        { /* some basic checks */
            size_t n1 = gen_length(formal_parameters), n2 = gen_length(call_arguments(callee));
            pips_assert("function call has enough arguments",n1 >= n2);
        }
        /* iterate over the parameters and perform substitution between formal and actual parameters */
        for(list iter = formal_parameters,c_iter = call_arguments(callee) ; !ENDP(c_iter); POP(iter),POP(c_iter) )
        {
            entity e = ENTITY(CAR(iter));
            expression from = EXPRESSION(CAR(c_iter));

            /* check if there is a write effect on this parameter */
            bool need_copy = (!use_effects(p)) || find_write_effect_on_entity(inlined_module_statement(p),e);

            /* generate a copy for this parameter */
            entity new = entity_undefined;
            if(need_copy)
            {
                if(entity_scalar_p(e)) {
                    new = make_new_scalar_variable_with_prefix(entity_user_name(e),get_current_module_entity(),copy_basic(entity_basic(e)));
                }
                else {
                    new = make_new_array_variable_with_prefix(entity_user_name(e),get_current_module_entity(),
                            copy_basic(entity_basic(e)), gen_full_copy_list(variable_dimensions(type_variable(entity_type(e)))));
                }

                /* fix value */
                entity_initial(new) = make_value_expression( copy_expression( from ) );


                /* add the entity to our list */
                //statement_declarations(declaration_holder)=CONS(ENTITY,new,statement_declarations(declaration_holder));
                gen_context_recurse(expanded, new, statement_domain, gen_true, &solve_name_clashes);
                AddLocalEntityToDeclarations(new,get_current_module_entity(),declaration_holder);
                replace_entity(expanded,e,new);
                pips_debug(2,"replace %s by %s",entity_user_name(e),entity_user_name(new));
            }
            /* substitute variables */
            else
            {
                /* get new reference */

                bool add_dereferencment = false;
reget:
                switch(syntax_tag(expression_syntax(from)))
                {
                    case is_syntax_reference:
                        {
                            reference r = syntax_reference(expression_syntax(from));
                            size_t nb_indices = gen_length(reference_indices(r));
                            if( nb_indices == 0 )
                            {
                                new = reference_variable(r);
                            }
                            else /* need a temporary variable */
                            {
                                if( ENDP(variable_dimensions(type_variable(entity_type(e)))) )
                                {
                                    statement st=statement_undefined;
                                    new = make_temporary_scalar_entity(from,&st);
                                    if(!statement_undefined_p(st))
                                        insert_statement(declaration_holder,st,false);
                                }
                                else
                                {
                                    new = make_temporary_pointer_to_array_entity(e,MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME),from));
                                    add_dereferencment=true;
                                }
                                AddLocalEntityToDeclarations(new,get_current_module_entity(),declaration_holder);

                            }
                        } break;
                        /* this one is more complicated than I thought,
                         * what of the side effect of the call ?
                         * we must create a new variable holding the call result before
                         */
                    case is_syntax_call:
                        if( expression_constant_p(from) )
                            new = call_function(expression_call(from));
                        else
                        {
                                if( ENDP(variable_dimensions(type_variable(entity_type(e)))) )
                                {
                                    statement st=statement_undefined;
                                    new = make_temporary_scalar_entity(from,&st);
                                    if(!statement_undefined_p(st))
                                        insert_statement(declaration_holder,st,false);
                                }
                                else
                                {
                                    statement st=statement_undefined;
                                    new = make_temporary_scalar_entity(from,&st);
                                    if(!statement_undefined_p(st))
                                        insert_statement(declaration_holder,st,false);
                                }
                                AddLocalEntityToDeclarations(new,get_current_module_entity(),declaration_holder);
                        } break;
                    case is_syntax_subscript:
                        /* need a temporary variable */
                        {
                            if( ENDP(variable_dimensions(type_variable(entity_type(e)))) )
                            {
                                    statement st=statement_undefined;
                                    new = make_temporary_scalar_entity(from,&st);
                                    if(!statement_undefined_p(st))
                                        insert_statement(declaration_holder,st,false);
                            }
                            else
                            {
                                new = make_temporary_pointer_to_array_entity(e,MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME),from));
                                add_dereferencment=true;
                            }
                            AddLocalEntityToDeclarations(new,get_current_module_entity(),declaration_holder);

                        } break;

                    case is_syntax_cast:
                        pips_user_warning("ignoring cast\n");
                        from = cast_expression(syntax_cast(expression_syntax(from)));
                        goto reget;
                    default:
                        pips_internal_error("unhandled tag %d\n", syntax_tag(expression_syntax(from)) );
                };

                /* check wether the substitution will cause naming clashes
                 * then perform the substitution
                 */
                    gen_context_recurse(expanded , new, statement_domain, gen_true, &solve_name_clashes);
                    if(add_dereferencment) replace_entity_by_expression(expanded ,e,MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),entity_to_expression(new)));
                    else replace_entity(expanded ,e,new);
                    pips_debug(3,"replace %s by %s\n",entity_user_name(e),entity_user_name(new));

            }

        }
        gen_free_list(formal_parameters);
    }

    /* add declaration at the beginning of the statement */
    insert_statement(declaration_holder,expanded,false);

    /* final cleanings
     */
    gen_recurse(expanded,statement_domain,gen_true,fix_statement_attributes_if_sequence);
    unnormalize_expression(expanded);
    ifdebug(1) statement_consistent_p(declaration_holder);
    ifdebug(2) {
        pips_debug(2,"inlined statement after substitution\n");
        print_statement(declaration_holder);
    }
    return declaration_holder;
}


/* recursievly inline an expression if needed
 */
static
void inline_expression(expression expr, inlining_parameters  p)
{
    if( expression_call_p(expr) )
    {
        call callee = syntax_call(expression_syntax(expr));
        if( inline_should_inline( inlined_module(p), callee ) )
        {
                statement s = inline_expression_call(p, expr, callee );
                if( !statement_undefined_p(s) )
                {
                    insert_statement(new_statements(p),s,true);
                }
                ifdebug(1) statement_consistent_p(s);
                ifdebug(2) {
                    pips_debug(2,"inserted inline statement\n");
                    print_statement(new_statements(p));
                }
        }
    }
}

/* check if a call has inlinable calls
 */
static
void inline_has_inlinable_calls_crawler(call callee,inlining_parameters p)
{
    if( has_inlinable_calls(p)|=inline_should_inline(inlined_module(p),callee) ) gen_recurse_stop(0);
}
static
bool inline_has_inlinable_calls(entity inlined_module,void* elem)
{
    iparam p = { ._inlined_module_=inlined_module,._has_inlinable_calls_=false};
    gen_context_recurse(elem, &p, call_domain, gen_true,&inline_has_inlinable_calls_crawler);
    return has_inlinable_calls(&p);
}


/* this is in charge of replacing instruction by new ones
 * only apply if this instruction does not contain other instructions
 */
static
void inline_statement_crawler(statement stmt, inlining_parameters p)
{
    instruction sti = statement_instruction(stmt);
    if( instruction_call_p(sti) && inline_has_inlinable_calls(inlined_module(p),sti) )
    {
        /* the gen_recurse can only handle expressions, so we turn this call into an expression */
        update_statement_instruction(stmt,
                sti=make_instruction_expression(call_to_expression(copy_call(instruction_call(sti)))));
    }

    new_statements(p)=make_block_statement(NIL);
    gen_context_recurse(sti,p,expression_domain,gen_true, inline_expression);

    if( !ENDP(statement_block(new_statements(p)))  ) /* something happens on the way to heaven */
    {
        type t= functional_result(type_functional(entity_type(inlined_module(p))));
        if( ! type_void_p(t) )
        {
            //pips_assert("inlining instruction modification is ok", instruction_consistent_p(sti));
            insert_statement(new_statements(p),instruction_to_statement(copy_instruction(sti)),false);
        }
        if(statement_block_p(stmt))
        {
            list iter=statement_block(stmt);
            for(stmt=STATEMENT(CAR(iter));continue_statement_p(stmt);POP(iter))
                stmt=STATEMENT(CAR(iter));
        }
        update_statement_instruction(stmt,statement_instruction(new_statements(p)));
        ifdebug(2) {
            pips_debug(2,"updated statement instruction\n");
            print_statement(stmt);
        }
        //pips_assert("inlining statement generation is ok",statement_consistent_p(stmt));
    }
    ifdebug(1) statement_consistent_p(stmt);
}

/* split the declarations from s from their initialization if they contain a call to inlined_module
 */
static
void inline_split_declarations(statement s, entity inlined_module)
{
    if(statement_block_p(s))
    {
        list prelude = NIL;
        set selected_entities = set_make(set_pointer);
        FOREACH(ENTITY,e,statement_declarations(s))
        {
            value v = entity_initial(e);
            if(!value_undefined_p(v) && value_expression_p(v))
            {
                /* the first condition is a bit tricky :
                 * check int a = foo(); int b=bar();
		 * once we decide to inline foo(), we must split b=bar()
                 * because foo may touch a global variable used in bar()
                 */
                if( !ENDP(prelude) ||
                    inline_has_inlinable_calls(inlined_module,value_expression(v)) )
                {
                    set_add_element(selected_entities,selected_entities,e);
                    prelude=CONS(STATEMENT,make_assign_statement(entity_to_expression(e),copy_expression(value_expression(v))),prelude);
                    free_value(entity_initial(e));
                    entity_initial(e)=make_value_unknown();
                }
            }
        }
        set_free(selected_entities);
        FOREACH(STATEMENT,st,prelude)
            insert_statement(s,st,true);
        gen_free_list(prelude);
    }
    else if(!declaration_statement_p(s) && !ENDP(statement_declarations(s)))
        pips_user_warning("only blocks and declaration statements should have declarations\n");
}

/* this should replace all call to `inlined' in `module'
 * by the expansion of `inlined'
 */
static void
inline_calls(inlining_parameters p ,char * module)
{
    entity modified_module = module_name_to_entity(module);
    /* get target module's ressources */
    statement modified_module_statement =
        (statement) db_get_memory_resource(DBR_CODE, module, TRUE);
    pips_assert("statements found", !statement_undefined_p(modified_module_statement) );
    pips_debug(2,"inlining %s in %s\n",entity_user_name(inlined_module(p)),module);

    set_current_module_entity( modified_module );
    set_current_module_statement( modified_module_statement );

    /* first pass : convert some declaration with assignment to declarations + statements, if needed */
    gen_context_recurse(modified_module_statement, inlined_module(p), statement_domain, gen_true, inline_split_declarations);
    /* inline all calls to inlined_module */
    gen_context_recurse(modified_module_statement, p, statement_domain, gen_true, inline_statement_crawler);
    ifdebug(1) statement_consistent_p(modified_module_statement);
    ifdebug(2) {
        pips_debug(2,"in inline_calls for %s\n",module);
        print_statement(modified_module_statement);
    }

    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module, modified_module_statement);
    DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, module, compute_callees(modified_module_statement));
    reset_current_module_entity();
    reset_current_module_statement();
}

/**
 * this should inline all calls to module `module_name'
 * in calling modules, if possible ...
 *
 * @param module_name name of the module to inline
 *
 * @return true if we did something
 */
static
bool do_inlining(inlining_parameters p,char *module_name)
{
    /* Get the module ressource */
    inlined_module (p)= module_name_to_entity( module_name );
    inlined_module_statement (p)= (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE);

    if(use_effects(p)) set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS,module_name,TRUE));

    /* check them */
    pips_assert("is a functionnal",entity_function_p(inlined_module(p)) || entity_subroutine_p(inlined_module(p)) );
    pips_assert("statements found", !statement_undefined_p(inlined_module_statement(p)) );
    debug_on("INLINING_DEBUG_LEVEL");


    /* parse filter property */
    string inlining_callers_name = strdup(get_string_property("INLINING_CALLERS"));
    list callers_l = NIL;

    string c_name= NULL;
    for(c_name = strtok(inlining_callers_name," ") ; c_name ; c_name=strtok(NULL," ") )
    {
        callers_l = CONS(STRING, c_name, callers_l);
    }
    /*  or get module's callers */
    if(ENDP(callers_l))
    {
        callees callers = (callees)db_get_memory_resource(DBR_CALLERS, module_name, TRUE);
        callers_l = callees_callees(callers);
    }

    /* inline call in each caller */
    FOREACH(STRING, caller_name,callers_l)
    {
        inline_calls(p, caller_name );
        recompile_module(caller_name);
        /* we can try to remove some labels now*/
        if( get_bool_property("INLINING_PURGE_LABELS"))
            if(!remove_useless_label(caller_name))
                pips_user_warning("failed to remove useless labels after restructure_control in inlining");
    }

    if(use_effects(p)) reset_cumulated_rw_effects();

    pips_debug(2, "inlining done for %s\n", module_name);
    debug_off();
    /* Should have worked: */
    return TRUE;
}

/**
 * perform inlining using effects
 *
 * @param module_name name of the module to inline
 *
 * @return
 */
bool inlining(char *module_name)
{
    iparam p =IPARAM_INIT;
    use_effects(&p)=true;
	return do_inlining(&p,module_name);
}

/**
 * perform inlining without using effects
 *
 * @param module_name name of the module to inline
 *
 * @return
 */
bool inlining_simple(char *module_name)
{
    iparam p =IPARAM_INIT;
    use_effects(&p)=false;
	return do_inlining(&p,module_name);
}

/**  @} */

/**
 * @name unfolding
 * @{ */

/**
 * get ressources for the call to inline and call
 * apropriate inlining function
 *
 * @param caller_name calling module name
 * @param module_name called module name
 */
static bool
run_inlining(string caller_name, string module_name, inlining_parameters p)
{
    /* Get the module ressource */
    inlined_module (p)= module_name_to_entity( module_name );
    inlined_module_statement (p)= (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE);
    if(statement_block_p(inlined_module_statement (p)) && ENDP(statement_block(inlined_module_statement (p))))
    {
        pips_user_warning("not inlining empty function, this should be a generated skeleton ...\n");
        return false;
    }
    else {

        if(use_effects(p)) set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS,module_name,TRUE));

        /* check them */
        pips_assert("is a functionnal",entity_function_p(inlined_module(p)) || entity_subroutine_p(inlined_module(p)) );
        pips_assert("statements found", !statement_undefined_p(inlined_module_statement(p)) );

        /* inline call */
        inline_calls( p, caller_name );
        if(use_effects(p)) reset_cumulated_rw_effects();
        return true;
    }
}


/**
 * this should inline all call in module `module_name'
 * it does not works recursievly, so multiple pass may be needed
 * returns true if at least one function has been inlined
 *
 * @param module_name name of the module to unfold
 *
 * @return true if we did something
 */
static
bool do_unfolding(inlining_parameters p, char* module_name)
{
    debug_on("UNFOLDING_DEBUG_LEVEL");

    /* parse filter property */
    string unfolding_filter_names = strdup(get_string_property("UNFOLDING_FILTER"));
    set unfolding_filters = set_make(set_string);

    string filter_name= NULL;
    for(filter_name = strtok(unfolding_filter_names," ") ; filter_name ; filter_name=strtok(NULL," ") )
    {
        set_add_element(unfolding_filters, unfolding_filters, filter_name);
        recompile_module(module_name);
        /* we can try to remove some labels now*/
        if( get_bool_property("INLINING_PURGE_LABELS"))
            if(!remove_useless_label(module_name))
                pips_user_warning("failed to remove useless labels after restructure_control in inlining");
    }

    /* parse callee property */
    string unfolding_callees_names = strdup(get_string_property("UNFOLDING_CALLEES"));
    set unfolding_callees = set_make(set_string);

    string callee_name= NULL;
    for(callee_name = strtok(unfolding_callees_names," ") ; callee_name ; callee_name=strtok(NULL," ") )
    {
        set_add_element(unfolding_callees, unfolding_callees, callee_name);
    }

    /* gather all referenced calls as long as there are some */
    bool statement_has_callee = false;
    do {
        statement_has_callee = false;
        statement unfolded_module_statement =
            (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE);
        /* gather all referenced calls */
        callees cc =compute_callees(unfolded_module_statement);
        set calls_name = set_make(set_string);
        set_assign_list(calls_name,callees_callees(cc));


        /* maybe the user put a restriction on the calls to inline ?*/
        if(!set_empty_p(unfolding_callees))
            calls_name=set_intersection(calls_name,calls_name,unfolding_callees);

        /* maybe the user used a filter ?*/
        calls_name=set_difference(calls_name,calls_name,unfolding_filters);



        /* there is something to inline */
        if( (statement_has_callee=!set_empty_p(calls_name)) )
        {
            SET_FOREACH(string,call_name,calls_name) {
                if(!run_inlining(module_name,call_name,p))
                    set_add_element(unfolding_filters,unfolding_filters,call_name);
            }
            recompile_module(module_name);
            /* we can try to remove some labels now*/
            if( get_bool_property("INLINING_PURGE_LABELS"))
                if(!remove_useless_label(module_name))
                    pips_user_warning("failed to remove useless labels after restructure_control in inlining");
        }
        set_free(calls_name);
        free_callees(cc);
    } while(statement_has_callee);

    set_free(unfolding_filters);
    free(unfolding_filter_names);


    pips_debug(2, "unfolding done for %s\n", module_name);

    debug_off();
    return true;
}


/**
 * perform unfolding using effect
 *
 * @param module_name name of the module to unfold
 *
 * @return
 */
bool unfolding(char* module_name)
{
    iparam p = IPARAM_INIT;
    use_effects(&p)=true;
	return do_unfolding(&p,module_name);
}


/**
 * perform unfolding without using effects
 *
 * @param module_name name of the module to unfold
 *
 * @return true upon success
 */
bool unfolding_simple(char* module_name)
{
    iparam p = IPARAM_INIT;
    use_effects(&p)=false;
	return do_unfolding(&p,module_name);
}
/**  @} */


/**
 * @name outlining
 * @{ */

#define STAT_ORDER "PRETTYPRINT_STATEMENT_NUMBER"

static
void patch_outlined_reference(expression x, entity e)
{
    if(expression_reference_p(x))
    {
        reference r =expression_reference(x);
        entity e1 = reference_variable(r);
        if(same_entity_p(e,e1))
        {
			variable v = type_variable(entity_type(e));
			if( (!basic_pointer_p(variable_basic(v))) && (ENDP(variable_dimensions(v))) ) /* not an array / pointer */
			{
				expression X = make_expression(expression_syntax(x),normalized_undefined);
				expression_syntax(x)=make_syntax_call(make_call(
							CreateIntrinsic(DEREFERENCING_OPERATOR_NAME),
							CONS(EXPRESSION,X,NIL)));
			}
        }
    }

}

static
void patch_outlined_reference_in_declarations(statement s, entity e)
{
    FOREACH(ENTITY, ent, statement_declarations(s))
    {
        value v = entity_initial(ent);
        if(!value_undefined_p(v) && value_expression_p(v))
            gen_context_recurse(value_expression(v),e,expression_domain,gen_true,patch_outlined_reference);
    }
}

static
void bug_in_patch_outlined_reference(loop l , entity e)
{
    if( same_entity_p(loop_index(l), e))
    {
        statement parent = (statement)gen_get_ancestor(statement_domain,l);
        pips_assert("child's parent child is me",statement_loop_p(parent) && statement_loop(parent)==l);
        statement body =loop_body(l);
        range r = loop_range(l);
        instruction new_instruction = make_instruction_forloop(
                    make_forloop(
                        make_assign_expression(
                            MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),entity_to_expression(e)),
                            range_lower(r)),
                        MakeBinaryCall(entity_intrinsic(LESS_OR_EQUAL_OPERATOR_NAME),
                            MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),entity_to_expression(e)),
                            range_upper(r)),
                        MakeBinaryCall(entity_intrinsic(PLUS_UPDATE_OPERATOR_NAME),
                            MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),entity_to_expression(e)),
                            range_increment(r)),
                        body
                        )
                    );

        statement_instruction(parent)=instruction_undefined;/*SG this is a small leak */
        update_statement_instruction(parent,new_instruction);
        pips_assert("statement consistent",statement_consistent_p(parent));
    }
}

static
void get_private_entities_walker(loop l, set s)
{
    set_append_list(s,loop_locals(l));
}

static
set get_private_entities(void *s)
{
    set tmp = set_make(set_pointer);
    gen_context_recurse(s,tmp,loop_domain,gen_true,get_private_entities_walker);
    return tmp;
}

static
void sort_entities_with_dep(list *l)
{
    set params = set_make(set_pointer);
    FOREACH(ENTITY,e,*l)
    {
        set e_ref = get_referenced_entities(e);
        set_del_element(e_ref,e_ref,e);
        set_union(params,params,e_ref);
        set_free(e_ref);
    }

    set base = set_make(set_pointer);
    set_assign_list(base,*l);
    set_difference(base,base,params);

    list l_params = set_to_sorted_list(params,(gen_cmp_func_t)compare_entities);
    list l_base = set_to_sorted_list(base,(gen_cmp_func_t)compare_entities);

    set_free(base);set_free(params);
    gen_free_list(*l);

    *l=gen_nconc(l_params,l_base);
}

struct cpv {
    entity e;
    bool rm;
};


static
void check_private_variables_call_walker(call c,struct cpv * p)
{
    set s = get_referenced_entities(c);
    if(set_belong_p(s,p->e)){
        p->rm=true;
        gen_recurse_stop(0);
    }
    set_free(s);
}
static
bool check_private_variables_loop_walker(loop l, struct cpv * p)
{
    return !has_entity_with_same_name(p->e,loop_locals(l));
}

static
list private_variables(statement stat)
{
    set s = get_private_entities(stat);
    list l =NIL;
    SET_FOREACH(entity,e,s) {
        struct cpv p = { .e=e, .rm=false };
        gen_context_multi_recurse(stat,&p,
                call_domain,gen_true,check_private_variables_call_walker,
                loop_domain,check_private_variables_loop_walker,gen_null,
                0);
        if(!p.rm)
            l=CONS(ENTITY,e,l);
    }
    set_free(s);

    return l;
}

typedef struct {
    entity old;
    entity new;
    size_t nb_dims;
} ocontext_t;
static void do_outliner_smart_replacment(reference r, ocontext_t * ctxt)
{
    if(same_entity_p(ctxt->old,reference_variable(r)))
    {
        size_t nb_dims = ctxt->nb_dims;
        list indices = reference_indices(r);
        while (nb_dims--) POP(indices); 
        indices=gen_full_copy_list(indices);
        if(basic_pointer_p(entity_basic(ctxt->new))) /*sg:may cause issues if basic_pointer_p(old) ? */
        {
            expression parent = (expression)gen_get_ancestor(expression_domain,r);
            pips_assert("parent exist",parent);
            free_syntax(expression_syntax(parent));
            if(!ENDP(indices))
                expression_syntax(parent)=
                    make_syntax_subscript(
                            make_subscript(
                                MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),entity_to_expression(ctxt->new)),
                                indices)
                            );
            else
                expression_syntax(parent)=
                    make_syntax_call(
                            make_call(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),make_expression_list(entity_to_expression(ctxt->new)))
                            );

        }
        else {
            reference_variable(r)=ctxt->new;
            gen_full_free_list(reference_indices(r));
            reference_indices(r)=indices;
        }
    }
}

static void outliner_smart_replacment(statement in, entity old, entity new,size_t nb_dims)
{
    ocontext_t ctxt = { old,new,nb_dims };
    gen_context_recurse(in,&ctxt,reference_domain,gen_true,do_outliner_smart_replacment);
}

/**
 * purge the list of referenced entities by replacing calls to a[i][j] where i is a constant in statements
 * outlined_statements by a call to a single (new) variable
 */
static hash_table outliner_smart_references_computation(list referenced_entities, list outlined_statements,entity new_module, statement new_body)
{
    /* this will hold new referenced_entities list */
    hash_table entity_to_init = hash_table_make(hash_pointer,HASH_DEFAULT_SIZE);
    /* first check candidates, that is array entities accessed by a constant index */
    FOREACH(ENTITY,e,referenced_entities)
    {
        FOREACH(STATEMENT,st,outlined_statements)
        {
            list regions = load_rw_effects_list(st);
            list the_constant_indices = NIL;
            action mode = action_undefined;
            FOREACH(REGION,reg,regions)
            {
                reference rr = region_any_reference(reg);
                if(same_entity_p(e,reference_variable(rr)))
                {
                    list constant_indices = NIL;
                    Psysteme sc = region_system(reg);
                    FOREACH(EXPRESSION,index,reference_indices(rr))
                    {
                        Variable phi = expression_to_entity(index);
                        expression index_value = expression_undefined;
                        /* we are looking for constant index, so only check equalities */
                        for(Pcontrainte iter = sc_egalites(sc);iter;iter=contrainte_succ(iter))
                        {
                            Pvecteur cvect = contrainte_vecteur(iter);
                            Value phi_coeff = vect_coeff(phi,cvect);
                            if(phi_coeff != VALUE_ZERO )
                            {
                                pips_assert("phi coeff sould be one",phi_coeff == VALUE_ONE);
                                Pvecteur lhs_vect = vect_del_var(cvect,phi);
                                vect_chg_sgn(lhs_vect);
                                pips_assert("phi coef should be mentionned only once",expression_undefined_p(index_value));
                                index_value = VECTEUR_NUL_P(lhs_vect) ? int_to_expression(0) : Pvecteur_to_expression(lhs_vect);
                                vect_rm(lhs_vect);
                            }
                        }
                        if(!expression_undefined_p(index_value))
                        {
                            /* it's ok, we can keep on finding constant indices */
                            constant_indices=CONS(EXPRESSION,index_value,constant_indices);
                        }
                        else break;
                    }
                    constant_indices=gen_nreverse(constant_indices);
                    /* check for clashes */
                    if(!ENDP(the_constant_indices) && ! gen_equals(constant_indices,the_constant_indices,(gen_eq_func_t)same_expression_p))
                    {
                        /* abort there , we could be smarter */
                        gen_full_free_list(the_constant_indices);
                        gen_full_free_list(constant_indices);
                        the_constant_indices=constant_indices=NIL;
                        break;
                    }
                    else if( ENDP(the_constant_indices) )
                    {
                        the_constant_indices=constant_indices;
                        mode=region_action(reg);
                    }
                    else if( action_read_p(mode) && action_write_p(region_action(reg)))
                    {
                        mode =region_action(reg);
                    }
                }
            }
            /* we have gathered a sub array of e that is constant and we know its mode
             * get ready for substitution in the statement */
            if(!ENDP(the_constant_indices))
            {
                size_t nb_constant_indices = gen_length(the_constant_indices);
                list entity_dimensions = variable_dimensions(type_variable(entity_type(e)));
                size_t nb_dimensions = gen_length(entity_dimensions);

                /* compute new dimensions */
                list new_dimensions = NIL;
                size_t count_dims = 0;
                for(list iter = entity_dimensions;!ENDP(iter);POP(iter))
                {
                    ++count_dims;
                    if(count_dims==nb_constant_indices) { new_dimensions=gen_full_copy_list(CDR(iter));break; }
                }


                basic new_basic;
                if(action_read_p(mode)&&nb_constant_indices==nb_dimensions)
                    new_basic=copy_basic(entity_basic(e));
                else
                {
                    type new_type = make_type_variable(
                            make_variable(
                                copy_basic(entity_basic(e)),
                                new_dimensions,
                                gen_full_copy_list(variable_qualifiers(type_variable(entity_type(e))))
                                )
                            );
                    new_basic=make_basic_pointer(new_type);
                }

                entity new_entity;
                if(action_read_p(mode)&&nb_constant_indices==nb_dimensions)
                {
                    new_entity = make_new_array_variable_with_prefix(
                            entity_user_name(e),
                            new_module,
                            new_basic,
                            new_dimensions);
                }
                else
                {
                    new_entity = make_new_scalar_variable_with_prefix(
                            entity_user_name(e),
                            new_module,
                            new_basic);
                }
                outliner_smart_replacment(st,e,new_entity,nb_constant_indices);
                expression effective_parameter = reference_to_expression(make_reference(e,the_constant_indices));
                if(!(action_read_p(mode)&&nb_constant_indices==nb_dimensions))
                    effective_parameter=MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME),effective_parameter);
                hash_put(entity_to_init,new_entity,effective_parameter);
            }
        }
    }
    return entity_to_init;
}
static void statements_localize_declarations(list statements,entity module,statement module_statement)
{
    list sd = statements_to_declarations(statements);
    FOREACH(STATEMENT, s, statements)
    {
        /* We want to declare private variables as locals, but it may not
           be valid */
        list private_ents = private_variables(s);
        gen_sort_list(private_ents,(gen_cmp_func_t)compare_entities);
        FOREACH(ENTITY,e,private_ents)
        {
            if(gen_chunk_undefined_p(gen_find_eq(e,sd))) {
                AddLocalEntityToDeclarations(e,module,module_statement);
            }
        }
        gen_free_list(private_ents);
    }
    gen_free_list(sd);
}
static list statements_referenced_entities(list statements)
{
    list referenced_entities = NIL;
    set sreferenced_entities = set_make(set_pointer);

    FOREACH(STATEMENT, s, statements)
    {
        set tmp = get_referenced_entities(s);
        set_union(sreferenced_entities,tmp,sreferenced_entities);
        set_free(tmp);
    }
    /* set to list */
    referenced_entities=set_to_list(sreferenced_entities);
    set_free(sreferenced_entities);
    return referenced_entities;
}

/**
 * outline the statements in statements_to_outline into a module named outline_module_name
 * the outlined statements are replaced by a call to the newly generated module
 * statements_to_outline is modified in place to represent that call
 *
 * @param outline_module_name name of the new module

 * @param statements_to_outline is a list of consecutive statements to
 * outline into outline_module_name
 *
 * @return pointer to the newly generated statement (already inserted in statements_to_outline)
 */
statement outliner(string outline_module_name, list statements_to_outline)
{
    pips_assert("there are some statements to outline",!ENDP(statements_to_outline));
    entity new_fun = make_empty_subroutine(outline_module_name,copy_language(module_language(get_current_module_entity())));
    statement new_body = instruction_to_statement(make_instruction_sequence(make_sequence(statements_to_outline)));


    /* Retrieve referenced entities */
    list referenced_entities = statements_referenced_entities(statements_to_outline);
    /* try to be smart concerning array references */
    hash_table entity_to_effective_parameter = hash_table_undefined;
    if(get_bool_property("OUTLINE_SMART_REFERENCE_COMPUTATION"))
    {
        entity_to_effective_parameter = outliner_smart_references_computation(referenced_entities,statements_to_outline,new_fun,new_body);
        /*and recompute referenced entities*/
        gen_free_list(referenced_entities);
        referenced_entities = statements_referenced_entities(statements_to_outline);
    }
    else
        entity_to_effective_parameter = hash_table_make(hash_pointer,1);

    /* Retrieve declared entities */
    statements_localize_declarations(statements_to_outline,new_fun,new_body);
    list declared_entities = statements_to_declarations(statements_to_outline);
    declared_entities=gen_nconc(declared_entities,statement_to_declarations(new_body));
    
    /* get the relative complements and create the parameter list*/
    gen_list_and_not(&referenced_entities,declared_entities);
    gen_free_list(declared_entities);


    /* purge the functions from the parameter list, we assume they are declared externally
     * also purge the formal parameters from other modules, gathered by get_referenced_entities but wrong here
     * also purge memebers, not relevant
     */
    list tmp_list=NIL;
    FOREACH(ENTITY,e,referenced_entities)
    {
        /* function should be add to compilation unit */
        if(entity_function_p(e))
            ;//AddEntityToModuleCompilationUnit(e,get_current_module_entity());
        else if( (!entity_constant_p(e) ) && (!entity_field_p(e)) &&
                !( entity_formal_p(e) && (!same_string_p(entity_module_name(e),get_current_module_name()))) )
            tmp_list=CONS(ENTITY,e,tmp_list);
    }
    gen_free_list(referenced_entities);
    referenced_entities=tmp_list;


    /* remove global variables if needed */
    if(get_bool_property("OUTLINE_ALLOW_GLOBALS"))
    {
        string cu_name = compilation_unit_of_module(get_current_module_name());
        entity cu = module_name_to_entity(cu_name);
        list cu_decls = entity_declarations(cu);

        tmp_list=NIL;

        FOREACH(ENTITY,e,referenced_entities)
        {
            if( !top_level_entity_p(e) && gen_chunk_undefined_p(gen_find_eq(e,cu_decls) ) )
                tmp_list=CONS(ENTITY,e,tmp_list);
            else if (gen_chunk_undefined_p(gen_find_eq(e,cu_decls)))
            {
                AddLocalEntityToDeclarations(e,new_fun,new_body);
            }
        }
        gen_free_list(referenced_entities);
        referenced_entities=tmp_list;
    }

    /* sort list, and put parameters first */
    sort_entities_with_dep(&referenced_entities);



    intptr_t i=0;

	/* all variables are promoted parameters */
    list effective_parameters = NIL;
    list formal_parameters = NIL;
    FOREACH(ENTITY,e,referenced_entities)
    {
        type t = entity_type(e);
        bool is_parameter_p = false;
        if( type_variable_p(t) ||
                /* for parameter case */ (is_parameter_p=(entity_symbolic_p(e) && storage_rom_p(entity_storage(e)) && type_functional_p(t))) )
        {
            /* this create the dummy parameter */
            entity dummy_entity = FindOrCreateEntity(
                    outline_module_name,
                    entity_user_name(e)
                    );
            entity_type(dummy_entity)=is_parameter_p?
                make_type_variable(make_variable(make_basic_int(DEFAULT_INTEGER_TYPE_SIZE),NIL,NIL)):
                copy_type(t);
            entity_storage(dummy_entity)=make_storage_formal(make_formal(dummy_entity,++i));


            formal_parameters=CONS(PARAMETER,make_parameter(
                        is_parameter_p?make_type_variable(make_variable(make_basic_int(DEFAULT_INTEGER_TYPE_SIZE),NIL,NIL)):copy_type(t),
                        fortran_module_p(get_current_module_entity())?make_mode_reference():make_mode_value(),
                        make_dummy_identifier(dummy_entity)),formal_parameters);

            /* this adds the effective parameter */
            expression effective_parameter = (expression)hash_get(entity_to_effective_parameter,e);
            if(effective_parameter == HASH_UNDEFINED_VALUE)
                effective_parameter = entity_to_expression(e);

            effective_parameters=CONS(EXPRESSION,effective_parameter,effective_parameters);
        }
        /* this is a constant variable */
        else if(entity_constant_p(e)) {
            AddLocalEntityToDeclarations(e,new_fun,new_body);
        }

    }
    formal_parameters=gen_nreverse(formal_parameters);
    effective_parameters=gen_nreverse(effective_parameters);
    hash_table_free(entity_to_effective_parameter);


    /* we need to patch parameters , effective parameters and body in C
     * because of by copy function call
	 * it's not needed if
	 * - the parameter is only read
	 * - it's an array / pointer
     */
    if(c_module_p(get_current_module_entity()))
    {
		list iter = effective_parameters;
        FOREACH(PARAMETER,p,formal_parameters)
        {
			expression x = EXPRESSION(CAR(iter));
            entity ex = reference_variable(expression_reference(x));
            entity e = dummy_identifier(parameter_dummy(p));
            if( type_variable_p(entity_type(ex)) ) {
                variable v = type_variable(entity_type(ex));
                bool entity_written=false;
                FOREACH(STATEMENT,stmt,statements_to_outline)
                    entity_written|=find_write_effect_on_entity(stmt,ex);

                if( (!basic_pointer_p(variable_basic(v))) &&
                        ENDP(variable_dimensions(v)) &&
                        entity_written
                  )
                {
                    entity_type(e)=make_type_variable(
                            make_variable(
                                make_basic_pointer(copy_type(entity_type(e))),
                                NIL,
                                NIL
                                )
                            );
                    parameter_type(p)=copy_type(entity_type(e));
                    syntax s = expression_syntax(x);
                    expression X = make_expression(s,normalized_undefined);
                    expression_syntax(x)=make_syntax_call(make_call(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME),CONS(EXPRESSION,X,NIL)));
                    gen_context_multi_recurse(new_body,ex,
                            statement_domain,gen_true,patch_outlined_reference_in_declarations,
                            loop_domain,gen_true,bug_in_patch_outlined_reference,
                            expression_domain,gen_true,patch_outlined_reference,
                            0);
                }
            }
			POP(iter);
        }
		pips_assert("no effective parameter left", ENDP(iter));
    }

    /* prepare parameters and body*/
    functional_parameters(type_functional(entity_type(new_fun)))=formal_parameters;
	FOREACH(PARAMETER,p,formal_parameters) {
		code_declarations(value_code(entity_initial(new_fun))) =
			gen_nconc(
					code_declarations(value_code(entity_initial(new_fun))),
					CONS(ENTITY,dummy_identifier(parameter_dummy(p)),NIL));
	}

    /* we can now begin the outlining */
    bool saved = get_bool_property(STAT_ORDER);
    set_bool_property(STAT_ORDER,false);
    text t = text_named_module(new_fun, new_fun /*get_current_module_entity()*/, new_body);
    add_new_module_from_text(outline_module_name, t, fortran_module_p(get_current_module_entity()), compilation_unit_of_module(get_current_module_name()) );
    set_bool_property(STAT_ORDER,saved);
	/* horrible hack to prevent declaration duplication
	 * signed : Serge Guelton
	 */
	gen_free_list(code_declarations(EntityCode(new_fun)));
	code_declarations(EntityCode(new_fun))=NIL;

    /* we need to free them now, otherwise recompilation fails */
    FOREACH(PARAMETER,p,formal_parameters) {
        entity e = dummy_identifier(parameter_dummy(p));
        if(entity_variable_p(e)) {
            free_type(entity_type(e));
            entity_type(e)=type_undefined;
        }
    }

    /* and return the replacement statement */
    instruction new_inst =  make_instruction_call(make_call(new_fun,effective_parameters));
    statement new_stmt = statement_undefined;

    /* perform substitution :
     * replace the original statements by a single call
     * and patch the remaining statement (yes it's ugly)
     */
    FOREACH(STATEMENT,old_statement,statements_to_outline)
    {
        free_instruction(statement_instruction(old_statement));
        if(statement_undefined_p(new_stmt))
        {
            statement_instruction(old_statement)=new_inst;
            new_stmt=old_statement;
        }
        else
            statement_instruction(old_statement)=make_continue_instruction();
        gen_free_list(statement_declarations(old_statement));
        statement_declarations(old_statement)=NIL;
    }
    return new_stmt;
}



/**
 * @brief entry point for outline module
 * outlining will be performed using either comment recognition
 * or interactively
 *
 * @param module_name name of the module containg the statements to outline
 */
bool
outline(char* module_name)
{
    /* prelude */
    set_current_module_entity(module_name_to_entity( module_name ));
    set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, TRUE) );
 	set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));
 	set_rw_effects((statement_effects)db_get_memory_resource(DBR_REGIONS, module_name, TRUE));

    /* retrieve name of the outiled module */
    string outline_module_name = get_string_property_or_ask("OUTLINE_MODULE_NAME","outline module name ?\n");

    /* retrieve statement to outline */
    list statements_to_outline = find_statements_with_pragma(get_current_module_statement(),OUTLINE_PRAGMA) ;
    if(ENDP(statements_to_outline)) {
        string label_name = get_string_property("OUTLINE_LABEL");
        if( empty_string_p(label_name) ) {
            statements_to_outline=find_statements_interactively(get_current_module_statement());
        }
        else  {
            statement statement_to_outline = find_statement_from_label_name(get_current_module_statement(),get_current_module_name(),label_name);
            if(statement_loop_p(statement_to_outline) && get_bool_property("OUTLINE_LOOP_STATEMENT"))
                statement_to_outline=loop_body(statement_loop(statement_to_outline));
            statements_to_outline=make_statement_list(statement_to_outline);
        }
    }

    /* apply outlining */
    (void)outliner(outline_module_name,statements_to_outline);


    /* validate */
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,
			   get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, module_name, compute_callees(get_current_module_statement()));

    /*postlude*/
	reset_cumulated_rw_effects();
    reset_rw_effects();
    reset_current_module_entity();
    reset_current_module_statement();

    return true;
}
/**  @} */

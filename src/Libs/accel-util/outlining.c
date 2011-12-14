/*

  $Id: outlining.c -1   $

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
/**
 * @file outlining.c
 * @brief add outlining support to pips, with two flavors
 *
 * @author Serge Guelton <serge.guelton@enst-bretagne.fr>
 * @date 2009-01-07
 */
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"
#include "text.h"
#include "pipsdbm.h"
#include "pipsmake.h"
#include "resources.h"
#include "properties.h"
#include "misc.h"
#include "control.h"
#include "callgraph.h"
#include "effects-generic.h"
#include "effects-convex.h"
#include "preprocessor.h"
#include "expressions.h"
#include "text-util.h"
#include "parser_private.h"
#include "accel-util.h"
/**
 * @name outlining
 * @{ */

#define STAT_ORDER "PRETTYPRINT_STATEMENT_NUMBER"

static void get_loop_locals_and_remove_walker(statement st, set s) {
    if(statement_loop_p(st)) {
        loop l = statement_loop(st);
        list locals = loop_locals(l);

        /* loop_locals is sometimes too big,
         * mainly due to the way gpu_ify is done ...
         */
        set re0 = get_referenced_entities(loop_body(l));
        set re1 = get_referenced_entities(loop_range(l));
        set_add_element(re0,re0,loop_index(l));
        set_union(re0,re0,re1);
        set_free(re1);

        /* validate everything */
        FOREACH(ENTITY,el,locals) {
            if(set_belong_p(re0,el))
                set_add_element(s,s,el);
        }
        set_free(re0);

        /* remove instructions for later processing */
        free_instruction(statement_instruction(st));
        statement_instruction(st)=instruction_undefined;
    }
}

static
set get_loop_locals_and_remove(statement st) {
    set locals = set_make(set_pointer);
    gen_context_recurse(st, locals, statement_domain, gen_true, get_loop_locals_and_remove_walker);
    return locals;
}

/* try hard to reproduce in / out regions with only loop_locals
 * it is time to move to regions ... are they stable enough ?
 */
static
set get_private_entities(statement st){
    statement clone = copy_statement(st);
    set locals = get_loop_locals_and_remove(clone);
    set re = get_referenced_entities(clone);
    set_difference(locals,locals,re);
    set_free(re);
    free_statement(clone);
    return locals;
}

static bool skip_values(void *v) {
    return !INSTANCE_OF(value,(gen_chunkp)v);
}

static bool skip_constants_intrinsics_members(entity e) {
    return entity_not_constant_or_intrinsic_p(e) &&
        !member_entity_p(e);
}


static
void sort_entities_with_dep(list *l)
{
    set params = set_make(set_pointer);
    FOREACH(ENTITY,e,*l)
    {
        if(!storage_rom_p(entity_storage(e))) { //<< to avoid gathering useless variables, unsure of the validity
            set e_ref = get_referenced_entities_filtered(entity_type(e),skip_values,skip_constants_intrinsics_members);
            set_del_element(e_ref,e_ref,e);
            set_union(params,params,e_ref);
            set_free(e_ref);
        }
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
        expression parent = (expression)gen_get_ancestor(expression_domain,r);
        unnormalize_expression(parent);
        if(basic_pointer_p(entity_basic(ctxt->new))) /*sg:may cause issues if basic_pointer_p(old) ? */
        {
            pips_assert("parent exist",parent);
            //free_syntax(expression_syntax(parent)); /* sg a small leak is better than a crash :) */
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

static list statements_referenced_entities(list statements)
{
    list referenced_entities = NIL;
    set sreferenced_entities = set_make(set_pointer);

    FOREACH(STATEMENT, s, statements)
    {
        /* we don't want initial values of outer entities */
        set tmp = get_referenced_entities_filtered(s,skip_values,skip_constants_intrinsics_members);
        /* gather local initial values skipped by the previous call*/
        list decl = statement_to_declarations(s);
        FOREACH(ENTITY,e,decl) {
            set se = get_referenced_entities_filtered(entity_initial(e),skip_values,skip_constants_intrinsics_members);
            set_union(tmp,tmp,se);
            set_free(se);
            se = get_referenced_entities_filtered(entity_type(e),skip_values,skip_constants_intrinsics_members);
            set_union(tmp,tmp,se);
            set_free(se);
        }

        ifdebug(7) {
          pips_debug(0,"Statement :");
          print_statement(s);
          pips_debug(0,"referenced entities :");
          print_entities(set_to_list(tmp));
          fprintf(stderr,"\n");
        }

        set_union(sreferenced_entities,tmp,sreferenced_entities);
        set_free(tmp);
    }
    /* set to list */
    referenced_entities=set_to_list(sreferenced_entities);
    set_free(sreferenced_entities);
    return referenced_entities;
}
/**
 * purge the list of referenced entities by replacing calls to a[i][j] where i is a constant in statements
 * outlined_statements by a call to a single (new) variable
 */
static hash_table outliner_smart_references_computation(list outlined_statements,entity new_module)
{
  list referenced_entities = statements_referenced_entities(outlined_statements);

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
                                pips_assert("phi coef should be one",phi_coeff == VALUE_ONE);
                                Pvecteur lhs_vect = vect_del_var(cvect,phi);
                                vect_chg_sgn(lhs_vect);
                                pips_assert("phi coef should be mentioned only once",expression_undefined_p(index_value));
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
    gen_free_list(referenced_entities);

    return entity_to_init;
}
static list statements_localize_declarations(list statements,entity module,statement module_statement)
{
    list sd = statements_to_declarations(statements);
    list localized = NIL;
    FOREACH(STATEMENT, s, statements)
    {
        /* We want to declare private variables **that are never used else where** as locals, but it may not
           be valid */
        list private_ents = private_variables(s);
        gen_sort_list(private_ents,(gen_cmp_func_t)compare_entities);
        FOREACH(ENTITY,e,private_ents)
        {
            if(gen_chunk_undefined_p(gen_find_eq(e,sd))) {
                if(formal_parameter_p(e) || 
                        (!get_bool_property("OUTLINE_ALLOW_GLOBALS") && top_level_entity_p(e) )) { // otherwise bad interaction with formal parameter pretty printing
                    localized=CONS(ENTITY,e,localized); // this is to make sure that the original `e' is removed from the referenced entities too
                    entity ep = make_new_scalar_variable_with_prefix(
                            entity_user_name(e),
                            module,
                            copy_basic(entity_basic(e))
                            );
                    replace_entity(s,e,ep);
                    e=ep;
                }
                AddLocalEntityToDeclarations(e,module,module_statement);
                localized=CONS(ENTITY,e,localized);
            }
        }
        gen_free_list(private_ents);
    }
    gen_free_list(sd);
    return localized;
}
static void outliner_extract_loop_bound(statement sloop, hash_table entity_to_effective_parameter)
{
    loop l =statement_loop(sloop);
    range r = loop_range(l);
    expression upper = range_upper(r);
    if(!expression_scalar_p(upper))
    {
        basic b = basic_of_expression(upper);
        if(basic_overloaded_p(b)) { free_basic(b); b = make_basic_int(DEFAULT_INTEGER_TYPE_SIZE); }
        entity holder = make_new_scalar_variable(get_current_module_entity(),b);
        hash_put(entity_to_effective_parameter,holder,
                MakeBinaryCall(entity_intrinsic(PLUS_OPERATOR_NAME),upper,int_to_expression(1)));
        range_upper(r)=
                MakeBinaryCall(entity_intrinsic(MINUS_OPERATOR_NAME),entity_to_expression(holder),int_to_expression(1));
    }
}
static void convert_pointer_to_array_aux(expression exp,entity e) {
    if(expression_reference_p(exp)) {
        reference r = expression_reference(exp);
        if(same_entity_p(reference_variable(r),e)) {
            syntax syn = expression_syntax(exp);
            expression_syntax(exp)=syntax_undefined;
            syntax syn2=make_syntax_call(
                    make_call(
                        entity_intrinsic(ADDRESS_OF_OPERATOR_NAME),
                        CONS(EXPRESSION,
                            make_expression(
                                syn,normalized_undefined
                                ),
                            NIL)
                        )
                    );
            update_expression_syntax(exp,syn2);
        }
    }
}
static void convert_pointer_to_array_aux2(statement s, entity e){
    FOREACH(ENTITY,en,statement_declarations(s))
    gen_context_recurse(entity_initial(en),e,expression_domain,gen_true,convert_pointer_to_array_aux);
}

static void convert_pointer_to_array(entity e,entity re, expression x, list statements) {
    type t =entity_type(e);
    type pointed_type = basic_pointer(
            variable_basic(
                type_variable(t)
                )
            );
    basic_pointer(
            variable_basic(
                type_variable(t)
                )
            )=type_undefined;
    free_type(t);
    entity_type(e)=pointed_type;
    FOREACH(STATEMENT,s,statements) {
        gen_context_multi_recurse(s,re,expression_domain,gen_true,convert_pointer_to_array_aux,
                statement_domain,gen_true,convert_pointer_to_array_aux2,
                NULL);
        cleanup_subscripts(s);
    }

    /* crado */
    syntax syn = expression_syntax(x);
    expression_syntax(x)=syntax_undefined;
    syntax syn2=make_syntax_call(
            make_call(
                entity_intrinsic(DEREFERENCING_OPERATOR_NAME),
                CONS(EXPRESSION,
                    make_expression(
                        syn,normalized_undefined
                        ),
                    NIL)
                )
            );
    update_expression_syntax(x,syn2);
}

static void outline_remove_duplicates(list *entities) {
    list tentities = gen_copy_seq(*entities);
    for(list iter=tentities;!ENDP(iter);POP(iter)) {
        entity e0 = ENTITY(CAR(iter));
        FOREACH(ENTITY,e1,CDR(iter)) {
            if( !same_entity_p(e0,e1) &&
                    same_entity_lname_p(e0,e1)) {
                gen_remove_once(entities,e1);
                break;
            }
        }
    }
    gen_free_list(tentities);
}

hash_table outliner_init(entity new_fun, list statements_to_outline)
{
  /* try to be smart concerning array references */
  if(get_bool_property("OUTLINE_SMART_REFERENCE_COMPUTATION"))
    return outliner_smart_references_computation(statements_to_outline, new_fun);
  else
    return hash_table_make(hash_pointer,1);
}

list outliner_scan(entity new_fun, list statements_to_outline, statement new_body)
{
    /* Retrieve referenced entities */
    list referenced_entities = statements_referenced_entities(statements_to_outline);

    ifdebug(5) {
      pips_debug(0,"Referenced entities :\n");
      print_entities(referenced_entities);
    }

    /* Retrieve declared entities */
    list localized = statements_localize_declarations(statements_to_outline,new_fun,new_body);
    list declared_entities = statements_to_declarations(statements_to_outline);
    declared_entities=gen_nconc(declared_entities,localized);

    /* get the relative complements and create the parameter list*/
    gen_list_and_not(&referenced_entities,declared_entities);
    gen_free_list(declared_entities);


    /* purge the functions from the parameter list, we assume they are declared externally
     * also purge the formal parameters from other modules, gathered by get_referenced_entities but wrong here
     */
    list tmp = gen_copy_seq(referenced_entities);
    FOREACH(ENTITY,e,referenced_entities)
    {
        //basic b = entity_basic(e);
        /* function should be added to compilation unit */
        if(entity_function_p(e))
	  {
            ;//AddEntityToModuleCompilationUnit(e,get_current_module_entity());
	    if(!fortran_module_p(new_fun)) /* fortran function results must be declared in the new function */
            gen_remove_once(&referenced_entities,e);
	  }
    }
    gen_free_list(tmp);



    /* remove global variables if needed */
    if(get_bool_property("OUTLINE_ALLOW_GLOBALS"))
    {
        string cu_name = compilation_unit_of_module(get_current_module_name());
        entity cu = module_name_to_entity(cu_name);
        list cu_decls = entity_declarations(cu);

        list tmp_list=NIL;

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


    /* in some rare case, we can have too functions with the same local name */
    outline_remove_duplicates(&referenced_entities);
    return referenced_entities;
}
/* create a new type from given type, eventually renaming unnamed structures inside
 * all new entities generated in the process are added to cu*/
static
type type_to_named_type(type t, entity cu) {
    type tp = t;
    variable tv = type_variable(tp);
    basic b = variable_basic(tv);
    if(basic_derived_p(b)) {
        entity at = basic_derived(b);
        bool is_struct = false;
        if( (is_struct=strncmp(entity_user_name(at),DUMMY_STRUCT_PREFIX,sizeof(DUMMY_STRUCT_PREFIX)-1)==0) 
                || strncmp(entity_user_name(at),DUMMY_UNION_PREFIX, sizeof(DUMMY_UNION_PREFIX)-1)==0 )
        {
            /* create a new named type */
            int index = 0;
            char* name = strdup("");;
            do {
                free(name);
                asprintf(&name, "%s" MODULE_SEP_STRING "%cstruct_%d", entity_local_name(cu), is_struct?STRUCT_PREFIX_CHAR:UNION_PREFIX_CHAR,index++);
            } while(!gen_chunk_undefined_p(gen_find_tabulated(name, entity_domain)));

            FOREACH(ENTITY,filed,is_struct?type_struct(entity_type(at)):type_union(entity_type(at))) {
            }
            list newfields = NIL;
            list fields = is_struct ? type_struct(entity_type(at)) : type_union(entity_type(at));
            FOREACH(ENTITY, field, fields) {
                char * ename;
                asprintf(&ename,"%s" MODULE_SEP_STRING "struct_%d%s" , entity_local_name(cu), index -1, strrchr(entity_name(field),MEMBER_SEP_CHAR));
                entity e = make_entity(ename,
                        copy_type(entity_type(field)),
                        copy_storage(entity_storage(field)),
                        copy_value(entity_initial(field))
                        );
                newfields=CONS(ENTITY,e,newfields);
            }
            newfields=gen_nreverse(newfields);
            type newet;
            if(is_struct) newet = make_type_struct(newfields);
            else newet = make_type_union(newfields);

            entity newe = make_entity(name, newet, copy_storage(entity_storage(at)), copy_value(entity_initial(at)));
            AddEntityToCompilationUnit(newe,cu);
            type newt =
                make_type_variable(
                    make_variable(
                        make_basic_derived(newe),
                        gen_full_copy_list(variable_dimensions(tv)),
                        gen_full_copy_list(variable_qualifiers(tv))
                        )
                    );
            return newt;
        }
    }
    return copy_type(t);
}

void outliner_parameters(entity new_fun,  statement new_body, list referenced_entities,
			 hash_table entity_to_effective_parameter,
			 list *effective_parameters_, list *formal_parameters_)
{
  string outline_module_name = (string)entity_user_name(new_fun);


    /* all variables are promoted parameters */
    list effective_parameters = *effective_parameters_;
    list formal_parameters = *formal_parameters_;
    intptr_t i=0;

    FOREACH(ENTITY,e,referenced_entities)
      {
        if( entity_variable_p(e) || entity_symbolic_p(e) )
        {
            pips_debug(6,"Add %s to outlined function parameters\n",entity_name(e));

            /* this create the dummy parameter */
            entity dummy_entity = FindOrCreateEntity(
                    outline_module_name,
                    entity_user_name(e)
                    );

            if(entity_symbolic_p(e))
                entity_type(dummy_entity) = fortran_module_p(new_fun)?
                    copy_type(functional_result(type_functional(entity_type(e)))):
                    make_type_variable(make_variable(make_basic_int(DEFAULT_INTEGER_TYPE_SIZE),NIL,NIL));
            else {
                if(!fortran_module_p(get_current_module_entity()))
                    entity_type(e) = type_to_named_type(entity_type(e),module_entity_to_compilation_unit_entity(get_current_module_entity()));
                entity_type(dummy_entity)=copy_type(entity_type(e));
            }


            entity_storage(dummy_entity)=make_storage_formal(make_formal(dummy_entity,++i));


            formal_parameters=CONS(PARAMETER,make_parameter(
                        copy_type(entity_type(dummy_entity)),
                        fortran_module_p(get_current_module_entity())?make_mode_reference():make_mode_value(),
                        make_dummy_identifier(dummy_entity)),formal_parameters);

            /* this adds the effective parameter */
            expression effective_parameter = (expression)hash_get(entity_to_effective_parameter,e);
            if(effective_parameter == HASH_UNDEFINED_VALUE)
                effective_parameter = entity_to_expression(e);

            effective_parameters=CONS(EXPRESSION,effective_parameter,effective_parameters);
        }
        /* this is a constant variable or fortran function result */
        else if(entity_constant_p(e)||entity_function_p(e)) {
	  AddLocalEntityToDeclarations(e,new_fun,new_body);
        }

    }
    *formal_parameters_=gen_nreverse(formal_parameters);
    *effective_parameters_=gen_nreverse(effective_parameters);
    hash_table_free(entity_to_effective_parameter);

    ifdebug(5) {
      pips_debug(0,"Formals : \n");
      print_parameters(*formal_parameters_);
      pips_debug(0,"Effectives : \n");
      print_expressions(*effective_parameters_);
     }
}

    /* we need to patch parameters , effective parameters and body in C
     * because parameters are passed by copy in function call
     * it's not needed if
     * - the parameter is only read
     * - it's an array / pointer
     *
     * Here a scalar will be passed by address and a prelude/postlude
     * will be used in the outlined module as below :
     *
     * void new_module( int *scalar_0 ) {
     *   int scalar;
     *   scalar = *scalar_0;
     *   ...
     *   // Work on scalar
     *   ...
     *   *scalar_0 = scalar;
     * }
     *
     */
void outliner_patch_parameters(list statements_to_outline, list referenced_entities, list effective_parameters, list formal_parameters,
			       statement new_body, statement begin, statement end)
{
  list iter  = effective_parameters;
  list riter = referenced_entities;

        FOREACH(PARAMETER,p,formal_parameters)
        {
            expression x = EXPRESSION(CAR(iter));
            entity re = ENTITY(CAR(riter));
            entity ex = entity_undefined;
            if(expression_reference_p(x))
                ex = reference_variable(expression_reference(x));

            entity e = dummy_identifier(parameter_dummy(p));
            if(!entity_undefined_p(ex)&& type_variable_p(entity_type(ex)) ) {
                variable v = type_variable(entity_type(ex));
                bool entity_written=false;
                FOREACH(STATEMENT,stmt,statements_to_outline) {
                  bool write_p = find_write_effect_on_entity(stmt,ex);
                  ifdebug(5) {
                    if(write_p) {
                      pips_debug(0,"Entity %s written by statement (%d) : \n",entity_name(ex),the_current_debug_level);
                      print_statement(stmt);
                    }
                  }
                  entity_written|=write_p;
                }

                if( (!basic_pointer_p(variable_basic(v))) &&
                        ENDP(variable_dimensions(v)) &&
                        entity_written
                  )
                {
                    entity fp = make_entity_copy_with_new_name(e,entity_name(e),false);
                    variable_dimensions(type_variable(entity_type(fp)))=
                        gen_append(variable_dimensions(type_variable(entity_type(fp))),
                        CONS(DIMENSION,make_dimension(int_to_expression(0),int_to_expression(0)),NIL)
                        );

                    parameter_type(p)=copy_type(entity_type(fp));
                    dummy_identifier(parameter_dummy(p))=fp;

                  //Change at call site (scalar=>&scalar)
                    syntax s = expression_syntax(x); // FIXME Leak ?
                    expression X = make_expression(s,normalized_undefined);
                    expression_syntax(x)=make_syntax_call(make_call(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME),CONS(EXPRESSION,X,NIL)));


                  //Create prelude && postlude
                    expression dereferenced = MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),entity_to_expression(fp));

                    // FIXME : is it ok to alias dereferenced expression in 2 statements ?
                    statement in = make_assign_statement(entity_to_expression(e),dereferenced);
                    statement out = make_assign_statement(dereferenced,entity_to_expression(e));

                    // Cheat on effects
                    store_cumulated_rw_effects_list(in,NIL);
                    store_cumulated_rw_effects_list(out,NIL);


                    insert_statement(begin,in,true);
                    insert_statement(end,out,false);

                    pips_debug(4,"Add declaration for %s",entity_name(e));
                    add_declaration_statement(new_body,e);

                }
            }
            if(type_variable_p(entity_type(re))) {
                variable v = type_variable(entity_type(re));
                if( basic_pointer_p(variable_basic(v)) &&
                        array_type_p(basic_pointer(variable_basic(v)))) {
                    convert_pointer_to_array(e,re,x,statements_to_outline);

                }
            }
            POP(iter);
            POP(riter);
        }
        pips_assert("no effective parameter left", ENDP(iter));
}

static void do_remove_entity_from_private(loop l, entity e) {
    gen_remove(&loop_locals(l),e);
    if(same_entity_p(e,loop_index(l))) loop_index(l)=entity_undefined;
}
static void do_remove_entity_from_decl(statement s, entity e) {
    gen_remove(&statement_declarations(s),e);
}

void outliner_file(entity new_fun, list formal_parameters, statement *new_body)
{
  string outline_module_name = (string)entity_user_name(new_fun);

    /* prepare parameters and body*/
    module_functional_parameters(new_fun)=formal_parameters;
    FOREACH(PARAMETER,p,formal_parameters) {
      code_declarations(value_code(entity_initial(new_fun))) =
          gen_nconc(code_declarations(value_code(entity_initial(new_fun))),
                    CONS(ENTITY,dummy_identifier(parameter_dummy(p)),NIL));
    }
    /* either use origin's compilation unit or a new one */
    char * cu_name = string_undefined;
    // In fortran we always want to generate the outline function
    // in its own new file
    if(fortran_module_p(get_current_module_entity())) {
      ;
    } else if(get_bool_property("OUTLINE_INDEPENDENT_COMPILATION_UNIT")) {
      // Declare in current module so that it's not undefined at call site
      AddEntityToModuleCompilationUnit(new_fun,get_current_module_entity());
    } else {
      cu_name = compilation_unit_of_module(get_current_module_name());
    }

    /* add a return at the end of the body, in all cases */
    insert_statement(*new_body, make_return_statement(new_fun), false);

    /* we can now begin the outlining */
    bool saved = get_bool_property(STAT_ORDER);
    set_bool_property(STAT_ORDER,false);
    text t = text_named_module(new_fun, new_fun /*get_current_module_entity()*/, *new_body);

    add_new_module_from_text(outline_module_name, t, fortran_module_p(get_current_module_entity()), cu_name );
    free_text(t);

    set_bool_property(STAT_ORDER,saved);



    /* horrible hack to prevent declaration duplication
     * signed : Serge Guelton
     */
    gen_free_list(code_declarations(EntityCode(new_fun)));
    code_declarations(EntityCode(new_fun))=NIL;

    /* we need to free them now, otherwise recompilation fails */
    FOREACH(PARAMETER,p,formal_parameters) {
        entity e = dummy_identifier(parameter_dummy(p));
        if(!type_undefined_p(entity_type(e)) &&
                entity_variable_p(e)) {
            free_type(entity_type(e));
            entity_type(e)=type_undefined;
        }
    }
}

statement outliner_call(entity new_fun, list statements_to_outline, list effective_parameters)
{

    /* and return the replacement statement */
    instruction new_inst =  make_instruction_call(make_call(new_fun,effective_parameters));
    statement new_stmt = statement_undefined;

    /* perform substitution :
     * replace the original statements by a single call
     * and patch the remaining statement (yes it's ugly)
     */
    FOREACH(STATEMENT,old_statement,statements_to_outline)
    {
        //free_instruction(statement_instruction(old_statement));
        if(statement_undefined_p(new_stmt))
        {
            statement_instruction(old_statement)=new_inst;
            new_stmt=old_statement;
        }
        else
            statement_instruction(old_statement)=make_continue_instruction();
        gen_free_list(statement_declarations(old_statement));
        statement_declarations(old_statement)=NIL;
        /* trash any extensions, they may not be valid now */
        free_extensions(statement_extensions(old_statement));
        statement_extensions(old_statement)=empty_extensions();

    }
    return new_stmt;
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
statement outliner(const char* outline_module_name, list statements_to_outline)
{
    pips_assert("there are some statements to outline",!ENDP(statements_to_outline));
    entity new_fun = make_empty_subroutine(outline_module_name, copy_language(module_language(get_current_module_entity())));

    statement new_body = make_block_statement(statements_to_outline);

    /* 1 : init */
    hash_table entity_to_effective_parameter = outliner_init(new_fun, statements_to_outline);

    /* pass loop bounds as parameters if required */
    const char* loop_label = get_string_property("OUTLINE_LOOP_BOUND_AS_PARAMETER");
    statement theloop = find_statement_from_label_name(get_current_module_statement(),get_current_module_name(),loop_label);
    if(!statement_undefined_p(theloop) && statement_loop(theloop))
      outliner_extract_loop_bound(theloop,entity_to_effective_parameter);

    /* 2 : scan */
    list referenced_entities = outliner_scan(new_fun, statements_to_outline, new_body);

    /* 3 : parameters */
    list effective_parameters = NIL;
    list formal_parameters = NIL;
    outliner_parameters(new_fun, new_body, referenced_entities, entity_to_effective_parameter, &effective_parameters, &formal_parameters);

    /* 4 : patch parameters */
    if(c_module_p(get_current_module_entity()) && get_bool_property("OUTLINE_WRITTEN_SCALAR_BY_REFERENCE"))
      outliner_patch_parameters(statements_to_outline, referenced_entities, effective_parameters, formal_parameters, new_body, new_body, new_body);

    /* 5 : file */
    outliner_file(new_fun, formal_parameters, &new_body);

    /* 6 : call */
    statement new_stmt = outliner_call(new_fun, statements_to_outline, effective_parameters);

    /* 7: remove obsolete entities, this is needed otherwise the IR keeps some obsolete data */
    list declared_entities = statements_to_declarations(statements_to_outline);
    FOREACH(ENTITY,de,declared_entities) {
        FOREACH(STATEMENT, sde, statements_to_outline) {
            gen_context_multi_recurse(sde, de, 
                    statement_domain, gen_true, do_remove_entity_from_decl,
                    loop_domain, gen_true, do_remove_entity_from_private,
                    NULL);
            gen_remove(&entity_declarations(get_current_module_entity()), de);
            free_entity(de);
        }
    }
    gen_free_list(declared_entities);


    return new_stmt;
}



/**
 * @brief entry point for outline module
 * outlining will be performed using either comment recognition
 * or interactively
 *
 * @param module_name name of the module containing the statements to outline
 */
bool
outline(char* module_name)
{
    /* prelude */
    set_current_module_entity(module_name_to_entity( module_name ));
    set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, true) );
    set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, true));
    set_rw_effects((statement_effects)db_get_memory_resource(DBR_REGIONS, module_name, true));

    debug_on("OUTLINE_DEBUG_LEVEL");

    /* retrieve name of the outlined module */
    const char* outline_module_name = get_string_property_or_ask("OUTLINE_MODULE_NAME","outline module name ?\n");

    // Check the language. In case of Fortran the module name must be in
    // capital letters.
    char * omn=strdup(outline_module_name);
    if(fortran_module_p(get_current_module_entity()))
        omn=strupper(omn,omn);

    /* retrieve statement to outline */
    list statements_to_outline = find_statements_with_pragma(get_current_module_statement(),get_string_property("OUTLINE_PRAGMA")) ;
    if(ENDP(statements_to_outline)) {
        const char* label_name = get_string_property("OUTLINE_LABEL");
        if( empty_string_p(label_name) ) {
            statements_to_outline=find_statements_interactively(get_current_module_statement());
        }
        else  {
            statement statement_to_outline = find_statement_from_label_name(get_current_module_statement(),get_current_module_name(),label_name);
            if(statement_undefined_p(statement_to_outline))
                pips_user_error("Could not find loop labeled %s\n",label_name);
            if(statement_loop_p(statement_to_outline) && get_bool_property("OUTLINE_LOOP_STATEMENT"))
                statement_to_outline=loop_body(statement_loop(statement_to_outline));
            statements_to_outline=make_statement_list(statement_to_outline);
        }
    }

    /* apply outlining */
    (void)outliner(omn,statements_to_outline);
    free(omn);


    debug_off();

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


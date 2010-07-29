#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linear.h"
#include "genC.h"
#include "misc.h"
#include "ri.h"

#include "ri-util.h"

/** @defgroup entity replacement in statements
 * it is not unusual to generate a new entity,
 * this helper functions will take care of substituing all referenced to
 * an old entity by reference in new entities
 * @{
 */

struct entity_pair
{
    entity old;
    entity new;
};

static void
replace_entity_declaration_walker(statement s, struct entity_pair* thecouple)
{
    FOREACH(ENTITY,decl_ent,statement_declarations(s))
    {
        replace_entity(decl_ent,thecouple->old,thecouple->new);
    }
}
static void
replace_entity_reference_walker(reference r,struct entity_pair* thecouple)
{
  if(same_entity_p(thecouple->old,reference_variable(r)))
    reference_variable(r)=thecouple->new;
}

static void replace_entity_loop_walker(loop l, struct entity_pair* thecouple)
{
	string emn_l = entity_module_name(loop_index(l));
	string emn_o = entity_module_name(thecouple->old);
	const char * eun_l = entity_user_name(loop_index(l));
	const char * eun_o = entity_user_name(thecouple->old);
	if( same_string_p(emn_l,emn_o) && same_string_p(eun_l,eun_o))
	{
		loop_index(l) = thecouple->new;
	}
    list ll=NIL;
    FOREACH(ENTITY,e,loop_locals(l))
    {
      if(same_entity_p(e,thecouple->old))
        ll=CONS(ENTITY,thecouple->new,ll);
      else
        ll=CONS(ENTITY,e,ll);
    }
    gen_free_list(loop_locals(l));
    loop_locals(l)=gen_nreverse(ll);
}

/**
 * @brief Recursievly substitute an entity to an old one in a statement
 *
 * @param s newgen type where the substitution must be done
 * @param old old entity
 * @param new new entity
 */
void
replace_entity(void* s, entity old, entity new) {
  struct entity_pair thecouple = { old, new };
  if( INSTANCE_OF(entity,(gen_chunkp)s) ) {
      replace_entity(entity_type((entity)s),old,new);
      value v = entity_initial((entity)s);
      if( !value_undefined_p(v) && value_expression_p( v ) )
          replace_entity(v,old,new);
  }
  else {
      gen_context_multi_recurse(s, &thecouple,
              reference_domain, gen_true, replace_entity_reference_walker,
              //expression_domain, gen_true, replace_entity_expression_walker,
              statement_domain, gen_true, replace_entity_declaration_walker,
              loop_domain, gen_true, replace_entity_loop_walker,
              NULL);
  }
}

void
replace_entities(void* s, hash_table ht)
{
    HASH_MAP(k, v, replace_entity(s,(entity)k,(entity)v);, ht);
}

/** Replace an old reference by a reference to a new entity in a statement
 */
void
replace_reference(void* s, reference old, entity new) {
  /* If the reference is a scalar, it's similar to replace_entity,
     otherwise, it's replace_entity_by_expression */
  if (ENDP(reference_indices(old)))
    replace_entity(s, reference_variable(old), new);
  else {
    pips_internal_error("not implemented yet");
  }
}


struct param { entity ent; expression exp; set visited_entities;};
static
void replace_entity_by_expression_expression_walker(expression e, struct param *p)
{
    if( expression_reference_p(e) )
    {
        reference r = expression_reference(e);
        if(same_entity_p(p->ent, reference_variable(r) ))
        {
            syntax syn = copy_syntax(expression_syntax(p->exp));
            if(!ENDP(reference_indices(r))) {
                list indices = gen_full_copy_list(reference_indices(r));
                /* if both are references , concatenante indices */
                if(expression_reference_p(p->exp))
                {
                    reference pr = expression_reference(p->exp);
                    syn = make_syntax_reference(
                            make_reference(
                                reference_variable(pr),
                                gen_nconc(gen_full_copy_list(reference_indices(pr)),indices)
                                )
                            );
                }
                /* else generate a subscript */
                else {
                    syn = make_syntax_subscript(make_subscript(make_expression(syn,normalized_undefined),indices));
                }
            }
            free_syntax(expression_syntax(e));
            expression_syntax(e) = syn;
            unnormalize_expression(e);
        }
    }
    else if(expression_call_p(e))
    {
        call c = expression_call(e);
        if(call_constant_p(c) && same_entity_p(p->ent,call_function(c)))
        {
            unnormalize_expression(e);
            free_syntax(expression_syntax(e));
            expression_syntax(e)=copy_syntax(expression_syntax(p->exp));
        }

    }
}

static
void replace_entity_by_expression_entity_walker(entity e, struct param *p)
{
    if( ! set_belong_p(p->visited_entities,e)) {
        set_add_element(p->visited_entities,p->visited_entities,e);
        value v = entity_initial(e);
        if( value_expression_p(v) )
            gen_context_recurse(value_expression(v),p,expression_domain,gen_true,replace_entity_by_expression_expression_walker);
        gen_context_recurse(entity_type(e),p,expression_domain,gen_true,replace_entity_by_expression_expression_walker);
    }
}


static
void replace_entity_by_expression_declarations_walker(statement s, struct param *p)
{
    FOREACH(ENTITY,e,statement_declarations(s))
        replace_entity_by_expression_entity_walker(e,p);

}

static
void replace_entity_by_expression_loop_walker(loop l, struct param *p)
{
    replace_entity_by_expression_entity_walker(loop_index(l),p);
}
void
replace_entity_by_expression_with_filter(void* s, entity ent, expression exp,bool (*filter)(expression))
{
    struct param p = { ent, exp, set_make(set_pointer)};
    gen_context_multi_recurse(s,&p,
            expression_domain,filter,replace_entity_by_expression_expression_walker,
            statement_domain,gen_true,replace_entity_by_expression_declarations_walker,
			loop_domain,gen_true, replace_entity_by_expression_loop_walker,
            NULL);
    set_free(p.visited_entities);
}

/** 
 * replace all reference to entity @a ent by expression @e exp
 * in @a s. @s can be any newgen type !
 */
void
replace_entity_by_expression(void* s, entity ent, expression exp)
{
    replace_entity_by_expression_with_filter(s,ent,exp,(bool(*)(expression))gen_true);
}
void
replace_entities_by_expression(void* s, hash_table ht)
{
    HASH_MAP(k, v, replace_entity_by_expression(s,(entity)k,(expression)v);, ht);
}
/** @} */

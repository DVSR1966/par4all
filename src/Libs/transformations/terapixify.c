/*
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
 * @file terapixify.c
 * apply transformations required to generate terapix microcode
 * @author Serge Guelton <serge.guelton@enst-bretagne.fr>
 * @date 2009-07-01
 */
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <ctype.h>


#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "text.h"
#include "pipsdbm.h"
#include "resources.h"
#include "properties.h"
#include "misc.h"
#include "conversion.h"
#include "control.h"
#include "callgraph.h"
#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"
#include "preprocessor.h"
#include "text-util.h"
#include "transformations.h"
#include "parser_private.h"
#include "syntax.h"
#include "c_syntax.h"



/** 
 * computes the distance between the lower bound and the upper bound of the range
 * It @a count_iter is set to true, it computes the number of iterations instead 
 * @param r range to analyse
 * @param mode wether we compute the distance or count the number of iterations
 * @return appropriate distance or count
 */
expression range_to_expression(range r,enum range_to_expression_mode mode)
{
    expression distance =  make_op_exp(PLUS_OPERATOR_NAME,
            copy_expression(range_upper(r)),
            make_op_exp(MINUS_OPERATOR_NAME,
                make_expression_1(),
                copy_expression(range_lower(r))));
    if( range_to_nbiter_p(mode) ) distance = make_op_exp(DIVIDE_OPERATOR_NAME,distance,copy_expression(range_increment(r)));
    return distance;
}

/** 
 * converts a loop range to a dma call from a memory @a from to another memory @a to
 * 
 * @param from expression giving the adress of the input memory
 * @param to expression giving the adress of the output memory
 * @param r range to analyze
 * @param m kind of call to generate
 * 
 * @return 
 */
static
call range_to_dma(expression from,
		  expression to,
		  range r,
		  enum region_to_dma_switch m)
{
  expression dest;
  list args;
  string function_name =
    dma_load_p(m) ? get_string_property("KERNEL_LOAD_STORE_LOAD_FUNCTION")
    : dma_store_p(m) ? get_string_property("KERNEL_LOAD_STORE_STORE_FUNCTION")
    : dma_allocate_p(m) ?
    get_string_property("KERNEL_LOAD_STORE_ALLOCATE_FUNCTION")
    : get_string_property("KERNEL_LOAD_STORE_DEALLOCATE_FUNCTION");

  entity mcpy = module_name_to_entity(function_name);
  if (entity_undefined_p(mcpy))
    pips_user_error("Cannot find \"%s\" method. Are you sure you have set\n"
		    "KERNEL_LOAD_STORE_..._FUNCTION "
		    "set to a defined entity and added the correct .c file?\n",function_name);

  if (dma_allocate_p(m))
    /* Need the address for the allocator to modify the pointer itself: */
    dest = MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME), to);
  else if (!dma_allocate_p(m))
    /* Except for the deallocation, the original array is referenced
       throudh pointer dereferencing: */
    dest = MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME), to);

  if (dma_deallocate_p(m))
    args = make_expression_list(dest);
  else {
    expression transfer_size = range_to_expression(r,range_to_distance);

    if (dma_load_p(m) || dma_store_p(m)) {
      expression source = from;
      args = make_expression_list(source, dest, transfer_size);
    }
    else
      args = make_expression_list(dest, transfer_size);
  }
  return make_call(mcpy, args);

}


#if 0
static
statement region_to_dma(statement stat, enum region_to_dma_switch s)
{
  list effects = dma_load_p(s) ?
    load_statement_in_regions(stat):
    load_statement_out_regions(stat);

  list ranges = NIL;
  list statements = NIL;
  FOREACH(EFFECT,eff,effects)
    {
      statement the_dma = statement_undefined;
      reference r = effect_any_reference(eff);
      Psysteme sc = sc_dup(region_system(eff));
      sc_transform_eg_in_ineg(sc);

      FOREACH(EXPRESSION,index,reference_indices(r))
        {
	  Variable endex = expression_to_entity(index);
	  Pcontrainte lower,upper;
	  constraints_for_bounds(endex,&sc_inegalites(sc),&lower,&upper);
	  expression lower_bound=expression_undefined, upper_bound=expression_undefined;
	  if(!CONTRAINTE_UNDEFINED_P(lower)) {
	    lower_bound= constraints_to_loop_bound(lower,endex,true,entity_intrinsic(DIVIDE_OPERATOR_NAME));
	  }
	  else
	    pips_internal_error("failed to get lower constraint on %s\n",entity_user_name((entity)endex));
	  if(!CONTRAINTE_UNDEFINED_P(upper)) {
	    upper_bound= constraints_to_loop_bound(upper,endex,false,entity_intrinsic(DIVIDE_OPERATOR_NAME));
	  }
	  else
	    pips_internal_error("failed to get upper constraint on %s\n",entity_user_name((entity)endex));

	  ranges=CONS(RANGE, make_range(lower_bound,upper_bound,make_expression_1()), ranges);
        }
      if(!ENDP(ranges))
        {

	  expression from = reference_to_expression( make_reference(reference_variable(r),CDR(reference_indices(r))) );
	  entity eto = make_temporary_array_entity(reference_variable(r),expression_undefined);
	  AddEntityToCurrentModule(eto);
	  expression to = reference_to_expression(make_reference(eto,NIL));
	  the_dma = instruction_to_statement(make_instruction_call(range_to_dma(from,to,RANGE(CAR(ranges)),s)));
	  /*FOREACH(RANGE,r,CDR(ranges))
            {
	    entity loop_index = make_new_scalar_variable(get_current_module_entity(),make_basic_int(DEFAULT_INTEGER_TYPE_SIZE));
	    AddLocalEntityToDeclarations(loop_index,get_current_module_entity(),get_current_module_statement());
	    loop l =make_loop(loop_index,r,the_dma,entity_empty_label(),make_execution_sequential(),NIL);
	    the_dma=instruction_to_statement(make_instruction_loop(l));
            }*/
        }
      else
	pips_internal_error("is this possible ?\n");
      statements=CONS(STATEMENT,the_dma,statements);
    }
  return make_block_statement(statements);

}
#endif

struct dma_pair {
  entity new_ent;
  enum region_to_dma_switch s;
};


/****** To move somewhere else. May be already exist ?


	Create an pointer to an array simlar to `efrom' initialized with
	expression `from'
 */
entity make_temporary_pointer_to_array_entity(entity efrom,
					      expression from) {
  basic pointee = copy_basic(variable_basic(type_variable(entity_type(efrom))));
  list dims = gen_copy_seq(variable_dimensions(type_variable(entity_type(efrom))));

  /* Make the pointer type */
  basic pointer = make_basic_pointer(make_type_variable(make_variable(pointee,
								      dims,
								      NIL)));
  /* Create the variable as a pointer */
  entity new = make_new_scalar_variable(get_current_module_entity(),
					pointer);
  /* Set its initial */
  entity_initial(new) = expression_undefined_p(from)?make_value_unknown():
    make_value_expression(make_expression(make_syntax_cast(make_cast(make_type_variable(make_variable(pointer,NIL,NIL)),copy_expression(from))),normalized_undefined));
  return new;
}

/* Compute a call to a DMA function from the effects of a statement

   @return a statement of the DMA transfers or statement_undefined if
   nothing needed
 */
static
statement effects_to_dma(statement stat,
			 enum region_to_dma_switch s,
			 hash_table e2e) {
  list rw_effects= load_cumulated_rw_effects_list(stat);
  list effects = NIL;

  FOREACH(EFFECT,e,rw_effects) {
    if ((dma_load_p(s) || dma_allocate_p(s) || dma_deallocate_p(s))
	&& action_read_p(effect_action(e)))
      effects=CONS(EFFECT,e,effects);
    else if ((dma_store_p(s)  || dma_allocate_p(s) || dma_deallocate_p(s))
	     && action_write_p(effect_action(e)))
      effects=CONS(EFFECT,e,effects);
  }

  list statements = NIL;
  FOREACH(EFFECT,eff,effects) {
    statement the_dma = statement_undefined;
    reference r = effect_any_reference(eff);
    entity re = reference_variable(r);
    struct dma_pair * val = (struct dma_pair *) hash_get(e2e, re);

    if( val == HASH_UNDEFINED_VALUE || (val->s != s) ) {
      if(!ENDP(variable_dimensions(type_variable(entity_type(re))))) {
	range the_range = make_range(make_expression_0(),
				     make_op_exp(MINUS_OPERATOR_NAME,
						 make_expression(make_syntax_sizeofexpression(make_sizeofexpression_type(entity_type(re))),normalized_undefined),
						 make_expression_1()),
				     make_expression_1());
	expression from = entity_to_expression(re);
	entity eto;
	if(val == HASH_UNDEFINED_VALUE) {
	  /* Replace the reference to the array re to *eto: */
	  eto = make_temporary_pointer_to_array_entity(re,expression_undefined);
	  AddEntityToCurrentModule(eto);
	  expression exp =
	    MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME),
			  entity_to_expression(eto));
	  replace_entity_by_expression(stat, re, exp);
	    //replace_entity(stat,re,eto);
	  val=malloc(sizeof(*val));
	  val->new_ent=eto;
	  val->s=s;
	  hash_put(e2e,re,val);
	}
	else {
	  eto = val->new_ent;
	  val->s=s;/*to avoid duplicate*/
	}
	expression to = reference_to_expression(make_reference(eto,NIL));
	the_dma = instruction_to_statement(make_instruction_call(range_to_dma(from,to,the_range,s)));
	statements=CONS(STATEMENT,the_dma,statements);
      }
    }
  }
  gen_free_list(effects);
  if (statements == NIL)
    return statement_undefined;
  else
    return make_block_statement(statements);
}


/**
 * create a statement eligible for outlining into a kernel
 * #1 find the loop flagged with loop_label
 * #2 make sure the loop is // with local index
 * #3 perform strip mining on this loop to make the kernel appear
 * #4 perform two outlining to separate kernel from host
 *
 * @param s statement where the kernel can be found
 * @param loop_label label of the loop to be turned into a kernel
 *
 * @return true as long as the kernel is not found
 */
static
bool do_kernelize(statement s, entity loop_label)
{
  if( same_entity_p(statement_label(s),loop_label) ||
      (statement_loop_p(s) && same_entity_p(loop_label(statement_loop(s)),loop_label)))
    {
      if( !instruction_loop_p(statement_instruction(s)) )
	pips_user_error("you choosed a label of a non-doloop statement\n");



      loop l = instruction_loop(statement_instruction(s));

      /* gather and check parameters */
      int nb_nodes = get_int_property("KERNELIZE_NBNODES");
      while(!nb_nodes)
        {
	  string ur = user_request("number of nodes for your kernel?\n");
	  nb_nodes=atoi(ur);
        }

      /* verify the loop is parallel */
      if( execution_sequential_p(loop_execution(l)) )
	pips_user_error("you tried to kernelize a sequential loop\n");
      if( !entity_is_argument_p(loop_index(statement_loop(s)),loop_locals(statement_loop(s))) )
	pips_user_error("you tried to kernelize a loop whose index is not private\n");

      /* we can strip mine the loop */
      loop_strip_mine(s,nb_nodes,-1);
      /* unfortunetly, the strip mining does not exactly does what we
	 want, fix it here

	 it is legal because we know the loop index is private,
	 otherwise the end value of the loop index may be used
	 incorrectly...
      */
      {
	statement s2 = loop_body(statement_loop(s));
	entity outer_index = loop_index(statement_loop(s));
	entity inner_index = loop_index(statement_loop(s2));
	replace_entity(s2,inner_index,outer_index);
	loop_index(statement_loop(s2))=outer_index;
	replace_entity(loop_range(statement_loop(s2)),outer_index,inner_index);
	if(!ENDP(loop_locals(statement_loop(s2)))) replace_entity(loop_locals(statement_loop(s2)),outer_index,inner_index);
	loop_index(statement_loop(s))=inner_index;
	replace_entity(loop_range(statement_loop(s)),outer_index,inner_index);
	gen_remove_once(&entity_declarations(get_current_module_entity()),outer_index);
	gen_remove_once(&statement_declarations(get_current_module_statement()),outer_index);
	loop_body(statement_loop(s))=make_block_statement(make_statement_list(s2));
	AddLocalEntityToDeclarations(outer_index,get_current_module_entity(),loop_body(statement_loop(s)));
	l = statement_loop(s);
      }

      string kernel_name=get_string_property_or_ask("KERNELIZE_KERNEL_NAME","name of the kernel ?");
      string host_call_name=get_string_property_or_ask("KERNELIZE_HOST_CALL_NAME","name of the fucntion to call the kernel ?");

      /* validate changes */
      callees kernels=(callees)db_get_memory_resource(DBR_KERNELS,"",true);
      callees_callees(kernels)= CONS(STRING,strdup(host_call_name),callees_callees(kernels));
      DB_PUT_MEMORY_RESOURCE(DBR_KERNELS,"",kernels);

      entity cme = get_current_module_entity();
      statement cms = get_current_module_statement();
      module_reorder(get_current_module_statement());
      DB_PUT_MEMORY_RESOURCE(DBR_CODE, get_current_module_name(),get_current_module_statement());
      DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, get_current_module_name(), compute_callees(get_current_module_statement()));
      reset_current_module_entity();
      reset_current_module_statement();

      /* recompute effects */
      proper_effects(module_local_name(cme));
      cumulated_effects(module_local_name(cme));
      set_current_module_entity(cme);
      set_current_module_statement(cms);
      set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS, get_current_module_name(), TRUE));
      /* outline the work and kernel parts*/
      outliner(kernel_name,make_statement_list(loop_body(l)));
      (void)outliner(host_call_name,make_statement_list(s));
      reset_cumulated_rw_effects();




      /* job done */
      gen_recurse_stop(NULL);

    }
  return true;
}


/**
 * turn a loop flagged with LOOP_LABEL into a kernel (GPU, terapix ...)
 *
 * @param module_name name of the module
 *
 * @return true
 */
bool kernelize(char * module_name)
{
  /* prelude */
  set_current_module_entity(module_name_to_entity( module_name ));
  set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, TRUE) );

  /* retreive loop label */
  string loop_label_name = get_string_property_or_ask("LOOP_LABEL","label of the loop to turn into a kernel ?\n");
  entity loop_label_entity = find_label_entity(module_name,loop_label_name);
  if( entity_undefined_p(loop_label_entity) )
    pips_user_error("label '%s' not found in module '%s' \n",loop_label_name,module_name);


  /* run kernelize */
  gen_context_recurse(get_current_module_statement(),loop_label_entity,statement_domain,do_kernelize,gen_null);

  /* validate */
  module_reorder(get_current_module_statement());
  DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,get_current_module_statement());
  DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, module_name, compute_callees(get_current_module_statement()));

  /*postlude*/
  reset_current_module_entity();
  reset_current_module_statement();
  return true;
}

bool flag_kernel(char * module_name)
{
  if (!db_resource_p(DBR_KERNELS, ""))
    pips_internal_error("kernels not initialized");
  callees kernels=(callees)db_get_memory_resource(DBR_KERNELS,"",true);
  callees_callees(kernels)= CONS(STRING,strdup(module_name),callees_callees(kernels));
  DB_PUT_MEMORY_RESOURCE(DBR_KERNELS,"",kernels);
  return true;
}

bool bootstrap_kernels(__attribute__((unused)) char * module_name)
{
  if (db_resource_p(DBR_KERNELS, ""))
    pips_internal_error("kernels already initialized");
  callees kernels=make_callees(NIL);
  DB_PUT_MEMORY_RESOURCE(DBR_KERNELS,"",kernels);
  return true;
}


static void
kernel_load_store_generator(statement s, string module_name)
{
  if(statement_call_p(s))
    {
      call c = statement_call(s);
      if(!call_intrinsic_p(c) &&
	 same_string_p(module_local_name(call_function(c)),module_name))
        {
	  statement allocates, loads, stores, deallocates;
#if 0
	  loads = region_to_dma(s,dma_load);
	  stores = region_to_dma(s,dma_store);
#else
	  hash_table e2e = hash_table_make(hash_pointer,HASH_DEFAULT_SIZE);
	  allocates = effects_to_dma(s,dma_allocate,e2e);
	  loads = effects_to_dma(s,dma_load,e2e);
	  stores = effects_to_dma(s,dma_store,e2e);
	  deallocates = effects_to_dma(s,dma_deallocate,e2e);
	  HASH_MAP(k,v,free(v),e2e);
	  hash_table_free(e2e);
#endif
	  /* Add the methods now if needed, in the correct order: */
	  if (loads != statement_undefined)
	    insert_a_statement(s, loads);
	  if (allocates != statement_undefined)
	    insert_a_statement(s, allocates);
	  if (stores != statement_undefined)
	    append_a_statement(s, stores);
	  if (deallocates != statement_undefined)
	    append_a_statement(s, deallocates);
        }
    }
}


/** Generate malloc/copy-in/copy-out on the call sites of this module.

    Do not work on global variables.
 */
bool kernel_load_store(char *module_name) {
  /* generate a load stores on each caller */
  {
    callees callers = (callees)db_get_memory_resource(DBR_CALLERS,module_name,true);
    FOREACH(STRING,caller_name,callees_callees(callers)) {
      /* prelude */
      set_current_module_entity(module_name_to_entity( caller_name ));
      set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, caller_name, true) );
      set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS, caller_name, TRUE));
      /*do the job */
      gen_context_recurse(get_current_module_statement(),module_name,statement_domain,gen_true,kernel_load_store_generator);
      /* validate */
      module_reorder(get_current_module_statement());
      DB_PUT_MEMORY_RESOURCE(DBR_CODE, caller_name,get_current_module_statement());
      DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, caller_name, compute_callees(get_current_module_statement()));

      /*postlude*/
      reset_cumulated_rw_effects();
      reset_current_module_entity();
      reset_current_module_statement();
    }
  }

  /*flag the module as kernel if not done */
  {
    callees kernels = (callees)db_get_memory_resource(DBR_KERNELS,"",true);
    bool found = false;
    FOREACH(STRING,kernel_name,callees_callees(kernels))
      if( (found=(same_string_p(kernel_name,module_name))) ) break;
    if(!found)
      callees_callees(kernels)=CONS(STRING,strdup(module_name),callees_callees(kernels));
    db_put_or_update_memory_resource(DBR_KERNELS,"",kernels,true);
  }


  return true;
}



/**
 * isolate_statement
 */

typedef struct {
    entity old;
    entity new;
    list offsets;
} isolate_param;

/** 
 * replace reference @p r on entity @p p->old  by a reference on entity @p p->new with offsets @p p->offsets
 */
static void isolate_patch_reference(reference r, isolate_param * p)
{
    if(same_entity_p(reference_variable(r),p->old))
    {
        reference_variable(r)=p->new;
        list offsets = p->offsets;
        FOREACH(EXPRESSION,index,reference_indices(r))
        {
            expression offset = EXPRESSION(CAR(offsets));
            unnormalize_expression(index);
            expression_syntax(index)=
                make_syntax_call(
                        make_call(
                            entity_intrinsic(MINUS_OPERATOR_NAME),
                            make_expression_list(
                                copy_expression(index),
                                copy_expression(offset)
                                )
                            )
                        );
            NORMALIZE_EXPRESSION(index);
            POP(offsets);
        }
    }
}

static void isolate_patch_entities(void * ,entity , entity ,list );
/** 
 * run isolate_patch_entities on all declared entities from @p s
 */
static void isolate_patch_statement(statement s, isolate_param *p)
{
    FOREACH(ENTITY,e,statement_declarations(s))
    {
        if(!value_undefined_p(entity_initial(e)))
            isolate_patch_entities(entity_initial(e),p->old,p->new,p->offsets);
    }
}

/** 
 * replace all references on entity @p old by references on entity @p new and adds offset @p offsets to its indices
 */
static void isolate_patch_entities(void * where,entity old, entity new,list offsets)
{
    isolate_param p = { old,new,offsets };
    gen_context_multi_recurse(where,&p,
            reference_domain,gen_true,isolate_patch_reference,
            statement_domain,gen_true,isolate_patch_statement,
            0);
}

/** 
 * generate a list of dimensions @p dims and of offsets @p from a region @p r
 * for example if r = a[phi0,phi1] 0<=phi0<=2 and 1<=phi1<=4
 * we get dims = ( (0,3), (0,4) )
 * and offsets = ( 0 , 1 )
 * 
 * @return false if we were enable to gather enough informations
 */
static bool region_to_minimal_dimensions(region r, list * dims, list *offsets)
{
    pips_assert("empty parameters\n",ENDP(*dims)&&ENDP(*offsets));
    reference ref = region_any_reference(r);
    Psysteme sc = sc_dup(region_system(r));
    sc_transform_eg_in_ineg(sc);
    FOREACH(EXPRESSION,index,reference_indices(ref))
    {
        Variable phi = expression_to_entity(index);
        Pcontrainte lower,upper;
        constraints_for_bounds(phi, &sc_inegalites(sc), &lower, &upper);
        if( !CONTRAINTE_UNDEFINED_P(lower) && !CONTRAINTE_UNDEFINED_P(upper))
        {
            /* this is a constant : the dimension is 1 and the offset is the bound */
            if(bounds_equal_p(phi,lower,upper))
            {
                expression bound = constraints_to_loop_bound(lower,phi,true,entity_intrinsic(DIVIDE_OPERATOR_NAME));
                *dims=CONS(DIMENSION,make_dimension(int_to_expression(0),int_to_expression(1)),*dims);
                *offsets=CONS(EXPRESSION,bound,*offsets);
            }
            /* this is a range : the dimension is eupper-elower +1 and the offset is elower */
            else
            {
                expression elower = constraints_to_loop_bound(lower,phi,true,entity_intrinsic(DIVIDE_OPERATOR_NAME));
                expression eupper = constraints_to_loop_bound(upper,phi,false,entity_intrinsic(DIVIDE_OPERATOR_NAME));
                *dims=CONS(DIMENSION,
                        make_dimension(
                            int_to_expression(0),
                            make_op_exp(MINUS_OPERATOR_NAME,eupper,elower)
                            ),*dims);
                *offsets=CONS(EXPRESSION,copy_expression(elower),*offsets);
            }
        }
        else {
            pips_user_warning("failed to analyse region");
            return false;
        }
    }
    *dims=gen_nreverse(*dims);
    *offsets=gen_nreverse(*offsets);
    return true;
}

/** 
 * 
 * @return region from @p regions on entity @p e
 */
static region find_region_on_entity(entity e,list regions)
{
    FOREACH(REGION,r,regions)
        if(same_entity_p(e,reference_variable(region_any_reference(r)))) return r;
    return region_undefined;
}

/** 
 * @return list of expressions so that for all i, out(i) = global(i)+local(i)
 */
static list isolate_merge_offsets(list global, list local)
{
    list out = NIL;
    FOREACH(EXPRESSION,gexp,global)
    {
        expression lexp = EXPRESSION(CAR(local));
        out=CONS(EXPRESSION,make_op_exp(PLUS_OPERATOR_NAME,copy_expression(gexp),copy_expression(lexp)),out);
        POP(local);
    }
    return gen_nreverse(out);
}

/** 
 * @return a range suitable for iteration over all the elements of dimension @p d
 */
static range dimension_to_range(dimension d)
{
    return make_range(
            copy_expression(dimension_lower(d)),
            copy_expression(dimension_upper(d)),
            int_to_expression(1));
}

/** 
 * @return a list of @p nb new integer entities 
 */
static list isolate_generate_indices(size_t nb)
{
    list indices = NIL;
    while(nb--)
    {
        entity e = make_new_scalar_variable_with_prefix("i",get_current_module_entity(),make_basic_int(DEFAULT_INTEGER_TYPE_SIZE));
        AddEntityToCurrentModule(e);
        indices=CONS(ENTITY,e,indices);
    }
    return indices;
}

/** 
 * @return a list of expressions, one for each entity in @p indices
 */
static list isolate_indices_to_expressions(list indices)
{
    list expressions=NIL;
    FOREACH(ENTITY,e,indices)
        expressions=CONS(EXPRESSION,entity_to_expression(e),expressions);
    return gen_nreverse(expressions);
}

typedef enum {
    transfer_in,
    transfer_out
} isolate_transfer;
#define transfer_in_p(e) ( (e) == transfer_in )
#define transfer_out_p(e) ( (e) == transfer_out )

/** 
 * 
 * @return a statement holding the loops necessary to initialize @p new from @p old,
 * knowing the dimension of the isolated entity @p dimensions and its offsets @p offsets and the direction of the transfer @p t
 */
static statement isolate_make_array_transfer(entity old,entity new, list dimensions, list offsets,isolate_transfer t)
{
    /* first create the assignment : we need a list of indices */
    list index_entities = isolate_generate_indices(gen_length(dimensions));
    list index_expressions = isolate_indices_to_expressions(index_entities);
    list index_expressions_with_offset = 
        isolate_merge_offsets(index_expressions,offsets);


    statement body = make_assign_statement(
            reference_to_expression(
                make_reference(new,transfer_in_p(t)?index_expressions:index_expressions_with_offset)
                ),
            reference_to_expression(
                make_reference(old,transfer_in_p(t)?index_expressions_with_offset:index_expressions)
                )
            );

    FOREACH(DIMENSION,d,dimensions)
    {
        expression offset = EXPRESSION(CAR(offsets));
        entity index = ENTITY(CAR(index_entities));
        body = instruction_to_statement(
                make_instruction_loop(
                    make_loop(
                        index,
                        dimension_to_range(d),
                        body,
                        entity_empty_label(),
                        make_execution_sequential(),
                        NIL
                        )
                    )
                );
        POP(offsets);
        POP(index_entities);
    }
    /* add a nice comment */
    asprintf(&statement_comments(body),"/* transfer loop generated by PIPS from %s to %s */",entity_user_name(old),entity_user_name(new));
    return body;
}

/** 
 * isolate statement @p s from the outer memory, generating appropriate local array copy and copy code
 */
static void do_isolate_statement(statement s)
{
    list regions = load_cumulated_rw_effects_list(s);

    list read_regions = regions_read_regions(regions);
    list write_regions = regions_write_regions(regions);

    statement prelude=make_empty_block_statement(),postlude=make_empty_block_statement();
    set visited_entities = set_make(set_pointer);

    FOREACH(REGION,reg,regions)
    {
        reference r = region_any_reference(reg);
        entity e = reference_variable(r);
        /* check we have not already dealt with this variable */
        if(!set_belong_p(visited_entities,e))
        {
            set_add_element(visited_entities,visited_entities,e);
            /* get the associated read and write regions, used for copy-in and copy-out
             * later on, in and out regions may be used instead
             * */
            region read_region = find_region_on_entity(e,read_regions);
            region write_region = find_region_on_entity(e,write_regions);

            /* compute their convex hull : that's what we need to allocate
             * in that case, the read and write regions must be used
             * */
            region rw_region = 
                region_undefined_p(read_region)?write_region:
                region_undefined_p(write_region)?read_region:
                regions_must_convex_hull(read_region,write_region);

            /* based on the rw_region, we can allocate a new entity with proper dimensions
             */
            list offsets = NIL,dimensions=NIL;
            if(region_to_minimal_dimensions(rw_region,&dimensions,&offsets))
            {
                /* a scalar */
                if(ENDP(dimensions))
                {
                }
                /* an array */
                else
                {
                    /* create the new entity */
                    entity new = make_new_array_variable_with_prefix(entity_local_name(e),get_current_module_entity(),copy_basic(entity_basic(e)),dimensions);
                    AddLocalEntityToDeclarations(new,get_current_module_entity(),s);

                    /* replace it everywhere, and patch references*/
                    isolate_patch_entities(s,e,new,offsets);

                    /* generate the copy - in from read region */
                    if(!region_undefined_p(read_region))
                    {
                        list read_dimensions=NIL,read_offsets=NIL;
                        if(region_to_minimal_dimensions(read_region,&read_dimensions,&read_offsets))
                        {
                            insert_statement(prelude,isolate_make_array_transfer(e,new,read_dimensions,read_offsets,transfer_in),true);
                        }
                        else
                        {
                            pips_user_warning("failed to recover information from read region\n");
                            return false;
                        }
                    }
                    /* and the copy-out from write region */
                    if(!region_undefined_p(write_region))
                    {
                        list write_dimensions=NIL,write_offsets=NIL;
                        if(region_to_minimal_dimensions(write_region,&write_dimensions,&write_offsets))
                        {
                            insert_statement(postlude,isolate_make_array_transfer(new,e,write_dimensions,write_offsets,transfer_out),false);
                        }
                        else
                        {
                            pips_user_warning("failed to recover information from write region\n");
                            return false;
                        }
                    }
                }

            }
            else
            {
                pips_user_warning("failed to convert regions to minimal array dimensions, using whole array instead\n");
                return false;
            }

        }

    }
    insert_statement(s,prelude,true);
    insert_statement(s,postlude,false);




    set_free(visited_entities);
    gen_free_list(read_regions);
    gen_free_list(write_regions);
}

bool
isolate_statement(string module_name)
{
    set_current_module_entity(module_name_to_entity( module_name ));
    set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, true) );
    set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_REGIONS, module_name, true));

    string stmt_label=get_string_property("ISOLATE_STATEMENT_LABEL");
    entity stmt_label_entity = find_label_entity(module_name,stmt_label);
    if(entity_undefined_p(stmt_label_entity))
        pips_user_error("label %s not found\n", stmt_label);
    list statements_to_isolate = find_statements_with_label(get_current_module_statement(),stmt_label_entity);
    statement statement_to_isolate = STATEMENT(CAR(statements_to_isolate));
    gen_free_list(statements_to_isolate);
    do_isolate_statement(statement_to_isolate);



    /* validate */
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,get_current_module_statement());

    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();
    return true;
}

/**
 * terapixify
 */

static
bool cannot_terapixify(gen_chunk * elem, bool *can_terapixify)
{
    printf("found invalid construct of type %td\n",elem->i);
    return *can_terapixify=false;
}

static
bool can_terapixify_call_p(call c, bool *can_terapixify)
{
    if( !value_intrinsic_p(entity_initial(call_function((c)))) && ! call_constant_p(c) )
    {
        printf("found invalid call to %s\n",entity_user_name(call_function(c)));
        return *can_terapixify=false;
    }
    return true;
}

static
bool can_terapixify_expression_p(expression e, bool *can_terapixify)
{
    basic b = expression_basic(e);
    while( basic_pointer_p(b))
        b = variable_basic(type_variable(ultimate_type(basic_pointer(b))));

    if(!basic_int_p(b) && ! basic_overloaded_p(b))
    {
      list ewords = words_expression(e,NIL);
        string estring = words_to_string(ewords);
        string bstring = basic_to_string(b);
        printf("found invalid expression %s of basic %s\n",estring, bstring);
        free(bstring);
        free(estring);
        gen_free_list(ewords);
        return *can_terapixify=false;
    }
    return true;
}

struct entity_bool { entity e; bool b; };

static
void entity_used_in_reference_walker(reference r, struct entity_bool *eb)
{
    if(same_entity_p(reference_variable(r),eb->e)) eb->b=true;
}

static
void entity_used_in_loop_bound_walker(loop l, struct entity_bool *eb)
{
    gen_context_recurse(loop_range(l),eb,reference_domain,gen_true,entity_used_in_reference_walker);
}

static
bool  entity_used_in_loop_bound_p(entity e)
{
    struct entity_bool eb = { e, false };
    gen_context_recurse(get_current_module_statement(),&eb,loop_domain,gen_true,entity_used_in_loop_bound_walker);
    return eb.b;
}

#define TERAPIX_PTRARG_PREFIX "FIFO"
#define TERAPIX_LOOPARG_PREFIX "N"
#define TERAPIX_IMAGE_PREFIX "im"
#define TERAPIX_MASK_PREFIX "ma"
#define TERAPIX_REGISTER_PREFIX "re"

static bool terapix_renamed_p(string s, string prefix)
{
    string found = strstr(s,prefix);
    if(found)
    {
        for(found+=strlen(prefix)+1;*found;++found)
            if(!isdigit(*found)) return false;
        return true;
    }
    return false;
}

static
entity terapix_argument_handler(entity e, string arg_prefix, size_t *arg_cnt,string ass_prefix, size_t *ass_cnt)
{
    /* change parameter name and generate an assignment */
    if(arg_prefix && !terapix_renamed_p(entity_user_name(e),arg_prefix) ) {
        string new_name;
        asprintf(&new_name,"%s" MODULE_SEP_STRING  "%s%u",entity_module_name(e),arg_prefix,(*arg_cnt)++);
        entity ne = make_entity_copy_with_new_name(e,new_name,false);
        free(new_name);

        for(list iter = code_declarations(value_code(entity_initial(get_current_module_entity())));
                !ENDP(iter);
                POP(iter))
        {
            entity ee = ENTITY(CAR(iter));
            if(same_entity_p(e,ee)) {
                CAR(iter).p=(gen_chunkp)ne;
            }
        }
#if 1
        /* we now have FIFOx in ne and will generate an assignment from ne to e 
         * we also have to change the storage for e ...*/
        free_storage(entity_storage(e)); entity_storage(e) = storage_undefined;
        AddEntityToCurrentModule(e);
        statement ass = make_assign_statement(entity_to_expression(e),entity_to_expression(ne));
        insert_statement(get_current_module_statement(),ass,true);
#endif
        return ne;
    }

    /* to respect terapix asm, we also have to change the name of variable e */
    if(ass_prefix && !terapix_renamed_p(entity_user_name(e),ass_prefix)) {
        string new_name;
        asprintf(&new_name,"%s" MODULE_SEP_STRING "%s%u",entity_module_name(e),ass_prefix,(*ass_cnt)++);
        entity ne = make_entity_copy_with_new_name(e,new_name,false);
        AddEntityToCurrentModule(ne);
        free(new_name);
        replace_entity(get_current_module_statement(),e,ne);
        return ne;
    }
}

static
bool terapix_suitable_loop_bound_walker(reference r,bool *suitable)
{
    return (*suitable) &= formal_parameter_p(reference_variable(r));
}

static
bool terapix_suitable_loop_bound_p(expression exp)
{
    bool suitable=true;
    gen_context_recurse(exp,&suitable,reference_domain,terapix_suitable_loop_bound_walker,gen_null);
    return suitable;
}

typedef struct {
    hash_table ht;
    size_t *cnt;
} terapix_loop_handler_param;

static void
terapix_loop_handler(statement sl,terapix_loop_handler_param *p)
{
    if(statement_loop_p(sl)){
        loop l = statement_loop(sl);
        range r = loop_range(l);
        expression nb_iter = range_to_expression(r,range_to_nbiter);
        entity loop_bound = entity_undefined;
        if(terapix_suitable_loop_bound_p(nb_iter))
        {
            /* generate new entity if needed */
            if(expression_reference_p(nb_iter)) /* use the reference , but we must rename it however !*/
            {
                loop_bound=reference_variable(expression_reference(nb_iter));
                string new_name;
                asprintf(&new_name,"%s" MODULE_SEP_STRING TERAPIX_LOOPARG_PREFIX "%u",get_current_module_name(),(*p->cnt)++);
                entity new_loop_bound=make_entity_copy_with_new_name(loop_bound,new_name,false);
                for(list iter = code_declarations(value_code(entity_initial(get_current_module_entity())));
                        !ENDP(iter);
                        POP(iter))
                {
                    entity ee = ENTITY(CAR(iter));
                    if(same_entity_p(loop_bound,ee)) {
                        CAR(iter).p=(gen_chunkp)new_loop_bound;
                    }
                }
                replace_entity(get_current_module_statement(),loop_bound,new_loop_bound);
                loop_bound=new_loop_bound;
            }
            else {
                string new_name;
                asprintf(&new_name,TERAPIX_LOOPARG_PREFIX "%u",(*p->cnt)++);
                loop_bound=make_scalar_integer_entity(new_name,get_current_module_name());
                value v = entity_initial(loop_bound);
                free_constant(value_constant(v));
                value_tag(v)=is_value_expression;
                value_expression(v)=nb_iter;
                AddEntityToCurrentModule(loop_bound);
                free(new_name);
            }

            /* patch loop */
            free_expression(range_lower(loop_range(l)));
            range_lower(loop_range(l))=make_expression_1();
            free_expression(range_upper(loop_range(l)));
            range_upper(loop_range(l))=entity_to_expression(loop_bound);

            /* convert the loop to a while loop */
            list statements = make_statement_list(
                    copy_statement(loop_body(l)),
                    make_assign_statement(entity_to_expression(loop_index(l)),make_op_exp(PLUS_OPERATOR_NAME,entity_to_expression(loop_index(l)),make_expression_1()))
            );
            whileloop wl = make_whileloop(
                    MakeBinaryCall(entity_intrinsic(LESS_OR_EQUAL_OPERATOR_NAME),entity_to_expression(loop_index(l)),entity_to_expression(loop_bound)),
                    make_block_statement(statements),
                    entity_empty_label(),
                    make_evaluation_before());
            sequence seq = make_sequence(
                    make_statement_list(
                        make_assign_statement(entity_to_expression(loop_index(l)),make_expression_1()),
                        make_stmt_of_instr(make_instruction_whileloop(wl))
                        )
                    );

            update_statement_instruction(sl,make_instruction_sequence(seq));

            /* save change for futher processing */
            hash_put(p->ht,loop_bound,nb_iter);
        }
    }
}

bool normalize_microcode( char * module_name)
{
    bool can_terapixify =true;
    /* prelude */
    set_current_module_entity(module_name_to_entity( module_name ));
    set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, TRUE) );
    set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS,module_name,TRUE));

    /* checks */

    /* make sure
     * - only do loops remain
     * - no call to external functions
     * - no float / double etc
     */
    gen_context_multi_recurse(get_current_module_statement(),&can_terapixify,
            whileloop_domain,cannot_terapixify,gen_null,
            forloop_domain,cannot_terapixify,gen_null,
            call_domain,can_terapixify_call_p,gen_null,
            expression_domain,can_terapixify_expression_p,gen_null,
            NULL);

    /* now, try to guess the goal of the parameters
     * - parameters are 16 bits signed integers (TODO)
     * - read-only arrays might be mask, but can also be images (depend of their size ?)
     * - written arrays must be images
     * - integer are loop parameters
     * - others are not allowded
     */
    size_t nb_fifo = 0;
    size_t nb_lu = 0;
    size_t nb_ptr = 0;
    size_t nb_re = 0;
    FOREACH(ENTITY,e,code_declarations(value_code(entity_initial(get_current_module_entity()))))
    {
        if(!entity_area_p(e))
        {
            if(formal_parameter_p(e))
            {
                variable v = type_variable(entity_type(e));
                if( basic_pointer_p(variable_basic(v)) ) /* it's a pointer */
                {
                    string prefix = NULL;
                    bool parameter_written = find_write_effect_on_entity(get_current_module_statement(),e);
                    if( parameter_written ) /* it's an image */
                    {
                        printf("%s seems an image\n",entity_user_name(e));
                        prefix = TERAPIX_IMAGE_PREFIX;
                        terapix_argument_handler(e,TERAPIX_PTRARG_PREFIX,&nb_fifo,prefix,&nb_ptr);
                    }
                    else /* cannot tell if it's a kernel or an image*/
                    {
                        int array_size=1;
                        FOREACH(DIMENSION,d,variable_dimensions(v))
                        {
                            int d_size;
                            expression sod = SizeOfDimension(d);
                            if(expression_integer_value(sod,&d_size)) {
                                array_size*=d_size;
                            }
                            else {
                                array_size=-1;
                                break;
                            }
                        }
                        if( array_size > 0 && 56 >= array_size ) {
                            printf("%s seems a mask\n",entity_user_name(e));
                            prefix = TERAPIX_MASK_PREFIX;
                        }
                        else {
                            printf("%s seems an image\n",entity_user_name(e));
                            prefix = TERAPIX_IMAGE_PREFIX;
                        }
                        terapix_argument_handler(e,TERAPIX_PTRARG_PREFIX,&nb_fifo,prefix,&nb_ptr);
                    }
                }
                else if( entity_used_in_loop_bound_p(e) )
                {
                    printf("%s belongs to a loop bound\n",entity_user_name(e));
                    //terapix_argument_handler(e,TERAPIX_LOOPARG_PREFIX,&nb_lu,NULL,NULL);
                }
                else {
                    printf("parameter %s is not valid\n",entity_user_name(e));
                    can_terapixify=false;
                }
            }
            else {
                terapix_argument_handler(e,NULL,NULL,TERAPIX_REGISTER_PREFIX,&nb_re);
            }
        }
    }

    /* rename all declared entities using terasm convention*/
    FOREACH(ENTITY,e,statement_declarations(get_current_module_statement()))
    {
        if(entity_variable_p(e))
        {
            variable v = type_variable(entity_type(e));
            if( basic_pointer_p(variable_basic(v)) ) /* it's a pointer */
                terapix_argument_handler(e,NULL,NULL,TERAPIX_IMAGE_PREFIX,&nb_ptr);
            else if( basic_int_p(variable_basic(v))) /* it's an int */
                terapix_argument_handler(e,NULL,NULL,TERAPIX_REGISTER_PREFIX,&nb_re);
        }
    }

    /* loops in terasm iterate over a given parameter, in the form DO I=1:N 
     * I is hidden to the user and N must be a parameter */
    {
        terapix_loop_handler_param p = {
            .ht = hash_table_make(hash_pointer,HASH_DEFAULT_SIZE),
            .cnt=&nb_lu
        };
        gen_context_recurse(get_current_module_statement(),&p,statement_domain,gen_true,terapix_loop_handler);
    }
    

    /* validate */
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,get_current_module_statement());

    /*postlude*/
    reset_current_module_entity();
    reset_current_module_statement();
    reset_cumulated_rw_effects();
    return true || can_terapixify;
}

/**
 * have a look to the pipsmake-rc description
 * basically call kernelize then outlining
 *
 * @param module_name name of the module
 *
 * @return true
 */
bool terapixify(__attribute__((unused)) char * module_name)
{
    return true; /* everything is done in pipsmake-rc */
}

typedef enum {
    NO_CONVERSION,
    ARRAY_1D,
    POINTER
}array_to_pointer_conversion_mode ;

static
array_to_pointer_conversion_mode get_array_to_pointer_conversion_mode()
{
    string mode = get_string_property("ARRAY_TO_POINTER_CONVERT_PARAMETERS");
    if(!mode || empty_string_p(mode)) return NO_CONVERSION;
    else if(same_string_p(mode,"1D")) return ARRAY_1D;
    else if(same_string_p(mode,"POINTER")) return POINTER;
    else pips_user_error("bad value %s for property ARRAY_TO_POINTER_CONVERT_PARAMETERS\n",mode);
}


/**
 * transform each subscript in expression @a exp into the equivalent pointer arithmetic expression
 *
 * @param exp expression to inspect
 *
 * @return true
 */
static
bool expression_array_to_pointer(expression exp, bool in_init)
{
    if(expression_reference_p(exp))
    {
        reference ref = expression_reference(exp);
        if( ! ENDP(reference_indices(ref) ) )
        {
            /* we need to check if we know the dimension of this reference */
            size_t nb_indices =gen_length(reference_indices(ref));
            size_t nb_dims =gen_length(variable_dimensions(type_variable(entity_type(reference_variable(ref))))) ;

            /* if the considered reference is a formal parameter and the property is properly set,
             * we are allowded to convert formal parameters such as int a[n][12] into int *a
             */
            bool force_cast = true;
            if( get_array_to_pointer_conversion_mode()==POINTER && 
                    ! get_bool_property("ARRAY_TO_POINTER_FLATTEN_ONLY") &&
                    formal_parameter_p(reference_variable(ref)) )
            {
                force_cast=false;
            }

            /* create a new reference without subscripts */
            reference ref_without_indices = make_reference(reference_variable(ref),NIL);

            expression base_ref = reference_to_expression(ref_without_indices);
            expression address_computation = EXPRESSION(CAR(reference_indices(ref)));
            /* create a pointer if needed */
            if( !get_bool_property("ARRAY_TO_POINTER_FLATTEN_ONLY"))
            {
                /* get the base type of the reference */
                type type_without_indices = make_type_variable(make_variable(
                            copy_basic(variable_basic(type_variable(entity_type(reference_variable(ref))))),
                            NIL,
                            gen_full_copy_list(variable_qualifiers(type_variable(entity_type(reference_variable(ref)))))));


                /* create an expression for the new reference, possibly casted */
                if( force_cast && ! basic_pointer_p( variable_basic(type_variable(entity_type(reference_variable(ref) ) ) ) ) )
                {
                    base_ref = make_expression(
                            make_syntax_cast(
                                make_cast(
                                    make_type_variable(
                                        make_variable(
                                            make_basic_pointer(type_without_indices),NIL,NIL
                                            )
                                        ),
                                    base_ref)
                                ),
                            normalized_undefined);
                }
            }

            /* iterate on the dimensions & indices to create the index expression */
            list dims = variable_dimensions(type_variable(entity_type(reference_variable(ref))));
            list indices = reference_indices(ref);
            POP(indices);
            if(!ENDP(dims)) POP(dims); // the first dimension is unused
            FOREACH(DIMENSION,dim,dims)
            {
                expression dimension_size = make_op_exp(
                        PLUS_OPERATOR_NAME,
                        make_op_exp(
                            MINUS_OPERATOR_NAME,
                            copy_expression(dimension_upper(dim)),
                            copy_expression(dimension_lower(dim))
                            ),
                        make_expression_1());

                if( !ENDP(indices) ) { /* there may be more dimensions than indices */
                    expression index_expression = EXPRESSION(CAR(indices));
                    address_computation = make_op_exp(
                            PLUS_OPERATOR_NAME,
                            index_expression,
                            make_op_exp(
                                MULTIPLY_OPERATOR_NAME,
                                dimension_size,address_computation
                                )
                            );
                    POP(indices);
                }
                else {
                    address_computation = make_op_exp(
                            MULTIPLY_OPERATOR_NAME,
                            dimension_size,address_computation
                            );
                }
            }

            /* there may be more indices than dimensions */
            FOREACH(EXPRESSION,e,indices)
            {
                address_computation = make_op_exp(
                        PLUS_OPERATOR_NAME,
                        address_computation,e
                        );
            }

            /* we now either add the DEREFERENCING_OPERATOR, or the [] */
            syntax new_syntax = syntax_undefined;
            if(!in_init && get_bool_property("ARRAY_TO_POINTER_FLATTEN_ONLY")) {
                reference_indices(ref_without_indices)=make_expression_list(address_computation);
                new_syntax=make_syntax_reference(ref_without_indices);
            }
            else {
                if(nb_indices == nb_dims || nb_dims == 0 ) {

                    new_syntax=make_syntax_call(
                            make_call(
                                CreateIntrinsic(DEREFERENCING_OPERATOR_NAME),
                                CONS(EXPRESSION,MakeBinaryCall(
                                        entity_intrinsic(PLUS_C_OPERATOR_NAME),
                                        base_ref,
                                        address_computation), NIL)
                                )
                            );
                }
                else {
                    new_syntax = make_syntax_call(
                            make_call(CreateIntrinsic(PLUS_C_OPERATOR_NAME),
                                make_expression_list(base_ref,address_computation))
                            );
                }
            }

            /* free stuffs */
            unnormalize_expression(exp);
            gen_free_list(reference_indices(ref));
            reference_indices(ref)=NIL;
            free_syntax(expression_syntax(exp));

            /* validate changes */
            expression_syntax(exp)=new_syntax;
        }

    }
    /* not tested */
    else if( syntax_subscript_p(expression_syntax(exp) ) )
    {
        subscript s = syntax_subscript(expression_syntax(exp));
        pips_assert("non empty subscript",!ENDP(subscript_indices(s)));
        call c = make_call(
                CreateIntrinsic(PLUS_C_OPERATOR_NAME),
                make_expression_list(
                    copy_expression(subscript_array(s)),
                    EXPRESSION(CAR(subscript_indices(s)))
                    ));
        list indices = subscript_indices(s);
        POP(indices);
        FOREACH(EXPRESSION,e,indices)
        {
            c = make_call(
                    CreateIntrinsic(PLUS_OPERATOR_NAME),
                    make_expression_list(call_to_expression(c),e));
        }
        unnormalize_expression(exp);
        gen_free_list(subscript_indices(s));
        subscript_indices(s)=NIL;
        free_syntax(expression_syntax(exp));
        expression_syntax(exp)=make_syntax_call(c);


    }
    return true;
}

/**
 * call expression_array_to_pointer on each entity declared in statement @s
 *
 * @param s statement to inspect
 *
 * @return true
 */
static
bool declaration_array_to_pointer(statement s,bool __attribute__((unused)) in_init)
{
    FOREACH(ENTITY,e,statement_declarations(s))
        gen_context_recurse(entity_initial(e),(void*)true,expression_domain,expression_array_to_pointer,gen_null);
    return true;
}

static
void make_pointer_from_variable(variable param)
{
    list parameter_dimensions = variable_dimensions(param);
    switch(get_array_to_pointer_conversion_mode())
    {
        case NO_CONVERSION:return;
        case ARRAY_1D:
            if(gen_length(parameter_dimensions) > 1)
            {
                list iter = parameter_dimensions;
                expression full_length = expression_undefined;
                {
                    dimension d = DIMENSION(CAR(iter));
                    full_length = SizeOfDimension(d);
                    POP(iter);
                }
                FOREACH(DIMENSION,d,iter)
                {
                    full_length=make_op_exp(
                            MULTIPLY_OPERATOR_NAME,
                            SizeOfDimension(d),
                            full_length);
                }
                gen_full_free_list(parameter_dimensions);
                variable_dimensions(param)=
                    CONS(DIMENSION,
                            make_dimension(
                                int_to_expression(0),
                                make_op_exp(MINUS_OPERATOR_NAME,full_length,int_to_expression(1))
                                ),
                            NIL);
            } break;
        case POINTER:
            if(!ENDP(parameter_dimensions))
            {
                gen_full_free_list(parameter_dimensions);
                variable_dimensions(param)=NIL;
                basic parameter_basic = variable_basic(param);
                basic new_parameter_basic = make_basic_pointer(
                        make_type_variable(
                            make_variable(parameter_basic,NIL,NIL)
                            )
                        );
                variable_basic(param)=new_parameter_basic;
            } break;

    }
}
static
void make_pointer_entity_from_reference_entity(entity e)
{
    variable param = type_variable(entity_type(e));
    make_pointer_from_variable(param);
}

static void
reduce_array_declaration_dimension(statement s)
{
    FOREACH(ENTITY,e,statement_declarations(s))
    {
        type t = ultimate_type(entity_type(e));
        if(type_variable_p(t))
        {
            variable v = type_variable(t);
            if(!ENDP(variable_dimensions(v)))
            {
                expression new_dim = expression_undefined;
                FOREACH(DIMENSION,d,variable_dimensions(v))
                {
                    new_dim = expression_undefined_p(new_dim)?
                        SizeOfDimension(d):
                        make_op_exp(MULTIPLY_OPERATOR_NAME,
                                new_dim,
                                SizeOfDimension(d)
                                );
                    print_expression(new_dim);
                }
                gen_full_free_list(variable_dimensions(v));
                variable_dimensions(v)=CONS(DIMENSION,make_dimension(make_expression_0(),make_op_exp(MINUS_OPERATOR_NAME,new_dim,make_expression_1())),NIL);
            }
        }
    }
}

bool array_to_pointer(char *module_name)
{
    /* prelude */
    set_current_module_entity(module_name_to_entity( module_name ));
    set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, TRUE) );

    /* run transformation */
    if(!c_module_p(get_current_module_entity()))
        pips_user_warning("this transformation will have no effect on a fortran module\n");
    else
    {
        gen_context_multi_recurse(get_current_module_statement(),(void*)false,
                expression_domain,expression_array_to_pointer,gen_null,
                statement_domain,declaration_array_to_pointer,gen_null,
                NULL);
        /* now fix array declarations : one dimension for every one ! */
        gen_recurse(get_current_module_statement(),statement_domain,gen_true,reduce_array_declaration_dimension);

        /* eventually change the signature of the module
         * tricky : signature must be changed in two places !
         */
        {
            FOREACH(ENTITY,e,code_declarations(value_code(entity_initial(get_current_module_entity()))))
            {
                if(formal_parameter_p(e))
                    make_pointer_entity_from_reference_entity(e);
            }
            FOREACH(PARAMETER,p,functional_parameters(type_functional(entity_type(get_current_module_entity()))))
            {
                dummy d = parameter_dummy(p);
                if(dummy_identifier_p(d))
                    make_pointer_entity_from_reference_entity(dummy_identifier(d));
                type t = parameter_type(p);
                make_pointer_from_variable(type_variable(t));
            }
        }

    }

    /* validate */
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,get_current_module_statement());

    /*postlude*/
    reset_current_module_entity();
    reset_current_module_statement();
    return true;
}

static
void two_addresses_code_generator(statement s)
{
    if(statement_call_p(s))
    {
        call c = statement_call(s);
        if(ENTITY_ASSIGN_P(call_function(c)))
        {
            list args = call_arguments(c);
            expression lhs = EXPRESSION(CAR(args));
            expression rhs = EXPRESSION(CAR(CDR(args)));
            if(expression_reference_p(lhs) && expression_call_p(rhs) && !expression_constant_p(rhs)) {
                call parent_call = call_undefined;
                do {
                    parent_call=expression_call(rhs);
                    rhs=EXPRESSION(CAR(call_arguments(parent_call)));
                } while(expression_call_p(rhs) && !expression_constant_p(rhs));
                if(! expression_equal_p(lhs,rhs) )
                {
                    /* a=b+c; -> (1) a=b; (2) a=a+c; */
                    statement theassign/*1*/= make_assign_statement(copy_expression(lhs),copy_expression(rhs));
                    statement thecall/*2*/= s;
                    CAR(call_arguments(parent_call)).p=(gen_chunkp)copy_expression(lhs);
                    insert_statement(thecall,theassign,true);
                }
            }
        }
    }
}

bool
generate_two_addresses_code(char *module_name)
{
    /* prelude */
    set_current_module_entity(module_name_to_entity( module_name ));
    set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, TRUE) );

    gen_recurse(get_current_module_statement(),statement_domain,gen_true,two_addresses_code_generator);

    /* validate */
    module_reorder(get_current_module_statement());
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,get_current_module_statement());

    /*postlude*/
    reset_current_module_entity();
    reset_current_module_statement();
    return true;
}

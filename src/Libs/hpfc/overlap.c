/*
 * Overlap Management Module for HPFC
 * Fabien Coelho, August 1993
 *
 * $RCSfile: overlap.c,v $ ($Date: 1995/03/13 15:37:07 $, )
 * version $Revision$
 * got on %D%, %T%
 * $Id$
 */

/*
 * included files, from C libraries, newgen and pips libraries.
 */

#include <stdio.h>
#include <string.h>
extern int fprintf();

#include "genC.h"

#include "ri.h"
#include "hpf.h"
#include "message.h"

#include "misc.h"
#include "ri-util.h"
#include "loop_normalize.h"
#include "hpfc.h"
#include "defines-local.h"

/*
 * local defines
 */

#define entry_defined_p(ent) \
    (GET_ENTITY_MAPPING(overlaps, ent) != HASH_UNDEFINED_VALUE)

/*
 * the overlap management is based on an hash table
 */

static entity_mapping
    overlaps;

/*
 * static void get_int_hash_table(ent)
 *
 * if not defined, create a new integer hash table.
 */
static hash_table get_int_hash_table(ent)
entity ent;
{
    if (!entry_defined_p(ent))
    {
	SET_ENTITY_MAPPING(overlaps, ent, hash_table_make(hash_int, 0));
    }
    
    return((hash_table) GET_ENTITY_MAPPING(overlaps, ent));
}

/*
 * init_overlap_management()
 *
 * initialize the overlap management
 */
void init_overlap_management()
{
    overlaps = MAKE_ENTITY_MAPPING();
    hash_dont_warn_on_redefinition();
}

/*
 * void close_overlap_management()
 *
 * free everything.
 */
void close_overlap_management()
{
    ENTITY_MAPPING_MAP(key, int_map, 
		   {
		       /* just to avoid a gcc warning, this debug line:-) */
		       debug(9, "close_overlap_management", "freeing for key %d\n", key);
		       hash_table_free((hash_table) int_map);
		   },
		       overlaps);

    FREE_ENTITY_MAPPING(overlaps);
}

/*
 * set_overlap(ent, dim, side, width)
 *
 * set the overlap value for entity ent, on dimension dim,
 * dans side side to width, which must be a positive integer.
 * if necessary, the overlap is updates with the value width.
 */
void set_overlap(ent, dim, side, width)
entity ent;
int dim, side, width;
{
    hash_table 
	hi = get_int_hash_table(ent);
    int
	key = 2*dim+side,
	i = (int) hash_get(hi, (char *)key);

    if ((i == (int) HASH_UNDEFINED_VALUE) || (i < width))
	hash_put(hi, (char *)key, (char *)width);
}

/*
 * int get_overlap(ent, dim, side)
 *
 * returns the overlap for a given entity, dimension and side,
 * to be used in the declaration modifications
 */
int get_overlap(ent, dim, side)
entity ent;
int dim, side;
{
    hash_table 
	hi = get_int_hash_table(ent);
    int
	key = 2*dim+side,
	i = (int) hash_get(hi, (char *)key);

    if (i == (int) HASH_UNDEFINED_VALUE)
	return(0);
    else
	return(i);
}

/*
 * static void overlap_redefine_expression(pexpr, ov)
 *
 * redefine the bound given the overlap which is to be included
 */
static void overlap_redefine_expression(pexpr, ov)
expression *pexpr;
int ov;
{
    expression
	copy = *pexpr;

    if (expression_constant_p(*pexpr))
    {
	*pexpr = int_to_expression(HpfcExpressionToInt(*pexpr)+ov);
	free_expression(copy); /* this avoid a memory leak */
    }
    else
	*pexpr = MakeBinaryCall(FindOrCreateEntity(TOP_LEVEL_MODULE_NAME, 
						   PLUS_OPERATOR_NAME),
				*pexpr,
				int_to_expression(ov));
}

/*
 * void declarations_with_overlaps()
 *
 *
 */
void declaration_with_overlaps(l)
list l;
{
    MAPL(ce,
     {
	 entity 
	     oldent = ENTITY(CAR(ce));
	 entity
	     ent = load_entity_node_new(oldent);
	 int 
	     ndim = variable_entity_dimension(ent);
	 int 
	     i;

	 assert(type_variable_p(entity_type(ent)));

	 for (i=1 ; i<=ndim ; i++)
	 {
	     dimension
		 the_dim = entity_ith_dimension(ent, i);

	     int lower_overlap = get_overlap(oldent, i, 0);
	     int upper_overlap = get_overlap(oldent, i, 1);

	     debug(8, "declaration_with_overlaps", 
		   "%s(DIM=%d): -%d, +%d\n", 
		   entity_name(ent), i, lower_overlap, upper_overlap);

	     if (lower_overlap!=0) 
	     {
		 overlap_redefine_expression(&dimension_lower(the_dim),
					     -lower_overlap);
	     }
		 
	     if (upper_overlap!=0) 
	     {
		 overlap_redefine_expression(&dimension_upper(the_dim),
					     upper_overlap);
	     }
	 }
     },
	 l);
}

void declaration_with_overlaps_for_module(module)
entity  module;
{
    list
	l = list_of_distributed_arrays_for_module(module);

    declaration_with_overlaps(l);
    gen_free_list(l);
}

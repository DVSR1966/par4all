/*

  $Id$

  Copyright 1989-2009 MINES ParisTech

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
/* package convex effects :  Be'atrice Creusillet 6/97
 *
 * $Id$
 *
 * File: compose.c
 * ~~~~~~~~~~~~~~~
 *
 * This File contains the intanciation of the generic functions necessary 
 * for the composition of convex effects with transformers or preconditions
 *
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "misc.h"

#include "effects-generic.h"
#include "effects-convex.h"

list
convex_regions_transformer_compose(list l_reg, transformer trans)
{
    project_regions_with_transformer_inverse(l_reg, trans, NIL);
    return l_reg;
}

list
convex_regions_inverse_transformer_compose(list l_reg, transformer trans)
{
    project_regions_with_transformer(l_reg, trans, NIL);
    return l_reg;
}

list 
convex_regions_precondition_compose(list l_reg, transformer context)
{
    list l_res = NIL;
    Psysteme sc_context = predicate_system(transformer_relation(context));
    
    ifdebug(8)
	{
	    pips_debug(8, "context: \n");
	    sc_syst_debug(sc_context);
	}
	
    MAP(EFFECT, reg,
	{
	  /* FI: this leads to problems when the context might become
	     later empty: there won't be any way to find out; in one
	     case I do not remember, the IN effects end up wrong;
	     however, adding descriptors for all scalar references may
	     slow down the region computation a lot. */
	    if (! effect_scalar_p(reg) )
		region_sc_append(reg, sc_context, FALSE);

	    if (!region_empty_p(reg))
		l_res = CONS(EFFECT, reg, l_res);    
	},
	l_reg);

    return l_res;
}

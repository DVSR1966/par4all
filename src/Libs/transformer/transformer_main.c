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
 /* main for transformer package
  *
  * Tests link-editing, no more
  *
  * Francois Irigoin, 21 April 1990
  */

#include "genC.h"
#include "linear.h"
#include "ri.h"

#include "transformer.h"

main()
{
    transformer t1 = transformer_undefined;
    transformer t2 = transformer_undefined;
    transformer t3;
    
    t3 = transformer_convex_hull(t1, t2);
    (void) print_transformer(t3);
}

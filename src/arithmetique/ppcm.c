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

/* package arithmetique
 */

/*LINTLIBRARY*/

#include <stdio.h>

#include "arithmetique.h"
#include "assert.h"

/* int ppcm(int i, int j): plus petit entier positif divisible par i et j
 *
 * Ancien nom et ancien type: void lcm(int i, int j, int *pk)
 */
Value ppcm(Value i, Value j)
{
    if (value_neg_p(i)) i = value_uminus(i);
    if (value_neg_p(j)) j = value_uminus(j);

    if (value_zero_p(i) || value_zero_p(j)) 
	return VALUE_ZERO;
    else {
	Value d = pgcd(i,j);
	d = value_div(i,d);
	return value_mult(d,j);
    }
}

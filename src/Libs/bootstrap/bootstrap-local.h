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
/* 
 * $Id$
 */

/* should be some properties to accomodate cray codes?? */
#define INT_LENGTH 4
#define REAL_LENGTH 4
#define DOUBLE_LENGTH 8
#define COMPLEX_LENGTH 8
#define DCOMPLEX_LENGTH 16

/* context for type checking. */
typedef struct 
{
    hash_table types;
    stack stats;
    int number_of_error;
    int number_of_conversion;
    int number_of_simplication;
} type_context_t, * type_context_p;

typedef basic (*typing_function_t)(call, type_context_p);

typedef void (*switch_name_function)(expression, type_context_p);

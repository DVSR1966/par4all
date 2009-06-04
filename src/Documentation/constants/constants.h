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
/* this file is maintained under Documentation/constants.h
 */

/* the following use to be "constants.h" alone in Include.
 * I put it there not to lose it someday. FC.
 */

#include "specs.h"

/* And now, a nice set of (minor) memory leak sources...
 */
 
/* Auxiliary data files
 */
#define PIPS_ETC(file) \
    (strdup(concatenate(getenv("PIPS_ROOT"), "/etc/", (file), NULL)))

#define PIPSMAKE_RC "pipsmake.rc"
#define DEFAULT_PIPSMAKE_RC PIPS_ETC(PIPSMAKE_RC)

#define WPIPS_RC PIPS_ETC("wpips.rc")

/* #define BOOTSTRAP_FILE PIPS_ETC("BOOT-STRAP.entities") */

#define XV_HELP_FILE PIPS_ETC("pips_help.txt")
 
#define PROPERTIES_FILE "properties.rc"
#define PROPERTIES_LIB_FILE PIPS_ETC(PROPERTIES_FILE)
 
#define MODEL_RC "model.rc"
#define DEFAULT_MODEL_RC PIPS_ETC(MODEL_RC)
 
/* filename extensions
 */
#define SEQUENTIAL_CODE_EXT ".code"
#define PARALLEL_CODE_EXT ".parcode"
 
#define SEQUENTIAL_FORTRAN_EXT ".f"
#define SEQUENTIAL_C_EXT ".c"
#define PARALLEL_FORTRAN_EXT ".parf"
#define PARALLEL_C_EXT ".parc"
#define PREDICAT_FORTRAN_EXT ".pref"
#define PRETTYPRINT_FORTRAN_EXT ".ppf"
 
#define WP65_BANK_EXT ".bank"
#define WP65_COMPUTE_EXT ".wp65"
 
#define ENTITIES_EXT ".entities"
 
#define EMACS_FILE_EXT "-emacs"
 
#define GRAPH_FILE_EXT "-graph"


/* Some directory names... */

/* Where is the output of HPFC in the workspace: */
#define HPFC_COMPILED_FILE_DIR "hpfc"
   
/* say that's all
 */

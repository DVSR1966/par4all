/* Top-level declares a extern jmp_buf pips_top_level :
 */
#include <setjmp.h>

/* the following use to be "constants.h" alone in Include.
 * I put it there not to lose it someday. FC.
 */

#include "specs.h"

/* And now, a nice set of (minor) memory leak sources...
 */
 
/* Auxiliary data files
 */
 
#define PIPSMAKE_RC "pipsmake.rc"
#define DEFAULT_PIPSMAKE_RC \
  (strdup(concatenate(getenv("PIPS_LIBDIR"), "/", PIPSMAKE_RC, NULL)))
#define WPIPS_RC \
  (strdup(concatenate(getenv("PIPS_LIBDIR"), "/", "wpips.rc", NULL)))
#define BOOTSTRAP_FILE \
  (strdup(concatenate(getenv("PIPS_LIBDIR"), "/", "BOOT-STRAP.entities", NULL)))
#define XV_HELP_FILE \
  (strdup(concatenate(getenv("PIPS_LIBDIR"), "/", "pips_help.txt", NULL)))
 
#define PROPERTIES_FILE "properties.rc"
#define PROPERTIES_LIB_FILE \
  (strdup(concatenate(getenv("PIPS_LIBDIR"), "/", PROPERTIES_FILE, NULL)))
 
#define MODEL_RC "model.rc"
#define DEFAULT_MODEL_RC \
  (strdup(concatenate(getenv("PIPS_LIBDIR"), "/", MODEL_RC, NULL)))
 
/* filename extensions
 */
#define SEQUENTIAL_CODE_EXT ".code"
#define PARALLEL_CODE_EXT ".parcode"
 
#define SEQUENTIAL_FORTRAN_EXT ".f"
#define PARALLEL_FORTRAN_EXT ".parf"
#define PREDICAT_FORTRAN_EXT ".pref"
#define PRETTYPRINT_FORTRAN_EXT ".ppf"
 
#define WP65_BANK_EXT ".bank"
#define WP65_COMPUTE_EXT ".wp65"
 
#define ENTITIES_EXT ".entities"
 
#define EMACS_FILE_EXT "-emacs"
 
#define GRAPH_FILE_EXT "-graph"

/* say that's all
 */

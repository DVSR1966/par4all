 /* Header File for WP65
  *
  * Francois Irigoin, Corinne Ancourt, Lei Zhou
  *
  * September 1991
  */

#ifndef WP65_INCLUDED
#define WP65_INCLUDED

#define EMULATED_SHARED_MEMORY_PREFIX  "ES_"
#define LOCAL_MEMORY_PREFIX            "L_"
#define LOCAL_MEMORY_SEPARATOR         "_"
#define COMPUTE_ENGINE_NAME            "WP65"
/* #define WP65_SUFFIX                    "_WP65" */
#define BANK_NAME                      "BANK"
/* #define MEMORY_EMULATOR_SUFFIX         "_BANK" */
#define PROCESSOR_IDENTIFIER           "PROC_ID"
#define BANK_IDENTIFIER                "BANK_ID"
#define BANK_LINE_IDENTIFIER           "L"
#define BANK_OFFSET_IDENTIFIER         "O"

#define SUFFIX_FOR_INDEX_TILE_IN_INITIAL_BASIS     "_1"
#define SUFFIX_FOR_INDEX_TILE_IN_TILE_BASIS        "_0"
#define PREFIX_FOR_LOCAL_TILE_BASIS                "L_"


/* These values should be read in model.rc (FI, 20/11/91)
   #define PROCESSOR_NUMBER 4
   #define BANK_NUMBER      4
   #define LINE_SIZE        25    
*/

/* FI: PIPELINE_DEPTH should be set to 3 */
#define PIPELINE_DEPTH    1

#define MAXIMUM_LOCAL_VARIABLE_NAME_SIZE 256

#endif /* WP65_INCLUDED */

/*
   #define ATOMIZER_MODULE_NAME "ATOMIZE"
   */
#define TMP_ENT 1
#define AUX_ENT 2
#define DOUBLE_PRECISION_SIZE 8

/* During the computation, the program has to deal with blocks of statements.
 * We define a new structure in order to have a simple control over all
 * the manipulations of the blocks of statements.
 *
 * With this new structure we know the current statement being translated
 * and the list of statements (ie the block) in which we put the new
 * statements created.
 */
typedef struct {
     list last;
     list first;
     bool stmt_generated;
    } Block;
/* The list "first" is a truncated list from the first to the current
 * statement (not included).
 * The list "last" is a truncated list from the current statement (included)
 * to the last.
 * The union of "first" and "last" is equal to the entire block.
 *
 * The boolean "stmt_generated" says if the current statement has:
 *       _ TRUE : already generated statements.
 *       _ FALSE : not generated statements.
 *
 * Thus, when the current statement generates a new statement it is put at the
 * end of the list "first" (just before the current statement).
 * The current statement gives its caracteristics to the new one if the bool
 * "stmt_generated" is FALSE; this allows to keep these caracteristics
 * at the first statement of the list generated by the translation of the
 * current statement.
 * The caracteristics of a statement are its "label", "number", "ordering" and
 * "comments" (cf. RI).
 */
 
/* This global variable is used for the modification of the control graph,
 * see commentaries of atomizer_of_unstructured() in atomizer.c.
 */
extern list l_inst;

/* These lists memorize all the new created entities of each type. They
 * are used for the declarations of these new variables : temporaries
 * and auxiliaries.
 */
extern list integer_entities, real_entities, complex_entities, logical_entities,
	    double_entities, char_entities;

/* Mappings for the cumulated effects of statements. */
extern statement_mapping cumulated_effects_map;

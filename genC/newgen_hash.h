/*

	-- NewGen Project

	The NewGen software has been designed by Remi Triolet and Pierre
	Jouvelot (Ecole des Mines de Paris). This prototype implementation
	has been written by Pierre Jouvelot.

	This software is provided as is, and no guarantee whatsoever is
	provided regarding its appropriate behavior. Any request or comment
	should be sent to newgen@isatis.ensmp.fr.

	(C) Copyright Ecole des Mines de Paris, 1989

*/

/* $RCSfile: newgen_hash.h,v $ ($Date: 2003/07/04 10:54:20 $, )
 * version $Revision$
 * got on %D%, %T%
 */

#ifndef newgen_hash_included
#define newgen_hash_included

#define HASH_DEFAULT_SIZE 7

typedef enum hash_key_type { 
    hash_string, hash_int, hash_pointer, hash_chunk } hash_key_type;


/* Define hash_table structure which is hided.
 * The only thing we know about it is that the entries are in an array
 * pointed to by hash_table_array(htp), it contains hash_table_size(htp)
 * elements. These elements can be read with hash_entry_val(...) or
 * hash_entry_key(...). If the key is HASH_ENTRY_FREE or
 * HASH_ENTRY_FREE_FOR_PUT then the slot is empty. 
 */

struct __hash_table;
typedef struct __hash_table *hash_table;

/* Value of an undefined hash_table 
 */

#define hash_table_undefined ((hash_table)gen_chunk_undefined)  
#define hash_table_undefined_p(h) ((h)==hash_table_undefined)

/* value returned by hash_get() when the key is not found; could also be
   called HASH_KEY_NOT_FOUND, but it's semantically a value; this bottom
   value will be user-definable in a future release of NewGen */

#define HASH_UNDEFINED_VALUE ((void *) gen_chunk_undefined)

#define hash_table_empty_p(htp) (hash_table_entry_count(htp) == 0)

#define HASH_MAP(k,v,code,h) \
    {\
    hash_table _map_hash_h = (h) ; \
    register void * _map_hash_p = NULL; \
    void *k, *v; \
    while ((_map_hash_p = hash_table_scan(_map_hash_h,_map_hash_p,&k,&v))) { \
            code ; }}

/* Let's define a new version of
 * hash_put_or_update() using the warn_on_redefinition 
 */

#define hash_put_or_update(h, k, v)			\
if (hash_warn_on_redefinition_p() == TRUE)		\
{							\
    hash_dont_warn_on_redefinition();			\
    hash_put((hash_table)h, (void*)k, (void*)v);	\
    hash_warn_on_redefinition();			\
} else							\
    hash_put((hash_table)h, (void*)k, (void*)v);

/* functions declared in hash.c 
 */
extern void hash_warn_on_redefinition GEN_PROTO((void));
extern void hash_dont_warn_on_redefinition GEN_PROTO((void));
extern void * hash_delget GEN_PROTO((hash_table, void *, void **));
extern void * hash_del GEN_PROTO((hash_table, void *));
extern void * hash_get GEN_PROTO((hash_table, void *));
extern bool hash_defined_p GEN_PROTO((hash_table, void *));
extern void hash_put GEN_PROTO((hash_table, void *, void *));
extern void hash_table_clear GEN_PROTO((hash_table));
extern void hash_table_free GEN_PROTO((hash_table));
extern hash_table hash_table_make GEN_PROTO((hash_key_type, int));
extern void hash_table_print_header GEN_PROTO((hash_table, FILE *));
extern void hash_table_print GEN_PROTO((hash_table));
extern void hash_table_fprintf GEN_PROTO((FILE *, char *(*)(), 
					  char *(*)(), hash_table));
extern void hash_update GEN_PROTO((hash_table, void *, void*));
extern bool hash_warn_on_redefinition_p GEN_PROTO((void));

extern int hash_table_entry_count GEN_PROTO((hash_table));
extern int hash_table_size GEN_PROTO((hash_table));
extern int hash_table_own_allocated_memory GEN_PROTO((hash_table));
extern hash_key_type hash_table_type GEN_PROTO((hash_table));
extern void * hash_table_scan GEN_PROTO((hash_table,
					 void *,
					 void **,
					 void **));

/* map stuff */
extern void * hash_map_get GEN_PROTO((hash_table, void *));
extern void hash_map_put GEN_PROTO((hash_table, void *, void *));
extern void hash_map_update GEN_PROTO((hash_table, void *, void *));
extern void * hash_map_del GEN_PROTO((hash_table, void *));
extern bool hash_map_defined_p GEN_PROTO((hash_table, void *));

#endif /* newgen_hash_included */

/*  that is all
 */

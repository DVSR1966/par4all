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

/*  SCCS Stuff
 *  $RCSfile: genC.h,v $ ($Date: 1995/12/14 17:59:36 $, )
 *  version $Revision$
 *  got on %D%, %T%
 */


#ifndef GENC_INCLUDED
#define GENC_INCLUDED
#define NEWGEN

/*
 * This is the include file to be used for the generation of C code.
 */
#include <sys/stdtypes.h>
#include <stdio.h>
#include "malloc.h"                    /* for debug with malloclib */
#include "newgen_assert.h"

#include "newgen_types.h"
#include "newgen_set.h"

/* The size of the management information inside each Newgen object (in
 *  gen_chunks) 
 */

#define GEN_HEADER (1)
#define GEN_HEADER_SIZE (sizeof(gen_chunk)*GEN_HEADER)

/* A gen_chunk is used to store every object. It has to be able to store,
 * at least, a (CHUNK *) and every inlinable value. To use a union is a
 * trick to enable the assignment opereator and the ability of passing and
 * returning them as values (for function): this requires a sufficiently
 * clever compiler !  * Note that the field name of inlinable types have
 * to begin with the same letter as the type itself (this can be fixed if
 * necessary but why bother).  This characteristic is used by the Newgen
 * code generator.
 */

typedef union gen_chunk {
	unit u ;
	bool b ;
	char c ;
	int i ;
	float f ;
	string s ;
	struct cons *l ;
	set t ;
	hash_table h ;
	union gen_chunk *p ;
} gen_chunk, *gen_chunkp, chunk /* obsolete */;

#define gen_chunk_undefined ((gen_chunk *)(-16))
#define gen_chunk_undefined_p(c) ((c)==gen_chunk_undefined)

/* obsolete
 */
#define chunk_undefined gen_chunk_undefined 
#define chunk_undefined_p(c) gen_chunk_undefined_p(c)

#define UNIT(x) "You don't want to take the value of a unit type, do you!"
#define BOOL(x) ((x).b)
#define CHAR(x) ((x).c)
#define INT(x) ((x).i)
#define FLOAT(x) ((x).f)
#define STRING(x) ((x).s)
#define CONSP(x) ((x).l)
#define SETP(x) ((x).t)
#define CHUNK(x) ((x).p)

/* added on 02/10/95, FC 
 */
#define HASH(x) ((x).h)
#define CHUNKP(x) ((x).p)
#define LIST(x) ((x).l) 
#define SET(c) ((x).t)

/* for the MAP macro to handle simple types correctly. FC.
 */
#define UNIT_TYPE "You don't want a unit type, do you!"
#define BOOL_TYPE bool
#define CHAR_TYPE char
#define INT_TYPE int
#define FLOAT_TYPE float
#define STRING_TYPE string
#define CONSP_TYPE list
#define LIST_TYPE list
#define SETP_TYPE set
#define SET_TYPE set
#define CHUNK_TYPE gen_chunkp
#define CHUNKP_TYPE gen_chunkp
#define HASH_TYPE hash_table

#include "newgen_list.h"
#include "newgen_stack.h"

/* The implementation of tabulated domains */

extern gen_chunk *Gen_tabulated_[] ;
extern struct gen_binding Domains[], *Tabulated_bp ;


#define TABULATED_MAP(_x,_code,_dom) \
	{int _tabulated_map_i=0 ; \
	 gen_chunk *_tabulated_map_t = Gen_tabulated_[Domains[_dom].index] ; \
         gen_chunk *_x ; \
	 for(;_tabulated_map_i<max_tabulated_elements();_tabulated_map_i++) {\
		if( (_x=(_tabulated_map_t+_tabulated_map_i)->p) != \
		     gen_chunk_undefined ) \
			_code ;}}

/* The root of the gen_chunk read with READ_CHUNK. */

extern gen_chunk *Read_chunk ;

/* Function interface for user applications. */

extern int gen_debug ;

#define GEN_DBG_TRAV_LEAF 1
#define GEN_DBG_TRAV_SIMPLE 2
#define GEN_DBG_TRAV_OBJECT 4
#define GEN_DBG_CHECK 8
#define GEN_DBG_RECURSE 16

#define GEN_DBG_TRAV \
     (GEN_DBG_TRAV_LEAF|GEN_DBG_TRAV_SIMPLE|GEN_DBG_TRAV_OBJECT)

extern void gen_free GEN_PROTO(( gen_chunk * )) ;
extern int gen_free_tabulated GEN_PROTO(( int )) ;
extern void gen_write GEN_PROTO(( FILE *, gen_chunk * )) ;
extern int gen_write_tabulated GEN_PROTO(( FILE *, int )) ;
extern void gen_read_spec GEN_PROTO((char *, ...)) ;
extern gen_chunk *gen_read GEN_PROTO(( FILE * )) ;
extern int gen_read_tabulated GEN_PROTO(( FILE *, int )) ;
extern int gen_read_and_check_tabulated GEN_PROTO(( FILE *, int )) ;
extern gen_chunk *gen_make_array GEN_PROTO(( int )) ;
extern gen_chunk *gen_alloc GEN_PROTO((int, int, int, ...)) ; 
extern void gen_init_external GEN_PROTO((int, 
					 char *(*)(), void (*)(), 
					 void (*)(), char *(*)(), 
					 int (*)() )) ;
extern gen_chunk *gen_check GEN_PROTO(( gen_chunk *, int )) ;
extern int gen_type GEN_PROTO((gen_chunk *)) ;
extern char *gen_domain_name GEN_PROTO((int)) ;
extern void gen_clear_tabulated_element GEN_PROTO(( gen_chunk * )) ;
extern gen_chunk *gen_copy_tree GEN_PROTO(( gen_chunk * )) ;
extern int gen_consistent_p GEN_PROTO(( gen_chunk * )) ;
extern char *alloc GEN_PROTO ((int )) ;
extern int gen_allocated_memory GEN_PROTO((gen_chunk*));

/*  recursion and utilities
 */
extern bool gen_true GEN_PROTO((gen_chunk *)) ;
extern bool gen_false GEN_PROTO((gen_chunk *)) ;
extern void gen_null GEN_PROTO((gen_chunk *)) ;

extern void gen_recurse_stop GEN_PROTO((gen_chunk *));
extern void gen_multi_recurse GEN_PROTO((gen_chunk *, ...));
extern void gen_recurse GEN_PROTO((gen_chunk *,
				   int, 
				   bool (*)( gen_chunk * ), 
				   void (*)( gen_chunk * ))) ;

/* Since C is not-orthogonal (chunk1 == chunk2 is prohibited),
 * this one is needed.
 */

#ifndef MEMORY_INCLUDED
#include <memory.h>
#define MEMORY_INCLUDED
#endif

#define gen_equal(lhs,rhs) (memcmp((lhs),(rhs))==0)

/* GEN_CHECK can be used to test run-time coherence of Newgen values.
 */

#ifdef GEN_CHECK
#undef GEN_CHECK
#define GEN_CHECK(e,t) (gen_check((e),(t)),e)
#define GEN_CHECK_ALLOC 1
#else
#define GEN_CHECK(e,t) (e)
#define GEN_CHECK_ALLOC 0
#endif

#include "newgen_map.h"

#include "newgen_generic_mapping.h"
#include "newgen_generic_stack.h"
#include "newgen_generic_function.h"

#endif

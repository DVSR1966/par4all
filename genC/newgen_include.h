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

#include "newgen_types.h"
#include "newgen_set.h"

/* Types of domain
 */
#define UNDEF_DT 0
#define EXTERNAL_DT 1
#define BASIS_DT 2
#define LIST_DT 3
#define ARRAY_DT 4
#define CONSTRUCTED_DT 5
#define IMPORT_DT 6
#define SET_DT 7

/* Operators for CONSTRUCTED domains
 */
#define UNDEF_OP 0
#define AND_OP 1
#define OR_OP 2
#define ARROW_OP 3

/* The UNIT_TYPE_NAME is the used to type expressions which only perform 
   side-effects. It is mainly used to build sets. */

#define UNIT_TYPE_NAME "unit"

/* A list type constructor */

#define DEFLIST(type,cartype,carname)\
  struct type {\
    cartype carname ;\
    struct type *cdr ;\
  }

DEFLIST(namelist,char *,name) ;
DEFLIST(intlist,int,val) ;
DEFLIST(domainlist,union domain *,domain) ;

/* actually defined in tabulated.c */
typedef struct _gtp * gen_tabulated_p;

union domain ;
struct inlinable ;
struct gen_binding {
  char * name;
  int compiled;
  int size; /* number of chunks to hold this data. */
  gen_tabulated_p tabulated;
  union domain *domain ;
  struct inlinable *inlined ;
} ;

/* The implementation of tabulated domains */
extern struct gen_binding Domains[], *Tabulated_bp ;

/* The root of the gen_chunk read with READ_CHUNK. */
extern gen_chunk *Read_chunk ;

struct gen_binding ;

/* A DOMAIN union describes the structure of a user type. 

   The (STRUCT BINDING *) are used either to point to strings (during
   the parsing of the specifications), or to gen_bindings (after compilation).

   TYPE and CONSTRUCTOR members have to be at the same offset */

/* FC, 10/06/1994, set_type in se moved.
 *
 * common part assumed for all: type
 * common part assumed for ba, li, se, ar: 
 *      type, constructor, persistant and one gen_binding
 *
 */
union domain {
  struct { /* EXTERNAL */
    int type ;
    char *(*read)() ;
    void (*write)() ;
    void (*free)() ;
    char *(*copy)() ;
    int (*allocated_memory)();
  } ex ;
  struct { /* BASIC */
    int type ;
    char *constructor ;
    int persistant ;
    struct gen_binding *constructand ;
  } ba ;
  struct { /* LIST */
    int type ;
    char *constructor ;
    int persistant ;
    struct gen_binding *element ;
  } li ;
  struct { /* SET */
    int type ;
    char *constructor ;
    int persistant ;
    struct gen_binding *element ;
    set_type what ;
  } se ;
  struct { /* ARRAY */
    int type;
    char *constructor;
    int persistant;
    struct gen_binding * element;
    struct intlist * dimensions;
  } ar ;
  struct { /* CONSTRUCTED */
    int type ;
    int op ;
    int first ;
    struct domainlist *components ;
  } co ;
  struct { /* IMPORT (another newgen type, not yet defined) */
    int type ;
    char *filename ;
  } im ;
} ;

/* MAX_DOMAIN is the maximum number of entries in the DOMAINS table */
#define MAX_DOMAIN 250

/* hack to walk thru tabulateds... */
extern struct gen_binding *Tabulated_bp ;

/* Different kinds of BINDING structure pointers */

#define IS_INLINABLE(bp) ((bp)->inlined != NULL)
#define IS_EXTERNAL(bp) ((bp)->domain->ba.type == EXTERNAL_DT)
#define IS_TABULATED(bp) ((bp)->tabulated!=NULL)
#define IS_IMPORT(bp) ((bp)->domain->ba.type == IMPORT_DT)
#define IS_NORMAL(bp) \
	(!IS_INLINABLE(bp)&&!IS_EXTERNAL(bp)&&!IS_TABULATED(bp))

/* Domains is the symbol table for user defined types. COMPILED refers to
   inlined data types. INDEX is used whenever the type is used with
   refs and defs (ALLOC is the next place to allocate tabulated values). */

extern struct gen_binding Domains[] ;

/* Action parameter to the LOOKUP() function in the symbol table. Are 
   looking for a new symbol or an old one ? */

#define NEW_BINDING 1
#define OLD_BINDING 2

extern struct gen_binding *lookup() ;
extern struct gen_binding *new_binding() ;

/* Used to check, while parsing specs, that a constructed domain use 
   only one operator type. */

extern int Current_op ;

/* To manage imported domains:

   - NUMBER_IMPORTS is the number of imported domains (for which no
     access functions have to be generated - see TYPE in genC.c)
   - CURRENT_START is the beginning index (after TABULATED_BP) in Domains
   - CURRENT_FIRST **WAS**, for each OR_OP domain, the first tag number (used
     only in READ_SPEC_MODE). It is now 0. 
*/

extern int Number_imports ;
extern int Current_start ;
/* extern int Current_first ; */
extern int Read_spec_mode ;

/* For tabulated objects, the offset HASH_OFFSET of the hashed subdomain */
#define HASH_OFFSET 2

#define same_string_p(s1, s2) (strcmp((s1), (s2)) == 0)

#define IS_UNIT_TYPE(n)		((n)==0)
#define IS_BOOL_TYPE(n)		((n)==1)
#define IS_CHAR_TYPE(n)		((n)==2)
#define IS_INT_TYPE(n)		((n)==3)
#define IS_FLOAT_TYPE(n)	((n)==4)
#define IS_STRING_TYPE(n)	((n)==5)

/*
 * MACROS added for code checking
 *
 * Fabien COELHO 10/06/94
 */
#define check_domain(dom) \
  message_assert("Consistant domain number",\
		 (dom)>=0 && (dom)<MAX_DOMAIN)

#define check_read_spec_performed() \
  message_assert("gen_read_spec not performed prior to use", \
		 Read_spec_performed);

#define NEWGEN_FREED ((void*)NULL)

#define newgen_free(p) (*((char*)(p))='\0',free(p)) /* just to hide bugs */

/* inlined version of domain_index. what is done by optimizing compilers?
 */
#define quick_domain_index(obj) \
  (((! (obj)) || ((obj)==gen_chunk_undefined) || \
    ((obj)->i<0) || ((obj)->i>MAX_DOMAIN)) ? \
   newgen_domain_index(obj) : (obj)->i) \
  /* prints the error message or returns */

/* External routines. */
extern void user(char *, ...) ;

extern void gencode(string);
extern void fatal(char *, ...) ;
extern char *itoa(int);
extern void print_domains(FILE *);
extern void init(void) ;
extern void compile(void) ;
extern int gen_size(int);
extern void print_domain(FILE *, union domain *);

extern int genspec_parse(void);
extern int genspec_lex(void);
extern int genread_parse(void);
extern int genread_lex(void);

extern gen_tabulated_p gen_init_tabulated(int);
extern gen_chunk * gen_enter_tabulated(int, char *, gen_chunk *, bool);
extern gen_chunk * gen_do_enter_tabulated(int, char *, gen_chunk *, bool);
extern gen_chunk * gen_tabulated_fake_object_hack(int);

extern int newgen_domain_index(gen_chunk *);

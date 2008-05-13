/*

	-- NewGen Project

	The NewGen software has been designed by Remi Triolet and Pierre
	Jouvelot (Ecole des Mines de Paris). This prototype implementation
	has been written by Pierre Jouvelot.

	This software is provided as is, and no guarantee whatsoever is
	provided regarding its appropriate behavior. Any request or comment
	should be sent to newgen@cri.ensmp.fr.

	(C) Copyright Ecole des Mines de Paris, 1989

*/

/* $Id$
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

#include "newgen_types.h"
#include "genC.h"
#include "newgen_include.h"
#include "newgen_hash.h"

/* Some predefined values for the key 
 */

#define HASH_ENTRY_FREE ((void *) 0)
#define HASH_ENTRY_FREE_FOR_PUT ((void *) -1)

typedef struct {
  void * key;
  void * val;
} hash_entry;

struct __hash_table {
  hash_key_type type;               /* the type of keys... */
  size_t size;                         /* size of actual array */
  size_t n_entry;                      /* number of associations stored */
  uintptr_t (*rank)(void*, uintptr_t); /* how to compute rank for key */
  int (*equals)(void*, void*);      /* how to compare keys */
  hash_entry *array;                /* actual array */
  size_t limit;                        /* max entries before reallocation */

  /* keep statistics on the life time of the hash table... FC 04/06/2003 */
  uintptr_t n_put, n_get, n_del, n_upd;
  uintptr_t n_put_iter, n_get_iter, n_del_iter, n_upd_iter;
};

/* Constant to find the end of the prime numbers table 
 */
#define END_OF_SIZE_TABLE (0)

/* Hash function to get the index of the array from the key 
 */
/* Does not work :
   #if sizeof(uintptr_t) == 8
   so go into less portable way: */
#if __WORDSIZE == 64
#define RANK(key, size) ((((uintptr_t)(key)) ^ 0xC001BabeFab1C0e1LLU)%(size))
#else
/* Fabien began with this joke... :-) */
#define RANK(key, size) ((((uintptr_t)(key)) ^ 0xfab1c0e1U)%(size))
#endif

/* Set of the different operations 
 */
typedef enum { hash_get_op , hash_put_op , hash_del_op } hash_operation;

/* Private functions 
 */
static void hash_enlarge_table(hash_table htp);
static hash_entry * hash_find_entry(hash_table htp, 
				    void * key, 
				    uintptr_t *prank, 
				    hash_operation operation,
				    uintptr_t * stats);
static string hash_print_key(hash_key_type, void*);

static int hash_int_equal(int, int);
static uintptr_t hash_int_rank(void*, size_t);
static int hash_pointer_equal(void*, void*);
static uintptr_t hash_pointer_rank(void*, size_t);
static int hash_string_equal(char*, char*);
static uintptr_t hash_string_rank(void*, size_t);
static int hash_chunk_equal(gen_chunk*, gen_chunk*) ;
static uintptr_t hash_chunk_rank(gen_chunk*, size_t);

/* list of the prime numbers from 17 to 2^31-1 
 * used as allocated size
 */
static size_t prime_list[] = {
    7,
    17,
    37,
    71,
    137,
    277,
    547,
    1091,
    2179,
    4357,
    8707,
    17417,
    34819,
    69653,
    139267,
    278543,
    557057,
    1114117,
    2228243,
    4456451,
    8912921,
    17825803,
    35651593,
    71303171,
    142606357,
    285212677,
    570425377,
    1140850699,
    END_OF_SIZE_TABLE
};

/* returns the maximum number of things to hold in a table
 */
static size_t hash_size_limit(size_t current_size)
{
  /* 50.0% : ((size)>>1)
   * 62.5% : (((size)>>1)+((size)>>3))
   * 75.0% : (((size)>>1)+((size)>>2))
   */
  return current_size>>1;
}

/* Now we need the table size to be a prime number.
 * So we need to retrieve the next prime number in a list.
 */
static size_t get_next_hash_table_size(size_t size)
{
  size_t * p_prime = prime_list;
  while (*p_prime <= size) {
    message_assert("size too big ", *p_prime != END_OF_SIZE_TABLE);
    p_prime++;
  }
  return *p_prime;
}

/* internal variable to know we should warm or not
 */
static bool should_i_warn_on_redefinition = TRUE;

/* these function set the variable should_i_warn_on_redefinition
   to the value TRUE or FALSE */

void hash_warn_on_redefinition(void)
{
    should_i_warn_on_redefinition = TRUE;
}

void hash_dont_warn_on_redefinition(void)
{
    should_i_warn_on_redefinition = FALSE;
}

bool hash_warn_on_redefinition_p(void)
{
    return should_i_warn_on_redefinition;
}

/* this function makes a hash table of size size. if size is less or
   equal to zero a default size is used. the type of keys is given by
   key_type (see hash.txt for further details). */

hash_table hash_table_make(hash_key_type key_type, size_t size)
{
    register size_t i;
    hash_table htp;

    if (size<HASH_DEFAULT_SIZE) size=HASH_DEFAULT_SIZE - 1;
    /* get the next prime number in the table */
    size = get_next_hash_table_size(size);

    htp = (hash_table) alloc(sizeof(struct __hash_table));
    htp->type = key_type;
    htp->size = size;
    htp->n_entry = 0;
    htp->limit = hash_size_limit(size);
    htp->array = (hash_entry*) alloc(size*sizeof(hash_entry));

    /* initialize statistics */
    htp->n_put = 0;
    htp->n_get = 0;
    htp->n_del = 0;
    htp->n_upd = 0;

    htp->n_put_iter = 0;
    htp->n_get_iter = 0;
    htp->n_del_iter = 0;
    htp->n_upd_iter = 0;

    for (i = 0; i < size; i++) 
	htp->array[i].key = HASH_ENTRY_FREE;
    
    switch(key_type)
    {
    case hash_string:
	htp->equals = (int(*)(void*,void*)) hash_string_equal;
	htp->rank = hash_string_rank;
	break;
    case hash_int:
	htp->equals = (int(*)(void*,void*)) hash_int_equal;
	htp->rank = hash_int_rank;
	break;
    case hash_chunk:
	htp->equals = (int(*)(void*,void*)) hash_chunk_equal;
	htp->rank = (uintptr_t (*)(void*, uintptr_t)) hash_chunk_rank;
	break;
    case hash_pointer:
	htp->equals = hash_pointer_equal;
	htp->rank = hash_pointer_rank;
	break;
    default:
	fprintf(stderr, "[make_hash_table] bad type %d\n", key_type);
	abort();
    }

    return htp;
}

static size_t max_size_seen = 0;

/* Clears all entries of a hash table HTP. [pj] */
void hash_table_clear(hash_table htp)
{
  register hash_entry * p, * end ;

  if (htp->size > max_size_seen) {
    max_size_seen = htp->size;
#ifdef DBG_HASH
    fprintf(stderr, "[hash_table_clear] maximum size is %d\n", 
	    max_size_seen);
#endif
  }
  
  end = htp->array + htp->size ;
  htp->n_entry = 0 ;
  
  for ( p = htp->array ; p < end ; p++ ) {
    p->key = HASH_ENTRY_FREE ;
  }
}

/* this function deletes a hash table that is no longer useful. unused
 memory is freed. */

void hash_table_free(hash_table htp)
{
  gen_free_area((void**) htp->array, htp->size*sizeof(hash_entry));
  gen_free_area((void**) htp, sizeof(struct __hash_table));
}

/* This functions stores a couple (key,val) in the hash table pointed to
   by htp. If a couple with the same key was already stored in the table
   and if hash_warn_on_redefintion was requested, hash_put complains but
   replace the old value by the new one. This is a potential source for a
   memory leak. If the value to store is HASH_UNDEFINED_VALUE or if the key
   is HASH_ENTRY_FREE or HASH_ENTRY_FREE_FOR_INPUT, hash_put
   aborts. The restrictions on the key should be avoided by changing
   the implementation. The undefined value should be
   user-definable. It might be argued that users should be free to
   assign HASH_UNDEFINED_VALUE, but they can always perform hash_del()
   to get the same result */

void hash_put(hash_table htp, void * key, void * val)
{
  uintptr_t rank;
  hash_entry * hep;
    
  if (htp->n_entry+1 >= (htp->limit)) 
    hash_enlarge_table(htp);
  
  message_assert("illegal input key", key!=HASH_ENTRY_FREE &&
		 key!=HASH_ENTRY_FREE_FOR_PUT);
  message_assert("illegal input value", val!=HASH_UNDEFINED_VALUE);
  
  htp->n_put++;
  hep = hash_find_entry(htp, key, &rank, hash_put_op, &htp->n_put_iter);
  
  if (hep->key != HASH_ENTRY_FREE && hep->key != HASH_ENTRY_FREE_FOR_PUT) {
    if (should_i_warn_on_redefinition && hep->val != val) {
      (void) fprintf(stderr, "[hash_put] key redefined: %s\n", 
		     hash_print_key(htp->type, key));
    }
    hep->val = val;
  }
  else {
    htp->n_entry += 1;
    hep->key = key;
    hep->val = val;
  }
}

/* deletes key from the hash table. returns the val and key
 */ 
void * 
hash_delget(
    hash_table htp, 
    void * key, 
    void ** pkey)
{
    hash_entry * hep;
    void *val;
    uintptr_t rank;
    
    message_assert("legal input key",
		   key!=HASH_ENTRY_FREE && key!=HASH_ENTRY_FREE_FOR_PUT);

    htp->n_del++;
    hep = hash_find_entry(htp, key, &rank, hash_del_op, &htp->n_del_iter);
    
    if (hep->key != HASH_ENTRY_FREE && hep->key != HASH_ENTRY_FREE_FOR_PUT) {
	val = hep->val;
	*pkey = hep->key;
	htp->array[rank].key = HASH_ENTRY_FREE_FOR_PUT;
	htp->n_entry -= 1;
	return val;
    }

    *pkey = 0;
    return HASH_UNDEFINED_VALUE;
}

/* this function removes from the hash table pointed to by htp the
   couple whose key is equal to key. nothing is done if no such couple
   exists. ??? should I abort ? (FC)
 */
void * hash_del(hash_table htp, void * key)
{
    void * tmp;
    return hash_delget(htp, key, &tmp);
}

/* this function retrieves in the hash table pointed to by htp the
   couple whose key is equal to key. the HASH_UNDEFINED_VALUE pointer is
   returned if no such couple exists. otherwise the corresponding value
   is returned. */ 

void * hash_get(hash_table htp, void * key)
{
  hash_entry * hep;
  uintptr_t n;
  
  message_assert("legal input key", key!=HASH_ENTRY_FREE &&
		 key!=HASH_ENTRY_FREE_FOR_PUT);
  
  if (!htp->n_entry) 
    return HASH_UNDEFINED_VALUE;
  
  /* else may be there */
  htp->n_get++;
  hep = hash_find_entry(htp, key, &n, hash_get_op, &htp->n_get_iter);
  
  return(hep->key!=HASH_ENTRY_FREE && hep->key!=HASH_ENTRY_FREE_FOR_PUT ? 
	 hep->val : HASH_UNDEFINED_VALUE);
}

/* TRUE if key has e value in htp.
 */
bool hash_defined_p(hash_table htp, void * key)
{
    return(hash_get(htp, key)!=HASH_UNDEFINED_VALUE);
}

/* update key->val in htp, that MUST be pre-existent.
 */
void hash_update(hash_table htp, void * key, void * val)
{
  hash_entry * hep;
  uintptr_t n;
  
  message_assert("illegal input key", key!=HASH_ENTRY_FREE &&
		 key!=HASH_ENTRY_FREE_FOR_PUT);
  htp->n_upd++;
  hep = hash_find_entry(htp, key, &n, hash_get_op, &htp->n_upd_iter);

  message_assert("no previous entry", htp->equals(hep->key, key));
  
  hep->val = val ;
}

/* this function prints the header of the hash_table pointed to by htp
 * on the opened stream fout 
 */
void hash_table_print_header(hash_table htp, FILE *fout)
{
  fprintf(fout, "hash_key_type:     %d\n", htp->type);
  fprintf(fout, "size:         %zd\n", htp->size);
  /* to be used by pips, we should not print this
     as it is only for debugging NewGen and it is not important data
     I (go) comment it.
     
     fprintf(fout, "limit    %d\n", htp->limit);
  */
  fprintf(fout, "n_entry: %zd\n", htp->n_entry);
}
 
/* this function prints the content of the hash_table pointed to by htp
on stderr. it is mostly useful when debugging programs. */

void hash_table_print(hash_table htp)
{
  size_t i;
  
  hash_table_print_header (htp,stderr);
  
  for (i = 0; i < htp->size; i++) {
    hash_entry he;
    
    he = htp->array[i];
    
    if (he.key != HASH_ENTRY_FREE && he.key != HASH_ENTRY_FREE_FOR_PUT) {
      fprintf(stderr, "%zd %s %p\n", 
	      i, hash_print_key(htp->type, he.key),
	      he.val);
    }
  }
}

/* This function prints the content of the hash_table pointed to by htp
on file descriptor f, using functions key_to_string and value_to string
to display the mapping. it is mostly useful when debugging programs. */

void hash_table_fprintf(
    FILE * f, 
    char *(*key_to_string)(void*),
    char *(*value_to_string)(void*), 
    hash_table htp)
{
    size_t i;

    hash_table_print_header (htp,f);

    for (i = 0; i < htp->size; i++) {
	hash_entry he;

	he = htp->array[i];

	if (he.key != HASH_ENTRY_FREE && he.key != HASH_ENTRY_FREE_FOR_PUT) {
	    fprintf(f, "%s -> %s\n", 
		    key_to_string(he.key), value_to_string(he.val));
	}
    }
}

/* function to enlarge the hash_table htp.
 * the new size will be first number in the array prime_numbers_for_table_size
 * that will be greater or equal to the actual size 
 */

static void 
hash_enlarge_table(hash_table htp)
{
  hash_entry * old_array;
  size_t i, old_size;
  
  old_size = htp->size;
  old_array = htp->array;
  
  htp->size++;
  /* Get the next prime number in the table */
  htp->size = get_next_hash_table_size(htp->size);
  htp->array = (hash_entry *) alloc(htp->size* sizeof(hash_entry));
  htp->limit = hash_size_limit(htp->size);
  
  for (i = 0; i < htp->size ; i++)
    htp->array[i].key = HASH_ENTRY_FREE;
  
  for (i = 0; i < old_size; i++) 
  {
    hash_entry he;
    he = old_array[i];
    
    if (he.key != HASH_ENTRY_FREE && he.key != HASH_ENTRY_FREE_FOR_PUT) {
      hash_entry * nhep;
      uintptr_t rank;
      
      htp->n_put++;
      nhep = hash_find_entry(htp, he.key, &rank, 
			     hash_put_op, &htp->n_put_iter);
      
      if (nhep->key != HASH_ENTRY_FREE) {
	fprintf(stderr, "[hash_enlarge_table] fatal error\n");
	abort();
      }    
      htp->array[rank] = he;
    }
  }
  gen_free_area((void**)old_array, old_size*sizeof(hash_entry));
}

/* en s'inspirant vaguement de 
 *   Fast Hashing of Variable-Length Text Strings
 *   Peter K. Pearson
 *   CACM vol 33, nb 6, June 1990
 * qui ne donne qu'une valeur entre 0 et 255
 *
 * unsigned int T[256] with random values
 * unsigned int h = 0;
 * for (char * s = (char*) key; *s; s++)
 *   h = ROTATION(...,h) ^ T[ (h^(*s)) % 256];
 * mais...
 */

static uintptr_t hash_string_rank(void * key, size_t size)
{
  uintptr_t v = 0;
  char * s;
  
  for (s = (char*) key; *s; s++)
    /* FC: */ v = ((v<<7) | (v>>25)) ^ *s;
    /* GO: v <<= 2, v += *s; */

  return v % size;
}

static uintptr_t hash_int_rank(void * key, size_t size)
{
  return RANK(key, size);
}

static uintptr_t hash_pointer_rank(void * key, size_t size)
{
  return RANK(key, size);
}

static uintptr_t hash_chunk_rank(gen_chunk * key, size_t size)
{
  return RANK(key->i, size);
}

static int hash_string_equal(char * key1, char * key2)
{
  if (key1==key2)
    return TRUE;
  /* else check contents */
  for(; *key1 && *key2; key1++, key2++)
    if (*key1!=*key2)
      return FALSE;
  return *key1==*key2;
}

static int hash_int_equal(int key1, int key2)
{
  return key1 == key2;
}

static int hash_pointer_equal(void * key1, void * key2)
{
  return key1 == key2;
}

static int hash_chunk_equal(gen_chunk * key1, gen_chunk * key2)
{
  return key1->p == key2->p;
}

static char * hash_print_key(hash_key_type t, void * key)
{
  static char buffer[32]; /* even 8 byte pointer => ~16 chars */

  if (t == hash_string)
    return (char*) key; /* hey, this is just what we need! */
  /* Use extensive C99 printf formats and stdint.h types to avoid
     truncation warnings: */
  else if (t == hash_int)
    sprintf(buffer, "%td", (intptr_t) key);
  else if (t == hash_pointer)
    sprintf(buffer, "%p", key);
  else if (t == hash_chunk)
    sprintf(buffer, "%zx", (uintptr_t) ((gen_chunk *)key)->p);	    
  else {
    fprintf(stderr, "[hash_print_key] bad type : %d\n", t);
    abort();
  }
  
  return buffer;
}


/* distinct primes for long cycle incremental search */
static int inc_prime_list[] = {
    2,   3,   5,  11,  13,  19,  23,  29,  31,  41, 
   43,  47,  53,  59,  61,  67,  73,  79,  83,  89,
   97, 101, 103, 107, 109, 113, 127, 131, 139, 149,
  151
};

#define HASH_INC_SIZE (31) /* 31... (yes, this one is prime;-) */


/*  buggy function, the hash table stuff should be made again from scratch.
 *  - FC 02/02/1995
 *  - So go on! :-) RK 17/01/2008
 */
static hash_entry * 
hash_find_entry(hash_table htp, 
		void * key, 
		uintptr_t *prank, 
		hash_operation operation,
		uintptr_t * stats)
{
  register uintptr_t
    r_init = (*(htp->rank))(key, htp->size),
    r = r_init,
    /* history of r_inc value
     * RT: 1
     * GO: 1 + abs(r_init)%(size-1)
     * FC: inc_prime_list[ RANK(r_init, HASH_INC_SIZE) ]
     * FC rationnal: if r_init is perfect, 1 is fine...
     *    if it is not perfect, let us randmize here some more...
     *    I'm not sure the result is any better than 1???
     *    It does seems to help significantly on some examples...
     */
    r_inc  = inc_prime_list[ RANK(r_init, HASH_INC_SIZE) ] ;
  hash_entry he;

  while (1) 
  {
    /* FC 05/06/2003
     * if r_init is randomized (i.e. perfect hash function)
     * and r_inc does not kill everything (could it?)
     * if p is the filled proportion for the table, 0<=p<1
     * we should have number_of_iterations 
     *        = \Sigma_{i=1}{\infinity} i*(1-p)p^{i-1}
     * this formula must simplify somehow... = 1/(1-p) ?
     * 0.20   => 1.25
     * 0.25   => 1.33..
     * 0.33.. => 1.50
     * 0.50   => 2.00
     * 0.66.. => 3.00
     * 0.70   => 3.33
     */
    if (stats) (*stats)++;
    
    he = htp->array[r];
    
    if (he.key == HASH_ENTRY_FREE)
      break;
    
    /*  ??? it may happen that the previous mapping is kept
     *  somewhere forward! So after a hash_del, the old value
     *  would be visible again!
     */
    if (he.key == HASH_ENTRY_FREE_FOR_PUT && operation == hash_put_op)
      break;
    
    if (he.key != HASH_ENTRY_FREE_FOR_PUT &&
	(*(htp->equals))(he.key, key))
      break;
    
    /* GO: it is not anymore the next slot, we skip some of them depending
     * on the reckonned increment 
     */
    r = (r + r_inc) % htp->size;
    
    /* FC: ??? this may happen in a hash_get after many put and del,
     * if the table contains no FREE, but many FREE_FOR_PUT instead!
     */
    if(r == r_init) {
      fprintf(stderr,"[hash_find_entry] cannot find entry\n") ;
      abort() ;
    }
  }
  
  *prank = r;
  return &(htp->array[r]);
}

/* now we define observers in order to
 * hide the hash_table type ... 
 */
int hash_table_entry_count(hash_table htp)
{
    return htp->n_entry;
}

int hash_table_size(hash_table htp)
{
    return htp->size;
}

hash_key_type hash_table_type(hash_table htp)
{
    return htp->type;
}

/*
 * This function allows a hash_table scanning
 * First you give a NULL hentryp and get the key and val
 * After you give the previous hentryp and so on
 * at the end NULL is returned
 */

void *
hash_table_scan(hash_table htp,
		void * hentryp_arg,
		void ** pkey,
		void ** pval)
{
  hash_entry * hentryp = (hash_entry *) hentryp_arg;
  hash_entry * hend = htp->array + htp->size;
  
  if (!hentryp)	hentryp = (void*) htp->array;

  while (hentryp < hend)
  {
    void *key = hentryp->key;
    
    if ((key !=HASH_ENTRY_FREE) && (key !=HASH_ENTRY_FREE_FOR_PUT))
    {
      *pkey = key;
      *pval = hentryp->val;
      return hentryp + 1;
    }
    hentryp++;
  }
  return NULL;
}

int hash_table_own_allocated_memory(hash_table htp)
{
    return htp ? 
      sizeof(struct __hash_table) + sizeof(hash_entry)*(htp->size) : 0 ;
}

/***************************************************************** MAP STUFF */
/* newgen mapping to newgen hash...
 */
void * hash_map_get(hash_table h, void * k)
{
  gen_chunk key, * val;
  key.e = k;
  val = (gen_chunk*)hash_get(h, &key);
  if (val==HASH_UNDEFINED_VALUE)
    fatal("no value correspond to key %p", k);
  return val->e;
}

bool hash_map_defined_p(hash_table h, void * k)
{
  gen_chunk key;
  key.e = k;
  return hash_defined_p(h, &key);
}

void hash_map_put(hash_table h, void * k, void * v)
{
  gen_chunk
    * key = (gen_chunk*) alloc(sizeof(gen_chunk)),
    * val = (gen_chunk*) alloc(sizeof(gen_chunk));
  key->e = k;
  val->e = v;
  hash_put(h, key, val);
}

void * hash_map_del(hash_table h, void * k)
{
  gen_chunk key, * oldkeychunk, * val;
  void * result;

  key.e = k;
  val = hash_delget(h, &key, (void**) &oldkeychunk);
  message_assert("defined value (entry to delete must be defined!)",
		 val!=HASH_UNDEFINED_VALUE);
  result = val->e;

  oldkeychunk->p = NEWGEN_FREED;
  free(oldkeychunk);

  val->p = NEWGEN_FREED;
  free(val);

  return result;
}

void hash_map_update(hash_table h, void * k, void * v)
{
  hash_map_del(h, k);
  hash_map_put(h, k, v);
}

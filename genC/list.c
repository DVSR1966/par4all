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

/* SCCS stuff:
 * $RCSfile: list.c,v $ ($Date: 1995/09/16 21:46:56 $, )
 * version $Revision$
 * got on %D%, %T%
 */

/* -- list.c

   The following functions implement a small library of utilities in the
   Lisp tradition. 

   . GEN_EQ is pointer comparison,
   . GEN_LENGTH returns the length of the list CP,
   . GEN_MAPL applies (*FP) to every CDR of CP.
   . GEN_MAP applies (*FP) to every item of the list.
   . GEN_REDUCE successively applies (*FP) on R adn every CRD of CP. 
   . GEN_SOME aplies (*FP) to every CDR of CP and returns the first sublist
     whose CAR verifies (*FP).
   . GEN_INSERT_AFTER inster a new object after a specified boject in the list
   . GEN_MAPC_TABULATED applies (*FP) on every non null element of the
     domain BINDING.
   . GEN_FIND_TABULATED retrieves the object of KEY in the tabulated DOMAIN.
   . GEN_FILTER_TABULATED returns a list of objects from DOMAIN that verify
     the FILTER.
   . GEN_FREE_LIST frees the spine of the list L.     
   . GEN_FULL_FREE_LIST frees the whole list L.     
   . GEN_NCONC physically concatenates CP1 and CP2 (returns CP1).
   . GEN_COPY copies one gen_chunks in another.
   . GEN_FIND_IF returns the leftmost element (extracted from the cons cell 
     by EXTRACT) of the sequence SEQ that satisfies TEST. EXTRACT should 
     be one of the car_to_domain function that are automatically generated 
     by Newgen.
   . GEN_FIND_IF_FROM_END is equivalent to GEN_FIND_IF but returns the 
     rightmost element of SEQ.
   . GEN_FIND returns the leftmost element (extracted from the cons cell 
     by EXTRACT) of the sequence SEQ such that TEST returns TRUE when applied 
     to ITEM and this element. EXTRACT should be one of the car_to_domain 
     function that are automatically generated by Newgen.
   . GEN_FIND_FROM_END is equivalent to GEN_FIND but returns the rightmost 
     element of SEQ.
   . GEN_FIND_EQ
   . GEN_CONCATENATE concatenates two lists. the structures of both lists are
     duplicated.
   . GEN_APPEND concatenates two lists. the structure of the first list is
     duplicated.
   . GEN_LAST returns the last cons of a list.
   . GEN_REMOVE updates the list (pointer) CPP by removing (and freeing) any
     ocurrence of the gen_chunk OBJ.
   . GEN_NTHCDR returns the N-th (beginning at 1) CDR element of L.
     CDR(L) = GEN_NTHCDR(1,L).
   . GEN_NTH returns the N-th (beginning at 0) car of L.
     CAR(L) = GEN_NTH(0,L).
   . GEN_SORT_LIST(L, compare) sorts L in place with compare (see man qsort)
   . GEN_ONCE(item, l) add item to l if not already there.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "newgen_include.h"
#include "genC.h"

int gen_eq( obj1, obj2 )
gen_chunk *obj1, *obj2 ;
{
  return( obj1 == obj2 ) ;
}

int gen_length( cp )
     cons *cp ;
{
  int i ;

  for( i=0; cp != NIL ; cp = cp->cdr, i++ )
    ;
  return( i ) ;
}

/*   MAP
 */
void gen_mapl( fp, cp )
void (*fp)() ;
cons *cp ;
{
  for( ; cp != NIL ; cp = cp->cdr )
    (*fp)( cp ) ;
}

void gen_map(fp, l)
void (*fp)();
list l;
{
    for (; !ENDP(l); l=CDR(l))
	(*fp)(CHUNK(CAR(l)));
}

char * gen_reduce( r, fp, cp )
char *r ;
char *(*fp)() ;
cons *cp ;
{
    for( ; cp != NIL ; cp = cp->cdr ) {
	r = (*fp)( r, cp ) ;
    }
    return( r ) ;
}

cons * gen_some( fp, cp )
bool (*fp)() ;
cons *cp ;
{
    for( ; cp!= NIL ; cp = cp->cdr ) {
	if( (*fp)( cp ))
		return( cp ) ;
    }
    return( NIL ) ;
}

/*  SPECIAL INSERTION
 */
static gen_chunk *gen_chunk_of_cons_of_gen_chunk = gen_chunk_undefined;
static bool cons_of_gen_chunk(cp)
cons *cp;
{
    return(CHUNK(CAR(cp))==gen_chunk_of_cons_of_gen_chunk);
}

void gen_insert_after(new_obj, obj, l)
gen_chunk *new_obj, *obj;
cons *l;
{
    cons *obj_cons = NIL;
    gen_chunk_of_cons_of_gen_chunk = obj;

    obj_cons = gen_some(cons_of_gen_chunk, l);
    assert(!ENDP(obj_cons));

    CDR(obj_cons) = CONS(CHUNK, new_obj, CDR(obj_cons));
}

#define NEXT(cp) (((cp) == NIL) ? NIL : (cp)->cdr)

cons * gen_nreverse( cp )
cons *cp ;
{
    cons *next, *next_next ;

    if( cp == NIL || cp->cdr == NIL ) return( cp ) ;

    next = cp->cdr ;
    cp->cdr = NIL ;
    next_next = NEXT( next ) ;
        
    for( ; next != NIL ; ) {
	next->cdr = cp ;
	cp = next ;
	next = next_next ;
	next_next = NEXT( next_next ) ;
    }
    return( cp ) ;
}
    
void gen_mapc_tabulated( fp, binding )
void (*fp)() ;
int binding ;
{
    struct gen_binding *bp = &Domains[ binding ] ;
    gen_chunk *table = Gen_tabulated_[ bp->index ] ;
    int i ;

    if( table == NULL ) {
	user( "gen_mapc_tabulated: non tabulated domain %s\n", bp->name ) ;
	return ;
    }
    for( i = 0 ; i < max_tabulated_elements() ; i++ ) {
	if( (table+i)->p != gen_chunk_undefined ) 
		(*fp)( (table+i)->p ) ;
    }
}

gen_chunk * gen_find_tabulated( key, domain )
char *key ;
int domain ;
{
    static char full_key[ 1024 ] ;
    gen_chunk *hash ;

    sprintf( full_key, "%d%c%s", domain, HASH_SEPAR, key ) ;

    if( (hash=(gen_chunk *)hash_get( Gen_tabulated_names, full_key ))
       == (gen_chunk *)HASH_UNDEFINED_VALUE ) {
	return( gen_chunk_undefined ) ;
    }
    return( (Gen_tabulated_[ Domains[ domain ].index ]+abs( hash->i ))->p ) ;
}

cons * gen_filter_tabulated( filter, domain )
int (*filter)() ;
int domain ;
{
    struct gen_binding *bp = &Domains[ domain ] ;
    gen_chunk *table = Gen_tabulated_[ bp->index ] ;
    int i ;
    cons *l ;

    if( table == NULL ) {
	user( "gen_filter_tabulated: non tabulated domain %s\n", bp->name ) ;
	return( NIL ) ;
    }
    for( i = 0, l = NIL ; i < max_tabulated_elements() ; i++ ) {
	gen_chunk *obj = (table+i)->p ;

	if( obj != gen_chunk_undefined && (*filter)( obj )) {
	    l = CONS( CHUNK, obj, l ) ;
	}
    }
    return( l ) ;
}

void gen_free_list( l )
cons *l ;
{
    cons *p, *nextp ;

    for( p = l ; p != NIL ; p = nextp ) {
	nextp = p->cdr ;
	free( p ) ;
    }
}

void gen_full_free_list( l )
cons *l ;
{
    cons *p, *nextp ;
    bool keep = FALSE ;

    for( p = l ; p != NIL ; p = nextp, keep = TRUE ) {
	shared_pointers( CAR(p).p, keep ) ;
	nextp = p->cdr ;
    }
    for( p = l ; p != NIL ; p = nextp ) {
	nextp = p->cdr ;
	gen_free_with_sharing( CAR(p).p ) ;
	free( p ) ;
    }
}

cons * gen_nconc( cp1, cp2 )
cons *cp1, *cp2 ;
{
    cons *head = cp1 ;

    if( cp1 == NIL ) 
	return( cp2 ) ;

    for( ; !ENDP( CDR( cp1 )) ; cp1 = CDR( cp1 )) 
	    ;
    CDR( cp1 ) = cp2 ;
    return( head ) ;
}

void gen_copy( a, b )
gen_chunk *a, *b ;
{
    *a = *b ;
}

gen_chunk *gen_car(l)
list l;
{
    return(CHUNK(CAR(l)));
}

gen_chunk *gen_identity(x)
gen_chunk *x;
{
    return(x);
}

gen_chunk *gen_find_if(test, pc, extract)
bool (*test)();
cons *pc;
gen_chunk *(*extract)();
{
    for (; pc!=NIL; pc=pc->cdr)
	if ((*test)((*extract)(CAR(pc))))
	    return((*extract)(CAR(pc)));

    return(gen_chunk_undefined);
}

/*  the last match is returned
 */
gen_chunk *gen_find_if_from_end(test, pc, extract)
bool (*test)();
cons *pc;
gen_chunk *(*extract)();
{
    gen_chunk *e = gen_chunk_undefined ;

    for (; pc!=NIL; pc=pc->cdr)
	if ((*test)((*extract)(CAR(pc))))
	    e = (*extract)(CAR(pc));

    return(e);
}

gen_chunk *gen_find(item, seq, test, extract)
gen_chunk *item;
cons *seq;
bool (*test)();
gen_chunk *(*extract)();
{
    cons *pc;

    for (pc = seq; pc != NIL; pc = pc->cdr ) {
	if ((*test)(item, (*extract)(CAR(pc))))
		return (*extract)(CAR(pc));
    }

    return( gen_chunk_undefined );
}

gen_chunk *gen_find_from_end(item, seq, test, extract)
gen_chunk *item;
cons *seq;
bool (*test)();
gen_chunk *(*extract)();
{
    cons *pc;
    gen_chunk *e = gen_chunk_undefined ;

    for (pc = seq; pc != NIL; pc = pc->cdr ) {
	if ((*test)(item, (*extract)(CAR(pc))))
		e = (*extract)(CAR(pc));
    }

    return(e);
}

gen_chunk *gen_find_eq(item, seq)
gen_chunk *item;
cons *seq;
{
    cons *pc;

    for (pc = seq; pc != NIL; pc = pc->cdr ) {
	if (item == CAR(pc).p)
		return CAR(pc).p;
    }

    return( gen_chunk_undefined );
}

cons *gen_concatenate(l1, l2)
cons *l1, *l2;
{
    cons *l = NIL, *q = NIL;

    if (l1 != NIL) {
	l = q = CONS(CHUNK, CHUNK(CAR(l1)), NIL);
	l1 = CDR(l1);
    }
    else if (l2 != NIL) {
	l = q = CONS(CHUNK, CHUNK(CAR(l2)), NIL);
	l2 = CDR(l2);
    }
    else {
	return(NIL);
    }

    while (l1 != NIL) {
	CDR(q) = CONS(CHUNK, CHUNK(CAR(l1)), NIL);
	q = CDR(q);

	l1 = CDR(l1);
    }

    while (l2 != NIL) {
	CDR(q) = CONS(CHUNK, CHUNK(CAR(l2)), NIL);
	q = CDR(q);

	l2 = CDR(l2);
    }

    return(l);
}

cons *gen_append(l1, l2)
cons *l1, *l2;
{
    cons *l = NIL, *q = NIL;

    if (l1 == NIL)
	return(l2);

    l = q = CONS(CHUNK, CHUNK(CAR(l1)), NIL);
    l1 = CDR(l1);

    while (l1 != NIL) {
	CDR(q) = CONS(CHUNK, CHUNK(CAR(l1)), NIL);
	q = CDR(q);

	l1 = CDR(l1);
    }

    CDR(q) = l2;

    return(l);
}

cons * gen_copy_seq(l)
cons *l;
{
    cons *nlb = NIL, *nle = NIL;

    while (! ENDP(l)) {
	cons *p = CONS(CHUNK, CHUNK(CAR(l)), NIL);

	if (nle == NIL)
	    nlb = p;
	else
	    CDR(nle) = p;
	nle = p;
	l = CDR(l);
    }

    return(nlb);
}

list gen_last(l)
list l;
{
    if (ENDP(l)) return(l);         /* NIL case */
    while (!ENDP(CDR(l))) l=CDR(l); /* else go to the last */
    return(l);
}
	
void gen_remove( cpp, obj )
cons **cpp ;
gen_chunk *obj ;
{
    if( ENDP( *cpp )) {
	return ;
    }
    if( obj == CHUNK( CAR( *cpp ))) {
	cons *aux = *cpp ;

	*cpp = CDR( *cpp ) ;
	free( aux ) ;
	gen_remove( cpp, obj ) ;
    }
    else {
	gen_remove( &CDR( *cpp ), obj ) ;
    }
}

/*  caution: the first item is 0!
 *  was:  return( (n<=0) ? l : gen_nthcdr( n-1, CDR( l ))) ;
 *  if n>gen_length(l), NIL is returned.
 */
list gen_nthcdr(n, l )
int n ;
list l ;
{
    assert(n>=0);
    for (; !ENDP(l) && n>0; l=CDR(l), n--);
    return(l);
}

/* to be used as ENTITY(gen_nth(3, l))...
 */
gen_chunk gen_nth(n, l)
int n;
list l;
{
    return(CAR(gen_nthcdr(n, l)));
}

list gen_once(item, l)
gen_chunk *item;
list l;
{
    list c;
    for(c=l; c!=NIL; c=CDR(c))
	if (CHUNK(CAR(c))==item) return(l);

    return(CONS(CHUNK, item, l));
}

bool gen_in_list_p(item, l)
gen_chunk *item;
list l;
{
    for (; !ENDP(l); POP(l))
	if (CHUNK(CAR(l))==item) return(TRUE); /* found! */

    return(FALSE); /* else no found */
}

/* Sorts a list of gen_chunks in place, to avoid mallocs. 
 * The list skeleton is not touched, but the items are replaced
 * within the list. If some of the cons are shared, it may trouble
 * the data and the program.
 *
 * See man qsort about the compare function: 
 *  - 2 pointers to the data are passed, 
 *  - and the result is <, =, > 0 if the comparison is lower than, equal...
 *
 * FC 27/12/94
 */
void gen_sort_list(l, compare)
list l;
int (*compare)();
{
    list c;
    int n = gen_length(l);
    gen_chunk 
	**table = (gen_chunk**) malloc(sizeof(gen_chunk*)*n),
	**point;

    /*   the list items are first put in the temporary table,
     */
    for (c=l, point=table; !ENDP(c); c=CDR(c), point++)
	*point = CHUNK(CAR(c));
    
    /*    then sorted,
     */
    qsort(table, n, sizeof(gen_chunk*), compare);

    /*    and the list items are updated with the sorted table
     */
    for (c=l, point=table; !ENDP(c); c=CDR(c), point++)
	CHUNK(CAR(c)) = *point;

    free(table); 
}

/* void gen_closure(iterate, initial)
 * list [of X] (*iterate)([ X, list of X ]), initial;
 * 
 * what: computes the transitive closure of sg starting from sg.
 * how: iterate till stability.
 * input: an iterate function and an initial list for the closure.
 *        the iterate functions performs some computations on X
 *        and should update the list of X to be looked at at the next 
 *        iteration. This list must be returned by the function.
 * output: none.
 * side effects:
 *  - *none* on initial...
 *  - those of iterate.
 * bugs or features:
 *  - not idiot proof. may run into an infinite execution...
 *  - a set base implementation would be nicer, but less deterministic.
 */
void gen_closure(iterate, initial)
list /* of X */ (*iterate)(/* X, list of X */), initial;
{
    list /* of X */ l_next, l_close = gen_copy_seq(initial);

    while (l_close)
    {
	l_next = NIL;

	MAPL(cc, l_next = iterate(CHUNK(CAR(cc)), l_next), l_close);

	gen_free_list(l_close), l_close = l_next;
    }
}

list gen_make_list(int domain, ...)
{
    list l, current;
    gen_chunk *item;
    va_list args;
    va_start(args, domain);

    item = va_arg(args, gen_chunk*);
    
    if (!item) return NIL;
	
    l = CONS(CHUNK, item, NIL), current = l;

    while((item=va_arg(args, gen_chunk*)))
	CDR(current) = CONS(CHUNK, item, NIL), POP(current);
    
    return l;
}

/*   That is all
 */

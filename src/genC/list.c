/*

  $Id$

  Copyright 1989-2010 MINES ParisTech

  This file is part of NewGen.

  NewGen is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or any later version.

  NewGen is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License along with
  NewGen.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "genC.h"
#include "newgen_include.h"

/** @defgroup newgen_list NewGen functions dealing with list objects

  The following functions implement a small library of utilities in the
  Lisp tradition.

  Lists are pointers to chunk objects that are linked together in a single
  forward way.

  . GEN_EQ is pointer comparison,
  . GEN_LENGTH returns the length of the list CP, if it is not cyclic,
  or loop forever
  . GEN_MAPL applies (*FP) to every CDR of CP.
  . GEN_MAP applies (*FP) to every item of the list.
  . GEN_REDUCE successively applies (*FP) on R adn every CRD of CP.
  . GEN_SOME aplies (*FP) to every CDR of CP and returns the first sublist
  whose CAR verifies (*FP).
  . GEN_INSERT_AFTER inster a new object after a specified object in the list
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
   by EXTRACT) of the sequence SEQ such that TEST returns true when applied
   to ITEM and this element. EXTRACT should be one of the car_to_domain
   function that are automatically generated by Newgen.
   . GEN_FIND_FROM_END is equivalent to GEN_FIND but returns the rightmost
   element of SEQ.
   . GEN_FIND_EQ
   . GEN_CONCATENATE concatenates two lists. the structures of both lists are
   duplicated.
   . GEN_APPEND concatenates two lists. the structure of the first list is
   duplicated.
   . GEN_COPY_SEQ
   . GEN_FULL_COPY_LIST
   . GEN_COPY_STRING_LIST
   . GEN_FREE_STRING_LIST
   . GEN_LAST returns the last cons of a list.
   . gen_substitute_chunk_by_list
   . GEN_REMOVE updates the list (pointer) CPP by removing (and freeing) any
     ocurrence of the gen_chunk OBJ.
   . GEN_REMOVE_ONCE : Remove the first occurence of obj in list l.
   . GEN_NTHCDR returns the N-th (beginning at 1) CDR element of L.
     CDR(L) = GEN_NTHCDR(1,L).
   . GEN_NTH returns the N-th (beginning at 0) car of L.
     CAR(L) = GEN_NTH(0,L).
   . GEN_SORT_LIST(L, compare) sorts L in place with compare (see man qsort)
   . GEN_ONCE(ITEM, L) prepends ITEM to L if not already there.
   . GEN_IN_LIST_P(ITEM, L) checks that item ITEM appears in list L
   . GEN_OCCURENCES(ITEM, L) returns the number of occurences of item
     ITEM in list L
   . GEN_ONCE_P(L) checks that each item in list L appears only once
   . GEN_CLOSURE()
   . GEN_MAKE_LIST(DOMAIN, ...) makes an homogeneous list of the varargs (but
     homogeneity is not checked)
   . gen_list_and(list * a, list b) : Compute A = A inter B
   . gen_list_and_not(list * a, list b) : Compute A = A inter non B
   . gen_list_patch(list l, gen_chunk * x, gen_chunk * y) :
     Replace all the reference to x in list l by a reference to y
   . gen_position(void * item, list l): rank of item in list l,
     0 if not present
*/

/** @{ */

/**@return true if object are identical
   @param obj1, the first object to test for equality
   @param obj2, the second object to test for equality
 */
bool gen_eq(const void * obj1, const void * obj2)
{
  return obj1 == obj2;
}

/**@return true if the list is cyclic. A list is considered cyclic if at least
 * one element points to a previously visited element.
 * @param list, the list to check
 */
bool gen_list_cyclic_p (const list ml)
{
  /* a list with 0 or 1 element is not cyclic */
  bool cyclic_p   = ! (ENDP(ml) || ENDP(CDR(ml)));

  if(cyclic_p) { /* it may be cyclic */
    list cl; /* To ease debugging */
    int  i        = 1;
    set  adresses = set_make (set_pointer);

    for (cl = ml; !ENDP(cl); POP(cl), i++) {
      if (set_belong_p ( adresses, cl)) {
	fprintf(stderr, "warning: cycle found");
	fprintf(stderr, "next elem %d:'%p' already in list\n",
		i, cl);
	cyclic_p = true;
	break;
      }
      set_add_element (adresses, adresses, cl);
      cyclic_p = false;
    }

    set_free (adresses);
  }

  return cyclic_p;
}

/** @return the length of the list
 *  @param cp, the list to evaluate, assumed to be acyclic
 */
size_t gen_length(const list l)
{
  list cp = l;
  size_t i;
  for (i = 0; cp != NIL ; cp = cp->cdr, i++) {;}
  return i;
}

size_t list_own_allocated_memory(const list l)
{
    return gen_length(l)*sizeof(cons);
}

/*   MAP
 */
void gen_mapl(gen_iter_func_t fp, const list l)
{
  list cp = (list) l;
  for (; cp != NIL ; cp = cp->cdr)
    (*fp)(cp);
}

void gen_map(gen_iter_func_t fp, const list l)
{
  list cp = (list) l;
  for (; !ENDP(cp); cp=CDR(cp))
    (*fp)(CHUNK(CAR(cp)));
}

// should be void * ?
void * gen_reduce(void * r, void *(*fp)(void *, const list), const list l)
{
  list cp = (list) l;
  for( ; cp != NIL ; cp = cp->cdr ) {
    r = (*fp)( r, cp );
  }
  return r;
}
/* compares two lists using the functor given in parameters
 * returns true if for all n, the n'th element of first list is equals
 * to the n'th element of the second list
 */
bool gen_equals(const list l0, const list l1,gen_eq_func_t equals)
{
    list iter0=l0,iter1=l1;
    while(!ENDP(iter0)&&!ENDP(iter1))
    {
        if(!equals(CHUNK(CAR(iter0)),CHUNK(CAR(iter1))))
            return false;
        POP(iter0);
        POP(iter1);
    }
    return ENDP(iter0)&&ENDP(iter1);

}

list gen_some(gen_filter_func_t fp, const list l)
{
  list cp = (list) l;
  for( ; cp!= NIL ; cp = cp->cdr )
    if( (*fp)(cp))
      return cp;
  return NIL;
}

/*  SPECIAL INSERTION
 */
static gen_chunk *gen_chunk_of_cons_of_gen_chunk = gen_chunk_undefined;
static bool cons_of_gen_chunk(const list cp)
{
  return CHUNK(CAR(cp))==gen_chunk_of_cons_of_gen_chunk;
}

void gen_insert_after(const void * no, const void * o, list l)
{
  gen_chunk * new_obj = (gen_chunk*) no, * obj = (gen_chunk*) o;
  cons *obj_cons = NIL;
  gen_chunk_of_cons_of_gen_chunk = obj;

  obj_cons = gen_some((gen_filter_func_t) cons_of_gen_chunk, l);
  assert(!ENDP(obj_cons));
  CDR(obj_cons) = CONS(CHUNK, new_obj, CDR(obj_cons));
}

/*
   insert object "no" before object "o" in the list "l". Return the new
   list.
*/
list gen_insert_before(const void * no, const void * o, list l)
{
  gen_chunk * new_obj = (gen_chunk*) no;
  gen_chunk * obj = (gen_chunk*) o;

  list r = NIL; /* result   */
  list c = l;   /* current  */
  list p = NIL; /* previous */

  /* search obj in list */
  for ( ; c!=NIL ; c=c->cdr)
    if ( CHUNK(CAR(c))==obj )
      break;
    else
      p = c;

  assert(!ENDP(c));

  if (p) { /* obj is not the first object of the list */
    CDR(p) = CONS(CHUNK, new_obj, CDR(p));
    r = l;
  }
  else { /* obj is the first object */
    r = CONS(CHUNK, new_obj, c);
  }
  return r;
}

/* insert nl before or after item in list l, both initial lists are consumed
   @return the new list
 */
list gen_insert_list(list nl, const void * item, list l, bool before)
{
  // fast case
  if (!nl) return l;

  // find insertion position in l
  list p = NIL, c = l, head;
  while (c && CHUNK(CAR(c))!=item)
    p = c, c = CDR(c);
  message_assert("item is in list", c!=NIL);

  // possibly forward one link if insertion is "after" the item
  if (!before)
    p = c, c = CDR(c);

  // link and set list head
  if (p) // some previous
    CDR(p) = nl, head = l;
  else // no previous
    p = nl, head = nl;

  // forward to link the tail, hold by "c"
  while (CDR(p)) p = CDR(p);
  CDR(p) = c;

  // done
  return head;
}

#define NEXT(cp) (((cp) == NIL) ? NIL : (cp)->cdr)

/* @brief reverse a list in place
 * @param cp, the list to be reversed
 * @return the list reversed
 */
list gen_nreverse(list cp)
{
  cons *next, *next_next ;

  if( cp == NIL || cp->cdr == NIL ) return( cp ) ;

  next = cp->cdr ;
  cp->cdr = NIL;
  next_next = NEXT( next );

  for( ; next != NIL ; )
  {
    next->cdr = cp ;
    cp = next ;
    next = next_next ;
    next_next = NEXT( next_next ) ;
  }
  return cp;
}

/**@brief free the spine of the list
 * @param l, the list to free
 */
void gen_free_list(list l)
{
  list p, nextp ;
  for( p = l ; p != NIL ; p = nextp ) {
    nextp = p->cdr ;
    CAR(p).p = NEWGEN_FREED; /* clean */
    CDR(p) = (struct cons*) NEWGEN_FREED;
    free( p );
  }
}

/**@brief physically concatenates CP1 and CP2 but do not duplicates the
 * elements
 * @return the concatenated list
 * @param cp1, the first list to concatenate
 * @param cp2, the second list to concatenate
 */
list gen_nconc(list cp1, list cp2)
{
    cons *head = cp1 ;

    if( cp1 == NIL )
	return( cp2 ) ;

    //message_assert ("cannot concatenate a cyclic list", gen_list_cyclic_p (cp1) == false);

    for( ; !ENDP( CDR( cp1 )) ; cp1 = CDR( cp1 ));

    CDR( cp1 ) = cp2 ;
    return( head ) ;
}

void gen_copy(void * a, void * b)
{
    * (gen_chunk*)a = * (gen_chunk*)b ;
}

void * gen_car(list l)
{
    return CHUNK(CAR(l));
}

void *
gen_find_if(gen_filter_func_t test, const list l, gen_extract_func_t extract)
{
  list pc = (list) l;
  for (; pc!=NIL; pc=pc->cdr)
    if ((*test)((*extract)(CAR(pc))))
      return (*extract)(CAR(pc));
  return gen_chunk_undefined;
}

/*  the last match is returned
 */
void *
gen_find_if_from_end(gen_filter_func_t test, const list l,
		     gen_extract_func_t extract)
{
  list pc = (list) l;
  void * e = gen_chunk_undefined ;
  for (; pc!=NIL; pc=pc->cdr)
    if ((*test)((*extract)(CAR(pc))))
      e = (*extract)(CAR(pc));
  return e;
}

/**@return the leftmost element (extracted from the cons cell
 * by EXTRACT) of the sequence SEQ such that TEST returns true when applied
 * to ITEM and this element. EXTRACT should be one of the car_to_domain
 * function that are automatically generated by Newgen.
 */
void * gen_find(const void * item, const list seq,
		gen_filter2_func_t test, gen_extract_func_t extract)
{
    list pc;
    for (pc = seq; pc != NIL; pc = pc->cdr )
	if ((*test)(item, (*extract)(CAR(pc))))
		return (*extract)(CAR(pc));
    return gen_chunk_undefined;
}

void * gen_find_from_end(const void * item, const list seq,
			 gen_filter2_func_t test, gen_extract_func_t extract)
{
    list pc;
    void * e = gen_chunk_undefined ;

    for (pc = seq; pc != NIL; pc = pc->cdr ) {
	if ((*test)(item, (*extract)(CAR(pc))))
		e = (*extract)(CAR(pc));
    }

    return e;
}

void * gen_find_eq(const void * item, const list seq)
{
  list pc;
  for (pc = (list) seq; pc != NIL; pc = pc->cdr )
    if (item == CAR(pc).p)
      return CAR(pc).p;
  return gen_chunk_undefined;
}

/**@brief concatenate two lists. the structures of both lists are duplicated.
 * @return a new allocated list with duplicated elements
 * @param l1, the first list to concatenate
 * @param l2, the second list to concatenate
 */
list gen_concatenate(const list l1x, const list l2x)
{
  // break const declaration...
  list l1 = (list) l1x, l2 = (list) l2x;
  list l = NIL, q = NIL;

  if (l1 != NIL) {
    l = q = CONS(CHUNK, CHUNK(CAR(l1)), NIL);
    l1 = CDR(l1);
  }
  else if (l2 != NIL) {
    l = q = CONS(CHUNK, CHUNK(CAR(l2)), NIL);
    l2 = CDR(l2);
  }
  else {
    return NIL;
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

  return l;
}

list gen_append(list l1, const list l2)
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


/* Copy a list structure

   It does not copy the list elements, the new list references the
   elements of the old one.

   @return the new list
 */
list gen_copy_seq(list l) {
  /* Begin of the new list: */
  list nlb = NIL;
  /* Pointer to the last element of th new list: */
  list nle = NIL;

  /* While we are not at the end of the list: */
  while (! ENDP(l)) {
    /* Create a new list element with the current element: */
    list p = CONS(CHUNK, CHUNK(CAR(l)), NIL);

    if (nle == NIL)
      /* If nle is NIL, it is the first element, so keep it as the list
	 beginning: */
      nlb = p;
    else
      /* Append the new element at the end of the new list: */
      CDR(nle) = p;
    /* Update the end pointer: */
    nle = p;
    /* Look for the next element of the list to copy: */
    l = CDR(l);
  }

  return nlb;
}


/* Copy a list structure with element copy

   It does copy the list elements.

   @return the new list
 */
list gen_full_copy_list(list l) {
  list nlb = NIL;
  list nle = NIL;

  while (! ENDP(l)) {
    /* Create a new list element with a copy of the current element: */
    list p = CONS(CHUNK, gen_copy_tree(CHUNK(CAR(l))), NIL);

    if (nle == NIL)
      nlb = p;
    else
      CDR(nle) = p;
    nle = p;
    l = CDR(l);
  }

  return nlb;
}


list /* of string */
gen_copy_string_list(list /* of string */ ls)
{
    list l = NIL;
    MAP(STRING, s, l = CONS(STRING, strdup(s), l), ls);
    return gen_nreverse(l);
}

void
gen_free_string_list(list /* of string */ ls)
{
    gen_map(free, ls);
    gen_free_list(ls);
}

list gen_last(list l)
{
    if (ENDP(l)) return l;         /* NIL case */
    while (!ENDP(CDR(l))) l=CDR(l); /* else go to the last */
    return l;
}

/* substitute item o by list sl in list *pl, which is modified as a
 * side effect.  The substitution is performed only once when the
 * first o is found. List sl is physically included in list *pl. If sl
 * is empty, item o is removed from list *pl. If o is not found in
 * *pl, *pl is left unmodified.
 */
void gen_substitute_chunk_by_list(list * pl, const void * o, list sl)
{
  list * pc = pl; // current indirect pointer
  list  ppc = NULL; // pointer to the previous cons

  if(ENDP(sl))
    gen_remove_once(pl, o);
  else
    while (*pc) {
      /* If the chunk to substitute is found, substitute it */
      if ((gen_chunk*) o == CHUNK(CAR(*pc))) {
	list tmp = *pc;
	list npc = CDR(*pc);

	/* Insert sl at the beggining of the new list or after ppc */
	if(ppc==NULL) {
	  *pl = sl;
	}
	else {
	  CDR(ppc) = sl;
	}

	/* Add the left over list after sl */
	CDR(gen_last(sl)) = npc;

	/* Get rid of the useless cons */
	CAR(tmp).p = NEWGEN_FREED;
	CDR(tmp) = NEWGEN_FREED;
	free(tmp);

	break;
      }
      else {
	/* Move down the input list */
	ppc = *pc;
	pc = &CDR(*pc);
      }
    }
}

/* substitute all item s by t in list l
 * @return whether the substitution was performed
 */
bool gen_replace_in_list(list l, const void * s, const void * t)
{
  bool done = false;
  while (l)
  {
    if (CHUNK(CAR(l))==s)
    {
      CHUNK(CAR(l)) = (gen_chunk*) t;
      done=true;
    }
    l = CDR(l);
  }
  return done;
}

/* exchange items i1 & i2 in the list
 */
void gen_exchange_in_list(list l, const void * i1, const void * i2)
{
  while (l)
  {
    if (CHUNK(CAR(l))==i1) CHUNK(CAR(l)) = (gen_chunk*) i2;
    else if (CHUNK(CAR(l))==i2) CHUNK(CAR(l)) = (gen_chunk*) i1;
    l = CDR(l);
  }
}

/* remove item o from list *pl which is modified as a side effect.
 * @param once whether to do it once, or to look for all occurences.
 */
static void gen_remove_from_list(list * pl, const void * o, bool once)
{
  list * pc = pl;
  while (*pc)
  {
    if ((gen_chunk*) o == CHUNK(CAR(*pc)))
    {
      list tmp = *pc;
      *pc = CDR(*pc);
      CAR(tmp).p = NEWGEN_FREED;
      CDR(tmp) = NEWGEN_FREED;
      free(tmp);
      if (once) return;
    }
    else
      pc = &CDR(*pc);
  }
}

/* remove all occurences of item o from list *cpp, which is thus modified.
 */
void gen_remove(list * cpp, const void * o)
{
  gen_remove_from_list(cpp, o, false);
}

/* Remove the first occurence of o in list pl: */
void gen_remove_once(list * pl, const void * o)
{
  gen_remove_from_list(pl, o, true);
}

/*  caution: the first item is 0!
 *  was:  return( (n<=0) ? l : gen_nthcdr( n-1, CDR( l ))) ;
 *  if n>gen_length(l), NIL is returned.
 */
list gen_nthcdr(int n, const list lx)
{
  list l = (list) lx;
  message_assert("valid n", n>=0);
  for (; !ENDP(l) && n>0; l=CDR(l), n--);
  return(l);
}

/* to be used as ENTITY(gen_nth(3, l))...
 */
gen_chunk gen_nth(int n, const list l)
{
  list r = gen_nthcdr(n, l);
  message_assert("not NIL", r);
  return CAR(r);
}


/* Prepend an item to a list only if it is not already in the list.

   Return the list anyway.
*/
list gen_once(const void * vo, list l)
{
  gen_chunk * item = (gen_chunk*) vo;
  list c;
  for(c=l; c!=NIL; c=CDR(c))
    if (CHUNK(CAR(c))==item) return l;

  return CONS(CHUNK, item, l);
}

bool gen_in_list_p(const void * vo, const list lx)
{
  list l = (list) lx;
  gen_chunk * item = (gen_chunk*) vo;
  for (; !ENDP(l); POP(l))
    if (CHUNK(CAR(l))==item) return true; /* found! */

  return false; /* else no found */
}

int gen_occurences(const void * vo, const list l)
{
  list c = (list) l;
  int n = 0;
  gen_chunk * item = (gen_chunk*) vo;
  for (; !ENDP(c); POP(c))
    if (CHUNK(CAR(c))==item) n++;
  return n;
}

/* FC: ARGH...O(n^2)!
*/
bool gen_once_p(list l)
{
  list c;
  for(c=l; c!=NIL && CDR(c)!=NIL; c=CDR(c)) {
    gen_chunk * item = CHUNK(CAR(c));
    if(gen_in_list_p(item , CDR(c)))
	    return false;
  }
  return true;
}

/* free an area.
 * @param p pointer to the zone to be freed.
 * @param size size in bytes.
 *
 * Why is this function located in list.c?
 */
void gen_free_area(void ** p, int size)
{
  int n = size/sizeof(void*);
  int i;
  for (i=0; i<n; i++) {
    *(p+i) = NEWGEN_FREED;
  }
  free(p);
}

/* Sorts a list of gen_chunks in place, to avoid allocations...
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
void gen_sort_list(list l, gen_cmp_func_t compare)
{
  list c;
  int n = gen_length(l);
  gen_chunk
    **table = (gen_chunk**) alloc(n*sizeof(gen_chunk*)),
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

  gen_free_area((void**) table, n*sizeof(gen_chunk*));
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
  gen_chunk *item = NULL;
  va_list args;
  va_start(args, domain);
  item = va_arg(args, gen_chunk*);
  if( !item ) return NIL;

  NEWGEN_CHECK_TYPE(domain, item);
  l = CONS(CHUNK, item, NIL), current = l;
  for(item = va_arg(args, gen_chunk*);item;item = va_arg(args, gen_chunk*)) {
    NEWGEN_CHECK_TYPE(domain, item);
    CDR(current) = CONS(CHUNK, item, NIL), POP(current);
  }
  va_end(args);
  return l;
}

list gen_cons(const void * item, const list next)
{
  list ncons = (list) alloc(sizeof(struct cons));
  ncons->car.e = (void *) item;
  ncons->cdr = (list) next;
  return ncons;
}

/* CONS a list with minimal type checking
 * this cannot be done within the CONS macro because
 * possible functions calls must not be replicated.
 */
list gen_typed_cons(_int type, const void * item, const list next)
{
  NEWGEN_CHECK_TYPE(type, item);
  // also check consistency with first item in list
  if (next!=NIL) NEWGEN_CHECK_TYPE(type, next->car.e);
  return gen_cons(item, next);
}

/* typed cons for "basic" types */
list gen_bool_cons(bool b, const list l)
{
  return gen_cons((const void *) b, l);
}

list gen_int_cons(_int i, const list l)
{
  return gen_cons((const void *) i, l);
}

list gen_string_cons(string s, const list l)
{
  return gen_cons((const void *) s, l);
}

list gen_list_cons(const list i, const list l)
{
  return gen_cons((const void *) i, l);
}

list gen_CHUNK_cons(const gen_chunk * c, const list l)
{
  return gen_cons((const void *) c, l);
}

/* Compute A = A inter B: complexity in O(n2) */
void
gen_list_and(list * a, const list b)
{
  if (ENDP(*a))
    return ;

  if (!gen_in_list_p(CHUNK(CAR(*a)), b)) {
    /* This element of a is not in list b: delete it: */
    cons *aux = *a;

    *a = CDR(*a);
    CAR(aux).p = NEWGEN_FREED;
    CDR(aux) = NEWGEN_FREED;
    free(aux);
    gen_list_and(a, b);
  }
  else
    gen_list_and(&CDR(*a), b);
}


/* Compute A = A inter non B: */
void
gen_list_and_not(list * a, const list b)
{
  if (ENDP(*a))
    return ;

  if (gen_in_list_p(CHUNK(CAR(*a)), b)) {
    /* This element of a is in list b: delete it: */
    cons *aux = *a;

    *a = CDR(*a);
    CAR(aux).p = NEWGEN_FREED;
    CDR(aux) = NEWGEN_FREED;
    free(aux);
    gen_list_and_not(a, b);
  }
  else
    gen_list_and_not(&CDR(*a), b);
}


/* Replace all the reference to x in list l by a reference to y: */
void
gen_list_patch(list l, const void * x, const void * y)
{
    MAPL(pc, {
	 if (CAR(pc).p == (gen_chunk *) x)
	     CAR(pc).p = (gen_chunk *) y;
     }, l);
}

/* Element ranks are strictly positive as for first, second, and so on. If
   item is not in l, 0 is returned. */
int gen_position(const void * item, const list l)
{
  list c_item  = (list) l;
  int rank = 0;

  for(; !ENDP(c_item); POP(c_item)) {
    rank++;
    if(item==CHUNK(CAR(c_item))) {
      return rank;
    }
  }
  return 0;
}

/* @return exactly the first n elements from *lp as a list;
 * *lp points to the remaining list, as a side effect.
 * if gen_length(*lp) is less than n, the function aborts.
 * @param lp pointeur to the list.1
 * @param n number of items to extract.
 */
list gen_list_head(list * lp, int n)
{
  if (n<=0) return NIL;
  // else n>0, something to skip
  list head = *lp, last = *lp;
  n--;
  while (n--) {
    message_assert("still some items", last);
    last = CDR(last);
  }
  *lp = CDR(last);
  CDR(last) = NIL;
  return head;
}

/** @} */


/*   That is all
 */

%{
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

/* read.y: The syntax of objects printed by GEN_WRITE. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genC.h"
#include "newgen_include.h"

extern void newgen_lexer_position(FILE *);

#define YYERROR_VERBOSE 1 /* better error messages by bison */

extern int yyinput(void);
extern void yyerror(char*);

extern FILE * yyin;

/* This constant should be adapted to the particular need of the application */

/* set to 10000 by BC - necessary in PIPS for DYNA */
/* Should be a compilation option ? */
/* CA: pb avec COX si a 10000... 
   p'tet mauvaise recursion dans le parser de newgen? 
*/
#define YYMAXDEPTH 100000

/* User selectable options. */

int warn_on_ref_without_def_in_read_tabulated = FALSE;

/* Where the root will be. */

gen_chunk *Read_chunk ;

/* The SHARED_TABLE 
 * maps a shared pointer number to its gen_chunk pointer value.
 * warning: big hack.
 */
static int shared_number;
static int shared_size;
static gen_chunk ** shared_table ;

/* The GEN_TABULATED_NAMES hash table maps ids to index in the table of
   the tabulated domains. In case of multiple definition, if the previous
   value is negative, then it came from a REF (by READ_TABULATED) and
   no error is reported. */

/* Management of forward references in read */

int newgen_allow_forward_ref = FALSE;

static void * read_external(int);
static gen_chunk * make_def(gen_chunk *);
static gen_chunk * make_ref(int, gen_chunk *);
static gen_chunk * chunk_for_domain(int);

%}

%token CHUNK_BEGIN
%token VECTOR_BEGIN
%token ARROW_BEGIN
%token READ_BOOL
%token TABULATED_BEGIN
%token LP
%token RP
%token LC
%token RC
%token LB
%token SHARED_POINTER
%token READ_EXTERNAL
%token READ_DEF
%token READ_REF
%token READ_NULL

%token READ_LIST_UNDEFINED
%token READ_SET_UNDEFINED
%token READ_ARRAY_UNDEFINED
%token READ_STRING

%union {
  gen_chunk chunk ;
  gen_chunk *chunkp ;
  cons *consp ;
  int val ;
  char * s;
  double d;
  char c;
}

%term READ_UNIT
%term <c> READ_CHAR
%term <val> READ_INT
%term <d> READ_FLOAT
%type <s> READ_STRING
%type <chunk> Data Basis 
%type <chunkp> Chunk String Contents
%type <consp> Sparse_Datas Datas
%type <val> Int Shared_chunk Type
%type <void> Datas2

%%
Read	: Nb_of_shared_pointers Contents
          { 
	    Read_chunk = $2;
	    free(shared_table);
	    /* return */ YYACCEPT;
	  }
	;

Nb_of_shared_pointers 
  	: Int
        { 
	  int i;
	  shared_number = 0;
	  shared_size = $1;
	  shared_table = (gen_chunk **)alloc($1*sizeof(gen_chunk*)); 
	  for (i=0; i<shared_size; i++)
	    shared_table[i] = gen_chunk_undefined;
        }
	;

Contents: Chunk { $$ = $1; }
	| TABULATED_BEGIN Type Datas2 RP
        { 
	   $$ = (gen_chunk*) alloc(sizeof(gen_chunk));
	   $$->i = $2;
	   /* gen_free_list($3); */
        }
	;

Chunk 	: Shared_chunk CHUNK_BEGIN Type Datas RP 
          {
	    int i, size = Domains[$3].size;
	    cons *cp ;

	    /* see HACK bellow. */
	    $$ = $1? shared_table[$1-1]: chunk_for_domain($3);
	    
	    /* copy contents. */
	    $$->i = $3;
	    for(i=size-1, cp=$4; i>0 && cp; i--, cp=cp->cdr)
	      *($$+i) = cp->car;

	    message_assert("all data copied", 
			   !cp && (i==0 || (i==1 && Domains[$3].tabulated)));

	    if (i==1) ($$+1)->i = 0; /* tabulated number... */
	    gen_free_list($4);
	  }
	;

Shared_chunk /* see HACK bellow */
	: LB Int { $$ = shared_number = $2; }
	|        { $$ = shared_number = 0; }
	;

Type	: Int 
        { 
	  $$ = gen_type_translation_old_to_actual($1); 

	  /* HACK: the first type after a  shared handles allocation.
	   * it MUST be performed here so that references to this
	   * can be valid even if its parsing is not finished yet.
	   */
	  if (shared_number) 
	  {
	    shared_table[shared_number-1] = chunk_for_domain($$);
	    shared_number = 0;
	  }
	}
	;

/* We should avoid to build explicit lists, as we know what we are reading? */
Datas	: Datas Data { $$ = CONS( CHUNK, $2.p, $1 ); }
	| { $$ = NIL; }
	;

/* no list is built as it is not needed */
Datas2 : Datas2 Data { }
       | Data { }
       ;


Sparse_Datas: Sparse_Datas Int Data { /* index, value */
	        $$ = CONS(CONSP, CONS(INT, $2, CONS(CHUNK, $3.p, NIL)), $1);
        }
	| { $$ = NIL; }
	;

Data	: Basis	{ $$ = $1; }
        | READ_LIST_UNDEFINED { $$.l = list_undefined; }
	| LP Datas RP {	$$.l = gen_nreverse($2); }
        | READ_SET_UNDEFINED { $$.t = set_undefined; }
        | LC Int Datas RC 
        {
	  $$.t = set_make( $2 ) ;
	  MAPL( cp, {
	    switch( $2 ) {
	    case set_int:
	      set_add_element( $$.t, $$.t, (char *)cp->car.i ) ;
	      break ;
	    default:
	      set_add_element( $$.t, $$.t, cp->car.s ) ;
	      break ;
	    }}, $3 ) ;
	  gen_free_list( $3 ) ;
	}
        | READ_ARRAY_UNDEFINED { $$.p = array_undefined ; }
	| VECTOR_BEGIN Int Sparse_Datas RP 
        {
	  gen_chunk *kp ;
	  cons *cp ;
	  int i ;
	  
	  kp = (gen_chunk *)alloc( $2*sizeof( gen_chunk )) ;
	  
	  for( i=0 ; i != $2 ; i++ ) {
	    kp[ i ].p = gen_chunk_undefined ;
	  }
	  for( cp=$3 ; cp!=NULL ; cp=cp->cdr ) {
	    cons *pair = CONSP( CAR( cp )) ;
	    int index = INT(CAR(pair));
	    gen_chunk val = CAR(CDR(pair));
	    assert(index>=0 && index<$2);
	    kp[index] = val;

	    gen_free_list(pair); /* free */
	  }

	  gen_free_list($3);
	  $$.p = kp ;
	}
	| ARROW_BEGIN Datas RP {
		hash_table h = hash_table_make( hash_chunk, 0 )	;
		cons *cp ;

		for( cp = gen_nreverse($2) ; cp != NULL ; cp=cp->cdr->cdr ) {
			gen_chunk *k = (gen_chunk *)alloc(sizeof(gen_chunk));
			gen_chunk *v = (gen_chunk *)alloc(sizeof(gen_chunk));
	
			*k = CAR(  cp ) ;
			*v = CAR( CDR( cp )) ;
			hash_put( h, (char *)k, (char *)v ) ;
		}
		gen_free_list( $2 ) ;
		$$.h = h ;
		}
	| Chunk { $$.p = $1 ; }
	| SHARED_POINTER Int 
        {
	  message_assert("shared is defined", 
			 shared_table[$2-1]!=gen_chunk_undefined);
	  $$.p = shared_table[$2-1];
	}
	;
  
Basis	: READ_UNIT { $$.u = 1; }
	| READ_BOOL Int { $$.b = $2; }
	| READ_CHAR { $$.c = $1; }
	| Int	{ $$.i = $1; }
	| READ_FLOAT { $$.f = $1; }
	| String { $$ = *$1 ; }
 	| READ_EXTERNAL Int { $$.s = (char*) read_external($2); }
	| READ_DEF Chunk { $$.p = make_def($2); }
	| READ_REF Type String { $$.p = make_ref($2, $3); }
	| READ_NULL { $$.p = gen_chunk_undefined ; }
	;

Int     : READ_INT   { $$ = $1; }
	;

String  : READ_STRING {
		gen_chunk *obj = (gen_chunk *)alloc(sizeof(gen_chunk));
		obj->s = $1;
		$$ = obj;
	    }
		    
%%

static gen_chunk * chunk_for_domain(int domain)
{
  gen_chunk * cp;
  check_domain(domain);
  cp = (gen_chunk*) alloc(sizeof(gen_chunk)*Domains[domain].size);
  cp->i = domain;
  return cp;
}

/* YYERROR manages a syntax error while reading an object. */

void yyerror(char * s)
{
  int c, n=40;
  newgen_lexer_position(stderr);
  fprintf(stderr, "%s before ", s);

  while (n-->0  && ((c=yyinput()) != EOF))
    putc(c, stderr);

  fprintf(stderr, "\n\n");

  fatal("Incorrect object written by GEN_WRITE\n", (char *) NULL);
}

/* READ_EXTERNAL reads external types on stdin */

static void * read_external(int which)
{
  struct gen_binding *bp;
  union domain *dp;
  extern int yyinput();

  which = gen_type_translation_old_to_actual(which);
  message_assert("consistent domain number", which>=0 && which<MAX_DOMAIN);

  bp = &Domains[ which ] ;
  dp = bp->domain ;
  
  if( dp->ba.type != EXTERNAL_DT ) {
    fatal( "gen_read: undefined external %s\n", bp->name ) ;
    /*NOTREACHED*/
  }
  if( dp->ex.read == NULL ) {
    user( "gen_read: uninitialized external %s\n", bp->name ) ;
    return( NULL ) ;
  }
  if( yyinput() != ' ' ) {
    fatal( "read_external: white space expected\n", (char *)NULL ) ;
    /*NOTREACHED*/
  }
  /*
    Attention, ce qui suit est absolument horrible. Les fonctions
    suceptibles d'etre  appelees a cet endroit sont:
    - soit des fonctions 'user-written' pour les domaines externes
    non geres par NewGen
    - soit la fonctions gen_read pour les domaines externes geres
    par NewGen 
    
    Dans le 1er cas, il faut passer la fonction de lecture d'un caractere
    (yyinput) a la fonction de lecture du domaine externe (on ne peut pas
    passer le pointeur de fichier car lex bufferise les caracteres en
    entree). Dans le second cas, il faut passer le pointeur de fichier a
    cause de yacc/lex.
    
    Je decide donc de passer les deux parametres: pointeur de fichier et
    pointeur de fonction de lecture. Dans chaque cas, l'un ou l'autre sera
    ignore. 
  */
  return (*(dp->ex.read))(yyin, yyinput);
}

/* MAKE_DEF defines the object CHUNK of name STRING to be in the tabulation 
   table INT. domain translation is handled before in Chunk.
 */
static gen_chunk * make_def(gen_chunk * gc)
{
  int domain = gc->i;
  string id = (gc+2)->s;
  return gen_enter_tabulated(domain, id, gc, newgen_allow_forward_ref);
}

/* MAKE_REF references the object of hash name STRING in the tabulation table
   INT. Forward references are dealt with here.
 */
static gen_chunk * make_ref(int domain, gen_chunk * st)
{
  gen_chunk * cp = gen_find_tabulated(st->s, domain);

  if (gen_chunk_undefined_p(cp))
  {
    if (newgen_allow_forward_ref) 
    {
      cp = (gen_chunk*) alloc(sizeof(gen_chunk)*Domains[domain].size);
      cp->i = domain;
      (cp+1)->i = 0; /* no number yet */
      (cp+2)->s = st->s; /* TAKEN! */
      cp = gen_do_enter_tabulated(domain, st->s, cp, TRUE);
    }
    else 
    {
      user("make_ref: forward references to %s prohibited\n", st->s) ;
    }
  }
  else 
  {
    free(st->s);
  }

  free(st);
  return cp;
}

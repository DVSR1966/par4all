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


/* read.y 

   The syntax of objects printed by GEN_WRITE. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genC.h"
#include "newgen_include.h"

extern int gen_find_free_tabulated(struct gen_binding *);
extern void newgen_lexer_position(FILE *);

#define YYERROR_VERBOSE 1 /* better error messages by bison */

extern int yyinput(void);
extern void yyerror(char*);

extern FILE * yyin;

/* This constant should be adapted to the particular need of the application */

/* set to 10000 by BC - necessary in PIPS for DYNA */
/* Should be a compilation option ? */
/* CA: pb avec COX si a 10000... p'tet mauvaise recursion dans le parser de newgen? */
#define YYMAXDEPTH 100000

/* User selectable options. */

int warn_on_ref_without_def_in_read_tabulated = FALSE;

/* Where the root will be. */

gen_chunk *Read_chunk ;

/* The SHARED_TABLE maps a shared pointer number to its gen_chunk pointer value. */

static gen_chunk **shared_table ;

/* The GEN_TABULATED_NAMES hash table maps ids to index in the table of
   the tabulated domains. In case of multiple definition, if the previous
   value is negative, then it came from a REF (by READ_TABULATED) and
   no error is reported. */

/* Management of forward references in read */

int newgen_allow_forward_ref = FALSE ;

static char * read_external(int);
static gen_chunk * make_def(gen_chunk *);
static gen_chunk * make_ref(int, gen_chunk *);

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

%%
Read	: Nb_of_shared_pointers Contents
          { 
	    Read_chunk = $2;
	    free(shared_table);
	    /* return */ YYACCEPT;
	  }
	;

Nb_of_shared_pointers 
  	: Int { shared_table = (gen_chunk **)alloc($1*sizeof(gen_chunk*)); }
	;

Contents: Chunk { $$ = $1; }
	| TABULATED_BEGIN Type Datas RP
        { 
	   $$ = (gen_chunk*) alloc(sizeof(gen_chunk));
	   $$->i = $2;
	   gen_free_list($3); 
        }
	;

Chunk 	: Shared_chunk CHUNK_BEGIN Type Datas RP 
          {
	    int i, size;
	    cons *cp ;

	    assert($3>=0 && $3<MAX_DOMAIN);
	    size = gen_size(Domains+$3);

	    $$ = (gen_chunk *)alloc(size*sizeof(gen_chunk));

	    /* chunck is shared. */
	    if ($1) shared_table[$1-1] = $$;

	    /* copy contents... */
	    $$->i = $3;
	    for(i=size-1, cp=$4; i>0 && cp; i--, cp=cp->cdr)
	      *($$+i) = cp->car;

	    message_assert("all data copied", !cp && (i==0 || i==1));

	    if (i==1) ($$+1)->i = -1; /* tabulated number... */
	    gen_free_list($4);
	  }
	;

Shared_chunk
	: LB Int { $$ = $2; }
	|        { $$ = 0; }
	;

Type	: Int { $$ = gen_type_translation_old_to_actual($1); }
	;

Datas	: Datas Data { $$ = CONS( CHUNK, $2.p, $1 ); }
	| { $$ = NIL; }
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
	| SHARED_POINTER Int { $$.p = shared_table[$2-1]; }
	;
  
Basis	: READ_UNIT { $$.u = 1; }
	| READ_BOOL Int { $$.b = $2; }
	| READ_CHAR { $$.c = $1; }
	| Int	{ $$.i = $1; }
	| READ_FLOAT { $$.f = $1; }
	| String { $$ = *$1 ; }
 	| READ_EXTERNAL Int { $$.s = read_external($2); }
	| READ_DEF Chunk { $$.p = make_def($2); }
	| READ_REF Int String { $$.p = make_ref($2, $3) ; }
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

static char * read_external(int which)
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

extern gen_chunk * 
  gen_enter_tabulated_def(int, int, char *, gen_chunk *, int);

/* MAKE_DEF defines the object CHUNK of name STRING to be in the tabulation 
   table INT. domain translation is handled before in Chunk.
 */
static gen_chunk * make_def(gen_chunk * gc)
{
  int domain = gc->i;
  char * id = strdup((gc+2)->s);
  message_assert("domain is tabulated", Domains[domain].index!=-1);
  return gen_enter_tabulated_def(Domains[domain].index, domain, id, gc, 
				 newgen_allow_forward_ref) ;
}

/* MAKE_REF references the object of hash name STRING in the tabulation table
   INT. Forward references are dealt with here.
 */
static gen_chunk * make_ref(int domain, gen_chunk * st)
{
    gen_chunk *hash ;
    gen_chunk *cp ;
    int Int;
    string String;

    domain = gen_type_translation_old_to_actual(domain);
    Int = Domains[domain].index;

    if(Gen_tabulated_[Int]==(gen_chunk *)NULL) {
      user( "read: Unloaded tabulated domain %s\n", Domains[domain].name ) ;
    }

    String = strdup(gen_build_unique_tabulated_name(domain, st->s));

    if((hash=(gen_chunk *)gen_get_tabulated_name_direct(String))
	== (gen_chunk *) HASH_UNDEFINED_VALUE) 
    {
      if (newgen_allow_forward_ref) 
      {
	hash = (gen_chunk *)alloc( sizeof( gen_chunk )) ;
	hash->i = -gen_find_free_tabulated( &Domains[ domain ] ) ;
	
	gen_put_tabulated_name_direct(String, (char *)hash) ;
	
	if((Gen_tabulated_[ Int ]+abs( hash->i ))->p != 
	   gen_chunk_undefined) {
	  fatal("make_ref: trying to re-allocate for %s\n", String);
	}
	(Gen_tabulated_[ Int ]+abs( hash->i ))->p = 
	  (gen_chunk *)alloc(gen_size(Domains+domain)*sizeof(gen_chunk));
      }
      else {
	user("make_ref: Forward references to %s prohibited\n", String) ;
        }
    }
    cp = (Gen_tabulated_[Int]+abs(hash->i))->p ;
    (cp+1)->i = abs( hash->i ) ;
    return cp;
}

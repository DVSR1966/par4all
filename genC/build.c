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


/* build.c

   This file manages the building of the "data dictionary" (i.e. the Domains
   table) and the generation of the specification file. 

   $Id$

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "genC.h"
#include "newgen_include.h"

/* FC: formats are directly inlined in genClib...
 */
struct inlinable Inline[] = {
  {UNIT_TYPE_NAME, "U", "U", ":unit", "U"},
  {"bool", "1", "B%d", "newgen:gen-true", "~S"},
  {"char", "#\\a", "#\\%c", "#\\space", "~C"},
  {"int", "0", "%d", "0", "~D"},
  {"float", "0.0", "%f", "0.0", "~F"},
  {"string", "\"\"", "\"%s\"", "\"\"", "~S"},
  {NULL, "-- HELP --", "-- HELP --", "-- HELP --", "-- HELP --"},
} ;

char *keywords[] = {
    "external",
    "import",
    "tabulated",
    "from",
    "persistant",
    NULL
} ;

int Current_index ;
int Current_op ;

int Current_start ;
/* int Current_first ; // tag global setting*/

/* Warning: this table knows about the actual values used for AND_OP 
   and OR_OP. */

char *Op_names[] = {
  "-- shouldn't appear --",
  "x",
  "+",
  "->"
  } ;

/* Have we seen a user error somewhere ? */

int error_seen ;

/* FATAL generates a fatal error by printing (according to FORMAT)
   the given MSG. If there already is a user error, let's suppose that's
   her fault ! */

void fatal(char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf( stderr, "\nNewgen Fatal Error -- " ) ;
    vfprintf( stderr, fmt, args) ;
    va_end(args);

    abort() ;
}

/* USER generates a user error (i.e., non fatal) by printing the given MSG
   according to the FMT. */

void user(char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf( stderr, "\nNewgen User Error -- " ) ;
    vfprintf( stderr, fmt, args ) ;

    va_end(args);
    error_seen++ ;
}

/* ITOA (Integer TO Ascii) yields a string for a given Integer. */

char *
itoa( i )
     int i ;
{
  static char buf[ 20 ] ;

  sprintf( &buf[0], "%d", i ) ;
  return( buf ) ;
}

/* CHECK_NOT_KEYWORD checks if S isn't a reserved word. */

void
check_not_keyword( s )
char *s ;
{
    char **sp ;

    for( sp = keywords ; *sp != NULL ; sp++ ) {
	if( strcmp( s, *sp ) == 0 ) {
	    user( "Trying to use the reserved word %s\n", s ) ;
	}
    }
}

/* MAX_TABULATED_ELEMENTS returns the maximum number of elements for 
   tabulated domains. */

#define DEFAULT_MTE 12000

int max_tabulated_elements(void)
{
  static int max = -1 ;

  if(max<0) /* then initialization needed */
  {
      char *s = getenv("NEWGEN_MAX_TABULATED_ELEMENTS");

      if (s) 
	  max = atoi(s); /* value taken from the environment */
      else
	  max = DEFAULT_MTE; /* default value assume if nothing specified */
  }

  return max;
}

/* INIT initializes global data structures. */

void
init()
{
    struct gen_binding *bp ;
    struct inlinable *ip ;

    for( bp = Domains ; bp < &Domains[ MAX_DOMAIN ] ; bp++ ) {
	bp->name = NULL ;
	bp->compiled = 0 ;
	bp->index = -1 ;
	bp->domain = NULL ;
	bp->inlined = NULL ;
    }
    for( ip = Inline, bp = Domains ; ip->name != NULL ; ip++, bp++ ) {
	bp->name = ip->name ;
	bp->compiled = 1 ;
	bp->inlined = ip ;
    }
{
    static union domain d ;
    static struct intlist il ;

    il.val = max_tabulated_elements() ;
    il.cdr = (struct intlist *)NULL ;

    bp->name = "Name for tabulated domain" ;
    bp->domain = &d ;
    d.ba.type = ARRAY_DT ;
    d.ar.constructor = "Constructor for tabulated domain" ;
    d.ar.element = (struct gen_binding *)NULL ;
    d.ar.dimensions = &il ;
    Tabulated_bp = bp ;
}
    Current_op = UNDEF_OP ;
    error_seen = 0 ;
    Current_index = -1 ;
    Current_start = -1 ;
    /* Current_first = -1 ; */
}

static int max_domain = -1 ;

int
max_domain_index()
{
    return( max_domain ) ;
}

/* LOOKUP checks whether a given NAME is in the Domains table. If not, the
   ACTION says whether this is an error or it should be introduced. If this
   is for a new gen_binding, look in the current allocation range, else from
   beginning */

struct gen_binding *
lookup( name, action )
char *name ;
int action ;
{
    struct gen_binding *bp ;

    //bp = (action == NEW_BINDING) ? Tabulated_bp+Current_start+1 : Domains ;
    bp = Domains + ((action == NEW_BINDING) ? Current_start : 0);

    for( ; bp < &Domains[ MAX_DOMAIN ] ; bp++ ) {
	if( bp->name == NULL ) {
	    if( action == NEW_BINDING )
		    break ;
	}
	else if( strcmp( bp->name, name ) == 0 ) break ;
    }
    switch( action ) {
    case NEW_BINDING:
	if( bp == &Domains[ MAX_DOMAIN ] )
		fatal( "lookup: Domains overflow on %s -- new\n", name ) ;

	break;
    case OLD_BINDING:
	if( bp == &Domains[ MAX_DOMAIN ] )
		user( "Unknown domain <%s>\n", name ) ;

	break ;
    default:
	fatal( "lookup: Unknown type %s\n", itoa( action )) ;
    }
    if( max_domain < bp-Domains ) {
	max_domain = bp-Domains ;
    }
    return( bp ) ;
}

/* NEW_BINDING introduces a new pair (NAME, VAL) in the Domains table. 
   Redeclaration is allowed if this is to overwrite a previous IMPORT
   definition. Note that we could (should ?) check that this new gen_binding
   isn't a new (and incompatible) IMPORT definition. */

struct gen_binding *
new_binding( name, val )
char *name ;
union domain *val ;
{
    struct gen_binding *bp ;

    if( Read_spec_mode && val->ba.type ==  IMPORT_DT ) {
	bp = lookup( name, OLD_BINDING ) ;
    }
    else {
	bp = lookup( name, NEW_BINDING ) ;
    }
    if( bp->domain == NULL ) {
#ifdef DBG_BINDING
	(void)fprintf(stderr, "Introducing %s at index %d\n", 
		      name, bp-Domains ) ;
#endif
	bp->domain = val ;
	bp->name = name ;
    }
    else
    {
	if( val->ba.type == IMPORT_DT || 
	    (Read_spec_mode && val->ba.type == EXTERNAL_DT )) {
	    free( (char *)val ) ;
	}
	else 
	    user( "Redeclaration skipped: <%s>\n", name ) ;

	free(name);
    }

    return( bp ) ;
}

/* PRINT_DOMAINLIST prints, in the OUT stream, the List of domains, bound
   together by an OPerator. */

void
print_domainlist( out, l, op )
     FILE *out ;
     struct domainlist *l ;
     int op ;
{
  if( l == NULL )
    fatal( "print_domainlist: null", "" ) ;

  for( ; l->cdr != NULL ; l = l->cdr ) {
    print_domain( out, l->domain ) ;
    (void)fprintf( out, " %s ", Op_names[ op ] ) ;
  }
  print_domain( out, l->domain ) ;
}
      
void
print_persistant( out, dp )
FILE *out ;
union domain *dp ;
{
    if( dp->ba.persistant ) {
	(void)fprintf( out, "persistant " ) ;
    }
}

/* PRINT_DOMAIN prints on the OUT stream a domain denoted by the DP pointer.
   This is done before the compilation so (STRUCT BINDING *) members are
   still strings. */

void
print_domain( out, dp )
FILE *out ;
union domain *dp ;
{
    if (!dp) {
	fprintf(out, " NULL union domain");
	return;
    }

    switch( dp->ba.type ) {
    case EXTERNAL_DT:
	break ;
    case IMPORT_DT:
	(void)fprintf( out, " from \"%s\"", dp->im.filename ) ;
	break ;
    case BASIS_DT:
	(void)print_persistant( out, dp ) ;
#ifdef DBG_DOMAINS
	(void)fprintf( out, "%s:%s (%d)", 
		      dp->ba.constructor, dp->ba.constructand->name,
		      dp->ba.constructand-Domains ) ;
#else
	(void)fprintf(out, "%s:%s", 
		      dp->ba.constructor, dp->ba.constructand->name );
#endif
	break ;
    case LIST_DT:
	print_persistant( out, dp ) ;
	(void)fprintf(out, "%s:%s*", 
		      dp->li.constructor, dp->li.element->name ) ;
	break ;
    case SET_DT:
	print_persistant( out, dp ) ;
	(void)fprintf(out, "%s:%s{}", 
		      dp->se.constructor, dp->se.element->name ) ;
	break ;
    case ARRAY_DT: {
	struct intlist *ilp ;

	print_persistant( out, dp ) ;
	(void)fprintf(out, "%s:%s", 
		      dp->ar.constructor, dp->ar.element->name ) ;

	for( ilp = dp->ar.dimensions ; ilp != NULL ; ilp = ilp->cdr ) 
		(void)fprintf( out, "[%d]", ilp->val ) ;

	break ;
    }
    case CONSTRUCTED_DT:
	print_domainlist( out, dp->co.components, dp->co.op ) ;
	break ;
    default:
	fatal( "print_domain: switch on %s\n", itoa( dp->ba.type )) ;
    }
}

/* PRINT_DOMAINS prints on the OUT stream the Domains table. Inlined domains
   aren't printed. */

void
print_domains( out )
FILE *out ;
{
    struct gen_binding *bp ;
    union domain *dp ;

    for( bp = Domains ; bp < &Domains[ MAX_DOMAIN ] ; bp++ ) {
	if( bp->name == NULL || bp == Tabulated_bp ) continue ;

	if( !IS_INLINABLE( bp )) {

	  /* if( (dp=bp->domain)->ba.type==CONSTRUCTED_DT && dp->co.op==OR_OP)
	     (void)fprintf( out, "--NEWGEN-FIRST %d\n", dp->co.first ) ; */

	    (void)fprintf( out, 
		    (IS_EXTERNAL( bp ) ? "external %s" : 
		     IS_IMPORT( bp ) ? "import %s" :
		     IS_TABULATED( bp ) ? "tabulated %s = " :
		     "%s = "), 
		    bp->name ) ;
	    print_domain( out, bp->domain ) ;
	    (void)fprintf( out, ";\n" ) ;
	}
    }
}

/* RECONNECT replaces the (STRUCT BINDING *) members of the DP domain
   which are, on entry, strings by their effective values, i.e. pointers
   in the Domains table which have the corresponding names. 

   FIRST members for OR_OP (checked with OP) domains are updated. */

static int current_first ;

void
reconnect( op, dp )
int op ;
union domain *dp ;
{
    struct domainlist *dlp ;

    if( op == OR_OP ) current_first++ ;

    switch( dp->ba.type ) {
    case EXTERNAL_DT:
    case IMPORT_DT:
	return ;
    case BASIS_DT:
	dp->ba.constructand = lookup( (char *)dp->ba.constructand, OLD_BINDING ) ;
	break;
    case LIST_DT:
	dp->li.element = lookup( (char *)dp->li.element, OLD_BINDING ) ;
	break ;
    case SET_DT:
	dp->se.element = lookup( (char *)dp->se.element, OLD_BINDING ) ;
	break ;
    case ARRAY_DT:
	dp->ar.element = lookup( (char *)dp->ar.element, OLD_BINDING ) ;
	break ;
    case CONSTRUCTED_DT: 
	if( dp->co.op == OR_OP && !Read_spec_mode ) 
		dp->co.first = current_first ;

	for( dlp = dp->co.components ; dlp != NULL ; dlp = dlp->cdr ) {
	    reconnect( dp->co.op, dlp->domain ) ;
	}
	return ;
    default:
	fatal( "reconnect: switch on %s\n", itoa( dp->ba.type )) ;
    }
}

/* COMPILE reconnects the Domains table (for not compiled types -- note that
   an inlined type is already compiled). */

void
compile()
{
    struct gen_binding *bp = Domains ;

    current_first = 0;

    for( ; bp < &Domains[ MAX_DOMAIN ] ; bp++ ) {
	if( bp->name == NULL || bp->compiled || bp == Tabulated_bp )
		continue ;

	reconnect( -1, bp->domain ) ;
	bp->compiled = (bp->domain->ba.type != EXTERNAL_DT ) ;

	if( IS_TABULATED( bp )) {
	    union domain *dp = NULL;

	    if( !(bp->domain->ba.type == CONSTRUCTED_DT &&
		  (dp=bp->domain->co.components->domain)->ba.type == BASIS_DT &&
		  strcmp( dp->ba.constructand->name, "string" ) == 0)) {
		user( "compile: tabulated first %s domain isn't string\n",
		      dp->ba.constructand->name ) ;
	    }
	}
    }
#ifdef DBG_COMPILE
    print_domains( stderr ) ;
#endif
}

/* GEN_WRITE_SPEC prints the Domains table in the given FILENAME. */

void gen_write_spec(char * filename)
{
    extern int fclose();
    FILE *id ;

    if( (id = fopen( filename, "w" )) == NULL ) {
	user( "Cannot open spec file %s in write mode\n", filename ) ;
	return ;
    }
    print_domains( id ) ;

    if( fclose( id )) 
	    user( "Cannot close spec file %s\n", filename ) ;
}

/* BUILD (in fact, the "main" function) parses the specifications and generates
   the manipulation functions. */

/*ARGSUSED*/
int build(int argc, char * argv[])
{
    init() ;
    genspec_parse() ;
    compile() ;
  
    if (error_seen == 0) {
	gencode(argv[1]);
	gen_write_spec(argv[2]);
	return( 0 ) ;
    }
    return( 1 ) ;
}

/* ALLOC is an "iron-clad" version of malloc(3). */

char * alloc(int size)
{
    char * p;

    if( (p=malloc( (unsigned)size )) == NULL && size != 0)
	    fatal( "alloc: No more memory for %s bytes\n", itoa( size )) ;
  
    return p;
}

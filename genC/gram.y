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


/* gram.y */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "malloc.h"
#include "newgen_include.h"

#define YYMAXDEPTH 300

struct gen_binding Domains[ MAX_DOMAIN ] ;
int Number_imports ;

/* UPDATE_OP checks whether the just read OPerator is compatible with
   the current one. If not, an ERROR_MSG is signaled. */

void
update_op( op, error_msg )
int op ;	
char *error_msg ;
{
    if( Current_op == UNDEF_OP ) {
	Current_op = op ;
    }
    else if( Current_op == ARROW_OP ) {
	user( error_msg, (char *)NULL ) ;
    }
    else if(Current_op != op ) {
	user( error_msg, (char *)NULL ) ;
    }
}


%}

%token COMMA
%token COLUMN
%token SEMI_COLUMN
%token AND
%token OR
%token ARROW
%token STAR
%token LB
%token RB
%token LR
%token RR
%token EQUAL
%token FROM
%token GRAM_EXTERNAL
%token GRAM_IMPORT
%token TABULATED
%token PERSISTANT

%term IDENT
%term GRAM_FILE
%term GRAM_INT

%union {
  union domain *domain ;
  struct domainlist *domainlist ;
  struct namelist *namelist ;
  struct intlist *intlist ;
  char *name ;
  int val ;
}

%type <namelist> Namelist
%type <intlist> Dimensions
%type <domain> Basis Simple Domain
%type <domainlist> Constructed
%type <name> Name File
%type <val> Int Tabulated Persistant

%%
Specification
	: {Number_imports = 0;} Imports Externals Definitions {
                YYACCEPT ;
                /*NOTREACHED*/
		}
	;

Externals
	: Externals GRAM_EXTERNAL Name SEMI_COLUMN {
		union domain *dp ;

		/*NOSTRICT*/
		dp = (union domain *)alloc( sizeof( union domain )) ;
		dp->ex.type = EXTERNAL_DT ;
		dp->ex.read = (char *(*)())NULL ;
                dp->ex.write = dp->ex.free = (void (*)())NULL ;
		new_binding( $3, dp ) ;
		}
	|
	;

Imports : Imports GRAM_IMPORT Name FROM File SEMI_COLUMN {
		union domain *dp ;

		/*NOSTRICT*/
		dp = (union domain *)alloc( sizeof( union domain )) ;
		dp->ba.type = IMPORT_DT ;
		dp->im.filename = $5 ;
		new_binding( $3, dp ) ;
		Number_imports++ ;
		}
	|
	;

File	: GRAM_FILE {
		$$ = alloc( strlen( yytext )) ;
		strcpy( $$, yytext+1 ) ;
		*( $$ + strlen( yytext+1 ) - 1 ) = '\0' ;
		}
	;

Definitions
	: Definitions Definition
	| Definition
	;

Definition
	: Tabulated Name EQUAL Domain SEMI_COLUMN {
		struct gen_binding *bp = new_binding( $2, $4 ) ;

		if( $1 ) {
		    if( Current_index == MAX_TABULATED ) {
			user( "Too many tabulated domains\n" ) ;
			}
		    else bp->index = Current_index++ ;
		}
		}
	;

Tabulated
	: TABULATED {
		$$ = 1 ;
		}
	|	{
		$$ = 0 ;
		}
	;

Domain	: Simple Constructed {
                /* A BASIS type with just one field is considered as an
		   AND_OP. */

  		if( $2 == NULL && $1->ba.type != BASIS_DT ) 
			$$ = $1 ;
		else {
			struct domainlist *dlp ;
	
			/*NOSTRICT*/
			dlp = (struct domainlist *)
				alloc( sizeof( struct domainlist ));
			dlp->domain = $1 ;
			dlp->cdr = $2 ;
			/*NOSTRICT*/
			$$ = (union domain *)alloc( sizeof( union domain )) ;
			$$->co.type = CONSTRUCTED_DT ;
			$$->co.components = dlp ;
			$$->co.op = ($2 == NULL) ? AND_OP : Current_op ;
			
			if( $$->co.op == OR_OP && Read_spec_mode )
				$$->co.first = Current_first ;

			Current_op = UNDEF_OP ;
		        }
		}
        | LR Namelist RR {
                /* This is sugar for an OR node with domains of type unit.*/

		/*NOSTRICT*/
	        struct domainlist *dlp =
		  (struct domainlist *)alloc( sizeof( struct domainlist )) ;

		/*NOSTRICT*/
	        $$ = (union domain *)
		        alloc( sizeof( union domain )) ;
		$$->co.type = CONSTRUCTED_DT ;
		$$->co.op = OR_OP ;
		$$->co.components = dlp ;

                if( Read_spec_mode ) $$->co.first = Current_first ;

	        for( ; $2 != NULL ; $2 = $2->cdr, dlp = dlp->cdr ) {
		  /*NOSTRICT*/
		  dlp->domain =
		    (union domain *)alloc( sizeof( union domain )) ;
		  dlp->cdr =
		    ($2->cdr == NULL) ?
		      NULL :
		      (struct domainlist *)alloc( sizeof( struct domainlist ));
		  dlp->domain->ba.type = BASIS_DT ;
		  dlp->domain->ba.constructor = $2->name ;
		  /*NOSTRICT*/
		  dlp->domain->ba.constructand = (struct gen_binding *)UNIT_TYPE_NAME ;
	          }
	      }
        ;
        
Simple	: Persistant Basis {
	        ($$ = $2)->ba.persistant = $1 ;
		}
	| Persistant Basis STAR {
		($$ = $2)->li.type = LIST_DT ;
		$$->li.persistant = $1 ;                
		$$->li.constructor = $2->ba.constructor ;
		$$->li.element = $2->ba.constructand ;
		}
	| Persistant Basis Dimensions {
		($$ = $2)->ar.type = ARRAY_DT ;
		$$->ar.persistant = $1 ;
		$$->ar.constructor = $2->ba.constructor ;
		$$->ar.element = $2->ba.constructand ;
		$$->ar.dimensions = $3 ;
		}
	| Persistant Basis LR RR {
	        char *below = (char *)$2->ba.constructand ;

		($$ = $2)->se.type = SET_DT ;
		$$->se.persistant = $1 ;
		$$->se.constructor = $2->ba.constructor ;
		$$->se.element = $2->ba.constructand ;
                $$->se.what =
	            (strcmp( below, "string" ) == 0) ? set_string :
	            (strcmp( below, "int" ) == 0) ? set_int :
	            set_pointer ;
		}
	;

Persistant
	: PERSISTANT {
                $$ = 1 ;
                }
        |       {$$ = 0;}
        ;

Basis   : Name {
		/*NOSTRICT*/
		$$ = (union domain *)alloc( sizeof( union domain )) ;
		$$->ba.type = BASIS_DT ;
		$$->ba.constructor = $1 ;
		/*NOSTRICT*/
		$$->ba.constructand = (struct gen_binding *)$1 ;
		}
	| Name COLUMN Name {
		/*NOSTRICT*/
		$$ = (union domain *)alloc( sizeof( union domain )) ;
		$$->ba.type = BASIS_DT ;
		$$->ba.constructor = $1 ;
		/*NOSTRICT*/
		$$->ba.constructand = (struct gen_binding *)$3 ;
		}
        ;

Constructed
	: AND Simple Constructed {
		/*NOSTRICT*/
		$$ = (struct domainlist *)alloc( sizeof( struct domainlist ));
		$$->domain = $2 ;
		$$->cdr = $3 ;
		update_op( AND_OP, "OR prohibited in an AND constructor\n" ) ;
		}
	| OR Simple Constructed {
		/*NOSTRICT*/
		$$ = (struct domainlist *)alloc( sizeof( struct domainlist ));
		$$->domain = $2 ;
		$$->cdr = $3 ;
		update_op( OR_OP, "AND prohibited in an OR constructor\n" ) ;
	        }
	| ARROW Simple {
		$$ = (struct domainlist *)alloc( sizeof( struct domainlist ));
		$$->domain = $2 ;
		$$->cdr = NULL ;
		update_op( ARROW_OP, "-> is a unary constructor\n" ) ;
		}	
	| 	{
		$$ = NULL ;
		}
	;

Namelist
	:  Name COMMA Namelist {
		/*NOSTRICT*/
		$$ = (struct namelist *)alloc( sizeof( struct namelist )) ;
		$$->name = $1 ;
		$$->cdr = $3 ;
		}
	| Name	{
		/*NOSTRICT*/
		$$ = (struct namelist *)alloc( sizeof( struct namelist )) ;
		$$->name = $1 ;
		$$->cdr = NULL ;
		}				
	;

Dimensions
	:  LB Int RB Dimensions {
		/*NOSTRICT*/
		$$ = (struct intlist *)alloc( sizeof( struct intlist )) ;
		$$->val = $2 ;
		$$->cdr = $4 ;
		}
	| LB Int RB {
		/*NOSTRICT*/
		$$ = (struct intlist *)alloc( sizeof( struct intlist )) ;
		$$->val = $2 ;
		$$->cdr = NULL ;
		}				
	;

Int     : GRAM_INT   {$$ = atoi( yytext );}
        ;

Name	: IDENT	{
	        $$ = strdup(yytext); 
		check_not_keyword( $$ ) ;
		}
	;
%%

/* Syntax error routines called by yacc. */

int yyerror( s )
char *s ;
{
  int c;

  user( "%s before ", s ) ;
	
  while( (c=yyinput()) != 0 )
    fprintf( stderr, "%c", c ) ;

  exit( 1 ) ;
}


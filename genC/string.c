/*

CONCATENATE concatenates a variable list of strings in a static string
(which is returned).

STRNDUP copie les N premiers caracteres de la chaine S dans une zone
allouee dynamiquement, puis retourne un pointeur sur cette zone. si la
longueur de S est superieure ou egale a N, aucun caratere null n'est
ajoute au resultat. sinon, la chaine resultat est padde avec des
caracteres null.


STRNDUP0 copie les N premiers caracteres de la chaine S dans une zone
allouee dynamiquement, puis retourne un pointeur sur cette zone.
l'allocation est systematiquement faite avec N+1 caracteres de telle
sorte qu'un caractere null soit ajoute a la chaine resultat, meme si la
longueur de la chaine S est superieure ou egale a N. dans le cas
contraire, la chaine resultat est padde avec des caracteres null.

*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#include <stdarg.h>
#include "string.h"
#include "genC.h"

#include "misc.h"

string strndup(n, s)
register int n; /* le nombre de caracteres a copier */
register string s; /* la chaine a copier */
{
 	register string r;
	register int i;

	/* allocation */
	if ((r = (string) malloc((unsigned) n)) == NULL) {
		fprintf(stderr, "strndup: out of memory\n");
		exit(1);
	}

	/* recopie */
	for (i = 0; i < n && s[i] != '\0'; i += 1 )
		r[i] = s[i];

	/* padding */
	while (i < n) {
		r[i] = '\0';
		i += 1;
	}

	return(r);
}

string strndup0(n, s)
register int n; /* le nombre de caracteres a copier */
register string s; /* la chaine a copier */
{
 	register string r;
	register int i;

	/* allocation */
	if ((r = (string) malloc((unsigned) n+1)) == NULL) {
		fprintf(stderr, "strndup0: out of memory\n");
		exit(1);
	}

	/* recopie */
	for (i = 0; i < n && s[i] != '\0'; i += 1 )
		r[i] = s[i];

	/* padding */
	while (i < n+1) {
		r[i] = '\0';
		i += 1;
	}

	return(r);
}

/* CONCATENATE() *********** Last argument must be NULL *********/

#define BUFFER_SIZE_INCREMENT 128
#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

static string buffer = (string) NULL;
static int buffer_size = 0;
static int current = 0;

void
init_the_buffer()
{
    /* initial allocation
     */
    if (buffer_size==0)
    {
	pips_assert("NULL buffer", buffer==NULL);
	buffer_size = BUFFER_SIZE_INCREMENT;
	buffer = (string) malloc(buffer_size);
	if (!buffer) pips_exit(1, "memory exhausted\n");
    }
    current = 0;
    buffer[0] = '\0';
}

string 
append_to_the_buffer(
    string s) /* what to append to the buffer */
{
    int len = strlen(s);

    /* reallocates if needed
     */
    if (current+len >= buffer_size)
    {
	buffer_size = MAX(current+len+1, buffer_size+BUFFER_SIZE_INCREMENT);
	buffer = realloc(buffer, buffer_size);
	if (!buffer) pips_exit(1, "memory exhausted\n");
    }

    (void) memcpy(&buffer[current], s, len);
    current+=len;
    buffer[current] = '\0' ;

    return buffer;
}

string 
get_the_buffer()
{
    return buffer;
}

/* concatenation is based on a static dynamic buffer
 * which is shared from one call to another.
 * FC.
 */
string 
concatenate(string next, ...)
{
    va_list args;
    
    init_the_buffer();

    /* now gets the strings and concatenates them
     */
    va_start(args, next);
    while (next)
    {
	(void) append_to_the_buffer(next);
	next = va_arg(args, string);
    }
    va_end(args);

    /* returns the static null terminated buffer
     */
    return buffer;
}

char *strupper(s1, s2)
char *s1, *s2;
{
    char *r = s1;

    while (*s2) {
	*s1 = (islower(*s2)) ? toupper(*s2) : *s2;
	s1++;
	s2++;
    }

    *s1 = '\0';
	
    return(r);
}

char *strlower(s1, s2)
char *s1, *s2;
{
    char *r = s1;

    while (*s2) {
	*s1 = (isupper(*s2)) ? tolower(*s2) : *s2;
	s1++;
	s2++;
    }

    *s1 = '\0';
	
    return(r);
}

string bool_to_string(b)
bool b;
{
    static string t = "TRUE";
    static string f = "FALSE";

    return b?t:f;
}

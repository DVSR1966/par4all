#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "genC.h"
#include "ri.h"
#include "text.h"

#include "misc.h"
#include "ri-util.h"
#include "properties.h"

#include "arithmetique.h"


/* #include "constants.h" */

/* FI: just to make sure that text.h is built; pips-makemake -l does not
   tale into account a library whose modules do not use the library header */
#include "text-util.h"


static int position_in_the_output = 0;

/* Before using print_sentence: */
static void
print_sentence_init()
{
    position_in_the_output = 0;
}


/* Output functions that tracks the number of output characters: */
static char
putc_sentence(char c,
	      FILE * fd)
{
    position_in_the_output ++;
    return putc(c, fd);
}

static int
fprintf_sentence(FILE * fd,
		 char * a_format,
		 ...)
{
    va_list some_arguments;
    int number_of_printed_char;
    
    va_start(some_arguments, a_format);
    number_of_printed_char = vfprintf(fd, a_format, some_arguments);
    va_end(some_arguments);
    
    position_in_the_output += number_of_printed_char;    
    return number_of_printed_char;
}


/* print_sentence:
 *
 * FI: I had to change this module to handle string longer than the space available
 * on one line; I tried to preserve as much as I could of the previous behavior to
 * avoid pseudo-hyphenation at the wrong place and to avoid extensicve problems
 * with validate; the resulting code is lousy, of course; FI, 15 March 1993
 * 
 * RK: the print_sentence could print lower case letter according to
 * a property... 17/12/1993.
 */
void
print_sentence(FILE * fd,
	       sentence s)
{
    if (sentence_formatted_p(s)) {
	string ps = sentence_formatted(s);
	
	while (*ps) {
	    char c = *ps;
	    putc_sentence(c, fd);
	    deal_with_attachments_at_this_character(ps,
						    position_in_the_output);
	    ps++;
	}
   }
    else {
	unformatted u = sentence_unformatted(s);
	int col;
	int i;
	int line_num = 1;
	string label = unformatted_label(u);
	int em = unformatted_extra_margin(u);
	int n = unformatted_number(u);
	cons *lw = unformatted_words(u);

	if (label != (char *) NULL) {
	    /* Keep track of the attachment against the padding: */
	    deal_with_attachments_in_this_string(label,
						 position_in_the_output);
	    fprintf_sentence(fd, "%-5s ", label);
	}
	else {
	    fprintf_sentence(fd, "      ");
	}

	/* FI: do not indent too much (9 June 1995)*/
	em = em > 42 ? 42 : em;

	for (i = 0; i < em; i++) 
	    putc_sentence(' ', fd);
	col = 7+em;

	pips_assert("print_sentence", col <= 72);

	while (lw) {
	    string w = STRING(CAR(lw));
	    STRING(CAR(lw)) = NULL;
	    lw = CDR(lw);

	    /* if the string fits on the current line: no problem */
	    if (col + strlen(w) <= 70) {
		deal_with_attachments_in_this_string(w,
						     position_in_the_output);
		col += fprintf_sentence(fd, "%s", w);
	    }
	    /* if the string fits on one line: 
	     * use the 88 algorithm to break as few
	     * syntactic constructs as possible */
	    else
		if(strlen(w) < 70-7-em) {
		    if (col + strlen(w) > 70) {
			/* Complete current line with the statement
                           line number: */
			if (n > 0) {
			    for (i = col; i <= 72; i++) putc_sentence(' ', fd);
			    fprintf_sentence(fd, "%04d", n);
			}

			/* start a new line with its prefix */
			putc_sentence('\n', fd);

			if(label != (char *) NULL 
			   && (strcmp(label,"CDIR$")==0
			       || strcmp(label,"CDIR@")==0
			       || strcmp(label,"CMIC$")==0)) {
			    /* Special label for Cray directives */
			    fprintf_sentence(fd, "%s%d", label, (++line_num)%10);
			}
			else
			    fprintf_sentence(fd, "     &");

			for (i = 0; i < em; i++)
			    putc_sentence(' ', fd);

			col = 7+em;
		    }
		    deal_with_attachments_in_this_string(w,
							 position_in_the_output);
		    col += fprintf_sentence(fd, "%s", w);
		}
	    /* if the string has to be broken in at least two lines: 
	     * new algorithmic part
	     * to avoid line overflow (FI, March 1993) */
		else {
		    char * line = w;
		    int ncar;

		    /* complete the current line */
		    ncar = 72 - col + 1;
		    deal_with_attachments_in_this_string_length(line,
								position_in_the_output,
								ncar);
		    fprintf_sentence(fd,"%.*s", ncar, line);
		    line += ncar;
		    col = 73;

		    /*
		       if (n > 0) {
		       for (i = col; i <= 72; i++) putc(' ', fd);
		       fprintf(fd, "%04d", n);
		       }
		       */

		    while(strlen(line)!=0) {
			ncar = MIN(72 - 7 +1, strlen(line));

			/* start a new line with its prefix but no indentation
			 * since string constants may be broken onto two lines */
			putc_sentence('\n', fd);

			if(label != (char *) NULL 
			   && (strcmp(label,"CDIR$")==0
			       || strcmp(label,"CDIR@")==0
			       || strcmp(label,"CMIC$")==0)) {
			    /* Special label for Cray directives */
			    (void) fprintf_sentence(fd, "%s%d", label, (++line_num)%10);
			}
			else
			    (void) fprintf_sentence(fd, "     &");

			col = 7 ;
			deal_with_attachments_in_this_string_length(line,
								    position_in_the_output,
								    ncar);
			(void) fprintf_sentence(fd, "%.*s", ncar, line);
			line += ncar;
			col += ncar;
		    }
		}
	    free(w);
	}

	pips_assert("print_sentence", col <= 72);

	/* Output the statement line number on the right end of the
           line: */
	if (n > 0) {
	    for (i = col; i <= 72; i++) putc_sentence(' ', fd);
	    fprintf_sentence(fd, "%04d", n);
	}
	putc_sentence('\n', fd);
    }
}

void dump_sentence(sentence s)
{
    print_sentence(stderr, s);
}

void print_text(fd, t)
FILE *fd;
text t;
{
    print_sentence_init();
    MAPL(cs, 
	 print_sentence(fd, SENTENCE(CAR(cs))), 
	 text_sentences(t));
}

/* FI: print_text() should be fprint_text() and dump_text(), print_text() */

void dump_text(t)
text t;
{
    print_text(stderr, t);
}


/* Convert a word list into a string and translate the position of
   eventual attachment accordingly: */
string words_to_string(lw)
cons *lw;
{
    static char buffer[1024];

    buffer[0] = '\0';
    MAPL(pw, {
	string w = STRING(CAR(pw));
	if (strlen(buffer)+strlen(w) > 1023) {
	    fprintf(stderr, "[words_to_string] buffer too small\n");
	    exit(1);
	}
	(void) strcat_word_and_migrate_attachments(buffer, w);
    }, lw);

    return(strdup_and_migrate_attachments(buffer));
}

void dump_words(list lw)
{
    print_words(stderr, lw);
}


void print_words(fd, lw)
FILE *fd;
cons *lw;
{
    string s = words_to_string(lw);
    fputs(s, fd);
    free(s);
}

 /* Code Generation for Distributed Memory Machines
  *
  * Reads and prints the target machine description (compatible formats)
  * Temporary version. A more general machine model should be defined by
  * Lei for complexity evaluation.
  *
  * File: model.c
  *
  * PUMA, ESPRIT contract 2701
  *
  * Francois Irigoin
  * 1991
  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "genC.h"
#include "misc.h"
#include "top-level.h"
/* #include "wp65.h" */

extern void model_fscan();

void get_model(ppn, pbn, pls)
int * ppn;
int * pbn;
int * pls;
{
    FILE * fd;

    if ((fd = fopen(MODEL_RC, "r")) == NULL) {
	if ((fd = fopen(DEFAULT_MODEL_RC, "r")) == NULL) {
	    pips_error("get_module", "no default model\n");
	}
    }

    model_fscan(fd, ppn, pbn, pls);

}

void model_fscan(fd, ppn, pbn, pls)
FILE * fd;
int * ppn;
int * pbn;
int * pls;
{
    int i1, i2, i3, i4;
    i1 = fscanf(fd, "Target Machine:\n");
    i2 = fscanf(fd, "Processor number: %d\n", ppn);
    i3 = fscanf(fd, "Memory bank number: %d\n", pbn);
    i4 = fscanf(fd, "Bank width (i.e. line size): %d\n\n", pls);
    if(i1!=0 && i2!=1 && i3!=1 && i4!=1) {
	user_error("model_fscan", "Bad format for machine model\n");
    }
}

void model_fprint(fd, pn, bn, ls)
FILE * fd;
int pn;
int bn;
int ls;
{
    (void) fprintf(fd, "Target Machine:\n");
    (void) fprintf(fd, "Processor number: %d\n", pn);
    (void) fprintf(fd, "Memory bank number: %d\n", bn);
    (void) fprintf(fd, "Bank width (i.e. line size): %d\n\n", ls);
}

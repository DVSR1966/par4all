 /* transformer package - convex hull computation
  *
  * Francois Irigoin, 21 April 1990
  */

#include <stdio.h>

#include "genC.h"
#include "ri.h"
#include "ri-util.h"

#include "misc.h"

#include "boolean.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"
#include "ray_dte.h"
#include "sommet.h"
#include "sg.h"
#include "polyedre.h"

/* temporarily, for ifdebug */
#include "transformer.h"


/* transformer transformer_exact_convex_hull(t1,t2,b) : compute convex hull for t1
 * and t2 using transformer_convex_hull; b is a pointer on a boolean : it is set to 
 * TRUE if the convex hull is exact; and to FALSE otherwise. For the moment,
 * there is no way to know if the convex hull is exact : b is always set to FALSE.
 * BA, September 16, 1993.
 */
transformer transformer_exact_convex_hull(t1, t2, b)
transformer t1;
transformer t2;
boolean *b;
{
    *b = FALSE;
    return transformer_convex_hull(t1, t2);
}


/* transformer transformer_convex_hull(t1, t2): compute convex hull for t1
 * and t2; t1 and t2 are slightly modified to give them the same basis; else
 * convex hull means nothing; some of the work is duplicated in sc_enveloppe;
 * however their "relation" fields are preserved; the whole thing is pretty
 * badly designed; shame on Francois! FI, 24 August 1990
 */
transformer transformer_convex_hull(t1, t2)
transformer t1;
transformer t2;
{
    /* return transformer_convex_hulls(t1, t2, sc_enveloppe);  */
    /* return transformer_convex_hulls(t1, t2, sc_enveloppe_chernikova); */
    return transformer_convex_hulls(t1, t2, sc_common_projection_convex_hull); 
}


transformer transformer_fast_convex_hull(t1, t2)
transformer t1;
transformer t2;
{
    return transformer_convex_hulls(t1, t2, sc_fast_convex_hull);
}


transformer transformer_chernikova_convex_hull(t1, t2)
transformer t1;
transformer t2;
{
    return transformer_convex_hulls(t1, t2, sc_enveloppe_chernikova);
}

transformer transformer_convex_hulls(t1, t2, method)
transformer t1;
transformer t2;
Psysteme (*method)();
{
    Psysteme r1;
    Psysteme r2;
    Psysteme r;
    Pbase b1;
    Pbase b2;
    Pbase b;
    transformer t = transformer_undefined;


    debug(1,"transformer_convex_hulls","begin\n");
    ifdebug(1) {
	(void) fprintf(stderr, "convex hull t1 (%x):\n", (unsigned int) t1);
	dump_transformer(t1) ;
    }
    ifdebug(1) {
	(void) fprintf(stderr, "convex hull t2 (%x):\n", (unsigned int) t2);
	dump_transformer(t2) ;
    }

    /* If one of the transformers is empty, you do not want to union
     * the arguments
     */
    if(transformer_empty_p(t1)) {
	t = transformer_dup(t2);
    }
    else if(transformer_empty_p(t2)) {
	t = transformer_dup(t1);
    }
    else {
	t = transformer_identity();
	transformer_arguments(t) = 
	    arguments_union(transformer_arguments(t1),
			    transformer_arguments(t2));

	/* get relation fields */
	r1 = (Psysteme) predicate_system(transformer_relation(t1));
	r2 = (Psysteme) predicate_system(transformer_relation(t2));

	/* update bases using their "union"; convex hull has to be computed 
	   relatively to ONE space */
	b1 = r1->base;
	b2 = r2->base;
	b = base_union(b1, b2);
	base_rm((Pvecteur) b1);
	base_rm((Pvecteur) b2);
	/* b is duplicated because it may be later freed by (*method)()
	 * FI->CA: To be changed when (*method)() is cleaned up
	 */
	sc_base(r1) = base_dup(b);
	/* please, no sharing between Psysteme's */
	sc_base(r2) = base_dup(b);
	sc_dimension(r1) = base_dimension(b);
	sc_dimension(r2) = sc_dimension(r1);

	/* meet operation */
	r = (* method)(r1, r2);
	if(SC_EMPTY_P(r)) {
	    /* FI: this could be eliminated if SC_EMPTY was really usable; 27/5/93 */
	    /* and replaced by a SC_UNDEFINED_P() and pips_error() */
	    r = sc_empty(b);
	}
	else {
	    base_rm(b);
	    b = BASE_NULLE;
	}

	predicate_system(transformer_relation(t)) = (char *) r;

    }
    ifdebug(1) {
	(void) fprintf(stderr, "convex hull, t (%x):\n", (unsigned int) t);
	dump_transformer(t) ;
    }

    debug(1,"transformer_convex_hulls","end\n");

    return t;
}

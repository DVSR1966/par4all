/* $RCSfile: feasability.c,v $ (version $Revision$)
 * $Date: 1996/08/09 12:50:59 $, 
 */

#include <stdio.h>
#include <malloc.h>

#include "boolean.h"
#include "assert.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "sc.h"

static void
test_system(Psysteme sc)
{
    CATCH(overflow_error) 
	fprintf(stdout, "*** Arithmetic error occured in simplex\n");
    TRY
	if (sc_feasibility_ofl_ctrl(sc, FALSE, OFL_CTRL, TRUE))
	    printf("Systeme faisable (soluble) en rationnels\n") ;
	else
	    printf("Systeme insoluble\n");
}

static void 
test_file(FILE * f, char * name)
{
    Psysteme sc=sc_new(); 
    printf("systeme initial \n");
    if(sc_fscan(f,&sc)) 
    {
	printf("syntaxe correcte dans %s\n",name);
	sc_fprint(stdout, sc, *variable_default_name);
	printf("Nb_eq %d , Nb_ineq %d, dimension %d\n",
	       sc->nb_eq, sc->nb_ineq, sc->dimension) ;
	test_system(sc);
    }
    else
    {
	fprintf(stderr,"erreur syntaxe dans %s\n",name);
	exit(1);
    }
}

int 
main(int argc, char *argv[])
{
    /*  Programme de test de faisabilite'
     *  d'un ensemble d'equations et d'inequations.
     */
    FILE * f1;
    int i; /* compte les systemes, chacun dans un fichier */

    initialize_sc(variable_default_name);
    
    /* lecture et test de la faisabilite' de systemes sur fichiers */

    if(argc>=2) 
    {
	for(i=1;i<argc;i++)
	{
	    if((f1 = fopen(argv[i],"r")) == NULL) {
		fprintf(stdout,"Ouverture fichier %s impossible\n", argv[i]);
		exit(4);
	    }
	    test_file(f1, argv[i]);
	    fclose(f1) ;
	}
    }
    else 
    {
	test_file(stdin, "standard input");
    }
    exit(0) ;
}


#include <stdio.h>
#include <setjmp.h>

#include "genC.h"
#include "list.h"
#include "ri.h"
#include "constants.h"
#include "ri-util.h"
#include "misc.h"

/* C3 includes          */
#include "boolean.h"
#include "arithmetique.h"
#include "vecteur.h"
#include "contrainte.h"
#include "ray_dte.h"
#include "sommet.h"
#include "sg.h"
#include "sc.h"
#include "polyedre.h"
#include "union.h"
#include "matrix.h"


/* Pips includes        */

#include "complexity_ri.h"
#include "database.h"
#include "dg.h"
#include "makefile.h"
#include "parser_private.h"
#include "property.h"
#include "reduction.h"
#include "tiling.h"
#include "loop_normalize.h"


#include "text.h"
#include "text-util.h"
#include "graph.h"
#include "paf_ri.h"
#include "paf-util.h"
#include "mapping.h"
#include "pipsdbm.h"
#include "resources.h"
#include "scheduling.h"
#include "array_dfg.h"

/* Local defines */
typedef dfg_arc_label arc_label;
typedef dfg_vertex_label vertex_label;

extern char *strdup();
/*extern int fprintf();
extern int fclose();
extern int fflush();
extern int printf();*/
extern sccs dfg_find_sccs();
extern bdt search_graph_bdt();

jmp_buf overflow_error;
jmp_buf pips_top_level;

/*==================================================================*/
/* void print_bdt(module_name): print the bdt of module name
 *
 * AC 94/03/30
 */

void print_bdt(module_name)

 char     *module_name;
{
 FILE     *fd;
 char     *filename;
 bdt      b;

 debug_on( "PRINT_BDT_DEBUG_LEVEL" );

 if (get_debug_level() > 1)
     user_log("\n\n *** PRINTING BDT for %s\n", module_name);

 b = (bdt) db_get_memory_resource(DBR_BDT, module_name, TRUE);
 filename = strdup(concatenate(db_get_current_program_directory(),
                    "/", module_name, ".bdt_file", NULL));

 fd = safe_fopen(filename, "w");
 fprint_bdt(fd, b);
 safe_fclose(fd, filename);

 DB_PUT_FILE_RESOURCE(DBR_BDT_FILE, strdup(module_name), filename);

 if (get_debug_level() > 0)
     fprintf(stderr, "\n\n *** PRINT_BDT DONE\n");

 debug_off();
}

/*==================================================================*/
/* void scheduling(mod_name ): this is the main function to calculate
 * the schedules of the node of a dfg. It first reverse the graph to
 * have each node in function of its predecessors, then calculates
 * the strongly connected components by the Trajan algorithm, then
 * calls the function that really find the schedules.
 *
 * AC 93/10/30
 */

void scheduling(mod_name)
char            *mod_name;
{
  graph           dfg, rdfg;
  sccs            rgraph;
  bdt             bdt_list;
  entity          ent;
  statement       mod_stat;
  static_control  stco;
  statement_mapping STS;
  
  debug_on("SCHEDULING_DEBUG_LEVEL");
  if (get_debug_level() > 0)
  {
    fprintf(stderr,"\n\nBegin scheduling\n");
    fprintf(stderr,"DEBUT DU PROGRAMME\n");
    fprintf(stderr,"==================\n\n");
  }
  
  /* We get the required data: module entity, code, static_control, dataflow
   * graph, timing function.
   */
  ent = local_name_to_top_level_entity(mod_name);
  
  set_current_module_entity(ent);
  
  mod_stat = (statement)db_get_memory_resource(DBR_CODE, mod_name, TRUE);
  STS = (statement_mapping) db_get_memory_resource(DBR_STATIC_CONTROL,
					    mod_name, TRUE);
  stco = (static_control) GET_STATEMENT_MAPPING(STS, mod_stat);
  
  set_current_stco_map(STS);

  if (stco == static_control_undefined) 
    pips_error("distribution", "This is an undefined static control !\n");
  
  if (!static_control_yes(stco)) 
    pips_error("distribution", "This is not a static control program !\n");
  
  /* Read the DFG */
  dfg = (graph)db_get_memory_resource(DBR_ADFG, mod_name, TRUE);
  dfg = adg_pure_dfg(dfg);
  rdfg = dfg_reverse_graph(dfg);
  if (get_debug_level() > 5) fprint_dfg(stderr, rdfg);
  rgraph = dfg_find_sccs(rdfg);
  
  if (get_debug_level() > 0) fprint_sccs(stderr, rgraph);
  
  bdt_list = search_graph_bdt(rgraph);
  
  if (get_debug_level() > 0)
  {
    fprintf(stderr,"\n==============================================");
    fprintf(stderr,"\nBase de temps trouvee :\n");
    fprint_bdt_with_stat(stderr, bdt_list);
    fprintf(stderr,"\nEnd of scheduling\n");
  }
  
  DB_PUT_MEMORY_RESOURCE(DBR_BDT, strdup(mod_name), bdt_list);
  
  reset_current_stco_map();
  reset_current_module_entity();

  debug_off();
}

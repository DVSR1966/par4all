/* Copyright 2007, 2008 Alain Muller, Frederique Silber-Chaussumier

This file is part of STEP.

The program is distributed under the terms of the GNU General Public
License.
*/

#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include "defines-local.h"


directive make_directive_omp_critical(statement stmt)
{
  return make_directive(strdup(statement_to_directive_txt(stmt)),step_make_new_directive_module_name(SUFFIX_OMP_CRITICAL,""),make_type_directive_omp_critical(),NIL,NIL);
} 

bool begin_directive_omp_critical(directive __attribute__ ((unused)) current,directive next)
{
  bool rep=type_directive_omp_critical_p(directive_type(next));
  pips_debug(2,"%d\n",rep);
  return rep;
}

directive make_directive_omp_end_critical(statement stmt)
{
  return make_directive(strdup(statement_to_directive_txt(stmt)),step_make_new_directive_module_name(SUFFIX_OMP_END_CRITICAL,""),make_type_directive_omp_end_critical(),NIL,NIL);
}

bool end_directive_omp_end_critical(directive current,directive next)
{
  bool rep= type_directive_omp_end_critical_p(directive_type(next))
    &&   type_directive_omp_critical_p(directive_type(current));
  pips_debug(2,"%d\n",rep);
  return rep;
}

instruction handle_omp_critical(directive begin,directive end)
{
  count_critical++;
  pips_debug(1,"outlining\n");
  statement call;
  instruction i;
  entity directive_module=outlining_start(directive_module_name(begin));
  outlining_scan_block(directive_body(begin));
  call=outlining_close(step_directives_USER_FILE_name());//généerr le fichier source avec les calls 
  if(statement_comments(call)!=empty_comments)
    {
      free(statement_comments(call));
      statement_comments(call)=empty_comments;
    }
  statement_label(call)=entity_empty_label();
  
  call=step_keep_directive_txt(begin,call,end);

  i=statement_instruction(call);
  statement_instruction(call)=instruction_undefined;
  free_statement(call);
  store_global_directives(directive_module,begin);
  free_directive(end);
  return i;
}


string directive_omp_critical_to_string(directive d,bool close)
{
  pips_debug(1, "d=%p, close=%u\n",d,close);
  if (close)
    return strdup(END_CRITICAL_TXT);
  else
    return strdup(CRITICAL_TXT);
}
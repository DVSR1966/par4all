/* Copyright 2007, 2008 Alain Muller, Frederique Silber-Chaussumier

This file is part of STEP.

The program is distributed under the terms of the GNU General Public
License.
*/
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif

#include "defines-local.h"

static list current_private_entity = list_undefined;

static list get_private_clause(entity module)
{
  pips_assert("directive module", bound_global_directives_p(module));
  directive d = load_global_directives(module);
  list privates = list_undefined;

  // recherche de la clause private dans la listes des clauses de la directive
  FOREACH(CLAUSE,c,directive_clauses(d))
    {
      if (clause_private_p(c))
	{
	  pips_assert("only one clause private per directive", list_undefined_p(privates));
	  privates = clause_private(c);
	}
    }
  
  if(list_undefined_p(privates))
    privates = NIL;
  
  return privates;
}

void step_private_before(entity directive_module)
{
  pips_assert("current_private_entity not undefined", list_undefined_p(current_private_entity));
  current_private_entity = get_private_clause(directive_module);
}

/* FSC: A COMMENTER */
void step_private_after()
{
  pips_assert("current_private_entity undefined", !list_undefined_p(current_private_entity));
  current_private_entity = list_undefined;
}

/* AM: fonctions permettant de verifier si un tableau a ete declare prive
   par une clause openMP private utiliser dans :
   - compile_share.c : en openMP, les donnees non private sont par defaut share 
     (et necessite une initialisation au niveau de la RT)
   - compile_do.c : on ne genere pas de communications pour une donnees declarees private
 */
bool step_private_p(entity e)
{
  pips_assert("current_private_entity undefined", !list_undefined_p(current_private_entity));
  return gen_in_list_p(e, current_private_entity);
}

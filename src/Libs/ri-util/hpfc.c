/* $RCSfile: hpfc.c,v $ (version $Revision$)
 * $Date: 1997/01/18 16:19:06 $, 
 *
 * functions to test hpfc-related special entities. needed by syntax.
 * moved here from hpfc to break a cyclic dependence hpfc -> syntax.
 * done on 22/09/95 by FC. Hpfc and sytax *must* be independent.
 * However I am not so happy about putting hpfc code here, 
 * outside of home:-)
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "ri.h"

#include "misc.h"
#include "properties.h"
#include "ri-util.h"

/* recognize an hpf directive special entity.
 * (the prefix of which is HPF_PREFIX, as a convention)
 * both functions are available, based on the name and on the entity.
 */
bool hpf_directive_string_p(string s)
{
    return strncmp(HPF_PREFIX, s, strlen(HPF_PREFIX))==0;
}

bool hpf_directive_entity_p(entity e)
{
    return top_level_entity_p(e) && 
	hpf_directive_string_p(entity_local_name(e));
}

bool realign_directive_p(entity f)
{
    return top_level_entity_p(f) && 
	same_string_p(HPF_PREFIX REALIGN_SUFFIX, entity_local_name(f));
}

bool redistribute_directive_p(entity f)
{
    return top_level_entity_p(f) && 
	same_string_p(HPF_PREFIX REDISTRIBUTE_SUFFIX, entity_local_name(f));
}

bool dead_fcd_directive_p(entity f)
{
    return top_level_entity_p(f) && 
	same_string_p(HPF_PREFIX DEAD_SUFFIX, entity_local_name(f));
}

bool fcd_directive_string_p(string s)
{
    return same_string_p(s, HPF_PREFIX SYNCHRO_SUFFIX) ||
	   same_string_p(s, HPF_PREFIX TIMEON_SUFFIX) ||
	   same_string_p(s, HPF_PREFIX TIMEOFF_SUFFIX) ||
	   same_string_p(s, HPF_PREFIX TELL_SUFFIX) ||
	   same_string_p(s, HPF_PREFIX HOSTSECTION_SUFFIX) ||
	   same_string_p(s, HPF_PREFIX DEAD_SUFFIX);
}

bool fcd_directive_p(entity f)
{
    return top_level_entity_p(f) &&
	fcd_directive_string_p(entity_local_name(f));
}

/* whether an entity must be kept in the code.
 * if so, a maybe fake source code must be supplied, 
 * and the directive will be kept in the callee list.
 * not kept if some property tells not to...
 */
bool keep_directive_in_code_p(string s)
{
    return fcd_directive_string_p(s) &&
	!(same_string_p(s, HPF_PREFIX SYNCHRO_SUFFIX) && 
	  get_bool_property(FCD_IGNORE_PREFIX "SYNCHRO")) &&
	!(same_string_p(s, HPF_PREFIX TIMEON_SUFFIX) &&
	  get_bool_property(FCD_IGNORE_PREFIX "TIME")) &&
	!(same_string_p(s, HPF_PREFIX TIMEOFF_SUFFIX) &&
	  get_bool_property(FCD_IGNORE_PREFIX "TIME")) ; 
	/* !(same_string_p(s, HPF_PREFIX TELL_SUFFIX) &&
	  get_bool_property(FCD_IGNORE_PREFIX "TELL")) */
}

/* that is all
 */

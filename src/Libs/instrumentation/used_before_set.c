#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "database.h"
#include "pipsdbm.h"
#include "resources.h"
#include "misc.h"
#include "control.h"
#include "properties.h"
#include "semantics.h"
#include "instrumentation.h"
#include "transformations.h"

bool used_before_set(char *module_name)
{ 
  return TRUE;
}

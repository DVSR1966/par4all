#include "property.h"

#define ONE_TRIP_DO "ONE_TRIP_DO"

#define PRETTYPRINT_TRANSFORMER "PRETTYPRINT_TRANSFORMER"
#define PRETTYPRINT_EXECUTION_CONTEXT "PRETTYPRINT_EXECUTION_CONTEXT"
#define PRETTYPRINT_EFFECTS "PRETTYPRINT_EFFECTS"
#define PRETTYPRINT_PARALLEL "PRETTYPRINT_PARALLEL"
#define PRETTYPRINT_REVERSE_DOALL "PRETTYPRINT_REVERSE_DOALL"
#define PRETTYPRINT_REGION "PRETTYPRINT_REGION"

#define SEMANTICS_FLOW_SENSITIVE "SEMANTICS_FLOW_SENSITIVE"
#define SEMANTICS_INTERPROCEDURAL "SEMANTICS_INTERPROCEDURAL"
#define SEMANTICS_INEQUALITY_INVARIANT "SEMANTICS_INEQUALITY_INVARIANT"
#define SEMANTICS_FIX_POINT "SEMANTICS_FIX_POINT"
#define SEMANTICS_DEBUG_LEVEL "SEMANTICS_DEBUG_LEVEL"
#define SEMANTICS_STDOUT "SEMANTICS_STDOUT"

#define PARALLELIZE_USE_EXECUTION_CONTEXT "PARALLELIZE_USE_EXECUTION_CONTEXT"

#define DEPENDENCE_TEST "DEPENDENCE_TEST"
#define RICEDG_PROVIDE_STATISTICS "RICEDG_PROVIDE_STATISTICS"

/* for upwards compatibility with Francois's modified version */
#define pips_flag_p(p) get_bool_property(p)
#define pips_flag_set(p) set_bool_property((p), TRUE)
#define pips_flag_reset(p) set_bool_property((p), FALSE)
#define pips_flag_fprint(fd) fprint_properties(fd)

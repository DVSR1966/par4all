/* $Id$
 */

#include "ri.h"
#include "ri-util.h"
#include "misc.h"

/* some useful SHORTHANDS for EFFECT:
 */
#define effect_entity(e) reference_variable(effect_reference(e))
#define effect_action_tag(eff) action_tag(effect_action(eff))
#define effect_approximation_tag(eff) approximation_tag(effect_approximation(eff))

#define effect_scalar_p(eff) entity_scalar_p(effect_entity(eff))
#define effect_read_p(eff) (action_tag(effect_action(eff))==is_action_read)
#define effect_write_p(eff) (action_tag(effect_action(eff))==is_action_write)
#define effect_may_p(eff) \
        (approximation_tag(effect_approximation(eff)) == is_approximation_may)
#define effect_must_p(eff) \
        (approximation_tag(effect_approximation(eff)) == is_approximation_must)
#define effect_exact_p(eff) \
        (approximation_tag(effect_approximation(eff)) == is_approximation_exact)

/* some string constants 
 */
#define ACTION_UNDEFINED 	string_undefined
#define ACTION_READ 		"R"
#define ACTION_WRITE 		"W"
#define ACTION_IN    		"IN"
#define ACTION_OUT		"OUT"
#define ACTION_COPYIN		"COPYIN"
#define ACTION_COPYOUT		"COPYOUT"
#define ACTION_PRIVATE		"PRIVATE"

/* GENERIC FUNCTIONS on lists of effects to be instanciated for specific 
   types of effects */

/* initialisation and finalization */
void (*effects_computation_init_func)(string /* module_name */);
void (*effects_computation_reset_func)(string /* module_name */);

/* dup and free - This should be handled by newgen, but there is a problem
 * with the persistency of references - I do not understand what happens. */
effect (*effect_dup_func)(effect eff);
void (*effect_free_func)(effect eff);

/* make functions for effects */
effect (*reference_to_effect_func)(reference, action);

/* union */
effect (*effect_union_op)(effect, effect);
list (*effects_union_op)(
    list, list, bool (*eff1_eff2_combinable_p)(effect, effect));
list (*effects_test_union_op)(
    list, list, bool (*eff1_eff2_combinable_p)(effect, effect));

/* intersection */
list (*effects_intersection_op)(
    list, list, bool (*eff1_eff2_combinable_p)(effect, effect));

/* difference */
list (*effects_sup_difference_op)(
    list, list, bool (*eff1_eff2_combinable_p)(effect, effect));
list (*effects_inf_difference_op)(
    list, list, bool (*eff1_eff2_combinable_p)(effect, effect));

/* composition with transformers */
list (*effects_transformer_composition_op)(list, transformer);
list (*effects_transformer_inverse_composition_op)(list, transformer);

/* composition with preconditions */
list (*effects_precondition_composition_op)(list,transformer);

/* union over a range */
list (*effects_descriptors_variable_change_func)(list, entity, entity);
descriptor (*loop_descriptor_make_func)(loop);
list (*effects_loop_normalize_func)(
    list /* of effects */, entity /* index */, range,
    entity* /* new loop index */, descriptor /* range descriptor */,
    bool /* normalize descriptor ? */);
list (*effects_union_over_range_op)(list, entity, range, descriptor);
descriptor (*vector_to_descriptor_func)(Pvecteur);

/* interprocedural translation */
list (*effects_backward_translation_op)(entity, list, list, transformer);
list (*effects_forward_translation_op)(entity /* callee */, list /* args */,
				       list /* effects */,
				       transformer /* context */);

/* local to global name space translation */
list (*effects_local_to_global_translation_op)(list);



/* functions to provide context and transformer information */
transformer (*load_context_func)(statement);
transformer (*load_transformer_func)(statement);

bool (*empty_context_test)(transformer);

/* proper to contracted proper effects or to summary effects functions */
effect (*proper_to_summary_effect_func)(effect);

/* normalization of descriptors */
void (*effects_descriptor_normalize_func)(list /* of effects */);

/* getting/putting resources from/to pipsdbm */
statement_effects (*db_get_proper_rw_effects_func)(char *);
void (*db_put_proper_rw_effects_func)(char *, statement_effects);

statement_effects (*db_get_invariant_rw_effects_func)(char *);
void (*db_put_invariant_rw_effects_func)(char *, statement_effects);

statement_effects (*db_get_rw_effects_func)(char *);
void (*db_put_rw_effects_func)(char *, statement_effects);

list (*db_get_summary_rw_effects_func)(char *);
void (*db_put_summary_rw_effects_func)(char *, list);

statement_effects (*db_get_in_effects_func)(char *);
void (*db_put_in_effects_func)(char *, statement_effects);

statement_effects (*db_get_cumulated_in_effects_func)(char *);
void (*db_put_cumulated_in_effects_func)(char *, statement_effects);

statement_effects (*db_get_invariant_in_effects_func)(char *);
void (*db_put_invariant_in_effects_func)(char *, statement_effects);

list (*db_get_summary_in_effects_func)(char *);
void (*db_put_summary_in_effects_func)(char *, list);

list (*db_get_summary_out_effects_func)(char *);
void (*db_put_summary_out_effects_func)(char *, list);

statement_effects  (*db_get_out_effects_func)(char *);
void (*db_put_out_effects_func)(char *, statement_effects);


/* prettyprint function types:
 */

typedef text (*generic_text_function)(list /* of effect */);
typedef void (*generic_prettyprint_function)(list /* of effect */);
typedef void (*generic_attachment_function)(text);


/* For COMPATIBILITY purpose only - DO NOT USE anymore
 */
#define effect_variable(e) reference_variable(effect_reference(e))

/*
 * CAUTION! 3 NEXTS ARE OBSOLETE! just kept for the old engine!
 */
/* prettyprint function for debug */
void (*effects_prettyprint_func)(list);

/* prettyprint function for sequential and user views */
text (*effects_to_text_func)(list);
void (*attach_effects_decoration_to_text_func)(text);


/* end of effects-generic-local.h
 */

#include <stdio.h>

#include "genC.h"
#include "ri.h"

#include "misc.h"
#include "ri-util.h"
#include "control.h"

static void rinitialize_ordering_to_statement(hash_table ots, statement s);

/* a hash table to map orderings (integer) to statements (pointers)
 * assumed to be valid for the current module returned by
 * get_current_module_entity(). This is assumed to hold when
 * pipsmake issues a request to lower level libraries.
 *
 * db_get_current_module() returns the module used in the request
 * to pipsmake and is usually different.
 */
static hash_table OrderingToStatement = (hash_table) NULL;



void initialize_ordering_to_statement(s)
statement s;
{
    if (OrderingToStatement != (hash_table) NULL) {
	reset_ordering_to_statement(OrderingToStatement);
    }

    OrderingToStatement = set_ordering_to_statement(s);
}

statement ordering_to_statement(o)
int o;
{
    return apply_ordering_to_statement(OrderingToStatement, o);
}



hash_table set_ordering_to_statement(s)
statement s;
{
    hash_table ots =  hash_table_make(hash_int, 101);

    rinitialize_ordering_to_statement(ots, s);

    return ots;
}

void reset_ordering_to_statement(ots)
hash_table ots;
{
    if (ots != (hash_table) NULL) {
	hash_table_clear(ots);
    }
    else {
	pips_error("reset_ordering_to_statement", "ill. NULL arg.\n");
    }
}

static void rinitialize_ordering_to_statement(ots, s)
hash_table ots;
statement s;
{
    instruction i = statement_instruction(s);

    if (statement_ordering(s) != STATEMENT_ORDERING_UNDEFINED)
	hash_put(ots,  
		 (char *) statement_ordering(s), (char *) s);

    switch (instruction_tag(i)) {

      case is_instruction_block:
	MAPL(ps, {
	    rinitialize_ordering_to_statement(ots, STATEMENT(CAR(ps)));
	}, instruction_block(i));
	break;

      case is_instruction_loop:
	rinitialize_ordering_to_statement(ots, loop_body(instruction_loop(i)));
	break;

      case is_instruction_test:
 	rinitialize_ordering_to_statement(ots, test_true(instruction_test(i)));
	rinitialize_ordering_to_statement(ots, test_false(instruction_test(i)));
	break;

      case is_instruction_call:
      case is_instruction_goto:
	break;

      case is_instruction_unstructured: {
	  cons *blocs = NIL ;

	  CONTROL_MAP(c, {
	      rinitialize_ordering_to_statement(ots, control_statement(c));
	  }, unstructured_control(instruction_unstructured(i)), blocs);
	  gen_free_list( blocs );

	  break;
      }
	    
      default:
	pips_error("rinitialize_ordering_to_statement", "bad tag\n");
    }
}

statement apply_ordering_to_statement(ots, o)
hash_table ots;
int o;
{
    statement s;
    s = (statement) hash_get(ots, (char *) o);
    if(s == statement_undefined) pips_error("ordering_to_statement",
					    "no statement for order %d\n",o);
    return s;
}

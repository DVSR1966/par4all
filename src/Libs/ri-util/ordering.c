#include <stdio.h>

#include "genC.h"
#include "ri.h"

#include "misc.h"
#include "ri-util.h"
#include "control.h"

/* a hash table to map orderings (integer) to statements (pointers)
 * assumed to be valid for the current module returned by
 * get_current_module_entity(). This is assumed to hold when
 * pipsmake issues a request to lower level libraries.
 *
 * db_get_current_module() returns the module used in the request
 * to pipsmake and is usually different.
 */
static hash_table OrderingToStatement = hash_table_undefined;

/* It would be possible to expose a lower level interface to manage
 * several ordering_to_statement hash tables
 */

static void rinitialize_ordering_to_statement(hash_table ots, statement s);

static statement apply_ordering_to_statement(hash_table ots, int o);

static hash_table set_ordering_to_statement(statement s);



bool ordering_to_statement_initialized_p()
{
    return OrderingToStatement != hash_table_undefined;
}

void initialize_ordering_to_statement(s)
statement s;
{
    /* FI: I do not like that automatic cleaning any more... */
    if (OrderingToStatement != hash_table_undefined) {
	reset_ordering_to_statement();
    }

    OrderingToStatement = set_ordering_to_statement(s);
}

statement ordering_to_statement(o)
int o;
{
    statement s = statement_undefined;

    s = apply_ordering_to_statement(OrderingToStatement, o);

    return s;
}



static hash_table set_ordering_to_statement(s)
statement s;
{
    hash_table ots =  hash_table_make(hash_int, 101);

    rinitialize_ordering_to_statement(ots, s);

    return ots;
}

void reset_ordering_to_statement()
{
    if (OrderingToStatement != hash_table_undefined) {
	hash_table_clear(OrderingToStatement);
	OrderingToStatement = hash_table_undefined;
    }
    else {
	pips_error("reset_ordering_to_statement", "ill. undefined arg.\n");
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

static statement apply_ordering_to_statement(ots, o)
hash_table ots;
int o;
{
    statement s;

    pips_assert("apply_ordering_to_statement", 
		ots != NULL && ots != hash_table_undefined);

    s = (statement) hash_get(ots, (char *) o);

    if(s == statement_undefined) {
	pips_error("ordering_to_statement",
		   "no statement for order %d=(%d,%d)\n",
		   o, ORDERING_NUMBER(o), ORDERING_STATEMENT(o));
    }
    return s;
}

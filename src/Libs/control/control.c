
/* - control.c

   Computes the Structured Control Graph of a given statement. 

   WARNINGS: 

   . Temporary locations malloced while recursing in the process are
     not freed (to be done latter ... if required)
   . The desugaring of DO loops is not perfect (in case of side-effects
     inside loop ranges.

   Pierre Jouvelot (27/5/89) <- this is a French date:-)

   MODIFICATIONS:

   . hash_get interface modification: in one hash table an undefined key
   meant an error; in another one an undefined key was associated to
   the default value empty_list; this worked as long as NULL was returned
   as NOT_FOUND value (i.e. HASH_UNDEFINED_VALUE); this would work again
   if HASH_UNDEFINED_VALUE can be user definable; Francois Irigoin, 7 Sept. 90

*/

#include <stdio.h>
#include <strings.h>

#include "genC.h"
#include "ri.h"
#include "text.h"
#include "text-util.h"
#include "properties.h"

#include "misc.h"
#include "ri-util.h"
#include "control.h"

#include "constants.h"

#define LABEL_TABLES_SIZE 10
 
/* UNREACHABLE is the hook used as a predecessor of statements that are  
   following a goto. The list of unreachable statements is kept in the
   successors list. */

static control Unreachable;		

/* LABEL_STATEMENTS maps label names to the list of statements where
   they appear (either as definition or reference). */

static hash_table Label_statements;	

/* LABEL_CONTROL maps label names to their (possible forward) control
   nodes. */

static hash_table Label_control;	


#define MAKE_CONTINUE_STATEMENT() make_continue_statement(entity_undefined) 

/* HASH_GET_DEFAULT_EMPTY_LIST: returns an empty list instead of
   HASH_UNDEFINED_VALUE when a key is not found */

static list hash_get_default_empty_list(h, k)
hash_table h;
char * k;
{
    list l = (list) hash_get(h, k);

    return (l == (list) HASH_UNDEFINED_VALUE)? NIL : l;
}

/* REMV removes a (unique) control X from the list L. */

static cons * remv(x, l)
control x;
cons *l;
{
    cons *pred = NIL;
    cons *elts = l;

    for(; !ENDP(elts); pred = elts, elts = CDR(elts)) {
	if(CONTROL(CAR(elts)) == x) {
	    if(ENDP(pred)) {
		return(CDR(elts));
	    }
	    CDR(pred) = CDR(elts);
	    return(l);
	}
    }
    return(l);
}

/* PUSHNEW pushes a control X on the list L if it's not here. */

static cons * pushnew(x, l)
control x;
cons *l;
{
    cons *ll = l;

    MAPL(elts, {if(CONTROL(CAR(elts)) == x) return(l);},
	  ll)
    return(CONS(CONTROL, x, l));
}

#define ADD_PRED(pred,c) (pushnew(pred,control_predecessors(c)))
#define ADD_SUCC(succ,c) (CONS(CONTROL, succ, NIL))
#define UPDATE_CONTROL(c,s,pd,sc) { \
	control_statement(c)=s; \
	MAPL(preds, {control_predecessors(c) = \
			      ADD_PRED(CONTROL(CAR(preds)), c);}, \
	      pd); \
	control_successors(c)=sc; \
	}

/* PATCH_REFERENCES replaces all occurrences of FNODE by TNODE in the
   predecessors or successors lists of its predecessors
   or successors list (according to HOW). */

#define PREDS_OF_SUCCS 1
#define SUCCS_OF_PREDS 2

static void patch_references(how, fnode, tnode)
int how;
control fnode, tnode;
{
    MAPL(preds, {
	control pred = CONTROL(CAR(preds));
	
	MAPL(succs, {
	    if(CONTROL(CAR(succs)) == fnode)
		    CONTROL(CAR(succs)) = tnode;
	}, (how == SUCCS_OF_PREDS) ? 
	     control_successors(pred) :
	     control_predecessors(pred));
    }, (how == SUCCS_OF_PREDS) ? 
	 control_predecessors(fnode) : 
	 control_successors(fnode));
}

/* MAKE_CONDITIONAL_CONTROL is make_control except when the statement ST
   has a label and is thus already in Label_control. */

static control make_conditional_control(st) 
statement st;
{
    string label = entity_name(statement_label(st));

    if(empty_label_p(label)) {
	return(make_control(st, NIL, NIL));
    }
    else {
	return((control)hash_get_default_empty_list(Label_control, label));
    }
}

/* GET_LABEL_CONTROL returns the control node corresponding to a
   useful label NAME in the Label_control table. */

static control get_label_control(name)
string name;
{
    control c;

    pips_assert("get_label_control", !empty_label_p(name)) ;
    c = (control)hash_get(Label_control, name);
    pips_assert("get_label_control", c != (control) HASH_UNDEFINED_VALUE);
    return(c);
}

/* UPDATE_USED_LABELS adds the reference to the label NAME in the
   statement ST. A used_label is a hash_table that maps the label
   name to the list of statements that references it. */

static void update_used_labels(used_labels, name, st)
hash_table used_labels;
string name;
statement st;
{
    cons *sts ;

    if( !empty_label_p(name) ) {
	sts = hash_get_default_empty_list(used_labels, name) ;
	hash_put(used_labels, name, (char*) CONS(STATEMENT, st, sts)) ;
	debug(5, "update_used_labels", "Reference to statement %d seen\n", 
	      statement_number( st )) ;
    }
}

/* UNION_USED_LABELS unions the used-labels list L1 and L2 and returns the
   result in L1 */

static hash_table union_used_labels(l1, l2)
hash_table l1, l2;
{
    HASH_MAP(name, sts, {
	MAPL(stts, {
	    update_used_labels(l1, name, STATEMENT(CAR(stts)));
	}, (cons *)sts);
    }, l2);
    return( l1 ) ;
}

/* COVERS_LABELS_P returns whether a USED_LABELS list for statement ST
   covers all the references to its labels. */

static bool covers_labels_p(st,used_labels)
statement st ;
hash_table used_labels;
{
    if( get_debug_level() >= 5 ) {
	fprintf(stderr, "Statement %d: \n ", statement_number( st )) ;
	print_text(stderr, text_statement(entity_undefined,0, st)) ;
    }
    HASH_MAP(name, sts, {
	cons *stats = (cons *)sts;
	
	MAPL(defs, {
	    bool found = FALSE;
	    statement def = STATEMENT(CAR(defs));

	    MAPL(sts, {
		found |= (STATEMENT(CAR(sts))==def);
	    }, stats);

	    if(!found) {
		if( get_debug_level() >= 5 ) {
		    fprintf( stderr, "doesn't cover label %s\n", name ) ;
		}
		return(FALSE);
	    }
	}, (cons *)hash_get_default_empty_list(Label_statements, name));
    }, used_labels);

    if( get_debug_level() >= 5 ) {
	fprintf( stderr, "covers its label usage\n" ) ;
    }
    return(TRUE);
}

/* CONTROLIZE computes in C_RES the control node of the statement ST
   whose predecessor control node is PRED and successor SUCC. The
   USED_LABELS is modified to deal with local use of labels. Returns TRUE
   if the current statement isn't a structured control. The invariant is
   that CONTROLIZE links predecessors and successors of C_RES, updates the
   successors of PRED and the predecessors of SUCC. */

bool controlize(st, pred, succ, c_res, used_labels)
statement st;
control pred, succ;
control c_res;
hash_table used_labels;
{
    instruction i = statement_instruction(st);
    string label = entity_name(statement_label(st));
    bool controlize_list(), controlize_test(), controlize_loop(),
         controlize_call();
    bool controlized=FALSE;

    switch(instruction_tag(i)) {
    case is_instruction_block: {
	controlized = controlize_list(st, instruction_block(i),
				      pred, succ, c_res, used_labels);
	break;
    }
    case is_instruction_test:
	controlized = controlize_test(st, instruction_test(i), 
				      pred, succ, c_res, used_labels);
	break;
    case is_instruction_loop:
	controlized = controlize_loop(st, instruction_loop(i),
				      pred, succ, c_res, used_labels);
	break;
    case is_instruction_goto: {
	string name = entity_name(statement_label(instruction_goto(i)));
        statement nop = make_continue_statement(statement_label(st));

        statement_number(nop) = statement_number(st);
        statement_comments(nop) = statement_comments(st);
	succ = get_label_control(name);
	control_successors(pred) = ADD_SUCC(c_res, pred);
	UPDATE_CONTROL(c_res, nop, 
		       CONS(CONTROL, pred, NIL), 
		       ADD_SUCC(succ, c_res )) ;
	control_predecessors(succ) = ADD_PRED(c_res, succ);
	update_used_labels(used_labels, name, st);
	controlized = TRUE;
	break;
    }
    case is_instruction_call:
	controlized = controlize_call(st, instruction_call(i), 
				       pred, succ, c_res, used_labels);
	break;
    default:
	pips_error("controlize", 
		    "Unknown instruction tag %d\n", instruction_tag(i));
    }
    update_used_labels(used_labels, label, st);
    return(controlized);
}
	
/* CONTROLIZE_CALL controlizes the call C of statement ST in C_RES. The deal
   is to correctly manage STOP; since we don't know how to do it, so we
   assume this is a usual call !! */

bool controlize_call(st, c, pred, succ, c_res, used_labels)
statement st;
call c;
control pred, succ;
control c_res;
hash_table used_labels;
{
    bool stop = 
	    (strcmp(entity_local_name(call_function(c)), "STOP") == 0);

    stop = FALSE ;

    UPDATE_CONTROL(c_res, st,
		    ADD_PRED(pred, c_res), 
		    (stop ? NIL : CONS(CONTROL, succ, NIL)));
    control_successors(pred) = ADD_SUCC(c_res, pred);
   
    if( !stop ) {
	control_predecessors(succ) = ADD_PRED(c_res, succ);
    }
    return(FALSE);
}

/* LOOP_HEADER, LOOP_TEST and LOOP_INC build the desugaring phases of a
   do-loop L for the loop header (i=1), the test (i<10) and the increment
   (i=i+1). */

statement loop_header(l) 
loop l;
{
    expression i = make_entity_expression(loop_index(l), NIL);

    return(make_assign_statement(i, range_lower(loop_range(l))));
}

statement loop_test(l)
loop l;
{
    call c = make_call(entity_intrinsic(".LE."),
		        CONS(EXPRESSION,
			     make_entity_expression(loop_index(l), NIL),
			     CONS(EXPRESSION,
				   range_upper(loop_range(l)),
				   NIL)));
    test t = make_test(make_expression(make_syntax(is_syntax_call, c),
					normalized_undefined), 
		       MAKE_CONTINUE_STATEMENT(), 
		       MAKE_CONTINUE_STATEMENT());
 
    return(make_statement(entity_empty_label(), 
			   STATEMENT_NUMBER_UNDEFINED,
			   STATEMENT_ORDERING_UNDEFINED,
			   string_undefined,
			   make_instruction(is_instruction_test, t)));
}

statement loop_inc(l)
loop l;
{
    expression I = make_entity_expression(loop_index(l), NIL);
    expression II = make_entity_expression(loop_index(l), NIL);
    call c = make_call(entity_intrinsic("+"), 
		        CONS(EXPRESSION, 
			      I, 
			      CONS(EXPRESSION, 
				    range_increment(loop_range(l)), 
				    NIL)));
    expression I_plus_one = 
	    make_expression(make_syntax(is_syntax_call, c), 
			    normalized_undefined);

    return(make_assign_statement(II, I_plus_one));
}

/* CONTROLIZE_LOOP computes in C_RES the control graph of the loop L (of
   statement ST) with PREDecessor and SUCCessor. */

bool controlize_loop(st, l, pred, succ, c_res, used_labels)
statement st;
loop l;
control pred, succ;
control c_res;
hash_table used_labels;
{
    hash_table loop_used_labels = hash_table_make(hash_string, 0);
    control c_body = make_conditional_control(loop_body(l));
    control c_inc = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    control c_test = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    bool controlized;

    controlize(loop_body(l), c_test, c_inc, c_body, loop_used_labels);

    if(covers_labels_p(loop_body(l),loop_used_labels)) {
	loop new_l = make_loop(loop_index(l),
			       loop_range(l),
			       control_statement(c_body),
			       loop_label(l),
			       loop_execution(l),
			       loop_locals(l));

	UPDATE_CONTROL(c_res,
		       make_statement(statement_label(st),
				      statement_number(st),
				      STATEMENT_ORDERING_UNDEFINED,
				      statement_comments(st),
				      make_instruction(is_instruction_loop, 
						       new_l)),
		       ADD_PRED(pred, c_res),
		       ADD_SUCC(succ, c_res )) ;
	controlized = FALSE;
	control_predecessors(succ) = ADD_PRED(c_res, succ);
    }
    else {
	control_statement(c_test) = loop_test(l);
	control_predecessors(c_test) =
		CONS(CONTROL, c_res, CONS(CONTROL, c_inc, NIL)),
	control_successors(c_test) =
		CONS(CONTROL, c_body, CONS(CONTROL, succ, NIL));
	control_statement(c_inc) = loop_inc(l);
	control_successors(c_inc) = CONS(CONTROL, c_test, NIL);
	UPDATE_CONTROL(c_res,
		       loop_header(l),
		       ADD_PRED(pred, c_res),
		       CONS(CONTROL, c_test, NIL));
	controlized = TRUE ;
	control_predecessors(succ) = ADD_PRED(c_test, succ);
    }
    control_successors(pred) = ADD_SUCC(c_res, pred);

    union_used_labels( used_labels, loop_used_labels);
    return(controlized);
}

/* COMPACT_LIST takes a list of controls CTLS coming from a CONTROLIZE_LIST
   and compacts the successive assignments, i.e. concatenates (i=1) followed
   by (j=2) in a single control with a block statement (i=1;j=2). The LAST
   control node is returned in case on terminal compaction. */

static control compact_list(ctls, c_end)
cons *ctls;
control c_end ;
{
    control c_res;
    control c_last = c_end ;

    if( ENDP( ctls )) {
	return( c_last ) ;
    }
    c_res = CONTROL(CAR(ctls));

    for(ctls = CDR(ctls); !ENDP(ctls); ctls = CDR(ctls)) {
	cons *succs, *succs_of_succ;
	instruction i, succ_i;
	statement st, succ_st;
	control succ;

	if(gen_length(succs=control_successors(c_res)) != 1 ||
	   gen_length(control_predecessors(succ=CONTROL(CAR(succs)))) != 1 ||
	   gen_length(succs_of_succ=control_successors(succ)) != 1 ||
	   CONTROL(CAR(succs_of_succ)) == c_res ) {
	    c_last = c_res = CONTROL(CAR(ctls));
	    continue;
	}
	st = control_statement(c_res) ;
	succ_st = control_statement(succ);

	if(succ == c_end) {
	    c_last = c_res;
	}
	if(!instruction_block_p(i=statement_instruction(st))) {
	    i = make_instruction(is_instruction_block,
				 CONS(STATEMENT, st, NIL));
	    control_statement(c_res) =
		make_statement(entity_empty_label(), 
			       STATEMENT_NUMBER_UNDEFINED,
			       STATEMENT_ORDERING_UNDEFINED,
			       string_undefined,
			       i);
	}
	if(c_res != succ) {
	    if(instruction_block_p(succ_i=statement_instruction(succ_st))){
		instruction_block(i) = 
			gen_nconc(instruction_block(i), 
				  instruction_block(succ_i));
	    }
	    else {
		instruction_block(i) =
			gen_nconc(instruction_block(i), 
				  CONS(STATEMENT, succ_st, NIL));
	    }
	}
	control_successors(c_res) = control_successors(succ);
	patch_references(PREDS_OF_SUCCS, succ, c_res);
    }
    return( c_last ) ;
}

/* CONTROLIZE_LIST_1 is the equivalent of a mapcar of controlize on STS.
   The trick is to keep a list of the controls to compact them latter. Note
   that if a statement is controlized, then the predecessor has to be
   computed (i.e. is not the previous control on STS).; this is the purpose
   of c_in. */

cons * controlize_list_1(sts, pred, succ, c_res, used_labels)
cons *sts;
control pred, succ;
control c_res;
hash_table used_labels;
{
    cons *ctls = NIL;

    for(; !ENDP(sts); sts = CDR(sts)) {
	statement st = STATEMENT(CAR(sts));
	control c_next = 
		ENDP(CDR(sts)) ? succ : 
			make_conditional_control(STATEMENT(CAR(CDR(sts))));
	bool controlized = 
		controlize(st, pred, c_next, c_res, used_labels);
	bool unreachable = ENDP(control_predecessors(c_next));

	ctls = CONS(CONTROL, c_res, ctls);

	if(unreachable) {
	    control_successors(Unreachable) =
		    CONS(CONTROL, (control)st,
			 control_successors(Unreachable)) ;
	}
	if(controlized) {
	    control c_in = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
	    
	    ctls = CONS(CONTROL, c_in, ctls);
	    control_predecessors(c_in) = control_predecessors(c_next);
	    control_successors(c_in) = CONS(CONTROL, c_next, NIL);
	    patch_references(SUCCS_OF_PREDS, c_next, c_in);
	    control_predecessors(c_next) = CONS(CONTROL, c_in, NIL) ;
	    pred = c_in;
	}
	else {
	    pred = (unreachable) ? 
		    make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL) :
		    c_res;
	}
	c_res = c_next ; 
    }
    return(gen_nreverse(ctls));
}

/* CONTROLIZE_LIST computes in C_RES the control graph of the list
   STS (of statement ST) with PREDecessor and SUCCessor. We try to
   minize the number of graphs by looking for graphs with one node
   only and picking the statement in that case. */

bool controlize_list(st, sts, pred, succ, c_res, used_labels)
statement st;
cons *sts;
control pred, succ;
control c_res;
hash_table used_labels;
{
    hash_table block_used_labels = hash_table_make(hash_string, 0);
    control c_block = 
	    (ENDP(sts)) ?
		    make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL) :
			    make_conditional_control(STATEMENT(CAR(sts)));
    control c_end = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    control c_last = c_end;
    cons *ctls =
	    controlize_list_1(sts, pred, c_end, c_block, block_used_labels);
    bool controlized;

    c_last = compact_list( ctls, c_end ) ;

    if(covers_labels_p(st,block_used_labels)) {
	statement new_st; 

	control_predecessors(c_block) = 
		remv(pred, control_predecessors(c_block));
	control_successors(c_block) = 
		remv(c_end, control_successors(c_block));

	if(ENDP(control_predecessors(c_block)) &&
	    ENDP(control_successors(c_block))) {
	    new_st = control_statement(c_block);
	}
	else {
	    unstructured u = make_unstructured(c_block, c_last);
	    instruction i =
		    make_instruction(is_instruction_unstructured, u);

	    new_st = make_statement(entity_empty_label(), 
				    statement_number(st),
				    STATEMENT_ORDERING_UNDEFINED,
				    statement_comments(st),
				    i);
	}
	UPDATE_CONTROL(c_res, new_st,
		        ADD_PRED(pred, c_res),
		        CONS(CONTROL, succ, NIL));
	control_predecessors(succ) = ADD_PRED(c_res, succ);
	control_successors(pred) = CONS(CONTROL, c_res, NIL);
	controlized = FALSE;
    }
    else {
	UPDATE_CONTROL(c_res,
		        control_statement(c_block),
		        control_predecessors(c_block),
		        control_successors(c_block));
	control_predecessors(succ) = ADD_PRED(c_end, succ);
	control_successors(c_end) = CONS(CONTROL, succ, NIL);
	patch_references(PREDS_OF_SUCCS, c_block, c_res);
	patch_references(SUCCS_OF_PREDS, c_block, c_res);
	controlized = TRUE;
    }
    union_used_labels( used_labels, block_used_labels);
    return(controlized);
}
	
/* CONTROL_TEST builds the control node of a statement ST in C_RES which is a 
   test T. */

bool controlize_test(st, t, pred, succ, c_res, used_labels)
test t;
statement st;
control pred, succ;
control c_res;
hash_table used_labels;
{
    hash_table t_used_labels = hash_table_make(hash_string, 0); 
    hash_table f_used_labels = hash_table_make(hash_string, 0);
    control c1 = make_conditional_control(test_true(t));
    control c2 = make_conditional_control(test_false(t));
    control c_join = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    statement s_t = test_true(t);
    statement s_f = test_false(t);
    bool controlized;

    controlize(s_t, c_res, c_join, c1, t_used_labels);	
    controlize(s_f, c_res, c_join, c2, f_used_labels);

    if(covers_labels_p(s_t, t_used_labels) && 
       covers_labels_p(s_f, f_used_labels)) {
	test it = make_test(test_condition(t), 
			     control_statement(c1),
			     control_statement(c2));

	UPDATE_CONTROL(c_res, 
		        make_statement(statement_label(st), 
				       statement_number(st),
				       STATEMENT_ORDERING_UNDEFINED,
				       statement_comments(st),
				       make_instruction(is_instruction_test, 
							it)),
		        ADD_PRED(pred, c_res),
		        CONS(CONTROL, succ, NIL));
	control_predecessors(succ) = ADD_PRED(c_res, succ);
	controlized = FALSE;
    }
    else {
	UPDATE_CONTROL(c_res, st, 
		        ADD_PRED(pred, c_res),
		        CONS(CONTROL, c1, CONS(CONTROL, c2, NIL)));
	test_true(t) = MAKE_CONTINUE_STATEMENT();
	test_false(t) = MAKE_CONTINUE_STATEMENT();
	control_predecessors(succ) = ADD_PRED(c_join, succ);
	control_successors(c_join) = CONS(CONTROL, succ, NIL);
	controlized = TRUE;
    }
    control_successors(pred) = ADD_SUCC(c_res, pred);
    union_used_labels(used_labels, 
		      union_used_labels(t_used_labels, f_used_labels));
    return(controlized);
}

/* INIT_LABEL puts the reference in the statement ST to the label NAME
   int the Label_statements table and allocate a slot in the Label_control
   table. */

void init_label(name, st ) 
string name;
statement st;
{
    if(!empty_label_p(name)) {
	cons *used =
		(cons *)hash_get_default_empty_list(Label_statements, name);

	hash_put(Label_statements, name, (char *)CONS(STATEMENT, st, used));

	if( hash_get(Label_control, name)==HASH_UNDEFINED_VALUE ) {
	    statement new_st = 
		    make_continue_statement(statement_label(st)) ;
	    control c = make_control( new_st, NIL, NIL);
	    
	    hash_put(Label_control, name, (char *)c);
	}
    }
}

/* CREATE_STATEMENTS_OF_LABELS gathers in the Label_statements table all
   the references to the useful label of the statement ST. Note that for
   loops, the label in the DO statement is NOT introduced. Label_control is
   also created. */

void create_statements_of_label(st)
statement st;
{
    string name = entity_name(statement_label(st));
    instruction i;

    init_label(name, st);

    switch(instruction_tag(i = statement_instruction(st))) {
    case is_instruction_goto: {
	string where = entity_name(statement_label(instruction_goto(i)));

	init_label(where, st);
	break;
    }
    case is_instruction_unstructured:
	pips_error("create_statement_of_labels", "Found unstructured", "");
    }
}

void create_statements_of_labels(st)
statement st ;
{
    gen_recurse(st, 
		statement_domain,
		gen_true,
		create_statements_of_label);
}
		

/* SIMPLIFIED_UNSTRUCTURED tries to get rid of top-level and useless
   unstructure nodes. */

static unstructured simplified_unstructured(top, bottom, res)
control top, bottom, res;
{
    cons *succs;
    statement st;
    unstructured u;
    instruction i;

    u = make_unstructured(top, bottom);

    if(!ENDP(control_predecessors(top))) {
	return(u);
    }
    if(gen_length(succs=control_successors(top)) != 1) {
	return(u);
    }
    pips_assert("simplify_control", CONTROL(CAR(succs)) == res);
    
    if(gen_length(control_predecessors(res)) != 1) {
	return(u);
    }
    if(gen_length(succs=control_successors(res)) != 1) {
	return(u);
    }
    if(CONTROL(CAR(succs)) != bottom) {
	return(u);
    }
    if(gen_length(control_predecessors(bottom)) != 1) {
	return(u);
    }
    if(!ENDP(control_successors(bottom))) {
	return(u);
    }
    control_predecessors(res) = control_successors(res) = NIL;
    st = control_statement(res);

    if(instruction_unstructured_p(i=statement_instruction(st))) {
	return(instruction_unstructured(i));
    }
    unstructured_control(u) = unstructured_exit(u) = res;
    return(u);
}

/* These functions compute the statement_ordering of their arguments. 
   U_NUMBER is the current unstructured number and *S_NUMBER the current
   statement number, in the current U_NUMBER. */

static int u_number;
#define RESET_UNSTRUCTURED_NUMBER {u_number = 1;}
#define NEW_UNSTRUCTURED_NUMBER (u_number++)

static int statement_reorder(st, un, sn)
statement st;
int un, sn;
{
    instruction i = statement_instruction(st);
    void unstructured_reorder();

    debug(5, "statement_reorder", "entering for %d : (%d,%d)\n",
	  statement_number(st), un, sn);

    statement_ordering(st) = MAKE_ORDERING(un, sn);

    sn += 1;

    switch (instruction_tag(i)) {
      case is_instruction_block:
	debug(5, "statement_reorder", "block\n");
	MAPL(sts, {
	    sn = statement_reorder(STATEMENT(CAR(sts)), un, sn);
	}, instruction_block(i));
	break;
      case is_instruction_test:
	debug(5, "statement_reorder", "test\n");
	sn = statement_reorder(test_true(instruction_test(i)), un, sn);
	sn = statement_reorder(test_false(instruction_test(i)), un, sn);
	break;
      case is_instruction_loop:
	debug(5, "statement_reorder", "loop\n");
	sn = statement_reorder(loop_body(instruction_loop(i)), un, sn);
	break;
      case is_instruction_goto:
      case is_instruction_call:
	debug(5, "statement_reorder", "goto or call\n");
	break;
      case is_instruction_unstructured: 
	debug(5, "statement_reorder", "unstructured\n");
	unstructured_reorder(instruction_unstructured(i));
	break;
      default:
	pips_error("statement_reorder", "Unknown tag %d\n", 
		   instruction_tag(i));
    }

    debug(5, "statement_reorder", "exiting %d\n", sn);

    return(sn);
}

void unstructured_reorder(u)
unstructured u;
{
    cons *blocs = NIL;

    debug(5, "unstructured_reorder", "entering\n");

    CONTROL_MAP(ctl, {
	statement st = control_statement(ctl);
	int un = NEW_UNSTRUCTURED_NUMBER;

	debug(5, "unstructured_reorder", "will reorder %d %d\n",
	      statement_number(st), un);

	statement_reorder(st, un, 1);
    }, unstructured_control(u), blocs);

    gen_free_list(blocs);

    debug(5, "unstructured_reorder", "exiting\n");
}

/* CONTROL_GRAPH returns the control graph of the statement ST. */

unstructured control_graph(st) 
statement st;
{
    control result, top, bottom;
    hash_table used_labels = hash_table_make(hash_string, 0);
    unstructured u;

    debug_on("CONTROL_DEBUG_LEVEL");

    if (get_debug_level() > 0) {
	set_bool_property("PRETTYPRINT_BLOCKS", TRUE);
	set_bool_property("PRETTYPRINT_EMPTY_BLOCKS", TRUE);
    }

    hash_dont_warn_on_redefinition();
    Label_statements = hash_table_make(hash_string, LABEL_TABLES_SIZE);
    Label_control = hash_table_make(hash_string, LABEL_TABLES_SIZE);
    create_statements_of_labels(st);

    result = make_conditional_control(st);
    top = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    bottom = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    Unreachable = make_control(MAKE_CONTINUE_STATEMENT(), NIL, NIL);
    controlize(st, top, bottom, result, used_labels);

    if(!ENDP(control_successors(Unreachable))) {
	user_warning("control_graph", "Some statements are unreachable\n");
    }
    hash_table_free(Label_statements);
    hash_table_free(Label_control);

    u = simplified_unstructured(top, bottom, result);

    if( get_debug_level() > 5) {
	cons *blocs = NIL ;

	fprintf(stderr, "Nodes in unstructured (%x, %x)\n",
		(unsigned int) unstructured_control(u),
		(unsigned int) unstructured_exit(u)) ;

	CONTROL_MAP(ctl, {
	    fprintf(stderr, "%x, ", (unsigned int) ctl) ;
	}, unstructured_control(u), blocs);
    }
    RESET_UNSTRUCTURED_NUMBER;
    unstructured_reorder(u);

    debug_off();

    return(u);
}

void module_body_reorder(body)
statement body;
{
    RESET_UNSTRUCTURED_NUMBER;

    debug_on("CONTROL_DEBUG_LEVEL");

    /* FI: I do not understand why unstructured numbering is not
     * restarted from 0.
     * statement_reorder(body, NEW_UNSTRUCTURED_NUMBER, 1);
     */
    statement_reorder(body, 0, 1);

    debug_off();
}

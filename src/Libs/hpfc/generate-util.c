/* HPFC module by Fabien COELHO
 *
 * $RCSfile: generate-util.c,v $ version $Revision$
 * ($Date: 1995/09/22 13:16:18 $, ) 
 */

#include "defines-local.h"
entity CreateIntrinsic(string); /* in syntax */

/* builds a statement
 *   VAR_i = MYPOS(i, proc_number) // i=1 to proc dimension
 */
statement 
define_node_processor_id(
    entity proc,
    entity (*creation)(int))
{
    int i, procn = load_hpf_number(proc);
    list /* of expression */ mypos_indices,
         /* of statement */  ls = NIL;
    reference mypos;
    entity dummy;

    for(i=NumberOfDimension(proc); i>0; i--)
    {
	dummy = creation(i);
	mypos_indices = CONS(EXPRESSION, int_to_expression(i),
			CONS(EXPRESSION, int_to_expression(procn), 
			     NIL));
	mypos = make_reference(hpfc_name_to_entity(MYPOS), mypos_indices);
	ls = CONS(STATEMENT,
		  make_assign_statement(entity_to_expression(dummy),
					reference_to_expression(mypos)),
		  ls);
    }
				       
    return make_block_statement(ls);
}

/* statement generate_deducables(list le)
 *
 * the le list of expression is used to generate the deducables.
 * The fields of interest are the variable which is referenced and 
 * the normalized field which is the expression that is going to
 * be used to define the variable.
 */
statement 
generate_deducables(
    list /* of expression */ le)
{
    list rev = gen_nreverse(gen_copy_seq(le)), ls = NIL;

    MAP(EXPRESSION, e,
    {
	entity var = reference_variable(expression_reference(e));
	Pvecteur v = normalized_linear(expression_normalized(e));

	ls = CONS(STATEMENT, Pvecteur_to_assign_statement(var, v), ls);
    },
	rev);

    gen_free_list(rev);
    return make_block_statement(ls);
}

list /* of expression */
hpfc_gen_n_vars_expr(
    entity (*creation)(),
    int number)
{
    list result = NIL;
    assert(number>=0 && number<=7);

    for(; number>0; number--)
	result = CONS(EXPRESSION, entity_to_expression(creation(number)),
		      result);

    return result;
}

expression 
make_reference_expression(
    entity e,
    entity (*creation)(int))
{
    return reference_to_expression(make_reference(e,
	   hpfc_gen_n_vars_expr(creation, NumberOfDimension(e))));
}

/* the following functions generate the statements to appear in
 * the I/O loop nest.
 */
statement 
set_logical(
    entity log, /* the logical variable to be assigned */
    bool val)   /* the assigned truth value */
{
    return make_assign_statement
	(entity_to_expression(log),
	 make_call_expression(MakeConstant
	      (val ? ".TRUE." : ".FALSE.", is_basic_logical),
			      NIL));
}

statement
set_integer(
    entity var, /* the integer scalar variable to be assigned */
    int val)    /* the assigned int value */
{
    return make_assign_statement(entity_to_expression(var),
				 int_to_expression(val));
}

/* returns statement VAR = VAR + N
 */
statement 
hpfc_add_n(
    entity var, /* integer scalar variable */
    int n)      /* added value */
{
    return make_assign_statement
	(entity_to_expression(var),
	 MakeBinaryCall(CreateIntrinsic(PLUS_OPERATOR_NAME),
			entity_to_expression(var), int_to_expression(n)));
}

/* expr = expr + 2
 */
statement 
hpfc_add_2(
    expression exp)
{
    entity plus = CreateIntrinsic(PLUS_OPERATOR_NAME);
    return(make_assign_statement
	   (expression_dup(exp), 
	    MakeBinaryCall(plus, exp, int_to_expression(2))));

}

statement 
hpfc_message(
    expression tid,
    expression channel,
    bool send)
{
    expression third;
    entity pvmf;

    third = entity_to_expression(hpfc_name_to_entity(send ? INFO : BUFID));
    pvmf = hpfc_name_to_entity(send ? PVM_SEND : PVM_RECV);

    return make_block_statement
	   (CONS(STATEMENT, hpfc_make_call_statement(pvmf,
				   CONS(EXPRESSION, tid,
				   CONS(EXPRESSION, channel,
				   CONS(EXPRESSION, third,
					NIL)))),
	    CONS(STATEMENT, hpfc_add_2(copy_expression(channel)),
		 NIL)));
}

/* returns if (LAZY_{SEND,RECV}) then
 */
statement 
hpfc_lazy_guard(
    bool snd, 
    statement then)
{
    entity decision = hpfc_name_to_entity(snd ? LAZY_SEND : LAZY_RECV);
    return test_to_statement
     (make_test(entity_to_expression(decision), then, make_empty_statement()));
}

/* IF (LAZY_snd) THEN 
 *   PVMFsnd()
 *   LAZY_snd = FALSE // if receive
 * ENDIF
 */
static statement 
hpfc_lazy_message(
    expression tid, 
    expression channel, 
    bool snd)
{
    entity decision = hpfc_name_to_entity(snd ? LAZY_SEND : LAZY_RECV);
    statement 
	comm = hpfc_message(tid, channel, snd),
	then = snd ? comm : 
	    make_block_statement(CONS(STATEMENT, comm,
				 CONS(STATEMENT, set_logical(decision, FALSE),
				      NIL))) ;
    
    return hpfc_lazy_guard(snd, then);
}

statement 
hpfc_generate_message(
    entity ld, 
    bool send, 
    bool lazy)
{
    entity nc, nt;
    expression lid, tid, chn;

    nc = hpfc_name_to_entity(send ? SEND_CHANNELS : RECV_CHANNELS);
    nt = hpfc_name_to_entity(NODETIDS);
    lid = entity_to_expression(ld);
    tid = reference_to_expression
	(make_reference(nt, CONS(EXPRESSION, lid, NIL)));
    chn = reference_to_expression
	(make_reference(nc, CONS(EXPRESSION, copy_expression(lid), NIL)));

    return lazy ? hpfc_lazy_message(tid, chn, send) : 
	          hpfc_message(tid, chn, send);
}

statement 
hpfc_initsend(
    bool lazy)
{
    statement init;

    /* 2 args to pvmfinitsend
     */
    init = hpfc_make_call_statement
	     (hpfc_name_to_entity(PVM_INITSEND), 
	      CONS(EXPRESSION, MakeCharacterConstantExpression("PVMRAW"),
	      CONS(EXPRESSION, entity_to_expression(hpfc_name_to_entity(BUFID)),
		   NIL)));

    return lazy ? make_block_statement
	(CONS(STATEMENT, init,
         CONS(STATEMENT, set_logical(hpfc_name_to_entity(LAZY_SEND), FALSE),
	      NIL))) :
		  init ;
}

/****************************************************************** PACKING */

/* returns the buffer entity for array
 */
static entity 
hpfc_buffer_entity(
    entity array,
    string suffix)
{
    return hpfc_name_to_entity(concatenate
			       (pvm_what_options(entity_basic(array)),
				suffix, NULL));
}

/* returns a reference to the typed common hpfc_buffer buffer,
 * that suits array basic type and with index as an index.
 */
expression
hpfc_buffer_reference(
    entity array, /* array to select the right typed buffer */
    entity index) /* index variable */
{
    return reference_to_expression
	(make_reference(hpfc_buffer_entity(array, BUFFER),
         CONS(EXPRESSION, entity_to_expression(index), 
	      NIL)));
}

/* generates the condition for testing the buffer state:
 * returns (BUFFER_INDEX.[eq|ne].BUFFER_[|RCV]SIZE) depending on swtiches.
 */
static expression 
buffer_full_condition(
    entity array, /* array for the typed buffer size */
    bool is_send, /* while sending or receiving */
    bool is_full) /* TRUE: is full, FALSE: is not empty */
{
    entity opera, bsize, index;

    index = hpfc_name_to_entity(BUFFER_INDEX);
    opera = is_full ? 
	CreateIntrinsic(EQUAL_OPERATOR_NAME) :
	CreateIntrinsic(NON_EQUAL_OPERATOR_NAME) ;
    bsize = is_send ? is_full ?
	hpfc_buffer_entity(array, BUFSZ) :
        MakeConstant("0", is_basic_int) :
	hpfc_name_to_entity(BUFFER_RCV_SIZE);

    return MakeBinaryCall(opera,
			  entity_to_expression(index),
			  entity_to_expression(bsize));
}

/* array(creation) = buffer(current++) or reverse assignment...
 */
statement 
hpfc_buffer_packing(
    entity array,
    entity (*creation)(), 
    bool pack)
{
    entity index;
    expression array_ref, buffer_ref;
    statement increment, assignment;

    index = hpfc_name_to_entity(BUFFER_INDEX);
    array_ref = make_reference_expression(array, creation);
    buffer_ref = hpfc_buffer_reference(array, index);
    increment = hpfc_add_n(index, 1);
    assignment = make_assign_statement(pack ? buffer_ref : array_ref,
				       pack ? array_ref : buffer_ref);
    
    return make_block_statement(CONS(STATEMENT, increment,
				CONS(STATEMENT, assignment,
				     NIL)));
}

static entity
hpfc_ith_broadcast_function(
    int dim) /* number of dimensions of the broadcast */
{
    char buffer[20]; /* ??? static buffer size */
    (void) sprintf(buffer, BROADCAST "%d", dim);
    return MakeRunTimeSupportSubroutine(buffer, 2*dim+1);
}

/* send the buffer, possibly a broadcast.
 * unconditional ? if non empty ?
 * generates hpfc_broadcast_x as required.
 */
statement 
hpfc_broadcast_buffers(
    entity array, /* should be the target array */
    entity lid,   /* broadcast base, maybe partial?! */
    entity proc)  /* processor related to the array... (redundant!?) */
{
    Pcontrainte c;
    int size, npdim, nreplicated;
    list /* of expression */ args = NIL;

    c = full_linearization(proc, (entity) NULL, &size, 
			   get_ith_temporary_dummy, FALSE, 0);

    npdim = NumberOfDimension(proc);
    
    for (nreplicated=0; npdim; npdim--)
    {
	if (processors_dim_replicated_p(proc, array, npdim))
	{
	    entity v;
	    int number, step;
	    
	    nreplicated++;
	    v = get_ith_temporary_dummy(npdim);
	    step = (int) vect_coeff((Variable) v, contrainte_vecteur(c));
	    number = SizeOfIthDimension(proc, npdim);

	    args = CONS(EXPRESSION, int_to_expression(number),
		   CONS(EXPRESSION, int_to_expression(step),
			args));
	}
    }

    args = CONS(EXPRESSION, entity_to_expression(lid), args);
    contraintes_free(c);

    return call_to_statement
	(make_call(hpfc_ith_broadcast_function(nreplicated), args));
}

statement 
hpfc_broadcast_if_necessary(
    entity array, /* remapped source array */
    entity trg,   /* remapped target array */
    entity lid,   /* lid for target processor(s) */
    entity proc,  /* target processor */
    bool is_lazy) /* lazy or not... */
{
    expression not_empty;
    statement send, pack;

    not_empty = buffer_full_condition(array, TRUE, FALSE);
    send = hpfc_broadcast_buffers(trg, lid, proc);
    pack = call_to_statement(make_call(hpfc_buffer_entity(array, BUFPCK), NIL));

    if (is_lazy)
	return test_to_statement
	    (make_test(not_empty,
	     make_block_statement(CONS(STATEMENT, pack,
				  CONS(STATEMENT, send, NIL))),
	     make_continue_statement(entity_undefined)));
    else
	return make_block_statement
	    (CONS(STATEMENT, test_to_statement(make_test(not_empty, pack,
			     make_continue_statement(entity_undefined))),
	     CONS(STATEMENT, test_to_statement(make_test(not_expression
                (entity_to_expression(hpfc_name_to_entity(SND_NOT_INIT))), send,
			     make_continue_statement(entity_undefined))),
		  NIL)));
}

/* lazy in actually sending or not the packed buffer immediatly...
 */
statement 
hpfc_lazy_buffer_packing(
    entity array, /* array being (un)packed */
    entity trg,   /* target array */
    entity lid,   /* local id for base target */
    entity proc,  /* the processors, needed for broadcasts */
    entity (*array_dim)(int), /* variables for array dimensions */
    bool is_send, /* send or receive ? */
    bool is_lazy) /* means you send the buffer directly, without packing... */
{
    statement packing, realpack, indexeq0, ifcond, optional;
    expression condition;
    list /* of statement */ l;

    packing = hpfc_buffer_packing(array, array_dim, is_send);
    condition = buffer_full_condition(array, is_send, TRUE);
    realpack = call_to_statement
	(make_call(hpfc_buffer_entity(array, is_send ? BUFPCK : BUFUPK), 
	   is_send ? NIL : CONS(EXPRESSION, entity_to_expression(lid), NIL)));
    indexeq0 = set_integer(hpfc_name_to_entity(BUFFER_INDEX), 0);
    optional = is_lazy ? 
	is_send ? hpfc_broadcast_buffers(trg, lid, proc) :
	          set_logical(hpfc_name_to_entity(RCV_NOT_PRF), TRUE) :
	make_continue_statement(entity_undefined);
			   
    if (is_send)
	l = CONS(STATEMENT, realpack,
	    CONS(STATEMENT, optional,
	    CONS(STATEMENT, indexeq0, NIL)));
    else
	l = CONS(STATEMENT, optional,
	    CONS(STATEMENT, realpack,
	    CONS(STATEMENT, indexeq0, NIL)));
    
    ifcond = test_to_statement
	(make_test(condition, 
		   make_block_statement(l),
		   make_continue_statement(entity_undefined)));

    return make_block_statement
	(is_send ? CONS(STATEMENT, packing, CONS(STATEMENT, ifcond, NIL)) :
	           CONS(STATEMENT, ifcond, CONS(STATEMENT, packing, NIL)));
}

statement
hpfc_buffer_initialization(
    bool is_send,
    bool is_lazy)
{
    statement buffindex, msgstate, other;
    list /* of statement */ l;

    buffindex = set_integer(hpfc_name_to_entity(BUFFER_INDEX), 0);
    msgstate = set_logical(hpfc_name_to_entity
			   (is_send ? SND_NOT_INIT : RCV_NOT_PRF), TRUE);
    other = set_integer
	(hpfc_name_to_entity(is_send ? BUFFER_ENCODING : BUFFER_RCV_SIZE),
	 is_send ? 1 : 0); /* PVMRAW, no PVM in place... */

    l = CONS(STATEMENT, buffindex,
        CONS(STATEMENT, msgstate, 
	CONS(STATEMENT, other,
	     NIL)));

    return make_block_statement(l);
}

/* returns PVMF(un)pack(..., array(creation), 1, 1, HPFC_INFO)
 */
statement 
hpfc_pvm_packing(
    entity array,
    entity (*creation)(int), 
    bool pack)

{
    return hpfc_make_call_statement
	(hpfc_name_to_entity(pack ? PVM_PACK : PVM_UNPACK),
	 CONS(EXPRESSION, pvm_what_option_expression(array),
         CONS(EXPRESSION, make_reference_expression(array, creation),
	 CONS(EXPRESSION, int_to_expression(1),
	 CONS(EXPRESSION, int_to_expression(1),
	 CONS(EXPRESSION, entity_to_expression(hpfc_name_to_entity(INFO)),
	      NIL))))));
}

/* the lazy issues.
 * note that target processors should be known 
 * to generate the appropriate broadcast?
 */
statement 
hpfc_lazy_packing(
    entity array,
    entity lid, 
    entity (*creation)(int),
    bool pack,
    bool lazy)
{
    statement pack_stmt = hpfc_pvm_packing(array, creation, pack);

    return lazy ? (pack ? make_block_statement
       (CONS(STATEMENT, pack_stmt,
	CONS(STATEMENT, set_logical(hpfc_name_to_entity(LAZY_SEND), TRUE),
	     NIL))) :
		   make_block_statement
       (CONS(STATEMENT, hpfc_generate_message(lid, FALSE, TRUE),
	CONS(STATEMENT, pack_stmt,
	     NIL)))) : pack_stmt ;
}

list /* of expression */
make_list_of_constant(
    int val,    /* the constant value */
    int number) /* the length of the created list */
{
    list l=NIL;

    assert(number>=0);
    for(; number; number--)
	l = CONS(EXPRESSION, int_to_expression(val), l);

    return l;
}

/* statement st_compute_lid(proc)
 *
 *       T_LID=CMP_LID(pn, pi...)
 *
 * if array is not NULL, partial according to array.
 * the offset is shifted as if proc dimensions were normalized,
 * in order to match the runtime library hpfc_broadcast_* expectations.
 */
statement 
hpfc_compute_lid(
    entity lid,               /* variable to be assigned to */
    entity proc,              /* processor arrangement */
    entity (*creation)(int),  /* individual variables */
    entity array)             /* to be used for partial (broadcasts...) */
{
    if (!get_bool_property("HPFC_EXPAND_CMPLID"))
    {
	int ndim = NumberOfDimension(proc);
	entity cmp_lid = hpfc_name_to_entity(CMP_LID);

	message_assert("implemented", !array);
	
	return make_assign_statement(entity_to_expression(lid),
	  make_call_expression
	    (cmp_lid, CONS(EXPRESSION,int_to_expression(load_hpf_number(proc)),
			   gen_nconc(hpfc_gen_n_vars_expr(creation, ndim),
				     make_list_of_constant(0, 7-ndim)))));
    }
    else
    {
	int size;
	Pcontrainte c;
	Pvecteur v;
	statement result;

	c = full_linearization(proc, lid, &size, creation, FALSE, 1);
	v = contrainte_vecteur(c);

	if (array)
	{
	    /* remove distributed dimensions from the constraint
	     * and normalize the partial system so that dims are 0:...
	     */
	    int npdim;
	    assert(array && !entity_undefined_p(array));
	    
	    for(npdim = NumberOfDimension(proc); npdim; npdim--)
		if (processors_dim_replicated_p(proc, array, npdim))
		{
		    Variable var = (Variable) creation(npdim);
		    int low, up, cf;

		    get_entity_dimensions(proc, npdim, &low, &up);
		    cf = vect_coeff(var, v);
		    vect_add_elem(&contrainte_vecteur(c), var, (Value) -cf);
		    vect_add_elem(&contrainte_vecteur(c), TCST, (Value) cf*low);
		}
	}

	result = Pvecteur_to_assign_statement(lid, contrainte_vecteur(c));
	contraintes_free(c);
	
	return result;
    }
}

/* that is all
 */

/* HPFC module by Fabien COELHO
 *
 * These functions deal with HPF directives.
 * (just a big hack, but few lines of code and neither lex nor yacc:-)
 * I'm definitely happy with this. FC.
 *
 * $RCSfile: directives.c,v $ version $Revision$,
 * ($Date: 1996/02/29 19:05:22 $, )
 */

#include "defines-local.h"

#include "pipsdbm.h"
#include "resources.h"
#include "bootstrap.h"
#include "control.h"

/***************************************************************** UTILITIES */

/* list of statements to be cleaned. the operation is delayed because
 * the directives are needed in place to stop the dynamic updates.
 */
static list /* of statements */ to_be_cleaned = NIL;

/* the directive is freed and replaced by a continue call or
 * a copy loop nest, depending on the renamings.
 */
static void clean_statement(statement s)
{
    instruction i = statement_instruction(s);

    pips_assert("call", instruction_call_p(i));

    free_call(instruction_call(i));
    instruction_call(i) = call_undefined;

    if (bound_renamings_p(s))
    {
	list /* of renamings */  lr = load_renamings(s),
	     /* of statements */ block = NIL;
	
	MAP(RENAMING, r,
	{
	    entity o = renaming_old(r);
	    entity n = renaming_new(r);
	    
	    block = CONS(STATEMENT, generate_copy_loop_nest(o, n), block);
	},
	    lr);

	free_instruction(i);
	statement_instruction(s) =
	    make_instruction(is_instruction_block, block);
    }
    else
	instruction_call(i) =
	    make_call(entity_intrinsic(CONTINUE_FUNCTION_NAME), NIL);
}

static void add_statement_to_clean(statement s)
{
    to_be_cleaned = CONS(STATEMENT, s, to_be_cleaned);
}

/* local primary dynamics
 */
GENERIC_STATIC_STATUS(/**/, the_dynamics, list, NIL, gen_free_list)

void add_a_dynamic(entity c)
{
    the_dynamics = gen_once(c, the_dynamics);
}

/*  the local stack is used to retrieve the current statement while 
 *  scanning the AST with gen_recurse.
 */
DEFINE_LOCAL_STACK(current_stmt, statement)

/* management of PROCESSORS and TEMPLATE directives.
 *
 * just changes the basic type to overloaded and 
 * stores the entity as a processor or a template.
 */
static void switch_basic_type_to_overloaded(entity e)
{
    basic b = entity_basic(e);
    pips_assert("basic defined", b!=basic_undefined);
    basic_tag(b)=is_basic_overloaded;
}

static void new_processor(expression e)
{
    entity p = expression_to_entity(e);
    switch_basic_type_to_overloaded(p);
    set_processor(p);

    pips_debug(3, "entity is %s\n", entity_name(p));
}

static void new_template(expression e)
{
    entity t = expression_to_entity(e);
    switch_basic_type_to_overloaded(t);
    set_template(t);

    pips_debug(3, "entity is %s\n", entity_name(t));
}

static void new_dynamic(expression e)
{
    entity a = expression_to_entity(e);
    set_entity_as_dynamic(a);
    add_a_dynamic(a);

    pips_debug(3, "entity is %s\n", entity_name(a));
}

static void new_io_function(expression e)
{
    add_an_io_function(expression_to_entity(e)); 
}

static void new_pure_function(expression e)
{
    add_a_pure(expression_to_entity(e)); 
}

/* array is to be seen as a template. aligned to itself...
 */
static void array_as_template(entity array)
{
    int ndim = NumberOfDimension(array);
    list /* of alignment */ l = NIL;

    set_array_as_distributed(array);
    set_template(array);

    for(; ndim>0; ndim--)
        l = CONS(ALIGNMENT, make_alignment(ndim, ndim,
                                           Value_to_expression(1),
                                           Value_to_expression(0)), l);

    store_hpf_alignment(array, make_align(l, array));
}

/* one simple ALIGN directive is handled.
 * retrieve the alignment from references array and template
 */
/*  TRUE if the template dimension subscript is an alignment.
 *  FALSE if the dimension is replicated. 
 */
static bool
alignment_p(list /* of expressions */ align_src,
	    expression subscript,
	    int *padim, Value *prate, Value *pshift)
{
    normalized n = expression_normalized(subscript);
    Pvecteur v, v_src;
    int size, array_dim;

    if (normalized_complex_p(n)) return FALSE;

    /*  else the subscript is affine
     */
    v = normalized_linear(n);
    size = vect_size(v);
    *pshift = vect_coeff(TCST, v);

    /*  the alignment should be a simple affine expression
     */
    pips_user_assert("affine align subscript", *pshift==0 ? size<=1 : size<=2)

    /*   constant alignment case
     */
    if (size==0 || (*pshift!=0 && size==1))
    {
	*padim = 0, *prate = 0;
	return TRUE;
    }

    /*   affine alignment case
     */
    for(array_dim = 1; !ENDP(align_src); POP(align_src), array_dim++)
    {
	n = expression_normalized(EXPRESSION(CAR(align_src)));
	if (normalized_linear_p(n))
	{
	    v_src = normalized_linear(n);

	    pips_user_assert("simple index",
			     vect_size(v_src)==1 && var_of(v_src)!=TCST);
	    
	    *prate = vect_coeff(var_of(v_src), v);

	    if (*prate!=0) 
	    {
		*padim = array_dim;
		return TRUE;   /* alignment ok */
	    }
	}
    }

    /*   matching array dimension not found, replicated!
     */
    *padim = 0, *prate = 0;
    return FALSE;
}

/*  builds an align from the alignee and template references.
 *  used by both align and realign management.
 */
static align 
extract_the_align(reference alignee,
		  reference temp)
{
    list/* of alignments  */ aligns    = NIL,
	/* of expressions */ align_src = reference_indices(alignee),
	                     align_sub = reference_indices(temp);
    entity template = reference_variable(temp), 
           array = reference_variable(alignee);
    int array_dim, template_dim, tndim, andim;
    Value rate, shift;

    if (!entity_template_p(template))
        array_as_template(template);

    pips_user_assert("align with a template", entity_template_p(template));
    tndim = NumberOfDimension(template);
    andim = NumberOfDimension(array);
    
    /*  each array dimension is looked for a possible alignment
     */

    if (ENDP(align_src)) /* align A with T - implicit alignment */
    {
	int dim, ndim, tlower, alower, unused;

	pips_user_assert("no template subscripts", ENDP(align_sub));
	ndim=MIN(andim, tndim);

	for (dim=1; dim<=ndim; dim++)
	{
	    get_entity_dimensions(template, dim, &tlower, &unused);
	    get_entity_dimensions(array, dim, &alower, &unused);
	      
	    aligns = CONS(ALIGNMENT,
			  make_alignment(dim, dim, 
					 Value_to_expression(1),
					 Value_to_expression(-alower+tlower)),
			  aligns);
	}
    }
    else /* explicit alignment */
    {
	pips_user_assert("align-source-list length = rank",
			 gen_length(align_src)==andim);
	pips_user_assert("align-subscript-list length = rank",
			 gen_length(align_sub)==tndim);

	for(template_dim=1; !ENDP(align_sub); POP(align_sub), template_dim++)
	{
	    if (alignment_p(align_src, EXPRESSION(CAR(align_sub)),
			    &array_dim, &rate, &shift))
		aligns = CONS(ALIGNMENT, 
			      make_alignment(array_dim,
					     template_dim,
					     Value_to_expression(rate),
					     Value_to_expression(shift)),
			      aligns);
	}
    }

    /* built align is returned. should be normalized?
     */
    return make_align(aligns, template);
}

/* handle s as the initial alignment...
 * to be called after the dynamics arrays...
 */
static void initial_alignment(statement s)
{
    MAP(ENTITY, array,
    {
	if (array_distributed_p(array))
        {
	    propagate_synonym(s, array, array);
	    update_renamings(s, CONS(RENAMING, make_renaming(array, array),
				     load_renamings(s)));
	}
    },
	get_the_dynamics());
}

/* handle a simple (re)align directive.
 * store the mappings in internal data structures.
 */
static void 
one_align_directive(reference alignee,
		    reference temp,
		    bool dynamic)
{
    entity template = reference_variable(temp),
	   array    = reference_variable(alignee);
    align a;
    
    pips_debug(3, "%s %saligned with %s\n", entity_name(array),
	       dynamic ? "re" : "", entity_name(template));

    a = extract_the_align(alignee, temp);
    normalize_align(array, a);

    ifdebug(8) print_align(a);

    if (dynamic)
    {
	statement current = current_stmt_head();
	entity new_array;

	pips_user_assert("dynamic array realignment",
	    array_distributed_p(array) && dynamic_entity_p(array));

	new_array = array_synonym_aligned_as(array, a);
	propagate_synonym(current, array, new_array);
	update_renamings(current, 
			 CONS(RENAMING, make_renaming(array, new_array),
			      load_renamings(current)));
    }
    else
    {
	set_array_as_distributed(array);
	store_hpf_alignment(array, a);
    }       
}

/* handle a full (re)align directive. 
 * just decompose into simple alignments...
 */
static void 
handle_align_and_realign_directive(entity f,
				   list /* of expressions */ args,
				   bool dynamic)
{
    list last = gen_last(args);
    reference template;

    /* last points to the last item of args, which should be the template
     */
    pips_user_assert("align sg with sg", gen_length(args)>=2);
    template = expression_to_reference(EXPRESSION(CAR(last)));

    gen_map(normalize_all_expressions_of, args);

    if (dynamic) store_renamings(current_stmt_head(), NIL);

    for(; args!=last; POP(args))
	one_align_directive(expression_to_reference(EXPRESSION(CAR(args))), 
			    template, dynamic);
}

/* one DISTRIBUTE directive management
 */
/* returns the expected style tag for the given distribution format,
 * plus a pointer to the list of arguments.
 */
static tag distribution_format(expression e,
			       list /* of expressions */ *pl)
{
    syntax s = expression_syntax(e);
    entity function;
    string name;
    call c;

    pips_assert("valid distribution format", syntax_call_p(s));

    c = syntax_call(s);
    function = call_function(c);
    *pl = call_arguments(c);

    pips_assert("valid distribution format", 
		   hpf_directive_entity_p(function));

    name = entity_local_name(function);
    
    if (same_string_p(name, HPF_PREFIX BLOCK_SUFFIX))  /* BLOCK() */
	return is_style_block;
    else 
    if (same_string_p(name, HPF_PREFIX CYCLIC_SUFFIX)) /* CYCLIC() */
	return is_style_cyclic;
    else
    if (same_string_p(name, HPF_PREFIX STAR_SUFFIX))   /* * [star] */
	return is_style_none;
    else
	pips_user_error("invalid distribution format");

    return 0; /* just to avoid a gcc warning */
}

/*  builds the distribute from the distributee and processor references.
 */
static distribute 
extract_the_distribute(reference distributee, reference proc)
{
    expression parameter = expression_undefined;
    entity processor = reference_variable(proc),
           template = reference_variable(distributee);
    list/* of expressions */   lformat = reference_indices(distributee),
	                       largs,
	/* of distributions */ ldist = NIL;
    int npdim, ntdim;
    tag format;

    if (!entity_template_p(template))
        array_as_template(template);

    ntdim = NumberOfDimension(template);
    npdim = NumberOfDimension(processor);

    pips_user_assert("more template dimensions than processor dimensions",
		     ntdim>=npdim);

    /* the template arguments are scanned to build the distribution
     */
    if (ENDP(lformat)) /* distribute T onto P - implicit */
    {
	int dim;
	for (dim=1; dim<=npdim; dim++)
	    ldist = CONS(DISTRIBUTION,
			 make_distribution(make_style(is_style_block, UU),
					   expression_undefined),
			 ldist);
    }
    else /* explicit distribution */
    {
	for(; !ENDP(lformat); POP(lformat))
	{
	    format = distribution_format(EXPRESSION(CAR(lformat)), &largs);
	    
	    switch (format)
	    {
	    case is_style_block:
	    case is_style_cyclic:
		pips_assert("valid distribution", gen_length(largs)<=1);
		
		parameter = ENDP(largs) ? 
		    expression_undefined :                /* implicit size */
                 copy_expression(EXPRESSION(CAR(largs))); /* explicit size */
		
		break;
	    case is_style_none:
		parameter = expression_undefined;
		break;
	    default:
		pips_internal_error("unexpected style tag (%d)\n", format);
	    }
	    
	    ldist = CONS(DISTRIBUTION, 
			 make_distribution(make_style(format, UU), parameter),
			 ldist);
	}
    }
    
    return make_distribute(gen_nreverse(ldist), processor);
}

/*  handles a simple (one template) distribute or redistribute directive.
 */
static void 
one_distribute_directive(reference distributee,
			 reference proc,
			 bool dynamic)
{
    entity processor = reference_variable(proc),
           template  = reference_variable(distributee);
    distribute d = extract_the_distribute(distributee, proc);

    pips_user_assert("no indices to processor", ENDP(reference_indices(proc)));
    
    normalize_distribute(template, d);

    pips_debug(3, "%s %sdistributed onto %s\n", entity_name(template), 
	       dynamic ? "re" : "", entity_name(processor));

    if (dynamic)
    {
	statement current = current_stmt_head();
	entity new_t;

	pips_user_assert("dynamic template redistribution",
		  entity_template_p(template) && dynamic_entity_p(template));

	new_t = template_synonym_distributed_as(template, d);
	propagate_synonym(current, template, new_t);

	/*  all arrays aligned to template are propagated in turn.
	 */
	MAP(ENTITY, array,
	{
	    align a;
	    entity new_array;

	    pips_debug(7, "array 0x%x\n", (unsigned int) array);
	    pips_debug(7, "alived array %s\n", entity_name(array));
	    
	    a = new_align_with_template(load_hpf_alignment(array), new_t);
	    new_array = array_synonym_aligned_as(array, a);
	    
	    propagate_synonym(current, array, new_array);
	    update_renamings(current, 
			     CONS(RENAMING, make_renaming(array, new_array),
				  load_renamings(current)));

	    pips_debug(9, "done with %s->%s\n", 
		       entity_name(array), entity_name(new_array));
	 },
	    alive_arrays(current, template));
    }
    else
	store_hpf_distribution(template, d);

    pips_debug(4, "out\n");
}

/*  handles a full distribute or redistribute directive.
 */
static void 
handle_distribute_and_redistribute_directive(
    entity f,
    list /* of expressions */ args,
    bool dynamic)
{
    list /* of expression */ last = gen_last(args);
    reference proc;

    if (dynamic) store_renamings(current_stmt_head(), NIL);

    /* last points to the last item of args, which should be the processors
     */
    pips_user_assert("distribute sg with sg", gen_length(args)>=2);
    proc = expression_to_reference(EXPRESSION(CAR(last)));
    gen_map(normalize_all_expressions_of, args);

    /*  calls the simple case handler.
     */
    for(; args!=last; POP(args))
       one_distribute_directive(expression_to_reference(EXPRESSION(CAR(args))), 
				proc, dynamic);
}


/******************************************************* DIRECTIVE HANDLERS */

/* each directive is handled by a function here.
 * these handlers may use the statement stack to proceed.
 * signature: void handle_(DIRECTIVE NAME)_directive (entity f, list args).
 * I may add some handlers for private directives?
 */
#define HANDLER(name) handle_##name##_directive
#define HANDLER_PROTOTYPE(name)\
static void HANDLER(name) (entity f, list /* of expressions */ args)

/*  default case issues an error.
 */
HANDLER_PROTOTYPE(unexpected)
{
    pips_user_error("unexpected hpf directive\n");
}

HANDLER_PROTOTYPE(processors)
{
    gen_map(new_processor, args); /* see new_processor */
}

HANDLER_PROTOTYPE(template)
{
    gen_map(new_template, args); /* see new_template */
}

HANDLER_PROTOTYPE(align)
{
    handle_align_and_realign_directive(f, args, FALSE);
}

HANDLER_PROTOTYPE(distribute)
{
    handle_distribute_and_redistribute_directive(f, args, FALSE);
}

/* ??? I wait for the next statements in a particular order, what
 * should not be necessary. Means I should deal with independent 
 * directives on the PARSED_CODE rather than after the CONTROLIZED.
 */
HANDLER_PROTOTYPE(independent)
{
    list /* of entities */ l = expression_list_to_entity_list(args);
    statement s;

    pips_debug(2, "%d index(es)\n", gen_length(l));

    /*  travels thru the full control graph to find the loops 
     *  and tag them as parallel.
     */
    init_ctrl_graph_travel(current_stmt_head(), gen_true);
    
    while(next_ctrl_graph_travel(&s))
    {
	instruction i = statement_instruction(s);
	
	if (instruction_loop_p(i))  /* what we're looking for */
	{
	    loop o = instruction_loop(i);
	    entity index = loop_index(o);

	    if (ENDP(l)) /* simple independent case, first loop is tagged // */
	    {
		pips_debug(3, "parallel loop\n");

		execution_tag(loop_execution(o)) = is_execution_parallel;
		close_ctrl_graph_travel();
		return;
	    }
	    /*  else general independent case (with a list of indexes)
	     */
	    if (gen_in_list_p(index, l))
	    {
		pips_debug(3, "parallel loop (%s)\n", entity_name(index));

		execution_tag(loop_execution(o)) = is_execution_parallel;
		gen_remove(&l, index);

		if (ENDP(l)) /* the end */
		{
		    close_ctrl_graph_travel();
		    return;
		}
	    }
	}
    }
    
    close_ctrl_graph_travel();
    pips_user_error("some loop not found!\n");
}

/* ??? not implemented and not used. The independent directive is trusted
 * by the compiler to apply its optimizations...
 */
HANDLER_PROTOTYPE(new)
{
    hpfc_warning("not implemented\n");
    return; /* (that's indeed a first implementation:-) */
}

/* should I modify the IR so as to add the reduction protected scalars?
 */
HANDLER_PROTOTYPE(reduction)
{
    hpfc_warning("not implemented\n");
    return;
}

HANDLER_PROTOTYPE(dynamic)
{
    gen_map(new_dynamic, args); /* see new_dynamic */
}

/*   may be used to declare functions as pure. 
 *   ??? it is not a directive in HPF, but I put it this way in F77.
 *   ??? pure declarations are not yet used by HPFC.
 */
HANDLER_PROTOTYPE(pure)
{
    entity module = get_current_module_entity();

    if (ENDP(args))
	add_a_pure(module);
    else
	gen_map(new_pure_function, args);
}

HANDLER_PROTOTYPE(io)
{
    entity module = get_current_module_entity();
    if (ENDP(args))
	add_an_io_function(module);
    else
	gen_map(new_io_function, args);
}

HANDLER_PROTOTYPE(realign)
{
    handle_align_and_realign_directive(f, args, TRUE);
}

HANDLER_PROTOTYPE(redistribute)
{
    handle_distribute_and_redistribute_directive(f, args, TRUE);
}

/*********************************************** handlers for FCD directives */

HANDLER_PROTOTYPE(synchro)
{
    if (get_bool_property(FCD_IGNORE_PREFIX "SYNCHRO"))
	add_statement_to_clean(current_stmt_head());
}

/* for both timeon and timeoff
 */
HANDLER_PROTOTYPE(time)
{
    if (get_bool_property(FCD_IGNORE_PREFIX "TIME"))
	add_statement_to_clean(current_stmt_head());
}

/* for both setbool and setint
 * ??? looks like a hack:-)
 */
HANDLER_PROTOTYPE(set)
{
    if (!get_bool_property(FCD_IGNORE_PREFIX "SET"))
    {
	expression arg1, arg2;
	string property;
	int val, i;
	
	pips_user_assert("two args", gen_length(args)==2);
	arg1 = EXPRESSION(CAR(args));
	arg2 = EXPRESSION(CAR(CDR(args)));
	pips_user_assert("constant args",
		    expression_is_constant_p(arg1) &&
		    expression_is_constant_p(arg2));

	/* property name.
	 * ??? moved to uppers because hpfc_directives put lowers.
	 * ??? plus having to deal with quotes that are put in the name!
	 */
	property = strdup(entity_local_name
	    (call_function(syntax_call(expression_syntax(arg1)))));
	for (i=0; property[i]; i++) property[i]=toupper(property[i]);
	property[i-1]='\0';

	val = HpfcExpressionToInt(arg2);

	if (same_string_p(entity_local_name(f), HPF_PREFIX SETBOOL_SUFFIX))
	    set_bool_property(property+1, val);
	else
	    set_int_property(property+1, val);

	free(property);
    }
    
    add_statement_to_clean(current_stmt_head());
}

/******************************************************** DIRECTIVE HANDLING */

/* finds the handler for a given entity.
 * the link between directive names and handlers is stored in the
 * handlers static table. Some "directives" (BLOCK, CYCLIC) are 
 * unexpected because they cannot appear after the chpf$...
 * Ok, they are not directives, but I put them here as if.
 */
struct DirectiveHandler 
{
  string name;                   /* all names must start with the HPF_PREFIX */
  void (*handler)(entity, list); /* handler for directive "name" */
};

static struct DirectiveHandler handlers[] =
{ 
  /* special functions for HPF keywords are not expected at this level
   */
  {HPF_PREFIX BLOCK_SUFFIX,		HANDLER(unexpected) },
  {HPF_PREFIX CYCLIC_SUFFIX,		HANDLER(unexpected) },
  {HPF_PREFIX STAR_SUFFIX,		HANDLER(unexpected) },

  /* HPF directives
   */
  {HPF_PREFIX ALIGN_SUFFIX,		HANDLER(align) },
  {HPF_PREFIX REALIGN_SUFFIX,		HANDLER(realign) },
  {HPF_PREFIX DISTRIBUTE_SUFFIX,	HANDLER(distribute) },
  {HPF_PREFIX REDISTRIBUTE_SUFFIX,	HANDLER(redistribute) },
  {HPF_PREFIX INDEPENDENT_SUFFIX,	HANDLER(independent) },
  {HPF_PREFIX NEW_SUFFIX,		HANDLER(new) },
  {HPF_PREFIX REDUCTION_SUFFIX,		HANDLER(reduction) }, 
  {HPF_PREFIX PROCESSORS_SUFFIX,	HANDLER(processors) },
  {HPF_PREFIX TEMPLATE_SUFFIX,		HANDLER(template) },
  {HPF_PREFIX DYNAMIC_SUFFIX,		HANDLER(dynamic) },
  {HPF_PREFIX PURE_SUFFIX,		HANDLER(pure) },

  /* FC (Fabien Coelho) directives
   */
  {HPF_PREFIX SYNCHRO_SUFFIX,		HANDLER(synchro) },
  {HPF_PREFIX TIMEON_SUFFIX,		HANDLER(time) },
  {HPF_PREFIX TIMEOFF_SUFFIX,		HANDLER(time) },
  {HPF_PREFIX SETBOOL_SUFFIX,		HANDLER(set) },
  {HPF_PREFIX SETINT_SUFFIX,		HANDLER(set) },
  {HPF_PREFIX HPFCIO_SUFFIX,		HANDLER(io) },

  /* default issues an error
   */
  { (string) NULL,			HANDLER(unexpected) }
};

/* returns the handler for directive name.
 * assumes that name should point to a directive.
 */
static void (*directive_handler(string name))(entity, list)
{
    struct DirectiveHandler *x=handlers;
    while (x->name && strcmp(name, x->name)) x++;
    return x->handler;
}

/* newgen recursion thru the IR.
 */
static bool directive_filter(call c)
{
    entity f = call_function(c);
    
    if (hpf_directive_entity_p(f))
    {
	pips_debug(8, "hpfc entity is %s\n", entity_name(f));

	/* call the appropriate handler for the directive.
	 */
	(directive_handler(entity_local_name(f)))(f, call_arguments(c));
	
	/* the current statement will have to be cleaned.
	 */
	if (!keep_directive_in_code_p(entity_local_name(f)))
	    add_statement_to_clean(current_stmt_head());
    }
    
    return FALSE; /* no instructions within a call! */
}

/* void handle_hpf_directives(s)
 * statement s;
 *
 * what: handles the HPF directives in statement s.
 * how: recurses thru the AST, looking for special "directive calls".
 *      when found, a special handler is called for the given directive.
 * input: the code statement s
 * output: none
 * side effects: (many)
 *  - the hpfc data structures are set/updated to store the hpf mapping.
 *  - parallel loops are tagged parallel.
 *  - a static stack is used to retrieve the current statement.
 *  - the ctrl_graph travelling is used, so should be initialized.
 *  - the special calls are freed and replaced by continues.
 * bugs or features:
 *  - the "new" directive is not used to tag private variables.
 *  - a non hpf "pure" directive is parsed.
 */
void handle_hpf_directives(statement s)
{
    make_current_stmt_stack();
    init_dynamic_locals();
    init_the_dynamics();

    to_be_cleaned = NIL;
    store_renamings(s, NIL);

    gen_multi_recurse(s,
        statement_domain,  current_stmt_filter, current_stmt_rewrite, 
	expression_domain, gen_false,           gen_null,
	call_domain,       directive_filter,    gen_null,
		      NULL);

    initial_alignment(s);

    DEBUG_STAT(7, "intermediate code", s);

    simplify_remapping_graph();
    gen_map(clean_statement, to_be_cleaned);
    pips_assert("empty stack", current_stmt_empty_p());

    gen_free_list(to_be_cleaned), to_be_cleaned=NIL;
    free_current_stmt_stack();
    close_dynamic_locals();
    close_the_dynamics();

    DEBUG_CODE(5, "resulting code", get_current_module_entity(), s);
}

/* that is all
 */

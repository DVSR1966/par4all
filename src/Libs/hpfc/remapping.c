/* HPFC module by Fabien COELHO
 *
 * $RCSfile: remapping.c,v $ version $Revision$
 * ($Date: 1995/09/18 13:23:29 $, ) 
 *
 * generates a remapping code. 
 * debug controlled with HPFC_REMAPPING_DEBUG_LEVEL.
 * ??? should drop the remapping domain.
 */

#include <stdarg.h>
#include "defines-local.h"
#include "conversion.h"

/* for USAGE information
 */
#include <sys/time.h>
#include <sys/resource.h>
int getrusage(int, struct rusage*); /* not found in any header! */

entity CreateIntrinsic(string name); /* in syntax */

/* linearize the processor arrangement on which array is distributed
 * or replicated (driven by distributed). The variables are those created 
 * or returned by the create_var function. var is set as the linearization 
 * result, if not NULL. *psize the the extent of the returned array.
 * Special care is taken about the declaration shifts.
 */
static Pcontrainte
partial_linearization(array, distributed, var, psize, create_var)
entity array;
bool distributed;
entity var;
int *psize;
entity (*create_var)(/* int */);
{
    entity proc = array_to_processors(array);
    int dim = NumberOfDimension(proc), low, up, size;
    Pvecteur v = VECTEUR_NUL;

    for(*psize=1; dim>0; dim--)
	if (distributed ^ processors_dim_replicated_p(proc, array, dim))
	{
	    get_entity_dimensions(proc, dim, &low, &up);
	    size = up - low + 1;
	    *psize *= size;
	    v = vect_multiply(v, size);
	    vect_add_elem(&v, (Variable) create_var(dim), 1);
	    vect_add_elem(&v, TCST, - low);
	}

    if (var) vect_add_elem(&v, (Variable) var, -1);

    return contrainte_make(v);
}

/* builds a linarization equation of the dimensions of obj.
 * var is set to the result. *psize returns the size of obj.
 * Done the Fortran way, or the other way around...
 */ 
Pcontrainte
full_linearization(
    entity obj,
    entity var,
    int *psize,
    entity (*create_var)(int),
    bool fortran_way,
    int initial_offset)
{
    int dim = NumberOfDimension(obj), low, up, size, i;
    Pvecteur v = VECTEUR_NUL;

    for(*psize=1, i=fortran_way ? dim : 1 ;
	fortran_way ? i>0 : i<=dim; 
	i+= fortran_way ? -1 : 1)
    {
	get_entity_dimensions(obj, i, &low, &up);
	size = up - low + 1;
	*psize *= size;
	v = vect_multiply(v, size);
	vect_add_elem(&v, (Variable) create_var(i), 1);
	vect_add_elem(&v, TCST, - low);
    }

    if (var) vect_add_elem(&v, (Variable) var, -1);
    if (initial_offset) vect_add_elem(&v, TCST, initial_offset);

    return contrainte_make(v);
}

static Psysteme 
generate_work_sharing_system(src, trg)
entity src, trg;
{
    entity psi_r = get_ith_temporary_dummy(1),
           psi_d = get_ith_temporary_dummy(2),
           delta = get_ith_temporary_dummy(3);
    int size_r, size_d;
    Psysteme sharing = sc_new();

    sc_add_egalite(sharing, partial_linearization(src, FALSE, psi_r, &size_r, 
						  get_ith_processor_dummy));
    sc_add_egalite(sharing, partial_linearization(trg, TRUE, psi_d, &size_d,
						  get_ith_processor_prime));
    
    /* psi_d = psi_r + |psi_r| delta
     */
    sc_add_egalite(sharing, contrainte_make(vect_make
	(VECTEUR_NUL, psi_d, -1, psi_r, 1, delta, size_r, TCST, 0)));

    if (size_d >= size_r)
    {
	/* 0 <= delta (there are cycles)
	 */
	sc_add_inegalite(sharing, contrainte_make
			 (vect_make(VECTEUR_NUL, delta, -1, TCST, 0)));
    }
    else
    {
	/* delta == 0
	 */
	sc_add_egalite(sharing, contrainte_make(vect_new((Variable) delta, 1)));
    }

    sc_creer_base(sharing);

    return sharing;
}

static Psysteme 
generate_remapping_system(src, trg)
entity src, trg;
{
    int ndim = variable_entity_dimension(src);
    Psysteme 
	result = sc_rn(NULL),
	s_src = generate_system_for_distributed_variable(src),
	s_trg = shift_system_to_prime_variables
	    (generate_system_for_distributed_variable(trg)),
	s_equ = generate_system_for_equal_variables
	    (ndim, get_ith_array_dummy, get_ith_array_prime),
	s_shr = generate_work_sharing_system(src, trg);
    
    DEBUG_SYST(6, concatenate("source ", entity_name(src), NULL), s_src);
    DEBUG_SYST(6, concatenate("target ", entity_name(trg), NULL), s_trg);
    DEBUG_SYST(6, "link", s_equ);
    DEBUG_SYST(6, "sharing", s_shr);

    result = sc_append(result, s_src), sc_rm(s_src);
    result = sc_append(result, s_trg), sc_rm(s_trg);
    result = sc_append(result, s_equ), sc_rm(s_equ);
    result = sc_append(result, s_shr), sc_rm(s_shr);

    return result;
}

/*   ??? assumes that there are no parameters. what should be the case...
 */
static void 
remapping_variables(s, a1, a2, pl, plp, pll, plrm, pld, plo)
Psysteme s;
entity a1, a2;
list *pl, 	/* P */
     *plp, 	/* P' */
     *pll, 	/* locals */
     *plrm, 	/* to remove */
     *pld,      /* diffusion processor variables */
     *plo;	/* others */
{
    entity
	t1 = array_to_template(a1),
	p1 = template_to_processors(t1),
	t2 = array_to_template(a2),
	p2 = template_to_processors(t2);
    int
	a1dim = variable_entity_dimension(a1),
	a2dim = variable_entity_dimension(a2),
	t1dim = variable_entity_dimension(t1),
	t2dim = variable_entity_dimension(t2),
	p1dim = variable_entity_dimension(p1),
	p2dim = variable_entity_dimension(p2), i;

    /*   processors.
     */
    *pl  = NIL; add_to_list_of_vars(*pl, get_ith_processor_dummy, p1dim);
    *plp = NIL; add_to_list_of_vars(*plp, get_ith_processor_prime, p2dim);

    *pld = NIL; 
    for (i=p2dim; i>0; i--)
	if (processors_dim_replicated_p(p2, a2, i))
	    *pld = CONS(ENTITY, get_ith_processor_prime(i), *pld);

    /*   to be removed.
     */
    *plrm = NIL;
    add_to_list_of_vars(*plrm, get_ith_template_dummy, t1dim);
    add_to_list_of_vars(*plrm, get_ith_template_prime, t2dim);
    add_to_list_of_vars(*plrm, get_ith_array_dummy, a1dim);
    add_to_list_of_vars(*plrm, get_ith_array_prime, a2dim);

    /*    corresponding equations generated in the sharing system
     */
    add_to_list_of_vars(*plrm, get_ith_temporary_dummy, 2);
    

    /*    Replicated dimensions associated variables must be removed.
     *    A nicer approach would have been not to generate them, but
     * it's not so hard to remove them, and the equation generation is
     * kept simpler this way. At least in my own opinion:-)
     */
    for (i=p1dim; i>0; i--)
	if (processors_dim_replicated_p(p1, a1, i))
	    *plrm = CONS(ENTITY, get_ith_block_dummy(i), 
		    CONS(ENTITY, get_ith_cycle_dummy(i), *plrm));

    for (i=p2dim; i>0; i--)
	if (processors_dim_replicated_p(p2, a2, i))
	    *plrm = CONS(ENTITY, get_ith_block_prime(i), 
		    CONS(ENTITY, get_ith_cycle_prime(i), *plrm));

    /*   locals.
     */
    *pll = NIL;
    add_to_list_of_vars(*pll, get_ith_local_dummy, a1dim);
    add_to_list_of_vars(*pll, get_ith_local_prime, a2dim);

    /*   others.
     */
    *plo = base_to_list(sc_base(s)); 
    gen_remove(plo, (entity) TCST);

    MAP(ENTITY, e, gen_remove(plo, e), *pl);
    MAP(ENTITY, e, gen_remove(plo, e), *plp);
    MAP(ENTITY, e, gen_remove(plo, e), *plrm);
    MAP(ENTITY, e, gen_remove(plo, e), *pll);

    DEBUG_ELST(7, "P", *pl);
    DEBUG_ELST(7, "P'", *plp);
    DEBUG_ELST(7, "RM", *plrm);
    DEBUG_ELST(7, "LOCALS", *pll);
    DEBUG_ELST(7, "DIFFUSION", *pld);
    DEBUG_ELST(7, "OTHERS", *plo);
}

/* to be generated:
 * ??? the Proc cycle should be deduce directly in some case...
 *
 *   PSI_i's definitions
 *   [ IF (I AM IN S(PSI_i)) THEN ]
 *     DO OTH_i's in S(OTH_i's)[PSI_i's]
 *       LID computation(OTH_i's) // if LID is not NULL!
 *       body
 *     ENDDO
 *   [ ENDIF ]
 */
static statement
processor_loop(
    Psysteme s,
    list /* of entities */ l_psi,
    list /* of entities */ l_oth,
    entity psi,
    entity oth,
    entity lid, 
    entity array,
    entity (*create_psi)(int),
    entity (*create_oth)(int),
    statement body,
    boolean sh) /* whether to shift the psi's */
{
    entity divide = hpfc_name_to_entity(IDIVIDE);
    Psysteme condition, enumeration, known, simpler;
    statement define_psis, compute_lid, oth_loop, if_guard;
    
    define_psis = define_node_processor_id(psi, create_psi);

    /* the lid computation is delayed in the body for diffusions
     */
    compute_lid = (lid) ?
	hpfc_compute_lid(lid, oth, create_oth, array) : 
	make_empty_statement(); 

    /* simplifies the processor arrangement for the condition
     */
    known = sc_dup(entity_to_declaration_constraints(psi));
    if (sh) known = shift_system_to_prime_variables(known);

    DEBUG_SYST(7, "initial system", s);
    DEBUG_ELST(7, "loop indexes", l_oth);

    hpfc_algorithm_row_echelon(s, l_oth, &condition, &enumeration);

    DEBUG_SYST(7, "P condition", condition);

    simpler = extract_nredund_subsystem(condition, known);
    sc_rm(condition), sc_rm(known);

    /*  the processor must be in the psi processor arrangement
     */
    sc_add_inegalite(simpler, 
		     contrainte_make
		     (vect_make(VECTEUR_NUL,
				hpfc_name_to_entity(MYLID), 1, 
				TCST, - NumberOfElements
      (variable_dimensions(type_variable(entity_type(psi)))))));

    DEBUG_SYST(5, "P simpler", simpler);
    DEBUG_SYST(5, "P enumeration", enumeration);

    /* target processors enumeration loop
     */
    oth_loop = systeme_to_loop_nest(enumeration, l_oth, 
	       make_block_statement(CONS(STATEMENT, compute_lid,
				    CONS(STATEMENT, body,
					 NIL))), divide);

    if_guard = generate_optional_if(simpler, oth_loop);

    sc_rm(simpler); sc_rm(enumeration);

    return make_block_statement(CONS(STATEMENT, define_psis,
				CONS(STATEMENT, if_guard,
				     NIL)));
}

/* to be generated:
 *
 *   DO ll's in S(ll)[...]
 *     DEDUCABLES(ld)
 *     body
 *   ENDDO
 */
static statement
elements_loop(
    Psysteme s,
    list /* of entities */ ll, 
    list /* of expressions */ ld,
    statement body)
{
    return systeme_to_loop_nest(s, ll,
	   make_block_statement(CONS(STATEMENT, generate_deducables(ld),
				CONS(STATEMENT, body,
				     NIL))), 
				hpfc_name_to_entity(IDIVIDE));
}

/* to be generated:
 * 
 *   IF (MYLID.NE.LID)
 *   THEN true
 *   ELSE false
 *   ENDIF
 */
static statement
if_different_pe(
    entity lid,
    statement true,
    statement false)
{
    return test_to_statement
      (make_test(MakeBinaryCall(CreateIntrinsic(NON_EQUAL_OPERATOR_NAME),
	   entity_to_expression(hpfc_name_to_entity(MYLID)),
	   entity_to_expression(lid)),
		      true, false));
}

/* builds the diffusion loop.
 *
 * [ IF (LAZY_SEND) THEN ]
 *   DO ldiff in sr
 *     LID computation(...)
 *     IF (MYLID.NE.LID) send to LID
 *   ENDDO
 * [ ENDIF ]
 */
static statement 
broadcast(
    entity lid,
    entity proc,
    Psysteme sr,
    list /* of entity */ ldiff,
    bool lazy)
{
    statement cmp_lid, body, nest;

    cmp_lid = hpfc_compute_lid(lid, proc, get_ith_processor_prime, NULL);
    body = make_block_statement
	(CONS(STATEMENT, cmp_lid,
	 CONS(STATEMENT, if_different_pe
	      (lid, hpfc_generate_message(lid, TRUE, FALSE),
	       make_empty_statement()),
	      NIL)));

    nest = systeme_to_loop_nest(sr, ldiff, body, hpfc_name_to_entity(IDIVIDE));

    return lazy ? hpfc_lazy_guard(TRUE, nest) : nest ;
}

/* in the following functions tag t controls the code generation, 
 * depending of what is to be generated (send, receive, copy, broadcast)...
 * tag t may have the following values:
 */

#define CPY	0
#define SND	1
#define RCV	2
#define BRD     3

#define PRE 	0
#define INL 	4
#define PST 	8

#define NLZ 	0
#define LZY 	16

#define NBF 	0
#define BUF 	32

/* I have to deal with a 4D space to generate some code:
 *
 * buffer/nobuf
 * lazy/nolazy
 *
 * pre/in/post
 * cpy/snd/rcv/brd
 */

#define ret(name) result = name; break

static statement 
gen(int what,
    entity src, entity trg, entity lid, entity proc,
    entity (*create_src)(), entity (*create_trg)(),
    Psysteme sr, list /* of entity */ ldiff)
{
    statement result;
    bool is_lazy = what & LZY, 
         is_buff = what & BUF;

    switch (what)
    {
	/* cpy pre/post
	 * rcv post nbuf
	 */
    case CPY+PRE: 
    case CPY+PST:
    case CPY+PRE+LZY: 
    case CPY+PST+LZY:
    case CPY+PRE+BUF:
    case CPY+PST+BUF:
    case CPY+PRE+LZY+BUF: 
    case CPY+PST+LZY+BUF:
    case RCV+PST: 
    case RCV+PST+LZY:
    case RCV+PST+BUF:
    case RCV+PST+LZY+BUF:
	ret(make_empty_statement());

	/* snd.brd pre nbuf
	 */
    case SND+PRE: 
    case BRD+PRE:
    case SND+PRE+LZY: 
    case BRD+PRE+LZY: 
	ret(hpfc_initsend(is_lazy));

    case RCV+PRE+LZY:
	ret(set_logical(hpfc_name_to_entity(LAZY_RECV), TRUE));

    case RCV+PRE:
	ret(hpfc_generate_message(lid, FALSE, FALSE));

	/* cpy inl 
	 */
    case CPY+INL: 
    case CPY+INL+LZY:
    case CPY+INL+BUF:
    case CPY+INL+LZY+BUF:
	ret(make_assign_statement(make_reference_expression(trg, create_trg),
				  make_reference_expression(src, create_src)));

	/* snd/brd inl nbf
	 */
    case SND+INL:
    case BRD+INL:
    case SND+INL+LZY:
    case BRD+INL+LZY:
	ret(hpfc_lazy_packing(src, lid, create_src, TRUE, is_lazy));

    case SND+INL+BUF:
    case SND+INL+LZY+BUF:
    case BRD+INL+BUF:
    case BRD+INL+LZY+BUF:
	ret(hpfc_lazy_buffer_packing(src, trg, lid, proc, create_src,
				     TRUE /* send! */, is_lazy));
    case RCV+INL+BUF:
    case RCV+INL+LZY+BUF:
	ret(hpfc_lazy_buffer_packing(trg, trg, lid, proc, create_trg,
				     FALSE /* receive! */, is_lazy));
    case RCV+INL:
    case RCV+INL+LZY:
	ret(hpfc_lazy_packing(trg, lid, create_trg, FALSE, is_lazy));

    case SND+PST:
    case SND+PST+LZY:
	ret(hpfc_generate_message(lid, TRUE, is_lazy));

    case BRD+PST:
    case BRD+PST+LZY:
	ret(broadcast(lid, proc, sr, ldiff, is_lazy));

    case SND+PST+BUF:
    case SND+PST+LZY+BUF:
    case BRD+PST+BUF:
    case BRD+PST+LZY+BUF:
	ret(hpfc_broadcast_if_necessary(src, trg, lid, proc, is_lazy));

	/* snd/rcv pre buf
	 */
    case SND+PRE+BUF:
    case SND+PRE+LZY+BUF:
    case BRD+PRE+BUF:
    case BRD+PRE+LZY+BUF:
	ret(hpfc_buffer_initialization(TRUE /* send! */, is_lazy));
    case RCV+PRE+BUF:
    case RCV+PRE+LZY+BUF:
	ret(hpfc_buffer_initialization(FALSE /* receive! */, is_lazy));
	
	/* default is a forgotten case, I guess
	 */
    default:
	pips_error("gen", "invalid tag %d\n", what);
	ret(statement_undefined); /* to avoid a gcc warning */
    }

    pips_debug(7, "tag is %d (%s, %s)\n", what, 
	       is_lazy ? "lazy" : "not lazy",
	       is_buff ? "bufferized" : "not bufferized");
    DEBUG_STAT(7, "returning", result);

    return result;
}

static statement 
remapping_stats(
    int t,
    Psysteme s,
    Psysteme sr,
    list /* of entity */ ll,
    list /* of entity */ ldiff, 
    list /* of expressions */ ld,
    entity lid,
    entity src,
    entity trg)
{
    entity proc = array_to_processors(trg);
    statement inner, prel, postl, result;
    bool is_buffer, is_lazy;
    int what;

    is_lazy = get_bool_property(LAZY_MESSAGES);
    is_buffer = get_bool_property(USE_BUFFERS);
    what = (is_lazy ? LZY : NLZ) + (is_buffer ? BUF : NBF) + t;
    
    prel  = gen(what+PRE, src, trg, lid, proc, 
		get_ith_local_dummy, get_ith_local_prime, sr, ldiff);
    inner = gen(what+INL, src, trg, lid, proc, 
		get_ith_local_dummy, get_ith_local_prime, sr, ldiff);
    postl = gen(what+PST, src, trg, lid, proc, 
		get_ith_local_dummy, get_ith_local_prime, sr, ldiff);

    result = make_block_statement
	    (CONS(STATEMENT, prel,
	     CONS(STATEMENT, elements_loop(s, ll, ld, inner),
	     CONS(STATEMENT, postl,
		  NIL))));

    statement_comments(result) = 
	strdup(concatenate("c - ", (what & LZY) ? "lazy " : "",
	   t==CPY ? "copy" : t==SND ? "send" : t==RCV ? "receiv" :
			   t==BRD ? "broadcast" : "?",  "ing\n", NULL));

    return result;
}

void 
sc_separate_on_vars(s, b, pwith, pwithout)
Psysteme s;
Pbase b;
Psysteme *pwith, *pwithout;
{
    Pcontrainte i_with, e_with, i_without, e_without;

    Pcontrainte_separate_on_vars(sc_inegalites(s), b, &i_with, &i_without);
    Pcontrainte_separate_on_vars(sc_egalites(s), b, &e_with, &e_without);

    *pwith = sc_make(e_with, i_with),
    *pwithout = sc_make(e_without, i_without);
}

static statement
generate_remapping_code(src, trg, procs, locals, l, lp, ll, ldiff, ld, dist_p)
entity src, trg;
Psysteme procs, locals;
list /* of entities */ l, lp, ll, ldiff, /* of expressions */ ld;
bool dist_p; /* true if must take care of lambda */
{
    entity lid = hpfc_name_to_entity(T_LID),
           p_src = array_to_processors(src),
           p_trg = array_to_processors(trg),
           lambda = get_ith_temporary_dummy(3),
           primary = load_primary_entity(src);
    statement copy, recv, send, receive, cont, result;
    bool is_buffer = get_bool_property(USE_BUFFERS);

    pips_debug(3, "%s taking care of processor cyclic distribution\n", 
	       dist_p ? "actually" : "not");

    copy = remapping_stats(CPY, locals, SC_EMPTY, ll, NIL, ld, lid, src, trg);
    recv = remapping_stats(RCV, locals, SC_EMPTY, ll, NIL, ld, lid, src, trg);

    if (dist_p) lp = CONS(ENTITY, lambda, lp);

    /* the send is different for diffusions
     */
    if (ENDP(ldiff))
    {
	statement rp_send;

	rp_send =
	    remapping_stats(SND, locals, SC_EMPTY, ll, NIL, ld, lid, src, trg);
    
	send = processor_loop
	    (procs, l, lp, p_src, p_trg, lid, NULL,
	     get_ith_processor_dummy, get_ith_processor_prime,
	     if_different_pe(lid, rp_send, make_empty_statement()), FALSE);
    }
    else
    {
	Pbase b = entity_list_to_base(ldiff);
	list lpproc = gen_copy_seq(lp);
	statement diff;
	Psysteme 
	    sd /* distributed */, 
	    sr /* replicated */;

	MAP(ENTITY, e, gen_remove(&lpproc, e), ldiff);

	/* polyhedron separation to extract the diffusions.
	 */
	sc_separate_on_vars(procs, b, &sr, &sd);
	base_rm(b);

	diff = remapping_stats(BRD, locals, sr, ll, ldiff, ld, lid, src, trg);

	send = processor_loop
	    (sd, l, lpproc, p_src, p_trg, lid, is_buffer ? trg : NULL,
	     get_ith_processor_dummy, get_ith_processor_prime,
	     diff, FALSE);

	gen_free_list(lpproc); sc_rm(sd); sc_rm(sr);
    }

    if (dist_p) 
    {
	gen_remove(&lp, lambda);
	l = gen_nconc(l, CONS(ENTITY, lambda, NIL)); /* ??? to be deduced */
    }

    receive = processor_loop
	(procs, lp, l, p_trg, p_src, lid, NULL,
	 get_ith_processor_prime, get_ith_processor_dummy,
	 if_different_pe(lid, recv, copy), TRUE);

    if (dist_p) gen_remove(&l, lambda);

    cont = make_empty_statement();

    result = make_block_statement(CONS(STATEMENT, send,
				  CONS(STATEMENT, receive,
				  CONS(STATEMENT, cont,
				       NIL))));

    /*   some comments to help understand the generated code
     */
    {
	char buffer[128];
	
	sprintf(buffer, "c remapping %s[%d]: %s[%d] -> %s[%d]\n",
		entity_local_name(primary), load_hpf_number(primary),
		entity_local_name(src), load_hpf_number(src),
		entity_local_name(trg), load_hpf_number(trg));
	
	statement_comments(result) = strdup(buffer);
	statement_comments(send) = strdup("c send part\n");
	statement_comments(receive) = strdup("c receive part\n");
	statement_comments(cont) = strdup("c end of remapping\n");
    }
    
    DEBUG_STAT(3, "result", result);
    
    return result;
}

/* IF (MSTATUS(primary_number).eq.src_number) THEN
 *   the_code
 *   MSTATUS(primary_number) = trg_number
 * ENDDIF
 */
static statement
generate_remapping_guard(src, trg, the_code)
entity src, trg;
statement the_code;
{
    int src_n = load_hpf_number(src), /* source, target and primary numbers */
        trg_n = load_hpf_number(trg),
        prm_n = load_hpf_number(load_primary_entity(src));
    entity m_status = hpfc_name_to_entity(MSTATUS); /* mapping status */
    expression m_stat_ref, cond;
    statement affecte, result;
    
    /* MSTATUS(primary_n) */
    m_stat_ref =
	reference_to_expression
	    (make_reference(m_status, 
			    CONS(EXPRESSION, int_to_expression(prm_n), NIL)));
    
    /* MSTATUS(primary_number) = trg_number */
    affecte = make_assign_statement(copy_expression(m_stat_ref), 
				    int_to_expression(trg_n));

    /* MSTATUS(primary_number).eq.src_number */
    cond = MakeBinaryCall(CreateIntrinsic(EQUAL_OPERATOR_NAME),
			  m_stat_ref, int_to_expression(src_n));

    result = test_to_statement(make_test(cond, 
      make_block_statement(CONS(STATEMENT, the_code,
			   CONS(STATEMENT, affecte, 
				NIL))),
      make_empty_statement()));

    return result;
}


/* to print the micros with leading and trailing 0s if needed
 */
static char *
string_micros(long micros)
{
    static char buffer[7];
    int i;

    sprintf(buffer, "%6ld", micros);

    for (i=0; i<6; i++)
	if (buffer[i]==' ') buffer[i]='0' ;

    return buffer;
}

static void 
print_rusage_delta(FILE * buffer,
		   struct rusage *in, 
		   struct rusage *out,
		   char * format,
		   ...)
{
    long u_seconds, u_micros, s_seconds, s_micros;

    va_list args;
    va_start(args, format);
    vfprintf(buffer, format, args);
    va_end(args);

    u_seconds = out->ru_utime.tv_sec - in->ru_utime.tv_sec;
    u_micros = out->ru_utime.tv_usec - in->ru_utime.tv_usec;
    if (u_micros<0) u_seconds++, u_micros+=1000000;

    s_seconds = out->ru_stime.tv_sec - in->ru_stime.tv_sec;
    s_micros = out->ru_stime.tv_usec - in->ru_stime.tv_usec;
    if (s_micros<0) s_seconds++, s_micros+=1000000;

    fprintf(buffer, "%ld.%suser ",  u_seconds, string_micros(u_micros));
    fprintf(buffer, "%ld.%ssystem\n", s_seconds, string_micros(s_micros));
}

/*  remaps src to trg.
 */
static statement 
hpf_remapping(src, trg)
entity src, trg;
{
    Psysteme p, proc, enume;
    statement s;
    entity lambda = get_ith_temporary_dummy(3) ; /* P cycle */
    bool proc_distribution_p;
    list /* of entities */ l, lp, ll, lrm, ld, lo, left, scanners,
         /* of expressions */ lddc;
    struct rusage start, stop;
    bool time_remapping = get_bool_property("HPFC_TIME_REMAPPINGS");

    if (time_remapping) getrusage(RUSAGE_SELF, &start);

    pips_debug(3, "%s -> %s\n", entity_name(src), entity_name(trg));

    if (src==trg) return make_empty_statement(); /* (optimization:-) */

    /*   builds and simplifies the systems.
     */
    p = generate_remapping_system(src, trg);
    remapping_variables(p, src, trg, &l, &lp, &ll, &lrm, &ld, &lo);
    clean_the_system(&p, &lrm, &lo);
    lddc = simplify_deducable_variables(p, ll, &left);
    gen_free_list(ll);
    
    /* the P cycle ?
     */
    proc_distribution_p = gen_in_list_p(lambda, lo);

    if (proc_distribution_p) gen_remove(&lo, lambda);

    scanners = gen_nconc(lo, left);

    DEBUG_SYST(4, "cleaned system", p);
    DEBUG_ELST(4, "scanners", scanners);

    /*   processors/array elements separation.
     */
    hpfc_algorithm_row_echelon(p, scanners, &proc, &enume);
    sc_rm(p);

    DEBUG_SYST(3, "proc", proc);
    DEBUG_SYST(3, "enum", enume);

    /*   generates the code.
     */
    s = generate_remapping_guard(src, trg, generate_remapping_code
      (src, trg, proc, enume, l, lp, scanners, ld, lddc, proc_distribution_p));

    /*   clean.
     */
    sc_rm(proc), sc_rm(enume);
    gen_free_list(scanners);
    gen_free_list(l), gen_free_list(lp), gen_free_list(ld);
    gen_map(gen_free, lddc), gen_free_list(lddc);  /* ??? */
    
    if (time_remapping)
    {
	getrusage(RUSAGE_SELF, &stop);
	print_rusage_delta(stderr, &start, &stop, 
	    "remapping %s -> %s: ", entity_name(src), entity_name(trg));
    }

    DEBUG_STAT(6, "result", s);

    return s;
}

/* void remapping_compile(s, hsp, nsp)
 * statement s, *hsp, *nsp;
 *
 * what: generates the remapping code for s.
 * how: polyhedron technique.
 * input: s, the statement.
 * output: statements *hsp and *nsp, the host and SPMD node code.
 * side effects: (none?)
 * bugs or features:
 */
void remapping_compile(s, hsp, nsp)
statement s, *hsp /* Host Statement Pointer */, *nsp /* idem Node */;
{
    list /* of statements */ l = NIL;

    debug_on("HPFC_REMAPPING_DEBUG_LEVEL");
    pips_debug(1, "dealing with statement 0x%x\n", (unsigned int) s);

    *hsp = make_empty_statement(); /* nothing for host */

    MAP(RENAMING, r,
	l = CONS(STATEMENT, hpf_remapping(renaming_old(r), renaming_new(r)), l),
	load_renamings(s));

    *nsp = make_block_statement(l); /* block of remaps for the nodes */

    DEBUG_STAT(8, "result", *nsp);

    debug_off();
}

/* that is all
 */

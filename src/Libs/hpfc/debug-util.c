/* this is a set of functions to help hpfc debugging
 *
 * $RCSfile: debug-util.c,v $ (version $Revision$)
 * $Date: 1996/03/01 13:00:17 $, 
 *
 * Fabien Coelho, May 1993.
 */

#include "defines-local.h"
#include "pipsdbm.h"

extern char *flint_print_basic(basic);

/* print_entity_variable(e)
 * 
 * if it is just a variable, the type is printed,
 * otherwise just the entity name is printed
 */

static void print_dimension(d)
dimension d;
{
    fprintf(stderr,"dimension :\n");
    print_expression(dimension_lower(d));
    print_expression(dimension_upper(d));
}

void print_entity_variable(e)
entity e;
{
    variable v;

    (void) fprintf(stderr,"name: %s\n",entity_name(e));
    
    if (!type_variable_p(entity_type(e)))
	return;

    v = type_variable(entity_type(e));

    fprintf(stderr,"basic %s\n",flint_print_basic(variable_basic(v)));
    gen_map(print_dimension, variable_dimensions(v));
}

void print_align(a)
align a;
{
    (void) fprintf(stderr, "aligned (%d dimensions)\n", 
		   gen_length(align_alignment(a)));
    gen_map(print_alignment, align_alignment(a));
    (void) fprintf(stderr, "to template %s\n\n",
		   entity_name(align_template(a)));
}

void print_alignment(a)
alignment a;
{
    (void) fprintf(stderr,
		   "Alignment: arraydim %d, templatedim %d,\n",
		   alignment_arraydim(a),
		   alignment_templatedim(a));
    
    (void) fprintf(stderr,"rate: ");
    print_expression(alignment_rate(a));
    (void) fprintf(stderr,"\nconstant: ");
    print_expression(alignment_constant(a));
    (void) fprintf(stderr,"\n");
}

void print_aligns()
{
    fprintf(stderr,"Aligns:\n");
    MAP(ENTITY, a,
     {
	 (void) fprintf(stderr, "of array %s\n", entity_name(a));
	 print_align(load_hpf_alignment(a));
	 (void) fprintf(stderr,"\n");
     },
	list_of_distributed_arrays());
}

void print_distributes()
{
    fprintf(stderr,"Distributes:\n");

    MAP(ENTITY, t,
     {
	 (void) fprintf(stderr, "of template %s\n", entity_name(t));
	 print_distribute(load_hpf_distribution(t));
	 (void) fprintf(stderr,"\n");
     },
	 list_of_templates());
    
}

void print_distribute(d)
distribute d;
{
    (void) fprintf(stderr,"distributed\n");

    gen_map(print_distribution, distribute_distribution(d));

    (void) fprintf(stderr, "to processors %s\n\n", 
		   entity_name(distribute_processors(d)));    
}

void print_distribution(d)
distribution d;
{
    switch(style_tag(distribution_style(d)))
    {
    case is_style_none:
	(void) fprintf(stderr,"none, ");
	break;
    case is_style_block:
	(void) fprintf(stderr,"BLOCK(");
	print_expression(distribution_parameter(d));
	(void) fprintf(stderr,"), ");
	break;
    case is_style_cyclic:
	(void) fprintf(stderr,"CYCLIC(");
	print_expression(distribution_parameter(d));
	(void) fprintf(stderr,"), ");
	break;
    default:
	pips_internal_error("unexpected style tag\n");
	break;
    }
    (void) fprintf(stderr,"\n");
}

void print_hpf_dir()
{
    (void) fprintf(stderr,"HPF directives:\n");

    print_templates();
    (void) fprintf(stderr,"--------\n");
    print_processors();
    (void) fprintf(stderr,"--------\n");
    print_distributed_arrays();
    (void) fprintf(stderr,"--------\n");
    print_aligns();
    (void) fprintf(stderr,"--------\n");
    print_distributes();
}

void print_templates()
{
    (void) fprintf(stderr,"Templates:\n");
    gen_map(print_entity_variable, list_of_templates());
}

void print_processors()
{
    (void) fprintf(stderr,"Processors:\n");
    gen_map(print_entity_variable, list_of_processors());
}

void print_distributed_arrays()
{
    (void) fprintf(stderr,"Distributed Arrays:\n");
    gen_map(print_entity_variable, list_of_distributed_arrays());
}

/* old name of obj while in module now.
 */
static string 
old_name(
    entity module, /* module in which obj appears */
    entity obj)    /* obj */
{
    return module_local_name
        (module==host_module ? load_old_host(obj) : load_old_node(obj));
}

/* to be used by the prettyprinter at the head of a file.
 * inclusion of needed runtime headers.
 */
static string
hpfc_head_hook(
    entity m) /* module */
{
    return strdup(concatenate
        ("      include \"fpvm3.h\"\n"
	 "      include \"real_parameters.h\"\n"
	 "      include \"hpfc_commons.h\"\n"
	 "      include \"hpfc_includes.h\"\n"
	 "      include \"", old_name(m, m), "_parameters.h\"\n", NULL));
}

/* to be used by the prettyprinter when dealing with a common.
 * inclusion of the parameters and commons...
 */
static string 
hpfc_common_hook(
    entity module,
    entity common)
{
    return strdup(concatenate
        ("      include \"", old_name(module, common), "_parameters.h\"\n"
	 "      include \"", old_name(module, common),  
	 module==host_module ? "_host.h\"\n" : "_node.h\"\n", NULL));
}

void hpfc_print_code(file, module, stat)
FILE* file;
entity module;
statement stat;
{
    text t;
    debug_on("PRETTYPRINT_DEBUG_LEVEL");

    set_prettyprinter_head_hook(hpfc_head_hook);
    set_prettyprinter_common_hook(hpfc_common_hook);

    t = text_module(module, stat);
    print_text(file, t);
    /* free_text(t); */ /* ??? memory leak or core dump... */
    
    reset_prettyprinter_common_hook();
    reset_prettyprinter_head_hook();

    debug_off();
}

void hpfc_print_common(file, module, common)
FILE *file;
entity module, common;
{
    text t;
    debug_on("PRETTYPRINT_DEBUG_LEVEL");

    t = text_common_declaration(common, module);
    print_text(file, t);
    free_text(t);
    
    debug_off();
}

void hpfc_print_file(file_name)
string file_name;
{
    string dir_name = db_get_current_workspace_directory();

    safe_system(concatenate("cat ", dir_name, "/", file_name, " >&2", NULL));
}

void fprint_range(file, r)
FILE* file;
range r;
{
    int lo, up, in;
    bool
	blo = hpfc_integer_constant_expression_p(range_lower(r), &lo),
	bup = hpfc_integer_constant_expression_p(range_upper(r), &up),
	bin = hpfc_integer_constant_expression_p(range_increment(r), &in);

    if (blo && bup && bin)
    {
	if (in==1)
	    if (lo==up)
		fprintf(file, "%d", lo);
	    else
		fprintf(file, "%d:%d", lo, up);
	else
	    fprintf(file, "%d:%d:%d", lo, up, in);
    }
    else
	fprintf(file, "X");
}

void fprint_lrange(file, l)
FILE* file;
list l;
{
    bool firstrange = TRUE;

    MAP(RANGE, r,
     {
	 if (!firstrange)
	     (void) fprintf(file, ", ");

	 firstrange = FALSE;
	 fprint_range(file, r);
     },
	l);
}

void fprint_message(file, m)
FILE* file;
message m;
{
    (void) fprintf(file, "message is array %s(", 
		   entity_local_name(message_array(m)));
    fprint_lrange(file, message_content(m));
    (void) fprintf(file, ")\nto\n");
    vect_fprint(file, (Pvecteur) message_neighbour(m), variable_dump_name);
    (void) fprintf(file, "domain is ");
    fprint_lrange(file, message_dom(m));
    (void) fprintf(file, "\n");
}

void fprint_lmessage(file, l)
FILE* file;
list l;
{
    if (ENDP(l))
	fprintf(file, "message list is empty\n");
    else
	MAP(MESSAGE, m, fprint_message(file, m), l);
}

/*  that is all
 */

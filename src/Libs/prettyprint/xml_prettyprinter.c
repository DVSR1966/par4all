#define DEBUG_XML 1

#include <stdio.h>
#include <ctype.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"

#include "c_syntax.h"
#include "c_parser_private.h"

#include "resources.h"

#include "misc.h"
#include "ri-util.h"
#include "pipsdbm.h"
#include "text-util.h"

#include "effects-convex.h"
#include "effects-generic.h"
#include "complexity_ri.h"
#include "complexity.h"
#include "transformations.h"
#include "callgraph.h"
#include "semantics.h"
 
#define COMMA         ","
#define EMPTY         ""
#define NL            "\n"
#define TAB           "    "
#define SEMICOLON     ";" NL
#define SPACE         " "

#define OPENBRACKET   "["
#define CLOSEBRACKET  "]"

#define OPENPAREN     "("
#define CLOSEPAREN    ")"

#define OPENBRACE     "{"
#define CLOSEBRACE    "}"

#define OPENANGLE     "<"
#define CLOSEANGLE    ">"
#define SLASH 	      "/"

#define SHARPDEF      "#define"
#define COMMENT	      "//" SPACE
#define QUOTE         "\""


#define XML_TASK_PREFIX "T_"
#define XML_MOTIF_PREFIX "M_"
#define XML_ARRAY_PREFIX "A_"

#define XML_RL      NL,TAB,TAB

#define code_is_a_box 0
#define code_is_a_te 1
#define code_is_a_main 2

/* array containing extern loop indices names */
static gen_array_t extern_indices_array;
/* array containing intern loop indices (name : "M_") */
static gen_array_t intern_indices_array;
/* array containing extern upperbounds */
static gen_array_t extern_upperbounds_array;
/* array containing intern upperbounds */
static gen_array_t intern_upperbounds_array; 
/* array containing the tasks names*/
static gen_array_t tasks_names;

static string global_module_name;
#define BL             " "
#define TB1            "\t"

static hash_table hash_entity_def_to_task = hash_table_undefined;
static string global_module_name;
static int global_margin =0;

extern boolean prettyprint_is_fortran;
static boolean box_in_statement_p=FALSE;
static boolean motif_in_statement_p=FALSE;

extern dimension find_ith_dimension();
/**************************************************************** MISC UTILS */

#define current_module_is_a_function() \
  (entity_function_p(get_current_module_entity()))

static bool variable_p(entity e)
{
  storage s = entity_storage(e);
  return type_variable_p(entity_type(e)) &&
    (storage_ram_p(s) || storage_return_p(s));
}


#define RESULT_NAME	"result"
static string xml_entity_local_name(entity var)
{
  string name;
  char * car;

  if (current_module_is_a_function() &&
      var != get_current_module_entity() &&
      same_string_p(entity_local_name(var), 
		    entity_local_name(get_current_module_entity())))
    name = RESULT_NAME;
  else
    {
      name = entity_local_name(var);
  
      /* Delete all the prefixes */

      if (strstr(name,STRUCT_PREFIX) != NULL)
	name = strstr(name,STRUCT_PREFIX) + 1;
      if (strstr(name,UNION_PREFIX) != NULL)
	name = strstr(name,UNION_PREFIX) + 1;
      if (strstr(name,ENUM_PREFIX) != NULL)
	name = strstr(name,ENUM_PREFIX) + 1;      
      if (strstr(name,TYPEDEF_PREFIX) != NULL)
	name = strstr(name,TYPEDEF_PREFIX) + 1;
      if (strstr(name,MEMBER_SEP_STRING) != NULL)
	name = strstr(name,MEMBER_SEP_STRING) + 1;
    }

  /* switch to upper cases... */
  for (car = name; *car; car++)
    *car = (char) toupper(*car);
  
  return name;
}


/************************************************************** DECLARATIONS */

/* 
   integer a(n,m) -> int a[m][n];
   parameter (n=4) -> #define n 4
 */

static string 
int_to_string(int i)
{
  char buffer[50];
  sprintf(buffer, "%d", i);
  return strdup(buffer);
}


/* forward declaration */
static string xml_expression(expression);

/* Attention with Fortran: the indices are reversed. */
static string xml_reference_with_explicit_motif(reference r)
{
  string result = strdup(EMPTY), old, svar;
  MAP(EXPRESSION, e, 
  {
    string s = strdup(xml_expression(e));
    
    old = result;
    result = strdup(concatenate(old, OPENBRACKET, s, CLOSEBRACKET, NULL));
    free(old);
    free(s);
  }, reference_indices(r));

  old = result;
  svar = xml_entity_local_name(reference_variable(r));
  result = strdup(concatenate(svar, old, NULL));
  free(old);
  free(svar);
  return result;
}

static string xml_expression(expression e)
{
  string result = string_undefined;
  syntax s = expression_syntax(e);

  switch (syntax_tag(s))
    {
    case is_syntax_reference:
      result = xml_reference_with_explicit_motif(syntax_reference(s));
      break;
    case is_syntax_call: {
      value ev = EvalExpression(e);
      constant ec = value_constant(ev);
      int eiv = 0;

      if(!value_constant_p(ev)) {
	pips_user_error("Constant expected for XML loop bounds.\n");
      }
      if(!constant_int_p(ec)) {
	pips_user_error("Integer constant expected for XML loop bounds.\n");
      }
      eiv = constant_int(ec);
      result = strdup(itoa(eiv));

     
      break;
    }
    default:
      pips_internal_error("unexpected syntax tag");
    }
  return result;
}

gen_array_t array_names;
gen_array_t array_dims;

#define ITEM_NOT_IN_ARRAY -1

static int gen_array_index(gen_array_t ar, string item){
  int i;

  for(i = 0; i<(int) gen_array_nitems(ar); i++){
    if(gen_array_item(ar, i) != NULL){
      if(same_string_p(item, *((string *)(gen_array_item(ar, i))))){
	return i;
      }
    }
  }
  return ITEM_NOT_IN_ARRAY;
}

static string xml_dim_string(list ldim, string name)
{
  string result = "";
  int nbdim = 0;
  string origins = "origins = list<integer>(";
  string dimensions = "dimSizes = list<integer>(";
  string deuxpoints = " :: ";
  string data_array = "DATA_ARRAY(";
  string data_decl = "name = symbol!(";
  string dimstring = "dim = ";
  string datatype = "dataType = INTEGER)";
  string name4p = name;
  string * namep = malloc(sizeof(string));
  int * nbdimptr = malloc(sizeof(int));
  *namep = name4p;
  if (ldim)
    {
      
      result = strdup(concatenate(name, deuxpoints, data_array, data_decl, QUOTE, name, QUOTE, CLOSEPAREN, COMMA, NL, NULL));
      result = strdup(concatenate(result, TAB, dimstring, NULL));
      MAP(DIMENSION, dim, {
	expression elow = dimension_lower(dim);
	expression eup = dimension_upper(dim);
	
	int low;
	int up;
	nbdim++;
	if (expression_integer_value(elow, &low)){
	  if(nbdim != 1)
	    origins = strdup(concatenate(origins, COMMA ,int_to_string(low), NULL));
	  else
	    origins = strdup(concatenate(origins, int_to_string(low), NULL));
	}
	else pips_user_error("Array origins must be integer\n");

	if (expression_integer_value(eup, &up)){
	  if(nbdim != 1)
	    dimensions = strdup(concatenate(dimensions, COMMA ,int_to_string(up-low+1), NULL));
	  else
	    dimensions = strdup(concatenate(dimensions, int_to_string(up-low+1), NULL));
	}
	else pips_user_error("Array dimensions must be integer\n");
      }, ldim);
      *nbdimptr = nbdim;
      gen_array_append(array_dims, nbdimptr);
      gen_array_append(array_names, namep);
      result = strdup(concatenate(result, int_to_string(nbdim), COMMA, NL, NULL));
      result = strdup(concatenate(result, TAB, origins, CLOSEPAREN, COMMA, NL, NULL));
      result = strdup(concatenate(result, TAB, dimensions, CLOSEPAREN, COMMA, NL, NULL));
      result = strdup(concatenate(result, TAB, datatype, NL, NL, NULL));
    }
  return result;
}

static string this_entity_xmldeclaration(entity var)
{
  string result = strdup("");
  string name = strdup(concatenate("A_", entity_local_name(var), NULL));
  type t = entity_type(var);
  pips_debug(2,"Entity name : %s\n",entity_name(var));
  /*  Many possible combinations */

  if (strstr(name,TYPEDEF_PREFIX) != NULL)
    pips_user_error("Structs not supported\n");

  switch (type_tag(t)) {
  case is_type_variable:
    {
      variable v = type_variable(t);  
      string sd;
      sd = strdup(xml_dim_string(variable_dimensions(v), name));
    
      result = strdup(concatenate(result, sd, NULL));
      break;
    }
  case is_type_struct:
    {
      pips_user_error("Struct not allowed\n");
      break;
    }
  case is_type_union:
    {
      pips_user_error("Union not allowed\n");
      break;
    }
  case is_type_enum:
    {
      pips_user_error("Enum not allowed\n");
      break;
    }
  default:
    pips_user_error("Something not allowed here\n");
  }
 
  return result;
}

static string 
xml_declarations_with_explicit_motif(entity module,
	       bool (*consider_this_entity)(entity),
	       string separator,
	       bool lastsep)
{
  string result = strdup("");
  code c;
  bool first = TRUE;

  pips_assert("it is a code", value_code_p(entity_initial(module)));

  c = value_code(entity_initial(module));
  MAP(ENTITY, var,
  {
    debug(2, "\n Prettyprinter declaration for variable :",xml_entity_local_name(var));   
    if (consider_this_entity(var))
      {
	string old = strdup(result);
	string svar = strdup(this_entity_xmldeclaration(var));
	result = strdup(concatenate(old, !first && !lastsep? separator: "",
				    svar, lastsep? separator: "", NULL));
	free(old);
	free(svar);
	first = FALSE;
      }
  },code_declarations(c));
  return result;
}

static string xml_array_in_task(reference r, bool first, int task_number);

static string xml_call_from_assignation(call c, int task_number, bool * input_provided){
  /* All arguments of this call are in Rmode (inputs of the task) */
  /* This function is called recursively */
  list arguments = call_arguments(c);
  syntax syn;
  string result = "";
  
  MAP(EXPRESSION, expr, {
    syn = expression_syntax(expr);
    switch(syntax_tag(syn)){
    case is_syntax_call:{
      result = strdup(concatenate(result, xml_call_from_assignation(syntax_call(syn), task_number, input_provided), NULL));
      break;
    }
    case is_syntax_reference:{
      reference ref = syntax_reference(syn);
      string varname = strdup(concatenate("A_", xml_entity_local_name(reference_variable(ref)), NULL));
      if(gen_array_index(array_names, varname) != ITEM_NOT_IN_ARRAY){
	result = strdup(concatenate(result, xml_array_in_task(ref, FALSE, task_number), NULL));
	*input_provided = TRUE;
      }

     
      break;
    }
    default:{
      pips_user_error("only call and references allowed here\n");
    }
    }
  }, arguments);
  return result;
}

static void xml_call_from_indice(call c, string * offset_array, string paving_array[], string fitting_array[]){
  entity called = call_function(c);
  string funname = xml_entity_local_name(called);
  list arguments = call_arguments(c);
  syntax args[2];
  int i = 0;
  int iterator_nr;
  if(gen_length(arguments)==2){
    if(same_string_p(funname, "+") || same_string_p(funname, "-") || same_string_p(funname, "*")){
      MAP(EXPRESSION, arg, {
	args[i] = expression_syntax(arg);
	i++;
      }, arguments);
      
      
      if(same_string_p(funname, "+")){
	if(syntax_tag(args[0]) == is_syntax_call){
	  xml_call_from_indice(syntax_call(args[0]), offset_array, paving_array, fitting_array);
	}
	if(syntax_tag(args[1]) == is_syntax_call){
	  xml_call_from_indice(syntax_call(args[1]), offset_array, paving_array, fitting_array);
	}
	if(syntax_tag(args[0]) == is_syntax_reference){
	  reference ref = syntax_reference(args[0]);
	  if((iterator_nr = gen_array_index(extern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	    paving_array[iterator_nr] = strdup("1");
	  }
	  else if((iterator_nr = gen_array_index(intern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	    fitting_array[iterator_nr] = strdup("1");
	  }
	}
	if(syntax_tag(args[1]) == is_syntax_reference){
	  reference ref = syntax_reference(args[1]);
	  if((iterator_nr = gen_array_index(extern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	   paving_array[iterator_nr] = strdup("1");
	  }
	  else if((iterator_nr = gen_array_index(intern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	   fitting_array[iterator_nr] = strdup("1");
	  }
	}
      }
      else if(same_string_p(funname, "-")){
	if(syntax_tag(args[1]) == is_syntax_call && gen_length(call_arguments(syntax_call(args[1])))==0){
	  if(syntax_tag(args[0]) == is_syntax_reference){
	    reference ref = syntax_reference(args[0]);
	    if((iterator_nr = gen_array_index(extern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	      paving_array[iterator_nr] = strdup("1");
	    }
	    else if((iterator_nr = gen_array_index(intern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	      fitting_array[iterator_nr] = strdup("1");
	    }
	  }
	  if(syntax_tag(args[0]) == is_syntax_call){
	    xml_call_from_indice(syntax_call(args[0]), offset_array, paving_array, fitting_array);
	  }
	  xml_call_from_indice(syntax_call(args[1]), offset_array, paving_array, fitting_array);
	}
	else {
	  pips_user_error("APOTRES doesn't allow negative coefficients in paving and fitting matrices\n");
	}
      }
      else if(same_string_p(funname, "*")){
	if(syntax_tag(args[0]) != is_syntax_call || syntax_tag(args[1]) != is_syntax_reference || gen_length(call_arguments(syntax_call(args[0])))!=0 ){
	  pips_user_error("Only scalar * reference are allowed here. Please develop expressions.\n");
	}
	else {
	  int intern_nr = gen_array_index(intern_indices_array, xml_entity_local_name(reference_variable(syntax_reference(args[1]))));
	  int extern_nr = gen_array_index(extern_indices_array, xml_entity_local_name(reference_variable(syntax_reference(args[1]))));
	  string mult =  strdup(xml_entity_local_name(call_function(syntax_call(args[0])))); 
	  if(extern_nr != ITEM_NOT_IN_ARRAY){
	    paving_array[extern_nr] = mult;
	  }
	  else if(intern_nr != ITEM_NOT_IN_ARRAY){
	    fitting_array[intern_nr] = strdup(mult);
	  }
	}
      }
    }
    else{
      pips_user_error("only linear expression of indices allowed\n");
    }
  }
  else if(gen_length(arguments) == 0){
    *offset_array = funname;
  }
  else{
    pips_user_error("only +, -, * and constants allowed\n");
  }
}

#define XML_ARRAY_PREFIX "A_"

static string xml_array_in_task(reference r, bool first, int task_number){
  /* XML name of the referenced array */
  string varname = strdup(concatenate(XML_ARRAY_PREFIX, 
				      xml_entity_local_name(reference_variable(r)), 
				      NULL));
  /* iterator for dimensions of array */
  int indice_nr = 0;
  list indices = reference_indices(r);
  string result = "";
  /* number of external loops*/
  int extern_nb = gen_array_nitems(extern_indices_array);
  
  /* number of dimensions of referenced array */
  int index_of_array = gen_length(indices); /*((int *) (gen_array_item(array_dims, gen_array_index(array_names, varname))));*/

   /* number of internal loops*/ 
  int intern_nb = gen_array_nitems(intern_indices_array);

  /* list of offsets for XML code */
  string offset_array[index_of_array];
  /* paving matrix for XML code
   1st coeff: array dimension (row index)
   2nd coeff: iteration dimension (column index) */
  string paving_array[index_of_array][extern_nb];
  
  /* fitting matrix for XML code 
   1st coeff: array dimension
   2nd coeff: iteration dimension*/
  string fitting_array[index_of_array][intern_nb];
  int i;
  int j;
  int depth = 0;

  bool null_fitting_p = TRUE;
  string internal_index_declarations = strdup("");
  string fitting_declaration = strdup("");
  string fitting_declaration2 = strdup("");

  /* initialization of the arrays */
  for (i=0; i<index_of_array; i++)
    offset_array[i] = "0";
  
  for (i=0; i<index_of_array ; i++)
    for (j=0; j<extern_nb; j++)
      paving_array[i][j] = "0";

  for (i=0; i<index_of_array ; i++)
    for (j=0; j<intern_nb; j++)
      fitting_array[i][j] = "0";

  /* XML reference header */
  result = strdup(concatenate(result, "DATA(name = symbol!(\"", "T_", int_to_string(task_number),
			      "\" /+ \"", varname, "\"),", NL, TAB, TAB, NULL));

  result = strdup(concatenate(result, "darray = ", varname, "," NL, TAB, TAB, "accessMode = ", (first?"Wmode,":"Rmode,"),
			      NL, TAB, TAB, "offset = list<VARTYPE>(", NULL));
  
  /* Fill in paving, fitting and offset matrices from index expressions. */
  MAP(EXPRESSION, ind, {
    syntax sind = expression_syntax(ind);
    int iterator_nr;
    switch(syntax_tag(sind)){
    case is_syntax_reference:{
      reference ref = syntax_reference(sind);
      if((iterator_nr = gen_array_index(extern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	paving_array[indice_nr][iterator_nr] = strdup("1");
      }
      else if((iterator_nr = gen_array_index(intern_indices_array, xml_entity_local_name(reference_variable(ref)))) != ITEM_NOT_IN_ARRAY){
	fitting_array[indice_nr][iterator_nr] = strdup("1");
      }

      break;
    }
    case is_syntax_call:{
      call c = syntax_call(sind);
      xml_call_from_indice(c, &(offset_array[indice_nr]), paving_array[indice_nr], fitting_array[indice_nr]);
      break;
    }
    default:{
      pips_user_error("Only call and reference allowed in indices.\n");
      break;
    }
    }
    indice_nr++;
  }, indices);


  /* generate offset list in XML code */  
  for(i=0; i<index_of_array - 1; i++){
    result=strdup(concatenate(result, "vartype!(", offset_array[i],"), ", NULL));
  }
  result = strdup(concatenate(result, "vartype!(", offset_array[i], "))," NL, NULL));

  /* fitting header */
  result = strdup(concatenate(result, TAB, TAB, "fitting = list<list[VARTYPE]>(", NULL));

  /* XML column-major storage of fitting matrix */
  for(i=0;i<intern_nb; i++){
    bool is_null_p = TRUE;
    for(j = 0; j<index_of_array; j++){
      is_null_p = is_null_p && (same_string_p(fitting_array[j][i], "0"));
    }
    if(!is_null_p){
      null_fitting_p = FALSE;
      fitting_declaration = strdup(concatenate(fitting_declaration, "list(", NULL));
      for(j = 0; j<index_of_array-1; j++){
	fitting_declaration = strdup(concatenate(fitting_declaration, "vartype!(", fitting_array[j][i], "), ", NULL));
      }
      fitting_declaration = strdup(concatenate(fitting_declaration, 
					       "vartype!(", 
					       fitting_array[j][i], 
					       ")), ",
					       NULL));
    }
  }
  
  if(!null_fitting_p){
    fitting_declaration2 = 
      strdup(concatenate(gen_strndup0(fitting_declaration, 
				      strlen(fitting_declaration) - 2), 
			 "),", NL, TAB, TAB, TAB, NULL));
    result = strdup(concatenate(result, fitting_declaration2, NULL));
  }

  if(null_fitting_p){
    result = strdup(concatenate(result, "list()),", NL, TAB, TAB, NULL));
    }

  null_fitting_p = TRUE;
  /* Generation of paving XML code*/
  result = strdup(concatenate(result, TAB, TAB, "paving = list<list[VARTYPE]>(", NULL));

  for(i=0;i<extern_nb-1; i++){
    result = strdup(concatenate(result, "list(", NULL));
    for(j = 0; j<index_of_array-1; j++){
      result = strdup(concatenate(result, "vartype!(", paving_array[j][i], "), ", NULL));
    }
    result = strdup(concatenate(result, "vartype!(", paving_array[j][i], ")),", NL, TAB, TAB, TAB, NULL));
  }
  result = strdup(concatenate(result, "list(", NULL));
  for(j = 0; j<index_of_array-1; j++){
    result = strdup(concatenate(result, "vartype!(", paving_array[j][i], "), ", NULL));
  }
  result = strdup(concatenate(result, "vartype!(", paving_array[j][i], "))),", NL, TAB, TAB, NULL));
  
#define MONMAX(a, b) ((a<b)?b:a)
  
  /* Definition of the inner loop nest */
  /* FI->IH: if some columns are removed, the effective depth is unkown and must be computed here */
  /* result = strdup(concatenate(result, "inLoopNest = LOOPNEST(deep = ", int_to_string(MONMAX(gen_array_nitems(intern_indices_array), 1)), ",", NL, TAB, TAB, TAB, NULL)); */

  for (j = 0; j<intern_nb; j++){
    bool is_null_p = TRUE;
    for(i = 0; i < index_of_array; i++){
      is_null_p = is_null_p && (same_string_p(fitting_array[i][j], "0"));
    }
    if(!is_null_p){
      depth++;
    }
  }
  if(depth==0) depth = 1; /* see comment just below about null fitting matrices. */
  result = strdup(concatenate(result, "inLoopNest = LOOPNEST(deep = ", itoa(depth), ",", NL, TAB, TAB, TAB, NULL));
  result = strdup(concatenate(result, "upperBound = list<VARTYPE>(", NULL));

  /* 3 cases :
     - the fitting matrix is null : must generate a (0,0) loop with dummy index
     - some fitting matrix column is null : do not generate anything
     - some fitting matrix column is not null : generate the corresponding loop bound and index name
  */

  for (j = 0; j<intern_nb; j++){
    bool is_null_p = TRUE;
    for(i = 0; i < index_of_array; i++){
      is_null_p = is_null_p && (same_string_p(fitting_array[i][j], "0"));
    }
    if(!is_null_p){
      null_fitting_p = FALSE;
      result = strdup(concatenate(result, 
				  "vartype!(", 
				  *((string *)(gen_array_item(intern_upperbounds_array, j))), 
				  "), ",
				  NULL));
      internal_index_declarations = 
	strdup(concatenate(internal_index_declarations, 
			   QUOTE, 
			   *((string *)(gen_array_item(intern_indices_array, j))), 
			   QUOTE, 
			   ", ",
			   NULL));
    }
  }
  if(!null_fitting_p)
    {
      result = strdup(concatenate(gen_strndup0(result, strlen(result) - 2), 
				  "),", NULL));
      internal_index_declarations = 
	strdup(concatenate(gen_strndup0(internal_index_declarations,
				      strlen(internal_index_declarations) -2),
			   ")", NULL));
    }



  if(null_fitting_p){
 result = strdup(concatenate(result, "vartype!(1)),", NL, TAB, TAB, TAB, "names = list<string>(\"M_I\")", NULL));
  }
  else{
    result = strdup(concatenate(result, NL, TAB, "names = list<string>(", internal_index_declarations, NULL));
  }

  /* Complete XML reference */
  result = strdup(concatenate(result, "))", (first?")":","), NL, NULL)); 
  return result;
  
}

static string xml_call_from_loopnest(call c, int task_number){
  entity called = call_function(c);
  list arguments = call_arguments(c);
  
  syntax s;
  string result = "";
  string first_result = "";
  bool first = TRUE;
  bool input_provided = FALSE, output_provided = FALSE;
  string name = strdup(xml_entity_local_name(called));

  if(!same_string_p(name, "="))
    pips_user_error("Only assignation allowed here.\n");
  
  MAP(EXPRESSION, e, {
    s = expression_syntax(e);
    switch(syntax_tag(s)){
    case is_syntax_call:{
      if(first)
	pips_user_error("Call not allowed in left-hand side argument of assignation.\n");
      else
	result = strdup(concatenate(result, xml_call_from_assignation(syntax_call(s), task_number, &input_provided), NULL));
      break;
    }
    case is_syntax_reference:{
      
      reference r = syntax_reference(s);
      string varname = strdup(concatenate("A_", xml_entity_local_name(reference_variable(r)), NULL));
      if(gen_array_index(array_names, varname) != ITEM_NOT_IN_ARRAY){
	if(first){
	  first_result = xml_array_in_task(r, first, task_number);
	  output_provided = TRUE;
	}
	else{
	  result = strdup(concatenate(result, xml_array_in_task(r, first, task_number), NULL));
	  input_provided = TRUE;
	}
      }
    }
    }
    first = FALSE;
  }, arguments);

  if(!input_provided){
    result = strdup(concatenate("data = list<DATA>(dummyDATA, ", result, first_result, NULL));
  }
  else{
    result = strdup(concatenate("data = list<DATA>(", result, first_result, NULL));
  }
  if(!output_provided){
    result = strdup(concatenate(result, " dummyDATA)", NULL));
  }
  result = strdup(concatenate(result, TAB, ")", NL, NULL));
  return result;
}


static call sequence_call(sequence seq)
{
  call mc = call_undefined; /* meaningful call */
  int nc = 0; /* number of calls */

  MAP(STATEMENT, s, {
    if(continue_statement_p(s))
      ;
    else if(statement_call_p(s)) {
      mc = instruction_call(statement_instruction(s));
      nc++;
    }
    else {
      nc = 0;
      break;
    }
  }, sequence_statements(seq));

  if(nc!=1)
    mc = call_undefined;

  return mc;
}

static loop sequence_loop(sequence seq)
{
  loop ml = loop_undefined; /* meaningful loop */
  int nl = 0; /* number of loops */

  MAP(STATEMENT, s, {
    if(continue_statement_p(s))
      ;
    else if(statement_loop_p(s)) {
      ml = instruction_loop(statement_instruction(s));
      nl++;
    }
    else {
      nl = 0;
      break;
    }
  }, sequence_statements(seq));

  if(nl!=1)
    ml = loop_undefined;

  return ml;
}

static call xml_loop_from_loop(loop l, string * result, int task_number){
  
  string * up = malloc(sizeof(string));
  string * xml_name = malloc(sizeof(string));
  statement s = loop_body(l);
  instruction i = statement_instruction(s);
  int u, low;
  expression incr_e = range_increment(loop_range(l));
  syntax incr_s = expression_syntax(incr_e);

  if(!syntax_call_p(incr_s) || 
     strcmp( entity_local_name(call_function(syntax_call(incr_s))), "1") != 0 ) {
    pips_user_error("Loop increments must be constant \"1\".\n");
  }

  u = atoi(xml_expression(range_upper(loop_range(l))));
  low = atoi(xml_expression(range_lower(loop_range(l))));
  /*  printf("%i %i\n", u, low); */
  *up = strdup(int_to_string(u - low+1));
	       //*up = xml_expression(range_upper(loop_range(l)) - range_lower(loop_range(l)) + 1);
  *xml_name = xml_entity_local_name(loop_index(l));
  if( (*xml_name)[0] == 'M'){
    gen_array_append(intern_indices_array, xml_name);
    gen_array_append(intern_upperbounds_array, up);
  }
  else{
    gen_array_append(extern_indices_array, xml_name);
    gen_array_append(extern_upperbounds_array, up);
  }

  switch(instruction_tag(i)){
  case is_instruction_loop:{
    loop l = instruction_loop(i);
    return xml_loop_from_loop(l, result, task_number);
    break;
  }
  case is_instruction_call: {
    call c = instruction_call(i);
    return c;
  }
  case is_instruction_sequence: {
    /* The sequence should contain only one meaningful call or one meaningful loop. */
    call c = sequence_call(instruction_sequence(i));
    loop l = sequence_loop(instruction_sequence(i));
    if(!call_undefined_p(c))
      return c;
    if(!loop_undefined_p(l))
      return xml_loop_from_loop(l, result, task_number);
    }
    default:
    pips_user_error("Only loops and calls allowed in a loop.\n");
  }
return call_undefined;
}


/* We enter a loop nest. The first loop must be an extern loop. */
static string xml_loop_from_sequence(loop l, int task_number){
  statement s = loop_body(l);
  call c= call_undefined;
  int i;
  string * taskname = (string *)(malloc(sizeof(string)));
  expression incr_e = range_increment(loop_range(l));
  syntax incr_s = expression_syntax(incr_e);


  /* Initialize result string with the declaration of the task */
  string result;

  instruction ins = statement_instruction(s);
  string * name = malloc(sizeof(string));
  string * up = malloc(sizeof(string));
  int u, low;
  if(!syntax_call_p(incr_s) || 
     strcmp( entity_local_name(call_function(syntax_call(incr_s))), "1") != 0 ) {
    pips_user_error("Loop increments must be constant \"1\".\n");
  }


  *taskname = strdup(concatenate("T_", int_to_string(task_number), NULL));
  result = strdup(concatenate(*taskname, 
			      " :: TASK(unitSpentTime = vartype!(1),"
			      NL, TAB, "exLoopNest = LOOPNEST(deep = ", NULL));
  gen_array_append(tasks_names, taskname);
  /* (re-)initialize task-scoped arrays*/
  extern_indices_array = gen_array_make(0);
  intern_indices_array = gen_array_make(0);
  extern_upperbounds_array = gen_array_make(0);
  intern_upperbounds_array = gen_array_make(0);
  

  *name = xml_entity_local_name(loop_index(l));
  u = atoi(xml_expression(range_upper(loop_range(l))));
  low = atoi(xml_expression(range_lower(loop_range(l))));
  *up = strdup(int_to_string(u - low+1));
  //*up = xml_expression(range_upper(loop_range(l)) - range_lower(loop_range(l)) + 1);

  if((*name)[0] == 'M'){
    pips_user_error("At least one extern loop is needed.\n");
  }
  else{
    gen_array_append(extern_indices_array, name);
    gen_array_append(extern_upperbounds_array, up);
  }


  switch(instruction_tag(ins)){
  case is_instruction_loop:{
    loop l = instruction_loop(ins);
    c = xml_loop_from_loop(l, &result, task_number);
    break;
  }
  case is_instruction_call:
    {
      c = instruction_call(ins);
    }
    break;
  case is_instruction_sequence:
    /* The sequence should contain only one meaningful call */
    if(!call_undefined_p(c=sequence_call(instruction_sequence(ins))))
      break;
    if(!loop_undefined_p(l=sequence_loop(instruction_sequence(ins)))) {
      c = xml_loop_from_loop(l, &result, task_number);
      break;
    }
    ;
  default:
    pips_user_error("Only loops and one significant call allowed in a loop.\n");
  }

  /* External loop nest depth */
  result = strdup(concatenate(result, int_to_string(gen_array_nitems(extern_upperbounds_array)), ",", NL, TAB, TAB, NULL));

  /* add external upperbounds */
  result = strdup(concatenate(result, "upperBound = list<VARTYPE>(", NULL));

  for(i=0; i< ((int) gen_array_nitems(extern_upperbounds_array)) - 1; i++){
    result = strdup(concatenate(result, "vartype!(", *((string *)(gen_array_item(extern_upperbounds_array, i))), "), ", NULL));
  }
  result = strdup(concatenate(result, "vartype!(",*((string *)(gen_array_item(extern_upperbounds_array, i))), ")),",NL, TAB, TAB, NULL));

  /* add external indices names*/
  result = strdup(concatenate(result, "names = list<string>(", NULL));
  for(i=0; i<((int) gen_array_nitems(extern_indices_array)) - 1; i++){
    result = strdup(concatenate(result, QUOTE, *((string *)(gen_array_item(extern_indices_array, i))), QUOTE ", ", NULL));
  }
  result = strdup(concatenate(result, QUOTE, *((string *)(gen_array_item(extern_indices_array, i))), QUOTE, ")),", NL, TAB, NULL));
  
  result = strdup(concatenate(result, xml_call_from_loopnest(c, task_number), NULL));

  gen_array_free(extern_indices_array);
  gen_array_free(intern_indices_array);
  gen_array_free(extern_upperbounds_array);
  gen_array_free(intern_upperbounds_array);
     
  result = strdup(concatenate(result, NL, NULL));
  return result;
}

/* We are here at the highest level of statements. The statements are either
   loopnests or a RETURN instruction. Any other possibility pips_user_errors
   the prettyprinter.*/
static string xml_statement_from_sequence(statement s, int task_number){
  string result = "";
  instruction i = statement_instruction(s);

  switch(instruction_tag(i)){
  case is_instruction_loop:{
    loop l = instruction_loop(i);
    result = xml_loop_from_sequence(l, task_number);
    break;
  }
  case is_instruction_call:{
    /* RETURN should only be allowed as the last statement in the sequence */
    if(!return_statement_p(s) && !continue_statement_p(s))
      pips_user_error("Only RETURN and CONTINUE allowed here.\n");
    break;
  }
  default:{
    pips_user_error("Only loops and calls allowed here.\n");
  }
  }

  return result;
}

/* Concatentates each task to the final result.
   The validity of the task is not checked in this function but
   it is into xml_statementement_from_sequence and subsequent
   functions.*/
static string xml_sequence_from_task(sequence seq){
  string result = "";
  int task_number = 0;
  MAP(STATEMENT, s,
  {
    string oldresult = strdup(result);
    string current = strdup(xml_statement_from_sequence(s, task_number));

    if(strlen(current)==0) {
      free(current);
      result = oldresult;
    }
    else {
      result = strdup(concatenate(oldresult, current, NULL));
      free(current);
      free(oldresult);
      task_number++;
    }
  }, sequence_statements(seq));
  return result;
}

/* Manages tasks. The code is very defensive and hangs if sth not
   predicted happens. Here basically we begin the code in itself
   and thus $stat is obligatory a sequence. */
static string xml_tasks_with_motif(statement stat){
  int j;
  instruction i;
  string result = "tasks\n";
    if(statement_undefined_p(stat))
    {
      pips_internal_error("statement error");
    }
    i = statement_instruction(stat);
  tasks_names = gen_array_make(0);
  switch(instruction_tag(i)){
  case is_instruction_sequence:{
    sequence seq = instruction_sequence(i);
    result = xml_sequence_from_task(seq);
    break;
  }
  default:{
    pips_user_error("Only a sequence can be here");
  }
  }
  result = strdup(concatenate(result, NL, NL, "PRES:APPLICATION := APPLICATION(name = symbol!(", QUOTE, global_module_name, QUOTE, "), ", NL, TAB,NULL));
  result = strdup(concatenate(result, "tasks = list<TASK>(", NULL));
  for(j = 0; j<(int) gen_array_nitems(tasks_names) - 1; j++){
    result = strdup(concatenate(result, *((string *)(gen_array_item(tasks_names, j))), ", ", NULL));
  }
  result = strdup(concatenate(result, *((string *)(gen_array_item(tasks_names, j))), "))", NULL));

  return result;
}


/* Creates string for xml pretty printer.
   This string divides in declarations (array decl.) and 
   tasks which are loopnest with an instruction at the core.
*/
static string xml_code_string(entity module, statement stat)
{
  string decls="", tasks="", result="";

  ifdebug(2)
    {
      printf("Module statement: \n");
      print_statement(stat);
      printf("and declarations: \n");
      print_entities(statement_declarations(stat));
    }

  decls       = xml_declarations_with_explicit_motif(module, variable_p, "", TRUE);
  tasks       = xml_tasks_with_motif(stat);
  
  result = strdup(concatenate(decls, NL, tasks, NL, NULL));
  ifdebug(2)
    {
      printf("%s", result);
    }
  return result;
}


/******************************************************** PIPSMAKE INTERFACE */

#define XMLPRETTY    ".xml"

/* Initiates xml pretty print modules
 */
bool print_xml_code_with_explicit_motif(string module_name)
{
  FILE * out;
  string ppt, xml, dir, filename;
  entity module;
  statement stat;
  array_names = gen_array_make(0);
  array_dims = gen_array_make(0);
  xml = db_build_file_resource_name(DBR_XML_PRINTED_FILE, module_name, XMLPRETTY);

  global_module_name = module_name;
  module = module_name_to_entity(module_name);
  dir = db_get_current_workspace_directory();
  filename = strdup(concatenate(dir, "/", xml, NULL));
  stat = (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE);

  if(statement_undefined_p(stat))
    {
      pips_internal_error("No statement for module %s\n", module_name);
    }
  set_current_module_entity(module);
  set_current_module_statement(stat);

  debug_on("XMLPRETTYPRINTER_DEBUG_LEVEL");
  pips_debug(1, "Begin Claire prettyprinter for %s\n", entity_name(module));
  ppt = xml_code_string(module, stat);
  pips_debug(1, "end\n");
  debug_off();  

  /* save to file */
  out = safe_fopen(filename, "w");
  fprintf(out, "%s", ppt);
  safe_fclose(out, filename);

  free(ppt);
  free(dir);
  free(filename);

  DB_PUT_FILE_RESOURCE(DBR_XML_PRINTED_FILE, module_name, xml);

  reset_current_module_statement();
  reset_current_module_entity();

  return TRUE;
}


/* ======================================================= */ 



typedef struct 
{
  stack  loops_for_call;
  stack loop_indices;
  stack current_stat;
  gen_array_t nested_loops;
  gen_array_t nested_loop_indices;
  gen_array_t nested_call;
} 
nest_context_t, 
  * nest_context_p;


static void
xml_declarations(entity module, string_buffer result)
{ 
  list dim;
  int nb_dim =0;
  list ldecl = code_declarations(value_code(entity_initial(module)));
  list ld;

  entity var = entity_undefined;
    
for(ld = ldecl; !ENDP(ld); ld = CDR(ld)){
        
	printf("inside for");
        var = ENTITY(CAR(ld));
	printf("Entity Name in the List : %s  ",entity_name(var));
    
	if (variable_p(var) && ( variable_entity_dimension(var) > 0))
        {
	 nb_dim = variable_entity_dimension(var);
 	 printf("inside p, %d",nb_dim);
	 
	 string_buffer_append(result,
			     strdup(concatenate(TAB, OPENANGLE, "array name =", QUOTE, XML_ARRAY_PREFIX,entity_user_name(var),QUOTE, 
						" dataType = ", QUOTE,
						"INTEGER", QUOTE,CLOSEANGLE
						, XML_RL, NULL)));
	 string_buffer_append(result,
			     strdup(concatenate(OPENANGLE,"dimensions",CLOSEANGLE 
						, XML_RL,NULL)));
	
	 for (dim = variable_dimensions(type_variable(entity_type(var))); !ENDP(dim); dim = CDR(dim)) {

	  int low;
	  int  up;
	  expression elow = dimension_lower(DIMENSION(CAR(dim)));
	  expression eup = dimension_upper(DIMENSION(CAR(dim)));
	  if (expression_integer_value(elow, &low) && expression_integer_value(eup, &up)){
	    string_buffer_append(result,
				 strdup(concatenate( TAB,OPENANGLE, "range min =", QUOTE,  i2a(low), QUOTE, NULL)));
	    string_buffer_append(result,
				 strdup(concatenate(" max =", QUOTE,  i2a(up-low+1),QUOTE, SLASH, CLOSEANGLE, XML_RL, NULL)));
	  }
	  else pips_user_error("Array dimensions must be integer\n");

	}	

	string_buffer_append(result,
			     strdup(concatenate(OPENANGLE, SLASH, "dimensions", CLOSEANGLE,NL ,NULL)));
	
	string_buffer_append(result,
			     strdup(concatenate(TAB, OPENANGLE, SLASH, "array", CLOSEANGLE, NL, NULL)));	

      }
  
    }
}


static void 
push_current_statement(statement s, nest_context_p nest)
{ 
    stack_push(s , nest->current_stat); 
}

static void 
pop_current_statement(statement s __attribute__ ((unused)), 
		      nest_context_p nest)
{ 
  /*   if (debug) print_statement(s);*/
  stack_pop(nest->current_stat); 
}
 
static void 
push_loop(loop l, nest_context_p nest)
{ 
  /* on sauve le statement associe a la boucle courante */ 
  statement sl = (statement) stack_head(nest->current_stat);
   stack_push(sl , nest->loops_for_call); 
   stack_push(loop_index(l) , nest->loop_indices); 
}

static void 
pop_loop(loop l __attribute__ ((unused)), 
	 nest_context_p nest)
{ 
 stack_pop(nest->loops_for_call); 
 stack_pop(nest->loop_indices); 
}
 
static bool call_selection(call c, nest_context_p nest __attribute__ ((unused))) 
{ 

  /* CA il faut implemeter  un choix judicieux ... distribution ou encapsulation*/
  /* pour le moment distribution systematique de tout call */
  /* il faut recuperer les appels de fonction value_code_p(entity_initial(f)*/
  entity f = call_function(c); 
  if  (ENTITY_ASSIGN_P(f) || entity_subroutine_p(f))
    {  
      return TRUE;
    }
  else return FALSE;

  /*  statement s = (statement) stack_head(nest->current_stat);
      return ((!return_statement_p(s) && !continue_statement_p(s)));*/
}

static void store_call_context(call c  __attribute__ ((unused)), 
			       nest_context_p nest)
{
  stack sl = stack_copy(nest->loops_for_call);
  stack si = stack_copy(nest->loop_indices);
  /* on sauve le statement associe au call */ 
  statement statc = (statement) stack_head(nest->current_stat) ;
  gen_array_append(nest->nested_loop_indices,si);
  gen_array_append(nest->nested_loops,sl);
  gen_array_append(nest->nested_call,statc);
}

static bool push_test(test t  __attribute__ ((unused)),  
		      nest_context_p nest  __attribute__ ((unused)))
{
  /* encapsulation de l'ensemble des instructions appartenant au test*/
  /* on ne fait rien pour le moment */
  return FALSE;
}


static void pop_test(test t __attribute__ ((unused)),  
		     nest_context_p nest __attribute__ ((unused)))
{
  /* encapsulation de l'ensemble des instructions appartenant au test*/
 
}


static void
search_nested_loops_and_calls(statement stmp, nest_context_p nest)
{
  gen_context_multi_recurse(stmp,nest, loop_domain,push_loop,pop_loop,
			    statement_domain, push_current_statement,pop_current_statement,
			    test_domain, push_test, pop_test,
			    call_domain,call_selection,store_call_context,
			    NULL);
}

static void __attribute__ ((unused)) print_call_selection(nest_context_p nest)
{
  int j;
  int numberOfTasks=(int) gen_array_nitems(nest->nested_call);
  for (j = 0; j<numberOfTasks; j++)
    {
      //statement s = gen_array_item(nest->nested_call,j);
      //stack st = gen_array_item(nest->nested_loops,j);
      /*   print_statement(s);
	   stack_map( st, print_statement);*/
    }
}


static expression expression_plusplus(expression e)
{
  expression new_e;
   if (expression_constant_p(e)) {
    new_e = int_to_expression(1+ expression_to_int(e));
  }
  else {
    entity add_ent = gen_find_entity("TOP-LEVEL:+");
    new_e =  make_call_expression(add_ent,
            CONS(EXPRESSION, e, CONS(EXPRESSION,  int_to_expression(1), NIL)));
  }
   return new_e;
}

static void xml_loop(stack st, string_buffer result)
{
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, OPENANGLE, "outLoop", CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, OPENANGLE, "loopNest", CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, SPACE, OPENANGLE, "bounds", CLOSEANGLE, NL, NULL)));

STACK_MAP_X(s, statement,
  {
    loop l = instruction_loop(statement_instruction(s));
    expression el =range_lower(loop_range(l));
    expression eu =range_upper(loop_range(l));
    expression new_eu= expression_plusplus(eu);

  string_buffer_append(result,
		       strdup(concatenate(TAB,SPACE,SPACE,SPACE,SPACE,OPENANGLE,"bound idx =",QUOTE,entity_user_name(loop_index(l)),QUOTE,NULL)));
  string_buffer_append(result,
		       strdup(concatenate(SPACE, "lower =",QUOTE, words_to_string(words_expression(el)),QUOTE,NULL)));
  string_buffer_append(result,
		       strdup(concatenate(SPACE, "upper =", QUOTE, words_to_string(words_expression(new_eu)),QUOTE, SLASH, CLOSEANGLE,NL,NULL)));
  },
	      st, 0);

  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, SPACE,  OPENANGLE,SLASH "bounds", CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, OPENANGLE,SLASH, "loopNest", CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,  OPENANGLE, SLASH, "openLoop", CLOSEANGLE, NL, NULL)));
}



static void xml_reference(int taskNumber __attribute__ ((unused)), reference r, bool wmode,
		      string_buffer result)
{

 string varname = entity_user_name(reference_variable(r));
 string_buffer_append
   (result,
    strdup(concatenate(SPACE, QUOTE, XML_ARRAY_PREFIX, varname, QUOTE, SPACE, "accessMode =", QUOTE,
		       (wmode?"W":"R"),QUOTE, CLOSEANGLE,NL,
		       NULL)));
}

static void  find_motif(Psysteme ps, Pvecteur nested_indices, int dim, int nb_dim __attribute__ ((unused)), Pcontrainte *bound_inf, Pcontrainte *bound_sup, Pcontrainte *iterator, int *motif_up_bound, int *lowerbound, int *upperbound)
{
  Variable phi;
  Value	v;
  Pvecteur pi;
  Pcontrainte c, next, cl, cu, cl_dup, cu_dup,lind, lind_dup,
    list_cl=NULL,
    list_cu=NULL,
    list_ind=NULL;
  int lower =1;
  int upper =2;
  int ind =3;
  Pcontrainte bounds[3][3];
  int nb_bounds =0;
  int nb_lower = 0;
  int nb_upper = 0;
  int nb_indices=0;
  int i,j;
  Pbase vars_to_eliminate = BASE_NULLE;

   
  for (i=1; i<=3;i++)
    for (j=1; j<=3;j++)
      bounds[i][j]=CONTRAINTE_UNDEFINED;
  
  phi = (Variable) make_phi_entity(dim); 
 
  
  /* elimination des variables autres de les phi et les indices de boucles englobants
copie de la base + mise a zero des indices englobants + projection selon les elem de ce vecteur*/

  vars_to_eliminate = vect_copy(ps->base); 
  /* printf("Base des variables :\n");
  vect_print(vars_to_eliminate, entity_local_name);
  */
  vect_erase_var(&vars_to_eliminate, phi);
  for (pi = nested_indices; !VECTEUR_NUL_P(pi); pi = pi->succ)  
    vect_erase_var(&vars_to_eliminate, var_of(pi));
  
  /* printf("Elimination des variables :\n");
  vect_print(vars_to_eliminate, entity_local_name);
  */

  sc_projection_along_variables_ofl_ctrl(&ps,vars_to_eliminate , NO_OFL_CTRL);

  for(c = sc_inegalites(ps), next=(c==NULL ? NULL : c->succ); 
      c!=NULL; 
      c=next, next=(c==NULL ? NULL : c->succ))
    { 
      Pvecteur indices_in_vecteur = VECTEUR_NUL;
      /* printf("Tri de la contrainte :\n");
      vect_print(c->vecteur, entity_local_name);
      */
      v = vect_coeff(phi, c->vecteur);
      for (pi = nested_indices; !VECTEUR_NUL_P(pi); pi = pi->succ)
	{  
	  int coeff_index = vect_coeff(var_of(pi),c->vecteur);
	  if (coeff_index)
	    vect_add_elem(&indices_in_vecteur,var_of(pi), coeff_index);
	}
      

      nb_indices=vect_size(indices_in_vecteur);
      nb_indices = (nb_indices >2) ? 2 : nb_indices;
      
      if (value_pos_p(v)) {
	c->succ = bounds[upper][nb_indices+1]; 
	bounds[upper][nb_indices+1] = c;
       	/* printf(" bornes inf avec indices de boucles englobants :\n");
	   vect_print(bounds[upper][nb_indices+1]->vecteur, entity_local_name); */
	nb_upper ++;
      }
      else if (value_neg_p(v)) {
	c->succ = bounds[lower][nb_indices+1]; 
	bounds[lower][nb_indices+1] = c;
       	/* printf(" bornes inf avec indices de boucles englobants :\n");
	   vect_print(bounds[lower][nb_indices+1]->vecteur, entity_local_name);*/
	lind = contrainte_make(indices_in_vecteur); 
	lind->succ = bounds[ind][nb_indices+1]; 
	bounds[ind][nb_indices+1] = lind;
	/* printf(" indices contenus dans la contrainte :\n");
	   vect_print(bounds[ind][nb_indices+1]->vecteur, entity_local_name); */
	nb_lower ++;
      }
    }
  /* printf("Nb borne inf = %d, Nb borne sup = %d ;\n",nb_lower,nb_upper); */
  

   if  (!CONTRAINTE_UNDEFINED_P(bounds[lower][2])) {
     /* case with 1 loop index in the loop bound constraints */ 
     for(cl = bounds[lower][2], lind= bounds[ind][2]; cl !=NULL; cl=cl->succ,lind=lind->succ)  {
       for(cu = bounds[upper][2]; cu !=NULL; cu =cu->succ) {
	 /*  printf("Tests de la negation des  contraintes :\n");
	 vect_print(cl->vecteur, entity_local_name);
	 vect_print(cu->vecteur, entity_local_name); */
	 if (vect_opposite_except(cl->vecteur,cu->vecteur,TCST)){
	   cl_dup = contrainte_dup(cl);
	   cl_dup->succ = list_cl, list_cl=cl_dup;
	   cu_dup = contrainte_dup(cu);
	   cu_dup->succ = list_cu, list_cu=cu_dup;
	   lind_dup = contrainte_dup(lind);
	   lind_dup->succ = list_ind, list_ind = lind_dup;
	   nb_bounds ++;
	 }
       }
     }  
     *bound_inf= list_cl;
     *bound_sup = list_cu;
     *iterator = list_ind;
     *motif_up_bound =- vect_coeff(TCST,list_cl->vecteur) - vect_coeff(TCST,list_cu->vecteur) +1;
     *lowerbound = vect_coeff(TCST,list_cl->vecteur);
     *upperbound = vect_coeff(TCST,list_cu->vecteur)+1;
   }
   else if (!CONTRAINTE_UNDEFINED_P(bounds[lower][1]) && !CONTRAINTE_UNDEFINED_P(bounds[upper][1])) {
     /* case where loop bounds are numeric */
     *bound_inf= bounds[lower][1];
     *bound_sup = bounds[upper][1];
     *iterator =  bounds[ind][1];
     *motif_up_bound = - vect_coeff(TCST,bounds[lower][1]->vecteur) 
       - vect_coeff(TCST,bounds[upper][1]->vecteur)+1;
     *upperbound = vect_coeff(TCST,bounds[upper][1]->vecteur)+1;
     *lowerbound = vect_coeff(TCST,bounds[lower][1]->vecteur);
   } else {
     /* Only bounds with several loop indices */ 
     /* printf("PB - Only bounds with several loop indices\n"); */ 
    *bound_inf= CONTRAINTE_UNDEFINED;
    *bound_sup = CONTRAINTE_UNDEFINED;
    *iterator = CONTRAINTE_UNDEFINED;
    *motif_up_bound = 1;
    *upperbound = 1;
    *lowerbound = 0;
    
   }
  
}


static void xml_tiling(int taskNumber, reference ref,  region reg, stack indices,  string_buffer result)
{
  fprintf(stderr,"XML");
  
  Psysteme ps_reg = sc_dup(region_system(reg));
  
  entity var = reference_variable(ref);
  int dim = (int) gen_length(variable_dimensions(type_variable(entity_type(var))));
  int i, j ;
  
  string_buffer buffer_bound = string_buffer_make();
  string_buffer buffer_offset = string_buffer_make();
  string_buffer buffer_fitting = string_buffer_make();
  string_buffer buffer_paving = string_buffer_make();
 
  string string_bound = "";
  string string_offset = "";
  string string_paving = "";
  string string_fitting =  "";
  
  Pvecteur iterat, pi= VECTEUR_NUL;
  Pcontrainte bound_inf = CONTRAINTE_UNDEFINED;
  Pcontrainte bound_up = CONTRAINTE_UNDEFINED;
  Pcontrainte iterator = CONTRAINTE_UNDEFINED;
  int motif_up_bound =0;
  int lowerbound = 0;
  int upperbound = 0;
  int dim_indices= stack_size(indices);
  int pav_matrix[10][10], fit_matrix[10][10];

  for (i=1; i<=9;i++)
    for (j=1;j<=9;j++)
      pav_matrix[i][j]=0, fit_matrix[i][j]=0;

  
  STACK_MAP_X(index,entity,
  { 
    vect_add_elem (&pi,(Variable) index ,VALUE_ONE);
  }, indices,1);

  for(i=1; i<=dim ; i++)
    { 
      Psysteme ps = sc_dup(ps_reg); 
      sc_transform_eg_in_ineg(ps);
     
      find_motif(ps, pi, i,  dim, &bound_inf, &bound_up, &iterator, 
		 &motif_up_bound, &lowerbound, &upperbound);


   
      string_buffer_append(buffer_offset,
			   strdup(concatenate(TAB,TAB,OPENANGLE,"offset val =", QUOTE,  
					      (CONTRAINTE_UNDEFINED_P(bound_inf))? "0" : 
					      i2a(vect_coeff(TCST,bound_inf->vecteur)),
					      QUOTE, SLASH, CLOSEANGLE,NL,NULL)));	
    
      
   
      if (!CONTRAINTE_UNDEFINED_P(iterator)) {
	for (iterat = pi, j=1; !VECTEUR_NUL_P(iterat); iterat = iterat->succ, j++)   
	  pav_matrix[i][j]= vect_coeff(var_of(iterat),iterator->vecteur);
      }
      
   
      if (!CONTRAINTE_UNDEFINED_P(bound_inf))
	fit_matrix[i][i]= (motif_up_bound >1) ? 1:0;
	

   
      string_buffer_append(buffer_bound,
			   strdup(concatenate(TAB,TAB, OPENANGLE, "bound idx=", 
					      QUOTE, XML_MOTIF_PREFIX, i2a(taskNumber),"_", 
					      entity_user_name(var), "_",i2a(i),QUOTE, SPACE, 
					      "lower =" QUOTE,i2a(lowerbound),QUOTE,
					      SPACE, "upper =", QUOTE, i2a(upperbound), 
					      QUOTE, SLASH, CLOSEANGLE, 
					      NL,NULL))); 

    } 
  
  for (j=1; j<=dim_indices ; j++){
    string_buffer_append(buffer_paving,strdup(concatenate(TAB,TAB, OPENANGLE,"row",
							  CLOSEANGLE,NULL)));
    for(i=1; i<=dim ; i++)
      string_buffer_append(buffer_paving,
			   strdup(concatenate(OPENANGLE, "cell val =", QUOTE,
					      i2a( pav_matrix[i][j]),QUOTE, SLASH,
					      CLOSEANGLE,NULL)));
    
    string_buffer_append(buffer_paving,strdup(concatenate(OPENANGLE,SLASH, "row",
							  CLOSEANGLE,NL,NULL)));
	      
  }
  for(i=1; i<=dim ; i++) { 
    string_buffer_append(buffer_fitting,strdup(concatenate(TAB, TAB,OPENANGLE,"row",
							   CLOSEANGLE,NULL)));    
    for(j=1; j<=dim ; j++)
      string_buffer_append(buffer_fitting, strdup(concatenate(OPENANGLE, "cell val =", QUOTE,
							      i2a( fit_matrix[i][j]),
							      QUOTE, SLASH,
							      CLOSEANGLE,NULL)));

    string_buffer_append(buffer_fitting,strdup(concatenate(OPENANGLE,SLASH, "row",
							   CLOSEANGLE,NL,NULL)));
    
  }
  
  
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,SPACE,OPENANGLE,"origin",
						 CLOSEANGLE,NL,NULL)));
  string_offset =string_buffer_to_string(buffer_offset);
  string_buffer_append(result,string_offset);
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,SPACE,OPENANGLE,SLASH,"origin",
						 CLOSEANGLE,NL,NULL)));
  
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,SPACE,OPENANGLE,"fitting",
						 CLOSEANGLE,NL,NULL))); 
  string_fitting =string_buffer_to_string(buffer_fitting);
  string_buffer_append(result,string_fitting);
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,SPACE,OPENANGLE,SLASH,"fitting",
						 CLOSEANGLE,NL,NULL)));
  
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,SPACE,OPENANGLE,"paving",
						 CLOSEANGLE,NL,NULL))); 
  string_paving =string_buffer_to_string(buffer_paving);
  string_buffer_append(result,string_paving);
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,SPACE,OPENANGLE,SLASH,"paving",
						 CLOSEANGLE,NL,NULL)));
  
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, OPENANGLE, "inLoop", 
						 CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, OPENANGLE, "loopNest", 
						 CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, SPACE, OPENANGLE, "bounds", 
						 CLOSEANGLE, NL, NULL)));
      
  
  string_bound =string_buffer_to_string(buffer_bound); 
  string_buffer_append(result,string_bound);

  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, SPACE,OPENANGLE,
						 SLASH "bounds", CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, OPENANGLE,SLASH, "loopNest", 
						 CLOSEANGLE, NL, NULL)));
  string_buffer_append(result,strdup(concatenate(TAB,SPACE,  OPENANGLE, SLASH, "inLoop", 
						 CLOSEANGLE, NL, NULL)));
      
}

static void xml_references(int taskNumber, list l_regions, stack indices, string_buffer result)
{ 
  list lr; 
  bool atleast_one_read_ref = FALSE;
  bool atleast_one_written_ref = FALSE;
  /*   Read array references first */
   for ( lr = l_regions; !ENDP(lr); lr = CDR(lr))
     {
       region re = REGION(CAR(lr));
       reference ref = region_reference(re);
       if (array_reference_p(ref) && region_read_p(re)) {
	 atleast_one_read_ref = TRUE;
	 string_buffer_append(result,strdup(concatenate(TAB, SPACE, SPACE, OPENANGLE,  "data darray=",NULL)));
	 xml_reference(taskNumber, ref,  region_write_p(re), result);
	 xml_tiling(taskNumber, ref,re, indices, result);
       } 
      }
   if (!atleast_one_read_ref) 
     string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE, "dummyDATA", 
						    NL,NULL)));
   
   for ( lr = l_regions; !ENDP(lr); lr = CDR(lr))
     {
       region re = REGION(CAR(lr));
       reference ref = region_reference(re);
       if (array_reference_p(ref) && region_write_p(re)) { 
	 atleast_one_written_ref = TRUE;
	 string_buffer_append(result,strdup(concatenate(TAB, SPACE, SPACE, OPENANGLE, "data darray=",NULL)));
	 xml_reference(taskNumber, ref,  region_write_p(re), result);
   	 xml_tiling(taskNumber, ref,re, indices, result); 
    }  
     }   
   if (!atleast_one_written_ref) 
     string_buffer_append(result,strdup(concatenate(TAB,SPACE, SPACE,"dummyDATA",NL,NULL)));
   
}

static void xml_data(int taskNumber,statement s, stack indices, string_buffer result )
{  

  list  l_regions = regions_dup(load_statement_local_regions(s)); 
  string_buffer_append(result,strdup(concatenate(TAB,SPACE, OPENANGLE, "dataList", CLOSEANGLE, NL,NULL)));
  /* 
  ifdebug(2) {
    fprintf(stderr, "\n list of regions ");    
    print_regions(l_regions);
    fprintf(stderr, "\n for the statement");    
    print_statement(s);    
  }  
  */
   xml_references(taskNumber, l_regions, indices, result);
 
   /*
    xml_tiling();
    xml_motif();
   */
        string_buffer_append(result,strdup(concatenate(TAB,SPACE,OPENANGLE, SLASH, "dataList", CLOSEANGLE,NL,NULL)));
	regions_free(l_regions);
}
    
static string task_complexity(statement s)
{
  complexity stat_comp = load_statement_complexity(s);
  string r;
    if(stat_comp != (complexity) HASH_UNDEFINED_VALUE && !complexity_zero_p(stat_comp)) {
	cons *pc = CHAIN_SWORD(NIL, complexity_sprint(stat_comp, FALSE,
						TRUE));
	 r = words_to_string(pc);
    }
    else r = i2a(1);
    return  (r);
}
static void xml_task( int taskNumber, nest_context_p nest, string_buffer result)
{
  
  statement s = gen_array_item(nest->nested_call,taskNumber);
  stack st = gen_array_item(nest->nested_loops,taskNumber); 
  stack sindices = gen_array_item(nest->nested_loop_indices,taskNumber); 
  
  string_buffer_append(result, strdup(concatenate(NL,TAB,OPENANGLE,"task name =", QUOTE, XML_TASK_PREFIX,i2a(taskNumber),QUOTE, CLOSEANGLE, NL, NULL)));
  string_buffer_append(result, strdup(concatenate(TAB, SPACE, OPENANGLE, "unitSpentTime", CLOSEANGLE,task_complexity(s),NULL))); 
  string_buffer_append(result, strdup(concatenate(OPENANGLE,SLASH,"unitSpentTime",CLOSEANGLE,NL,NULL)));
  
  xml_loop(st, result);
  xml_data (taskNumber, s,sindices, result);
  string_buffer_append(result, strdup(concatenate(TAB,OPENANGLE,SLASH,"task",CLOSEANGLE, NL, NULL)));
 
}

static void  xml_tasks(statement stat, string_buffer result){

  string  module_name = get_current_module_name();
  nest_context_t nest;
  int taskNumber =0;
  nest.loops_for_call = stack_make(statement_domain,0,0);
  nest.loop_indices = stack_make(entity_domain,0,0);
  nest.current_stat = stack_make(statement_domain,0,0);
  nest.nested_loops=  gen_array_make(0);
  nest.nested_loop_indices =  gen_array_make(0);
  nest.nested_call=  gen_array_make(0);
  
  string_buffer_append(result, strdup(concatenate(NL,SPACE,OPENANGLE,"tasks",CLOSEANGLE,NL, NULL)));
  
  if(statement_undefined_p(stat)) {
    pips_internal_error("statement error");
  }
  
  search_nested_loops_and_calls(stat,&nest);
  /* ifdebug(2)  print_call_selection(&nest); */
  
 // string_buffer_append(result(concatenate(NL,OPENANGLE,"tasks",CLOSEANGLE,NL, NULL)));
  
  for (taskNumber = 0; taskNumber<(int) gen_array_nitems(nest.nested_call); taskNumber++)
    
    xml_task(taskNumber, &nest,result);
  
    
  string_buffer_append(result, strdup(concatenate(SPACE,OPENANGLE, SLASH, "tasks", CLOSEANGLE, NL,NULL)));
  string_buffer_append(result,
		       strdup(concatenate(SPACE, OPENANGLE, 
					  "application name = ", 
					  QUOTE, module_name, QUOTE, CLOSEANGLE 
					  NL, NULL)));
  
  for(taskNumber = 0; taskNumber<(int) gen_array_nitems(nest.nested_call)-1; taskNumber++)
   string_buffer_append(result,strdup(concatenate(TAB, OPENANGLE, "taskref ref = ", QUOTE, XML_TASK_PREFIX,
						  i2a(taskNumber),QUOTE, SLASH,  CLOSEANGLE, NL, NULL)));  
  
   string_buffer_append(result,strdup(concatenate(TAB, OPENANGLE, "taskref ref = ", QUOTE, XML_TASK_PREFIX,
 						  i2a(taskNumber),QUOTE, SLASH,  CLOSEANGLE, NL, NULL)));  
 
   string_buffer_append(result,strdup(concatenate(SPACE, OPENANGLE, SLASH, "application",CLOSEANGLE, NL, NULL)));
   
  gen_array_free(nest.nested_loops);
  gen_array_free(nest.nested_loop_indices);
  gen_array_free(nest.nested_call);
  stack_free(&(nest.loops_for_call));
  stack_free(&(nest.loop_indices));
  stack_free(&(nest.current_stat));

}

/* Creates string for xml pretty printer.
   This string divides in declarations (array decl.) and 
   tasks which are loopnests with an instruction at the core.
*/

static string xml_code(entity module, statement stat)
{
  string_buffer result=string_buffer_make();
  string result2;

  string_buffer_append(result,strdup(concatenate(OPENANGLE, "module name =", QUOTE,get_current_module_name(), QUOTE, CLOSEANGLE, NL, NULL )));
  xml_declarations(module,result);
  xml_tasks(stat,result);

  string_buffer_append(result,strdup(concatenate(OPENANGLE, SLASH, "module", CLOSEANGLE, NL, NULL )));
  result2=string_buffer_to_string(result); 
  /*  string_buffer_free(&result,TRUE); */
  /* ifdebug(2)
    {
      printf("%s", result2);
      } */
  return result2;
}

static bool valid_specification_p(entity module __attribute__ ((unused)), 
				  statement stat __attribute__ ((unused)))
{ return TRUE;
}

/******************************************************** PIPSMAKE INTERFACE */

#define XMLPRETTY    ".xml"

/* Initiates xml pretty print modules
*/

bool print_xml_code(string module_name)
{
  FILE * out;
  string ppt;
  entity module = module_name_to_entity(module_name);
  string xml = db_build_file_resource_name(DBR_XML_PRINTED_FILE, 
					      module_name, XMLPRETTY);
  string  dir = db_get_current_workspace_directory();
  string filename = strdup(concatenate(dir, "/", xml, NULL));
  
  statement stat=(statement) db_get_memory_resource(DBR_CODE, 
						    module_name, TRUE);

  init_cost_table();
 /* Get the READ and WRITE regions of the module */
   set_rw_effects((statement_effects) 
		 db_get_memory_resource(DBR_REGIONS, module_name, TRUE)); 

  set_complexity_map( (statement_mapping)
	db_get_memory_resource(DBR_COMPLEXITIES, module_name, TRUE));

  if(statement_undefined_p(stat))
    {
      pips_internal_error("No statement for module %s\n", module_name);
    }
  set_current_module_entity(module);
  set_current_module_statement(stat);

  debug_on("XMLPRETTYPRINTER_DEBUG_LEVEL"); 
  pips_debug(1, "Spec validation before xml prettyprinter for %s\n", 
	     entity_name(module));
  if (valid_specification_p(module,stat)){ 
    pips_debug(1, "Spec is valid\n");
    pips_debug(1, "Begin Xml prettyprinter for %s\n", entity_name(module));
    
    ppt = xml_code(module, stat);
    pips_debug(1, "end\n");
    debug_off();  
    
    /* save to file */
    out = safe_fopen(filename, "w");
    fprintf(out,"%s",ppt);
    safe_fclose(out, filename);
    free(ppt);
  }

  free(dir);
  free(filename);

  DB_PUT_FILE_RESOURCE(DBR_XML_PRINTED_FILE, module_name, xml);

  reset_current_module_statement();
  reset_current_module_entity();

  return TRUE;
}



//================== PRETTYPRINT TERAOPT DTD ============================






static string vect_to_string(Pvecteur pv) {
  return  words_to_string(words_syntax(expression_syntax(make_vecteur_expression(pv))));
}


boolean vect_one_p(Pvecteur v) {
  return  (!VECTEUR_NUL_P(v) && vect_size(v) == 1 && vect_coeff(TCST, v) ==1); 
}

boolean vect_zero_p(Pvecteur v) {
  return  (VECTEUR_NUL_P(v) || 
	   (!VECTEUR_NUL_P(v) && vect_size(v) == 1 && value_zero_p(vect_coeff(TCST, v)))); 
  
}

static void type_and_size_of_var(entity var, char ** datatype, int *size)
{
  // type t = ultimate_type(entity_type(var)); 
  type t = entity_type(var);
  if (type_variable_p(t)) {
    basic b = variable_basic(type_variable(t));
    entity eb = entity_undefined; 
    int e = SizeOfElements(b);
    if (e==-1)
      *size = 9999;
    else
      *size = e;
    switch (basic_tag(b)) 
      {
      case is_basic_int:
	*datatype = "INTEGER ";
	break;
      case is_basic_float:
	*datatype = "REAL";
	break;
      case is_basic_logical:
	*datatype = "BOOLEAN";
	break;
      case is_basic_complex: 
	*datatype = "COMPLEX";
	break;
      case is_basic_string: 
     	*datatype = "CHARACTER";
       
	break;
      case is_basic_pointer: 
	*datatype = "POINTER";
	break; 
      case is_basic_derived:{ 
	eb = basic_derived(b);
	*datatype = entity_user_name(eb);
	break;
      }
      case is_basic_typedef: {
	eb = basic_typedef(b);
	*datatype = entity_user_name(eb);
	break;
      }
      default:{
	*datatype = "UNKNOWN";
      }
      }
  }
}


static void add_margin(int gm, string_buffer sb_result) {
  int i; 
  for (i=1;i<=gm;i++) 
    string_buffer_append(sb_result,strdup(TB1));
}

static void string_buffer_append_word(string str, string_buffer sb_result)
{
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  str, 
					  CLOSEANGLE, 
					  NL, NULL)));
}

static void string_buffer_append_symbolic(string str, string_buffer sb_result){
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE,"Symbolic",CLOSEANGLE, 
					  str,
					  OPENANGLE,"/Symbolic",CLOSEANGLE, 
					  NL, NULL)));
}

static void string_buffer_append_numeric(string str, string_buffer sb_result){
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE,"Numeric",CLOSEANGLE, 
					  str,
					  OPENANGLE,"/Numeric",CLOSEANGLE, 
					  NL, NULL)));
}


boolean main_c_program_p(string module_name)
{
  return (strstr(module_name,"main")!=NULL);

}


static void
find_loops_and_calls_in_box(statement stmp, nest_context_p nest)
{
  gen_context_multi_recurse(stmp,nest, loop_domain,push_loop,pop_loop,
			    statement_domain, push_current_statement,pop_current_statement,
			    test_domain, push_test, pop_test,
			    call_domain,call_selection,store_call_context,
			    NULL);
}

// A completer
static void xml_Library(string_buffer sb_result)
{
  string_buffer_append_word("Library",sb_result);
  string_buffer_append_word("/Library",sb_result);
}
// A completer
static void xml_Returns(string_buffer sb_result __attribute__ ((unused)))
{
  //string_buffer_append_word("Returns",sb_result);
  //string_buffer_append_word("/Returns",sb_result);
}
// A completer
static void xml_Timecosts(string_buffer sb_result __attribute__ ((unused)))
{
  //string_buffer_append_word("Timecosts",sb_result);
  // string_buffer_append_word("/Timecosts",sb_result);
}

// A completer
static void  xml_AccessFunction(string_buffer sb_result __attribute__ ((unused)))
{
  //string_buffer_append_word("AccessFunction",sb_result);
  //string_buffer_append_word("/AccessFunction",sb_result);
}
// A completer
static void xml_Regions(string_buffer sb_result __attribute__ ((unused)))
{
  // string_buffer_append_word("Regions",sb_result);
  // string_buffer_append_word("/Regions",sb_result);
}
// A completer
static void xml_CodeSize(string_buffer sb_result)
{
  string_buffer_append_word("CodeSize",sb_result);
  string_buffer_append_word("/CodeSize",sb_result);
}

void insert_xml_callees(string module_name) {
  FILE * out;
  string dir = db_get_current_workspace_directory();
  string sm = db_build_file_resource_name(DBR_XML_PRINTED_FILE, 
					  module_name, XMLPRETTY);
  string xml_module_name = strdup(concatenate(dir, "/", sm, NULL));
  callees callers = (callees)db_get_memory_resource(DBR_CALLEES,module_name, TRUE);
  out = safe_fopen(xml_module_name, "a");

  MAP(STRING, callee_name, {
    string sc=(string) db_get_memory_resource(DBR_XML_PRINTED_FILE, 
					      callee_name, TRUE);  
    string xml_callee_name = strdup(concatenate(dir, "/", sc, NULL));
     safe_append(out, xml_callee_name,0, TRUE);
    free(xml_callee_name);
  },
      callees_callees(callers)); 
  safe_fclose(out, xml_module_name);
  free(xml_module_name);
}

void insert_xml_string(string module_name, string s) {
  FILE * out;
  string dir = db_get_current_workspace_directory();
  string sm = db_build_file_resource_name(DBR_XML_PRINTED_FILE, 
					  module_name, XMLPRETTY);
  string xml_module_name = strdup(concatenate(dir, "/", sm, NULL));
  out = safe_fopen(xml_module_name, "a");
  fprintf(out,"%s",s);
  safe_fclose(out, xml_module_name);
  free(xml_module_name);
}


// A changer par une fonction qui detectera si la variable a ete definie 
// dans un fichier de parametres ...
static boolean entity_xml_parameter_p(entity e)
{
  string s = entity_local_name(e);
  boolean b=FALSE;
  if (strstr(s,"PARAM")!=NULL || strstr(s,"param")!=NULL) b = TRUE;
    return (b);
}


static void  find_pattern(Psysteme ps, Pvecteur paving_indices, Pvecteur formal_parameters, int dim,  Pcontrainte *bound_inf, Pcontrainte *bound_sup, Pcontrainte *pattern_up_bound , Pcontrainte *iterator)
{
  Variable phi;
  Value	v;
  Pvecteur pi;
  Pcontrainte c, next, cl, cu, cl_dup, cu_dup,pattern_dup, lind, lind_dup,
    list_cl=NULL,
    list_cu=NULL,
    list_ind=NULL,
    list_pattern=NULL,
    pattern = CONTRAINTE_UNDEFINED;
  int lower =1;
  int upper =2;
  int ind =3;
  Pcontrainte bounds[3][3];
  int nb_bounds =0;
  int nb_lower = 0;
  int nb_upper = 0;
  int nb_indices=0;
  int i,j;
  Pbase vars_to_eliminate = BASE_NULLE;
  Pvecteur vdiff = VECTEUR_NUL;
   
  for (i=1; i<=3;i++)
    for (j=1; j<=3;j++)
      bounds[i][j]=CONTRAINTE_UNDEFINED;
  
  phi = (Variable) make_phi_entity(dim); 
 
  
  /* Liste des  variables a eliminer autres de les phi et les indices de boucles englobants 
     et les parametres formels de la fonction.
     copie de la base + mise a zero des indices englobants 
     + projection selon les elem de ce vecteur
  */

  vars_to_eliminate = vect_copy(ps->base); 
  vect_erase_var(&vars_to_eliminate, phi);
 
  //printf("paving_indices:\n");
  //vect_fprint(stdout,paving_indices,(char * (*)(Variable))  entity_local_name);

  for (pi = paving_indices; !VECTEUR_NUL_P(pi); pi = pi->succ)  
    vect_erase_var(&vars_to_eliminate, var_of(pi));
  //printf("formal_parameters:\n");
  // vect_fprint(stdout,formal_parameters,(char * (*)(Variable)) entity_local_name);

  for (pi = formal_parameters; !VECTEUR_NUL_P(pi); pi = pi->succ)  
    vect_erase_var(&vars_to_eliminate, var_of(pi));
  //printf("vars_to_eliminate:\n");
  //vect_fprint(stdout,vars_to_eliminate,(char * (*)(Variable)) entity_local_name);

  sc_projection_along_variables_ofl_ctrl(&ps,vars_to_eliminate , NO_OFL_CTRL);

  //printf("Systeme a partir duquel on genere les contraintes  du motif:\n");
  //sc_fprint(stdout,ps,entity_local_name);

  for(c = sc_inegalites(ps), next=(c==NULL ? NULL : c->succ); 
      c!=NULL; 
      c=next, next=(c==NULL ? NULL : c->succ))
    { 
      Pvecteur indices_in_vecteur = VECTEUR_NUL;
      vdiff = VECTEUR_NUL;
      v = vect_coeff(phi, c->vecteur);
      for (pi = paving_indices; !VECTEUR_NUL_P(pi); pi = pi->succ)
	{  
	  int coeff_index = vect_coeff(var_of(pi),c->vecteur);
	  // CA prendre la valeur absolue de coeff_index
	  if (coeff_index)
	    vect_add_elem(&indices_in_vecteur,var_of(pi), coeff_index);
	}
      
      /* on classe toutes les contraintes ayant plus de 2 indices de boucles 
	 externes ensemble */
      nb_indices=vect_size(indices_in_vecteur);
      nb_indices = (nb_indices >2) ? 2 : nb_indices;
      
      if (value_pos_p(v)) {
	c->succ = bounds[upper][nb_indices+1]; 
	bounds[upper][nb_indices+1] = c;
       	/* printf(" bornes inf avec indices de boucles englobants :\n");
	   vect_print(bounds[upper][nb_indices+1]->vecteur, entity_local_name); */
	nb_upper ++;
      }
      else if (value_neg_p(v)) {
	c->succ = bounds[lower][nb_indices+1]; 
	bounds[lower][nb_indices+1] = c;
       	/* printf(" bornes inf avec indices de boucles englobants :\n");
	   vect_print(bounds[lower][nb_indices+1]->vecteur, entity_local_name);*/
	lind = contrainte_make(indices_in_vecteur); 
	lind->succ = bounds[ind][nb_indices+1]; 
	bounds[ind][nb_indices+1] = lind;
	/* printf(" indices contenus dans la contrainte :\n");
	   vect_print(bounds[ind][nb_indices+1]->vecteur, entity_local_name); */
	nb_lower ++;
      }
    }
  /* printf("Nb borne inf = %d, Nb borne sup = %d ;\n",nb_lower,nb_upper); */
  

  if  (!CONTRAINTE_UNDEFINED_P(bounds[lower][2])) {
    /* case with 1 loop index in the loop bound constraints */ 
    for(cl = bounds[lower][2], lind= bounds[ind][2]; cl !=NULL; cl=cl->succ,lind=lind->succ)  {
      for(cu = bounds[upper][2]; cu !=NULL; cu =cu->succ) {
	vdiff = vect_add(cu->vecteur,cl->vecteur);
	vect_chg_sgn(vdiff);
	vect_add_elem(&vdiff,TCST,1);
	pattern = contrainte_make(vect_dup(vdiff));
	for (pi = formal_parameters; !VECTEUR_NUL_P(pi); pi = pi->succ)  
	  vect_erase_var(&vdiff, var_of(pi));
	 
	if (vect_dimension(vdiff)==0) {
	  cl_dup = contrainte_dup(cl);
	  cu_dup = contrainte_dup(cu);

	  for (pi = lind->vecteur; !VECTEUR_NUL_P(pi); pi = pi->succ)  
	    {
	      vect_erase_var(&(cl_dup->vecteur), var_of(pi));
	      vect_erase_var(&(cu_dup->vecteur), var_of(pi));
	    }
	  vect_erase_var(&(cl_dup->vecteur), phi);
	  vect_erase_var(&(cu_dup->vecteur), phi);
	  
	  cl_dup->succ = list_cl, list_cl=cl_dup;

	  vect_chg_sgn(cu_dup->vecteur);
	  cu_dup->succ = list_cu, list_cu=cu_dup;

	  lind_dup = contrainte_dup(lind);
	  lind_dup->succ = list_ind, list_ind = lind_dup;
	  pattern_dup = pattern;
	  pattern_dup->succ = list_pattern, list_pattern = pattern_dup;
	  nb_bounds ++;
	}
      }
    }  
    *bound_inf= list_cl;
    *bound_sup = list_cu;
    *pattern_up_bound = list_pattern;
    *iterator = list_ind;
  }
  else if (!CONTRAINTE_UNDEFINED_P(bounds[lower][1]) 
	   && !CONTRAINTE_UNDEFINED_P(bounds[upper][1])) {
    /* case where loop bounds are numeric */
    *bound_inf= bounds[lower][1];
    vect_erase_var(&(bounds[lower][1]->vecteur), phi);
    *bound_sup = bounds[upper][1];
    vect_erase_var(&(bounds[upper][1]->vecteur), phi);
    vect_chg_sgn(bounds[upper][1]->vecteur);

    for (pi = bounds[ind][1]->vecteur; !VECTEUR_NUL_P(pi); pi = pi->succ)  
      {
	vect_erase_var(&(bounds[lower][1]->vecteur), var_of(pi));
	vect_erase_var(&(bounds[upper][1]->vecteur), var_of(pi));
      }
    vdiff = vect_substract(bounds[upper][1]->vecteur,bounds[lower][1]->vecteur);
   
    vect_add_elem(&vdiff,TCST,1);
    *pattern_up_bound = contrainte_make(vdiff);
    *iterator =  bounds[ind][1];
  } 
  else {
    /* Only bounds with several loop indices */ 
    /* printf("PB - Only bounds with several loop indices\n"); */
    *bound_inf= CONTRAINTE_UNDEFINED;
    *bound_sup = CONTRAINTE_UNDEFINED;
    *iterator = CONTRAINTE_UNDEFINED; 
  } 
}

static void xml_Pattern_Paving(entity var, boolean effet_read, Pvecteur formal_parameters, list pattern_region, Pvecteur paving_indices, string_buffer sb_result)
{ 

  // int taskNumber =0;
  list lr; 
  string_buffer buffer_pattern = string_buffer_make();
  string_buffer buffer_paving = string_buffer_make();
  string string_paving = "";
  string string_pattern =  "";

  Pvecteur voffset, vpattern_up_bound;
  // pour chacune des variables
  // rechercher la region
  // pour chaque dimension : generer en meme temps le pattern et le pavage
  for ( lr = pattern_region; !ENDP(lr); lr = CDR(lr))
    {
      region reg = REGION(CAR(lr));
      reference ref = region_reference(reg);  
      entity vreg = reference_variable(ref);
      if (array_reference_p(ref) && same_entity_p(vreg,var) && region_read_p(reg)== effet_read) {
	Psysteme ps_reg = sc_dup(region_system(reg));
	Pvecteur iterat= VECTEUR_NUL;
	Pcontrainte bound_inf = CONTRAINTE_UNDEFINED;
	Pcontrainte bound_up = CONTRAINTE_UNDEFINED;
	Pcontrainte pattern_up_bound = CONTRAINTE_UNDEFINED;
	Pcontrainte iterator = CONTRAINTE_UNDEFINED;
	int dim = (int) gen_length(variable_dimensions(type_variable(entity_type(var))));
	int i ;
	int val =0;
	int inc =0;

	string_buffer_append_word("Pattern",buffer_pattern);
	string_buffer_append_word("Pavage",buffer_paving);
	
	global_margin++;
	for(i=1; i<=dim; i++)
	  { 
	    Psysteme ps = sc_dup(ps_reg); 
	    sc_transform_eg_in_ineg(ps);
	    //printf("find pattern for entity %s [dim=%d]\n",entity_local_name(var),i);
	    //sc_fprint(stdout,ps,(char * (*)(Variable)) entity_local_name);
	    find_pattern(ps, paving_indices, formal_parameters, i, &bound_inf, &bound_up, &pattern_up_bound, &iterator);

	    if (!CONTRAINTE_UNDEFINED_P(bound_inf) &&   !VECTEUR_NUL_P(bound_inf->vecteur))
	      voffset = bound_inf->vecteur;
	    else
	      voffset = vect_new(TCST,0);

	    if (!CONTRAINTE_UNDEFINED_P(pattern_up_bound) &&   !VECTEUR_NUL_P(pattern_up_bound->vecteur))
	      vpattern_up_bound = pattern_up_bound->vecteur;
	    else  { // if we cannot deduce pattern length form region, array dim size is taken
	      list ldim = variable_dimensions(type_variable(entity_type(vreg)));
	      dimension vreg_dim = find_ith_dimension(ldim,i);
	      normalized ndim = NORMALIZE_EXPRESSION(dimension_upper(vreg_dim));
	      vpattern_up_bound = vect_dup((Pvecteur) normalized_linear(ndim));
	    }
	    
	    /* PRINT PATTERN and PAVING */
	    if ( vect_zero_p(voffset)  && vect_one_p(vpattern_up_bound))
	      string_buffer_append_word("DimUnitPattern/",buffer_pattern);
	    else {
	      add_margin(global_margin,buffer_pattern);
	      // A completer - choisir un indice pour le motif ?
	      string_buffer_append(buffer_pattern,
				   strdup(concatenate(OPENANGLE, 
						      "DimPattern Index=",
						      QUOTE,QUOTE,CLOSEANGLE,NL,NULL)));

	      global_margin++;

	      string_buffer_append_word("Offset",buffer_pattern);
	      string_buffer_append_symbolic(vect_to_string(voffset),
					    buffer_pattern);   
	      if (vect_dimension(voffset)==0) 
		string_buffer_append_numeric(vect_to_string(voffset),
					     buffer_pattern);
	      string_buffer_append_word("/Offset",buffer_pattern);

	      string_buffer_append_word("Length",buffer_pattern);
	      string_buffer_append_symbolic(vect_to_string(vpattern_up_bound),
					    buffer_pattern);   
	      if (vect_dimension(vpattern_up_bound)==0) 
		string_buffer_append_numeric(vect_to_string(vpattern_up_bound),
					     buffer_pattern); 
	      string_buffer_append_word("/Length",buffer_pattern);

	      string_buffer_append_word("Stride",buffer_pattern);
	      val =1;
	      string_buffer_append_symbolic(i2a(val),buffer_pattern);
	      string_buffer_append_numeric(i2a(val),buffer_pattern);	 
	      string_buffer_append_word("/Stride",buffer_pattern);
	      global_margin--;
	      string_buffer_append_word("/DimPattern",buffer_pattern);
	    }
	    if (!CONTRAINTE_UNDEFINED_P(iterator) &&!CONTRAINTE_NULLE_P(iterator)) {
	      string_buffer_append_word("DimPavage",buffer_paving);
	      
	      for (iterat = paving_indices; !VECTEUR_NUL_P(iterat); iterat = iterat->succ)
		{
		  if ((inc = vect_coeff(var_of(iterat),iterator->vecteur)) !=0) {
		    add_margin(global_margin,buffer_paving);
		    string_buffer_append(buffer_paving,
					 strdup(concatenate(OPENANGLE, 
							    "RefLoopIndex Name=",
							    QUOTE,entity_user_name(var_of(iterat)),QUOTE, BL, 
							    "Inc=", QUOTE,
							    i2a(inc),QUOTE,"/", CLOSEANGLE,NL,NULL)));
		  }
		}
	      string_buffer_append_word("/DimPavage",buffer_paving);
	    }
	    else 
	      string_buffer_append_word("DimPavage/",buffer_paving);
	    sc_free(ps);
	    
	  }
	global_margin--;
	string_buffer_append_word("/Pattern",buffer_pattern);
	string_buffer_append_word("/Pavage",buffer_paving);
	string_pattern = string_buffer_to_string(buffer_pattern);
	string_paving = string_buffer_to_string(buffer_paving);
	string_buffer_append(sb_result, string_pattern);
	string_buffer_append(sb_result, string_paving);
	xml_AccessFunction(sb_result);
	sc_free(ps_reg); 
	
      }
    }  
}


static void xml_TaskParameter(boolean assign_function,boolean is_not_main_p, Variable var, Pvecteur formal_parameters, list pattern_region, Pvecteur paving_indices, string_buffer sb_result)
{

 list lr;
 boolean effet_read = TRUE;
 
  for ( lr = pattern_region; !ENDP(lr); lr = CDR(lr))
    { 
      region reg = REGION(CAR(lr));
      reference ref = region_reference(reg);  
      entity v = reference_variable(ref);
      // rappel : les regions contiennent les effects sur les scalaires
      // pour les fonctions, on ecrit les parametres formels dans l'ordre
      // pour le main, ordre des regions
      if ((is_not_main_p && same_entity_p(v,var)) || (!is_not_main_p &&  vect_coeff(v,paving_indices) == 0)){
	
	effet_read = (region_read_p(reg))? TRUE: FALSE;
	add_margin(global_margin,sb_result);
	string_buffer_append(sb_result,
			     strdup(concatenate(OPENANGLE, 
						(assign_function)?"AssignParameter":"TaskParameter"," Name=",
						QUOTE,entity_user_name(v),QUOTE, BL, 
						"Type=", QUOTE,(entity_xml_parameter_p(v))? "CONTROL":"DATA",QUOTE,BL,
						"AccessMode=", QUOTE, (effet_read)? "USE":"DEF",QUOTE,BL,
						"ArrayP=", QUOTE, (array_entity_p(v))?"TRUE":"FALSE",QUOTE, BL, 
						"Kind=", QUOTE,  "VARIABLE",QUOTE,
						 

						CLOSEANGLE
						NL, NULL))); 
	global_margin++;
	xml_Pattern_Paving(v, effet_read, formal_parameters, 
			   pattern_region,paving_indices, 
			   sb_result);
	
	global_margin--;
	if (assign_function)
	  string_buffer_append_word("/AssignParameter",sb_result);
	else
	  string_buffer_append_word("/TaskParameter",sb_result);
      }
    }
}


static void xml_TaskParameters(boolean assign_function, int code_tag, entity module, list pattern_region, Pvecteur paving_indices, string_buffer sb_result)
{
 
  int ith;
  int FormalParameterNumber = (int) gen_length(module_formal_parameters(module));
  Pvecteur formal_parameters = VECTEUR_NUL;
  entity FormalArrayName = entity_undefined;

  if (assign_function)
    string_buffer_append_word("AssignParameters",sb_result);
  else
    string_buffer_append_word("TaskParameters",sb_result);
  global_margin++;
  
  // Creation liste des parametres formels
  for (ith=1;ith<=FormalParameterNumber;ith++) {
    FormalArrayName = find_ith_formal_parameter(module,ith);
    vect_add_elem (&formal_parameters,(Variable) FormalArrayName,VALUE_ONE);
  }

  if (code_tag != code_is_a_main) {
    // les parametres formels doivent etre introduits dans l'ordre.
    for (ith=1;ith<=FormalParameterNumber;ith++) {
      FormalArrayName = find_ith_formal_parameter(module,ith);
  
      xml_TaskParameter(assign_function,TRUE,FormalArrayName,
			formal_parameters,pattern_region,paving_indices,sb_result);
   
    }
  }
  else   xml_TaskParameter(assign_function,FALSE,FormalArrayName,
			   formal_parameters,pattern_region,paving_indices,sb_result);


  global_margin--;
  if (assign_function)
    string_buffer_append_word("/AssignParameters",sb_result);
  else
    string_buffer_append_word("/TaskParameters",sb_result);
}


boolean  eval_linear_expression(expression exp, Psysteme ps, int *val)
{
  normalized norm= normalized_undefined;
  Pvecteur   vexp,pv;
  boolean result = TRUE;
  *val = 0;

  // fprintf(stdout,"Expression a evaluer : %s",words_to_string(words_expression(exp)));
  
  if (expression_normalized(exp) == normalized_undefined) 
    expression_normalized(exp)= NormalizeExpression(exp); 
  norm = expression_normalized(exp);
  if (normalized_linear_p(norm) && !SC_UNDEFINED_P(ps)) {
    vexp = (Pvecteur)normalized_linear(norm);
    for (pv= vexp; pv != VECTEUR_NUL; pv=pv->succ) {
      Variable v = var_of(pv);
      if (v==TCST) {
	*val += vect_coeff(TCST,vexp);
      }
      else {
	if(base_contains_variable_p(sc_base(ps), v)) {  
	  Value min, max;
	  Psysteme ps1 = sc_dup(ps);
	  bool  feasible = sc_minmax_of_variable2(ps1, (Variable)v, &min, &max);
	  if (feasible && value_eq(min,max)) {
	    *val += vect_coeff(v,vexp) *min;
	  }
	  else {
	    // fprintf(stdout,"le systeme est non faisable\n");
	    result = FALSE;
	  }
	  //  sc_free(ps1);
	}
	else 
	  result = FALSE;
      }	
    }
  }
  else {
    // fprintf(stdout,"Ce n'est pas une expression lineaire\n");
	 result = FALSE;
  }
  //fprintf(stdout,"Valeur trouvee : %d \n",*val);
  
  return result;
}

static void xml_Bounds(expression elow, expression eup,Psysteme prec, string_buffer sb_result)
{
  int low,up;
  int valr =0;
  string_buffer_append_word("LowerBound",sb_result);
  global_margin++;
  string_buffer_append_symbolic(words_to_string(words_syntax(expression_syntax(elow))),
				sb_result);     
  if (expression_integer_value(elow, &low))  
    string_buffer_append_numeric(i2a(low),sb_result);
  else if (eval_linear_expression(elow,prec,&valr))
    string_buffer_append_numeric(i2a(valr),sb_result);
  
  global_margin--;
  string_buffer_append_word("/LowerBound",sb_result);
    
  /* Print XML Array UPPER BOUND */
  string_buffer_append_word("UpperBound",sb_result);
  global_margin++;
  string_buffer_append_symbolic(words_to_string(words_syntax(expression_syntax(eup))),
				sb_result);     
  if (expression_integer_value(eup, &up))  
    string_buffer_append_numeric(i2a(up),sb_result);
  else if (eval_linear_expression(eup,prec,&valr))
    string_buffer_append_numeric(i2a(valr),sb_result);

  global_margin--;
  string_buffer_append_word("/UpperBound",sb_result);
  
}
static void xml_Bounds_and_Stride(expression elow, expression eup, expression stride, 
				  Psysteme prec, string_buffer sb_result)
{
  int inc;
  int  valr =0;
  xml_Bounds(elow, eup,prec,sb_result);
  string_buffer_append_word("Stride",sb_result);
  global_margin++;
  string_buffer_append_symbolic(words_to_string(words_syntax(expression_syntax(stride))),
				sb_result);     
  if (expression_integer_value(stride, &inc))  
    string_buffer_append_numeric(i2a(inc),sb_result);
  else if (eval_linear_expression(stride,prec,&valr))
     string_buffer_append_numeric(i2a(valr),sb_result);
 
  global_margin--;
  string_buffer_append_word("/Stride",sb_result);
    
}

static void xml_Array(entity var,Psysteme prec,string_buffer sb_result)
{
  string datatype;
  list ld, ldim = variable_dimensions(type_variable(entity_type(var)));
  int i,j, size =0;
  int nb_dim = (int) gen_length(ldim);
  char* layout_up[nb_dim];    
  char *layout_low[nb_dim];
  int no_dim=0;
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "Array Name=",
					  QUOTE,entity_user_name(var),QUOTE, BL, 
					  "Type=", QUOTE,(entity_xml_parameter_p(var))? "CONTROL":"DATA",QUOTE,BL,
					  "Allocation=", QUOTE,
					  (heap_area_p(var) || stack_area_p(var)) ? 
					  "DYNAMIC": "STATIC",
					  QUOTE,BL,
					  "Kind=", QUOTE,   "VARIABLE",QUOTE,
					  CLOSEANGLE
					  NL, NULL)));

  /* Print XML Array DATA TYPE and DATA SIZE */
  type_and_size_of_var(var, &datatype,&size); 
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE,
					  "DataType Type=",QUOTE,datatype,QUOTE, BL,
					  "Size=",QUOTE, i2a(size), QUOTE, "/"
					  CLOSEANGLE, 
					  NL,NULL)));
  /* Print XML Array DIMENSION */ 
  
  if((int) gen_length(ldim) >0) {
    string_buffer_append_word("Dimensions",sb_result);
   
    for (ld = ldim ; !ENDP(ld); ld = CDR(ld)) {
      expression elow = dimension_lower(DIMENSION(CAR(ld)));
      expression eup = dimension_upper(DIMENSION(CAR(ld)));
      global_margin++;
      add_margin(global_margin,sb_result);
      string_buffer_append(sb_result,
			   strdup(concatenate(OPENANGLE, 
					      "Dimension", 
					      CLOSEANGLE, 
					      NL, NULL)));
      /* Print XML Array Bound */
      global_margin++;
      xml_Bounds(elow,eup,prec,sb_result);
      global_margin--;
      add_margin(global_margin,sb_result);
      string_buffer_append(sb_result,
			   strdup(concatenate(OPENANGLE, 
					      "/Dimension", 
					      CLOSEANGLE, 
					      NL, NULL)));
      global_margin--;
      layout_low[no_dim] = words_to_string(words_syntax(expression_syntax(elow)));
      layout_up[no_dim] = words_to_string(words_syntax(expression_syntax(eup)));
      no_dim++;
    }
  }   
 
  string_buffer_append_word("/Dimensions",sb_result);

  /* Print XML Array LAYOUT */
  string_buffer_append_word("Layout",sb_result);
  global_margin++;
  for (i =0; i<= no_dim-1; i++) {
    string_buffer_append_word("DimLayout",sb_result); 
    global_margin++;
    string_buffer_append_word("Symbolic",sb_result);
    if (i==no_dim-1) {
      add_margin(global_margin,sb_result);
      string_buffer_append(sb_result,strdup(concatenate("1",NL,NULL)));
    }
    else { 
      add_margin(global_margin,sb_result);
      for (j=no_dim-1; j>=i+1; j--)
	{ 
	  if (strcmp(layout_low[j],"0")==0) 
	    string_buffer_append(sb_result,
				 strdup(concatenate("(",layout_up[j],"+1)",NULL))); 
	  else
	    string_buffer_append(sb_result,
				 strdup(concatenate("(",layout_up[j],"-",
						    layout_low[j],"+1)",NULL)));   
	  if (j==i+1)
	    string_buffer_append(sb_result,strdup(NL));
	}
    }
    string_buffer_append_word("/Symbolic",sb_result);
    global_margin--;
    string_buffer_append_word("/DimLayout",sb_result); 
  }
  global_margin--;
  string_buffer_append_word("/Layout",sb_result);
  string_buffer_append_word("/Array",sb_result);
}


static void xml_LocalArrays(entity module, Psysteme prec,string_buffer sb_result)
{
 list ldecl;
 int nb_dim=0;

 if (prettyprint_is_fortran)
    ldecl = code_declarations(value_code(entity_initial(module)));
  else {
    statement s = get_current_module_statement();
    ldecl = statement_declarations(s);
  }
  if((int) gen_length(ldecl) >0) {
   
    string_buffer_append_word("LocalArrays",sb_result);
    
    global_margin++;
    
    MAP(ENTITY,var, {
      if (type_variable_p(entity_type(var)) 
	  && ((nb_dim = variable_entity_dimension(var))>0)
	  &&  !(storage_formal_p(entity_storage(var)))) {
	xml_Array(var,prec,sb_result);
      }
    },ldecl);

    global_margin--;
    string_buffer_append_word("/LocalArrays",sb_result);
 
  }
  else  
    string_buffer_append_word("LocalArrays/",sb_result);
}

// A completer
// Ne faire qu'une fonction avec la precedente
static void xml_FormalArrays(entity module, Psysteme prec,string_buffer sb_result)
{

  int FormalParameterNumber = (int) gen_length(module_formal_parameters(module)); 
  entity FormalArrayName = entity_undefined;
  int ith ;
  if(FormalParameterNumber >=1) {
    string_buffer_append_word("FormalArrays",sb_result);
    global_margin++;

    for (ith=1;ith<=FormalParameterNumber;ith++) {
      FormalArrayName = find_ith_formal_parameter(module,ith);
       if (type_variable_p(entity_type(FormalArrayName)) 
	  && ( variable_entity_dimension(FormalArrayName)>0)
	  &&  (storage_formal_p(entity_storage(FormalArrayName)))) {
	 xml_Array(FormalArrayName,prec,sb_result);
       }
    }
    global_margin--;
    string_buffer_append_word("/FormalArrays",sb_result);
  } 
  else string_buffer_append_word("FormalArrays/",sb_result);
}


static void motif_in_statement(statement s)
{
  string comm = statement_comments(s);
  if (!string_undefined_p(comm) && strstr(comm,"MOTIF")!=NULL)
    motif_in_statement_p= TRUE;
}
static void box_in_statement(statement s)
{
  string comm = statement_comments(s);
  if (!string_undefined_p(comm) && strstr(comm,"BOX")!=NULL)
    box_in_statement_p= TRUE;
}


static void xml_Loop(statement s, string_buffer sb_result)
{
  loop l = instruction_loop(statement_instruction(s)); 
  transformer t = load_statement_precondition(s);
  Psysteme prec = sc_dup((Psysteme) predicate_system(transformer_relation(t)));
  expression el =range_lower(loop_range(l));
  expression eu =range_upper(loop_range(l));
  expression stride = range_increment(loop_range(l)); 
  entity index =loop_index(l);
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE,
					  "Loop Index=",QUOTE,
					  entity_user_name(index),QUOTE, BL,
					  "ExecutionMode=",QUOTE,
					  (execution_parallel_p(loop_execution(l)))? "PARALLEL":"SEQUENTIAL", 
					  QUOTE, 
					  CLOSEANGLE,NL,NULL)));
  xml_Bounds_and_Stride(el,eu,stride,prec,sb_result);
  string_buffer_append_word("/Loop",sb_result);
  sc_free(prec);
}

// if comment MOTIF is on the loop, the pattern_region is the loop region 
// if comment MOTIF is on a statement inside the sequence of the loop body
// the cumulated region of the loop body is taken 
// if comment MOTIF is outside the loop nest, the pattern_region is the call region
static void xml_Loops(stack st,boolean call_external_loop_p, list *pattern_region, Pvecteur *paving_indices, Pvecteur *pattern_indices, boolean motif_in_te_p, string_buffer sb_result)
{
  boolean in_motif_p=!call_external_loop_p && !motif_in_te_p;
  boolean motif_on_loop_p=FALSE;
  // Boucles externes a la TE
  if (call_external_loop_p)
    string_buffer_append_word("ExternalLoops",sb_result); 
  else 
    // Boucles externes au motif dans la TE
    string_buffer_append_word("Loops",sb_result); 

  global_margin++;
  if (st != STACK_NULL) {
  STACK_MAP_X(s, statement,
  {
    loop l = instruction_loop(statement_instruction(s));
    string comm = statement_comments(s);
    entity index =loop_index(l);

     if (!in_motif_p) {
      // Test : Motif is in the loop body  or not 
      motif_in_statement_p=FALSE;
      gen_recurse(loop_body(l), statement_domain, gen_true,motif_in_statement); 
      
      // comment MOTIF  is on the Loop
      motif_on_loop_p = !string_undefined_p(comm) && strstr(comm,"MOTIF")!=NULL;
      if (motif_on_loop_p) {
	  *pattern_region = regions_dup(load_statement_local_regions(s)); 
	  vect_add_elem (pattern_indices,(Variable) index ,VALUE_ONE);
	}
      else if (motif_in_statement_p) {
	  // cumulated regions on the sequence of the loop body instructions are needed
	  *pattern_region = regions_dup(load_statement_local_regions(loop_body(l))); 
	}
      
      in_motif_p =   (!call_external_loop_p && !motif_in_te_p) //Par default on englobe si TE
	 || in_motif_p        // on etait deja dans le motif
	|| motif_on_loop_p  // on vient de trouver un Motif sur la boucle
	|| (motif_in_te_p && !motif_on_loop_p && !motif_in_statement_p); 
      // motif externe au nid de boucles (cas des motif au milieu d'une sequence) 
    }
    if (!in_motif_p) {
      vect_add_elem (paving_indices,(Variable) index ,VALUE_ONE);
      xml_Loop(s, sb_result);
    }
  },
	      st, 0);
  }
  global_margin--;
  if (call_external_loop_p)
    string_buffer_append_word("/ExternalLoops",sb_result); 
  else 
    string_buffer_append_word("/Loops",sb_result); 
  
}

static Psysteme first_precondition_of_module(string module_name __attribute__ ((unused)))
{
  statement st1 = get_current_module_statement();
  statement fst = statement_undefined;
  instruction inst = statement_instruction(st1);
  transformer t;
  Psysteme prec= SC_UNDEFINED;
  switch instruction_tag(inst)
    {
    case is_instruction_sequence:{
      fst = STATEMENT(CAR(sequence_statements(instruction_sequence(inst))));
      break;
    }
   
    default:
       fst = st1;
    }
  t = (transformer)load_statement_precondition(fst);
  prec = sc_dup((Psysteme) predicate_system(transformer_relation(t)));
  return prec;
}
static void  xml_Task(string module_name, int code_tag,string_buffer sb_result)
{
  nest_context_t task_loopnest;
  task_loopnest.loops_for_call = stack_make(statement_domain,0,0);
  task_loopnest.loop_indices = stack_make(entity_domain,0,0);
  task_loopnest.current_stat = stack_make(statement_domain,0,0);
  task_loopnest.nested_loops=  gen_array_make(0);
  task_loopnest.nested_loop_indices =  gen_array_make(0);
  task_loopnest.nested_call=  gen_array_make(0); 
  stack nested_loops;
  list pattern_region =NIL; 
  Pvecteur paving_indices = VECTEUR_NUL;
  Pvecteur pattern_indices = VECTEUR_NUL;
  boolean motif_in_te_p = FALSE;
  entity module = module_name_to_entity(module_name);
  Psysteme prec;

  string string_sb_result;
  statement stat_module=(statement) db_get_memory_resource(DBR_CODE, 
							   module_name, TRUE);
  reset_rw_effects();
  set_rw_effects
    ((statement_effects)
     db_get_memory_resource(DBR_REGIONS, module_name, TRUE));

  push_current_module_statement(stat_module);
  prec = first_precondition_of_module(module_name);
  // printf("first_precondition_of_module %s\n",module_name);
  // sc_fprint(stdout,prec, (char * (*)(Variable)) entity_local_name);
  global_margin++;
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "Task Name=",QUOTE, 
					  module_name,QUOTE,CLOSEANGLE, 
					  NL, NULL)));
  global_margin++;

  find_loops_and_calls_in_box(stat_module,&task_loopnest);

  xml_Library(sb_result);
  xml_Returns(sb_result);
  xml_Timecosts(sb_result);
  xml_LocalArrays(module, prec,sb_result);
  xml_FormalArrays(module,prec,sb_result);
  /*  On ne traite qu'une TE : un seul nid de boucles */
  nested_loops = gen_array_item(task_loopnest.nested_loops,0); 

  pattern_region = regions_dup(load_statement_local_regions(stat_module));
  gen_recurse(stat_module, statement_domain, gen_true,motif_in_statement); 
  motif_in_te_p = motif_in_statement_p;
  xml_Loops(nested_loops,FALSE,&pattern_region,&paving_indices, &pattern_indices, motif_in_te_p, sb_result);

  xml_TaskParameters(FALSE,code_tag, module,pattern_region,paving_indices,sb_result);

  xml_Regions(sb_result);
  xml_CodeSize(sb_result);
  global_margin--;
  string_buffer_append_word("/Task",sb_result);
  global_margin--;
  
   string_sb_result=string_buffer_to_string(sb_result); 
  insert_xml_string(module_name,string_sb_result);
  pop_current_module_statement();
  gen_array_free(task_loopnest.nested_loops);
  gen_array_free(task_loopnest.nested_loop_indices);
  gen_array_free(task_loopnest.nested_call);
  stack_free(&(task_loopnest.loops_for_call));
  stack_free(&(task_loopnest.loop_indices));
  stack_free(&(task_loopnest.current_stat));
  regions_free(pattern_region);

}


static void xml_ActualArrays(entity module, Psysteme prec,string_buffer sb_result)
{ 
  int nb_dim =0;
  list ldecl;

   if (prettyprint_is_fortran)
    ldecl = code_declarations(value_code(entity_initial(module)));
  else {
    statement s = get_current_module_statement();
    ldecl = statement_declarations(s);
  }

  global_margin++;
  string_buffer_append_word("ActualArrays",sb_result);
  if(gen_length(ldecl) >0) {
    global_margin++;
 
    MAP(ENTITY,var, {
      if (type_variable_p(entity_type(var)) 
	  && (nb_dim = variable_entity_dimension(var))>0) {
	xml_Array(var,prec,sb_result);
      }
    },ldecl);
    global_margin--;
  }
  string_buffer_append_word("/ActualArrays",sb_result); 
  global_margin--;
}




void matrix_init(Pmatrix mat, int n, int m)
{
  int i,j;
  for (i=1;i<=n;i++) {
    for (j=1;j<=m;j++) {
      MATRIX_ELEM(mat,i,j)=0;
    }
  }
}

static void xml_Matrix(Pmatrix mat, int n, int m, string_buffer sb_result)
{
  string srow, scolumn;
  int i,j; 
  // cas des nids de boucles vides
  if (n==0 && m!=0) m=0;
  if (m==0 && n!=0) n=0;
  srow =strdup(itoa(n));
  scolumn=strdup(itoa(m));

  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "Matrix NbRows=", 
					  QUOTE,srow,QUOTE,BL,
					  "NbColumns=", QUOTE, scolumn,QUOTE,
					  BL,CLOSEANGLE,NL, NULL)));
  for (i=1;i<=n;i++) {
    add_margin(global_margin,sb_result);
    string_buffer_append(sb_result,
			 strdup(concatenate(OPENANGLE, 
					    "Row", CLOSEANGLE,BL, NULL)));
    for (j=1;j<=m;j++) {
      string_buffer_append(sb_result,
			     strdup(concatenate(OPENANGLE,"c", CLOSEANGLE,
						itoa(MATRIX_ELEM(mat,i,j)),
						OPENANGLE, "/c", CLOSEANGLE,
						BL, NULL)));
    } 
    string_buffer_append(sb_result,
			 strdup(concatenate(OPENANGLE, 
					    "/Row", CLOSEANGLE, NL, NULL)));
  }
  string_buffer_append_word("/Matrix",sb_result);
}


// A completer 
// ne traite que les cas ou tout est correctement aligne
// A traiter aussi le cas  ActualArrayDim = NIL
// 
static void xml_Connection(list  ActualArrayInd,int ActualArrayDim, int FormalArrayDim, string_buffer sb_result)
{
  Pmatrix mat; 
  int i,j;
  Pvecteur pv;
  string_buffer_append_word("Connection",sb_result); 
  mat = matrix_new(ActualArrayDim,FormalArrayDim);
  matrix_init(mat,ActualArrayDim,FormalArrayDim);
  if (prettyprint_is_fortran) {
    for (i=1;i<=ActualArrayDim && i<= FormalArrayDim;i++)
      MATRIX_ELEM(mat,i,i)=1;
  }
  else 
    for (i=ActualArrayDim, j=FormalArrayDim;i>=1 && j>=1;i=i-1,j=j-1) 
      MATRIX_ELEM(mat,i,j)=1;
    
  // Normalise les expressions lors du premier parcours
  // Utile aussi a  xml_LoopOffset
  MAP(EXPRESSION, e , { 
    if (expression_normalized(e) == normalized_undefined)
      expression_normalized(e)= NormalizeExpression(e); 
      
    pv = (Pvecteur)normalized_linear(expression_normalized(e));
  },
      ActualArrayInd);
    
  xml_Matrix(mat,ActualArrayDim,FormalArrayDim,sb_result);
  string_buffer_append_word("/Connection",sb_result);
}

static void xml_LoopOffset(list  ActualArrayInd,int ActualArrayDim, Pvecteur loop_indices,string_buffer sb_result)
{
  Pmatrix mat; 
  Pvecteur loop_indices2 = vect_reversal(loop_indices);
  int nestloop_dim = vect_size(loop_indices2);
  Pvecteur pv, pv2;
  int i,j;

  string_buffer_append_word("LoopOffset",sb_result); 
  mat = matrix_new(ActualArrayDim,nestloop_dim);
  matrix_init(mat,ActualArrayDim,nestloop_dim);
  i=1;
  MAP(EXPRESSION, e , { 
    pv = (Pvecteur)normalized_linear(expression_normalized(e));
    for (pv2 = loop_indices2, j=1; !VECTEUR_NUL_P(pv2);pv2=pv2->succ,j++) 
      MATRIX_ELEM(mat,i,j)=vect_coeff(pv2->var,pv);
    i++;
  },
      ActualArrayInd);
 
  xml_Matrix(mat,ActualArrayDim,nestloop_dim,sb_result);
  string_buffer_append_word("/LoopOffset",sb_result);
}

// A completer
// Ici, la constante vaut 0
static void xml_ConstOffset(int ActualArrayDim, string_buffer sb_result)
{ 
  Pmatrix mat; 
  string_buffer_append_word("ConstOffset",sb_result); 
  mat = matrix_new(ActualArrayDim,1);
  matrix_init(mat,ActualArrayDim,1);
  xml_Matrix(mat,ActualArrayDim,1,sb_result);
  string_buffer_append_word("/ConstOffset",sb_result);
}

int find_rw_effect_for_entity(list leff, effect *eff, entity e)
{
  // return effet_rwb = 1 for Read, 2 for Write, 3 for Read/Write
  int effet_rwb=0;
  list lr = NIL;
  bool er = FALSE;
  bool ew = FALSE;

  for ( lr = leff; !ENDP(lr) && (effet_rwb==0); lr = CDR(lr)) {
    *eff= EFFECT(CAR(lr));
    reference ref = effect_any_reference(*eff);  
    entity v = reference_variable(ref);
    if (same_entity_p(v,e)) {
      er = er || action_read_p(effect_action(*eff));
      ew = ew || action_write_p(effect_action(*eff));	       
    }
  }
  effet_rwb = (er?1:0) +(ew?2:0);
  return (effet_rwb); 
}



static void  xml_Arguments(statement s, entity function, Pvecteur loop_indices, Psysteme prec, string_buffer sb_result )
{
  call c = instruction_call(statement_instruction(s));
  list call_effect =load_proper_rw_effects_list(s);
  entity FormalArrayName, ActualArrayName = entity_undefined;
  reference ActualRef=reference_undefined;
  syntax sr;
  effect ef = effect_undefined; 
  int iexp,ith=0;
  int rw_ef=0;
  string aan ="";
  int valr  ;

  //   printf("xml_Arguments statement: \n");
  //   print_statement(s);

  string_buffer_append_word("Arguments",sb_result);
  global_margin++;
  
  MAP(EXPRESSION,exp,{  
    ith ++; 
    FormalArrayName = find_ith_formal_parameter(call_function(c),ith);
    sr = expression_syntax(exp);
    
    if (syntax_reference_p(sr)) {
      ActualRef = syntax_reference(sr);
      ActualArrayName = reference_variable(ActualRef);
      aan = strdup(entity_user_name(ActualArrayName));
      rw_ef = find_rw_effect_for_entity(call_effect,&ef, ActualArrayName);
    }
    else {
      // Actual Parameter could be  an expression
      aan = words_to_string(words_syntax(sr));
      rw_ef = 1;

    }
    string_buffer_append_word("Argument",sb_result);
    if (!array_argument_p(exp)) { /* Scalar Argument */
      global_margin++;
      add_margin(global_margin,sb_result);
      string_buffer_append(sb_result,
			   strdup(concatenate(OPENANGLE, 
					      "ScalarArgument ActualName=", 
					      QUOTE,
					      aan,
					      QUOTE,BL,
					      "FormalName=", QUOTE,entity_user_name(FormalArrayName), QUOTE,BL,
					      "AccessMode=",QUOTE,(rw_ef>=2)? "DEF": "USE", QUOTE,CLOSEANGLE,
					      NL, NULL)));
      if (expression_integer_value(exp, &iexp))  
	string_buffer_append_numeric(i2a(iexp),sb_result);
      else if (eval_linear_expression(exp,prec,&valr))
    	string_buffer_append_numeric(i2a(valr),sb_result);
      string_buffer_append_word("/ScalarArgument",sb_result);    
      global_margin--;
    }
    else { /* Array Argument */
      int ActualArrayDim = variable_entity_dimension(ActualArrayName);
      char *  SActualArrayDim = strdup(itoa(ActualArrayDim));
      int FormalArrayDim = variable_entity_dimension(FormalArrayName);
      list ActualArrayInd = reference_indices(ActualRef);
      global_margin++;
      add_margin(global_margin,sb_result);
      string_buffer_append(sb_result,
			   strdup(concatenate(OPENANGLE, 
					      "ArrayArgument ActualName=", 
					      QUOTE,
					      entity_user_name(ActualArrayName),QUOTE,BL,
					      "ActualDim=", QUOTE,SActualArrayDim,QUOTE,BL,
					      "FormalName=", QUOTE,entity_user_name(FormalArrayName), QUOTE,BL,
					      "FormalDim=", QUOTE,itoa(FormalArrayDim),QUOTE,BL,
					      "AccessMode=",QUOTE,(rw_ef>=2)? "DEF":"USE",QUOTE,CLOSEANGLE,
					      NL, NULL)));
      /* Save information to generate Task Graph */
      if (rw_ef>=2)
	hash_put(hash_entity_def_to_task, (char *)ActualArrayName,(char *)function); 
      free(SActualArrayDim);
      
      xml_Connection(ActualArrayInd,ActualArrayDim,FormalArrayDim,sb_result);
      xml_LoopOffset(ActualArrayInd,ActualArrayDim, loop_indices,sb_result);
      xml_ConstOffset(ActualArrayDim,sb_result);
      global_margin--;
      string_buffer_append_word("/ArrayArgument",sb_result);    
    }  
    string_buffer_append_word("/Argument",sb_result);
  },call_arguments(c)) ;
  global_margin--;
  string_buffer_append_word("/Arguments",sb_result);
}


// A completer
static void   xml_Dependances()
{
  // Not implemented Yet
}

static void xml_Call(entity module,  int code_tag,int taskNumber, nest_context_p nest, string_buffer sb_result)
{
  statement s = gen_array_item(nest->nested_call,taskNumber);
  stack st = gen_array_item(nest->nested_loops,taskNumber); 
  //  stack sindices = gen_array_item(nest->nested_loop_indices,taskNumber); 
  call c = instruction_call(statement_instruction(s));
  entity func= call_function(c);
  list pattern_region=NIL;
  Pvecteur paving_indices = VECTEUR_NUL;
  Pvecteur pattern_indices = VECTEUR_NUL;
  boolean motif_in_te_p=FALSE;
  transformer t = load_statement_precondition(s);
  Psysteme prec = sc_dup((Psysteme) predicate_system(transformer_relation(t)));
  
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "Call Name=", 
					  QUOTE,
					  ENTITY_ASSIGN_P(func) ? 
					  "LocalAssignment" : entity_user_name(func), 
					  QUOTE,
 					  CLOSEANGLE,NL, NULL)));
 global_margin++;
 
 pattern_region = regions_dup(load_statement_local_regions(s));

  xml_Loops(st,TRUE,&pattern_region,&paving_indices, &pattern_indices,motif_in_te_p, sb_result);

  if (ENTITY_ASSIGN_P(func)) { 
    xml_TaskParameters(TRUE,code_tag, module,pattern_region,paving_indices,sb_result);
  }
  else
    xml_Arguments(s,func,paving_indices, prec,sb_result);
  xml_Dependances();
  global_margin--;
  string_buffer_append_word("/Call",sb_result);
  regions_free(pattern_region);
  sc_free(prec);
}

boolean array_in_effect_list(list effects_list)
{
  list pc;
 for (pc= effects_list;pc != NIL; pc = CDR(pc)){
      effect e = EFFECT(CAR(pc));
      reference r = effect_any_reference(e);
      entity v =  reference_variable(r);
      if (array_entity_p(v)){
	return(TRUE);
      }
 }
 return(FALSE);
}

static void xml_BoxGraph(entity module, nest_context_p nest, string_buffer sb_result,string_buffer sb_ac)
{
  string_buffer appli_needs = string_buffer_make();
  string string_appli_needs = "";
  list pc;
  int nb_call,nc,callnumber;
 
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "BoxGraph Name=", 
					  QUOTE,entity_user_name(module), 
					  QUOTE,
					  CLOSEANGLE,NL, NULL)));

  nb_call = (int)gen_array_nitems(nest->nested_call);
  nc = 1;
  global_margin++;
  for (callnumber = 0; callnumber<nb_call; callnumber++) {
    statement s = gen_array_item(nest->nested_call,callnumber); 
    call c = instruction_call(statement_instruction(s));
    // list effects_list =load_proper_rw_effects_list(s);
    list effects_list = regions_dup(load_statement_local_regions(s));
    entity func= call_function(c);
    string_buffer buffer_needs = string_buffer_make();  
    string string_needs = "";

    boolean assign_func = ENTITY_ASSIGN_P(func);
    string n= assign_func ? "LocalAssignment" : entity_user_name(func);

    if (!assign_func || array_in_effect_list(effects_list)) {

      add_margin(global_margin,sb_result);
      string_buffer_append(sb_result,
			   strdup(concatenate(OPENANGLE, 
					      "TaskRef Name=", 
					      QUOTE,n,QUOTE, CLOSEANGLE,NL,
					      NULL)));
      for (pc= effects_list;pc != NIL; pc = CDR(pc)){
	effect e = EFFECT(CAR(pc));
	reference r = effect_any_reference(e);
	action ac = effect_action(e);
	entity v =  reference_variable(r);
	if (array_entity_p(v)){
	  if (action_read_p(ac)) {
	    entity t =  hash_get(hash_entity_def_to_task,(char *) v);
	    global_margin++;
	    add_margin(global_margin,buffer_needs);
	    string_buffer_append(buffer_needs,
				 strdup(concatenate(OPENANGLE, 
						    "Needs ArrayName=", 
						    QUOTE,entity_user_name(v),QUOTE, BL,
						    "DefinedBy=",
						    QUOTE,
						    (t!= HASH_UNDEFINED_VALUE) ? entity_local_name(t): "IN_VALUE",
						    QUOTE,"/",
						    CLOSEANGLE,NL,
						    NULL))); 
	    // Temporaire en attendant les effects IN de l'appli
	    if (nc==1) {
	      add_margin(global_margin,appli_needs);
	      string_buffer_append(appli_needs,
				   strdup(concatenate(OPENANGLE, 
						      "Needs ArrayName=", 
						      QUOTE,entity_user_name(v),QUOTE, BL,
						      "DefinedBy=",
						      QUOTE,(t!= HASH_UNDEFINED_VALUE) ? entity_local_name(t): "IN_VALUE",
						      QUOTE,"/",
						      CLOSEANGLE,NL,
						      NULL))); 
	    }
	    global_margin--;
	  }
	  else {   
	    global_margin++;
	    add_margin(global_margin,sb_result);
	    string_buffer_append(sb_result,
				 strdup(concatenate(OPENANGLE,"Computes ArrayName=",
						    QUOTE,entity_user_name(v),QUOTE,"/",
						    CLOSEANGLE,NL,
						    NULL)));
	    // Temporaire en attendant les effects OUT de l'appli
	    if (nc==nb_call) {
	      add_margin(global_margin,sb_ac);
	      string_buffer_append(sb_ac,
				   strdup(concatenate(OPENANGLE, 
						      "Computes ArrayName=",
						      QUOTE,entity_user_name(v),QUOTE,"/",
						      CLOSEANGLE,NL,
						      NULL)));
	      string_appli_needs =string_buffer_to_string(appli_needs);
	      string_buffer_append(sb_ac,string_appli_needs);
	  	
	    }
	    global_margin--;
	  } 
	}
      } 
    }
      string_needs =string_buffer_to_string(buffer_needs);
      string_buffer_append(sb_result,string_needs);
      string_buffer_append_word("/TaskRef",sb_result);	 
      nc++;  
      regions_free(effects_list);
   
  }
    global_margin--;
  string_buffer_append_word("/BoxGraph",sb_result); 
}



static void xml_Boxes(string module_name, int code_tag,string_buffer sb_result,string_buffer sb_ac)
{
  entity module = module_name_to_entity(module_name);
  
  nest_context_t nest;
  int callnumber =0;
  statement stat = get_current_module_statement();
  string string_sb_result;  
  Psysteme prec = first_precondition_of_module(module_name);
  // printf("first_precondition_of_module %s\n",module_name);
  //sc_fprint(stdout,prec,(char * (*)(Variable)) entity_local_name);
 
  nest.loops_for_call = stack_make(statement_domain,0,0);
  nest.loop_indices = stack_make(entity_domain,0,0);
  nest.current_stat = stack_make(statement_domain,0,0);
  nest.nested_loops=  gen_array_make(0);
  nest.nested_loop_indices =  gen_array_make(0);
  nest.nested_call=  gen_array_make(0);

  global_margin++;
  add_margin(global_margin,sb_result);
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "Box Name=", 
					  QUOTE,module_name, 
					  QUOTE,
					  CLOSEANGLE,NL, NULL)));
  global_margin++; 

  if (!main_c_program_p(module_name)) {
    xml_LocalArrays(module,prec,sb_result);  
    xml_FormalArrays(module,prec,sb_result);
  }
  else {
    string_buffer_append_word("LocalArrays/",sb_result); 
    string_buffer_append_word("FormalArrays/",sb_result); 
  }
 

  /* Search calls in Box */
  find_loops_and_calls_in_box(stat,&nest);
 
  for (callnumber = 0; callnumber<(int)gen_array_nitems(nest.nested_call); callnumber++)
    xml_Call(module, code_tag, callnumber, &nest,sb_result);
    
  xml_BoxGraph(module,&nest,sb_result,sb_ac);
  global_margin--;
  string_buffer_append_word("/Box",sb_result); 
   string_sb_result=string_buffer_to_string(sb_result); 

  insert_xml_string(module_name,string_sb_result);

  string_buffer_append_word("Tasks",sb_result);
  insert_xml_callees(module_name);
  string_buffer_append_word("/Tasks",sb_result);
   global_margin--;
  gen_array_free(nest.nested_loops);
  gen_array_free(nest.nested_loop_indices);
  gen_array_free(nest.nested_call);
  stack_free(&(nest.loops_for_call));
  stack_free(&(nest.loop_indices));
  stack_free(&(nest.current_stat));
  sc_free(prec);

}

// A completer avec les effects IN et OUT de l'application
static void __attribute__ ((unused)) xml_ApplicationGraph(string module_name, string_buffer sb_ac )
{
  string_buffer sb_pref_ac = string_buffer_make();
  string string_sb_ac="";
  add_margin(global_margin,sb_pref_ac);
  string_buffer_append(sb_pref_ac,
		       strdup(concatenate(OPENANGLE, 
					  "ApplicationGraph Name=", 
					  QUOTE,
					  module_name, 
					  QUOTE,
					  CLOSEANGLE,NL, NULL)));
  string_sb_ac =string_buffer_to_string(sb_ac);
  string_buffer_append(sb_pref_ac,string_sb_ac);
  string_buffer_append_word("/ApplicationGraph",sb_pref_ac); 
  sb_ac=sb_pref_ac;
  
  
}


static void xml_Application(string module_name, int code_tag,string_buffer sb_result)
{
  entity module = module_name_to_entity(module_name);
  string_buffer sb_ac = string_buffer_make();
  string sr;
  global_margin = 0; 
  Psysteme prec;
  prec = first_precondition_of_module(module_name);
  //printf("first_precondition_of_module %s\n",module_name);
  //sc_fprint(stdout,prec,(char * (*)(Variable)) entity_local_name);
 
   string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "!DOCTYPE Application SYSTEM ",
					  QUOTE,
					  "APPLI_TERAOPS_v4.dtd",
					  QUOTE,
					  CLOSEANGLE, 
					  NL, NULL)));
  string_buffer_append(sb_result,
		       strdup(concatenate(OPENANGLE, 
					  "Application Name=", 
					  QUOTE,
					  get_current_module_name(), 
					  QUOTE, BL,
					  "Language=",QUOTE,
					  (prettyprint_is_fortran) ? "FORTRAN":"C",
					  QUOTE, BL,
					  "PassingMode=",
					  QUOTE,
					  (prettyprint_is_fortran) ? "BYREFERENCE":"BYVALUE",
					  QUOTE,
					  CLOSEANGLE, 
					  NL, NULL)));
 xml_ActualArrays(module,prec,sb_result);
 global_margin ++;
  add_margin(global_margin,sb_ac);
  string_buffer_append(sb_ac,
		       strdup(concatenate(OPENANGLE, 
					  "ApplicationGraph Name=", 
					  QUOTE,
					  module_name, 
					  QUOTE,
					  CLOSEANGLE,NL, NULL)));
  global_margin ++;
  add_margin(global_margin,sb_ac);
  string_buffer_append(sb_ac,
		       strdup(concatenate(OPENANGLE, 
					  "TaskRef Name=", 
					  QUOTE,entity_user_name(module),
					  QUOTE,CLOSEANGLE,NL,
					  NULL)));  
  global_margin -=2;
  xml_Boxes(module_name,code_tag,sb_result,sb_ac);  
  global_margin +=2;
  string_buffer_append_word("/TaskRef",sb_ac);  
 global_margin --;
 string_buffer_append_word("/ApplicationGraph",sb_ac); 
 global_margin --;


 
   add_margin(global_margin,sb_ac);
  string_buffer_append(sb_ac,
		       strdup(concatenate(OPENANGLE, 
					  "/Application", 
					  CLOSEANGLE, 
					  NL, NULL)));
  sr=string_buffer_to_string(sb_ac); 

  insert_xml_string(module_name,sr);  
  sc_free(prec);
}

/******************************************************** PIPSMAKE INTERFACE */

#define XMLPRETTY    ".xml"


int find_code_status(string module_name)
{

  statement stat=(statement) db_get_memory_resource(DBR_CODE, 
						    module_name, TRUE); 
  boolean wmotif = FALSE;
  boolean wbox = FALSE;

  motif_in_statement_p=FALSE;
  gen_recurse(stat, statement_domain, gen_true,motif_in_statement);  
  wmotif = motif_in_statement_p;
  gen_recurse(stat, statement_domain, gen_true,box_in_statement);  
  wbox = box_in_statement_p;
  
  if (main_c_program_p(module_name))
    return(code_is_a_main); 
  else {
    if (wmotif && !wbox)
      return (code_is_a_te);
    else return(code_is_a_box);
  }  
}



bool print_xml_application(string module_name)
{
  FILE * out;
  entity module;
  string xml, dir, filename;
  statement stat;
  string_buffer sb_result=string_buffer_make();
 string_buffer sb_ac = string_buffer_make();  
 hash_entity_def_to_task = hash_table_make(hash_pointer,0);
 int code_tag;
  module = module_name_to_entity(module_name);
  xml = db_build_file_resource_name(DBR_XML_PRINTED_FILE, 
				    module_name, XMLPRETTY);
  dir = db_get_current_workspace_directory();
  filename = strdup(concatenate(dir, "/", xml, NULL));
  stat=(statement) db_get_memory_resource(DBR_CODE, 
						    module_name, TRUE);

  set_current_module_entity(module);
  set_current_module_statement(stat);
  if(statement_undefined_p(stat))
    {
      pips_internal_error("No statement for module %s\n", module_name);
    }
  set_proper_rw_effects((statement_effects)
			db_get_memory_resource(DBR_PROPER_EFFECTS,
					       module_name,TRUE));

  init_cost_table();
  /* Get the READ and WRITE regions of the module */
  set_rw_effects((statement_effects) 
		 db_get_memory_resource(DBR_REGIONS, module_name, TRUE)); 

  // set_complexity_map( (statement_mapping)
  //		      db_get_memory_resource(DBR_COMPLEXITIES, module_name, TRUE));

  set_precondition_map((statement_mapping)
		       db_get_memory_resource(DBR_PRECONDITIONS,
					      module_name,
					      TRUE));
  debug_on("XMLPRETTYPRINTER_DEBUG_LEVEL");
  out = safe_fopen(filename, "w");
  fprintf(out,"<!-- XML prettyprint for module %s. --> \n",module_name);
  safe_fclose(out, filename);

   code_tag = find_code_status(module_name);
  switch (code_tag) 
    {
    case code_is_a_box: {
      xml_Boxes(module_name,code_tag,sb_result,sb_ac);
   break;
    }
    case code_is_a_te:{ 
      xml_Task(module_name,code_tag,sb_result);
      break;
    }
    case code_is_a_main:{

      xml_Application(module_name, code_tag,sb_result);  


      break;
    }
   default:
      pips_internal_error("unexpected kind of code for xml_prettyprinter\n");
    }

  pips_debug(1, "End Xml prettyprinter for %s\n", module_name);
  debug_off();  
   string_buffer_free(&sb_result,TRUE);
  hash_table_free(hash_entity_def_to_task);
  free(dir);
  free(filename);

  DB_PUT_FILE_RESOURCE(DBR_XML_PRINTED_FILE, module_name, xml);

  reset_current_module_statement();
  reset_current_module_entity();
  //  reset_complexity_map();
    reset_precondition_map();
    reset_rw_effects();
  reset_proper_rw_effects();
  return TRUE;
}

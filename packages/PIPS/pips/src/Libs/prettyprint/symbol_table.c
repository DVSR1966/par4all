/*

  $Id$

  Copyright 1989-2010 MINES ParisTech

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "text.h"
#include "text-util.h"

#include "ri-util.h"
#include "effects-util.h"
#include "c_parser_private.h"
#include "parser_private.h" /* FI: for syntax.h */

#include "syntax.h"
#include "misc.h"

#include "resources.h"
#include "database.h"

#include "misc.h"
#include "pipsdbm.h"

#define NL       "\n"

/* This function is called from c_parse() via ResetCurrentModule() and fprint_environment() */

void dump_common_layout(string_buffer result, entity c, bool debug_p, bool isfortran)
{
  entity mod = get_current_module_entity();
  list members = get_common_members(c, mod, FALSE);
  list equiv_members = NIL;

  string_buffer_append(result,
		       concatenate(NL,"Layout for ",isfortran?"common /":"memory area \"",
				   entity_name(c),isfortran?"/":"\""," of size ",
				   itoa(area_size(type_area(entity_type(c)))),
				   ":",NL,NULL));


  if(ENDP(members)) {
    pips_assert("An empty area has size 0", area_size(type_area(entity_type(c))) ==0);
    string_buffer_append(result, concatenate("\t* empty area *",NL,NL,NULL));
  }
  else {
    // Different preconditions for C and fortran

    if(isfortran){
      if(area_size(type_area(entity_type(c)))==0
	 && fortran_relevant_area_entity_p(c))
	{
	  if(debug_p) {
	    user_warning("dump_common_layout",
			 "Non-empty area %s should have a final size greater than 0\n",
			 entity_module_name(c));
	  }
	  else {
	    pips_error("dump_common_layout",
		       "Non-empty area %s should have a size greater than 0\n",
		       entity_module_name(c));
	  }
	}
    }
    else {
      if(area_size(type_area(entity_type(c))) == 0)
	{
	  if(debug_p) {
	    user_warning("dump_common_layout","Non-empty area %s should have a final size greater than 0\n",
			 entity_module_name(c));
	  }
	}
    }
    if(isfortran){
      FOREACH(ENTITY, m, members) {
	pips_assert("RAM storage",
		    storage_ram_p(entity_storage(m)));
	if(ram_function(storage_ram(entity_storage(m)))==mod) {
	  int s;

	  if(ram_section(storage_ram(entity_storage(m)))!=c) {
	    pips_error("print_common_layout",
		       "Variable %s declared in area %s but allocated in area %s\n",
		       entity_local_name(m), entity_module_name(c),
		       entity_module_name(ram_section(storage_ram(entity_storage(m)))));
	  }

	  if(!SizeOfArray(m, &s)) {
	    if(ram_section(storage_ram(entity_storage(m)))==HeapArea
	       || ram_section(storage_ram(entity_storage(m)))==StackArea) {
	      s = -1;
	    }
	    else {
	      user_warning("print_common_layout",
			   "Varying size of array \"%s\"\n", entity_name(m));
	      ParserError("print_common_layout",
			  "Fortran standard prohibit varying size array\n");
	    }
	  }

	  string_buffer_append(result,
			       concatenate
			       ("\tVariable ",entity_name(m),"\toffset = ",
				itoa(ram_offset(storage_ram(entity_storage(m)))),NULL));
	  string_buffer_append(result,
			       concatenate("\tsize = ",itoa(s),NL,NULL));
	}
      }
    }
    else { // C language
      FOREACH(ENTITY, m, members) {
	pips_assert("RAM storage",
		    storage_ram_p(entity_storage(m)));
	int s;

	SizeOfArray(m, &s);

	pips_assert("An area has no offset as -1",
		    (ram_offset(storage_ram(entity_storage(m)))!= -1));

	if(ram_offset(storage_ram(entity_storage(m))) == DYNAMIC_RAM_OFFSET) {
	  string_buffer_append(result,
			       concatenate(
					   "\tDynamic Variable \"",entity_name(m),
					   "\"\toffset = UNKNOWN, \tsize = DYNAMIC",
					   NL,NULL));
	}
	else if (ram_offset(storage_ram(entity_storage(m))) == UNDEFINED_RAM_OFFSET) {
	  string_buffer_append(result,
			       concatenate
			       ("\tExternal Variable \"", entity_name(m),
				"\"\toffset = UNKNOWN,\tsize = ",
				s>0?itoa(s): "unknown", NL, NULL));
	}
	else {
	  string_buffer_append(result,
			       concatenate
				      ("\tVariable \"", entity_name(m),"\"\toffset = ",
				       itoa(ram_offset(storage_ram(entity_storage(m)))),NULL));
	  string_buffer_append(result,
			       concatenate("\tsize = ",itoa(s),NL,NULL));
	}
      }
    }

    string_buffer_append(result, NL);

    FOREACH(ENTITY, m, members) {
      list equiv = ram_shared(storage_ram(entity_storage(m)));

      equiv_members = arguments_union(equiv_members, equiv);
    }

    if(!ENDP(equiv_members)){
      equiv_members = arguments_difference(equiv_members, members);
      if(!ENDP(equiv_members)) {
	sort_list_of_entities(equiv_members);

	string_buffer_append(result, concatenate("\tVariables aliased to this common:",NL,
							NULL));

	FOREACH(ENTITY, m, equiv_members) {
	  pips_assert("RAM storage",
		      storage_ram_p(entity_storage(m)));
	  string_buffer_append(result,
			       concatenate
			       ("\tVariable", entity_name(m) ,"\toffset =",
				ram_offset(storage_ram(entity_storage(m))),"\tsize = ",
				SafeSizeOfArray(m),NL,NULL));
	}
	string_buffer_append(result, concatenate(NL,NULL));
	gen_free_list(equiv_members);
      }
    }
  }
  gen_free_list(members);
}

void dump_functional(functional f, string_buffer result)
{
  type tr = functional_result(f);
  list cp = list_undefined;

  for(cp=functional_parameters(f); !ENDP(cp); POP(cp)) {
    parameter p = PARAMETER(CAR(cp));
    type ta = parameter_type(p);

    pips_assert("Argument type is variable or varags:variable or functional or void",
		type_variable_p(ta)
		|| (type_varargs_p(ta) && type_variable_p(type_varargs(ta)))
		|| type_functional_p(ta)
		|| type_void_p(ta));

    if(type_variable_p(ta)) {
      variable v = type_variable(ta);
      basic b = variable_basic(v);
      if(basic_pointer_p(b) && type_functional_p(basic_pointer(b))) {
	functional f = type_functional(basic_pointer(b));
	string_buffer_append(result, "(");
	dump_functional(f,result);
	string_buffer_append(result, ") *");
      }
      else {
	string_buffer_append(result, basic_to_string(b));
      }
    }
    else if(type_functional_p(ta)) {
      functional fa = type_functional(ta);

      string_buffer_append(result, "(");
      dump_functional(fa, result);
      string_buffer_append(result, ")");
    }
    else if(type_varargs_p(ta)) {
      string_buffer_append(result, concatenate(type_to_string(ta),":",NULL));
      ta = type_varargs(ta);
      string_buffer_append(result,
			   basic_to_string(variable_basic(type_variable(ta))));
    }
    else if(type_void_p(ta)) {
      /* FI: we could do nothing or put "void". I choose to put "void"
	 to give more information about the internal
	 representation. */
      string_buffer_append(result, type_to_string(ta));
    }
    if(!ENDP(cp->cdr))
      string_buffer_append(result, concatenate(" x ",NULL));
  }
  if(ENDP(functional_parameters(f))) {
    string_buffer_append(result, concatenate("()",NULL));
  }

  string_buffer_append(result, concatenate(" -> ",NULL));

  if(type_variable_p(tr))
    string_buffer_append(result,
			 concatenate(basic_to_string(variable_basic(type_variable(tr)))
					    /*,NL*/,NULL));
  else if(type_void_p(tr))
    string_buffer_append(result, concatenate(type_to_string(tr)/*,NL*/,NULL));
  else if(type_unknown_p(tr)){
    string_buffer_append(result, concatenate(type_to_string(tr)/*,NL*/,NULL));
  }
  else if(type_varargs_p(tr)) {
    string_buffer_append(result, concatenate(type_to_string(tr),":",
					     basic_to_string(variable_basic(type_variable(type_varargs(tr)))),NULL));
  }
  else
    /* An argument can be functional, but not (yet) a result. */
    pips_internal_error("Ill. type %d\n", type_tag(tr));
}

string get_symbol_table(entity m, bool isfortran)
{
  string_buffer result = string_buffer_make(true);
  string result2;
  list decls = gen_copy_seq(code_declarations(value_code(entity_initial(m))));
  int nth = 0;
  entity rv = entity_undefined; /* return variable */
  list ce = list_undefined;

  pips_assert("get_symbol_table", entity_module_p(m));

  /* To simplify validation, at the expense of some information about
     the parsing process. */

  gen_sort_list(decls, (gen_cmp_func_t)compare_entities);

  string_buffer_append(result, concatenate(NL,"Declarations for module \"",
					   module_local_name(m),"\" with type \"", NULL));

  dump_functional(type_functional(entity_type(m)), result);
  string_buffer_append(result, "\"" NL);

  /*
   * List of implicitly and explicitly declared variables, functions
   * and areas
   *
   */
  if(ENDP(decls))
    string_buffer_append(result, concatenate("\n* empty declaration list *",NL,NL,NULL));
  else
    string_buffer_append(result, concatenate("\nVariable list:",NL,NL,NULL));

  FOREACH(ENTITY, e, decls) {
    if(!typedef_entity_p(e) && !derived_entity_p(e)) {
      type t = entity_type(e);

      pips_debug(8, "Processing entity \"%s\"\n", entity_name(e));
      string_buffer_append(result, concatenate("\tDeclared entity \"",
					       entity_name(e),"\" with type \"",
					       type_to_string(t),"\" ",NULL));

      /* FI: struct, union and enum are also declared (in theory...), but
	 their characteristics should be given differently. */
      if(type_variable_p(t)
	 /* && (storage_ram_p(s) || storage_return_p(s) || storage_formal_p(s))*/) {
	variable v = type_variable(t);
	basic b = variable_basic(v);
	if(basic_pointer_p(b) && type_functional_p(basic_pointer(b))) {
	  functional f = type_functional(basic_pointer(b));
	  string_buffer_append(result, "\"(");
	  dump_functional(f,result);
	  string_buffer_append(result, ") *\""NL);
	}
	else {
	  string_buffer_append(result,
			       concatenate("\"", basic_to_string(b), "\""NL,NULL));
	}
      }
      else if(type_functional_p(t)) {
	string_buffer_append(result, "\"");
	dump_functional(type_functional(t),result);
	string_buffer_append(result, "\""NL);
      }
      else if(type_area_p(t)) {
	string_buffer_append(result,concatenate("with size ",
						itoa(area_size(type_area(t))),NL, NULL));
      }
      else {
	/* FI: How do we want to print out structures, unions and enums? */
	/* FI: How do we want to print out typedefs? */
	string_buffer_append(result, NL);
      }
    }
  }

  /*
   * Extern declarations
   */

  if(!isfortran) {
    list edecls = gen_copy_seq(code_externs(value_code(entity_initial(m))));

    //gen_sort_list(decls, compare_entities);

    if(ENDP(edecls))
      string_buffer_append(result, concatenate("\n* empty extern declaration list *",NL,NL,NULL));
    else
      string_buffer_append(result, concatenate("\nExternal variable list:",NL,NL,NULL));

    for(ce=edecls; !ENDP(ce); POP(ce)) {
      entity e = ENTITY(CAR(ce));
      type t = entity_type(e);

      pips_debug(8, "Processing external entity \"%s\"\n", entity_name(e));
      pips_assert("e is an entity", entity_domain_number(e)==entity_domain);
      string_buffer_append(result, concatenate("\tDeclared external entity \"",
					       entity_name(e),"\"\twith type \"",
					       type_to_string(t),"\" ",NULL));

      if(type_variable_p(t)) {
	variable v = type_variable(t);
	basic b = variable_basic(v);
	if(basic_pointer_p(b) && type_functional_p(basic_pointer(b))) {
	  functional f = type_functional(basic_pointer(b));
	  string_buffer_append(result, "\"(");
	  dump_functional(f,result);
	  string_buffer_append(result, ") *\"" NL);
	}
	else {
	  string_buffer_append(result,
			       concatenate("\"", basic_to_string(b), "\""NL,NULL));
	}
      }
      else if(type_functional_p(t)) {
	string_buffer_append(result, "\"");
	dump_functional(type_functional(t),result);
	string_buffer_append(result, "\"" NL);
      }
      else if(type_area_p(t)) {
	string_buffer_append(result,concatenate("with size ",
						itoa(area_size(type_area(t))),NL, NULL));
      }
      else
	string_buffer_append(result, NL);
    }
  }

  /*
   * Derived entities: struct, union and enum
   */

  nth = 0;
  FOREACH(ENTITY, de, decls) {
    if(entity_struct_p(de) || entity_union_p(de) || entity_enum_p(de)) {
	nth++;
	if(nth==1) {
	  string_buffer_append(result, concatenate(NL,"Derived entities:"
						   ,NL,NL,NULL));
	}
	/* FI: it would be useful to dump more information aout
	   fields and members */
	string_buffer_append(result, concatenate("\tVariable \"", entity_name(de),
						 "\"\tkind = ",
						 entity_struct_p(de)? "struct" :
						 entity_union_p(de)? "union" : "enum",
						 "\n", NULL));
      }
  }

  /*
   * Typedefs
   */

  nth = 0;
  FOREACH(ENTITY, te, decls) {
    if(typedef_entity_p(te)) {
      type t = entity_type(te);
      nth++;
      if(nth==1) {
	string_buffer_append(result, concatenate(NL,"Typedef entities:"
						 ,NL,NL,NULL));
      }
      string_buffer_append(result, concatenate("\tTypedef \"", entity_name(te),
					       "\"\twith type \"",type_to_string(t),"\" ", NULL));

      if(type_variable_p(t)) {
	variable v = type_variable(t);
	basic b = variable_basic(v);
	if(basic_pointer_p(b) && type_functional_p(basic_pointer(b))) {
	  functional f = type_functional(basic_pointer(b));
	  string_buffer_append(result, "\"(");
	  dump_functional(f,result);
	  string_buffer_append(result, ") *\"" NL);
	}
	else {
	  string_buffer_append(result,
			       concatenate("\"", basic_to_string(b), "\""NL,NULL));
	}
      }
      else if(type_functional_p(t)) {
	string_buffer_append(result, "\"");
	dump_functional(type_functional(t),result);
	string_buffer_append(result, "\"" NL);
      }
      else if(type_void_p(t)) {
	string_buffer_append(result, "\"void\"" NL);
      }
      else {
	pips_internal_error("unexpected type for a typedef\n");
      }
    }
  }

  /* Formal parameters */
  nth = 0;
  FOREACH(ENTITY, v, decls) {
      storage vs = entity_storage(v);

      pips_assert("All storages are defined", !storage_undefined_p(vs));

      if(storage_formal_p(vs)) {
	nth++;
	if(nth==1) {
	  string_buffer_append(result, concatenate(NL,"Layout for formal parameters:"
						   ,NL,NL,NULL));
	}
	string_buffer_append(result, concatenate("\tVariable ", entity_name(v),
						 "\toffset = ",
						 itoa(formal_offset(storage_formal(vs))),
						 "\n", NULL));
      }
      else if(storage_return_p(vs)) {
	pips_assert("No more than one return variable", entity_undefined_p(rv));
	rv = v;
      }
  }

  /* Return variable */
  if(!entity_undefined_p(rv)) {
    basic rb = variable_basic(type_variable(ultimate_type(entity_type(rv))));
    string_buffer_append(result, concatenate(NL,"Layout for return variable:",NL,NL,NULL));
    string_buffer_append(result, concatenate("\tVariable \"",
					     entity_name(rv),
					     "\"\tsize = ",
					     strdup(itoa(SizeOfElements(rb))),
					     "\n", NULL));
  }

  /* Structure of each area/common */
  if(!ENDP(decls)) {
    string_buffer_append(result, concatenate(NL,"Layouts for ",isfortran?"commons:":"memory areas:",NL,NULL));
  }

  MAP(ENTITY, e, {
      if(type_area_p(entity_type(e))) {
	dump_common_layout(result, e, FALSE, isfortran);
      }
    },
    decls);

  string_buffer_append(result, concatenate("End of declarations for module ",
					   module_local_name(m), NL,NL, NULL));

  gen_free_list(decls);

  result2 = string_buffer_to_string(result);
  string_buffer_free(&result);

  return result2;
}

void actual_symbol_table_dump(string module_name, bool isfortran)
{
  FILE *out;
  string ppt;

  entity module = module_name_to_entity(module_name);

  string symboltable = db_build_file_resource_name(DBR_SYMBOL_TABLE_FILE,
						   module_name, NULL);
  string dir = db_get_current_workspace_directory();
  string filename = strdup(concatenate(dir, "/", symboltable, NULL));
  bool reset_p = FALSE;

  /* This function is called in two different context: as a
     standalone phase or as part of debugging the parser?!? */
  if(entity_undefined_p(get_current_module_entity())) {
    reset_p = TRUE;
    set_current_module_entity(module);
  }

  out = safe_fopen(filename, "w");

  ppt = get_symbol_table(module, isfortran);

  fprintf(out, "%s\n%s", module_name,ppt);
  safe_fclose(out, filename);

  free(dir);
  free(filename);

  if(reset_p)
    reset_current_module_entity();

  DB_PUT_FILE_RESOURCE(DBR_SYMBOL_TABLE_FILE, module_name, symboltable);

}

bool c_symbol_table(string module_name)
{
  set_prettyprint_language_tag(is_language_c);
  //all the way down to words_basic()
  actual_symbol_table_dump(module_name, FALSE);
  return TRUE;
}

bool fortran_symbol_table(string module_name)
{
  actual_symbol_table_dump(module_name, TRUE);
  return TRUE;
}

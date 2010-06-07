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

#include <stdint.h>
#include <stdlib.h>

#include "genC.h"
#include "misc.h"
#include "freia.h"

#include "linear.h"
#include "pipsdbm.h"

#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"
#include "properties.h"

#include "freia_spoc_private.h"
#include "hwac.h"

// dirty debug helper...
string dagvtx_to_string(const dagvtx v)
{
  return itoa(dagvtx_number(v));
}

/* return statement if any, or NULL (for input nodes).
 */
statement dagvtx_statement(const dagvtx v)
{
  pstatement ps = vtxcontent_source(dagvtx_content(v));
  return pstatement_statement_p(ps)? pstatement_statement(ps): NULL;
}

/* a vertex with a non AIPO or image related statement.
 */
bool dagvtx_other_stuff_p(const dagvtx v)
{
  return vtxcontent_optype(dagvtx_content(v))==spoc_type_oth;
}

/* return the produced image or NULL */
entity dagvtx_image(const dagvtx v)
{
  vtxcontent c = dagvtx_content(v);
  return (vtxcontent_out(c) != entity_undefined)? vtxcontent_out(c): NULL;
}

static void entity_list_dump(FILE * out, const string what, const list l)
{
  fprintf(out, "%s: ", what);
  FOREACH(entity, e, l)
    fprintf(out, " %s,", safe_entity_name(e));
  fprintf(out, "\n");
}

/* returns the vertex number, i.e. the underlying statement number.
 */
_int dagvtx_number(const dagvtx v)
{
  if (v==NULL) return 0;
  pstatement source = vtxcontent_source(dagvtx_content(v));
  if (pstatement_statement_p(source))
  {
    statement s = pstatement_statement(source);
    return s? statement_number(s): 0;
  }
  else
    return 0;
}

_int dagvtx_optype(const dagvtx v)
{
  return vtxcontent_optype(dagvtx_content(v));
}

_int dagvtx_opid(const dagvtx v)
{
  return vtxcontent_opid(dagvtx_content(v));
}

string dagvtx_operation(const dagvtx v)
{
  if (v==NULL) return "null";
  int index = vtxcontent_opid(dagvtx_content(v));
  const freia_api_t * api = get_freia_api(index);
  return strncmp(api->function_name, AIPO, strlen(AIPO))==0?
    api->function_name + strlen(AIPO): api->function_name;
}

string dagvtx_compact_operation(const dagvtx v)
{
  if (v==NULL) return "null";
  int index = vtxcontent_opid(dagvtx_content(v));
  const freia_api_t * api = get_freia_api(index);
  return api->compact_name;
}

int dagvtx_ordering(const dagvtx * v1, const dagvtx * v2)
{
  return dagvtx_number(*v1) - dagvtx_number(*v2);
}

/* return (last) producer vertex or NULL if none found.
 * this is one of the two predecessors of sink.
 */
dagvtx
dagvtx_get_producer(const dag d, const dagvtx sink, const entity e)
{
  pips_assert("some image", e!=entity_undefined);
  FOREACH(dagvtx, v, dag_vertices(d))
  {
    vtxcontent c = dagvtx_content(v);
    // the image may be kept within a pipe
    if (vtxcontent_out(c)==e &&
	(sink==NULL || gen_in_list_p(sink, dagvtx_succs(v))))
      return v;
  }
  return NULL; // it is an external parameter?
}

void dagvtx_nb_dump(FILE * out, const string what, const list l)
{
  fprintf(out, "%s: ", what);
  FOREACH(dagvtx, v, l)
    fprintf(out, " %" _intFMT ",", dagvtx_number(v));
  fprintf(out, "\n");
}

/* for dag debug.
 */
void dagvtx_dump(FILE * out, const string name, const dagvtx v)
{
  if (!v) {
    fprintf(out, "vertex %s is NULL\n", name? name: "");
    return;
  }
  fprintf(out, "vertex %s %" _intFMT " %s (%p)\n",
	  name? name: "", dagvtx_number(v), dagvtx_operation(v), v);
  dagvtx_nb_dump(out, "  succs", dagvtx_succs(v));
  vtxcontent c = dagvtx_content(v);
  statement s = dagvtx_statement(v);
  fprintf(out,
	  "  optype: %s\n"
	  "  opid: %" _intFMT "\n"
	  "  source: %" _intFMT "/%" _intFMT "\n",
	  what_operation(vtxcontent_optype(c)),
	  vtxcontent_opid(c),
	  s? statement_number(s): 0,
	  s? statement_ordering(s): 0);
  entity_list_dump(out, "  inputs", vtxcontent_inputs(c));
  fprintf(out, "  output: %s\n", safe_entity_name(vtxcontent_out(c)));
  // to be continued...
}

/* for dag debug
 */
void dag_dump(FILE * out, const string what, const dag d)
{
  fprintf(out, "dag '%s' (%p):\n", what, d);

  dagvtx_nb_dump(out, "inputs", dag_inputs(d));
  dagvtx_nb_dump(out, "outputs", dag_outputs(d));

  FOREACH(dagvtx, vx, dag_vertices(d)) {
    dagvtx_dump(out, NULL, vx);
    fprintf(out, "\n");
  }

  fprintf(out, "\n");

  // ifdebug(1) dag_consistent_p(d);
}

// #define IMG_DEP " [arrowhead=normal]"
#define IMG_DEP ""
#define SCL_DEP "arrowhead=empty"

static const char* entity_dot_name(entity e)
{
  return entity_user_name(e);
}

static void dagvtx_dot_node(FILE * out, const string prefix, const dagvtx v)
{
  fprintf(out, "%s\"%"_intFMT" %s\"", prefix? prefix: "",
	  dagvtx_number(v), dagvtx_compact_operation(v));
}

static void dagvtx_dot(FILE * out, const dag d, const dagvtx vtx)
{
  bool label_nodes = get_bool_property("FREIA_LABEL_NODES");
  bool show_arcs = get_bool_property("FREIA_LABEL_ARCS");

  vtxcontent co = dagvtx_content(vtx);
  const char* vname = NULL;
  if (vtxcontent_out(co)!=entity_undefined)
    vname = entity_dot_name(vtxcontent_out(co));

  if (dagvtx_number(vtx)!=0)
  {
    string attribute = what_operation_shape(vtxcontent_optype(co));

    dagvtx_dot_node(out, "  ", vtx);
    fprintf(out, " [%s", attribute);
    if (!label_nodes)
      // show a short label with only the operation
      fprintf(out, ",label=\"%s\"", dagvtx_compact_operation(vtx));
    fprintf(out, "];\n");

    // image dependencies
    FOREACH(dagvtx, succ, dagvtx_succs(vtx))
    {
      dagvtx_dot_node(out, "  ", vtx);
      dagvtx_dot_node(out, " -> ", succ);
      fprintf(out, "%s%s%s;\n", show_arcs? " [label=\"": "",
	      show_arcs? vname: "", show_arcs? "\"]": "");
    }

    // scalar dependencies anywhere... hmmm...
    FOREACH(dagvtx, v, dag_vertices(d))
    {
      list vars = NIL;
      if (vtx!=v && freia_scalar_rw_dep(dagvtx_statement(vtx),
					dagvtx_statement(v), &vars))
      {
	dagvtx_dot_node(out, "  ", vtx);
	dagvtx_dot_node(out, " -> ", v);
	fprintf(out, " [" SCL_DEP);

	if (vars && show_arcs)
	{
	  int count = 0;
	  fprintf(out, ",label=\"");
	  FOREACH(entity, var, vars)
	    fprintf(out, "%s%s", count++? " ": "", entity_dot_name(var));
	  fprintf(out, "\"");
	}
	fprintf(out, "];\n");
      }
      gen_free_list(vars), vars = NIL;
    }
  }
  else
  {
    // input image...
    FOREACH(dagvtx, succ, dagvtx_succs(vtx))
    {
      fprintf(out, "  \"%s\"", vname);
      dagvtx_dot_node(out, " -> ", succ);
      fprintf(out, ";\n");
    }
  }
}

static void dagvtx_list_dot(FILE * out, const string comment, const list l)
{
  if (comment) fprintf(out, "  // %s\n", comment);
  FOREACH(dagvtx, v, l)
    fprintf(out, "  \"%s\" [shape=circle];\n",
	    entity_dot_name(vtxcontent_out(dagvtx_content(v))));
  fprintf(out, "\n");
}

/* dag debug with dot format.
 */
void dag_dot(FILE * out, const string what, const dag d)
{
  fprintf(out, "digraph \"%s\" {\n", what);

  dagvtx_list_dot(out, "inputs", dag_inputs(d));
  dagvtx_list_dot(out, "outputs", dag_outputs(d));

  fprintf(out, "  // computation vertices\n");
  FOREACH(dagvtx, vx, dag_vertices(d))
  {
    dagvtx_dot(out, d, vx);
    vtxcontent c = dagvtx_content(vx);

    // outputs arcs for vx
    if (gen_in_list_p(vx, dag_outputs(d)))
    {
      dagvtx_dot_node(out, "  ", vx);
      fprintf(out, " -> \"%s\";\n", entity_dot_name(vtxcontent_out(c)));
    }
  }

  fprintf(out, "}\n");
}

#define DOT_SUFFIX ".dot"

/* generate a "dot" format from a dag to a file.
 */
void dag_dot_dump(const string module, const string name, const dag d)
{
  // build file name
  string src_dir = db_get_directory_name_for_module(module);
  string fn = strdup(cat(src_dir, "/", name, DOT_SUFFIX, NULL));
  free(src_dir);

  FILE * out = safe_fopen(fn, "w");
  fprintf(out, "// graph for dag \"%s\" of module \"%s\" in dot format\n",
	  name, module);
  dag_dot(out, name, d);
  safe_fclose(out, fn);
  free(fn);
}

void dag_dot_dump_prefix(string module, string prefix, int number, dag d)
{
  string name = strdup(cat(prefix, itoa(number), NULL));
  dag_dot_dump(module, name, d);
  free(name);
}

// debug help
static void check_removed(const dagvtx v, const dagvtx removed)
{ pips_assert("not removed vertex", v!=removed); }

static int dagvtx_cmp_entity(const dagvtx * v1, const dagvtx * v2)
{
  return compare_entities(&vtxcontent_out(dagvtx_content(*v1)),
			  &vtxcontent_out(dagvtx_content(*v2)));
}

static void vertex_list_sorted_by_entities(list l)
{
  gen_sort_list(l, (gen_cmp_func_t) dagvtx_cmp_entity);
}

/* remove vertex v from dag d.
 * if v isx a used computation vertex, it is substituted by an input vertex.
 */
void dag_remove_vertex(dag d, const dagvtx v)
{
  pips_assert("vertex is in dag", gen_in_list_p(v, dag_vertices(d)));

  if (dagvtx_succs(v))
  {
    entity var = vtxcontent_out(dagvtx_content(v));
    pips_assert("some variable", var!=entity_undefined);
    dagvtx input =
      make_dagvtx(make_vtxcontent(0, 0, make_pstatement_empty(), NIL, var),
		  gen_copy_seq(dagvtx_succs(v)));
    dag_inputs(d) = CONS(dagvtx, input, dag_inputs(d));
    dag_vertices(d) = CONS(dagvtx, input, dag_vertices(d));
    vertex_list_sorted_by_entities(dag_inputs(d));
  }

  // remove from vertex lists
  gen_remove(&dag_vertices(d), v);
  gen_remove(&dag_inputs(d), v);
  gen_remove(&dag_outputs(d), v);

  // remove from successors of any ???
  FOREACH(dagvtx, dv, dag_vertices(d))
    gen_remove(&dagvtx_succs(dv), v);

  // unlink vertex itself
  gen_free_list(dagvtx_succs(v)), dagvtx_succs(v) = NIL;

  ifdebug(8) gen_context_recurse(d, v, dagvtx_domain, gen_true, check_removed);
}

/* copy a vertex, but without its successors.
 */
dagvtx copy_dagvtx_norec(dagvtx v)
{
  list lsave = dagvtx_succs(v);
  // temporary cut costs
  dagvtx_succs(v) = NIL;
  dagvtx copy = copy_dagvtx(v);
  dagvtx_succs(v) = lsave;
  return copy;
}

/* returns whether the vertex is an image copy operation.
 */
static bool dagvtx_is_copy_p(const dagvtx v)
{
  vtxcontent c = dagvtx_content(v);
  const freia_api_t * api = get_freia_api(vtxcontent_opid(c));
  return same_string_p(AIPO "copy", api->function_name);
}

/* returns whether the vertex is an image measurement operation.
 */
static bool dagvtx_is_measurement_p(const dagvtx v)
{
  vtxcontent c = dagvtx_content(v);
  const freia_api_t * api = get_freia_api(vtxcontent_opid(c));
  return strncmp(api->function_name, AIPO "global_", strlen(AIPO "global_"))==0;
}

/* append new vertex nv to dag d.
 */
void dag_append_vertex(dag d, dagvtx nv)
{
  pips_assert("not in dag", !gen_in_list_p(nv, dag_vertices(d)));
  pips_assert("no successors", dagvtx_succs(nv) == NIL);

  // pips_assert("dag d ok 1", dag_consistent_p(d));
  // pips_assert("nv is ok", dagvtx_consistent_p(nv));

  FOREACH(entity, e, vtxcontent_inputs(dagvtx_content(nv)))
  {
    pips_assert("e is defined", e!=entity_undefined);
    dagvtx pv = dagvtx_get_producer(d, NULL, e);
    if (!pv)
    {
      // side effect, create an input node of type 0 (not a computation)
      pv = make_dagvtx
	(make_vtxcontent(0, 0, make_pstatement_empty(), NIL, e), NIL);

      dag_inputs(d) = CONS(dagvtx, pv, dag_inputs(d));
      vertex_list_sorted_by_entities(dag_inputs(d));
      dag_vertices(d) = CONS(dagvtx, pv, dag_vertices(d));
    }
    // a vertex may have several time the same successor: b = a + a
    dagvtx_succs(pv) = CONS(dagvtx, nv, dagvtx_succs(pv));
  }
  dag_vertices(d) = CONS(dagvtx, nv, dag_vertices(d));

  // ??? what about scalar deps?
}

/* return the number of actual operations in dag d.
 * may be zero if only input vertices remain in the dag after optimizations.
 */
int dag_computation_count(const dag d)
{
  int count = 0;
  FOREACH(dagvtx, v, dag_vertices(d))
    if (dagvtx_number(v)!=0)
      count++;
  return count;
}

/* return target predecessors as a list.
 * the same predecessor appears twice in b = a+a
 * build them in call order!
 */
list dag_vertex_preds(const dag d, const dagvtx target)
{
  list inputs = vtxcontent_inputs(dagvtx_content(target));
  int nins = (int) gen_length(inputs);
  entity first_img = NULL, second_img = NULL;
  if (nins>=1) first_img = ENTITY(CAR(inputs));
  if (nins==2) second_img = ENTITY(CAR(CDR(inputs)));
  dagvtx first_v = NULL, second_v = NULL;

  FOREACH(dagvtx, v, dag_vertices(d))
  {
    if (v!=target && gen_in_list_p(target, dagvtx_succs(v)))
    {
      if (dagvtx_image(v)==first_img)
	first_v = v;
      if (dagvtx_image(v)==second_img)
	second_v = v;
    }
  }

  list preds = NIL;
  if (second_v) preds = CONS(dagvtx, second_v, NIL);
  if (first_v) preds = CONS(dagvtx, first_v, preds);
  return preds;
}

static bool gen_list_equals_p(const list l1, const list l2)
{
  bool equal = true;
  list p1 = (list) l1, p2 = (list) l2;
  while (equal && p1 && p2) {
    if (CHUNK(CAR(p1))!=CHUNK(CAR(p2)))
      equal = false;
    p1 = CDR(p1), p2 = CDR(p2);
  }
  equal &= (!p1 && !p2);
  return equal;
}

/* replace target vertex by a copy of source results...
 */
static void
switch_vertex_to_a_copy(dagvtx target, dagvtx source, list tpreds)
{
  pips_debug(5, "replacing %"_intFMT" by %"_intFMT"\n",
	     dagvtx_number(target), dagvtx_number(source));

  ifdebug(9) {
    dagvtx_dump(stderr, "in source", source);
    dagvtx_dump(stderr, "in target", target);
  }

  entity src_img = dagvtx_image(source);
  // fix contents
  vtxcontent cot = dagvtx_content(target);
  vtxcontent_optype(cot) = spoc_type_nop;
  vtxcontent_opid(cot) = hwac_freia_api_index("freia_aipo_copy");
  gen_free_list(vtxcontent_inputs(cot));
  vtxcontent_inputs(cot) = CONS(entity, src_img, NIL);

  // fix vertices

  FOREACH(dagvtx, v, tpreds)
    gen_remove(&dagvtx_succs(v), target);

  FOREACH(dagvtx, s, dagvtx_succs(target))
    gen_list_patch(vtxcontent_inputs(dagvtx_content(s)),
		   dagvtx_image(target), src_img);
  dagvtx_succs(source) = gen_once(target, dagvtx_succs(source));
  dagvtx_succs(source) = gen_nconc(dagvtx_succs(target), dagvtx_succs(source));
  dagvtx_succs(target) = NIL;

  // should I kill the statement? no, done by the copy removal stuff

  ifdebug(9) {
    dagvtx_dump(stderr, "out source", source);
    dagvtx_dump(stderr, "out target", target);
  }
}

static bool same_operation_p(const dagvtx v1, const dagvtx v2)
{
  return
    dagvtx_optype(v1) == dagvtx_optype(v2) &&
    dagvtx_opid(v1) == dagvtx_opid(v2);
}

static bool commutative_operation_p(const dagvtx v1, const dagvtx v2)
{
  if (dagvtx_optype(v1) == dagvtx_optype(v2))
  {
    int n1 = (int) dagvtx_opid(v1), n2 = (int) dagvtx_opid(v2);
    const freia_api_t * f1 = get_freia_api(n1);
    return f1->commutator && hwac_freia_api_index(f1->commutator)==n2;
  }
  else return false;
}

static bool list_commuted_p(const list l1, const list l2)
{
  pips_assert("length 2", gen_length(l1)==2 && gen_length(l2)==2);
  return CHUNKP(CAR(CDR(l1)))==CHUNKP(CAR(l2)) &&
    CHUNKP(CAR(l1))==CHUNKP(CAR(CDR(l2)));
}

/* "copy" copies "source" image in dag "d".
 * remove it properly.
 */
static void unlink_copy_vertex(dag d, const entity source, dagvtx copy)
{
  entity target = vtxcontent_out(dagvtx_content(copy));
  // may be NULL if source is an input
  dagvtx prod = dagvtx_get_producer(d, copy, source);

  // add copy successors as successors of prod
  // it is kept as a successor in case it is not removed
  if (prod)
  {
    FOREACH(dagvtx, vs, dagvtx_succs(copy))
      dagvtx_succs(prod) = gen_once(vs, dagvtx_succs(prod));
  }

  // replace target image by source image in all v successors
  FOREACH(dagvtx, succ, dagvtx_succs(copy))
  {
    vtxcontent sc = dagvtx_content(succ);
    gen_list_patch(vtxcontent_inputs(sc), target, source);
  }

  // copy has no more successors
  gen_free_list(dagvtx_succs(copy));
  dagvtx_succs(copy) = NIL;
}

/* @return whether all vertices in list "lv" are copies.
 */
static bool all_vertices_are_copies_or_measures_p(const list lv)
{
  FOREACH(dagvtx, v, lv)
    if (!(dagvtx_is_copy_p(v) || dagvtx_is_measurement_p(v)))
      return false;
  return true;
}

/* remove AIPO copies detected as useless.
 * remove identical operations.
 * @return statements to be managed outside...
 */
list /* of statements */ dag_optimize(dag d)
{
  list lstats = NIL;
  set remove = set_make(set_pointer);

  ifdebug(6) {
    pips_debug(4, "considering dag:\n");
    dag_dump(stderr, "input", d);
  }

  // first, look for identical image operations (same inputs, same params)
  // (that produce image, we do not care about measures??)
  // the second one is replaced by a copy.
  // also handle commutations.

  if (get_bool_property("FREIA_REMOVE_DUPLICATE_OPERATIONS"))
  {
    list vertices = gen_nreverse(gen_copy_seq(dag_vertices(d)));
    hash_table previous = hash_table_make(hash_pointer, 10);

    FOREACH(dagvtx, vr, vertices)
    {
      pips_debug(8, "at vertex %"_intFMT"\n", dagvtx_number(vr));
      // skip no-operations
      int op = (int) vtxcontent_optype(dagvtx_content(vr));
      if (op<spoc_type_poc || op>spoc_type_thr) continue;

      list preds = dag_vertex_preds(d, vr);
      bool switched = false;
      HASH_MAP(pp, lp,
      {
	dagvtx p = (dagvtx) pp;

	pips_debug(8, "comparing %"_intFMT" and %"_intFMT"\n",
		   dagvtx_number(vr), dagvtx_number(p));

	// ??? maybe I should not remove all duplicates, because
	// recomputing them may be chip?
	if (same_operation_p(vr, p) &&
	    gen_list_equals_p(preds, (list) lp) &&
	    same_constant_parameters(vr, p))
	{
	  switch_vertex_to_a_copy(vr, p, preds);
	  switched = true;
	  break;
	}
	else if (commutative_operation_p(vr, p) &&
		 list_commuted_p(preds, (list) lp))
	{
	  switch_vertex_to_a_copy(vr, p, preds);
	  switched = true;
	  break;
	}
      },  previous);

      if (switched)
	gen_free_list(preds);
      else
	hash_put(previous, vr, preds);
    }

    ifdebug(8) {
      pips_debug(4, "after pass 1, dag:\n");
      dag_dump(stderr, "optim 1", d);
    }

    // cleanup
    HASH_MAP(k, v, if (v) gen_free_list(v), previous);
    hash_table_free(previous), previous = NULL;
    gen_free_list(vertices), vertices = NULL;
  }

  // only one pass is needed because we're going backwards
  // op-> X -copy-> Y images copies are replaced by op-> X & Y
  FOREACH(dagvtx, v, dag_vertices(d))
  {
    // skip special input nodes
    if (dagvtx_number(v)==0) break;

    if (dagvtx_is_copy_p(v))
    {
      vtxcontent c = dagvtx_content(v);
      entity target = vtxcontent_out(c);
      pips_assert("one output and one input to copy",
	  target!=entity_undefined && gen_length(vtxcontent_inputs(c))==1);

      // replace by its source everywhere it is used
      entity source = ENTITY(CAR(vtxcontent_inputs(c)));

      // remove!
      unlink_copy_vertex(d, source, v);

      // whether to actually remove v
      if (!gen_in_list_p(v, dag_outputs(d)))
	set_add_element(remove, remove, v);
    }
  }

  // what copies are kept in the dag
  hash_table intra_pipe_copies = hash_table_make(hash_pointer, 10);

  // A-copy->B where A is an input is removed from the dag and managed outside
  // if A-copy->X and A-copy->Y where A is not an input, the second copy
  // is replaced by an external X-copy->Y
  FOREACH(dagvtx, w, dag_vertices(d))
  {
    if (set_belong_p(remove, w))
      continue;

    if (dagvtx_is_copy_p(w))
    {
      vtxcontent c = dagvtx_content(w);
      entity target = vtxcontent_out(c);
      pips_assert("one output and one input to copy",
	  target!=entity_undefined && gen_length(vtxcontent_inputs(c))==1);

      entity source = ENTITY(CAR(vtxcontent_inputs(c)));
      dagvtx prod = dagvtx_get_producer(d, w, source);

      if (source==target)
      {
	// ??? this should not happen?
	set_add_element(remove, remove, w);
      }
      else if (dagvtx_number(prod)==0)
      {
	// fprintf(stderr, "COPY 1 removing %"_intFMT"\n", dagvtx_number(w));
	unlink_copy_vertex(d, source, w);
	set_add_element(remove, remove, w);
	lstats = CONS(statement, freia_copy_image(source, target), lstats);
      }
      else // source is not an input, but the result of a internal computation
      {
	if (all_vertices_are_copies_or_measures_p(dagvtx_succs(prod)))
	{
	  // ??? hmmm... there is an implicit assumption here that the
	  // source of the copy will be an output...
	  unlink_copy_vertex(d, source, w);
	  set_add_element(remove, remove, w);
	  lstats = CONS(statement, freia_copy_image(source, target), lstats);
	}
	else if (hash_defined_p(intra_pipe_copies, source))
	{
	  unlink_copy_vertex(d, source, w);
	  set_add_element(remove, remove, w);
	  lstats = CONS(statement,
		freia_copy_image((entity) hash_get(intra_pipe_copies, source),
				 target), lstats);
	}
	else // keep first copy
	  hash_put(intra_pipe_copies, source, target);
      }
    }
  }

  SET_FOREACH(dagvtx, r, remove)
  {
    pips_debug(7, "removing vertex %" _intFMT "\n", dagvtx_number(r));

    vtxcontent c = dagvtx_content(r);
    if (pstatement_statement_p(vtxcontent_source(c)))
      hwac_kill_statement(pstatement_statement(vtxcontent_source(c)));
    dag_remove_vertex(d, r);

    ifdebug(8)
      gen_context_recurse(d, r, dagvtx_domain, gen_true, check_removed);

    free_dagvtx(r);
  }

  set_free(remove);
  hash_table_free(intra_pipe_copies);

  ifdebug(6) {
    pips_debug(4, "resulting dag:\n");
    dag_dump(stderr, "cleaned", d);
  }

  return lstats;
}

/* return whether all vertices in list are mesures...
 */
static bool all_mesures_p(list lv)
{
  bool only_mes = true;
  FOREACH(dagvtx, v, lv)
  {
    if (dagvtx_optype(v)!=spoc_type_mes)
    {
      only_mes = false;
      break;
    }
  }
  return only_mes;
}

/* (re)compute the list of *GLOBAL* input & output images for this dag
 * ??? BUG the output is rather an approximation
 * should rely on used defs or out effects for the underlying
 * sequence. however, the status of chains and effects on C does not
 * allow it.
 */
void dag_compute_outputs(dag d)
{
  set outvars = set_make(set_pointer);
  set outs = set_make(set_pointer);
  set toremove = set_make(set_pointer);

  FOREACH(dagvtx, v, dag_vertices(d))
  {
    // pips_debug(8, "considering vertex %" _intFMT "\n", dagvtx_number(v));
    vtxcontent c = dagvtx_content(v);
    // skip special input nodes...
    if (dagvtx_number(v)!=0)
    {
      entity out = vtxcontent_out(c);
      if (out!=entity_undefined &&
	  // no successors
	  (!dagvtx_succs(v) ||
	   // all successors are mesures?
	   all_mesures_p(dagvtx_succs(v)) ||
	   // new parameter not yet an output
	   (formal_parameter_p(out) && !set_belong_p(outvars, out))))
      {
	// pips_debug(7, "appending %" _intFMT "\n", dagvtx_number(v));
	set_add_element(outvars, outvars, out);
	set_add_element(outs, outs, v);
      }
    }
    else
    {
      // ??? this aborts with terapix...
      // pips_assert("is an input vertex", gen_in_list_p(v, dag_inputs(d)));
      if (!dagvtx_succs(v))
	set_add_element(toremove, toremove, v);
    }
  }

  ifdebug(8)
  {
    dag_dump(stderr, "dag_compute_outputs", d);
    set_fprint(stderr, "new outs", outs, (gen_string_func_t) dagvtx_number);
  }

  // cleanup unused node inputs
  SET_FOREACH(dagvtx, vr, toremove)
    dag_remove_vertex(d, vr);

  gen_free_list(dag_outputs(d));
  dag_outputs(d) = set_to_sorted_list(outs, (gen_cmp_func_t) dagvtx_ordering);

  // cleanup
  set_free(outs);
  set_free(outvars);
  set_free(toremove);
}

static dagvtx find_twin_vertex(dag d, dagvtx target)
{
  // pips_debug(9, "target is %"_intFMT"\n", dagvtx_number(target));
  FOREACH(dagvtx, v, dag_vertices(d))
    if (dagvtx_number(target)==dagvtx_number(v))
      return v;
  pips_internal_error("twin vertex not found for %" _intFMT "\n",
		      dagvtx_number(target));
  return NULL;
}

/* catch some cases of missing outs between splits...
 * for "freia_scalar_03"...
 * I'm not that sure about the algorithm.
 * @param dfull full dag
 * @param ld list of sub dags of d
 */
void freia_hack_fix_global_ins_outs(dag dfull, dag d)
{
  FOREACH(dagvtx, v, dag_vertices(d))
  {
    if (dagvtx_number(v)!=0 &&
	// the vertex was an output node in the full dag
	(gen_in_list_p(find_twin_vertex(dfull, v), dag_outputs(dfull)) ||
	 // OR there were more successors in the full dag
	 gen_length(dagvtx_succs(v))!=
	 gen_length(dagvtx_succs(find_twin_vertex(dfull, v)))))
    {
      pips_debug(4, "adding %" _intFMT " as output\n", dagvtx_number(v));
      dag_outputs(d) = gen_once(v, dag_outputs(d));
    }
  }
}

/* remove unneeded statements?
 * you must know they are really un-needed!
 */
void dag_cleanup_other_statements(dag d)
{
  set toremove = set_make(set_pointer);

  FOREACH(dagvtx, v, dag_vertices(d))
    if (dagvtx_other_stuff_p(v))
      set_add_element(toremove, toremove, v);

  SET_FOREACH(dagvtx, vr, toremove)
    dag_remove_vertex(d, vr);
}

/* ??? I'm unsure about what happens to dead code in the pipeline...
 */

/* ??? BUG the code may not work properly when image variable are
 * reused within a pipe. The underlying issues are subtle and
 * would need careful thinking: the initial code is correct, the dag
 * representation is correct, but the generated code may reorder
 * dag vertices so that reused variables are made to interact one with
 * the other. Maybe I should recreate output variables in the generated code
 * for every pipeline. this would imply a cleanup phase to removed
 * unused images at the end. I would really need an SSA form on
 * images? this function checks the assumption before proceeding
 * further.
 */
bool single_image_assignement_p(dag d)
{
  set outs = set_make(set_pointer);
  FOREACH(dagvtx, v, dag_vertices(d))
  {
    vtxcontent c = dagvtx_content(v);
    entity out = vtxcontent_out(c);
    if (out!=entity_undefined)
    {
      if (set_belong_p(outs, out)) {
	set_free(outs);
	return false;
      }
      set_add_element(outs, outs, out);
    }
  }
  set_free(outs);
  return true;
}

/* returns whether there is a scalar RW dependency from any vs to v
 */
static bool any_scalar_dep(dagvtx v, set vs)
{
  bool dep = false;
  statement target = dagvtx_statement(v);
  SET_FOREACH(dagvtx, source, vs)
  {
    if (freia_scalar_rw_dep(dagvtx_statement(source), target, NULL))
    {
      dep = true;
      break;
    }
  }
  return dep;
}

/* check scalar dependency from computed to v.
 */
static bool
all_previous_stats_with_deps_are_computed(dag d, const set computed, dagvtx v)
{
  bool okay = true;

  // scan in statement order... does it matter?
  list lv = gen_nreverse(gen_copy_seq(dag_vertices(d)));
  FOREACH(dagvtx, pv, lv)
  {
    // all previous have been scanned
    if (pv==v) break;
    if (freia_scalar_rw_dep(dagvtx_statement(pv), dagvtx_statement(v), NULL) &&
	!set_belong_p(computed, pv))
    {
      okay = false;
      break;
    }
  }
  gen_free_list(lv);
  return okay;
}

/* return the vertices which may be computed from the list of
 * available images, excluding vertices in exclude.
 * return a list for determinism.
 * @param d is the considered full dag
 * @param computed holds all previously computed vertices
 * @param currents holds those in the current pipeline
 * @params maybe holds vertices with live images
 */
list /* of dagvtx */ get_computable_vertices
  (dag d, const set computed, const set maybe, const set currents)
{
  list computable = NIL;
  set local_currents = set_make(set_pointer);
  set_assign(local_currents, currents);

  // hmmm... should reverse the list to handle implicit dependencies?
  // where, there is an assert() to check that it does not happen.
  list lv = gen_nreverse(gen_copy_seq(dag_vertices(d)));

  FOREACH(dagvtx, v, lv)
  {
    if (set_belong_p(computed, v))
      continue;

    if (dagvtx_other_stuff_p(v))
    {
      // a vertex with other stuff is assimilated to the pipeline
      // as soon as its dependences are fullfilled.
      // I have a problem here... I really need use_defs?
      if (all_previous_stats_with_deps_are_computed(d, computed, v))
      {
	computable = CONS(dagvtx, v, computable);
	set_add_element(local_currents, local_currents, v);
      }
    }
    else // we have an image computation
    {
      list preds = dag_vertex_preds(d, v);
      pips_debug(9, "%d predecessors to %" _intFMT "\n",
		 (int) gen_length(preds), dagvtx_number(v));

      if(// no scalar dependencies in the current pipeline
	 !any_scalar_dep(v, local_currents) &&
	 // and image dependencies are fulfilled.
	 list_in_set_p(preds, maybe))
      {
	computable = CONS(dagvtx, v, computable);
	// we do not want deps with other currents considered!
	set_add_element(local_currents, local_currents, v);
      }

      gen_free_list(preds), preds = NIL;
    }

    // update availables: not needed under assert for no img reuse.
    // if (vtxcontent_out(c)!=entity_undefined)
    //  set_del_element(avails, avails, vtxcontent_out(c));
  }

  // cleanup
  set_free(local_currents);
  gen_free_list(lv);
  return computable;
}

void set_append_vertex_statements(set s, list lv)
{
  FOREACH(dagvtx, v, lv)
  {
    pstatement ps = vtxcontent_source(dagvtx_content(v));
    if (pstatement_statement_p(ps))
      set_add_element(s, s, pstatement_statement(ps));
  }
}

/* convert the first n items in list args to entities.
 */
static list
fs_expression_list_to_entity_list(list /* of expression */ args, int nargs)
{
  list /* of entity */ lent = NIL;
  int n=0;
  FOREACH(expression, ex, args)
  {
    syntax s = expression_syntax(ex);
    pips_assert("is a ref", syntax_reference_p(s));
    reference r = syntax_reference(s);
    pips_assert("simple ref", reference_indices(r)==NIL);
    lent = CONS(entity, reference_variable(r), lent);
    if (++n==nargs) break;
  }
  lent = gen_nreverse(lent);
  return lent;
}

/* extract first entity item from list.
 */
static entity extract_fist_item(list * lp)
{
  list l = gen_list_head(lp, 1);
  entity e = ENTITY(CAR(l));
  gen_free_list(l);
  return e;
}

/* append statement s to dag d
 */
static void dag_append_freia_call(dag d, statement s)
{
  pips_debug(5, "adding statement %" _intFMT "\n", statement_number(s));

  call c = freia_statement_to_call(s);

  if (c && entity_freia_api_p(call_function(c)))
  {
    entity called = call_function(c);
    const freia_api_t * api = hwac_freia_api(entity_local_name(called));
    pips_assert("some api", api!=NULL);
    list /* of entity */ args =
      fs_expression_list_to_entity_list(call_arguments(c),
					api->arg_img_in+api->arg_img_out);

    // extract arguments
    entity out = entity_undefined;
    pips_assert("one out image max for an AIPO", api->arg_img_out<=1);
    if (api->arg_img_out==1)
      out = extract_fist_item(&args);
    list ins = gen_list_head(&args, api->arg_img_in);
    pips_assert("no more arguments", gen_length(args)==0);

    vtxcontent cont =
      make_vtxcontent(-1, 0, make_pstatement_statement(s), ins, out);
    set_operation(api, &vtxcontent_optype(cont), &vtxcontent_opid(cont));

    dagvtx nv = make_dagvtx(cont, NIL);
    dag_append_vertex(d, nv);
  }
  else // some other kind of statement that we may keep in the DAG
  {
    dagvtx nv =
      make_dagvtx(make_vtxcontent(spoc_type_oth, 0,
	  make_pstatement_statement(s), NIL, entity_undefined), NIL);
    dag_vertices(d) = CONS(dagvtx, nv, dag_vertices(d));
  }
}

/* build a full dag from list of statements ls.
 * @param module
 * @param list of statements
 * @param number dag identifier in function
 * @param added_stat list of statements killed by dag optimization
 */
dag build_freia_dag(string module, list ls, int number, list * added_stats)
{
  // build full dag
  dag fulld = make_dag(NIL, NIL, NIL);

  FOREACH(statement, s, ls)
    dag_append_freia_call(fulld, s);
  dag_compute_outputs(fulld);
  ifdebug(3) dag_dump(stderr, "fulld", fulld);

  // dump resulting dag
  dag_dot_dump_prefix(module, "dag_", number, fulld);

  // remove copies if possible...
  *added_stats = dag_optimize(fulld);

  // dump final dag
  dag_dot_dump_prefix(module, "dag_cleaned_", number, fulld);

  return fulld;
}

/* Made after the generic current mappings
 * Fabien COELHO, 05/12/94
 */

#define DEFINE_STACK(PREFIX, name, type) \
\
static stack name##_stack = stack_undefined; \
\
PREFIX void make_##name##_stack() \
{\
  assert(name##_stack==stack_undefined);\
  name##_stack = stack_make(type##_domain, 0);\
}\
\
PREFIX void free_##name##_stack() \
{\
  stack_free(name##_stack); \
  name##_stack = stack_undefined; \
}\
\
PREFIX stack get_##name##_stack() \
{\
  return(name##_stack);\
}\
\
PREFIX void set_##name##_stack(s) \
stack s; \
{\
  assert(name##_stack==stack_undefined);\
  name##_stack = s;\
}\
\
PREFIX void reset_##name##_stack()\
{\
  name##_stack = stack_undefined; \
}\
\
PREFIX void name##_push(i)\
type i;\
{\
  stack_push((char *)i, name##_stack);\
}\
PREFIX type name##_replace(i)\
type i;\
{\
  return(stack_replace((char *)i, name##_stack));\
}\
\
PREFIX type name##_pop()\
{\
  return((type) stack_pop(name##_stack));\
}\
\
PREFIX type name##_head()\
{\
  return((type) stack_head(name##_stack));\
}\
\
PREFIX bool name##_empty_p()\
{\
  return(stack_empty_p(name##_stack));\
}\
\
PREFIX int name##_size()\
{\
  return(stack_size(name##_stack));\
}\
\
static void check_##name##_stack()\
{\
  stack s = get_##name##_stack();\
  char\
     *item_1 = (char *) check_##name##_stack,\
     *item_2 = (char *) get_##name##_stack;\
  \
  reset_##name##_stack();\
  make_##name##_stack();\
  assert(name##_empty_p());\
  name##_push(item_1);\
  assert((char *) name##_head()==item_1);\
  name##_replace(item_2);\
  assert((char *) name##_pop()==item_2);\
  assert(name##_size()==0);\
  free_##name##_stack();\
  reset_##name##_stack();\
  set_##name##_stack(s);\
}

#define DEFINE_LOCAL_STACK(name, type) DEFINE_STACK(static, name, type)
#define DEFINE_GLOBAL_STACK(name, type) DEFINE_STACK(/**/, name, type)

/*  That is all
 */

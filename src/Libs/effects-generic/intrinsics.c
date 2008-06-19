/* package generic effects :  Be'atrice Creusillet 5/97
 *
 * $Id$
 *
 * File: intrinsics.c
 * ~~~~~~~~~~~~~~~~~~
 *
 * This File contains the generic functions necessary for the computation of 
 * all types of proper effects and proper references of intrinsics.
 *
 */

#include <stdio.h>
#include <string.h>

#include "genC.h"

#include "linear.h"
#include "ri.h"
#include "database.h"

#include "ri-util.h"
#include "control.h"
#include "constants.h"
#include "misc.h"
#include "text-util.h"
#include "text.h"
#include "makefile.h"

#include "properties.h"
#include "pipsmake.h"

#include "transformer.h"
#include "semantics.h"
#include "pipsdbm.h"
#include "resources.h"

#include "effects-generic.h"


/********************************************************* LOCAL FUNCTIONS */

static list no_write_effects(entity e,list args);
static list affect_effects(entity e,list args);
static list update_effects(entity e,list args);
static list unique_update_effects(entity e,list args);
static list assign_substring_effects(entity e,list args);
static list substring_effect(entity e,list args);
static list some_io_effects(entity e, list args);
static list io_effects(entity e, list args);
static list read_io_effects(entity e, list args);
static list effects_of_ioelem(expression exp, tag act);
static list effects_of_iolist(list exprs, tag act);
static list effects_of_implied_do(expression exp, tag act);


/* the following data structure indicates wether an io element generates
a read effects or a write effect. the kind of effect depends on the
instruction type: for instance, access generates a read effect if used
within an open statement, and a write effect when used inside an inquire
statement */

typedef struct IoElementDescriptor {
    string StmtName;
    string IoElementName;
    tag ReadOrWrite, MayOrMust;
} IoElementDescriptor;

static IoElementDescriptor IoElementDescriptorUndefined;

static IoElementDescriptor IoElementDescriptorTable[] = {
    {"OPEN",      "UNIT=",        is_action_read, is_approximation_must},
    {"OPEN",      "ERR=",         is_action_read, is_approximation_may},
    {"OPEN",      "FILE=",        is_action_read, is_approximation_must},
    {"OPEN",      "STATUS=",      is_action_read, is_approximation_may},
    {"OPEN",      "ACCESS=",      is_action_read, is_approximation_must},
    {"OPEN",      "FORM=",        is_action_read, is_approximation_must},
    {"OPEN",      "RECL=",        is_action_read, is_approximation_must},
    {"OPEN",      "BLANK=",       is_action_read, is_approximation_may},
    {"OPEN",      "IOSTAT=",      is_action_write, is_approximation_may},

    {"CLOSE",     "UNIT=",        is_action_read, is_approximation_must},
    {"CLOSE",     "ERR=",         is_action_read, is_approximation_may},
    {"CLOSE",     "STATUS=",      is_action_read, is_approximation_may},
    {"CLOSE",     "IOSTAT=",      is_action_write, is_approximation_may},

    {"INQUIRE",   "UNIT=",        is_action_read, is_approximation_must},
    {"INQUIRE",   "ERR=",         is_action_read, is_approximation_may},
    {"INQUIRE",   "FILE=",        is_action_read, is_approximation_must},
    {"INQUIRE",   "IOSTAT=",      is_action_write, is_approximation_must},
    {"INQUIRE",   "EXIST=",       is_action_write, is_approximation_must},
    {"INQUIRE",   "OPENED=",      is_action_write, is_approximation_must},
    {"INQUIRE",   "NUMBER=",      is_action_write, is_approximation_must},
    {"INQUIRE",   "NAMED=",       is_action_write, is_approximation_must},
    {"INQUIRE",   "NAME=",        is_action_write, is_approximation_must},
    {"INQUIRE",   "ACCESS=",      is_action_write, is_approximation_must},
    {"INQUIRE",   "SEQUENTIAL=",  is_action_write, is_approximation_must},
    {"INQUIRE",   "DIRECT=",      is_action_write, is_approximation_must},
    {"INQUIRE",   "FORM=",        is_action_write, is_approximation_must},
    {"INQUIRE",   "FORMATTED=",   is_action_write, is_approximation_must},
    {"INQUIRE",   "UNFORMATTED=", is_action_write, is_approximation_must},
    {"INQUIRE",   "RECL=",        is_action_write, is_approximation_must},
    {"INQUIRE",   "NEXTREC=",     is_action_write, is_approximation_must},
    {"INQUIRE",   "BLANK=",       is_action_write, is_approximation_must},

    {"BACKSPACE", "UNIT=",        is_action_read, is_approximation_must},
    {"BACKSPACE", "ERR=",         is_action_read, is_approximation_may},
    {"BACKSPACE", "IOSTAT=",      is_action_write, is_approximation_may},

    {"ENDFILE",   "UNIT=",        is_action_read, is_approximation_must},
    {"ENDFILE",   "ERR=",         is_action_read, is_approximation_may},
    {"ENDFILE",   "IOSTAT=",      is_action_write, is_approximation_may},

    {"REWIND",    "UNIT=",        is_action_read, is_approximation_must},
    {"REWIND",    "ERR=",         is_action_read, is_approximation_may},
    {"REWIND",    "IOSTAT=",      is_action_write, is_approximation_may},

    {"READ",      "FMT=",         is_action_read, is_approximation_must},
    {"READ",      "UNIT=",        is_action_read, is_approximation_must},
    {"READ",      "REC=",         is_action_read, is_approximation_must},
    {"READ",      "ERR=",         is_action_read, is_approximation_may},
    {"READ",      "END=",         is_action_read, is_approximation_must},
    {"READ",      "IOSTAT=",      is_action_write, is_approximation_may},
    {"READ",      "IOLIST=",      is_action_write, is_approximation_must},

    {"WRITE",     "FMT=",         is_action_read, is_approximation_must},
    {"WRITE",     "UNIT=",        is_action_read, is_approximation_must},
    {"WRITE",     "REC=",         is_action_read, is_approximation_must},
    {"WRITE",     "ERR=",         is_action_read, is_approximation_may},
    {"WRITE",     "END=",         is_action_read, is_approximation_must},
    {"WRITE",     "IOSTAT=",      is_action_write, is_approximation_may},
    {"WRITE",     "IOLIST=",      is_action_read, is_approximation_must},

    /* C IO intrinsics */

    {"printf",     "FMT=",        is_action_read, is_approximation_must},
    {"fprintf",     "&",        is_action_read, is_approximation_must},

    {0,           0,              0,              0}
};


/* the following data structure describes an intrinsic function: its
name and the function to apply on a call to this intrinsic to get the
effects of the call */

typedef struct IntrinsicDescriptor
{
    string name;
    list (*f)();
} IntrinsicDescriptor;

static IntrinsicDescriptor IntrinsicDescriptorTable[] = {
    {"+",                        no_write_effects},
    {"-",                        no_write_effects},
    {"/",                        no_write_effects},
    {"*",                        no_write_effects},
    {INVERSE_OPERATOR_NAME,      no_write_effects},
    {"--",                       no_write_effects},
    {"**",                       no_write_effects},
    {".EQV.",                    no_write_effects},
    {".NEQV.",                   no_write_effects},
    {".OR.",                     no_write_effects},
    {".AND.",                    no_write_effects},
    {".LT.",                     no_write_effects},
    {".GT.",                     no_write_effects},
    {".LE.",                     no_write_effects},
    {".GE.",                     no_write_effects},
    {".EQ.",                     no_write_effects},
    {".NE.",                     no_write_effects},
    {"//",                       no_write_effects},
    {".NOT.",                    no_write_effects},

    {"CONTINUE",                 no_write_effects},
    {"ENDDO",                    no_write_effects},
    {"PAUSE",                    some_io_effects},
    {"RETURN",                   no_write_effects},
    {"STOP",                     some_io_effects},
    {"END",                      no_write_effects},
    {"FORMAT",                   no_write_effects},

    { IMPLIED_COMPLEX_NAME,      no_write_effects},
    { IMPLIED_DCOMPLEX_NAME,     no_write_effects},

    {"INT",                      no_write_effects},
    {"IFIX",                     no_write_effects},
    {"IDINT",                    no_write_effects},
    {"REAL",                     no_write_effects},
    {"FLOAT",                    no_write_effects},
    {"DFLOAT",                   no_write_effects},
    {"SNGL",                     no_write_effects},
    {"DBLE",                     no_write_effects},
    {"DREAL",                    no_write_effects}, /* Added for Arnauld Leservot */
    {"CMPLX",                    no_write_effects},
    {"DCMPLX",                   no_write_effects},
    {"ICHAR",                    no_write_effects},
    {"CHAR",                     no_write_effects},
    {"AINT",                     no_write_effects},
    {"DINT",                     no_write_effects},
    {"ANINT",                    no_write_effects},
    {"DNINT",                    no_write_effects},
    {"NINT",                     no_write_effects},
    {"IDNINT",                   no_write_effects},
    {"IABS",                     no_write_effects},
    {"ABS",                      no_write_effects},
    {"DABS",                     no_write_effects},
    {"CABS",                     no_write_effects},
    {"CDABS",                    no_write_effects},

    {"MOD",                      no_write_effects},
    {"AMOD",                     no_write_effects},
    {"DMOD",                     no_write_effects},
    {"ISIGN",                    no_write_effects},
    {"SIGN",                     no_write_effects},
    {"DSIGN",                    no_write_effects},
    {"IDIM",                     no_write_effects},
    {"DIM",                      no_write_effects},
    {"DDIM",                     no_write_effects},
    {"DPROD",                    no_write_effects},
    {"MAX",                      no_write_effects},
    {"MAX0",                     no_write_effects},
    {"AMAX1",                    no_write_effects},
    {"DMAX1",                    no_write_effects},
    {"AMAX0",                    no_write_effects},
    {"MAX1",                     no_write_effects},
    {"MIN",                      no_write_effects},
    {"MIN0",                     no_write_effects},
    {"AMIN1",                    no_write_effects},
    {"DMIN1",                    no_write_effects},
    {"AMIN0",                    no_write_effects},
    {"MIN1",                     no_write_effects},
    {"LEN",                      no_write_effects},
    {"INDEX",                    no_write_effects},
    {"AIMAG",                    no_write_effects},
    {"DIMAG",                    no_write_effects},
    {"CONJG",                    no_write_effects},
    {"DCONJG",                   no_write_effects},
    {"SQRT",                     no_write_effects},
    {"DSQRT",                    no_write_effects},
    {"CSQRT",                    no_write_effects},

    {"EXP",                      no_write_effects},
    {"DEXP",                     no_write_effects},
    {"CEXP",                     no_write_effects},
    {"LOG",                      no_write_effects},
    {"ALOG",                     no_write_effects},
    {"DLOG",                     no_write_effects},
    {"CLOG",                     no_write_effects},
    {"LOG10",                    no_write_effects},
    {"ALOG10",                   no_write_effects},
    {"DLOG10",                   no_write_effects},
    {"SIN",                      no_write_effects},
    {"DSIN",                     no_write_effects},
    {"CSIN",                     no_write_effects},
    {"COS",                      no_write_effects},
    {"DCOS",                     no_write_effects},
    {"CCOS",                     no_write_effects},
    {"TAN",                      no_write_effects},
    {"DTAN",                     no_write_effects},
    {"ASIN",                     no_write_effects},
    {"DASIN",                    no_write_effects},
    {"ACOS",                     no_write_effects},
    {"DACOS",                    no_write_effects},
    {"ATAN",                     no_write_effects},
    {"DATAN",                    no_write_effects},
    {"ATAN2",                    no_write_effects},
    {"DATAN2",                   no_write_effects},
    {"SINH",                     no_write_effects},
    {"DSINH",                    no_write_effects},
    {"COSH",                     no_write_effects},
    {"DCOSH",                    no_write_effects},
    {"TANH",                     no_write_effects},
    {"DTANH",                    no_write_effects},

    {"LGE",                      no_write_effects},
    {"LGT",                      no_write_effects},
    {"LLE",                      no_write_effects},
    {"LLT",                      no_write_effects},

    {LIST_DIRECTED_FORMAT_NAME,  no_write_effects},
    {UNBOUNDED_DIMENSION_NAME,   no_write_effects},

    {"=",                        affect_effects},

    {"WRITE",                    io_effects},
    {"REWIND",                   io_effects},
    {"BACKSPACE",                io_effects},
    {"OPEN",                     io_effects},
    {"CLOSE",                    io_effects},
    {"INQUIRE",                  io_effects},
    {"READ",                     read_io_effects},
    {"BUFFERIN",                 io_effects},
    {"BUFFEROUT",                io_effects},
    {"ENDFILE",                  io_effects},
    {IMPLIED_DO_NAME,            effects_of_implied_do},

    {SUBSTRING_FUNCTION_NAME,    substring_effect},
    {ASSIGN_SUBSTRING_FUNCTION_NAME, assign_substring_effects},

    /* These operators are used within the OPTIMIZE transformation in
       order to manipulate operators such as n-ary add and multiply or
       multiply-add operators ( JZ - sept 98) */
    {EOLE_SUM_OPERATOR_NAME,     no_write_effects },
    {EOLE_PROD_OPERATOR_NAME,    no_write_effects },
    {EOLE_FMA_OPERATOR_NAME,     no_write_effects },

	{IMA_OPERATOR_NAME,          no_write_effects },
	{IMS_OPERATOR_NAME,          no_write_effects },

    /* Here are C intrinsics.*/
  
    {".",                    no_write_effects},
    {"->",                    no_write_effects},
    {"post++",                    unique_update_effects},
    {"post--",                    unique_update_effects},
    {"++pre",                    unique_update_effects},
    {"--pre",                    unique_update_effects},
    {"&",                    no_write_effects},
    {"*indirection",                    no_write_effects},
    {"+unary",                    no_write_effects},
    {"-unary",                    no_write_effects},
    {"~",                    no_write_effects},
    {"!",                    no_write_effects},
    {"%",                    no_write_effects},
    {"+C",                    no_write_effects},
    {"-C",                    no_write_effects},
    {"<<",                    no_write_effects},
    {">>",                    no_write_effects},
    {"<",                    no_write_effects},
    {">",                    no_write_effects},
    {"<=",                    no_write_effects},
    {">=",                    no_write_effects},
    {"==",                    no_write_effects},
    {"!=",                    no_write_effects},
    {"&bitand",                    no_write_effects},
    {"^",                    no_write_effects},
    {"|",                    no_write_effects},
    {"&&",                    no_write_effects},
    {"||",                    no_write_effects},
    {"*=",                    update_effects},
    {"/=",                    update_effects},
    {"%=",                    update_effects},
    {"+=",                    update_effects},
    {"-=",                    update_effects},
    {"<<=",                   update_effects},
    {">>=",                   update_effects},
    {"&=",                    update_effects},
    {"^=",                    update_effects},
    {"|=",                    update_effects},
    {",",                    no_write_effects}, 

    {BRACE_INTRINSIC,            no_write_effects},
    {"break",                    no_write_effects},
    {"case",                    no_write_effects},  
    {"default",                    no_write_effects},
    {"return",                    no_write_effects},

    /* These intrinsics are added with no_write_effects to work with C. 
       The real effects must be studied !!! I do not have time for the moment */
       
    {"__assert",                    no_write_effects},

    /* #include <ctype.h>*/

    {"isalnum",                    no_write_effects}, 
    {"isalpha",                    no_write_effects}, 
    {"iscntrl",                    no_write_effects}, 
    {"isdigit",                    no_write_effects}, 
    {"isgraph",                    no_write_effects}, 
    {"islower",                    no_write_effects}, 
    {"isprint",                    no_write_effects}, 
    {"ispunct",                    no_write_effects}, 
    {"isspace",                    no_write_effects}, 
    {"isupper",                    no_write_effects}, 
    {"isxdigit",                    no_write_effects}, 
    {"tolower",                    no_write_effects}, 
    {"toupper",                    no_write_effects}, 
    {"isascii",                    no_write_effects}, 
    {"toascii",                    no_write_effects}, 
    {"_tolower",                    no_write_effects}, 
    {"_toupper",                    no_write_effects}, 
  
    {"errno",                    no_write_effects}, 

    {"__flt_rounds",                    no_write_effects}, 

    {"_sysconf",                    no_write_effects}, 
    {"setlocale",                    no_write_effects},
    {"localeconv",                    no_write_effects},
    {"dcgettext",                    no_write_effects},
    {"dgettext",                    no_write_effects},
    {"gettext",                    no_write_effects},
    {"textdomain",                    no_write_effects},
    {"bindtextdomain",                    no_write_effects},
    {"wdinit",                    no_write_effects}, 
    {"wdchkind",                    no_write_effects}, 
    {"wdbindf",                    no_write_effects}, 
    {"wddelim",                    no_write_effects}, 
    {"mcfiller",                    no_write_effects},
    {"mcwrap",                    no_write_effects},

    /* #include <math.h>*/

    {"acos",                    no_write_effects},  
    {"asin",                    no_write_effects}, 
    {"atan",                    no_write_effects}, 
    {"atan2",                    no_write_effects},   
    {"cos",                    no_write_effects}, 
    {"sin",                    no_write_effects}, 
    {"tan",                    no_write_effects}, 
    {"cosh",                    no_write_effects}, 
    {"sinh",                    no_write_effects}, 
    {"tanh",                    no_write_effects}, 
    {"exp",                    no_write_effects}, 
    {"frexp",                    no_write_effects},  
    {"ldexp",                    no_write_effects},  
    {"log",                    no_write_effects}, 
    {"log10",                    no_write_effects}, 
    {"modf",                    no_write_effects},   
    {"pow",                    no_write_effects},   
    {"sqrt",                    no_write_effects},  
    {"ceil",                    no_write_effects},  
    {"fabs",                    no_write_effects},  
    {"floor",                    no_write_effects},  
    {"fmod",                    no_write_effects},  
    {"erf",                    no_write_effects}, 
    {"erfc",                    no_write_effects}, 
    {"gamma",                    no_write_effects}, 
    {"hypot",                    no_write_effects}, 
    {"isnan",                    no_write_effects},  
    {"j0",                    no_write_effects}, 
    {"j1",                    no_write_effects}, 
    {"jn",                    no_write_effects}, 
    {"lgamma",                    no_write_effects}, 
    {"y0",                    no_write_effects}, 
    {"y1",                    no_write_effects}, 
    {"yn",                    no_write_effects}, 
    {"acosh",                    no_write_effects}, 
    {"asinh",                    no_write_effects}, 
    {"atanh",                    no_write_effects}, 
    {"cbrt",                    no_write_effects}, 
    {"logb",                    no_write_effects}, 
    {"nextafter",                    no_write_effects},   
    {"remainder",                    no_write_effects},   
    {"scalb",                    no_write_effects},   
    {"expm1",                    no_write_effects}, 
    {"ilogb",                    no_write_effects}, 
    {"log1p",                    no_write_effects}, 
    {"rint",                    no_write_effects}, 
    {"matherr",                    no_write_effects},  
    {"significand",                    no_write_effects}, 
    {"copysign",                    no_write_effects},   
    {"scalbn",                    no_write_effects}, 
    {"modff",                    no_write_effects},  
    {"sigfpe",                    no_write_effects},  
    {"single_to_decimal",                    no_write_effects}, 
    {"double_to_decimal",                    no_write_effects}, 
    {"extended_to_decimal",                    no_write_effects},
    {"quadruple_to_decimal",                    no_write_effects},
    {"decimal_to_single",                    no_write_effects},
    {"decimal_to_double",                    no_write_effects},
    {"decimal_to_extended",                    no_write_effects},
    {"decimal_to_quadruple",                    no_write_effects},
    {"string_to_decimal",                    no_write_effects},
    {"func_to_decimal",                    no_write_effects},
    {"file_to_decimal",                    no_write_effects},
    {"seconvert",                    no_write_effects},  
    {"sfconvert",                    no_write_effects},  
    {"sgconvert",                    no_write_effects},  
    {"econvert",                    no_write_effects},  
    {"fconvert",                    no_write_effects},  
    {"gconvert",                    no_write_effects},  
    {"qeconvert",                    no_write_effects},  
    {"qfconvert",                    no_write_effects},  
    {"qgconvert",                    no_write_effects},  
    {"ecvt",                    no_write_effects},  
    {"fcvt",                    no_write_effects},  
    {"gcvt",                    no_write_effects},  
    {"atof",                    no_write_effects},  
    {"strtod",                    no_write_effects},  

     /*#include <setjmp.h>*/

    {"setjmp",                    no_write_effects},
    {"__setjmp",                    no_write_effects},
    {"longjmp",                    no_write_effects},
    {"__longjmp",                    no_write_effects},
    {"sigsetjmp",                    no_write_effects},
    {"siglongjmp",                    no_write_effects},

    /*#include <stdio.h>*/
    {"remove",                    no_write_effects},
    {"rename",                    no_write_effects},
    {"tmpfile",                    no_write_effects},
    {"tmpnam",                    no_write_effects}, 
    {"fclose",                    no_write_effects},
    {"fflush",                    no_write_effects},
    {"fopen",                    no_write_effects}, 
    {"freopen",                    no_write_effects}, 
    {"setbuf",                    no_write_effects},
    {"setvbuf",                    no_write_effects},
    {"fprintf",                    io_effects},
    {"fscanf",                    io_effects},
    {"printf",                    io_effects},
    {"scanf",                    io_effects},
    {"sprintf",                    io_effects},
    {"sscanf",                    io_effects},
    {"vfprintf",                    io_effects},
    {"vprintf",                    io_effects},
    {"vsprintf",                    io_effects},
    {"fgetc",                    io_effects},
    {"fgets",                    io_effects}, 
    {"fputc",                    io_effects},
    {"fputs",                    io_effects},
    {"getc",                    io_effects},
    {"putc",                    io_effects},
    {"getchar",                    io_effects},
    {"putchar",                    io_effects},
    {"gets",                    io_effects}, 
    {"puts",                    io_effects},
    {"ungetc",                    io_effects},
    {"fread",                    io_effects}, 
    {"fwrite",                    io_effects},
    {"fgetpos",                    no_write_effects},
    {"fseek",                    no_write_effects},
    {"fsetpos",                    no_write_effects},
    {"ftell",                    no_write_effects}, 
    {"rewind",                    no_write_effects},
    {"clearerr",                    no_write_effects},
    {"feof",                    no_write_effects},
    {"ferror",                    no_write_effects},
    {"perror",                    no_write_effects},
    {"__filbuf",                    no_write_effects},
    {"__flsbuf",                    no_write_effects},
    {"setbuffer",                    no_write_effects},
    {"setlinebuf",                    no_write_effects},
    {"snprintf",                    no_write_effects},
    {"vsnprintf",                    no_write_effects},
    {"fdopen",                    no_write_effects}, 
    {"ctermid",                    no_write_effects}, 
    {"fileno",                    no_write_effects},
    {"popen",                    no_write_effects}, 
    {"cuserid",                    no_write_effects}, 
    {"tempnam",                    no_write_effects}, 
    {"getopt",                    no_write_effects},
    {"getsubopt",                    no_write_effects},
    {"getw",                    no_write_effects},
    {"putw",                    no_write_effects},
    {"pclose",                    no_write_effects},
    {"fseeko",                    no_write_effects},
    {"ftello",                    no_write_effects},
    {"fopen64",                    no_write_effects}, 
    {"freopen64",                    no_write_effects},
    {"tmpfile64",                    no_write_effects},
    {"fgetpos64",                    no_write_effects},
    {"fsetpos64",                    no_write_effects},
    {"fseeko64",                    no_write_effects},
    {"ftello64",                    no_write_effects},

    /*#include <stdlib.h>*/
    {"abort", no_write_effects},
    {"abs", no_write_effects},
    {"atexit", no_write_effects},
    {"atof", no_write_effects},
    {"atoi", no_write_effects},
    {"atol", no_write_effects},
    {"bsearch", no_write_effects},
    {"calloc", no_write_effects},
    {"div", no_write_effects},
    {"exit", no_write_effects},
    {"free", no_write_effects},
  /*  {char *getenv(const char *, 0, 0},
  {long int labs(long, 0, 0},
  {ldiv_t ldiv(long, long, 0, 0},*/
  {"malloc", no_write_effects},
  /* {int mblen(const char *, size_t, 0, 0},
  {size_t mbstowcs(wchar_t *, const char *, size_t, 0, 0},
  {int mbtowc(wchar_t *, const char *, size_t, 0, 0},
  {void qsort(void *, size_t, size_t,
	int (*)(const void *, const void *), 0, 0},
  {int rand(void, 0, 0},
  {void *realloc(void *, size_t, 0, 0},
  {void srand(unsigned int, 0, 0},
  {double strtod(const char *, char **, 0, 0},
  {long int strtol(const char *, char **, int, 0, 0},
  {unsigned long int strtoul(const char *, char **, int, 0, 0},
  {int system(const char *, 0, 0},
  {int wctomb(char *, wchar_t, 0, 0},
  {size_t wcstombs(char *, const wchar_t *, size_t, 0, 0},
  {void _exithandle(void, 0, 0},
  {double drand48(void, 0, 0},
  {double erand48(unsigned short *, 0, 0},
  {long jrand48(unsigned short *, 0, 0},
  {void lcong48(unsigned short *, 0, 0},
  {long lrand48(void, 0, 0},
  {long mrand48(void, 0, 0},
  {long nrand48(unsigned short *, 0, 0},
  {unsigned short *seed48(unsigned short *, 0, 0},
  {void srand48(long, 0, 0},
  {int putenv(char *, 0, 0},
  {void setkey(const char *, 0, 0},
  {void swab(const char *, char *, ssize_t, 0, 0},
  {int	mkstemp(char *, 0, 0},
  {int	mkstemp64(char *, 0, 0},
  {long a64l(const char *, 0, 0},
  {char *ecvt(double, int, int *, int *, 0, 0},
  {char *fcvt(double, int, int *, int *, 0, 0},
  {char *gcvt(double, int, char *, 0, 0},
  {int getsubopt(char **, char *const *, char **, 0, 0},
  {int  grantpt(int, 0, 0},
  {char *initstate(unsigned, char *, size_t, 0, 0},
  {char *l64a(long, 0, 0},
  {char *mktemp(char *, 0, 0},
  {char *ptsname(int, 0, 0},
  {long random(void, 0, 0},
  {char *realpath(const char *, char *, 0, 0},
  {char *setstate(const char *, 0, 0},
  {void srandom(unsigned, 0, 0},
  {int ttyslot(void, 0, 0},
  {int  unlockpt(int, 0, 0},
  {void *valloc(size_t, 0, 0},
  {int dup2(int, int, 0, 0},
  {char *qecvt(long double, int, int *, int *, 0, 0},
  {char *qfcvt(long double, int, int *, int *, 0, 0},
  {char *qgcvt(long double, int, char *, 0, 0},
  {char *getcwd(char *, size_t, 0, 0},
  {const char *getexecname(void, 0, 0},
  {char *getlogin(void, 0, 0},
  {int getopt(int, char *const *, const char *, 0, 0},
  {char *optarg;
  {int optind, opterr, optopt;
  {char *getpass(const char *, 0, 0},
  {char *getpassphrase(const char *, 0, 0},
  {int getpw(uid_t, char *, 0, 0},
  {int isatty(int, 0, 0},
  {void *memalign(size_t, size_t, 0, 0},
  {char *ttyname(int, 0, 0},
  {long long atoll(const char *, 0, 0},
  {long long llabs(long long, 0, 0},
  {lldiv_t lldiv(long long, long long, 0, 0},
  {char *lltostr(long long, char *, 0, 0},
  {long long strtoll(const char *, char **, int, 0, 0},
  {unsigned long long strtoull(const char *, char **, int, 0, 0},
  {char *ulltostr(unsigned long long, char *, 0, 0},*/
    {NULL, 0}
};



/* list generic_proper_effects_of_intrinsic(entity e, list args)
 * input    : a intrinsic function name, and the list or arguments. 
 * output   : the corresponding list of effects.
 * modifies : nothing.
 * comment  :	
 */
list
generic_proper_effects_of_intrinsic(entity e, list args)
{
    string s = entity_local_name(e);
    IntrinsicDescriptor *pid = IntrinsicDescriptorTable;
    list lr;

    pips_debug(3, "begin\n");

    while (pid->name != NULL) {
        if (strcmp(pid->name, s) == 0) {
	        lr = (*(pid->f))(e, args);
		pips_debug(3, "end\n");
                return(lr);
	    }

        pid += 1;
    }

    pips_error("generic_proper_effects_of_intrinsic", "unknown intrinsic %s\n", s);

    return(NIL);
}



static list 
no_write_effects(entity e __attribute__ ((__unused__)),list args)
{
    list lr;

    debug(5, "no_write_effects", "begin\n");
    lr = generic_proper_effects_of_expressions(args);
    debug(5, "no_write_effects", "end\n");
    return(lr);
}

static list 
affect_effects(entity e __attribute__ ((__unused__)),list args)
{
    list le = NIL;

    expression lhs = EXPRESSION(CAR(args));
    syntax s = expression_syntax(lhs);

    expression rhs = EXPRESSION(CAR(CDR(args)));

    pips_debug(5, "begin\n");

    if (! syntax_reference_p(s))
            pips_error("affect_effects", "not a reference\n");

    le = generic_proper_effects_of_lhs(syntax_reference(s));

    le = gen_nconc(le, generic_proper_effects_of_expression(rhs));

    pips_debug(5, "end\n");

    return(le);
}

static list 
update_effects(entity e __attribute__ ((__unused__)),list args)
{
    list le = NIL;

    expression lhs = EXPRESSION(CAR(args));
    syntax s = expression_syntax(lhs);

    expression rhs = EXPRESSION(CAR(CDR(args)));

    pips_debug(5, "begin\n");

    if (! syntax_reference_p(s))
            pips_error("affect_effects", "not a reference\n");

    le = generic_proper_effects_of_lhs(syntax_reference(s));

    le = gen_nconc(le, generic_proper_effects_of_expression(lhs));

    le = gen_nconc(le, generic_proper_effects_of_expression(rhs));

    pips_debug(5, "end\n");

    return(le);
}

static list 
unique_update_effects(entity e __attribute__ ((__unused__)),list args)
{
    list le = NIL;

    expression lhs = EXPRESSION(CAR(args));
    syntax s = expression_syntax(lhs);

    pips_debug(5, "begin\n");

    if (! syntax_reference_p(s))
            pips_error("affect_effects", "not a reference\n");

    le = generic_proper_effects_of_lhs(syntax_reference(s));

    le = gen_nconc(le, generic_proper_effects_of_expression(lhs));

    pips_debug(5, "end\n");

    return(le);
}

static list
assign_substring_effects(entity e __attribute__ ((__unused__)), list args)
{
    list le = NIL;

    expression lhs = EXPRESSION(CAR(args));
    syntax s = expression_syntax(lhs);
    expression l = EXPRESSION(CAR(CDR(args)));
    expression u = EXPRESSION(CAR(CDR(CDR(args))));
    expression rhs = EXPRESSION(CAR(CDR(CDR(CDR(args)))));

    pips_debug(5, "begin\n");

    if (! syntax_reference_p(s))
            pips_error("assign_substring_effects", "not a reference\n");


    le = generic_proper_effects_of_lhs(syntax_reference(s));
    le = gen_nconc(le, generic_proper_effects_of_expression(l));
    le = gen_nconc(le, generic_proper_effects_of_expression(u));

    le = gen_nconc(le, generic_proper_effects_of_expression(rhs));

    pips_debug(5, "end\n");
    return(le);
}

static list
substring_effect(entity e __attribute__ ((__unused__)), list args)
{
    list le = NIL;
    expression expr = EXPRESSION(CAR(args));
    expression l = EXPRESSION(CAR(CDR(args)));
    expression u = EXPRESSION(CAR(CDR(CDR(args))));

    pips_debug(5, "begin\n");

    if (! syntax_reference_p(expression_syntax(expr)))
            pips_error("substring_effect", "not a reference\n");

    le = generic_proper_effects_of_expression(expr);
    le = gen_nconc(le, generic_proper_effects_of_expression(l));
    le = gen_nconc(le, generic_proper_effects_of_expression(u));

    pips_debug(5, "end\n");

    return(le);
}

static IoElementDescriptor*
SearchIoElement(char *s, char *i)
{
    IoElementDescriptor *p = IoElementDescriptorTable;

    while (p->StmtName != NULL) {
        if (strcmp(p->StmtName, s) == 0 && strcmp(p->IoElementName, i) == 0)
                return(p);
        p += 1;
    }

    pips_error("SearchIoElement", "unknown io element %s %s\n", s, i);
    /* Never reaches this point. Only to avoid a warning at compile time. BC. */
    return(&IoElementDescriptorUndefined);
}

static list
some_io_effects(entity e __attribute__ ((__unused__)), list args __attribute__ ((__unused__)))
{
    /* Fortran standard deliberately does not define the exact output
       device of a PAUSE or STOP statement. See Page B-6 in ANSI X3.9-1978
       FORTRAN 77. We assume a WRITE on stderr, i.e. unit 0 on UNIX, if
       one argument is available or not. */
    list le = NIL;
    entity private_io_entity;
    reference ref;
    list indices = NIL;

    indices = CONS(EXPRESSION,
		   int_to_expression(STDERR_LUN),
		   NIL);

    private_io_entity = global_name_to_entity
      (IO_EFFECTS_PACKAGE_NAME,
       IO_EFFECTS_ARRAY_NAME);

    pips_assert("io_effects", private_io_entity != entity_undefined);

    ref = make_reference(private_io_entity,indices);
    le = gen_nconc(le, generic_proper_effects_of_reference(ref));
    le = gen_nconc(le, generic_proper_effects_of_lhs(ref));

    return le;
}

static list read_io_effects(entity e, list args)
{
  /* READ is a special case , for example 
     
     N = 5
     READ *,N, (T(I),I=1,N) 

     There is write effect on N in the READ statement, so we have 
     to project N from the proper region of the READ statement, then add
     the precondition after.

     The correct region in this case is :
     
     <T(PHI1)-MAY-{1 <= PHI1, N==5} >*/
     

  /*  get standard io effects 

      Attention : in this list of io_effects, there are already 
      preconditions  so the too late application of reverse transformer
      still gives the false result !!!

      io_effects => generic_proper_effects_of_expression => ....
      effects_precondition_composition_op

 */

  list le = io_effects(e, args);

  /* get current transformer */
  statement s = effects_private_current_stmt_head();
  transformer t = (*load_transformer_func)(s);

  if (!transformer_undefined_p(t)) /* hummm... */
  {
    /* reverse-apply transformer to le. */
    le = (*effects_transformer_composition_op)(le, t); 
  }

  return le;
}

static list
io_effects(entity e, list args)
{
    list le = NIL, pc, lep;

    pips_debug(5, "begin\n");

    for (pc = args; pc != NIL; pc = CDR(pc)) {
	IoElementDescriptor *p;
	entity ci;
        syntax s = expression_syntax(EXPRESSION(CAR(pc)));

        pips_assert("io_effects", syntax_call_p(s));

	ci = call_function(syntax_call(s));
	p = SearchIoElement(entity_local_name(e), entity_local_name(ci));

	pc = CDR(pc);

	if (strcmp(p->IoElementName, "IOLIST=") == 0) {
	    lep = effects_of_iolist(pc, p->ReadOrWrite);
	}
	else {
	    lep = effects_of_ioelem(EXPRESSION(CAR(pc)), 
				    p->ReadOrWrite);
	}

	if (p->MayOrMust == is_approximation_may)        
	    effects_to_may_effects(lep);

	le = gen_nconc(le, lep);

	/* effects effects on logical units - taken from effects/io.c */
	if ((get_bool_property ("PRETTYPRINT_IO_EFFECTS")) &&
	    (pc != NIL) &&
	    (strcmp(p->IoElementName, "UNIT=") == 0))
	{
	    /* We simulate actions on files by read/write actions
	       to a static integer array 
	       GO:
	       It is necessary to do a read and and write action to
	       the array, because it updates the file-pointer so
	       it reads it and then writes it ...*/
	    entity private_io_entity;
	    reference ref;
	    list indices = NIL;
	    expression unit = EXPRESSION(CAR(pc));

	    if(expression_list_directed_p(unit)) {
		if(same_string_p(entity_local_name(e), READ_FUNCTION_NAME))
		    unit = int_to_expression(STDIN_LUN);
		else if(same_string_p(entity_local_name(e), WRITE_FUNCTION_NAME))
		    unit = int_to_expression(STDOUT_LUN);
		else
		    pips_error("io_effects", "Which logical unit?\n");
	    }

	    indices = gen_nconc(indices, CONS(EXPRESSION, unit, NIL));

	    private_io_entity = global_name_to_entity
		(IO_EFFECTS_PACKAGE_NAME,
		 IO_EFFECTS_ARRAY_NAME);

	    pips_assert("io_effects", private_io_entity != entity_undefined);

	    ref = make_reference(private_io_entity, indices);
	    le = gen_nconc(le, generic_proper_effects_of_reference(ref));
	    le = gen_nconc(le, generic_proper_effects_of_lhs(ref));
	}	
    }

    pips_debug(5, "end\n");

    return(le);
}    

static list
effects_of_ioelem(expression exp, tag act)
{   
    list lr;

    pips_debug(5, "begin\n");
    if (act == is_action_write)
    {
	syntax s = expression_syntax(exp);

	pips_debug(6, "is_action_write\n");
	pips_assert("effects_of_ioelem", syntax_reference_p(s));

	lr = generic_proper_effects_of_lhs(syntax_reference(s));
    }
    else
    {  
	debug(6, "effects_of_io_elem", "is_action_read\n");  
	lr = generic_proper_effects_of_expression(exp);
    }   
 
    pips_debug(5, "end\n");
    return(lr);
}

static list
effects_of_iolist(list exprs, tag act)
{
    list lep = NIL;
    expression exp = EXPRESSION(CAR(exprs));

    pips_debug(5, "begin\n");

    if (expression_implied_do_p(exp)) 
	lep = effects_of_implied_do(exp, act);
    else
    {
	if (act == is_action_write)
	{
	    syntax s = expression_syntax(exp);

	    pips_debug(6, "is_action_write");
	    /* pips_assert("effects_of_iolist", syntax_reference_p(s)); */
	    if(syntax_reference_p(s))
	      lep = generic_proper_effects_of_lhs(syntax_reference(s));
	    else
	    {
	      /* write action on a substring */
	      if(syntax_call_p(s) &&
		 strcmp(entity_local_name(call_function(syntax_call(s))),
			SUBSTRING_FUNCTION_NAME) == 0 )
	      {
		expression e = EXPRESSION(CAR(call_arguments(syntax_call(s))));
		expression l = EXPRESSION(CAR(CDR(call_arguments(syntax_call(s)))));
		expression u = EXPRESSION(CAR(CDR(CDR(call_arguments(syntax_call(s))))));

		lep = generic_proper_effects_of_lhs
		    (syntax_reference(expression_syntax(e)));
		lep = gen_nconc(lep, generic_proper_effects_of_expression(l));
		lep = gen_nconc(lep, generic_proper_effects_of_expression(u));
	      }
	      else {
		pips_internal_error("Impossible memory write effect!");
	      }
	    }
	}
	else {	
	    pips_debug(6, "is_action_read");
	    lep = generic_proper_effects_of_expression(exp);
	}
    }

    pips_debug(5, "end\n");

    return lep;
}

/* an implied do is a call to an intrinsic function named IMPLIED-DO;
 * its first argument is the loop index, the second one is a range, and the
 * remaining ones are expressions to be written or references to be read,
 * or another implied_do (BA).
 */

static list
effects_of_implied_do(expression exp, tag act)
{
    list le, lep, lr, args;
    expression arg1, arg2;
    entity index;
    range r;
    reference ref;
    transformer context = effects_private_current_context_head();
    transformer local_context = transformer_undefined;

    pips_assert("effects_of_implied_do", expression_implied_do_p(exp));

    pips_debug(5, "begin\n");

    args = call_arguments(syntax_call(expression_syntax(exp)));
    arg1 = EXPRESSION(CAR(args));       /* loop index */
    arg2 = EXPRESSION(CAR(CDR(args)));  /* range */
    
    pips_assert("effects_of_implied_do", 
		syntax_reference_p(expression_syntax(arg1)));

    pips_assert("effects_of_implied_do", 
		syntax_range_p(expression_syntax(arg2)));

    index = reference_variable(syntax_reference(expression_syntax(arg1)));
    ref = make_reference(index, NIL);

    r = syntax_range(expression_syntax(arg2));

    /* effects of implied do index 
     * it is must_written but may read because the implied loop 
     * might execute no iteration. 
     */

    le = generic_proper_effects_of_lhs(ref); /* the loop index is must-written */
    /* Read effects are masked by the first write to the implied-do loop variable */
	
    /* effects of implied-loop bounds and increment */
    le = gen_nconc(le, generic_proper_effects_of_expression(arg2));

    /* Do we use context information */
    if (! transformer_undefined_p(context))
    {    
	transformer tmp_trans;
	Psysteme context_sc;

    /* the preconditions of the current statement don't include those
     * induced by the implied_do, because they are local to the statement.
     * But we need them to properly calculate the regions.
     * the solution is to add to the current context the preconditions 
     * due to the current implied_do (think of nested implied_do).
     * Beware: the implied-do index variable may already appear 
     * in the preconditions. So we have to eliminate it first.
     * the regions are calculated, and projected along the index.
     * BA, September 27, 1993.
     */

	local_context = transformer_dup(context);
	/* we first eliminate the implied-do index variable */
	context_sc = predicate_system(transformer_relation(local_context));
	if(base_contains_variable_p(context_sc->base, (Variable) index))
	{
	    sc_and_base_projection_along_variable_ofl_ctrl(&context_sc,
							   (Variable) index,
							   NO_OFL_CTRL);
	    predicate_system_(transformer_relation(local_context)) =
		newgen_Psysteme(context_sc);
	}
	/* tmp_trans simulates the transformer of the implied-do loop body */
	tmp_trans = transformer_identity();
	local_context = add_index_range_conditions(local_context, index, r, 
						   tmp_trans);
	free_transformer(tmp_trans);
	transformer_arguments(local_context) = 
	    arguments_add_entity(transformer_arguments(local_context), 
				 entity_to_new_value(index)); 


	ifdebug(7) {
	    pips_debug(7, "local context : \n%s\n", 
		       precondition_to_string(local_context));
	}	
    }
    else
	local_context = transformer_undefined;

    effects_private_current_context_push(local_context);
    
    MAP(EXPRESSION, expr, 
    { 
      syntax s = expression_syntax(expr);
      
      if (syntax_reference_p(s))
	if (act == is_action_write) 
	  lep = generic_proper_effects_of_lhs(syntax_reference(s));
	else
	  lep = generic_proper_effects_of_expression(expr);
      else
	if (syntax_range_p(s))
	  lep = generic_proper_effects_of_range(syntax_range(s));
	else
	  /* syntax_call_p(s) is true here */
	  if (expression_implied_do_p(expr))
	    lep = effects_of_implied_do(expr, act);
	  else
	    lep = generic_r_proper_effects_of_call(syntax_call(s));
      
      /* indices are removed from effects because this is a loop */
      lr = NIL;
      MAP(EFFECT, eff,
      {
	if (effect_entity(eff) != index)
	  lr =  CONS(EFFECT, eff, lr);
	else if(act==is_action_write /* This is a read */
		&& action_write_p(effect_action(eff))) {
	  pips_user_error("Index %s in implied DO is read. "
			  "Standard violation, see Section 12.8.2.3\n",
			  entity_local_name(index));
	}
	else
	{
	  debug(5, "effects_of_implied_do", "index removed");
	  free_effect(eff);
	}
      }, lep);
      gen_free_list(lep);
      lr = gen_nreverse(lr); /* preserve initial order??? */
      le = gen_nconc(le, lr);	    
    }, CDR(CDR(args)));
    
    
    (*effects_union_over_range_op)(le, 
				   index, 
				   r, descriptor_undefined);
    
    ifdebug(6) {
      pips_debug(6, "effects:\n");
      (*effects_prettyprint_func)(le);
      fprintf(stderr, "\n");
    }
    
    effects_private_current_context_pop();
    pips_debug(5, "end\n");
    
    return le;
}

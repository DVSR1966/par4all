/*{{{  defines*/
/* classifying subscript types for reference template 
 * done in newgen now
 */
#define LIN_INVARIANT		2
#define LIN_VARIANT			1
#define NON_LINEAR		 -1

/* accessing DAD components */
#define LSEC(x,i)	 	GetBoundary(x,i,1)
#define USEC(x,i)	 	GetBoundary(x,i,0)
#define PUT_NEST(d,val)         context_info_nest(simple_section_context(comp_sec_hull(comp_desc_section(d)))) = val
#define GET_NEST(d)         context_info_nest(simple_section_context(comp_sec_hull(comp_desc_section(d))))

/*}}}*/

enum BoundType {LOWER, UPPER};
enum RefType {READ, WRITE};
enum NestType {ZERO, SINGLE, MULTI};

/* used for merging linear expressions */
typedef enum { PLUS, MINUS} OpFlag;

/* only used for in print routines. The analysis does not
 * pose any limit on the number of array dimensions
 */
#define MAX_RANK 8

/*{{{  Dad definition*/
  /* data structures for data access descriptor */
  
  /* Reference Template part of DAD, an array of integers allocated dynamically */
  typedef unsigned int tRT;

  /*  A linear expression in Pips ; Pvecteur is a pointer */
#define LinExpr Pvecteur

  /* bounds are retained as high level tree structures to accommodate symbolic
     information in boundary expressions. When all the symbolic information
     gets resolved then the tree nodes are collapsed into a single instruction
     holding the constant value  
  */

  typedef struct sSimpBound 
  {
    LinExpr lb; /* lower bound */
    LinExpr ub; /* upper bound */
  } tSS;

  /* Simple Section part of DAD
   An array of type SimpBound struct allocated 
   dynamically based on rank of array
  */

  typedef struct DadComponent 
  {
    tRT  *RefTemp;
    tSS  *SimpSec;
  } DadComp;


/*}}}*/
/*{{{  Data structures required for computing Dads*/

/*{{{  structures for TranslateToLoop*/
/* structure to hold both Old and New variants */
typedef struct Variants  {
  list Old;
  list New;
}tVariants;

/*}}}*/

/*}}}*/

typedef simple_section tDad;


#define SEQUENTIAL_COMPSEC_SUFFIX ".csec"
#define USER_COMPSEC_SUFFIX ".ucsec"


/*#define VERBOSE     */
//  #define DEBUG   
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dludcmp.c"
//#include "dlubksb.c"
#include "svbksb.c" 
/*#include "svdcmp_mod.c" */
/*#include "svdcmp.c"   */
/*#include "mysvdcmp.c"  */
#include "brent.c" 
#include "nrutil.h"
#include "pythag.c"
#include "svdcmp.c"
#include "dpythag.c"
#include "dsvdcmp.c" 
#include "dsvdcmp_var.c"

#include "matrix_util.h"
#include "share.h"
#include "global.h"
#include "platform.h"


void dlubksb(double **a, int n, int *indx, double b[])
{
	int i,ii=0,ip,j;
	double sum;

	for (i=1;i<=n;i++) {
		ip=indx[i];
		sum=b[ip];
		b[ip]=b[i];
		if (ii)
			for (j=ii;j<=i-1;j++) sum -= a[i][j]*b[j];
		else if (sum) ii=i;
		b[i]=sum;
	}
	for (i=n;i>=1;i--) {
		sum=b[i];
		for (j=i+1;j<=n;j++) sum -= a[i][j]*b[j];
		b[i]=sum/a[i][i];
	}
}


#define ALMOST_ZERO .001


void myexit(int i) { exit(0); /*printf("exit here\n"); */}

#include "belief.h"
//#define NEW_ALLOC  // allocate both permanent and temporary memory as needed, rather than having a fixed buffer size

#ifdef NEW_ALLOC   // ------------------------------------------------------------------------------

#define MATRIX_MEM_CHUNK  1000000  // will allocate in units this size
#define MAX_CHUNKS 50

typedef struct {
  int num;  // number of allocated chunks
  int cur;  // the current chunk we are using
  int offset;  // how far into the current chunk
  char *buff[MAX_CHUNKS];  // pointers to the actual memory
} MATRIX_MEM_STRUCT;


static MATRIX_MEM_STRUCT chunk;
static MATRIX_MEM_STRUCT pchunk;

static void
cmb(MATRIX_MEM_STRUCT *mms) {
  mms->offset = mms->cur = 0;
}

void clear_matrix_buff(void) {  // the currently allocated chunks remain available
  cmb(&chunk);
}


static void
amb(MATRIX_MEM_STRUCT *mms) {
  mms->buff[0] = malloc(MATRIX_MEM_CHUNK);
  mms->num = 1;
  cmb(mms);
}


void
alloc_matrix_buff() {
  amb(&chunk);
}


static char*
hiadd(MATRIX_MEM_STRUCT *mms) {
  return(mms->buff[mms->cur] + MATRIX_MEM_CHUNK);
}

char* highest_address(void) {
  return(hiadd(&chunk));
}

static char *
allsp(MATRIX_MEM_STRUCT *mms, int n) {
  char *ptr;

  if (n > MATRIX_MEM_CHUNK) { printf("cannot allocate %d\n",n); exit(0); }
  if (n + mms->offset > MATRIX_MEM_CHUNK) {
    if (mms->cur + 1 == mms->num) { 
      if (mms->num >= MAX_CHUNKS) { printf("already at maximum chunk num\n"); exit(0); }
      mms->buff[mms->num++] = malloc(MATRIX_MEM_CHUNK);
      printf("alloced %d chunks\n",mms->num);
    }
    mms->cur++;
    mms->offset = 0;
  }
  ptr = mms->buff[mms->cur] + mms->offset;
  mms->offset += n;
  //  printf("ptr = %d offset = %d n = %d\n",ptr,mms->offset,n);
  return(ptr);
}

static char *
alloc_space(int n) {
  char *ptr;

  return(allsp(&chunk,n));
}

void
alloc_perm_matrix_buff() {
  amb(&pchunk);
}


void
free_perm_space() {
  cmb(&pchunk);
}

static char *
perm_alloc_space(int n) {
  return(allsp(&pchunk,n));
}

#else // ----------------------------------------------------------

static char *matrix_buff;
static int matrix_water_level=0;
static char *perm_matrix_buff;
static int perm_matrix_water_level=0;

#define MATRIX_BUFF_SIZE 12000000 // 8000000  /*4000000  */

#ifdef ROMEO_JULIET
  #define PERM_MATRIX_BUFF_SIZE  30000000 //20000000//15000000//12000000//8000000 /*2000000*/ 
#else
  #define PERM_MATRIX_BUFF_SIZE  20000000//15000000//12000000//8000000 /*2000000*/ 
#endif


void
alloc_matrix_buff() {
  matrix_buff = malloc(MATRIX_BUFF_SIZE);
  if (matrix_buff == NULL) { printf("falied in alloc_matrix_buff\n"); exit(0); }
}



void clear_matrix_buff(void) {
  /*   printf("clearing matrix buf: used %d bytes\n",matrix_water_level);  */
  /*  printf("aborting clear_matrix_buff()\n");
  return; */
  if (thread_nesting) {
    /*    printf("timer interrupt in progress.  can't clear buffer\n");*/
    return;
  }
  matrix_water_level = 0;
}


char* highest_address(void) {
  return(matrix_buff + MATRIX_BUFF_SIZE);
}

static char *
alloc_space(int n) {
  char *ptr;

  ptr = matrix_buff + matrix_water_level;
  matrix_water_level += n;
  if (matrix_water_level >= MATRIX_BUFF_SIZE) {
    printf("out of room in alloc_float\n");
    exit(0);
  }
  return(ptr);
}
  
void
alloc_perm_matrix_buff() {
  perm_matrix_buff = malloc(PERM_MATRIX_BUFF_SIZE);
  if (perm_matrix_buff == NULL) { printf("falied in alloc_perm_matrix_buff\n"); exit(0); }
}

void
free_perm_space() {
  perm_matrix_water_level=0;
}

static char *
perm_alloc_space(int n) {
  char *ptr;

  ptr = perm_matrix_buff + perm_matrix_water_level;
  perm_matrix_water_level += n;
  if (perm_matrix_water_level >= PERM_MATRIX_BUFF_SIZE) {
    printf("out of room in perm_alloc_float\n");
    exit(0);
  }
  return(ptr);
}

#endif // --------------------------------------------------------------------



  


//CF:  memory allocation for arbitrarily sized chunks
//CF:  (temporary storage; see also permanent function, Mperm_alloc)







MATRIX Malloc(int n, int m)
{     
  /*  double **matrix(int nrl, int nrh, int ncl, int nch);*/
  MATRIX mat;
  int i;

  mat.rows = n;
  mat.cols = m;
  mat.el = (FLOAT_DOUBLE **) alloc_space(n*sizeof(FLOAT_DOUBLE *));
  for (i=0; i < n; i++) mat.el[i] = (FLOAT_DOUBLE *) alloc_space(m*sizeof(FLOAT_DOUBLE));
  /*  mat.el = matrix(0,n-1,0,m-1); */
  return(mat);
}

SUPERMAT
Salloc(int r, int c) {
  SUPERMAT temp;
  int i,j;
  
  temp.rows = r;
  temp.cols = c;
  temp.sub = (MATRIX **) alloc_space(r*sizeof(MATRIX*));
  for (i=0; i < r; i++) temp.sub[i] = (MATRIX *) alloc_space(c*sizeof(MATRIX));
  return(temp);
}
    

//CF:  allocate permanent storage for matrix
MATRIX 
Mperm_alloc(int n, int m)  /* memory never reclaimed */
{     
  /*  double **matrix(int nrl, int nrh, int ncl, int nch);*/
  MATRIX mat;
  int i,d,max;

  mat.rows = n;
  mat.cols = m;
  /* mat.el = matrix(0,n-1,0,m-1);   */
  max = (n > m) ? n : m;
  mat.el = (FLOAT_DOUBLE **) malloc(max*sizeof(FLOAT_DOUBLE *));   
  for (i=0; i < max; i++) mat.el[i] = (FLOAT_DOUBLE *) malloc(max*sizeof(FLOAT_DOUBLE));  
  return(mat);
}

//CF:  copy matrix from temp buffer to newly allocated perm storage
MATRIX
Mperm(MATRIX m) {
  int i,j;
  MATRIX temp;

  temp = Mperm_alloc(m.rows,m.cols);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    temp.el[i][j] = m.el[i][j];
  return(temp);
}


//static char perm_matrix_buff[PERM_MATRIX_BUFF_SIZE];


MATRIX 
Mperm_alloc_id(int n, int m)  /* memory never reclaimed */
{     
  /*  double **matrix(int nrl, int nrh, int ncl, int nch);*/
  MATRIX mat;
  int i,d,max;


  mat.rows = n;
  mat.cols = m;
  mat.el = (FLOAT_DOUBLE **) perm_alloc_space(n*sizeof(FLOAT_DOUBLE *));   
  for (i=0; i < n; i++) mat.el[i] = (FLOAT_DOUBLE *) perm_alloc_space(m*sizeof(FLOAT_DOUBLE));  
  return(mat);
}



MATRIX
Mperm_id(MATRIX m) {
  int i,j;
  MATRIX temp;

  temp = Mperm_alloc_id(m.rows,m.cols);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    temp.el[i][j] = m.el[i][j];
  return(temp);
}


void Mfree(MATRIX m) {
  
}

void
Mset_zero(MATRIX m) {
  int i,j;

  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    m.el[i][j] = 0;
}

MATRIX
Munset(void) {
  int i,j;
  MATRIX temp;

  temp.rows = temp.cols = 0;
  temp.el = NULL;
  return(temp);
}

void
Mset_iden(MATRIX m) {
  int i,j;

  if (m.rows != m.cols) {
    printf("rows != cols in Mset_iden\n");
    myexit(0);
  }
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    m.el[i][j] = (i == j) ? 1 : 0;
}


void
Mcopymat(MATRIX from, MATRIX *to) {
  /* we assume that "to" has space allocated to accomodate "from" */
  int i,j;

  if (from.el == NULL) {
    /*    *to = Munset(); */
    printf("from matrix is unset\n");
    myexit(0);
  }
  to->rows = from.rows;
  to->cols = from.cols;
  if (from.rows == 0 && from.cols == 0) {
    printf("trying to copy an empty matrix\n");
    myexit(0);
  }
  for (i=0; i < from.rows; i++) {
    for (j=0; j < from.cols; j++) {
      (to->el)[i][j] = from.el[i][j];
    }
  }
}

MATRIX 
Mmt(MATRIX m1, MATRIX m2)  /* matrix multiply */
              {
  MATRIX temp;
  int i,j,k;
  FLOAT_DOUBLE *acc,*row1,*rowstart,*row2,error;

  if (m1.cols != m2.cols) { 
    printf("matrices incompatible in Mmt");  
    myexit(0); 
  }
  temp = Malloc(m1.rows,m2.rows);
  for (i=0; i < temp.rows; i++) {
    acc = temp.el[i];
    rowstart = m1.el[i];
    for (j=0; j < temp.cols; j++) {
      row1 = rowstart;
      row2 = m2.el[j];
      *acc = 0;
      for (k=0; k < m1.cols; k++) *acc += *row1++ * *row2++;
      acc++;
    }
  }
  error = Mnorm(Ms(temp,Mm(m1,Mt(m2))));
  if (error > .01) {
    printf("Mmt doesn't work\n");
    exit(0);
  }
  return(temp);
}

MATRIX
Mconvert(MX om) { /* old style to new style */
  MATRIX temp;
  int i,j;
  temp = Malloc(om.rows,om.cols);
  for (i=0; i < temp.rows; i++)
    for (j=0; j < temp.cols; j++)
      temp.el[i][j] = om.el[i][j];
  return(temp);
}
  
  

MATRIX 
Mm(MATRIX m1, MATRIX m2)  {  /* matrix multiply */
  MATRIX temp;
  int i,j,k,dim;
  FLOAT_DOUBLE *acc,*row,*rowstart;


  if (m1.cols != m2.rows) { 
    printf("matrices incompatible in Mm");  
    myexit(0); 
  }
  dim = m1.cols;
  temp = Malloc(m1.rows,m2.cols);
  for (i=0; i < temp.rows; i++) {
      acc = temp.el[i];
      rowstart = m1.el[i];
      for (j=0; j < temp.cols; j++) {
	row = rowstart;
	*acc = 0;
	for (k=0; k < dim; k++) *acc += *row++ * m2.el[k][j];
	acc++;
      }
  }
  return(temp);
}


void 
Mma(MATRIX m1, MATRIX m2, MATRIX *temp)  {  /* matrix multiply */
  int i,j,k,dim;
  FLOAT_DOUBLE *acc,*row,*rowstart;


  if (m1.cols != m2.rows) { 
    printf("m1.cols = %d m2.rows = %d\n",m1.rows,m2.cols);
    printf("matrices incompatible in Mma");  
    myexit(0); 
  }
  temp->rows = m1.rows;
  temp->cols = m2.cols;
  dim = m1.cols;
  for (i=0; i < temp->rows; i++) {
      acc = temp->el[i];
      rowstart = m1.el[i];
      for (j=0; j < temp->cols; j++) {
	row = rowstart;
	*acc = 0;
	for (k=0; k < dim; k++) *acc += *row++ * m2.el[k][j];
	acc++;
      }
  }
}





MATRIX 
Ma(MATRIX m1, MATRIX m2)   /* add matrices */
              {
  MATRIX temp;
  int i,j,k;

  if (m1.cols != m2.cols || m1.rows != m2.rows) { 
    printf("matrices incompatible in ma\n");  myexit(0); 
  }
  temp = Malloc(m1.rows,m2.cols);
  for (i=0; i < temp.rows; i++) for (j=0; j < temp.cols; j++) 
    temp.el[i][j] = m1.el[i][j] + m2.el[i][j];
  return(temp);
}

MATRIX 
Mdiag_invert(MATRIX m)  { /* add matrices */
  MATRIX temp;
  int i,j,k;

  if (m.cols != m.rows) {
    printf("matrix not diagnoal\n"); 
    myexit(0); 
  }
  temp = Mzeros(m.rows,m.cols);
  for (i=0; i < temp.rows; i++) 
    if  (m.el[i][i] == 0) temp.el[i][i] =  HUGE_VAL;
    else if  (m.el[i][i] == HUGE_VAL) temp.el[i][i] =  0;
    else temp.el[i][i] =  1. / m.el[i][i];
  return(temp);
}

MATRIX 
Mdiag_neg(MATRIX m)  { /* negative of matrix */
  MATRIX temp;
  int i,j,k;

  if (m.cols != m.rows) {
    printf("matrix not diagnoal\n"); 
    myexit(0); 
  }
  temp = Mzeros(m.rows,m.cols);
  for (i=0; i < temp.rows; i++) 
    temp.el[i][i] =  -m.el[i][i];
  return(temp);
}

MATRIX 
Mcolumn(MATRIX m, int c)  { /* pulls out cth column */
  MATRIX temp;
  int i,j;

  if (c >= m.cols) {
    printf("column out of range\n"); 
    myexit(0); 
  }
  temp = Mzeros(m.rows,1);
  for (i=0; i < temp.rows; i++) 
    temp.el[i][j] = m.el[c][i];
  return(temp);
}

MATRIX 
Mcat(MATRIX m1, MATRIX m2)   /* concatenate horizontally matrices */
              {
  MATRIX temp;
  int i,j,k;

  if (m1.rows != m2.rows) { 
    printf("matrices incompatible in Mcat\n"); 
    myexit(0); 
  }
  temp = Malloc(m1.rows,m1.cols+m2.cols);
  for (i=0; i < temp.rows; i++) for (j=0; j < m1.cols; j++) 
    temp.el[i][j] = m1.el[i][j];
  for (i=0; i < temp.rows; i++) for (j=0; j < m2.cols; j++) 
    temp.el[i][j+m1.cols] = m2.el[i][j];
  return(temp);
}

MATRIX 
Mvcat(MATRIX m1, MATRIX m2)   /* concatenate  matrices  vertically */
              {
  MATRIX temp;
  int i,j,k;

  if (m1.cols != m2.cols) { 
    printf("matrices incompatible in Mvcat\n"); 
    myexit(0); 
  }
  temp = Malloc(m1.rows+m2.rows,m1.cols);
  for (i=0; i < m1.rows; i++) for (j=0; j < m1.cols; j++) 
    temp.el[i][j] = m1.el[i][j];
  for (i=0; i < m2.rows; i++) for (j=0; j < m2.cols; j++) 
    temp.el[i+m1.rows][j] = m2.el[i][j];
  return(temp);
}

MATRIX 
Mdcat(MATRIX m1, MATRIX m2)   /* concatenate diagonal matrices */
              {
  MATRIX temp;
  int i,j,k;

  if (m1.rows != m1.cols || m2.rows != m2.cols) { 
    printf("matrices incompatible in Mdcat\n"); 
    myexit(0); 
  }
  temp = Mzeros(m1.rows+m2.rows,m1.cols+m2.cols);
  for (i=0; i < m1.rows; i++) temp.el[i][i] = m1.el[i][i];
  for (i=0; i < m2.rows; i++) temp.el[i+m1.rows][i+m1.rows] = m2.el[i][i];
  return(temp);
}

MATRIX 
Mdirect_sum(MATRIX m1, MATRIX m2)   /* m1 and m2 are orthogonal bases. 
				       return orthog basis for direct sum */
              {
  MATRIX temp;
  int a,b,i,j,k=0;

  temp = Malloc(m1.rows+m2.rows,m1.cols*m2.cols);
  for (i=0; i < m1.cols; i++)   for (j=0; j < m2.cols; j++) {
    for (a = 0; a < m1.rows; a++) temp.el[a][k] = m1.el[a][i];
    for (a = 0; a < m2.rows; a++) temp.el[m1.rows+a][k] = m2.el[a][j];
    k++;
  }
  return(temp);
}



FLOAT_DOUBLE 
Mnormsq(MATRIX m)  /* norm square */ 
{
  FLOAT_DOUBLE total;
  int i,j;
  
  total = 0;
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    total += m.el[i][j]*m.el[i][j];
  return(total);
}


FLOAT_DOUBLE 
Mnorm(MATRIX m)  
{
  FLOAT_DOUBLE total;
  int i,j;
  
  total = 0;
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    total += m.el[i][j]*m.el[i][j];
  /*  total /= (m.rows*m.cols); */
  return(sqrt(total));
}





MATRIX 
Morthog(MATRIX U, MATRIX V) { 
/* return an ortho basis for the orth comp of R(U) in R(V). it is assumed 
that R(U) in R(V) */
  MATRIX null,comp,diag,vortho,t1,t2;
  FLOAT_DOUBLE error;

  if (U.rows != V.rows) {
    printf("matrices disagree\n"); 
    myexit(0); 
  }
  if (V.cols == 0) return(Mzeros(V.rows,V.cols));
  Mnull_decomp(Mt(V),&vortho,&t1); 
  /* vortho is orthogonal complement of V since N(A') = ortho(R(A))  */
#ifdef DEBUG  
  error = Mnorm(Mm(Mt(U),vortho));  
  if (error > .01) {
    printf("U not contained in V in Morthog\n");
    printf("U = \n");
    Mp(U);
    printf("V = \n");
    Mp(V);
    myexit(0); 
  }
#endif
  if (U.cols == V.cols) return(Mzeros(U.rows,0));
  Mnull_decomp(Mm(Mt(U),V),&null,&comp);
  return(Mm(V,null));
}

int
Mis_subspace(MATRIX U, MATRIX V) { 
/* is U a subspace of V? */
  MATRIX null,comp,diag,vortho,t1,t2;
  FLOAT_DOUBLE error;

  if (U.rows != V.rows) {
    printf("matrices disagree\n"); 
    myexit(0); 
  }
  if (U.cols == 0) return(1);
  if (V.cols == 0) return(0);
  Mnull_decomp(Mt(V),&vortho,&t1); 
  error = Mnorm(Mm(Mt(U),vortho));  
  return (error < .01);
}

MATRIX
Mcomp_intersect(MATRIX Uc, MATRIX Vc) { /* intesection of complements of 
two othogonal subspaces */      
  MATRIX m,ret,temp;

  m = Mvcat(Mt(Uc),Mt(Vc));

  Mnull_decomp(m,&ret,&temp); 
  return(ret);
}



MATRIX
Mintersect(MATRIX U, MATRIX V) { /* intesection of two othogonal subspaces */      
  MATRIX P,Q,R,ret,temp;

  
  P = Mm(U,Mt(U));
  Q = Mm(V,Mt(V));
  R = Ms(Miden(U.rows),Mm(P,Q));  /* I - PQ */
  Mnull_decomp(R,&ret,&temp); 
  return(ret);
}

void
Minter(MATRIX U, MATRIX V, MATRIX *inter, MATRIX *comp) { 
  /* intesection of two othogonal subspaces */      
  /* inter + comp spans while space */
  MATRIX P,Q,R,T,ret,temp;

  if (U.cols == 0 || V.cols == 0) {
    *inter = Mzeros(U.rows,0);
    *comp = Miden(U.rows);
    return;
  }
  P = Mm(U,Mt(U));
  Q = Mm(V,Mt(V));
  /*  Mp(Q);
  Mp(P);
  Mp(Mm(P,Q)); */
  R = Ms(Miden(U.rows),Mm(P,Q));  /* I - PQ */
  Mnull_decomp(R,inter,comp); 
}


int
same_space(MATRIX m1, MATRIX m2) {
  MATRIX t,P1,P2;
  FLOAT_DOUBLE error;
  
  P1 = Mm(m1,Mt(m1));
  P2 = Mm(m2,Mt(m2));
  error = Mnorm(Ms(P1,P2));
  if (error > .01) {
    printf("matrices (1) do not represent same spaces (error = %f)\n",error);
    Mp(m1);
    Mp(m2);
    return(0);
  }

  t = Mm(Mt(m1),m2);
  error = Mnorm(Ms(m2,Mm(m1,t)));
  if (error > .01) {
    printf("matrices (2) do not represent same spaces (error = %f)\n",error);
    Mp(m1);
    Mp(m2);
    return(0);
  }
  t = Mm(Mt(m2),m1);
  error = Mnorm(Ms(m1,Mm(m2,t)));
  if (error > .01) {
    printf("matrices (3) do not represent same spaces (error = %f)\n",error);
    Mp(m1);
    Mp(m2);
    return(0);
  }
  return(1);
}

int
Mspaces_equal(MATRIX m1, MATRIX m2) {
  MATRIX t,P1,P2;
  FLOAT_DOUBLE error;
  
  P1 = Mm(m1,Mt(m1));
  P2 = Mm(m2,Mt(m2));
  error = Mnorm(Ms(P1,P2));
  return(error < .001);
}



void
Mint_comp(MATRIX U, MATRIX V, MATRIX *intersect, MATRIX *int_comp) { 
/* U and V are orthogonal spaces.
   returns an ortho basis for intersection of U and V and orthogonal
   complement of V in U. thus U is decomposed into two orthogonal
   subspaces */

  MATRIX null,comp,diag,vortho,t1,t2,prod,m,uv;
  FLOAT_DOUBLE error;
  int same,i;

  if (U.rows != V.rows) {
    printf("matrices disagree in Mint_comp\n"); 
    myexit(0); 
  }
  if (V.cols == 0) {
    *intersect = V;
    *int_comp = U;
    return;
  }
  if (U.cols == 0) {
    *intersect = U;
    *int_comp = U;
    return;
  }
  if (U.cols == 1 && V.cols == 1) {
    same = (Mnorm(Ms(U,V)) < ALMOST_ZERO || Mnorm(Ma(U,V)) < ALMOST_ZERO);
    *intersect = (same) ? U : Mzeros(U.rows,0);
    *int_comp  = (same) ? Mzeros(U.rows,0) : U;
    return;
  }
  prod = Mm(Mt(U),Mm(V,Mm(Mt(V),U))); 


  if (Mnorm(prod) < ALMOST_ZERO) {
    *int_comp = U;
    *intersect = Mzeros(U.rows,0);
    return;
  }

  /*  printf("11111111111\n");
Mp(Mm(V,Mt(V)));
printf("2\n");
Mp(Mm(U,Mt(U)));
printf("3\n");
Mp(prod);  */



  /*printf("before subtracting I\n");
Mp(prod); */

  for (i = 0; i < prod.rows; i++) prod.el[i][i] -= 1.;
  /*printf("after subtracting I\n");
Mp(prod); */
  /* prod = prod-I */
  /*uv = Mm(Mt(U),V);
uv = Mm(uv,Mt(uv)); */


  if (Mnorm(prod) < ALMOST_ZERO) {
    *int_comp = Mzeros(U.rows,0);
    *intersect = U;
    return;
  }
  /*printf("prod is \n");
Mp(prod); */

  Mnull_decomp(prod,&null,&comp);
  *intersect = Mm(U,null);
  *int_comp = Mm(U,comp);
  /*printf("4\n");
Mp(*intersect);
printf("5\n");
Mp(*int_comp);
printf("6\n");
Mp(null);
printf("7\n");
Mp(comp); */
}


MATRIX 
Mzeros(int r, int c)
{
  MATRIX temp;
  int i,j;

  temp = Malloc(r,c);
  for (i=0; i < r; i++) for (j=0; j < c; j++) temp.el[i][j] = 0;
  return(temp);
}

MATRIX 
Mleft_corner(int r, int c)
{
  MATRIX temp;
  int i,j;

  temp = Malloc(r,c);
  for (i=0; i < r; i++) for (j=0; j < c; j++) temp.el[i][j] = 0;
  temp.el[0][0] = 1;
  return(temp);
}

MATRIX 
Mpartial_iden(int r, int c)
{
  MATRIX temp;
  int i,j,min;

  temp = Malloc(r,c);
  for (i=0; i < r; i++) for (j=0; j < c; j++) temp.el[i][j] = 0;
  min = (r < c) ? r : c;
  for (i=0; i < min; i++) temp.el[i][i] = 1;
  return(temp);
}

MATRIX 
Mconst(int r, int c, FLOAT_DOUBLE v)
{
  MATRIX temp;
  int i,j;

  temp = Malloc(r,c);
  for (i=0; i < r; i++) for (j=0; j < c; j++) temp.el[i][j] = v;
  return(temp);
}

MATRIX 
Ms(MATRIX m1, MATRIX m2)  /* subtract matrices */ 
              {
  MATRIX temp;
  int i,j,k;

  if (m1.cols != m2.cols || m1.rows != m2.rows) { 
    printf("matrices incompatible in ms\n");  myexit(0); 
  }
  temp = Malloc(m1.rows,m1.cols);
  for (i=0; i < temp.rows; i++) for (j=0; j < temp.cols; j++) 
    temp.el[i][j] = m1.el[i][j] - m2.el[i][j];
  return(temp);
}

void
Mp(MATRIX m)
{
  int i,j;

 if (m.rows == 0) printf("matrix has 0 rows\n");
  if (m.cols == 0) printf("matrix has 0 cols\n");
printf("rows = %d cols = %d\n",m.rows,m.cols);
//if (m.cols > 100 || m.cols < 0) myexit(0);
//if (m.rows > 100 || m.rows < 0) myexit(0);
    for (i=0; i < m.rows; i++) {
    for (j=0; j < m.cols; j++) printf("%8.5f ",m.el[i][j]); 
    printf("\n");
  } 
  printf("\n");
}

MATRIX 
Mt(MATRIX m)   /* matrix transpose */
          {
  MATRIX temp;
  int i,j;

  temp = Malloc(m.cols,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[j][i] = m.el[i][j];
  return(temp);
}

MATRIX 
Mmissing(MATRIX perm) {
  MATRIX ret;
  int i,j,sum,k=0;

  ret = Mzeros(perm.rows,perm.rows-perm.cols);
  for (i=0; i < perm.rows; i++) {
    sum = 0;
    for (j=0; j < perm.cols; j++) sum += (int) perm.el[i][j];
    if (sum == 0) ret.el[i][k++] = 1;
  }
  return(ret);
}
static int
Mcmp(MATRIX m1, MATRIX m2) {
  int d,i,j;

  d = m1.rows-m2.rows;
  if (d) return(d);
  d = m1.cols-m2.cols;
  if (d) return(d);
  for (i=0; i < m1.rows; i++)
    for (j=0; j < m1.cols; j++) 
      if (m1.el[i][j] != m2.el[i][j]) {
	if (m1.el[i][j] > m2.el[i][j]) return(1);
	else return(-1);
      }
  return(0);
}


#define MAX_SVD_ITER 8

//static SVD_STRUCT svdstr;

//static int /*binary search */


/*lookup_decomp(MATRIX m,int *found) {
  int cmp,top,bot,mid;

  top = svdstr.num-1;
  bot = 0;
  while (bot <= top) {
    mid = (top + bot) >> 1;
    cmp = Mcmp(m,svdstr.decomp[mid].m);
    if (cmp == 0) { *found = 1; return(mid); }
    if (cmp > 0) bot = mid+1;
    else top = mid-1;
  }
  *found = 0;
  return(bot);
}

add_decomp(MATRIX m, MATRIX U, MATRIX D, MATRIX V, int loc) {
  int i;
  DECOMP_STR d;

    printf("adding loc %d\n",loc);
  if (svdstr.num == DECOMP_NUMBER) {
    printf("decomp table full\n");
    exit(0);
  }
  for (i = svdstr.num; i > loc; i--) svdstr.decomp[i] = svdstr.decomp[i-1];
  d.m = Mperm(m);
  d.U = Mperm(U);
  d.V = Mperm(V);
  d.D = Mperm(D);
  svdstr.decomp[loc] = d;
  svdstr.num++;
}

*/


void
Msvd(MATRIX m, MATRIX *U, MATRIX *D, MATRIX *V)   {
  double **a,*w,**v,**y,**x;
  FLOAT_DOUBLE error,len,max=0.,r;
  int i,j,k,zdim,cdim,nn,nc,rows,d,min,flag,fail,ii=0,jj=0,found,loc;
  MATRIX UU;
  

  /*  loc = lookup_decomp(m,&found);
  if (found) { 
    *U = svdstr.decomp[loc].U;
    *D = svdstr.decomp[loc].D;
    *V = svdstr.decomp[loc].V;
    return;
    } */


  if (m.cols == 0)  { 
    U->rows = m.rows;
    V->rows = V->cols = D->rows = D->cols = U->cols = 0; 
    return;
  }
  if (m.rows == 0) {
    U->rows = 0;
    U->cols = m.cols;
    *D = Mzeros(m.cols,m.cols);
    *V = Miden(m.cols);
    return;
  }
  if ((error = Mnorm(m)) < ALMOST_ZERO) { /*matrix is 0 matrix */
    *D = Mzeros(m.cols,m.cols);
    *V = Miden(m.cols);
    *U = Mzeros(m.rows,m.cols);
    min = (m.rows < m.cols) ? m.rows : m.cols;
    for (i=0; i < min; i++) U->el[i][i] = 1;
#ifdef VERBOSE    
    printf("this might be a bad idea (error = %f)\n",error);
#endif
    return;
  }
  if (m.cols == 1) {
    *V = Miden(1);
    len = Mnorm(m);
    *U = Msc(m,1./len);
    *D = Miden(1);
    D->el[0][0] = len;
    return;
  }
  /*  if (m.cols > m.rows) {
    d = m.cols - m.rows;
    m = Mvcat(m,Mzeros(d,m.cols));
  }  /* rows now >= cols */
  rows = (m.rows < m.cols) ? m.cols : m.rows;
  a = my_dmatrix(1,rows,1,m.cols);
  y = my_dmatrix(1,rows,1,m.cols);
  x = my_dmatrix(1,rows,1,m.cols);
  v = my_dmatrix(1,m.cols,1,m.cols);
  w = my_dvector(1,m.cols);
  /*printf("m is \n");
     Mp(m); */
     /*printf("rows = %d m.cols = %d m.rows = %d\n",rows,m.cols,m.rows); 
for (i=1; i <= rows; i++) for (j=1; j <= m.cols; j++) printf("%4.25f ",a[i][j]);
printf("\n");  
/*  a[1][1] += .0000001;  */

  *V = Malloc(m.cols,m.cols);
  *D = Mzeros(m.cols,m.cols);
  *U = Malloc(m.rows,m.cols);



  /*    printf("a is\n");
  for (i=1; i <= rows; i++)  {
    for (j=1; j <= m.cols; j++) printf("%7.3f ",a[i][j]);
    printf("\n");
  }  */


  for (k=0; k < MAX_SVD_ITER; k++) {
    for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = m.el[i][j]; 
    for (i = m.rows; i < rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = 0;  /* augment when m.rows < m.cols */
    fail = 0;
    dsvdcmp_var(a,rows,m.cols,w,v,&fail);
    ii = (ii+1)% rows;
    jj = (jj+1)% m.cols;

    if (fail)  m.el[ii][jj] += .000001;
    else break;
  }
#ifdef VERBOSE  
  if (k > 0) printf("%d extra iterations of dsvdcmp required\n",k);
#endif
  if (fail) {
    printf("didn't converge in %d tries at singular vaule decomp\n",MAX_SVD_ITER);
    Mp(m);
    exit(0);
  }





  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    U->el[i][j] = a[i+1][j+1];
  for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
    V->el[i][j] = v[i+1][j+1];
  for (i=0; i < m.cols; i++) if (w[i+1] > max) max = w[i+1];
  for (i=0; i < m.cols; i++) {
    r = w[i+1]/max;
    D->el[i][i] = (r  < .00001/* .00035*/) ? 0. : w[i+1];
#ifdef VERBOSE

        if (r < .1 && r > .00001) 
      printf("ratio = %f a semi-small singular value (%12.8f)\n",r,D->el[i][i]);

    /*           if (r < .000290 && r > .000288) 
		 exit(0);  */
    /*  if (r > 0. && r < .00001) {
      printf("a truncated singular value (%12.8f)\n",r);
      Mp(m);
      }*/
    
#endif
  }
#ifdef DEBUG
  error = Mnorm(Ms(Mm(*U,Mm(*D,Mt(*V))),m)); 
  if (error > .001 || isnanf(error)) {
    printf("problem in mysvdcmp() error = %f\n",error);
    printf("m is\n");
    Mp(m);
    printf("U is\n");
    Mp(*U);
    printf("D is\n");
    Mp(*D);
    printf("V is\n");
    Mp(*V);
    /*    printf("prod isn");
    Mp(Mm(*U,Mm(*D,Mt(*V))));
    UU = Malloc(m.cols,m.cols);
    for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
    UU.el[i][j] = a[i+1][j+1];
    printf("big U is\n");
    Mp(UU);
    printf("big prod is\n");
    Mp(Mm(UU,Mm(*D,Mt(*V))));
    printf("diff is\n");
    Mp(Ms(Mm(*U,Mm(*D,Mt(*V))),m)); 
   
 for (i=1; i <= rows; i++)  for (j=1; j <= m.cols; j++) y[i][j] = a[i][j]*w[j];
  for (i=1; i <= rows; i++)  for (j=1; j <= m.cols; j++) {
    x[i][j] = 0;
    for (k=1; k <= m.cols; k++) x[i][j] += y[i][k]*v[j][k];
  } 
  printf("prod is\n");
  for (i=1; i <= rows; i++)  {
    for (j=1; j <= m.cols; j++) printf("%7.3f ",x[i][j]);
    printf("\n");
  }
  printf("u is\n");
  for (i=1; i <= rows; i++)  {
    for (j=1; j <= m.cols; j++) printf("%7.3f ",a[i][j]);
    printf("\n");
  }
  printf("v is\n");
  for (i=1; i <= m.cols; i++)  {
    for (j=1; j <= m.cols; j++) printf("%7.3f ",v[i][j]);
    printf("\n");
  }
  printf("d is\n");
  for (i=1; i <= m.cols; i++)  {
    for (j=1; j <= m.cols; j++) printf("%7.3f ",(i == j) ? w[i] : 0.);
    printf("\n");
  }*/


    myexit(0); 

  }
#endif
  /*  else printf("svd error is %f\n",error);*/
  
  /*  printf("m = \n");
Mp(m); 
        printf("xxxxxxxxxxxxxsvd D is      \n");
for (i=0; i < m.cols; i++) printf("w[i] = %f w[i]/max = %f\n",w[i+1],w[i+1]/max);    */

   free_my_dmatrix(a,1,rows,1,m.cols);
   free_my_dmatrix(v,1,m.cols,1,m.cols);
  free_my_dvector(w,1,m.cols);


  /*  add_decomp(m,*U,*D,*V,loc);   */

}



void
Msvd_inv_in_place(MATRIX m)   {  /* matrices already alloced */
  double **a,*w,**v,**y,**x;
  FLOAT_DOUBLE error,len,max=0.,r;
  int i,j,k,zdim,cdim,nn,nc,rows,d,min,flag,fail,ii=0,jj=0,found,loc;
  MATRIX UU;
  

  /*  if (m.cols == 0)  { 
    U->rows = m.rows;
    V->rows = V->cols = D->rows = D->cols = U->cols = 0; 
    return;
    }*/

  if (m.rows == 0 || m.cols == 0) return;
  rows = (m.rows < m.cols) ? m.cols : m.rows;
  a = my_dmatrix(1,rows,1,m.cols);
  y = my_dmatrix(1,rows,1,m.cols);
  x = my_dmatrix(1,rows,1,m.cols);
  v = my_dmatrix(1,m.cols,1,m.cols);
  w = my_dvector(1,m.cols);

  for (k=0; k < MAX_SVD_ITER; k++) {
    for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = m.el[i][j]; 
    for (i = m.rows; i < rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = 0;  /* augment when m.rows < m.cols */
    fail = 0;
    dsvdcmp_var(a,rows,m.cols,w,v,&fail);
    ii = (ii+1)% rows;
    jj = (jj+1)% m.cols;

    if (fail)  m.el[ii][jj] += .000001;
    else break;
  }
  if (fail) {
    printf("didn't converge in %d tries at singular vaule decomp\n",MAX_SVD_ITER);
    Mp(m);
    exit(0);
  }

  /*  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    U->el[i][j] = a[i+1][j+1];
  for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
  V->el[i][j] = v[i+1][j+1];*/

  for (i=0; i < m.cols; i++) if (w[i+1] > max) max = w[i+1];
  for (i=0; i < m.cols; i++) {
    r = w[i+1]/max;
    //    printf("r[%d] = %f\n",i,r);
    /*D->el[i][i] = */w[i+1] = (r  < .01/* .00035*/) ? 0. : 1/w[i+1];

    //#ifdef VERBOSE

    //        if (r < .1 && r > .00001) 
    //      printf("ratio = %f a semi-small singular value (%12.8f)\n",r,D->el[i][i]);

    /*           if (r < .000290 && r > .000288) 
		 exit(0);  */
    /*  if (r > 0. && r < .00001) {
      printf("a truncated singular value (%12.8f)\n",r);
      Mp(m);
      }*/
    
	//#endif
  }
  for (i=0; i < m.rows; i++)  for (j=0; j < m.rows; j++) {
    m.el[i][j] = 0;
    for (k=0; k < m.rows; k++) m.el[i][j] += a[i+1][k+1]*a[j+1][k+1]*w[k+1];
  }
   free_my_dmatrix(a,1,rows,1,m.cols);
   free_my_dmatrix(v,1,m.cols,1,m.cols);
  free_my_dvector(w,1,m.cols);
}



void
Msvd_thresh(MATRIX m, MATRIX *U, MATRIX *D, MATRIX *V, float thresh)   {
  double **a,*w,**v,**y,**x;
  FLOAT_DOUBLE error,len,max=0.,r;
  int i,j,k,zdim,cdim,nn,nc,rows,d,min,flag,fail,ii=0,jj=0,found,loc;
  MATRIX UU;
  

  if (m.cols == 0)  { 
    U->rows = m.rows;
    V->rows = V->cols = D->rows = D->cols = U->cols = 0; 
    return;
  }
  if (m.rows == 0) {
    U->rows = 0;
    U->cols = m.cols;
    *D = Mzeros(m.cols,m.cols);
    *V = Miden(m.cols);
    return;
  }
  if (m.cols == 1) {
    *V = Miden(1);
    len = Mnorm(m);
    *U = Msc(m,1./len);
    *D = Miden(1);
    //    D->el[0][0] = len;
    D->el[0][0] = (len > thresh) ? len : 0;
    return;
  }
  rows = (m.rows < m.cols) ? m.cols : m.rows;
  a = my_dmatrix(1,rows,1,m.cols);
  y = my_dmatrix(1,rows,1,m.cols);
  x = my_dmatrix(1,rows,1,m.cols);
  v = my_dmatrix(1,m.cols,1,m.cols);
  w = my_dvector(1,m.cols);

  *V = Malloc(m.cols,m.cols);
  *D = Mzeros(m.cols,m.cols);
  *U = Malloc(m.rows,m.cols);



  for (k=0; k < MAX_SVD_ITER; k++) {
    for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = m.el[i][j]; 
    for (i = m.rows; i < rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = 0;  /* augment when m.rows < m.cols */
    fail = 0;
    dsvdcmp_var(a,rows,m.cols,w,v,&fail);
    ii = (ii+1)% rows;
    jj = (jj+1)% m.cols;

    if (fail)  m.el[ii][jj] += .000001;
    else break;
  }
  if (fail) {
    printf("didn't converge in %d tries at singular vaule decomp\n",MAX_SVD_ITER);
    Mp(m);
    exit(0);
  }
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    U->el[i][j] = a[i+1][j+1];
  for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
    V->el[i][j] = v[i+1][j+1];
  for (i=0; i < m.cols; i++)  D->el[i][i] = (w[i+1]  < thresh) ? 0. : w[i+1];
  free_my_dmatrix(a,1,rows,1,m.cols);
  free_my_dmatrix(v,1,m.cols,1,m.cols);
  free_my_dvector(w,1,m.cols);
}




static void
M2string(MATRIX m) {
  char buff[5000],num[1000];
  int i,j;


  /*  if (m.rows == 2 && m.cols == 2)
    if (fabs(m.el[0][0] - -0.473861) < .00001)
      Mp(m); */
  strcpy(buff,"tag ");
  sprintf(num,"%d %d ",m.rows,m.cols);
  strcat(buff,num);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) {
    sprintf(num,"%7.10f ",m.el[i][j]);
    strcat(buff,num);
  }
  printf("%s\n",buff);
}



void
oldMsvd(MATRIX m, MATRIX *U, MATRIX *D, MATRIX *V)   {
  /* U is not going to have orthogonal cols when m.rows < m.cols */
  double **a,*w,**v;
  FLOAT_DOUBLE error,len,max=0.,r;
  int i,j,k,zdim,cdim,nn,nc,rows,d,min,flag,found,loc,converge;


  /*       M2string(m);   */

  /* loc = lookup_decomp(m,&found);
  if (found) { 
    *U = svdstr.decomp[loc].U;
    *D = svdstr.decomp[loc].D;
    *V = svdstr.decomp[loc].V;
    return;
  } */



  if (m.cols == 0)  { 
    U->rows = m.rows;
    V->rows = V->cols = D->rows = D->cols = U->cols = 0; 
    return;
  }
  if (m.rows == 0) {
    U->rows = 0;
    U->cols = m.cols;
    *D = Mzeros(m.cols,m.cols);
    *V = Miden(m.cols);
    return;
  }
  if ((error = Mnorm(m)) < ALMOST_ZERO) { /*matrix is 0 matrix */
    *D = Mzeros(m.cols,m.cols);
    *V = Miden(m.cols);
    *U = Mzeros(m.rows,m.cols);
    min = (m.rows < m.cols) ? m.rows : m.cols;
    for (i=0; i < min; i++) U->el[i][i] = 1;
#ifdef VERBOSE     
    printf("this might be a bad idea (error = %f)\n",error);
#endif
    return;
  }
  if (m.cols == 1) {
    *V = Miden(1);
    len = Mnorm(m);
    *U = Msc(m,1./len);
    *D = Miden(1);
    D->el[0][0] = len;
    return;
  }
  /*  rows = (m.rows < m.cols) ? m.cols : m.rows;
rows = m.rows; */
  a = my_dmatrix(1,m.rows,1,m.cols);
  v = my_dmatrix(1,m.cols,1,m.cols);
  w = my_dvector(1,m.cols);
  /*printf("m is \n");
     Mp(m); */
     /*printf("rows = %d m.cols = %d m.rows = %d\n",m.rows,m.cols,m.rows); 
for (i=1; i <= m.rows; i++) for (j=1; j <= m.cols; j++) printf("%4.25f ",a[i][j]);
printf("\n");  
/*  a[1][1] += .0000001;  */

  *V = Malloc(m.cols,m.cols);
  *D = Mzeros(m.cols,m.cols);
  *U = Malloc(m.rows,m.cols);


  /*  for (i = m.rows; i < j.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = 0;  /* augment when m.rows < m.cols */


  /*    printf("a is\n");
  for (i=1; i <= m.rows; i++)  {
    for (j=1; j <= m.cols; j++) printf("%7.3f ",a[i][j]);
    printf("\n");
  } */


  do {
    for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
      a[i+1][j+1] = m.el[i][j]; 
    converge = 1;
    dsvdcmp(a,m.rows,m.cols,w,v);
    if (converge == 0) {
      printf("didn't converge in svdcmp\n");
      Mp(m);
      m.el[4][4] += .001;
      exit(0);
    } 
  } while( converge == 0);





  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    U->el[i][j] = a[i+1][j+1];
  for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
    V->el[i][j] = v[i+1][j+1];
  for (i=0; i < m.cols; i++) if (w[i+1] > max) max = w[i+1];
  for (i=0; i < m.cols; i++) {
    r = w[i+1]/max;
    /*    if (r < .00001) printf("singular value of %1.20f truncated to 0 (max is  %f)\n",w[i+1],max); */
    D->el[i][i] = (w[i+1]  < .0001) ? 0. : w[i+1];
	/*    D->el[i][i] = (r  < .001 || w[i+1] < .0001) ? 0. : w[i+1];*/





#ifdef VERBOSE 
        if (w[i+1] < .01 && w[i+1] > .0001) 
      printf("a semi-small singular value not truncated (%f)\n",w[i+1]);
    if (w[i+1] < .0001) printf("a small singular value truncated (%f)\n",w[i+1]); 
    /*   if ((r < .01  || w[i+1] < .01) && (r >  .001 && w[i+1] > .0001)) 
      printf("a semi-small singular value not truncated (%f) r = %f\n",w[i+1],r);
    if (r < .001 || w[i+1] < .0001) printf("a small singular value truncated (%f)\n",w[i+1]); */
#endif 
    
  }

  /*printf("and UVD here\n");
Mp(m);
Mp(*U);
Mp(*V);
Mp(*D);
for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
    printf("%f\n", v[i+1][j+1]); */


#ifdef DEBUG
  error = Mnorm(Ms(Mm(*U,Mm(*D,Mt(*V))),m)); 
  error /= Mnorm(m);
  if (error > .001 || isnanf(error)) {
    printf("problem in mysvdcmp() error = %f\n",error);
    printf("m is\n");
    Mp(m);
    printf("U is\n");
    Mp(*U);
    printf("D is\n");
    Mp(*D);
    printf("V is\n");
    Mp(*V);
    printf("prod isn");
    Mp(Mm(*U,Mm(*D,Mt(*V))));
    myexit(0);
  }
#endif
  /*  else printf("svd error is %f\n",error); */
  
  /*  printf("m = \n");
Mp(m); 
        printf("xxxxxxxxxxxxxsvd D is      \n");
for (i=0; i < m.cols; i++) printf("w[i] = %f w[i]/max = %f\n",w[i+1],w[i+1]/max);    */


  free_my_dmatrix(a,1,m.rows,1,m.cols);
  free_my_dmatrix(v,1,m.cols,1,m.cols);
  free_my_dvector(w,1,m.cols);

  /*     add_decomp(m,*U,*D,*V,loc);  */
}




#define MAX_XROWS 20
#define MAX_XCOLS 20

static void
Mnew_simplify_ortho_basis(MATRIX orth) {
  int col[MAX_XCOLS],row[MAX_XCOLS],i,rn,cn,ii,jj,k,l,j;
  MATRIX P;
  FLOAT_DOUBLE error;

  for (j=0; j < orth.cols; j++) col[j] = 0;  /* basis vectors to be simplified */
  P = Mm(orth,Mt(orth));
  for (i=0; i < orth.rows; i++) {
    row[i] = (P.el[i][i] > .99999);  /* componenets to be 1's */
    if (row[i]) for (j=0; j < orth.cols; j++) if (fabs(orth.el[i][j]) > .0001) col[j] = 1;
  }
  rn = cn = 0;
  for (i=0; i < orth.rows; i++) rn += row[i];
  for (j=0; j < orth.cols; j++) cn += col[j];
  if (rn != cn) return;  /* simplification might be possible here too */
  ii = jj = 0;
  for (k=0; k < rn; k++) {
    while (row[ii] == 0) ii++;
    while (col[jj] == 0) jj++;
    for (l = 0; l < orth.rows; l++) orth.el[l][jj] = (l == ii);
    ii++;
    jj++;
  }
  error = Mnorm(Ms(P,Mm(orth,Mt(orth))));
  if (error > .0001) {
    printf("error in Mnew_simplify_ortho_basis (%f)\n",error);
    Mp(orth);
    Mp(P);
    exit(0);
  }
}

    

  

static  void
Msimplify_ortho_basis(MATRIX orth) {
  int zero[MAX_XROWS],i,j,numz,d;

  return;
  Mnew_simplify_ortho_basis(orth);
  return;
  if (orth.rows > MAX_XROWS) {
    printf("matrix too big in Msimplify_ortho_basis\n");
    exit(0);
  }
  numz = orth.rows;
  for (i=0; i < orth.rows; i++) {
    zero[i] = 1;
    for (j=0; j < orth.cols; j++) {
      if (orth.el[i][j] != 0.) {
	zero[i] = 0;  /* row i has non zero entries */
	numz--;
	break;
      }
    }
  }
  if ((orth.rows - numz) == orth.cols) {
    /*     printf("simplify rows = %d cols = %d numz = %d\n",orth.rows,orth.cols,numz);
    Mp(orth);  */
    d = 0;
    for (i=0; i < orth.rows; i++) {
      if (zero[i]) continue;
      for (j=0; j < orth.cols; j++) orth.el[i][j] = (j == d);
      d++;
    }
    /*        Mp(orth);   */
  }
}


MATRIX 
Morthogonalize(MATRIX m) {
  MATRIX U,D,V,B; 
  int flag=0,count=0,i,j,ii=0;
  FLOAT_DOUBLE error;


  Msvd(m,&U,&D,&V);
  for (i=0; i < D.rows; i++) if (D.el[i][i] != 0.) count++;
  B = Mzeros(U.rows,count);
  for (i=0; i < D.rows; i++) if (D.el[i][i] != 0.) {
    for (j=0; j < U.rows; j++) B.el[j][ii] = U.el[j][i];
    ii++;
  }
#ifdef DEBUG
  error = Mnorm(Ms(Miden(B.cols),Mm(Mt(B),B)));
  if (error > .001) {
    printf("problem in Morthogonalize\n");
    myexit(0);
  }
  /*  if (m.rows < m.cols) {
    printf("Morthogonalize gives\n");
    Mp(B);
  } */
#endif
  /*  if (B.rows == B.cols) B = Miden(B.rows); /* testing */
  Msimplify_ortho_basis(B);
  return(B);
}


      


void
Mnull_decomp_thresh(MATRIX m, MATRIX *null, MATRIX *comp, float thresh )   {
  int i,j,zdim,cdim,nn,nc;
  MATRIX U,V,D;

 Msvd_thresh(m,&U,&D,&V,thresh);
  zdim = 0;
  for (i=0; i < m.cols; i++) if (D.el[i][i] == 0.) zdim++;
  cdim = m.cols - zdim;
  *null = Malloc(m.cols,zdim);
  *comp = Malloc(m.cols,cdim);
  nn = nc = 0;
  /*  Mp(*null);
  Mp(*comp); */
  for (j=0; j < m.cols; j++) {
    if (D.el[j][j] == 0.) {
      for (i=0; i < m.cols; i++) null->el[i][nn] = V.el[i][j];
      nn++;
    }
    else {
      for (i=0; i < m.cols; i++) comp->el[i][nc] = V.el[i][j];
      nc++;
    }
  }
    Msimplify_ortho_basis(*null);
  Msimplify_ortho_basis(*comp); 
  /*  if (null->rows == null->cols) *null = Miden(null->rows);  /* testing */
  /*  if (comp->rows == comp->cols) *comp = Miden(comp->rows); */
}

void
Mnull_decomp(MATRIX m, MATRIX *null, MATRIX *comp)   {
  int i,j,zdim,cdim,nn,nc;
  MATRIX U,V,D;
  float det;

 Msvd(m,&U,&D,&V);
  zdim = 0;
  for (i=0; i < m.cols; i++) if (D.el[i][i] == 0.) zdim++;
  cdim = m.cols - zdim;
  *null = Malloc(m.cols,zdim);
  *comp = Malloc(m.cols,cdim);
  nn = nc = 0;
  /*  Mp(*null);
  Mp(*comp); */
  for (j=0; j < m.cols; j++) {
    if (D.el[j][j] == 0.) {
      for (i=0; i < m.cols; i++) null->el[i][nn] = V.el[i][j];
      nn++;
    }
    else {
      for (i=0; i < m.cols; i++) comp->el[i][nc] = V.el[i][j];
      nc++;
    }
  }
    Msimplify_ortho_basis(*null);
  Msimplify_ortho_basis(*comp); 
  /*  if (null->rows == null->cols) *null = Miden(null->rows);  /* testing */
  /*  if (comp->rows == comp->cols) *comp = Miden(comp->rows); */
}






void
Msym_decomp(MATRIX m, MATRIX *UU, MATRIX *DD)  
     /* m is non-neg definite symmetric matrix */
     /* if svd fails probably matrix is not non-neg definite.
	routines always seems to return a diagonal matrix
	made of pos values and a negative eigenvalue is
	represented by a different sign in the relevant
	columsn of U and V */
{
  MATRIX U,V,D;
  FLOAT_DOUBLE error;

  if (m.rows != m.cols) { 
    printf("matrix not square\n"); 
    Mp(m);
    myexit(0); 
  }
  Msvd(m,&U,&D,&V);
  *UU = U;
  *DD = D;
#ifdef DEBUG
  error = Mnorm(Ms( Mm( Mm(*UU,*DD) ,Mt(*UU) ) ,m));
  if (error > .01/*ALMOST_ZERO*/) {
    printf("sym_decomp failed, error = %f \n",error);
    Mp(Mm(Mm(*UU,*DD),Mt(*UU)));
    Mp(m);
    Mp( Ms( Mm( Mm(*UU,*DD) ,Mt(*UU) ) , m) );
    Mp(*UU);
    Mp(*DD);
    myexit(0);
  }
#endif
}





MATRIX 
Mlu_solve(MATRIX m, MATRIX b) {
  MATRIX temp,oldm,U,D;
  double **a,*col,**dmatrix(),d,error,*dvector();
  int i,j,*index,*ivector(),diff;
  void free_vector(),free_dmatrix(),free_ivector();
  


  /*  Mp(m);
  Mp(b); */
  temp = Mzeros(m.cols,1);  /* return value */
  if (Mnorm(m) < ALMOST_ZERO) return(temp);
  if (Mnorm(b) < ALMOST_ZERO) return(temp);
  oldm = m;
  if (b.cols != 1) {
    printf("b must be a vector in Mlu_solve\n");
    myexit(0);
  }
  if (b.rows != m.rows) {
    printf("dim mismatch in Mlu_sovle\n");
    myexit(0);
  }
  if (m.cols == 0 || m.rows == 0) { 
    printf("cannot have 0 cols or rows in Mlu_solve\n"); 
    printf("m = \n");
    Mp(m);
    printf("b = \n");
    Mp(b);
    myexit(0); 
  }
  if (m.rows < m.cols) {
    diff = m.cols-m.rows;
    m = Mvcat(m,Mzeros(diff,m.cols));
    b = Mvcat(b,Mzeros(diff,1));
  } 
  else if (m.rows > m.cols) {
    diff = m.rows-m.cols;
    m = Mcat(m,Mzeros(m.rows,diff));
  } 
  a = my_dmatrix(1,m.rows,1,m.rows);
  col = my_dvector(1,m.rows);
  index = my_ivector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];
  dludcmp(a,m.rows,index,&d);
  for (j=0; j < m.rows; j++)  col[j+1] = b.el[j][0];
  dlubksb(a,m.rows,index,col);
  for (i=0; i < temp.rows; i++) temp.el[i][0] = col[i+1];
  free_my_dmatrix(a,1,m.rows,1,m.rows);
  free_my_dvector(col,1,m.rows);
  free_my_ivector(index,1,m.rows);
#ifdef DEBUG
  error = Mnorm(Ms(b,Mm(oldm,temp))); 
  if (error > .01) {
      /*  error = Mcompare(b,Mm(oldm,temp));
  if (error > .01) {*/
    printf("Mlu_solve gave inaccurate results\n error = %f\n",error);
    /*    printf("m = \n");
    Mp(Mm(oldm,temp));
    printf("b = \n");
    Mp(b); */
    /*    printf("det = %f\n",m.el[0][0]*m.el[1][1]-m.el[1][0]*m.el[0][1]);
    Msym_decomp(m,&U,&D); 
    for (i=0; i < U.rows; i++) printf("D[%d] = %f\n",i,D.el[i][i]); */
     myexit(0);
  }
#endif
  return(temp);
}



int 
Mequal(MATRIX m1, MATRIX m2) {
  int i,j;

  if (m1.rows != m2.rows) return(0);
  if (m1.cols != m2.cols) return(0);
  for (i=0; i < m1.rows; i++) for (j=0; j < m1.cols; j++)
    if (m1.el[i][j] != m2.el[i][j]) return(0);
  return(1);
}


FLOAT_DOUBLE
Mcompare(MATRIX true, MATRIX hat) {
  int i,j;
  FLOAT_DOUBLE max = 0,max_dif=0,den,error;

  if (true.rows != hat.rows || true.cols != hat.cols) {
    printf("dimensions disagree in Mcompare\n");
    myexit(0);
  }
  
  error = Mnorm(Ms(true,hat));
  den = Mnorm(true);
  if (den < 1.) den = 1.;
  /*printf("Mcompare %f %f\n",error,den);*/
  return(error/den);
  /*  for (i=0; i < true.rows; i++)
    for (j=0; j < true.cols; j++)
      if (fabs(true.el[i][j]) > max) max = fabs(true.el[i][j]);
  for (i=0; i < true.rows; i++)
    for (j=0; j < true.cols; j++)
      if (fabs(true.el[i][j]-hat.el[i][j]) > max_dif) max_dif = fabs(true.el[i][j]-hat.el[i][j]);
  return(max_dif/max); */
}


MATRIX 
Msolve(MATRIX m, MATRIX b)
     /* solve mx = b and return x or remark that it is impossible */
   {
  double **a,*w,**v,error;
  int i,j,zdim,cdim,nn,nc,rows,d;
  MATRIX U,V,D,temp,x;

  if (b.cols != 1) {
    printf("b must be a vector in Msolve\n");
    myexit(0);
  }
  if (b.rows != m.rows) {
    printf("dim mismatch in Msovle\n");
    myexit(0);
  }
  /*  if (m.rows < m.cols) {
    d = m.cols-m.rows;
    m = Mvcat(m,Mzeros(d,m.cols));
    b = Mvcat(b,Mzeros(d,1));
  } */
  if (m.cols == 0 || m.rows == 0) { 
    printf("cannot have 0 cols or rows in Msolve\n"); 
    printf("m = \n");
    Mp(m);
    printf("b = \n");
    Mp(b);
    myexit(0); 
  }
  Msvd(m,&U,&D,&V);
  /*  a = matrix(1,m.rows,1,m.cols);
  v = matrix(1,m.cols,1,m.cols);
  w = vector(1,m.cols);
  U = Malloc(m.rows,m.cols);
  V = Malloc(m.cols,m.cols);
  D = Malloc(m.cols,m.cols);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];
  svdcmp(a,m.rows,m.cols,w,v);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++) 
    U.el[i][j] = a[i+1][j+1];
  for (i=0; i < m.cols; i++) for (j=0; j < m.cols; j++) 
    V.el[i][j] = v[i+1][j+1];
  for (i=0; i < m.cols; i++) {
    D.el[i][i] = (w[i+1] < .001) ? 0 : w[i+1];
  } */
  for (i=0; i < m.cols; i++) D.el[i][i] = (D.el[i][i] == 0.) ? 0. : 1./D.el[i][i];
  x = Mm(V,Mm(D,Mm(Mt(U),b)));

  /*


  for (i=0; i < m.cols; i++) {
    if (D.el[i][i] == 0) {
      if (temp.el[i][0] > .001) {
	printf("equation is unsolvable\n");
	myexit(0);
      }
      else temp.el[i][0] = 0;
    }
    else temp.el[i][0] /= D.el[i][i];
  } */
  /*  free_matrix(a,1,m.rows,1,m.cols);
  free_matrix(v,1,m.cols,1,m.cols);
  free_vector(w,1,m.cols);*/
  /*  x = Mm(V,temp);*/
#ifdef DEBUG
  error = Mnorm(Ms(b,Mm(m,x)));
  if (error > .01) {
    printf("Msolve gave inaccurate result\n error = %f\n",error);
    printf("m = \n");
    Mp(m);
    printf("b = \n");
    Mp(b);
    printf("x = \n");
    Mp(x);
    printf("Dinv = \n");
    Mp(D);
    
    /*    myexit(0);*/
  }
#endif
  return(x);
}


MATRIX 
Mgen_inv(MATRIX m) {
  double **a,*w,**v,error;
  int i,j,zdim,cdim,nn,nc,rows,d;
  MATRIX U,V,D,temp,x;

  if (m.cols == 0 || m.rows == 0) { 
    printf("cannot have 0 cols or rows in Mgen_inv2\n"); 
    myexit(0); 
  }
  Msvd(m,&U,&D,&V);
  for (i=0; i < m.cols; i++) D.el[i][i] = (D.el[i][i] == 0.) ? 0. : 1./D.el[i][i];
  return(Mm(V,Mm(D,Mt(U))));
}


MATRIX 
Mi(MATRIX m)
{
  MATRIX temp;
  double det,**a,*col,**y,d;
  int i,j,*index;


  if (m.rows != m.cols) { 
    printf("matrix not invertible\n"); 
    Mp(m);
    myexit(0); 
  }
  temp = Malloc(m.rows,m.cols);
  col = my_dvector(1,m.rows);
  a = my_dmatrix(1,m.rows,1,m.cols);
  y = my_dmatrix(1,m.rows,1,m.cols);
  index = my_ivector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];

  dludcmp(a,m.rows,index,&d);
  for (j=1; j <= m.rows; j++) {
    for (i=1; i <= m.rows; i++) col[i] = 0;
    col[j] = 1;
    dlubksb(a,m.rows,index,col);
    for (i=1; i <= m.rows; i++) y[i][j] = col[i];
  }
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[i][j] = y[i+1][j+1];
  free_my_dmatrix(a,1,m.rows,1,m.cols);
  free_my_dmatrix(y,1,m.rows,1,m.cols);
  free_my_ivector(index,1,m.rows);
  return(temp);
}


void 
Mi_inplace(MATRIX m)
{
  MATRIX temp;
  double det,**a,*col,**y,d;
  int i,j,*index;


  if (m.rows != m.cols) { 
    printf("matrix not invertible\n"); 
    Mp(m);
    myexit(0); 
  }
  y = my_dmatrix(1,m.rows,1,m.cols);
  col = my_dvector(1,m.rows);
  index = my_ivector(1,m.rows);
  a = m.el-1;
  for (i=1; i <= m.rows; i++) a[i] -= 1; 

  dludcmp(a,m.rows,index,&d);
  for (j=1; j <= m.rows; j++) {
    for (i=1; i <= m.rows; i++) col[i] = 0;
    col[j] = 1;
    dlubksb(a,m.rows,index,col);
    for (i=1; i <= m.rows; i++) y[i][j] = col[i];
  }
  for (i=1; i <= m.rows; i++) a[i] += 1; 
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    m.el[i][j] = y[i+1][j+1];
}


MATRIX
Msym_range_inv(MATRIX S, MATRIX R) {
/*  S is symmetric matrix not nec of full rank.
    R is orthonormal basis for R(S) = comp(N(S))
    return matrix T st. T is inverse of S: R -> R */

  int dim = S.cols;

  
  if (R.cols == 0) return(Mzeros(S.rows,S.cols));
  if (R.cols != dim) S = Mm(Mm(Mt(R),S),R);
  S = Mi(S);
  if (R.cols != dim) S = Mm(Mm(R,S),Mt(R));
  return(S);
}



MATRIX
Msym_solve(MATRIX S, MATRIX b, MATRIX R) {
/*  Solve Sx = b where S  is symmetric matrix with R and
    orthogonal  basis for R(S).
    Note:
      Sx = b <==>  SUy = b  where Uy = x  (we choose x in N(S) comp = R(S) = R(U))
             <==>  U'SUy = U'b   (b and SUy in R(S) so nothing lost by this */ 
    
  MATRIX y,x,T,c,P,SS,bb;
  int dim = S.cols;
  FLOAT_DOUBLE error;
  
  if (R.cols == 0) return(Mzeros(S.rows,1));
  if (R.cols != dim) {
    T = Mm(Mm(Mt(R),S),R);
    /*printf("checking T\n");
c = Morthogonalize(T);*/
    c = Mm(Mt(R),b); 
    /*    Mp(R);*/
    /*  Mp(S); */
	/*	Mp(T); */

      
      
    y = Mlu_solve(T,c);
    x = Mm(R,y);
    
    /*    P = Mm(R,Mt(R));
    SS = Mm(Mm(P,S),P);
    bb = Mm(P,b); */
  }
  else x = Mlu_solve(S,b);
  /*  error = Mcompare(b,Mm(S,x)); */
#ifdef DEBUG
    error = Mnorm(Ms(Mm(S,x),b));
  if (error > .01) {
    printf("Msym_solve gave inaccurate result\n");
    printf("error is %f\n",error);
        printf("S is\n");
    Mp(S);
    printf("b is\n");
    Mp(b);
    printf("proj(R) is\n");
    P = Mm(R,Mt(R));
    Mp(Mm(R,Mt(R)));
    printf("proj(R) should be\n");
    Mnull_decomp(S,&T,&R);
    Mp(Mm(R,Mt(R)));
    printf("Pb is\n");
    Mp(Mm(P,b));
    Msvd(S,&P,&T,&SS);
    printf("eigs of S are\n");
    Mp(T);
    exit(0);
  }
#endif
  return(x);
}



MATRIX 
Msqrt(MATRIX m)   /* sqare root of symmetric matrix   (not unique) */
          {
  MATRIX temp,m1,m2,m3;
  double det,**a,**y,*w,**v;
  int i,j,*index,d,converge;

  if (m.rows != m.cols) { printf("matrix not square\n"); myexit(0); }
  temp = Malloc(m.rows,m.rows);
  a = my_dmatrix(1,m.rows,1,m.cols);
  v = my_dmatrix(1,m.rows,1,m.cols);
  w = my_dvector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];
  dsvdcmp(a,m.rows,m.rows,w,v);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[i][j] = a[i+1][j+1]*sqrt(w[j+1]);
  free_my_dmatrix(a,1,m.rows,1,m.cols);
  free_my_dmatrix(v,1,m.rows,1,m.cols);
  free_my_dvector(w,1,m.rows);
  return(m);
}


FLOAT_DOUBLE 
Mdet(MATRIX m)   /* determinant  matrix */
          {
  double **a,d;
  int i,j,*index;

  if (m.rows != m.cols) { printf("matrix not square\n"); myexit(0); }
  if (m.rows == 1 && m.cols == 1) return(m.el[0][0]);
  a = my_dmatrix(1,m.rows,1,m.cols);
  index = my_ivector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];
  dludcmp(a,m.rows,index,&d);
  for (j=1; j <= m.rows; j++) d *= a[j][j];
  free_my_dmatrix(a,1,m.rows,1,m.cols);
  free_my_ivector(index,1,m.rows);
  return(d);
}






MATRIX 
Msc(MATRIX m, FLOAT_DOUBLE c)   /* scale matrix */
          
{
  int i,j;
  MATRIX temp;

  temp = Malloc(m.rows,m.cols);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[i][j] = c*m.el[i][j];
  return(temp);
}

MATRIX 
Mcopy(MATRIX m)  {  /* copy matrix */
  int i,j;
  MATRIX temp;

  temp = Malloc(m.rows,m.cols);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[i][j] = m.el[i][j];
  return(temp);
}



void 
Mp_file(MATRIX m, FILE *fp)
{
    int i,j;

    fprintf(fp,"dim = %d %d\n",m.rows,m.cols);
    for (i=0; i < m.rows; i++) {
	for (j=0; j < m.cols; j++) fprintf(fp,"%10.7f ",m.el[i][j]);
	fprintf(fp,"\n");
    }
    fprintf(fp,"\n");
}


MATRIX 
Mr_file(FILE *fp)
{
    MATRIX m; 
    int i,j,r,c;
    char s[1000];
    float x;

    fscanf(fp,"dim = %d %d\n",&r,&c);
    m = Malloc(r,c);
    for (i=0; i < m.rows; i++) {
	for (j=0; j < m.cols; j++) {
	  fscanf(fp,"%f",&x);

	  m.el[i][j] = x;
	}
	fscanf(fp,"\n");
    }
    return(m);
}



MATRIX 
Miden(int r)
{
  MATRIX temp;
  int i,j;

  temp = Malloc(r,r);
  for (i=0; i < r; i++) for (j=0; j < r; j++) temp.el[i][j] = (i == j) ? 1 : 0;
  return(temp);
}


void
Mmp_file(MATRIX m,FILE *fp) {
    int i,j;

    fprintf(fp,"dim = %d %d\n",m.rows,m.cols);
    for (i=0; i < m.rows; i++) {
	for (j=0; j < m.cols; j++) fprintf(fp,"%f ",m.el[i][j]);
	fprintf(fp,"\n");
    }
    fprintf(fp,"\n");
}


MATRIX 
Mmr_file(FILE *fp) {
    MATRIX m; 
    int i,j,r,c;

    fscanf(fp,"dim = %d %d\n",&r,&c);
    m = Malloc(r,c);
    for (i=0; i < m.rows; i++) {
	for (j=0; j < m.cols; j++) fscanf(fp,"%f ",&(m.el[i][j]));
    }
    return(m);
}



double
**my_dmatrix(int nrl, int nrh, int ncl, int nch)
{
	int i;
	double **m;

	m=(double **) alloc_space((unsigned) (nrh-nrl+1)*sizeof(double*));
	if (!m) nrerror("allocation failure 1 in dmatrix()");
	m -= nrl;

	for(i=nrl;i<=nrh;i++) {
		m[i]=(double *) alloc_space((unsigned) (nch-ncl+1)*sizeof(double));
		if (!m[i]) nrerror("allocation failure 2 in dmatrix()");
		m[i] -= ncl;
	}
	return m;
}

void 
free_my_dmatrix(double **m, int nrl, int nrh, int ncl, int nch)
{
	return;
}

double 
*my_dvector(int nl, int nh)
{
	double *v;

	v=(double *) alloc_space((unsigned) (nh-nl+1)*sizeof(double));
	if (!v) nrerror("allocation failure in dvector()");
	return v-nl;
}



void
free_my_dvector(double *v, int nl, int nh)
{
	return;
}

int 
*my_ivector(int nl, int nh)
{
	int *v;

	v=(int *) alloc_space((unsigned) (nh-nl+1)*sizeof(int));
	if (!v) nrerror("allocation failure in ivector()");
	return v-nl;
}

void 
free_my_ivector(int *v, int nl, int nh)
{
	return;
}

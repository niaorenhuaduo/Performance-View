#ifndef MATRIX_UTIL
#define MATRIX_UTIL

#include <stdio.h>
#include "kalman.h"


#define FLOAT_DOUBLE double /* float  */
 /* equilibrium can be *very* inaccurate with float, but ...
    float saves space */

typedef struct {
  int rows;
  int cols;
  FLOAT_DOUBLE **el;  
} MATRIX; 

typedef struct {
  MATRIX m;
  MATRIX U;
  MATRIX D;
  MATRIX V;
} DECOMP_STR; 

#define DECOMP_NUMBER 50000

typedef struct {
  int num;
  DECOMP_STR decomp[DECOMP_NUMBER];
} SVD_STRUCT;


typedef struct { /* matrix of matrices */
  int rows;
  int cols;
  MATRIX **sub;
} SUPERMAT;  


/*#include "share.c" // for matrix defn */
/*#include "joint_norm.h" */

#define COV_TO_S 1  /* when going from a covariance matrix to its inverse */
#define S_TO_COV 0


void  Mi_inplace(MATRIX m);
void Mma(MATRIX m1, MATRIX m2, MATRIX *temp);
void Msvd_inv_in_place(MATRIX m);
MATRIX Mmissing(MATRIX perm);
MATRIX  Mpartial_iden(int r, int c);
MATRIX  Mmt(MATRIX m1, MATRIX m2);
void Mmp_file(MATRIX m,FILE *fp);
MATRIX Mmr_file(FILE *fp);
MATRIX Mconvert(MX om);
void Msvd(MATRIX m, MATRIX *U, MATRIX *D, MATRIX *V);
FLOAT_DOUBLE Mcompare(MATRIX truee, MATRIX hat);
void myexit(int i);
char* highest_address(void);
int Mis_subspace(MATRIX U, MATRIX V);
FLOAT_DOUBLE Mnorm(MATRIX m);
MATRIX Morthogonalize(MATRIX m);
void Msym_decomp(MATRIX m, MATRIX *UU, MATRIX *DD);  
MATRIX Munset(void);
MATRIX Mperm(MATRIX m);
void Mcopymat(MATRIX from, MATRIX *to);
MATRIX Mperm_alloc(int n, int m);
void Mset_zero(MATRIX m);
void Mset_iden(MATRIX m);
void Minter(MATRIX U, MATRIX V, MATRIX *inter, MATRIX *comp);
int same_space(MATRIX m1, MATRIX m2);
MATRIX Mintersect(MATRIX U, MATRIX V);
void Mint_comp(MATRIX U, MATRIX V, MATRIX *intersect, MATRIX *int_comp);
MATRIX Msym_solve(MATRIX S, MATRIX b, MATRIX R);
MATRIX Mdirect_sum(MATRIX m1, MATRIX m2);
MATRIX Msym_range_inv(MATRIX S, MATRIX R);
MATRIX Mfinite_space(MATRIX U, MATRIX D);
MATRIX Mlu_solve(MATRIX m, MATRIX b);
MATRIX Msolve(MATRIX m, MATRIX b);
MATRIX Mvcat(MATRIX m1, MATRIX m2);
MATRIX Minf(int r);
MATRIX Mdiag_neg(MATRIX m);
MATRIX Meigen_space(MATRIX U, MATRIX D, FLOAT_DOUBLE v);
void Mset_inf(MATRIX *U, MATRIX *D, MATRIX M);
void Mfinite_part(MATRIX U, MATRIX D, MATRIX *UU, MATRIX *DD);
MATRIX Mdcat(MATRIX m1, MATRIX m2);   /* concatenate diagonal matrices */
MATRIX Morthog(MATRIX U, MATRIX V);  /* ortho basis of U in V */
MATRIX Mcovariance(MATRIX U, MATRIX D);  /* finite part of UDU' */
MATRIX Mconcentration(MATRIX U, MATRIX D);  /* finite part of UDinvU' */
MATRIX Mcolumn(MATRIX m, int c);
MATRIX Mdiag_invert(MATRIX m);
MATRIX Mcat(MATRIX m1, MATRIX m2);   /* concatenate horizontally matrices */
void Mnull_decomp(MATRIX m, MATRIX *null, MATRIX *comp);
FLOAT_DOUBLE Mnormsq(MATRIX m);  /* norm square */ 
MATRIX Malloc(int n, int m);
MATRIX Mm(MATRIX m1, MATRIX m2);  /* matrix multiply */
MATRIX Ma(MATRIX m1, MATRIX m2);   /* add matrices */
MATRIX Mzeros(int r, int c);
MATRIX Ms(MATRIX m1, MATRIX m2);  /* subtract matrices */ 
void Mp(MATRIX m);
MATRIX Mt(MATRIX m);   /* matrix transpose */
MATRIX Mgeninv(MATRIX m, FLOAT_DOUBLE *det, int *rank, int mode);   /* invert matrix */
MATRIX Mi(MATRIX m);
MATRIX Msqrt(MATRIX m);   /* sqare root of symmetric matrix   (not unique) */
FLOAT_DOUBLE Mdet(MATRIX m);   /* determinant  matrix */
MATRIX Msc(MATRIX m, FLOAT_DOUBLE c);   /* scale matrix */
void Mp_file(MATRIX m, FILE *fp);
MATRIX Mr_file(FILE *fp);
MATRIX Vcombine(MATRIX v1, MATRIX v2);
MATRIX Mcombine(MATRIX m11, MATRIX m12, MATRIX m21, MATRIX m22);
MATRIX Miden(int r);
MATRIX Mcopy(MATRIX m);
MATRIX Mconst(int r, int c, FLOAT_DOUBLE v);
MATRIX Mleft_corner(int r, int c);
MATRIX Mcomp_intersect(MATRIX Uc, MATRIX Vc);
int Mequal(MATRIX m1, MATRIX m2);
MATRIX Mperm_id(MATRIX m);
MATRIX Mperm_alloc_id(int n, int m);
int Mspaces_equal(MATRIX m1, MATRIX m2);
void Msvd_thresh(MATRIX m, MATRIX *U, MATRIX *D, MATRIX *V, float thresh);
void Mnull_decomp_thresh(MATRIX m, MATRIX *null, MATRIX *comp, float thresh );
SUPERMAT Salloc(int r, int c);
double **my_dmatrix(int nrl, int nrh, int ncl, int nch);
void free_my_dmatrix(double **m, int nrl, int nrh, int ncl, int nch);
double *my_dvector(int nl, int nh);
void free_my_dvector(double *v, int nl, int nh);
int *my_ivector(int nl, int nh);
void free_my_ivector(int *v, int nl, int nh);
void alloc_perm_matrix_buff();
void alloc_matrix_buff();
#endif




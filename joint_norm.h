#ifndef JOINT

#include "kalman.h"
#include "matrix_util.h"
#include "supermat.h"


typedef struct {
  MATRIX mean;
  MATRIX var;
} JOINT_NORM;   


typedef struct {
  MATRIX mean;
  MATRIX var;
  MATRIX U; 
  MATRIX D;
  float lc;  /* lc is log of multiplicative constant */
} JOINT_NORMAL;  



/* we represent a generailzed normal dist as

exp -1/2  (x-m)'S(x-m) + (x-minf)'Uinf Dinf Uinf' (x-minf)

the first part represents the finite part of S (S might
have a non trivial null space) and the second part represents
an affine space where the exponent is infinte. */


//CF:  a high-demiensional gaussian with potential for two kinds of degeneracy (spike and flat)
typedef struct {
  MATRIX m;
  MATRIX S;  /* the finite part of concentration (aka. precision) matrix */
/* U,D an alternative representation of covariance matrix inv(S). 
   U is orthogonal matrix
   and D is Diagonal.  inv(S) = UDU' */  /*  MATRIX U;
      MATRIX D; */
      float  c;
  MATRIX cov; /* the finite part of covariance matrix */
  MATRIX null; /* the null space of covariance matrix.  inf space of concentration */
  MATRIX inf;  /* the infinte space of covaraince matrix. null space of concentration */
  MATRIX fin;  /* the space where cov is finite but not 0 (same for concentration)*/
  /*  MATRIX minf;  // the distribution is constrained to concentrate on inf*inf'x = minf 
		  ie the projection of x into the inf space must be minf */
  /* these last three matrices have orthonormal rows.
     fin and inf are orthogonal to one another but there might
     be some overlap of null with either fin or inf*/
} QUAD_FORM;   /* y(x) = (x-m)'S(s-m) + c */


typedef struct {
  SUPERMAT m;
  SUPERMAT S;
  SUPERMAT U;
  SUPERMAT D;
  float  c;
} SUPER_QUAD;   /* quadratic form using super matrices */


typedef struct {
  SUPERMAT mean;
  SUPERMAT var;
  SUPERMAT U;
  SUPERMAT D;
  float lc;   /* lc is log of multiplicative constant */
} SUPER_JN;


QUAD_FORM QFpromote(QUAD_FORM qf, MATRIX perm);  //CF:  extend qf to higher dimensions
QUAD_FORM QFcond_dist(MATRIX A, QUAD_FORM e);    //CF:  creates a conditional distribution p(y|x) from y=Ax+e (y may be hi-D)
QUAD_FORM QFfollow_at_all_costs(MATRIX A, float length);
QUAD_FORM QFpos_tempo_near();
QUAD_FORM QFpos_slop(float length);
QUAD_FORM QFpos_var(int dim, float var);
QUAD_FORM QFhold_steady(void);
QUAD_FORM QFimmediate_tempo_change_slop(MATRIX A, float length);
QUAD_FORM QFimmediate_tempo_change(MATRIX A, float length);
QUAD_FORM QFmake_pos_unif(QUAD_FORM qf);
QUAD_FORM QFmv(MATRIX m, MATRIX cov);
QUAD_FORM QFmeas_size(float size, float std);
QUAD_FORM QFtempo_change_pos_slop(float length);
QUAD_FORM QFunif_accel_zero();
QUAD_FORM QFmeas_size_slop(float std_dev);
QUAD_FORM QFtempo_change(float length);
QUAD_FORM  QFconvert(JN ojn);
QUAD_FORM QFnull_inf(int dim, int *null); 
int approx_equal(QUAD_FORM q1, QUAD_FORM q2);
QUAD_FORM nondegenerate(QUAD_FORM qf);
void QFcompare(QUAD_FORM qf1, QUAD_FORM qf2);
QUAD_FORM QFxform(QUAD_FORM qf, MATRIX A);      //CF:  transform gaussian qf by linear transform A
void QFcheck(QUAD_FORM qf);
MATRIX  rand_gauss(QUAD_FORM qf);
QUAD_FORM QFperm(QUAD_FORM qf);
QUAD_FORM QFpos_same(int dim);
QUAD_FORM QFpos_near(int dim);
QUAD_FORM QFindep_sum(QUAD_FORM q1, QUAD_FORM q2);
QUAD_FORM  linear_QFplus(QUAD_FORM qf1, QUAD_FORM qf2, MATRIX B1, 
			 MATRIX B2, MATRIX *C1, MATRIX *C2);
void matrix_restricted_mean(MATRIX S, MATRIX V1, MATRIX V2,
			    MATRIX fin, MATRIX *A, MATRIX *B);
void matrix_infinite_mean(MATRIX U1, MATRIX U2, MATRIX U, MATRIX *R1, MATRIX *R2);
QUAD_FORM matrix_QFplus(QUAD_FORM qf1, QUAD_FORM qf2, MATRIX *A1, MATRIX *A2);
QUAD_FORM QFsphere(int dim, MATRIX mean, float var);
QUAD_FORM QFpoint(MATRIX m);            //CF:  construct hi-D (usually 1D)delta function, all mass concentrated at m
                                        //CF:  this is of same dimension as the number of observations
                                        //CF:  It can be extened to higher dims later.  
QUAD_FORM QFobs(int dim, int fixed, float obs);
QUAD_FORM QFunif(int dim); //CF:  create uniform (flat) distro, flat over all dims.
void QFcopy(QUAD_FORM from, QUAD_FORM *to);
QUAD_FORM QFinit(int dim);
QUAD_FORM QFminus(QUAD_FORM qf1, QUAD_FORM qf2);   //CF:  gaussian division (ie subtracting exponents)
void compare_reps(QUAD_FORM q);
QUAD_FORM  QFplus(QUAD_FORM qf1, QUAD_FORM qf2);   //CF:  gaussian multiplication (ie adding exponents)
SUPER_QUAD SQadd(SUPER_QUAD sq1, SUPER_QUAD sq2);
SUPER_QUAD QFtoSQ(QUAD_FORM qf, SUPER_QUAD templatee);
QUAD_FORM SQtoQF(SUPER_QUAD sq);
QUAD_FORM QFneg(QUAD_FORM qf);
QUAD_FORM QFadd(QUAD_FORM qf1, QUAD_FORM qf2);
void SQprint(SUPER_QUAD sq);
/*JOINT_NORMAL JNset(MATRIX m, MATRIX v);
JOINT_NORMAL JNprint(JOINT_NORMAL jn); */
QUAD_FORM QFset(MATRIX m, MATRIX U, MATRIX D);
QUAD_FORM QFprint(QUAD_FORM qf);
/*QUAD_FORM JNtoQF(JOINT_NORMAL jn);
JOINT_NORMAL QFtoJN (QUAD_FORM qf);
SUPER_QUAD SJNtoSQ(SUPER_JN sjn);
SUPER_JN SJNset(SUPERMAT m, SUPERMAT v, float lc);
SUPER_JN SQtoSJN(SUPER_QUAD sq);
void SJNprint(SUPER_JN sjn); */
SUPER_QUAD linear_update(MATRIX *A, int n, JOINT_NORMAL e);
SUPER_QUAD SQset(SUPERMAT m, SUPERMAT v, float con);
QUAD_FORM restrct(QUAD_FORM qf, MATRIX V1, MATRIX V2, MATRIX c);
QUAD_FORM QFindep(int dim, MATRIX m, float *var);
QUAD_FORM QFtempo_change_pos_slop_parms(float pos, float size);
QUAD_FORM QFmargin(QUAD_FORM qf, MATRIX perm);   //CF:  compute marginals specified in perm
int QFspace_equal(QUAD_FORM q1, QUAD_FORM q2);
QUAD_FORM QFmargin_sep(QUAD_FORM qf, MATRIX perm, QUAD_FORM space, int set);
QUAD_FORM QFplus_sep(QUAD_FORM qf1, QUAD_FORM qf2);
QUAD_FORM QFminus_sep(QUAD_FORM qf1, QUAD_FORM qf2);
QUAD_FORM QFpromote_sep(QUAD_FORM qf, MATRIX perm);
void QFprint_file(QUAD_FORM qf, FILE *fp);
QUAD_FORM QFread_file(FILE *fp);
QUAD_FORM QFmv_simplify(MATRIX m, MATRIX cov);
QUAD_FORM QFperm_id(QUAD_FORM qf);
QUAD_FORM QFpos_tempo_sim();
QUAD_FORM QFconditional(QUAD_FORM qf, MATRIX A);
QUAD_FORM QFatempo(float size);
QUAD_FORM QFpos_sim();
QUAD_FORM QFmeas_size2(float size, float std);
QUAD_FORM QFmv_t_simplify(MATRIX m, MATRIX cov);
QUAD_FORM QFpos_tempo_very_sim();
QUAD_FORM QFmv_simple(MATRIX m, MATRIX cov);    //CF:  basic constructor for full-rank covariance
QUAD_FORM QFmeas_size_at(float size, float std);
QUAD_FORM QFpos_meassize_update(float pos, float size);

#define JOINT
#endif

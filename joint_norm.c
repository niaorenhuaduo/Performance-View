//#define DEBUG    
#include "joint_norm.h" 
#include <math.h>
#define PI 3.14159


static float square(float x) {
  return(x*x);
}

void
QFcompare(QUAD_FORM qf1, QUAD_FORM qf2) {
  float error;
  MATRIX t1,t2;

  error = Mnorm(Ms(qf1.S,qf2.S));
  if (error > .1) {
    printf("S's disagree in QF_compare() (error = %f)\n",error);
    exit(0);
  }
  error = Mnorm(Ms(qf1.cov,qf2.cov));
  /*  if (error > .01) {
    Mp(qf1.cov);
    Mp(qf2.cov);
    Mp(Ms(qf1.S,qf2.S));
    printf("cov's disagree in QF_compare()\n");
    exit(0);
    }*/
  error = Mnorm(Ms(Mm(qf1.fin,Mt(qf1.fin)),Mm(qf2.fin,Mt(qf2.fin))));
  if (error > .012) {
    printf("fin's disagree in QF_compare() (%f) \n",error);
    exit(0);
  }
  error = Mnorm(Ms(Mm(qf1.inf,Mt(qf1.inf)),Mm(qf2.inf,Mt(qf2.inf))));
  if (error > .001) {
    printf("inf's disagree in QF_compare() (error = %f)\n",error);
    Mp(qf1.inf);
    Mp(qf2.inf);
    exit(0);
  }
  t2 = Mm(qf2.null,Mt(qf2.null));
  t1 = Mm(qf1.null,Mt(qf1.null));
  error = Mnorm(Ms(t1,t2));
  if (error > .001) {
    printf("null's disagree in QF_compare()\n");
    exit(0);
  }
  error = Mnorm(Ms(qf1.m,qf2.m));
  if (error > .03) {
    printf("means disagree in QF_compare() (%f)\n",error);
    Mp(qf1.m);
    Mp(qf2.m);
    /*    Mp(Mm(Mm(qf1.null,Mt(qf1.null)),qf1.m));
    Mp(Mm(Mm(qf2.null,Mt(qf2.null)),qf2.m));
    Mp(Mm(Mm(qf1.inf,Mt(qf1.inf)),qf1.m));
    Mp(Mm(Mm(qf2.inf,Mt(qf2.inf)),qf2.m));
    Mp(Mm(Mm(qf1.fin,Mt(qf1.fin)),qf1.m));
    Mp(Mm(Mm(qf2.fin,Mt(qf2.fin)),qf2.m)); */
    exit(0);
  }
  /*   printf("okay\n"); */
}
#include "belief.h"

  

void check_sym(MATRIX m) {
  int i,j;

  for (i=0; i < m.rows; i++)  for (j=i; j < m.rows; j++) 
    if (m.el[i][j] != m.el[j][i]) {
      printf("matrix not symmetric\n");
      Mp(m);
      printf("%10.10f  != %10.10f\n",m.el[i][j],m.el[j][i]);
      exit(0);
    }
}
     



void 
QFcheck(QUAD_FORM qf) {
  MATRIX temp,null,comp,inf,inter,U,D,both_null,P;
  float error;
  int i;

  /* return; */

  /* DO NOT CALL THIS WITH A PERMANENT QUADRATIC FORM !!! */ 



  if (qf.m.rows > 0) {
    error =  Mnorm(Mm(Mm(qf.inf,Mt(qf.inf)),qf.m));
    if (error  > .001) {
      printf("quad form has mean with non-zero (%f) projection onto inf space (exit)\n",error);
      QFprint(qf);
      exit(0); 
    } 
  }

  for (i=0; i < qf.m.rows; i++) if (qf.S.el[i][i] < -.000001) {
    printf("matrix not non-negative definite (%.20f)\n",qf.S.el[i][i]);
    exit(0);
  }


  /*    if ((Mnorm(qf.S) + Mnorm(qf.cov)) > 100000) {
    printf("suspicision S or cov (%f, %f)\n",Mnorm(qf.S),Mnorm(qf.cov));
    QFprint(qf);
    exit(0);
    } */
  

  U = Mcat(Mcat(qf.inf,qf.null),qf.fin);
  error = Mnorm(Ms(Miden(U.rows),Mm(U,Mt(U))));
  if (error > .01) {
    printf("spaces not orthogonal decomp\n");
    exit(0);
  }

  P = Mm(qf.null,Mt(qf.null));
  error = Mnorm(Mm(Mm(P,qf.S),P));
  if (error > .001) {
    printf("nonzero projection of S onto null space (error = %f)\n",error);
    Mp(qf.null);
    Mp(qf.S);
    exit(0);
  }

  P = Mm(qf.inf,Mt(qf.inf));
  error = Mnorm(Mm(Mm(P,qf.S),P));
  if (error > .001) {
    printf("nonzero projection of S onto inf space\n");
    exit(0);
  }
  P = Mm(qf.null,Mt(qf.null));
  error = Mnorm(Mm(Mm(P,qf.S),P));
  if (error > .001) {
    printf("nonzero projection of S onto null space\n");
    exit(0);
  }


    if(qf.cov.el == NULL || qf.cov.rows == 0) {
      printf("qf.cov is not set\n");
      exit(0);
    }
    if(qf.S.el == NULL || qf.S.rows == 0)  {
      printf("qf.S is not set\n");
      exit(0);
    }
    
    error = Mnorm(Ms(Mm(qf.fin,Mt(qf.fin)),Mm(qf.S,qf.cov)));
    if (error > .01){ 
      printf("qf.S does not have range qf.fin (%f)\n",error);
      Mp(Mm(qf.fin,Mt(qf.fin)));
      Mp(Mm(qf.S,qf.cov));
      Mp(Ms(Mm(qf.fin,Mt(qf.fin)),Mm(qf.S,qf.cov)));
      exit(0);
    }

    temp = Msym_range_inv(qf.cov,qf.fin);
    error = Mcompare(qf.S,temp); 
    if (error > .001) {
      printf("problem in QFcheck error = %f\n",error);
      printf("inconsistent S and cov\n");
      Mp(qf.cov);
      Mp(qf.S);
      QFprint(qf);    
      exit(0);
    }
    temp = Msym_range_inv(qf.S,qf.fin);
    error = Mcompare(qf.cov,temp); 
    if (error > .005) {
      printf("problem in QFcheck error = %f\n",error);
      printf("inconsistent S and cov\n");
      Mp(qf.cov);
      Mp(qf.S);
      QFprint(qf);    
      exit(0);
    }



    return;





    /*    if(qf.cov.el == NULL || qf.cov.rows == 0) qf.cov =  Msym_range_inv(qf.S,qf.fin);*/
    /*  if(qf.S.el == NULL || qf.S.rows == 0)  qf.S =  Msym_range_inv(qf.cov,qf.fin);*/
/*  error = Mnorm(Ms(temp,qf.S))/(qf.S.rows*qf.S.cols); */
  /*printf("cov and S:\n");
Mp(qf.cov);
Mp(qf.S); */
/*  Mnull_decomp(Ma(qf.cov,qf.S),&null,&comp);  */




  /*  Msym_decomp(qf.cov, &U, &D);
  for (i = 0; i < D.rows; i++) if (D.el[i][i] < 0) {
    printf("qf.cov is not nonnegative definite\n");
    exit(0); 
  } */


      Mnull_decomp(Ma(qf.cov,qf.S),&null,&comp); 
  if (!same_space(comp,qf.fin)) {
    printf("qf.fin not equal to non-null of covariance\n");
    printf("cov_space\n");
    Mp(Mm(comp,Mt(comp)));
    printf("qf.fin space\n");
    Mp(qf.fin);
    Mp(Mm(qf.fin,Mt(qf.fin)));
    printf("cov = \n");
    Mp(qf.cov);
    printf("fin = \n");
    Mp(qf.fin);
    printf("non null = \n");
    Mp(comp);
    printf("null = \n");
    Mp(null);
    printf("qf.null = \n");
    Mp(qf.null);
    printf("qf.inf = \n");
    Mp(qf.inf);
    
    QFprint(qf);
    exit(0);
    }
  Mint_comp(null,qf.null,&inter,&comp); 
 if (!same_space(comp,qf.inf)) { 
    printf("qf.null + qf.inf not equal to null of covariance\n");
	   
        printf("cov\n");
    Mp(qf.cov);
    printf("null of cov\n");
    Mp(null);
    printf("null\n");
    Mp(qf.null);
    printf("qf.inf\n");
    Mp(qf.inf);
    printf("comp\n");
    Mp(comp);
    printf("inter\n");
    Mp(inter);
    printf("null*t(null)\n");
    Mp(Mm(null,Mt(null)));
    printf("qf.null*t(qf.null)\n");
    Mp(Mm(qf.null,Mt(qf.null)));
    
    printf("\n\n\n\nbad qf\n");
    QFprint(qf);
    Mp(Mm(Mt(qf.null),qf.inf));
    Mp(Mm(Mt(qf.null),qf.null));
    Mp(Mm(Mt(qf.inf),qf.inf));
    exit(0); 
  }
  if (!same_space(inter,qf.null)) {
    printf("qf.null not contained in null of covariance\n");
    printf("cov = \n");
    Mp(qf.cov);
    printf("null = \n");
    Mp(null);
    printf("inter = \n");
    Mp(inter);
    printf("comp = \n");
    Mp(comp);
    printf("qf.null = \n");
    Mp(qf.null);
    printf("qf.inf = \n");
    Mp(qf.inf);
    printf("qf.fin = \n");
    Mp(qf.fin);
    exit(0);
  }
  
}  
  

    




void
QFcopy(QUAD_FORM from, QUAD_FORM *to) { 
  Mcopymat(from.m,&(to->m));
  Mcopymat(from.S,&(to->S));
  Mcopymat(from.cov,&(to->cov));
  Mcopymat(from.null,&(to->null));
  Mcopymat(from.fin,&(to->fin));
  Mcopymat(from.inf,&(to->inf));
}

//CF:  malloc for a qf of dimension dim
QUAD_FORM
QFinit(int dim) {
  QUAD_FORM temp;

  temp.m = Mperm_alloc(dim, 1);
  temp.S = Mperm_alloc(dim, dim);
  temp.cov = Mperm_alloc(dim, dim);
  temp.fin = Mperm_alloc(dim, dim);  /* maximum dimensions */
  temp.inf = Mperm_alloc(dim, dim);
  temp.null = Mperm_alloc(dim, dim);
  return(temp);
}

int
QFspace_equal(QUAD_FORM q1, QUAD_FORM q2) {
  if (Mequal(q1.null,q2.null) == 0) return(0);
  if (Mequal(q1.fin,q2.fin) == 0) return(0);
  if (Mequal(q1.inf,q2.inf) == 0) return(0);
  return(1);
}





//CF:  constructor for quad forms with independent dimensions
QUAD_FORM
QFindep(int dim, MATRIX m, float *var) { /* indep coords */
  QUAD_FORM temp;
  int i;
  MATRIX v;

  if (m.rows != dim) {
    printf("problem in QFindep\n");
    exit(0);
  }
  if (m.cols != 1) {
    printf("m not vector in QFpoint\n");
    exit(0);
  }
  temp.m =  m;
  temp.cov =  Mzeros(dim,dim);
  temp.null = Mzeros(dim,0);
  temp.inf  = Mzeros(dim,0);
  temp.fin  = Mzeros(dim,0);
  for (i=0; i < dim; i++) { 
    v = Mzeros(dim,1);
    v.el[i][0] = 1;
    if (var[i] != 0. && var[i] != HUGE_VAL) temp.cov.el[i][i] = var[i];
    if (var[i] == 0.) temp.null = Mcat(temp.null,v);
    else if (var[i] == HUGE_VAL) temp.inf = Mcat(temp.inf,v);
    else temp.fin = Mcat(temp.fin,v);
  }
  temp.S = Msym_range_inv(temp.cov,temp.fin);
  return(temp);
}


QUAD_FORM
QFunif(int dim) { /* a noninformative distribution */
  QUAD_FORM temp;

  temp.m =  Mzeros(dim,1);
  temp.cov = Mzeros(dim,dim);
  temp.inf = Miden(dim);
  temp.null = Mzeros(dim,0);
  temp.fin = Mzeros(dim,0);
  temp.S =   Msym_range_inv(temp.cov,temp.fin);
  return(temp);
}

QUAD_FORM
QFpoint(MATRIX m) { /* a dirac mass at m */
  QUAD_FORM temp;
  int dim;

  if (m.cols != 1) {
    printf("m not vector in QFpoint\n");
    exit(0);
  }
  dim =m.rows;
  temp.m =  m;
  temp.cov = Mzeros(dim,dim);
  temp.null = Miden(dim);
  temp.inf  = Mzeros(dim,0);
  temp.fin  = Mzeros(dim,0);
  temp.S =   Mzeros(dim,dim);
  return(temp);
}


QUAD_FORM
QFmv(MATRIX m, MATRIX cov) { 
  QUAD_FORM temp;
  int dim;
  MATRIX P;

  if (m.cols != 1 || m.rows != cov.rows || m.rows != cov.cols) {
    printf("bad input to QFmv\n");
    exit(0);
  }
  dim = m.rows;
  temp.m =  m;
  Mnull_decomp(cov,&temp.null,&temp.fin);
  temp.inf  = Mzeros(dim,0);
  P = Mm(temp.fin,Mt(temp.fin));
  temp.cov = Mm(Mm(P,cov),P);
  temp.S = Msym_range_inv(temp.cov,temp.fin);
  /*  temp.cov = cov;
  temp.null = Mzeros(dim,0);
  temp.fin  = Miden(dim);
  temp.S =   Mi(cov); */
  return(temp);
}


QUAD_FORM
QFmv_simple(MATRIX m, MATRIX cov) { 
  QUAD_FORM temp;
  int dim,i;
  MATRIX P;

  if (m.cols != 1 || m.rows != cov.rows || m.rows != cov.cols) {
    printf("bad input to QFmv\n");
    exit(0);
  }
  dim = m.rows;
  temp.m =  m;
  for (i=0; i < cov.rows; i++) if (cov.el[i][i] < 0) cov.el[i][i] = 0;
  Mnull_decomp_thresh(cov,&temp.null,&temp.fin,.0001);
  temp.inf  = Mzeros(dim,0);
  P = Mm(temp.fin,Mt(temp.fin));
  temp.cov = Mm(Mm(P,cov),P);
  temp.S = Msym_range_inv(temp.cov,temp.fin);
  /*  if (cov.rows == 1) {
    Mp(cov);
    Mp(temp.S);    
    }*/
  /*  temp.cov = cov;
  temp.null = Mzeros(dim,0);
  temp.fin  = Miden(dim);
  temp.S =   Mi(cov); */
  return(temp);
}


QUAD_FORM
QFmv_simplify(MATRIX m, MATRIX cov) { 
  QUAD_FORM temp;
  int dim;
  MATRIX P;

  if (m.cols != 1 || m.rows != cov.rows || m.rows != cov.cols) {
    printf("bad input to QFmv\n");
    exit(0);
  }
  dim = m.rows;
  temp.m =  m;
  Mnull_decomp_thresh(cov,&temp.null,&temp.fin,.01);
  temp.inf  = Mzeros(dim,0);
  P = Mm(temp.fin,Mt(temp.fin));
  temp.cov = Mm(Mm(P,cov),P);
  temp.S = Msym_range_inv(temp.cov,temp.fin);
  /*  temp.cov = cov;
  temp.null = Mzeros(dim,0);
  temp.fin  = Miden(dim);
  temp.S =   Mi(cov); */
  return(temp);
}

QUAD_FORM
QFmv_t_simplify(MATRIX m, MATRIX cov) { 
  QUAD_FORM temp;
  int dim,i;
  MATRIX P;
  float t;

  if (m.cols != 1 || m.rows != cov.rows || m.rows != cov.cols) {
    printf("bad input to QFmv\n");
    exit(0);
  }
  if (cov.cols != 2) {
    printf("don't understand input in QFmv_t_simplify()\n");
    exit(0);
  }
  /*  if (m.el[0][0] < .02 && sqrt(cov.el[0][0]) < .02) m.el[0][0] = cov.el[0][0] = cov.el[0][1] = cov.el[1][0] = 0;
      if (m.el[1][0] < .05 && sqrt(cov.el[1][1]) < .05) m.el[1][0] = cov.el[1][1] = cov.el[0][1] = cov.el[1][0] = 0;*/

  /*  if (sqrt(cov.el[0][0]) < .002) m.el[0][0] = cov.el[0][0] = cov.el[0][1] = cov.el[1][0] = 0.;*/
  if (sqrt(cov.el[1][1])  < .01) cov.el[1][1] = cov.el[0][1] = cov.el[1][0] = 0.;
  /*  for (i=0; i < 2; i++) if (cov.el[i][i] == 0.) m.el[i][0] = 0;*/
  dim = m.rows;
  temp.m =  m;
  Mnull_decomp(cov,&temp.null,&temp.fin);
  temp.inf  = Mzeros(dim,0);
  P = Mm(temp.fin,Mt(temp.fin));
  /*  temp.m = Mm(P,temp.m);  /* don't allow m to be non-negative in determinisitic past of covariance */
  temp.cov = Mm(Mm(P,cov),P);
  temp.S = Msym_range_inv(temp.cov,temp.fin);
  /*  temp.cov = cov;
  temp.null = Mzeros(dim,0);
  temp.fin  = Miden(dim);
  temp.S =   Mi(cov); */
  return(temp);
}

#define CONSTRAINED_POS_STD  .05  /*.1  /*.005  /*.1 /*.005 /*.001*/

QUAD_FORM
QFpos_near(int dim) { /* assuming pos is 0th coord */
  QUAD_FORM temp;
  int i;
  float var;

  var = CONSTRAINED_POS_STD * CONSTRAINED_POS_STD;  
  temp.m =  Mzeros(dim,1);
  temp.cov = Mzeros(dim,dim);
  temp.cov.el[0][0] = var;
  temp.null = Mzeros(dim,0);
  temp.inf  = Mzeros(dim,dim-1);
  for (i=0; i < dim-1; i++) temp.inf.el[i+1][i] = 1;
  temp.fin  = Mzeros(dim,1);
  temp.fin.el[0][0] = 1;
  temp.S =   Mzeros(dim,dim);
  temp.S.el[0][0] = 1./var;
  return(temp);
}

QUAD_FORM
QFpos_var(int dim, float v) { /* assuming rate is 1st coord */
  QUAD_FORM temp;
  int i;
  float var;

  temp.m =  Mzeros(dim,1);
  temp.cov = Mzeros(dim,dim);
  temp.cov.el[0][0] = v;
  temp.null = Mzeros(dim,0);
  temp.inf  = Mzeros(dim,dim-1);
  for (i=0; i < dim-1; i++) temp.inf.el[i+1][i] = 1;
  temp.fin  = Mzeros(dim,1);
  temp.fin.el[0][0] = 1;
  temp.S =   Mzeros(dim,dim);
  temp.S.el[0][0] = 1./v;
  return(temp);
}




QUAD_FORM
QFpos_tempo_near() { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;

  var[0] = square(/*.1*/ .1);
  var[1] = square(.1);
  var[2] = HUGE_VAL;
  m = Mzeros(ACCOM_DIM,1);
  return(QFindep(ACCOM_DIM,  m, var));
}


QUAD_FORM
QFpos_tempo_sim() { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;

  var[0] = square(0.);
  var[1] = square(0.);
  var[0] = square(.1);
  var[1] = square(.1);
  m = Mzeros(ACCOM_DIM,1);
  return(QFindep(ACCOM_DIM,  m, var));
}

QUAD_FORM
QFpos_tempo_very_sim() { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;

  var[0] = square(0.);
  var[1] = square(0.);
  var[0] = square(.01);
  var[1] = square(.01);
  m = Mzeros(ACCOM_DIM,1);
  return(QFindep(ACCOM_DIM,  m, var));
}

QUAD_FORM
QFpos_sim() { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;

  var[0] = square(.1);
  var[1] = HUGE_VAL;
  m = Mzeros(ACCOM_DIM,1);
  return(QFindep(ACCOM_DIM,  m, var));
}



QUAD_FORM
QFpos_same(int dim) { /* assuming pos is 0th coord */
  QUAD_FORM temp;
  int i;
  MATRIX v;

  temp.m =  Mzeros(dim,1);
  temp.cov = Mzeros(dim,dim);
  temp.null = Mzeros(dim,0);
  temp.inf  = Mzeros(dim,0);
  temp.fin  = Mzeros(dim,0);
  for (i=0; i < dim; i++) {
    v = Mzeros(dim,1);
    v.el[i][0] = 1;
    if (i == 0) temp.null = Mcat(temp.null,v);
    else  temp.inf = Mcat(temp.inf,v);
  }
  temp.S =   Mzeros(dim,dim);
  return(temp);
}

QUAD_FORM
QFsphere(int dim, MATRIX m, float var) { /* spherically symmetric dist */
  QUAD_FORM temp;

  if (m.rows != dim) {
    printf("problem in QFpoint\n");
    exit(0);
  }
  if (m.cols != 1) {
    printf("m not vector in QFpoint\n");
    exit(0);
  }
  temp.m =  m;
  temp.cov =  Msc(Miden(dim),var);
  temp.null = Mzeros(dim,0);
  temp.inf  = Mzeros(dim,0);
  temp.fin  = Miden(dim);
  temp.S =   Msc(Miden(dim),1./var);
  return(temp);
}




#define XYZ /*1.*/ .001


QUAD_FORM
QFimmediate_tempo_change(MATRIX A, float length) { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i,dim;
  QUAD_FORM ret;

  dim = ACCOM_DIM;
  for (i=0; i < dim; i++)  var[i] = (i == 1) ? /*.1*/100.*length : 0.;
  m = Mzeros(dim,1);
  ret = QFindep(dim,  m, var);
  ret = QFxform(ret,A);
  return(ret);
}

QUAD_FORM
QFhold_steady(void) { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i,dim;
  QUAD_FORM ret;

  dim = ACCOM_DIM;
  m = Mzeros(dim,1);
  for (i=0; i < dim; i++)  var[i] = 0.;
  ret = QFindep(dim,  m, var);
  return(ret);
}


#define POS_SLOP_STD_DEV  .05/*.3*/ /* in secs / meas */


QUAD_FORM
QFpos_slop(float length) { /* assuming rate is 1st coord */
  float var[100],v;
  MATRIX m;
  int i,dim;
  QUAD_FORM ret;

  dim = ACCOM_DIM;
  m = Mzeros(dim,1);
  v = POS_SLOP_STD_DEV*POS_SLOP_STD_DEV*length;
  for (i=0; i < dim; i++)  var[i] = (i == 0) ? v : 0.;
  ret = QFindep(dim,  m, var);
  return(ret);
}


QUAD_FORM
QFimmediate_tempo_change_slop(MATRIX A, float length) { /* assuming rate is 1st coord */
  float var[100],std;
  MATRIX m;
  int i,dim;
  QUAD_FORM ret,slop;  /* the position "slop" doesn't depend on the length of note */

  dim = ACCOM_DIM;
  for (i=0; i < dim; i++)  var[i] = (i == 1) ? 10. /*1./*10.*//*1.*/*length : 0.;
  m = Mzeros(dim,1);
  ret = QFindep(dim,  m, var);
  ret = QFxform(ret,A);
  std = .1;
  for (i=0; i < dim; i++)  var[i] = (i == 0) ? std*std : 0.;
  slop = QFindep(dim,  m, var);
  ret = QFindep_sum(ret,slop);
  /*  printf("length = %f\n",length);
Mp(slop.cov);
Mp(ret.cov);  */
  return(ret);
}

QUAD_FORM
QFfollow_at_all_costs(MATRIX A, float length) { /* assuming rate is 1st coord */
  float var[100],std;
  MATRIX m;
  int i,dim;
  QUAD_FORM ret,slop;  /* the position "slop" doesn't depend on the length of note */

  dim = ACCOM_DIM;
  for (i=0; i < dim; i++)  var[i] = (i == 1) ? 1./*10.*//*1.*/*length : 0.;
  m = Mzeros(dim,1);
  ret = QFindep(dim,  m, var);
  ret = QFxform(ret,A);
  std = 10.;
  for (i=0; i < dim; i++)  var[i] = (i == 0) ? std*std : 0.;
  slop = QFindep(dim,  m, var);
  ret = QFindep_sum(ret,slop);
  return(ret);
}

QUAD_FORM
QFmeas_size_slop(float std_dev) { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i,dim;
  QUAD_FORM ret;

  dim = ACCOM_DIM;
  for (i=0; i < dim; i++)  var[i] = (i == 1) ? std_dev*std_dev : 0.;
  m = Mzeros(dim,1);
  ret = QFindep(dim,  m, var);
  return(ret);
}

QUAD_FORM
QFtempo_change(float length) { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i,dim;
  QUAD_FORM ret;

  dim = ACCOM_DIM;
  for (i=0; i < dim; i++)  var[i] = (i == 1) ? 4*length : 0.;
  m = Mzeros(dim,1);
  ret = QFindep(dim,  m, var);
  return(ret);
}

QUAD_FORM
QFtempo_change_pos_slop(float length) { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;
  QUAD_FORM ret;


  var[0] = .050; /*.001; /*.3; */
  var[1] = /*.1*/ 4*length; /*.01*length;*/
  var[2] = 0;
  m = Mzeros(ACCOM_DIM,1);
  ret = QFindep(ACCOM_DIM,  m, var);
  return(ret);
}

QUAD_FORM
QFtempo_change_pos_slop_parms(float pos, float size) { /* assuming rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;
  QUAD_FORM ret;


  var[0] = pos;
  var[1] = size;
  /*  var[2] = 0;*/
  m = Mzeros(ACCOM_DIM,1);
  ret = QFindep(ACCOM_DIM,  m, var);
  return(ret);
}


//CF:  create 2D quadform with zero mean; indenpendent variance components of (pos,size).
QUAD_FORM
QFpos_meassize_update(float pos, float size) { /* assuming position is zeroth coord and rate is 1st coord */
  float var[100];
  MATRIX m;
  int i;
  QUAD_FORM ret;


  var[0] = pos;
  var[1] = size;
  /*  var[2] = 0;*/
  m = Mzeros(ACCOM_DIM,1);
  ret = QFindep(ACCOM_DIM,  m, var);
  return(ret);
}


QUAD_FORM
QFnull_inf(int dim, int *null) { /* a qf having only inf and null spaces.  the non-zero 
				   comps of null[0...dim-1] are the null space */
  QUAD_FORM temp;
  int i;
  MATRIX v;

  temp.m =  Mzeros(dim,1);
  temp.cov =  Mzeros(dim,dim);
  temp.S  =  Mzeros(dim,dim);
  temp.fin  = Mzeros(dim,0);
  temp.null = Mzeros(dim,0);
  temp.inf  = Mzeros(dim,0);
  for (i=0; i < dim; i++) { 
    v = Mzeros(dim,1);
    v.el[i][0] = 1;
    if (null[i]) temp.null = Mcat(temp.null,v);
    else  temp.inf = Mcat(temp.inf,v);
  }
  return(temp);
}


QUAD_FORM
QFunif_accel_zero() {
  int null[ACCOM_DIM],i;
  QUAD_FORM qf;
  MATRIX m;
  float var[ACCOM_DIM];

  for (i=0; i < ACCOM_DIM; i++) null[i] = (i == 2);
  qf = QFnull_inf(ACCOM_DIM,null); 
  /*  QFprint(qf);
    var[0] = HUGE_VAL;
  var[1] = 0.;
  var[2] = 0.;
  m = Mzeros(ACCOM_DIM,1);
  qf = QFindep(ACCOM_DIM,  m, var); */
  
  return(qf);
}

QUAD_FORM
QFmeas_size2(float size, float std) {
  int null[ACCOM_DIM],i;
  QUAD_FORM qf;
  MATRIX m;
  float var[ACCOM_DIM];

  var[0] = HUGE_VAL;
  var[1] = std*std;
  m = Mzeros(ACCOM_DIM,1);
  m.el[1][0] = size;
  qf = QFindep(ACCOM_DIM,  m, var);
  return(qf);
}

//CF:  Just for cue points:
//CF:  Create a prior for (t,s) state (size for size of whole-note in secs, time of event)
//CF:  For cue point, the time is flat degenerate; and the invtempo is specified by (mu,sigma)=(size,std)
//CF:  (see QUAD_FORM docs for disussion of representaiton)
QUAD_FORM
QFmeas_size(float size, float std) {
  int null[ACCOM_DIM],i;
  QUAD_FORM qf;
  MATRIX m;
  float var[ACCOM_DIM];

  var[0] = HUGE_VAL;  //CF:  never compute with this -- its used to request a degenerate represenation
  var[1] = std*std;
//  var[2] = 0.;
  m = Mzeros(ACCOM_DIM,1);
  m.el[1][0] = size;
  qf = QFindep(ACCOM_DIM,  m, var);
  return(qf);
}

QUAD_FORM
QFmeas_size_at(float size, float std) {  /* the atempo model (0th comp = pos; 1st comp = meas size; 2nd comp = global meas size */
  int null[BACKBONE_ATEMPO_DIM],i;
  QUAD_FORM qf1,qf2;
  MATRIX m,A;
  float var[BACKBONE_ATEMPO_DIM];

  var[0] = HUGE_VAL;
  var[1] = std*std;
  m = Mzeros(2,1);

  m.el[1][0] = size;
  qf1 = QFindep(2,  m, var);
  A = Mzeros(3,2);  A.el[2][1] = A.el[1][1] = A.el[0][0] = 1;
  qf2 = QFxform(qf1,A);
  return(qf2);
}

QUAD_FORM
QFmeas_size_at_old(float size, float std) {  /* the atempo model (0th comp = pos; 1st comp = meas size; 2nd comp = global meas size */
  int null[BACKBONE_ATEMPO_DIM],i;
  QUAD_FORM qf;
  MATRIX m;
  float var[BACKBONE_ATEMPO_DIM];

  var[0] = HUGE_VAL;
  var[1] = std*std;
  var[2] = std*std;;
  m = Mzeros(BACKBONE_ATEMPO_DIM,1);
  m.el[1][0] = m.el[2][0] = size;
  qf = QFindep(BACKBONE_ATEMPO_DIM,  m, var);
  return(qf);
}

QUAD_FORM
QFatempo(float size) {
  int null[ACCOM_DIM],i;
  QUAD_FORM qf;
  MATRIX m;
  float var[ACCOM_DIM];

  var[0] = HUGE_VAL;
  var[1] = 0.;
  m = Mzeros(ACCOM_DIM,1);
  m.el[1][0] = size;
  qf = QFindep(ACCOM_DIM,  m, var);
  return(qf);
}

QUAD_FORM
/* after a r.v. is observed its distribution has inf variance for unobserved
comps and 0 variance for observed components */
QFobs(int dim, int fixed, float obs) {
  QUAD_FORM qf;
  int i,j,a=0;
  
  qf.m = Mzeros(dim,1);
  qf.m.el[fixed][0] = obs;
  qf.cov = Mzeros(dim,dim);
  qf.S = Mzeros(dim,dim);
  qf.null = Mzeros(dim,1);  /* zero variance */
  qf.inf = Mzeros(dim,dim-1);  /* inf variance */
  qf.fin = Mzeros(dim,0);
  for (i=0; i < dim; i++) {
    if (i == fixed) {
      for (j=0; j < dim; j++) qf.null.el[j][0] = (j == i) ? 1 : 0;
    }
    else {
      for (j=0; j < dim; j++) qf.inf.el[j][a] = (j == i) ? 1 : 0;
      a++;
    }
  }
  return(qf);
}


QUAD_FORM 
QFprint(QUAD_FORM qf) {
  printf("m is:\n");
  Mp(qf.m);
  printf("cov is (ignoring infinite directions):\n");
  Mp(qf.cov);
  printf("S is (ignoring infinite directions):\n");
  Mp(qf.S);
  printf("null space of covariance:\n");
  Mp(qf.null);
  printf("inf space of covariance:\n");
  Mp(qf.inf);
  printf("finite space of covariance:\n");
  Mp(qf.fin);
}



void
QFprint_file(QUAD_FORM qf, FILE *fp) {
  fprintf(fp,"m is:\n");
  Mp_file(qf.m,fp);
  fprintf(fp,"cov is (ignoring infinite directions):\n");
  Mp_file(qf.cov,fp);
  fprintf(fp,"S is (ignoring infinite directions):\n");
  Mp_file(qf.S,fp);
  fprintf(fp,"null space of covariance:\n");
  Mp_file(qf.null,fp);
  fprintf(fp,"inf space of covariance:\n");
  Mp_file(qf.inf,fp);
  fprintf(fp,"finite space of covariance:\n");
  Mp_file(qf.fin,fp);
}


QUAD_FORM 
QFread_file(FILE *fp) {
  QUAD_FORM qf;
  char s[100],c;

  fscanf(fp,"m is:\n");
  qf.m = Mr_file(fp);
  fscanf(fp,"cov is (ignoring infinite directions):\n");   
  qf.cov = Mr_file(fp);
  fscanf(fp,"S is (ignoring infinite directions):\n");
  qf.S = Mr_file(fp);
  fscanf(fp,"null space of covariance:\n");
  qf.null = Mr_file(fp);
  fscanf(fp,"inf space of covariance:\n");
  qf.inf = Mr_file(fp);
  fscanf(fp,"finite space of covariance:\n");
  qf.fin = Mr_file(fp);
  return(qf);
}






static MATRIX
infinite_mean(MATRIX m1, MATRIX m2, MATRIX U1, MATRIX U2, MATRIX U) {
  /* qf1 has mean m1 with 0-variance orthonormal basis U1
     qf2 has mean m2 with 0-variance orthonormal basis U2 
     U is orthonormal basis for U1+U2
     return mean m satisfying both sets of constraints.
     the mean, m, must satistfy (among other things) the     relations:
       P1*m = P1*m1
       P2*m = P2*m2
     where P1 and P2 are projections into the spaces of U1 and
     U2.  The matrices P1 and P2 are
       P1 = U1*t(U1)
       P2 = U2*t(U2)
     where U1 and U2 have as columns orthonormal basis elements of the
     infinite spaces.  Thus we can solve
       t(U1)*m = t(U1)*m1
       t(U2)*m = t(U2)*m2
     and the resulting m will satisfy the projection equations.
     We seek a solution that lies in U1+U2 since
     we will solve for the part of m lying in the complement of U
     later.  Thus we would like to solve:
       t(U1)*U*x = t(U1)*m1
       t(U2)*U*x = t(U2)*m1
     We will find m by solving these
     equations simultaneouly:
       t(U1)*U        t(U1)*m1
       ------  x   = -----------
       t(U2)*U        t(U2)*m2
     and letting m = U*x. */


  /* by the way.  i think this is equivalent to solving
     (P1+P2)Ux  = P1*m1 + P2*m2 
     and letting m = Ux.  note that (i think) R(P1+P2) = R(U) */
  MATRIX b,A,x,P1,P2;
  float error;

  /*printf("m1 = \n");
Mp(m1);
printf("m2 = \n");
Mp(m2);
printf("U1 = \n");
Mp(U1);
printf("U2 = \n");
Mp(U2);
printf("U = \n");
Mp(U);*/


  if (U.cols == 0) return(Mzeros(m1.rows,1));
  if (U1.cols == 0) return(Mm(Mm(U2,Mt(U2)),m2));
  if (U2.cols == 0) return(Mm(Mm(U1,Mt(U1)),m1));

  /*  P1 = Mm(U1,Mt(U1));  this calculation is not correct.  means must
      agree in intersection of two null spaces 
  P2 = Mm(U2,Mt(U2));
  error = Mnorm(Ms(Mm(P1,m1),Mm(P2,m2)));
  if (error > .01) {
    printf("the means do not agree in the null spaces\n");
    exit(0);
  } */

  /*printf("hello\n");*/
  b = Mvcat(Mm(Mt(U1),m1),Mm(Mt(U2),m2));

  A = Mvcat(Mm(Mt(U1),U),Mm(Mt(U2),U));   /* A has more rows than cols */
  x= Msolve(A,b);
  return(Mm(U,x));
}





void
matrix_infinite_mean(MATRIX U1, MATRIX U2, MATRIX U, MATRIX *R1, MATRIX *R2) {
  /*    Suppose we have two quadratic forms, 
	the first  has 0-variance space U1 (o.n. basis),
	the second has 0-variance space U2 (o.n. basis),
	U is orthonormal basis for (U1,U2)
	Set R1 R2 s.t. 
	R1*m1+R2*m2 = m where m satisfies both sets of constraints, ie
	P1*m = P1*m1
	P2*m = P2*m2
	where P1 and P2 are projections into the spaces of U1 and
	U2.  The matrices P1 and P2 are
	P1 = U1*t(U1)
	P2 = U2*t(U2)
	by the way.  i think this is equivalent to solving
	(P1+P2)m  = P1*m1 + P2*m2 
	with m in R(P1+P2)
	note that (i think) R(P1+P2) = R(U) */
  MATRIX b,A,x,P1,P2,inv,m,B1,B2,Uint,Pint,P2var,m2var;
  float error;

  /*printf("matrix_infinite_mean\n");
printf("U1\n");
Mp(U1);
printf("U2\n");
Mp(U2); */

  if (U.cols == 0) {
    *R1 = Mzeros(U1.rows,U1.rows);
    *R2 = Mzeros(U2.rows,U2.rows);
    return;
  }
  P1 = Mm(U1,Mt(U1));
  P2 = Mm(U2,Mt(U2));
  if (U1.cols == 0 || U2.cols == 0) {
    *R1 = P1;
    *R2 = P2;
    return;
  }
  
  /*  printf("hello\n"); */
  Uint = Mintersect(U1,U2);

  if (Uint.cols > 0) {
    Pint = Mm(Uint,Mt(Uint));
    P2var = Ms(P2,Pint);
  }
  else {
    Pint = Miden(Uint.rows);
    P2var = P2;
  }
  A = Ma(P1,P2var);

  /*     b = Ma(Mm(P1,m1),Mm(P2var,m2));      */
  /*  m = Msym_solve(A,b,U);*/

  inv = Msym_range_inv(A,U);
  *R1 = Mm(inv,P1);
  *R2 = Mm(inv,P2var);

  /* if m1 and m2 are variables then 
     m(m1,m2)  = inv(A)*b = inv(A)*(P1*m1 + P2var*m2var) */

#ifdef DEBUG
  /*  P1*m1 = P1*m  subject to Pint*m1 = Pint*m2
      
      P1*m1 = P1*m 
            = P1*(R1*m1 + R2*m2)  
            = P1*(R1*m1 + R2*(P2var*m2 + Pint*m2))
	    = P1*(R1*m1 + R2*(P2var*m2 + Pint*m1))
      ==>	    
      
      P1 = P1(R1 + R2*Pint) 
      P1*R2*P2var = 0* */
  error = Mnorm(Ms(P1,Mm(P1,Ma(*R1,Mm(*R2,Pint)))));
  if (error > .001) {
    printf("problem in matrix_infinite_mean()\n");
    Mp(P1);
    Mp(*R1);
    exit(0);
  }
  error = Mnorm(Mm(P1,Mm(*R2,P2var)));
  if (error > .001) {
    printf("problem in matrix_infinite_mean()\n");
    exit(0);
  }
  /*       P2*m2 = P2*m  subject to Pint*m1 = Pint*m2

     ==>   P2var*m2 +Pint*m1  = P2var*m2 +Pint*m2  
                              = P2*m2
                              = P2*(R1*m1 + R2*m2) 
                              = P2*(R1*m1 + R2*(P2var*m2 + Pint*m2))
			      = P2*(R1*m1 + R2*(P2var*m2 + Pint*m1))
			      = (P2*R1 + P2*R2*Pint)*m1 + P2*R2*P2var*m2
      ==>  P2var = P2*R2*P2var
      and  Pint = P2*R1 + P2*R2*Pint = 0 */
  error = Mnorm(Ms(P2var,Mm(P2,Mm(*R2,P2var))));
  if (error > .001) {
    printf("problem 1 in matrix_infinite_mean()\n");
    exit(0);
  }
  error = Mnorm(Ms(Pint,Ma(Mm(P2,*R1),Mm(Mm(P2,*R2),Pint))));
  if (error > .001) {
    printf("problem 2 in matrix_infinite_mean()\n");
    /*    exit(0); */
  }
#endif 
}




MATRIX
conditional_mean(MATRIX m1, MATRIX m2, MATRIX S11, MATRIX S12, MATRIX x2) {
  /* x,y have dist N(m1,m2, S11,S12)
                              S21,S22

  find conditional mean given that y = x2 */

 			       
  MATRIX b,diff,cond_mean;

  b = Mm(S12,Ms(m2,x2));
  diff = Msolve(S11,b);
  cond_mean = Ma(diff,m1);
  return(cond_mean);
}
  






MATRIX
restricted_mean(QUAD_FORM qf, MATRIX V1, MATRIX V2, MATRIX C, MATRIX fin) {
  /* if the quadratic form qf = m=Ax,S is restricted to the 
     set V2'z = c = Cx
     where V = (V1 V2) is unitary, if R is return value resulting mean is Rx */
  QUAD_FORM res;
  MATRIX SS,m1,m2,S11,S12,cm,inv,V1S;
  MATRIX b,diff,range,mean;

  
  if (V1.cols == 0) {
    return(Mm(V2,C));
  }
  if (V2.cols == 0) return(qf.m);

  m1 = Mm(Mt(V1),qf.m); /* m1*x,m2*x is mean of qf for y = V'z */
  m2 = Mm(Mt(V2),qf.m);
  /*  V1S = Mm(Mt(V1),qf.S); */
  S11 = Mm(Mm(Mt(V1),qf.S),V1);  /* S11, S12 is 1st row of covariance for y */
  S12 = Mm(Mm(Mt(V1),qf.S),V2); 
  /*  S11 = Mm(V1S,V1);
  S12 = Mm(V1S,V2); */
  range = Mm(Mt(V1),fin);  /* the range of S11 */
  b = Mm(S12,Ms(m2,C)); 
  /*  b = Mm(S12,Ms(C,m2)); */
   inv = Msym_range_inv(S11,range);
  diff = Mm(inv,b); 
  /*  diff = Msym_solve(S11,b,range); */
  cm = Ma(diff,m1);
  mean = Ma(Mm(V2,C),Mm(V1,cm));
  return(mean);
}
  

static QUAD_FORM
enforce_constraints(QUAD_FORM qf, MATRIX mu) {
  /* change qf to respect the constraint Px = mu where P = Mm(qf.null,Mt(qf.null)).
     in doing this: 
     1) change m
     2) change S  to respect qf.fin,
     3) set cov */
  
  MATRIX not_null,c,P;

  not_null = Mcat(qf.fin,qf.inf);
  c = Mm(Mt(qf.null),mu);  /* the constraint is qf.null'x = c  */
  qf.m = restricted_mean(qf,not_null,qf.null,c,qf.fin);
  P = Mm(qf.fin,Mt(qf.fin));
  qf.S = Mm(P,Mm(qf.S,P));  
  qf.cov = Msym_range_inv(qf.S,qf.fin);
  return(qf);
}


     



/* a r.v. z has guassian dist with mean m and concentration matrix S.  
   suppose we are given that V2*V2'z = c, ie that the projection of
   z onto the subspace V2 is some constant c.
   set matrices A and B s.t the posterior mean of z is A*m + B*c
   the matrix V1,V2 is unitary */
void
matrix_restricted_mean(MATRIX S, MATRIX V1, MATRIX V2, 
		       MATRIX fin, MATRIX *A, MATRIX *B) {
  MATRIX S11,S12,inv,range,prod,V1t;
  
  if (V1.cols == 0) {
    *A = Mzeros(V1.rows,V1.rows);
    *B = Miden(V1.rows);
    return;
  }
  V1t = Mt(V1);
  S11 = Mm(Mm(V1t,S),V1);  /* S11, S12 is 1st row of covariance for y */
  S12 = Mm(Mm(V1t,S),V2);
  range = Mm(V1t,fin);  /* the range of S11 */
  inv = Msym_range_inv(S11,range);
  prod = Mm(Mm(V1,Mm(inv,S12)),Mt(V2));
  *A = Ma(Mm(V1,V1t),prod);
  *B = Ms(Miden(prod.rows),prod);
}
  



  
  
static MATRIX
direct_sum(MATRIX U, MATRIX V) { /* direct sum of two othogonal subspaces */
  MATRIX P,Q,R,ret,t1;

  ret = Mcat(U,V);
  Mnull_decomp(Mt(ret),&t1,&ret);  /* N(ret') comp = R(ret) */
  return(ret);
}
  
static MATRIX
orthog_comp(MATRIX U) { /* orthogonal complement of U */
  MATRIX ret,temp;

  Mnull_decomp(Mt(U),&ret,&temp);  /*N(U')  = R(U) comp  */
  return(ret);
}
  
			   


static void
split_spaces(MATRIX i1, MATRIX s1, MATRIX r1, MATRIX i2, MATRIX s2, MATRIX r2,
	     MATRIX *i3, MATRIX *s3, MATRIX *r3) {
     /* matrix (i1, s1, r1) is unitary as is (i2, s2, r2).  The same
	for (i3,s3,r3) on return.  on return i3 is the intersection
	of the spaces spanned by i1 and i2.  s3 is the direct sum of the
	spaces spanned by s1 and s2.  and r3 is the remainder */
  QUAD_FORM ret;
  MATRIX temp,null,sum_comp,int_comp,t1,t2;

  if (s1.cols == 0) {
    *s3 = s2;
    sum_comp = Mcat(r2,i2);
  }
  else if (s2.cols == 0) {
    *s3 = s1;
    sum_comp = Mcat(r1,i1);
  }
  else {
    temp = Mcat(s1,s2);
    Mnull_decomp(Mt(temp),&sum_comp,s3);
  }
  if (i1.cols == 0 || i2.cols == 0) {
    *i3 = Mzeros(i1.rows,0);
    int_comp = Miden(i1.rows);
  }
  else Minter(i1,i2,i3,&int_comp);

  if (s3->cols == 0) *r3 = int_comp;
  else if (i3->cols == 0) *r3 = sum_comp;
  else {
    temp = Mcat(*i3,*s3);
    Mnull_decomp(Mt(temp),r3,&temp);
  }
}
    

static QUAD_FORM
decompose_sum(QUAD_FORM q1, QUAD_FORM q2, MATRIX *either_fin) {
  QUAD_FORM ret;
  MATRIX temp,null,null_comp,inf_comp,t1,t2,both_null,P,p1,p2;
  float error;
  int count1, count2;

  /* should be done with split_spaces() */

#ifdef DEBUG
   Minter(q1.null,q2.null,&both_null,&temp);
   P = Mm(both_null,Mt(both_null));
   p1 = Mm(P,q1.m);
   p2 = Mm(P,q2.m);
   error = Mnorm(Ms(p1,p2));
   if (error > .001) {
     printf("conflicting constraints  (error = %f)\n",error);
     printf("q1.null\n");
     Mp(q1.null);
     printf("q2.null\n");
     Mp(q2.null);
     printf("both_null\n");
     Mp(both_null);
     printf("p1\n");
     Mp(p1);
     printf("p2\n");
     Mp(p2);
     exit(0);
   }
#endif
  


  if (q1.null.cols == 0) {
    ret.null = q2.null;
    null_comp = Mcat(q2.fin,q2.inf);
  }
  else if (q2.null.cols == 0) {
    ret.null = q1.null;
    null_comp = Mcat(q1.fin,q1.inf);
  }
  else {
    temp = Mcat(q1.null,q2.null);
    Mnull_decomp(Mt(temp),&null_comp,&ret.null);
  }

  if (q1.inf.cols == 0 || q2.inf.cols == 0) {
    ret.inf = Mzeros(q1.inf.rows,0);
    inf_comp = Miden(q1.inf.rows);
  }
  else {


    Minter(q1.inf,q2.inf,&ret.inf,&inf_comp); 
    /*    count1 = ret.inf.cols;

    temp = Mcat(Mcat(q1.null,q1.fin),Mcat(q2.null,q2.fin));
    Mnull_decomp(Mt(temp),&ret.inf,&inf_comp);
    count2 = ret.inf.cols;
    if (count1 != count2) {
      printf("disagreement\n");
      Mp(q1.inf);
      Mp(q2.inf);  
      printf("other way\n");
      Mnull_decomp(Mt(temp),&ret.inf,&inf_comp);
      printf("Minter\n");
      Minter(q1.inf,q2.inf,&ret.inf,&inf_comp); 
      q1.inf.el[1][1] += .0000001;
      printf("Minter2\n");
      Minter(q1.inf,q2.inf,&ret.inf,&inf_comp); 
      print_flag = 0;
      
      }*/
      

		
  }

  if (ret.null.cols == 0) ret.fin = inf_comp;
  else if (ret.inf.cols == 0) ret.fin = null_comp;
  else {
    temp = Mcat(ret.null,ret.inf);
    Mnull_decomp(Mt(temp),&ret.fin,&temp);
  }

  if (q1.fin.cols == q1.fin.rows)  *either_fin = Miden(q1.fin.rows);
  else if (q2.fin.cols == q2.fin.rows)  *either_fin = Miden(q2.fin.rows);
  else {
    /*    t1 = Mcat(q1.inf,q1.null);
    t2 = Mcat(q2.inf,q2.null); */
    /*    Minter(t1,t2,&temp,either_fin);  /* not both non-finite */
    *either_fin = Morthogonalize(Mcat(q1.fin,q2.fin));
  }
  return(ret);
}
 

static QUAD_FORM
decomp_sum(QUAD_FORM q1, QUAD_FORM q2) {
  QUAD_FORM ret;
  MATRIX temp,null,null_comp,inf_comp,t1,t2,both_null,P,p1,p2;
  float error;
  int count1, count2;


#ifdef DEBUG
   Minter(q1.null,q2.null,&both_null,&temp);
   P = Mm(both_null,Mt(both_null));
   p1 = Mm(P,q1.m);
   p2 = Mm(P,q2.m);
   error = Mnorm(Ms(p1,p2));
   if (error > .001) {
     printf("conflicting constraints  (error = %f)\n",error);
     printf("q1.null\n");
     Mp(q1.null);
     printf("q2.null\n");
     Mp(q2.null);
     printf("both_null\n");
     Mp(both_null);
     printf("p1\n");
     Mp(p1);
     printf("p2\n");
     Mp(p2);
     exit(0);
   }
#endif
  

  if (q1.null.cols == 0) {
    ret.null = q2.null;
    null_comp = Mcat(q2.fin,q2.inf);
  }
  else if (q2.null.cols == 0) {
    ret.null = q1.null;
    null_comp = Mcat(q1.fin,q1.inf);
  }
  else {
    temp = Mcat(q1.null,q2.null);
    Mnull_decomp(Mt(temp),&null_comp,&ret.null);
  }

  if (q1.inf.cols == 0 || q2.inf.cols == 0) {
    ret.inf = Mzeros(q1.inf.rows,0);
    inf_comp = Miden(q1.inf.rows);
  }
  else   Minter(q1.inf,q2.inf,&ret.inf,&inf_comp); 
  if (ret.null.cols == 0) ret.fin = inf_comp;
  else if (ret.inf.cols == 0) ret.fin = null_comp;
  else {
    temp = Mcat(ret.null,ret.inf);
    Mnull_decomp(Mt(temp),&ret.fin,&temp);
  }
  return(ret);
}
 

  
  static MATRIX
new_infinite_mean(MATRIX m1, MATRIX m2, MATRIX U1, MATRIX U2, MATRIX U) {
  /* qf1 has mean m1 with 0-variance orthonormal basis U1
     qf2 has mean m2 with 0-variance orthonormal basis U2 
     U is orthonormal basis for U1+U2
     return mean m must be in U and satisfy both sets of constraints.
     return value is undefined if constraints not consistent.
     
     We assume that the equations can be satisfied.
     The mean, m = Ux, must satistfy (among other things) the     relations:
       P1*Ux = P1*m1
       P2*Ux = P2*m2
     where P1 and P2 are projections into the spaces of U1 and
     U2.  The matrices P1 and P2 are
       P1 = U1*t(U1)
       P2 = U2*t(U2)
     where U1 and U2 have as columns orthonormal basis elements of the
     infinite spaces. 
     This is equivalent to:
       (P1 + P2)Ux = P1*m1 + P2*m2
     since clearly the first system implies the second and both solution
     sets contain a single point since the matricies (P1 + P2)U and 
       P1*U
       P2*U
     have trivial null space.  The latter equation is equivalent to 
       t(U)*(P1 + P2)Ux = t(U)*(P1*m1 + P2*m2)*/
           
  MATRIX b,A,x,P1,P2,m,S;
  double error;
  extern int global_flag;


  if (U.cols == 0) return(Mzeros(m1.rows,1));
  if (U1.cols == 0) return(Mm(Mm(U2,Mt(U2)),m2));
  if (U2.cols == 0) return(Mm(Mm(U1,Mt(U1)),m1));

  P1 = Mm(U1,Mt(U1));
  P2 = Mm(U2,Mt(U2));
  S = Ma(P1,P2);
  b = Ma(Mm(P1,m1),Mm(P2,m2));
  m = Msym_solve(S,b,U);

  error = Mnorm(Mm(Mt(U1),Ms(m,m1))) + Mnorm(Mm(Mt(U2),Ms(m,m2)));
  if (error > .0001) {
    printf("trying to solve conflicting constraints (%f)\n",error);
    /*    global_flag = 1;*/
  }  
  return(m);
}



QUAD_FORM 
QFplus_sep(QUAD_FORM qf1, QUAD_FORM qf2) {
  /* QFplus separated into two stages --- 
     one for null,inf,fin spaces and one for remainder */
  MATRIX m0,temp,inf_comp,null_comp,P,A;
  float error;
  QUAD_FORM ret,xxx;
  extern int global_iter;

  if (qf1.inf.rows == qf1.inf.cols) return(qf2);
  if (qf2.inf.rows == qf2.inf.cols) return(qf1);

#ifdef DEBUG
  QFcheck(qf1);
  QFcheck(qf2);
#endif


  ret = decomp_sum(qf1, qf2);

  /*  Mnull_decomp(Mt(Mcat(qf1.null,qf2.null)),&null_comp,&ret.null);
  Minter(qf1.inf,qf2.inf,&ret.inf,&inf_comp); 
  Mnull_decomp(Mt(Mcat(ret.null,ret.inf)),&ret.fin,&temp);  */

  A =  Mcat(ret.inf,ret.fin);
  P = Mm(A,Mt(A));
  ret.S = Mm(Mm(P,Ma(qf1.S,qf2.S)),P);
  ret.cov = Msym_range_inv(ret.S,ret.fin);
  m0 = new_infinite_mean(qf1.m, qf2.m, qf1.null, qf2.null, ret.null);
  ret.m = Ma(m0,Mm(ret.cov,Ma(Mm(qf1.S,Ms(qf1.m,m0)),Mm(qf2.S,Ms(qf2.m,m0)))));
#ifdef DEBUG
  QFcheck(ret);
#endif
  return(ret);
}

  
QUAD_FORM  /* assumes U,D format */
QFplus(QUAD_FORM qf1, QUAD_FORM qf2) {
  QUAD_FORM ret,temp;
  MATRIX m_inf,b,either_fin,P;
  int x,y;

  if (qf1.inf.rows == qf1.inf.cols) return(qf2);
  if (qf2.inf.rows == qf2.inf.cols) return(qf1);


#ifdef DEBUG
  QFcheck(qf1);
  QFcheck(qf2);
#endif
  ret = decompose_sum(qf1,qf2,&either_fin); /* compute ret.null, ret.fin, ret.inf, either_fin */

  /*ret.cov = Malloc(0,0);
ret.S = Malloc(0,0);
ret.m = Malloc(0,0);
printf("after decomponse\n");
QFprint(ret);
printf("qf1.S and qf2.S\n");
Mp(qf1.S);
Mp(qf2.S); */
  ret.S = Ma(qf1.S, qf2.S); 
  /*printf("ret.S\n");
Mp(ret.S); */
  b = Ma(Mm(qf1.S,qf1.m),Mm(qf2.S,qf2.m));
  ret.m = Msym_solve(ret.S,b,either_fin);  /* finsum.S has range either_fin */
  /*printf("ret.m = \n");
Mp(ret.m); */
  m_inf = infinite_mean(qf1.m, qf2.m, qf1.null, qf2.null, ret.null);
  ret = enforce_constraints(ret, m_inf);
  /*  if (thread_nesting) { Mp(m_inf); Mp(ret.m); Mp(ret.S); } */


  ret.m = Ms(ret.m,Mm(Mm(ret.inf,Mt(ret.inf)),ret.m));
  


#ifdef DEBUG
  QFcheck(ret);
#endif

  /*  temp = QFplus_sep(qf1,qf2);
      QFcompare(temp,ret); */

  return(ret);
}
  


QUAD_FORM  /* assumes U,D format */
matrix_QFplus(QUAD_FORM qf1, QUAD_FORM qf2, MATRIX *A1, MATRIX *A2) {
  /* on return the mean of the product can be set by
     ret.m = A1*qf1.m + A2*qf2.m */

  QUAD_FORM finsum,ret;
  MATRIX P,either_fin;
  MATRIX null_comp,inv,infm1,infm2,temp,Rm,Rc,inter;



  ret = decompose_sum(qf1,qf2,&either_fin);
  finsum.S = Ma(qf1.S, qf2.S); 
  /*  b = Ma(Mm(qf1.S,qf1.m),Mm(qf2.S,qf2.m)); */
  inv = Msym_range_inv(finsum.S,either_fin); 
  /*finsum.m = inv*qf1.S*qf1.m + inv*qf2.S*qf2.m */
  /*  finsum.m = Msym_solve(finsum.S,b,either_fin);  /* finsum.S has range either_fin */
  null_comp = Mcat(ret.fin,ret.inf);
  /*   m_inf = infinite_mean(qf1.m, qf2.m, qf1.null, qf2.null, ret.null); */
   matrix_infinite_mean(qf1.null,qf2.null,ret.null,&infm1,&infm2);
   /* m_inf = infm1*qf1.m + infm2*qf2.m */
   /*   c = Mm(Mt(ret.null),m_inf);  /* the constraint is ret.null'x = c  */
   /*   ret.m  = restricted_mean(finsum, null_comp, ret.null, c, ret.fin);  */
   matrix_restricted_mean(finsum.S, null_comp, ret.null, ret.fin,&Rm,&Rc); 
   temp = Mm(Rm,inv);
   /* ret.m = Rm*finsum.m + Rc*m_inf  
      = Rm*(inv*qf1.S*qf1.m + inv*qf2.S*qf2.m) + Rc*(infm1*qf1.m + infm2*qf2.m) 
      = (temp*qf1.S + Rc*infm1)*qf1.m + (temp*qf2.S + Rc*infm2)*qf2.m  */

   *A1 = Ma(Mm(temp,qf1.S),Mm(Rc,infm1));
   *A2 = Ma(Mm(temp,qf2.S),Mm(Rc,infm2));
   /* A1*qf1.m + A2* qf2.m */
   /*   ret.m = Ma(Mm(A1,qf1.m),Mm(A2,qf2.m)); */
   P = Mm(null_comp,Mt(null_comp));
   ret.S = Mm(P,Mm(finsum.S,P));
   ret.cov = Msym_range_inv(ret.S,ret.fin);
   ret.m = Munset();
   return(ret);
}

    






QUAD_FORM  
QFminus_sep(QUAD_FORM qf1, QUAD_FORM qf2) { 
  MATRIX c;
  QUAD_FORM ret,temp;
  MATRIX m0,P,not_null;

#ifdef DEBUG    
  QFcheck(qf1);   
  QFcheck(qf2); 
#endif
#ifdef DEBUG
  if (!Mis_subspace(qf1.inf,qf2.inf)) {
    printf("big problem in breakdown_diff()\n");
    Mp(qf1.inf);
    Mp(qf2.inf);
    exit(0);
  }
#endif 
  ret.null = qf1.null; 
  ret.inf = qf1.inf; 
  ret.fin = qf1.fin;  
  not_null = Mcat(ret.inf,ret.fin);
  P = Mm(not_null,Mt(not_null));
  ret.S = Mm(Mm(P,Ms(qf1.S,qf2.S)),P);
  ret.cov = Msym_range_inv(ret.S,ret.fin);
  m0 = Mm(Mm(qf1.null,Mt(qf1.null)),qf1.m);
  ret.m = Ma(m0,Mm(ret.cov,Ms(Mm(qf1.S,Ms(qf1.m,m0)),Mm(qf2.S,Ms(qf2.m,m0)))));

#ifdef DEBUG    
  QFcheck(ret);   
#endif

  return(ret);
}
  
  








/*the flow calculation which computes (among other things) the sum
  r1 + r2 - r3
  where r1 is the original distribution, 
  r2 is the new promoted marginal
  r3 is the old promoted marginal.
  r2-r3 is guaranteed to be non-negative definite but the calculation
we perform is is with qf1 = r1+r2 and qf2 = r3.  we return the
difference of qf1-qf2. */
QUAD_FORM  
QFminus(QUAD_FORM qf1, QUAD_FORM qf2) { 
  MATRIX c;
  QUAD_FORM ret,temp;
  MATRIX m_inf,P,resid,resid_comp,b,either_fin;

  if (qf1.S.rows == 0 || qf1.S.el == NULL) qf1.S =   Msym_range_inv(qf1.cov,qf1.fin);
  if (qf2.S.rows == 0 || qf2.S.el == NULL) qf2.S =   Msym_range_inv(qf2.cov,qf2.fin);

#ifdef DEBUG    
  QFcheck(qf1);   
  QFcheck(qf2); 
#endif
#ifdef DEBUG
  if (!Mis_subspace(qf1.inf,qf2.inf)) {
    printf("big problem in breakdown_diff()\n");
    Mp(qf1.inf);
    Mp(qf2.inf);
    exit(0);
  }
#endif 
  ret.null = qf1.null;  /* any null space in qf1 can be attributed to a null space
			   in r1 or a null space in r2.  if r1 null space comps have
  			   r1 + r2 - r3 = inf + inf - inf = inf.
			   for r2  null space comps (not contained in r1) have
			   r1 + r2 - r3 = x + inf - y = inf.   */
  ret.inf = qf1.inf;   /* the qf1.inf space corresponsds to the intersection of
			  r1.inf and r2.inf.  clear ret.inf contains qf1.inf
			  since r1 + r2 - r3 = 0 + 0 - 0 = 0  (r2 > r3).
			  in qf1.fin, r1 + r2 - r3  = x + x - y > 0 ( y >= x)
			  so qf1.fin cannot contribute to ret.inf. */
  ret.fin = qf1.fin;   /* through previous statements */
  ret.S = Ms(qf1.S,qf2.S); /* temporary ret.S is changed later */
#ifdef DEBUG
  resid = Morthog(qf2.null,qf1.null);
  if (resid.cols + qf2.null.cols != qf1.null.cols) {
    printf("inf2 not contained in inf1\n");
    exit(0);
  } /* resid will be the null space (inf of concentration) */
#endif
  b = Ms(Mm(qf1.S,qf1.m),Mm(qf2.S,qf2.m));
  either_fin = Morthogonalize(Mcat(qf1.fin,qf2.fin));
  ret.m = Msym_solve(ret.S,b,either_fin); /* ret.m not taking account of cnstrnts */
  P = Mm(ret.null,Mt(ret.null));
  m_inf = Mm(P,qf1.m);  /* the part of mean that is deterministic */
  ret = enforce_constraints(ret, m_inf);
#ifdef DEBUG
  QFcheck(ret);
#endif

  /*     temp = QFminus_sep(qf1,qf2);
	 QFcompare(temp,ret); */

  return(ret);
}
  
  


QUAD_FORM  
QFpromote(QUAD_FORM qf, MATRIX perm) {
  /* perm is a permutation matrix (0's and 1's) mapping old order to new order 
   so perm is rxn where r is new dim and n is old r > n */
  QUAD_FORM ret;
  MATRIX ihc;

  if (perm.cols != qf.null.rows) {
    printf("dimension mismatch in QFpromote\n");
    exit(0);
  }
  ihc = Mmissing(perm);
  ret.inf = Mcat(Mm(perm,qf.inf),ihc);
  ret.null = Mm(perm,qf.null);
  ret.fin = Mm(perm,qf.fin);
  ret.cov = Mm(Mm(perm,qf.cov),Mt(perm));
  ret.S = Mm(Mm(perm,qf.S),Mt(perm));
  ret.m = Mm(perm,qf.m);
#ifdef DEBUG
  QFcheck(ret);
#endif
  return(ret);
}
  
QUAD_FORM  
QFpromote_sep(QUAD_FORM qf, MATRIX perm) {
  /* perm is a permutation matrix (0's and 1's) mapping old order to new order 
   so perm is rxn where r is new dim and n is old r > n */
  QUAD_FORM ret;
  MATRIX ihc;

  if (perm.cols != qf.null.rows) {
    printf("dimension mismatch in QFpromote\n");
    exit(0);
  }
  ihc = Mmissing(perm);
  ret.inf = Mcat(Mm(perm,qf.inf),ihc);
  ret.null = Mm(perm,qf.null);
  ret.fin = Mm(perm,qf.fin);
  ret.cov = Mm(Mm(perm,qf.cov),Mt(perm));
  ret.S = Mm(Mm(perm,qf.S),Mt(perm));
  ret.m = Mm(perm,qf.m);
#ifdef DEBUG
  QFcheck(ret);
#endif
  return(ret);
}
  



QUAD_FORM
QFmargin_sep(QUAD_FORM qf, MATRIX perm, QUAD_FORM space, int set) {
  /* perm is a permutation matrix (0's and 1's) mapping old order to new order 
   so perm is rxn where r is new dim and n is old  and n > r */
  QUAD_FORM ret;
  MATRIX null_comp,temp,Pfin,Pinf,inf,P,ct;
  
  if (set) {
    null_comp = Mcat(qf.fin,qf.inf);
    Mnull_decomp(Mt(Mm(perm,null_comp)),&ret.null,&temp);
    ct = Mm(perm,qf.inf);
    inf = Morthogonalize(ct);
    Mint_comp(temp,inf,&ret.inf,&ret.fin); 
  }
  else {
    ret.inf = space.inf;
    ret.fin = space.fin;
    ret.null = space.null;
  }

  ret.cov = Mm(Mm(perm,qf.cov),Mt(perm));
  ret.m = Mm(perm,qf.m);
  Pinf = Mm(ret.inf,Mt(ret.inf));
  ret.m =  Ms(ret.m,Mm(Pinf,ret.m));
  Pfin = Mm(ret.fin,Mt(ret.fin));
  ret.cov = Mm(Mm(Pfin,ret.cov),Pfin);  /* no reason to suppose that
					    ret.cov as defined above will
					    be null in ret.inf space */
  ret.S = Msym_range_inv(ret.cov,ret.fin);
  return(ret);
}


QUAD_FORM
QFmargin(QUAD_FORM qf, MATRIX perm) {
  /* perm is a permutation matrix (0's and 1's) mapping old order to new order 
   so perm is rxn where r is new dim and n is old  and n > r */
  QUAD_FORM ret;
  MATRIX null_comp,temp,Pfin,Pinf,inf,P,ct;
  
  ret.cov = Mm(Mm(perm,qf.cov),Mt(perm));
  ret.m = Mm(perm,qf.m);
  null_comp = Mcat(qf.fin,qf.inf);
  Mnull_decomp(Mt(Mm(perm,null_comp)),&ret.null,&temp);
  ct = Mm(perm,qf.inf);
  inf = Morthogonalize(ct);
  Mint_comp(temp,inf,&ret.inf,&ret.fin); 
  Pinf = Mm(ret.inf,Mt(ret.inf));
  ret.m =  Ms(ret.m,Mm(Pinf,ret.m));
  Pfin = Mm(ret.fin,Mt(ret.fin));
  ret.cov = Mm(Mm(Pfin,ret.cov),Pfin);  /* no reason to suppose that
					    ret.cov as defined above will
					    be null in ret.inf space */
  ret.S = Msym_range_inv(ret.cov,ret.fin);
  return(ret);
}


QUAD_FORM
QFjoint2update(QUAD_FORM qf, MATRIX yext, MATRIX xext, MATRIX *A) {
  /* yext is a permutation matrix (0's and 1's) that extracts
     the y component of the qf (x,y)^t.  return A and 
   return value (z) so that conditional of y | x is Ax + z */
  QUAD_FORM ret,prod,con;
  MATRIX V,xmem,sxy;
  int bin[100],i,j;
  
  V = Mconst(1,xext.rows,1.);
  /*    Mp(xext);
  Mp(yext);  */
  xmem = Mm(V,xext);
  for (i=0; i < xext.cols; i++) bin[i] = xmem.el[0][i];
  con = QFnull_inf(qf.S.rows,bin);
  prod = QFplus(qf, con);
  ret = QFmargin(prod,yext);
  /*  Mp(ret.S);*/
  sxy = Mm(Mm(yext,qf.S),Mt(xext));
  /*  printf("sxy\n");
  Mp(sxy);
  printf("ret.cov\n");
  Mp(ret.cov); 
  printf("qf.cov\n");
  QFprint(qf);*/
  *A = Msc(Mm(ret.cov,sxy),-1.);
  /*  Mp(*A); */
  /*  QFprint(ret);
exit(0); */
  return(ret);
}


QUAD_FORM
QFld_cond_dist(MATRIX A, QUAD_FORM e) {
  /* return potential for z = (x,y)^t where y = Ax + \xi   with \xi \sim e */
  QUAD_FORM ret;
  MATRIX AI,temp,P,B,inf1,inf2,Q;
  int xdim,ydim;
  float error;
  
#ifdef DEBUG
  QFcheck(e);
#endif  
  /*  Mp(A);
      QFprint(e);*/
  xdim = A.cols;
  ydim = A.rows;
  /*  printf("xdim = %d ydim = %d\n",xdim,ydim);
      printf("1\n");*/
  AI = Mvcat(Msc(Mt(A),-1),Miden(ydim));
  inf1 = Mvcat(Miden(xdim),A);
  inf2 = Mvcat(Mzeros(xdim,e.inf.cols),e.inf);
  ret.inf = Morthogonalize(Mcat(inf1,inf2));
  ret.null = Morthogonalize(Mm(AI,e.null));
  Mnull_decomp(Mt(Mcat(ret.null,ret.inf)), &ret.fin, &temp);
  P = Mm(ret.null,Mt(ret.null));
  Q = Ms(Miden(P.rows),P);
  ret.S = Mm(Mm(Q,Mm(Mm(AI,e.S),Mt(AI))),Q);

    Mnull_decomp(ret.S, &temp, &ret.fin);
      Mnull_decomp(Mt(Mcat(ret.fin,ret.null)),&ret.inf, &temp); 
  



  /*  printf("3\n");
  printf("e.S\n");
  Mp(e.S);
  printf("e.null\n");
  Mp(e.null);
  printf("e.inf\n");
  Mp(e.inf);
  printf("ret.S\n");
  Mp(ret.S);
  printf("ret.fin\n");
  Mp(ret.fin);
  printf("ret.null\n");
  Mp(ret.null);
  printf("ret.inf\n");
  Mp(ret.inf);*/
  ret.cov =  Msym_range_inv(ret.S,ret.fin);
  B = Mcat(ret.fin,ret.null);
  P = Mm(B,Mt(B));
  ret.m = Mm(P,Mvcat(Mzeros(xdim,1),e.m));

  /*  P = Ms(Miden(ret.inf.rows),Mm(ret.inf,Mt(ret.inf)));
      ret.m = Mm(P,Mvcat(Mzeros(xdim,1),e.m));*/


#ifdef DEBUG
  QFcheck(ret);
#endif  
  return(ret);
}



//CF:  creates a conditional distribution p(y|x) from y=Ax+e (y may be hi-D)
QUAD_FORM 
QFcond_dist(MATRIX A, QUAD_FORM qf) {
  QUAD_FORM temp,ret;
  int xdim,ydim;
  MATRIX AI,t,s,P,inf,c1,c2,c3;
  float error;
  MATRIX xext,yext,AA;
  int i;

#ifdef DEBUG
  QFcheck(qf);
#endif  


  xdim = A.cols;
  ydim = A.rows;

  /* first find qf for unrestricted case (making null space of qf inf space) */

  AI = Mcat(Msc(A,-1.),Miden(ydim));
  temp.m = Mvcat(Mzeros(xdim,1),qf.m); /* check this lies in support */
  temp.S = Mm(Mm(Mt(AI),qf.S),AI);
  c1 = Mvcat(Miden(xdim),A);
  c2 = Mvcat(Mzeros(xdim,qf.inf.cols),qf.inf);
  c3 = Mvcat(Mzeros(xdim,qf.null.cols),qf.null);
  inf = Mcat(Mcat(c1,c2),c3);
  Mnull_decomp(Mt(inf),&temp.fin,&temp.inf);
  temp.null = Mzeros(temp.m.rows,0);
  temp.cov =  Msym_range_inv(temp.S,temp.fin);
  
  
  /* then restrict the qf to obey the constraint */

  ret.null = Morthogonalize(Mm(Mt(AI),qf.null));
  P = Ms(Miden(temp.m.rows),Mm(ret.null,Mt(ret.null)));
  ret.cov = Mm(Mm(P,temp.cov),P);
  ret.inf = Morthogonalize(Mcat(c1,c2));
  Mnull_decomp(Mt(Mcat(ret.null,ret.inf)),&ret.fin,&t);
  ret.S =  Msym_range_inv(ret.cov,ret.fin);
  ret.m = Ms(temp.m,Mm(Mm(ret.inf,Mt(ret.inf)),temp.m));


#ifdef DEBUG
  QFcheck(ret);
#endif  
  return(ret);
}


QUAD_FORM 
xxxQFcond_dist(MATRIX A, QUAD_FORM qf) {
  MATRIX AA,xext,yext;
  QUAD_FORM ret,var,temp;
  int i;

  ret = QFcond_dist(A, qf);

  xext = Mzeros(A.cols,A.cols+A.rows);
  yext = Mzeros(A.rows,A.cols+A.rows);
  for (i=0; i < A.cols; i++) xext.el[i][i] = 1;
  for (i=0; i < A.rows; i++) yext.el[i][i+A.cols] = 1;
  temp = QFjoint2update(ret,  yext, xext, &AA);
  var = QFcond_dist(AA,temp);
  QFcompare(ret,var);
  printf("okay\n");
  return(ret);
}



QUAD_FORM
QFconditional(QUAD_FORM qf, MATRIX A) {
  /* return conditional dist of qf given the components specified by
     A.  Each column of A is a collection of 0's with a 1 in the position
     of the specified component. */

  /* not tested for qf with a non-trivial null space    */


  MATRIX range,B,comp,null_inf,P,in,out;
  QUAD_FORM ret,marg,prom,sum;

  
  in = Mm(A,Mt(A)); 
  out = comp = Ms(Miden(A.rows),in);  /* picks off components not in A */
  range = Mm(qf.S,comp);
  /*  Mp(qf.S);
      Mp(range); */
  Mnull_decomp(Mt(range),&null_inf,&ret.fin);
  /*  Mp(ret.fin); */
  Mint_comp(null_inf, qf.null, &ret.null, &ret.inf);
  P = Mm(ret.fin,Mt(ret.fin));

  marg = QFmargin(qf,Mt(A));
  prom = QFpromote(marg,A);
  ret.S = Ms(qf.S,prom.S);

/*  ret.S = Mm(P,Mm(qf.S,P)); */
  ret.cov =  Msym_range_inv(ret.S,ret.fin);
  ret.m = Mm(P,qf.m);

#ifdef DEBUG
  sum = QFplus(prom,ret);
  QFcompare(qf,sum);
#endif 
  return(ret);
}

  
     



QUAD_FORM
QFindep_sum(QUAD_FORM q1, QUAD_FORM q2) {
  /* if q1 and q2 represent independent normal random vectors compute
     the distribution of their sum */
  QUAD_FORM ret;
  MATRIX null_comp,temp,inf_comp,fin_cov,U,D,x,P;

#ifdef DEBUG  
  QFcheck(q1);
  QFcheck(q2);
#endif


  split_spaces(q1.null, q1.inf, q1.fin, q2.null, q2.inf, q2.fin,
	       &ret.null, &ret.inf, &ret.fin);
  if (q1.cov.rows == 0) q1.cov =   Msym_range_inv(q1.S,q1.fin);
  if (q2.cov.rows == 0) q2.cov =   Msym_range_inv(q2.S,q2.fin);
  U = ret.fin;
  fin_cov = Ma(Mm(Mm(Mt(U),q1.cov),U) ,  Mm(Mm(Mt(U),q2.cov),U)); 
  ret.cov = Mm(Mm(U,fin_cov),Mt(U)) ;
  ret.m = Ma(q1.m,q2.m);
  P = Mm(ret.inf,Mt(ret.inf));
  ret.m = Ms(ret.m,Mm(P,ret.m));

  ret.S =  Msym_range_inv(ret.cov,ret.fin);
#ifdef DEBUG
  QFcheck(ret);
#endif
  return(ret);
}


  



QUAD_FORM
QFxform(QUAD_FORM qf, MATRIX A) {
  /* if x is an rv with normal dist
     described by qf, find (and return) qf of y = Ax.
     To find the null space of y, note that if U is
     null of x then every possible value of x can
     be written as
         x = orth(U)*z
     so 
         y = Ax = A*orth(U)*z
     so
        orth(a*orth(U))
     is null space for y.   */

     
  QUAD_FORM ret;
  MATRIX null_comp,inf_comp,temp,inv,U,V,D,Ai,big,inf,Pinf,C,B,Pfin;

#ifdef DEBUG
  QFcheck(qf); 
#endif


  ret.cov = Mm(Mm(A,qf.cov),Mt(A));
  ret.m = Mm(A,qf.m);
  Mnull_decomp(Mt(Mm(A,Mcat(qf.fin,qf.inf))),&ret.null,&temp);


  inf = Morthogonalize(Mm(A,qf.inf));  /* old way retried */
  Mint_comp(temp,inf,&ret.inf,&ret.fin); 

  Pinf = Mm(ret.inf,Mt(ret.inf));
  ret.m =  Ms(ret.m,Mm(Pinf,ret.m));
  
   Pfin = Mm(ret.fin,Mt(ret.fin));
    ret.cov = Mm(Mm(Pfin,ret.cov),Pfin);  /* no reason to suppose that
					    ret.cov as defined above will
					    be null in ret.inf space */

  ret.S = Msym_range_inv(ret.cov,ret.fin);
#ifdef DEBUG
  QFcheck(ret);
#endif
  return(ret);
}  


  
  
  



SUPER_QUAD 
SQset(SUPERMAT m, SUPERMAT v, float con) {
  int i,j,r=0,c=0;
  SUPER_QUAD temp;

  if (m.cols != 1) {
    printf("m must be vector in SQset\n");
    exit(0);
  }
  if (v.cols != v.rows) {
    printf("s must be square in SQset\n");
    exit(0);
  }
  if (v.rows != m.rows) {
    printf("m and s must agree in SQset\n");
    exit(0);
  }
  for (i=0; i < m.rows; i++) if (m.sub[i][0].cols != 1) {
    printf("sub means must be cols in SQset\n");
    exit(0);
  }
  for (i=0; i < v.rows; i++) r += v.sub[i][0].rows;
  for (j=0; j < v.cols; j++) c += v.sub[0][j].cols;
  if (r != c) {
    printf("covariance not square in SQset\n");
    exit(0);
  }
  temp.m = m;
  temp.S = v;
  temp.c = con;
  return(temp);
}

QUAD_FORM 
SQtoQF(SUPER_QUAD sq) {
  QUAD_FORM qf;
  int i,j;

  qf.c = sq.c;
  qf.m = expand(sq.m);
  qf.S = expand(sq.S);
  return(qf);
}
 
SUPER_QUAD 
QFtoSQ(QUAD_FORM qf, SUPER_QUAD template) {
  int i,j;
  SUPER_QUAD sq;

  sq.c = qf.c;
  sq.m = partition(qf.m,template.m);
  sq.S = partition(qf.S,template.S);
  return(sq);
}





void
SQprint(SUPER_QUAD sq) {

  printf("m = \n");
  Sprint(sq.m);
  printf("S = \n");
  Sprint(sq.S);
  printf("constant = %f\n",sq.c);
}

 

QUAD_FORM 
QFperm(QUAD_FORM qf) {
  QUAD_FORM temp;

  if (qf.S.el == NULL || qf.S.rows == 0)
    qf.S =  Msym_range_inv(qf.cov, qf.fin);
  if (qf.cov.el == NULL || qf.cov.rows == 0)
    qf.cov =  Msym_range_inv(qf.S, qf.fin);
  temp.m = Mperm(qf.m);
  temp.S = Mperm(qf.S);
  temp.cov = Mperm(qf.cov);
  temp.inf = Mperm(qf.inf);
  temp.fin = Mperm(qf.fin);
  temp.null = Mperm(qf.null);
  return(temp);
}

QUAD_FORM 
QFperm_id(QUAD_FORM qf) {
  QUAD_FORM temp;
  int dim;
  

  dim = qf.m.rows;
  /*  temp.m = Mperm_id(qf.m);
  temp.S = Mperm_id(qf.S);
  temp.cov = Mperm_id(qf.cov);
  temp.inf = Mperm_id(qf.inf);
  temp.fin = Mperm_id(qf.fin);
  temp.null = Mperm_id(qf.null);*/

  temp.m = Mperm_alloc_id(dim,1);
  temp.S = Mperm_alloc_id(dim,dim);
  temp.cov = Mperm_alloc_id(dim,dim);
  temp.inf = Mperm_alloc_id(dim,dim);
  temp.fin = Mperm_alloc_id(dim,dim);
  temp.null = Mperm_alloc_id(dim,dim);
  QFcopy(qf,&temp);
  
  return(temp);
}


#define COV_NULL_THRESH .0001


QUAD_FORM 
QFconvert(JN ojn) {
  QUAD_FORM temp;
  int dim,j,i,zdim,cdim,nn,nc;
  MATRIX null,not_null,P,U,V,D;

  dim = ojn.mean.rows;
  temp.m = Mconvert(ojn.mean);
  temp.cov = Mconvert(ojn.var);

  Msvd(temp.cov,&U,&D,&V);
  zdim = 0;
  for (i=0; i < D.cols; i++) if (D.el[i][i] < COV_NULL_THRESH) zdim++;
  cdim = D.cols - zdim;
  temp.null = Malloc(D.cols,zdim);
  temp.fin = Malloc(D.cols,cdim);
  nn = nc = 0;
  for (j=0; j < D.cols; j++) {
    if (D.el[j][j] < COV_NULL_THRESH) {
      for (i=0; i < D.cols; i++) temp.null.el[i][nn] = V.el[i][j];
      nn++;
    }
    else {
      for (i=0; i < D.cols; i++) temp.fin.el[i][nc] = V.el[i][j];
      nc++;
    }
  }



  /*  printf("cov\n");
  Mp(temp.cov);
  printf("D\n");
  Mp(D); */


  /*      Mnull_decomp(temp.cov,&temp.null,&temp.fin);*/



  /*  if (temp.null.cols > 0) {
    printf("encountred null space of dim %d\n",temp.null.cols);
    
    Mp(temp.cov);
    }*/
  P = Mm(temp.fin,Mt(temp.fin));
  temp.cov = Mm(Mm(P,temp.cov),P);
  temp.inf  = Mzeros(dim,0);
  /*  Mp(temp.cov);*/
  temp.S =  Msym_range_inv(temp.cov, temp.fin);
  /*         QFprint(temp);    */
  return(temp);
}

QUAD_FORM
QFmake_pos_unif(QUAD_FORM qf) {
  /* assuming pos is 0th index */
  MATRIX m,P,e,t1,t2,ecomp;
  int i,j,dim,null[ACCOM_DIM],r;
  QUAD_FORM pu,ret;

  /*  printf("needs testing\n");

  r = qf.inf.rows;
  e = Mzeros(r,1);
  e.el[0][0] = 1;
  ecomp = Mzeros(r,r-1);
  for (i=0; i < r-1; i++) ecomp.el[i+1][i] = 1;
  qf.inf =  Morthogonalize(Mcat(e,qf.inf));
  qf.null = Mintersect(ecomp,qf.null);
  Mnull_decomp(Mt(Mcat(qf.null,qf.inf)), &qf.fin, &t1);
  P = Mm(qf.fin,Mt(qf.fin));
  qf.S = Mm(Mm(P,qf.S),P);
  qf.cov = Mm(Mm(P,qf.cov),P);
  qf.m = Mm(P,qf.m);
  return(qf); */

  /*  if (qf.S.rows == 2 && qf.fin.rows == 2) {
    qf.inf = Mzeros(2,1);
    qf.inf.el[0][0] = 1;
    qf.fin = Mzeros(2,1);
    qf.fin.el[0][1] = 1;
    P = Mm(qf.fin,Mt(qf.fin));
    qf.S = Mm(Mm(P,qf.S),P);
    qf.cov = Mm(Mm(P,qf.cov),P);  /* this onlyworks for this specific case */
  /*    qf.m = Mm(P,qf.m);
    return(qf);
  }
  else {
    printf("this won't work ...\n");
    QFprint(qf);
    exit(0);
    }*/

  dim = qf.S.rows;
  if (dim  > ACCOM_DIM) {
    printf("maybe problem in QFmake_pos_unif\n");
    exit(0);
  }
  
  for (i=0; i < dim; i++) null[i] = (i != 0);
  pu = QFnull_inf(dim,null);
  ret = QFindep_sum(pu,qf);
  /*  printf("before\n");
  QFprint(ret); */
  return(ret);

    
  
  dim = qf.S.cols;
  if (qf.fin.cols != dim || dim != 2) {
    printf("can't handle this situation\n");
    exit(0);
  }
  for (i=0; i < dim; i++) for (j=0; j < dim; j++) {
    if (i == 0 || j == 0)
      qf.S.el[i][j] = qf.cov.el[i][j] = 0;
  }
  qf.S.el[1][1] = 1/qf.cov.el[1][1];
  qf.m.el[0][0] = 0;
  qf.inf = Malloc(dim,1);
  for (i=0; i < dim; i++) qf.inf.el[i][0] = (i == 0) ? 1 : 0;
  qf.fin = Malloc(dim,1);
  for (i=0; i < dim; i++) qf.fin.el[i][0] = (i != 0) ? 1 : 0;
  qf.null = Malloc(dim,0);
  return(qf);
}
  
  
  


#include "ran1.c"
#include "gasdev.c"

MATRIX 
rand_gauss(QUAD_FORM qf) {
  MATRIX ret,U,D,v;
  int i,n,x;
  double drand48();

  x = 0;
  if (qf.cov.el == NULL || qf.cov.rows == 0)  qf.cov = Msym_range_inv(qf.S, qf.fin);
  /*  printf("the qf:\n");
  QFprint(qf); */
 Msym_decomp(qf.cov,&U,&D);
  n = qf.cov.rows;
  v = Mzeros(n,1);
  for (i=0; i < n; i++) v.el[i][0] = gasdev(&x)*sqrt(D.el[i][i]);
  return(Ma(Mm(U,v),qf.m));
}



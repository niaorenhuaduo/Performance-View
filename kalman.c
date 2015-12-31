#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ludcmp.c"
#include "lubksb.c" 
/*#include "svbksb.c"*/
/*#include "svdcmp.c"*/
/*#include "brent.c"*/


#include "share.c"       
#include "global.h"        
#include "belief.h"        
#include "conductor.h"
#include "dp.h"
#include "new_score.h"
#include "midi.h"
#include "linux.h"

#define LENGTH 44 /*12 /* 24  */
#define SIMUL   10   /* number of simulations */

static global_iter;  /* just for testing */


extern PITCH *sol;   /* solfege array */

#define MAX_EX 100
#define MAX_NOTES 1000 /* only applies to A but still should be done otherwise */
#define MAX_JN 5000

typedef struct {
    JN jn[MAX_JN];
    int cur;
} JN_BUFF;
    

static global_print_flag=0;


static JN_BUFF jn_buff;
/*JN incr[MAX_NOTES],newhat[MAX_NOTES];
JN hat[MAX_NOTES]; */
static JN *incr,*newhat,*hat,*explore; 
static int *ncount;  /* number of examples for each note */
   /* hat[i] is estimate  TO state i. hat[start_phrase] is inital dist */
   /* hat[i] is defined for start_phrase <= i <= end_phrase */
/*int count[MAX_NOTES]; */

static float *note_length;
static float **note_time;
static EXAMPLE *phrase_examp[MAX_EX];
static int  num_examps;      /* number of different sequences of start times */
static int  num_notes;       /* number of notes n phrase */
static int start_phrase;
static int end_phrase;  /* note numbers (end_phrase is start of next */


static MX A[MAX_NOTES];  /* a(i) is defined for start_phrase <= i <= end_phrase
	       see below for these definitions */
static MX RR;       
static MX C;  // the observation matrix that picks of the position coordinate
static MX S;
static MX D;   

#define STATE_DIM 2
#define UPDATE_DIM 2
#define POS_INDEX  0   /* matrix index of the position  in tokens */
#define SIZ_INDEX  1   /* matrix index of the beat size (tokens/meas) */
#define GRW_INDEX  2   /* matrix index of the growth rate  (tokens/meas^2) */

/*                          the model is this 


                              x(0) = hat[0] ~ N(parms)   

 x(n+1) = A(n)x(n) + Se(n+1)     e(n) = hat[n] ~ N(parms)
 y(n)   = Cx(n) + d(n)       d(n) ~ N(0,R) 




We assume S'S = I



in order to specify the model generically the following
functions must exit



TO SET A(n)

state_eqn_mat(length)
float length; (the note length in measures)  



TO SET ALL OTHER MODEL MATRICES

init_matrices()

TO INITIALIZE x(0)

init_start_state(length)
float length; (length of measure in secs) 


*/


/*here things are made explicit */

#define OBSERVE_VAR  1.   /* this is in frames (bad) */





/*********************************************************************/



/* EVENT_LIST event; */



/*********************************************************************/




static JN 
*alloc_jn(i)
int i; {
    JN *j;

    if (i+jn_buff.cur > MAX_JN) {
	printf("can't allocate %d JNs\n",i);
	exit(0);
    }
    j = jn_buff.jn+jn_buff.cur;
    jn_buff.cur += i;
    return(j);
}






static float state2pos(s)
MX s; { /* a state (STATE_DIM x 1 matrix ) */
  return(s.el[POS_INDEX][0]);
}




/*MX get_matrix(r,c)
int r,c; {
  MX temp;
  float **matrix();

  temp.rows = r;
  temp.cols = c;
  temp.el = matrix(0,r-1,0,c-1);
  return(temp);
}*/



static MX mm(m1,m2)  /* matrix multiply */
MX m1,m2; {
  MX temp;
  int i,j,k;

  if (m1.cols != m2.rows) { 
    printf("matrices incompatible in mmn");  
    exit(0); 
  }
  temp.rows = m1.rows;
  temp.cols = m2.cols;
  for (i=0; i < temp.rows; i++) for (j=0; j < temp.cols; j++) {
    temp.el[i][j] = 0;
    for (k=0; k < m1.cols; k++) temp.el[i][j] += m1.el[i][k]*m2.el[k][j];
  }
  return(temp);
}

static MX ma(m1,m2)   /* add matrices */
MX m1,m2; {
  MX temp;
  int i,j,k;

  if (m1.cols != m2.cols || m1.rows != m2.rows) { 
    printf("matrices incompatible in ma\n");  exit(0); 
  }
  temp.rows = m1.rows;
  temp.cols = m1.cols;
  for (i=0; i < temp.rows; i++) for (j=0; j < temp.cols; j++) 
    temp.el[i][j] = m1.el[i][j] + m2.el[i][j];
  return(temp);
}

static MX zeros(r,c)
int r,c; {
  MX temp;
  int i,j;

  temp.rows = r;
  temp.cols = c;
  for (i=0; i < r; i++) for (j=0; j < c; j++) temp.el[i][j] = 0;
  return(temp);
}

static MX ms(m1,m2)  /* subtract matrices */ 
MX m1,m2; {
  MX temp;
  int i,j,k;

  if (m1.cols != m2.cols || m1.rows != m2.rows) { 
    printf("matrices incompatible in ms\n");  exit(0); 
  }
  temp.rows = m1.rows;
  temp.cols = m1.cols;
  for (i=0; i < temp.rows; i++) for (j=0; j < temp.cols; j++) 
    temp.el[i][j] = m1.el[i][j] - m2.el[i][j];
  return(temp);
}

static MX mi(m)   /* invert matrix */
MX m; {
  MX temp;
  float det,**a,col[MAXDIM+1],**y,**matrix(),d;
  int i,j,*index,*ivector();
  void free_vector(),free_matrix(),free_ivector();

  if (m.rows != m.cols) { printf("matrix not invertible\n"); exit(0); }
  a = matrix(1,m.rows,1,m.cols);
  y = matrix(1,m.rows,1,m.cols);
  index = ivector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];

  ludcmp(a,m.rows,index,&d);
  for (j=1; j <= m.rows; j++) {
    for (i=1; i <= m.rows; i++) col[i] = 0;
    col[j] = 1;
    lubksb(a,m.rows,index,col);
    for (i=1; i <= m.rows; i++) y[i][j] = col[i];
  }
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    m.el[i][j] = y[i+1][j+1];
  free_matrix(a,1,m.rows,1,m.cols);
  free_matrix(y,1,m.rows,1,m.cols);
  free_ivector(index,1,m.rows);
  return(m);
}

static MX mt(m)   /* matrix transpose */
MX m; {
  MX temp;
  int i,j;

  temp.rows = m.cols;
  temp.cols = m.rows;
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[j][i] = m.el[i][j];
  return(temp);
}

static MX msqrt(m)   /* sqare root of symmetric matrix   (not unique) */
MX m; {
  MX temp,m1,m2,m3;
  float det,**a,col[MAXDIM+1],**y,**matrix(),*vector(),*w,**v;
  int i,j,*index,d,*ivector();
  void free_vector(),free_matrix();

  if (m.rows != m.cols) { printf("matrix not square\n"); exit(0); }
  a = matrix(1,m.rows,1,m.cols);
  v = matrix(1,m.rows,1,m.cols);
  w = vector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];
  svdcmp(a,m.rows,m.rows,w,v);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    m.el[i][j] = a[i+1][j+1]*sqrt(w[j+1]);
  free_matrix(a,1,m.rows,1,m.cols);
  free_matrix(v,1,m.rows,1,m.cols);
  free_vector(w,1,m.rows);
  return(m);
}


static float mdet(m)   /* determinant  matrix */
MX m; {
  float **a,d,**matrix();
  int i,j,*index,*ivector();
  void free_vector(),free_matrix(),free_ivector();

  if (m.rows != m.cols) { printf("matrix not square\n"); exit(0); }



  /*  if (m.rows == 2 && m.cols == 2) return(m.el[0][0]*m.el[1][1]-m.el[0][1]*m.el[1][0]); */

  a = matrix(1,m.rows,1,m.cols);
  index = ivector(1,m.rows);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    a[i+1][j+1] = m.el[i][j];
  ludcmp(a,m.rows,index,&d);
  for (j=1; j <= m.rows; j++) d *= a[j][j];
  free_matrix(a,1,m.rows,1,m.cols);
  free_ivector(index,1,m.rows);
  return(d);
}


static int
mcov_mat(m)
MX m; {
  int i,j;

  if (m.rows != m.cols) return(0);
  for (i=0; i < m.rows; i++)
    for (j=i+1; j < m.rows; j++)
      if (fabs(m.el[i][j]-m.el[j][i]) > .01) return(0);
  if (mdet(m) < 0.) return(0);
  return(1);
}

MX msym(MX m) {
  int i,j;

  if (m.rows != m.cols) {
    printf("can't symmtrize matrix\n");
    exit(0);
  }
  for (i=0; i < m.rows; i++) for (j=i+1; j < m.rows; j++)
    m.el[i][j] = m.el[j][i] = (m.el[i][j] + m.el[j][i])/2;
  return(m);    
}






static MX msc(m,c)   /* scale matrix */
MX m; 
float c; {
  int i,j;

  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    m.el[i][j] *=c;
  return(m);
}

/* MX mc(m)
MX m; {
  MX temp;
  float det;
  int i,j;

  temp = get_matrix(m.rows,m.cols);
  for (i=0; i < m.rows; i++) for (j=0; j < m.cols; j++)
    temp.el[i][j] = m.el[i][j];
  return(temp);
}
*/


mp(m)
MX m; {
  int i,j;

  for (i=0; i < m.rows; i++) {
    for (j=0; j < m.cols; j++) printf("%f ",m.el[i][j]);
    printf("\n");
  }
  printf("\n");
}


mp_file(m,fp)
MX m; 
FILE *fp; {
    int i,j;

    fprintf(fp,"dim = %d %d\n",m.rows,m.cols);
    for (i=0; i < m.rows; i++) {
	for (j=0; j < m.cols; j++) fprintf(fp,"%f ",m.el[i][j]);
	fprintf(fp,"\n");
    }
    fprintf(fp,"\n");
}


MX mr_file(fp)
FILE *fp; {
    MX m; 
    int i,j;

    fscanf(fp,"dim = %d %d\n",&(m.rows),&(m.cols));
    for (i=0; i < m.rows; i++) {
	for (j=0; j < m.cols; j++) fscanf(fp,"%f ",&(m.el[i][j]));
    }
    return(m);
}



static MX mcombine(m11,m12,m21,m22)
MX m11,m12,m21,m22; {
  MX temp;
  int i,j;

  if (m11.rows != m12.rows) printf("mismatch in mcombine()\n");
  if (m11.cols != m21.cols) printf("mismatch in mcombine()\n");
  if (m12.cols != m22.cols) printf("mismatch in mcombine()\n");
  if (m21.rows != m22.rows) printf("mismatch in mcombine()\n");
  if (m11.rows + m21.rows > MAXDIM) printf("matrix too big\n");
  if (m11.cols + m12.cols > MAXDIM) printf("matrix too big\n");
  temp.rows = m11.rows + m21.rows; 
  temp.cols = m11.cols + m12.cols; 

  for (i=0; i < m11.rows; i++) for (j=0; j < m11.cols; j++)
    temp.el[i][j] = m11.el[i][j];
  for (i=0; i < m12.rows; i++) for (j=0; j < m12.cols; j++)
    temp.el[i][j+m11.cols] = m12.el[i][j];
  for (i=0; i < m21.rows; i++) for (j=0; j < m21.cols; j++)
    temp.el[i+m11.rows][j] = m21.el[i][j];
  for (i=0; i < m22.rows; i++) for (j=0; j < m22.cols; j++)
    temp.el[i+m11.rows][j+m11.cols] = m22.el[i][j];
  return(temp); 
}




static float log_norm(d,y)  /* scaled likelihood of y under d */
JN d;
MX y; {
  float total,det;
  MX temp,xx;

  y = ms(y,d.mean);

  /*  xx = mm(mt(y),y); /* for getting squared error  */
  /*                 return(-xx.el[0][0]);   */



  temp = mm(mm(mt(y),mi(d.var)),y);
  total = temp.el[0][0];
  det = mdet(d.var);  
  if (det < 0) {
    printf("negative deternminant in log_norm\n");
    mp(d.var);
    exit(0);
  }
  total += (det < 0) ? HUGE_VAL : log(det);   
  return(-total);
}


static JN condition(x,y,cov,yval)  /* returns cond dist of x given y */  
JN x,y;
MX yval,cov; {
  JN cond,cond2;
  float **matrix(),*vector(),*w,**v,**a;
  MX temp,uu,vv,dd;
  int n,i,j;


  /*  printf("x.var = \n");
  mp(x.var);
  printf("y.var = \n");
  mp(y.var); */

  cond.mean =  ma(x.mean,mm(cov,mm(mi(y.var), ms(yval,y.mean))));
  temp = msym(mm(cov,mm(mi(y.var),mt(cov))));
  cond.var = ms(x.var,temp);
  if (mcov_mat(cond.var) == 0) {
    printf("problem in condition\n");
    printf("x.var\n");
    mp(x.var);
    printf("y.var\n");
    mp(y.var);
    printf("y.var + 1000\n");
    y.var.el[0][0] += 1000;
    mp(y.var);
    printf("y.var inverse \n");
    mp(mi(y.var));
    mp(mm(cov,mm(mi(y.var),mt(cov))));
    mp(cond.var);
    mp(cov);
    exit(0);
  }
  return(cond);
}



static JN full_complete_square(x1,x2)  /* covariances inverted  on input*/
     // this seems to be the product of distributions
JN x1,x2; { 
  JN C;

  x1.var = mi(x1.var);
  x2.var = mi(x2.var);
  C.var = mi( ma(x1.var,x2.var) );
  C.mean = mm(C.var, ma( mm(x1.var,x1.mean), mm(x2.var,x2.mean)));
  return(C);
}


static JN complete_square(x1,x2)  /* covariances not inverted  on input*/
     // this seems to be the product of distributions
JN x1,x2; { 
  JN C;

/*printf("matrices\n");
mp(x1.var);
mp(x2.var);
printf("sum of matrices \n");
mp(ma(x1.var,x2.var));
printf("inverse of sum\n"); */
  C.var = mi( ma(x1.var,x2.var) );
/*mp(C.var); */
  C.mean = mm(C.var, ma( mm(x1.var,x1.mean), mm(x2.var,x2.mean)));
  return(C);
}



static 
MX identity(r)
int r; {
  MX temp;
  int i,j;

  temp.rows = r;
  temp.cols = r;
  for (i=0; i < r; i++) for (j=0; j < r; j++) temp.el[i][j] = (i == j) ? 1 : 0;
  return(temp);
}



static 
JN transform(A,j)
     // return dist of Aj where j is joint normal
MX A;
JN j; {
  j.mean = mm(A,j.mean);
  j.var =  mm(mm(A,j.var),mt(A));
  return(j);
}




static MX
get_obs_matrix(float var) {
  MX V;

  V = zeros(1,1);
  V.el[0][0] = var;
  return(V);
}

static MX
get_unif_variance(void) {
  MX V;
  int i;

  V = zeros(STATE_DIM,STATE_DIM);
  for (i=0; i < STATE_DIM; i++) V.el[i][i] = 10000000;
  return(V);
}


static 
MX state_eqn_mat(length)
     // if x current state Ax is the predicted distribution of next state not including further "noise"
float length; {
  MX A;

  A = identity(STATE_DIM);
  A.el[POS_INDEX][SIZ_INDEX] = length;
  return(A);
}



init_matrices() {  
  int i;
  float length;

  RR = zeros(1,1);
  RR.el[0][0] = OBSERVE_VAR;
  /*  RR = get_obs_matrix(OBSERVE_VAR);  */
  C = zeros(1,2);
  C.el[0][0] = 1;
/*  S = zeros(STATE_DIM,UPDATE_DIM);
  S.el[0][0] = cos(.0);
  S.el[1][0] = sin(.0); */
  S = identity(UPDATE_DIM); 
  D = zeros(UPDATE_DIM,2*UPDATE_DIM);   /* for subtracting */
  for (i=0; i < UPDATE_DIM; i++) D.el[i][i] = -1;
  for (i=0; i < UPDATE_DIM; i++) D.el[i][i+UPDATE_DIM] = 1;
}




JN init_start_state(length)
     // initial distribution before any info.  all we know is approx tempo
     // units of time are tokens --- want secs instead
float length; /* approx length of measure in secs */ {
  JN init;
  /*  float secs2tokens(); */

  length = secs2tokens(length);  
  init.mean = zeros(STATE_DIM,1);
  init.mean.el[SIZ_INDEX][0] = length; 
  init.var = identity(STATE_DIM);
  init.var.el[POS_INDEX][POS_INDEX] =  10000.; /*1; /*0.;*/
  init.var.el[SIZ_INDEX][SIZ_INDEX] = 10000.;  
  return(init);
}


#define STD_CNST 1 /*10 */


JN init_update(length)
float length; /* approx length of note in  measures */ {
  // distribution of the update (e_n) --- this should be taken from belief.c for better parameters
    JN init;
    float var; /*,secs2tokens();*/

    length *= score.meastime;  /* length of note in secs */
    length = secs2tokens(length);
    length *= .1;
    var = length*length;
    if (var > 10000) var = 10000;
    init.mean = zeros(STATE_DIM,1);
    init.var = identity(STATE_DIM);
    init.var.el[POS_INDEX][POS_INDEX] =   1/*var/* / 10*/;
     init.var.el[SIZ_INDEX][SIZ_INDEX] =  var;
     return(init);
 }

 static
 write_matrix(fp,m)
 FILE *fp;
 MX m; { 
   int i,j;

   fprintf(fp,"dim = %d %d\n",m.rows,m.cols);
   for (i=0; i < m.rows; i++) {
     for (j=0; j < m.cols; j++)  fprintf(fp,"%f ",m.el[i][j]);
     fprintf(fp,"\n");
   }
   fprintf(fp,"\n");
 }

 static
 MX read_matrix(fp)
 FILE *fp; {
   int i,j;
   MX m;

   fscanf(fp,"dim = %d %d\n",&m.rows,&m.cols);
   for (i=0; i < m.rows; i++)  {
     for (j=0; j < m.cols; j++)  fscanf(fp,"%f ",&m.el[i][j]);
     fscanf(fp,"\n");
   }
   fscanf(fp,"\n");
   return(m);
 }







static  JN /* R is variance (or covariance matrix) of the observation */
state_update(JN  xdist,MX AA,JN innov,MX C,MX  R,float yval) {
   JN ydist;
   MX cov;
   MX y;

   y = identity(1);
   y.el[0][0] = yval; 
   xdist.mean = ma(mm(AA,xdist.mean),innov.mean);
   xdist.var = ma(mm(mm(AA,xdist.var),mt(AA)),innov.var);
   ydist.mean = mm(C,xdist.mean);
   /*  ydist.var = ma(mm(mm(C,xdist.var),mt(C)),mm(R,mt(R)));*/
   ydist.var = ma(mm(mm(C,xdist.var),mt(C)),R);
   cov = mm(xdist.var,mt(C));


 /*mp(xdist.mean);
 mp(xdist.var);
 mp(ydist.mean);
 mp(ydist.var);
 mp(cov);
 printf("time = %f\n",yval);
 getchar();
 getchar(); */




   xdist  = condition(xdist,ydist,cov,y);


   return(xdist);
 }




static
JN state_update_like(JN xdist,MX  AA,JN  innov,MX  C,MX  R,float yval,float *like) {
   JN ydist,oldxdist,orig;
   MX cov;
   MX y;

   orig = xdist;
   /*  if (global_iter == 23) {
    printf("A:\n");
     mp(AA);
     printf("xdist before updating :\n");
     mp(xdist.mean);
     mp(xdist.var);
     printf("yval:\n");
     mp(y);
   } */
   y = identity(1);
   y.el[0][0] = yval; 
   xdist.mean = ma(mm(AA,xdist.mean),innov.mean);
   xdist.var = ma(mm(mm(AA,xdist.var),mt(AA)),innov.var);




   ydist.mean = mm(C,xdist.mean);
   /*  ydist.var = ma(mm(mm(C,xdist.var),mt(C)),mm(R,mt(R)));*/
   ydist.var = ma(mm(mm(C,xdist.var),mt(C)),R);
   cov = mm(xdist.var,mt(C));
   xdist  = condition(xdist,ydist,cov,y);

   /*  if (global_iter == 23) {
     printf("xdist after updating :\n");
     mp(xdist.mean);
     mp(xdist.var);
     printf("prediction of y\n");
     mp(ydist.mean);
     mp(ydist.var);
   }*/


 /*printf("innov:\n");
 mp(innov.mean);
 mp(xdist.mean);
 mp(ydist.mean);
 printf("yval = %f\n",yval);
 getchar();
 getchar(); */
   /*  if (global_print_flag)
     printf("variance = %f\n",ydist.var.el[0][0]);*/

   /*mp(ydist.var); */
   *like = log_norm(ydist,y);


   /*  if (global_iter == 23) {
     printf("xdist after conditioning :\n");
     mp(xdist.mean);
     mp(xdist.var);
     getchar();
     getchar(); 
   }
   */

 /*printf("y = %f ydist = :\n",yval);
 mp(ydist.mean);
 mp(ydist.var); */
   return(xdist);
 }

static
float tok2secs(float t) {
   return(t*(float)TOKENLEN/(float)SR);
}

static
recalc_pos(p1,p2,p3,len,score_time)
 /* p1 is estimated token time for last note
    p2 is current time
    p3 is predicted next time
    len is length of note in measures
    score_time is start time of last note in measures */
 float p1,p2,p3,len,score_time; {
   float size,cur;

   size = tok2secs(p3-p1)/len;   /* estimated measure size in secs */
   cur = score_time;
   cur += tok2secs(p2-p1)/size;  
   set_pos_tempo(cur,size);
   conduct();  
   set_pos_tempo(cur,size); 
   /* printf("updating cur_meas = %f meas_size = %f \n",cur,size);  */
 }

 float interpolate(m1,m2,m3,s1,s3) 
 float m1,m2,m3,s1,s3; {
   float a,b;
   a = (m2-m1)/(m3-m1);
   b = 1-a;
 /*printf("%f %f %f\n%f %f %f\n",m1,m2,m3,s1,b*s1+a*s3,s3);*/
   return(b*s1 + a*s3);
 }



void
predict_future(num,delay)  /* num and delay unused currently */
 int num; /* prediction based on num note */
 int delay; { 
   JN innov;
   MX A,pred;
   float cur,size,len,tok,next_accom_meas,next_accom_secs;
   float meas1,meas2,secs1,secs2;
   extern int cur_event;

   if (cur_note >= score.solo.num-1) return;
   if (cur_note == firstnote) return;  /* state_hat not yet initialized */
   if (cur_event == event.num) return;
   num = cur_note-1;
   pred = state_hat.mean;




   meas1 = meas2 = score.solo.note[num].time;  
   /* time in measures of last solo note played */
   tok = state2pos(pred);             /* want next note here */
   secs1 = secs2 = (tok*TOKENLEN)/SR;   /* time of most recent note  secs */
   next_accom_meas =  event.list[cur_event].meas;  

 if (next_accom_meas == 33.  && cur_note == 105) {
   printf("weird kludge\n");
   meas1 = score.solo.note[103].time;
   meas2 = score.solo.note[104].time;
   secs1 = TOKENLEN*score.solo.note[103].realize/(float)SR;
   secs2 = TOKENLEN*score.solo.note[104].realize/(float)SR;
 }
 else


   do { /* until we have info to predict coming accomp note */ 
     meas1 = meas2;
     secs1 = secs2;
     len = score.solo.note[num].length;
     A = state_eqn_mat(len);
     innov = transform(S,score.solo.note[num+1].update);
     pred = ma(mm(A,pred),innov.mean);/* prediction of next note */
     tok = state2pos(pred);             /* want next note here */
     num++;
     meas2 = score.solo.note[num].time;
     secs2 = (tok*TOKENLEN)/SR;
   } while (meas2 < next_accom_meas);
 /* if (next_accom_meas <  score.solo.note[num].time) then 
    next_accom_meas < meas1 < meas2   
    this is okay */
   next_accom_secs = 
     interpolate(meas1,next_accom_meas,meas2,secs1,secs2); 
 /* printf("meas1 = %5.3f next_accom_meas = %5.3f meas2 = %5.3f secs1 = %5.3f nas = %5.3f secs2 = %5.3f\n",meas1,next_accom_meas ,meas2, secs1,next_accom_secs,secs2); 
 printf("cur_event = %d\n",cur_event);     */
  set_goal(next_accom_meas,next_accom_secs); 
 }


 static void
 less_old_predict_future(num,delay)  /* num and delay unused currently */
 int num; /* prediction based on num note */
 int delay; { 
   JN innov; 
   MX A,pred;
   float cur,size,len,tok,meas,secs,next_accom_meas,last_accom_secs;
   float last_accom_meas,meas1,meas2,secs1,secs2;
   extern float cur_meas,cur_secs;
   extern int cur_event;

   if (cur_note == firstnote) return;  /* state_hat not yet initialized */
   num = cur_note-1;
   pred = state_hat.mean;

 /* this middle piece was added from here to the return statement */

   meas = score.solo.note[num].time;  /* time in measures of cur_note */
   tok = state2pos(pred);             /* want next note here */
   secs = (tok*TOKENLEN)/SR;   /* time of most recent note  secs */
 /*  next_accom_meas =  event.list[cur_event].meas;  */
   last_accom_meas =  event.list[cur_event-1].meas; 
   last_accom_secs =  event.list[cur_event-1].secs; 
 /*  while (secs < last_accom_secs || meas <  next_accom_meas) {  */
   while (secs <= last_accom_secs || meas <=  last_accom_meas) { 
 /* until we have info to predict coming accomp note */ 
     len = score.solo.note[num].length;
     A = state_eqn_mat(len);
     innov = transform(S,score.solo.note[num+1].update);
     pred = ma(mm(A,pred),innov.mean);/* prediction of next note */
     tok = state2pos(pred);             /* want next note here */
     num++;
     meas = score.solo.note[num].time;
     secs = (tok*TOKENLEN)/SR;
 /*printf("num = %d secs = %f cur_secs = %f meas = %f cur_meas = %f\n",num,secs,cur_secs,meas,cur_meas);    */
   } 
   /*if (cur_note >= 107) printf("pred fut: meas = %f secs = %f\n",meas,secs); */

 /*printf("meas = %f next accomp = %f\n",meas, event.list[cur_event].meas);   */

  set_goal(meas,secs);
   return;



   do { 
     len = score.solo.note[num].length;
     A = state_eqn_mat(len);
     innov = transform(S,score.solo.note[num+1].update);
     pred = ma(mm(A,pred),innov.mean);/* prediction of next note */
     tok = state2pos(pred);             /* want next note here */
     num++;
     meas = score.solo.note[num].time;
     secs = (tok*TOKENLEN)/SR;
  printf("num = %d secs = %f cur_secs = %f meas = %f cur_meas = %f\n",num,secs,cur_secs,meas,cur_meas);   
   } while (secs < cur_secs || meas < cur_meas);  /* while my predictions concern past */  /* what if we search forward over cue boundary (maybe don't matter) */
   set_goal(meas,secs);
 }




 static
 old_predict_future(num,delay)
 int num; /* prediction based on num note */
 int delay; { 
   JN innov;
   MX A,pred;
   float cur,size,len,p1,p2,p3,score_time;

   len = score.solo.note[num].length;
   A = state_eqn_mat(len);
   innov = transform(S,score.solo.note[num+1].update);
 if (print_level == 0) {
 printf("update is\n");
 mp(innov.mean);
 mp(innov.var);
 }
   pred = ma(mm(A,state_hat.mean),innov.mean);/* prediction of next note */
 /*mp(pred); */

   p1 = state2pos(state_hat.mean);   /* last note here */
   p3 = state2pos(pred);             /* want next note here */
   p2 = p1+delay;                    /* here now */
   score_time = score.solo.note[num].time;
   if (p3 > token)  /* next note predicted to occur later than now */
     recalc_pos(p1,p2,p3,len,score_time);
 }



 kalman_update(num,time,delay) /* assumes state_hat is for num-1 note */
 /* this sould work off of  cur_note */
 int num; 
 int time; /* time in tokens of num th note */ 
 int delay; { 
   JN innov;
   MX A,pred,R;
   float cur,size,len,p1,p2,p3,score_time;

   A = state_eqn_mat(score.solo.note[num-1].length);
   innov = transform(S,score.solo.note[num].update);
   R = get_obs_matrix(score.solo.note[num].obs_var);
   if (print_level == 0) {
     mp(state_hat.mean);
     mp(state_hat.var); 
   }

 /* Ithink state_hat, cur_note and cue.cur must be grouped into a structure
 so they are forced to agree */
   state_hat = state_update(state_hat,A,innov,C,/*RR*/ R,(float)time);
   if (print_level == 0) {
     mp(state_hat.mean);
     mp(state_hat.var); 
   }
 /*  predict_future(num,delay); */
 }


 new_kalman_update(time) 
 float time; { /* time in tokens of num th note */ 
   JN innov;
   MX A,pred,R;

   A = state_eqn_mat(score.solo.note[cur_note-1].length);
   innov = transform(S,score.solo.note[cur_note].update);
   R = get_obs_matrix(score.solo.note[cur_note].obs_var);
 /*printf("before state_hat = \n");
 mp(state_hat.mean);
 if (cur_note == 229) {
 printf("innov = \n");
 mp(innov); 
 } */
   state_hat = state_update(state_hat,A,innov,C,/*RR*/R,time);
 /*printf("after state_hat = \n");
 mp(state_hat.mean); */
 /*  mp(state_hat);*/
 }



 blind_kalman_update()  {/* when I didn't catch the note onset online */
   JN innov;
   MX A;

 /* new state_hat dist is the dist of Ax+b where x is the current state_hat,
    and b is the update */

   A = state_eqn_mat(score.solo.note[cur_note-1].length);
   innov = transform(S,score.solo.note[cur_note].update);
   state_hat.mean = ma(mm(A,state_hat.mean),innov.mean);
   state_hat.var = ma(mm(mm(A,state_hat.var),mt(A)),innov.var);
 }




 cue_phrase(num,lag)
 int lag;
 int num; /* note number */ {
   SOLO_NOTE *nt;
   float p1,p2,p3,len,score_time;
   MX A,pred;
   JN innov;

 /* printf("phrase cued  at token = %d\n",token); */

   nt = &score.solo.note[num];
   state_hat = nt->update;
 /*    cue.list[phrase].state_init = hat[start_phrase]; */
 if (print_level == 0) {
 mp(state_hat.mean);
 mp(state_hat.var);
 }
   state_hat.mean.el[POS_INDEX][0] += (token-lag);  
   old_predict_future(num,lag);
   predict_future(num,lag);
 }



 new_cue_phrase(cue_time)  /* time cued note occured in tokens */
 float cue_time; {
   float meas,size,cue_meas,onset;
   int n;
   float now();
   extern float cur_secs;

 /*   printf("new phrase cued  at %f\n",cue_time);  */

   state_hat = cue.list[cue.cur/*++*/].state_init;
   state_hat.mean.el[POS_INDEX][0] = cue_time;
   size = tok2secs(state_hat.mean.el[SIZ_INDEX][0]); /* secs / meas */
   cue_meas = score.solo.note[cur_note].time; /* measures where cued note occurs */
   onset = tok2secs(cue_time);   /* secs where cued note occured */
   meas =  cue_meas + (cur_secs-onset)/size;  /* where we should be now */
 /*   printf("meas = %f size = %f\n",meas,size); */
   set_pos_tempo(meas,size); 
 /*  predict_future(0,0);  /* neither argument is used */
 /*   printf("done with new_cue_phrase at %f\n",now()); */
 }



static
MX sub_matrix(MX m,int lr,int hr,int lc,int  hc) {
  MX temp;
  int i,j;

  temp.rows = hr-lr+1;
  temp.cols = hc-lc+1;
  for (i=0; i < temp.rows; i++)
    for (j=0; j < temp.cols; j++)
      temp.el[i][j] = m.el[i+lr][j+lc];
  return(temp);
}

static
JN sub_jn(JN x,int low,int high) {
  JN temp;

  temp.mean = sub_matrix(x.mean,low,high,0,0);
  temp.var = sub_matrix(x.var,low,high,low,high);
  return(temp);
}


static
JN init_after_obs(d,val)  // not used presently
     JN d;
     float val; {
  int i,j;

  d.mean = zeros(STATE_DIM,1);
  d.mean.el[POS_INDEX][0] = val;
  for (i=0; i < d.var.rows; i++) {
    for (j=0; j < d.var.cols; j++) {
      if (i==POS_INDEX || j == POS_INDEX) d.var.el[i][j] = 0;
    }
  }
  d.var.el[POS_INDEX][POS_INDEX] = OBSERVE_VAR;
  return(d);
     }







static float 
data_log_like(JN *p)  {  /* p[start_phrase] is initialization. 
			    p[start_phrase+1] ... are updates */
  int s,i,start,end;
  float total,like,time,length,ov;
  MX AA,R,y,I;
  JN xhat,innov,prior,output,oldhat;
  EXAMPLE *ex;


  total = 0;
  /*    printf("adding prior term\n");
	for (i=start_phrase+1; i <= end_phrase; i++) {
	length = score.solo.note[i-1].length;
	prior = init_update(length);
	total += log_norm(prior,p[i].mean);
	} */
  for (s=0; s < num_examps; s++) { 
    ex  = phrase_examp[s];
    start = ex->start;
    end = ex->end;
    xhat = p[start_phrase]; /* pos value is meaningless */
    for (i=start_phrase; i < start; i++) {
      xhat = transform(A[i],xhat);
      xhat.mean = ma(xhat.mean,hat[i+1].mean);
      xhat.var = ma(xhat.var,hat[i+1].var);
    }


    time = ex->time[i-start];

    I = identity(STATE_DIM);
    innov.mean = zeros(STATE_DIM,1);
    innov.var = zeros(STATE_DIM,STATE_DIM);
    R = get_obs_matrix(score.solo.note[i].obs_var);
    xhat = state_update_like(xhat,I,innov,C,R,time,&like);
    total += like;

    /*	ov = score.solo.note[i].obs_var;*/
    /*	xhat = init_after_obs(xhat,time);*/
    for (i=i+1; i <= end_phrase; i++) {
      if (i > end) break;
      time = ex->time[i-start];
      innov = transform(S,p[i]);

      R = get_obs_matrix(score.solo.note[i].obs_var);
      oldhat = xhat;
      xhat = state_update_like(xhat,A[i-1],innov,C,/*RR*/R,time,&like);
      /*printf("i = %d\n",i);
	mp(xhat);
	printf("time = %f\n",time);
	getchar();
	getchar(); */
      total += like;
      /*printf("total = %f like = %f i = %d s = %d \n",total,like,i,s); */
    }
  }
  return(total);
}






 int parm_length;
 float *parms;
 JN *parm_mat;

 load_parms(p,m)
 float *p;
 JN *m; {
   int c,i,j,n;
		       /* should this be LENGTH+1? */
   c = 1;
   for (i=0; i < STATE_DIM; i++) p[c++] = m[0].mean.el[i][0];
   for (i=0; i < STATE_DIM; i++)  for (j=0; j < STATE_DIM; j++)
     if (i >= j)   p[c++] = m[0].var.el[i][j];
   for (n=1; n < LENGTH; n++) {
     for (i=0; i < STATE_DIM; i++) p[c++] = m[n].mean.el[i][0];
     for (i=0; i < STATE_DIM; i++)  for (j=0; j < STATE_DIM; j++)
       if (i >= j) p[c++] = m[n].var.el[i][j];
   }
 }


 unload_parms(p,m)
 float *p;  
 JN *m; {
   int c,i,j,n;

   c = 1;
   m[0].mean = zeros(STATE_DIM,1);
   for (i=0; i < STATE_DIM; i++)  m[0].mean.el[i][0] = p[c++];
   m[0].var = zeros(STATE_DIM,STATE_DIM);
   for (i=0; i < STATE_DIM; i++)  for (j=0; j < STATE_DIM; j++)
     if (i >= j)     
       m[0].var.el[j][i] = m[0].var.el[i][j] = p[c++] ;
   for (n=1; n < LENGTH; n++) {
     m[n].mean = zeros(UPDATE_DIM,1);
     for (i=0; i < UPDATE_DIM; i++) m[n].mean.el[i][0] = p[c++];
     m[n].var = zeros(UPDATE_DIM,UPDATE_DIM);
     for (i=0; i < UPDATE_DIM; i++)  for (j=0; j < UPDATE_DIM; j++)
       if (i >= j) 
	m[n].var.el[j][i] = m[n].var.el[i][j] = p[c++];
   }
 }


 float dll(p)   /* users 1 offset array */
 float *p; {
   float x;

   unload_parms(p,parm_mat);
   x = data_log_like(parm_mat);
 /*  printf(" like = %f\n",x); */
   return(-x);
 }

 #define INCR .001

 grad_dll(p,grad)
 float *p,*grad; {
   int i;
   float x,y;

   x = dll(p);
   for (i = 1; i <= parm_length; i++) {
     p[i] += INCR;
     y = dll(p);  
     p[i] -= INCR;
     grad[i] = (y-x)/INCR;
   }
 }

 /*train() {
   float **xi,**matrix(),fret;
   int iter,i,j; 

   parm_length = STATE_DIM + STATE_DIM*STATE_DIM + 
     (LENGTH-1) * (UPDATE_DIM + UPDATE_DIM*UPDATE_DIM);
   parms = (float *) malloc(parm_length*sizeof(float));
   load_parms(parms,hat);
   parm_mat = (JN *) malloc(sizeof(JN)*LENGTH);
   xi = matrix(1,parm_length,1,parm_length);
   for (i=1; i <= parm_length; i++) 
     for (j=1; j <=parm_length; j++)  xi[i][j] = (i == j) ? 1 : 0;
   powell(parms-1,xi,parm_length,.01,&iter,&fret,dll);    
   frprmn(parms-1,parm_length,.0001,&iter,&fret,dll,grad_dll);    
   load_parms(parms,parm_mat);
   printf("log like = %f\n",data_log_like(parm_mat));
 if (print_level == 0) {
   mp(hat[0].mean);
   mp(hat[0].var);
 }
   if (print_level == 0) 
   for (i=0; i < LENGTH; i++) {
     mp(hat[i].mean);
     mp(hat[i].var);  
   }
   free(parm_mat);


 }*/


 float search_func(float t) { /* minimizing this */
     int i;


     for (i=start_phrase; i <= end_phrase; i++) {
	 explore[i].mean = ma(hat[i].mean,msc(incr[i].mean,t));
	 explore[i].var = ma(hat[i].var,msc(incr[i].var,t));
     }
     return(-data_log_like(explore));
 }


 float old_search_func(t)
 float t; {
   int i;

 /*  newhat[0] = hat[0];*/
   for (i=0; i < num_notes; i++) 
     newhat[i].mean = ma(hat[i].mean,msc(incr[i].mean,t));
   for (i=0; i < num_notes; i++) 
     newhat[i].var = ma(hat[i].var,msc(incr[i].var,t));
   return(-data_log_like(newhat));
 }





 #define MAX_ITER 100
 #define MIN_IMPROVE .5
 #define VAR_ITER 100 /* start estimating variance after this */
 #define PRECISION .001


 static JN end_hat;

 static
 fb() {
   int i,s,iter,r,dim,j,k;
   JN xhat,ydist,x1,x2,joint,future,**innov_est,*explore;
   JN *forward,*backward,temp_end,temp_init,innov,back,test;
   MX cov,T,temp,AA,R;
   float ov,nv,del_mean,del_var,tvar,minvar,cur,last,t,oldcur,ax,bx,cx,fa,fb,fc,val;

 /*  newhat   = (JN *) malloc(num_notes*sizeof(JN));
   incr     = (JN *) malloc(num_notes*sizeof(JN)); */
   explore  = (JN *) malloc(num_notes*sizeof(JN));
   backward = (JN *) malloc(num_notes*sizeof(JN));
   forward  = (JN *) malloc(num_notes*sizeof(JN));
   innov_est = (JN **) malloc(num_notes*sizeof(JN *));
   for (i=0; i < num_notes; i++) 
     innov_est[i] = (JN *) malloc(num_examps*sizeof(JN));

   end_hat.mean = zeros(STATE_DIM,1);  
   end_hat.var = msc(identity(STATE_DIM),1000000.);  

   for (iter = 0; iter < MAX_ITER; iter++) {
 printf("iter = %d\n",iter);
     temp_init.mean = temp_end.mean = zeros(STATE_DIM,1);
     temp_init.var = temp_end.var = zeros(STATE_DIM,STATE_DIM);
     for (s=0; s < num_examps; s++) {   /* for each example */


       AA = A[0];   /* anyting will do */
       xhat = hat[0];
       xhat.mean = mm(mi(AA),xhat.mean);
       innov.mean = zeros(STATE_DIM,1);
       innov.var = zeros(STATE_DIM,STATE_DIM);
       for (i=0; i < num_notes-1; i++) {  /* compute forward probs */
	 R = get_obs_matrix(score.solo.note[i].obs_var);
	 forward[i] = xhat = state_update(xhat,AA,innov,C,/*RR*/R,note_time[s][i]);
	 innov = transform(S,hat[i+1]);
	 AA = A[i];
       }
       temp_end.mean = ma(temp_end.mean,xhat.mean);  
       temp_end.var = ma(temp_end.var,mm(xhat.mean,mt(xhat.mean)));
       xhat = end_hat; 


       AA = A[0];    /* anything will do */
       xhat.mean = mm(AA,xhat.mean);
       innov.mean = zeros(STATE_DIM,1);
       innov.var = zeros(STATE_DIM,STATE_DIM);
       for (i = num_notes-1; i >= 0;  i--) {  /* compute backward probs */
	 R = get_obs_matrix(score.solo.note[i].obs_var);
	 backward[i] = xhat = state_update(xhat,mi(AA),innov,C,/*RR*/R,note_time[s][i]);
	 if (i==0) break;
	 AA = A[i-1];
	 innov = transform(msc(mm(mi(AA),S),-1.),hat[i]);
       }
       temp_init.mean = ma(temp_init.mean,xhat.mean);
       temp_init.var = ma(temp_init.var,mm(xhat.mean,mt(xhat.mean)));


       for (i=0; i < num_notes-1; i++) {
	 AA = A[i];
	 x1 = transform(mm(mt(S),AA),forward[i]);  /* dist of S'Ax(i)  (based on past) */
	 x2.mean = ma(x1.mean,hat[i+1].mean);
	 x2.var = ma(x1.var,hat[i+1].var);         /* dist of S'x(i+1) (based on past) */
	 r = x1.var.rows;
	 joint.mean = mcombine(x1.mean,zeros(r,0),x2.mean,zeros(r,0));
	 joint.var = mcombine(x1.var,x1.var,mt(x1.var),x2.var);
	 joint.var = mi(joint.var);
	 back = transform(mt(S),backward[i+1]);   /* dist of S'x(i+1) (based on future) */
	 future.mean = mcombine(zeros(r,1),zeros(r,0),back.mean,zeros(r,0));
	 future.var = mcombine(zeros(r,r),zeros(r,r),zeros(r,r),mi(back.var));
	 joint  = complete_square(joint,future);
	 joint.mean = mcombine(x1.mean,zeros(r,0),back.mean,zeros(r,0));
	 joint.var = mcombine(x1.var,zeros(r,r),zeros(r,r),back.var);   /* posterior joint dist of x[i] and x[i+1] */
	 innov_est[i+1][s] = transform(D,joint);    /* dist of x[i] - x[i+1] */               
	 innov_est[i+1][s].mean = ms(back.mean,x1.mean);
	 innov_est[i+1][s].var = ma(back.var,x1.var); 
       }

     }    /* for examples */

     newhat[0].mean = msc(temp_init.mean,1./num_examps);
     newhat[0].var = msc(temp_init.var,1./num_examps);
     newhat[0].var = ms(newhat[0].var,mm(newhat[0].mean,mt(newhat[0].mean)));
     end_hat.mean = msc(temp_end.mean,1./num_examps);
     del_mean = del_var = 0;
     minvar = HUGE_VAL;

     for (i=1; i < num_notes; i++) {   /* sum the update estimates */
       newhat[i].mean = zeros(UPDATE_DIM,1);
       newhat[i].var = zeros(UPDATE_DIM,UPDATE_DIM); 
       minvar = HUGE_VAL;
       for (s=0; s < num_examps; s++) { 
	 temp = innov_est[i][s].mean;
	 newhat[i].mean = ma(temp,newhat[i].mean);
	 newhat[i].var =  ma(mm(temp,mt(temp)),newhat[i].var); 
       } 
       newhat[i].mean = msc(newhat[i].mean,1./(num_examps));
       newhat[i].var = msc(newhat[i].var,1./(num_examps));
       newhat[i].var = ms(newhat[i].var,mm(newhat[i].mean,mt(newhat[i].mean)));

       incr[i].mean = ms(newhat[i].mean,hat[i].mean);  /* differences between this est and last  */
       incr[i].var = ms(newhat[i].var,hat[i].var);
     }
     incr[0].mean = ms(newhat[0].mean,hat[0].mean);
     incr[0].var = ms(newhat[0].var,hat[0].var);
     last = search_func(0.);
     t = 2;
     do {
       t /= 2.;
       cur = search_func(t);
     } while (cur < last);
     brent(0.,t,2*t,search_func,.001,&t);
     for (i=0; i < num_notes; i++) 
       newhat[i].mean = ma(hat[i].mean,msc(incr[i].mean,t));
     for (i=0; i < num_notes; i++) 
       newhat[i].var = ma(hat[i].var,msc(incr[i].var,t));
     ov = data_log_like(hat);
     nv = data_log_like(newhat);
     printf("old value = %f new value = %f\n",ov,nv);
     val =  ov/nv - 1; 
     for (i=0; i < num_notes; i++) hat[i] = newhat[i]; 
     if (val < PRECISION) break;
   }
 }


 static void
 compute_counts() {
   int i,j,s,bool;
   EXAMPLE *ex;

   for (i=start_phrase; i <= end_phrase;  i++) {
     ncount[i] = 0;
     for (s=0; s < num_examps; s++) {
       ex = phrase_examp[s];
       if (i == start_phrase) bool =  (i >= ex->start);
       else bool =  (i > ex->start && i <= ex->end);
       if (bool)  ncount[i]++;
     }
   }


   /*    for (i=start_phrase; i <= end_phrase; i++) count[i] = 0;
     for (j=0; j < num_examps; j++)
	 for (i=phrase_examp[j]->start; i <= phrase_examp[j]->end; i++)
	     count[i]++;
     for (i=start_phrase; i <= end_phrase; i++) 
	 if (count[i] == 0) printf("problem in compute_counts()\n"); */
 }


 JN condition_on_observation( JN d, float val,float var) {
   MX I,R;
   JN null_update;

   I = identity(STATE_DIM);
   null_update.mean = zeros(STATE_DIM,1);
   null_update.var = zeros(STATE_DIM,STATE_DIM);
   R = get_obs_matrix(var);
   d = state_update(d,I,null_update,C,R,val);
   return(d);
 }



 static
 compute_foward_dists(example,forward)
 /* assumes hat[] and A[] are set (notenum is index) */
 EXAMPLE *example;   /* the actual note time struct */
 JN *forward;  /* on output the forward state estimates */
 {
     JN xhat,innov;
     MX R;
     int i,j,first;
     float time,ov;

	 /* initialize xhat dist after  first note observed */
       xhat = hat[start_phrase];
       for (i=start_phrase; i < example->start; i++) {
	   xhat = transform(A[i],xhat);
	   xhat.mean = ma(xhat.mean,hat[i+1].mean);
	   xhat.var = ma(xhat.var,hat[i+1].var);
       }
 /*mp(xhat.mean);
 mp(xhat.var);*/
       first =  i - example->start;
       time = example->time[first];
       ov = score.solo.note[i].obs_var;
       /*       printf("temporarily took out next statement\n"); */
       /*          xhat = condition_on_observation(xhat,time,ov); */
       
	  /*              xhat = init_after_obs(xhat,(float) example->time[first]);*/
       forward[i] = xhat;
       for (i=i+1; i <= example->end; i++) {  /* compute forward probs */
	   if (i > end_phrase) break;
	   time =  example->time[i-example->start]; 
	   innov = transform(S,hat[i]);
	   R = get_obs_matrix(score.solo.note[i].obs_var);
	   forward[i] = xhat = state_update(xhat,A[i-1],innov,C,/*RR*/R,time);
	   /*   printf("foward[%d] = \n",i);
 mp(forward[i].mean);	  
 mp(forward[i].var);	   */
      }
 }

 JN totally_unknown(void) {
   JN x;
   int i;

   x.mean = zeros(STATE_DIM,1);
   x.var = zeros(STATE_DIM,STATE_DIM);
   for (i=0; i < STATE_DIM; i++) x.var.el[i][i] = 10000000000;
   return(x);
 }

 JN init_back_xhat(time,ov) 
   float time,ov; {
 
   JN x;
   int i;

   x.mean = zeros(STATE_DIM,1);
   x.mean.el[0][0] = time;
   for (i=1; i < STATE_DIM; i++) x.mean.el[0][i] = 0;
   x.var = zeros(STATE_DIM,STATE_DIM);
   x.var.el[0][0] = ov;
   for (i=1; i < STATE_DIM; i++) x.var.el[i][i] =  100000;
   return(x);
 }


 static
 compute_backward_dists(example,backward)
 /* assumes hat[] and A[] are set (notenum is index) */
 EXAMPLE *example;   /* the actual note time struct */
 JN *backward;  /* on output the forward state estimates */
 {
     JN xhat,innov;
     int i,j,last;
     float time,ov;
     MX R;

	 /* initialize xhat dist after  last note observed */
       xhat = hat[start_phrase];
       /*       for (i=start_phrase; i < example->end; i++) {
	   if (i == end_phrase) break;
	   xhat = transform(A[i],xhat);
	   xhat.mean = ma(xhat.mean,hat[i+1].mean);
	   xhat.var = ma(xhat.var,hat[i+1].var);
       }*/
       i = (end_phrase < example->end) ? end_phrase : example->end;
       last =  i - example->start;
       time = example->time[last];
       ov = score.solo.note[i].obs_var;
       /*       xhat = totally_unknown();
		xhat = condition_on_observation(xhat,time,ov); */
        xhat = init_back_xhat(time,ov); 
	/*              printf("kludge\n");
       xhat.mean.el[0][0] = time;
       xhat.mean.el[0][1] = 0;
       xhat.var.el[0][0] = ov;
       xhat.var.el[1][1] = 100000;  */
       if (mcov_mat(xhat.var) == 0) {
	     printf("not covariance for xhat in compute_backward_dists\n");
	     mp(xhat.var);
	     exit(0);
	   }

       
       /*      xhat = init_after_obs(xhat,(float) example->time[last]); */
       /*printf("time = %f\n",(float) example->time[last]);*/
       backward[i] = xhat;
       if (mcov_mat(backward[i].var) == 0) {
	 printf("not covariance here matrix in compute_backward_dists\n");
	 mp(backward[i].var);
	 exit(0);
       }
       /*       printf("xhat after init_after_obs\n");
		mp(xhat.mean);
		mp(xhat.var); */


       for (i=i-1; i >= example->start; i--) {  /* compute backward probs */
	   time =  example->time[i-example->start]; 
	   innov = transform(msc(mm(mi(A[i]),S),-1.),hat[i+1]);
	   R = get_obs_matrix(score.solo.note[i].obs_var);
	   backward[i] = xhat = state_update(xhat,mi(A[i]),innov,C,/*RR*/R,time);
	   if (mcov_mat(backward[i].var) == 0) {
	     printf("not covariance over here matrix in compute_backward_dists\n");
	     mp(backward[i].var);
	     exit(0);
	   }
	   /*printf("i = %d time = %f obs var = %f\n",i,time,score.solo.note[i].obs_var);
 printf("beta[] = \n");
 mp(backward[i].mean);
 mp(backward[i].var); */
	   if (i == start_phrase) break;
       }
 }


 static MX
 stack_matrices(MX A, MX B) {
   MX M;
   int i,j;

   M.rows = A.rows + B.rows;
   if (A.cols != B.cols) {
     printf("matrices incompatible in stack_matrices\n");
     exit(0);
   }
   for (i=0; i < A.rows; i++) for (j=0; j < A.cols; j++)
     M.el[i][j] = A.el[i][j];
   for (i=0; i < B.rows; i++) for (j=0; j < B.cols; j++)
     M.el[A.rows + i][j] = B.el[i][j];
   return(M);
 }


 static void
 part_inverse(MX *A11, MX *A12, MX *A21, MX *A22,
	      MX *I11, MX *I12, MX *I21, MX *I22) {
   MX A22I;


   A22I = mi(*A22);
   *I11 = mi(ms(*A11,mm(*A12,mm(A22I,*A21))));
   *I12 = msc(mm(*I11,mm(*A12,A22I)),-1.);
   *I21 = mt(*I12);
   *I22 = mm(ms(identity(STATE_DIM),mm(*I21,*A12)),A22I);
   /*   printf("------------\n");   
   mp(ma(mm(*I11,*A11),mm(*I12,*A21)));
   mp(ma(mm(*I11,*A12),mm(*I12,*A22)));
   mp(ma(mm(*I21,*A11),mm(*I22,*A21)));
   mp(ma(mm(*I21,*A12),mm(*I22,*A22)));
   printf("------------\n");   */
 }


 static JN
 compute_update(JN forward, JN backward, 
		JN link, MX A) {
   /* forward for state i, backward for i+1 */
 MX f11,f12,f21,f22,fm1,fm2;
 MX i11,i12,i21,i22,im1,im2;
 MX c11,c12,c21,c22,cm1,cm2;
 MX b22,bm2,b22i,t1,t2,temp;
 JN update;

 /*forward.mean = zeros(2,1);
forward.var = identity(2);

backward.mean = zeros(2,1);
backward.var = identity(2);
backward.mean.el[1][0] = 1;

link.mean = zeros(2,1);
link.var = msc(identity(2),100000.);

A = identity(2); */

 /*  link.var = msc(identity(2),1000000.); */
  /*  link.mean = zeros(2,1);  */
	 f11 = forward.var;  /* distribution on x[i],x[i+1] | past */
	 f21 = mm(A,f11);
	 f12 = mt(f21);
	 f22 = ma(mm(f21,mt(A)),link.var); 
	 fm1 = forward.mean;
	 fm2 = ma(mm(A,fm1),link.mean);
	  printf("link\n");
mp(link.mean);
 mp(link.var);
 /* printf("f matrix\n");
 mp(f11);
 mp(f12);
 mp(f21);
 mp(f22);*/



	 b22 = backward.var;  /* distribution on future | x[i+1] */
	 bm2 = backward.mean;
	 /*printf("backward variance\n");
mp(b22); */

	 part_inverse(&f11,&f12,&f21,&f22,&i11,&i12,&i21,&i22);
	 b22i = mi(b22);
	 t1 = ma(mm(i11,fm1),mm(i12,fm2)); 
	 t2 = ma(ma(mm(i21,fm1),mm(i22,fm2)),mm(b22i,bm2));
	  i22 = ma(i22,b22i); 

	  /*printf("b22i\n");
mp(b22i);

printf("i matrix\n");
mp(i11);
mp(i12);
mp(i21);
mp(i22); */
	
	part_inverse(&i11,&i12,&i21,&i22,&c11,&c12,&c21,&c22);
	cm1 = ma(mm(c11,t1),mm(c12,t2));
	cm2 = ma(mm(c21,t1),mm(c22,t2));  /* square completed */

		printf("completed square\n");
mp(cm1);
mp(cm2);
mp(c11);
mp(c12);
mp(c21);
mp(c22);
	update.mean = ms(cm2,mm(A,cm1));
	temp = mm(A,mm(c11,mt(A)));
	temp = ms(temp,mm(A,c12));
	temp = ms(temp,mm(c21,mt(A)));
	temp = ma(temp,c22);
	update.var = temp;  /* distribution on update */
		printf("internal update\n");
mp(update.mean);
mp(update.var);
/*exit(0); */
	return(update);
}


static
compute_update_ests(example,forward,backward,update)
EXAMPLE *example;
JN *forward,*backward,*update; {
    JN xhat,ydist,x1,x2,joint,future,back,left,rite,temp;
    int top,bot,i;
    MX B;

    top = (example->end > end_phrase) ? end_phrase : example->end;
    bot = (example->start < start_phrase) ? start_phrase : example->start;
    if (bot == start_phrase) {
      /*      printf("back = \n");
      mp(backward[bot].mean);
      mp(backward[bot].var);
      printf("hat = \n");
      mp(hat[bot].mean);
      mp(hat[bot].var); */

      update[bot] = full_complete_square(backward[bot],hat[bot]);
      /*      printf("square = \n");
      mp(update[bot].mean);
      mp(update[bot].var); */
    }
    for (i=bot; i < top; i++) {
      x1 = transform(mm(mt(S),A[i]),forward[i]);  /* dist of S'Ax(i)  (based on past) */


	      if(mdet(x1.var) < 0) {
		printf("negative determinant in x1 of compute_update_ests\n");
		mp(x1.var);
		exit(0); 
	      }


        back = transform(mt(S),backward[i+1]);   /* dist of S'x(i+1) (based on future) */


	      if(mdet(back.var) < 0) {
		printf("negative determinant in back of compute_update_ests\n");
		mp(back.var);
		exit(0); 
	      }



	temp.var = ma(back.var,x1.var);
	if( mdet(temp.var) < 0) {
		printf("negative determinant in temp of compute_update_ests\n");
		mp(temp.var);
		printf("det = %f\n",mdet(temp.var));
		mp(back.var);
		printf("det = %f\n",mdet(back.var));
		mp(x1.var);
		printf("det = %f\n",mdet(x1.var));
		printf("backward[i+1].var = \n");
		mp(backward[i+1].var);


		exit(0); 
	      }


	update[i+1].mean = ms(back.mean,x1.mean);
	update[i+1].var = ma(back.var,x1.var);


	      if(mdet(hat[i+1].var) < 0) {
		printf("negative determinant in compute_update_ests\n");
		mp(hat[i+1].var);
		exit(0); 
	      }

	      if(mdet(update[i+1].var) < 0) {
		printf("negative determinant int update[i+1] in compute_update_ests\n");
		mp(update[i+1].var);
		exit(0); 
	      }



	      update[i+1] = full_complete_square(update[i+1],hat[i+1]);

	/*		printf("old update\n");
	mp(update[i+1].mean);
	mp(update[i+1].var); */

	/*		update[i+1] = compute_update(forward[i], backward[i+1],  hat[i+1], A[i]);  */

		/*		if (i == 2) {
		  printf("forward\n");
		  mp(forward[i].mean);
		  mp(forward[i].var);
		  printf("backward\n");
		  mp(backward[i+1].mean);
		  mp(backward[i+1].var);
		  printf("update est \n");
		  mp(update[i+1].mean);
		  mp(update[i+1].var);
		} */

	/*	printf("new update\n");
	mp(update[i+1].mean);
	mp(update[i+1].var); */
	
    
    
    }
}

static
in_range(i,examp)
int i; 
EXAMPLE *examp; {
    return( i >= examp->start && i <= examp->end);
}

static
old_compute_direction(innov_est) 
JN **innov_est; {
    
    int i,s,bool,count;  /* count is taken from global ncount array */
    MX temp,v,xbar,mxbar,xxt,post_cov,cross;
    EXAMPLE *ex;

    for (i=start_phrase; i <= end_phrase;  i++) {
	xbar = mxbar = newhat[i].mean = zeros(UPDATE_DIM,1);
	xxt =  newhat[i].var = zeros(UPDATE_DIM,UPDATE_DIM); 
	post_cov  = zeros(UPDATE_DIM,UPDATE_DIM); 
	count = 0;
	for (s=0; s < num_examps; s++) {
	    ex = phrase_examp[s];
	    if (i == start_phrase) bool =  (i >= ex->start);
	    else bool =  (i > ex->start && i <= ex->end);
	    if (bool)  {
		/* innov_est[i][s] set */ 
		count++;
		temp = innov_est[s][i].mean;
		/*		if (i == 0)	{	mp(temp); printf("rat = %f\n",temp.el[0][0]/temp.el[1][0]); } */
		xbar = ma(temp,xbar);
		xxt =  ma(mm(temp,mt(temp)),xxt);
		post_cov = ma(post_cov,innov_est[s][i].var);
		if (mdet(innov_est[s][i].var) < 0) {
		  printf("innov[%d][%d].var has negative det\n",s,i);
		  mp(innov_est[s][i].var);
		  printf("i = %d ex->start = %d ex->end = %d\n",i,ex->start,ex->end);
		  printf("start phrase = %d end_phrase = %d\n",start_phrase,end_phrase);
		  exit(0);
		}

		if (i == -1/*start_phrase*/) {
		  printf("s = %d count = %d start time = %f\n",s,count,ex->time[0]);
		  printf("s = %d count = %d start time = %f\n",s,count,ex->time[1]);
		  printf("temp = \n");
		  mp(temp);
		  printf("xbar = \n");
		  mp(xbar);
		  printf("xxt = \n");
		  mp(xxt);
		}
	    }
	}
	if (count != ncount[i]) {
	  printf("count = %d ncount[%d] = %d\n",count,i,ncount[i]);
	  exit(0);
	}
	count = ncount[i];
	if (count) {
	  mxbar = xbar = msc(xbar,1./count);
	  post_cov  = msc(post_cov,1./count);
	  xxt = msc(xxt,1./count);
	  /*	     mxbar.el[0][0] = 0.;  /* constraining stretch to be 0 */ 
	  newhat[i].mean = mxbar;
	  newhat[i].var = xxt;
	  
	  cross = mm(mxbar,mt(mxbar));  

	  /*	  mp(newhat[i].var);
		  mp(cross); */
	  newhat[i].var = ms(newhat[i].var,cross);



	  /*	  if (i == 46) {printf("i = %d\n",i); mp(newhat[i].var); }*/
	  /* the empirical cov of estimated means */
	  /* new correct EM way */
	  /*	   newhat[i].var = ma(newhat[i].var,innov_est[0][i].var);  */
	   newhat[i].var = ma(newhat[i].var,post_cov);

	   if (mdet(newhat[i].var) < 0) {
	     printf("negative determinant (%f)\n",mdet(newhat[i].var));
	     printf("count = %d i = %d\n",count,i);
	     mp(newhat[i].var);
	     mp(post_cov);
	     printf("post_cov det = %f\n",mdet(post_cov));
	     	     exit(0);
	   }

	   /* any set version of innov[s][i].var will do here but there
 is not guarantee that the s=0th example (what you have above) will 
 be set here*/

	  /* are you sure that innov_est[0][i] is set */
	  /* the posterior variance doesn't depend on 1st argument so just use 0*/
	   newhat[i].var = msym(newhat[i].var);
	  
	  /*printf("i = %d\n",i);
	    mp(innov_est[0][i].var);
	    mp(innov_est[1][i].var);
	    mp(innov_est[2][i].var);
	    mp(newhat[i].var); */
	  
	  /*	 old way   newhat[i].var = ms(newhat[i].var,mm(xbar,mt(mxbar)));
		 newhat[i].var = ms(newhat[i].var,mm(mxbar,mt(xbar)));
		 newhat[i].var = ma(newhat[i].var,mm(mxbar,mt(mxbar))); */
	  
	   /*	     if (i > 90) {
	    newhat[i].var = hat[i].var;  
	    	    	    newhat[i].mean = hat[i].mean;
	  }*/
	}
	else newhat[i] = hat[i];
	/* differences between this est and last  */
	incr[i].mean = ms(newhat[i].mean,hat[i].mean);  
	incr[i].var = ms(newhat[i].var,hat[i].var);
    }
}


static
compute_direction(innov_est) 
JN **innov_est; {
    
    int count,i,s,bool[MAX_EXAMPS];  /* count is taken from global ncount array */
    MX temp,v,xbar,mxbar,xxt,post_cov,cross;
    EXAMPLE *ex;

    for (i=start_phrase; i <= end_phrase;  i++) {
	xbar = mxbar = newhat[i].mean = zeros(UPDATE_DIM,1);
	xxt =  newhat[i].var = zeros(UPDATE_DIM,UPDATE_DIM); 
	post_cov  = zeros(UPDATE_DIM,UPDATE_DIM); 
	count = ncount[i];



	for (s=0; s < num_examps; s++) {
	    ex = phrase_examp[s];
	    if (i == start_phrase) bool[s] =  (i >= ex->start);
	    else bool[s] =  (i > ex->start && i <= ex->end);
	}
	for (s=0; s < num_examps; s++)  if (bool[s]) {
	  temp = innov_est[s][i].mean;
	  xbar = ma(temp,xbar);
	  post_cov = ma(post_cov,innov_est[s][i].var);
	}
	if (count) {
	  xbar = msc(xbar,1./count);
	  post_cov  = msc(post_cov,1./count);
	}
	for (s=0; s < num_examps; s++)  if (bool[s]) {
	  temp = innov_est[s][i].mean;
	  temp = ms(temp,xbar);
	  xxt = ma(xxt,mm(temp,mt(temp)));
	}
	if (count) {
	  xxt = msc(xxt,1./count);
	  newhat[i].mean = xbar;
	  newhat[i].var = msym(ma(xxt,post_cov));
	}
	else newhat[i] = hat[i];


	  if (mdet(newhat[i].var) < 0) {
	    printf("negative determinant (%f)\n",mdet(newhat[i].var));
	    printf("count = %d i = %d\n",count,i);
	    mp(newhat[i].var);
	    mp(post_cov);
	    printf("post_cov det = %f\n",mdet(post_cov));
	    exit(0);
	  }

	incr[i].mean = ms(newhat[i].mean,hat[i].mean);  
	incr[i].var = ms(newhat[i].var,hat[i].var);
    }
}





#define STEPPE .1 /*.2  /* should divide 1 */

static float
one_d_search() {
    float last,t,cur,ov,nv,val,m,mint;
    int i;

    /*   m = HUGE_VAL;
    for (t=0; t <= 3; t += STEPPE) {
	val = search_func(t);
	if (val < m) {
	    m = val;
	    mint = t;
	}
    }
    t = mint;   */
    t = 1.;  /* just regular EM */




/*    if (t > 0 && t < 1) 
	brent(t-STEPPE,t,t+STEPPE,search_func,.001,&t);  */
/*    last = search_func(0.);   /* minimizing search_func() */
/*    t = 2;
    do {
	t /= 2.;
	cur = search_func(t);

    } while (cur > last); /* think about sign of inequality */
/*    brent(0.,t,2*t,search_func,.001,&t); */
    for (i=start_phrase; i <= end_phrase; i++) {
	newhat[i].mean = ma(hat[i].mean,msc(incr[i].mean,t));
	newhat[i].var = ma(hat[i].var,msc(incr[i].var,t));
    }
    ov = data_log_like(hat);
    nv = data_log_like(newhat);
    val =  ov/nv - 1; 
    /*        printf("hat = \n");
mp(hat[start_phrase].mean); 
mp(hat[start_phrase].var);   */

    printf("old value = %f new value = %f t = %f val = %f\n",ov,nv,t,val);
    for (i=start_phrase; i <= end_phrase; i++) hat[i] = newhat[i]; 
    return(val);
}


#define PRIOR_WEIGHT 3. /*.5*/ /*3. */

static void
mollify_estimates(innov_est) 
JN **innov_est; {
    
    int i;
    MX temp;
    float count;
    JN prior;

                printf("not mollifying\n");
		  return;    
    printf("mollifying estimates\n");
    for (i=start_phrase; i <= end_phrase;  i++) {  
      /*    for (i=start_phrase+1; i <= end_phrase;  i++) {  
 used to have this: don't mollify state states */
      count = ncount[i];
      prior = (i == start_phrase) ? 
	init_start_state(score.meastime) : 
	init_update(score.solo.note[i-1].length);
      /*      printf("i = %d start  = %d\n",i,start_phrase);
	      mp( prior.mean);*/

      temp = ma(msc(hat[i].mean,count), msc(prior.mean,PRIOR_WEIGHT));
      hat[i].mean = msc(temp,1/(count+PRIOR_WEIGHT));
      temp = ma(msc(hat[i].var,count), msc(prior.var,PRIOR_WEIGHT));
      hat[i].var = msc(temp,1/(count+PRIOR_WEIGHT));
    }
}





/* assumes A[] and other matrices set 
  start_phrase, end_phrase , num_examples, phrase_examp[]   set */



static
new_fb() {
    int iter,s,i;
    JN *forward,*backward,*innov_est[MAX_EX];
    float val;
    EXAMPLE *ex;






    forward = alloc_jn(num_notes) - start_phrase;
    backward = alloc_jn(num_notes) - start_phrase;
    for (s=0; s < num_examps; s++) 
	innov_est[s] = alloc_jn(num_notes) - start_phrase;
    for (iter = 0; iter < MAX_ITER; iter++) {
	printf("iter = %d\n",iter);
	global_iter = iter;
	for (s=0; s < num_examps; s++) {   /* for each example */
	    compute_foward_dists(phrase_examp[s],forward); 
	    compute_backward_dists(phrase_examp[s],backward); 
	    compute_update_ests(phrase_examp[s],forward,backward,innov_est[s]);
	    /*printf("innov[0] = \n");
mp(innov_est[s][0].mean); 
mp(forward[0].mean);
mp(forward[0].var);
mp(backward[0].mean);
mp(backward[0].var);
printf("value = %f\n",phrase_examp[s]->time[0]);
exit(0);
*/



/*for (i=start_phrase; i <= end_phrase; i++) {
    ex = phrase_examp[s];
    printf("i = %d time = %f\n",i,ex->time[i-ex->start]);
    mp(forward[i].mean);
    mp(backward[i].mean);
    mp(innov_est[s][i].mean);
    mp(innov_est[s][i].var);
getchar();
getchar();
} */
/* return;  */
	}
	compute_direction(innov_est);
/*for (i=start_phrase; i <= end_phrase; i++) {
    printf("i = %d\n",i);
    mp(incr[i].mean);
    mp(incr[i].var);
} 
return; */
    
	val = one_d_search();
	if (val < PRECISION) break;
    }
}




static
test_likelihoods() {
  float x;
  
  global_print_flag = 1;
  x = data_log_like(hat);
  global_print_flag = 0;
  printf("likelihood is %f\n",x);
}



void
play_seq() {
  FILE *fp;
  char name[500];
  int i=0,pitch,vel,status;
  float time,t0,start;

  mode = NO_SOLO_MODE;
  printf("enter the name of the sequence file\n");
  scanf("%s",name);
  if ((fp = fopen(name,"r")) == NULL) { printf("couldn't open %s\n",name); exit(0); }
  for (i=0; i < MAX_ACCOMP_NOTES; i++) {
    fscanf(fp,"%f %x %d %d",&time,&status,&pitch,&vel);
    if (i == 0) t0 = time-1;
    printf("time = %f status = %x pitch = %d vel = %d\n",time,status,pitch,vel);
    score.midi.burst[i].ideal_secs = time-t0;
    score.midi.burst[i].action.num = 1;
    score.midi.burst[i].action.event = (MIDI_EVENT *) malloc(sizeof(MIDI_EVENT));  
    score.midi.burst[i].action.event[0].secs = time;
    score.midi.burst[i].action.event[0].volume = vel;
    score.midi.burst[i].action.event[0].command = status; /*0x90; /*NOTE_ON*/
    score.midi.burst[i].action.event[0].notenum = pitch;
    if (feof(fp)) break;
  }
  if (i == MAX_ACCOMP_NOTES) { printf("couln't fit whole seq file %s\n",name); }
  score.midi.num = i;
  last_accomp = i-1;
  new_start_cond(0.,1000.,1.) ;
  queue_event();
 // while (cur_accomp <= last_accomp) sleep(1);/* printf("cur accomp = %d last_accomp = %d\n",cur_accomp,last_accomp); */
  end_midi();
}

void
play_solo_part_regular() {
  FILE *fp;
  char name[500],ts[500];
  int i=0,pitch,vel,status,p,lastp,num,den,bpm;
  float time,t0,start,ms;
  RATIONAL start_pos,end_pos,r;

  mode = NO_SOLO_MODE;  /* kludge for new_start_cond */
  printf("enter the start time (d+d/d)\n");
  scanf("%s",ts);
  start_pos  = string2wholerat(ts);
  printf("enter the end time (d+d/d)\n");
  scanf("%s",ts);
  end_pos  = string2wholerat(ts);
  printf("enter tempo (p/q = bpm): ");
  num = den = bpm = 0;
  scanf("%d/%d = %d",&num,&den,&bpm);
  if (num == 0 || den == 0 || bpm == 0) {
    printf("tempo entered incorrectly\n");
    exit(0);
  }
  ms = (float) (60*den) / (float) (num*bpm); 



  t0 = start_pos.num / (float) start_pos.den;
  lastp = 0;
  for (i=0; i < score.solo.num; i++) {
    r = score.solo.note[i].wholerat;
    p = score.solo.note[i].num;
    if (rat_cmp(r,start_pos) < 0) continue; 
    if (rat_cmp(r,end_pos) > 0) continue; 
    time = r.num/(float) r.den;
    score.midi.burst[i].ideal_secs = ms*(time-t0);
    score.midi.burst[i].action.num = 2;
    score.midi.burst[i].action.event = (MIDI_EVENT *) malloc(2*sizeof(MIDI_EVENT));  

    score.midi.burst[i].action.event[0].secs = time;
    score.midi.burst[i].action.event[0].volume = 0;
    score.midi.burst[i].action.event[0].command = NOTE_ON;
    score.midi.burst[i].action.event[0].notenum = lastp;

    score.midi.burst[i].action.event[1].secs = time;
    score.midi.burst[i].action.event[1].volume = 50;
    score.midi.burst[i].action.event[1].command = NOTE_ON;
    score.midi.burst[i].action.event[1].notenum = p;
    lastp = p;
  }
  score.midi.num = score.solo.num;
  last_accomp = score.solo.num-1;
  new_start_cond(0.,1000.,1.) ;
  queue_event();
 // while (cur_accomp <= last_accomp) sleep(1); printf("cur accomp = %d last_accomp = %d\n",cur_accomp,last_accomp);
  end_midi();
}

void
play_solo_part_polyphonic() {
  FILE *fp;
  char name[500],ts[500];
  int i=0,pitch,vel,status,p,lastp,num,den,bpm;
  float time,t0,start,ms;
  RATIONAL start_pos,end_pos,r;

  mode = NO_SOLO_MODE;  /* kludge for new_start_cond */
  printf("enter the start time (d+d/d)\n");
  scanf("%s",ts);
  start_pos  = string2wholerat(ts);
  printf("enter the end time (d+d/d)\n");
  scanf("%s",ts);
  end_pos  = string2wholerat(ts);
  printf("enter tempo (p/q = bpm): ");
  num = den = bpm = 0;
  scanf("%d/%d = %d",&num,&den,&bpm);
  if (num == 0 || den == 0 || bpm == 0) {
    printf("tempo entered incorrectly\n");
    exit(0);
  }
  ms = (float) (60*den) / (float) (num*bpm); 


  t0 = start_pos.num / (float) start_pos.den;
  lastp = 0;
  for (i=0; i < score.solo.num; i++) {
    score.midi.burst[i].action.num = 0;
    r = score.solo.note[i].wholerat;
    p = score.solo.note[i].num;
    if (rat_cmp(r,start_pos) < 0) continue; 
    if (rat_cmp(r,end_pos) > 0) continue; 
    time = r.num/(float) r.den;
    score.midi.burst[i].ideal_secs = ms*(time-t0);

    score.midi.burst[i].action =  score.solo.note[i].action;
    printf("time = %f\n",time);
    continue;
    score.midi.burst[i].action.num = 2;
    score.midi.burst[i].action.event = (MIDI_EVENT *) malloc(2*sizeof(MIDI_EVENT));  

    score.midi.burst[i].action.event[0].secs = time;
    score.midi.burst[i].action.event[0].volume = 0;
    score.midi.burst[i].action.event[0].command = NOTE_ON;
    score.midi.burst[i].action.event[0].notenum = lastp;

    score.midi.burst[i].action.event[1].secs = time;
    score.midi.burst[i].action.event[1].volume = 50;
    score.midi.burst[i].action.event[1].command = NOTE_ON;
    score.midi.burst[i].action.event[1].notenum = p;
    lastp = p;
  }
  score.midi.num = score.solo.num;
  last_accomp = score.solo.num-1;
  new_start_cond(0.,1000.,1.) ;
  queue_event();
 // while (cur_accomp <= last_accomp) sleep(1); printf("cur accomp = %d last_accomp = %d\n",cur_accomp,last_accomp);
  end_midi();
}




void
play_solo_part() {
#ifdef  POLYPHONIC_INPUT_EXPERIMENT
  play_solo_part_polyphonic();
#else
  play_solo_part_regular();
#endif
}



static void
read_seq_to_midi(char *name) {
  FILE *fp;
  int i=0,pitch,vel,status;
  float time,t0,start;

  if ((fp = fopen(name,"r")) == NULL) { printf("couldn't open %s\n",name); exit(0); }
  for (i=0; i < MAX_ACCOMP_NOTES; i++) {
    fscanf(fp,"%f %x %d %d",&time,&status,&pitch,&vel);
    if (i == 0) t0 = time-1;
    printf("time = %f status = %x pitch = %d vel = %d\n",time,status,pitch,vel);
    score.midi.burst[i].ideal_secs = time-t0;
    score.midi.burst[i].action.num = 1;
    score.midi.burst[i].action.event = (MIDI_EVENT *) malloc(sizeof(MIDI_EVENT));  
    score.midi.burst[i].action.event[0].secs = time;
    score.midi.burst[i].action.event[0].volume = vel;
    score.midi.burst[i].action.event[0].command = status; /*0x90; /*NOTE_ON*/
    score.midi.burst[i].action.event[0].notenum = pitch;
    if (feof(fp)) break;
  }
  if (i == MAX_ACCOMP_NOTES) { printf("couln't fit whole seq file %s\n",name); }
  score.midi.num = i;
}
    
 
static void 
add_to_seq(SEQ_LIST *seq, float secs, int command, int pitch, int vel) {

  if (seq->num >= MAX_SEQ-1) {
    printf("out of room in add_to_seq\n");
    exit(0);
  }
  seq->event[seq->num].time = secs;
  seq->event[seq->num].command = command;
  seq->event[seq->num].d1 = pitch;
  seq->event[seq->num].d2 = vel;
  seq->num++;
}

#define ANNOTATE_VOL 75

static void
set_markers(SEQ_LIST *mark) {
  int i;
  RATIONAL pos;
  ACCOMPANIMENT_NOTE *a;

  mark->num = 0;
  for (i=0; i < score.accompaniment.num; i++) {
    a = &score.accompaniment.note[i];
    if (a->found == 0) continue;
    //    printf("start_time = %f\n",a->start_time);
    pos = wholerat2measrat(a->wholerat);
    //    printf("%d/%d\n",pos.num,pos.den);
    if (pos.num == 0)	add_to_seq(mark,a->start_time,NOTE_ON+1,100,ANNOTATE_VOL);
	     
    if (pos.num == 1 && pos.den == 8) add_to_seq(mark,a->start_time,NOTE_ON+2,101,ANNOTATE_VOL);
    if (pos.num == 1 && pos.den == 4)  add_to_seq(mark,a->start_time,NOTE_ON+3,102,ANNOTATE_VOL);
    if (pos.num == 3 && pos.den == 8)  add_to_seq(mark,a->start_time,NOTE_ON+4,103,ANNOTATE_VOL);
  }
}

static int
seq_cmp(const void *p1, const void *p2) {
  SEQ_EVENT *e1, *e2;

  e1 = (SEQ_EVENT *) p1;
  e2 = (SEQ_EVENT *) p2;
  if (e1->time < e2->time) return(-1);
  if (e1->time > e2->time) return(1);
  return(0);
}



static void
merge_seq(SEQ_LIST *s1, SEQ_LIST *s2) {
  int i;

  if (s1->num + s2->num > MAX_SEQ) {
    printf("not enough room to merge\n");
    exit(0);
  }
  for (i=0; i < s2->num; i++) s1->event[s1->num+i] = s2->event[i];
  s1->num += s2->num;
  qsort(s1->event,s1->num,sizeof(SEQ_EVENT),seq_cmp);
  //  for (i=0; i < s1->num; i++)
  //    printf("%f %d %d %d\n",s1->event[i].time,s1->event[i].command,s1->event[i].d1,s1->event[i].d2);
  //  for (i=0; i < s1->num; i++)
  //    printf("%f %d %d %d\n",s2->event[i].time,s2->event[i].command,s2->event[i].d1,s2->event[i].d2);
}


static void
select_seq(SEQ_LIST *seq) {
  int i,j;
  float t0,t1;

  for (i=0; i < score.accompaniment.num; i++) 
    if (rat_cmp(score.accompaniment.note[i].wholerat,start_pos) >= 0) {
      t0 = score.accompaniment.note[i].start_time;
      break;
    }
  for (i=score.accompaniment.num-1; i >= 0;  i--) 
    if (rat_cmp(score.accompaniment.note[i].wholerat,end_pos) <= 0) {
      t1 = score.accompaniment.note[i].start_time;
      break;
    }
  //  printf("t0 = %f t1 = %f\n",t0,t1);
  for (i=0; i < seq->num; i++) if (seq->event[i].time >= (t0-.05)) break;
  for (j=i; j < seq->num; j++) {
    if (seq->event[i].time > t1) break;
    seq->event[j-i] = seq->event[j];
    seq->event[j-i].time -= (t0-1);
  }
  seq->num = j-i;
}

static void
quantize_seq(SEQ_LIST *seq) {
  int i;

  for (i=0; i < seq->num; i++) {  /* quantize to millisecs */
    seq->event[i].time = ((int) 1000*seq->event[i].time) / 1000.;
  }
}

static void
seq2midi(SEQ_LIST *seq) {
  int i,j1,j2,j,k,n,i0,c;

  i0 = j = 0;
  for (i=0; i < seq->num; i++) {
    if (fabs(seq->event[i].time-seq->event[i0].time) > .001) {
      n = i-i0;
      score.midi.burst[j].action.event = 
	(MIDI_EVENT *) malloc(n*sizeof(MIDI_EVENT));  
      score.midi.burst[j].action.num = n;
      score.midi.burst[j].ideal_secs = seq->event[i0].time;
      for (k=0; k < n; k++) {
	//	score.midi.burst[j].action.event[k].secs = seq->event[i0+k].time;
	score.midi.burst[j].action.event[k].volume = seq->event[i0+k].d2;
	score.midi.burst[j].action.event[k].command = seq->event[i0+k].command;
	score.midi.burst[j].action.event[k].notenum = seq->event[i0+k].d1;
      }
      j++;
      i0 = i;
    }
  }
  score.midi.num = j;
}


static void
set_up_instruments() {
  int chan,prog;

  /* the percussion instruments seem to mess up the alesis somehow 
   eg 115,116,117 etc*/

  midi_change_program(chan=0,prog=0);   /* AcGrandPno */
  midi_change_program(chan=1,prog=4); /* crotale? */
  midi_change_program(chan=2,prog=13); /*  */
  midi_change_program(chan=3,prog=12); /* marimba */
  midi_change_program(chan=4,prog=11); 


}

void
annotate_match() {  /* play back a sequencer file with clicks and things to mark matching */
  FILE *fp;
  char name[500],match[500],ts[500];
  int i=0,pitch,vel,status,j;
  float time,t0,start;
  SEQ_LIST seq,mark;
  MIDI_EVENT e;

  mode = NO_SOLO_MODE;
  printf("enter the name of the file with no extension (must have .seq and .mch)\n");
  scanf("%s",name);
  printf("enter the start time (%d+%d/%d)\n");
  scanf("%s",ts);
  start_pos  = string2wholerat(ts);
  printf("enter the end time (%d+%d/%d)\n");
  scanf("%s",ts);
  end_pos  = string2wholerat(ts);
  read_seq_file(name,&seq);
  read_match(name);
  set_markers(&mark);
  merge_seq(&seq,&mark);
  select_seq(&seq);
  seq2midi(&seq);
  last_accomp = score.midi.num-1;

  /*  for (i=0; i < score.midi.num; i++) {
    printf("note %s %f\n",score.midi.burst[i].observable_tag,score.midi.burst[i].time);
    for (j=0; j < score.midi.burst[i].action.num; j++) {
      e = score.midi.burst[i].action.event[j];
      num2name(e.notenum,name);
      printf("command = %x\n note =  %s\nvol = %d\n time = %f\n\n",
	     e.command,name,e.volume,e.meas);    
    }
    }*/


  new_start_cond(0.,1000.,1.);
  set_up_instruments();
  queue_event();
  while (cur_accomp <= last_accomp) { 
 //   sleep(1);
    /*printf("cur accomp = %d last_accomp = %d\n",cur_accomp,last_accomp);*/ 
  }
  end_midi();
}
    
  



static
kal_play_events() {
    int num,den,bpm;
    float ms,s,e,start,end;
    extern float cur_meas;
    int i,first_event,n,j,v;
    extern int cur_event;
    char nn[500],name[500],st[500],en[500];
    ACCOMPANIMENT_NOTE *a;

    printf("enter 1st measure to be played: ");
    scanf("%s",st);
    start = meas2score_pos(st);
    printf("enter last measure to be played: ");
    scanf("%s",en);
    end = meas2score_pos(en);
    printf("enter tempo (p/q = bpm): ");
    num = den = bpm = 0;
    scanf("%d/%d = %d",&num,&den,&bpm);
    if (num == 0 || den == 0 || bpm == 0) {
      printf("tempo entered incorrectly\n");
      exit(0);
    }
    ms = (float) (60*den) / (float) (num*bpm); 
    /*    printf("enter match file (no .mch): ");
	  scanf("%s",name);*/
    printf("start = %f end = %f ms = %f\n",start,end,ms);

    /*    read_match(name); */
    for (i=0; i < score.midi.num; i++) {
      n = score.midi.burst[i].action.num;
      num2name(score.midi.burst[i].action.event[n-1].notenum,nn);
      //           printf("i = %d pitch = %s %f\n",i,nn,score.midi.burst[i].time); 
      if (score.midi.burst[i].time > end) break;
    }
      last_accomp = i-1; 
printf("scorenum = %d last = %d\n",score.midi.num,last_accomp);      


    for (i=event.num-1; i >= 0; i--) {  /* this should be phased out */
      if (event.list[i].meas >= start) {
	first_event = i; 
	if (event.list[i].meas <= end) 
	  event.list[i].secs = ms * (event.list[i].meas-start);
      }
    }
    printf("first_event = %d\n",first_event);
      
    /*        for (i=0; i < score.accompaniment.num; i++) {
      v = (score.accompaniment.note[i].found) ? score.accompaniment.note[i].vel : 0;
      score.accompaniment.note[i].note_on->volume = v;
      }  */


    for (i=score.midi.num-1; i >= 0; i--) {  
      if (score.midi.burst[i].time >= start) {
	if (score.midi.burst[i].time <= end) {
	  score.midi.burst[i].ideal_secs = ms * (score.midi.burst[i].time-start) + 1;
	  /*	  for (j=0; j < score.midi.burst[i].action.num; j++) {
	    if (score.midi.burst[i].action.event[j].command != NOTE_ON) continue;
	    a = score.midi.burst[i].action.event[j].accomp_note;
	    score.midi.burst[i].action.event[j].volume = (a->found) ? a->vel   : 0;
	    }*/
	}
      }
      }
      
    /*    while(1) { */

    new_start_cond(start,end,ms) ;
    queue_event();
    /*    new_conduct(); /* this plays the first events and starts the cycle */
  /*    while (cur_event < event.num && event.list[cur_event].meas < end);*/
    /*printf("cur_accomp = %d last_accomp = %d time = %f end = %f\n",cur_accomp,last_accomp,score.midi.burst[cur_accomp].time,end); */
      while (cur_accomp <= last_accomp && 
	     score.midi.burst[cur_accomp].time <= end) pause();
      /*    } */
  end_midi();
}


music_minus_one() {
    float start,end,ms;
    extern float cur_meas;
    int i,first_event,n,samps=0;
    extern int cur_event;
    char nn[500],a[500];
    extern float now();

    mode = NO_SOLO_MODE; 
    printf("enter the audio data directory: ");
    scanf("%s",audio_data_dir);
    printf("enter start time in measures: ");
    scanf("%f",&start_meas);
    printf("enter end time in measures: ");
    scanf("%f",&end_meas);
    printf("enter measure time in secs: ");
    scanf("%f",&ms);
    printf("start_meas = %f end_meas = %f ms = %f\n",start_meas,end_meas,ms);

    for (i=0; i < score.midi.num; i++)  if (score.midi.burst[i].time > end_meas) break;
    last_accomp = i-1; 
      
    for (i=score.midi.num-1; i >= 0; i--) {  
      if (score.midi.burst[i].time >= start_meas) {
	if (score.midi.burst[i].time <= end_meas) 
	  score.midi.burst[i].ideal_secs = 3 + ms * (score.midi.burst[i].time-start_meas);
      }
    }
    /*        prepare_sampling(); 
    begin_sampling();
    new_start_cond(start_meas,end_meas,ms) ;  */
    new_synchronize(start_meas); 
    queue_event();
    token = 0;
    while (cur_accomp <= last_accomp && score.midi.burst[cur_accomp].time <= end_meas) {
      /*      sleep(1); /* if no command here while condition is never checked */
     
      wait_for_samples();
      token++;
    }
    end_sampling();
    frames = token;
    printf("sampled %d frames\n",frames);
    printf("would you like to save the audio file?");
    scanf("%s",a);
    if (a[0] == 'y') save_labelled_audio_file(); 
    printf("would you like to hear the audio file?");
    scanf("%s",a);
    if (a[0] == 'y') play_audio_buffer();
    end_midi();
    write_accomp_times();
 }






play_accomp() {
  int i;

  //  printf("hello\n");
  mode = NO_SOLO_MODE; 
  kal_play_events();
  eval_accomp_note_times();
  exit(0);
}


void
play_accomp_phrase() {
  float start,end,ms,time;
  extern float cur_meas;
  int i,first_event,phrase;
  extern int cur_event;
  int firsta,lasta,firsts,lasts;

  printf("enter the phrase number:");
  scanf("%d",&phrase);
  printf("enter the measure size in secs:");
  scanf("%f",&ms);
  phrase_span(phrase,&firsts,&lasts,&firsta,&lasta);
  start = score.midi.burst[firsta].time;
  for (i=firsta; i <= lasta; i++) {
    score.midi.burst[i].ideal_secs = ms * (score.midi.burst[i].time-start);
  }
   mode = NO_SOLO_MODE;
   last_accomp = lasta;
   new_start_cond(score.midi.burst[firsta].time,score.midi.burst[lasta].time,1.);
   queue_event(); /* this  queues the first events and starts the cycle*/
   while (cur_accomp < score.midi.num && 
	  cur_accomp <= lasta);
   end_midi();
}


void
play_accomp_phrase_with_recording(int phrase) {
  float start,end,ms,end_secs,ns;
  extern float cur_meas,now();
  int i,first_event;
  extern int cur_event;
  int firsta,lasta,firsts,lasts,cursolo;

   phrase_span(phrase,&firsts,&lasts,&firsta,&lasta);
   for (i=firsta; i <= lasta; i++) 
     printf("%d %f %f\n",i,score.midi.burst[i].ideal_secs,score.midi.burst[i].time);
   /*   exit(0); */
   mode = BELIEF_TEST_MODE;
   last_accomp = lasta;
   end_secs = tok2secs((float)frames) + .5;
   new_synchronize(score.midi.burst[firsta].time);
   queue_event(); /* this  queues the first events and starts the cycle*/
   /*   for (i= firsts; i <= lasts; i++) {
     ns = TOKENLEN*score.solo.note[i].realize/(float)SR + .1;
     while (now() < ns); 
     printf("time for %d\n",i);
   } */
   
   while (cur_accomp < score.midi.num && 
	  /*cur_accomp < lasta*/  now() < end_secs);
   end_midi();
   end_playing(); 
}


void
play_accomp_with_recording() {
  float start,end,ms,end_secs,ns;
  extern float cur_meas,now();
  int i,first_event;
  extern int cur_event;
  int firsta,lasta,firsts,lasts,cursolo;

  mode = BELIEF_TEST_MODE;
   end_secs = tok2secs((float)frames) + .5;
   new_synchronize(score.solo.note[firstnote].time);
   queue_event(); /* this  queues the first events and starts the cycle*/
   while (cur_accomp < score.midi.num && 
	  /*cur_accomp < lasta*/  now() < end_secs);
printf("%d %d\n",cur_accomp,score.midi.num);
printf("%f %f\n",now(),end_secs);
   end_midi();

   end_playing(); 
}



/**************************************************************************/






static
store_results(phrase)
int phrase; {
    int i;

    /*    for (i=start_phrase+1; i <= end_phrase; i++) {
      printf("i = %d\n",i);
      mp(hat[i].var);
      } */
    for (i=start_phrase+1; i <= end_phrase; i++)
	score.solo.note[i].update = hat[i];
    cue.list[phrase].state_init = hat[start_phrase];
    mp(hat[start_phrase].var);
}






static
setup(length,examps,times,note_len,update)
int length;  /* number of notes in phrase */
int examps;  /* number of examps */
float **times;   /* examps x length array of start times */
float *note_len; /* length array of note lengths in measures */
JN *update;   /* array of update distributions (already initialized) */
{
  int i;

  note_length = note_len;
  note_time = times;
  num_examps = examps;
  num_notes = length;
/*  hat = update; */
/*  A = (MX *) malloc(length*sizeof(MX));*/
  for (i=0; i < length; i++) {
    A[i] = identity(STATE_DIM);
    A[i].el[0][1] = note_length[i];
  }
}	     


static
train_kalman(length,examps,times,note_len,update)
int length;  /* number of notes in phrase */
int examps;  /* number of examps */
float **times;   /* examps x length array of start times */
float *note_len; /* length array of note lengths in measures */
JN *update;   /* array of update distributions (already initialized) */
{

  setup(length,examps,times,note_len,update);
  fb();
}	     

static void
train_phrase() {
  int p,start,stop,num,length,examps,i,j,plen;
  float **times,**matrix(),*vector(),*note_len; /*,secs2tokens(); */
  JN *update;
  char ans[10];
  void free_vector(),free_matrix();

  printf("which phrase to train? ");
  scanf("%d",&p);
  printf("training phrase %d\n",p);
  num = -1;
  start = -1;
  while (num < p) {
    if (score.solo.note[++start].cue) num++;
    if (start == score.solo.num -1 && num < p) {
      printf("score does not have %d phrases\n",p);
      return;
    }
  }
  printf("start of phrase is note %d\n",start);
  stop = start;
  while (score.solo.note[++stop].cue == 0) {
    if (stop == score.solo.num -1) break;
  }
  printf("end of phrase is note %d\n",stop);
  plen = length = stop-start;  
  if (stop < score.solo.num-1) length++;
  examps = score.solo.note[start].examp.num;
  for (i = start; i < stop; i++) {
    printf("examps = %d note = %d\n",examps,score.solo.note[i].examp.num);
    if (score.solo.note[i].examp.num != examps) {
      printf("all notes must have same number of examples\n");
      return;
    }
  }
  times = matrix(0,examps-1,0,length-1);
  for (i=0; i < examps; i++) for (j=0; j < length; j++)
    times[i][j] = score.solo.note[j+start].examp.list[i];
  for (i=0; i < examps; i++) for (j=length-1; j >= 0; j--)
    times[i][j] -= times[i][0]; 
  note_len = vector(0,length-1);
  for (i=0; i < length; i++) note_len[i] = score.solo.note[i+start].length;
  update = (JN *) malloc(length*sizeof(JN));
  for (i=0; i < length; i++) update[i] = score.solo.note[i+start].update;
  printf("initalize updates? (y/n):");
  scanf("%s",ans);
  if (ans[0] == 'y') {
    for (i=1; i < length; i++) {
      update[i].mean = zeros(UPDATE_DIM,1);
/*      update[i].var = msc(identity(UPDATE_DIM),1.);  */
      update[i].var = msc(identity(UPDATE_DIM),note_len[i-1]*note_len[i-1]); 
    }
    update[0] = init_start_state(secs2tokens(score.meastime));
  }


  for (i=0; i < length; i++) {
    mp(update[i].mean);
    mp(update[i].var);
  }

for (j=0; j < examps; j++) {
  for (i=0; i < length; i++) printf("%f ",times[j][i]);
  printf("\n");
}
for (i=0; i < length; i++) printf("%f\n",note_len[i]);

  train_kalman(length,examps,times,note_len,update);
  for (i=0; i < length; i++) {
    printf("note = %s length = %f\n",
      sol[score.solo.note[i+start].num].name,score.solo.note[i+start].length);
    mp(update[i].mean);
    mp(update[i].var);
  }
  for (i=0; i < plen/*length*/; i++) score.solo.note[i+start].update = update[i];
  free_matrix(times,0,examps-1,0,length-1);
  free_vector(note_len,0,length-1);
  free(update);
}




#define TRAIN_FROM_SCRATCH_LIM /*5 */ 50 


void 
phrase_span(int phrase, int *start_solo, int *last_solo, int *start_accom, int *last_accom) {
  float s,e;
  int i;

  *start_solo = cue.list[phrase].note_num;
  if (phrase == cue.num-1) {
    *last_solo = score.solo.num-1;
    e = HUGE_VAL;
  }
  else {
    *last_solo = cue.list[phrase+1].note_num-1;
    e = score.solo.note[*last_solo + 1].time;
  }
  s = score.solo.note[*start_solo].time;
  for (i=0; i < score.midi.num; i++) {
    if (score.midi.burst[i].time >= s-.0001) {
      *start_accom = i;
      break;
    }
  }
  for (i=score.midi.num-1; i >= 0; i--) {
    if (score.midi.burst[i].time < e-.0001) {
      *last_accom = i;
      break;
    }
  }
}


static 
new_setup(phrase_num)
int phrase_num;
{
    int i,j,k;
    EXAMPLE *ex;
    float length,t0;


    /*printf("temporarily leaving at 0-basing the examples\n"); */
    start_phrase = cue.list[phrase_num].note_num;
    jn_buff.cur = 0;
    if (phrase_num == cue.num-1) end_phrase = score.solo.num-1;
    else end_phrase = cue.list[phrase_num+1].note_num ;  
    printf("startphrase = %s lastnote = %s\n",score.solo.note[start_phrase].observable_tag,score.solo.note[end_phrase-1].observable_tag); 
    num_notes = end_phrase+1-start_phrase;
    incr = alloc_jn(num_notes) - start_phrase;
    hat  = alloc_jn(num_notes) - start_phrase;
    newhat = alloc_jn(num_notes) - start_phrase;
    explore = alloc_jn(num_notes) - start_phrase;
    ncount = (int *) malloc(sizeof(int)*num_notes) - start_phrase;
    num_examps=0;
    for (i=0; i < score.example.num; i++) {
	ex = score.example.list + i;
	if (ex->start +1 < end_phrase && ex->end-1 > start_phrase) {/* enough overlap */
	    phrase_examp[num_examps++] = ex;
	    t0 = ex->time[start_phrase-ex->start];   
	    for (j=ex->start; j <= ex->end; j++) {
	      k = j-ex->start;
	      ex->time[k] -= t0;
	      /*	      printf("note = %s\n",score.solo.note[k].observable_tag); */
	       /*  printf("start_phrase = %d j = %d time =  %f\n",start_phrase,j,ex->time[j-ex->start]); */
	    }
		
	}
    }
    for (i=start_phrase; i <= end_phrase; i++) {
	length = score.solo.note[i].length;
	A[i] = state_eqn_mat(length); 
    }
    for (i=start_phrase+1; i <= end_phrase; i++) {
	length = score.solo.note[i-1].length;
	if (num_examps <TRAIN_FROM_SCRATCH_LIM )	hat[i] = init_update(length);
	else	hat[i] = score.solo.note[i].update; 
	/*	printf("note = %s\n",score.solo.note[i].observable_tag);  */
    }
    if (num_examps < TRAIN_FROM_SCRATCH_LIM)  hat[start_phrase] = init_start_state(score.meastime);
    else     hat[start_phrase] = cue.list[phrase_num].state_init; 
    compute_counts(); 
    init_matrices();	     
}


#ifdef IAMCRAZY
belief_train_setup(phrase_num)
int phrase_num;
{
    int i;
    EXAMPLE *ex;
    float length;
    BELIEF_NET  bn;
    QUAD_FORM init,*ptr;
    BNODE *b;

    start_phrase = cue.list[phrase_num].note_num;
    if (phrase_num == cue.num-1) end_phrase = score.solo.num-1;
    else end_phrase = cue.list[phrase_num+1].note_num;
    bn = make_solo_graph(start_phrase, end_phrase);

    
    num_notes = end_phrase+1-start_phrase;
    incr = alloc_jn(num_notes) - start_phrase;
    hat  = alloc_jn(num_notes) - start_phrase;
    newhat = alloc_jn(num_notes) - start_phrase;
    explore = alloc_jn(num_notes) - start_phrase;
    num_examps=0;
    for (i=0; i < score.example.num; i++) {
	ex = score.example.list + i;
	if (ex->start < end_phrase && ex->end > start_phrase) /* any overlap */
	    phrase_examp[num_examps++] = ex;
    }
    for (i=start_phrase+1; i <= end_phrase; i++) {
	length = score.solo.note[i-1].length;
	b = score.midi.burst[i].belief;
	ptr = &(b->clnode->clique->qf); 

	init = (num_examps <TRAIN_FROM_SCRATCH_LIM ) ? 
	  belief_init_update(length) : score.solo.note[i].qf_update; 
	QFcopy(init,ptr);
    }
    b = score.midi.burst[start_phrase].belief;
    ptr = &(b->clnode->clique->qf); 
    if (num_examps < TRAIN_FROM_SCRATCH_LIM)  hat[start_phrase] = init_start_state(score.meastime);
    else     hat[start_phrase] = cue.list[phrase_num].state_init; 
/*    compute_counts(); */
    init_matrices();	     
}

#endif

static
new_train_kalman(phrase)
int phrase; {

    new_setup(phrase);
    new_fb();
    store_results(phrase);
}	     


update_training() {
    int i,j,phrase_len;

    /*printf("enter the number of examples to use: ");
      scanf("%d",&score.example.num);
      printf("using %d examples\n",score.example.num); */
    
    /*printf("currently constraint the stretches to be 0 in mean\n");*/
/* search for the comment "constraining stretch to be 0" to find out where in code */

    printf("have %d phrases: \n",cue.num);
    for (i=0; i < cue.num; i++) {
      if (cue.list[i].trained == 0) {
	printf("updating phrase %d\n",i);
	phrase_len = cue.list[i+1].note_num - cue.list[i].note_num;
	if (i <= cue.num-1 && phrase_len == 1) continue;
	new_setup(i);
	new_fb();
	cue.list[i].trained = 1;   /* phrase now current in training */
	mollify_estimates();
	test_likelihoods();
	store_results(i);
      }
      else printf("phrase %d current\n",i);
    }
    write_updates();
}




init_updates() {
  int i;
  float len,scale;
  MATRIX m,v;

  scale = TOKENLEN/(float)SR;
  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].cue == 0) {
      len = score.solo.note[i-1].length;
      score.solo.note[i].update = init_update(len);
    }
    else  score.solo.note[i].update = init_start_state(score.meastime);
    m = Msc(Mconvert(score.solo.note[i].update.mean),scale);
    v = Msc(Mconvert(score.solo.note[i].update.var),scale*scale);
    if (score.solo.note[i].cue)  score.solo.note[i].qf_update =  QFperm(QFmake_pos_unif(QFmv(m,v)));
    else score.solo.note[i].qf_update = QFperm(QFmv(m,v));
  }
  for (i=0; i < cue.num; i++)
    cue.list[i].state_init = init_start_state(score.meastime);
}

old_init_updates() {
  int i;
  float len;
  MATRIX m,v;

  for (i=1; i < score.solo.num; i++) {
    len = score.solo.note[i-1].length;
    score.solo.note[i].update = init_update(len);
    m = Mconvert(score.solo.note[i].update.mean);
    v = Mconvert(score.solo.note[i].update.var);
    score.solo.note[i].qf_update = QFperm(QFmv(m,v));
  }
  for (i=0; i < cue.num; i++)
    cue.list[i].state_init = init_start_state(score.meastime);
}


static
test_predict() {
  int i,n,j;
  JN innov;
  MX A,X;
  float x;

  n = score.example.num;
  init_updates();
  for (i=0; i < n; i++) {
    printf("i = %d\n",i);
    if (score.example.list[i].start == 0) {
      state_hat = cue.list[0].state_init;
      state_hat.mean.el[POS_INDEX][0] = score.example.list[i].time[0];
      for (cur_note=1; cur_note < 20; cur_note++) {
	A = state_eqn_mat(score.solo.note[cur_note-1].length);
	innov = transform(S,score.solo.note[cur_note].update);
	X = ma(mm(A,state_hat.mean),innov.mean);
	printf("%f %f\n",score.example.list[i].time[cur_note],state2pos(X)); 
	new_kalman_update(score.example.list[i].time[cur_note]); 
      }
      init_updates();
      update_training();
    }
  }
}
	
      


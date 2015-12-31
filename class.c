


 
#include "share.c"
//#include "gammln.c"

#include "class.h"
#include "global.h"
#include "dp.h"
#include "gui.h"
#include "new_score.h"
#include "audio.h"
#include <math.h>
#ifdef SUN_COMPILE 
#include <sunmath.h>
#endif
#include <stdio.h>
#include <stdlib.h> 




/* FEATURE is a generic structure for a specfic probability model as follows.
   We assume that there are a number of possible states s(1) ... s(N)
   for which we wish to train probability distributions.  From each observation
   we extract feature f(1) ... f(M).  We assume a muddle of conditional
   independence and sufficiency :

     p(x | s(k))  =  prod p(f(m) | s(k))^w(m)
                       m

   where the p(f(m) | s(k)) will be learned and the w(m) supplied empirically.
   We assume that each state s = s(k) has a "signature" which is vector
   of M values --- one for each feature.  Let t(m,k) be the mth signature
   component of the kth state.  We assume that the signature values for
   a given feature correspond, roughly, to the expected order of the actual
   feature values.  We then further assume that

     p(f(m) | s(k)) = p(f(m) | t(m,k))   

   To compute these quantities, the f(m) vectors are quantized using a tree
   structure with integers 0 ... L-1 at its L leaves.  The empirical 
   distributions for th e p(f(m) | t(m,k)) are estimated using the 
   probabilistically labelled training data from the forward backward
   algorithm.    
  
   Inital estimates a taken by creating a quantized statistic which
   is the quanta of the first feature component.  These quanta are 
   intialized to a binomial distribution whose means respect the
   ordering suggested by the feature signatures.

*/



/*#define MAX_QUANTA 16  /* maximum number of quanta for feature */
#define QUANTILES 100
#define MAX_DIM 5
#define COUNT_BUFF_LEN 1000


// table with the values of close harmonics (???)
// harm_table[i][j] where i is the pitch number and j the window (or bin) index


#define MAX_PITCH_INTERVALS 50

#define MIDI_PITCH_N 128 // total # of midi pitches

float *atck_spect;

 
static int harm_table[MIDI_PITCH_N][MAX_PITCH_INTERVALS];



typedef struct tnode {  /* tree for quantizing a feature */
  struct tnode *lc;
  struct tnode *rc;
  int    split;     /* which element to split on */
  float  cutoff;    /* left (right) child if val(split) < (>) cutoff */ //CF:  the split point
/*  int    quant;     /* unique tag for each terminal node */
   float  cond_dist[MAX_SIG];  /* p(node | d) = cond_dist[d] */
  float  count[MAX_SIG];    //CF:  number of training observations that reached this terminal node (not a int because weighted soft sig state assignments)
  float  entropy_cont; /* we seek to minimize H(L|X) = sum p(x) H(L|X=x) //CF:  L is the signiature state
		       where x are the terminal tree nodes and L is the //CF:  X=the observation of this vector quantum  
		       label.  each terminal node of the tree makes a
		       contribution to this total we denote this 
		       contribution by  entropy_cont.  if p(x,l) are
		       the joint probs for a node then this contribution
		       is given by :

		         p(x)log(p(x)) - sum p(x,l)log(p(x,l))    (ENTROPY_FORMULA)   */
//CF:  when we sum the contributions over the quantised features x we get H(L|X), which is to be min'd
//CF:   (ie we max the relative entropy)
//CF:  entropy_cont is the contribution if we were to stop at this node
} TNODE;




typedef struct {
  void (*feat)();      /* the raw vector-valued feature (takes *float) */
  int   dim;             /* dimension of feature vector */
  int    parm;           /* parameter  for above functions */
  float **quantile;      /* empirical quantiles of marg dists  */
  TNODE *tree;           /* the quantizing tree */
  int   num_quanta;      /* number of quanta */
  int   num_sig;         /* number of different signature values for feature */
  int   (*signat)();        /* arg is index of state. ret sig of index */
  float **dist;         /* dist[i][j] is jth (wtd) prob for ith sig value */
  float weight;          /* exponential weighting for feature */
  void  (*tree_z)();    /* the function initializing tree takes feature number */
} FEATURE;


//#define MAX_FEATURES 21 
#define MAX_FEATURES 70  //with 21 has no more space for features     

typedef struct {
  FEATURE el[MAX_FEATURES];
  int num;
  int attribs; /* sum of dimensions for each feature */
} FEAT_STRUCT;


typedef struct {
/*  int start_note;
  int end_note; */
  GNODE *start_graph;
  GNODE *end_graph;
  int firstnote;
  int lastnote;
  char audio_file[300];
  char score_file[300];
} TRAIN_INFO;

#define MAX_TRAIN_FILES 100

typedef struct {
  int num;
  TRAIN_INFO info[MAX_TRAIN_FILES];
} TRAIN_INFO_STRUCT;

TRAIN_INFO_STRUCT tis;


typedef struct {
  float top;
  float bot;
} INTERVAL;

typedef struct {
  INTERVAL interior;
  INTERVAL exterior;
} TEST;


typedef struct {
  float alpha;
  float beta;
  float count;
  float log_partition;  /* log of partition func */
} BETA;

#define CELLS 10
#define MAX_RANGE 10

typedef struct {  /* parms for conditioanal dists */
  int range;
  BETA parm[MAX_RANGE];
  float dist[MAX_RANGE][CELLS];
  float expon;  /* an exponent for weighting.  expon = 1 if equal to prior in weight */
} ATRIB_COND;






/*#define OCT_TESTS 2
#define HARM_TESTS 10
#define TESTNUM (OCT_TESTS+HARM_TESTS)     
#define ALL  2
#define SOME 1
#define NONE 0 */

#define VERSIONS 4  /* each pitch has this many associated states. 
		       eg  attack,fall,sustain,rise (trill_half, trill_whole) */


#define ENERGY_ATRIB 0
#define BURST_ATRIB 1
#define TESIT_ATRIB 2
#define ENV_DIFF_1_ATRIB 3
/*#define ENV_DIFF_2_ATRIB 4*/
#define FIRST_HARM_ATRIB 4
#define HARM_ATRIBS  36 /*24*/ /* 12*/
#define MAX_ATTRIBS 50  /*this change 6-11*/ //150 // 100 //50



#define TESIT_QUANTA 5







TEST test[HARM_ATRIBS];
int **signature,atrib_num,state_num,num_pitches;
float low_freq;
float low_omega,high_omega; 
ATRIB_COND dm[MAX_ATTRIBS],train_count[MAX_ATTRIBS];

int root[MAX_DIM+1];  /* root[i] is the ith root of COUNT_BUFF_LEN */

int quant_stat[MAX_FEATURES];
static float *dist_ptr[MAX_FEATURES];
static TNODE *tree_quant[MAX_FEATURES];
static float ***count;  //CF:  
static float ***marg;
static int total_training_frames;
static float background[FREQDIM/2];    // it appears the trained more from disk is still used --- this might be bad
static float ambient[FREQDIM/2];

FEAT_STRUCT feature;


#include "feature.c"



#define ROOM_RESPONSE_FILE "response.dat"
#define AMBIENT_NOISE_FILE "ambient.dat"


float*
get_background_spect() {
  return(background);
}


void
read_room_response() {
  int i,j;
  FILE *fp;
  char name[200];

  strcpy(name,MISC_DIR);
  strcat(name,"/");
  strcat(name,ROOM_RESPONSE_FILE);
  fp = fopen(name,"r");
  if (fp == NULL) {
    for (i=0; i < freqs; i++) room_response[i] = 1;
    return;
  }
  for (i=0; i < freqs; i++) fscanf(fp,"%d %f\n",&j,room_response + i);
  fclose(fp);
  //  for (i=0; i < freqs; i++) printf("%d %f\n",i,room_response[i]);
}


/*void
read_ambient() {
  int i,j;
  FILE *fp;
  char name[200];

  strcpy(name,MISC_DIR);
  strcat(name,"/");
  strcat(name,AMBIENT_NOISE_FILE);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't find %s setting ambient to constant\n",name);
    for (i=0; i < freqs; i++) ambient[i] = 1;
    return;
  }
  for (i=0; i < freqs; i++) fscanf(fp,"%d %f\n",&j,ambient + i);
  fclose(fp);
  return;
}
 */

void
analyze_room_response(int lo, int hi) {
  int i,j;
  float received[FREQDIM],sent[FREQDIM];
  FILE *fp;
  char name[200];

  for (i=0; i < freqs; i++) sent[i] = received[i]=0;


  downsample_audio_to_mono(orchdata, orchdata, 
			   hi*SKIPLEN*BYTES_PER_SAMPLE*IO_FACTOR, IO_FACTOR);


  for (token=lo; token < hi; token++) {
    samples2data();
    create_spect(data , spect);
    create_spect(orch_data_frame , orchestra_spect);
    for (i=0; i < freqs; i++) {
      received[i] += spect[i];
      sent[i] += orchestra_spect[i];
    }
  }
  printf("not writing here anymore\n"); exit(0);
  strcpy(name,MISC_DIR);
  strcat(name,"/");
  strcat(name,ROOM_RESPONSE_FILE);
  fp = fopen(name,"w");
  for (i=0; i < freqs; i++) fprintf(fp,"%d\t%f\n",i,received[i]/sent[i]);
  fclose(fp);
  //  for (i=0; i < freqs; i++) printf("%d\t%f\t%f\t%f\n",i,sent[i],received[i],received[i]/sent[i]);
}



void
analyze_background(int lo, int hi) {
  int i,j;
  float received[FREQDIM];
  FILE *fp;
  char name[200];

  for (i=0; i < freqs; i++) received[i]=0;
  for (token=lo; token < hi; token++) {
    samples2data();
    create_spect(data , spect);
    for (i=0; i < freqs; i++) received[i] += spect[i];
  }
  strcpy(name,MISC_DIR);
  strcat(name,"/");
  strcat(name,AMBIENT_NOISE_FILE);
  fp = fopen(name,"w");
  for (i=0; i < freqs; i++) fprintf(fp,"%d\t%f\n",i,received[i]/(hi-lo));
  fclose(fp);
}




#define LOW_NOISE_FREQS 40  // should be hz instead
#define LOW_NOISE_PROP  .5

static void
init_background_model() {
  int i;
  float p1,p2;

  p1 = LOW_NOISE_PROP/LOW_NOISE_FREQS;
  p2 = (1-LOW_NOISE_PROP)/ (float) freqs;
  for (i=0; i < freqs; i++) 
    background[i] = (i < LOW_NOISE_FREQS) ? p1+p2 : p2;
    //    background[i] = 1/(float)freqs;
  /*  for (i=0; i < freqs; i++) printf("%d %f\n",i,background[i]);
      exit(0);*/

}

void 
read_background_model(char *s) {
  FILE *fp;
  int i;

  fp = fopen(s,"r");
  if (fp == NULL) { printf("couldn't open %s (using preset)\n",s); return; }
  for (i=0; i < freqs; i++) fscanf(fp,"%f",background+i);
  //  for (i=0; i < freqs; i++)   printf("%d %f\n",i,background[i]);
  //  exit(0);

  fclose(fp);
  //  for (i=0; i < freqs; i++) printf("%f\n",background[i]);
}

static int set_roots() {
  int i;

  for (i=1; i < MAX_DIM+1; i++)
    root[i] = pow((float) COUNT_BUFF_LEN , 1./ (float) i);
}


static float val2quant(x,q)
float x;
float *q; {
  int hi,lo,mid,ret;
  float r,pp,qq; 
  
  if (x <= q[0]) return(0.);
  if (x >= q[QUANTILES-1]) return(1.);
  hi = QUANTILES;   /* binary search  */
  lo = 0;
  while (hi - lo > 1) {
    mid = hi+lo;
    mid >>= 1;
    if (x <= q[mid]) hi = mid;
    if (x >= q[mid]) lo = mid;  /* both can happen (exit while) */
  }
  pp = (q[hi] == q[lo]) ? 1 :  (x-q[lo])/(q[hi]-q[lo]);  
  qq = 1-pp;
  r = lo*qq + hi*pp;  /* interpolate */
  return(r/(QUANTILES-1));
}

static float quant2val(f,q)
float f,*q; {
  float r,pp,qq;
  int lo,hi;

  if (f > 1. || f < 0.) printf("problem in quant2val() %f\n",f);
  if (f == 1.) return(q[QUANTILES-1]);
  r = f*(QUANTILES-1);
  lo = r;
  hi = lo+1;
  pp = r-lo;
  qq = 1-pp;
  return(qq*q[lo] + pp*q[hi]);
}
  


  
//CF:  This is for one component of a n-dim feature.
//CF:  x is the value of the component. (no particular range yet)
//CF:  but we know the quantiles q of x.
//CF:  We rescale x to lie between 0 and b-1.
//CF:  eg. b could be a number of bins to quantise the function.
static float val2bin(x,b,q) /* if the QUANTILES el array q had b bins which
			     would contain x */
float x;
int b;
float *q; {
  float f;
  int d;

  f = val2quant(x,q);
  d = f*(b-1);
  return(d);
}

static float bin2val(n,b,q)
int b;
float n,*q; {
  float f,x;

  f = ((float)n) /(b-1);
  x = quant2val(f,q);
  return(x);
}
  

static int bin_num(x,b,q) /* if the QUANTILES el array q had b bins which
			     would contain x */
float x;
int b;
float *q; {
  int hi,lo,mid,ret;
  float r,pp,qq; 
  
  if (x <= q[0]) return(0);
  if (x >= q[QUANTILES-1]) return(b-1);
  hi = QUANTILES;   /* binary search  */
  lo = 0;
  while (hi - lo > 1) {
    mid = hi+lo;
    mid >>= 1;
    if (x <= q[mid]) hi = mid;
    if (x >= q[mid]) lo = mid;  /* both can happen (exit while) */
  }
  pp = (q[hi] == q[lo]) ? 1 :  (x-q[lo])/(q[hi]-q[lo]);  
  qq = 1-pp;
  r = lo*qq + hi*pp;  /* interpolate */
  ret = ((r*b)/QUANTILES)+.5;
  return(ret);
}
     
static float bin_num_inv(n,b,q) 
int n,b;
float *q; {
  int lo,hi;
  float x,pp,qq;
  
  x = (QUANTILES-1)*((float) n)/b;
  lo = x;
  hi = lo+1;
  if (hi >= QUANTILES) printf("problem in bin_num_inv()\n");
  pp = x-lo;
  qq = 1-pp;
  return(qq*q[lo] + pp*q[hi]);
}
     
static int comp_index(c,dim) /* c is array of dim indices. compute indx */
int *c,dim; {
  int i,a;

  a = c[dim-1];
  for (i=dim-2; i >= 0; i--) {
    a *= root[dim];
    a += c[i];
  }
  return(a);
}
    
    
static void comp_index_inv(c,dim,a)  /* inverse of above */
int *c,dim,a; {
  int i;

  for (i=0; i < dim; i++) {
    c[i] = a % root[dim];
    a /= root[dim];
  }
}


int
state2index(int st,int p, FULL_STATE fs)  {
  /* reatric_state and attack_state treated the same */

  int r,x,m;

  if (st == REST_STATE) return(0);
  r = p-LOWEST_NOTE;
  switch(st) {
  case NOTE_STATE        : { m = 0;  break; }
  case ATTACK_STATE      : { m = 1;  break; }
  case SLUR_STATE        : { m = 1;  break; }
  case REARTIC_STATE     : { m = 1;  break; }
  case FALL_STATE        : { m = 2;  break; }
  case RISE_STATE        : { m = 3;  break; }
  case TRILL_HALF_STATE  : { m = 4;  break; }  /* not used now */
  case TRILL_WHOLE_STATE : { m = 5;  break; } /* not used now */
  default            : { printf("unrecognized state: %d p = %d\n",st,p); exit(0); }
  }
  x = 1 + r + m*num_pitches;
  if (x >= state_num || x < 0)
    printf("problem in state2index() st = %d p = %d num_pitches = %d\n",st,p,num_pitches); 
  return(x); 
}


int
index2state(int i, int *st, int *p, FULL_STATE *fs) {
  int type;

  if (i == 0) { *st = REST_STATE; return(0); }
  i--;
  *p = LOWEST_NOTE + i%num_pitches;
  type  = i/num_pitches;
  switch (type) {
    case 0 : { *st = NOTE_STATE; break; }
    case 1 : { *st = ATTACK_STATE; break; }
    case 2 : { *st = FALL_STATE; break; }
    case 3 : { *st = RISE_STATE; break; }
    case 4 : { *st = TRILL_HALF_STATE; break; }  /* not used now */
    case 5 : { *st = TRILL_WHOLE_STATE; break; }/* not used now */
    default: { printf("unrecognized state\n"); exit(0); }  
  }
}


float binom(x,n,p) 
int x,n;
float p; {
  float tot,gammln(); 

  tot =  x*log(p) + (n-x)*log(1-p);
  tot += (gammln((float) n+1) - (gammln((float) x+1) + gammln((float) n+1-x)));
  return(exp(tot));
}



    



#define INIT_MIN .5

init_dists() {
  int j,f,s,sigs,quants;
  float **matrix(),p,mu,sum;
  FEATURE *feat;

  for (f = 0; f < feature.num; f++) {
    feat = feature.el+f;
    sigs = feat->num_sig;
    quants = feat->num_quanta;
    feat->dist = matrix(0,sigs-1,0,quants-1);
    for (s=0; s < sigs; s++) {
      mu = (2*s+1)/ (float) (2*sigs);
      p = mu;
      sum = 0;
      for (j=0; j < quants; j++) 
	sum+= feat->dist[s][j] = binom(j,quants-1,p)+.1;
      for (j=0; j < quants; j++) feat->dist[s][j] /= (sum/quants);
    }
  }
}



#define PITCH_WEIGHT  4. 
 
init_class() {
  int **imatrix(),*ivector(),a,i,j;
  float *vector(),**matrix(),mm,aa,bb,p,mu;

  set_roots();
  init_features();
  make_log_tab();
  num_pitches = 1+HIGHEST_NOTE-LOWEST_NOTE;  

  atrib_num = FIRST_HARM_ATRIB + HARM_ATRIBS;
  if (atrib_num > MAX_ATTRIBS) {
    printf("too many attributes\n");
    exit(0);
  }
/*  dm = (ATRIB_COND *) malloc(atrib_num*sizeof(ATRIB_COND));*/
  state_num = 1 + VERSIONS*(num_pitches);  
    /* a note with or without attack and rest */
  
  //  signature = imatrix(0,state_num-1,0,feature.num-1);
  if (feature.num) signature = imatrix(0,state_num-1,0,feature.num-1);
  dm[ENERGY_ATRIB].range = 2;
  dm[BURST_ATRIB].range = 2;
  dm[TESIT_ATRIB].range = TESIT_QUANTA+1;
  dm[ENV_DIFF_1_ATRIB].range = 3;
  for (a = FIRST_HARM_ATRIB; a < atrib_num; a++) dm[a].range = 5/*4*/;
  for (a = 0; a < atrib_num; a++) dm[a].expon = 1;
  for (a = FIRST_HARM_ATRIB; a < atrib_num; a++) 
    dm[a].expon = PITCH_WEIGHT / (float) HARM_ATRIBS;
/*  for (a = 0; a < atrib_num; a++ {
    dm[a].parm = (BETA *) malloc(sizeof(BETA)*dm[a].range);
    dm[a].dist = matrix(0,dm[a].range-1,0,CELLS-1);
  } */
  for (a = 0; a < atrib_num; a++)  train_count[a].range = dm[a].range;
  low_freq = sol[LOWEST_NOTE].omega;
  low_omega= sol[LOWEST_NOTE].omega;
  high_omega= sol[HIGHEST_NOTE].omega;
  //make_signatures();
  
  // generates the harm_table;
  
  // initialization of the table

  for (i=0; i<MIDI_PITCH_N; i++) {
    for (j=0; j<MAX_PITCH_INTERVALS; j++)
      harm_table[i][j] = 0;
  }

  get_harm_table();
    init_background_model();
  

}

make_tests() {
  int t,i;
  float pitch_band(),band,w,qstep;

  qstep = pow(2. , 1./ (12*2));
  for (t=0; t < HARM_ATRIBS; t++) {
    i = LOWEST_NOTE + 2*t;
    test[t].interior.bot = sol[i].omega/qstep;
    test[t].interior.top = test[t].exterior.bot =  sol[i+1].omega/qstep;
    test[t].exterior.top = sol[i+2].omega/qstep;
;
  }
}


#define OCT_THRESH .025

int oct_class() {
 
  float low,high,power=0,rat,integ(),interior=0,qs;
  int i,n;

  qs = HALF_STEP/2;
  for (i=0; i < freqs; i++)  power += spect[i];
  for (i=0; i < OCTAVES; i++) {
    low =  sol[12*i].omega/qs;
    high = sol[12*(i+1)].omega/qs;
    interior += integ(low,high);
    rat = interior/power;
/*printf("%d %f\n",i,rat); */
    if (rat > OCT_THRESH) return(i);
  }
  return(0);
} 





int pitch_class() {
 
  float rat,interior = 0, exterior = 0,w,p,band,pitch_band(),best_rat = 0,integ();
  int i,n,best_n,harm;
 

  for (n=0; n < 12; n++) {
    harm = interior = 0;
    w = sol[n+LOWEST_NOTE].omega;
    for ( p=w; p < freqs;  p += w)  {
      band = pitch_band(p);
      interior += integ( p-band,p+band ); 
      harm++;
    }
    rat = interior/harm;
    if (rat > best_rat) { best_n = n; best_rat = rat; }
  }
  return(best_n);
} 






#define CONSIDERED_OCTAVES 3.5   /* this many octaves above low note is
                                  range of tests */
#define FAIL 0
#define AMBIG 1
#define PASS 2








#define MARGIN .1 
#define NOTE_BAND .45  

state_sig(st,note,t)
/* signature of (st,note) under t-th atribute  */
int st,note,t; {
  float q0,q1,q3,q4,pitch_band(),band,w,range,p,hstep(),lo,hi;
  int inter,exter;

  if (st == REST_STATE) switch(t) {
    case ENERGY_ATRIB : return(0);  
    case  BURST_ATRIB : return(0);  
    case  TESIT_ATRIB : return(0);  
    case  ENV_DIFF_1_ATRIB  : return(1);
    default           : return(1/*2*/);
  } 
  if (t == ENERGY_ATRIB) return(1);
  if (t == ENV_DIFF_1_ATRIB) 
    return( (st == FALL_STATE) ? 0 : (st == RISE_STATE) ? 2 : 1);  
  if (t == BURST_ATRIB) 
    return( (st == NOTE_STATE || st == RISE_STATE || st == FALL_STATE) ? 0 : 1);  

  if (t == TESIT_ATRIB) {
    p = TESIT_QUANTA;
    p *= (note-LOWEST_NOTE);
    p /= num_pitches;
    p += 1;    /* to separate from rest */
    return( (int) p);
  }
  t -= FIRST_HARM_ATRIB;
  inter = exter = 0;
  w = sol[note].omega;
  for ( p=w; p < freqs;  p += w)  {
    band = hstep(p,.24 /*.5*/) - p;  /* not used */
    lo = hstep(p,-NOTE_BAND);
    hi = hstep(p, NOTE_BAND);
    if (hi > test[t].interior.bot && lo < test[t].interior.top) 
      inter = 1;
    if (hi > test[t].exterior.bot && lo < test[t].exterior.top) 
      exter = 1;
  }
  if (inter && exter) return(2/*1*/);
  if (inter && !exter) return(/*3*/4);  
  if (!inter && exter) return(0);  
  if (!inter && !exter) return(/*2*/3);  
}



comp_sig(p1,p2)
int **p1,**p2; {
  int t;

  for (t=0; t < atrib_num; t++) {
    if ((*p1)[t] > (*p2)[t]) return(1);
    else if ((*p1)[t] < (*p2)[t]) return(-1);
  }
  return(0);
}

/*make_signatures() {
  int n,t,st,note,nn,p;
  char s[10];
  int   (*sig)();   
  FULL_STATE fs;

  for (n=0; n < state_num; n++) {
    index2state(n,&st,&note,&fs);
    nn = state2index(st,note,fs);
    if (nn != n) {
      printf("look at index2state()\n");
    }
    for (t=0; t < feature.num; t++)  {
      p = feature.el[t].parm;
      sig = feature.el[t].sig;
	  signature[n][t] = sig(n,p);
	}
  }
  count_signatures();
  }*/


#define ALL_THRESH .9
#define NONE_THRESH .1


/*class() {
  int temp[TESTNUM],t,n,min_dist,best_n,dist;
  float inter,exter,s,integ();

  for (t=0; t < TESTNUM; t++) {
     inter = integ(test[t].interior.bot,test[t].interior.top);
     exter = integ(test[t].exterior.bot,test[t].exterior.top);
     s = inter/(inter+exter);
     if (s > ALL_THRESH) temp[t] = ALL;
     else if (s < NONE_THRESH) temp[t]=NONE;
     else temp[t] = SOME;
  }
  min_dist = 1000;
  for (n=LOWEST_NOTE; n <= HIGHEST_NOTE; n++) {
    dist = 0;
    for (t=0; t < TESTNUM; t++) dist += abs(signature[n][t]-temp[t]);
    if (dist < min_dist) { min_dist = dist; best_n = n; }
  }
  return(best_n);
}*/



#define FUND_QUANTILE .025

float tesit_stat()  /* estimate of location of fundamental.  units are 
logarithmic so lowest pitch = 0 and hightest pitch = 1 */
{   
  float power = 0,cdf=0,cdf2=0,cut,quantile,ret;
  int l,h,i;


  l =  sol[LOWEST_NOTE-1].omega;
  h =  sol[HIGHEST_NOTE+1].omega;
  for (i=l; i <= h; i++)  power += spect[i];
  if (power <= 0) return(0.);
  cut = FUND_QUANTILE*power;
  for (i=l; i <= h; i++)  if ((cdf += spect[i]) > cut) break;
  cdf2 = cdf - spect[i];
  quantile = (i+1) - (cut-cdf2)/(cdf-cdf2);
  ret = log(quantile/l) / log((float) h / (float) l);

if (ret < 0)
printf("xx\n");
  return(ret);
}






/*float energy_stat() {
  float power=0;
  int   i;

  for (i=1; i < TOKENLEN; i++) 
    power += (data[i]-data[i-1])* (data[i]-data[i-1]);
  return(power);
}*/

float energy_stat() {
  float power=0,x;
  int   i;

  for (i=0; i < FRAMELEN; i++) {
   x = 3*(i-FRAMELEN/2) / (float) FRAMELEN;
   power += data[i]*data[i]; /**exp(-x*x);*/
  }



  for (i=0; i < TOKENLEN; i++) {
   x = 3*(i-TOKENLEN/2) / (float) TOKENLEN;  
   power += data[i]*data[i]; /**exp(-x*x);*/
  }
  return(power);
}


/*float energy_stat() 
 /* energy in sub note range / all energy */
/*{   
  float power = 0,bgd = 0,t;
  int l,h,i;

  h =  sol[LOWEST_NOTE-1].omega;
  for (i=0; i < freqs; i++)  power += spect[i];
  for (i=0; i <= h; i++)  bgd += spect[i];
  t = 1-bgd/power;
  return(t);
}

*/


#define MEM_LEN   5 /* 9 /* 5  /* probably need > 5 value */

float memory[MEM_LEN];
float envelope[MEM_LEN];


float diff_memory[MAX_FRAMES];


float envel_diff_1() {
  float diff=0,w,s=0;
  int i;

  if (mode == BACKWARD_MODE) diff = diff_memory[token];
  else {
/*      diff = envelope[1]-envelope[0]; */
/*      for (i=0; i < 1; i++) diff += memory[i];
      for (i=MEM_LEN-1; i < MEM_LEN; i++) diff -= memory[i];
      diff /= 2; */
      w = (((float) MEM_LEN)-1)/2;   /* fit line to data and take slope */
      for (i=0; i < MEM_LEN; i++) {
	diff += memory[i]*w; 
	s += fabs(w);
	w -= 1;
      }
      diff /= s;

      diff_memory[token] = diff;
  }
  return(diff);
}
  
float envel_diff_2() {

  return(envelope[0]+envelope[4]-2*envelope[2]);
}
  




#define SUBDIV 2
#define DERV 3
float lobo[SUBDIV+DERV-1];
float wiki[DERV] = {1 , -2 , 1};


float burst_memory[MAX_FRAMES];


float burst_stat() {
  int i,len,j;
  float r,x,w,a[SUBDIV],sum,d,mx;

  if (mode == BACKWARD_MODE) r = burst_memory[token];
  else {
    for (i=0; i < DERV-1; i++) lobo[i+SUBDIV] = lobo[i];
    for (j=0; j < SUBDIV; j++) sum = a[j] = lobo[j] = 0;
    len =  TOKENLEN/SUBDIV;
    for (i=0; i < len; i++) {
      /*    x = 3* (i-len/2) / (float) len;
	    w = exp(-x*x); */
      for (j=0; j < SUBDIV; j++) lobo[j] += data[j*len+i]*data[j*len+i]/**w*/;
    }
    for (i=0; i < SUBDIV; i++) for (j=0; j < DERV; j++)
      a[i] += wiki[j]*lobo[i+j];
    mx = -HUGE_VAL;
    for (i=0; i < SUBDIV+DERV; i++) sum += lobo[i];
    for (i=0; i < SUBDIV; i++) if (fabs(a[i]) > mx) mx = fabs(a[i]);
    r = mx/sum/*pow(sum,1.)*/;
    burst_memory[token] = r;
  }
  return(r);
}




float testy_stat() {}


fcomp(const void *n1, const void *n2) {
  float *p1,*p2; 

  p1 = (float *) n1;
  p2 = (float *) n2;
  if (*p1 < *p2 ) return(-1);
  if (*p1 > *p2 ) return(1);
  return(0);
}
      


#define LARGE_VAL 2.


#define MAX_CDF_FRAMES 5000

static void read_audio_train(char *af)
{
  read_audio(af);
  strcpy(audio_file,af);
  read_parse_accomp();
#ifdef ORCHESTRA_EXPERIMENT 
  read_orchestra_audio();
#endif
}


float quant[MAX_ATTRIBS][QUANTILES]; 

train_cdfs() {   /* cumulative dist functions (stat is F(X) ) */
  char file[500],ans[10];
  int t,s,i,n,a,j,tot,f,p;
  float prob,*x,**matrix();
  float dat[MAX_ATTRIBS][MAX_CDF_FRAMES],temp[MAX_DIM];
  FILE *fp;
  FEATURE *struc;


  tot=0;
  for (j=0; j < tis.num; j++) {
    printf("atrib_num = %d tot = %d\n",atrib_num,tot);
    read_audio_train(tis.info[j].audio_file);
    for (token=0; token < frames && tot < MAX_CDF_FRAMES; token++) { 
      samples2data();
      setspect();
/*      calc_statistics(); */
      
      n = 0;
      for (f=0; f < feature.num; f++) {
	struc = feature.el + f;
	p = struc->parm;
	(struc->feat)(temp,p);
	for (a=0; a < struc->dim; a++)  dat[n++][tot] = temp[a];
      }
      tot++;
    }
    if (tot == MAX_CDF_FRAMES) {
      printf("ignoring subsequent data\n");
      break;
    }
  }
  printf("total frames = %d\n",tot);

  n=0;
  for (f=0; f < feature.num; f++) {
    struc = feature.el + f;
    struc->quantile = matrix(0,struc->dim-1,0,QUANTILES);
    for (a=0; a < struc->dim; a++)  {
      qsort(dat[n],tot,sizeof(float),fcomp);
      for (i=0; i < QUANTILES; i++) {
	(struc->quantile)[a][i] = dat[n][i*tot/QUANTILES];
/*	printf("%f\n",(struc->quantile)[a][i]);*/
      }
/*      printf("\n"); */
      n++;
    }
  }
      

  strcpy(file,TRAIN_DIR);
  strcat(file,QUANTFILE);
  fp = fopen(file,"w");
  if (fp == NULL) {
	printf("can't open %s\n",file);
	exit(0);
  }
  n=0;
  for (f=0; f < feature.num; f++) {
    struc = feature.el + f;
    for (a=0; a < struc->dim; a++)  {
      for (i=0; i < QUANTILES; i++) fprintf(fp,"%f\n",(struc->quantile)[a][i]);
      fprintf(fp,"\n");
      n++;
    }
  }
  fclose(fp); 

  /*

  fp = fopen(file,"w");
  if (fp == NULL) {
	printf("can't open %s\n",file);
	exit(0);
  }
  for (a=0; a < atrib_num; a++) {
    qsort(dat[a],tot,sizeof(float),fcomp);
    for (i=0; i < QUANTILES; i++) {
      quant[a][i] = dat[a][i*tot/QUANTILES];
      fprintf(fp,"%f\n",quant[a][i]);
    }
    fprintf(fp,"\n");
   }
   fclose(fp); */
}   
    

train_cdfs_file(char *quant_file) {   /* cumulative dist functions (stat is F(X) ) */
  char file[500],ans[10];
  int t,s,i,n,a,j,tot,f,p;
  float prob,*x,**matrix();
  float dat[MAX_ATTRIBS][MAX_CDF_FRAMES],temp[MAX_DIM];
  FILE *fp;
  FEATURE *struc;

  tot=0;
  for (j=0; j < tis.num; j++) {
    printf("atrib_num = %d tot = %d\n",atrib_num,tot);
    read_audio_train(tis.info[j].audio_file);
    for (token=0; token < frames && tot < MAX_CDF_FRAMES; token++) { 
      samples2data();
      setspect();
/*      calc_statistics(); */
      
      n = 0;
      for (f=0; f < feature.num; f++) {
	struc = feature.el + f;
	p = struc->parm;
	(struc->feat)(temp,p);
	for (a=0; a < struc->dim; a++)  dat[n++][tot] = temp[a];
      }
      tot++;
    }
    if (tot == MAX_CDF_FRAMES) {
      printf("ignoring subsequent data\n");
      break;
    }
  }
  printf("total frames = %d\n",tot);

  n=0;
  for (f=0; f < feature.num; f++) {
    struc = feature.el + f;
    struc->quantile = matrix(0,struc->dim-1,0,QUANTILES);
    for (a=0; a < struc->dim; a++)  {
      qsort(dat[n],tot,sizeof(float),fcomp);
      for (i=0; i < QUANTILES; i++) {
	(struc->quantile)[a][i] = dat[n][i*tot/QUANTILES];
/*	printf("%f\n",(struc->quantile)[a][i]);*/
      }
/*      printf("\n"); */
      n++;
    }
  }


  fp = fopen(quant_file,"w");
  if (fp == NULL) {
	printf("can't open %s\n",file);
	exit(0);
  }
  n=0;
  for (f=0; f < feature.num; f++) {
    struc = feature.el + f;
    for (a=0; a < struc->dim; a++)  {
      for (i=0; i < QUANTILES; i++) fprintf(fp,"%f\n",(struc->quantile)[a][i]);
      fprintf(fp,"\n");
      n++;
    }
  }
  fclose(fp); 
      

  /*  fp = fopen(quant_file,"w");
  if (fp == NULL) {
	printf("can't open %s\n",file);
	exit(0);
  }
  for (a=0; a < atrib_num; a++) {
    qsort(dat[a],tot,sizeof(float),fcomp);
    for (i=0; i < QUANTILES; i++) {
      quant[a][i] = dat[a][i*tot/QUANTILES];
      fprintf(fp,"%f\n",quant[a][i]);
    }
    fprintf(fp,"\n");
  }
  fclose(fp); */
}   
    



void
read_cdfs() {
  FILE *fp;
  int a,i,n,f;
  char file[500];
  FEATURE *struc;
  float **matrix();

  strcpy(file,TRAIN_DIR);
  strcat(file,QUANTFILE);
  fp = fopen(file,"r");
  if (fp == NULL) { 
    printf("could not find cdf file: %s\n",file);
    return;
  }

  n=0;
  for (f=0; f < feature.num; f++) {
    struc = feature.el + f;
    struc->quantile = matrix(0,struc->dim-1,0,QUANTILES);
    for (a=0; a < struc->dim; a++)  {
      for (i=0; i < QUANTILES; i++) fscanf(fp,"%f",&struc->quantile[a][i]);
      n++;
    }
  }
  fclose(fp); 





  /*  for (a=0; a < atrib_num; a++) {
    for (i=0; i < QUANTILES; i++) fscanf(fp,"%f\n",quant[a]+i);
    fscanf(fp,"\n");
  }
  fclose(fp); */
}

int
read_cdfs_file(char *quant_file) {
  FILE *fp;
  int a,i,n,f;
  char file[500];
  FEATURE *struc;
  float **matrix();

  fp = fopen(quant_file,"r");
  if (fp == NULL) { 
    printf("could not find cdf file: %s\n",quant_file);
    return(0);
  }
  n=0;
  for (f=0; f < feature.num; f++) {
    struc = feature.el + f;
    struc->quantile = matrix(0,struc->dim-1,0,QUANTILES);
    for (a=0; a < struc->dim; a++)  {
      for (i=0; i < QUANTILES; i++) fscanf(fp,"%f",&struc->quantile[a][i]);
      n++;
    }
  }
  fclose(fp); 
  return(1);



  /*  fp = fopen(quant_file,"r");
  if (fp == NULL) { 
    printf("could not find cdf file: %s\n",file);
    return;
  }
  for (a=0; a < atrib_num; a++) {
    for (i=0; i < QUANTILES; i++) fscanf(fp,"%f\n",quant[a]+i);
    fscanf(fp,"\n");
  }
  fclose(fp);*/
}


static TNODE *tree_quantize(t,v)
TNODE *t;
float *v; {
  while (t->split != -1) {
    if (t->cutoff > v[t->split]) t = t->lc;
    else t = t->rc;
  }
  return(t);
}


calc_statistics() {
  int i,p,r,f;
  float val[MAX_DIM];
  FEATURE *feat;
  void (*func)();
  

  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    p = feat->parm;
    func = feat->feat;
    func(val,p);
/*     dist_ptr[f] = tree_quantize(feat->tree,val); */
    tree_quant[f] = tree_quantize(feat->tree,val);
    // if (f==1) printf("token = %d probs = %f %f %f\n",token,tree_quant[f]->cond_dist[0],tree_quant[f]->cond_dist[1],tree_quant[f]->cond_dist[2]);
  }
} 




#define LOG_TAB_LEN 1000

float logtab[LOG_TAB_LEN];

make_log_tab() {
  int i;

  for (i=0; i < LOG_TAB_LEN; i++) logtab[i] = 
    log(((float) i+1) / LOG_TAB_LEN );
}

float tab_log(x)
float x; {
  float p,q,ii;
  int i;

  if ((x > 1) ||  (x <= 0)) { printf("bad arg tp tab_log()\n" ); exit(0); }
  ii = x * LOG_TAB_LEN;
  i = ii;
  p = ii - i;
  q = 1-p;
  return(q*logtab[i-1]+p*logtab[i]);
}


set_part(beta)
BETA *beta; {
  float gammln();

  beta->log_partition = 
  gammln(beta->alpha) + gammln(beta->beta) - gammln(beta->alpha+beta->beta);  
}

int flag = 0;

set_flag(i)
  int i; {
  flag = i;
}




float class_like(s)  /* assumes quantized stats are computed */
int s; {
  int f,q,sig;
  float prob=1,*dist;
  FEATURE *feat;
  TNODE *t;

  for (f=0; f < feature.num; f++) {          
    feat = feature.el+f;             
/*    q = quant_stat[f];*/
/*     dist = dist_ptr[f];*/
    t = tree_quant[f];
    sig = signature[s][f];
    prob *= t->cond_dist[sig];
  }
  return(prob);  
}

#define MIN_SPECT_VAL 0. //.000001


//#define freq_cutoff 0 // 20 // 5   /* this used to be LO_FREQ_CUTOFF */

#define ATTACK_SCALING_FACT   2. //1.  /* .7 */

float
working_attack_spect_like(float *model) {  /* stored model (not log !!) */
  int i,t;
  float null_const,norm,total,data[FREQDIM/2],scale=0;  /* spectrogram data */
  extern float diff_spect[];

  null_const = log(1/(float)freqs);  // for the null=uniform model
  /*  norm = 0;
  for (i=freq_cutoff; i < freqs; i++) {
    data[i] = spect[i]-last_spect[i];
    if (data[i] < 0) data[i] = 0;
    norm += data[i];
    }*/
  //  if (norm > 0) for (i=freq_cutoff; i < freqs; i++) data[i] /= norm;  /* normalizing for loudness */
  total = 0;
  for (i=freq_cutoff; i < freqs; i++)  {
    //    printf("model[%d] = %f\n",i,model[i]);
    //    if (model[i] <= 0 || data[i] <= 0) { printf("%d %f %f\n",i,model[i],data[i]); exit(0); }
    //    if (model[i] <= 0) { printf("1 log(0) is bad %f\n",model[i]); exit(0); }
    //    if (data[i] > 0.) total += data[i]*(log(model[i])-null_const);
    total += diff_spect[i]*(log(model[i])-null_const);
    scale += diff_spect[i];
    //      printf("%d %f %f\n",i,data[i],model[i]);
  }
  //  printf("total = %f spect[10] = %f model[10] = %f\n",total,spect[10],model[10]);
  if (scale > 0) total /= scale;
  return(exp(ATTACK_SCALING_FACT*total));
}

static 
float logdgamma(float x, float shape, float scale) {
  float gammln();
  
  // 1/a^b Gamma(b)  x^{b-1} e^-x/a

  /* mean = ab
     var = ab^2 */

  if (x < 0) { printf("bad input to logdgamma()\n"); exit(0); }
  if (x == 0) return(0.);  // prob not right
  return(-shape*log(scale) - gammln(shape) +  (scale-1)*log(x) - x/scale);
}

static float
attack_spect_like(float *model) {  /* stored model (not log !!) */
  int i,t;
  float null_const,norm,total=0,data[FREQDIM/2];  /* spectrogram data */
  extern float diff_spect[];
  float lp_null=0,lp_model=0;

  //  lp_model = logdgamma(total, 3.,3.);
  //  lp_null = logdgamma(total, 1.,1.);
  null_const = log(1/(float)freqs);  // for the null=uniform model
  //  printf("anncx\n");
  for (i=freq_cutoff; i < freqs; i++)  {
    total += diff_spect[i];
    lp_model += diff_spect[i]*log(model[i]);
    lp_null += diff_spect[i]*null_const;
  }
  if (total > 0.) { lp_model /= total; lp_null /= total; }
  //  printf("%f %f\n",lp_model,lp_null);
  return(exp( 2. *(lp_model-lp_null)) );
}



#define SOLO_PROP .65  // fix this
#define ORCH_PROP .30 //.35


void
meld_solo_orch_back(float *s1, float *s2, float *s3, float *sout) {
  int i;
  float p1,p2,p3,t=0;


  //   for (i=0; i < freqs; i++) printf("%d %f\n",i,s3[i]); exit(0);

  p1 = SOLO_PROP;
  p2 = ORCH_PROP;
  p3 = (1-(p1+p2));
  for (i=0; i < freqs; i++) t += sout[i] = p1*s1[i] + p2*s2[i] + p3*s3[i];
  for (i=0; i < freqs; i++) sout[i] /= t;
  /*  for (i=0; i < freqs; i++) if (sout[i] <= 0) {
    for (i=0; i < freqs; i++) printf("%f %f %f\n",s1[i],s2[i],s3[i]);
    exit(0);
    }*/
}


void
meld_solo_orch_back_atck(float *s1, float *s2, float *s3, float *s4, float *sout) {
  int i;
  float p1,p2,p3,p4,t=0;


  //   for (i=0; i < freqs; i++) printf("%d %f\n",i,s3[i]); exit(0);

  p1 = .4;  // solo
  p2 = .1;  // sustain piano note
  p3 = .1;  // background
  p4 = .4;  // attacking piano notes
  for (i=0; i < freqs; i++) t += sout[i] = p1*s1[i] + p2*s2[i] + p3*s3[i] + p4*s4[i];
  for (i=0; i < freqs; i++) sout[i] /= t;
}


#define PROB_UNSET -1. 

static float attack_spect_like_table[MAX_SOLO_NOTES+1];
static float attack_rest_like; 


float
table_attack_spect_like(SA_STATE sas) {  
  float *solo_spect,meld[FREQDIM/2],p,*mix,t,*mem,flat[FREQDIM/2];
  int i;

  if (is_solo_rest(sas.index) || sas.shade == REST_STATE || sas.pos > 1 )   
    { return(1.); mem = &attack_rest_like;  solo_spect = restspect; }
  else { mem = &(attack_spect_like_table[sas.index]);  solo_spect = score.solo.note[sas.index].attack_spect; }

  p = *mem;
  if (p != PROB_UNSET) return(p);

  for (i=0; i < freqs; i++) flat[i] = 1./(float)freqs;

#ifdef ORCHESTRA_EXPERIMENT
  //mix = (accomp_on_speakers) ? orchestra_spect : solo_spect;
  if (mem == &attack_rest_like)  meld_solo_orch_back(background, background, background, meld);
  else   meld_solo_orch_back(solo_spect, solo_spect, flat, meld);
#else
  meld_spects(solo_spect, acc_spect, meld, MELD_CONST /*.7*/);
#endif


  for (i=0; i < freqs; i++) if (meld[i] <= 0) {
    printf("meld = %f solo = %f mix = %f back = %f\n",meld[i],solo_spect[i],mix[i],background[i]);
    printf("pos = %d rest = %d\n",sas.pos,sas.shade == REST_STATE);
    printf("index = %d\n",sas.index);
    printf("attack = %f\n",score.solo.note[sas.index].meld_spect[sas.pos][i]);
    exit(0);
  }

  p = attack_spect_like(meld);
  *mem = p;
  //  printf("attack prob = %f\n",p);
  return(p);
}




float sa_class_like(SA_STATE sas)  /* assumes quantized stats are computed */
{
  int f,q,p,sign,s,old_sign,st,note;
  float prob=1,*dist,*solo_spect,*acc_spect,spect_prob(),spect_like();
  float table_spect_like(),x,y;
  FEATURE *feat;
  TNODE *t;
  int (*sig)();
  FULL_STATE fs;
   

  for (f=0; f < feature.num; f++) {
#ifdef POLYPHONIC_INPUT_EXPERIMENT
    //       if (f == 1)   continue;
#endif

    //        if (f == 2) continue;
    //        if (f ==  1) continue;  
    //    if (f >=  0) continue;  
    feat = feature.el+f;
      
    t = tree_quant[f];
    p = feature.el[f].parm;
    sig = feature.el[f].signat;
    sign = sig(sas,p);
    //    if (f == 0) printf("sign = %d\n",sign);
    //    printf("shade = %d last = %f sig = %d\n",sas.shade,sas.last_accomp_time,sign);
    /*    if (f == 2 && sign == 1) { printf("f = 2 sign = 1"); }*/
    if (sign < 0 || sign >= feat->num_sig) {
      printf("bad sig f= %d sign = %d \n",f,sign);
      exit(0);
    }
    prob *=  t->cond_dist[sign];

    //     printf("frame = %d feat = %d sig = %d prob = %f\n",token,f,sign,t->cond_dist[sign]);
    /*    printf("f = %d sign = %d prob = %f\n",f,sign, t->cond_dist[sign]);*/
    
    // if (f==1 && sign==2) exit(0);

    //   s = state2index(sas.shade,sas.midi_pitch,fs); 
	 //printf("shade = %d",sas.shade);
    // index2state(s,&st,&note,&fs);
	 //printf("st = %d",st);

	 //if (token > 200) printf("s = %d shade = %d pitch = %d\n",s,sas.shade,sas.midi_pitch);
	 
    // if (f>3) {
    //   old_sign = pitch_signature(s,p);
    //   if (old_sign != sign) {
    //     printf("old = %d new = %d\n",old_sign,sign);
    //     printf("s = %d p = %d shade = %d pitch = %d\n",s,p,sas.shade,sas.midi_pitch);
    //   }
    // }
  }

#ifdef POLYPHONIC_INPUT_EXPERIMENT
  //    if (sas.index >= firstnote && sas.index <= lastnote) 
  //      solo_spect = score.solo.note[sas.index].spect;
  //    else solo_spect = restspect;

  //  acc_spect = restspect; //sas.acc_spect;

  //  printf("index = %d pitch = %d\n",sas.index,sas.midi_pitch);
  //  meld_spects(solo_spect, acc_spect, meld, MELD_PROB);

  x = table_spect_like(sas.index, sas.acc_spect,sas);
#ifdef ATTACK_SPECT
  y = table_attack_spect_like(sas);
     x *= pow(y,.25);  // .05

  
       return(pow(prob*x, .75/* 1.*/  /*2.*/   /*1.*/ /*.5*/ /*.8*/));
#endif
  //  printf("x = %f\n",x);
       //#ifdef JAN_BERAN  return(pow(prob*x, 1.0)); #endif // this makes more sensitive and helps with the entrances after long pauses
    return(pow(prob*x, .75 /*1.*/  /*2.*/   /*1.*/ /*.5*/ /*.8*/));
       //                       return(pow(prob*x, 1. /*1.*/  /*2.*/   /*1.*/ /*.5*/ /*.8*/));
       //             return(pow(prob*x, 1.5  /*2.*/   /*1.*/ /*.5*/ /*.8*/));
  //       return(prob*spect_like(solo_spect));
  
#endif

  return(prob);  
}

  

extern char scorename[];


read_train_info() {
  int i,j;
  char info[300];
  FILE  *fp;
  float ms,me;     
  void set_endpoints(float,float,int *, int *);

 /* printf("enter the training information file: ");
  scanf("%s",info); */
  strcpy(info,"train_info");
  fp = fopen(info,"r");     
  if (fp == NULL) {
     printf("couldn't open %s\n",info);
     exit(0);
  }
  fscanf(fp,"num files = %d\n",&tis.num);
/*  printf("num files = %d\n",tis.num);*/
  if (tis.num > MAX_TRAIN_FILES) {
     printf("cannot train on %d files\n",tis.num);
     exit(0);
   }
  for (i=0; i < tis.num; i++) {
    fscanf(fp,"score = %s\n",tis.info[i].score_file);
/*    printf("score = %s\n",tis.info[i].score_file);*/
	fscanf(fp,"audio data = %[^\n]\n",tis.info[i].audio_file);
    /*     printf("audio data = %s\n",tis.info[i].audio_file); */
	strcpy(scorename,tis.info[i].score_file);
    strcpy(scoretag,tis.info[i].score_file);  
    if (i == 0 || strcmp(tis.info[i].score_file,tis.info[i-1].score_file) != 0) {
#ifdef POLYPHONIC_INPUT_EXPERIMENT
      //      select_nick_score_var(tis.info[i].score_file);  /* sets audio_file inadvertently */
      read_midi_score_input(tis.info[i].score_file);
#endif
#ifndef POLYPHONIC_INPUT_EXPERIMENT
      readscore();
#endif
    }

    /*    fscanf(fp,"start time in measures = %f\n",&ms);
    fscanf(fp,"end   time in measures = %f\n",&me); */


    /*    ms = start_meas;   /* 1-21-02 commented this out 
    me = end_meas;
    set_endpoints(ms, me, &firstnote, &lastnote); 
    printf("start note = %d   end note = %d\n", firstnote,lastnote); 
    */

    strcpy(audio_file,tis.info[i].audio_file);
    read_parse();

    make_dp_graph();
    tis.info[i].start_graph = start_node; 
    tis.info[i].end_graph = end_node; 
    tis.info[i].firstnote = firstnote;
    tis.info[i].lastnote = lastnote;
  }
}

void
zc(tree)
TNODE *tree; {
  int s;

  if (tree->split != -1) {
    zc(tree->lc);
    zc(tree->rc);
    return;
  }
  for (s=0; s < MAX_SIG; s++) tree->count[s] = 0;
} 

//CF:  set tree->count[s] to zero for all nodes and all s
static void zero_tree_counts() {
  int f;
  FEATURE *feat;

  for (f = 0; f < feature.num; f++) {
     feat = feature.el+f;
     zc(feat->tree);
   }
}


static void  cumulate_counts() { 
  int i,n,j,p,type,v,st,a,f,r,sig;
  float prob;
  FULL_STATE *s1,*s2,fs;
  FEATURE *feat;
  TNODE *t;

  printf("thought this wasn't called\n");
  exit(0);
  for (token=0; token < frames; token++) {
    audio_listen();
    for (n=0; n < prob_hist[token].num; n++) {
      s1 = &(prob_hist[token].list[n].place->state);
      prob = prob_hist[token].list[n].prob;
      st = s1->statenum;
      j = s1->arg[0]; 
      v = state2index(st,j,fs);
      for (f = 0; f < feature.num; f++) {
	sig = signature[v][f];
	t = tree_quant[f];
	t->count[sig] += prob;
      }
    }
  }
}



SA_STATE
set_sa_state(float *acc_spect, int index, int shade, int midi_pitch, int last_pitch, float last_acc, SOUND_NUMBERS  *sn, int pos) {
  SA_STATE sas;

  sas.index = index;
  sas.acc_spect = acc_spect;
  sas.shade = shade;
  sas.midi_pitch = midi_pitch;
  sas.last_pitch = last_pitch;
  sas.last_accomp_time = last_acc;
  sas.acc_notes = sn;
  sas.pos = pos;
  return(sas);
}


//CF:  runs during training, just after fws-bkwd pass is complete.
//CF:  for each feature f, compute an empirical pdf P(naive_bin(f) | sig_f(state))
//CF:  As features may be vector valued, simension unknown at compile time, we flatten the naive bins
//CF:  into a 1D array.  We fix things so that there are always 1000 bins regardless of dimension.
static void  accumulate_counts() { 
  int i,n,j,p,type,v,st,a,f,r,sig;
  float prob;
  FULL_STATE *s1,*s2,fs;
  float val[MAX_DIM],*ar,time;
  FEATURE *feat;
  void (*func)();
  int dex[MAX_DIM],dim;
  int joint_index[MAX_FEATURES];
  SA_STATE sas;
  int (*fnc)();
  SOUND_NUMBERS *sounding_accomp(),*notes;
  float time_to_last_accomp();
  GNODE *gptr;

  for (token=0; token < frames; token++) { //CF:  for each frame
    notes = sounding_accomp(token);   //CF:  for midi version, collection of sounding ac notes
    time = time_to_last_accomp(token); //CF:  time elapsed since last acc note 
    audio_listen(); //CF:  get features
    for (f = 0; f < feature.num; f++) {  //CF:  each feature
      feat = feature.el+f;
      p = feat->parm;
      func = feat->feat;
      func(val,p);  //CF:  compute the feature on this frame -- stored in output val.
      dim = feat->dim;
/* if (token >= 1338) printf("%f %f\n",val[0],val[1]); */


      //CF:  this code chunk takes the n-tuple feature value at this frame
      //CF:  and converts it to a 1D int bin index (with COUNT_BUFF_LEN bins) which is stored in joint_index[f]

      for (i=0; i < dim; i++) {   //CF:  each component of feature n-tuple
	ar = feat->quantile[i];   //CF:  array of CDF quantiles (already computed)

	//CF:  compute the 1D int index of the bin for this feature value
	//CF:  the (int) recasts the float value as a bin index
	dex[i] =  (int) val2bin(val[i],root[dim],ar);     //CF:  root[dim] is (dim)th root of number of bins
	/*	if (f == 0) printf("i = %d dex = %d\n",i,dex[i]);	 */
      }
      //CF:  converts a n-tuple of attribute bin numbers and converts to a 1D bin index (doc this!)
      joint_index[f] = comp_index(dex,dim); 
      /*    if (f == 0) printf("joint_index = %d\n",joint_index[f]); */
    }

    //CF:  We now have joint_index[f] containing the flattened bin index for each feature f in this frame.

    for (n=0; n < prob_hist[token].num; n++) {   //CF:  each hypothesis that existed at this frame
      gptr = prob_hist[token].list[n].place;     //CF:  the GNODE hypothesis
      s1 = &(prob_hist[token].list[n].place->state);
      prob = prob_hist[token].list[n].prob;      //CF:  the gamma prob
      /*   printf("token = %d n = %d prob = %f\n",token,n,prob); */

      /*       sas.shade = st = s1->statenum;
      sas.midi_pitch = j = s1->arg[0]; 
      sas.acc_notes = sounding_accomp(token); */

      //CF: sas is relevant conditional information about soloist position inscore and accompaniment 
      //CF: The signiatures sig_{feature}(sas) are functions of this SAS. 
      //CF:  0 and 1 are cur and last pitch (unused now)
      sas = set_sa_state(restspect,gptr->note,s1->statenum,s1->arg[0],s1->arg[1],time,notes,gptr->pos); 


      /*      v = state2index(st,j,fs); */
      for (f = 0; f < feature.num; f++) {
	/*        sig  = signature[v][f]; */
	p = feature.el[f].parm;  
	fnc = feature.el[f].signat;
	sig = fnc(sas,p);  //CF:  compute feature's signiature value
	count[f][sig][joint_index[f]] += prob;  //CF:  **** increment the bin count *****
      }
    }
  }
}


static void init_counts() { 
  int a,i,s;

  for (a = 0; a < feature.num; a++) 
    for (s=0; s < feature.el[a].num_sig; s++) 
      for (i=0; i < COUNT_BUFF_LEN; i++) 
        count[a][s][i] = 0;
}

static void normalize_counts() {
  int f,s,i,sigs;
  float total;

  for (f=0; f < feature.num; f++) {
    total = 0;
    sigs =  feature.el[f].num_sig;
    for (s = 0; s < sigs; s++) 
      for (i=0; i < COUNT_BUFF_LEN; i++) total += count[f][s][i]; //CF:  for each bin (of this feature-sig pair)
    /*   for (s = 0; s < sigs; s++)  for (i=0; i < COUNT_BUFF_LEN; i++)  printf("%d %d %d = %f\n",f,s,i,count[f][s][i]); */
    if (total == 0.) { printf("about to divide by 0\n"); exit(0); }
    for (s = 0; s < sigs; s++) 
      for (i=0; i < COUNT_BUFF_LEN; i++) count[f][s][i] /= total ;
  }
}

#define PROB_PADDING .01


void
cs(t,s,sum)
TNODE *t;
int s;
float *sum; {
  if (t->split != -1) {
    cs(t->lc,s,sum);
    cs(t->rc,s,sum);
    return;
  }
  *sum += t->count[s];
}

static float count_sum(t,s) 
TNODE *t;
int s; {
  float sum=0;
  
  cs(t,s,&sum);
  return(sum);
}


void
cn(t,num)
TNODE *t;
int *num; {
  if (t->split != -1) {
    cn(t->lc,num);
    cn(t->rc,num);
    return;
  }
  (*num)++;
}

static int num_nodes(t) 
TNODE *t; {
  int num=0;
  
  cn(t,&num);
  return(num);
}


#define SUM_PADDING .01 
#define PADDING_COUNT 2.

//CF:  writes TNODE.cond_dist using TNODE.counts
//CF:  cond_dist is normalized, then multipled to sum to number of terminal tree nodes.
static void norm_tree(t,sum,s,q,w)
TNODE *t;
float sum;
int s,q;
float w; {
 

  if (t->split == -1) {
    /*    printf("unsmoothed count = %f\n",t->count[s]*total_training_frames);*/
    //    printf("count = %f sum = %f\n",t->count[s],sum);

    /*    t->count[s] += SUM_PADDING;   /* smooth dist */
    /*    t->cond_dist[s] = t->count[s] / (sum + q*SUM_PADDING);*/

    t->cond_dist[s] = (total_training_frames*t->count[s]+ PADDING_COUNT) / 
      (total_training_frames*sum + q*PADDING_COUNT);

    t->cond_dist[s] *= q;  /* a uniform value is 1 */
    t->cond_dist[s] = pow(t->cond_dist[s],w);
  }  else {
    norm_tree(t->lc,sum,s,q,w);
    norm_tree(t->rc,sum,s,q,w);
  }
}


static void 
ttl_ent(TNODE *t, float *h) {
  if (t->split == -1) *h += t->entropy_cont;
  else {
    ttl_ent(t->lc,h);
    ttl_ent(t->rc,h);
  }
}

static float
mutual_info(TNODE *t) {
  float h=0;

  ttl_ent(t, &h);
  return(t->entropy_cont - h);
}




#define TRAIN_OUTPUT_FILE "counts.txt"
  
static void set_probs() {
  int f,sigs,s,num;
  float sum,w;
  TNODE *t;
  char file[200];
  FILE *fp;

 /* strcpy(file,TRAIN_DIR);
  strcat(file,TRAIN_OUTPUT_FILE);
  */
  strcpy(file,"train/jan_beran/counts.txt");
  fp = fopen(file,"w");
  if (fp == NULL) {
	printf("can't open %s\n",file);
	exit(0);
  }
  for (f=0; f < feature.num; f++) {
    sigs =  feature.el[f].num_sig;
    t =  feature.el[f].tree;
    w =  feature.el[f].weight;
    fprintf(fp,"feature %d\tsig marginal ent = %f\tmut info = %f\n",f,t->entropy_cont,mutual_info(t));
    for (s = 0; s < sigs; s++) {
      sum = count_sum(t,s);
      fprintf(fp,"\tsignature %d\t count = %f\n",s,sum*total_training_frames);
      num = num_nodes(t);
      //            printf("signature = %d\n",s);
      norm_tree(t,sum,s,num,w);
    }
  }
  fclose(fp);
}

  


static void normalize_tree(t,sum,sigs,q,w)
TNODE *t;
float *sum;
int sigs,q;
float w; {
  int s;

  if (t->split == -1) for (s=0; s < sigs; s++) {
    t->cond_dist[s] += PROB_PADDING;   /* smooth dist */
    t->cond_dist[s] /= (sum[s] + q*PROB_PADDING);
    t->cond_dist[s] *= q;  /* a uniform value is 1 */
    t->cond_dist[s] = pow(t->cond_dist[s],w);
  }
  else {
    normalize_tree(t->lc,sum,sigs,q,w);
    normalize_tree(t->rc,sum,sigs,q,w);
  }
}

#define TREE_LEVELS 3

static void tree_init(feat)  /* assumes feat->quantile is set */
FEATURE *feat; {
  int q,s;
  float sum[MAX_SIG];

  q=1;
  q <<= TREE_LEVELS;
  flat_tree(0.,1.,feat->quantile[0],&(feat->tree),q,1,feat->num_sig);
/*  feat->num_quanta =  label_tree(feat->tree);
  init_tree_probs(feat->tree,feat->num_sig,feat->num_quanta);
  for (s=0; s < MAX_SIG; s++) sum[s]=1;
  normalize_tree(feat->tree,sum,feat->num_sig,
		 feat->num_quanta,feat->weight);  */
}


//CF:  ct is a slice of the count matrix for one feature.
//CF:  its 2D: sigs * pdf  (where pdf is actually hi-D but stored in a 1D array)
//CF:   (we will need to unpack this 1D array, using lo and hi which describe the subrectangle to be split)
//CF:  dim is the dimensionality of the feature (ie. the length of lo and hi arrays)
//CF:  sig is the number of signiatures. 
//CF:  sig_marg is the output variable ?   marg is a global var also used for output.
//CF:  sig_marg stores P(sig) for the subrectangle -- we will compare against P(sig|split) for maxent search
//CF:  marg     stores P(component_i=bin,sig) for each marginalized component_i (So 3D)
//CF:    (note these are unnormalised counts)
//CF:  This is all for ONE feature
static void make_marg(ct,lo,hi,dim,sigs,sig_marg)
/* on return sig_marg is  sum p(x,s) = p(t,s)
                           x
   where the sum is taken over the values of x coresponding to the range given in hi,lo 
   and t is the tree node and s is the label value 
   using this notation
     marg[s][d][j] = p(t,s,x(d)=j)
  i.e. the dth comonent of the feature vector in the jth bin */ 
float **ct;
float sig_marg[MAX_SIG];
int *lo,*hi,dim,sigs; {
  int i,j,num,cur[MAX_DIM],c,w,s,d;

//CF:  need picture!
//CF:  marg 

  for (s=0; s < sigs; s++)  {
    sig_marg[s] = 0;
    for (d=0; d < dim; d++) //CF:  number of components of feature
      for (j=lo[d]; j < hi[d]; j++)  marg[s][d][j] = 0;
  }
  num = 1;
  for (d=0; d < dim; d++) {
    num *= (hi[d]- lo[d]);  //CF:  num is the volume in bins of the subrectangle 
    cur[d] = lo[d];   //CF:  cur is a vector bin cordinate, that will iterate over the subrectangle
  }
  for (i=0; i < num; i++) {  /* for different marginals */  //CF:  for every bin in the hiD rectangle
    c =  comp_index(cur,dim);  //CF:  compute index.  Convert vector index to flattened index
    for (s=0; s < sigs; s++) {
      sig_marg[s] += ct[s][c];
      //CF:  this data point affects all the different dimensional marginals
      //CF:  for of these marginals, add the data point to it, at its position within that dimension (picture!)
      for (d=0; d < dim; d++)  
	marg[s][d][cur[d]] += ct[s][c];
    }
    //CF:  move cur (the vector position) to the next place
    //CF:  carrying -- if at end of a dimension, increment on the next dim and set current dimension's location to zero
    cur[0]++;
    for (d=0; d < dim-1; d++) if (cur[d] == hi[d]) {  
      cur[d] = lo[d];
      cur[d+1]++;   /* cur[d] is index of dth marginal */
    }
  }
}

//CF:  See comment where entropy_cont is declared.  (TODO vs defined?)
//CF:  Each terminal node in the tree will make a contribution to the total conditional entropy H(L|X)
//CF:  where L is the signiature and X is the termninal node. We do not yet know if the current node being
//CF:  considered is terminal. But if it were, then entropy contribution would be what is computed here.
//CF:  Which is exactly the forumla given in the declaration for fixed x (x is a terminal node).
//CF:  sigs: number of sigs
//CF:  v is the cond_dist, which is a sigs-length array of unnormalised P(sig)
//CF:  the elements v[s] represent P(x,L) for a fixed x, ranging over signiature labels L.
static float entropy_contribution(v,sigs) /* a vector of length MAX_SIG or less */
float *v; /* v[i] = P( X=x, label = i) */
int sigs; {
  float h=0,p=0,l;
  int s;

  for (s=0; s < sigs; s++) if (v[s] > 0)  {
    h += v[s]*log(v[s])/log(2.);
    p += v[s];  //CF:  normalisation, Z
  }
  l = (p > 0) ? p*log(p)/log(2.) : 0;
  return(l - h);
}

//CF:  lo,hi are arrays describing current subrectangle
//CF:  dim is the number of components of the feature
//CF:  sigs = num of signiatures
//CF:  on return...
//CF:     best_h is the change in entropy caused by the best split  (see formula ENTROPY_FORMULA)
//CF:     best_loc is the index of the naive bin to split on, range: lo[best_dim] to hi[best_dim]-1
//CF:     best_dim is the component to split on
//CF:   TODO picture!
static void best_split(lo,hi,dim,sigs,best_h,best_loc,best_dim)
float *best_h;
int *lo,*hi,dim,sigs,*best_loc,*best_dim; {
  int j,s,d,change;
  float left[MAX_SIG],rite[MAX_SIG],h;

  *best_h = HUGE_VAL;  //CF:  we try to min h, so start search at high value
  for (d=0; d < dim; d++) {  //CF:  try each component
    for (s=0; s < sigs; s++) left[s] = rite[s] =  0;  //CF:  will be (unnormed) pdfs over sigs, for the two child rectangles

    //CF:  start by putting all the probability mass in rite
    //CF:  we will move some of the mass out of rite and into left later
    for (s=0; s < sigs; s++) for (j=lo[d]; j < hi[d]; j++) 
      rite[s] += marg[s][d][j];

    //CF:  do the move
    for (j=lo[d]; j < hi[d]; j++) {  //CF:  j indexes possible split values
      change = 0;
      for (s=0; s < sigs; s++) {
	left[s] += marg[s][d][j];
	rite[s] -= marg[s][d][j];
	if (marg[s][d][j] != 0) change = 1;
      }
      if (change) {
	h = entropy_contribution(left,sigs) + entropy_contribution(rite,sigs);
/*printf("h = %f el = %f er = %f l0= %f l1 = %f r0 = %f r1= %f\n",h,entropy_contribution(left,sigs),entropy_contribution(rite,sigs),left[0],left[1],rite[0],rite[1]); */
	if (h < *best_h) { *best_h = h; *best_loc = j+1; *best_dim = d; } //CF:  store this split if best so far
      }
    }
  }
}
  

static void ws(t,minv,mint)
TNODE *t,**mint;
float *minv; {
  int both_children_terminal;
  float value;

 /*  printf("minv = %f\n",*minv); */
  both_children_terminal =  (t->lc->split == -1 && t->rc->split == -1);
  if (both_children_terminal) {
    value = t->entropy_cont - (t->lc->entropy_cont + t->rc->entropy_cont);
    if (value < *minv) {
      *minv = value;
      *mint = t;
    }
  }
  if (t->lc->split != -1) ws(t->lc,minv,mint);
  if (t->rc->split != -1) ws(t->rc,minv,mint);
}

static TNODE *worst_split(t)
TNODE *t; {
  float minv;
  TNODE *mint;

  minv = HUGE_VAL;
  ws(t,&minv,&mint);
  return(mint);
}

static void prune_tree(t,targ_num)
TNODE *t; 
int targ_num; {
  TNODE *cut;
  int num;
       
  num = label_tree(t);
  if (num < targ_num) {
    printf("target number greater than current number\n");
    return;
  }
  for (; num > targ_num; num--) {
    cut = worst_split(t);
    cut->split = -1;
  }
}
    
  



#define MIN_DELTA_ENTROPY .001

//CF:  Build Tree.
//CF:  ct=2D part of count for one feature; sig * pdf(really multi-D over componetents, but rep'd as 1D array)
//CF:    dim=dimension of this feature  ; 
//CF:  See daigram. Recursively subpartition feature space.
//CF:  At each subrectangle, find the best partition.
//CF:  If the partition is useful, recurse.  Otherwise stop.
static void bt(ct,dim,t,hi,lo,sigs,q) 
float **ct; 
int dim,*hi,*lo,sigs; 
TNODE *t; 
float **q; {
  float best_h;
  int s,best_dim,best_loc,new_lo[MAX_DIM],new_hi[MAX_DIM],i;

  t->split = -1; //CF:  mark root node as terminal initially (if we do split, overwrite this)
  make_marg(ct,lo,hi,dim,sigs,t->cond_dist);  /* t->cond_dist doesn't sum to 1 */
/* for (s=0; s < sigs; s++) printf("%d %f\n",s,t->cond_dist[s]);  */
/*for (s=0; s < COUNT_BUFF_LEN; s++) if (marg[0][0][s]+marg[1][0][s] > 0) printf("%d %f %f\n",s,marg[0][0][s],marg[1][0][s]); */
  t->entropy_cont = entropy_contribution(t->cond_dist,sigs);
  best_split(lo,hi,dim,sigs,&best_h,&best_loc,&best_dim);  //CF:  &s are outputs
  /*printf("dim = %d loc = %d, entropy = %f\n",best_dim,best_loc,best_h);
printf("node entropy = %f\n",t->entropy_cont);  */
  
  //CF:  this (non-negative) difference is the entropy improvement.  Was it enough to be worthwhile?  
  //CF:  If it's good, include it in the tree, make children, and recurse!
  if (t->entropy_cont - best_h > MIN_DELTA_ENTROPY ) { /* usable split  */
    t->lc = new_tnode();
    t->rc = new_tnode();
    t->split = best_dim;
/*    i = (QUANTILES*best_loc)/root[dim];
    t->cutoff = q[best_dim][i]; */
    t->cutoff =  bin2val((float) best_loc,root[dim],q[best_dim]);

    //CF: make new coordinates for children.  
    for (i=0; i < dim; i++) {
      new_lo[i] = lo[i];
      new_hi[i] = hi[i];
    }
    new_hi[best_dim] = new_lo[best_dim] = best_loc; 
   
    //CF:  recurse!
    bt(ct,dim,t->lc, new_hi,  lo,     sigs,q);
    bt(ct,dim,t->rc, hi,      new_lo, sigs,q);
  }
}




static alloc_arrays() {
  int f,s,ss,d;

  count = (float ***) malloc(feature.num*sizeof(float **));
  for (f=0; f < feature.num; f++) {
    ss = feature.el[f].num_sig;
    count[f] = (float **) malloc(ss*sizeof(float *));
    for (s = 0; s < ss; s++) count[f][s] = 
       (float *) malloc(COUNT_BUFF_LEN*sizeof(float));
  }

  marg = (float ***) malloc(MAX_SIG*sizeof(float **));
  for (s=0; s < MAX_SIG; s++) {
    marg[s] = (float **) malloc(MAX_DIM*sizeof(float *));
    for (d = 0; d < MAX_DIM; d++) marg[s][d] = 
       (float *) malloc(COUNT_BUFF_LEN*sizeof(float));
  }
}
     

     
     

#define TARGET_LEAVES /*8 */ 16

//CF:  rebuild the VQ trees using greedy maxent
static void build_trees() {
  int f,hi[MAX_DIM],lo[MAX_DIM],d,s;
  FEATURE *feat;
  float sum[MAX_SIG],h;
  
  for (f=0; f < feature.num; f++)  {
    feat = feature.el+f;
    for (d=0; d < feat->dim; d++) {
      lo[d] = 0;
      hi[d] = root[feat->dim];   //CF:  number of bins on one side of hypercube
    }
    //CF:  count[f] is a 2D array of sigs * pdf values
    
    //CF:  build the tree (recursing until entropy gains are insignificant)
    bt(count[f],feat->dim,feat->tree,hi,lo,feat->num_sig,feat->quantile);

    feat->num_quanta =  label_tree(feat->tree); //CF:  count number of terminals
    printf("quanta before pruning = %d (mutual info = %f)\t",feat->num_quanta,mutual_info(feat->tree));
    
    //CF:  prune tree to (at most) a fixed number of terminals, TARGET_LEAVES
    prune_tree(feat->tree,TARGET_LEAVES);
    feat->num_quanta =  label_tree(feat->tree);
    
    printf("quanta after pruning = %d (mutual info = %f) signature entropy = %f\n",feat->num_quanta,
       mutual_info(feat->tree),feat->tree->entropy_cont);

    //CF:  at this stage, probabilities sum to ? 
    //CF:  make them sum to ?
    for (s=0; s < feat->num_sig; s++) 
      sum[s] = feat->tree->cond_dist[s];
    normalize_tree(feat->tree,sum,feat->num_sig,
		   feat->num_quanta,feat->weight); 

  }
}
  

/*static void visit_tree(t,func)
TNODE *t;
void  (*func)();  {
  func(t);
  if (t->lc) visit_tree(t->lc,func);
  if (t->rc) visit_tree(t->rc,func);
}*/


static void pinball_trees() {
  int f,sigs,s,i,dex[MAX_DIM],dim,a,d,m,lim;
  FEATURE *feat;
  TNODE *t;     
  float val[MAX_DIM],*ar,x,v;
     
  for (f=0; f < feature.num; f++)  {
     feat = feature.el+f;
     dim = feat->dim;
     sigs =  feat->num_sig;
     lim = 1;
     for (d=0; d < dim; d++) lim *= root[dim];
     for (i=0; i <  lim/*COUNT_BUFF_LEN*/; i++) { 
       comp_index_inv(dex,dim,i);
       a = comp_index(dex,dim);
       if (a != i)  {
	 printf("whoa nellie\n");
	 printf("a = %d i = %d\n",a,i);
	 for (m=0; m < dim; m++) printf("%d %d\n",dex[m],dim);
       }
       for (d=0; d < dim; d++) {
	 ar = feat->quantile[d];
	 val[d] = bin2val((float) dex[d],root[dim],ar);
/*	 m =  val2bin((float) val[d],root[dim],ar);
	 v =  bin2val(m,root[dim],ar);
	 if (v != val[d]) printf("whoa millie %d %d %f %f  root = %d\n",m,dex[d],val[d],v,root[dim]); */
       }
       t = tree_quantize(feat->tree,val);
       for (s=0; s < sigs; s++) t->count[s] += count[f][s][i];
     }
   }
}



init_trees() {  /* inialize based on 1st comp of feat vect */
  int f,i,q,s;
  FEATURE *feat;
  float sum[MAX_SIG];
  void (*func)();

  q=1;
  q <<= TREE_LEVELS;
  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    func = feat->tree_z;
    func(f);
    for (s=0; s < feat->num_sig; s++) 
      sum[s] = 1;
    normalize_tree(feat->tree,sum,feat->num_sig,
		   INIT_QUANTS,feat->weight);  
  }
}



     
#define EM_ITER 5


new_train_class_probs()
{
  int i,j,success;
 /* float count[MAX_FEATURES][MAX_SIG][COUNT_BUFF_LEN];  */

/*scale_dists();
write_distributions();
return; */

  printf("attrib_num = %d\n",atrib_num);
  read_train_info(); 
  train_cdfs();
/*  init_class(); */
 
  init_trees(); 
  
alloc_arrays();  /* probly only nmeed this for entropy maxing quantization */
/*  print_trees();
  write_trees(); */
/*  read_trees();
  print_trees(); */
/*  init_dists(); */
  write_distributions();

  for (i=0; i < EM_ITER; i++) {
/*    init_counts(); */
     zero_tree_counts();
    for (j=0; j < tis.num; j++) {
      read_audio_train(tis.info[j].audio_file);
      start_node = tis.info[j].start_graph;
      end_node = tis.info[j].end_graph;
      success = forward_backward();
      if (success)/*  accumulate_counts(); */ cumulate_counts();
      else printf("couldn't train on %s\n",tis.info[j].audio_file);
    }
    set_probs();
    write_distributions();


/*printf("a\n");
    normalize_counts();
printf("a\n");
    build_trees();
printf("a\n");
    write_distributions();
printf("a\n"); */
  }
}

#define MIN_TRAINING_FILES 1

    
mod_train_class_probs()
{
  int i,j,success,total_files=0;
  char tmode[500],out_dist_file[300],in_dist_file[300],in_quant_file[300],out_quant_file[300];


  printf("enter 'scratch' to train from scratch, 'initialize' to train from partial results");
  scanf("%s",tmode);
  if (1/*strcmp(tmode,"scratch") == 0*/) {
   /* printf("enter the full path name to write quant file");
	scanf("%s",out_quant_file);  */
	strcpy(out_quant_file, "train/jan_beran/quant.dat");
   /* printf("enter the full path name of the distribution file\n");
	scanf("%s",out_dist_file);  */
	strcpy(out_dist_file, "train/jan_beran/distribution.dat");
	read_train_info();
	train_cdfs_file(out_quant_file);
    init_trees(); 
    alloc_arrays();  /* probly only nmeed this for entropy maxing quantization */
    write_distributions_file(out_dist_file);
  }
  else if (strcmp(tmode,"initialize") == 0) {
    printf("enter the full path name of input quant file");
    scanf("%s",in_quant_file);
    read_cdfs_file(in_quant_file);
    printf("enter the full path name of input distribution file\n");
    scanf("%s",in_dist_file);
    read_distributions_file(in_dist_file);
    printf("enter the full path name of output distribution file\n");
    scanf("%s",out_dist_file);
    read_train_info(); 
    alloc_arrays();  /* probly only nmeed this for entropy maxing quantization */
  }
  else { printf("undefined training option: %s\n",tmode); exit(0); }

  for (i=0; i < EM_ITER; i++) {
    total_training_frames = 0;
    init_counts(); 
    for (j=0; j < tis.num; j++) {
      read_audio_train(tis.info[j].audio_file);
      start_node = tis.info[j].start_graph;
      end_node = tis.info[j].end_graph;
      firstnote = tis.info[j].firstnote;
      lastnote = tis.info[j].lastnote;
      success = forward_backward(); //CF:  run the passes ******
      if (success)  { //CF:  could fail if last node (absorbing state) not reached by beam search 
	accumulate_counts(); //CF:  into naive bins (ie just make 1000 equally sized bins in quantile space)
	total_files++;
	total_training_frames += frames; 
      }
      else printf("couldn't train on %s\n",tis.info[j].audio_file);
    }
    if (total_files < MIN_TRAINING_FILES) {
      printf("not enough successful files to train\n");
      exit(0);
    }
    normalize_counts();  //CF:  normalise the global count matrix
    build_trees();     //CF:  lots of code here
    
    //CF:  this seems to replicate the caluclation already done, sets counts at each node?
    //CF:  (exce
    zero_tree_counts(); //CF:  set TNODE.counts to zeroes (not the global matrix)
    pinball_trees();  

    set_probs(); 
    write_distributions_file(out_dist_file);
  }
}

      

newer_train_class_probs()
{
  int i,j,success,total_files=0;
 /* float count[MAX_FEATURES][MAX_SIG][COUNT_BUFF_LEN]; */

/*scale_dists();
write_distributions();
return; */


  printf("attrib_num = %d\n",atrib_num);
  read_train_info(); 
  train_cdfs();
/*  init_class(); */
 
  init_trees(); 
  
  alloc_arrays();  /* probly only nmeed this for entropy maxing quantization */
/*  print_trees();
  write_trees(); */
/*  read_trees();
  print_trees(); */
/*  init_dists(); */
  write_distributions();

  for (i=0; i < EM_ITER; i++) {
    init_counts(); 
    for (j=0; j < tis.num; j++) {
      read_audio_train(tis.info[j].audio_file);
      start_node = tis.info[j].start_graph;
      end_node = tis.info[j].end_graph;
      success = forward_backward();
      if (success)  {
	accumulate_counts();
	total_files++;
      }
      else printf("couldn't train on %s\n",tis.info[j].audio_file);
    }
    if (total_files < MIN_TRAINING_FILES) {
      printf("not enough successful files to train\n");
      exit(0);
    }
    normalize_counts();
    build_trees(); 
    zero_tree_counts();
    pinball_trees();  
    set_probs(); 
    write_distributions();
  }
}

      

    
    


  


  
/* static float node_entropy(marg,lo,hi,sigs)
float marg[MAX_SIG][MAX_DIM][COUNT_BUFF_LEN];
int *lo,*hi; {
  int i,j,s;
  float left[MAX_SIG];
  
  for (s=0; s < sigs; s++) left[s] = 0;
  for (s=0; s < sigs; i++) for (j=lo[0]; j < hi[0]; j++) 
    left[s] += marg[s][0][j];
  return(entropy(left,sigs));
}
  
*/  
  




scale_dists() {
  int a,j,i;

  for (a = 0; a < atrib_num; a++)  
    for (i=0; i < dm[a].range; i++) 
      for (j=0; j < CELLS; j++) 
	dm[a].dist[i][j] =  pow(dm[a].dist[i][j],dm[a].expon);
}  

set_dists() {
  int a,j,i;
  float count;

  for (a = 0; a < atrib_num; a++)  for (i=0; i < train_count[a].range; i++) {
      count = 0;
      for (j=0; j < CELLS; j++) count += train_count[a].dist[i][j];
      for (j=0; j < CELLS; j++) 
	dm[a].dist[i][j] =  train_count[a].dist[i][j] / (count/CELLS);
  }
  scale_dists();
  write_distributions();
}

void
train_class_probs() {
  char file[500],ans[10];
  int t,s,i,n;
  float prob,*x;

  mod_train_class_probs();
  return;
  newer_train_class_probs();
    return;  
}





#define PARM_FILE "parm.dat"

write_parms() {
  FILE *fp;
  int a,i;
  BETA *b;
			
  fp = fopen(PARM_FILE,"w");
  for (a = 0; a < atrib_num; a++) {
    fprintf(fp,"attrib %d  categories = %d\n",a,dm[a].range);
    for (i=0; i < dm[a].range; i++)  {
      b = dm[a].parm+i;
      fprintf(fp,"%f %f %f %f",b->alpha,b->beta,b->count,b->log_partition);
      fprintf(fp,"\n");
    }
    fprintf(fp,"\n");
  }
  fclose(fp);
}

read_parms() {
  FILE *fp;
  int a,d,i;
  BETA *b;
			
  fp = fopen(PARM_FILE,"r");
  for (a = 0; a < atrib_num; a++) {
    fscanf(fp,"attrib %d  categories = %d\n",&d,&(dm[a].range));
    for (i=0; i < dm[a].range; i++)  {
      b = dm[a].parm+i;
      fscanf(fp,"%f %f %f %f",&(b->alpha),&(b->beta),&(b->count),&(b->log_partition));
      fscanf(fp,"\n");
    }
    fscanf(fp,"\n");
  }
  fclose(fp);
}


add_feature(f)
FEATURE f; {
  int a,i;

  if (f.dim > MAX_DIM) {
    printf("dim greater than %d\n",MAX_DIM);
    exit(0);
  }
  if (f.num_sig > MAX_SIG) {
    printf("too many sigs %d\n",MAX_SIG);
    exit(0);
  }
  if (feature.num >= MAX_FEATURES) {
    printf("no more room for features\n");
    exit(0);
  }
  feature.el[feature.num++] = f;
  feature.attribs += f.dim;
  if (feature.attribs > MAX_ATTRIBS) {
    printf("only allowed %d differnt attributes\n",MAX_ATTRIBS);
    exit(0);
  }
}


count_signatures() {
  int i,j,mx;

  printf("num = %d\n",feature.num);
  for (i=0; i < feature.num; i++) {
    mx = 0;
    for (j=0; j < state_num; j++) 
      if (signature[j][i] > mx) mx = signature[j][i];
    feature.el[i].num_sig = mx+1;
    if (feature.el[i].num_sig >= MAX_SIG) {
      printf("too many signatures for feature %d\n",i);
      exit(0);
    }
/*    printf("feat = %d sigs = %d\n",i,feature.el[i].num_sig);
    getchar();
    getchar(); */
  }
}

old_init_features() { /* mila - high level routine to initialize features */
     FEATURE f;
     int i;
     
     init_pitch_intervals();

     feature.num = feature.attribs = 0;
     f.feat = energy_feature;
     f.signat = sa_energy_signature; 
     f.dim = 1;
     f.weight = .3;
     f.num_sig = 3; /*3; */
     f.tree_z = /*even_tree;*/ unif_tree; //pos_cor_tree; 
     add_feature(f);



     f.feat = bang_feature;
     f.signat = sa_bang_signature;
     f.dim = 1;
     f.weight = .95; /*.5;*/ 
     f.num_sig =9;
     f.tree_z = /*even_tree;*/ pos_cor_tree; 
       /*       f.tree_z = unif_tree;   */
     add_feature(f);

     f.feat = slope_feature;
     f.signat = sa_slope_signature;
     f.dim = 1;
     f.weight = /*.95;*/ .5; 
     f.num_sig = 8;
     f.tree_z = /*even_tree;*/ pos_cor_tree; 
       /*       f.tree_z = unif_tree;   */
     add_feature(f);


     f.feat = spect_diff_feature;
     f.signat = sa_spect_diff_signature;
     f.dim = 1;
     f.weight = .95; /* .5;*/
     f.num_sig = 7;
     f.tree_z = pos_cor_tree; 
       /*       f.tree_z = unif_tree;   */
     add_feature(f); 

     for (i=0; i < pitch_int_num; i++) {
       f.feat = pitch_feature;
       f.signat = sa_pitch_signature;
       f.parm = i;
       f.dim = 2;
       f.num_sig = 4+2*(NUM_DIVIDES+1);  // NUM_DIVIDES+2
       f.weight = 4./*3.*//(float) pitch_int_num;
       f.tree_z = /*even_tree;*/ unif_tree;/*pitch_cor_tree; */
       /*              f.tree_z = pitch_cor_tree;  */
       add_feature(f);
     }
}


int
init_features() { /* mila - high level routine to initialize features */
     FEATURE f;
     int i;



     
     new_init_pitch_intervals();

#ifdef POLYPHONIC_INPUT_EXPERIMENT
     init_attack_pitch_intervals();
#endif

     feature.num = feature.attribs = 0;

#ifdef ORCHESTRA_EXPERIMENT
     /*     f.feat = energy_feature;
     f.signat = sa_energy_signature; 
     f.num_sig = 5; */


     /* this was the well-established feature */

     /*               f.feat = orchestra_energy_feature;
     f.signat = sa_orch_energy_signature;  
     f.num_sig = 3; 
     f.dim = 1;
     f.weight = .1;
     f.tree_z =  pos_cor_tree; 
     add_feature(f);    */


     f.feat = high_energy_feature;
     f.signat = sa_high_energy_signature; 
     f.num_sig = 2; 
     f.weight = 0.; //.05; //.1;   // look for a situation where this feature helps.
#ifdef JAN_BERAN
     f.weight = .2; 
#endif
     f.dim = 2;
     f.tree_z =  pos_cor_tree;            
     add_feature(f);   

     /*                    f.feat = energy_feature; // this is for JAN_BERAN
     f.signat = sa_energy_signature; 
     f.dim = 1;
     f.weight = .3;
     f.num_sig = 5; 
     f.tree_z =  pos_cor_tree; 
     add_feature(f);*/

#else
     f.feat = energy_feature;
     f.signat = sa_energy_signature; 
     f.dim = 1;
     f.weight = .3;
     f.num_sig = 5; /*3; */
     f.tree_z =  pos_cor_tree; 
     add_feature(f);
#endif

     f.feat = disruption_feature;
     //     f.signat = sa_bang_signature;
     f.signat = new_sa_bang_signature;
     /*          f.dim = 3;*/
     /*     f.dim = 5;*/
     f.dim = 2;
     f.weight = .5; 

#ifdef POLYPHONIC_INPUT_EXPERIMENT
     f.weight =  .1; //.6; //.5;  // .25 //.5; /* 9 */
#ifdef JAN_BERAN
     f.weight = .5; 
#endif

#endif
#ifdef CLARINET_EXPERIMENT
       f.weight = .25;
#endif
#ifdef VIOLIN_EXPERIMENT
	  f.weight = 0.;
#endif

	  //#ifdef WINDOWS
	  //	  f.weight = 0.;
	  //#endif

     f.num_sig =9;
     f.tree_z = unif_tree;   
     f.tree_z = /*even_tree;*/ pos_cor_tree; 
     add_feature(f);


#ifdef POLYPHONIC_INPUT_EXPERIMENT
          return(0);
     for (i=0; i < attack_int_num; i++) {
       f.feat = attack_pitch_feature;
       f.signat = sa_attack_pitch_signature;
       f.parm = i;
       f.dim = 1; 

       f.num_sig = ATTACK_HARM_NUM +1;

       f.weight = 1./(float) attack_int_num;
       f.tree_z = pos_cor_tree;
       add_feature(f);
     }

#endif

     for (i=0; i < pitch_int_num; i++) {
       f.feat = pitch_feature;
       f.signat = sa_pitch_signature;
       f.parm = i;
       f.dim = 2; 
       /*             f.dim = 3;  /* added new component 1-02 */

       f.num_sig = 4+2*(NUM_DIVIDES+1);  // NUM_DIVIDES+2*/
		   /*       f.num_sig = 4+3*(NUM_DIVIDES+2);  */


       f.weight = 4./*3.*//(float) pitch_int_num;
       f.tree_z = /*even_tree;*/ unif_tree;

       f.weight = 2. / (float) pitch_int_num;
       f.tree_z =  pitch_cor_tree; 


       f.weight = 5./(float) pitch_int_num;
       f.tree_z = pos_cor_tree;

       /*              f.tree_z = pitch_cor_tree;  */
       add_feature(f);
     }
     /*     for (i=0; i < pitch_int_num; i++) {
       f.feat = pitch_burst_feature;
       f.signat = sa_pitch_burst_signature;
       f.parm = i;
       f.dim = 1; 
       f.num_sig = 2;
       f.weight = .2/(float) pitch_int_num;
       f.tree_z = pos_cor_tree;
       add_feature(f);
       }*/
}


test_main() {
     init_features();
     train_cdfs();

}





SOUND_NUMBERS 
*old_sounding_accomp(int frame)
{
  int i=0,n,low,high,mid,pos;
  float frame_in_secs;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  
  if (mode == LIVE_MODE) {
    if (accomp_on_speakers == 0) return(&empty_sn);
    if (cur_accomp == first_accomp) return(&empty_sn);
    return(&(score.midi.burst[cur_accomp-1].snd_notes));
  }

  frame_in_secs = frame*(TOKENLEN/(float)SR);
  p = &(score.midi);
  n = p->num; // total # of accomp events
  low = first_accomp;
  high = last_accomp;
  empty_sn.num = 0;
  if ( first_accomp == 0 && last_accomp == 0) 
    return(&empty_sn);
  if (frame_in_secs < p->burst[low].actual_secs || frame_in_secs > p->burst[high].actual_secs)
    {
      return(&empty_sn); 
    }  
  else {
    
    for (i=first_accomp+1; i<=last_accomp; i++) {
      if (frame_in_secs <= p->burst[i].actual_secs) {
	return(&(p->burst[i-1].snd_notes));
      }
    }
    printf("time = %f first event = %f\n",frame_in_secs, p->burst[first_accomp].actual_secs);
    return(&empty_sn);
  }
}


void
sndnums2string(SOUND_NUMBERS s, char *target) {
  char note[500],x[500];
  int i;

  target[0] = 0;
  for (i=0; i < s.num; i++) {
    num2name(s.snd_nums[i],note);
    strcat(target,note);
    strcat(target," ");
  }
}


static float
get_acc_event_time(int i) {
  
  if (mode == SYNTH_MODE)  return(score.midi.burst[i].recorded_secs);
  else return(score.midi.burst[i].actual_secs);
}



float *
working_frame2spect(int frame)
{
  int i=0,n,low,high,mid,pos,j;
  float frame_in_secs,t;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];


  frame_in_secs = frame*(TOKENLEN/(float)SR);
  //  printf("first_accomp = %d time = %f frameinsecs = %f\n",first_accomp,score.midi.burst[first_accomp].actual_secs,frame_in_secs);
  if (mode == SYNTH_MODE) t = score.midi.burst[first_accomp].recorded_secs;
  else t = score.midi.burst[first_accomp].actual_secs;
  if (frame_in_secs <= t)
    return(restspect);
  for (i=first_accomp+1; i<=last_accomp; i++) {
    if (mode == SYNTH_MODE) t = score.midi.burst[i].recorded_secs;
    else t = score.midi.burst[i].actual_secs;
    if (t >= frame_in_secs) {
      //      printf("melding with spect %d\n",i-1);
      //      printf("i = %d\n",i);
      //      printf("returning burst i = %d %d\n",i,score.midi.burst[i-1].spect);
      printf("token = %d using chord %d\n",token,i-1);
      return(score.midi.burst[i-1].spect);
    }
  }
  return(restspect);
}



int
recent_piano_index(int frame) {
  int i=0,n,low,high,mid,pos,j,lasta;
  float frame_in_secs,t;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];


  frame_in_secs = tokens2secs((float)frame-1);  // -1 is more accurate for unknown reason on Bosendorfer
  lasta = (mode == LIVE_MODE) ? cur_accomp-1 : last_accomp;
  t = get_acc_event_time(first_accomp);
  if (frame_in_secs <= t) return(-1);
  for (i=first_accomp+1; i<=lasta; i++) {
    t = get_acc_event_time(i);
    if (t >= frame_in_secs) return(i-1);
  }
  if (lasta < 0) {printf(" this should not be\n"); exit(0); }
  return(lasta);
}



int
has_note_on(int index) {
  int i;

  for (i=0; i < score.midi.burst[i].action.num; i++) 
    if (is_note_on(score.midi.burst[i].action.event[i].command)) return(1);
  return(0);
}



float *
frame2spect(int frame)
{
  int i=0,n,low,high,mid,pos,j,lasta;
  float frame_in_secs,t;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];


  frame_in_secs = tokens2secs((float)frame-1);  // -1 is more accurate for unknown reason on Bosendorfer
  lasta = (mode == LIVE_MODE) ? cur_accomp-1 : last_accomp;
  //  printf("first_accomp = %d time = %f frameinsecs = %f\n",first_accomp,score.midi.burst[first_accomp].actual_secs,frame_in_secs);
  /*  if (mode == SYNTH_MODE) t = score.midi.burst[first_accomp].recorded_secs;
      else t = score.midi.burst[first_accomp].actual_secs;*/

  t = get_acc_event_time(first_accomp);


  //  printf("frame at %f first_accomp at %f last_accomp at %f\n",frame_in_secs,t,get_acc_event_time(lasta));

  if (frame_in_secs <= t) {
  //      printf("token = %d using restchord at start\n",token);
	return(restspect);
  }
  for (i=first_accomp+1; i<=lasta; i++) {
    /*    if (mode == SYNTH_MODE) t = score.midi.burst[i].recorded_secs;
	  else t = score.midi.burst[i].actual_secs;*/
    t = get_acc_event_time(i);
    if (t >= frame_in_secs) {
      //            printf("token = %d using chord %d\n",token,i-1);
      return(score.midi.burst[i-1].spect);
	}
  }
  //    printf("token = %d using chord %d\n",token,lasta);
	if (lasta < 0) {
	lasta = 0;
	//printf("remember to take care of this ....\n");
	 }
 // if (lasta < 0) {printf(" this should not be\n"); exit(0); }
  return(score.midi.burst[lasta].spect);
}


void
frame2accomp_index(int frame, int *index, float *secs)  {
  int i=0,n,low,high,mid,pos,j,lasta;
  float frame_in_secs,t;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];


  frame_in_secs = tokens2secs((float)frame-1);  // -1 is more accurate for unknown reason on Bosendorfer
  lasta = (mode == LIVE_MODE) ? cur_accomp-1 : last_accomp;
  //  printf("first_accomp = %d time = %f frameinsecs = %f\n",first_accomp,score.midi.burst[first_accomp].actual_secs,frame_in_secs);
  /*  if (mode == SYNTH_MODE) t = score.midi.burst[first_accomp].recorded_secs;
      else t = score.midi.burst[first_accomp].actual_secs;*/

  t = get_acc_event_time(first_accomp);


  //  printf("frame at %f first_accomp at %f last_accomp at %f\n",frame_in_secs,t,get_acc_event_time(lasta));

  if (frame_in_secs <= t) {
    //        printf("token = %d using restchord at start\n",token);
    *secs = 0;
    *index = -1;
    return;
  }
  for (i=first_accomp+1; i<=lasta; i++) {
    /*    if (mode == SYNTH_MODE) t = score.midi.burst[i].recorded_secs;
	  else t = score.midi.burst[i].actual_secs;*/
    t = get_acc_event_time(i);
    if (t >= frame_in_secs) {
      //            printf("token = %d using chord %d\n",token,i-1);
      *secs = frame_in_secs - get_acc_event_time(i-1);
      *index = i-1;
      return;
    }
  }
  //    printf("token = %d using chord %d\n",token,lasta);
  //  if (lasta < 0) { lasta = 0; printf("remember to take care of this ....\n");   }
  if (lasta < 0) {printf(" this should not be\n"); exit(0); }
  *index = lasta;
  *secs = frame_in_secs - get_acc_event_time(i-1);
  return;
}



float*
get_overlap_accomp_spect(int frame) {
  int index;
  float secs;

  frame2accomp_index(token, &index,&secs);
  if (index == -1) return(restspect);
  return((secs < .2) ? score.midi.burst[index].atck_spect : score.midi.burst[index].spect);
}



void
set_atckspect(int frame)
{
  int i=0,n,low,high,mid,pos,j,lasta;
  float frame_in_secs,t;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];


  frame_in_secs = frame*(TOKENLEN/(float)SR);
  /*#ifdef BOSENDORFER
  //  if (mode == LIVE_MODE) frame_in_secs - BOSENDORFER_LAG;
  #endif*/
  lasta = (mode == LIVE_MODE) ? cur_accomp-1 : last_accomp;
  //  printf("first_accomp = %d time = %f frameinsecs = %f\n",first_accomp,score.midi.burst[first_accomp].actual_secs,frame_in_secs);
  /*  if (mode == SYNTH_MODE) t = score.midi.burst[first_accomp].recorded_secs;
      else t = score.midi.burst[first_accomp].actual_secs;*/

  t = get_acc_event_time(first_accomp);


  //  printf("frame at %f first_accomp at %f last_accomp at %f\n",frame_in_secs,t,get_acc_event_time(lasta));

  if (frame_in_secs <= t) {
//        printf("token = %d using restchord at start\n",token);
	
	atck_spect = NULL;
	//	return(restspect);
  }
  for (i=first_accomp+1; i<=lasta; i++) {
    /*    if (mode == SYNTH_MODE) t = score.midi.burst[i].recorded_secs;
	  else t = score.midi.burst[i].actual_secs;*/
    t = get_acc_event_time(i);
    if (t >= frame_in_secs) {
      if (secs2tokens(t - frame_in_secs) < 3) atck_spect = score.midi.burst[i-1].atck_spect;
      else atck_spect = NULL;
    }
  }
  //    printf("token = %d using chord %d\n",token,lasta);
  //  if (lasta < 0) { lasta = 0; printf("remember to take care of this ....\n");   }
  if (lasta < 0) {printf(" this should not be\n"); exit(0); }
  atck_spect = NULL;
  //  return(score.midi.burst[lasta].spect);
}


float*
frame2atckspect(int frame)
{
  int i=0,n,low,high,mid,pos,j,lasta;
  float frame_in_secs,t,tk;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];


  frame_in_secs = frame*(TOKENLEN/(float)SR);
  /*#ifdef BOSENDORFER
  //  if (mode == LIVE_MODE) frame_in_secs - BOSENDORFER_LAG;
  #endif*/
  lasta = (mode == LIVE_MODE) ? cur_accomp-1 : last_accomp;
  //  printf("first_accomp = %d time = %f frameinsecs = %f\n",first_accomp,score.midi.burst[first_accomp].actual_secs,frame_in_secs);
  /*  if (mode == SYNTH_MODE) t = score.midi.burst[first_accomp].recorded_secs;
      else t = score.midi.burst[first_accomp].actual_secs;*/

  t = get_acc_event_time(first_accomp);


  //  printf("frame at %f first_accomp at %f last_accomp at %f\n",frame_in_secs,t,get_acc_event_time(lasta));

  if (frame_in_secs <= t) {
  //      printf("token = %d using restchord at start\n",token);
	return(restspect);
  }
  for (i=first_accomp+1; i<=lasta; i++) {
    /*    if (mode == SYNTH_MODE) t = score.midi.burst[i].recorded_secs;
	  else t = score.midi.burst[i].actual_secs;*/
    t = get_acc_event_time(i);
    if (t >= frame_in_secs) {
      tk = secs2tokens(get_acc_event_time(i-1));
      tk = secs2tokens(frame_in_secs - get_acc_event_time(i-1));
      //      if (tk < 3) 
      return(score.midi.burst[i-1].atck_spect);
    }
  }
  //    printf("token = %d using chord %d\n",token,lasta);
  //  if (lasta < 0) { lasta = 0; printf("remember to take care of this ....\n");   }
  if (lasta < 0) {printf(" this should not be\n"); exit(0); }
  return(score.midi.burst[lasta].atck_spect);
}




static SOUND_NUMBERS 
*sounding_accomp_guts(int frame)
{
  int i=0,n,low,high,mid,pos;
  float frame_in_secs;
  static SOUND_NUMBERS empty_sn;
  SOUND_NUMBERS *sn;
  MIDI_PART *p;
  char chord[100];
  


  if (mode == LIVE_MODE) {
    if (accomp_on_speakers == 0) return(&empty_sn);
    if (cur_accomp == first_accomp) return(&empty_sn);
    /*    sndnums2string(score.midi.burst[cur_accomp-1].snd_notes,chord);
	  printf("chord = %s\n",chord);*/
    return(&(score.midi.burst[cur_accomp-1].snd_notes));
  }

  frame_in_secs = frame*(TOKENLEN/(float)SR);
  p = &(score.midi);
  n = p->num; // total # of accomp events
  low = first_accomp;
  high = last_accomp;
  empty_sn.num = 0;
  if ( first_accomp == 0 && last_accomp == 0) 
    return(&empty_sn);
  if (frame_in_secs < p->burst[low].recorded_secs || frame_in_secs > p->burst[high].recorded_secs)
    {
      return(&empty_sn); 
    }  
  else {
    
    for (i=first_accomp+1; i<=last_accomp; i++) {
      if (frame_in_secs <= p->burst[i].recorded_secs) {
	return(&(p->burst[i-1].snd_notes));
      }
    }
    printf("time = %f first event = %f\n",frame_in_secs, p->burst[first_accomp].actual_secs);
    return(&empty_sn);
  }
}


SOUND_NUMBERS 
*sounding_accomp(int frame)
{
  SOUND_NUMBERS *sn;

  sn = sounding_accomp_guts(frame);
  //  if (frame%100 == 0) printf("frame = %d acc_notes = %d\n",frame,sn->num);

  init_sa_pitch_signature_table(sn); // I don't believe this is doing anything now.
  return(sn);
}


float 
time_to_last_accomp(int frame)  {
  /* searches for the accomp event before and returns the time difference between that event and that of the frame given; if frame is outside the range returns -1 */

  int i=0,j,k,m,n,low,high;
  float frame_in_secs,diff;
  MIDI_PART *p;
  MIDI_EVENT_LIST nt_evt;


  frame_in_secs = (frame+1)*(TOKENLEN/(float)SR);
  if (mode == LIVE_MODE) {
    if (cur_accomp == first_accomp) return(100.);
    m = cur_accomp-1;
  }
  else {
    frame_in_secs = (frame+1)*(TOKENLEN/(float)SR);
    p = &(score.midi);
    n = p->num; // total # of accomp events
    i = low = first_accomp;
    high = last_accomp;
    while (i /*<n*/ <= high && frame_in_secs > p->burst[i].actual_secs) i++; /* i has the index to the accomp event that happened after the frame given */
    if (i == /*0*/ low) return(100.);
    m=i-1;
  }
  for (; m >= 0; m--) {
    nt_evt = score.midi.burst[m].action;
    //    printf("m = %d nt_evt.num = %d\n",m,nt_evt.num);
    for (j=0; j < nt_evt.num; j++)
      if ((nt_evt.event[j].command & 0xf0) == NOTE_ON) {
      //   if ((score.midi.burst[m].action.event[j].command & 0xf0) == NOTE_ON) {
	//printf("last_accomp = %f\n", score.midi.burst[m].actual_secs);
	//	printf("frame = %d m = %d frame_in_secs = %f actual_secs = %f\n",token,m,frame_in_secs, score.midi.burst[m].actual_secs); fflush(stdout);
	diff = frame_in_secs - score.midi.burst[m].actual_secs;
	if (diff < .01) diff = .01; 
	// due to difference between now() and frame_in_secs small error can occur
	// need to have a positive value or numerical problem in computing meld for piano spect
	return(diff); 
      }
  }
  return(100.);
  //    exit(0);
}
 


void 
last_attack_desc(int frame, float *secs, float **spect) {
  /* searches for the accomp event before and returns the time difference between that event and that of the frame given; if frame is outside the range returns -1 */

  int i=0,j,k,m,n,low,high;
  float frame_in_secs;
  MIDI_PART *p;
  MIDI_EVENT_LIST nt_evt;


  frame_in_secs = (frame+1)*(TOKENLEN/(float)SR);
  if (mode == LIVE_MODE) {
    if (cur_accomp == first_accomp) { *secs = 100.; *spect = restspect; return; }
    else m = cur_accomp-1; 
  }
  else {
    frame_in_secs = (frame+1)*(TOKENLEN/(float)SR);
    p = &(score.midi);
    n = p->num; // total # of accomp events
    i = low = first_accomp;
    high = last_accomp;
    while (i /*<n*/ <= high && frame_in_secs > p->burst[i].actual_secs) i++; /* i has the index to the accomp event that happened after the frame given */
    if (i == /*0*/ low) { *secs = 100.; *spect = restspect; return; }
    m=i-1;
  }
  for (; m >= 0; m--) {
    nt_evt = score.midi.burst[m].action;
    //    printf("m = %d nt_evt.num = %d\n",m,nt_evt.num);
    for (j=0; j < nt_evt.num; j++)
      if ((nt_evt.event[j].command & 0xf0) == NOTE_ON) {
      //   if ((score.midi.burst[m].action.event[j].command & 0xf0) == NOTE_ON) {
	//printf("last_accomp = %f\n", score.midi.burst[m].actual_secs);
	*secs = frame_in_secs - score.midi.burst[m].actual_secs;
	*spect = score.midi.burst[m].atck_spect;
	return;
      }
  }
  *secs = 100.; *spect = restspect;
}
 






#define HARM_WIDTH 10  /* both of these should depend on resolutions */
#define HARM_STD 4. //2.
#define BASELINE 1. 
#define MAX_HARMONICS 16 //4 //4 //8
#define HALF_STEP_STD  .0002
#define HALF_STEP_RAT 1.059463
#define STD_CNST  .015 //.02
#define MIN_STD_VAL   1.
#define EXP_HARM_DECAY 1. //.7





void
working_gauss_mixture_model(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,total=0,sigma/*,midi2omega()*/,decay/*,hz2omega()*/;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = BASELINE/freqs;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 

    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      sigma = f* STD_CNST;
      //      sigma = .01 + f*.005;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] += /*((midi > 60) ? 3 : 1)* */  ((attack) ? 4 : 1) * 
	  	  decay*exp(-.5*x*x)/sigma;
	  //	  exp(-k*.1)* decay*exp(-.5*x*x)/sigma;
      }
    }
  }
        for (k=0; k < freqs; k++) total += spect[k];
        for (k=0; k < freqs; k++) spect[k]/= total; 
      /* this issue not resolved yet.  without normalization I get inaccurate
detection of high notes.  better probably would be only to normalize when there
is some notes in the chord */
}

#ifdef JAN_BERAN
  #define A_STD_CONST .02//.005 
#else
  #define A_STD_CONST .01//.005 
#endif

#define VOICE_A_STD_CONST  .05//.005 
  /* this change on 2-1-04 was missing last note on adagio
     of mozart 4tet due to tuning */
#define B_STD_CONST  .01

#define FREQ_DECAY  /*(-.01)*/ .01


static float
get_a_std_const() {
  if (strcmp(instrument,"voice") == 0) { return(VOICE_A_STD_CONST); }
  return(A_STD_CONST);
}


void
gauss_mixture_model(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,total=0,sigma/*,midi2omega()*/,decay/*,hz2omega()*/;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 

    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      //      sigma = f* A_STD_CONST + B_STD_CONST;
      sigma = f* A_STD_CONST + B_STD_CONST;
      //      sigma = .01 + f*.005;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] += /*((midi > 60) ? 3 : 1)* */  ((attack) ? 4 : 1) * 
	  //	  	  decay*exp(-.5*x*x)/sigma;
	  	  exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;
      }
    }
  }
  if (notes.num) {
	for (k=0; k < freqs; k++) total += spect[k];
	if (total <= 0)  { total = freqs; for (k=0; k < freqs; k++) spect[k] = 1;  }
	for (k=0; k < freqs; k++) spect[k]/= total; 
  }
  //  for (k=0; k < freqs; k++) spect[k] += .01/freqs;
      /* this issue not resolved yet.  without normalization I get inaccurate
detection of high notes.  better probably would be only to normalize when there
is some notes in the chord */
}


//#define MAX_NOT_SUM


//#define MAX_NOT_SUM

static float
dbeta(float x, float p, float s) {  // p is center s is shape 
  float a,b,r;
  
  a = s*p;
  b = s*(1-p);

  if (x <= 0 || x >= 1) return(0);
  return(pow(x,a-1)*pow(1-x,b-1));
}

void
piano_mixture_model(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,s,total=0,sigma/*,midi2omega()*/,decay/*,hz2omega()*/,age;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    age = notes.age[i];
    decay = exp(-2*age);
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 

    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      if (f > freqs) break;
      sigma = f* A_STD_CONST + B_STD_CONST;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	s = k / (float) freqs;
#ifdef PROFILE_EXPERIMENT
	//	spect[k] += decay*dbeta(s,.1,30.)*exp(-.5*x*x)/sigma;
	        spect[k] += decay*dbeta(s,.1,15.)*exp(-.5*x*x)/sigma;
#else
	spect[k] += decay*exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;
#endif
      }
    }
  }
  if (notes.num) {
    for (k=0; k < freqs; k++) total += spect[k];
    total /= notes.num;  // sum to notes.num
    for (k=0; k < freqs; k++) spect[k]/= total; 
  }
}

void
atck_piano_mixture_model(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,total=0,sigma/*,midi2omega()*/,decay/*,hz2omega()*/;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    if (notes.attack[i] == 0) continue;
    
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 
    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      if (f > freqs) break;
      sigma = f* A_STD_CONST + B_STD_CONST;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] += exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;
      }
    }
  }
  if (notes.num) {
    for (k=0; k < freqs; k++) total += spect[k];
    if (total == 0) return;  //
    for (k=0; k < freqs; k++) spect[k]/= total; 
  }
}


//#define MAX_NOT_SUM



// xxxx

void
solo_gauss_mixture_model(SOUND_NUMBERS notes, float *spect, int *peaks) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,s,total=0,sigma,/*midi2omega(),*/decay,a_std_const/*,hz2omega()*/;
  extern float restspect[];

  *peaks = 0;
  a_std_const = get_a_std_const();
  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 

    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      //      sigma = f* A_STD_CONST + B_STD_CONST;
      sigma = f* a_std_const + B_STD_CONST;
      //         sigma = .01 + f*.025;  // widening peaks ...

      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      //            mink = (int) (f - HARM_WIDTH);
      mink = (int) (f - 4*sigma);
      if (mink < 0) mink = 0;
      //            maxk = (int) (f + HARM_WIDTH);
      maxk = (int) (f + 4*sigma);
      if (maxk > freqs) maxk = freqs;
      if (mink < freqs) (*peaks)++;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	s = k/(float) freqs;
#ifdef PROFILE_EXPERIMENT
	spect[k] += dbeta(s,.25 /*.35*/,15.)*exp(-.5*x*x)/sigma;
#else
	spect[k] += ((attack) ? 4 : 1) *  exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;
#endif




	/*	spect[k] += ((attack) ? 4 : 1) * 
	  //	  	  decay*exp(-.5*x*x)/sigma;
	  exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;*/
      }
    }
  }
  if (notes.num) {
#ifdef MAX_NOT_SUM
    for (k=0,total=0; k < freqs; k++) if ( spect[k] > total) total = spect[k];
#else    
    for (k=0; k < freqs; k++) total += spect[k];
#endif
    for (k=0; k < freqs; k++) spect[k]/= total; 
  }
  //  for (k=0; k < freqs; k++) spect[k] += .01/freqs;
      /* this issue not resolved yet.  without normalization I get inaccurate
detection of high notes.  better probably would be only to normalize when there
is some notes in the chord */
}

#define TUNE_STD_CONST  2.

void
solo_gauss_mixture_model_tune(SOUND_NUMBERS notes, float tune, float *spect) {  // tune in 1/2 steps
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,total=0,sigma,/*midi2omega(),*/decay,a_std_const/*,hz2omega()*/;
  extern float restspect[];

  a_std_const = get_a_std_const();
  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    omega = midi2omega((float) midi+tune); 
    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      sigma = TUNE_STD_CONST;
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] +=  exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;
      }
    }
  }
  if (notes.num) {
    for (k=0; k < freqs; k++) total += spect[k];
    for (k=0; k < freqs; k++) spect[k]/= total; 
  }
}


#define ATTACK_STD_CONST  (10*A_STD_CONST)


// xxxx

void
gauss_mixture_model_attack(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack,has_attack=0;
  float omega,f,x,total=0,sigma/*,midi2omega()*/,decay/*,hz2omega()*/;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) { 
    attack = notes.attack[i];
    //    printf("pitch = %d attack = %d\n",midi,attack);
    if (attack == 0) continue;
    has_attack = 1;
    midi = notes.snd_nums[i];
    omega = midi2omega((float) midi); 

    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      if (f > freqs) break;
      sigma = f* ATTACK_STD_CONST + 5*B_STD_CONST;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] +=  exp(-k*FREQ_DECAY)*exp(-.5*x*x)/sigma;
      }
    }
  }
  if (has_attack) {
    for (k=0; k < freqs; k++) total += spect[k];
    for (k=0; k < freqs; k++) spect[k]/= total; 
  }
}


void
gauss_accomp_mixture_model(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,total=0,sigma/*,midi2omega()*/,decay/*,hz2omega()*/;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = 0;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 

    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      sigma = f* A_STD_CONST + B_STD_CONST;
      //      sigma = .01 + f*.005;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] += /*((midi > 60) ? 3 : 1)* */  ((attack) ? 4 : 1) * 
	  //	  	  decay*exp(-.5*x*x)/sigma;
	  	  exp(-k*6*FREQ_DECAY)*exp(-.5*x*x)/sigma;
      }
    }
  }
  if (notes.num) {
    for (k=0; k < freqs; k++) total += spect[k];
    for (k=0; k < freqs; k++) spect[k]/= total; 
  }
  //  for (k=0; k < freqs; k++) spect[k] += .01/freqs;
      /* this issue not resolved yet.  without normalization I get inaccurate
detection of high notes.  better probably would be only to normalize when there
is some notes in the chord */
}

/*
#define AMP_HALF_LIFE .2

void
gauss_mixture_model_attack(SOUND_NUMBERS notes, float *spect, float secs) {
  int i,midi,h,k,mink,maxk,attack,vel;
  float omega,f,x,total=0,sigma,midi2omega(),decay,age,amp;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = BASELINE/freqs;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    vel = notes.vel[i];
    age= notes.age[i] + secs;
    amp = vel*pow(.5,age/AMP_HALF_LIFE);
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 
    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      sigma = f* STD_CNST;
      //      sigma = .01 + f*.005;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] +=  amp*decay*exp(-.5*x*x)/sigma;
      }
    }
  }
  //  for (k=0; k < freqs; k++) total += spect[k];
  //  for (k=0; k < freqs; k++) spect[k]/= total;
}

*/


void
superpos_model(SOUND_NUMBERS notes, float *spect) {
  int i,midi,h,k,mink,maxk,attack;
  float omega,f,x,total=0,sigma/*,midi2omega()*/,decay;
  extern float restspect[];

  for (k=0; k < freqs; k++) spect[k] = BASELINE/freqs;
  for (i=0; i < notes.num; i++) {
    midi = notes.snd_nums[i];
    attack = notes.attack[i];
    //    printf("pitch = %d attack = %d\n",midi,attack);
    omega = midi2omega((float) midi); 
    for (h=1; h <= MAX_HARMONICS; h++) {
      f = (int) (h*omega + .5);
      decay = pow(EXP_HARM_DECAY,(float) h);
      if (f > freqs) break;
      sigma = f* STD_CNST;
      //      sigma = .01 + f*.005;
      sigma = (sigma > MIN_STD_VAL) ? sigma : MIN_STD_VAL;
      
      mink = (int) (f - HARM_WIDTH);
      if (mink < 0) mink = 0;
      maxk = (int) (f + HARM_WIDTH);
      if (maxk > freqs) maxk = freqs;
      for (k= mink; k < maxk; k++) {
	if (k >= freqs) break;
	x = (k-f)/sigma;
	spect[k] += /*((midi > 60) ? 3 : 1)* */  ((attack) ? 4 : 1) * 
	  decay*exp(-.5*x*x)/sigma;
      }
    }
  }
  for (k=0; k < freqs; k++) total += spect[k];
  for (k=0; k < freqs; k++) spect[k]/= total;
  for (k=0; k < freqs; k++) spect[k] = log(spect[k]);
  //  for (k=0; k < freqs; k++) printf("%d %d %f %f\n",notes.num,k,spect[k],restspect[k]);
  
}


#ifdef VIOLIN_EXPERIMENT
  #define SCALING_FACT   1.0
#else
#define SCALING_FACT   1.  /* .7 */
#endif




float
spect_prob(float *s) {
  int i,t;
  float norm,total,ss[FREQDIM/2];


  norm = 0;
  for (i=freq_cutoff; i < freqs; i++) {
    ss[i] = (spect[i] < MIN_SPECT_VAL) ? MIN_SPECT_VAL : spect[i];
    norm += ss[i];
  }
  for (i=freq_cutoff; i < freqs; i++) ss[i] /= norm;
  total = 0;
  for (i=freq_cutoff; i < freqs; i++) {
    total += ss[i]*s[i];
    //           printf("%d %f %f\n",i,s[i],ss[i]);
  }
  //    printf("total = %f prob = %f\n",total,exp(SCALING_FACT*total));
  return(exp(SCALING_FACT*total));
}


float
spect_like(float *model) {  /* stored model (not log !!) */
  int i,t;
  float norm,total,data[FREQDIM/2];  /* spectrogram data */

  norm = 0;
  for (i=freq_cutoff; i < freqs; i++) {
    data[i] = (spect[i] < MIN_SPECT_VAL) ? MIN_SPECT_VAL : spect[i];
    norm += data[i];
  }
  if (norm > 0) for (i=freq_cutoff; i < freqs; i++) data[i] /= norm;  /* normalizing for loudness */


  //  for (i=freq_cutoff; i < freqs; i++) printf("%d %f\n",i,data[i]); exit(0);


  //  else for (i=freq_cutoff; i < freqs; i++) data[i] = 1/ (float) freqs;  // some audio gives runs of 0 data
  total = 0;
  for (i=freq_cutoff; i < freqs; i++)  {
    //    printf("model[%d] = %f\n",i,model[i]);
    //    if (model[i] <= 0 || data[i] <= 0) { printf("%d %f %f\n",i,model[i],data[i]); exit(0); }
    if (model[i] <= 0) { 
      printf("bad log %f %f norm = %f\n",data[i],model[i],norm); exit(0); 
    }
    total += data[i]*(log(model[i])/*-log(data[i])*/);   /* c log(p) */
    //      printf("%d %f %f\n",i,data[i],model[i]);
  }
  //  printf("total = %f spect[10] = %f model[10] = %f\n",total,spect[10],model[10]);
  return(exp(SCALING_FACT*total));
}

float
spect_like_mask(float *model) {  
  int i,t;
  float model_sum,spect_sum,total,data[FREQDIM/2];  /* spectrogram data */

  model_sum = spect_sum = 0;
  for (i=0;  i < freqs; i++) {
    if (hyp_mask[i])  spect_sum += spect[i];
    if (hyp_mask[i])  model_sum += model[i];
  }
  if (spect_sum == 0) { printf(" spect has zero energy after masking\n"); return(1.);}
  if (model_sum == 0) { printf(" model has zero energy after masking\n"); exit(0); }
  total = 0;
  for (i=0; i < freqs; i++)  {
        if (hyp_mask[i] == 0) continue;
    //    printf("model[%d] = %f\n",i,model[i]);
    //    if (model[i] <= 0 || data[i] <= 0) { printf("%d %f %f\n",i,model[i],data[i]); exit(0); }
    if (model[i] <= 0) { 
      printf("bad log %f %f norm = %f\n",data[i],model[i],spect_sum); exit(0); 
    }
    total += (spect[i]/spect_sum) * log(model[i]/model_sum);
    //      printf("%d %f %f\n",i,data[i],model[i]);
  }
  //  printf("total = %f spect[10] = %f model[10] = %f\n",total,spect[10],model[10]);
  return(exp(1.*total));
}



float
spect_like_test(float *model) {  /* stored model (not log !!) */
  int i,t;
  float norm,total,data[FREQDIM/2];  /* spectrogram data */

  norm = 0;
  for (i=freq_cutoff; i < freqs; i++) {
    data[i] = (spect[i] < MIN_SPECT_VAL) ? MIN_SPECT_VAL : spect[i];
    norm += data[i];
  }
  for (i=freq_cutoff; i < freqs; i++) data[i] /= norm;  /* normalizing for loudness */
  total = 0;
  for (i=freq_cutoff; i < freqs; i++)  {
    //    printf("model[%d] = %f\n",i,model[i]);
    total += data[i]*(log(model[i])-log(data[i]));   /* c log(p) */
    //    printf("%d total = %f increment = %f model = %f data = %f\n",i,total, data[i]*(log(model[i])-log(data[i])),model[i],data[i]);  
    printf("%d model = %f data = %f\n",i,model[i],data[i]);  
  }
  printf("total = %f spect[10] = %f model[10] = %f\n",total,spect[10],model[10]);

  return(exp(SCALING_FACT*total));
}


#define BASEVAL 1.
#define BACKGROUND_RATIO   .1


void
meld_spects(float *s1, float *s2, float *sout, float p) {
  int i;
  float q,total = 0,b,d;

  q = 1-p;

  /*    b = BASEVAL/freqs;
    for (i=0; i < freqs; i++) total += sout[i] = p*s1[i] + q*s2[i] + b;
    for (i=0; i < freqs; i++) sout[i] /= total;*/

  

  /*  for (i=0; i < freqs; i++) if (s1[i] < 0 || s2[i] < 0) {
    printf("bad input to meld_spects %f %f\n",s1[i],s2[i]);
    exit(0);
    }*/
    for (i=0; i < freqs; i++) total += sout[i] = p*s1[i] + q*s2[i];
  if (total == 0)  {
    b = 1./freqs;
    for (i=0; i < freqs; i++) sout[i] = b;
    return;
  }
  for (i=0; i < freqs; i++) sout[i] /= total;
  b = BACKGROUND_RATIO/freqs;
  d = 1 + BACKGROUND_RATIO;
  for (i=0; i < freqs; i++) sout[i] = (sout[i] + b)/d ;




  /*  for (i=0; i < freqs; i++) if (sout[i] < 0) { 
    printf("%d %f\n",i,s1[i]); 
    exit(0); } */
}

void
simple_meld_spects(float *s1, float *s2, float *sout, float p) {
  int i;
  float q,total = 0,b,d;

  q = 1-p;
  for (i=0; i < freqs; i++) total += sout[i] = p*s1[i] + q*s2[i];
  if (total > 0) for (i=0; i < freqs; i++) sout[i] /= total;
  /*  for (i=0; i < freqs; i++) if (sout[i] <= 0){
    for (i=0; i < freqs; i++) printf("%f %f %f\n",s1[i],s2[i],sout[i]);
    exit(0);
    }*/
}





#define MELD_PROB .8


#define MELD_EXPT  // considering this for violin 

static float spect_like_table[MAX_SOLO_NOTES+1];
static float meld_spect_like_table[MAX_SOLO_NOTES+1][MELD_LEN];
#ifdef SPECT_TRAIN_EXPT
  static float train_spect_like_table[MAX_SOLO_NOTES][SPECT_TRAIN_NUM];
#endif
static float rest_like; 






void
clear_spect_like_table() {
  int i,j;


  for (i= firstnote; i <= lastnote; i++)   {
    spect_like_table[i] = PROB_UNSET;
#ifdef MELD_EXPT
    for (j=0; j < MELD_LEN; j++) meld_spect_like_table[i][j] = PROB_UNSET;
#endif
#ifdef SPECT_TRAIN_EXPT
    for (j=0; j < SPECT_TRAIN_NUM; j++) train_spect_like_table[i][j] = PROB_UNSET;
#endif
  }
  rest_like = PROB_UNSET;
  spect_like_table[MAX_SOLO_NOTES] = PROB_UNSET;


#ifdef ATTACK_SPECT
  for (i= firstnote; i <= lastnote; i++)  attack_spect_like_table[i] = PROB_UNSET;
  attack_rest_like = PROB_UNSET;
#endif

}

float
old_table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,meld[FREQDIM/2],p;
  int i;


    i =   (solo_index >= firstnote && solo_index <= lastnote) ? solo_index  : MAX_SOLO_NOTES;
    //    printf("solo_index = %d firstnote = %d lastnote = %d\n",solo_index,firstnote,lastnote);

  p = spect_like_table[i];
  if (p != PROB_UNSET) return(p);


  if (solo_index >= firstnote && solo_index <= lastnote /* && sas.shade != REST_STATE*/ ) 
    solo_spect = score.solo.note[solo_index].spect;


  else solo_spect = restspect;
  //            meld_spects(solo_spect, acc_spect, meld, MELD_PROB);
  meld_spects(solo_spect, orchestra_spect, meld, /*.9*/ .7);
  //  for (i=0; i < freqs; i++) printf("%d %f\n",i,meld[i]);
  p = spect_like_table[solo_index] = spect_like(meld);
  //  printf("index = %d p = %f\n",solo_index,p);
  return(p);
}

#ifdef ORCHESTRA_EXPERIMENT
  #define MELD_CONST .9
#else
  #define MELD_CONST .9  /*.99 should really mess up but it doesn't ?? */
#endif

void
get_spect_hyp(float *acc_spect, SA_STATE sas, float *meld) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,p;
  int i;


  solo_spect = (sas.shade == REST_STATE) ? restspect : score.solo.note[sas.index].spect;
  meld_spects(solo_spect, orchestra_spect, meld,MELD_CONST/*.7*/);
}

void
get_acc_spect_hyp(float *acc_spect, SA_STATE sas, float *meld) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,p,t;
  int i;


  solo_spect = (sas.shade == REST_STATE) ? restspect : score.solo.note[sas.index].spect;
  meld_spects(solo_spect, acc_spect, meld,MELD_CONST /*.9*/);
  /*  t=0;
    for (i=0; i < freqs; i++) {
    printf("%f %f\n",solo_spect[i],acc_spect[i]);
    }*/
}




#define POISSON_FACT (10./freqs)
#define MAX_BASIS 8
#define POISSON_DR  .3 // .1 //1  // control Poisson dynamic range

static float
poisson_like(float *q) {
  /* the basic model is that the spect components are indepedent Poisson rvs with
     s_i \sim Poiss(q_i).   This model can be factored as
     
     p(s|q) = p(\sum s | q) p(s | \sum s q)

     where the first factor \sum s \sim Poiss(\sum q)
     and the second factor is multinomial with probs q/\sum q

     The logs of these factors are scaled in a data-dependent way to control
     the dynamic range.  The second factor is scaled by assuming \sum s = 1.
     The first factor is scaled by assuming that  \sum s = POISSON_DR.  
     and \sum q is scaled by the same constant.

     In the case where solo note are entertained \sum q should be nearly
     or exactly constant over differnt models so the poisson term doesn't
     contribute to the log likelihood and the model is exactly the old
     multinomial model */

  int i;
  float t1=0,t2=0,ss=0,qs=0;

  for (i=1; i < freqs; i++) {
    ss += spect[i];
    qs += q[i];
    if (q[i] <= 0) { printf("bad value for log in possion_like-\n");  exit(0); }
  }
  for (i=1; i < freqs; i++) t1 += spect[i]*log(q[i]/qs);  // the multinomial part
  t1 /= ss;  // scaled to control dynamic range
  qs *= POISSON_DR/ss;  /* scale ss and qs so that ss = POISSON_DR */
  ss = POISSON_DR;
  t2 = -qs + ss*log(qs);  // the poisson part (can't find anything that helps here)
  //  printf("t1 = %f t2 = %f\n",t1,t2);
  //  printf("poisson t = %f\n",t);
  
  return(exp(t1+t2));
}



static float
opt_poisson_like(int n, float *w, float **q, float *lb, float *ub) {
  /*     one iter of em and then compute poisson log like */
  float model[FREQDIM/2],t,acc[MAX_BASIS],sum[MAX_BASIS],p;
  int i,j,k;


  /*    for (j=0; j < n; j++) {
  for (i=t = 0; i < freqs; i++)  t += q[j][i];
  for (i=0; i < freqs; i++)   q[j][i] /= t;
  }*/
  /*    for (i=0; i < freqs; i++) for (j=0,spect[i]=0; j < n; j++) 
	spect[i] += (j+1)*q[j][i];*/


  
  if (n > MAX_BASIS) { printf("out of range in opt_poission_like\n"); exit(0); }
  

  //  printf("before %d weights are:",k);
  //  for (j=0; j < n; j++) printf("%f\t",w[j]);
  //  printf("\n");

  for (j=0; j < n; j++) sum[j] = acc[j] = 0;
  for (i=0; i < freqs; i++) {
    for (j=0; j < n; j++) sum[j] += q[j][i];  // would be better if didn't compute this every time
    for (j=0,t=0; j < n; j++)  t += w[j]*q[j][i];
    for (j=0; j < n; j++)  acc[j] += spect[i]*w[j]*q[j][i]/t;
  }
  for (j=0; j < n; j++) {
    w[j] = acc[j]/sum[j];
    if (w[j] < lb[j]) w[j] = lb[j];
    if (w[j] > ub[j]) w[j] = ub[j];
  }
  //  for (j=0; j < n; j++) printf("sum %d = %f\n",j,q[1][j]/*sum[j]*/);

  for (i=0; i < freqs; i++) for (j=0,model[i]=0; j < n; j++)  model[i] += w[j]*q[j][i];
  p = poisson_like(model);
  //  for (i=0; i < freqs; i++) printf("%f ",/*model[i]*/q[0][i]);
  //    for (i=0; i < freqs; i++) printf("%f ",model[i]);

 
  return(p);
}



static float
simple_opt_spect_like(int n, float *w, float **q) {
  /*     one iter of em and then compute spect_like */
  float model[FREQDIM/2],t,acc[MAX_BASIS],sum[MAX_BASIS],p;
  int i,j,k;


  if (n > MAX_BASIS) { printf("out of range in opt_poission_like\n"); exit(0); }
  
  for (i=0; i < freqs; i++) for (j=0,model[i]=0; j < n; j++)  model[i] += w[j]*q[j][i];
  p = spect_like(model);
  printf("before em w[0] = %f w[1] = %f prob = %f\n",w[0],w[1],p);

  //  printf("before %d weights are:",k);
  //  for (j=0; j < n; j++) printf("%f\t",w[j]);
  //  printf("\n");

  for (j=0; j < n; j++) sum[j] = acc[j] = 0;
  for (i=0; i < freqs; i++) {
    for (j=0; j < n; j++)  sum[j] += q[j][i];
    for (j=0,t=0; j < n; j++)  t += w[j]*q[j][i];
    for (j=0; j < n; j++)  acc[j] += spect[i]*w[j]*q[j][i]/t;
  }
  printf("sum[0] = %f sum[1] = %f\n",sum[0],sum[1]);
  for (t=j=0; j < n; j++) t += acc[j];
  for (j=0; j < n; j++) w[j] = acc[j] / t;

  for (i=0; i < freqs; i++) for (j=0,model[i]=0; j < n; j++)  model[i] += w[j]*q[j][i];
  p = spect_like(model);

  printf("after em w[0] = %f w[1] = %f prob = %f\n",w[0],w[1],p);
  //  for (i=0; i < freqs; i++) printf("%f ",/*model[i]*/q[0][i]);
  //    for (i=0; i < freqs; i++) printf("%f ",model[i]);
  return(p);
}



static void
simple_opt_meld(int n, float *w, float **q, float *meld) {
  /*     one iter of em and then compute spect_like */
  float t,acc[MAX_BASIS],sum[MAX_BASIS],p;
  int i,j,k;


  if (n > MAX_BASIS) { printf("out of range in opt_poission_like\n"); exit(0); }
  
  for (i=0; i < freqs; i++) for (j=0,meld[i]=0; j < n; j++)  meld[i] += w[j]*q[j][i];
  //  p = spect_like(meld);
  //  printf("before em w[0] = %f w[1] = %f prob = %f\n",w[0],w[1],p);

  //  printf("before %d weights are:",k);
  //  for (j=0; j < n; j++) printf("%f\t",w[j]);
  //  printf("\n");

  for (j=0; j < n; j++) sum[j] = acc[j] = 0;
  for (i=0; i < freqs; i++) {
    for (j=0; j < n; j++)  sum[j] += q[j][i];
    for (j=0,t=0; j < n; j++)  t += w[j]*q[j][i];
    for (j=0; j < n; j++)  acc[j] += spect[i]*w[j]*q[j][i]/t;
  }
  //  printf("sum[0] = %f sum[1] = %f\n",sum[0],sum[1]);
  for (t=j=0; j < n; j++) t += acc[j];
  if (t <= 0) {
    for (i=0; i < freqs; i++)  meld[i] = 1 / (float) freqs;
    return;
  }

  for (j=0; j < n; j++) w[j] = acc[j] / t;
  for (i=0; i < freqs; i++) for (j=0,meld[i]=0; j < n; j++)  meld[i] += w[j]*q[j][i];
}






float
poisson_table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,meld[FREQDIM/2],p,*mix,t,*mem,*q[MAX_BASIS],w[MAX_BASIS],lb[MAX_BASIS],ub[MAX_BASIS],sum=0,x,mx,ssum;
  int i,n=0,j;



  if (sas.shade == REST_STATE)   { 
    mem = &rest_like;  
    solo_spect = restspect; 
  }
  else if (sas.pos < 2)   { 
    mem = &(meld_spect_like_table[sas.index][0]); 
    solo_spect = score.solo.note[sas.index].meld_spect[0]; 
  }
  else { 
    mem = &(spect_like_table[sas.index]); 
    solo_spect = score.solo.note[sas.index].spect; 
  }

  p = *mem;
  if (p != PROB_UNSET) return(p);

  
  /* renormalizing solo spectra */


  for (i=0,mx=0; i < freqs; i++) if (mx < solo_spect[i]) mx = solo_spect[i];  
  

/*  for (i=0,mx=0; i < freqs; i++) if (mx < solo_spect[i]) mx = solo_spect[i];  
  if (mx > 0)   for (i=0; i < freqs; i++) solo_spect[i] /= mx;
  for (i=0,ssum=0; i < freqs; i++) ssum += solo_spect[i];  */
  

  for (i=1; i < freqs; i++) sum += spect[i]; // could make this global so don't need to keep computing it.
  
  //  q[n] = ambient; w[n] = 1; lb[n] = .5; ub[n] = 2;  n++;

  q[n] = background; w[n] = .05*sum ; lb[n] = 0*sum; ub[n] = 1.*sum;  n++;
  //  if (accomp_on_speakers)  { q[n] = acc_spect;  w[n] = .5*sum; lb[n] = 0; ub[n] = HUGE_VAL; n++; }


  if (accomp_on_speakers)  { q[n] = orchestra_spect;  w[n] = .05*sum; lb[n] = .0*sum; ub[n] = 1*sum; n++; }
  //  if (accomp_on_speakers)  { q[n] = unscaled_orchestra_spect;  w[n] = 1.; lb[n] = .01; ub[n] = 10.; n++; }
  if (is_solo_rest(sas.index) == 0 && sas.shade != REST_STATE) { 
  q[n] = solo_spect;  w[n] = .65*sum; lb[n] = .3*sum; ub[n] = 1*sum; n++; 


  /*  if (accomp_on_speakers)  { q[n] = orchestra_spect;  w[n] = .35*sum; lb[n] = .01*sum; ub[n] = .3*sum; n++; }
  if (is_solo_rest(sas.index) == 0 && sas.shade != REST_STATE) { 
  q[n] = solo_spect;  w[n] = .65*sum; lb[n] = .3*sum; ub[n] = HUGE_VAL; n++; */






    // if the proportion of solo is allowed to be too low, then rests looks reasonable under solo (bad)
    /*    if (sas.pos < 2)   { 
      q[n] = score.solo.note[sas.index-1].spect;
      w[n] = .8*sum; lb[n] = 0; ub[n] = HUGE_VAL; n++; 
      }*/
	/*    for (j=0; j < TUNE_LEN; j++) { 
      q[n] = score.solo.note[sas.index].tune_spect[j];  
      w[n] = .8*sum; lb[n] = 0; ub[n] = HUGE_VAL; n++; 
      }*/
  }
    p = opt_poisson_like(n, w, q, lb,ub);

    //    printf("%f\t%d\t%f\t%d\t%f\t",p,sas.index,sum,score.solo.note[sas.index].snd_notes.snd_nums[0],ssum);
    /*    for (j=0; j < 5; j++) {
      x = (j >= n) ? 0 : w[j];
            printf("%f\t",x);
    }
    printf("\n");*/

  /*  mix = (accomp_on_speakers) ? orchestra_spect : solo_spect;
  if (is_solo_rest(sas.index) || sas.shade == REST_STATE)   // added || sas.shad ... 12-06
    meld_solo_orch_back(mix, mix, background, meld);
  else   meld_solo_orch_back(solo_spect, mix, background, meld);
  p = spect_like(meld);*/

  /*  if (token >= 9436 && token <= 9436) {
    printf("frame = %d note = %s state = %d prob = %f\n",token,score.solo.note[solo_index].observable_tag,(REST_STATE ==sas.shade),p);
      //        spect_like_test(mix);
    if (sas.shade != REST_STATE) for (i=0; i < freqs; i++) printf("%d %f %f %f %f %f %f\n",i,mix[i],background[i],meld[i],solo_spect[i],spect[i],spect[i]*log(meld[i]));
      
      }    
  */


  *mem = p; 
  return(p);
}

static float
solo_proportion(int index) {
  float p;

  p = .15*score.solo.note[index].peaks; 
  //  if (p > .8) p = .8;
    if (p > .7) p = .7;
  //  if (score.solo.note[index].peaks <= 2) return(.3); 
  //  return(.65);
  return(p);
}

void
arb_meld(int n, float *p, float **q, float *meld) {
  int i,j;
  float t=0;

  for (i=0; i < freqs; i++) {
    meld[i] = 0;
    for (j=0; j < n; j++) meld[i] += p[j]*q[j][i];
  }
  for (i=0; i < freqs; i++) t += meld[i];
  if (t == 0) return;
  for (i=0; i < freqs; i++)  meld[i] /= t;
}



static void
opt_spect_meld(int n, float *p, float **q, float *meld) {
  int i,j,k,dex[MAX_BASIS],*index;
  float grad_buff[MAX_BASIS],hess_ptr[MAX_BASIS],
    hess_buff[MAX_BASIS*MAX_BASIS],*v,**H,A[FREQDIM/2],t,s;
  double d;

  arb_meld(n,p,q,meld);
  //  printf("init like = %f n = %d (%f %f %f %f)\n",plain_like(meld),n, p[0],p[1],p[2],p[3]);

  index = dex-1;
  v = grad_buff-1;  // v[1...n] is gradient if log likelihood
  H = hess_ptr-1;  // H[1...n][1...n] is Hessian  matrix of log likelihood
  for (i=1; i <= n; i++) H[i] = hess_buff + (i-1)*n -1;
  for (i=0; i < freqs; i++) {
    A[i] = 0;
    for (j=0; j < n; j++) A[i] += p[j]*q[j][i];  // A[i] used in grad and Hess calc
  }
  for (j=0; j < n; j++) {
    v[j+1] = 0;
    for (i=0; i < freqs; i++) v[j+1] += spect[i]*q[j][i]/A[i];   // compute gradient
  }
  for (j=0; j < n; j++) {
    for (k=0; k < n; k++) {
      H[j+1][k+1] = 0;
      for (i=0; i < freqs; i++) 
	H[j+1][k+1] -= spect[i]* (q[j][i]*q[k][i])/(A[i]*A[i]);   // compute Hessian
    }
  }




  // want to compute U^tHUy = -U^tv  where U is basis for 0-sum vectors.  have chosen
  // basis elements with 1 and -1 in ith and last positions so
  //
  //
  //              I
  //   U =  -------------
  //        -1 ..... -1
  




  for (j=1; j <= n; j++)  for (k=1; k <= n-1; k++) H[j][k] -= H[j][n];
  for (j=1; j <= n-1; j++)  for (k=1; k <= n-1; k++) H[j][k] -= H[n][k];  //  H = U^tHU
  for (j=1; j <= n-1; j++)  v[j] = v[n] - v[j];      // v = -U^tv


  /*    for (j=1; j < n; j++) {
    for (k=1; k < n; k++) printf("%f\t",H[j][k]);
    printf("\n");
    } */

  //  for (j=1; j < n; j++) printf("%f\n",v[j]);

  ludcmp(H,n-1,index,&d);  
  lubksb(H,n-1,index,v);   // solve Hy = v


  v[n] = 0;
  for (j=1; j < n; j++) v[n] -= v[j];  // v = Uv
  

  /*  v[1] = v[1]/H[1][1];
      v[2] = -v[1];  */
  

  for (j=0; j < n; j++) p[j] += v[j+1];   // p is the optimal vector using quadratic approx
  //  printf("before trunc %f %f %f\n",p[0],p[1],p[2]);

  for (j=0,s=0; j < n; j++) if (p[j] < 0) p[j] = 0;  // truncate and renormalize if oob
  for (j=0,t=0; j < n; j++) t += p[j];
  for (j=0; j < n; j++) if (p[j] > 0) p[j] /= t;

  //  printf("p = %f %f\n",p[0],p[1]);
  arb_meld(n,p,q,meld);
  //  printf("%f %f %f\n",p[0],p[1],p[2]);
  //      printf("opt like = %f (%f %f %f)\n",plain_like(meld),p[0],p[1],p[2]);
  


      /*  if (n == 3)  {
    for (i=0; i < 10; i++) {
      for (j=0; j < (10-i); j++) {
	p[0] = .05 + i/10.;
	p[1] = .05 + j/10.;
	p[2] = 1-p[0]-p[1];
	arb_meld(n,p,q,meld);
	//	printf("%f ",plain_like(meld));
	printf("(%f %f %f) %f\n",p[0],p[1],p[2],plain_like(meld));
      }
      printf("\n");
    }
    exit(0);
    } */
}







float
new_table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,meld[FREQDIM/2],flat_spect[FREQDIM/2],*q[3],p,*mix,t,*mem,po,solo_prop,w[3],pp;
  int i;

  //    for (i=0; i < freqs; i++) flat_spect[i] = 1./ freqs;

  if (sas.shade == REST_STATE)   { 
    mem = &rest_like;  
    solo_spect = restspect; 
  }
  //   else if (sas.pos < 2)   {  // long established way
  else if (sas.pos < 2 && sas.shade != ATTACK_STATE && sas.shade != FALL_STATE)   {  // recognition of notes following space tends to be early  .. maybe this will help ...?
    /* this isn't really right.  what we really want is to use the single note model when coming from silence.  not possible to tell this from 
       the "STATES" that are used, but could put another member into the graph nodes that "knows" if the node comes from silence or sound */
    mem = &(meld_spect_like_table[sas.index][0]); 
    solo_spect = score.solo.note[sas.index].meld_spect[0]; 
  }
  else { 
    mem = &(spect_like_table[sas.index]); 
    solo_spect = score.solo.note[sas.index].spect; 
  }

  p = *mem;
  if (p != PROB_UNSET) return(p);

  if (midi_accomp) mix = sas.acc_spect;
  else mix = (accomp_on_speakers) ? orchestra_spect : solo_spect;
  if (is_solo_rest(sas.index) || sas.shade == REST_STATE)  { // added || sas.shad ... 12-06

    //       meld_solo_orch_back(background, mix, background, meld);  // changed 3-09  seems to work better than mix,mix,background 
  
	//	meld_solo_orch_back(mix, mix, background, meld);  // this way seems to jump in too early after rests

    q[0] = mix; q[1] = background; w[0] = .8;   w[1] = .2;  // need to test this vs above
		 simple_opt_meld(2,w,q,meld);   // not able to show any improvment here, yet

    /*    p = spect_like(meld);
    q[0] = mix; q[1] = background; w[0] = .8;   w[1] = .2;  // need to test this vs above
    pp = simple_opt_spect_like(2,w,q);
    printf("p = %f pp = %f rat = %f\n",p,pp,pp/p); */


    /*                  p = spect_like(meld);
                 q[0] = mix; q[1] = background; w[0] = w[1] = .5;  // need to test this vs above
		   opt_spect_meld(2, w, q, meld); 
                  pp = spect_like(meld);
		  printf("p = %f pp = %f\n",p,pp);  // this isn't working since the opt_spect is not always greater than the fixed parm spect
    */

  }
  /*#ifdef PIANO_RECOG
  else if (atck_spect) meld_solo_orch_back_atck(solo_spect, mix, background, atck_spect,meld);  // this is best currently
  else meld_solo_orch_back(solo_spect, mix, background, meld);  // this is best currently
  #else*/
  else   {
    meld_solo_orch_back(solo_spect, mix, background, meld);  // this is best currently

    /*    solo_prop = solo_proportion(sas.index);
    q[0] = solo_spect; q[1] = mix; q[2] = background; w[0] = solo_prop; w[1] = .25; w[2] = 1-(w[0]+w[1]);
    arb_meld(3,w,q,meld);
	    //	    simple_meld_spects(solo_spect, mix, meld , solo_prop);
	    //	    meld_solo_orch_back(solo_spect, mix, background, meld);*/
 }
  /*#endif*/


  //  else   meld_solo_orch_back(solo_spect, solo_spect, background,  meld); // works with mix instead of background

  for (i=0; i < freqs; i++) if (meld[i] <= 0) {
    printf("meld = %f solo = %f mix = %f back = %f\n",meld[i],solo_spect[i],mix[i],background[i]);
    printf("pos = %d rest = %d\n",sas.pos,sas.shade == REST_STATE);
    printf("index = %d\n",sas.index);
    printf("attack = %f\n",score.solo.note[sas.index].meld_spect[sas.pos][i]);
    exit(0);
  }



  //        for (i=0; i < freqs; i++) printf("%f ",meld[i]);



  //  if (token == 0)   { printf("accomp_on_speakers = %d\n",accomp_on_speakers); for (i=0; i < freqs; i++) printf("%d %f %f\n",i,solo_spect[i],mix[i]);    printf("\n"); }

                 p = spect_like(meld);



  //         p = spect_like_mask(meld);
  //  	     p = poisson_table_spect_like(solo_index, acc_spect,  sas);
	      //    printf("regular model = %f possion model = %f rat = %f\n",p,po,p/po);

		 /*		 if (token >= 2500 && token <= 2700) {
		   //      for (i=0; i < freqs; i++) printf("%f ",spect[i]);
		   //            printf("\n");
		   //      for (i=0; i < freqs; i++) printf("%f ",spect[i]);
		   //      printf("\n");
      
		   //	   printf("frame = %d\tnote = %s\trest = %d\tprob = %f\n",token,score.solo.note[solo_index].observable_tag,(REST_STATE ==sas.shade),p);
		   //        spect_like_test(mix);
		   //		   if (sas.shade != REST_STATE) 	   for (i=0; i < freqs; i++) printf("%d mx=%f bk=%f mld=%f sol=%f y=%f %f\n",i,mix[i],background[i],meld[i],solo_spect[i],spect[i],spect[i]*log(meld[i]));
      
		   }    */



  *mem = p;
  return(p);
}


#define BACK_PROB .05

void
piano_table_spect(SA_STATE sas, float *meld) {
  float *solo_spect,*q[3],*mix,w[3],s;
  int i;

  if (sas.shade == REST_STATE)  solo_spect = restspect; 
  else if (sas.pos < 2)  solo_spect = score.solo.note[sas.index].meld_spect[0]; 
  else   solo_spect = score.solo.note[sas.index].spect; 

  if (midi_accomp) mix = sas.acc_spect;
  else mix = (accomp_on_speakers) ? orchestra_spect : solo_spect;
  if (is_solo_rest(sas.index) || sas.shade == REST_STATE) {  // added || sas.shad ... 12-06
    //    printf("time = %f\n",sas.last_accomp_time);
    w[0] = exp(-2*sas.last_accomp_time); q[0] = mix;
    w[1] = 1-w[0]; q[1] = background;
    //    printf("last time = %f w[0] = %f w[1] = %f\n",sas.last_accomp_time,w[0],w[1]);
    arb_meld(2,w,q,meld);
  }
  //#ifdef PIANO_RECOG
  else   {
    w[0] = exp(-2*sas.last_accomp_time); q[0] = mix;
    w[1] = 1; q[1] = solo_spect;
    w[2] = BACK_PROB; q[2] = background;
    s = w[0] + w[1];
    w[0] *= (1-BACK_PROB)/s;
    w[1] *= (1-BACK_PROB)/s;
    //    printf("last time = %f w[0] = %f w[1] = w[2] = %f %f\n",sas.last_accomp_time,w[0],w[1],w[2]);
    arb_meld(3,w,q,meld);
    //    meld_solo_orch_back(solo_spect, mix, background, meld);  // this is best currently
 }
  for (i=0; i < freqs; i++) if (meld[i] <= 0) { 
    printf("bad that this happens\n");
    exit(0);
  }

}

static float
piano_table_spect_like(SA_STATE sas) {  
  float meld[FREQDIM/2],p,*mem;

  if (sas.shade == REST_STATE) mem = &rest_like;  
  else if (sas.pos < 2)   mem = &(meld_spect_like_table[sas.index][0]); 
  else  mem = &(spect_like_table[sas.index]); 

  p = *mem;
  if (p != PROB_UNSET) return(p);
  piano_table_spect(sas,meld);
  p = spect_like(meld);
  *mem = p;
  return(p);
}




#ifdef SPECT_TRAIN_EXPT
static float
train_table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float meld[FREQDIM/2],*mix,*mem;
  int i,in_notes;

  in_notes = (sas.index >= firstnote && sas.index <= lastnote);
  if (in_notes) {
    i = spect_train_pos(sas.pos,sas.index);
    mem = &(train_spect_like_table[sas.index][i]); 
  }
  else   mem = &rest_like;  

  if (*mem != PROB_UNSET) return(*mem);
  if (in_notes) {
    *mem = spect_like(score.solo.note[sas.index].train_spect[i]);
    return(*mem);
  }
#ifdef ORCHESTRA_EXPERIMENT
  mix = (accomp_on_speakers) ? orchestra_spect : restspect;
  meld_solo_orch_back(restspect, mix, background, meld);  // this is best currently
#else
  meld_spects(restspect, acc_spect, meld, MELD_CONST /*.7*/);
#endif
  *mem = spect_like(meld);
  return(*mem);
}
#endif














static float
plain_like(float *meld) {
  int i;
  float t=0,data[FREQDIM];

  for (i=0; i < freqs; i++) t += data[i] = spect[i];
  for (i=0; i < freqs; i++) data[i] /=t;
  t = 0;
  for (i=0; i < freqs; i++) {
    //    if (meld[i] <= 0) continue;
    if (meld[i] <= 0 || spect[i] <= 0) { printf("%f %f in plain_like\n",meld[i],spect[i]); exit(0); }
    t += data[i]*log(meld[i]/data[i]);
  }
  return(t);
}




float
opt_spect_like(int n, float *p, float **q, float *b) {
  int i,j,k,dex[MAX_BASIS],*index;
  float grad_buff[MAX_BASIS],hess_ptr[MAX_BASIS],
    hess_buff[MAX_BASIS*MAX_BASIS],*v,**H,A[FREQDIM/2],meld[FREQDIM/2],t,s;
  double d;

  arb_meld(n,p,q,meld);
  //  printf("init like = %f n = %d (%f %f %f %f)\n",plain_like(meld),n, p[0],p[1],p[2],p[3]);

  index = dex-1;
  v = grad_buff-1;  // v[1...n] is gradient if log likelihood
  H = hess_ptr-1;  // H[1...n][1...n] is Hessian  matrix of log likelihood
  for (i=1; i <= n; i++) H[i] = hess_buff + (i-1)*n -1;
  for (i=0; i < freqs; i++) {
    A[i] = 0;
    for (j=0; j < n; j++) A[i] += p[j]*q[j][i];  // A[i] used in grad and Hess calc
  }
  for (j=0; j < n; j++) {
    v[j+1] = 0;
    for (i=0; i < freqs; i++) v[j+1] += spect[i]*q[j][i]/A[i];   // compute gradient
  }
  for (j=0; j < n; j++) {
    for (k=0; k < n; k++) {
      H[j+1][k+1] = 0;
      for (i=0; i < freqs; i++) 
	H[j+1][k+1] -= spect[i]* (q[j][i]*q[k][i])/(A[i]*A[i]);   // compute Hessian
    }
  }




  // want to compute U^tHUy = -U^tv  where U is basis for 0-sum vectors.  have chosen
  // basis elements with 1 and -1 in ith and last positions so
  //
  //
  //              I
  //   U =  -------------
  //        -1 ..... -1
  




  for (j=1; j <= n; j++)  for (k=1; k <= n-1; k++) H[j][k] -= H[j][n];
  for (j=1; j <= n-1; j++)  for (k=1; k <= n-1; k++) H[j][k] -= H[n][k];  //  H = U^tHU
  for (j=1; j <= n-1; j++)  v[j] = v[n] - v[j];      // v = -U^tv


  /*    for (j=1; j < n; j++) {
    for (k=1; k < n; k++) printf("%f\t",H[j][k]);
    printf("\n");
    } */

  //  for (j=1; j < n; j++) printf("%f\n",v[j]);

  ludcmp(H,n-1,index,&d);  
  lubksb(H,n-1,index,v);   // solve Hy = v


  v[n] = 0;
  for (j=1; j < n; j++) v[n] -= v[j];  // v = Uv
  

  /*  v[1] = v[1]/H[1][1];
      v[2] = -v[1];  */
  

  for (j=0; j < n; j++) p[j] += v[j+1];   // p is the optimal vector using quadratic approx
  //  printf("before trunc %f %f %f\n",p[0],p[1],p[2]);

  for (j=0,s=0; j < n; j++) if (p[j] < b[j]) s += p[j] = b[j];  // truncate and renormalize if oob
  for (j=0,t=0; j < n; j++) if (p[j] > b[j]) t += p[j];
  for (j=0; j < n; j++) if (p[j] > b[j]) p[j] /= (t/(1-s));


  arb_meld(n,p,q,meld);
  //  printf("%f %f %f\n",p[0],p[1],p[2]);
  //      printf("opt like = %f (%f %f %f)\n",plain_like(meld),p[0],p[1],p[2]);
  


      /*  if (n == 3)  {
    for (i=0; i < 10; i++) {
      for (j=0; j < (10-i); j++) {
	p[0] = .05 + i/10.;
	p[1] = .05 + j/10.;
	p[2] = 1-p[0]-p[1];
	arb_meld(n,p,q,meld);
	//	printf("%f ",plain_like(meld));
	printf("(%f %f %f) %f\n",p[0],p[1],p[2],plain_like(meld));
      }
      printf("\n");
    }
    exit(0);
    } */


  return(spect_like(meld));

}






float
working_table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,meld[FREQDIM/2],p,*mix,t;
  int i;
  float w[2]= {.5,.5}, *q[2];

  
#ifdef MELD_EXPT
    return(new_table_spect_like(solo_index, acc_spect, sas));
#endif


  p = (sas.shade == REST_STATE) ? rest_like : spect_like_table[sas.index];
  if (p != PROB_UNSET) return(p);
  solo_spect = (sas.shade == REST_STATE) ? restspect : score.solo.note[sas.index].spect;
#ifdef ORCHESTRA_EXPERIMENT
  mix = (accomp_on_speakers) ? orchestra_spect : solo_spect;
  /* if no accomp accomp_on_speakers isn't set to 0 as it should, but this still
     works since then orchestra_sprect = 0 */


  //  meld_spects(solo_spect, orchestra_spect, meld,MELD_CONST /*.7*/);
  
  //    meld_spects(solo_spect, mix, meld,MELD_CONST /*.7*/);
  if (is_solo_rest(sas.index) || sas.shade == REST_STATE)   // added || sas.shad ... 4-06
    meld_solo_orch_back(mix, mix, background, meld);  
    //    meld_solo_orch_back(background, mix, background, meld);  
  else   meld_solo_orch_back(solo_spect, mix, background, meld);


#else
  meld_spects(solo_spect, acc_spect, meld, MELD_CONST /*.7*/);
#endif


  p = spect_like(meld);

  /*  if (token >= 9400 && token <= 9500) {
    printf("frame = %d note = %s state = %d prob = %f\n",token,score.solo.note[solo_index].observable_tag,(REST_STATE ==sas.shade),p);
      //        spect_like_test(mix);
      //      if (sas.shade == REST_STATE) for (i=0; i < freqs; i++) printf("%d %f %f %f %f\n",i,mix[i],meld[i],background[i],spect[i]);
      
      }    */

  if (sas.shade == REST_STATE)     rest_like = p;
  else  spect_like_table[sas.index] = p;


  //printf("token = %d index = %d %s speakers = %d is_solo_rest = %d is_rest = %d  p = %f\n",token,sas.index,score.solo.note[sas.index].observable_tag,accomp_on_speakers,is_solo_rest(sas.index),sas.shade == REST_STATE , p);

  //  if (token == 7 && sas.index == -1)    for (i=0; i < freqs; i++) printf("%d %f %f %f %f %f\n",i,solo_spect[i],mix[i],background[i],meld[i],log(meld[i])*spect[i]);


  /*  if (sas.shade == REST_STATE) {
    rest_like = p;
    //pr//intf("token = %d rest_like = %f\n",token,p);
    //    if (token == 75)    for (i=0; i < freqs; i++) printf("%d %f %f\n",i,meld[i],spect[i]);
  }
  else {
    spect_like_table[sas.index] = p;
    //    printf("token = %d %d-like = %f\n",token,sas.index,p);
    //    if (token == 75)    for (i=0; i < freqs; i++) printf("%d %f %f\n",i,meld[i],spect[i]);
    } */
  //  if (solo_index == 1 || solo_index == -1) printf("frame = %d index = %d state = %d sas.shade prob = %f\n",token,solo_index,(REST_STATE ==sas.shade),p);
  return(p);
}



float
experim_table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  /* can get rid of 1st two arguments since they are part of sas */
  float *solo_spect,meld[FREQDIM/2],p,*mix,t;
  int i;
  float w[10], *q[10],b[10];

  
#ifdef MELD_EXPT
  return(new_table_spect_like(solo_index, acc_spect, sas));
#endif


  p = (sas.shade == REST_STATE) ? rest_like : spect_like_table[sas.index];
  if (p != PROB_UNSET) return(p);
  solo_spect = (sas.shade == REST_STATE) ? restspect : score.solo.note[sas.index].spect;
#ifdef ORCHESTRA_EXPERIMENT
  mix = (accomp_on_speakers) ? orchestra_spect : solo_spect;
  /* if no accomp accomp_on_speakers isn't set to 0 as it should, but this still
     works since then orchestra_sprect = 0 */


  
  //  meld_spects(solo_spect, orchestra_spect, meld,MELD_CONST /*.7*/);
  
  //    meld_spects(solo_spect, mix, meld,MELD_CONST /*.7*/);
  if (is_solo_rest(sas.index) || sas.shade == REST_STATE)   // added || sas.shad ... 4-06
    meld_solo_orch_back(mix, mix, background, meld);  
    //    meld_solo_orch_back(background, mix, background, meld);  
  else   meld_solo_orch_back(solo_spect, mix, background, meld);


#else
  meld_spects(solo_spect, acc_spect, meld, MELD_CONST /*.7*/);
#endif
  //if (token == 5)    for (i=0; i < freqs; i++) printf("%d %f %f\n",i,solo_spect[i],orchestra_spect[i]);  
  //    printf("\n");


  /*    if ( sas.index > 0 && score.solo.note[sas.index].snd_notes.snd_nums[0] != score.solo.note[sas.index-1].snd_notes.snd_nums[0] && is_solo_rest(sas.index-1) == 0 && is_solo_rest(sas.index) == 0)   { 
    q[0] = background; q[1] = orchestra_spect;  q[2] =  score.solo.note[sas.index].spect;  
    q[3] = score.solo.note[sas.index-1].spect;
      w[0] = .1;            w[1] = .3;  w[2] = .5;  w[3] = .1;
      b[0] = .01;           b[1] = .01; b[2] = .3; b[3] = .01;
      p = opt_spect_like(4,w,q,b);

  }
  else */ if (is_solo_rest(sas.index) || sas.shade == REST_STATE) {
    q[0] = background; q[1] = orchestra_spect; q[2] = acc_spect;
    //    w[0] = .2;         w[1] = .8;
    w[0] = .2;         w[1] = .4;     w[2] = .4;
    b[0] = .01;        b[1] = .01;    b[2] = .01;
    p = opt_spect_like(3,w,q,b);
  }
  else {
    q[0] = background;    q[1] = orchestra_spect; q[2] = solo_spect;
    w[0] = .1;            w[1] = .3;  w[2] = .6;
    b[0] = .01;           b[1] = .01; b[2] = .3;
    p = opt_spect_like(3,w,q,b); 
    // p = spect_like(meld);

  }

  /*  if (token >= 2155 && token <= 2157) {
      printf("frame = %d note = %s state = %d sas.shade prob = %f\n",token,score.solo.note[solo_index].observable_tag,(REST_STATE ==sas.shade),p);
      //        spect_like_test(mix);
      //      if (sas.shade == REST_STATE) for (i=0; i < freqs; i++) printf("%d %f %f %f %f\n",i,mix[i],meld[i],background[i],spect[i]);
      
      }    */



  //  printf("spect prob =  %f\n",p);
    //  p = spect_like(meld);

    /*          if (token >= 2695 && token <= 2695) {
      printf("frame = %d note = %s state = %d sas.shade prob = %f\n",token,score.solo.note[solo_index].observable_tag,(REST_STATE ==sas.shade),p);
      //        spect_like_test(mix);
      for (i=0; i < freqs; i++) printf("%d %f %f %f\n",i,orchestra_spect[i],sas.acc_spect[i],spect[i]);
      exit(0);
      }    */

  if (sas.shade == REST_STATE)     rest_like = p;
  else  spect_like_table[sas.index] = p;


  //printf("token = %d index = %d %s speakers = %d is_solo_rest = %d is_rest = %d  p = %f\n",token,sas.index,score.solo.note[sas.index].observable_tag,accomp_on_speakers,is_solo_rest(sas.index),sas.shade == REST_STATE , p);

  //  if (token == 7 && sas.index == -1)    for (i=0; i < freqs; i++) printf("%d %f %f %f %f %f\n",i,solo_spect[i],mix[i],background[i],meld[i],log(meld[i])*spect[i]);


  /*  if (sas.shade == REST_STATE) {
    rest_like = p;
    //pr//intf("token = %d rest_like = %f\n",token,p);
    //    if (token == 75)    for (i=0; i < freqs; i++) printf("%d %f %f\n",i,meld[i],spect[i]);
  }
  else {
    spect_like_table[sas.index] = p;
    //    printf("token = %d %d-like = %f\n",token,sas.index,p);
    //    if (token == 75)    for (i=0; i < freqs; i++) printf("%d %f %f\n",i,meld[i],spect[i]);
    } */
  //  if (solo_index == 1 || solo_index == -1) printf("frame = %d index = %d state = %d sas.shade prob = %f\n",token,solo_index,(REST_STATE ==sas.shade),p);
  return(p);
}



int
spect_train_pos(int pos, int n) {
  int i;

#ifdef SPECT_TRAIN_EXPT
  if (pos == (score.solo.note[n].positions-1)) return(SPECT_TRAIN_NUM-1);
  if (pos < SPECT_TRAIN_NUM-2) return(pos);
  return(SPECT_TRAIN_NUM-2);
#endif
}


float
table_spect_like(int solo_index,float *acc_spect, SA_STATE sas) {  
  int i;

#ifdef SPECT_TRAIN_EXPT
  i = spect_train_pos(sas.pos,sas.index);
  if (score.solo.note[sas.index].train_spect[i] && score.solo.note[sas.index].train_spect[i][0] > 0)
  return(train_table_spect_like(solo_index, acc_spect, sas));
#endif
  
#ifdef MELD_EXPT
 #ifdef PIANO_RECOG
  return(piano_table_spect_like(sas));
 #else
  return(new_table_spect_like(solo_index, acc_spect, sas));
 #endif
  /*  if (mode == FORWARD_MODE || mode == BACKWARD_MODE)
    //    return(poisson_table_spect_like(solo_index, acc_spect, sas));
    return(new_table_spect_like(solo_index, acc_spect, sas));
  else if (mode == SYNTH_MODE || mode == LIVE_MODE ||  mode == SIMULATE_MODE || mode == SIMULATE_FIXED_LAG_MODE) 
    return(new_table_spect_like(solo_index, acc_spect, sas));
    else { printf("unknown mode in table_spect_like\n"); exit(0); }*/
#endif
  return(working_table_spect_like(solo_index, acc_spect, sas));

}






#define AUDIO_PERCENTILE 70

void
audio_level() {
  float v[MAX_FRAMES],obs_q,des_q;
  int i;

  for (token=0; token < frames; token++) {
    samples2data();
    orchestra_energy_feature(v+token,0);
  }
  qsort(v,frames,sizeof(float),fcomp);
  obs_q = v[frames*AUDIO_PERCENTILE/100];
  //  printf("val = %f %f\n",v[frames*AUDIO_PERCENTILE/100],v[1+frames*AUDIO_PERCENTILE/100]);
  des_q = feature.el[0].quantile[0][AUDIO_PERCENTILE*QUANTILES/100];
  printf("obs = %f des = %f\n",obs_q,des_q);
}


float
max_audio_level() {
  float v[MAX_FRAMES],obs_q,des_q,m=0;
  unsigned int i;
  unsigned char x[2];


  /*  i = 0x8000; printf("%f\n",sample2float(&i));
  i = 0x7fff; printf("%f\n",sample2float(&i));
		   
  for (i=0; i < 100; i++) {
    m = .1*(i-10);
    float2sample(m,x);
    printf("%f %f\n",m,sample2float(x));
    }*/
  for (token=0; token < frames; token++) {
    samples2data();
    for(i=0; i < FRAMELEN; i++) if (data[i] > m) m = data[i];
    energy_feature(v+token,0);

    //  printf("%d %f\n",token,v[token]);
  }
  printf("max = %f\n",m);
  qsort(v,frames,sizeof(float),fcomp);
  obs_q = v[frames-1];
  return(obs_q);
}


void
poisson_like_test() {
  int i;
  SA_STATE sas;
  float m;

  mode = FORWARD_MODE;
 
  for (token = 206; token <= 208; token++) {
    audio_listen();
    sas.index = 2;
    sas.shade = NOTE_STATE;
    sas.pos = 2;
    for (i=0; i < freqs; i++)   printf("%f ",spect[i]);
    printf("\n");
    for (i=0; i < freqs; i++)   printf("%f ",orchestra_spect[i]);
    printf("\n");
    for (i=0; i < freqs; i++)   printf("%f ",background[i]);
    printf("\n");
    /*    table_spect_like(0,0,sas);
    printf("\n");
    sas.index = 3250;
    table_spect_like(0,0,sas);
    printf("\n");
    sas.shade = REST_STATE;
    table_spect_like(0,0,sas);
    printf("\n");*/
  } 
  return;

for (i=firstnote; i < lastnote; i++) {
    //    printf("%s\n",score.solo.note[i].observable_tag);
    sas.index = i;
    sas.shade = NOTE_STATE;
    sas.pos = 2;
    for (token = score.solo.note[i].frames; token < score.solo.note[i+1].frames; token++) {
      audio_listen();
      //      poisson_table_spect_like(0,0,sas);

    }
  }
}




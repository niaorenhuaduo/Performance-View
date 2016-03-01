


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>




#include "share.c"
#include "global.h"
#include "four1.c"
#include "realft.c"
#include "audio.h"



#define QUANTA 10
#define MAX_CELLS 20
#define UNSET  HUGE_VAL

#define MAX_REC_NUM 100

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a > _b ? _a : _b; })




typedef struct {
  int num;
  STAT_REC list[MAX_REC_NUM];
} REC_LIST;


/* typedef struct tnode;  */
  
 
typedef struct spline_node {
  int split_stat;
  float breakpt[MAX_CELLS];  /* num is last breakpt so num+1 breakprts */
  float val[MAX_CELLS];
  struct spline_node *child[MAX_CELLS];
  int num;
} SPLINE_TREE;
        
typedef struct spline {
  float breakpt[MAX_CELLS];  /* num is last breakpt so num+1 breakprts */
  float val[MAX_CELLS];
  int num;
} SPLINE;
        

typedef struct {
  float breakpt[MAX_CELLS];
  struct tnode *child[MAX_CELLS];
  int num;
} OFFSPRING;
        

typedef struct {
  float prob[QUANTA];
  float div[QUANTA+1];   /*  corresponds to boundaries of intervals */
  int   type;            /* what type of dist NOTE_DIST,REST_DIST etc */
} DIST;

typedef struct tnode {
  struct tnode *lc;
  struct tnode *rc;
  int    split_stat;   /* 0 means a stat of type type[0] */
  SPLINE_TREE spline;
  OFFSPRING offspring;
  float cutoff;
  float logq;     /* log of quotient p(statistic|state)/p(statistic) */
  float quot;     /* quotient p(statistic|state)/p(statistic) */
  float x;       /* to the left of x function takes value fx */
  float fx;      /* to the right of y function takes value fy */
  float y;       /* linear in between */
  float fy;      /* ultimate func is product of tehse funcs */
} TNODE;

typedef struct {
  int dist_num;    /* number of distributions */
  float (*stat_fun[STAT_NUM])();   /* might no need this */
  int   type[STAT_NUM];   /* the type of the statistics */
  SPLINE_TREE *spline;
  TNODE *root;  
  DIST *dist;
  SPLINE marg[STAT_NUM];
} STATE;


REC_LIST stat_rec[STAT_NUM];
STATE state[STATE_NUM];
float (*stat_func[STAT_NUM])();
int arg_num[STAT_NUM];   /* this stat takes how many arguments */
float state_pr[STATE_NUM][PITCHNUM];
int arg_bool[STAT_NUM][ARG_NUM];

/* these arrays store values to avoid duplications of expensive calls to stat functions and 
    recomputation of probabilities */

float state_logq[STATE_NUM][PITCHNUM];
float stat_val[STAT_NUM][PITCHNUM];


/* the most important statistic should be listed 1st and is assumed to have a decr. dist. */


int template[STATE_NUM][STAT_NUM+2] =  {
  { NOTE_STATE        ,  5  , PITCH_STAT  , OCTAVE_STAT , BURST_STAT , COLOR_STAT, ENERGY_STAT } ,
  { REST_STATE        ,  1  , ENERGY_STAT                              } ,
  { ATTACK_STATE      ,  3  , BURST_STAT, SIGNED_BURST_STAT, ENERGY_STAT   } ,
  { ATTACK2_STATE      ,  3  , BURST_STAT, SIGNED_BURST_STAT, ENERGY_STAT   } ,
  { REARTIC_STATE   ,  7  , BURST_STAT ,COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT,  PITCH_STAT, OCTAVE_STAT, ENERGY_STAT } ,
  { REARTIC2_STATE   ,  7  , COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT, BURST_STAT, PITCH_STAT, OCTAVE_STAT, ENERGY_STAT } ,
  { SLUR2_STATE     ,     5  , BURST_STAT, SIGNED_BURST_STAT, PITCH_STAT, ENERGY_STAT, FUND_STAT  } ,
  { SLUR_STATE     ,     2  , COLOR_STAT , BURST_STAT } ,
}; 



/*int template[STATE_NUM][STAT_NUM+2] =  {
  { NOTE_STATE        ,  7  , PITCH_STAT  , OCTAVE_STAT  ,  BURST_STAT , COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT, ENERGY_STAT } ,
  { REST_STATE        ,  7  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  BURST_STAT , COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT} ,
  { ATTACK_STATE      ,  7  , BURST_STAT  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT } ,
  { ATTACK2_STATE     ,  7  , BURST_STAT  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT } ,
  { REARTIC_STATE     ,  7  , BURST_STAT  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT } ,
  { REARTIC2_STATE    ,  7  , BURST_STAT  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT } ,
  { SLUR_STATE        ,  7  , BURST_STAT  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT } ,
  { SLUR2_STATE       ,  7  , BURST_STAT  , ENERGY_STAT , PITCH_STAT  , OCTAVE_STAT  ,  COLOR_STAT , FUND_STAT ,  SIGNED_BURST_STAT } ,
}; 

*/


/*
int template[STATE_NUM][STAT_NUM+2] =  {
  { NOTE_STATE        ,  2  , PITCH_STAT  , OCTAVE_STAT } ,
  { REST_STATE        ,  1  , ENERGY_STAT  } ,
  { ATTACK_STATE      ,  1  , BURST_STAT } ,
  { ATTACK2_STATE      ,  1  , BURST_STAT } ,
  { REARTIC_STATE   ,  2  , BURST_STAT , PITCH_STAT } ,
  { REARTIC2_STATE   ,  2  , BURST_STAT , PITCH_STAT } ,
  { SLUR_STATE     ,     2  ,  BURST_STAT, COLOR_STAT } ,
  { SLUR2_STATE     ,     2  , COLOR_STAT , BURST_STAT } ,
};
*/




#define MAX_DATA 10000   /* the maximum #of examples that will be stored */









typedef struct {
  float *val;
  float prob;
} DATUM;


typedef struct {
  DATUM *list;
  int   num;    /* max number of data vectors */
  float total;  /* total amount of probability in struct */
  int   size;   /* number of floats in datum */
} DATA;


extern float *spect;        /* the fft'ed and processed token */
extern float *data;
extern int freqs;
extern PITCH *sol;   /* solfege array */
extern SCORE score;         /* contains the score we expect to hear */
extern int  *notetime;       /* notetime[i] is time in beats the ith note starts */
extern int  token;
extern char scorename[];
extern int  firstnote;
extern int  lastnote;


float pr[PITCHNUM];    /* pr[i] is probability token represents pitch i */
float oct[PITCHNUM];    
float chgpr;           /* probability of change taking place this token */
float restpr;          /* probability of rest taking place this token */
float articpr;          /* probability of articulation taking place this token */

DATA marg[STATE_NUM],exam[STATE_NUM];
DIST  restd,changed,noted,articd;



states_equal(s1,s2)
FULL_STATE *s1,*s2; {
  int a;

  if (s1->statenum != s2->statenum) return(0);
  for (a=0; a < ARG_NUM; a++) if (s1->arg[a] != s2->arg[a]) return(0);
  return (1);
}

add_rec(rec)
STAT_REC rec; {
  int n,s;

  s = rec.si.statnum;
  if (stat_rec[s].num == MAX_REC_NUM) printf("stat_rec full\n");
  stat_rec[s].list[stat_rec[s].num++] = rec;
}

STAT_REC *find_rec(si)
STAT_INDEX si; {
  int i,j,s,n;
  STAT_REC *cand;

  s = si.statnum;
  for (i=0; i < stat_rec[s].num; i++)  {
    cand = stat_rec[s].list+i;
    for (j=0; j < ARG_NUM; j++) 
      if (cand->si.arg[j] != si.arg[j])  break;
    if (j == ARG_NUM) return(cand);
  }
  return(NULL);
}





DATA init_data(n,size)
int n; 
int size; {
  DATA temp;
  
  temp.list = (DATUM *) malloc(n*sizeof(DATUM));
  temp.total = temp.num = 0;
  temp.size = size;
  return(temp);
}


add_data(d,x)
DATA *d;
DATUM x; {
  int i;

  d->list[(d->num)++] = x;
  d->total += x.prob;
/*  for (i=0; i < d->size; i++) d->list[d->num*d->size + i] = x[i];
  d->prob[d->num] = p;
  (d->num)++; */
}



#define DISTFILE "dist.parms"

initdist() {
  int i;

  for (i=0; i < QUANTA; i++) {
    restd.prob[i] = log(1. - i*.1);
    articd.prob[i] =  .1*(i+1);  
    changed.prob[i] = pow(.8,(float) (QUANTA-i));    
    noted.prob[i] =  log((QUANTA - i)/(float) QUANTA);
  }    
  for (i=0; i <= QUANTA; i++) {
    restd.div[i] = .1*i;
    articd.div[i] = .13*i;
    noted.div[i] = .1*i;
    changed.div[i] = .7 + .3*(i-1)/(float)QUANTA ;
  }
}

void
writedist() {
  int fd,i,j; 
  char name[500];

  fd = creat(DISTFILE,0755);
  for (i=0; i < STATE_NUM; i++) for (j=0; j < state[i].dist_num; j++)
    write(fd,state[i].dist + j,sizeof(DIST));
  close(fd);
}

void
readdist() {
  int fd,i,j; 
  
  fd = open(DISTFILE,0);
  if (fd == 1) { printf("%s not found\n",DISTFILE); return; }
  for (i=0; i < STATE_NUM; i++) for (j=0; j < state[i].dist_num; j++)
    read(fd,state[i].dist + j,sizeof(DIST));
  close(fd);
}



float lookup(stat,dist)
float stat;
DIST  *dist; {
  int i;
  float p,q;

  if (stat < dist->div[1]) return(dist->prob[0]);  
  if (stat > dist->div[QUANTA-1]) return(dist->prob[QUANTA-1]);  
  for (i=1; i <= QUANTA; i++) if (dist->div[i] >= stat) break;
  if (i == QUANTA) printf("problem in lookup\n");
return(dist->prob[i-1]);
  q = (stat-dist->div[i-1])/(dist->div[i]-dist->div[i-1]);
  p = 1-q;
  return(p*dist->prob[i-1]+q*dist->prob[i]);
}
  
float *myfilter;
float xxx_last_spect[1000];


float window[FRAMELEN];
static float beta[FREQDIM/2];

#define WIND_WIDTH 3.


old_init_window() {
  int i;
  float x;

  for (i=0; i < TOKENLEN; i++)  {
    x = WIND_WIDTH  * (i-TOKENLEN/2)/ (float) (TOKENLEN);
    window[i] = exp(-x*x);
  }
}

#define HS_SMOOTH /*.2*/ .35 /* .2 -> .35 on 10-09*/      /*.85  /*.5*/ /* will smooth spect by this many*/



init_window() {
  float hz,secs,sig,x;
  int i;

  hz = HS_SMOOTH*440*(pow(2.,1./12.) - 1);  /* hz to smooth */
  secs = 1/(2*3.14159*hz); /* std of gaussian kernal in secs */
  sig = SR*secs;   /* std of gaussian kernal in samples */


  for (i=0; i < FRAMELEN; i++)  {
    x = (i-FRAMELEN/2)/ sig;
    window[i] = exp(-.5*x*x);
  }
}


static void
init_beta() {
  int i;
  float a,b,p=.65,m=3,x,t=0;

  a = m*p;
  b = m*(1-p);
  for (i=0; i < freqs; i++) {
    x = i/(float)freqs;
            t += beta[i] = pow(x,a-1)*pow(1-x,b-1);
    //    t += beta[i] = 1;
  }
  for (i=0; i < freqs; i++) beta[i] /= t;
  /*  for (i=0; i < freqs; i++) printf("%d %f\n",i,beta[i]);
  fflush(stdout);
  exit(0);*/
}


init_like() {
  init_window();
  init_beta();
}

float princarg(float phasein) {
    float a = (float) phasein / (2 * PI);
    int k = floorf(a); //??? check that rounds down not up
    return (phasein - (float) k * 2 * PI);
}


void
create_spect(float *d, float *s) {  /* data has FRAMELEN pts */
  float temp[FREQDIM],m,x,scale,t;
  int i;


  for (i=0; i < FRAMELEN; i++) temp[i] = d[i];
  for (i=FRAMELEN; i < FREQDIM; i++) temp[i] = 0;
  for (i=0; i < FRAMELEN; i++)  temp[i] *= window[i];



  realft(temp-1,freqs,1);



  s[0] = square(temp[0]);
  for (i=1; i < freqs; i++) s[i] = (square(temp[2*i]) + square(temp[2*i+1])); //temp fft stored as re - im - re - im ...


#ifdef JAN_BERAN
  for (i=0; i < 50; i++) spect[i] = 0;  // lower part is covered by noising breathing from John Sanderson
#endif

  //    for (i=0; i < freqs/2; i++) s[i] = 0;  

  // maybe should look at modulus rather than modulus squared 
  //    for (i=0; i < freqs; i++) s[i] *= ((i < 30) ? 0 : 1);
  //  for (i=0; i < freqs; i++) s[i] *= (i > 200) ? 1 :  pow(i/200., 3.);

  //        for (i=0; i < freqs; i++) s[i] *= beta[i];

  //  for (i=0; i < freqs; i++)  s[i] =  pow(log(1+s[i]),.4);


  //  for (i=0; i < freqs; i++) s[i] = log(1+s[i]/.3 /*.5*);  // transformation to even out engergy 


  //  if (token == 300) for (i=0; i < freqs; i++)  printf("%d %f\n",i,spect[i]);


  //      if (token < 500) printf("during token = %d data[50] = %f temp[100] = %f temp[101] = %f s[50] = %f spect[50] = %f direct = %f alt = %f\n",token,
  //			      data[50],temp[100],temp[101],s[50],spect[50],square(temp[2*50]) + square(temp[2*50+1]),temp[2*50]*temp[2*50] + temp[101]*temp[101]);
  
}



extern unsigned char *audiodata;
extern float last_spect[];


//CF:  write the spectrum of the current input audio frame to global variable 'spect'
void
setspect() {
  float temp[FREQDIM],m,x,tp[FRAMELEN];
  int i,t,offset;
  unsigned char *ptr;

  //  printf("setting spect token = %d\n",token);
  
#ifdef POLYPHONIC_INPUT_EXPERIMENT    
  if (mode == BACKWARD_MODE) {
    t = (token > 0) ? token : 1;
    offset = max(0,(t*SKIPLEN - FRAMELEN)*BYTES_PER_SAMPLE);   // should this be t+1 like in samples2data?
    //    ptr =  audiodata + (t*SKIPLEN - FRAMELEN)*BYTES_PER_SAMPLE;  /* last spect */
    ptr =  audiodata + offset;
    samples2floats(ptr, tp, FRAMELEN); 
    create_spect(tp,last_spect);
  }

  else save_spect();
#endif
    

  create_spect(data,spect); //CF:  creates the spectrum. spect contains squared modulus


#ifdef ATTACK_SPECT
  compute_diff_spect();
#endif

#ifdef VIOLIN_EXPERIMENT
  for (i=0; i < freqs; i++) spect[i] *= 10*i/(float)freqs;
#endif
  return; //function returns here *?*


  for (i=0; i < FRAMELEN; i++) temp[i] = data[i];
  for (i=FRAMELEN; i < FREQDIM; i++) temp[i] = 0;
  for (i=0; i < FRAMELEN; i++)  temp[i] *= window[i];



  for (i=0; i < freqs; i++) xxx_last_spect[i] = spect[i];
  realft(temp-1,freqs,1);


  spect[0] = square(temp[0])/**filter[0]*/;
  for (i=1; i < freqs; i++) 
    spect[i] = (square(temp[2*i]) + square(temp[2*i+1]))/**filter[i]*/;
  
  
/*m = 0; 
  for (i=0; i < freqs; i++) if (spect[i] > m) m = spect[i];
  for (i=0; i < freqs; i++) spect[i] /= m;
  for (i=0; i < freqs; i++) spect[i] = (spect[i] <  .005) ? 0 : log(spect[i]/.005);  */
}




init_filter() {
  int i;
  FILE *fp;

  myfilter = (float *) malloc(freqs*sizeof(float));
  for (i=0; i < freqs; i++) myfilter[i] = 1.;
}


#define FILTER_FILE  "filter.dat"


write_filter() {
  FILE *fp;
  char name[500];
  int i;

/*  strcpy(name,scorename);
  strcat(name,".filt"); */
  fp = fopen(FILTER_FILE,"w");
  if (fp == NULL) printf("couldn't open %s\n",FILTER_FILE);
  for (i=0; i < freqs; i++) fprintf(fp,"%f\n",myfilter[i]);
  fclose(fp); 
}

read_filter() {
  FILE *fp;
  char name[500];
  int i;

  myfilter = (float *) malloc(freqs*sizeof(float));
/*  strcpy(name,scorename);
  strcat(name,".filt"); */
  fp = fopen(FILTER_FILE,"r");
  if (fp == NULL) printf("couldn't find %s\n",FILTER_FILE);
  for (i=0; i < freqs; i++) fscanf(fp,"%f\n",myfilter+i);
  fclose(fp); 
}


#define SMOOTH 5
#define LOW_END 55   /* g below low b float on oboe */
#define HIGH_END 91   /* g above high c */

set_filter()
{
  int i,j;
  float *f,*g,e;

  f = (float *) malloc(freqs*sizeof(float));
  g = (float *) malloc(freqs*sizeof(float));
  for (i=0; i < freqs; i++) g[i] = f[i] = 0;
  token = -1;
  init_filter();
  for (i=0; i < frames; i++) { 
    audio_listen();
    e = 0;
    for (j=0; j < freqs; j++) e += spect[j];
    e = sqrt(e);
    for (j=0; j < freqs; j++) f[j] += spect[j]/e;
  }
  for (j=0; j < freqs; j++) for (i=-SMOOTH; i <= SMOOTH; i++) g[j] += f[(i+j+freqs)%freqs];
  for (i=0; i < freqs; i++) 
    if (i > sol[LOW_END].omega && i < sol[HIGH_END].omega)
      myfilter[i] = 1000*frames/g[i];
    else myfilter[i] = 0;
}

float integ(low,high) /* integrate spectrogram at time t from low to high */
float low,high; {
  float total=0;
  int h,l,w;

  if (high <= 0) return(0);
  if (low  >= freqs) return(0);
  low = max(low,0);
  high = min(high,freqs);
  h = high;
  l = low;
  if (h == l) return( (high-low)*spect[h] );
  total += (1 - (low - l))*spect[l];
  total += (high - h)*spect[h];
  for (w = l+1; w < h; w++) total += spect[w];
  return(total);
}
  
								       
#define TOL     .05   /*.02    /* tolerance as percentage */

#define RAD     .5  /* in half steps */     
#define T1      1
#define T2      .1
#define T3      .1


clearprobs() {
  int i,j;

  for (i=0; i < PITCHNUM; i++) pr[i] = oct[i] = UNSET;
  restpr = UNSET;
  chgpr = UNSET;
  articpr = UNSET;
}

clear_probs() {
  int i,j;

/*  for (i=0; i < STATE_NUM; i++) for (j=0; j < PITCHNUM; j++) state_logq[i][j] = UNSET;
  for (i=0; i < STAT_NUM; i++) for (j=0; j < PITCHNUM; j++) stat_val[i][j] = UNSET; */
  for (i=0; i < STAT_NUM; i++) stat_rec[i].num=0;
}


ii_1() {
  int i,j;

  for (i=0; i < STATE_NUM; i++) for (j=0; j < PITCHNUM; j++) 
    state_logq[i][j] = UNSET;
}

ii_2() {
  int i,j;

  for (i=0; i < STAT_NUM; i++) for (j=0; j < PITCHNUM; j++) 
    stat_val[i][j] = UNSET;
}


init_iter() { /* set probs and stats based om previous runs */
  int i,j,sn,p;
  float stat;

/*  new_data = 0;*/
  if (mode == BACKWARD_MODE) token--;
  else token++;
/*  clear_probs();*/
  audio_listen();
}

float hstep(f,h)  /* raises f by h halfsteps */
float f,h; {
  return(f*pow(2.,h/12.));
}


#define HZ_BAND  50     /* search this much */
#define HF_STEP_BAND .5


float pitch_band(w)
float w; {

  return(HZ_BAND*FREQDIM/(float)SR +  hstep(w,HF_STEP_BAND) - w);
}


float pitchstat(arg)
   /* stats returned ins s1 and s2 are power in exterior int/int+ext for note */ 
int *arg;  {

 
  float power=0,interior = 0, exterior = 0,w,p,band;
  int i,n;

  n = arg[0];
  w = sol[n].omega;
  for (i=0; i < freqs; i++)  power += spect[i];
/*  for ( p=w; p < freqs;  p += w)  interior += integ( hstep(p,-1.),hstep(p,1.) ); */
  for ( p=w; p < freqs;  p += w)  {
    band = pitch_band(p);
    interior += integ( p-band,p+band ); 
  }
  return(1 - interior/power);
} 

float pitch_stat(state)
   /* stats returned ins s1 and s2 are power in exterior int/int+ext for note */ 
FULL_STATE state;  {

 
  float power=0,interior = 0, exterior = 0,w,p,band;
  int i,n;

  n = state.arg[THIS_PITCH];
  w = sol[n].omega;
  for (i=0; i < freqs; i++)  power += spect[i];
/*  for ( p=w; p < freqs;  p += w)  interior += integ( hstep(p,-1.),hstep(p,1.) ); */
  for ( p=w; p < freqs;  p += w)  {
    band = pitch_band(p);
    interior += integ( p-band,p+band ); 
  }
  return(1 - interior/power);
} 

float fund_stat(state)   /* this shift in fundamental energy  change */
FULL_STATE state; {

 
  float power=0,interior = 0, exterior = 0,w,p,band,total=0;
  int i,low,high,n;

  n = state.arg[THIS_PITCH];
  for (i=0; i < freqs; i++)  power += (spect[i]+xxx_last_spect[i]);
  w = sol[n].omega;
  band = pitch_band(w);
  low = w-band;
  high = w+band+.5;
  for ( i=low; i <=  high;  i++)  total += fabs(xxx_last_spect[i] - spect[i]); 
  return(-total/power);  
} 

float color_stat(state)   /* this shift in fundamental energy  change */
FULL_STATE state; {

 
  float power=0,interior = 0, exterior = 0,w,p,band,total=0;
  int i,low,high,n;

  n = state.arg[THIS_PITCH];
  w = sol[n].omega;
  for ( p=w; p < freqs;  p += w) {
    band = pitch_band(p);
    low = w-band;
    high = w+band+.5;
    for ( i=low; i <=  high;  i++)  {
      total += fabs(xxx_last_spect[i] - spect[i]); 
      power += (xxx_last_spect[i] + spect[i]);
    }
  }
  return(-total/power);
} 


float octave_stat(state)
   /* stats returned ins s1 and s2 are power in exterior int/int+ext for note */ 
FULL_STATE state; {

 
  float power=0,interior = 0, exterior = 0,w,p,band;
  int i,n;

  n = state.arg[THIS_PITCH];
  w = sol[n].omega;
  for ( p=w; p < freqs;  p += w) {
    band = pitch_band(p);
    power += integ( p-band,p+band ); 
  }
  for ( p=w; p < freqs;  p += 2*w) {
    band = pitch_band(p);
    interior += integ( p-band,p+band ); 
  }
  return(1-interior/power);
} 

float two_pitch_stat(state)
FULL_STATE state; {

 
  float power=0,interior = 0, exterior = 0,w1,w2,p1,p2,band,low,high,bot,top;
  int i,n;

  for (i=0; i < freqs; i++)  power += spect[i];
  p1 = w1 = sol[state.arg[THIS_PITCH]].omega;
  p2 = w2 = sol[state.arg[LAST_PITCH]].omega;
  band = pitch_band(w1);
  do {
    if (p1 < p2) { low = p1; high = p2; }
    else {low = p2; high = p1; }
    band = pitch_band(low);
    top = (high-band < low+band) ? high-band : low+band;
    bot = low-band;
    interior += integ(bot,top);
    if (p1 < p2) p1 += w1;
    else p2 += w2;
  } while (p1 < freqs && p2 < freqs);
  return(1-interior/power);
} 


float probw(n)  /* prob of pitch n */
int n; {
  float extstat,stat;
 
  if (pr[n] != UNSET) return(pr[n]);
  stat = pitchstat(n);
  pr[n] = lookup(stat,&noted);
  return(pr[n]);
}



#define TSLUR .5


    
float slurstat()  { /* probability a slur from one note to another is here */
  float stat,power=0,p1=0,p2=0,p3=0,p4=0,diff,inc;
  int   i;

  for (i=0; i < TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p1 += inc;
   }
  for (i=TOKENLEN/3; i < 2*TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p2 += inc;
   }
  for (i=2*TOKENLEN/3; i < TOKENLEN; i++) {
    power += (inc = data[i]*data[i]);
    p3 += inc;
   }
  stat= 1 - fabs( p1 - 2*p2 + p3)/power ;
    
  return(stat); 
}

float slur_stat(state)
FULL_STATE state; {
  float stat,power=0,p1=0,p2=0,p3=0,p4=0,diff,inc;
  int   i;

  for (i=0; i < TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p1 += inc;
   }
  for (i=TOKENLEN/3; i < 2*TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p2 += inc;
   }
  for (i=2*TOKENLEN/3; i < TOKENLEN; i++) {
    power += (inc = data[i]*data[i]);
    p3 += inc;
   }
  stat= 1 - fabs( p1 + p3 - 2*p2)/power ;
  return(stat); 
}


float tongue_stat(state)
FULL_STATE state; {
  float stat,power=0,p1=0,p2=0,p3=0,p4=0,diff,inc;
  int   i;

  for (i=0; i < TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p1 += inc;
   }
  for (i=TOKENLEN/3; i < 2*TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p2 += inc;
   }
  for (i=2*TOKENLEN/3; i < TOKENLEN; i++) {
    power += (inc = data[i]*data[i]);
    p3 += inc;
   }
  stat= -fabs( p1 + p3 - 2*p2)/sqrt(power) ;
    
  return(stat); 
}




#define SLUR_LEN 85

float xslurstat() {
  float power[100],mx=0,x;
  int i,d,j,b=0;

  d = TOKENLEN/SLUR_LEN;
  for (j=0; j < d; j++) {
    power[j] = 0;
    for (i=0; i < SLUR_LEN; i++) power[j] += data[b+i]*data[b+i];
    b += SLUR_LEN;
  }
  for (j=0; j < d-2; j++) {
    x = 1 - fabs((power[j] - 2*power[j+1] + power[j+2])/(power[j]+power[j+1]+power[j+2]));
    if (x > mx) mx = x;
  }
  return(mx);
}

  


float probslur()  { /* probability a slur from one note to another is here */
  float power=0,p1=0,p2=0,diff,inc,stat;
  int   i;

  if (chgpr != UNSET) return(chgpr);
  stat = slurstat();
  chgpr = lookup(stat,&changed);
  return(chgpr);
}
    


float articstat()  { /* probability a slur from one note to another is here */
  float stat,power=0,p1=0,p2=0,p3=0,p4=0,diff,inc;
  int   i;

  for (i=0; i < TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p1 += inc;
   }
  for (i=TOKENLEN/3; i < 2*TOKENLEN/3; i++) {
    power += (inc = data[i]*data[i]);
    p2 += inc;
   }
  for (i=2*TOKENLEN/3; i < TOKENLEN; i++) {
    power += (inc = data[i]*data[i]);
    p3 += inc;
   }
  stat=  1 - fabs( p1 - 2*p2 + p3)/power ;
    
  return(stat); 
}

float probartic()  { /* probability a slur from one note to another is here */
  float power=0,p1=0,p2=0,diff,inc,stat;
  int   i;

  if (articpr != UNSET) return(articpr);
  stat = articstat();
  articpr = lookup(stat,&articd);
  return(articpr);
}
    


float reststat() { /* assumed to be chisquare dist */
  float power=0;
  int   i;

  for (i=0; i < TOKENLEN; i++) power += data[i]*data[i];
  power /= TOKENLEN; 
/*  return(log(power));*/
  return(10000*power);
}

float rest_stat(state)
FULL_STATE state; {
  float power=0;
  int   i;

  for (i=0; i < TOKENLEN; i++) power += data[i]*data[i];
  power /= TOKENLEN; 
/*  return(log(power));*/
  return(10000*power);
}
    
float probrest() { /* probability of a rest */
  float power=0,stat;
  int   i;

  if (restpr != UNSET) return(restpr);
  stat = reststat(); 
  restpr = lookup(stat,&restd);
  return(restpr);
}



float prob_state(st,n)  
int st;              /* the state */
int n;            /* pitch number (if needed) */ {
  float extstat,stat,total=0,x,p;
  int d,type; 
  DIST *ds;

  if (state_pr[st][n] != UNSET) return(state_pr[st][n]);
  for (d=0; d < state[st].dist_num; d++) {
    ds = state[st].dist + d;
    stat = stat_func[ds->type](n);
    p = state_pr[st][n] = lookup(stat,ds);
/*    


    switch(ds->type) {
      case REST_STAT   : if (restpr == UNSET) {
                           stat = reststat();
                           p = restpr = lookup(stat,ds);
                         }
                         else p = restpr;
                         break;
      case ATTACK_STAT : if (chgpr == UNSET) {
                           stat = slurstat(); 
                           p = chgpr = lookup(stat,ds);
                         }
                         else p = chgpr; 
                         break;
      case PITCH_STAT   : if  (pr[n] == UNSET) {
                           stat =  pitchstat(n);
                           p = pr[n] = lookup(stat,ds);
                         } 
                         else p = pr[n];
                         break;
      case OCTAVE_STAT : if  (oct[n]== UNSET) {
                           stat =  octavestat(n);
                           p =  oct[n] = lookup(stat,ds);
                         }
                         else p = oct[n];
                         break;
    } */
    total += p;
  }
  return(total);
}  

    
#define LOW 7
#define HIGH 255


static int fcompare(const void *p1, const void *p2) {
  float *i,*j; 

  i = (float *) p1;
  j = (float *) p2;
  if (*j > *i) return(-1);
   return(1);

}  
 
 
#define MINCOUNT 2


print_dist(dist)
DIST *dist; {
  int k;

  for (k = 0; k < QUANTA; k++)   printf("%d %f %f\n",k,dist->div[k],dist->prob[k]);
}

print_dists() {
  int k,i,j;

  for (i=0; i < STATE_NUM; i++) {
    printf("state = %d\n",i);
    for (j=0; j < state[i].dist_num; j++)  printf("dist = %d             ",j);
    printf("\n");
    for (k = 0; k < QUANTA; k++) {
      printf("%d ",k);
      for (j=0; j < state[i].dist_num; j++)  
        printf("%f %f  ",state[i].dist[j].div[k],state[i].dist[j].prob[k]);
      printf("\n");
    }
    printf("\n");
  }
}




idist(n,x,dist)  /* initialize a dist */
float *x;
int n;
DIST *dist;  {
  int c=0,k;

  qsort(x,n,sizeof(float),fcompare );
  for (k = 0; k < QUANTA; k++)  dist->div[k]   = x[n*(2*k+1)/(2*QUANTA)];
  for (k = 0; k < QUANTA; k++) {
    dist->prob[k] = pow(.5,(float)k);
    dist->prob[k] = log(dist->prob[k]);
  }
  print_dist(dist);
}


DIST nd,rd,cd;


#define MIN_DIFF .025

same_dist(d1,d2)
DIST  *d1,*d2; {
  int k;
  for (k = 0; k < QUANTA; k++)  
   if (fabs(d1->prob[k] - d2->prob[k]) > MIN_DIFF)   return(0);
  return(1);
}




set_data() {
  int i,n,nn,l,j,p,s,type;
  float prob;
  DATUM d;
  FULL_STATE *s1,*s2,fs;
  STAT_INDEX si;
  STAT_REC *sr;
 
  for (i=0; i < frames; i++) {
    token = i;
    clear_probs();
    get_hist();
    for (n=0; n < prob_hist[i].num; n++) {
      s1 = &(prob_hist[i].list[n].place->state);
      prob = prob_hist[i].list[n].prob;
      for (nn=n+1; nn < prob_hist[i].num; nn++) {
        s2 = &(prob_hist[i].list[nn].place->state);
        if (states_equal(s1,s2)) {
          prob +=prob_hist[i].list[nn].prob;
          prob_hist[i].list[nn].prob =0;
        }
      }
      if (prob > 0) {
        fs  = prob_hist[i].list[n].place->state;
        s = fs.statenum;
        d.prob = prob;
        l = state[s].dist_num;
        d.val = (float *) malloc(l*sizeof(float));
        for (j=0; j < l; j++)  {
          si.statnum =  type = state[s].type[j];
          for (nn=0; nn < ARG_NUM; nn++) 
            si.arg[nn] = (arg_bool[type][nn]) ? fs.arg[nn] : IRLVNT;
          sr = find_rec(si);
          if (sr == NULL)  printf("stat not found\n");
          else d.val[j] = sr->val;
/*           for (nn=0; nn < hist[i].num; nn++)
            if (states_equal(&fs,&(stat_hist[i].list[nn].si))) {
               break;
            }
          if (nn == hist[i].num)  */
        }
        add_data(exam+s,d);
      }
    }
  }
}


      
void
trainprobs() {
  char file[500],ans[10];
  int t,s,i,n;
  float prob,*x;
  DATUM d;

  printf("train from scratch? (y or n):");
  scanf("%s",ans);
  if (ans[0] == 'y') { 
    printf("enter the training score (.score and .au file must exist): ");
    scanf("%s",scorename);
    if (readscore() == 0) { 
      printf("problem in readscore()\n"); 
      return; 
    }
    if ((t=readaudio()) == 0) { 
      printf("problem in readaudio()\n"); 
      return; 
    } 
/*    set_filter();
    write_filter();
    read_filter();    */
    init_data_files();

    train_marg(t);  /*return; read_marg(); */
    init_states(); 
    /*write_splines(); */
    write_trees();
    write_data();
    return;  
  }
    printf("enter the training score (.score and .au file must exist): ");
    scanf("%s",scorename);
    if (readscore() == 0) { 
      printf("problem in readscore()\n"); 
      return; 
    }
    if ((t=readaudio()) == 0) { 
      printf("problem in readaudio()\n"); 
      return; 
    }    

    read_data();
/*  read_splines();*/
/*  read_trees();    */
print_like_trees();
/*  readtable();  /* ???? */

  make_dp_graph();
/*  init_stat_hist();*/

  for (i=0; i < 4; i++) {
  init_prob_hist();
  mode = FORWARD_MODE;
  olinit();
  dp();



  mode = BACKWARD_MODE;
  olinit();
  dp();
/*mean_parse(); myaddclick(); exit(0);*/
/*  write_prob_hist(); */


  for (s=0; s < STATE_NUM; s++)  {
    marg[s] = init_data(frames,state[s].dist_num);
    exam[s] = init_data(frames,state[s].dist_num);
  }

  set_data(); 
  read_data(); 



/*  printf("want to add marginals? ");
  scanf("%s",ans);
  if (ans[0] == 'y')  train_marg(frames); */

  train_trees(frames);
  write_trees();

/*  train_splines(frames);
  write_splines();  */
}
  write_data();
/*  myaddclick();  */
}




void
x_init_dist() {  /* initial output distributions */
  int i,j,fd,length,k,rsx,csx,nsx,rscx,nscx,cscx,asx,ascx,rcount,ncount,ccount,n,rp;
  float ext,rat;
  float *rs,*rsc,*cs,*csc,*ns,*nsc,*as,*asc;

  if ( (length = readaudio()) == 0) return;
  rs = (float *) malloc(sizeof(float)*length);
  cs = (float *) malloc(sizeof(float)*length);
  ns = (float *) malloc(sizeof(float)*length);
  for (i=0; i < length; i++) {
    audio_listen();
    rs[i] = reststat();
    cs[i] = slurstat();
    rp = MIDI_MIDDLE_C + (rand()%24);  /* a note in oboe range */
    ns[i] = pitchstat(rp);
  }
  printf("note probs\n");
  idist(length,ns,&noted);
  printf("rest probs\n");
  idist(length,rs,&restd);
  printf("change probs\n");
  idist(length,cs,&changed);
  writedist();
}
      


void
init_dist() {  /* initial output distributions */
  float **data;
  int i,j,k,type,rp,length,l;
  DIST d[STAT_NUM];

  if ( (length = readaudio()) == 0) return;
  data = (float **) malloc(STAT_NUM*sizeof(float**));
  for (i=0; i < STAT_NUM; i++) data[i] = (float *) malloc(length*sizeof(float));
  for (i=0; i < length; i++) {
    audio_listen();
    rp = MIDI_MIDDLE_C + (rand()%24);  /* a note in oboe range */
    for (j=0; j < STAT_NUM; j++)  data[j][i] = stat_func[j](rp);
  }
  for (j=0; j < STAT_NUM; j++)  qsort(data[j],length,sizeof(float),fcompare );
  for (i=0; i < STATE_NUM; i++) {
    for (j=0; j < state[i].dist_num; j++) {
      type = state[i].dist[j].type;
      for (k = 0; k < QUANTA; k++) state[i].dist[j].div[k] = data[type][length*k/QUANTA];
      state[i].dist[j].div[QUANTA] = data[type][length-1];
    }
    for (k = 0; k < QUANTA; k++) state[i].dist[0].prob[k] =  pow(.5,(float)k);
    for (j=1; j < state[i].dist_num; j++) 
      for (k = 0; k < QUANTA; k++) state[i].dist[j].prob[k] = 0;
  }
  print_dists();
}
      




init_template() {
  int i,s,j,type;

  stat_func[PITCH_STAT] = pitch_stat;
  stat_func[TWO_PITCH_STAT] = two_pitch_stat;
  stat_func[ENERGY_STAT] = rest_stat;
  stat_func[OCTAVE_STAT] = octave_stat;
  stat_func[BURST_STAT] = slur_stat;
  stat_func[SIGNED_BURST_STAT] = tongue_stat;
  stat_func[FUND_STAT] = fund_stat;
  stat_func[COLOR_STAT] = color_stat;

  arg_bool[PITCH_STAT][THIS_PITCH] = 1;
  arg_bool[PITCH_STAT][LAST_PITCH] = 0;
  arg_bool[ENERGY_STAT][THIS_PITCH] = 0;
  arg_bool[ENERGY_STAT][LAST_PITCH] = 0;
  arg_bool[OCTAVE_STAT][THIS_PITCH] = 1;
  arg_bool[OCTAVE_STAT][LAST_PITCH] = 0;
  arg_bool[BURST_STAT][THIS_PITCH] = 0;
  arg_bool[BURST_STAT][LAST_PITCH] = 0;
  arg_bool[SIGNED_BURST_STAT][THIS_PITCH] = 0;
  arg_bool[SIGNED_BURST_STAT][LAST_PITCH] = 0;
  arg_bool[COLOR_STAT][THIS_PITCH] = 1;
  arg_bool[COLOR_STAT][LAST_PITCH] = 0;
  arg_bool[FUND_STAT][THIS_PITCH] = 1;
  arg_bool[FUND_STAT][LAST_PITCH] = 0;
  arg_bool[TWO_PITCH_STAT][THIS_PITCH] = 1;
  arg_bool[TWO_PITCH_STAT][LAST_PITCH] = 1;

  arg_num[PITCH_STAT] = 1;
  arg_num[ENERGY_STAT] = 0;
  arg_num[OCTAVE_STAT] = 1;
  arg_num[BURST_STAT] = 0;
  arg_num[SIGNED_BURST_STAT] = 0;
  arg_num[FUND_STAT] = 1;
  arg_num[COLOR_STAT] = 1;
  arg_num[TWO_PITCH_STAT] = 2;

  for (i=0; i < STATE_NUM; i++) {
    s = template[i][0];
    state[s].dist_num = template[i][1];
    state[s].dist = (DIST *) malloc(template[i][1]*sizeof(DIST));
    for (j=0; j < state[s].dist_num; j++)  state[s].dist[j].type = template[i][2+j];
    for (j=0; j < state[s].dist_num; j++) {
      state[s].stat_fun[j]  = stat_func[template[i][2+j]];
      state[s].type[j]  = template[i][2+j];
    }
  }
}


init_data_files() {
  int s;

  for (s=0; s < STATE_NUM; s++)  {
    marg[s] = init_data(MAX_DATA,state[s].dist_num);
    exam[s] = init_data(MAX_DATA,state[s].dist_num);
  }
}


int
train_marg(frm)   /* initialize  and write marginal statistics */
int frm;  {  /* number of frames */
  int i,j,k,type,rp,rp2,length,l,s,arg[ARG_NUM];
  FILE *fp;
  char name[500];
  float temp[STAT_NUM];
  DATUM d;
  FULL_STATE fs;

/*  if ( (length = readaudio()) == 0) return;*/
  token = -1;
  for (i=0; i < frm; i++) {
    audio_listen();
    fs.arg[THIS_PITCH]  = MIDI_MIDDLE_C + (rand()%24);  /* a note in oboe range */
    fs.arg[LAST_PITCH] = MIDI_MIDDLE_C + (rand()%24);  /* a note in oboe range */
    for (s=0; s < STATE_NUM; s++) {
      l = state[s].dist_num;
      d.val = (float *) malloc(l*sizeof(float));
      for (j=0; j < l; j++)  {
        type = state[s].type[j];     
        d.val[j] = stat_func[type](fs);
        d.prob = 1.;
      }
      if (marg[s].num < MAX_DATA) add_data(marg+s,d);
      else return(0);
    }
  }
  write_data();
/*  strcpy(name,scorename);
  strcat(name,".mar");
  fp = fopen(name,"w");
  for (j=0; j < STATE_NUM; j++) { 
    fwrite(&(marg[j].num),sizeof(int),1,fp);
    fwrite(&(marg[j].size),sizeof(int),1,fp);
    fwrite((marg[j].list),sizeof(float),marg[j].size*marg[j].num,fp);
this won't work with new verstion of DATA sturct 
  }
  fclose(fp); */
}    

#define EXAMPLE_FILE "example.dat"
#define MARGINAL_FILE "marginal.dat"


read_marg() {  /* read in  marginal statistics */
  int s,length;
  FILE *fp;
  char name[500];

/*  strcpy(name,scorename);
  strcat(name,".mar");
  fp = fopen(name,"r"); */
  fp = fopen(MARGINAL_FILE,"r");
  if (fp == NULL) printf("couldn't open %s\n",MARGINAL_FILE);
  for (s=0; s < STATE_NUM; s++) { 
    fread(&length,sizeof(int),1,fp);
    marg[s] = init_data(MAX_DATA,state[s].dist_num);
    marg[s].num = length;
    fread(&(marg[s].size),sizeof(int),1,fp);
    if (state[s].dist_num != marg[s].size) printf("problem in read_marg()\n");
    fread(marg[s].list,sizeof(float),marg[s].size*marg[s].num,fp);
  }
  fclose(fp);
}    
  

read_d(fp,dat)   /* read in  marg or  exam */
FILE *fp;
DATA *dat; {
  int s,length,i;
  char name[500];

  for (s=0; s < STATE_NUM; s++) { 
    fread(&length,sizeof(int),1,fp);
    dat[s] = init_data(MAX_DATA,state[s].dist_num);
    dat[s].num = length;
 printf("state = %d length = %d\n",s,length);
    fread(&(dat[s].size),sizeof(int),1,fp);
    if (state[s].dist_num != dat[s].size) printf("problem in read_d()\n");
    fread(&(dat[s].total),sizeof(float),1,fp);
    for (i=0; i < dat[s].num; i++) {
      fread(&(dat[s].list[i].prob),sizeof(float),1,fp);
      dat[s].list[i].val = (float *) malloc(sizeof(float)*dat[s].size);
      fread(dat[s].list[i].val,sizeof(float),dat[s].size,fp);
    }
  }
}    
  
write_d(fp,dat)   /* write in  marg or  exam */
FILE *fp;
DATA *dat; {
  int s,length,i;
  char name[500];

  for (s=0; s < STATE_NUM; s++) { 
    fwrite(&(dat[s].num),sizeof(int),1,fp);
 printf("state = %d length = %d\n",s,dat[s].num);
    fwrite(&(dat[s].size),sizeof(int),1,fp);
    fwrite(&(dat[s].total),sizeof(float),1,fp);
    for (i=0; i < dat[s].num; i++) {
      fwrite(&(dat[s].list[i].prob),sizeof(float),1,fp);
      fwrite(dat[s].list[i].val,sizeof(float),dat[s].size,fp);
    }
  }
}    




int
read_data() {  /* read in  marg and exam */
  int s,length;
  FILE *fp;
  char name[500];

/*  strcpy(name,scorename);
  strcat(name,".mar"); */
  fp = fopen(MARGINAL_FILE,"r");
  if (fp == NULL) printf("couldn't open %s\n",MARGINAL_FILE);
printf("marginal:\n");
  read_d(fp,marg);
printf("\n");
return(0);
  fclose(fp);
/*  strcpy(name,scorename);
  strcat(name,".exam"); */
  fp = fopen(EXAMPLE_FILE,"r");
  if (fp == NULL) printf("couldn't open %s\n",EXAMPLE_FILE);
printf("examples:\n");
  read_d(fp,exam);
printf("\n");
  fclose(fp);
}    
  


write_data() {  /* read in  marg and exam */
  int s,length;
  FILE *fp;
  char name[500];

/*  strcpy(name,scorename);
  strcat(name,".mar"); */
  fp = fopen(MARGINAL_FILE,"w");
printf("marginal:\n");
  write_d(fp,marg);
printf("\n");
  fclose(fp);
/*  strcpy(name,scorename);
  strcat(name,".exam");*/
  fp = fopen(EXAMPLE_FILE,"w");
printf("examples:\n");
  write_d(fp,exam);
printf("\n");
  fclose(fp);
}    
  





init_like_tree(t,data,n,p)
TNODE **t;
DATA data;
int n;
float p; {
  
  *t = (TNODE *) malloc(sizeof(TNODE));
  (*t)->split_stat = 0;
  (*t)->cutoff = (data.list[n/2].val[0] + data.list[n/2+1].val[0])/2;




  (*t)->x = data.list[n/4].val[0];
  (*t)->y = data.list[3*n/4].val[0];
  (*t)->x = data.list[n/2].val[0];
  (*t)->y = data.list[n/2].val[0]; 
  (*t)->fx = 2.;
  (*t)->fy = 2.; 

  (*t)->logq = log(1./p);  
  (*t)->quot = 1./p;  
  if (p > .03 && n > 0) {
    init_like_tree(&((*t)->rc),data,0,p/2.);
    init_like_tree(&((*t)->lc),data,n/2,p/2);
  }
  else (*t)->rc = (*t)->lc = NULL;
}




  
init_spline(s,data,st)
SPLINE *s;
DATA data;
int st; {
  int i,c;
  
  s[0].num = c = MAX_CELLS-1;
  for (i=0; i < c; i++) {
    s[0].breakpt[i] = data.list[(i*data.num)/c].val[0];
    s[0].val[i] = pow(.5,(float) i);
  }
  s[0].breakpt[c] = data.list[data.num-1].val[0];
  for (i=1; i < state[st].dist_num; i++) {
    s[i].num=1;
    s[i].breakpt[0] = s[i].breakpt[1] = 0;
    s[i].val[0] = 1;
  }
}

old_init_spline(s,data,n)
SPLINE_TREE **s;
DATA data;
int n; {
  int i,c=MAX_CELLS-1;
  
  
  *s = (SPLINE_TREE *) malloc(sizeof(SPLINE_TREE));
  (*s)->split_stat = 0;
  (*s)->num = c;
  for (i=0; i < c; i++) {
    (*s)->breakpt[i] = data.list[(i*n)/c].val[0];
    (*s)->val[i] = pow(.5,(float) i);
    (*s)->child[i] = NULL;
  }
  (*s)->breakpt[c] = data.list[n-1].val[0];
}

  
void
print_like_tree(t,l)
TNODE *t; 
int l;  {
  int i;

  if (t == NULL) return;
  print_like_tree(t->lc,l+1);
  for (i=0; i < l; i++) printf(" ");
  printf("type = %d cutoff = %f score = %f\n",t->split_stat,t->cutoff,t->quot);
/*  printf("type = %d (x,y) = (%f,%f) (f(x),f(y)) = (%f,%f)\n",t->split_stat,t->x,t->y,t->fx,t->fy); */
  print_like_tree(t->rc,l+1);
}

print_like_trees() {
  int s;

  for (s=0; s < STATE_NUM; s++) {
    print_like_tree(state[s].root,0);
    printf("\n");
  }
}

add_hist(id)
STAT_ID id; {
  float stat;
  int i,n;

/*  if (hist[token].num >= HIST_LIM) { 
printf(" out of room in hist\n"); 
/*for (i=0; i < HIST_LIM; i++) printf("%d %d %d\n",i,hist[token].list[i].type,hist[token].list[i].pitch); */
/*return; }
  for (i=0; i < hist[token].num; i++)
  if (hist[token].list[i].type == s && hist[token].list[i].pitch == p) return;
  stat = stat_val[s][p];
  n = hist[token].num;
  hist[token].list[n].type = s;
  hist[token].list[n].pitch = p;
  hist[token].list[n].stat= stat;
  hist[token].num++; */
}

compute_stats(fs) /* all stats are computed even if not needed */
FULL_STATE fs; {
  int i,type,n,s;
  STAT_REC rec,*find;

  s = fs.statenum;

  for (i=0; i < state[s].dist_num; i++) {
    rec.si.statnum =  type = state[s].type[i];
    for (n=0; n < ARG_NUM; n++) 
      rec.si.arg[n] = (arg_bool[type][n]) ? fs.arg[n] : IRLVNT;
    if (find_rec(rec.si) == NULL) {
      rec.val  = stat_func[type](rec.si);
      add_rec(rec);
    }
  }
}


float tree_like(fs)
FULL_STATE fs; {
  float f[STAT_NUM],total=1,p,q,val;
  int i,type,n,s;
  TNODE *t;
  STAT_REC rec,*find;

  s = fs.statenum;
  t = state[s].root;
  while (t->lc != NULL) {
    rec.si.statnum = type = state[s].type[t->split_stat];
    for (n=0; n < ARG_NUM; n++) 
      rec.si.arg[n] = (arg_bool[type][n]) ? fs.arg[n] : IRLVNT;
    if ((find=find_rec(rec.si)) == NULL) {
      val = rec.val  = stat_func[type](rec.si);
      add_rec(rec);
    }
    else val = find->val; 
    if (val <= t->x)  total *= t->fx;
    else if (val >= t->y)  total *= t->fy;
    else {
     p = (val - t->x)/(t->y - t->x);
     q = 1-p;
     total *= (q*t->fx + p*t->fy); 
    } 
    if (val < t->cutoff) t = t->lc;
    else t = t->rc; 
  }
  return(t->quot);  
} 


save_hist() {
  int i,s;


/*  stat_hist[token].num = 0;
  for (s=0; s < STAT_NUM; s++)   for (i=0; i < stat_rec[s].num; i++) 
      stat_hist[token].list[stat_hist[token].num++] = stat_rec[s].list[i]; */

}


get_hist() {
  int i,s;

/*  for (i=0; i < stat_hist[token].num; i++) 
    add_rec(stat_hist[token].list[i]); */
}




#define RAMP .5

float spline_like(fs)
FULL_STATE fs; {
  float f[STAT_NUM],total=1,p,q,highval,lowval,midval,high,low,val;
  int i,type,m,x,s,n;
  SPLINE *spline;
  STAT_REC rec,*find;


if (token == 0)
printf("");

    s = fs.statenum;
/*  if (state_logq[s][n] != UNSET) return(state_logq[s][n]); */
  for (i=0; i < state[s].dist_num; i++) {
    rec.si.statnum =  type = state[s].type[i];
    for (n=0; n < ARG_NUM; n++) 
      rec.si.arg[n] = (arg_bool[type][n]) ? fs.arg[n] : IRLVNT;
    if (find_rec(rec.si) == NULL) {
      if (new_data == 0) samples2data();  /*  should happen in offline modes*/
      rec.val  = stat_func[type](rec.si);
      add_rec(rec);
    }
  }
  for (m=0; m < state[s].dist_num; m++) {
    spline = state[s].marg+m;
    rec.si.statnum =  type = state[s].type[m];
    for (n=0; n < ARG_NUM; n++) 
      rec.si.arg[n] = (arg_bool[type][n]) ? fs.arg[n] : IRLVNT;
    if ((find=find_rec(rec.si)) == NULL) {
       printf("look at spline_like()\n");
       find_rec(rec.si);
     }

/*    rec.state.type =  type = state[s].type[m];
    val  = stat_func[type](st); */

    val = find->val; 

    for (i=0; i <= spline->num; i++) 
      if (spline->breakpt[i] > val) break;
    if (i==0) {
      low = val;
      high = spline->breakpt[0];
      highval = midval = lowval = spline->val[0];
    }
    else if (i==1) {
      low = spline->breakpt[i-1];
      high = spline->breakpt[i];
      lowval =  midval = spline->val[i-1];   
      highval = spline->val[i-0];   
    }   
    else if (i == spline->num+1) {
      high = val;
      low = spline->breakpt[spline->num];
      highval = midval = lowval = spline->val[spline->num-1];
    }  
    else if (i == spline->num) {
      low = spline->breakpt[i-1];
      high = spline->breakpt[i];
      lowval = spline->val[i-2];   
      highval =  midval = spline->val[i-1];   
    }
    else {
      low = spline->breakpt[i-1];
      high = spline->breakpt[i];
      lowval = spline->val[i-2];   
      midval = spline->val[i-1];   
      highval = spline->val[i-0];   
    }
    p = (val - low)/(high-low);
    if (spline->num > 0) {
      if (p < RAMP) {
        p /= RAMP;
        q = 1-p;
        total *= (p*midval + q*(lowval+midval)/2);    
      }
      else if (p > 1-RAMP) {
        p = 1-p;
        p /= RAMP;
        q = 1-p;
        total *= (p*midval + q*(midval+highval)/2);    
      }
      else total *= midval;
    }
if (total <= 0.)
printf("fick\n");    
  }
  return( /*state_logq[s][n] = */total);  
} 


int stat_key;


static int stat_compare(const void *p1, const void *p2) {
  DATUM *i,*j;

  i = (DATUM *) p1;
  j = (DATUM *) p2;
  if ((j->val)[stat_key] > (i->val)[stat_key]) return(-1);
  if ((j->val)[stat_key] < (i->val)[stat_key]) return(1);
  return(0);

}  




init_states() {  /* train on 0th stat.  assume decreasing distribution */
  int i,t,j,length,rp,s;
  FILE *fp;
  char name[500];

  stat_key = 0;  /* train on 0th stat for each state */
  for (s=0; s < STATE_NUM; s++) {
    qsort(marg[s].list,marg[s].num,sizeof(DATUM),stat_compare );
/*    init_like_tree(&(state[s].root),marg[s].list,marg[s].num,1.,marg[s].size);*/
    init_like_tree(&(state[s].root),marg[s],marg[s].num,1.);
/*    init_spline(state[s].marg,marg[s],s); */
  }
/*    print_like_trees(); */
}


/*ws(s,fp)
SPLINE *s;
FILE *fp; {

  int i;

  
  fprintf(fp,"%d %d\n",s->num,s->split_stat);
  for (i=0; i < s->num; i++) {
    fprintf(fp,"%f %f %d \n",s->breakpt[i],s->val[i],s->child[i]);
    if (s->child[i] != NULL) ws(s->child[i],fp);
  }
  fprintf(fp,"%f\n",s->breakpt[i]);
}*/

wt(t,fp)
TNODE *t;
FILE *fp; {

  fprintf(fp,"%d %f %f %d\n",t->split_stat,t->cutoff,t->quot,(t->lc == NULL));
/*  fwrite(t,sizeof(TNODE),1,fp); */
  if (t->lc != NULL) {
    wt(t->lc,fp);    
    wt(t->rc,fp);    
  }
}

#define TREE_FILE "tree.dat"
#define SPLINE_FILE "spline.dat"

write_trees() {
  FILE *fp;
  char name[500];
  int j;
			
/*  strcpy(name,scorename);
  strcat(name,".tree"); */
  fp = fopen(TREE_FILE,"w");
  for (j=0; j < STATE_NUM; j++) {
    wt(state[j].root,fp);
    fprintf(fp,"\n");
  }
  fclose(fp);
}    
  

write_splines() {
  FILE *fp;
  char name[500];
  int k,j,i;
			
  fp = fopen(SPLINE_FILE,"w");
  for (j=0; j < STATE_NUM; j++) {
    for (i=0; i < state[j].dist_num; i++) {
      fprintf(fp,"%d\n",state[j].marg[i].num);
      for (k=0; k <= state[j].marg[i].num; k++) 
        fprintf(fp,"%f ",state[j].marg[i].breakpt[k]);
      fprintf(fp,"\n");
      for (k=0; k <= state[j].marg[i].num; k++) 
        fprintf(fp,"%f ",state[j].marg[i].val[k]);
      fprintf(fp,"\n");
    }
  }
  fclose(fp);
}    
  
read_splines() {
  FILE *fp;
  char name[500];
  int k,j,i;
			
  fp = fopen(SPLINE_FILE,"r");
  for (j=0; j < STATE_NUM; j++) {
    for (i=0; i < state[j].dist_num; i++) {
      fscanf(fp,"%d\n",&state[j].marg[i].num);
      for (k=0; k <= state[j].marg[i].num; k++) 
        fscanf(fp,"%f ",state[j].marg[i].breakpt+k);
      for (k=0; k <= state[j].marg[i].num; k++) 
        fscanf(fp,"%f ",state[j].marg[i].val+k);
    }
  }
  fclose(fp);
/*  read_filter();   /* not really the right way to deal with this */
}    
  


rt(t,fp)
TNODE **t;
FILE *fp; {
  int term;
  
  *t = (TNODE *) malloc(sizeof(TNODE));
  fscanf(fp,"%d %f %f %d\n",
    &((*t)->split_stat),&((*t)->cutoff),&((*t)->quot),&term);
 /*  fread(*t,sizeof(TNODE),1,fp); */
  if (term) (*t)->lc = (*t)->rc = NULL;
  else {
    rt(&((*t)->lc),fp);    
    rt(&((*t)->rc),fp);    
  }
}

rs(s,fp)
SPLINE_TREE **s;
FILE *fp; {
  int i;
  
  *s = (SPLINE_TREE *) malloc(sizeof(SPLINE_TREE));
  fscanf(fp,"%d %d\n",&((*s)->num),&((*s)->split_stat));
  for (i=0; i < (*s)->num; i++) {
    fscanf(fp,"%f %f %d\n",
      &((*s)->breakpt[i]),&((*s)->val[i]),&((*s)->child[i]));
    if ((*s)->child[i] != NULL) rs(&((*s)->child[i]),fp);
  }
  fscanf(fp,"%f\n",&((*s)->breakpt[i]));
  if ((*s)->num == 0) *s = NULL;
}

void
read_trees() {
  FILE *fp;
  char name[500];
  int j;
			
/*  strcpy(name,scorename);
  strcat(name,".tree"); */
  fp = fopen(TREE_FILE,"r");
  if (fp == NULL) {
    printf("couldn't read trees \n");
    return;
  }
  for (j=0; j < STATE_NUM; j++) {
    rt(&(state[j].root),fp);
    fscanf(fp,"\n");
  }
  fclose(fp);
/*  read_filter();   /* not really the right way to deal with this */
/*  print_like_trees(); */
}    
  
  


typedef float SUPER_STAT[STAT_NUM];


typedef struct {
  SUPER_STAT *list;
  int   num;
} SUPER_DATA;

/*typedef DATA MARG_LIST[STAT_NUM];*/



#define MIN_EXAMP 40   /* need at least this many examples to train tree */





#define MIN_SAMPS  10
#define FUDGE  5    /* used as smoother */

#define MIN_DISC  10
#define MIN_TOT    5  /* each split must have this many expected numbers of examles */



#define ZLIM 2.

float test_stat(phat1,phat2,n1,n2) 
float phat1,phat2,n1,n2; {
/* returns normal deviate for hyp test. phat1 is estimate for sucess out of
   n1 bernoulii trials.  sim for phat2.  retrun is test statistic for p1
   different from p2. */

  float z;

  z = fabs(phat1-phat2) / sqrt(1/n1 + 1/n2);
  return (z < ZLIM) ? -HUGE_VAL : z ;
}



void
tt(int st,DATA ml,DATA ex,float mprob,float exprob,TNODE **t,int level) {
  int i,j,k,best_i,mid,type,mlen,xlen,jex,jml,best_jex,best_jml;
  float best_score,cutoff,p,q,score,pml,qml,pex,qex,best_cut,best_pex,best_qex,best_pml,best_qml,mltot,extot;
  DATA sub_ml,sub_ex;



  *t = (TNODE *) malloc(sizeof(TNODE));

  p = (ex.total+FUDGE)/(float)(exam[st].total+FUDGE);
  q = (ml.total+FUDGE)/(float)(marg[st].total+FUDGE);
  (*t)->logq = log(p/q); 
  (*t)->quot = p/q; 


for (i=0; i < level; i++) printf(" ");
printf("%f %d %d \n",(*t)->logq,ex.num,ml.num); 

  best_score = -HUGE_VAL;
 for (i=0; i < state[st].dist_num; i++) {
    stat_key = i;
    qsort(ex.list,ex.num,sizeof(DATUM),stat_compare);
    qsort(ml.list,ml.num,sizeof(DATUM),stat_compare);

    jex = 0;
    mltot = extot = 0;
    for (jml = 0; jml < ml.num; jml++) {
      while (jex < ex.num && ex.list[jex].val[i] < ml.list[jml].val[i] ) 
        extot += ex.list[jex++].prob;
/*      if (extot >= MIN_TOT) 
      if (mltot >= MIN_TOT) 
      if ((ex.total - extot) >= MIN_TOT ) 
      if ((ml.total - mltot) >= MIN_TOT )  */
      {
        pml =  mltot/ml.total;
        qml = 1 - pml;
        pex = extot/ex.total;
        qex = 1 - pex;
/*        score = calc_score(pml,qml,pex,qex);*/
        score = test_stat(pml,pex,ml.total,ex.total); 
        if (score > best_score) {
          best_i = i;
          best_score = score;
          best_jex = jex;
          best_jml = jml;
          best_cut = ml.list[jml].val[i];
          best_pml = pml;
          best_qml = qml;
          best_pex = pex;
          best_qex = qex;
        }
      }
      mltot += ml.list[jml].prob;
    }
  }
  if (best_score == -HUGE_VAL) {
    (*t)->lc = (*t)->rc = NULL;
    (*t)->cutoff = -HUGE_VAL;
    (*t)->split_stat = -2;
    return;
  }   

  stat_key = best_i;
  qsort(ex.list,ex.num,sizeof(DATUM),stat_compare);
  qsort(ml.list,ml.num,sizeof(DATUM),stat_compare);


  (*t)->split_stat = best_i;
  (*t)->cutoff = best_cut;


  sub_ml.num = best_jml;
  sub_ml.size = ml.size;
  sub_ml.list = ml.list;
  sub_ml.total = ml.total*best_pml;
  sub_ex.num = best_jex;
  sub_ex.size = ex.size;
  sub_ex.list = ex.list;
  sub_ex.total = ex.total*best_pex;
  tt(st,sub_ml,sub_ex,best_pml*mprob,best_pex*exprob,&((*t)->lc),level+1);
  sub_ml.list += sub_ml.num;
  sub_ml.num = ml.num - sub_ml.num;
  sub_ml.total= ml.total- sub_ml.total;
  sub_ex.list += sub_ex.num;
  sub_ex.num = ex.num - sub_ex.num;
  sub_ex.total = ex.total - sub_ex.total;
  tt(st,sub_ml,sub_ex,best_qml*mprob,best_qex*exprob,&((*t)->rc),level+1);
}





int
train_trees(tokens)
int tokens;  {  /* train output distributions */
  int j,n,rp,rl,lab,noten,s,type,i,l;
  char name[500],ans[10];
  FILE *fp;
  float temp[STAT_NUM];
  DATUM d; 
  int avail[STAT_NUM];

  for (s=0; s < STATE_NUM; s++) 
    tt(s,marg[s],exam[s],1.,1.,&(state[s].root),0);

  return(0);

  for (s=0; s < STATE_NUM; s++) /*if (exam[s].num > MIN_EXAMP) */{ 
    for (j=0; j < state[s].dist_num; j++)  {
      stat_key = j;
      qsort(exam[s].list,exam[s].num,sizeof(DATUM),stat_compare);
      qsort(marg[s].list,marg[s].num,sizeof(DATUM),stat_compare);
      tt(s,marg[s],exam[s],1.,1.,&(state[s].root),0);
/*      tt(s,marg[s],exam[s],&(state[s].spline),0,avail); */
    }
  }  
}







void
train_splines(tokens)
int tokens;  {  /* train output distributions */
  int j,n,rp,rl,lab,noten,s,type,i,l;
  char name[500],ans[10];
  FILE *fp;
  float temp[STAT_NUM];
  DATUM d; 
  int avail[STAT_NUM];

/*  for (s=0; s < STATE_NUM; s++)  exam[s] = init_data(tokens,state[s].dist_num); */


  for (s=0; s < STATE_NUM; s++) /*if (exam[s].num > MIN_EXAMP) */{ 
    for (j=0; j < state[s].dist_num; j++)  {
      stat_key = j;
      qsort(exam[s].list,exam[s].num,sizeof(DATUM),stat_compare);
      qsort(marg[s].list,marg[s].num,sizeof(DATUM),stat_compare);
      make_spline(marg[s],exam[s],j,s,state[s].marg+j);
/*    tt(s,marg[s],exam[s],&(state[s].spline),0,avail);
    printf("\n"); */
    }
  }  
return;
}



int sort_key;

static int super_compare(i,j)
float *i,*j; {
  if (*(j+sort_key) > *(i+sort_key)) return(-1);
   return(1);

}  
 


float calc_score(pml,qml,pex,qex)   /* relative entropy ? */
float pml,qml,pex,qex; {
  float t1,t2,e,p,q,m;

  if (pml == 0. || qml == 0. || pex == 0. || qex == 0.)   return(-HUGE_VAL);
  
  p = pex/pml;
  q = qex/qml;
  p /= (p+q);
  q = 1-p;
  m =  (q > p) ? q : p;
  return( (m  < .80) ? -HUGE_VAL : m); 
}

float old_calc_score(pml,qml,pex,qex)   /* relative entropy ? */
float pml,qml,pex,qex; {
  float t1,t2,e,p,q;

  if (pml == 0. || qml == 0. || pex == 0. || qex == 0.)   return(-HUGE_VAL);
  e = pex*log(pex/pml) + qex*log(qex/qml);
/*  e = pml*log(pml/pex) + qml*log(qml/qex);  */
/*  p = pex/pml;
  q = qex/qml;
  p /= (p+q);
  q = 1-p;
  e = p*log(p) + q*log(q); */
/*if (p <= 0 || q <= 0) printf("h3llo\n");*/
  return( (e < .3) ? -HUGE_VAL : e); 
}


void
m_s(ml,ex,i,t,st,quot)
DATA ml,ex;        
TNODE **t;
int i,st;
float quot;  { /* sort on this stat */
  int best_i,jex,jml,best_jex,best_jml,j;
  float best_score,cutoff,p,q,score,pml,qml,pex,qex,best_cut,best_pex;
  float best_qex,best_pml,best_qml,mltot,extot;
  DATA sub_ml,sub_ex;




  *t = (TNODE *) malloc(sizeof(TNODE));
  p = (ex.total+FUDGE)/(float)(exam[st].total+FUDGE);
  q = (ml.total+FUDGE)/(float)(marg[st].total+FUDGE);
  (*t)->quot =  p/q;
   
  mltot = extot = jex = 0;
  best_score = -HUGE_VAL;
  for (jml = 0; jml < ml.num; jml++) {
    while (jex < ex.num && ex.list[jex].val[i] < ml.list[jml].val[i] )  
      extot += ex.list[jex++].prob;
    if (extot >= MIN_TOT) 
    if (mltot >= MIN_TOT) 
    if ((ex.total - extot) >= MIN_TOT ) 
    if ((ml.total - mltot) >= MIN_TOT ) {
      pml =  mltot/ml.total;
      qml = 1 - pml;
      pex = extot/ex.total;
      qex = 1 - pex;
      score = calc_score(pml,qml,pex,qex);
      if (score > best_score) {
        best_i = i;
        best_score = score;
        best_jex = jex;
        best_jml = jml;
        best_cut = ml.list[jml].val[i];
/*if (best_cut == 0.) for (j=0; j < ml.num; j++) printf("%d %f\n",j,ml.list[j].val[i]); */
        best_pml = pml;
        best_qml = qml;
        best_pex = pex;
        best_qex = qex;
      }
    }
    mltot += ml.list[jml].prob;
  }
  if (best_score == -HUGE_VAL) {
    (*t)->lc = (*t)->rc = NULL;
    (*t)->cutoff = 0;
    return;
  }
  (*t)->cutoff = best_cut;


  sub_ml.num = best_jml;
  sub_ml.size = ml.size;
  sub_ml.list = ml.list;
  sub_ml.total = ml.total*best_pml;
  sub_ex.num = best_jex;
  sub_ex.size = ex.size;
  sub_ex.list = ex.list;
  sub_ex.total = ex.total*best_pex;
  m_s(sub_ml,sub_ex,i,&((*t)->lc),st,best_pex/best_pml);
  sub_ml.list += sub_ml.num;
  sub_ml.num = ml.num - sub_ml.num;
  sub_ml.total= ml.total- sub_ml.total;
  sub_ex.list += sub_ex.num;
  sub_ex.num = ex.num - sub_ex.num;
  sub_ex.total = ex.total - sub_ex.total;

  m_s(sub_ml,sub_ex,i,&((*t)->rc),st,best_qex/best_qml);
}


traverse_tree(t,sp) /* sp->num is cell containing last val */
TNODE *t; 
SPLINE *sp; {

  if (t->lc != NULL) {
    traverse_tree(t->lc,sp);
    if (t->lc->lc == NULL || t->rc->rc == NULL)
      sp->breakpt[++(sp->num)] = t->cutoff;
    if (sp->num == MAX_CELLS) printf("spline full\n");
    traverse_tree(t->rc,sp);
  }
  else  sp->val[sp->num] = t->quot;    
}


#define ROUGH .25


int
make_spline(ml,ex,i,st,spline)
DATA ml,ex;        
int i,st;
SPLINE *spline;  {
  TNODE *t;
  float mn,mx,slop;
  int j;
  SPLINE *sp;

  spline->num = 0;
  m_s(ml,ex,i,&t,st,1.);
  spline->breakpt[0] = ml.list[0].val[i];
  traverse_tree(t,spline);
  if (spline->num) {
    (spline->num)++;
    spline->breakpt[spline->num] = ml.list[ml.num-1].val[i];
  }
return(0);
/*  for (j=0; j < spline->num; j++) off->breakpt[j] = spline->breakpt[j];
  off->num = spline->num;
  if (off->num) {
    off->breakpt[off->num] = HUGE_VAL;
    (off->num)++;
  }
  sp->breakpt[0] = -HUGE_VAL;
  mn = ml.list[0].val[i];
  sp->breakpt[1] = spline->breakpt[0]-ROUGH*(spline->breakpt[0]-mn);
  sp->val[0] = sp->val[1] = spline->val[0];
  for (j=0; j < spline->num-1; j++) {
    slop =  ROUGH*(spline->breakpt[j+1]-spline->breakpt[j]);
    sp->breakpt[2*j+2] =  spline->breakpt[j] + slop;
    sp->breakpt[2*j+3] = spline->breakpt[j+1] -  slop;
    sp->val[2*j+2] = sp->val[2*j+3] = spline->val[j+1];
  }
  mx = ml.list[ml.num-1].val[i];
  slop =  ROUGH*(mx-spline->breakpt[spline->num-1]);
  sp->breakpt[2*spline->num] = spline->breakpt[spline->num-1] + slop;
  sp->breakpt[2*spline->num+1] = HUGE_VAL;
  sp->val[2*spline->num] =  sp->val[2*spline->num+1] = spline->val[spline->num];
  sp->num = 2*spline->num+1;
/*for (j=0; j <= spline->num; j++) printf("%f %f\n",spline->breakpt[j],spline->val[j]);
printf("\n");
for (j=0; j <= sp->num; j++) printf("%f %f\n",sp->breakpt[j],sp->val[j]);
exit(0); */
}





float eval(p,q)
float p,q; {
  float temp;

  if (p < q) { temp = p; p = q; q = temp; }
  if (p < .4) return(-HUGE_VAL);     /* split doesn't differentiate well */ 
  return(p);
}

float eval_spline(sp)
SPLINE_TREE *sp; {
  int i;
  float best = -HUGE_VAL;

  for (i=0; i < sp->num; i++) if (sp->val[i] > best) 
    best = sp->val[i];
  return((best > 1.1) ? best : -HUGE_VAL);
}
    
















init_stat_hist() {
  int i;

/*  if (frames == 0) { printf("problem in init_stat_hist()\n"); return; }
  stat_hist = (STAT_HIST *) malloc(sizeof(STAT_HIST)*frames); */
/*  for (i=0; i < frames; i++) stat_hist[i].num = 0;*/
}

//CF:  prob_hist will store the history record of active hypotheses.
//CF:  At each frame, a list resembling active is stored. Sets init lengths of these lists to zero.
init_prob_hist() {
  int i;

/*  if (frames == 0) { printf("problem in init_stat_hist()\n"); return; }
  prob_hist = (PROB_HIST *) malloc(sizeof(PROB_HIST)*frames); */
  for (i=0; i < frames; i++) prob_hist[i].num = 0;
}

#define HIST_FILE "history"

write_prob_hist() {
  FILE *fp;
  int i,j,n;
  char s[500],t[500];

  fp = fopen(HIST_FILE,"w");
  for (i=0; i < frames; i++) {
    fprintf(fp,"frame = %d\n",i);
    for (j=0; j < prob_hist[i].num; j++) {
      n = prob_hist[i].list[j].place->note;
      num2name(score.solo.note[n].num,s);
      /*   state2name(prob_hist[i].list[j].place->type,t); */
      fprintf(fp,"%d %f %s %s\n",prob_hist[i].list[j].place,prob_hist[i].list[j].prob,s,t);
    }
    fprintf(fp,"\n");
  }
  fclose(fp);
}


void
state2name(n,s)
int n; 
char *s; {
  if (n == NOTE_STATE) { strcpy(s,"note"); return; }
  if (n == REST_STATE) { strcpy(s,"rest"); return; }
  if (n == ATTACK_STATE) { strcpy(s,"attk"); return; }
  if (n == REARTIC_STATE) { strcpy(s,"reartc"); return; }
  if (n == SLUR_STATE) { strcpy(s,"slur"); return; }
  if (n == SETTLE_STATE) { strcpy(s,"setl"); return; }
  if (n == FALL_STATE) { strcpy(s,"fall"); return; }
  if (n == RISE_STATE) { strcpy(s,"rise"); return; }
  if (n == TRILL_HALF_STATE) { strcpy(s,"tr_half"); return; }
  if (n == TRILL_WHOLE_STATE) { strcpy(s,"tr_whole"); return; }
  strcpy(s,"unknown");
}





#define DOWNHUMPS /*2*/  3
float downsampling_sinc[2*IO_FACTOR*DOWNHUMPS];
float *down_sinc;

#define FD_CBUFF 4096

void
filtered_downsample(unsigned char *fr, unsigned char *to, int samps, int frame, int *produced) {
  int i,j,k,lim,first;
  float s;
  static int lpos;  // pos of center of region that will compose the next lo-res sample
  static int hpos;  // next unprocess hi-res sample in circ
  static float circ[FD_CBUFF];

  lim = IO_FACTOR*DOWNHUMPS;  // down_sinc[-lim ... lim-1]
  if (frame == 0) {  // cue to initialization
    for (i=0; i < FD_CBUFF; i++) circ[i] = 0;
    lpos = (IO_FACTOR - lim + FD_CBUFF) % FD_CBUFF;
    hpos = 0;
  }
  first = (lpos + FD_CBUFF - lim) % FD_CBUFF;
  for (j=0; j < samps; j++,hpos = (hpos+1)%FD_CBUFF) {
    circ[hpos] = sample2float(fr + j*BYTES_PER_SAMPLE);
    if (hpos == first) { printf("circ buff not big enought in filtered_downsample\n"); exit(0); }
  }
  for (i=0; ; i++,lpos = (lpos+IO_FACTOR)%FD_CBUFF) {
    for (j=-lim,s=0; j < lim; j++)  {
      k = (lpos+j+FD_CBUFF)%FD_CBUFF;
      if (k == hpos) break;
      s += down_sinc[j]* circ[k];
    }
    if (j != lim) break;
    float2sample(s ,to + BYTES_PER_SAMPLE*i);
  }
  *produced = i; // on termination to contains this many samples
}

void
make_downsampling_filter() {
  int i;
  float t,total=0;

  down_sinc = downsampling_sinc + IO_FACTOR*DOWNHUMPS;
  for (i=-IO_FACTOR*DOWNHUMPS; i < IO_FACTOR*DOWNHUMPS; i++) {


    t = i*PI/IO_FACTOR;
    down_sinc[i] = (i == 0) ? 1 : sin(t)/t;

       /*       down_sinc[i] = (i < IO_FACTOR && i >= 0) ? 1 : 0;
       down_sinc[i] = (i < -IO_FACTOR*DOWNHUMPS+IO_FACTOR) ? 1 : 0;*/
       
    //          t = i / (float) (IO_FACTOR*DOWNHUMPS);
    //      total += down_sinc[i] = (1 + cos(PI*t))/(2*IO_FACTOR*DOWNHUMPS);  // no longer a sinc, now a Hanning.

    //    printf("%d %f\n",i,down_sinc[i]);
  }
  //      for (i=-IO_FACTOR*DOWNHUMPS; i < IO_FACTOR*DOWNHUMPS; i++) down_sinc[i] /= total;
  for (i=-IO_FACTOR*DOWNHUMPS; i < IO_FACTOR*DOWNHUMPS; i++) down_sinc[i] /= (IO_FACTOR);  // for sinc function
  //for (i=-IO_FACTOR*DOWNHUMPS; i <= IO_FACTOR*DOWNHUMPS; i++) printf("%d %f\n",i,down_sinc[i]);
}






















static float log_2(x)
float x; {
	return(log(x)/log(2.));
}

static float omega2midi(o) /* a frequency in spect units to a midi note
			      number (float) */
float o; {
  float fc;
  int nc;

 fc = sol[MIDI_MIDDLE_C].omega;
  nc = MIDI_MIDDLE_C;
  return(log_2(o/fc)*12+nc);  /* used to be log2 */
}


float midi2omega(float m) {
  float fc;
  int nc;

  fc = sol[MIDI_MIDDLE_C].omega;
  nc = MIDI_MIDDLE_C;
  return(pow(2.,(m-nc)/12)*fc);  /* inverse of above */
}


//#define MAX_PITCH_INTERVALS 50
#define TARGET_GROUPS 3
#define NUM_DIVIDES   6 //5 //4
#define MAX_HZ_INFL 20.  /* a harmonic has no effect more than this 
			   distance away */
#define MAX(a,b) (a>b? (a):(b))
#define MIN(a,b) (a>b? (b):(a))

#define MIDI_PITCH_N 128 // total # of midi pitches
 
    
PITCH_INTERVAL pitch_int[MAX_PITCH_INTERVALS];
static PITCH_INTERVAL attack_pitch_int[MAX_PITCH_INTERVALS];

int pitch_int_num;
static int attack_int_num;

static void init_pitch_intervals() {
  int low_omega,i,span,j;
  float f1,f2,p,q,f;

  //    span = sol[LOWEST_NOTE].omega; 
    span = sol[MIDI_MIDDLE_C - 2].omega; 
/* printf("span = %d note = %d\n",span,LOWEST_NOTE); */
  /* size of interval so that it can only overlap one harmonic */
  for (i=0; i < MAX_PITCH_INTERVALS; i++) {
    pitch_int[i].lo = (i+1)*span;
    pitch_int[i].hi = (i+2)*span;
    /*printf("int = %d %d freqs = %d\n",pitch_int[i].lo,pitch_int[i].hi,freqs); */
    f1 = omega2midi((float)(i+1)*span);
    f2 = omega2midi((float)(i+2)*span);
    pitch_int[i].group = /*anint*/((f2-f1)/TARGET_GROUPS);
    if (pitch_int[i].group < 1) pitch_int[i].group = 1;
    for (j=0; j < NUM_DIVIDES; j++) {
      f = f1 + (j+1)*(f2-f1)/(NUM_DIVIDES+1);
      pitch_int[i].div[j] = midi2omega(f);
    }
    if (pitch_int[i].hi > freqs) break;
  }
  pitch_int_num = i;
/*  for (i=0; i < pitch_int_num; i++) {
    printf("hi = %d lo = %d group = %f\n",
	   pitch_int[i].hi, pitch_int[i].lo, pitch_int[i].group); 
   for (j=0; j < NUM_DIVIDES; j++)
     printf("div = %f\n",pitch_int[i].div[j]);
  } */
}  

#define LOWEST_PITCH_INTERVAL_BASE_PITCH  MIDI_MIDDLE_C - 28
#define HIGHEST_PITCH_INTERVAL_BASE_PITCH  MIDI_MIDDLE_C 
#define MEANINGFUL_HARMS 5

static void new_init_pitch_intervals() {
  int low_omega,i,span,j,last_top,min_span,skip,max_span;
  float f1,f2,p,q,f;

  min_span = last_top = (int) sol[LOWEST_PITCH_INTERVAL_BASE_PITCH].omega; 
  max_span = (int) sol[HIGHEST_PITCH_INTERVAL_BASE_PITCH].omega; 
  span = sol[MIDI_MIDDLE_C - 2].omega; 
  for (i=0; i < MAX_PITCH_INTERVALS; i++) {
    pitch_int[i].lo = last_top;
    skip = pitch_int[i].lo/MEANINGFUL_HARMS;
    span = min(max(skip,min_span),max_span);
    last_top = pitch_int[i].hi = pitch_int[i].lo + span;
    /*printf("int = %d %d freqs = %d\n",pitch_int[i].lo,pitch_int[i].hi,freqs); */
    f1 = omega2midi((float)pitch_int[i].lo);
    f2 = omega2midi((float)pitch_int[i].hi);
    pitch_int[i].group = (int) ((f2-f1)/TARGET_GROUPS);
    if (pitch_int[i].group < 1) pitch_int[i].group = 1;
    for (j=0; j < NUM_DIVIDES; j++) {
      f = f1 + (j+1)*(f2-f1)/(NUM_DIVIDES+1);
      pitch_int[i].div[j] = midi2omega(f);
    }
    if (pitch_int[i].hi > freqs) break;
  }
  
  pitch_int_num = i;

/*  for (i=0; i < pitch_int_num; i++) {
    printf("hi = %d lo = %d group = %f\n",
	   pitch_int[i].hi, pitch_int[i].lo, pitch_int[i].group); 
   for (j=0; j < NUM_DIVIDES; j++)
     printf("div = %f\n",pitch_int[i].div[j]);
  } */
}  









#define ATTACK_OCT_DIV  4.   /* 4 = 3 half steps */
#define ATTACK_LOWEST_PITCH MIDI_MIDDLE_C - 12  
#define ATTACK_OCTAVES 4

static void init_attack_pitch_intervals() {
  int low_omega,i,span,j,last_top,min_span,skip,max_span,num;
  float f1,f2,p,q,f;

  min_span = last_top = (int) sol[LOWEST_PITCH_INTERVAL_BASE_PITCH].omega; 
  max_span = (int) sol[HIGHEST_PITCH_INTERVAL_BASE_PITCH].omega; 
  span = sol[MIDI_MIDDLE_C - 2].omega; 

  num = (int) ATTACK_OCTAVES*ATTACK_OCT_DIV;
  for (i=0; i < num; i++) {
    attack_pitch_int[i].lo = 
      midi2omega(ATTACK_LOWEST_PITCH + i*12/(float)ATTACK_OCT_DIV - .5);
    attack_pitch_int[i].hi = 
      midi2omega(ATTACK_LOWEST_PITCH + (i+1)*12/(float)ATTACK_OCT_DIV - .5);
    if (i == MAX_PITCH_INTERVALS) { printf("out of range here\n"); exit(0); }
    //    printf("%d %d\n",pitch_int[i].lo,pitch_int[i].hi);
  }
  attack_int_num = i;
}  



static TNODE *new_tnode() {
TNODE *t; 
  
  t = (TNODE *) malloc(sizeof(TNODE));
  t->lc = t->rc = NULL;
  t->split = -1;
  t->cutoff = HUGE_VAL;
  return(t);
}

#ifdef FRAME_LENGTH_EXPERIMENT  
float last_spect[FRAMELEN];
#endif
#ifndef FRAME_LENGTH_EXPERIMENT  
float last_spect[TOKENLEN];
#endif

float diff_spect[FRAMELEN];


static float spect_mem[MAX_FRAMES];
/*static */float high_spect_mem[MAX_FRAMES];
/*static*/ float low_spect_mem[MAX_FRAMES];

extern int freqs;

void
save_spect() {
  int i,j;

  for (i=0; i < freqs; i++) last_spect[i] = spect[i];
}


#define DIFF_SEARCH 2


#define ATTACK_MOD 
#define DIFF_SEARCH_CONST .01

void
compute_diff_spect() {
  int i,j,lo,hi,w;
  float pred[FRAMELEN],x,t=0,s=0;
 
  for (j=0; j < freqs; j++) pred[j] = 0;
    for (j=DIFF_SEARCH; j < freqs-DIFF_SEARCH; j++) 
    for (i=-DIFF_SEARCH; i <= DIFF_SEARCH; i++) 
    if (last_spect[j+i] > pred[j]) pred[j] = last_spect[j+i]; 

    /*  for (j=0; j < freqs; j++) {
    w = DIFF_SEARCH_CONST*j;
    lo = j - w;
    if (lo < 0) lo = 0;
    hi = j + w;
    if (hi > (freqs-1)) hi = freqs-1;
    for (i=lo; i <= hi; i++) if (last_spect[i] > pred[j]) pred[j] = last_spect[i]; 
    }*/


  for (j=0; j < freqs; j++) {
    x = spect[j]-pred[j];
    diff_spect[j] = (x > 0) ? x : 0;
    s += fabs(x);
    t += diff_spect[j];
  }
#ifdef ATTACK_MOD
  if (s > 0) for (j=0; j < freqs; j++) diff_spect[j] /= s;
#else
  if (t > 0) for (j=0; j < freqs; j++) diff_spect[j] /= t;
#endif
  //    printf("zzz = %f\t%f \n",t,s);
}






float spect_diff() {
  int i;
  float diff=0,sum=0,stat;

  if (mode == BACKWARD_MODE) return(spect_mem[token]);
  for (i=0; i < freqs; i++) {
    diff += fabs(spect[i]-last_spect[i]);
    sum += spect[i]+last_spect[i];
    last_spect[i] = spect[i];
  }
  stat = (sum == 0) ? 0. : diff/sum;
  spect_mem[token] = stat;
  
  return(stat);
}

    

float high_spect_diff() {
  int i;
  float diff=0,sum=0,stat;

  if (mode == BACKWARD_MODE) return(high_spect_mem[token]);
  for (i=2*freqs/3; i < freqs; i++) {
    diff += fabs(spect[i]-last_spect[i]);
    sum += spect[i]+last_spect[i];
  }
  stat = (sum == 0) ? 0. : diff/sum;
  high_spect_mem[token] = stat;
  
  return(stat);
}
    
static float 
high_spect_energy() {
  int i;
  float diff=0,sum=0,stat;

  for (i=2*freqs/3; i < freqs; i++) {
    sum += spect[i];
  }
  return(sum);
}
    
static float 
low_spect_energy() {
  int i;
  float diff=0,sum=0,stat;

  for (i=0;  i < freqs/3; i++) {
    sum += spect[i];
  }
  return(sum);
}
    

float low_spect_diff() {
  int i;
  float diff=0,sum=0,stat;

  if (mode == BACKWARD_MODE) return(low_spect_mem[token]);
  for (i=0; i < freqs/3; i++) {
    diff += fabs(spect[i]-last_spect[i]);
    sum += spect[i]+last_spect[i];
  }
  stat = (sum == 0) ? 0. : diff/sum;
  low_spect_mem[token] = stat;
  
  return(stat);
}
    
static void high_energy_feature(val,p)
float *val; 
int p; {
  float power=0,x;
  int   i;

  for (i=0; i < FRAMELEN; i++) 
    power += data[i]*data[i];
  val[0] = power; 
  val[1] = high_spect_energy();
  //      val[2] = low_spect_energy() / high_spect_energy();*/
}



void energy_feature(float *val, int p) {
  float power=0,x;
  int   i;

#ifdef FRAME_LENGTH_EXPERIMENT  
  for (i=0; i < FRAMELEN; i++) 
#endif
#ifndef FRAME_LENGTH_EXPERIMENT  
  for (i=0; i < TOKENLEN; i++) 
#endif

    power += data[i]*data[i];
  val[0] = power; 
  //  printf("%d %f\n",token,power); 
  /*  val[1] = high_spect_energy();
      val[2] = low_spect_energy() / high_spect_energy();*/
}

static void 
orchestra_energy_feature(float *val, int p) {
  float power=0,opower=0,x;
  int   i;

#ifdef FRAME_LENGTH_EXPERIMENT  
  for (i=0; i < FRAMELEN; i++) {
    power += data[i]*data[i];
    opower += orch_data_frame[i]*orch_data_frame[i];
  }
#endif
#ifndef FRAME_LENGTH_EXPERIMENT  
  for (i=0; i < TOKENLEN; i++) {
    power += data[i]*data[i];
    opower += orch_data_frame[i]*orch_data_frame[i];
  }
#endif

  //    printf("power = %f opower = %f rat = %f\n",power,opower,power); ///opower);

  val[0] = (opower == 0)  ? 0 : power/opower;
  //#ifdef WINDOWS
  //          val[0] /= 20;   /* take this out before retraining */
  //#endif  
}

static int energy_signature(n,p)
int n,p; {
  int st,note;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  return (st == REST_STATE) ? 0 : 1;
}

#define ENERGY_CUTOFF   0.3  /* beyond this many seconds after the attack of
				an accompaniment note it is essentially not making significant
				energy */

#define ENERGY_REGIONS 4
static float energy_cutoff[ENERGY_REGIONS-1] = {.1, .2, .4 };

static int sa_energy_signature(SA_STATE sas) 
{
  int i;


  if (sas.shade == REST_STATE) { // solo is in rest_state
    for (i=ENERGY_REGIONS-2; i >= 0; i--)
      if (sas.last_accomp_time >  energy_cutoff[i]) /*return(i+1);*/ return(3-i-1);
    /*    return(0);*/
    return(3);

    /*    if (sas.last_accomp_time >  ENERGY_CUTOFF) return(0);
	  else return(1);*/

    //    if (sas.acc_notes->num == 0) return(0); // no accomp notes
    //    else return(1); // accomp is sounding
  }
  /*  else return(2); // solo is sounding sig = 2 */
   return(4);
}


static int 
sa_orch_energy_signature(SA_STATE sas) {
  if (sas.shade == REST_STATE) return(0);
  if (sas.shade == NOTE_STATE) return(1);
  if (sas.shade == FALL_STATE)    return(2);  
  if (sas.shade == REARTIC_STATE) return(2);
  if (sas.shade == SLUR_STATE) return(2);
  if (sas.shade == ATTACK_STATE)  return(2);
  printf("unknown state in sa_orch_eneregy_signature() %d \n",sas.shade);
  exit(0);
}


static int 
sa_high_energy_signature(SA_STATE sas) {
  if (sas.shade == REST_STATE) return(0);
  if (sas.shade == NOTE_STATE) return(1);
  if (sas.shade == FALL_STATE)    return(1);  
  if (sas.shade == REARTIC_STATE) return(1);
  if (sas.shade == SLUR_STATE) return(1);
  if (sas.shade == ATTACK_STATE)  return(1);
  printf("unknown state in sa_orch_eneregy_signature() %d \n",sas.shade);
  exit(0);
}



static void flat_tree(low,hgh,q,t,num,cur,sigs)
float hgh,low,*q;
int num,cur,sigs;  
TNODE **t; {
  float mid,p,binom();
  int m,i,s;

  *t = new_tnode();
  mid  = (hgh+low)/2;
  m = mid*QUANTILES;
  if (cur == num) {
    (*t)->cutoff = 0;
    (*t)->split = -1;
    i = mid*num;
    for (s=0; s < sigs; s++) {
      p = (2*s+1) / (float) (2*sigs);
      if ( s == 0) p = .1;
      if (s == sigs-1) p = .9;
      (*t)->cond_dist[s] = binom(i,num-1,p);
    }
    return;
  }
  (*t)->cutoff = q[m];
  (*t)->split = 0;
  cur <<= 1;
  flat_tree(low,mid,q,&((*t)->lc),num,cur,sigs);
  flat_tree(mid,hgh,q,&((*t)->rc),num,cur,sigs);
}


static void constant_tree(low,hgh,q,t,num,cur,sigs)
float hgh,low,*q;
int num,cur,sigs;  
TNODE **t; {
  float mid,p,binom();
  int m,i,s;

  *t = new_tnode();
  mid  = (hgh+low)/2;
  m = mid*QUANTILES;
  if (cur == num) {
    (*t)->cutoff = 0;
    (*t)->split = -1;
    i = mid*num;
    for (s=0; s < sigs; s++)  (*t)->cond_dist[s] = 1;
    return;
  }
  (*t)->cutoff = q[m];
  (*t)->split = 0;
  cur <<= 1;
  constant_tree(low,mid,q,&((*t)->lc),num,cur,sigs);
  constant_tree(mid,hgh,q,&((*t)->rc),num,cur,sigs);
}


static void pitch_tree(low,hgh,q,t,num,cur,sigs)
float hgh,low,*q;
int num,cur,sigs;  
TNODE **t; {
  float mid,p,binom();
  int m,i,s;

  *t = new_tnode();
  mid  = (hgh+low)/2;
  m = mid*QUANTILES;
  if (cur == num) {
    i = mid*num;
    (*t)->cond_dist[0] = 1./num;
    for (s=1; s < sigs; s++) {
      p = (2*s+1) / (float) (2*sigs);
      (*t)->cond_dist[s] = binom(i,num-1,p);
    }
    return;
  }
  (*t)->cutoff = q[0]+mid*(q[QUANTILES-1]-q[0]);
  (*t)->split = 0;
  cur <<= 1;
  pitch_tree(low,mid,q,&((*t)->lc),num,cur,sigs);
  pitch_tree(mid,hgh,q,&((*t)->rc),num,cur,sigs);
}



static void sig_pitch_tree(lo,hi,sigs,div,t)  
/* make sigs-1 quanta divided b the sigs-2 elements of div */
float *div;
int lo,hi,sigs;  
TNODE **t; {
  float p,binom();
  int s,mid;

  if (lo == hi) return; /* should not happen */
  *t = new_tnode();
  if (lo+1 == hi) {
    (*t)->cond_dist[0] = 1./(sigs-1);
    for (s=0; s < sigs-1; s++) {
      p = (2*s+1)/(float)(2*(sigs-1));
      (*t)->cond_dist[s+1] = binom(lo,sigs-2,p);
    }
     return;
  }
  mid = (lo+hi)/2;
  (*t)->cutoff = div[mid-1];
  (*t)->split = 0;  /* split on first (pitch) component */
  sig_pitch_tree(lo,mid,sigs,div,&((*t)->lc));
  sig_pitch_tree(mid,hi,sigs,div,&((*t)->rc));
}


#define INIT_QUANTS 8
  
static void unif_tree(f) /* high signatures expect high values */
int f; {
  int sigs,p;
  TNODE **t;
  float *q;

  t = &(feature.el[f].tree);
  sigs = feature.el[f].num_sig;
  q = feature.el[f].quantile[0];
  *t = new_tnode();
  constant_tree(0.,1.,q,t,INIT_QUANTS,1,sigs);
}


static void pos_cor_tree(f) /* high signatures expect high values */
int f; {
  int sigs,p;
  TNODE **t;
  float *q;

  t = &(feature.el[f].tree);
  sigs = feature.el[f].num_sig;
  q = feature.el[f].quantile[0];
  *t = new_tnode();
  flat_tree(0.,1.,q,t,INIT_QUANTS,1,sigs);
}


static void pitch_cor_tree(f) /* high signatures expect high values */
int f; {
  int sigs,p;
  TNODE **t;
  float *div;

  t = &(feature.el[f].tree);
  sigs = feature.el[f].num_sig;
  p =  feature.el[f].parm;
  div = pitch_int[p].div;
  sig_pitch_tree(0,sigs-1,sigs,div,t);
}




static void even_tree(t,q,sigs)
int sigs;  
float *q;
TNODE **t; {
  int s;

  *t = new_tnode();
  for (s=0; s < sigs; s++)  (*t)->cond_dist[s] = 1;
}

  
static int current_label;

static void lt(t)
TNODE *t; {
  if (t->split == -1) current_label++;
  else {
    lt(t->lc);
    lt(t->rc);
  }
}
  

static int label_tree(t)
TNODE *t; {
  current_label = 0;
  lt(t);
  return(current_label);
}


/*static void init_tree_probs(t,sigs,q)
TNODE *t;
int sigs,q; {
  int s;
  float p,binom();

  if (t->split == -1) for (s=0; s < sigs; s++) {
    p = (2*s+1) / (float) (2*sigs);
    t->cond_dist[s] = binom(t->quant,q-1,p);
  }
  else {
    init_tree_probs(t->lc,sigs,q);
    init_tree_probs(t->rc,sigs,q);
  }
}
*/

  


/* #define SUB_DIV 4  /* a frame divided up into this many subregions */ 
#define SUB_DIV 2  /* a frame divided up into this many subregions  */
#define DERV_LEN 3   
/* #define DERV_LEN 6   */
static float coarse[SUB_DIV+DERV_LEN-1];
static float derv[DERV_LEN] = {1 , -2 , 1};
/* static float derv[DERV_LEN] = {1, 1, -2, -2 , 1, 1};   */

float burst_mem[MAX_FRAMES];

float burst() {
  int i,len,j;
  float r,x,w,a[SUB_DIV],sum,d,mx;

  if (mode == BACKWARD_MODE) r = burst_mem[token];
  else {
    for (i=0; i < DERV_LEN-1; i++) coarse[i+SUB_DIV] = coarse[i];
    for (j=0; j < SUB_DIV; j++) sum = a[j] = coarse[j] = 0;

#ifdef FRAME_LENGTH_EXPERIMENT  
    len =  FRAMELEN/SUB_DIV;
#endif
#ifndef FRAME_LENGTH_EXPERIMENT  
    len =  TOKENLEN/SUB_DIV;
#endif



    for (i=0; i < len; i++) {
      /*    x = 3* (i-len/2) / (float) len;
	    w = exp(-x*x); */
      for (j=0; j < SUB_DIV; j++) coarse[j] += data[j*len+i]*data[j*len+i]/**w*/;
    }
    for (i=0; i < SUB_DIV; i++) for (j=0; j < DERV_LEN; j++)
      a[i] += derv[j]*coarse[i+j];
    mx = -HUGE_VAL;
    for (i=0; i < SUB_DIV+DERV_LEN-1; i++) sum += coarse[i];
    for (i=0; i < SUB_DIV; i++) if (fabs(a[i]) > mx) mx = fabs(a[i]);
    if (sum > 0) r = mx/sum/*pow(sum,1.)*/;
    else r = 0;
    burst_mem[token] = r;
  }
  return(r);
}

#define MEM_LENGTH   5 /* 9 /* 5  /* probably need > 5 value */

static float memry[MEM_LENGTH];

static updt_envelope() {   /* energy_stat must arleardy be called */
  float stat;
  int   i;

  for (i=MEM_LENGTH-2; i >= 0; i--) {
    memry[i+1] = memry[i];
  }
  energy_feature(memry+0,0);
  stat = 0;
  for (i=0; i < MEM_LENGTH; i++) stat += memry[i];
}


float diff_memry[MAX_FRAMES];


static float envel_diff() {
  float diff=0,w,s=0;
  int i;

  if (token < MEM_LENGTH) return(0.);
  if (mode == BACKWARD_MODE) diff = diff_memry[token];
  else {
    updt_envelope();
      w = (((float) MEM_LENGTH)-1)/2;   /* fit line to data and take slope */
      for (i=0; i < MEM_LENGTH; i++) {
	diff += memry[i]*w; 
	s += fabs(w);
	w -= 1;
      }
      diff /= s;

      diff_memry[token] = diff;
  }
  return(diff);
}
  

#define FRAME_CUTOFF  .128 /*0.064 */   //.128 .064 .032

static void bang_feature(val,p)
float *val;
int p; {
  val[0] = burst();
/*  val[1] = envel_diff();
  energy_feature(val+2,0); */
}





static int bang_signature(n,p)
int n,p; {
  int st,note;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  if (st == NOTE_STATE)    return(0);
  if (st == REST_STATE)    return(1);
  if (st == FALL_STATE)    return(2);
  if (st == RISE_STATE)    return(3);
  if (st == REARTIC_STATE) return(4);
  if (st == ATTACK_STATE)  return(5);
  if (st == TRILL_HALF_STATE)  return(6);  /* not used now */
  if (st == TRILL_WHOLE_STATE)  return(6);  /* now used now */
  printf("unknown state in bang_signature() %d \n",st);
}


static int old_sa_bang_signature(SA_STATE sas)
{
  if (sas.shade == NOTE_STATE)    return(0);  
  if (sas.shade == REST_STATE)    return(1); 
  if (sas.shade == FALL_STATE)    return(2);  
  if (sas.shade == RISE_STATE)    return(3); 
  if (sas.shade == REARTIC_STATE) return(4);
  if (sas.shade == SLUR_STATE) return(5);
  if (sas.shade == ATTACK_STATE)  return(5);
  if (sas.shade == TRILL_HALF_STATE)  return(6);  /* not used now */
  if (sas.shade == TRILL_WHOLE_STATE)  return(6);  /* now used now */    
  printf("unknown state in bang_signature() %d \n",sas.shade);
  exit(0);
}


static int sa_bang_signature(SA_STATE sas)
{
  //if (sas.shade == NOTE_STATE)    return(0);  
  //if (sas.shade == REST_STATE)    return(1); 
  if (sas.shade == NOTE_STATE && sas.last_accomp_time <= FRAME_CUTOFF) return(7); 
  if (sas.shade == NOTE_STATE && sas.last_accomp_time > FRAME_CUTOFF) return(8); 
  if (sas.shade == REST_STATE && sas.last_accomp_time <= FRAME_CUTOFF) return(0);
  if (sas.shade == REST_STATE && sas.last_accomp_time > FRAME_CUTOFF) return(1); 
  if (sas.shade == FALL_STATE)    return(2);  
  if (sas.shade == RISE_STATE)    return(3); 
  if (sas.shade == REARTIC_STATE) return(4);
  if (sas.shade == SLUR_STATE) return(5);
  if (sas.shade == ATTACK_STATE)  return(5);
  if (sas.shade == TRILL_HALF_STATE)  return(6);  /* not used now */
  if (sas.shade == TRILL_WHOLE_STATE)  return(6);  /* now used now */    
  printf("unknown state in bang_signature() %d \n",sas.shade);
  exit(0);
}

static int new_sa_bang_signature(SA_STATE sas)
{
  //if (sas.shade == NOTE_STATE)    return(0);  
  //if (sas.shade == REST_STATE)    return(1); 
  if (sas.shade == NOTE_STATE && sas.last_accomp_time <= FRAME_CUTOFF) return(0); 
  if (sas.shade == NOTE_STATE && sas.last_accomp_time > FRAME_CUTOFF) return(1); 
  if (sas.shade == REST_STATE && sas.last_accomp_time <= FRAME_CUTOFF) return(2);
  if (sas.shade == REST_STATE && sas.last_accomp_time > FRAME_CUTOFF) return(3); 
  if (sas.shade == FALL_STATE)    return(4);  
  if (sas.shade == REARTIC_STATE) return(6/*5*/);
  if (sas.shade == SLUR_STATE) return(5);
  if (sas.shade == ATTACK_STATE)  return(5);
  printf("unknown state in bang_signature() %d \n",sas.shade);
  exit(0);
}


static void slope_feature(val,p)
float *val;
int p; {
  val[0] = envel_diff();
}



static int slope_signature(n,p)
int n,p; {
  int st,note;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  if (st == FALL_STATE)    return(0);
  if (st == NOTE_STATE)    return(1);
  if (st == REST_STATE)    return(2);
  if (st == RISE_STATE)    return(3);
  if (st == REARTIC_STATE) return(4);
  if (st == ATTACK_STATE)  return(5);
  if (st == TRILL_HALF_STATE)  return(6); /* now used now */
  if (st == TRILL_WHOLE_STATE)  return(6);/* now used now */
  printf("unknown state in slope_signature() %d \n",st);
}

static int old_sa_slope_signature(SA_STATE sas)
{

  if (sas.shade == FALL_STATE)    return(0); 
  if (sas.shade == NOTE_STATE)    return(1);
  if (sas.shade == REST_STATE && sas.acc_notes == 0)    return(2); 
  if (sas.shade == REST_STATE && sas.acc_notes > 0)    return(/*1*/3); 
  if (sas.shade == RISE_STATE)    return(4); 
  if (sas.shade == REARTIC_STATE) return(5);
  if (sas.shade == SLUR_STATE) return(6);
  if (sas.shade == ATTACK_STATE)  return(6); 
  if (sas.shade == TRILL_HALF_STATE)  return(6); /* now used now */
  if (sas.shade == TRILL_WHOLE_STATE)  return(6);/* now used now */
  printf("unknown state in slope_signature() %d \n",sas.shade);
}


int sa_slope_signature(SA_STATE sas)
{
  if (sas.shade == FALL_STATE)    return(0);   /* not used currently (?) */
  //if (sas.shade == NOTE_STATE)    return(1);
  //if (sas.shade == REST_STATE)    return(2); 
  if (sas.shade == NOTE_STATE && sas.last_accomp_time <= FRAME_CUTOFF) return(1); 
  if (sas.shade == NOTE_STATE && sas.last_accomp_time > FRAME_CUTOFF) return(7); 
  if (sas.shade == REST_STATE && sas.last_accomp_time <= FRAME_CUTOFF) return(2); 
  if (sas.shade == REST_STATE && sas.last_accomp_time > FRAME_CUTOFF) return(3); 
  if (sas.shade == RISE_STATE)    return(4);  /* not currently used (?) */
  if (sas.shade == REARTIC_STATE) return(5);
  if (sas.shade == SLUR_STATE) return(6);
  if (sas.shade == ATTACK_STATE)  return(6); 
  if (sas.shade == TRILL_HALF_STATE)  return(6); /* now used now */
  if (sas.shade == TRILL_WHOLE_STATE)  return(6);/* now used now */
  printf("unknown state in slope_signature() %d \n",sas.shade);
}




static void spect_diff_feature(val,p)
float *val;
int p; {
  val[0] = spect_diff();
}



static int spect_diff_signature(n,p)
int n,p; {
  int st,note;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  if (st == FALL_STATE)    return(0);
  if (st == NOTE_STATE)    return(1);
  if (st == REST_STATE)    return(2);
  if (st == RISE_STATE)    return(3);
  if (st == REARTIC_STATE) return(4);
  if (st == ATTACK_STATE)  return(5);
  if (st == TRILL_HALF_STATE)  return(6); /* now used now */
  if (st == TRILL_WHOLE_STATE)  return(6); /* now used now */
  printf("unknown state in spect_diff_signature() %d \n",st);
}

static int sa_spect_diff_signature(SA_STATE sas)
{
  if (sas.shade == FALL_STATE)    return(0); 
  if (sas.shade == NOTE_STATE)    return(1); 
  if (sas.shade == REST_STATE)    return(2); 
  if (sas.shade == RISE_STATE)    return(3);
  if (sas.shade == REARTIC_STATE) return(4);
  if (sas.shade == SLUR_STATE) return(5);
  if (sas.shade == ATTACK_STATE)  return(5); 
  if (sas.shade == TRILL_HALF_STATE)  return(6); /* now used now */
  if (sas.shade == TRILL_WHOLE_STATE)  return(6); /* now used now */
  printf("unknown state in spect_diff_signature() %d \n",sas.shade);
}



static void 
disruption_feature(float *val, int p) {
  int i;

  val[0] = burst();
  val[1] = envel_diff();  
  //  printf("vals = %f %f\n",val[0],val[1]);
  /*    val[2] = spect_diff();  */
  //   val[2] = low_spect_diff(); 
  //  val[3] = high_spect_diff(); 
  //  val[4] = spect_diff();  /*spect_diff must be called last! */


}



/*
static int burst_signature(n,p)
int n,p; {
  int st,note;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  return( (st == REST_STATE || st == NOTE_STATE || 
    st == RISE_STATE || st == FALL_STATE) ? 0 : 1);  
}

static int envel_diff_signature(n,p)
int n,p; {
  int st,note;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  return( (st == FALL_STATE) ? 0 : (st == RISE_STATE) ? 2 : 1);  
}
*/

static void pitch_feature(val,p)
float *val;
int p; {
  int i,imx,n,lo,hi;
  float mx,m,s,sum,d;

  mx = -HUGE_VAL;
  sum = m = s = d = 0;
  for (i=pitch_int[p].lo; i < pitch_int[p].hi; i++) {
    if (spect[i] > mx) { mx = spect[i]; imx = i; }
    sum += spect[i];
    m += i*spect[i];
    s += i*i*spect[i];
    //    d += max((spect[i]-last_spect[i]),0);
  }
  n = pitch_int[p].hi -  pitch_int[p].lo;
  /*  if (sum > .01) { */
  if (sum > 0.) {
    m /= sum;
    /*    val[1] = s/sum - m*m;*/
    val[0] = m*m - s/sum;  /* negative of empirical variance */
  }
  /* this looks like a bug in next statment (should be val[0]) */
  else val[1] = n*n/12.;  /* uniform variance */
/*   val[0] = (float) imx; */
  lo = (imx == pitch_int[p].lo) ? imx : imx-1;
  hi = (imx == pitch_int[p].hi) ? imx : imx+1;
  m = sum = 0;
  for (i=lo; i <= hi; i++) {
    m += i*spect[i];
    sum += spect[i];
  }
  /*  val[0] = (sum > 0.) ? m/sum : (float) imx;*/
  val[1] = (sum > 0.) ? m/sum : (float) imx;
  //  val[1] = (float) imx;
  //  val[2] = d;

  //    val[2] = sum;  /* a new feature 1-02 */
    
}


static void attack_pitch_feature(float *val, int p) {
  int i,imx,n,lo,hi;
  float mx,m,s,sum,d;

  d = 0;
  for (i=attack_pitch_int[p].lo; i < attack_pitch_int[p].hi; i++) {
    d += max((spect[i]-last_spect[i]),0);
  }
  val[0] = d;
}

static void pitch_burst_feature(val,p)
float *val;
int p; {
  int i,imx,n,lo,hi;
  float mx,m,s,sum,d;

  d = 0;
  for (i=pitch_int[p].lo; i < pitch_int[p].hi; i++) {
    d += max((spect[i]-last_spect[i]),0);
  }
  val[0] = d;
}

#define TWO_HARM_INSIDE_WINDOW -1


static void get_harm_table()
{
  int i,j,k,q,too_hi,too_lo,lo,hi;
  float lim,omega /*hz2omega(),*//*midi2omega()*/;

  // initialization of the table

  // for (i=0; i<MIDI_PITCH_N; i++) {
  //for (j=0; j<MAX_PITCH_INTERVALS; j++)
  //  harm_table[i][j] = 0;
  //}
  lim = hz2omega(MAX_HZ_INFL); 
  // printf("pitch_int_num = %d \n lim = %f \n",pitch_int_num,lim);
  
  for (i=0; i<MIDI_PITCH_N; i++) {
    omega = midi2omega((float) i);       // base frequency    ?????
    //printf("omega = %f \n",omega);	
    for (j=0; j<pitch_int_num; j++) {
      lo = pitch_int[j].lo;       // bin limits
      hi = pitch_int[j].hi;
      //printf("lo = %d hi = %d \n",lo,hi);
      k = 0;
      while (k*omega < lo) k++;
      //printf("k = %d \n ",k);
  
      if ((k+1)*omega < hi) {  /* two harmonic inside window */
	harm_table[i][j] = TWO_HARM_INSIDE_WINDOW;
	continue;
      }
      if (k*omega > hi) { // no harm inside bin
	too_hi = (k*omega - hi > lim);
	too_lo = (lo - (k-1)*omega > lim);
	if (too_hi && too_lo) {  // no close harm
	  harm_table[i][j] = 0;
	  //printf("l1 i = %d j = %d q = %d \n",i,j,harm_table[i][j]);
	  continue;
	}
	if (k*omega - hi < lo - (k-1)*omega) { // upper harm closer
	  harm_table[i][j] = NUM_DIVIDES+1;
	  //printf("l2 i = %d j = %d q = %d \n",i,j,harm_table[i][j]);
	  continue;
	}
	else {
	  harm_table[i][j]  = 1;
	  //printf("l3 i = %d j = %d q = %d \n",i,j,harm_table[i][j]);
	  continue;
	}
      }
      else for (q=0; q<NUM_DIVIDES; q++) 
	if (k*omega < pitch_int[j].div[q]) break;
      harm_table[i][j] = q+1;
      //printf("l4 i = %d j = %d q = %d \n",i,j,harm_table[i][j]);
    }
  }
}



static int sa_pitch_signature_mine(SA_STATE sas, int p)
{
   /* sas.shade == 0 tells solo is not playing  
     if sas.shade != 0 sas.midi_pitch gives the pitch that is playing
     sas.acc_notes->num gives total # of accomp notes playing
    sas.acc_notes->snd_nums[0]...sas.acc_notes->snd_nums[n-1] are the midi # of the accomp. */

  int i,n,q,tally[NUM_DIVIDES+2],aq,sq,numa,tp,lp;
  
  tp = harm_table[sas.midi_pitch][p];  /* a value of 0 means no near harmonic */
  lp = harm_table[sas.last_pitch][p];  /* ditto */

  if (sas.shade == REST_STATE) sq = 0;
  else if (sas.shade == NOTE_STATE) sq = tp;
  else if (sas.shade == FALL_STATE  || sas.shade == ATTACK_STATE ||
	   sas.shade == REARTIC_STATE  || sas.shade == SLUR_STATE || 
	   sas.shade == RISE_STATE)  
    sq = (tp != 0) ? tp : (is_a_rest(sas.last_pitch)) ? 0 : lp; 
  else { printf("unknown state %d in sa_pitch_signature_mine\n"); exit(0); } 


  
  /*  sq = (sas.shade == REST_STATE) ? 0 : harm_table[sas.midi_pitch][p];*/
  /* 0 if no sounding solo harmonic in window.  ow quanta of center of harmonic */

  for (q=0; q < NUM_DIVIDES+2; q++) tally[q] = 0;
  numa = 0;
  for (i=0; i < sas.acc_notes->num; i++) {
    n = sas.acc_notes->snd_nums[i];
    q = harm_table[n][p];
    if (q != 0 && tally[q] == 0) { aq = q; numa++; tally[q] = 1; }
  }
  /* numa is number of quanta where there are accomp harmonics.   */
  if (sq == 0) {
    if (numa == 0) return(0);  /* no harmonics from either solo or accomp */
    if (numa >= 2) return(1);  /* multiple accompaniment harms and no solo */
    return(2+2*aq);  /* even signatures for accompaniment only */
  }
  return(3+2*sq);   /* odd signautures when solo plays */
}


static int sa_ps_num[MAX_FEATURES];       /* number of harmonics in pth pitch 
					     feature window */
static int sa_ps_acc_quant[MAX_FEATURES];  /* the quantum of one of the harms.
					      only useful when number of harms is 1 */
     
void init_sa_pitch_signature_table(SOUND_NUMBERS *acc_notes) {
  int p,q,tally[NUM_DIVIDES+2],n,numa,aq,i;
  
  for (p=0; p < pitch_int_num; p++) {  /* each feature pitch window */
    for (q=0; q < NUM_DIVIDES+2; q++) tally[q] = 0;  /* possible quanta initialized */
    numa = 0;  /* number of accompaniment harmonics in window */
    for (i=0; i < acc_notes->num; i++) {  /* for each currently sounding acc note */
      n = acc_notes->snd_nums[i];
      q = harm_table[n][p];  /* quantum of harm  in pth feature window (0 if none ) */
      if (q != 0 && tally[q] == 0) { aq = q; numa++; tally[q] = 1; }
    }
    sa_ps_num[p] = numa;  /* number of different harms in pth pitch feature window */
    sa_ps_acc_quant[p] = aq; /* the quantum of one of the harms. 
				only useful when the number of harms is one */
  }
}



static int sa_pitch_signature_table(SA_STATE sas, int p)
{
   /* sas.shade == 0 tells solo is not playing  
     if sas.shade != 0 sas.midi_pitch gives the pitch that is playing
     sas.acc_notes->num gives total # of accomp notes playing
    sas.acc_notes->snd_nums[0]...sas.acc_notes->snd_nums[n-1] are the midi # of the accomp. */

  int i,n,q,tally[NUM_DIVIDES+2],aq,sq,numa,tp,lp,son,two_solo,x,last_rest,this_rest;
  
  this_rest = is_a_rest(sas.midi_pitch);
  last_rest = is_a_rest(sas.last_pitch);

  tp = is_a_rest(sas.midi_pitch) ? 0 : harm_table[sas.midi_pitch][p];  


  /* a value of 0 means no near harmonic */
  lp = is_a_rest(sas.last_pitch) ? 0 : harm_table[sas.last_pitch][p];  /* ditto */

  /* added the conditional assignment above */



    /* start of note */
    son = (sas.shade == FALL_STATE || sas.shade == ATTACK_STATE || sas.shade == SLUR_STATE);
    
    //    if (son && this_rest == 0 && last_rest == 0) tp = lp;
    //        if (son && tp != 0 && lp != 0) tp = lp;
    two_solo = (son && tp != 0 && lp != 0 && tp != lp);
#ifdef HORN_LISTEN_EXPERIMENT
    if (two_solo) tp = lp;
#endif
    if (son && tp == 0) sq = lp;
    else sq = tp;
    

    /*  if (sas.shade == REST_STATE) sq = 0;
  else if (sas.shade == NOTE_STATE) sq = tp;
  else if (sas.shade == FALL_STATE  || sas.shade == ATTACK_STATE ||
	   sas.shade == REARTIC_STATE  || sas.shade == SLUR_STATE ||  
	   sas.shade == RISE_STATE)  
  sq = (tp != 0) ? tp : (is_a_rest(sas.last_pitch)) ? 0 : lp; 

  else { printf("unknown state %d in sa_pitch_signature_table\n"); exit(0); } */

  
  /*  sq = (sas.shade == REST_STATE) ? 0 : harm_table[sas.midi_pitch][p];*/
  /* 0 if no sounding solo harmonic in window.  ow quanta of center of harmonic */

  numa = sa_ps_num[p];
  aq = sa_ps_acc_quant[p];  
  /* numa is number of quanta where there are accomp harmonics.   */
  //    if (two_solo) return(1);  /* added this */


  //    if (son && sq) return(2); /* experiment */

  //    if (son && sq) return(1); /* experiment */
  /* this would have to be modified since I am using an accomp only signature here */



  /*   if (sq == 0 && numa == 0) return(0);
  if (tp == TWO_HARM_INSIDE_WINDOW || numa >= 2) return(1);
  if (sq == 0) return(2+3*aq);
  if (numa == 0) return(3+3*sq);
  return(4+3*sq);*/


  if (tp == TWO_HARM_INSIDE_WINDOW) return(1);
  if (sq == 0) {
    if (numa == 0) return(0);  /* no harmonics from either solo or accomp */
    if (numa >= 2) return(1);  /* multiple accompaniment harms and no solo */
    return(2+2*aq);  /* even signatures for accompaniment only */
  }
  return(3+2*sq);   /* odd signautures when solo plays */
 

  

}


static int sa_pitch_burst_signature(SA_STATE sas, int p)
{
   /* sas.shade == 0 tells solo is not playing  
     if sas.shade != 0 sas.midi_pitch gives the pitch that is playing
     sas.acc_notes->num gives total # of accomp notes playing
    sas.acc_notes->snd_nums[0]...sas.acc_notes->snd_nums[n-1] are the midi # of the accomp. */

  int i,n,q,tally[NUM_DIVIDES+2],aq,sq,numa,tp,lp,son,two_solo,x;
  
  tp = is_a_rest(sas.midi_pitch) ? 0 : harm_table[sas.midi_pitch][p];  
  /* a value of 0 means no near harmonic */
  lp = is_a_rest(sas.last_pitch) ? 0 : harm_table[sas.last_pitch][p];  /* ditto */




    /* start of note */
  son = (sas.shade == FALL_STATE  || sas.shade == ATTACK_STATE ||sas.shade == SLUR_STATE);
    
  if (son && tp) return(1); 
  return(0);
}


#define ATTACK_HARM_NUM 4

static int sa_attack_pitch_signature(SA_STATE sas, int p)  {
  int i,j,m,v,son,r;
  SOUND_NUMBERS notes;
  float omega;


  son = (sas.shade == FALL_STATE  || sas.shade == ATTACK_STATE ||sas.shade == SLUR_STATE);
  if (son == 0) return(0);
  notes = score.solo.note[sas.index].snd_notes;
  m = ATTACK_HARM_NUM + 1;
  for (i=0; i < notes.num; i++) {
    if (notes.attack[i] == 0) continue;
    omega = sol[notes.snd_nums[i]].omega;
    for (j=1; j <= ATTACK_HARM_NUM; j++) {
      v = (int)(j*omega + .5);
      if (v < attack_pitch_int[p].lo || v > attack_pitch_int[p].hi) continue; 
      if (j < m) m = j;
    }
  }
  r  = ATTACK_HARM_NUM + 1 - m;
  return(r);
}



static int sa_pitch_signature(SA_STATE sas, int p) {
  int a,b;

#ifndef SA_PITCH_TABLE_IMPROV  
  a = sa_pitch_signature_mine(sas, p);
  b = sa_pitch_signature_table(sas, p);
  if (a != b) { printf("problem in sa_pitch_signature\n"); exit(0); }
  return(a); 
#endif

#ifdef SA_PITCH_TABLE_IMPROV  
  return(sa_pitch_signature_table(sas, p));
#endif

}





static int sa_pitch_signature_maria(SA_STATE sas, int p)
{
   /* sas.shade == 0 tells solo is not playing  
     if sas.shade != 0 sas.midi_pitch gives the pitch that is playing
     sas.acc_notes->num gives total # of accomp notes playing
    sas.acc_notes->snd_nums[0]...sas.acc_notes->snd_nums[n-1] are the midi # of the accomp. */

  int i,j,n,q,an,m,aux_list[MAX_PITCH_INTERVALS];
  


  // calculation of the signatures
  
  //if (sas.shade == REST_STATE) return(0);
  //else {
    //n = sas.midi_pitch;
  //printf("n = %d p = %d \n",n,p);
  //q = harm_table[n][p];
  //printf("n = %d p = %d q = %d \n",n,p,q);
  //return(q);
  //}
  if (sas.shade != REST_STATE) { // solo sounding
    n = sas.midi_pitch;
    q = harm_table[n][p];
    return(3+2*q);
    
  }

  // initialization of the aux array to zero

  for (i=0; i<MAX_PITCH_INTERVALS; i++) aux_list[i] = 0;

  an = (sas.acc_notes -> num); // total # of accomp notes sounding
  j = 0;
  m = 0; // m holds total # of accomp harms on bin

  for (i=0; i<an; i++) { // aux_list holds accomp notes on bin
    n = sas.acc_notes -> snd_nums[i];
    aux_list[j] = harm_table[i][j];
    if (aux_list[j] != 0) ++m;
    ++j;
  }

  if (m = 0) return(0); // no accomp sounding
  else { 
    if (m = 1) {        // only one accomp harm on bin
      j = 0;
      while (aux_list[j] == 0) ++j;
      q = harm_table[j][p];
      return(2+2*q);
    }
    else return(1);
  }
}  







 
static int pitch_signature(n,p)
int n,p; {
  int st,note,k=0,k_trill = 0,too_hi,too_lo,q,i,hs,t_too_hi,t_too_lo;
  float omega,omega_trill,lo,hi,lim/*,hz2omega()*/;
  FULL_STATE fs;

  index2state(n,&st,&note,&fs);
  if (st == REST_STATE) return(0);
  lim = hz2omega(MAX_HZ_INFL);
  lo =  pitch_int[p].lo;
  hi =  pitch_int[p].hi;              
  omega = sol[note].omega;

  while (k*omega < lo) k++;

  if (st == TRILL_WHOLE_STATE || st == TRILL_HALF_STATE) { /* now used now */
    hs =  (st == TRILL_HALF_STATE) ? 1 : 2;
    omega_trill = sol[note+hs].omega;
    while (k_trill*omega_trill < lo) k_trill++;
    too_hi =(k*omega - hi > lim);  
    too_lo =  (lo - (k-1)*omega > lim);
    t_too_hi = (k_trill*omega_trill - hi > lim);  
    t_too_lo =  (lo - (k_trill-1)*omega_trill > lim);
    if (too_hi && too_lo && t_too_hi && t_too_lo) return(0);  /* neither note near band */
    else return(NUM_DIVIDES+1);
  }
  if (k*omega > hi) {  /* no harmonic inside band */
    too_hi = (k*omega - hi > lim);  
    too_lo =  (lo - (k-1)*omega > lim);
    if (too_hi && too_lo) /* no close harmonics */
      return(0);
    if (k*omega - hi < lo - (k-1)*omega) /* upper harm closer */
      q = NUM_DIVIDES;
    else q = 0;
  } 
  else   for (q=0; q < NUM_DIVIDES; q++) 
    if (k*omega < pitch_int[p].div[q]) break;
  return(q+1);
}


      

static void wt(t,fp)
TNODE *t;
FILE *fp; {
  
  fwrite(t,sizeof(TNODE),1,fp);
  if (t->split != -1) {
    wt(t->lc,fp);    
    wt(t->rc,fp);    
  }
}



static void wt2(TNODE *t ,FILE *fp, int sigs, int level, float w) {
  int s;
  
  for (s=0; s < level; s++) fprintf(fp, "  ");
  fprintf(fp,"split = %d  cutoff = %f h = %f\n",
	  t->split,(t->split == -1) ? 0. : t->cutoff,t->entropy_cont);
  if (t->split == -1) {
    fprintf(fp,"prob = ");
    for (s=0; s < sigs; s++) fprintf(fp,"%f ",pow(t->cond_dist[s],1./w));
    /* undo the weight so that it can be changed on input */
				     
    fprintf(fp,"\n");
  }
  else {
    wt2(t->lc,fp,sigs,level+1,w);    
    wt2(t->rc,fp,sigs,level+1,w);    
  }
}

#define TREE_FILE "tree.dat"

static void write_trees() {
  FILE *fp;
  char file[100];
  int f;
			
/*  strcpy(name,scorename);
  strcat(name,".tree"); */
  strcpy(file,TRAIN_DIR);
  strcat(file,TREE_FILE);
  fp = fopen(file,"w");
  for (f=0; f < feature.num; f++) wt(feature.el[f].tree,fp);
  fclose(fp);
}    
  


static void rt(t,fp)
TNODE **t;
FILE *fp; {
  
  *t = new_tnode();
  fread(*t,sizeof(TNODE),1,fp);
  if ((*t)->split != -1) {
    rt(&((*t)->lc),fp);    
    rt(&((*t)->rc),fp);    
  }
}

static void rt2(TNODE **t ,FILE *fp , int sigs, float w) {
  int s,sp;
  float x,cu,h;
  char st1[100],st2[100],st3[100];

  *t = new_tnode();
  /*  fscanf(fp,"split = %d  cutoff = %f h = %f\n",
      &((*t)->split),&((*t)->cutoff),&((*t)->entropy_cont)); */
  fscanf(fp,"%s = %d  %s = %f %s = %f",st1,&sp,st2,&cu,st3,&h);
  if (strcmp(st1,"split") != 0) { printf("training file bad\n"); 
  printf("st1 = %s\n",st1);
  exit(0); 
  }
  if (strcmp(st2,"cutoff") != 0) { printf("training file bad2\n"); exit(0); }
  if (strcmp(st3,"h") != 0) { printf("training file bad3\n"); exit(0); }
  (*t)->split = sp; (*t)->cutoff = cu; (*t)->entropy_cont = h;
  if ((*t)->split == -1) {
    fscanf(fp,"%s %s",st1,st2);
    if (strcmp(st1,"prob") != 0) { printf("training file bad4\n"); exit(0); }
    if (strcmp(st2,"=") != 0) { printf("training file bad5\n"); exit(0); }
    //    fscanf(fp,"prob = ");
    for (s=0; s < sigs; s++) {
      //      fscanf(fp,"%f ",&x);
      if (fscanf(fp,"%f",&x) == 0) 
	{ printf("wrong number of features\n"); exit(0); }
      //     (*t)->cond_dist[s] = x; 
      (*t)->cond_dist[s] = pow(x,w); 

     /* maybe should modify power to weight here */
    }
      
    //    fscanf(fp,"\n");
  }
  else {
    rt2(&((*t)->lc),fp,sigs,w);    
    rt2(&((*t)->rc),fp,sigs,w);    
  }
}




static void read_trees() {
  FILE *fp;
  char file[100];
  int f;
			
/*  strcpy(name,scorename);
  strcat(name,".tree"); */
  strcpy(file,TRAIN_DIR);
  strcat(file,TREE_FILE);
  fp = fopen(file,"r");
  if (fp == NULL) {
    printf("couldn't read trees \n");
    return;
  }
  for (f=0; f < feature.num; f++) rt(&feature.el[f].tree,fp);
  fclose(fp);
}    
  


static void pt(t,sigs) 
TNODE *t; {
  int s;

  if (t->split != -1) {
    printf("cutoff = %f split = %d\n",t->cutoff,t->split);
    pt(t->lc,sigs);    
    pt(t->rc,sigs);    
  }
  else {
    printf("prob = ");
    for (s=0; s < sigs; s++) printf("%f ",t->cond_dist[s]);
  }
}


static void print_trees() {
  int f;

  for (f=0; f < feature.num; f++) 
    pt(feature.el[f].tree,feature.el[f].num_sig);
  printf("\n");
}




write_distributions() {
  FILE *fp;
  int a,i,j,f,s,q;
  BETA *b;
  char file[100];
  FEATURE *feat;
			
  strcpy(file,TRAIN_DIR);
  strcat(file,DISTRIBUTION_FILE);
  fp = fopen(file,"w");
  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    wt2(feat->tree,fp,feat->num_sig,0,feat->weight);
    fprintf(fp,"\n\n");
  }
  fclose(fp);
}

write_distributions_file(char *dist_file) {
  FILE *fp;
  int a,i,j,f,s,q;
  BETA *b;
  char file[100];
  FEATURE *feat;
			
  fp = fopen(dist_file,"w");
  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    wt2(feat->tree,fp,feat->num_sig,0,feat->weight);
    fprintf(fp,"\n\n");
  }
  fclose(fp);
}

void
read_distributions() {
  FILE *fp;
  int a,d,i,j,r,f,s,ff,ss,q,sigs,quants;
  BETA *b;
  char file[100],st[100];
  FEATURE *feat;
  float **matrix();

			
  strcpy(file,TRAIN_DIR);
  strcat(file,DISTRIBUTION_FILE);
  fp = fopen(file,"r");
  if (fp == NULL) { 
    printf("no distributions to read in %s\n",file);
    exit(0);
     return;
  }
  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    rt2(&(feat->tree),fp,feat->num_sig,feat->weight);
    fscanf(fp,"\n\n"); 
  }

/*
  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    for (s = 0; s < feat->num_sig; s++) {
      fscanf(fp,"feature = %d  signature = %d quanta = %d\n",&ff,&ss,&q);
      if (f != ff || s != ss) {
	printf("problem in read_distributions()\n");
	printf("not reading distribuitons\n");
	fclose(fp);
	return;
      }
      if (s == 0) feat->dist = matrix(0,feat->num_sig-1,0,q-1);
      feat->num_quanta = q;
      for (j=0; j < feat->num_quanta; j++) {
	fscanf(fp,"%f",&((feat->dist)[s][j]));
      }
      fscanf(fp,"\n");
      rt2(&(feat->tree),fp);
      fscanf(fp,"\n\n"); 
    }
  } */
  fclose(fp);
}


int
read_distributions_file(char *dist_file) {
  FILE *fp;
  int a,d,i,j,r,f,s,ff,ss,q,sigs,quants;
  BETA *b;
  char file[100],st[100];
  FEATURE *feat;
  float **matrix();

			
  fp = fopen(dist_file,"r");
  if (fp == NULL) { 
    printf("no distributions to read \n");
    return(0);
  }
  for (f=0; f < feature.num; f++) {
    feat = feature.el+f;
    rt2(&(feat->tree),fp,feat->num_sig,feat->weight);
    fscanf(fp,"\n\n"); 
  }
  fclose(fp);
  return(1);
}


  

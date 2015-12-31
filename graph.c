#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "share.c"
#include "global.h"
#include "new_score.h"


#define SHORT_NOTE_HAS_REST



void
add_arc(n1,n2,p)
GNODE *n1,*n2;
float p; {
  int num1,num2;

  if (p > 1) {
    printf("bad probability\n");
    exit(0);
  }
  if ( p <= 0.) return;
  if (n1 == NULL || n2 == NULL) {
    printf("can't add to null node\n");
    exit(0);
  }
  num1 = n1->next.num;
  if (num1 == n1->next.limit) { 
printf("can't add next arc lim = %d\n",n1->next.limit); exit(0); }
  num2 = n2->prev.num;
  if (num2 == n2->prev.limit) { 
printf("can't add prev arc lim = %d\n",n2->prev.limit); exit(0); }
  n1->next.list[num1].ptr = n2;
  n1->next.list[num1].prob = p;
  (n1->next.num)++;
  n2->prev.list[num2].ptr = n1;
  n2->prev.list[num2].prob = p;
  (n2->prev.num)++;
}


#define SHORT_NOTE_THRESH 5




#define NEXT_LIM 5
#define PREV_LIM 20


int mask_note(n)
int n; {
     if (n == firstnote-1 || n == lastnote+1) return(RESTNUM);
     return(score.solo.note[n].num);
 }

#define MAX_GNODE 100000 //50000  // might need to increase this ... schumann pc needed more

typedef struct {
  int num;
  GNODE *nd_buff[MAX_GNODE];
}  GNODE_MEM;

GNODE_MEM gnode_mem;


void
alloc_gnode_buff() {
  int i,a,b;

  for (i=0; i < MAX_GNODE; i++) {
    gnode_mem.nd_buff[i] = (GNODE *) malloc(sizeof(GNODE));
    gnode_mem.nd_buff[i]->next.list = (ARC *) malloc(sizeof(ARC)*NEXT_LIM);
    gnode_mem.nd_buff[i]->prev.list = (ARC *) malloc(sizeof(ARC)*PREV_LIM);
  }
  a = gnode_mem.nd_buff[0];
  b = gnode_mem.nd_buff[MAX_GNODE-1];
  //  printf("alloced %d\n",b-a);
}

static void
init_gnode_buff() {
  gnode_mem.num = 0;
}


//CF:  like malloc for node creation requests
//CF:  NB we never need to free memory so this is easy.
static GNODE* 
grab_gnode() {
  if (gnode_mem.num >= MAX_GNODE) {
    printf("out of room in grab_gnode()  max=%d\n",MAX_GNODE);
    exit(0);
  }
  return(gnode_mem.nd_buff[gnode_mem.num++]);   //CF:  preallocated storage for GNODEs   .num is index of next free space.
}

//CF:  Constructor for one GNODE.  Gets some storage and fills it.
//CF:  note = int index into score
//CF:  type = REST_STATE, ATTACK_STATE etc.   NB. different from note TYPES (short note, rearticulated note etc)
//CF:  So these are types for a GNODE rather than for a NOTE.
//CF:  pos is important if zero (0=this is an onset)  (used later when asking questions about which notes have occurred)
//CF:  mostly not used.
GNODE *get_gnode(type,note,pos,is_trill,row)
int type,note,pos,is_trill,row; {
  GNODE *temp;


  if (type > TRILL_WHOLE_STATE) { printf("weird state in get_gnode\n"); exit(0); }
  //  temp = (GNODE *) malloc(sizeof(GNODE));
  temp = grab_gnode();
  temp->state.statenum = type;
  //  temp->is_trill = is_trill;
  temp->row = row;
  switch (type) {
    case REST_STATE : 
      temp->state.arg[THIS_PITCH] = RESTNUM;  /*IRLVNT;*/
      temp->state.arg[LAST_PITCH] = RESTNUM; /*IRLVNT;*/
      break;
    case SLUR_STATE :
      if (note > 0)
        temp->state.arg[LAST_PITCH] = mask_note(note-1);
      temp->state.arg[THIS_PITCH] =   (is_trill) ? 
	score.solo.note[note].trill : 
	  mask_note(note);

 
      /*      temp->state.arg[THIS_PITCH] = mask_note(note); */
      break;
    default :
      /*      temp->state.arg[LAST_PITCH] = IRLVNT;  old way */
      if (note <= 0) temp->state.arg[LAST_PITCH] = RESTNUM;
      else if (is_trill) temp->state.arg[LAST_PITCH] = mask_note(note);
      else  temp->state.arg[LAST_PITCH] = mask_note(note-1);
      temp->state.arg[THIS_PITCH] =   (is_trill) ? score.solo.note[note].trill :  mask_note(note);
      break;
  }
  temp->visit = 0;
  temp->note = note;
  temp->pos = pos;
  temp->next.limit = NEXT_LIM;
  temp->next.num = 0;
  //  temp->next.list = (ARC *) malloc(sizeof(ARC)*NEXT_LIM);
  temp->prev.limit = PREV_LIM;
  temp->prev.num = 0;
  //  temp->prev.list = (ARC *) malloc(sizeof(ARC)*PREV_LIM);
  //  printf("pos = %d note = %d\n",pos,note);
if (temp->state.arg[0] == 34)
printf("%d %d\n",temp->state.statenum,temp->state.arg[0]);

/* if (temp->state.arg[0] == 0 && temp->state.statenum == 2) {
   printf("eureka mask_note(%d) = %d\n",note,mask_note(note));
   //   exit(0);
   }*/
  return(temp); 
}
#ifdef GRAPH_EXPERIMENT
#define PROB_CONT  .75 // /*.9995 */  /*.5*/ .995 /* prob of a note continuing to sound rather than resting */
#else
  #define PROB_CONT   /*.9995 */  /*.5*/ .995 /* prob of a note continuing to sound rather than resting */
#endif
#define MAX_NOTES 1000

static void check_sum(nd)
GNODE *nd; {
  int i,this,that;
  float sum=0;

  if (nd->visit == 1) return;
  nd->visit = 1;
  if (nd->state.statenum > TRILL_WHOLE_STATE) {
    printf("unrecognized state %d num = %d pos = %d\n",nd->state.statenum,nd->note,nd->pos);
    exit(0);
  } 
  for (i=0; i < nd->prev.num; i++) 
    if (nd->prev.list[i].prob > 1) {
      printf("impossible probability in check_sum()\n");
      printf("note = %d pos = %d prob = %f\n",
	     nd->note,nd->pos,nd->prev.list[i].prob);
    }
  for (i=0; i < nd->next.num; i++) sum += nd->next.list[i].prob;
  if (fabs(sum-1) > .01) {
    printf("problem in check_sum()\n");
    printf("  sum = %f note = %d pos = %d\n",sum,nd->note,nd->pos);
  }
  this = 1000*nd->note + nd->pos;
/* printf("this = %d\n",this); */
  for (i=0; i < nd->next.num; i++) check_sum(nd->next.list[i].ptr); 
}



#define TEST_ITERS 1000

static void test_note_length_dists() {
  int i,n,r,count;
  float one[MAX_SOLO_NOTES],two[MAX_SOLO_NOTES];
  GNODE *cur;
  float rr,total,m,v,mhat,vhat,s,shat;
  //  float drand48();

  printf("testing length distributions\n");
  for (i=firstnote; i <= lastnote; i++) one[i] = two[i] = 0;
  for (n=0; n < TEST_ITERS; n++) {
    cur = start_node;
    while (cur->note <= lastnote) {
      count++;
      rr = drand48();
      rr = rand() / (float) RAND_MAX;
      total = 0;
      for (r=0; r < cur->next.num; r++)  {
	total += cur->next.list[r].prob;
	if (total >= rr) break;
      }
      if (r >=  cur->next.num) {
	printf("problem in test_note_length_dists()\n");
	exit(0);
      }
      if (cur->note != cur->next.list[r].ptr->note) {
	if (cur->note >= 0) {
	  one[cur->note] += count;
	  two[cur->note] += count*count;
	}
	count = 0;
      }
      cur = cur->next.list[r].ptr;
    }
  }
  for (i=firstnote; i <= lastnote; i++) {
    mhat = one[i]/TEST_ITERS;
    vhat = two[i]/TEST_ITERS - mhat*mhat;
    shat = sqrt(vhat);
    m = score.solo.note[i].mean;
    s = score.solo.note[i].std;
    if (fabs(1-mhat/m) > .1 || fabs(1-shat/s) > .2) {
      printf("innacurate length model for note %s\n",score.solo.note[i].observable_tag);
    }
    printf("m = %f mhat = %f s = %f shat = %f\n",m,mhat,s,shat);
  }
  
}


//CF:  not used
static void test_graph()  {
  GNODE *cur;
  int i;
  char c[10],d[10];

  check_sum(start_node); 
  printf("graph ok\n");
  printf("not testing note lengths\n");
  test_note_length_dists(); 
  return;
  cur = start_node;
  while (1) {
/*    num2name(score.solo.note[cur->note].num,c); */
      num2name(cur->state.arg[THIS_PITCH],c);

    state2name(cur->state.statenum,d);
    printf("cur: (%d note=%d pos=%d frac = %f %s %s)\n",cur,cur->note,cur->pos,cur->token_done,c,d);
    printf("next: \n");
    for (i=0; i < cur->next.num; i++){
/*      num2name(score.solo.note[cur->next.list[i].ptr->note].num,c); */
      num2name(cur->next.list[i].ptr->state.arg[THIS_PITCH],c);
      state2name(cur->next.list[i].ptr->state.statenum,d);
      printf("      (%d note=%d pos=%d %s %s %f)\n",
      cur->next.list[i].ptr,cur->next.list[i].ptr->note,
      cur->next.list[i].ptr->pos,c,d,cur->next.list[i].prob);
    }
    printf("prev: \n");
    for (i=0; i < cur->prev.num; i++) {
/*      num2name(score.solo.note[cur->prev.list[i].ptr->note].num,c); */
	num2name(cur->prev.list[i].ptr->state.arg[THIS_PITCH],c);
      state2name(cur->prev.list[i].ptr->state.statenum,d);
     printf("      (%d %d %d %s %s %f)\n",
      cur->prev.list[i].ptr,cur->prev.list[i].ptr->note,cur->prev.list[i].ptr->pos,
c,d,cur->prev.list[i].prob);
   }
    printf("\n next  (n) or previous (p) jump (j): quit (q) \n");
    scanf("%s",c);
    if (c[0] == 'q') exit(0);
    if (c[0] == 'n') {
      i = -1;
      while (i >= cur->next.num || i < 0) {
	printf("which node to expand? ");
	scanf("%d",&i);
      }
      printf("expanding %d\n",i);
      cur = cur->next.list[i].ptr; 
    }
    if (c[0] == 'p')  {
      i = -1;
      while (i >= cur->prev.num || i < 0) {
	printf("which node to expand? ");
	scanf("%d",&i);
      }
      printf("expanding %d\n",i);
      cur = cur->prev.list[i].ptr;  
    }
    if (c[0] == 'j')  {
      printf("which note to jump to? ");
      scanf("%d",&i);
      cur = start_node; 
      while (cur->note != i) 
        if (cur != cur->next.list[0].ptr)   cur = cur->next.list[0].ptr;
        else   cur = cur->next.list[1].ptr;
    }
  }
}


//CF:  compute n and p for a chain model, given required mean and variance
//CF:  choose n first becuase p is continuous and get then be set to make mean exact.
#define MAX_CHAIN  11 /* 15/*   /* has strong effect on computational demand but allows better approx of variance on long notes */
#define MIN_CHAIN 5
moments2parms(m,ss,n,p,note) /* converts mean + var to n , p for neg binomial dist */
float m,ss,*p; /* p is prob of LEAVING state */
int *n; {
  float tb,vhat,mhat;    /* mean = n/p  var = nq/p^2 */

  //CF:  compute n (automatically casted to int).  Truncate to MAX_CHAIN.
  *n = m/(ss/m + 1);
  if (*n > MAX_CHAIN) *n = MAX_CHAIN;
/*   if (*n < MIN_CHAIN) *n = MIN_CHAIN; */
  if (*n < 1) *n=1;

  if (*n < 1 || *n > MAX_CHAIN) 
    printf("trouble in moments2parms() n = %d\n",*n);

  //CF:  choose p to make the mean exact
  *p = *n / m ;    /* gives desired mean */

  if (*p > 1) { //CF:  never happens?
    printf("impossible trans prob\n");
    exit(0);
  }
  /*  *p = (sqrt(1 + 4*ss/(*n)) - 1) / (2*ss/(*n));   /* gives desired variance ss.  */

  //CF:  mean and var actually produced by the approx distro.
  //CF:  (the mean should mattachexactly, mhat=m, but the var may be different)
  mhat = (*n)/(*p);
  vhat = (*n)*(1-(*p)) / ((*p)*(*p)); 

/*printf("mean = %f std = %f n= %d p = %f np_mean = %f np_std = %f\n",m,sqrt(ss),*n,*p,*n / *p,sqrt(*n * (1- *p) / (*p * *p))); */
  if ( ((m-mhat) > 1) || ((ss-vhat) > 1)) {
    printf("problem matching desired distribution\n");
    printf("m = %f mhat = %f v = %f vhat = %f n = %d *p = %f (MAX_CHAIN = %d)\n",m,mhat,ss,vhat,*n,*p,MAX_CHAIN);
    printf("note is %s\n",score.solo.note[note].observable_tag);
    }
}










/*********************************************************************/


typedef struct {
  GNODE *head;
  GNODE *tail;
  GNODE *alt_head;     //CF:  alternate head for trills (not used)
  GNODE *alt_tail;     //CF:  altertate tail for trills (not used)
} SUB_GRAPH;


typedef struct {        
  SUB_GRAPH resting;
  SUB_GRAPH singing;
} PARALLEL_GRAPH; 

typedef struct {             //CF:  graph for one complete note
  PARALLEL_GRAPH prefix;     //CF:  its made of a prefix (attack) and chain (sustain) strung together
  PARALLEL_GRAPH chain;
} NOTE_GRAPH; 



#define MAX_SHORT_PREFIX_LEN 15 /* should relate this to PREV_LIM */
/* setting this equal to 5 gave better results in no-training mendelssohn recognition
   this suggests the normal model is bad */

static int
create_cum_dist(float *dist, float m, float v, int minl)  {/* dist[i] = P(X <= i) */
  /*  set  dist[1... MAX_SHORT_PREFIX_LEN] */
  int i;
  float mahal,sum=0,mean,var,a,b,x,k,theta;

  /*    printf("m = %f v = %f minl = %d\n",m,v,minl);*/
  if (m > MAX_SHORT_PREFIX_LEN) return(0);
  if (minl > MAX_SHORT_PREFIX_LEN) return(0);
  //  k = m*m/v;  /* gamma parameters from m,v */
  
  //  theta = v/m;
  for (i = 1; i <= minl-1; i++) dist[i] = 0;
  for (i = minl; i <= MAX_SHORT_PREFIX_LEN; i++) {
#ifdef GAMMA_EXPERIMRNT
    x = (float) i;
    dist[i] = (i == 0) ? 0 : pow(x,k-1)*exp(-x/theta);
#else
    mahal = (i-m)*(i-m)/v;
    dist[i] = exp(-0.5*mahal);
#endif
    //    printf("x = %f k = %f dist = %f\n",x,k,dist[i]);
  }
  for (i = 1; i <=  MAX_SHORT_PREFIX_LEN; i++) sum += dist[i];
  if (sum <= 0) {
    printf("problem in create_cum_dist\n");
    printf("minl = %d MAX_SHORT_PREFIX_LEN = %d m = %f\n",minl,MAX_SHORT_PREFIX_LEN,m );
    for (i = minl; i <= MAX_SHORT_PREFIX_LEN; i++) printf("dist[%d] = %f\n",i,dist[i]);

    exit(0);
  }

  //CF:  We now have an ideal target distribution in dist.  
  //CF:  Now try to approximate it with a markov model

  //CF:  mean and var of the finite, cropped, discretized ideal dist model
  for (i = 1; i <=  MAX_SHORT_PREFIX_LEN; i++) dist[i] /= sum;  //CF:  normalize dist
  mean = var = 0;
  for (i = 1; i <=  MAX_SHORT_PREFIX_LEN; i++) {
    mean += i*dist[i];
    var += i*i*dist[i];
  }
  var -= mean*mean;
  
  //CF:  if cropped discretization is a poor approximation of the ideal, then die 
  if ((fabs(mean-m) > 1) || (fabs(sqrt(var)-sqrt(v)) > 1)) return(0); //CF:  0 is boolean false

  //CF:  else compute the cumulate distibution, by summing up the dist array.
  for (i = 2; i <=  MAX_SHORT_PREFIX_LEN; i++) {
    dist[i] += dist[i-1];
    if (dist[i] > 1) dist[i] = 1; /* weird numerical problem */
  }
  return(1);
}


#define VAR_TAIL_FRAC .2  /* fraction of variance due to self-loop and end*/
#define MIN_LENGTH_FRAC .666

static int min_length(m)
float m; {
  int r;
  r = (int) (m*MIN_LENGTH_FRAC);
  if (r < 1) r = 1;
  return(r);
}


//CF:  decidces if we can represent this note as verbose
static int
short_prefix_parms(int n, float *tail_p, float *dist) {
  /* returns a boolean value if it is possible to find parameters tailp, and dist such that they
     give the desired mean and stdev when used as paramters in the model for a 
     short note prefix. */
  
  float m,v,tail_m,tail_v,m_alt,v_alt;
  int minl,b;
  
  m = score.solo.note[n].mean;   //CF:  desired mean and variance
  v = score.solo.note[n].std;
  v *= v;
  tail_v = v*VAR_TAIL_FRAC;
  *tail_p = 2/(1+sqrt(1+4*tail_v));  /* one-chain self loop gives dsrd  variance*/
  tail_m = 1. / *tail_p;
  minl = min_length(m);   //CF:  minimum number of frames the note can sound, a constraint, as a fn of the mean length
  m_alt = m-tail_m;
  if (m_alt < 0) m_alt = 0;
  v_alt = v-tail_v;
  //  if (v_alt < 0) v_alt = 0;
  if (v_alt < .1) v_alt = .1;
  //  b = create_cum_dist(dist,m-tail_m,v-tail_v,minl-1);
  b = create_cum_dist(dist,m_alt,v_alt,minl-1);   //CF:  true if we can match the parameters well enough
  return(b);
}



static int
is_reartic(int i) {
  int j,n;

  if (i == firstnote) return(0);
  if (score.solo.note[i].snd_notes.num != score.solo.note[i-1].snd_notes.num)
    return(0);
  n = score.solo.note[i].snd_notes.num;
  for (j=0; j < n; j++) 
    if (score.solo.note[i].snd_notes.snd_nums[j] !=  score.solo.note[i-1].snd_notes.snd_nums[j]) return(0);
  return(1);
}



#ifdef SHORT_NOTE_HAS_REST

#define REST_SPLIT .5


//CF:  for 'verbose' model
static void 
short_prefix(PARALLEL_GRAPH *prefix, int n,GNODE *prev_sing, GNODE *prev_rest) {
  float m,v,dist[MAX_SHORT_PREFIX_LEN+1],tail_m,tail_v,tail_p,p;
  GNODE *node[MAX_SHORT_PREFIX_LEN+1],*self,*attack,*slur,*rest_self;
  /* node[i] means X >= i (the ith node starting at 1) */
  int i,st,minl;

  prefix->singing.head =  prefix->singing.tail = NULL;
  prefix->resting.head =  prefix->resting.tail = NULL;
  attack = slur = NULL;
  if (short_prefix_parms(n, &tail_p, dist) == 0) { printf("short_prefix prob\n"); exit(0); }
  for (i=2; i <= MAX_SHORT_PREFIX_LEN; i++) {
    st = (i == 2) ? FALL_STATE : NOTE_STATE;
    node[i] = get_gnode(st,n,i-1,0,0);
  }
  self = get_gnode(NOTE_STATE,n, MAX_SHORT_PREFIX_LEN,0,0);
  add_arc(self,self,1-tail_p);
  rest_self = get_gnode(REST_STATE,n, MAX_SHORT_PREFIX_LEN,0,0);
  add_arc(rest_self, rest_self,1-tail_p);
  p = 1-dist[1];  /* P(X > 1) */
  if (prev_sing) {
    st = (is_reartic(n)) ? REARTIC_STATE : SLUR_STATE;
    slur =    get_gnode(st /*SLUR_STATE*/  ,n,0,0,0);  
    add_arc(slur,node[2],p);
    add_arc(slur,self,REST_SPLIT*(1-p));
    add_arc(slur,rest_self,(1-REST_SPLIT)*(1-p));
  }
  if (prev_rest) {
    attack =  get_gnode(ATTACK_STATE,n,0,0,1);
    add_arc(attack,node[2],p);
    add_arc(attack,self,REST_SPLIT*(1-p));
    add_arc(attack,rest_self,(1-REST_SPLIT)*(1-p));
  }
  for (i=2; i < MAX_SHORT_PREFIX_LEN; i++)  {  //CF:  link every state to its successor and to the end node (self)
    if (dist[i-1] >= 1) p=0;
    else  p = (1-dist[i])/(1-dist[i-1]);  /* prob of >= i+1 when >= i */
    add_arc(node[i],node[i+1],p);   /* node[i] means note >= i */
    add_arc(node[i],self,REST_SPLIT*(1-p));
    add_arc(node[i],rest_self,(1-REST_SPLIT)*(1-p));
  } 
  add_arc(node[MAX_SHORT_PREFIX_LEN],self,REST_SPLIT);
  add_arc(node[MAX_SHORT_PREFIX_LEN],rest_self,1-REST_SPLIT);
  prefix->singing.head = slur;
  prefix->resting.head = attack;
  prefix->singing.tail = self;
  prefix->resting.tail = rest_self;
}
  
#else    


//CF:  for 'verbose' model
static void short_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {
  float m,v,dist[MAX_SHORT_PREFIX_LEN+1],tail_m,tail_v,tail_p,p;
  GNODE *node[MAX_SHORT_PREFIX_LEN+1],*self,*attack,*slur;
  /* node[i] means X >= i (the ith node starting at 1) */
  int i,st,minl;

  prefix->singing.head =  prefix->singing.tail = NULL;
  prefix->resting.head =  prefix->resting.tail = NULL;
  attack = slur = NULL;
  /*  m = score.solo.note[n].mean;
  v = score.solo.note[n].std;
  v *= v;
  tail_v = v*VAR_TAIL_FRAC;
  tail_p = 2/(1+sqrt(1+4*tail_v)); 
  tail_m = 1/tail_p;
  minl = min_length(m);
  create_cum_dist(dist,m-tail_m,v-tail_v,minl-1);*/

  if (short_prefix_parms(n, &tail_p, dist) == 0) { printf("short_prefix prob\n"); exit(0); }
  for (i=2; i <= MAX_SHORT_PREFIX_LEN; i++) {
    st = (i == 2) ? FALL_STATE : NOTE_STATE;
    node[i] = get_gnode(st,n,i-1,0,0);
  }
  self = get_gnode(NOTE_STATE,n, MAX_SHORT_PREFIX_LEN,0,0);
  add_arc(self,self,1-tail_p);
  p = 1-dist[1];  /* P(X > 1) */
  if (prev_sing) {
    st = (is_reartic(n)) ? REARTIC_STATE : SLUR_STATE;
    slur =    get_gnode(st /*SLUR_STATE*/  ,n,0,0,0);  
    add_arc(slur,node[2],p);
    add_arc(slur,self,1-p);
  }
  if (prev_rest) {
    attack =  get_gnode(ATTACK_STATE,n,0,0,1);
    add_arc(attack,node[2],p);
    add_arc(attack,self,1-p);
  }
  for (i=2; i < MAX_SHORT_PREFIX_LEN; i++)  {  //CF:  link every state to its successor and to the end node (self)
    if (dist[i-1] >= 1) p=0;
    else  p = (1-dist[i])/(1-dist[i-1]);  /* prob of >= i+1 when >= i */
    add_arc(node[i],node[i+1],p);   /* node[i] means note >= i */
    add_arc(node[i],self,1-p);
  } 
  add_arc(node[MAX_SHORT_PREFIX_LEN],self,1.);
  prefix->singing.head = slur;
  prefix->resting.head = attack;
  prefix->singing.tail = self;
}
  
    
#endif







/*#define REARTIC_PREF_LEN 4
#define REARTIC_LEVELS 4





int re_lev[REARTIC_LEVELS] = 
{FALL_STATE, REARTIC_STATE, RISE_STATE, NOTE_STATE};
*/


/*#ifdef POLYPHONIC_INPUT_EXPERIMENT
#define REARTIC_PREF_LEN 5
#define REARTIC_LEVELS 5
int re_lev[REARTIC_LEVELS] = 
  { ATTACK_STATE, ATTACK_STATE, ATTACK_STATE, ATTACK_STATE, ATTACK_STATE }; 
  #endif*/

/*#ifndef POLYPHONIC_INPUT_EXPERIMENT*/
  #define REARTIC_PREF_LEN 3
  #define REARTIC_LEVELS 3
  int re_lev[REARTIC_LEVELS] = 
    { REARTIC_STATE, FALL_STATE,NOTE_STATE};
/*#endif*/


/*   states are connected to the right and down 

   1   1   1   1   1
       2   2   2   2   2
           3   3   3   3   3   */




void reartic_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {
  GNODE *nd[REARTIC_LEVELS][REARTIC_PREF_LEN];
  int i,j,pos,st,diff;
  char name[500];


  num2name(score.solo.note[n].num,name);
  /*printf("reartic of note %d with pitch %s\n",n,name);*/

  prefix->singing.head =  prefix->singing.tail = NULL;
  prefix->resting.head =  prefix->resting.tail = NULL;
  if (prev_sing) {   //CF:  if prev note CAN end in singing then we need to make a singing prefix
    diff = REARTIC_PREF_LEN-REARTIC_LEVELS;
    for (i=0; i <  REARTIC_LEVELS; i++) for (j=i; j <= i+diff; j++) 
      nd[i][j] = get_gnode(re_lev[i],n,j,0,/*i*/0);
/*    for (i=0; i <  REARTIC_LEVELS; i++) for (j=i; j <= i+diff; j++) {
      if (j < i+diff)  add_arc(nd[i][j],nd[i][j+1],1.);
      if (i < REARTIC_LEVELS-1)  add_arc(nd[i][j],nd[i+1][j+1],1.);
    } */
    for (i=0; i <  REARTIC_LEVELS; i++) for (j=i; j <= i+diff; j++) {
      if (j < i+diff && i < REARTIC_LEVELS-1) {
	add_arc(nd[i][j],nd[i][j+1],.5);
	add_arc(nd[i][j],nd[i+1][j+1],.5);
      }
      else if (j < i+diff && i == REARTIC_LEVELS-1) {
	add_arc(nd[i][j],nd[i][j+1],1.);
      }
      else if (j == i+diff && i < REARTIC_LEVELS-1) {
	add_arc(nd[i][j],nd[i+1][j+1],1.);
      }
    }
    prefix->singing.head = nd[0][0];
    prefix->singing.tail = nd[REARTIC_LEVELS-1][REARTIC_PREF_LEN-1];
  }
  if (prev_rest) {   //CF:   if prev note can end in rest we need to make a resting prefix
    nd[0][0] =  get_gnode(ATTACK_STATE,n,0,0,REARTIC_LEVELS); 
    for (i=1; i < REARTIC_PREF_LEN; i++) {
      nd[0][i] =  get_gnode(NOTE_STATE,n,i,0,REARTIC_LEVELS); 
      add_arc(nd[0][i-1],nd[0][i],1.);
    }
    prefix->resting.head =  nd[0][0]; 
    prefix->resting.tail =  nd[0][REARTIC_PREF_LEN-1];
  }
}



#define TRILL_PREFIX_LEN 2

void alt_trill_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {

  int i,st,p1,p2,state,last;
  GNODE *node[TRILL_PREFIX_LEN];


  p1 = score.solo.note[n].num;
  p2 = score.solo.note[n].trill;
  if ((p2-p1) == 1) state = TRILL_HALF_STATE;
  else if ((p2-p1) == 2) state = TRILL_WHOLE_STATE;
  else {
    printf("unknown trill interval in trill_prefix()\n");
    exit(0);
  }


   prefix->singing.head =  prefix->singing.tail = NULL;
   prefix->resting.head =  prefix->resting.tail = NULL;
   if (prev_rest) {
     node[0] = prefix->resting.head =  get_gnode(state,n,0,0,0);
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  {
       prefix->resting.tail = node[i] = get_gnode(state,n,i,0,0);
     }
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);
   }
   if (prev_sing) {
     node[0] = prefix->singing.head =  get_gnode(state,n,0,0,0);
     for (i=1; i <  TRILL_PREFIX_LEN; i++) {
       prefix->singing.tail = node[i] = get_gnode(state,n,i,0,0);
     }
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);
   }
}

static void 
trill_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {

  int i,st,p1,p2,state,last;
  GNODE *node[TRILL_PREFIX_LEN];


   prefix->singing.head =  prefix->singing.tail = NULL;
   prefix->resting.head =  prefix->resting.tail = NULL;
   if (prev_rest) {
     node[0] = prefix->resting.head =  get_gnode(ATTACK_STATE,n,0,0,0);
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  {
       prefix->resting.tail = node[i] = get_gnode(FALL_STATE,n,i,0,0);
     }
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);

     /* this needs to be duplicated as below*/
   }
   if (prev_sing) {
     node[0] = prefix->singing.head =  get_gnode(SLUR_STATE,n,0,0,0);
     for (i=1; i <  TRILL_PREFIX_LEN; i++) {
       prefix->singing.tail = node[i] = get_gnode(FALL_STATE,n,i,0,0);
     }
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);

     node[0] = prefix->singing.alt_head =  get_gnode(SLUR_STATE,n,0,1,1);
     for (i=1; i <  TRILL_PREFIX_LEN; i++) {
       prefix->singing.alt_tail = node[i] = get_gnode(FALL_STATE,n,i,1,1);
     }
     for (i=1; i <  TRILL_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);
   }
}

  #define SLUR_PREFIX_LEN 3

void slur_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {

  int i,st;
  GNODE *node[SLUR_PREFIX_LEN];

   prefix->singing.head =  prefix->singing.tail = NULL;
   prefix->resting.head =  prefix->resting.tail = NULL;
   if (prev_rest) {
     node[0] = prefix->resting.head =  get_gnode(ATTACK_STATE,n,0,0,1);
     for (i=1; i <  SLUR_PREFIX_LEN; i++)  {
       st = (i == 1) ? FALL_STATE : NOTE_STATE; 
       prefix->resting.tail = node[i] = get_gnode(st/* NOTE_STATE*/,n,i,0,0);
     }
     for (i=1; i <  SLUR_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);
   }
   if (prev_sing) {
     node[0] = prefix->singing.head =  get_gnode(SLUR_STATE,n,0,0,0);
     for (i=1; i <  SLUR_PREFIX_LEN; i++) {
        st = (i == 1) ? FALL_STATE : NOTE_STATE; 
       prefix->singing.tail = node[i] = get_gnode(st/*FALL_STATE*/,n,i,0,0);
     }
     for (i=1; i <  SLUR_PREFIX_LEN; i++)  add_arc(node[i-1],node[i],1.);
   }
}



void rest_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {
  prefix->singing.head =  prefix->singing.tail = NULL;
  prefix->resting.head =  prefix->resting.tail = NULL;
  if (prev_rest) 
    prefix->resting.head =  prefix->resting.tail = get_gnode(REST_STATE,n,0,0,1);
  if (prev_sing) 
    prefix->singing.head =  prefix->singing.tail = get_gnode(REST_STATE,n,0,0,1);
}

void null_prefix(prefix,n,prev_sing,prev_rest)
PARALLEL_GRAPH *prefix;
GNODE *prev_sing,*prev_rest;
int n; {
    prefix->resting.head =  prefix->singing.head =  NULL;
}


#define NUM_PREFIXES 6
#define REARTIC_PREFIX 0
#define SLUR_PREFIX 1
#define REST_PREFIX 2
#define SHORT_PREFIX 3
#define TRILL_PREFIX 4
#define NULL_PREFIX 5

//CF:  functions to create subnote models for the start of notes (see also chain_func)  
void (*prefix_func[NUM_PREFIXES])() = {reartic_prefix,slur_prefix,rest_prefix,short_prefix,trill_prefix,null_prefix};
int prefix_length[NUM_PREFIXES] = {REARTIC_PREF_LEN,SLUR_PREFIX_LEN,1,0,TRILL_PREFIX_LEN,0};

//CF:  create chain for a rest given n,p
void rest_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *list[100];

  int i;

  /*  printf(" n = %d p = %f in rest_chain\n",n,p);*/
  for (i=0; i < n; i++) list[i] = get_gnode(REST_STATE,note_num,i+pref_len,0,1);
  for (i=0; i < n; i++) add_arc(list[i],list[i],1-p);
  for (i=1; i < n; i++) add_arc(list[i-1],list[i],p); 
  chain->resting.head =  list[0];
  chain->resting.tail = list[n-1];
  chain->singing.head =  chain->singing.tail = NULL;
}


void null_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {

  chain->resting.head =  chain->resting.tail = NULL;
  chain->singing.head =  chain->singing.tail = NULL;
}



void sing_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *list[100];

  int i;

  for (i=0; i < n; i++) list[i] = get_gnode(NOTE_STATE,note_num,i+pref_len,0,0);
  for (i=0; i < n; i++) add_arc(list[i],list[i],1-p);
  for (i=1; i < n; i++) add_arc(list[i-1],list[i],p); 
  chain->singing.head =  list[0];
  chain->singing.tail = list[n-1];
  chain->resting.head =  chain->resting.tail = NULL;
}

void sing_rest_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *rest[100],*sing[100];

  int i;


  for (i=0; i < n; i++) rest[i] = get_gnode(REST_STATE,note_num,i+pref_len,0,1);
  for (i=0; i < n; i++) sing[i] = get_gnode(NOTE_STATE,note_num,i+pref_len,0,0);
  for (i=0; i < n; i++) add_arc(rest[i],rest[i],1-p);
  for (i=1; i < n; i++) add_arc(rest[i-1],rest[i],p); 

#ifdef GRAPH_EXPERIMENT
  /*  for (i=0; i < n; i++)   add_arc(sing[i],sing[i]  ,(1-p)*PROB_CONT);
  for (i=0; i < n-1; i++) add_arc(sing[i],sing[i+1],p*PROB_CONT); 
  for (i=0; i < n-1; i++) add_arc(sing[i],rest[i+1],p*(1-PROB_CONT)); 
  for (i=0; i < n; i++)   add_arc(sing[i],rest[i]  ,(1-p)*(1-PROB_CONT)); */

  for (i=0; i < n; i++) add_arc(sing[i],sing[i],(1-p)*PROB_CONT);
  for (i=1; i < n; i++) add_arc(sing[i-1],sing[i],p); 
  for (i=0; i < n; i++) add_arc(sing[i],rest[i],(1-p)*(1-PROB_CONT)); 

#else
  for (i=0; i < n; i++) add_arc(sing[i],sing[i],(1-p)*PROB_CONT);
  for (i=1; i < n; i++) add_arc(sing[i-1],sing[i],p); 
  for (i=0; i < n; i++) add_arc(sing[i],rest[i],(1-p)*(1-PROB_CONT)); 
#endif


  //    add_arc(sing[0],sing[n-1],.000001);  // 8-10  
  //    add_arc(rest[0],rest[n-1],.000001);
// this expeiment at least makes it possible to traverse the body of the note in only 1 frame


  chain->singing.head = sing[0];
  chain->singing.tail = sing[n-1];
  chain->resting.head = rest[0];
  chain->resting.tail = rest[n-1];
}


void alt_trill_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *rest[100],*sing[100];
  int i,state,p1,p2;

  p1 = score.solo.note[note_num].num;
  p2 = score.solo.note[note_num].trill;
  if ((p2-p1) == 1) state = TRILL_HALF_STATE;
  else if ((p2-p1) == 2) state = TRILL_WHOLE_STATE;
  else {
    printf("unknown trill interval in trill_chain()\n");
    exit(0);
  }
  for (i=0; i < n; i++) rest[i] = get_gnode(REST_STATE,note_num,i+pref_len,0,1);
  for (i=0; i < n; i++) sing[i] = get_gnode(state,note_num,i+pref_len,0,0);
  for (i=0; i < n; i++) add_arc(rest[i],rest[i],1-p);
  for (i=1; i < n; i++) add_arc(rest[i-1],rest[i],p); 
  for (i=0; i < n; i++) add_arc(sing[i],sing[i],(1-p)*PROB_CONT);
  for (i=1; i < n; i++) add_arc(sing[i-1],sing[i],p); 
  for (i=0; i < n; i++) add_arc(sing[i],rest[i],(1-p)*(1-PROB_CONT)); 
  chain->singing.head = sing[0];
  chain->singing.tail = sing[n-1];
  chain->resting.head = rest[0];
  chain->resting.tail = rest[n-1];
}



void old_trill_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *rest,*lo,*hi,*hi_slur,*lo_slur;


  int i=0;



  rest = get_gnode(REST_STATE,note_num,i+pref_len,0,2);
  lo = get_gnode(NOTE_STATE,note_num,i+pref_len+1,0,0);
  lo_slur = get_gnode(SLUR_STATE,note_num,i+pref_len,0,0);
  hi = get_gnode(NOTE_STATE,note_num,i+pref_len+1,1,1);
  hi_slur = get_gnode(SLUR_STATE,note_num,i+pref_len,1,1);

  add_arc(lo,lo,.5);
  add_arc(lo,rest,.01);
  add_arc(lo,hi_slur,.49);
  add_arc(hi_slur,hi,1.);
  add_arc(rest,rest,1-p);
  add_arc(hi,hi,.5);
  add_arc(hi,lo_slur,.49);
  add_arc(lo_slur,lo,1.);
  add_arc(hi,rest,.01);
  

  chain->singing.head = chain->singing.alt_tail = lo;
  chain->singing.tail = chain->singing.alt_head = hi;  
  chain->resting.head = rest;
  chain->resting.tail = rest;
}


#define TRILL_PROB_SWITCH .01 
/*#define TRILL_PROB_REST .01 */
#define TRILL_PROB_STAY .99

void trill_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *rest,*lo,*hi,*hi_slur,*lo_slur,*lo_loop,*hi_loop,*lo_lead,*hi_lead;
  GNODE *h1l,*h1a,*h2l,*h2a,*h3l,*h3a;
  GNODE *l1l,*l1a,*l2l,*l2a,*l3l,*l3a,*la,*ll,*hl,*ha;
  GNODE *lastll,*lasthl;
  float f;


  int i=0;


    rest = get_gnode(REST_STATE,note_num,pref_len+1,0,2); 

  for (i=0; i < n; i++) {
    ha = get_gnode(SLUR_STATE,note_num,pref_len+0+2*i,1,1);
    hl = get_gnode(NOTE_STATE,note_num,pref_len+1+2*i,1,1);
    la = get_gnode(SLUR_STATE,note_num,pref_len+0+2*i,0,0);
    ll = get_gnode(NOTE_STATE,note_num,pref_len+1+2*i,0,0);
    f = (i == n-1) ? 1-p : 1-p;
    add_arc(ha,hl,1.);
    add_arc(hl,hl,f*TRILL_PROB_STAY);  
    add_arc(hl,la,f*TRILL_PROB_SWITCH);
    add_arc(la,ll,1.);
    add_arc(ll,ll,f*TRILL_PROB_STAY);  
    add_arc(ll,ha,f*TRILL_PROB_SWITCH);
    if (i > 0) {
      add_arc(lastll,ll,p);
      add_arc(lasthl,hl,p);
    }
    else {
      chain->singing.head = la;
      chain->singing.alt_head = ha;
      chain->resting.head = NULL; /*rest;  */
    }
    lastll = ll;
    lasthl = hl;
  }
  chain->singing.tail = hl;  
  chain->singing.alt_tail = ll;
  chain->resting.tail = NULL; /* rest; */
}




void less_old_trill_chain(chain,n,p,note_num,pref_len)
PARALLEL_GRAPH *chain;
int n,note_num,pref_len; 
float p; {
  GNODE *rest,*lo,*hi,*hi_slur,*lo_slur,*lo_loop,*hi_loop,*lo_lead,*hi_lead;
  GNODE *h1l,*h1a,*h2l,*h2a,*h3l,*h3a;
  GNODE *l1l,*l1a,*l2l,*l2a,*l3l,*l3a;


  int i=0;



  rest = get_gnode(REST_STATE,note_num,pref_len+1,0,2);

  /*  h1a = get_gnode(SLUR_STATE,note_num,pref_len+0,1,1); */
  h1l = get_gnode(NOTE_STATE,note_num,pref_len+1,1,1);
  h2a = get_gnode(SLUR_STATE,note_num,pref_len+2,0,0);
  h2l = get_gnode(NOTE_STATE,note_num,pref_len+3,0,0);
  h3a = get_gnode(SLUR_STATE,note_num,pref_len+4,1,1);
  h3l = get_gnode(NOTE_STATE,note_num,pref_len+5,1,1);

  /*  l1a = get_gnode(SLUR_STATE,note_num,pref_len+0,0,0); */
  l1l = get_gnode(NOTE_STATE,note_num,pref_len+1,0,0);
  l2a = get_gnode(SLUR_STATE,note_num,pref_len+2,1,1);
  l2l = get_gnode(NOTE_STATE,note_num,pref_len+3,1,1);
  l3a = get_gnode(SLUR_STATE,note_num,pref_len+4,0,0);
  l3l = get_gnode(NOTE_STATE,note_num,pref_len+5,0,0);

  /*  add_arc(h1a,h1l,1.); */

  add_arc(h1l,h1l,.5);
  add_arc(h1l,h2a,.5);

  add_arc(h2a,h2l,1.);

  add_arc(h2l,h2l,.5);
  add_arc(h2l,h3a,.5);

  add_arc(h3a,h3l,1.);

  add_arc(h3l,h3l,.5);   /* these shouldn't add to one */
  add_arc(h3l,rest,.01);
  add_arc(h3l,l3a,.4);

  /*  add_arc(l1a,l1l,1.);*/

  add_arc(l1l,l1l,.5);
  add_arc(l1l,l2a,.5);

  add_arc(l2a,l2l,1.);

  add_arc(l2l,l2l,.5);
  add_arc(l2l,l3a,.5);

  add_arc(l3a,l3l,1.);

  add_arc(l3l,l3l,.5);   /* these shouldn't add to one */
  add_arc(l3l,rest,.01);
  add_arc(l3l,h3a,.4);

#ifdef OLD

    lo_slur = get_gnode(SLUR_STATE,note_num,pref_len,0,1);
  lo_loop = get_gnode(NOTE_STATE,note_num,pref_len+1,0,1);
  hi_lead = get_gnode(SLUR_STATE,note_num,pref_len+2,1,1);
  hi = get_gnode(NOTE_STATE,note_num,pref_len+3,1,1);

  hi_slur = get_gnode(SLUR_STATE,note_num,pref_len,1,1);
  hi_loop = get_gnode(NOTE_STATE,note_num,pref_len+1,1,1);
  lo_slur = get_gnode(SLUR_STATE,note_num,pref_len,0,1);
  lo_loop = get_gnode(NOTE_STATE,note_num,pref_len+1,0,1);
  hi_lead = get_gnode(SLUR_STATE,note_num,pref_len+2,1,1);
  hi = get_gnode(NOTE_STATE,note_num,pref_len+3,1,1);

  lo_lead = get_gnode(SLUR_STATE,note_num,pref_len+2,0,0);
  lo = get_gnode(NOTE_STATE,note_num,pref_len+3,0,0);
 

  add_arc(lo,lo,.5);
  add_arc(lo,rest,.01);
  add_arc(lo,hi_slur,.4);
  /* these probs can't add to one since we must leave room for connection to next note */
  add_arc(lo_slur,lo,1.);

  add_arc(hi_slur,hi,1.);
  add_arc(rest,rest,1-p);
  add_arc(hi,hi,.5);
  add_arc(hi,lo_slur,.4);
  add_arc(hi,rest,.01);

  

  chain->singing.alt_tail = lo;
  chain->singing.tail = hi;  
  chain->singing.head = lo_slur;
  chain->singing.alt_head = hi_slur;  
  chain->resting.head = rest;
  chain->resting.tail = rest;
#endif
  
  chain->singing.tail = h3l;  
  chain->singing.alt_tail = l3l;
  /*  chain->singing.head = l1a; */
  chain->singing.head = l1l;
  /*  chain->singing.alt_head = h1a;*/
  chain->singing.alt_head = h1l;
  chain->resting.head = rest;
  chain->resting.tail = rest;


}







#define CHAIN_NUM 5
#define SING_CHAIN 0
#define REST_CHAIN 1
#define SING_REST_CHAIN 2
#define NULL_CHAIN 3
#define TRILL_CHAIN 4

//CF:  functions used to create different note body models (see also prefix_func)
void (*chain_func[CHAIN_NUM])() = {sing_chain, rest_chain, sing_rest_chain, null_chain, trill_chain /*xtrilx */};

//CF:  null_chain: no chain, all prefix (for short notes)
//CF:  trill_chain not used
//CF:   rest, sing, sing_rest are long note chains; given n and p

#define NUM_STY 7   /* number of note styles */
#define REST_STY 0
#define NEW_NOTE_STY 1
#define SAME_NOTE_STY 2
#define SHORT_NOTE_STY 3
#define TRILL_STY 4
#define NEW_SING_THROUGH_STY 5
#define SAME_SING_THROUGH_STY 6


typedef struct {
  int prefix;            /* index of the prefix  */
  int chain;             /* index to type of chain */
} STYLE_DESC;
			   

STYLE_DESC note_style[NUM_STY];


//CF:  every style of note has two parts, the beginning (PREFIX) of the node and the body (CHAIN)
init_styles() {
  int i;

  note_style[REST_STY].prefix = REST_PREFIX;             //CF:  rest
  note_style[REST_STY].chain =  REST_CHAIN;

  note_style[NEW_NOTE_STY].prefix = SLUR_PREFIX;         //CF:  a regular attack, new pitch (not rearticulated), long note
  note_style[NEW_NOTE_STY].chain =  SING_REST_CHAIN;

  note_style[SAME_NOTE_STY].prefix = REARTIC_PREFIX;     //CF:  rearticulation
  note_style[SAME_NOTE_STY].chain =  SING_REST_CHAIN;

  note_style[SHORT_NOTE_STY].prefix = SHORT_PREFIX;      //CF:  short note (will use 'verbose' pdf model instead of chain)
  note_style[SHORT_NOTE_STY].chain =  NULL_CHAIN;

  note_style[TRILL_STY].prefix = TRILL_PREFIX/*NULL_PREFIX xtrillx */;     //CF:  not used
  note_style[TRILL_STY].chain =  /*SING_REST_CHAIN*/TRILL_CHAIN;

  note_style[NEW_SING_THROUGH_STY].prefix = SLUR_PREFIX;     //CF:  legato
  note_style[NEW_SING_THROUGH_STY].chain =  SING_CHAIN;

  note_style[SAME_SING_THROUGH_STY].prefix = REARTIC_PREFIX;   //CF:  legato, rearticulation
  note_style[SAME_SING_THROUGH_STY].chain =  SING_CHAIN;

}

//CF:  returns an int which is an index into note_style for the style of this note.      
//CF:  eg. short note, rearticulation, rest.
static int
get_style(int i) {  
/* at present if the note is a short reatriculation  then it get short note model */
  float x,dist[MAX_SHORT_PREFIX_LEN+1];
  int possible_space,j;

  //CF:  ?
  possible_space = ((i < (score.solo.num-1)) && (score.solo.note[i+1].num != RHYTHMIC_RESTNUM)); 
  if (i == firstnote-1 || i == lastnote+1) return(REST_STY);
  //  if (is_a_rest(score.solo.note[i].num)) return(REST_STY); 
  if (is_solo_rest(i)) return(REST_STY); 
  if (score.solo.note[i].trill) return(TRILL_STY);
  if (i == firstnote) return(NEW_NOTE_STY); 
  //CF:  if we can't represent the note in the verbose style (verbose= all nodes link to final node)
  if (short_prefix_parms(i,&x,dist) == 0) {
    /* couldn't fit short note model to  desired mean and variance */
    if (i == lastnote) return(NEW_SING_THROUGH_STY);  //CF:  spec case -- can't have 2 rests in a row at the end of the piece

#ifdef POLYPHONIC_INPUT_EXPERIMENT
  if (is_reartic(i)) {
    if (possible_space)   return(SAME_NOTE_STY);  //CF:  rearticulation
    else return(SAME_SING_THROUGH_STY);
  }
#else
  if (score.solo.note[i-1].num == score.solo.note[i].num && 
      score.solo.note[i-1].trill == 0) {
    //    if (i > 0 && score.solo.note[i-1].num != RHYTHMIC_RESTNUM)   return(SAME_NOTE_STY);
    if (possible_space)   return(SAME_NOTE_STY);  //CF:  rearticulation
    else return(SAME_SING_THROUGH_STY);
  }
#endif
    if (possible_space) return(NEW_NOTE_STY);
    else  return(NEW_SING_THROUGH_STY);  
  }
  return(SHORT_NOTE_STY);  
}

//CF:  compute nn and pp for chain part of long notes
//CF:  input n is the index of the required note within the score
set_parms(n,pref_len,nn,pp) 
int n,pref_len,*nn;
float *pp; {
  float m,v;

#ifdef JAN_BERAN  
	if (tokens2secs(score.solo.note[n].mean) > 5.) { // for long notes make a hair trigger.  gates should help make this work
    m = secs2tokens(5.);
    v = m*m;
	moments2parms(m,v,nn,pp,n);
	return;
	}
#endif
  if (n < firstnote || n > lastnote)  { 
    m = secs2tokens(1.);
    v = m*m;
  }
  else {
    m = score.solo.note[n].mean-pref_len;    //CF:  mean of chain duration fot this note
    if (m < 1) m = 1;               
    v = score.solo.note[n].std;              //CF:  ideal variance, prescribed by score
    v *= v;
  }
  moments2parms(m,v,nn,pp,n);   //CF:  ***
/*   printf("n = %d m = %f v = %f nn = %d pp = %f\n",n,m,v,*nn,*pp);  */

}


static void
connect_prefix_chain(PARALLEL_GRAPH *prefix, PARALLEL_GRAPH *chain) {
/* connect deterministically to chain.singing.head if it exits.  ow to 
chain.resting.head */
  int i,j,count = 0;
  float p;
  GNODE *nd[2][2],*target;

  //CF:  if there is no chain, then there's nothing to do
  if (chain->singing.head == NULL && chain->resting.head == NULL) return;

  //CF:  ? no prefix (not sure?)
  if (prefix->singing.head == NULL && prefix->resting.head == NULL) {
    *prefix = *chain;
    /*    printf("could this be a trill?\n");*/
    return;
  }

  //CF:  main code:

  /* as in null_chain */
  //CF:  chain either has a singing or a resting SUBGRAPH. Which one is this?
  //CF:  We take the head GNODE of the (appropriate SUBGRAPH of the) chain.
  if (chain->singing.head) target = chain->singing.head;
  else if (chain->resting.head) target = chain->resting.head;
  else  {
    printf("error in connect_pref_chain()\n");
    exit(0);
  }
  //CF:  the actual connections
  if (prefix->singing.tail) add_arc(prefix->singing.tail,target,1.);  //CF:  transition with probability 1.
  if (prefix->resting.tail) add_arc(prefix->resting.tail,target,1.);
  if (prefix->singing.alt_tail && chain->singing.alt_head)   //CF:  for trills; not currently used?
    add_arc(prefix->singing.alt_tail,chain->singing.alt_head,1.);
/*  nd[0][0] = prefix.singing.tail;
  nd[0][1] = prefix.resting.tail;
  nd[1][0] = chain.singing.head;
  nd[1][1] = chain.resting.head;
  


  for (i=0; i < 2; i++)  if (nd[1][i]) count++;
  if (count == 0) {
    printf("error in connect_pref_chain()\n");
    exit(0);
  }
  p = 1 / (float) count;
  for (i=0; i < 2; i++) for (j=0; j < 2; j++) 
    if (nd[0][i] && nd[1][j]) add_arc(nd[0][i],nd[1][j],p); */
}

//CF:  connect the end of the chain of one note
//CF:   to either start1 or both (start1 and start2) prefix heads of the next note.
static void
make_connect(GNODE *end, GNODE *start1, GNODE *start2) {
  float p;
  int j;

  if (end == NULL) return;
  if (start1 == NULL) {
    printf("problem in make_connect()\n");
    exit(0);
  }
  p = 0;
  //CF:  sum up the total output transition probability used so far. Check we some left for this new transition.
  for (j=0; j < end->next.num; j++)  
    p += end->next.list[j].prob;
  if (p > .99999) {
    printf("haven't left enough room for connection probability (%f)\n",p);
    exit(0);
  }
  //CF:  if only start1 exists, all the remaining output transition probability goes to start1
  //CF:  if start1 and start2, then it's shared between them. (eg. a trill could start on ether pitch)
  if (start2 != NULL) {
    add_arc(end,start1,(1-p)/2);    
    add_arc(end,start2,(1-p)/2);     
  }
  else  {
    add_arc(end,start1,1-p);    
  }
}


connect_notes(n1,n2)
NOTE_GRAPH n1,n2; {
  GNODE *nd[2][2],*m1,*m2,*m2p;
  int j,i,sum;
  float p,q;

  /*  nd[0][0] = n1.chain.singing.tail;
  nd[0][1] = n1.chain.resting.tail;
  nd[1][0] = n2.prefix.singing.head;
  nd[1][1] = n2.prefix.resting.head;
  for (i=0; i < 2; i++) {
    sum = (nd[0][i] == NULL)  + (nd[1][i] == NULL);
    if (sum == 1) {
      printf("problem in connect_notes()\n");
      exit(0);
    }
    if (sum == 0) {
      p = 0;
      for (j=0; j < nd[0][i]->next.num; j++)  
	p += nd[0][i]->next.list[j].prob;
      if (nd[0][i] == nd[1][i]) {
	printf("yikes in connect_notes\n");
	exit(0); 
      }
      add_arc(nd[0][i],nd[1][i],1-p);    
    }
  }*/

  //CF:  usually, the chain tail of the first note is connected to prefix head of the second note
  //CF:  (details concern the alt_head, not usually used?)
  make_connect(n1.chain.singing.tail,n2.prefix.singing.head,n2.prefix.singing.alt_head);
  make_connect(n1.chain.singing.alt_tail,n2.prefix.singing.head,n2.prefix.singing.alt_head); 
  make_connect(n1.chain.resting.tail,n2.prefix.resting.head,n2.prefix.resting.alt_head);
  make_connect(n1.chain.resting.alt_tail,n2.prefix.resting.head,n2.prefix.resting.alt_head);
  /*  if (n1.chain.singing.tail)
  if (n1.chain.singing.tail->note == 8) {
    printf("hello\n");
    exit(0);
  }*/

  /*  if (n1.chain.singing.tail != NULL && n2.prefix.singing.head != NULL) {
    p = 0;
    m1 = n1.chain.singing.tail;
    m2 = n2.prefix.singing.head;
    m2p = n2.prefix.singing.alt_head;
    if (m2p != NULL) {
      add_arc(m1,m2,(1-p)/2);    
      add_arc(m1,m2p,(1-p)/2);    
    }
    else  add_arc(m1,m2,1-p);    
  }
  if (n1.chain.resting.tail != NULL && n2.prefix.resting.head != NULL) {
    p = 0;
    m1 = n1.chain.resting.tail;
    m2 = n2.prefix.resting.head;
    m2p = n2.prefix.resting.alt_head;
    if (m2p != NULL) {
      add_arc(m1,m2,(1-p)/2);    
      add_arc(m1,m2p,(1-p)/2);    
    }
    else  add_arc(m1,m2,1-p);     
  }*/
}  

connect_to_end(last,end)
PARALLEL_GRAPH last;
GNODE *end; {
  GNODE *nd[2];
  int i,j;
  float p;
  
  nd[0] = last.singing.tail;  //CF:  last note of score could end singing or resting
  nd[1] = last.resting.tail;
  //CF:  look for all remaining outgoing transition probs, and pour them into the end state.
  for (i=0; i < 2; i++) if (nd[i]) {
    p = 0;
    for (j=0; j < nd[i]->next.num; j++)  
      p += nd[i]->next.list[j].prob;
    add_arc(nd[i],end,1-p);    
  }
}
    

#define MAX_ROWS_PER_NOTE 4
#define NODE_RADIUS .65
#define ROW_SCALE 3.
#define COL_SCALE 1.5


static int first_tex_graph_note;
static int last_tex_graph_note;

static void
grid2pos(GNODE *node, float *x, float *y) {
  int row,col;
  

  row = MAX_ROWS_PER_NOTE*(node->note-first_tex_graph_note) + node->row;
  col = node->pos;
  *x = ROW_SCALE*row;
  *y = COL_SCALE*col;
}

static void
tex_arc(FILE *fp, GNODE *orig, GNODE *dest) {
  float sx,sy,dx,dy,dirx,diry,len;

  if (orig == dest) return;
  grid2pos(orig,&sx,&sy);
  grid2pos(dest,&dx,&dy);
  
  dirx = dx-sx;
  diry = dy-sy;
  len = sqrt(dirx*dirx + diry*diry);
  dirx /= len;
  diry /= len;
  sx += dirx*NODE_RADIUS;
  sy += diry*NODE_RADIUS;
  dx -= dirx*NODE_RADIUS;
  dy -= diry*NODE_RADIUS;
  fprintf(fp,"\\move(%3.3f %3.3f)\n",sx,sy);
  fprintf(fp,"\\avec(%3.3f %3.3f)\n",dx,dy);
}

static void
get_node_pos(GNODE *node, float *x, float *y) {
  *x = node->xpos*ROW_SCALE;
  *y = node->ypos*COL_SCALE;
}

static void
new_tex_arc(FILE *fp, GNODE *orig, GNODE *dest) {
  float sx,sy,dx,dy,dirx,diry,len;

  if (orig == dest) return;
  if (dest->xpos == -1) return;
  get_node_pos(orig,&sx,&sy);
  get_node_pos(dest,&dx,&dy);
  /*printf("dest = %d %d\n",dest->xpos,dest->ypos); */

  dirx = dx-sx;
  diry = dy-sy;
  len = sqrt(dirx*dirx + diry*diry);
  if (len == 0.) printf("dx = %f dy = %f dx = %f dy = %f\n",dx,dy,sx,sy);
  dirx /= len;
  diry /= len;
  sx += dirx*NODE_RADIUS;
  sy += diry*NODE_RADIUS;
  dx -= dirx*NODE_RADIUS;
  dy -= diry*NODE_RADIUS;
  fprintf(fp,"\\move(%3.3f %3.3f)\n",sx,sy);
  fprintf(fp,"\\avec(%3.3f %3.3f)\n",dx,dy);
}

static void
dmtg(GNODE *cur, FILE *fp, GNODE **list, int *n) {
  int i,pic_row,pic_col,j;
  char name[500],desc[500];
  float x,y,xx,yy;


  /*  if (cur->row)printf("row = %d\n",cur->row); */
  if (cur->visit) {
    return;
  }

  cur->visit = 1;
  cur->xpos = -1;
  if ((rat_cmp(score.solo.note[cur->note].wholerat,start_pos) >= 0)
      &&  (rat_cmp(score.solo.note[cur->note].wholerat,end_pos) <= 0)) {
    /*  if (cur->note >= first_tex_graph_note && cur->note <= last_tex_graph_note) {*/

    list[(*n)++] = cur;

    /*        num2name(cur->state.arg[THIS_PITCH],name);
    state2name(cur->state.statenum,desc);
    grid2pos(cur,&x,&y);
    fprintf(fp,"\\move(%3.3f %3.3f)\n",x,y);
    fprintf(fp,"\\lcir r:%3.3f\n",NODE_RADIUS);
    fprintf(fp,"\\htext{%s,%s,%d}\n",desc,name,cur->note);
    for (j=0; j < cur->next.num; j++) {
      tex_arc(fp,cur,cur->next.list[j].ptr); 
    }*/
      
  }
for (i=0; i < cur->next.num; i++) dmtg(cur->next.list[i].ptr, fp,list,n);
}

static void
de_underscore(char *s) {
  int i,j;

  for (i=strlen(s); i >= 0; i--)     if (s[i] == '_')  s[i] = '-';
}


static void
draw_tex_node(FILE *fp, GNODE *cur) {
  int i,pic_row,pic_col,j;
  char name[500],desc[500],*nm;
  float x,y,xx,yy;


strcpy(name,score.solo.note[cur->note].observable_tag);
  de_underscore(name);
  /*  num2name(cur->state.arg[THIS_PITCH],name); */
  state2name(cur->state.statenum,desc);
  get_node_pos(cur,&x,&y);
  fprintf(fp,"\\move(%3.3f %3.3f)\n",x,y);
  fprintf(fp,"\\lcir r:%3.3f\n",NODE_RADIUS);
  /*  fprintf(fp,"\\htext{%s,%s,%d,%d,%d}\n",desc,name,cur->note,cur->xpos,cur->ypos);*/
  fprintf(fp,"\\htext{%s}\n",name);
  for (j=0; j < cur->next.num; j++) {
    new_tex_arc(fp,cur,cur->next.list[j].ptr); 
  }
}

static int
graph_comp(const void *i1, const void *i2) {

  GNODE **p1,**p2;

  p1 = (GNODE **) i1;
  p2 = (GNODE **) i2;

  if ((*p1)->note < (*p2)->note) return(-1);
  if ((*p1)->note > (*p2)->note) return(1);
  if ((*p1)->row <  (*p2)->row) return(-1);
  if ((*p1)->row >  (*p2)->row) return(1);
  return(0);
}


#define GRAPH_FILE "graph.tex"

static void    
dp_make_tex_graph() {
  GNODE *cur,*list[1000];
  float start,end;
  int first,last,n=0,i,j;
  FILE *fp;
  char string[500];


  fp = fopen(GRAPH_FILE,"w");
  fprintf(fp,"\\drawdim cm \\setunitscale .75 \\linewd 0.02\n"); 
  fprintf(fp,"\\textref h:C v:C\n"); 
  fprintf(fp,"\\arrowheadtype t:F\n");
  fprintf(fp,"\\arrowheadsize l:.2 w:.2\n");

  printf("enter start time of solo part (in d1+d2/d3 format):  ");
  scanf("%s",string);
  start_pos  = string2wholerat(string);
  printf("enter end time of solo part (in d1+d2/d3 format):  ");
  scanf("%s",string);
  end_pos  = string2wholerat(string);


  /*  printf("enter the start time (in meas) of what you what graphed:\n");
  scanf("%f",&start);
  printf("enter the end time (in meas) of what you what graphed:\n");
  scanf("%f",&end);
  first = 0;
  while (score.solo.note[first].time < start) first++;
  first_tex_graph_note = first;
  last = 0;
  while (score.solo.note[last].time < end) last++;
  last_tex_graph_note = last; */
  cur = start_node;
printf("first_tex_graph_note = %d last_tex_graph_note = %d\n",first_tex_graph_note, last_tex_graph_note);


  dmtg(cur,fp,list,&n);
  printf("n = %d\n",n);
  qsort(list,n,sizeof(GNODE *),graph_comp);
  j = 0;
  for (i=0; i < n; i++) {
    list[i]->xpos = j;
    list[i]->ypos = list[i]->pos;
    if (i < n-1) if (list[i]->row != list[i+1]->row || list[i]->note != list[i+1]->note)
      j++;
  }
  for (i=0; i < n; i++)  draw_tex_node(fp,list[i]);
   fclose(fp);
}

//CF:  we construct a special start_node state, representing the 'rest' before the performer starts playing.
//CF:  This is the self-transition probabiliy for that state:
#define SELF_LOOP .975 //.95  
/* this change 8-12.  having early entrace of orch when pieces starts with long cued solo note.  
   What happens is that the rest part of the long note scores the same as the intial rest, so that there is a slight amount of leakage out of the
   first state.  eventually this accumulates to the point where the original state seems like it has passed. */
//CF:  It's stored in the (firstnote-1)th position in the array of GNODE pointers, buff.
//CF:  (Note that this is accesses exclusively through the anchor pointer, which begins indexing at -1,
//CF:   so the first solo note is anchor[0], and the special state_node is anchor[-1].

//CF:  Main entry to construct a graph for a complete score.
make_my_graph() {
  NOTE_GRAPH buff[MAX_SOLO_NOTES],*anchor;   //CF:  keeps pointers to all nodes that we create
  int i,n,style,pi,ci,pref_len,nn;
  float pp;
  GNODE *prev_sing,*prev_rest,*temp;

  printf("lastnote = %d\n",lastnote);

  memset(buff,0,MAX_SOLO_NOTES*sizeof(NOTE_GRAPH));  /* zero out memory */
  init_styles();  //CF:  set up array of functions for prefixes and bodies of different note models
  anchor = buff+1; //CF:  pointer arithmetic
  start_node = get_gnode(REST_STATE,-1,0,0,0);            //CF:  all pieces begin with a rest.  Constructor.
  add_arc(start_node,start_node,SELF_LOOP);               //CF:  add arc from node to itself.

  anchor[firstnote-1].chain.resting.tail = start_node;
  anchor[firstnote-1].chain.singing.tail = NULL;  //CF:  special start_node does have a tail, its only node!

  //CF:  step through score, create notes
  //CF:  remember we can use different note models for special notes}
 
  for (n=firstnote; n <= lastnote; n++) {
    /*    printf("getting style\n");*/
    style = get_style(n);
     
    
    //    printf("n = %d\tnote  = %s mean = %f std = %f style = %d pitch = %d meas_size = %f\n",n,score.solo.note[n].observable_tag,score.solo.note[n].mean,score.solo.note[n].std,style,score.solo.note[n].num,score.solo.note[n].meas_size);       

			       //			       if (style == TRILL_STY) { printf("trill\n"); exit(0); }
			       
    pi = note_style[style].prefix;  //CF:  each note style has its own prefix and chain constructors (prefix index)
    prev_sing = anchor[n-1].chain.singing.tail;
    prev_rest = anchor[n-1].chain.resting.tail;
    prefix_func[pi](&(anchor[n].prefix),n,prev_sing,prev_rest); //CF:  run the constructor function for this prefix type
    ci = note_style[style].chain;
    pref_len = prefix_length[pi];
    if (style == SHORT_NOTE_STY)  { //CF:  short-note (verbose) style has no chain
      anchor[n].chain.singing.tail = anchor[n].prefix.singing.tail; //CF:  trick so linking works; link will be from chain
      //CF:  so make it look like prefix is the chain.
#ifdef SHORT_NOTE_HAS_REST
      anchor[n].chain.resting.tail = anchor[n].prefix.resting.tail; 
#else
      anchor[n].chain.resting.tail =  NULL; 
#endif
    }
    else {
      //CF:  for long notes:  all prefixes have fixed length
      set_parms(n,pref_len,&nn,&pp);  //CF:  nn is num of gnodes with self-loops; pp is selfloop probability; nn pp returned.
      /*      printf("n = %d nn = %d pp = %f\n",n,nn,pp); */
      chain_func[ci](&(anchor[n].chain),nn,pp,n,pref_len);  //CF:  chain constructor. 
      connect_prefix_chain(&anchor[n].prefix,&anchor[n].chain); //CF:  link up the prefix (head) to the chain (body)
    }
 
#ifdef SPECT_TRAIN_EXPT
    if (anchor[n].chain.singing.tail)   score.solo.note[n].positions =  anchor[n].chain.singing.tail->pos;
    else score.solo.note[n].positions =  anchor[n].chain.resting.tail->pos;
#endif

/*    set_parms(n,pref_len,&nn,&pp);
    chain_func[ci](&(anchor[n].chain),nn,pp,n,pref_len);
    if (style == SHORT_NOTE_STY) 
      anchor[n].chain.singing.tail = anchor[n].prefix.singing.tail;
    else   connect_prefix_chain(anchor[n].prefix,anchor[n].chain); */
    connect_notes(anchor[n-1],anchor[n]);  //CF:  connected this note to previous note
  }

  //  if (is_a_rest(score.solo.note[lastnote].num)) {
  if (is_solo_rest(lastnote)) {
    printf("ending score with a rest.  probably want to aviod this connect to another reststate as end_node\n");
    //    exit(0);
  }
  end_node = get_gnode(REST_STATE,lastnote+1,0,0,0);  //CF:  constructor. Special final node is an absorbing node.
  add_arc(end_node,end_node,1.);
  connect_to_end(anchor[lastnote].chain,end_node); //CF:  spec case for end note

  //CF:  if the first solo note is a rest, then ignore the special start state, 
  //CF:  and set the start_node to pointer to the rest. (because we dont want two rests together)


  if (is_solo_rest(firstnote) && lastnote >= firstnote)   // latter is 0 note case
    start_node =  anchor[firstnote].chain.resting.head;


  /*#ifdef POLYPHONIC_INPUT_EXPERIMENT
    if (score.solo.note[firstnote].snd_notes.num == 0) {
    start_node =  anchor[firstnote].chain.resting.head;
  }
#endif
#ifndef POLYPHONIC_INPUT_EXPERIMENT
  if (is_a_rest(score.solo.note[firstnote].num)) 
    start_node =  anchor[firstnote].chain.resting.head;
    #endif */
}


void    
make_dp_graph() {

  /*  printf("building graph from %s to %s\n",score.solo.note[firstnote].observable_tag,score.solo.note[lastnote].observable_tag);
      exit(0);*/
  init_gnode_buff();
  make_my_graph();
  //  exit(0); 
  /*            test_graph();                    
		exit(0); */

  /*       dp_make_tex_graph();  exit(0);            */
}


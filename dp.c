/*  In its current incarnation this program reads in a "score" file named  SCOREFILE.
   Also read is an audio file with the usual sun audio file format named INFILE.
    INFILE is a rendition of what is in SCOREFILE.  The program uses a dynamic 
    programming algorithm to determine how the score file lines up with the
    audio file.  This is the result of the routine dp().  The result, which is
    a list of start times (in tokens) for the vaious notes, is written to the
    file TIMEFILE.  if TIMEFILE has already been written, readbeat() will read
    in the file and addclick() can be run to create a file named CLICKFILE which
    is a headerless audio file with clicks added every time a new note ebegins.
*/


#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "share.c" 
#include "global.h"
#include "belief.h"
#include "conductor.h"
#include "times.h"
#include "class.h"
#include "audio.h"
#include "midi.h"
#include "timer.h"
#include "linux.h"
#include "new_score.h"
#include "gui.h"
#include "vocoder.h"
#include "wasapi.h"

#define MAXNODESOL 500      /* max nodes at END of dp iter in offline comp */
#define MAXNODESRT 250 //100 /*50      /* max nodes at END of dp iter in real time comp */

#define MAX_ACTIVE_BUFFER /*1000*/   4*MAXNODESOL 

#define TOTAL_NODES 40000 //20000  /* total number of nodes used per parse */
#define MAX_LAG  20  /* crashes if 0 */
#define MISSED_STD_ERROR  (MAX_STD_SECS_DETECT + 10)  // placeholder for std error of note onset when not detected at all



//CF:  list of active hypotheses (ie NODEs) for the current frame
//CF:  (NB. this struct also gets used for the 2D components of 'link' in the mini-backward step)
typedef struct act  {        /* the active list of nodes */  //CF:  active hypotheses for HMM
   NODEPTR el[MAX_ACTIVE_BUFFER];   //CF:  node hypotheses in the list
   int     length;           /* number of active nodes */
} ACTLIST;


typedef struct {
  //  SNAPSHOT buff[MAX_SNAPS];
  SNAPSHOT *buff;
  int count;
} PH_BUFF;


typedef struct {
  /*    NODE node[TOTAL_NODES];
	NODE *stack[TOTAL_NODES];*/
    NODE *node;
    NODE **stack;
    int cur;
} NODE_BUFF;




/* global variables exported to linker */

int   failed;
int   last_event;
int   lasttoken;     /* boolean for (out of data?) */ 
int   token;         /* currently examining the token-th frame */
int   freqs;         /* number of measured frequecies in FFT (half of FFT length */
float *data;         /* contains one token's worth of raw data */
PITCH *sol;          /* solfege array */


/**********************************/








/*************************************************************************/
int first_cue,first_event;

   /* global variables (for this source file) */

static PH_BUFF ph_buff;
static NODE_BUFF node_buff;
static char start_string[100];
static char end_string[100];


static NODEPTR start;         /* pointer to start of terminal list */
static NODEPTR root;         /* root of dp search tree */
static ACTLIST active;        /* the active nodes */ 
static ACTLIST *lnk[MAX_LAG+1]; /* link[i] = p(x(j) | x(j-1)) p(y(j) | x(j))
			     where j = token-i */
static ACTLIST *fwd[MAX_LAG+1]; /* fwd[i] =  p(x(j) | y(0), y(1), ... y(j)
			    where j = token-i */
static ACTLIST *bwd[MAX_LAG+1]; 
static float minrate,maxrate;
static int   maxnodes;
static char start_string[100];
static char end_string[100];

/*********************************************************/
    /* external variales */



extern int   *notetime;       /* notetime[i] is time in tokens the ith note starts */
extern char  scorename[];



/******************************************************************************/

void
init_live_state() {
  live_range.start.num = -1; live_range.start.den = 1;     // -1 means unset
  live_range.end.num = 1; live_range.end.den = 1;
  live_range.firstnote = live_range.lastnote = 0;
  start_pos.num = -1;  start_pos.den = -1;
  firstnote = lastnote = 0;

}


void
store_live_state() {
  live_range.start = start_pos;
  live_range.end = end_pos;
  live_range.firstnote = firstnote;
  live_range.lastnote = lastnote;
}

void
store_take_state() {
  take_range.start = start_pos;
  take_range.end = end_pos;
  take_range.firstnote = firstnote;
  take_range.lastnote = lastnote;
    take_range.frames = frames;
  strcpy(take_range.current_examp,current_examp);
}

void
clear_take_state() {
    take_range.start.num = 0; take_range.start.den = 1;
    take_range.end.num = 0; take_range.end.den = 1;
    take_range.firstnote = 0;
    take_range.lastnote = 0;
    take_range.frames = 0;
    take_range.current_examp[0] = 0;
    strcpy(take_range.current_examp,current_examp);
}


void
restore_live_state() {
  start_pos = live_range.start;
  end_pos = live_range.end;
  firstnote = live_range.firstnote;
  lastnote = live_range.lastnote;
}

void
restore_take_state() {
  start_pos = take_range.start;
  end_pos = take_range.end;
  firstnote = take_range.firstnote;
  lastnote = take_range.lastnote;
    frames = take_range.frames;
    strcpy(current_examp,take_range.current_examp);
}




/**************************************************************************/

void
alloc_node_buff() {
  node_buff.node = (NODE *) malloc(TOTAL_NODES*sizeof(NODE));
  node_buff.stack = (NODE **) malloc(TOTAL_NODES*sizeof(NODE *));
}



   /* conversion routines */


float 
hz2omega(float hz) {
  return(FREQDIM*hz/(float) SR);
}

//CF:  Baum-Welch inference is done using an active hypothesis list called 'active'.
//CF:  (NB hypotheses are called 'NODEs', not to be confused with state graph GNODEs.)
//CF:  We need a free/alloc system to manage this active list.
//CF:  initialised for the recycling (free_node and alloc_node)
//CF:  node_buff is a stack of free positions in the hypothesis list
//CF:  At init, every location is free
//CF:   node_buff is the index struct to tell about where free mem is in active
//CF:    node_buff.stack is the stack of free locations
//CF:             .(see diagram of this!)
static
init_node_stack() {
    int i;

    for (i=0; i < TOTAL_NODES; i++) node_buff.stack[i]  = node_buff.node+i;
    node_buff.cur=0;
}


static NODE
*alloc_node() {
    NODE *n;

    if (node_buff.cur >= TOTAL_NODES) {
	printf("out of nodes in dp.c\n");
	exit(0);
    }
    n = node_buff.stack[node_buff.cur];
    node_buff.cur++;
    return(n);
}

static
free_node(n)
NODE *n; {

    if (node_buff.cur <= 0) {
	printf("problem in free_node()\n");
	exit(0);
    }
    node_buff.cur--;
    node_buff.stack[node_buff.cur] = n;
}

static int
beats2samps(b,t)
float b,t; {
  return(b*60.*SR/t);
}

void
alloc_snapshot_buffer() {
  ph_buff.buff = (SNAPSHOT *) malloc(sizeof(SNAPSHOT)*MAX_SNAPS);
}


/*float beats2tokens(b)
float b; {
  float s;

  
  s = b*60.*SR/score.tempo;
  s /= TOKENLEN;
  return(s);
} */

float 
meas2tokens(m)
float m; {
  float s;

  
  s = m * score.meastime *SR;
  s /= TOKENLEN;
  return(s);
}

float 
tokens2secs(float t) {
  return(t*(float)TOKENLEN/(float)SR);
}

float 
tokens2secs_sync(float t) {
  return(t*(float)TOKENLEN/(float)SR  -   sound_info.samp_zero_secs);
}

float 
secs2tokens(float s) {
  return(s*(float)SR/(float)TOKENLEN);
}



/*************************************************************************/




/*#define EXPINC  1.
#define MAXBUF  1000000
#define MAXIND  1000

*/


static void
addlist(n)
NODEPTR n; {
  
  if (active.length == MAX_ACTIVE_BUFFER) printf("check addlist\n");
  active.el[active.length++] = n;
}



#define BEFORE_START -1
#define AFTER_END -2
#define IN_NOTES -3



static void
init_backward_to_reality() {
  /* ideally the end_node would be countained in the active list after
     the last forward iteration.  If not, the correct implementation
     of the algorithm cannot proceed.  So we initialize the backward
     pass to include all graph locations active at the end of the forward
     pass */
  PROB_HIST *ph;
  int j;
  NODEPTR start;

  ph = prob_hist + frames - 1;
  for (j=0; j < ph->num; j++)  if (ph->list[j].place == end_node) {
    start =  alloc_node();
    start->place = end_node;
    start->prob = 1;
    addlist(start);
    return;
  }
  for (j=0; j < ph->num; j++)  {
    start =  alloc_node();
    start->place = ph->list[j].place;
    start->prob = 1;
    addlist(start);
  }
}


static void
inittree() {
  float tk,rate;
  NODEPTR start;
  SOLO_NOTE *n;
  int i;

  if (mode == BACKWARD_MODE) {
    init_backward_to_reality();
    return;
  }

  root = start =  alloc_node();
  start->prob = 1;  
/*  start->begin = 0;
  start->note  = firstnote;
  start->active  = 1;
  start->mom = NULL;  
  n = &(score.solo.note[firstnote]);
  start->rest =  (score.solo.note[0].num == RESTNUM) ? 1 : 0;  */
  if (mode == BACKWARD_MODE)   start->place = end_node;
  else  start->place = start_node;
  //  printf("start_node = %d\n",start_node); exit(0);

  start->gs.status = BEFORE_START;
  // start->gs.gk = univ_gk(score.meastime,INIT_GAUSS_VAR);
  //  start->gs.tempo = perm_gk(univ_gk(score.meastime,INIT_GAUSS_VAR));
  start->gs.tempo.h = 1;
  start->gs.tempo.m = 0;
  start->gs.tempo.v = 0;
  start->gs.note = -1;
  start->gs.frame = 0;
  start->gs.opt_scale = 1;

  addlist(start);
}



initscore() {
  int i,x,j;
  SOLO_NOTE *n;
  float xx,mean2std();


  for (i=0; i < score.solo.num; i++) {
    n = &(score.solo.note[i]);
    n->mean = meas2tokens(n->length);
    n->std  =  mean2std(n->mean);
/*printf("i = %d mean = %f std = %f\n",i,n->mean,n->std); */
    n->count = 0;
    n->size = score.meastime; 
    n->click = 0;
    //    xx = 8 * n->time/score.measure;
    xx = n->time*2;
    x = xx;
    if (i == 0 || is_a_rest(n->num) == 0) 
      if ( (xx-x) < .001)  n->click = 1;    /* an eigth note division */
  }
  for (i=0; i < score.solo.num; i++) {
    n = &(score.solo.note[i]);
    if (n->click == 1) {
      j = i+1;
      while ( score.solo.note[j].click == 0  && j < score.solo.num) j++;
      if (j >= score.solo.num) break;
      n->clicklen = score.solo.note[j].time - n->time;
      n->clickmean = meas2tokens(n->clicklen);
/*      printf("note = %d clicklen = %f clickmean = %f\n",i,n->clicklen,n->clickmean);*/
    }
  }  
  for (i=0; i < score.midi.num; i++) score.midi.burst[i].size = score.meastime;
}




static void
dpinit() {
  int i;

    if (mode == BACKWARD_MODE)  token = frames;
    else        token = -1;   /* alittle kludgy */
    init_node_stack();
    active.length = 0;
    lasttoken = 0;
    failed = 0;
    inittree(); 
    /*    readaudio();; */
  }




static int
place_comp(const void *p1, const void *p2) {
  int c;
  NODEPTR *i,*j;

  i = (NODEPTR *) p1;
  j = (NODEPTR *) p2;
  c =  (int) ((*i)->place - (*j)->place);
  if (c) return(c);
  if ( (*i)->prob > (*j)->prob) return(-1);
  else if ( (*i)->prob < (*j)->prob) return(1);
  else return(0);
}


static void
dellist(i)  /*deletes element i from active list */
int i; {
  active.el[i]->active = 0;
  free_node(active.el[i]);
  active.el[i] = active.el[--active.length];
}


static void
collect() {
  int i;

  qsort(active.el,active.length,sizeof(NODEPTR),place_comp);
  for (i=active.length-1; i >= 1; i--) 
  if (active.el[i]->place == active.el[i-1]->place) {
    active.el[i-1]->prob += active.el[i]->prob;
    dellist(i); 
  }
}


static void
parse_branch() {
  NODEPTR cur=start,new;
  int i,ind,j;
  ARC_LIST next;

  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    if (cur->place != end_node) {
      next = cur->place->next;
      for (j=0; j < next.num; j++) {
        new = alloc_node();
        addlist(new);
        new->place = next.list[j].ptr; 
        new->prob = cur->prob   * next.list[j].prob;  
        new->mom = cur;
      }
    } 
    dellist(i);
  }
  qsort(active.el,active.length,sizeof(NODEPTR),place_comp);
  for (i=active.length-1; i >= 1; i--) 
    if (active.el[i]->place == active.el[i-1]->place)     
      dellist(i); 
}

static void
old_forward_branch() {
  NODEPTR cur=start,new;
  int i,ind,j;
  ARC_LIST next;

  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
/*if (token == 147 && cur->place->pos == 8) {
  printf("collect\n"); 
}*/
    if (1 /*cur->place != end_node*/) {
      next = cur->place->next;
      for (j=0; j < next.num; j++) {
        new = alloc_node();
        addlist(new);
        new->place = next.list[j].ptr; 
        new->prob = cur->prob * next.list[j].prob; 
	//	printf("next.list[%d].prob = %f\n",j,next.list[j].prob); 
      }
    } 
    dellist(i);
  }
  collect();
}





static void
forward_branch() {
  NODEPTR cur=start,new;
  int i,ind,j,n,setnote=-1;
  ARC_LIST next;
  GNODE *gn;

//CF:  clever backwards loop over hypotheses, ensures that hyps from the previous frame get erased
//CF:  leaving only hyps for the current frame.

  //  for (n = firstnote; n <= lastnote; n++) 
  //    if (score.solo.note[n].set_by_hand && token == score.solo.note[n].frames)   setnote = n;


  for (i=active.length-1; i >= 0; i--) {   //CF:  loop through active hypotheses
    cur = active.el[i];   //CF:  current hypothesis
    next = cur->place->next; //CF:  next is list of child nodes (ie possible future states, inc self sometimes)
    for (j=0; j < next.num; j++) {
      gn = next.list[j].ptr;
      n = gn->note;
      /*if (n >= firstnote && n <= lastnote // enforcing hand-set constraints 
	  && (gn->pos == 0)
	  && score.solo.note[n].set_by_hand
	  && (token == score.solo.note[n].frames)) {
	//	printf("fixing note %d to frame %d\n",n,token);
	}*/
      if (n >= firstnote && n <= lastnote  // enforcing hand-set constraints 
	  && (gn->pos == 0)
	  && score.solo.note[n].set_by_hand
	  && (token != score.solo.note[n].frames)) {
	//printf("killing onset for note %d at token %d\n",n,token);
	continue;
      } 
      //      if ((setnote != -1) && ((n != setnote) ||  (gn->pos != 0))) continue;
      new = alloc_node();
      addlist(new);
      new->place = gn;
      //	printf("note is %d\n",new->place->note);
      new->prob = cur->prob * next.list[j].prob;   //CF:  transition *********
      //	printf("next.list[%d].prob = %f\n",j,next.list[j].prob); 
    }
    dellist(i);
  }
  collect();
}









static void
forward_solo_gates() {
  NODEPTR cur=start;
  float total;
  int i,n,g,r,fr;

  //  for (g=0; g < score.midi.num; g++) printf("%d %f\n",score.midi.burst[g].actual_secs); 
  //  exit(0);
  
  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    n = cur->place->note;
    if (n < firstnote || n > lastnote) continue;
    g = score.solo.note[n].gate;
    if (g == -1) continue;  /* usually this will happen */

    fr = (int) secs2tokens(score.midi.burst[g].actual_secs); 
    /* if note n is set_by_hand at a time *before* fr,
       don't take action --- this doesn't seem to fully
       solve the problem since the notes following the
       one set by hand are also delayed until gate  */
    if (score.solo.note[n].set_by_hand)      continue;

    if (token < fr) {
      dellist(i);
      //      printf("not allowing %s to be placed before midi note %s which was placed at %d (token = %d) \n",score.solo.note[n].observable_tag, score.midi.burst[g].observable_tag,fr,token);
    }
  }	
} 






static void
backward_branch() {
  NODEPTR cur,new;
  int i,ind,j;
  ARC_LIST prev;

  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    if (1 /*cur->place != start_node*/) {
      prev = cur->place->prev;
      for (j=0; j < prev.num; j++) {
        new = alloc_node();
        addlist(new);
        new->place = prev.list[j].ptr; 
        new->prob = cur->prob * prev.list[j].prob; 
      }
    } 
    dellist(i);
  }
  collect();
}




static void
old_graph_comp_score() {  
  NODEPTR cur=start;
  int s,i,st,n,l;
  float tree_like(),class_like(),x,y,z;
  FULL_STATE fs;

  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    st = cur->place->state.statenum;
    n = cur->place->state.arg[0]; 
    s = state2index(st,n,fs);
/* printf("st = %d n = %d\n",st,n); */
/* if (mode == BACKWARD_MODE) printf("n = %d st = %d s = %d rest = %d note = %d\n",n,st,s,REST_STATE,NOTE_STATE); */
/*if ((x=cur->prob) == 0) 
printf("help\n"); */
    cur->prob *=  class_like(s);
/*if (cur->prob == 0) { 
printf("real help\n");
y = class_like(s);
printf("x = %f y = %f z = %f\n",x,y,z);
}*/




/*    st = cur->place->type;
    n = cur->place->pitch; 
    l = cur->place->last_pitch;  */
/*    if (mode == FORWARD_MODE || mode == BACKWARD_MODE)
     compute_stats(cur->place->state);
    cur->prob *=  tree_like(cur->place->state); */
  
  }
}



  


static void
graph_comp_score() {  
  NODEPTR cur=start;
  int na,j,s,i,st,n,l,shade,mp,lp;
  float tree_like(),sa_class_like(),x,y,z,last_time,*acc_spect,x1,x2;
  FULL_STATE fs;
  SOUND_NUMBERS *acc_nts;
  SA_STATE sas;

  //  if (token >= 840 && token <= 870) return;
  
  acc_nts = sounding_accomp(token);
  last_time = time_to_last_accomp(token);
  acc_spect = frame2spect(token);

  //    printf("last_time = %f\n",last_time);
  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    shade = cur->place->state.statenum; 
    mp = cur->place->state.arg[0];  
    lp = cur->place->state.arg[1];  
    //    for (j=0; j < freqs; j++) printf("%d %f\n",j,acc_spect[j]);
    //    printf("index is %d\n",cur->place->note);
    sas = set_sa_state(acc_spect , cur->place->note,shade,mp,lp,last_time,acc_nts,cur->place->pos);




    //s = state2index(st,n,fs);  // call sounding_notes(token)
    
    cur->prob *=  sa_class_like(sas);

    
    //    if (token > 720  && token < 780)    printf("note = %s shade = %d prob = %f\n",score.solo.note[cur->place->note].observable_tag,shade,sa_class_like(sas));

  }
  /*    sas.index = 0;
    x1= sa_class_like(sas);
    sas.index = 1;
    x2= sa_class_like(sas);
    printf("token = %d rat = %f\n",token,x2/x1);*/
    
}




static int
prcut(cur,p)
NODEPTR cur;
float p; {
  return (cur->prob < p);
}









static void
checkdone() {
  if (mode == SYNTH_MODE) {
    if (active.el[0]->place == end_node) lasttoken = 1;
    return;
  }
  if (mode == BACKWARD_MODE) {
    if (token == 0) lasttoken = 1;
    return;
  }
  if (mode == FORWARD_MODE) {
    if (token == frames-1) lasttoken = 1;
    return;
  }
  if (mode == PARSE_MODE || mode == SIMULATE_MODE) {
    if (token == frames-1) lasttoken = 1;
    return;
  }
  else {
    if (token == frames-1) lasttoken = 1;
    return;
  }
}




static int  
probcomp(const void *p1, const void *p2)  { /* compare two nodes by probability */
  int a,b;
  NODEPTR *n1,*n2;

  n1 = (NODEPTR *) p1;
  n2 = (NODEPTR *) p2;
  if ( (*n1)->prob > (*n2)->prob) return(-1);
  else if ( (*n1)->prob < (*n2)->prob) return(1);
  else return(0);
}


#define ALN2I 1.442695022
#define TINY 1.0e-5

static void 
shell_pr(n,arr)    /* sorts by probcomp.  modified  from numerical recipes  */
NODEPTR arr[];
int n;
{
  int nn,m,j,i,lognb2;
  NODEPTR t;

  lognb2=(log((double) n)*ALN2I+TINY);
  m=n;
  for (nn=1;nn<=lognb2;nn++) {
    m >>= 1;
    for (j=m;j<n;j++) {
      i=j-m;
      t=arr[j];
      while (i >= 0 && probcomp(arr+i, &t) == 1) {
        arr[i+m]=arr[i];
        i -= m;
      }
      arr[i+m]=t;
    }
  }
}


#undef ALN2I
#undef TINY



#define TEENY .000001

#define MIN_NODES  50/*20*/  /*100*/     /*0 /*10*/
// making this as large as 1000 can help parse excepts that completely fail otherwise.  



static void
graph_prune() { /* both dp cutoffs and low probability cutoffs */
  int i;


  // printf("nodes = %d max = %d\n",active.length,maxnodes); 
  qsort(active.el,active.length,sizeof(NODEPTR),probcomp); 
  for (i=active.length-1; i >= maxnodes; i--) dellist(i); 
  
  //  printf("have %d nodes before\n",active.length);
  while (active.el[active.length-1]->prob == 0)
    dellist(active.length-1);   /* zero probs cause problem in weed out */
 
  //  printf("have %d notes after getting rid of 0 probs\n",active.length);
  if (active.length > 1)
    while (active.el[active.length-1]->prob < TEENY && active.length > MIN_NODES) 
      dellist(active.length-1);   
  if (active.length == 0) printf("trouble in atlanta at frame %d\n",token);
  // printf("nodes = %d\n",active.length); 
}


static void
print_pair_list(list)
ACTLIST *list; {
  int i,shade;
  NODEPTR cur;
  char s[10];

    printf("token = %d length = %d\n",token,list->length);    
    for (i=0; i < list->length; i++) {
      cur = list->el[i];
      sndnums2string(score.solo.note[cur->place->note].snd_notes,s);
      shade = cur->place->state.statenum; 
		     //      num2name(score.solo.note[cur->place->note].num,s); 
      printf("%d %d %d (%d,%d) -> (%d,%d) (%s) shade = %d %f\n",
      i,
      list->el[i]->orig,
      list->el[i]->place,
      list->el[i]->orig->note,
      list->el[i]->orig->pos,
      list->el[i]->place->note,
      list->el[i]->place->pos,
      s,	     
      shade,	     
      list->el[i]->prob);
  }
  printf("\n"); 
}




static void
add_list(list,new)
ACTLIST *list;
NODE *new; {
  if (list->length == MAX_ACTIVE_BUFFER) { 
    printf("list full\n"); 
    print_pair_list(list);
    exit(0); }
  list->el[list->length] = new;
  list->length++; 
}


static void
del_list(list,i) 
ACTLIST *list;
int i; {

  free_node(list->el[i]);
  list->el[i] = list->el[--(list->length)];
}


static void
free_list(list)
ACTLIST *list; {
  int i; 

  while (list->length) del_list(list,0);
}






static void
graph_prune_list(list)  /* both dp cutoffs and low probability cutoffs */
ACTLIST *list; {
  int i,l;

  qsort(list->el,list->length,sizeof(NODEPTR),probcomp); 
  for (i=list->length-1; i >= maxnodes; i--) del_list(list,i);  
    if (list->length > 1)
    while (list->el[list->length-1]->prob < TEENY && list->length > MIN_NODES) 
    del_list(list,list->length-1);   
if (list->length == 0)
printf("trouble in atlanta\n");
}


static void
f2string(float f, char *s) {
  if (f == 0.) strcpy(s,"underflow");
  else sprintf(s,"%10.9f",f);
}


static void
graph_print_tree() {
  NODEPTR cur=start,par;
  int i,j,shade;
  char s[20],type[100],fl[500];


  printf("\n token = %d nodes = %d\n",token,active.length);
  for (i=0; i < active.length; i++)  {
    j = 0;
    cur = active.el[i];
    shade = cur->place->state.statenum; 
    if (shade == REST_STATE) strcpy(type,"rest");
    if (shade == NOTE_STATE) strcpy(type,"note");
    if (shade == REARTIC_STATE) strcpy(type,"reartic");
    if (shade == ATTACK_STATE) strcpy(type,"attack");
    if (shade == SLUR_STATE) strcpy(type,"slur");
    if (shade == RISE_STATE) strcpy(type,"rise");
    if (shade == FALL_STATE) strcpy(type,"fall");
    num2name(score.solo.note[cur->place->note].num,s); 
    f2string(cur->prob,fl);
    printf("%s\t%d %d %d %s %10.9f %e\n",score.solo.note[cur->place->note].observable_tag,cur->place,cur->place->note,cur->place->pos,type,cur->prob,cur->prob);
  }
}

static void
unity_scale() {  /* scale to sum to 1 */
  NODEPTR cur=start;
  float total;
  int i;

  total = 0;
  for (i=0; i < active.length; i++)  {
    cur = active.el[i];
    total += cur->prob;
  }
  if (total <= 0) {
    printf("token = %d\n",token);
    printf("total unnormalized prob = %f active = %d\n",
	   total,active.length);
    failed = 1;
  }
  for (i=0; i < active.length; i++)  {
    cur = active.el[i];
    cur->prob /= total;
  }
} 
  


#define HIST_BINS 21






static void
lag_stats() {
  int j,i,hist[HIST_BINS],ehist[HIST_BINS],chist[HIST_BINS],d,missed_notes=0,missed_rests=0,treating_as_observed,low_prob_detect=0;
  float late,diff,bin_width = .02,detect_early;
  char name[500];

  for (i=0; i < HIST_BINS; i++) chist[i] = ehist[i] = hist[i]=0;
  for (i=firstnote;  i <= lastnote; i++) {
    if (is_solo_rest(i)) continue;

    //    treating_as_observed = ((score.solo.note[i].std_err <  MAX_STD_SECS_DETECT)  || score.solo.note[i].cue);
    treating_as_observed = score.solo.note[i].treating_as_observed;

    //      if (score.solo.note[i].on_line_secs == 0) {
    if (treating_as_observed == 0) {
      if (score.solo.note[i].num == RESTNUM) missed_rests++;
      else missed_notes++;
      if (score.solo.note[i].lag == MAX_LAG) low_prob_detect++;
      continue;  /* inaccurate detect */
    }
    diff = score.solo.note[i].on_line_secs - score.solo.note[i].off_line_secs; 
    //    late = score.solo.note[i].detect_time - score.solo.note[i].off_line_secs; 
    late = score.solo.note[i].detect_time - score.solo.note[i].off_line_secs; 
    detect_early = score.solo.note[i].off_line_secs -  tokens2secs((float)score.solo.note[i].detect_frame); 
     num2name(score.solo.note[i].num,name);
    d = (int) (fabs(diff)/bin_width);
    if (d > 10/*5*/) {
      printf("%s\toff_line-on_line (late) = %f\tdetected late %f\n",
	     	     score.solo.note[i].observable_tag,diff,late);
      printf("online = %d offline = %d detect = %d\n",(int) secs2tokens(score.solo.note[i].on_line_secs), (int) secs2tokens(score.solo.note[i].off_line_secs), score.solo.note[i].detect_frame); 
      //      printf("on = %f off = %f\n",score.solo.note[i].on_line_secs, score.solo.note[i].off_line_secs); 
    }
    /*    if (d >= HIST_BINS) j++;
	  else hist[d]++;*/
    if (d >= HIST_BINS) d = HIST_BINS-1;
    hist[d]++;
    for (j=0; j <=d; j++) chist[j]++;
    //    if (score.solo.note[i].lag == MAX_LAG) ehist[d]++;
    if (detect_early > 0) ehist[d]++;
  }
  printf("histogram of | off_line - on_line | and early detections (units =  %f secs)\n",bin_width);
  printf("decreasing MAX_STD_SECS_DETECT  will cause more notes to be undetected and will improve histogram\n");


  printf("error in %3.2f secs\tnum notes\tcum notes\tnum early detections\n",bin_width);
  for (i=0; i < HIST_BINS; i++) printf("%d\t%d\t%d\t%d\n",i,hist[i],chist[i],ehist[i]);
  /*  printf("%d %d\n",HIST_BINS,j);*/
  printf("\n\n%d missed notes, %d too low post prob to detect, and %d missed rests out of %d notes\n",missed_notes,low_prob_detect,missed_rests,lastnote-firstnote+1);
}





static int missed_index[MAX_SOLO_NOTES];

static int
std_err_cmp(const void *p1, const void *p2)   {/* compare two nodes by probability */
  SOLO_NOTE *n1,*n2;
  int *i1, *i2;

  i1 = (int *) p1;
  i2 = (int *) p2;
  n1 = (SOLO_NOTE *) score.solo.note + (*i1);
  n2 = (SOLO_NOTE *) score.solo.note + (*i2);

  if ( n1->std_err > n2->std_err) return(-1);
  if ( n1->std_err < n2->std_err) return(1);
  else return(0);
}




#define MISSED_AVERAGE 20

static void
missed_note_thresh_stats() {
  int i,num = 0,n,j,k,l,fe;

  float on_line_err,sum=0;
  char tag[500];

  for (i=firstnote;  i <= lastnote; i++) missed_index[num++] = i;
  qsort(missed_index,num,sizeof(int),std_err_cmp); 
  printf("sorted list of on line errors\n");
  /*  for (i=0; i < num; i++) {
    if (fabs(score.solo.note[j].std_err- MISSED_STD_ERROR) > .01) break;  // don't know why I can't just test equality
    }*/
  for (i=0; i < num; i++) {
    j = missed_index[i];
    on_line_err = score.solo.note[j].on_line_secs-score.solo.note[j].off_line_secs;
    fe = fabs(on_line_err / (256/8000.)) + .5;
    printf("std_err = %f\test error = %f\tave est error = %f\n",score.solo.note[j].std_err,on_line_err,sum/MISSED_AVERAGE);
    //    if (fabs(on_line_err) > .2) printf("std_err = %f\test error = %f\n",score.solo.note[j].std_err,on_line_err);
    //    for (k=0; k < fe; k++)  printf(" ");
    //printf("std_err = %f\test error = %f\n",score.solo.note[j].std_err,on_line_err);
  }
}


#define REALIZE_FRAME_LAG  0 /*4 /* how many frames to wait until we consider a note realized */
#define CUE_FRAME_LAG 2      /* how many frames to wait until cue is given */






static void
print_resource_usage() {
  printf("snapshot buffer utilized %f percent\n",100*ph_buff.count/(float)MAX_SNAPS);
  if (frames)   printf("average snapshots per frame = %f\n",ph_buff.count/(float)frames);
}


static void
state_probs() {
  int i,n=0;
  NODEPTR cur; 
  float prob;


  if (token == 0)  ph_buff.count = 0;
  prob_hist[token].list = ph_buff.buff + ph_buff.count;
  ph_buff.count += active.length;
  if (ph_buff.count > MAX_SNAPS) {
    printf("out of room in state_probs()\n");
    exit(0);
  }

  prob_hist[token].num = active.length;
  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    prob_hist[token].list[n].place = cur->place;
    prob_hist[token].list[n++].prob = cur->prob;
  } 
  return;

   



  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    if (/*cur->prob > TEENY && */ n < HIST_LIM)  {
      prob_hist[token].list[n].place = cur->place;
      prob_hist[token].list[n++].prob = cur->prob;
    }
  }  
    
  prob_hist[token].num = n;
  if (n == HIST_LIM) {
   printf("problem in state_probs()\n");
  for (i=0; i < prob_hist[token].num; i++) 
  printf("%d %f\n", prob_hist[token].list[i].place, prob_hist[token].list[i].prob);
  printf("\n");  
 }
}


static int  
ppcomp(const void *p1, const void *p2)   {/* compare two nodes by probability */
  SNAPSHOT *n1,*n2;

  n1 = (SNAPSHOT *) p1;
  n2 = (SNAPSHOT *) p2;

  if ( n1->prob > n2->prob) return(-1);
  else if ( n1->prob < n2->prob) return(1);
  else return(0);
}



static void
weed_out() {
  int i,j;
  NODEPTR cur; 
  PROB_HIST *ph;

  if (token < 0) { printf("weird thing in weed_out\n"); exit(0); }
   /* kill paths not matching forward pass  recompute probs */

/*if (token > 1070 && token < 1499) graph_print_tree();                 */

  ph = prob_hist + token;
/*printf("active.length = %d ph.num = %d\n",active.length,ph->num); */
  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    for (j=0; j < ph->num; j++)  {
      if (cur->place == ph->list[j].place) break;
/*printf("%d\n",ph->list[j].place); */
    }
    if (j == ph->num)  dellist(i);
/* cur->prob = -1; */
  }
/*  qsort(active.el,active.length,sizeof(NODEPTR),probcomp); 
  for (i=0; i < active.length; i++) if (active.el[i]->prob == -1.) break;
  active.length=i; */
/*  for (i=active.length-1; i >= 0; i--) if (active.el[i]->prob == -1.) dellist(i);*/
  if (active.length == 0) {
    failed = 1; 
    printf("alpha and beta hypotheses do not intersect in frame %d\n",token);
  }
  unity_scale();
}


static void
back_state_probs() {
  int j,i,n=0;
  NODEPTR cur; 
  PROB_HIST *ph;
  float prob=0;
  char s[500];

/*  for (i=0; i < active.length; i++) printf("%d %f\n",active.el[i]->place,active.el[i]->prob);
printf("\n"); */

  ph = prob_hist + token;

/*printf("token = %d\n",token);
  for (i=0; i < active.length; i++) {
     num2name(score.solo.note[active.el[i]->place->note].num,s);
     printf("%d %d %s %d %f\n",
     active.el[i]->place,active.el[i]->place->note,s,active.el[i]->place->pos,active.el[i]->prob);
   }
  printf("\n");
  for (j=0; j < ph->num; j++) {
    num2name(score.solo.note[ph->list[j].place->note].num,s);
    printf("%d %d %s %d %f\n",
  ph->list[j].place,ph->list[j].place->note,s,ph->list[j].place->pos,ph->list[j].prob);
  }
  printf("\n"); */

  for (j=0; j < ph->num; j++)  {
    for (i=0; i < active.length; i++) {
      cur = active.el[i];
      if (cur->place == ph->list[j].place) break;
    }
    ph->list[j].prob *= (i < active.length) ? cur->prob : 0;
  }
  qsort(ph->list,ph->num,sizeof(SNAPSHOT),ppcomp); 
  while (ph->list[ph->num-1].prob == 0) {
    (ph->num)--;
    if (ph->num == 0) { 
printf("no match\n"); return; }
  }
  for (i=0; i < ph->num; i++) prob += ph->list[i].prob;
  for (i=0; i < ph->num; i++) ph->list[i].prob /= prob;
/*  for (i=0; i < ph->num; i++) 
/*if (token > 491 && token < 499) printf("%d %f\n",ph->list[i].place,ph->list[i].prob); */
}



static int
rel_note(node,note)  /* ret of 
			+1 = node past note
			0  = node = start of note
			-1 = node before note */
GNODE *node;
int note; {
  int n,pos;

  n = node->note;
  pos = node->pos;
  if (n > note) return(1);
  if (n < note) return(-1);
  if (pos == 0) return(0);
  return(1);
}
  
static int
all_past_note(ph,note)
PROB_HIST *ph;
int note; {
  int pos,n,k;
  GNODE *nd;

  for (k=0; k < ph->num; k++) {
    nd = ph->list[k].place,note;
    if (rel_note(nd,note) <= 0) return(0);
  }
  return(1);
}




int
is_parse_okay(int *pos) {
  int i,start;

  for (i=firstnote; i <= lastnote; i++) printf("%d %s %d\n",i,score.solo.note[i].observable_tag,score.solo.note[i].frames);
  //  start =(firstnote==0) ? firstnote+1 : firstnote; 
  start = firstnote+1;
  for (i= start; i <= lastnote; i++) if (score.solo.note[i].frames <= score.solo.note[i-1].frames) {
    *pos = i;
    return(0);
  }
  return(1);
}




#define WINDOW 1  /* if WINDOW = 0 then prob contains the posterior prob of the
		     note beginning at the given token */

//CF:  Assumes forward and bwd passes have been done in the HMM.
//CF:  This does the segmentation.
//CF:  Rather than posterior modes or viterbi, the segmenation is chosen to minise the 
//CF:  number of 'parse errors' which are musical NOTES that are misclassified.
//CF:  (Recall that one note has many score states.)
//CF:  So this form of segmentation is specific to the music task.
//CF:  Rougly,                                               *
//CF:    for each note                           ***         *
//CF:    look at the pdf of its onset time:  ___*****________**_________
//CF:  
//CF:    consider moving a (5 frames?) window over it.  Find the best window center to max the mass inside the window.
//CF:    This is like waht human musicians do.  Theres difference between being out by a fraction of a sec and by a bar.
static int
mean_parse() {
  int i,j,n,pos,note,k,start;
  PROB_HIST *ph;
  GNODE *pi,*pj,*nd;
  float sum,*mean,*opt,*vector(),p,prob; 
  char name[500],*nm;


  notetime  = (int*) malloc(score.solo.num*sizeof(int));
  opt  = vector(firstnote-1,lastnote+1);
  //  for (i=firstnote; i <= lastnote; i++) notetime[i] = opt[i] = -1; // always set note at least once0;
  for (i=firstnote; i <= lastnote; i++) notetime[i] = opt[i] = 0; // never set if all 0.
  for (note=firstnote; note <= lastnote; note++) {
    num2name(score.solo.note[note].num,name);
    nm = score.solo.note[note].observable_tag;
    //    start = (note == 0) ? 0 : notetime[note-1]+1;
    start = (note == firstnote) ? 0 : notetime[note-1]+1;
    //      printf("notetime[%d] = %d start = %d\n",note-1,notetime[note-1],start); exit(0);
    //    for (token =notetime[/*i*/ note-1]+1; token < frames-WINDOW; token++) { 
    for (token =start+WINDOW; token < frames-WINDOW; token++) { 
      prob = 0;
      for (j=token-WINDOW; j <= token+WINDOW; j++) {
        ph = prob_hist + j;
        for (k=0; k < ph->num; k++) {
          nd = ph->list[k].place;
	  if (rel_note(nd,note) == 0)
	    prob += ph->list[k].prob;
        }
      }
      //                        if (note == 10 && token > 600 && token < 700) printf("%d %f\n",token,prob);      
      /*      if (strcmp("solo_100+9/8",score.solo.note[note].observable_tag) == 0 && token >= 0 && token < 10000) {
	printf("%d %f\n",token,prob);      
	if (prob > .0001) printf("the note = %s token = %d prob = %f\n",nm,token,prob);
	}*/
      if (prob > opt[note]) { notetime[note] = token; opt[note] = prob;  /*printf("note %d = %d\n",note,token);*/ }
      if (all_past_note(ph,note)) break;
    }
  }

  //  for (i=firstnote+1; i <= lastnote; i++)  if (notetime[i] == 0)  { printf("no estimate of %s\n",score.solo.note[i].observable_tag); return(0); }
  for (i=firstnote; i <= lastnote; i++) {
    if (score.solo.note[i].set_by_hand) continue;  
    /* this case would be correct anyway but since there will be several adjacent frames that get
       prob 1, one is chosen arbitrarily */
    score.solo.note[i].frames = notetime[i];  
    //    printf("got %d at %d\n",i,notetime[i]);
    score.solo.note[i].realize = notetime[i];
    score.solo.note[i].off_line_secs = toks2secs(notetime[i]);
  }
  return(1);
  //  writebeat();
}


static void
set_left_rite_edges(int *left_edge, int *rite_edge) {
  int j,k,n;
  PROB_HIST *ph;
  GNODE *nd;

  for (j=firstnote; j <= lastnote; j++) {
    left_edge[j] = frames-1;
    rite_edge[j] = 0;
  }
  for (j=0; j < frames;  j++) {
    ph = prob_hist + j;
    for (k=0; k < ph->num; k++) {
      nd = ph->list[k].place;
      n = nd->note;
      if (nd->pos != 0) continue;
      if (j < left_edge[n])  left_edge[n] = j;
      if (j > rite_edge[n])  rite_edge[n] = j;
    }
  }
  for (j=firstnote; j <= lastnote; j++) printf("left = %d rite = %d\n",left_edge[j],rite_edge[j]);
  
}



static void 
parse_mean_var(int left, int rite, float *prob, float *mean, float *var) {
  int i;
  float tot,x,xx,m;

  tot = x = xx = 0;
  for (i=left; i <= rite; i++) {
    tot += prob[i];
    x += prob[i]*i;
    xx += prob[i]*i*i;
  }
  if (tot == 0.) {
    printf("problem in parse_mean_var\n");
    exit(0); 
  }
  *mean = m = x/tot;
  *var = xx/tot - m*m;
  if (*var < 0) *var = 0;  // numerical error
  //  if (*var < 0) { printf("x = %f xx = %f tot = %f mean = %f var = %f\n",x,xx,tot,*mean,*var); for (i=left; i <= rite; i++) printf("%d %f\n",i,prob[i]); exit(0); }
}


static void
wiser_mean_parse() {
  int i,j,n,pos,note,k,left_edge[MAX_SOLO_NOTES],rite_edge[MAX_SOLO_NOTES],t;
  PROB_HIST *ph;
  GNODE *pi,*pj,*nd;
  float sum,*mean,*opt,*vector(),p,prob,post_dist[MAX_FRAMES]; 
  float m,v;


  notetime  = (int*) malloc(score.solo.num*sizeof(int));
  opt  = vector(firstnote-1,lastnote+1);
  set_left_rite_edges(left_edge,rite_edge);
  for (i=firstnote; i <= lastnote; i++) notetime[i] = opt[i] = 0;

  for (i=firstnote; i <= lastnote; i++) {
    for (j = left_edge[i]; j <=  rite_edge[i]; j++) {
      ph = prob_hist + j;
      post_dist[j]= 0;
      for (k=0; k < ph->num; k++) {
	nd = ph->list[k].place;
	if (nd->note == i && nd->pos == 0) post_dist[j] += ph->list[k].prob;
      }
      /*      printf("note = %s  token = %d prob = %f\n",score.solo.note[i].observable_tag,j,post_dist[j]);*/
    }
    if (i) parse_mean_var(left_edge[i], rite_edge[i], post_dist, &m, &v);
    printf("note = %10s\tm = %f\ts = %f\n",score.solo.note[i].observable_tag,m,sqrt(v));
    
  }
  
  /*  for (token =notetime[i-1]; token < frames-WINDOW; token++) { 
      prob = 0;
      for (j=token-WINDOW; j <= token+WINDOW; j++) {
        ph = prob_hist + j;
        for (k=0; k < ph->num; k++) {
          nd = ph->list[k].place;
	  if (rel_note(nd,note) == 0)
	    prob += ph->list[k].prob;
        }
      }
      if (prob > opt[note]) { notetime[note] = token; opt[note] = prob; }
      if (all_past_note(ph,note)) break;
    }
  }
  for (i=firstnote; i <= lastnote; i++) {
    score.solo.note[i].realize = notetime[i];
    score.solo.note[i].off_line_secs = toks2secs(notetime[i]);
  }
  writebeat();*/
}


static void
traceback_parse() {
  int i,j,n,pos,note,k,mxk;
  PROB_HIST *ph;
  GNODE *pi,*pj,*nd;
  NODE *best;
  float sum,*mean,*opt,*vector(),p,prob,mx=0; 
  
  ph = prob_hist + frames - 1;
  for (i=0; i < active.length; i++) if (active.el[i]->prob > mx) {
    mx = active.el[i]->prob;
    best = active.el[i];
  }
  for (i=frames-1; i >= 0; i--) {
    /*    printf("note = %d pos = %d frame = %d\n",best->place->note,best->place->pos,i);  */
    if (best->place->pos == 0)  {
      n = best->place->note;
      /*            printf("note = %d at frame %d\n",best->place->note,i);  */
      score.solo.note[n].realize = i;
      score.solo.note[n].off_line_secs = toks2secs(i); 
    }
    best = best->mom;
  }
}






static void
recent_mean_parse() {
  int i,j,n,pos,note,k;
  PROB_HIST *ph;
  GNODE *pi,*pj;
  float sum,*mean,*opt,*vector(),p,prob; 
  
  notetime  = (int*) malloc(score.solo.num*sizeof(int));
  opt  = vector(firstnote-1,lastnote+1);
  for (i=firstnote; i <= lastnote; i++) notetime[i] = opt[i] = 0;
  for (note=firstnote; note <= lastnote; note++) {
    for (token =WINDOW; token < frames-WINDOW; token++) { 
      prob = 0;
      for (j=token-WINDOW; j <= token+WINDOW; j++) {
        ph = prob_hist + j;
        for (k=0; k < ph->num; k++) {
          n = ph->list[k].place->note;
          p = ph->list[k].prob;
          pos = ph->list[k].place->pos;
          if (n == note && pos == 0) prob += p;
        }
      }
 if (note == 40 && token > 1310  && token < 1350) printf("%d %f\n",token,prob);    
      if (prob > opt[note]) { notetime[note] = token; opt[note] = prob; }
    }
  }
  //  writebeat();
}


static void
mark_cues() {
    int i,n;

    for (i=0; i < cue.num; i++) {
	n = cue.list[i].note_num;
	if (n >= firstnote && n < lastnote) cue.list[i].trained = 0;  
	/* no longer current in traning */
    }
}


static void
add_example() {
    int i,j,n,phrase_start,phrase_end,m;

    if (score.example.num == MAX_EXAMPS) {
      printf("cannot hold more examples\n");
      return;
    }
    mark_cues();
    n = score.example.num;
    for (i=firstnote; i <= lastnote; i++) {
      if (score.solo.note[i].off_line_secs/*notetime[i]*/ <= 0. ) printf("problem in add_example()\n");
      /*   if (i > firstnote)
	if (notetime[i] < notetime[i-1]) printf("problem in add_example()\n"); */
      score.example.list[n].time[i-firstnote] = /*notetime[i];*/
	secs2tokens(score.solo.note[i].off_line_secs);
	/*printf("notetime[%d] = %d\n",i,notetime[i]); */
      }
    score.example.list[n].start = firstnote;
    score.example.list[n].end = lastnote;
    score.example.num++;
    write_examples();
}


static int  
note_comp(n1,n2)   /* compare nodes by note */
NODEPTR *n1,*n2; {
  NODEPTR na,nb;
  int a,b;
  

  return( (*n1)->note - (*n2)->note );
}


// routine that calculates the accomp notes sounding when called
// used on real time

SOUND_NUMBERS *sound_accomp()
{
  static SOUND_NUMBERS empty_sn;
  MIDI_PART *p;
  
  p = &(score.midi);
  empty_sn.num = 0;
  if (cur_accomp <= first_accomp)
    return(&empty_sn);
  else
    return(&(p->burst[cur_accomp-1].snd_notes));
}


static float *
get_acc_spect() {
  
  if (cur_accomp <= first_accomp || cur_accomp > last_accomp) return(restspect);
  printf("token = %d using chord %d\n",token,cur_accomp-1);
  return(score.midi.burst[cur_accomp-1].spect);
}

static void
set_hyp_mask(ACTLIST *src) {
  int i,j,k,index,m,hit[128],w,need,chnum,count;
  NODEPTR cur;
  ARC_LIST next;
  float *spect;


  for (w = 0 ; w < freqs; w++)  hyp_mask[w] = 1;
  return;

  for (m=0; m < 128; m++) hit[m] = 0;
  for (w=0; w < freqs; w++) hyp_mask[w] = 0;
  for (w=0; w < freqs; w++) hyp_mask[w] = 0;
  for (i=0; i <  src->length; i++) {
    cur = src->el[i];
    next = cur->place->next;       
    for (j=0; j < next.num; j++) { 
      index = next.list[j].ptr->note;
      /*      chnum = score.solo.note[index].snd_notes.num;
      for (need = 0, k=0; k < chnum; k++) {
	m = score.solo.note[index].snd_notes.snd_nums[k];
	if (hit[m] == 0) need = 1;
	hit[m] = 1;
	}*/
      /*if (need) */ {  // if any of the notes in chord not yet accounted for
	spect = score.solo.note[index].spect;
	for (w=0; w < freqs; w++) if (spect[w] > 0.) hyp_mask[w] = 1;
      }
    }
  }
  //  for (w=0; w < freqs; w++) if (hyp_mask[w]) break;
  //  for (w = 50 ; w < freqs; w++)  hyp_mask[w] = 1;
  

  
  //  for (count=w=0; w < freqs; w++) count += hyp_mask[w];
  //  printf("set_hyp_mask count = %d\n",count);
}


//CF:  Called during the live version of fwd-bkward
//CF:  target trg is always link[0], we construct it here.
//CF:  src is always previous set of forward hypotheses
//CF:  
//CF:  NB link is a set of current (time t) hypotheses, 
//CF:  but unlike fwd, each hypothesis stores its orignating state.
static void
couples(src,trg)  
ACTLIST *src,*trg; {
/* set link[0] as  p(x(t) | x(t-1)) p(y(t) | x(t))  [ t = token ] */
  NODEPTR cur=start,new;
  int i,ind,j;
  ARC_LIST next;
  int s,st,n,l,ln,index;
  float class_like();
  float sa_class_like(),time;
  extern int num_notes;
  FULL_STATE fs;
  SA_STATE sas;
  SOUND_NUMBERS *notes;
  int ii;
  char chord[100];
  float *acc_spect;
  float t1,t2,x;


  notes = sounding_accomp(token);
  time = time_to_last_accomp(token);
  //  acc_spect = (mode == LIVE_MODE) ? get_acc_spect() : frame2spect(token);
 
  acc_spect = frame2spect(token);

#ifdef PIANO_RECOG
  //  set_atckspect(token);
  //  acc_spect = get_overlap_accomp_spect(token);
  acc_spect = get_overlap_accomp_spect(token);
#endif  

  //  if (token%100 == 0) for (i=0; i < freqs; i++) printf("acc_spect[%d] = %f\n",i,acc_spect[i]);

  /*      printf("token = %d time to last = %f\n",token,time);
  for (i=0; i < notes->num; i++) printf("%d ",notes->snd_nums[i]);
  printf("\n");*/


  /*  if (token > 7900 && token < 8400) {
    sndnums2string(*notes,chord);
    printf("token = %d chord = %s\n",token,chord);
    }*/


  /* st = REST_STATE;
 n = 0;
 sas = set_sa_state(restspect,0,st,n,n,time,notes);
 printf("token = %d time = %f like = %f\n",token,time,sa_class_like(sas)); 
 for (ii = 0; ii < notes->num; ii++) printf("%d\n",notes->snd_nums[ii]);
 printf("\n"); */


  free_list(trg);
/*  link[0]->length = 0; */
  for (i=0; i <  src->length; i++) {
    cur = src->el[i];
    next = cur->place->next;       //CF:  place a GNODE; next is list of possible sucessor GNODEs (destinations)
    for (j=0; j < next.num; j++) { //CF:  for each possible destination x_t GNODE
      new = alloc_node();
      new->place = next.list[j].ptr; //CF:  the destination
      new->orig = cur->place;        //CF:  the origin (source)
      new->prob = next.list[j].prob; //CF:  the transition probability
/*printf("%d %d %f\n",new->orig,new->place,new->prob);*/

      //CF:  get a bunch of state infromation used to compute P(data|state)
      st = new->place->state.statenum;
      n = new->place->state.arg[0]; 
      ln = new->place->state.arg[1]; 
      index = new->place->note;     //CF:  index of solo score note
      

      sas = set_sa_state(acc_spect,index,st,n,ln,time,notes,new->place->pos);


    /*    sas = set_sa_state(acc_spect,-1,st,n,ln,time,notes);
    t1 =  sa_class_like(sas); 
    sas = set_sa_state(acc_spect,0,st,n,ln,time,notes);
    t2 =  sa_class_like(sas); 
    printf("rat = %f/%f = %f\n",t1,t2,t1/t2); */

      /*      if (token > 0 && token < 1250)  
	      printf("token = %d index = %d st = %d n = %d like = %f\n",token,index,st, n,sa_class_like(sas));*/

      x =   sa_class_like(sas);   //CF:  compute data likelihood
      new->prob *=   x;           //CF:  store the link probability, P(x_t | x_{t-1}) . P(y_t | x_t)

      //            if (token < 150) printf("%f\n",x);

      /*      s = state2index(st,n,fs);  
	      new->prob *=  class_like(s);  */


/*printf("s = %d st = %d n = %d num_notes = %d like = %f\n",s,st,n,num_notes,class_like(s)); */
      add_list(trg,new);
    }
  }
}


static int
comb_comp(const void *p1, const void *p2) {
  int c;
  NODEPTR *i,*j;

  i = (NODEPTR *) p1;
  j = (NODEPTR *) p2;

  c =  (int) ((*i)->place - (*j)->place);
  if (c) return(c);
  c =  (int) ((*i)->orig - (*j)->orig);
  return(c);
}

static int
comb_comp_orig(const void *p1, const void *p2) {
  int c;
  NODEPTR *i,*j;

  i = (NODEPTR *) p1;
  j = (NODEPTR *) p2;
  return ((int) ((*i)->orig - (*j)->orig));
}

static int
comb_comp_place(i,j)
NODEPTR *i,*j; {
  int c;

  return((int) ((*i)->place - (*j)->place));
}

/* l1 = f(x(1),x(2)) ; l2 = g(x(2),x(3)) ; 
   
   l3 = sum f(x(1),x(2))g(x(2),x(3))
        x(2)   */

//CF:  Used in both forward and backward recursions for live perfomance.
//CF:           l1 : P(a->b)  :ranging over variables a and b
//CF:           l2 : P(b->c)  :ranging over variable b and c
//CF:  result:  l3 : P(a->c)  :ranging over variable a and c
static void
combine(l1,l2,l3)
ACTLIST *l1,*l2,*l3; {
  int i,j,i0,j0,i1,j1,x,done=0;
  NODE  *cur1,*cur2,*new;

/*  l3->length = 0; */
  free_list(l3);
  for (i=0; i < l1->length; i++) {    //CF:  for each state x(t-1)
     cur1 = l1->el[i];
     for (j=0; j < l2->length; j++) {  //CF:  search for links out of x(t-1)
       cur2 = l2->el[j];
       if (cur1->place == cur2->orig) {  //CF:  if found a link out
         new = alloc_node();              //CF:  create a new hypothesis for x(t) at link destination
         new->place = cur2->place;
         new->orig = cur1->orig;
         new->prob = cur1->prob*cur2->prob;
         add_list(l3,new);               //CF:  add it to list of x(t) hypotheses
       }
     }
   }

  //CF:  collate destination probabilities
  qsort(l3->el,l3->length,sizeof(NODEPTR),comb_comp);
  for (i=l3->length-1; i >= 1; i--) 
    if (l3->el[i]->place == l3->el[i-1]->place) 
      if (l3->el[i]->orig  == l3->el[i-1]->orig) {
	l3->el[i-1]->prob += l3->el[i]->prob;
	del_list(l3,i); 
      }
}



static void
unity_scale_list(list)   /* scale to sum to 1 */
ACTLIST *list; {
  NODEPTR cur=start;
  float total;
  int i;

  total = 0;
  for (i=0; i < list->length; i++)  {
    cur = list->el[i];
    total += cur->prob;
  }
  for (i=0; i < list->length; i++)  {
    cur = list->el[i];
    cur->prob /= total;
  }
} 



static void
implement_solo_gates(list)  
ACTLIST *list; {
  NODEPTR cur=start;
  float total;
  int i,n,g;


  for (i=0; i < list->length; i++)  {
    cur = list->el[i];
    n = cur->place->note;
    if (n < firstnote || n > lastnote) continue;
    g = score.solo.note[n].gate;
    if (g == -1) continue;  /* usually this will happen */
    if (mode == SIMULATE_MODE) { if (token2secs(token) > score.midi.burst[g].actual_secs) continue; }
    else { if (cur_accomp > g) continue; }
    //    printf("token = %d  solo gate pruning here of %s solo note gate = %d cur_accmomp = %d\n",token,score.solo.note[n].observable_tag,g,cur_accomp); 
    cur->prob = 0;  /* if gated prune out hypothesis */
  }	
} 



/*#define NOW     0
#define WAITING -1
#define GONE    1*/





static void
init_bkwd(bkwd,frwd)
ACTLIST *bkwd,*frwd; {
  int i;
  NODEPTR cur,new;

  free_list(bkwd);
  for (i=0; i < frwd->length; i++)  {
   cur = frwd->el[i];
    new = alloc_node();
    new->orig = cur->place;
    new->place = cur->place;
    new->prob = 1;
    add_list(bkwd,new);
  }  
}

static void
make_post(frwd,bkwd)  
/* take product of frwd (place) and bkwd (orig) lists 
    result in bkwd; frwd unchanged */
ACTLIST *bkwd,*frwd; {
  int i,j;
  NODEPTR curf,curb;   //CF:  current backward and forward hypotheses under consideration

  for (i=0; i < bkwd->length; i++)  {
    curb = bkwd->el[i];
    curb->place = curb->orig;
    for (j=0; j < frwd->length; j++)  {
      curf = frwd->el[j];
      if (curf->place == curb->orig) {
	curb->prob *= curf->prob;
	break;
      }
    }
    if (j == frwd->length) curb->prob = 0;
  }
  unity_scale_list(bkwd);
}

static void
new_make_post(ACTLIST *frwd, ACTLIST *bkwd, ACTLIST *post) {
/* take product of frwd (place) and bkwd (orig) lists 
    result put in post; frwd unchanged  elments of bkwd that don't match anything in
   frwd are deleted  */
  int i,j;
  NODEPTR curf,curb,new;


  free_list(post);
  for (i=bkwd->length-1; i >= 0; i--)  {
    curb = bkwd->el[i];
    curb->place = curb->orig;
    for (j=0; j < frwd->length; j++)  {
      curf = frwd->el[j];
      if (curf->place == curb->orig) {
	new = alloc_node();
	*new = *curb;
	new->prob *= curf->prob;
	add_list(post,new);
	break;
      }
    }
    if (j == frwd->length) del_list(bkwd,i);  /* no match found */
  }
  unity_scale_list(bkwd);
}



static void
cycle(ptr)
ACTLIST **ptr; {
  ACTLIST *temp;
  int i;

  temp = ptr[MAX_LAG];  /* cyclic permutation of fwd */
  for (i=MAX_LAG; i > 0; i--) ptr[i] = ptr[i-1];  
  ptr[0] = temp; 
}


#define NOTE_TRIGGER  /*.995*/ .95    /* necessary prob to say note has already happened */
//#define NOTE_HAIR_TRIGGER  .75
#define NOTE_HAIR_TRIGGER  .75

//CF:  is the cur_note passed yet?
//CF:  (cur_note is the solo note we are currently looking for, and will report to Anticipate)
//CF:  'after' is P(cur_note is passed)
//CF:     if our belief in this is above a threshold, report that it is indeed passed.
static int 
is_note_past(list)
ACTLIST *list; {

  float after=0;
  int i;
  NODEPTR node;

  //    printf("cur_note = %d\n",cur_note); 
  for (i=0; i < list->length; i++) {       //CF:  for each hypothesis
    node = list->el[i];
    //             printf("note = %d %f\n",node->place->note,node->prob);
    //CF:  if the hypothesis says cur_note onset has passed, add its probability 
    if (node->place->note >= cur_note) after += node->prob;  
  }
  //    if (score.solo.note[cur_note].cue)  printf("frame = %d curnote = %d after = %f\n",token,cur_note,after); 
  if (score.solo.note[cur_note].trigger) return(after > NOTE_HAIR_TRIGGER); //CF:  not used
#ifdef JAN_BERAN
  else return(after > NOTE_HAIR_TRIGGER);  
#else
  else return(after > NOTE_TRIGGER);
#endif
}


static int 
is_solo_performance_over(list)
ACTLIST *list; {

  float after=0;
  int i;
  NODEPTR node;

  if (mode == SYNTH_MODE) return(token == frames-1); 
  for (i=0; i < list->length; i++) {
    node = list->el[i];
    if (node->place == end_node) after += node->prob;
  }
  //  printf("after = %f cur_note = %d lastnote = %d\n",after,cur_note,lastnote);
  return(after > NOTE_TRIGGER);
}


//CF:  
static float 
prob_event(list)  /* prob that cur_note starts at token assoc w/ list */
ACTLIST *list; {
  float at=0;
  int i;
  NODEPTR node;

  for (i=0; i < list->length; i++) {
    node = list->el[i];
    if (node->place->note == cur_note && node->place->pos == 0)  //CF:  pos=0 means GNODE represents the onset
      at += node->prob; 
  }
  return(at);
}


#define EVENT_THRESH .5  
#define AFTER_THRESH .9
#define HIST_LENGTH 3

static float start_window[HIST_LENGTH];   /* prob of cur_note occurring i frames ago */


static void
goto_next_note() { 
  int i;

  cur_note++;
  for (i=0; i < HIST_LENGTH; i++) start_window[i]=0;
}  





static void
collect_list(list) 
ACTLIST *list; {
  int i;

  qsort(list->el,list->length,sizeof(NODEPTR),comb_comp_orig);
  for (i=list->length-1; i >= 1; i--) 
  if (list->el[i]->orig == list->el[i-1]->orig) {
    list->el[i-1]->prob += list->el[i]->prob;
    del_list(list,i); 
  }
  qsort(list->el,list->length,sizeof(NODEPTR),comb_comp_orig);
}


static void
account_future(l1,l2)
ACTLIST *l1,*l2; {
  int i,j;

  i=j=0;
  while (i < l1->length && j < l2->length) {
    if (l1->el[i]->place == l2->el[j]->orig) {  
      l1->el[i]->prob *= l2->el[j]->prob;
      i++;
      j++;
    } 
    else if (l1->el[i]->place < l2->el[j]->orig) {
      l1->el[i]->prob = 0;
      i++; 
    }
    else j++;
  }
  while (i < l1->length) l1->el[i++]->prob = 0;
}



static int
frame_center(int f) {
  return((f+1)*SKIPLEN - FRAMELEN/2);
}




static void
new_note_event(int lag, float detect_secs, int is_accurate, float std_err) { 
  /* how many tokens ago note occurred */ 
  float time,secs,now(); /* cur_note occured at time (in tokens) */
  int missed_note,not_last_note,treating_as_observed; /* boolean */ 
  int i;
  SOLO_NOTE *nt;
  extern float cur_secs;
  extern int global_flag;
  char name[500];

  //  printf("new note event for note %d at %d\n",cur_note,token);

  /*  printf("token = %d lag = %d\n",token,lag);
      exit(0); */

  /*  if (mode == SYNTH_MODE && cur_note > lastnote) return;
  if (mode == LIVE_MODE && cur_note > lastnote) {
    printf("just recognized last solo note\n");
    lasttoken = 1; 
    frames = token;
    return;
    } */

  not_last_note= (cur_note < score.solo.num-1);
/*  if (cur_note < firstnote || cur_note >= score.solo.num-1) return; /* no update exists */ 
  missed_note = (lag == MAX_LAG);






  time = token + .5 - lag;  


  //  time = frame_center(token-lag);

  time = frame_center(token-lag);



#ifndef BOSENDORFER
  if (mode == LIVE_MODE) time -= (audio_skew*(SR/(float)OUTPUT_SR));  /* this doesn't seem to work for Vista.  I have seen
									sudden large jumps in the skew that gave disatrous 
									results when corrected for.  Mostly likely the measurement
									of the skew is inaccurate, sometimes very much so. */
   //  time -= .05;
#endif
  time /=SKIPLEN; 

  //  if (midi_accomp) time += 2; // experiment 'cause midi is early otherwise
  



  secs = tokens2secs(time); 

#ifdef JAN_BERAN
  secs += .030;
#endif

  /*  time = frame_center(token-lag) / (float) SKIPLEN;
      secs = tokens2secs_sync(time); */

  nt = &score.solo.note[cur_note];
  treating_as_observed = (is_accurate || nt->cue);
  nt->treating_as_observed  = treating_as_observed;
  nt->realize = (missed_note) ? HUGE_VAL : time;

  //    nt->on_line_secs = (treating_as_observed) ? secs : 0.;
  nt->on_line_secs = secs;   // 10-06 changed for post hoc analysis of detection threshold


  nt->lag = lag; 
  nt->detect_frame = token;
  nt->detect_time = now();
  nt->std_err = std_err;  // std err of onset time estimate in secs.  save for post hoc evaluation of error threshold
  /*  if (nt->cue) new_cue_phrase(time);
  else if (missed_note && not_last_note) blind_kalman_update();
  else if (not_last_note) new_kalman_update(time); */

/*if (cur_note == lastnote) printf("note event: token = %d tokkks = %f time = %f  %f %f\n",token,secs2tokens(cur_secs),time,cur_secs,tokens2secs(time)); */

  num2name(score.solo.note[cur_note].num,name);

  //CF:  ***
  cur_note++;  /* cur_note and cue.cur must be kept in agreement with state_hat
		  i.e. cur_note is note AFTER the one described by state_hat */
  
  /*  if (mode == SYNTH_MODE && cur_note > lastnote) return;*/
  if (mode == LIVE_MODE && cur_note > lastnote) {
    printf("just recognized last solo note\n");
    lasttoken = 1; 
    frames = token;
  } 

  //  if ((is_accurate ==0 && nt->cue == 0)) {
  if (treating_as_observed == 0) {
    printf("missed note %d (%s) at %f (now = %f) lag = %d frames  std_err = %f\n",cur_note-1,score.solo.note[cur_note-1].observable_tag,secs,now(),lag,std_err);               
    return;
  }
      printf("heard note %d (%s) at %f (%d) (now = %f) lag = %d frames     late = %f\n",cur_note-1,score.solo.note[cur_note-1].observable_tag,nt->on_line_secs,token,now(),lag,nt->on_line_secs - nt->off_line_secs);               
      
      //         printf("%f %f\n",nt->off_line_secs,nt->on_line_secs);
     if (mode != SIMULATE_MODE) {
    /*    if (cur_note == 10 ) { 
      printf("undo this bad mojo\n"); 
      update_belief_solo(cur_note-1,secs-120.);
      } else*/


    //      if ((cur_note-1) == 1476) update_belief_solo(cur_note-1,secs-.3);
    //CF:  call with detected solo note numbers, and observed time
    if (cur_accomp < score.midi.num)  //CF:  call to Bayes net BN *******
      if (mmo_mode == 0 || nt->cue) update_belief_solo(cur_note-1,secs);   // this change 5-11
    //abcde    if (nt->cue) queue_event();
  }

  /*  if (not_last_note) predict_future(10,10);  /* neither argument is used */
  if (nt->cue) cue.cur++; //CF:  not used
  for (i=0; i < HIST_LENGTH; i++) start_window[i]=0; // not used  

}







//CF:  meat of the live version of 'fwd-bkwd step'
//CF:  See documentation!
//CF:  link[k] is an efficient implementeation of the mini-backward step for incomplete observation data at t.
//CF:    it is a 2D function of BOTH the state of x_{t-k} and the latest state x_t.
static int
new_bridge() {
  int i,maxi,accurate_detection;
  float p,max_prob = -HUGE_VAL,total=0,time,detect_secs;
  float now(),mt,m,v,post[MAX_LAG],std_err;


  //    if (token > 825)  print_pair_list(fwd[0]); 
  //       if (token > 1800 && token < 1850)  print_pair_list(fwd[0]); 
  //  else exit(0);

  //CF:  shift the contents of fwd and link forward by one position, freeing up the 0 space
  cycle(fwd);   //CF:  fwd[k] is P(X_{t-k}|Y_0 ... Y_{t-k})
  cycle(lnk);

  //  set_hyp_mask(fwd[1]);

  couples(fwd[1],lnk[0]); //CF:  init link 0, using fwd[1] to get reasonable hypotheses for x_{t-1}
  /* set link[0] as  p(x(j) | x(j-1)) p(y(j) | x(j))  [ j = token ] */
  
  //CF:  forward pass (same as offline?)
  combine(fwd[1],lnk[0],fwd[0]); /* fwd[0] = p(x(j) | y(0), ... y(j)) [j=token] */
  
  //  if (mode != SIMULATE_MODE && mode != SIMULATE_FIXED_LAG_MODE) 
    implement_solo_gates(fwd[0]); //CF:  set prob to zero for fwd hypotheses failing gate constraint
  unity_scale_list(fwd[0]);  //CF:  normalize
  graph_prune_list(fwd[0]);  //CF:  remove hyps with prob<thresh, subject to keeping a min number of them.

  //    if (token < 300 && token <= 798)  print_pair_list(fwd[0]); 
      
  if (is_solo_performance_over(fwd[0])) {
    //    printf("solo performance over\n");
    return(1);
  }
  if (cur_note > lastnote) return(1);  // maybe redundant
  /* no more note detections at this point */
  
  //CF:  Important bit.  Has the note we are currently searching for passed?   ****************
  //CF:  (If it has passed, then we do the mini backward pass, and call Anticipate)
  if (mode == SIMULATE_FIXED_LAG_MODE) { if (token != score.solo.note[cur_note].detect_frame) return(0); }
  else { if (is_note_past(fwd[0]) == 0) return(0);  } //CF:  return if note not passed
    //CF:  We have detected that cur_note has passed --
    //CF:  Estimate its onset more accurately using a mini-backward pass

    //CF:  set a flat initial bwd distribution over reasonable x(t) states.  (Pick reasonables from fwd)
  init_bkwd(bwd[0],fwd[0]);  /* bkwd has same hypotheses as fwd[0] places but probs = 1 */
  for (i=0; i < MAX_LAG; i++) { 

    //CF:  at this point, bwd[i] is still a bakward probability -- it becomes a posterior later!
    //CF:  bwd[k] = P(y_{t-k+1:t}, x_t   |x_{t-k})
    combine(lnk[i],bwd[i], bwd[i+1]);

    //CF:  make joint gamma(x_{t-k}, x_t)
    make_post(fwd[i],bwd[i]);  //CF:  aka. gamma -- written into bwd[i] -- OVERWRITING old bwd with posterior.
    
    //CF:  marginalize out x_t to get gamma(x_{t-k})
    post[i] = p = prob_event(bwd[i]);  //CF:  post[i]=P(cur_note onset happened exactly i frames in the past)
    //    if (cur_note == 67) printf("i = %d p = %f\n",i,p);
    total += p;
    if (p > max_prob) { maxi = i; max_prob = p; } //CF:  keep track of best frame to explain onset

    //CF:  stop search if we know we have the best (eg. if we have a candidate with P > (that of all other remaining frames))
    if ((1-total) < max_prob) {
      detect_secs = tokens2secs((float) token-maxi);
      parse_mean_var(0, i, post, &m, &v);   //CF:  mean_i(post),  var_i(post)   ; i is the frame number
      std_err = tokens2secs(sqrt(v));
      accurate_detection = (std_err < MAX_STD_SECS_DETECT); 
      
      if (accurate_detection == 0) printf("inaccurate detect (%f > %f)\n",std_err,MAX_STD_SECS_DETECT);
      //            printf("note = %s std error in detect = %f\n",score.solo.note[cur_note].observable_tag,std_err);
      
      //CF:  (wrapper to) call Anticipate -- if the detection is accurate *****
      new_note_event(maxi, detect_secs, accurate_detection, std_err);
      return(0);
    }

    //CF:  ignore this ?
    unity_scale_list(bwd[i+1]);   /* these two lines might not be needed*/
    if (bwd[i+1]->length) graph_prune_list(bwd[i+1]); 
  /* bwd[i] no longer used so it can be destroyed in make_post */
  }

  //CF:  We land here if:  it appears that the note occured more than MAX_LAG (eg 20) frames in the past
  //CF:  We thus have no ability to estimate where it happened.

  //CF:  special case -- if soloist starts with a rest, report right on the first frame.
  if (token == 0)  {
    printf("hello kitty\n");
#ifndef ORCHESTRA_EXPERIMENT    
    new_note_event(0,0.,accurate_detection=1,std_err=0.); 
#endif
  }
  else {
    detect_secs = tokens2secs((float) token-MAX_LAG);
    new_note_event(MAX_LAG,detect_secs,0,MISSED_STD_ERROR);  //CF:  pass accurate_detect=0, meaning uncertain of when note occured
  }
  return(0);
}

#define HAVE_ENOUGH_PROB .95

static void
wise_bridge() {
  int i,maxi,accurate_detection;
  float p,max_prob = -HUGE_VAL,total=0,time,std_err,detect_secs;
  float now(),mt,post[MAX_LAG],m,v;

  cycle(fwd);
  cycle(lnk);
  couples(fwd[1],lnk[0]);
  /* set link[0] as  p(x(j) | x(j-1)) p(y(j) | x(j))  [ j = token ] */
  combine(fwd[1],lnk[0],fwd[0]); /* fwd[0] = p(x(j) | y(0), ... y(j)) [j=token] */
  unity_scale_list(fwd[0]);
  graph_prune_list(fwd[0]);

  /*if (token > 2440 && token < 2540)       print_pair_list(fwd[0]); */



  if (is_note_past(fwd[0]) == 0) return;

  if (token == 0)  {
    printf("hello kitty\n");
    new_note_event(0,0., 1,std_err = 0.); 
    return;
  }
  init_bkwd(bwd[0],fwd[0]);  /* bkwd has same origs as fwd[0] places but probs = 1 */
  for (i=0; i < MAX_LAG; i++) { /* compute backward probs */
    combine(lnk[i],bwd[i],bwd[i+1]);
    make_post(fwd[i],bwd[i]);  
    post[i]= p = prob_event(bwd[i]);
    total += p;
    if (total > HAVE_ENOUGH_PROB) break;
    unity_scale_list(bwd[i+1]);   /* these two lines might not be needed*/
    if (bwd[i+1]->length) graph_prune_list(bwd[i+1]); 
    /* bwd[i] no longer used so it can be destroyed in make_post */
  }
  if (i == MAX_LAG) {
    detect_secs = tokens2secs((float) token-MAX_LAG);
    new_note_event(MAX_LAG,detect_secs,0,std_err=MISSED_STD_ERROR);  /* time unused */
  }
  else {
    parse_mean_var(0, i, post, &m, &v);
    std_err = tokens2secs(sqrt(v));
    accurate_detection = (std_err < MAX_STD_SECS_DETECT);
    detect_secs = tokens2secs((float) token-m);
    new_note_event((int) (m+.5) ,detect_secs,accurate_detection,std_err);
  }
}



static void
calc_orchestra_spect() {
  float dat[FRAMELEN],tot=0,total=0,new_spect[FREQDIM/2],t,u_new_spect[FREQDIM/2];
  unsigned char *temp;
  int i;

  /*     temp =  orchdata + ((token+1)*SKIPLEN - FRAMELEN)*BYTES_PER_SAMPLE;
  samples2floats(temp, dat, FRAMELEN); 
*/


  /*  create_spect(orch_data_frame , orchestra_spect);
  for (i=0; i < freqs; i++) total += orchestra_spect[i];
  if (total > 0) for (i=0; i < freqs; i++) orchestra_spect[i] /= total;  */
  //  for (i=0; i < freqs; i++) printf("%d %f %f\n",i,orchestra_spect[i], orch_data_frame[i]);



  create_spect( orch_data_frame , new_spect);

  /*  for (i=0; i < freqs; i++) u_new_spect[i] = new_spect[i]*room_response[i];
  for (i=0; i < freqs; i++) tot += u_new_spect[i];
  if (tot > 0) for (i=0; i < freqs; i++) u_new_spect[i] /= tot;

  for (i=0; i < freqs; i++) 
    unscaled_orchestra_spect[i] = .75*unscaled_orchestra_spect[i] + .25*u_new_spect[i];
  */


  for (i=0; i < freqs; i++) total += new_spect[i];
  if (total > 0) for (i=0; i < freqs; i++) new_spect[i] /= total;
  else for (i=0; i < freqs; i++) new_spect[i] = 1 / (float) freqs;
  

  for (i=0; i < freqs; i++)  orchestra_spect[i] = .75*orchestra_spect[i] + .25*new_spect[i]; 
  //    for (i=0; i < freqs; i++) printf("%f ",new_spect[i]);
  //    printf("\n");
  //    for (i=0; i < freqs; i++) printf("%f ",spect[i]);
  //    printf("\n");
  //    if (token == 2000) exit(0);
    //    for (i=0; i < freqs; i++) printf("%d %d %f (%f %f)\n",token, i,orchestra_spect[i]/spect[i],orchestra_spect[i],spect[i]);

}


//CF:  gets features for current frame: pitch feature (ie spectogram) and energy and burstiness (or whatever)
//CF:  tree_quant[f] gets filled with quantum for the f feature
//CF:  (this is vector quantization)
//CF:  (this fn is used by both offline and live versions)
void
calc_features() {
  SOUND_NUMBERS *acc_nts;

  setspect();

#ifdef ORCHESTRA_EXPERIMENT
  calc_orchestra_spect();
#endif  

  calc_statistics();

  acc_nts = sounding_accomp(token);


  init_sa_pitch_signature_table(acc_nts);
  clear_spect_like_table();

  /*  make_quantiles(); doesn't seem to be used for anything */
}


//CF:  time that next frame will be ready
static float
frame_samples_available_sec(int f) {
  return(((f+1)*TOKENLEN+SAMPLE_DELIVERY)/(float)SR);
}

float
next_frame_avail_secs() {
  return(frame_samples_available_sec(token+1));
}



static float
next_dp_time() {
  float read_time;

  read_time = tokens2secs((float) token+2);
  /* earliest time at which frame would be avail */
  return (read_time);
} 

static int
dp_finished() {
  int accomp_done,solo_done;

    accomp_done = (cur_accomp == score.midi.num || cur_accomp > last_accomp);
    /*  printf("cur_accomp = %d last_accomp = %d score.midi.num = %d\n",cur_accomp, last_accomp, score.midi.num);*/

  if (mode == LIVE_MODE) {
    if (cur_note <= lastnote) solo_done = 0;
    else solo_done = (now () > score.solo.note[lastnote].detect_time + 1.);

    //  if (cur_note >= 53) printf("cur_accomp = %d last_accomp = %d cur_note = %d lastnote = %d\n",cur_accomp,last_accomp,cur_note,lastnote);
    return((token == MAX_FRAMES-1) ||
	   (accomp_done &&  solo_done /*(cur_note > lastnote)*/    )); 
  }
  //	   (cur_accomp > last_accomp &&  cur_note > lastnote)); 
  if (mode == SYNTH_MODE) {
//    if (cur_note > 305) printf("accmop_done = %d cur_accomp = %d last_accomp = %d\n",accomp_done,cur_accomp,last_accomp);
//    return((token == frames-1) || (cur_accomp > last_accomp &&  cur_note > lastnote)); 
#ifndef ORCHESTRA_EXPERIMENT    
    return((token == frames-1) || (accomp_done &&  cur_note > lastnote)); 
#endif
#ifdef ORCHESTRA_EXPERIMENT    
    /* might want both of these to be same */
    return((token == frames-1));
#endif
  }
  else return(lasttoken || failed);
}

static int
done_with_callbacks(int end_of_solo) {
  int accomp_done,solo_done;
  float last_acc_time,end_time,cur_time;

  //  if (token % 30 == 0)
 // printf("solo = %d cur_accomp = %d last = %d\n",end_of_solo,cur_accomp,last_accomp);

  last_acc_time = score.midi.burst[last_accomp].actual_secs;  /* only meaningful of last_accomp has been played */
  end_time = last_acc_time+3;  // also only meaning f last_accomp has been played
  cur_time = tokens2secs((float)token); 
  accomp_done = ((cur_accomp == score.midi.num || cur_accomp > last_accomp) 
		 && (cur_time > end_time));

    //    && (last_acc_time < (tokens2secs((float)token) - 3.));

    if (accomp_done)
      printf("accom done ... end_of_solo = %d\n",end_of_solo);

  if (mode == LIVE_MODE) return((token == MAX_FRAMES-1) || (accomp_done &&  end_of_solo)); 
  if (mode == SYNTH_MODE) return((token == frames-1)/* || (accomp_done &&  cur_note > lastnote)*/); 
  else return(lasttoken || failed);
}

#define SCHEDULE_FILE "schedule_times.dat"


static void
write_schedule_times() {
  int i,j;
  FILE *fp;

  fp = fopen(SCHEDULE_FILE,"w");
  for (i=first_accomp; i <= last_accomp; i++) {
    fprintf(fp,"%s\t",score.midi.burst[i].observable_tag);
    for (j=0; j < MAX_SCHEDULE; j++) {
   //
   //fprintf(fp,"%f\t",score.midi.burst[i].schedule.exec_secs[j]);
    //  fprintf(fp,"%f\t",score.midi.burst[i].schedule.set_secs[j]);
    }
    fprintf(fp,"\n");
  }
  for (i=firstnote; i <= lastnote; i++) {
    fprintf(fp,"%s\t%f\n",score.solo.note[i].observable_tag,
	    score.solo.note[i].off_line_secs);
  }
  fclose(fp);
}



static void
get_parse_name(char *name) {
  int i;

  strcpy(name,audio_file);
  for (i=strlen(name); i >=0; i--)
    if (name[i] == '.') break;
  name[i+1]=0;
  strcat(name,"times");
}

static void
get_orch_parse_name(char *name) {
  int i;

  strcpy(name,user_dir);
  strcat(name,SCORE_DIR);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,".times");
}


static void
get_orch_name(char *name) {
  int i;

  strcpy(name,audio_file);
  for (i=strlen(name); i >=0; i--)
    if (name[i] == '.') break;
  name[i+1]=0;
  strcat(name,"ork");
}

static void
get_atm_name(char *name) {
  int i;

  strcpy(name,audio_file);
  for (i=strlen(name); i >=0; i--)
    if (name[i] == '.') break;
  name[i+1]=0;
  strcat(name,"atm");
}


static void
get_orch_atm_name(char *name) {
  int i;

  strcpy(name,user_dir);
  strcat(name,SCORE_DIR);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,".atm");
}


void
get_score_file_name(char *name, char *suff) {
  int i;
    
  strcpy(name,user_dir);
 strcat(name,SCORE_DIR);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,suff);
}


void
get_data_file_name_piece(char *name, char *piece, char *num, char *suff) {

  strcpy(name,user_dir);
   strcat(name,AUDIO_DIR);
  strcat(name,"/");
  strcat(name,player);
  strcat(name,"/");

  strcat(name,piece);
  strcat(name,"/");
  strcat(name,piece);
  strcat(name,".");
  strcat(name,num);
  strcat(name,".");
  strcat(name,suff);
}



void
get_data_file_name(char *name, char *num, char *suff) {

  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".");
  strcat(name,num);
  strcat(name,".");
  strcat(name,suff);
}


/*static void
get_parse_name_num(char *name, char *num) {

  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".");
  strcat(name,num);
  strcat(name,".times");
}

static void
get_atm_name_num(char *name, char *num) {

  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".");
  strcat(name,num);
  strcat(name,".atm");
}

static void
get_raw_name_num(char *name, char *num) {

  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".");
  strcat(name,num);
  strcat(name,".raw");
}
*/


void
write_parse() {
  char file[200],name[500],tag[500],atm_file[500];
  int i;
  FILE *fp,*atm_fp;

  /* strcpy(file,audio_file);
  for (i=strlen(file); i >=0; i--)
    if (file[i] == '.') break;
  file[i+1]=0;
  strcat(file,"times"); */
  get_parse_name(file);
  get_atm_name(atm_file);
  fp = fopen(file,"w");
  atm_fp = fopen(atm_file,"w");
  if (atm_fp == 0) { printf("couldn't open %s\n",atm_file); exit(0); }
  /*  fprintf(fp,"start = %f\n",start_meas);
      fprintf(fp,"end = %f\n",end_meas); */
  fprintf(fp,"start_pos = %s\n",start_string);
  fprintf(fp,"end_pos = %s\n",end_string);
  fprintf(fp,"firstnote = %d\n",firstnote);
  fprintf(fp,"lastnote = %d\n",lastnote);
  for (i=firstnote; i <= lastnote; i++) {
    /*    num2name(score.solo.note[i].num,name); 
    sprintf(tag,"%04d",i);
    strcat(tag,"_");
    strcat(tag,name);
       fprintf(fp,"%d\t%s\t%6.3f\n",i,name,score.solo.note[i].realize);*/
    //    fprintf(fp,"%s\t%6.3f\t%d\n",score.solo.note[i].observable_tag,score.solo.note[i].realize,score.solo.note[i].set_by_hand);
        fprintf(fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,score.solo.note[i].realize);
    fprintf(atm_fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,score.solo.note[i].off_line_secs);
  }
  fclose(fp);
  fclose(atm_fp);
}

void
write_orch_parse() {
   char file[200],name[500],tag[500],atm_file[500];
  int i;
  FILE *fp,*atm_fp;

  get_orch_parse_name(file);
  get_orch_atm_name(atm_file);
  fp = fopen(file,"wt");
  atm_fp = fopen(atm_file,"wt");
  if (atm_fp == 0) { printf("couldn't open %s\n",atm_file); exit(0); }
  /*  fprintf(fp,"start = %f\n",start_meas);
      fprintf(fp,"end = %f\n",end_meas); */
  fprintf(fp,"start_pos = %s\n","unknown");
  fprintf(fp,"end_pos = %s\n","unknown");
  fprintf(fp,"firstnote = %d\n",0);
  fprintf(fp,"lastnote = %d\n",score.midi.num-1);
  for (i=0; i < score.midi.num; i++) {
    /*    num2name(score.solo.note[i].num,name); 
    sprintf(tag,"%04d",i);
    strcat(tag,"_");
    strcat(tag,name);
       fprintf(fp,"%d\t%s\t%6.3f\n",i,name,score.solo.note[i].realize);*/
    fprintf(fp,"%s\t%f\t%d\n",score.midi.burst[i].observable_tag,
	    (float) score.midi.burst[i].frames,
	    score.midi.burst[i].set_by_hand);
    fprintf(atm_fp,"%s\t%6.3f\n",score.midi.burst[i].observable_tag,
	    tokens2secs(score.midi.burst[i].frames));
  }
  fclose(fp);
  fclose(atm_fp);
}


void
write_current_parse() {
  char file[500],name[500],tag[500],atm_file[500];
  int i;
  FILE *fp,*atm_fp;

  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".times");

  strcpy(atm_file,audio_data_dir);  
  strcat(atm_file,current_examp);   
  strcat(atm_file,".atm");

  printf("saving %s and %s\n",file,atm_file);
  wholerat2string(start_pos,start_string);
  wholerat2string(end_pos,end_string);
  fp = fopen(file,"w");
  atm_fp = fopen(atm_file,"w");
  if (atm_fp == 0) { printf("couldn't open %s\n",atm_file); exit(0); }
  fprintf(fp,"start_pos = %s\n",start_string);
  fprintf(fp,"end_pos = %s\n",end_string);
  fprintf(fp,"firstnote = %d\n",firstnote);
  fprintf(fp,"lastnote = %d\n",lastnote);
  for (i=firstnote; i <= lastnote; i++) {
    //    fprintf(fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,score.solo.note[i].realize);
    //	    fprintf(fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,(float) score.solo.note[i].frames);
    fprintf(fp,"%s\t%6.3f\t%d\n",score.solo.note[i].observable_tag,(float) score.solo.note[i].frames,score.solo.note[i].set_by_hand);
    fprintf(atm_fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,score.solo.note[i].off_line_secs);
  }
  fprintf(fp,"accomp_on_speakers = %d\n",accomp_on_speakers);
  fclose(fp);
  fclose(atm_fp);
}


void
write_current_parse_stump() {
  char file[500],name[500],tag[500],atm_file[500];
  int i;
  FILE *fp,*atm_fp;

  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".times");

  wholerat2string(start_pos,start_string);
  wholerat2string(end_pos,end_string);
  fp = fopen(file,"w");
  fprintf(fp,"start_pos = %s\n",start_string);
  fprintf(fp,"end_pos = %s\n",end_string);
  fclose(fp);
}




static void
write_parse_num(char *num) {
  char file[200],name[500],tag[500],atm_file[500];
  int i;
  FILE *fp,*atm_fp;

  get_data_file_name(file, num, "times");
  get_data_file_name(atm_file, num, "atm");
  fp = fopen(file,"w");
  atm_fp = fopen(atm_file,"w");
  /*  fprintf(fp,"start = %f\n",start_meas);
      fprintf(fp,"end = %f\n",end_meas); */
  fprintf(fp,"start_pos = %s\n",start_string);
  fprintf(fp,"end_pos = %s\n",end_string);
  fprintf(fp,"firstnote = %d\n",firstnote);
  fprintf(fp,"lastnote = %d\n",lastnote);
  for (i=firstnote; i <= lastnote; i++) {
    /*    num2name(score.solo.note[i].num,name); 
    sprintf(tag,"%04d",i);
    strcat(tag,"_");
    strcat(tag,name);
       fprintf(fp,"%d\t%s\t%6.3f\n",i,name,score.solo.note[i].realize);*/
    fprintf(fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,score.solo.note[i].realize);
    fprintf(atm_fp,"%s\t%6.3f\n",score.solo.note[i].observable_tag,score.solo.note[i].off_line_secs);
  }
  fclose(fp);
  fclose(atm_fp);
}





static void
summarize_performance() {
    /*  async_dp_wrap(); */

#ifdef JAN_BERAN
return;
#endif
  /*  end_playing();*/
  /*  set_notetime(); */
  lag_stats();
  /*  review_parse(0);*/
  show_token_hist();
  eval_accomp_note_times();
  //   make_performance_data();
  //   write_schedule_times();
  /*  show_cond_hist();*/
}


static void 
finish_synth_mode() {
  void write_accomp_times();
  int i;

  done_recording = 1;
  printf("finishing synth mode \n");
  init_timer_list();  /* kills all current timer action */
  print_resource_usage();
  summarize_performance();


  //  write_accomp_times();
}

static void
get_accomp_times_name(char *name) {
  int i;

  strcpy(name,audio_file);
  for (i=strlen(name); i >=0; i--)
    if (name[i] == '.') break;
  name[i+1]=0;
  strcat(name,"acc");
}


void
write_accomp_times() {
  char file[200];
  int i;
  FILE *fp;

  get_accomp_times_name(file);
  fp = fopen(file,"w");
  for (i=first_accomp; i <= last_accomp; i++) 
    fprintf(fp,"%s\t%f\n",score.midi.burst[i].observable_tag,score.midi.burst[i].actual_secs); // 11-06
  //     fprintf(fp,"%d %f\n",i,score.midi.burst[i].actual_secs);
  fclose(fp);
}

void
write_current_accomp_times() {
  char file[500];
  int i;
  FILE *fp;

  
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".acc");
  fp = fopen(file,"w");
  for (i=first_accomp; i <= last_accomp; i++) 
    fprintf(fp,"%s\t%f\n",score.midi.burst[i].observable_tag,score.midi.burst[i].actual_secs); // 11-06
  //    fprintf(fp,"%d %f\n",i,score.midi.burst[i].actual_secs);
  fclose(fp);
}

void
write_ideal_accomp_times() {
  char file[500];
  int i;
  FILE *fp;

  
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".acc");
  fp = fopen(file,"w");
  for (i=first_accomp; i <= last_accomp; i++) 
    fprintf(fp,"%s\t%f\n",score.midi.burst[i].observable_tag,score.midi.burst[i].ideal_secs); // 11-06
  //    fprintf(fp,"%d %f\n",i,score.midi.burst[i].actual_secs);
  fclose(fp);
}


void
write_current_hand_set_parms() {
  char file[500];
  FILE *fp;
  HAND_MARK_LIST hnd;
  
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".hnd");
  fp = fopen(file,"w");
  make_hand_mark_list(&hnd);
  write_hnd_to_file(&hnd,fp);
  fclose(fp);
}


void
write_accomp_times_num(char *num) {
  char file[500];
  int i;
  FILE *fp;

  get_data_file_name(file, num, "acc");
  fp = fopen(file,"w");
  for (i=first_accomp; i <= last_accomp; i++) 
    fprintf(fp,"%d %f\n",i,score.midi.burst[i].actual_secs);
  fclose(fp);
}


static void
write_parse_stump() {
  char file[200];
  int i;
  FILE *fp;

  /* strcpy(file,audio_file);
  for (i=strlen(file); i >=0; i--)
    if (file[i] == '.') break;
  file[i+1]=0;
  strcat(file,"times"); */
  wholerat2string(start_pos,start_string);
  wholerat2string(end_pos,end_string);
  get_parse_name(file);
  fp = fopen(file,"w");
  fprintf(fp,"start_pos = %s\n",start_string);
  fprintf(fp,"end_pos = %s\n",end_string);
  fprintf(fp,"firstnote = %d\n",firstnote);
  fprintf(fp,"lastnote = %d\n",lastnote);
  /*  fprintf(fp,"start = %f\n",start_meas);
      fprintf(fp,"end = %f\n",end_meas); */
  fclose(fp);
}


static int
file_exists(char *file_name) {
  FILE *fp;

  fp = NULL;
  fp = fopen(file_name,"r");
  if (fp) { fclose(fp); return(1); }
  else return(0);
}


void
set_audio_file_num_string(char *num) {
  /* 1st number string from 000 to 999 not accounted for by a .raw or .times */
  char af[500],bf[500];
  int i;


  for (i=1; i < 1000; i++) {
    sprintf(num,"%03d",i);
    get_data_file_name(af, num, "raw");
    get_data_file_name(bf, num, "times");
    printf("af = %s bf = %s\n",af,bf);
    if (file_exists(af) == 0 && file_exists(bf) == 0) return;
  }
  printf("1000 files???\n");  exit(0);
}



static void
write_orchestra_audio() {
  FILE *fp;
  char orch_file[1000];


  get_orch_name(orch_file);
  fp = fopen(orch_file,"wb");
  if (fp == NULL) { printf("couldn't open %s\n",orch_file); exit(0); }
  fwrite(orchdata,frames*TOKENLEN,BYTES_PER_SAMPLE,fp);
  fclose(fp);
}

static void
write_orchestra_audio_file(char *orch_file) {
  FILE *fp;

  fp = fopen(orch_file,"wb");
  fwrite(orchdata,frames*TOKENLEN,BYTES_PER_SAMPLE,fp);
  fclose(fp);
}


void
read_orchestra_audio() {  /* assumes audio_file is set  (and frames too)*/
  FILE *fp;
  char orch_file[1000];

#ifdef WINDOWS
  strcpy(orch_file,audio_data_dir);  
  strcat(orch_file,current_examp);   
  strcat(orch_file,".ork");
#else
  get_orch_name(orch_file);
#endif

  fp = fopen(orch_file,"rb");
  if (fp == NULL) { printf("couldn't find %s\n",orch_file); return; }
  fread(orchdata,frames*TOKENLEN,BYTES_PER_SAMPLE,fp);
  fclose(fp);
}



void
make_current_examp(char *name, int n) {
  char num[50];

  strcpy(name,scoretag);
  strcat(name,".");
  sprintf(num,"%03d",n);
  strcat(name,num);
}



int audio_takes_exist() {
    char *name;
    struct dirent *de;
    DIR *dir;
    
   // strcpy(user_dir,audio_data_dir);
    
    dir = opendir(audio_data_dir);
    if (dir == NULL) return(0);
    while ((de = readdir(dir)) != NULL) {
        name = de->d_name;
        if (strcmp(name+strlen(name)-3,"raw") == 0) return(1);;
    }
    return(0);
}


static void
make_examp_name(int i, char *suff, char *file) {  // not tested
  char name[500];

  strcpy(file,audio_data_dir);
  make_current_examp(name, i);
  strcat(file,name);
  strcat(file,".");
  strcat(file,suff);
}

void
next_free_examp(char *name) { // set first examp where no .raw exists
  int i;
  char num[500];
  char stump[500];
  char file[500];
  FILE *fp;

  for (i=0; i < 1000; i++) {
    strcpy(file,audio_data_dir);
    make_current_examp(name, i);
    strcat(file,name);
    strcat(file,".raw");
    fp = NULL;
    fp = fopen(file,"r");
    //    printf("cur_examp = %s %d\n",file,fp);
    fclose(fp);
    if (fp) continue;

    /*    strcpy(file,audio_data_dir);  
    make_current_examp(name, i);
    strcat(file,name);
    strcat(file,".48k");
    fp = NULL;
    fp = fopen(file,"r");
    //    printf("cur_examp = %s %d\n",file,fp);
    fclose(fp);
    if (fp) continue;*/

    //    printf("i = %d fp = %d file = %s\n",i,fp,file);
    return;
  }
}

void
save_labelled_audio_file() {
  char stump[500];
  char af[500];
  char num[500];
  char s[500];
  int i,fd;
  FILE *fp;

  strcpy(stump,audio_data_dir);
  //  strcat(stump,"/");
  //  strcat(stump,scorename);
  strcat(stump,scoretag);
  strcat(stump,".");
  for (i=0; i < 1000; i++) {
    strcpy(audio_file,stump);
    sprintf(num,"%03d",i);
    strcat(audio_file,num);
    strcat(audio_file,".raw");
    fp = NULL;
    fp = fopen(audio_file,"r");
    if (fp) fclose(fp);
    else {
      make_current_examp(current_examp,i);  /* should use this above */
      /* and should use next_free_example */
      write_audio(audio_file);
      write_parse_stump();
#ifdef ORCHESTRA_EXPERIMENT
      if (mode != LIVE_MODE) { return; /*printf("wrong mode here\n"); exit(0);*/ }
      write_orchestra_audio();
#endif      
      break;
    }
  }
}



void
save_labelled_audio() {
  char file[500],ofile[500];
  char af[500];
  char num[500];
  char s[500];
  int i,fd;
  FILE *fp;

  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcpy(ofile,file);
  strcat(file,".raw");
  strcat(ofile,".ork");
  write_audio(file);
  //      write_parse_stump();
#ifdef ORCHESTRA_EXPERIMENT
  if (mode != LIVE_MODE) { return; /*printf("wrong mode here\n"); exit(0);*/ }
  write_orchestra_audio_file(ofile);
#endif      
}







static void
add_solo_example_num(char *num) {
  char name[500],f[2100];
  FILE *fp;
  int i;


  get_data_file_name(f, num, "atm");
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".sex");
  fp = fopen(name,"a");
  fprintf(fp,"%s\n",f);
  fclose(fp);
}

static void
new_add_example() {
  char name[500],f[2100];
  FILE *fp;
  int i;


  strcpy(name,score_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
  /*  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".ex");*/

  /*  strcpy(name,scorename);*/
  /*  strcat(name,".ex");*/

  fp = fopen(name,"a");
  fprintf(fp,"%s\n",audio_file);
  fclose(fp);

  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".sex");
  /*  strcpy(name,scorename);
      strcat(name,".sex");*/
  fp = fopen(name,"a");
  strcpy(f,audio_file);
  for (i=0; ; i++) if (f[i] == 0) break;
  f[i-3] = 0;  /* assuming filename ends in "raw" */
  strcat(f,"atm");
  fprintf(fp,"%s\n",f);
  fclose(fp);
}


void
save_audio_file_num(char *num) {
  char file[200];

  get_data_file_name(file, num, "raw");
  write_audio(file);
  write_parse_stump();
}


void
good_example() {  /* user indicated the performance was good */
  char num[500];

  printf("lastnote = %d\n",lastnote);
  set_audio_file_num_string(num);
  save_audio_file_num(num); 
  //  read_audio_indep();  /* just temporary !!!!! */
  parse();  
  if (failed) return;  /* or do something */
  // review_parse(0);      
  write_parse_num(num); 
  if (accomp_on_speakers) write_accomp_times_num(num);
  add_solo_example_num(num);
  bbn_train_composite_model_mem();
  
}



void 
finish_live_mode() {
  char a[10],b[10];
  void review_parse();

  //  printf("finish live mode entered\n");
  done_recording = 1;  // this duplicates the same in end_sampling 
  //  init_timer_list();  /* kills all current timer action */
  //  printf("killed timer list\n");
  //  cancel_outstanding_events();
#ifdef ORCHESTRA_EXPERIMENT
  //  performance_interrupted = 1;
    if (midi_accomp == 0) { /*end_playing();*/ /*end_duplex_audio(); */
        stop_apple_duplex();  // one of these functions will be null.  better to have a single function that changes depending on platform
       
        printf("ended duplex audio\n");
    }
  fflush(stdout);
#endif
  end_sampling();
  printf("ended sampling\n");
  frames = token;

  if (is_hires && mode == LIVE_MODE) close_io_buff(&orch_out);


#ifdef WINDOWS 
  return;
#endif


#ifdef RUNNING_GUI
  gui_learn_from_rehearsal();
  return;
#endif 
  print_resource_usage();
  printf("finishing real time mode\n");

  summarize_performance();
  printf("would you like to save the audio file?");
  scanf("%s",a);
  printf("would you like to write the parse?");
  scanf("%s",b);
  if (a[0] == 'n') return;
  save_labelled_audio_file(); 




  if (accomp_on_speakers) write_accomp_times();
  parse();  
  
  //   review_parse(0);   
   
  write_parse(); 
  if (b[0] == 'y') {
    new_add_example();
  }
  lag_stats();  
  show_token_hist();
  eval_accomp_note_times();
}


int
after_live_perform() {  
  int i;

    
  for (i=firstnote; i <= lastnote; i++) 
    score.solo.note[i].frames = score.solo.note[i].set_by_hand = 0;
  print_resource_usage();
  if (performance_interrupted == 0) summarize_performance();
   next_free_examp(current_examp);  /* sets global current_examp */
  save_labelled_audio(); 

    

    /*    save_hires_audio_files();*/

  /*if (accomp_on_speakers) */write_current_accomp_times();

  write_current_parse();  // why is this here and also a few lines down???

  write_current_hand_set_parms();

  if (parse() == 0) return(0);  
  if (failed) return(0);
  else write_current_parse();  
  //    new_add_example();
     lag_stats();  
  //  missed_note_thresh_stats();
  show_token_hist();
  eval_accomp_note_times();
  return(1);
}



static void 
temp_finish_live_mode() {
  char a[10];
  void review_parse();

  end_sampling();
  frames = token;
#ifdef RUNNING_GUI
  gui_learn_from_rehearsal();
  return;
#endif
  print_resource_usage();
  printf("finishing real time mode\n");
  summarize_performance();
  printf("would you like to save the audio file?");
  scanf("%s",a);
  if (a[0] == 'y') save_labelled_audio_file(); 
  parse();  
  //   review_parse(0);      

  printf("would you like to write the parse?");
  scanf("%s",a);
  if (a[0] == 'y') {
    write_parse(); 
    if (accomp_on_speakers) write_accomp_times();
    new_add_example();
  }
  lag_stats();  
  show_token_hist();
  eval_accomp_note_times();
}






#define INIT_GAUSS_VAR 1.
#define AGOGIC_VAR (.01*.01) //(.05*.05) //(.01*.01) //(.05*.05)  /* .01 works okay byt .001 messes up */
#define TEMPO_INCREMENT_FACTOR .01 //.05 // .01




NODEPTR replica_node(NODEPTR cur) {
  NODEPTR new;
  RATIONAL s;
  float l;
  
  new = alloc_node();
  *new = *cur;
  new->gs.frame = token;
  new->mom = cur;
  return(new);
}



static ONED_KERNEL
pos_dist(NODEPTR cur) {
  ONED_KERNEL temp;
  float l,tvar,durhat;

  l = score.solo.note[cur->gs.note].length;
  durhat = cur->gs.tempo.m * l;
  temp.m = token2secs(cur->gs.first_frame) + durhat;
  tvar = cur->gs.tempo.v + TEMPO_INCREMENT_FACTOR*l;
    temp.v = l*l*tvar + AGOGIC_VAR;
  //  temp.v = l*l*tvar + durhat*durhat*.005;
    if (durhat > 2.)  temp.v = l*l*tvar + (.1*.1); 
       else if (durhat > 1.);
    else if (durhat > .4)  temp.v = l*l*tvar + (.05*.05); 
  temp.h = 1;
  return(temp);
}




NODEPTR gauss_first_node(NODEPTR cur) {
  NODEPTR new;
  RATIONAL s;
  float l;
  



  new = alloc_node();
  new->gs.status = IN_NOTES;
  new->gs.parent = &(cur->gs);
  new->gs.note = firstnote;
  new->gs.first_frame = token;
  //  new->gs.tempo = perm_gk(univariate_gk(cur->gs.tempo.h,score.meastime,INIT_GAUSS_VAR));
  new->gs.tempo.h = cur->gs.tempo.h;
  new->gs.tempo.m = score.meastime;
  new->gs.tempo.m = get_whole_secs(score.solo.note[firstnote].wholerat);
  new->gs.tempo.v = INIT_GAUSS_VAR;
  new->gs.frame = token;
  new->gs.pos = pos_dist(new);
  new->gs.opt_scale = 1;
  new->mom = cur;
  return(new);
}


#define INIT_CFDP_DEN 2  /* looking at 1/2 notes */


static float
eval_gauss(ONED_KERNEL k, float v) {
  float d;

  d = v -k.m;
  //  return(exp(-.5 * d*d/k.v) / sqrt(2*PI*k.v));
  return(k.h*exp(-.5 * d*d/k.v)) ; // / sqrt(2*PI*k.v));
} 


NODEPTR 
gauss_next_node(NODEPTR cur) {
  NODEPTR new;
  RATIONAL s;
  int i;
  float obs_note_len,tvar,cov,pvar,l,esecs,old_tempo,zscore,asecs,start_secs,ll,m;
  //  MATRIX m,Q,A;
  

  s.num = 1; s.den = INIT_CFDP_DEN;
  new = alloc_node();
  new->gs.first_frame = token;
  new->gs.parent = &(cur->gs);
  new->mom = cur;
  new->gs.frame = token;
  if (cur->gs.note == lastnote) { 
    new->gs.status = AFTER_END; 
    new->gs.note = 0;
    //    new->gs.tempo = perm_gk(univariate_gk(cur->gs.tempo.h,0.,0.));
    new->gs.tempo.h = cur->gs.tempo.h;
    new->gs.opt_scale = 1;
    new->gs.tempo.m = 0;
    new->gs.tempo.v = 0;
    new->gs.note = -2;
    return(new); 
  }
  new->gs.status = IN_NOTES;
  new->gs.note = cur->gs.note+1;
  new->gs.first_frame = token;
  l = score.solo.note[cur->gs.note].length;
  tvar = cur->gs.tempo.v + TEMPO_INCREMENT_FACTOR*l;
  cov = l*tvar;
  //  pvar = l*l*tvar + AGOGIC_VAR;
  pvar = cur->gs.pos.v;
  old_tempo = cur->gs.tempo.m;
  //  esecs = token2secs(cur->gs.first_frame) + old_tempo*l;
  esecs = cur->gs.pos.m;


  asecs = token2secs(token);
  new->gs.tempo.m = old_tempo + (cov/pvar)*(asecs-esecs);
  new->gs.tempo.v = tvar - cov*cov/pvar;
  /*    new->gs.tempo.h = cur->gs.tempo.h * 
	exp(-.5 * (asecs - esecs)*(asecs - esecs)/pvar) / sqrt(2*PI*pvar);*/
  new->gs.opt_scale = 1;
  new->gs.tempo.h = cur->gs.tempo.h * 
        eval_gauss(cur->gs.pos,asecs);
  //      exp(-.5 * (asecs - esecs)*(asecs - esecs)/pvar);

  /* the denominator is correct, but this scales the hypotheses so that
     different note positioned ones can be compared better */
  new->gs.pos = pos_dist(new);

  //  printf("len = %f\n",l);
  //  printf("token = %d note = %d start = %d exp_len = %f actual = %f expected = %f h = %f m = %f\n",token,
  //	 cur->gs.note,cur->gs.first_frame,old_tempo*l,asecs,esecs,new->gs.tempo.h,new->gs.tempo.m );
      /*  for (i=0; i < 100; i++) {
    asecs = token2secs(i);
    new->gs.tempo.h = cur->gs.tempo.h * 
      exp(-.5 * (asecs - esecs)*(asecs - esecs)/pvar) / sqrt(2*PI*pvar);
    new->gs.tempo.m = old_tempo + (cov/pvar)*(asecs-esecs);
    printf("i = %d asecs = %f esecs = %f h = %f m = %f\n",i,asecs,esecs,new->gs.tempo.h,new->gs.tempo.m);
  }
  exit(0);*/
  return(new);
}



NODEPTR age_node(NODEPTR cur) {
  NODEPTR new;
  RATIONAL s;
  float p,l,m;
  

    
  new = alloc_node();
  *new = *cur;
  new->gs.frame = token;
  new->mom = cur;
  p = token2secs(token);
  if (p >= cur->gs.pos.m) new->gs.opt_scale = eval_gauss(cur->gs.pos,p);
  return(new);
}


static void
dellist_no_free(i)  /*deletes element i from active list */
int i; {
  /*  free_node(active.el[i]);*/
  active.el[i] = active.el[--active.length];
}


static void
gauss_parse_branch() {
  NODEPTR cur=start,new;
  int i,ind,j,sc,k,basic,t;
  ARC_LIST next;
  RATIONAL w;
  float r;

  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    if (cur->gs.status == BEFORE_START) {
      new = replica_node(cur);
      if (new) addlist(new);
      new = gauss_first_node(cur);
      addlist(new);
    }
    else if (cur->gs.status == AFTER_END) {
      new = replica_node(cur);
      if (new) addlist(new);
    }
    else {
      //      new = replica_node(cur);
      new = age_node(cur);
      if (new) addlist(new);
      new = gauss_next_node(cur);
      if (new) addlist(new);
    }
    dellist_no_free(i);
  }
  //  dp_cfdp_pruning();
}


gauss_graph_print_tree() {
  NODEPTR cur=start,par;
  int i,j,shade,k;
  char s[200],type[100],chrd[200],*sp;


  strcpy(s,"start");
  printf("\n token = %d nodes = %d\n",token,active.length);
  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    printf("%d\tstat = %d\tnote = %d\ttag=%s\tst=%d\th=%f (%e)\tmean=%f\tq=%f\topt_scale=%f\n",
	   i,
	   cur->gs.status,
	   cur->gs.note,
	   score.solo.note[cur->gs.note].observable_tag,
	   cur->gs.first_frame,
	   cur->gs.tempo.h,
	   cur->gs.tempo.h,
	   //	   cur->gs.tempo.m.el[0][0],
	   //	   cur->gs.tempo.Q.el[0][0]);
	   cur->gs.tempo.m,
	   cur->gs.tempo.v,
	   cur->gs.opt_scale);
  }
  printf("\n");
}

static void
gauss_enforce_hand_set_constraints() {
  int i,k;
  NODEPTR cur;


  for (k=firstnote; k <= lastnote; k++) 
    if (score.solo.note[k].set_by_hand && (score.solo.note[k].frames == token)) break;
  if (k > lastnote) return;
  
   printf("token = %d note = %s index = %d\n",token,score.solo.note[k].observable_tag,k);
  
  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    //        if (k == 2) cur->gs.tempo.m = 4;
    if ((cur->gs.note != k) || (cur->gs.first_frame != token)) dellist_no_free(i);  
  }
  if (active.length == 0) { printf("couldn't match constraint\n"); exit(0); }
  //    gauss_graph_print_tree();
  //   exit(0);
}

#define WHOLESECS_STD_RAT 10.

static void
gauss_enforce_tempo_changes() {
  int i,k,n,j;
  NODEPTR cur;
  float w;



  
  for (i=active.length-1; i >= 0; i--) {
    cur = active.el[i];
    if (cur->gs.first_frame != token) continue;
    n = cur->gs.note;
    if (score.solo.note[n].tempo_change == 0) continue;
    w = score.solo.note[n].meas_size;
    cur->gs.tempo.m = w;
    w *= WHOLESECS_STD_RAT;
    //    cur->gs.tempo.v = w*w;
    //    printf("set wholesecs to %f\n",cur->gs.tempo.m);
  }
}



#define ATTACK_CONST .1 //2.
#define ENERGY_CONST 1.




static void
gauss_comp_score() {  
  NODEPTR cur=start;
  int na,j,s,i,st,n,l,shade,mp,lp,age,nn,index;
  float tree_like(),sa_class_like(),x,y,z,last_time,t;
  FULL_STATE fs;
  SOUND_NUMBERS *acc_nts,temp;
  SA_STATE sas,sas2;
  float *spect,*acc_spect;
  
  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    index = cur->gs.note;
    nn = (index < 0) ? 0 : score.solo.note[index].snd_notes.num;
    if (cur->gs.status == BEFORE_START || cur->gs.status == AFTER_END) n = -1;
    else n = cur->gs.note;
    age = token - cur->gs.first_frame;

    shade = (age == 0) ? ATTACK_STATE : ((age == 1) ? FALL_STATE : NOTE_STATE);
    if (cur->gs.status == BEFORE_START || cur->gs.status == AFTER_END || 
	score.solo.note[index].snd_notes.num == 0) shade = REST_STATE;


    acc_spect = frame2spect(token);

    sas = set_sa_state(acc_spect , index, shade, mp, lp, last_time, acc_nts,age);
    cur->gs.tempo.h *=  sa_class_like(sas);
    //        printf("n = %d score = %f\n",n,sa_class_like(sas));
  }
}

static void
gauss_unity_scale() {  /* scale so max is one */
  NODEPTR cur=start;
  float total,m;
  int i;

  m = 0;
  for (i=0; i < active.length; i++)  {
    cur = active.el[i];
    if (cur->gs.tempo.h > m) m = cur->gs.tempo.h;
  }
  if (m <= 0.) {
    printf("token = %d\n",token);
    printf("total unnormalized prob = %f active = %d\n",
	   total,active.length);
    failed = 1;
  }
  for (i=0; i < active.length; i++)  {
    cur = active.el[i];
    cur->gs.tempo.h /= m;
  }
} 
  

static int  
gausscomp(const void *na, const void *nb)  {
  NODEPTR *n1,*n2;
  int a,b;

  n1 = (NODEPTR *) na;
  n2 = (NODEPTR *) nb;
  
  if ((*n1)->gs.first_frame != (*n2)->gs.first_frame) 
    return((*n2)->gs.first_frame -(*n1)->gs.first_frame);
  return((*n2)->gs.note -(*n1)->gs.note);
  return(0);
}



static void
gauss_dom_graph_prune() { /* both dp cutoffs and low probability cutoffs */
  int i,j,count[5000],counta[5000];
  NODEPTR curi,curj;
  float vi,vj,d,v;


  /*  for (i=firstnote; i<= lastnote; i++) counta[i] = count[i] = 0;
  for (i=0; i < active.length; i++) {
    curi = active.el[i];
    if (curi->gs.first_frame != token) continue;  
    count[curi->gs.note]++;
    }*/

  qsort(active.el,active.length,sizeof(NODEPTR),gausscomp); 
  //  if (token == 500) {gauss_graph_print_tree(); exit(0); }
  for (i=active.length-1; i >= 0; i--) {
    curi = active.el[i];
    if (curi->gs.first_frame != token) continue;  /* only compare new notes */
    //    for (j=i-1; j >= 0; j--) {
    for (j=active.length-1; j >= 0; j--) {
      curj = active.el[j];
      if (curj->gs.first_frame != token) continue; 
      if (curj->gs.note != curi->gs.note) continue; /* only compare same notes */
      if (i == j) continue; 
      /*d = curi->gs.tempo.m - curj->gs.tempo.m;
	v = curj->gs.tempo.h * exp(-.5 * d * d / curj->gs.tempo.v);*/
      v = eval_gauss(curj->gs.tempo, curi->gs.tempo.m);
      if (v >= curi->gs.tempo.h) { dellist_no_free(i);  break; }  /* >= since both might be 0 */
    }
  }


  /*    for (i=0; i < active.length; i++) {
    curi = active.el[i];
    if (curi->gs.first_frame != token) continue;  
    counta[curi->gs.note]++;
  }
  qsort(active.el,active.length,sizeof(NODEPTR),gausscomp); 
  for (i=firstnote; i<= lastnote; i++) if (count[i]) {
    printf("before = %d after = %d\n",count[i],counta[i]);
  }
  if (token == 100) { gauss_graph_print_tree(); exit(0); }*/
}

#define MAX_GAUSS_HYPS  200 //1000 //200  /* need 1000 to match global opt on 09 */



static int  
althcomp(const void *na, const void *nb) {
  NODEPTR *n1,*n2;
  int a,b;

  n1 = (NODEPTR *) na;
  n2 = (NODEPTR *) nb;
  if ((*n1)->gs.alth > (*n2)->gs.alth) return(-1);
  if ((*n1)->gs.alth < (*n2)->gs.alth) return(1);
  return(0);
}


static void
gauss_prob_graph_prune() { 
  int i;
  NODEPTR cur;



  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    cur->gs.alth = cur->gs.tempo.h * cur->gs.opt_scale;
  }
  qsort(active.el,active.length,sizeof(NODEPTR),althcomp); 
  for (i=active.length-1; i >= MAX_GAUSS_HYPS; i--) dellist_no_free(i);    /* why not free here? */
  for (; i >= 0; i--) if (active.el[i]->gs.alth==0) dellist_no_free(i);  else break;  /* why not free here? */
  /* if 0-prob nodes stay can't prune the trellis effectively*/
}

static void
gauss_solo_gates() {
  NODEPTR cur=start;
  float total;
  int i,n,g,r,fr;


  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    n = cur->gs.note;
    if (n < firstnote || n > lastnote) continue;
    g = score.solo.note[n].gate;
    if (g == -1) continue;  /* usually this will happen */
    fr = (int) secs2tokens(score.midi.burst[g].actual_secs);
    if (token < fr) dellist_no_free(i);

    //    printf("gate = %d name = %s fr = %f token = %d\n",g,score.midi.burst[g].observable_tag,fr,token);
  }	
} 





static int  
activecomp(const void *na, const void *nb) {
  NODEPTR *n1,*n2;

  n1 = (NODEPTR *) na;
  n2 = (NODEPTR *) nb;
  return((*n2)->active - (*n1)->active);
}





static void
prune_trellis() {
  int i,before;
  NODE *n;

  
  before = node_buff.cur;
  for (i=0; i < node_buff.cur; i++) {
    n = node_buff.stack[i];
    n->active = 0;
  }
  for (i=0; i < active.length; i++) {
    n = active.el[i];
    while (n != NULL && n->active == 0) { n->active = 1; n = n->mom; }
  }
  /*    qsort(node_buff.stack,node_buff.cur,sizeof(NODEPTR),activecomp); 
  for (i=0; i < node_buff.cur; i++)  if (node_buff.stack[i]->active == 0) break;
  node_buff.cur = i;*/

  
    for (i=0; i < node_buff.cur; i++)  if (node_buff.stack[i]->active == 0) {
      node_buff.cur--;
      n = node_buff.stack[i];
      node_buff.stack[i] = node_buff.stack[node_buff.cur]; 
      node_buff.stack[node_buff.cur]=n; 
      i--;
    }
    //       printf("buffer pruned from %d to %d nodes\n",before,node_buff.cur);
}



static void
record_completion_time() {
  extern float listen_complete_time[];
  extern float token_time[];

  listen_complete_time[token] = now();
  //  printf("%d %f %f\n",token,token_time[token],listen_complete_time[token]);
}



void
dpiter() {    /* one dynamic programming iteration */
int i,n,listen_finished,callback_finished;
 float p,tm,nw,t1,t2;
char s[500];
TIMER_EVENT e;



/*if (token > -1) {
  printf("token = %d\n",token);
  printf("nodes = %d\n",node_buff.cur);
  graph_print_tree(); 
}
*/

/*if (token >= 685) { getchar(); getchar(); } */


  if (mode == FORWARD_MODE ) {
    //    if (token > 4000)    graph_print_tree();
    //        if ((token % 100)== 0)    graph_print_tree();
    //       if (token == 100) exit(0);
    //                          if (token == 1000) exit(0);
    //    if (token > 1465 && token < 1485)    graph_print_tree();                
    //    if (token >= 758 && token <= 798)  graph_print_tree();
    init_iter();
    get_hist();
	forward_branch();
	if (bug_flag) graph_print_tree();
	//	    graph_print_tree();                
    //    if (token < 345 && token > 335) graph_print_tree();          
    //    if ((token % 100)== 0)    graph_print_tree();
    forward_solo_gates();  // maybe needs to be more lenient
    graph_comp_score(); 
    unity_scale();
/*num2name(score.solo.note[active.el[0]->place->note].num,s); 
printf("iter = %d note = %d %s\n",token,active.el[0]->place->note,s);   */
    graph_prune();
    state_probs();
/*locate_place(&n,&p);
printf(" token = %d n = %d p = %f\n",token,n,p); */
    checkdone();
    save_hist();
 
    return; 
  }

  if (mode == GAUSS_DP_MODE ) {
    //    qsort(active.el,active.length,sizeof(NODEPTR),gausscomp); 
    //     gauss_graph_print_tree();   
    //    printf("active.length = %d buff  at %d\n",active.length,node_buff.cur);
    //        if ((token % 50) == 0)	gauss_graph_print_tree();
    //    if ((token % 100) == 0)	printf("nodes = %d\n",active.length);
	//	if (token == 100) exit(0);
	// 	if (token == 100) exit(0);
	//	    if (token == 200) exit(0);
    init_iter();
    gauss_parse_branch();
    gauss_solo_gates();
    gauss_enforce_hand_set_constraints();
    gauss_enforce_tempo_changes();
    //    if (token == 91) exit(0);

    gauss_comp_score();

    gauss_unity_scale();
    gauss_dom_graph_prune();
    gauss_prob_graph_prune();
    if ((token % 100) == 0)	prune_trellis();
	//	if ((token % 100) == 0)	prune_trellis();
    //	add_cfdp_states();
    checkdone();
    return;
  }

  if (mode == PARSE_MODE ) {

    init_iter();
/*if (token > 470 && token < 490) graph_print_tree();               */

/*    samples2data;*/
    parse_branch();
    graph_comp_score();
    unity_scale();
    graph_prune();
    printf("hllo\n");
    graph_print_tree();
    checkdone();
    return;
  }
  if (mode == BACKWARD_MODE ) {
/*if (token < 150) graph_print_tree();          */
    init_iter();
    weed_out();
    back_state_probs();
    get_hist();
    graph_comp_score(); 
    backward_branch();
    unity_scale();
    graph_prune();
    checkdone();
    save_hist();
    //    if (token < 345 && token > 335) graph_print_tree();          
    return;
  }
  //CF:  Live performance.  
  //CF:  if synth mode (prerec solo) we have to artifically wait before next frame to make it real time
  if (mode == SYNTH_MODE || mode == LIVE_MODE) {
      //            printf("dpiter at %f\n",now());
    //    if (performance_interrupted) { premature_end_action(); return; }
    //    printf("token = %d\n",token);
    readtoken(); /* reads input audio to global 'audiodata' increments token also */
    calc_features(); 
    listen_finished = new_bridge();   
    if (/*token == 400 || */done_with_callbacks(listen_finished)) {

      printf("dp finished\n");
      if (mode == SYNTH_MODE) { finish_synth_mode(); return;}
      if (mode == LIVE_MODE)  {   done_recording = 1;  /*finish_live_mode(); */ return; }
    }
    record_completion_time();
#ifdef WINDOWS  
  return;  /* windows correction */
#endif

/*    tm = next_dp_time();*/
    //CF:  create and schedule a request to be called back when the next frame
    //CF:  is ready to be HMM-processed.
  
    tm = frame_samples_available_sec(token+1);
    nw = now();

    e = make_timer_event(PROCESSING_TOKEN,PROCESSING_TOKEN,token,(nw > tm) ? nw : tm);

    /*    e.priority = e.type = PROCESSING_TOKEN;
    e.time = (nw > tm) ? nw : tm;
    e.id = token;*/

    
/*    if (nw > tm) printf("scheduling right away\n");*/
    set_timer_event(e);
    return;
  }
  if (mode == SIMULATE_MODE || mode == SIMULATE_FIXED_LAG_MODE) {

    init_iter(); 
    new_bridge();   
    checkdone();
    return;
  }
} 


/*
static void
myaddclick() {
 
 int i,nf;
 static wolf = 100;
 char ans[10],file[200],s[200],command[500];
 int fd;
  
  for (i=firstnote; i <= lastnote; i++) {
   audiodata[(notetime[i]*TOKENLEN)] = 0;    
 }



  strcpy(file,audio_file);
  strcat(file,".click");
  fd = creat(file,0755);
  nf = (mode == LIVE_MODE) ? token : frames;
  write(fd,audiodata,frames*TOKENLEN);
  strcpy(s,"/d4m/craphael/disk/space/projects/sound/suntools/raw2audio ");
  strcat(s,file);
  system(s);
  return;
  while (1) { 
    printf("want to hear click file? ");
    scanf("%s",ans);
    if (ans[0] == 'y') {
      strcpy(command,"play ");
      strcat(command,scorename);
      strcat(command,".click");
      system(command);
    }
    else return;
  }
}

*/



int
get_parse_type(char *s) {
  FILE *fp;
  int i,tab=0;
  char c,str[500];

  fp = fopen(s,"r");
  if (fp == 0) return(-1);  // failed
  for (i=0; i < 4; i++ ) while (fgetc(fp) != '\n') if (feof(fp)) {
  // file doesn't have any times
    fclose(fp);
    printf("type of %s = %d\n",s,0);
    return(0);
  }
  while ((c=fgetc(fp)) != '\n') {
    if (feof(fp)) return(1);  // no note times so could call it either type
    //  printf("c = %d %c tab = %c\n",c,c,'\t');
    if (c == '\t') tab++;
  }
  fclose(fp);
  printf("type of %s = %d\n",s,tab);
  return(tab);
}

int
is_on_speakers() {  // was the current audio file recorded with orch on speakers?
  char file[300],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,fn,ln,j=0;
  float time,type;

  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".ork");
  fp = fopen(file,"r");
  fclose(fp);
  if (fp == 0)  return(0);   // no .ork file so can't be on speakers


#ifdef WINDOWS
  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".times");
#else
  get_parse_name(file);
#endif
  fp = fopen(file,"r");
  if (fp == NULL) return(0);  // really don't know here, but aviod crash
  while (!feof(fp)) {
    fscanf(fp,"%s",name);
    if (strcmp(name,"accomp_on_speakers") != 0) continue;
    fscanf(fp,"%s %d",name,&j);
    return(j);
  }
  return(1);  // default if unknown
}




int
/*newish_*/read_parse() {
  char file[300],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,fn,ln,j=0,k,i0;
  float time,type;

  for (i=0; i < score.solo.num; i++) {
    score.solo.note[i].set_by_hand = 0;
    score.solo.note[i].frames = 0;
  }
#ifdef WINDOWS
  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".times");
  for (i=0; i < score.solo.num; i++) score.solo.note[i].frames = 0;
#else
  get_parse_name(file);
#endif

  type = get_parse_type(file);
  if (type == -1) { printf("bad file %s\n",file);  return(0); exit(0); }
  fp = fopen(file,"r");
  printf("opening %s\n",file); 
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    return(0);
  }
  start_meas = end_meas = HUGE_VAL;
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    return(0);
  }
  start_meas = meas2score_pos(start_string); 
  start_pos  = string2wholerat(start_string);
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    return(0);
  }
  end_pos  = string2wholerat(end_string);
  end_meas = meas2score_pos(end_string); 
  
  //  printf("end_string = %s, end_pos = %d/%d\n",end_string,end_pos.num,end_pos.den);
  set_range();
  store_take_state();
  if (feof(fp)) {
    fclose(fp);
    return(1);  /* parse file doesn't have times --- this is ok */
  }
  fscanf(fp,"firstnote = %d\n",&fn);
  fscanf(fp,"lastnote = %d\n",&ln);
  //  for (i=firstnote; i <= lastnote; i++) {
  i0 = firstnote;
  while (1) {
    if (type == 2) fscanf(fp,"%s %f %d",tag,&time,&j); //use this to read new style
    else fscanf(fp,"%s %f",tag,&time);
    if (feof(fp)) break;
    for (i=i0; i <= lastnote; i++) 
      if (strcmp(tag,score.solo.note[i].observable_tag) == 0) break;
    if (i > lastnote) continue;
    i0 = i;  // last found note
    score.solo.note[i].realize = time;
    score.solo.note[i].frames = (int) (time + .5);
    score.solo.note[i].saved_frames = (int) (time + .5);
    score.solo.note[i].off_line_secs = toks2secs(time);
    score.solo.note[i].set_by_hand = j;
  }
  fclose(fp); 
  return(1);
}





int
from_new_computer_read_parse() {
  char file[300],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,fn,ln,j=0;
  float time,type;

#ifdef WINDOWS
  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".times");
  for (i=0; i < score.solo.num; i++) score.solo.note[i].frames = 0;
#else
  get_parse_name(file);
#endif

  type = get_parse_type(file);

  if (type == -1) { printf("bad file %s\n",file);  return(0); exit(0); }



  fp = fopen(file,"r");
  printf("opening %s\n",file); 
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    //    exit(0);   
    return(0);
  }
  start_meas = end_meas = HUGE_VAL;
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    return(0);
    exit(0);
  }
  start_meas = meas2score_pos(start_string); 
  start_pos  = string2wholerat(start_string);
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    return(0);
    exit(0);
  }
  end_pos  = string2wholerat(end_string);
  end_meas = meas2score_pos(end_string); 
  
  //  printf("end_string = %s, end_pos = %d/%d\n",end_string,end_pos.num,end_pos.den);
  /*  printf("end_string = %s end_pos = %d/%d\n",end_string,end_pos.num,end_pos.den);*/


  /*  fscanf(fp,"start = %f\n",&start_meas);
      fscanf(fp,"end = %f\n",&end_meas); */

  /*  if (start_meas == HUGE_VAL || end_meas == HUGE_VAL) {
    printf("couldn't understand the .times file\n");
    exit(0);
    } */

  set_range();
  if (feof(fp)) {
    fclose(fp);
    for (i=0; i < score.solo.num; i++) score.solo.note[i].set_by_hand = 0;
    //    return(0);  /* parse file doesn't have times */
    return(1);  /* parse file doesn't have times --- this is ok */
  }
  fscanf(fp,"firstnote = %d\n",&fn);
  fscanf(fp,"lastnote = %d\n",&ln);
  if (fn != firstnote || ln != lastnote) {
    printf("fn = %d firstnote = %d ln = %d lastnote = %d\n",fn,firstnote,ln,lastnote);
    printf("ln --> %s last_note --> %s\n",score.solo.note[ln].observable_tag, score.solo.note[lastnote].observable_tag); 
    printf("Attention: score is inconsisent with parse file (000) (%s) --- parse data again\n",file);
    //    exit(0);
    return(0);
    firstnote = fn;
    lastnote = ln;
    /*    fclose(fp);
     exit(0);
     return(0);*/
  }
  for (i=fn; i <= ln; i++) {
    /*    fscanf(fp,"%d %s %f",&n,name,&time); */
    if (type == 2) fscanf(fp,"%s %f %d",tag,&time,&j); //use this to read new style
    else fscanf(fp,"%s %f",tag,&time);

    /*    num2name(score.solo.note[i].num,alt); */
    /*    if (strcmp(alt,name) != 0) {*/
    if (strcmp(tag,score.solo.note[i].observable_tag) != 0) {
      printf("%s != %s\n",tag,score.solo.note[i].observable_tag);
      printf("score inconsistent with parse file (11) %s --- parse again\n",file);
      fclose(fp);
      return(0);
    }
    score.solo.note[i].realize = time;
    score.solo.note[i].frames = (int) (time + .5);
    score.solo.note[i].saved_frames = (int) (time + .5);
    score.solo.note[i].off_line_secs = toks2secs(time);
    score.solo.note[i].set_by_hand = j;
    //    printf("note %d at %d\n",i,score.solo.note[i].frames);
    //    printf("%s %f\n",score.solo.note[i].observable_tag,score.solo.note[i].off_line_secs);
    /*    printf("%s %f\n",tag,time);
	  printf("%s %f\n",score.solo.note[i].observable_tag,score.solo.note[i].off_line_secs);*/
    /*    printf("read %d at %f\n",i, score.solo.note[i].off_line_secs);*/
  }
  /*  printf("parse file okay\n");*/
  fclose(fp); 
  return(1);
}




int
simple_read_parse(int *fn, int *ln) { /* doesn't set firstnote and lastnote */
  char file[300],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,j;
  float time;

#ifdef WINDOWS
  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".times");
#else
  get_parse_name(file);
#endif

  fp = fopen(file,"r");
  printf("opening %s\n",file); 
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    exit(0);   
  }
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    exit(0);
  }
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    exit(0);
  }
  if (feof(fp)) {
    fclose(fp);
    return(0);  /* parse file doesn't have times */
  }
  fscanf(fp,"firstnote = %d\n",fn);
  fscanf(fp,"lastnote = %d\n",ln);
  for (i=*fn; i <= *ln; i++) {
    fscanf(fp,"%s %f",tag,&time);
    if (strcmp(tag,score.solo.note[i].observable_tag) != 0) {
      printf("%s != %s\n",tag,score.solo.note[i].observable_tag);
      printf("score inconsistent with parse file (2) %s --- parse again\n",file);
      fclose(fp);
      return(0);
    }
    score.solo.note[i].realize = time;  
    score.solo.note[i].frames = (int) (time + .5);
    score.solo.note[i].off_line_secs = toks2secs(time); 
  }
  /*  printf("parse file okay\n");*/
  fclose(fp); 
  return(1);
}





int
read_parse_arg(char *file) {
  char name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,fn,ln,j;
  float time;

  fp = fopen(file,"r");
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    exit(0);   
  }
  start_meas = end_meas = HUGE_VAL;
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    return(-1);
  }
  start_meas = meas2score_pos(start_string); 
  start_pos  = string2wholerat(start_string);
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    return(-2);
  }
  end_pos  = string2wholerat(end_string);
  end_meas = meas2score_pos(end_string); 
  set_range();
  if (feof(fp)) {
    fclose(fp);
    return(0);  /* parse file doesn't have times */
  }
  fscanf(fp,"firstnote = %d\n",&fn);
  fscanf(fp,"lastnote = %d\n",&ln);

  if (fn != firstnote || ln != lastnote) {
    printf("fn = %d firstnote = %d ln = %d lastnote = %d\n",fn,firstnote,ln,lastnote);
    printf("ln --> %s last_note --> %s\n",score.solo.note[ln].observable_tag, score.solo.note[lastnote].observable_tag); 
    printf("score is inconsisent with parse file (111) (%s) --- parse data again\n",file);
    return(-3);
    firstnote = fn;
    lastnote = ln;
  }
  for (i=firstnote; i <= lastnote; i++) {
    fscanf(fp,"%s %f",tag,&time);

    if (strcmp(tag,score.solo.note[i].observable_tag) != 0) {
      printf("%s != %s\n",tag,score.solo.note[i].observable_tag);
      printf("score inconsistent with parse file (3) %s --- parse again\n",file);
      fclose(fp);
      return(-4);
    }
    score.solo.note[i].realize = time;
    score.solo.note[i].frames = (int) (time + .5);
    score.solo.note[i].off_line_secs = toks2secs(time);
    /*    printf("%s %f\n",tag,time);
	  printf("%s %f\n",score.solo.note[i].observable_tag,score.solo.note[i].off_line_secs);*/
  }
  fclose(fp); 
  return(1);
}



int
read_parse_exper() {
  char file[200],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,fn,ln,j,l;
  float time,t;

  get_parse_name(file);
  fp = fopen(file,"r");
  /*  printf("opening %s\n",file); */
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    exit(0);   
  }
  start_meas = end_meas = HUGE_VAL;
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    exit(0);
  }
  start_meas = meas2score_pos(start_string); 
  start_pos  = string2wholerat(start_string);
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    exit(0);
  }
  end_pos  = string2wholerat(end_string);
  end_meas = meas2score_pos(end_string); 

  /*  printf("end_string = %s end_pos = %d/%d\n",end_string,end_pos.num,end_pos.den);*/


  /*  fscanf(fp,"start = %f\n",&start_meas);
      fscanf(fp,"end = %f\n",&end_meas); */

  /*  if (start_meas == HUGE_VAL || end_meas == HUGE_VAL) {
    printf("couldn't understand the .times file\n");
    exit(0);
    } */

  set_range();
  if (feof(fp)) {
    fclose(fp);
    return(0);  /* parse file doesn't have times */
  }
  fscanf(fp,"firstnote = %d\n",&fn);
  fscanf(fp,"lastnote = %d\n",&ln);
  /*  if (fn != firstnote || ln != lastnote) {
    printf("fn = %d firstnote = %d ln = %d lastnote = %d\n",fn,firstnote,ln,lastnote);
    printf("score is inconsisent with parse file (0) %s --- parse data again\n",file);
    fclose(fp);
    return(0);
    }*/
  while (1) { //for (i=firstnote; i <= lastnote; i++) {
    /*    fscanf(fp,"%d %s %f",&n,name,&time); */
    fscanf(fp,"%s %f",tag,&time);
    if (feof(fp)) break;

    for (i=0; i <= score.solo.num; i++)  /* could be more efficient*/
      if (strcmp(tag,score.solo.note[i].observable_tag) == 0) break;
    /*    if (i < score.solo.num) {
      score.solo.note[i].realize = time;
      score.solo.note[i].off_line_secs = toks2secs(time);
      l = score.solo.note[i].length_hist.num++;
      score.solo.note[i].length_hist.secs[l] = toks2secs(time);
      } */
  }
  //      printf("%s != %s\n",tag,score.solo.note[i].observable_tag);
  //      printf("score inconsistent with parse file (4) %s --- parse again\n",file);
  fclose(fp);
  return(1);
}




int
read_parse_accomp() {
  char file1[500];
  FILE *fp1;
  int i,j,k=0,an,aux[MIDI_PITCH_NUM],n,p,type,first_set,not_found=0;
  float atime;
  MIDI_EVENT_LIST nt_evt;
  unsigned char com,vol;
  char tag[500];

  // initialize current sounding notes
  first_accomp = last_accomp = 0;
#ifdef WINDOWS
  strcpy(file1,audio_data_dir);  
  strcat(file1,current_examp);   
  strcat(file1,".acc");
#else
  get_accomp_times_name(file1);
#endif


  for (i=0; i < score.midi.num; i++) 
    score.midi.burst[i].actual_secs = score.midi.burst[i].recorded_secs = 0;  
/* 0 because the accomp that we have times for will be set later in 
   this routine.  others should be treated as in the past so they 
   won't gate anything */
  type = file_type(file1);  // counts number of tabs per line
  fp1 = fopen(file1,"r");
  if (fp1 == NULL) {
    printf("couldn't find %s\n",file1);
    //    score.midi.burst[0].actual_secs = HUGE_VAL;  /* last note long way in past */
    return(0);
  }


  first_set = 0;
  while (1) {
    if (type == 0) fscanf(fp1,"%d %f",&an,&atime);
    else  fscanf(fp1,"%s %f",tag, &atime);
    if (feof(fp1)) break;
    //    printf("%s %f\n",tag,atime);
    if (type == 0);
    else {
            not_found =1;
      for (an=0; an < score.midi.num; an++) {
	if (strcmp(score.midi.burst[an].observable_tag,tag) == 0) break;
      }
      if (an < score.midi.num)  not_found = 0; 
    }
    if (not_found) continue;  
    score.midi.burst[an].actual_secs = score.midi.burst[an].recorded_secs = atime;
     score.midi.burst[an].frames =  secs2tokens(atime);  // 4-13

    //    printf("%d %f\n",an,atime);
      if (first_set == 0) { first_set = 1;  first_accomp = an; }
  }
  if (first_set == 0) { first_accomp = last_accomp = 0; }
  else last_accomp = an;
  fclose(fp1);
  
  //      printf("first = %d last = %d\n",first_accomp,last_accomp); exit(0);

  //    write_accomp_times(); exit(0);


  return(1);  // this is the new way as of 11-06
			  



  // if (feof(fp1)) return(0);  /* parse file doesn't have times */
  fp1 = fopen(file1,"r");
  fscanf(fp1,"%d %f",&an,&atime);
  first_accomp = an;
  score.midi.burst[an].actual_secs = atime;
  while ((fscanf(fp1,"%d %f",&an,&atime)) != EOF) {
    score.midi.burst[an].actual_secs = atime;
    //    printf("an = %d time = %f\n",an,atime);
  }
  last_accomp = an;

#ifdef POLYPHONIC_INPUT_EXPERIMENT   /* I think I always want to return here*/
  return(1);
#endif


  // doesn't the remainder already happen in set_sounding_notes?

  // initialization of the aux array to zero

  for (i=0; i<MIDI_PITCH_NUM; i++) aux[i]=0;
  
  // finding the current sounding notes

  for (j = first_accomp; j <= last_accomp; j++) {
    nt_evt = score.midi.burst[j].action;
    for (i = 0; i < nt_evt.num; i++) {
      n = nt_evt.event[i].notenum;
      com = nt_evt.event[i].command;
      vol = nt_evt.event[i].volume;
      if ((com & 0xf0) == NOTE_ON && vol > 0) aux[n] = 1;
      else if (com == PEDAL_COM); 
      else if ((com & 0xf0) == NOTE_OFF ||  vol == 0) {
	aux[n] = 0;
      }
      else printf("unknown midi command %x\n",nt_evt.event[i].command);
    } // aux gives the indexes to all currently sounding notes
    k=0;
    for (p=0; p<MIDI_PITCH_NUM; p++) {
      if (aux[p] == 1) {
	score.midi.burst[j].snd_notes.snd_nums[k] = p;
	++k;
      }
    }
    score.midi.burst[j].snd_notes.num = k;
  }
  return(1);
}












static void
put_in_click(float secs) {
  int click,i;

  click = (int) (secs*SR*2);
  for (i=0; i < 2; i++) audiodata[click+i]= 100;
}

static int
is_quarter_note(RATIONAL r) {
  return(((4*r.num) % r.den) == 0);
}


void      
review_parse(int start_frame) {
 
 int i,click;
 static wolf = 100;
 char ans[10],file[200],s[200],command[500];
 int fd;

/* save_labelled_audio_file(); */
 printf("playing audio buffer \n");
 for (i=firstnote; i <= lastnote; i++) {
   /*if (score.solo.note[i].num != RESTNUM) */
   /*   audiodata[(notetime[i]*TOKENLEN)] = 0;    */
   click = (int) (score.solo.note[i].off_line_secs*SR);
   printf("note %d at %f\n",i,secs2tokens(score.solo.note[i].off_line_secs));
   /*    audiodata[click] = 0;    */
   /*               put_in_click(score.solo.note[i].off_line_secs - .032);  */
   if (is_a_rest(score.solo.note[i].num) == 0) 
     /*              put_in_click(score.solo.note[i].off_line_secs - .032);  */
     //if (is_quarter_note(score.solo.note[i].wholerat))    
     put_in_click(score.solo.note[i].off_line_secs);  
 }
 // play_audio_buffer();
 play_audio_buffer_one_channel(start_frame);  /* first frame */
}





/*
static void
note_event(time,missed_note) 
float time;
int missed_note;  {
  int i;
  SOLO_NOTE *nt;
extern float cur_secs;

  if (cur_note < firstnote) return;
  nt = &score.solo.note[cur_note];
  nt->realize = (missed_note) ? HUGE_VAL : time;
  if (nt->cue) new_cue_phrase(time);
  else if (missed_note) blind_kalman_update();
  else new_kalman_update(time);
  cur_note++;  
  predict_future(10,10);  
  if (nt->cue) cue.cur++;
  for (i=0; i < HIST_LENGTH; i++) start_window[i]=0;
}

*/



/*

 strcpy(file,"temp.au");
 fd = creat(file,0755);
 write(fd,audiodata,frames*TOKENLEN);
 close(file);
 strcpy(s,"/usr/demo/SOUND/raw2audio ");
 strcat(s,file);
 system(s);  
 while (1) { 
   printf("want to hear click file? ");
   scanf("%s",ans);
   if (ans[0] == 'y') {
     strcpy(command,"/usr/demo/SOUND/play ");
     strcat(command,file);
     system(command);
   }
   else break;
 }
}
*/
      



dp() {
  NODEPTR best,cur;
  char ans[10],command[500];
  int p,i=0,success=0,accomp_done;
  TIMER_EVENT e;



  if (mode == SYNTH_MODE || mode == LIVE_MODE) {
    e.priority = e.type = PROCESSING_TOKEN;
    /*    e.time = tokens2secs(1.); /* end time of 1st token */
    e.time = frame_samples_available_sec(0);
    e.id = token;
    set_timer_event(e);
  }
    /*    while (lasttoken == 0 && failed == 0) {     */
    while (dp_finished() == 0) {     
      /*printf("nodes used = %d\n",node_buff.cur); */
      /*      if (mode == FORWARD_MODE)  timer_position = (int) (50*(token/(float)frames));
	      if (mode == BACKWARD_MODE) timer_position = 100- (int)(50*token/(float)frames);*/

      if (mode == FORWARD_MODE)  background_info.fraction_done =  .5*token/(float)frames;
      if (mode == BACKWARD_MODE) background_info.fraction_done = 1. - .5*token/(float)frames;




      if (mode == SYNTH_MODE || mode == LIVE_MODE)  pause(); 
    else  dpiter();



    
     if (token%50 == 0 && mode != SYNTH_MODE &&  mode != LIVE_MODE) 
     printf("token = %d time = %f\n",token,tokens2secs((float)token));    
     /*          graph_print_tree();                 */
 } 
 background_info.fraction_done = 1 ;

     printf("xxx token = %d frames = %d cur_accomp = %d last_accomp = %d cur_note = %d lastnote = %d\n",token,frames,cur_accomp,last_accomp,cur_note,lastnote);

     accomp_done = (cur_accomp == score.midi.num || cur_accomp > last_accomp);
   if (mode == SYNTH_MODE || mode == LIVE_MODE)  {/* wait until midi playing done */
     //      while (cur_accomp <= last_accomp) pause();
     printf("accomp_done = %d\n",accomp_done);
      while (accomp_done == 0) pause();
   }


  if (mode == LIVE_MODE) frames = token;

  print_resource_usage();
}


void
dp_live() {
  NODEPTR best,cur;
  char ans[10],command[500];
  int p,i=0,success=0,accomp_done;
  TIMER_EVENT e;


  if (mode != SYNTH_MODE && mode != LIVE_MODE) { 
    printf("only call dp_live with SYNTH_MODE or LIVE_MODE\n"); exit(0);
  }

  e.priority = e.type = PROCESSING_TOKEN;
  e.time = frame_samples_available_sec(0);
  e.id = token;
  set_timer_event(e);
  return;  /* high level control is higher level function  */

  /****************************************************/ //CF:  rest of this fn not used!

  /*    while (lasttoken == 0 && failed == 0) {     */

  while (dp_finished() == 0) { pause(); printf("pausing\n"); }



  printf("token = %d frames = %d cur_accomp = %d last_accomp = %d cur_note = %d lastnote = %d\n",token,frames,cur_accomp,last_accomp,cur_note,lastnote);

  accomp_done = (cur_accomp == score.midi.num || cur_accomp > last_accomp);

  //   if (mode == SYNTH_MODE)  while (cur_accomp <= last_accomp) pause();
   if (mode == SYNTH_MODE)  while (accomp_done == 0) pause();
   /* wait until midi playing done */
   if (mode == LIVE_MODE) frames = token;

  print_resource_usage();
}


void
async_dp() {
  NODEPTR best,cur;
  char ans[10],command[500];
  int p,i=0,success=0;
  float now(),next,nw;

  nw = now();
  next =  next_dp_time();
  if (nw  < next){
    printf("too early in async_dp()\n");
    printf("token = %d now = %f next = %f\n",token,nw,next);
    exit(0);
  }
  /*  printf("token = %d now = %f\n",token,now()); */
  if  (lasttoken == 0 && failed == 0) {     
    /*    printf("now = %f next = %f\n",now(),next);*/
    while (now() >= next_dp_time()) {
      /*      printf("now = %f next = %f\n",now(),next_dp_time());*/
      dpiter();
    }
    add_async(next_dp_time()+.01,DP_ASYNC_ID);
  } 
  else  print_resource_usage();
}

void
async_dp_wrap() {
  /*  async_dp(); */
  add_async(tokens2secs(1.5), DP_ASYNC_ID);
  while (lasttoken == 0) pause();
}



static void
rtinit(m)  
int m;  { /* real time dp init */
  int i;
  NODEPTR start;
  extern int cur_event;

  audio_skew = 0;
  performance_interrupted = 0;
  init_node_stack();
  mode = m;
  maxnodes = MAXNODESRT;
  cur_note = firstnote;
  for (i=0; i <=  MAX_LAG; i++ ) {
    lnk[i] = (ACTLIST *) malloc(sizeof(ACTLIST));
    lnk[i]->length=0;
    fwd[i] = (ACTLIST *) malloc(sizeof(ACTLIST));
    fwd[i]->length=0;
    bwd[i] = (ACTLIST *) malloc(sizeof(ACTLIST));
    bwd[i]->length=0;
  }
  token = -1;   /* alittle kludgy */
  lasttoken = 0;
  failed = 0;
  cue.cur = first_cue;
  cur_event = first_event;


  start =  alloc_node();
  start->prob = 1; 
  start->orig = start->place = start_node;
  add_list(fwd[0],start);
}

statinit() {
  int i;
  float power();


    token = -1;   /* alittle kludgy */
}



olinit()   { /* off line dp init */    
  int i;

  failed = 0;
  cur_note = 0;
/*  mode = TRAIN_MODE;*/
  maxnodes = MAXNODESOL;
  dpinit();
}


void
set_endpoints(float meas_start, float meas_end, int *note_start, int *note_end) {
    int i;

    /*    printf("start = %f end = %f\n",meas_start,meas_end);
	  exit(0); */


    i=0;
    while (score.solo.note[i].time < meas_start - .001) i++;
    *note_start = i;
    i=0;
      printf("i = %d time = %f end = %f\n",i,score.solo.note[i].time,meas_end);
    while (score.solo.note[i].time < meas_end - .001) {
      if (i == score.solo.num) break; else i++;
    }
    *note_end = i-1;
}


static void
set_dp_range() {
  int i;

  for (i=0; i < score.solo.num; i++) 
    if (rat_cmp(score.solo.note[i].wholerat,start_pos) >=  0) break;
  for (; i < score.solo.num; i++) 
    if (is_a_rest(score.solo.note[i].num) == 0) break;
  if (i == score.solo.num) {
    printf("out of range in set_dp_range\n");
    exit(0);
  }
  firstnote = i;
  for (i=score.solo.num-1; i >= 0; i--) 
    if (rat_cmp(score.solo.note[i].wholerat,end_pos) <=  0) break;
  lastnote = i;
}
  



set_range() {   
    float start,end,lead;
    int i;
    RATIONAL r;



    for (i=0; i < score.solo.num; i++)   // put this in 9-06
      if (rat_cmp(score.solo.note[i].wholerat,end_pos) > 0) break;
    lastnote = i-1;
    if (is_solo_rest(lastnote) && lastnote > 0) lastnote--;   


    /*    printf("enter start time of solo part  in measures: ");
    scanf("%s",start_string);
    sscanf(start_string,"%f",&start); */

    start = start_meas;

    if ( score.solo.note[score.solo.num-1].time < start)
      printf("start meas time not in score %f %f %d\n",start,score.solo.note[score.solo.num-1].time,score.solo.num); 
    i=0;
    /*    while (score.solo.note[i].time < start - .001) i++;*/
    while (rat_cmp(score.solo.note[i].wholerat,start_pos) < 0)   { 
      //	printf("%d/%d\n",score.solo.note[i].wholerat.num,score.solo.note[i].wholerat.den); 
	i++; 
    }
    firstnote = i;
    if (is_solo_rest(firstnote) && firstnote < lastnote) firstnote++;  // 2-20-06
        


    r = score.solo.note[firstnote].wholerat;

    /*    printf("enter end time in measures: ");
    scanf("%s",end_string);
    sscanf(end_string,"%f",&end); */

    end = end_meas;


    /*    if ( event.list[event.num-1].meas < end)
     	printf("end meas time not in score\n");*/

    /*    i=0;
    while (score.solo.note[i].time < end - .001) if (i == score.solo.num) break; else i++;
    lastnote = i-1;*/



    /* took this out 9-06 in favor of for loop for lastnote  above */
    /*    while (rat_cmp(score.solo.note[i].wholerat,end_pos) <= 0) {  
      //          printf("%s\n",score.solo.note[i].observable_tag);
      if (i == score.solo.num)  break; else i++;
    }
    lastnote = i-1;
    if (is_solo_rest(lastnote) && lastnote > 0) lastnote--;   */




    /* can't have two rests in a row at end */

    //            printf("last note is %s\n",score.solo.note[lastnote].observable_tag);


    //    for (i=0; i < score.solo.num; i++) 
    //          if (rat_cmp(score.solo.note[i].wholerat,end_pos) >= 0) break;
    //        lastnote = (i == score.solo.num) ? i-1 : i;
    
	       
    /*    r = score.solo.note[i].wholerat;
    printf("end_pos = %d/%d  score[last] = %d/%d\n",end_pos.num,end_pos.den,r.num,r.den);
    printf("lastnote  = %d i = %d\n",lastnote,i);
    printf("score.solo.num = %d\n",score.solo.num);*/
    /*    if (end_pos.num != 409) exit(0);*/
 

  /*    printf("enter the amout of accomp lead-in:");
    scanf("%f",&lead); */
    lead = 0;
    start = score.solo.note[firstnote].time-lead;
    first_event = 0;
    while (event.list[first_event].meas < start - .001) {
	first_event++;
	if (first_event == event.num) printf("no such event\n");
    }
    last_event = 0;
    while (event.list[last_event].meas < end - .001) {
	last_event++;
	if (last_event == event.num) break;
    }
    last_event--;

    /*    printf("firstnote = %d (%s) lastnote = %d (%s) \n",firstnote,score.solo.note[firstnote].observable_tag,lastnote,score.solo.note[lastnote].observable_tag);
        set_dp_range();  
	printf("firstnote = %d (%s) lastnote = %d (%s) \n",firstnote,score.solo.note[firstnote].observable_tag,lastnote,score.solo.note[lastnote].observable_tag); */


    /*    first_cue = 0;
printf("cue num = %d\n",cue.num);
    if (cue.num == 0) printf("no such cue\n");
    while (score.solo.note[cue.list[first_cue].note_num].time < start - .001) {
	first_cue++;
	if (first_cue == cue.num) printf("no such cue\n");
    } */
    /*    printf("start = %f end = %f firstnote = %d lastnote = %d\n",start,end,firstnote,lastnote);*/
}

static void
set_notetime() {
  float best_score;
  NODEPTR best,cur;
  int success = 0,i;

  best_score = -HUGE_VAL;
  for (i=0; i < active.length; i++) {
    cur = active.el[i];
    if ( (cur->prob > best_score)  && (cur->place == end_node) ) {
      best_score = cur->prob;
      best = cur;
      success = 1;
    }
  }
  if (success == 0) {
    printf("unsuccessful parse \n");
    best =active.el[0];
    graph_print_tree();
  }
  set_beat(best);
}


void
rehearse() {
  char a[10],file[500];
  int n=0,flag;

  set_range();
  if (readaudio() == 0) {   /* keeps this in for articial sampling */
    strcpy(scorename,file); 
    printf("problem in readaudio()\n"); 
    return; 
  }
/*  read_trees(); */
/*   mode = SYNTH_MODE; */
  make_dp_graph(); 
 
/* do accomp thing */

/*  forward_backward();
  mean_parse(); 
    review_parse(0);
  printf("add example to training? ");
  scanf("%s",a);
  if (a[0] == 'y')      add_example();
  /*return; */


  rtinit(SYNTH_MODE); 
  synchronize();
  dp();
  end_playing();
  parse();
  lag_stats();
/*  set_notetime();
  review_parse(0); */
  show_token_hist();
  /*  show_cond_hist(); */
  match_realizations();
}

old_live_rehearse() {
  char a[10],file[500];
  int n=0,flag;

  set_range();
  make_dp_graph(); 
  do {
    rtinit(LIVE_MODE); 
    synchronize();
    dp();
    end_sampling();
    parse();
    lag_stats();
    show_token_hist();
    show_cond_hist();
    match_realizations();
    review_parse(0);
    printf("add example to training? ");
    scanf("%s",a);
    if (a[0] == 'y') {
      add_example();
      update_training();
    }
    printf("rehearse section again? ");
    scanf("%s",a);
  }  while (a[0] == 'y');
}






int print_all=0;





void
synthetic_play_guts(char *type) {
  char a[10],file[500];
  int n=0,flag;
  FILE *fp;



  /*  init_async();  /* ready for asynchronous events */
/*  strcpy(file,scorename);
  printf("enter file to be played (no suffix) (.score .au file must exist): ");
  scanf("%s",scorename);
  if (readscore() == 0) { 
    printf("problem in readscore()\n"); 
    return;
  } */
  /*  if (readaudio() == 0) {
    /*    strcpy(scorename,file); 
    printf("problem in readaudio()\n"); 
    return; 
  } */

  fp = fopen(LOG_FILE,"a");
  fprintf(fp,"%s\n",audio_file);
  fclose(fp);


/*  readdist();*/
 /*  read_splines(); */
/*  read_trees(); */
/*  readtable();*/
  mode = SYNTH_MODE;
  make_dp_graph();
  /*  exper_init_forest(firstnote,lastnote);*/
  /*  read_bbn_training_dists_to_score("accom");  /* this is in make_complete_bbn_graph now */
  make_complete_bbn_graph_arg(type);
  print_all = 1;
  rtinit(SYNTH_MODE); 
  printf("firstnote = %d time = %f\n",firstnote,score.solo.note[firstnote].time);
  new_synchronize(score.solo.note[firstnote].time);
  //  init_midi();
  /*    test_my_timer();*/


  /*    dp();*/

  dp_live();  /* return in this call.  after return, all computing is driven by callbacks */
  return;
    summarize_performance();
}


void 
synthetic_play() {
  char answer[100];

  read_audio_indep();
  printf("enter the audio data directory: ");
  scanf("%s",audio_data_dir);
  printf("enter the training you want to use (null/accomp/both)\n");
  scanf("%s",answer);
  synthetic_play_guts(answer);
  //  while (dp_finished() == 0) pause();

  //CF:  **** direct control stops here **** everything now driven by callbacks until the end. ****
  while (num_pending_timer_events() > 0 /*dp_finished() == 0*/)     pause(); 

  printf("leaving synthetic play\n");
    summarize_performance();
    //              write_orchestra_audio();  /*only for fudging data */

    /*  printf("must takes this out right away\n");
	write_accomp_times();*/

 }




void
simulate_play() {
  char a[10],file[500];
  int n=0,flag;

  read_audio_indep();
  mode = SIMULATE_MODE;
  make_dp_graph();
  rtinit(SIMULATE_MODE); 
  dp();
  lag_stats();
  /*  show_token_hist();
      eval_accomp_note_times();*/
}


write_lags() {
  FILE *fp;
  char file[500];
  int i;

  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".lag");
  fp = fopen(file,"w"); 
  if (fp == NULL) { printf("couldn't open %s\n",file); exit(0); }
  for (i=firstnote; i < lastnote; i++) fprintf(fp,"%s\t%d\n",score.solo.note[i].observable_tag,score.solo.note[i].detect_frame);
  fclose(fp);
}

read_lags() {
  FILE *fp;
  char file[500],nn[500];
  int i,f;

  strcpy(file,audio_data_dir);  
  strcat(file,current_examp);   
  strcat(file,".lag");
  fp = fopen(file,"r"); 
  if (fp == NULL) { printf("couldn't open %s\n",file); exit(0); }
  for (i=firstnote; i < lastnote; i++) {
    fscanf(fp,"%s %d\n",&nn,&f);
    if (strcmp(nn,score.solo.note[i].observable_tag) != 0) { printf("didn't match %s\n",nn); exit(0); }
    score.solo.note[i].detect_frame = f;
  }
  fclose(fp);
}


void
windows_simulate_play() {
  char a[10],file[500];
  int n=0,flag;

  mode = SIMULATE_MODE;
  make_dp_graph();
  rtinit(SIMULATE_MODE); 
  dp();
  lag_stats();
  write_lags();
    missed_note_thresh_stats();  // for setting the std threshold for note detection
  /*  show_token_hist();
      eval_accomp_note_times();*/
}

void
windows_simulate_play_fixed_lag() {  /* just like simulate but only report note estimates at the stored detect_frames */
  char a[10],file[500];
  int n=0,flag;

  read_lags();
  mode = SIMULATE_MODE;
  make_dp_graph();
  rtinit(SIMULATE_MODE); 
  dp();
  lag_stats();
  //  missed_note_thresh_stats();  // for setting the std threshold for note detection
  /*  show_token_hist();
      eval_accomp_note_times();*/
}





static void
set_solo_range() {  /* maybe skip this for windos implementation */

  printf("enter start time of solo part (in d1+d2/d3 format):  ");
  scanf("%s",start_string);
  start_pos  = string2wholerat(start_string);
  start_meas = (float) start_pos.num / (float) start_pos.den;
  printf("enter end time of solo part (in d1+d2/d3 format):  ");
  scanf("%s",end_string);
  end_pos = string2wholerat(end_string);
  printf("end_string = %s end_pos = %d/%d\n",end_string,end_pos.num,end_pos.den);
  end_meas = (float) end_pos.num / (float) end_pos.den;
  /*  scanf("%f",&start_meas);
  printf("enter end time of solo part  in measures: ");
  scanf("%f",&end_meas);
  printf("start = %f end = %f\n",start_meas,end_meas); */
}


void
most_recent_live_rehearse() {
  char a[10],file[500],hs[500];
  int n=0,flag,just_solo;
  FILE *fp;


  fp = fopen(LOG_FILE,"a");
  fprintf(fp,"%s\n",audio_file);
  fclose(fp);


  printf("enter the audio data directory: ");
  scanf("%s",audio_data_dir);
  printf("accompaniment on headphones or speakers?");
  scanf("%s",hs);
  if (strcmp(hs,"headphones") == 0) accomp_on_speakers = 0;
  else if (strcmp(hs,"speakers") == 0) accomp_on_speakers = 1;
  else { printf("possibles answers are: headphones or speakers\n"); exit(0); }
  set_solo_range();

  /**********************************************************/

  set_range();
  /*  read_audio_indep(); */


  mode = LIVE_MODE;
  make_dp_graph();

  /*    if (score.solo.note[firstnote].num == RESTNUM) {
      printf("adjusing firstnote\n");
      while (score.solo.note[firstnote].num == RESTNUM && firstnote < score.solo.num-1) firstnote++;
    }
    printf("firstnote = %s\n",score.solo.note[firstnote].observable_tag); */


  /*  exper_init_forest(firstnote,lastnote);*/
  /*  read_bbn_training_dists_to_score("accom");  /* maybe should be moved elsewhere */
  /*  make_complete_bbn_graph(); */
  make_complete_bbn_graph();
  /*  make_complete_bbn_graph_arg(type);*/

  rtinit(LIVE_MODE); 

  new_synchronize(score.solo.note[firstnote].time);
  /*  dp(); */
  dp_live();

  /* ----  in finish .... */
  end_sampling();
  printf("would you like to save the audio file?");
  scanf("%s",a);
  if (a[0] == 'y') save_labelled_audio_file(); 
  /*  else return;*/
  parse();  
  // review_parse(0);      
  printf("would you like to write the parse?");
  scanf("%s",a);
  if (a[0] == 'y') {
    write_parse(); 
    if (accomp_on_speakers) write_accomp_times();
    new_add_example();
  }
  /*  end_playing();*/
  /*  set_notetime(); */

  lag_stats();  
  show_token_hist();
  eval_accomp_note_times();
  
  /*   make_performance_data();*
  /*  show_cond_hist();*/
}



void 
live_rehearse_guts(char *type) {
  set_range();
  mode = LIVE_MODE;
  make_dp_graph();
  //  make_complete_bbn_graph_arg(type);
  rtinit(LIVE_MODE); 
  new_synchronize(score.solo.note[firstnote].time); //CF:  start Timer, Play
  dp_live();
}

void 
prepare_live(int m) { /* only called when range is set */
  char name[200];
  FILE *fp;

  //  accomp_on_speakers = 1;
  //  printf("making bbn graph\n");
  //  make_complete_bbn_graph_windows();


  /*bbn_training_name(SOLO_TRAIN, name);
  fp = fopen(name,"r");
  fclose(fp);
  if (fp == NULL)  make_complete_bbn_graph_arg("null");
  else make_complete_bbn_graph_arg("both"); */

  set_range();

  //  wholerat2string(end_pos,name);
  //  printf("%s\n",name); exit(0);
  //  printf("lastnote = %s\n",score.solo.note[lastnote].observable_tag);
  //  printf("last_accomp = %s\n",score.midi.burst[last_accomp].observable_tag);

  //  mode = LIVE_MODE;
  mode = m;
  //  printf("making dp graph\n");
  //  make_dp_graph();
 add_needed_clique_trees();  
}

void
make_current_dp_graph() { /* transition probs change with new examples so
			     must be remade each run */
  read_examples();
  estimate_length_stats();  
  make_dp_graph();
}



int
start_live(int md) {

  //  exit(0);
  //  if (mode == LIVE_MODE) 
  if (md == LIVE_MODE) {
    printf("about to set cur_examp\n");
    next_free_examp(current_examp);  /* sets global current_examp */
    printf("set cur_examp to %s\n",current_examp);
  }
  make_current_dp_graph();
  // add_needed_clique_trees();  
 recall_partial_global_state();  
  //  rtinit(LIVE_MODE); 
  rtinit(md); 
  return(1);
  /* stripped down from new_synchronize */
  /*
  new_synchronize(score.solo.note[firstnote].time);
  printf("finished sync\n");
  dp_live();
  printf("dp_live\n"); */
}



//CF:  called from main, this is the live performance/rehearsal 
void 
live_rehearse() {
  char a[10],file[500],hs[500],answer[500];

  /* printf("enter the audio data directory: ");
      scanf("%s",audio_data_dir);*/
  printf("accompaniment on headphones or speakers?");
  scanf("%s",hs);
  if (strcmp(hs,"headphones") == 0) accomp_on_speakers = 0;
  else if (strcmp(hs,"speakers") == 0) accomp_on_speakers = 1;
  else { printf("possibles answers are: headphones or speakers\n"); exit(0); }
  set_solo_range();
  printf("enter the audio data directory: ");
  scanf("%s",audio_data_dir);
  printf("enter the training you want to use (null/accomp/both)\n");
  scanf("%s",answer);
  make_complete_bbn_graph_arg(answer);
  live_rehearse_guts(answer);
  while (num_pending_timer_events() > 0 /*dp_finished() == 0*/)   {
    pause(); 
  }
  printf("falling through live_rehearse\n");
}


#ifdef ABCDE


void
less_old_live_rehearse() {
  char a[10],file[500],hs[500],answer[500];
  int n=0,flag,just_solo;
  FILE *fp;


  fp = fopen(LOG_FILE,"a");
  fprintf(fp,"%s\n",audio_file);
  fclose(fp);


  printf("enter the audio data directory: ");
  scanf("%s",audio_data_dir);
  printf("accompaniment on headphones or speakers?");
  scanf("%s",hs);
  if (strcmp(hs,"headphones") == 0) accomp_on_speakers = 0;
  else if (strcmp(hs,"speakers") == 0) accomp_on_speakers = 1;
  else { printf("possibles answers are: headphones or speakers\n"); exit(0); }
  set_solo_range();

  /**********************************************************/

  set_range();
  /*  read_audio_indep(); */


  mode = LIVE_MODE;
  make_dp_graph();

  /*    if (score.solo.note[firstnote].num == RESTNUM) {
      printf("adjusing firstnote\n");
      while (score.solo.note[firstnote].num == RESTNUM && firstnote < score.solo.num-1) firstnote++;
    }
    printf("firstnote = %s\n",score.solo.note[firstnote].observable_tag); */


  /*  exper_init_forest(firstnote,lastnote);*/
  /*  read_bbn_training_dists_to_score("accom");  /* maybe should be moved elsewhere */
  /*  make_complete_bbn_graph(); */
  make_complete_bbn_graph();
  /*  make_complete_bbn_graph_arg(type);*/

  rtinit(LIVE_MODE); 

  new_synchronize(score.solo.note[firstnote].time);
  /*  dp(); */
  dp_live();

  /* ----  in finish .... */
  end_sampling();
  printf("would you like to save the audio file?");
  scanf("%s",a);
  if (a[0] == 'y') save_labelled_audio_file(); 
  /*  else return;*/
  parse();  
  // review_parse(0);      
  printf("would you like to write the parse?");
  scanf("%s",a);
  if (a[0] == 'y') {
    write_parse(); 
    if (accomp_on_speakers) write_accomp_times();
    new_add_example();
  }
  /*  end_playing();*/
  /*  set_notetime(); */

  lag_stats();  
  show_token_hist();
  eval_accomp_note_times();
  
  /*   make_performance_data();*
  /*  show_cond_hist();*/
}

#endif

static void
setclick() { /* to set up a click file for training probabilities */
  
  readaudio();
initdist();
/*  readtable(); */
  firstnote = 0;
  lastnote = score.solo.num - 1;
/*  initdist();  */
readdist(); 
  olinit();
  dp();
/*  daddclick(); 
  writebeat();  */
}






void
test_probs() {

/*   init_dist();    */
/*  Readdist();    */
/*  read_splines();*/
/*  read_trees();*/

/*  read_parms();  */
  read_distributions();
  /*  readaudio();*/
  read_audio_indep();
       testprobs(); return;                     
       /*           new_testprobs();   */
}



int
forward_backward() {
/*  init_stat_hist(); */
  init_prob_hist();
  mode = FORWARD_MODE;
  olinit();
  dp();
  mode = BACKWARD_MODE;
  olinit();
  dp();  
  return(1-failed);
}


#define DOWN_CHUNK 1000

static void
create_raw_from_48k() {
  FILE *fpl,*fph;
  char audio_file[500];
  unsigned char buff[2*DOWN_CHUNK*6],obuff[2*DOWN_CHUNK];
  
  strcpy(audio_file,audio_data_dir);  
  strcat(audio_file,current_examp);   
  strcat(audio_file,".48k");
  fph = fopen(audio_file,"rb"); 
  if (fph == NULL) {printf("coulndn't\n"); exit(0); }

  strcpy(audio_file,audio_data_dir);  
  strcat(audio_file,current_examp);   
  strcat(audio_file,".raw");
  fpl = fopen(audio_file,"wb"); 
  
  while (1) {
    fread(buff,DOWN_CHUNK*6,2,fph);
    if (feof(fph)) break;
    downsample_audio(buff,obuff, 6*DOWN_CHUNK,6);
    fwrite(obuff,DOWN_CHUNK,2,fpl);
  }
  fclose(fpl);
  fclose(fph);
  printf("wrote %s\n",audio_file);
}  
  

int
read_audio_indep() {  /* a machine independent version of readaudio */

  if (readaudio() == 0) { create_raw_from_48k(); exit(0); }
  if (read_parse() == 0) {return(0); }
  read_parse_accomp();
#ifdef ORCHESTRA_EXPERIMENT 
  read_orchestra_audio();
#endif
  return(1);
}



static void
gauss_traceback_parse() {
  int i,j,n,pos,note,k,mxk,d;
  PROB_HIST *ph;
  GNODE *pi,*pj,*nd;
  NODE *best;
  float sum,*mean,*opt,*vector(),p,prob,mx=-1,t; 
  char chrd[200];
  SOUND_NUMBERS s,ss;
  FILE *fp;
  
  ph = prob_hist + frames - 1;
  //  for (i=0; i < active.length; i++) printf("i = %d\tage = %d\tpos = %d\ttempo=%d\tnote = %d\tprob = %f\n",i,active.el[i]->age,active.el[i]->pos,active.el[i]->tempo,active.el[i]->dex,active.el[i]->prob);

  for (i=0; i < active.length; i++) {
    //    if (active.el[i]->gs.status != AFTER_END) continue;
    if( active.el[i]->gs.tempo.h > mx) {
      mx = active.el[i]->gs.tempo.h;
      best = active.el[i];
    }
  }
  if (mx == -1.) { printf("no path to end state found\n"); exit(0); }

  for (i=frames-1; i > 0; i--) {
    //    printf("i = %d mom = %d\n",i,best->mom);
    best->mom->son = best;
    best = best->mom;
  }
  for (i=0; i < frames-1; i++) {
    //        printf("i = %d\tframe = %d\tnote = %d\ttempo=%f\tbest = %d\n",i,best->gs.frame,best->gs.note,best->gs.tempo.m,best);
    if (best->gs.note != best->mom->gs.note) {
      score.solo.note[best->gs.note].frames = i;
      score.solo.note[best->gs.note].realize = (float) i;
      score.solo.note[best->gs.note].saved_frames = i;
      //      score.solo.note[best->gs.note].off_line_secs = toks2secs(i);
    }
    if (best->gs.note != best->mom->gs.note) {
      printf("note %d at %d\n",best->gs.note,i);
      if (best->gs.note >= 0)  printf("note =  %s\n",score.solo.note[best->gs.note].observable_tag);
    }
    best = best->son;
  }
  for (i=firstnote; i <= lastnote; i++) printf("%s gauss later by %d\n",score.solo.note[i].observable_tag,score.solo.note[i].saved_frames-score.solo.note[i].frames);
}






//CF:  gets called from main.c
//CF:  Main entry for doing a parse (ie. finding state labels for an audio performance)
//CF:  Segments an existing audio performance, using a score.
//CF:  Using the forward-backward algorithm (with a mod for optimal music segmentation)
//CF:  (assumes trained HMM already done and stored in a file)
int
parse() {
  int success = 0,i;
  NODEPTR best,cur;
  char s[30],ans[30],command[50];
  float best_score;

#ifndef WINDOWS
  if (read_parse() == 1)   {/* add_example();*/  /*return;*/ } 
  

    read_distributions(); //CF:  read output distributions of the HMM from file
    make_dp_graph(); //CF:  makes the HMM data structure (graph.c)   
#endif
        read_parse_accomp();  // added 2-10  sometimes notes are gated during parse due to leftover acc times

    /*  read_trees();*/
    /*  init_stat_hist(); */
    init_prob_hist();  //CF:  set lengths of hypothesis history lists to zero
    printf("forward\n");
    mode = FORWARD_MODE;
    olinit();
    dp();
    if (failed) return(0);
    mode = BACKWARD_MODE;
    printf("backward\n");
    olinit();
    dp();
    if (failed) return(0);
    if (mean_parse() == 0) return(0); 
    /*    wiser_mean_parse();  */
    //                   review_parse(0);                         




#ifdef WINDOWS
    //    write_current_parse();   /* move toward "current" method */
#else
    write_parse(); 
#endif



    /* for comparing with gauss parse */

    /*    mode = GAUSS_DP_MODE;
    olinit();
    dp();

    gauss_traceback_parse();*/
    return(1);


}






int
mirex_parse() {
   int success = 0,i;
  NODEPTR best,cur;
  char s[30],ans[30],command[50];
  float best_score;


  firstnote = 0;
  lastnote = score.solo.num-1;
  make_dp_graph(); //CF:  makes the HMM data structure (graph.c)   

  init_prob_hist();  //CF:  set lengths of hypothesis history lists to zero
  printf("forward\n");
  mode = FORWARD_MODE;
  olinit();
  dp();
  if (failed) return(0);
  mode = BACKWARD_MODE;
  printf("backward\n");
  olinit();
  dp();
  if (failed) return(0);
  if (mean_parse() == 0) return(0); 
  return(1);
}









void
playback_parse() {
  int success = 0,i;
  NODEPTR best,cur;
  char s[30],ans[30],command[50],start[500];
  float best_score;
  int start_frame;
  RATIONAL r;

  read_audio_indep();
  printf("enter the frame to start:");
  scanf("%s %s",command,start);
  if (strcmp(command,"start") != 0) 
    { printf("wanted to see '1st_frame'\n"); exit(0); }
  
  r = string2wholerat(start);
  for (i=0; i < score.solo.num; i++) 
    if (rat_cmp(r,score.solo.note[i].wholerat) <=0) break;
  start_frame = score.solo.note[i].realize;
  start_frame = max(0,start_frame-50);
  review_parse(start_frame);                         
}



dp_parse() {
  int success = 0,i;
  NODEPTR best,cur;
  char s[30],ans[30],command[50];
  float best_score;

  read_audio_indep();
    read_distributions();
    make_dp_graph();
    init_prob_hist();
    printf("forward\n");
    mode = PARSE_MODE;
    olinit();
    dp();
    traceback_parse();
    //    review_parse(0);                   
    write_parse(); 
}

		  
	  

void 
sim_wait_for_samples() {/* when samples are not being collected on-line wait until
			  the time (approx) at which we would have the samples
			  and return */

  /* this works for sun and pc */

  float read_time,now();

  read_time = tokens2secs((float) token+1);  /* earliest time at which frame would be avail */

  //    printf("now = %f read_time = %f diff = %f\n",now(),read_time,now()-read_time);
         while (now() < read_time);
}




int
readtoken() {
  int b,i,tot;
  float read_time,n,now();
  extern int lasttoken;
  short int *si;
  unsigned char *ci;
  extern float token_time[];

  if (mode != SYNTH_MODE && mode != LIVE_MODE)  {
    printf("illegal mode in readtoken()\n");
    return(0);
  }
  token++;
  read_time = tokens2secs((float)token+1);  //CF:  time that next frame will be ready (in perfect world!)
  /*  if (now() > read_time) 
    printf("behind by %f secs in readtoken() %f %f\n",
	   now()-read_time,now(),read_time);  */

  if (mode == SYNTH_MODE) {
#ifndef ORCHESTRA_EXPERIMENT
    play_a_frame();
#endif
 //   sim_wait_for_samples();  // took this out on MAC --- terrible to loop on now() !
    if (token == frames-1) lasttoken=1; 
  } 
  if (mode == LIVE_MODE) {
    wait_for_samples();
  } 
  //  token_time[token] = read_time - now();

  token_time[token] = now();
  /*   if (token > 30 && token_time[token] < -.2) printf("token %d late by %f\n",token,token_time[token]);*/
  samples2data();  //CF:  convert to floating point
}


/*void
listen() {

  if (mode == SYNTH_MODE) { 
    printf("this shouldn't happen\n");
    exit(0);
    //    readtoken(); 
  } 
  else samples2data();
  calc_features();
} */


void
audio_listen() {

  if (mode == SYNTH_MODE) { 
    printf("this shouldn't happen\n");
    exit(0);
    //    readtoken(); 
  } 
  else samples2data();
  calc_features();
}









gauss_dp_parse() {
  int success = 0,i;
  NODEPTR best,cur;
  char s[30],ans[30],command[50];
  float best_score;
  RATIONAL r;
  int a;


  
  //  readaudio();
  read_audio_indep();
  //    frames = 400;
  //  frames = 2000;
  /*    make_dp_graph();*/
    init_prob_hist();
    mode = GAUSS_DP_MODE;
    olinit();
    dp();

        gauss_traceback_parse();
		write_parse();
}




void
get_rgb_spect(unsigned char **rgb, int *rows, int *cols) {
  int i,j,t,v,k,start,end,n,spec_ht;
  float **s,spread,mmax= -10000,mmin=10000,m,**matrix(),*mem;
  FILE *fp;
  
  start = 0;
  end = frames;
  *rgb = (unsigned char *) malloc(3*(end-start)*freqs);
  //  s = matrix(0,FREQDIM,0,frames);

  mem = (float *) malloc(FREQDIM*frames*sizeof(float));
  s = (float **) malloc(freqs*sizeof(float *));
  for (j=0; j < freqs; j++) s[j] = mem + frames*j;

  readbeat();
  for (token=start; token < end; token++) {
    audio_listen();
    for (j=0; j < freqs; j++) { 
      s[j][token] =  pow(log(1+spect[j]),.4);
      //      s[j][token] =  spect[j];
      //      s[j][token] =  spect[j]-last_spect[j];
      //      if (s[j][token] < 0) s[j][token] = 0;
      //            printf("%f %f\n",spect[j],last_spect[j]);
    }
  }
  m=HUGE_VAL;
  for (i = 0; i < freqs; i++)   for (j=start; j < end; j++) {
    if (s[i][j] > mmax) mmax = s[i][j];  
    if (s[i][j] < mmin) mmin = s[i][j];  
  }
  spread = mmax - mmin;
  k= 0;
  for (i = 0; i < freqs; i++) {
    for (j=start; j < end; j++) {
      v = (int) (256*(s[freqs-i-1][j] - mmin)/spread);
      (*rgb)[k++] = v;      (*rgb)[k++] = v;      (*rgb)[k++] = v;
    }
  }
  free(mem);
  *cols = end-start;
  *rows = freqs;
}

void
get_example_file_name(char *examps) {
  strcpy(examps,user_dir);
   strcat(examps,AUDIO_DIR);
  strcat(examps,"/");
  strcat(examps,player);
  strcat(examps,"/");
  strcat(examps,scoretag);
  strcat(examps,"/");
  strcat(examps,scoretag);
  strcat(examps,".ex");
}

void
get_player_file_name(char *examps, char *suffix) {
  strcpy(examps,user_dir);
   strcat(examps,AUDIO_DIR);
  strcat(examps,"/");
  strcat(examps,player);
  strcat(examps,"/");
  strcat(examps,scoretag);
  strcat(examps,"/");
  strcat(examps,scoretag);
  strcat(examps,".");
  strcat(examps,suffix);
}



#define PEAK_WIDD 5

int
is_mic_working(float freq) {
   int i;
  float f,t1,t2,mx=0;
  SOUND_NUMBERS tuninga;


  token = .5*FRAMES_PER_LEVEL * VOL_TEST_QUANTA- 20;
  //  for (token = 0; token < frames; token++) {
    samples2data();
    setspect();
    f = hz2omega(freq);

    for (i = (int) f-PEAK_WIDD,t1=0 ; i < (int) f + PEAK_WIDD; i++) t1 += spect[i];
    for (i = (int) f-3*PEAK_WIDD,t2=0 ; i < (int) f + 3*PEAK_WIDD; i++) t2 += spect[i];
  //  for (i = 10,t2=0; i < (int) f + PEAK_WIDD; i++) t2 += spect[i];
    printf("rat = %f %f %f \n",t1,t2,t1/t2);
    //    if (t1/t2 > mx) mx = t1/t2;
    //  }
  

    for(i=0; i < freqs; i++) printf("%d %f %f \n",i,f, spect[i]);
    //  printf("mx = %f\n",mx);
  return(t1/t2 > .75);
  
}







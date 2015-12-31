

//#define PARAM_EXPERIMENT
//#define PARALLEL_GRAPH_EXPERIMENT
//#define GRADUAL_CATCH_UP_EXPERIMENT
//#define CONSTRAINT_EXPERIMENT



#define CHUNK 1024
/*#define VERBOSE  */
/*#include "share.c"       
#include "global.h"         */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "matrix_util.h"
#include "supermat.h"
#include "joint_norm.h"
#include "belief.h"
#include "share.c"
#include "kalman.h"
#include "global.h"
#include "times.h"
#include "new_score.h"
#include "gui.h"
#include "vocoder.h"
#include "linux.h"
#include "platform.h"

int global_flag = 0;

#define PI 3.14159

#define MAX_NOTES_PER_PHRASE 1000
  
#define WEENY .0001

#define MCS_UNSET -1  /* mcs_number is unset */
#define RANDOM_VECT_DIM 3
#define POS_INDEX 0
#define TEMPO_INDEX 1
#define ACCEL_INDEX 2
#define OBS_NOTE_TYPE 0
#define SOLO_NOTE_TYPE 1
#define ACCOM_NOTE_TYPE 2

#define MAX_FRONTIER 1000


#define MEANS_ONLY 1
#define MEANS_AND_VARS 2

typedef struct {
  int num;
  BNODE *list[MAX_FRONTIER];
} FRONTIER_LIST;



typedef struct {
  char *name;
  float val;
} TAGGED_OBS;


typedef struct {
  int num;
  TAGGED_OBS *tag_obs;
} DATA_OBSERVATION;

#define MAX_TRAIN_FILES 300

typedef struct {
  int num;
  char **name;
  DATA_OBSERVATION *dato;
} TRAIN_FILE_LIST;

       
//SCORE score;  
static int bel_num_nodes;
static FRONTIER_LIST frontier;
static BNODE **perfect;
static MATRIX evol;  /* the evolution Matrix */
static CLIQUE_TREE ctree; 
static TRAINING_RESULTS glob_tr;  // a global variable containing the training

static void  fix_clique_value(CLIQUE_NODE *clnode, int component, float val);


/*

static BELIEF_NET bnet; */


/*extern void *malloc (size_t); taking this out*/
static PHRASE_LIST component;



float my_log(float x) {
  if (x <= 0) {
    printf("trying to take log(%f)\n",x);
    exit(0);
  }
  return(log(x));
}


/*static QUAD_FORM 
jn2qf(JOINT_NORMAL jn)
{
  QUAD_FORM qf;
  int d;

  d = jn.mean.rows;
  qf.m = jn.mean;
  qf.S = Msc(Mi(jn.var),-.5);
  qf.c = -( d*my_log(2*PI)+ my_log(Mdet(jn.var))) / 2 ;
  return(qf);
}

static JOINT_NORMAL
qf2jn(QUAD_FORM qf)
{
  int d;
  JOINT_NORMAL jn;

  jn.mean = qf.m; 
  jn.var =  Msc(Mi(qf.S),-.5);
  return(jn);
}
*/
static BNODE*
alloc_bel_node(int dim, float meas, int type, int index) {
  BNODE *temp;


  temp = (BNODE *) malloc(sizeof(BNODE));
  temp->next.num = 0;
  temp->prev.num = 0;
  temp->neigh.num = 0;
  temp->node_num = bel_num_nodes++;
  temp->dim = dim;
  temp->obs.observed = 0;
  temp->meas_time = meas;
  temp->note_type = type;
  temp->index = index;
  temp->focus = 1;
  return(temp);
}


static BNODE*
alloc_belief_node(int dim, QUAD_FORM e, int type, int index, BELIEF_NET *bn) {
  BNODE *temp;


  temp = (BNODE *) malloc(sizeof(BNODE));
  temp->next.num = 0;
  temp->prev.num = 0;
  temp->neigh.num = 0;
  temp->node_num = bel_num_nodes++;
  temp->dim = dim;
  temp->obs.observed = 0;
  temp->e = QFperm(e);
  temp->note_type = type;
  temp->index = index;
  temp->focus = 1;

  if (bn->num < MAX_BELIEF_NODES) {
    bn->list[bn->num]  = temp;
    (bn->num)++;
  }
  else {
    printf("out of room in alloc_belief_node\n");
    exit(0);
  }
  
  return(temp);
}


char 
*trainable_tag(int type, int index) {
  char name[500],pname[500],tag[500],*ret,rats[500];
  int pitch,solo;
  float time;
  RATIONAL rat;

  if (type == SOLO_PHRASE_START) {   
    strcpy(name,"start");
    solo = 1;
  }
  else if (type == SOLO_UPDATE_NODE) {
    strcpy(name,"update");
    solo = 1;
  }
  else if (type == ACCOM_PHRASE_START) {   
    strcpy(name,"acc_start");
    solo = 0;
  }
  else if (type == ACCOM_UPDATE_NODE) {   
    strcpy(name,"acc_update");
    solo = 0;
  }
  else if (type == BACKBONE_UPDATE_SOLO_NODE) {   
    strcpy(name,"bbone_solo_update");
    solo = 1;
  }
  else if (type == BACKBONE_UPDATE_MIDI_NODE) {   
    strcpy(name,"bbone_midi_update");
    solo = 0;
  }
  else if (type == BACKBONE_PHRASE_START) {   
    strcpy(name,"bbone_phrase_start");
    solo = 0;
  }
  else { printf("can't handle type\n"); exit(0); }
  if (solo) {
    pitch = score.solo.note[index].num;
    time = score.solo.note[index].time;
    rat = score.solo.note[index].wholerat;
  }
  else {
    pitch = score.midi.burst[index].num;
    time = score.midi.burst[index].time;
    rat = score.midi.burst[index].wholerat;
  }
  num2name(pitch,pname); 
  wholerat2string(rat,rats);
  /*  sprintf(tag,"%5.3f",time); */
  strcpy(tag,rats);
  strcat(tag,"_");
  strcat(tag,pname);
  strcat(name,"_");
  strcat(name,tag);
  ret = (char *) malloc(strlen(name)+1);
  strcpy(ret,name);
  return(ret);
}
  
char 
*trainable_tag_rat(int type,  RATIONAL rat) {
  char name[500],pname[500],tag[500],*ret,rats[500];
  int pitch,solo;
  float time;

  if (type == SOLO_PHRASE_START) {   
    strcpy(name,"solo_phrase_start");
    solo = 1;
  }
  else if (type == SOLO_UPDATE_NODE) {
    strcpy(name,"solo_update");
    solo = 1;
  }
  else if (type == SOLO_XCHG_NODE) {
    strcpy(name,"solo_xchg");
    solo = 1;
  }
  else if (type == SOLO_NODE) {
    strcpy(name,"solo");
    solo = 1;
  }
  else if (type == ACCOM_PHRASE_START) {   
    strcpy(name,"acc_start");
    solo = 0;
  }
  else if (type == ACCOM_UPDATE_NODE) {   
    strcpy(name,"acc_update");
    solo = 0;
  }
  else if (type == ACCOM_XCHG_NODE) {   
    strcpy(name,"acc_xchg");
    solo = 0;
  }
  else if (type == BACKBONE_UPDATE_SOLO_NODE) {   
    strcpy(name,"bbone_solo_update");
    solo = 1;
  }
  else if (type == BACKBONE_UPDATE_MIDI_NODE) {   
    strcpy(name,"bbone_midi_update");
    solo = 0;
  }
  else if (type == BACKBONE_UPDATE_NODE) {   
    strcpy(name,"bbone_update");
    solo = 0;
  }
  else if (type == BACKBONE_PHRASE_START) {   
    strcpy(name,"bbone_phrase_start");
    solo = 0;
  }
  else if (type == BACKBONE_XCHG) {   
    strcpy(name,"bbone_xchg");
    solo = 0;
  }
  else { printf("can't handle type\n"); exit(0); }
  wholerat2string(rat,rats);
  /*  sprintf(tag,"%5.3f",time); */
  strcat(name,"_");
  strcat(name,rats);
  ret = (char *) malloc(strlen(name)+1);
  strcpy(ret,name);
  return(ret);
}
  


static BNODE*
alloc_belief_node_pot(int dim, int type, int index, BELIEF_NET *bn) {
  BNODE *temp;


  temp = (BNODE *) malloc(sizeof(BNODE));
  temp->next.num = 0;
  temp->prev.num = 0;
  temp->neigh.num = 0;
  temp->node_num = bel_num_nodes++;
  temp->dim = dim;
  temp->obs.observed = 0;
  temp->note_type = type;
  temp->index = index;
  temp->focus = 1;
  temp->train_dist = NULL;
  temp->trainable = (type == SOLO_PHRASE_START || type == SOLO_UPDATE_NODE || type == ACCOM_PHRASE_START || type == ACCOM_UPDATE_NODE || type == BACKBONE_UPDATE_SOLO_NODE  || type == BACKBONE_UPDATE_MIDI_NODE);
  temp->trainable_tag = (temp->trainable) ? trainable_tag(type,index) : NULL;

  if (type == OBS_NODE)  {
    temp->observable_tag = score.solo.note[index].observable_tag;
    temp->wholerat = score.solo.note[index].wholerat;
  }
  else if (type == ACCOM_OBS_NODE)  {
    temp->observable_tag = score.midi.burst[index].observable_tag;
    temp->wholerat = score.midi.burst[index].wholerat;
  }
  else temp->observable_tag = NULL;

  



  if (type == SOLO_NODE || type == OBS_NODE || 
      type == PHANTOM_NODE || type == SOLO_PHRASE_START ||
      type == SOLO_UPDATE_NODE || type == BACKBONE_SOLO_NODE || type == BACKBONE_UPDATE_SOLO_NODE) {
    temp->meas_time = score.solo.note[index].time;
  }
  else if (type == INTERIOR_NODE || type == ACCOM_OBS_NODE || 
	   type == LEAD_CONNECT_NODE || type == ACCOMP_LEAD_NODE || 
      type == ANCHOR_NODE || type == ACCOM_PHRASE_START || type == ACCOM_UPDATE_NODE || 
	   type == ACCOM_NODE || type == BACKBONE_MIDI_NODE || type == BACKBONE_UPDATE_MIDI_NODE)
    temp->meas_time = score.midi.burst[index].time;
  else {
    printf("unknown type (%d) in alloc_belief_node_pot\n",type);
    exit(0);
  }
  if (bn->num < MAX_BELIEF_NODES) {
    bn->list[bn->num]  = temp;
    (bn->num)++;
  }
  else {
    printf("out of room in alloc_belief_node\n");
    exit(0);
  }
  
  return(temp);
}



static BNODE*  
alloc_belief_node_pot_rat(int dim, int type, RATIONAL rat, BELIEF_NET *bn) {
  BNODE *temp;


  if (type != BACKBONE_XCHG && type != BACKBONE_NODE && type != BACKBONE_UPDATE_NODE && type != SOLO_PHRASE_START) {
    printf("can't handle this type in alloc_belief_node_pot_rat %d \n",type);
    exit(0);
  }
  temp = (BNODE *) malloc(sizeof(BNODE));
  temp->next.num = 0;
  temp->prev.num = 0;
  temp->neigh.num = 0;
  temp->node_num = bel_num_nodes++;
  temp->dim = dim;
  temp->obs.observed = 0;
  temp->note_type = type;
  temp->focus = 1;
  temp->train_dist = NULL;
  temp->trainable = (type == BACKBONE_XCHG || type ==  BACKBONE_UPDATE_NODE || type == SOLO_PHRASE_START);
  temp->trainable_tag = (temp->trainable) ? trainable_tag_rat(type,rat) : NULL;
  temp->observable_tag = NULL;
  temp->meas_time = (float) rat.num / (float) rat.den;
  if (bn->num < MAX_BELIEF_NODES) {
    bn->list[bn->num]  = temp;
    (bn->num)++;
  }
  else {
    printf("out of room in alloc_belief_node\n");
    exit(0);
  }
  return(temp);
}

static MATRIX
prior_update_cov(float length) {
  MATRIX cov;

  cov = Miden(BACKBONE_DIM);
  cov.el[0][0] = length*.01;  /* not thought out very carefully */
  cov.el[1][1] = length*.01;
  return(cov);
}

static MATRIX
prior_init_mean() {
  MATRIX m;

  m = Mzeros(BACKBONE_DIM,1);
  m.el[1][0] = score.meastime;  /* uneasy about this.  is score.meastime in wholenote units */
  return(m);
}

static MATRIX
prior_init_cov() {
  MATRIX cov;

  cov = Miden(BACKBONE_DIM);  /* need to consider this */
  return(cov);
}


static void
set_constraint(BNODE_CONSTRAINT *c, int dim, int type) {
  int i;
  
  for (i=0; i < dim; i++) c[i].active = 0;
  if (type == BACKBONE_NODE || type == BACKBONE_PHRASE_START) {
    c[1].active = 1; c[1].lim = 0.; 
  }
  if (type == BACKBONE_XCHG) {
    c[0].active = 1; c[0].lim = 0.; 
  }
}


static QUAD_FORM
prior_update_dist(float length) {
  return(QFpos_meassize_update(.01*length,.01*length)); 
}

static QUAD_FORM
prior_cue_dist() {
  return(QFmeas_size(score.meastime,.1));                          
}

static QUAD_FORM
prior_xchg_dist() {
  return(QFmv(Mconst(1,1,score.meastime),Miden(1)));
}

//CF:  
static int
is_node_trainable(int type, RATIONAL rat) {
  int i;

  if (type == BACKBONE_UPDATE_NODE) return(1);
  if (type != BACKBONE_XCHG && type != BACKBONE_PHRASE_START) return(0);
  for (i=0; i < tempo_list.num; i++) if (rat_cmp(rat,tempo_list.el[i].wholerat) == 0) break;
  if (i == tempo_list.num) return(1);
  return(0);
}

//CF:  constructor for a belief node (using malloc).
//CF:  Handles all types of BACKBONE belief notes

static BNODE*  
alloc_belief_node_composite(int dim, int type, RATIONAL rat, BELIEF_NET *bn, float whole_len) {
  BNODE *temp;
  QUAD_FORM e;
  int s,a,si;


  //CF:  type of node.  BACKBONE_NODE is regular node.  PHRASE_START is beginning of a stat independent group of nodes.(cue)
  //CF:                 XCHG:tempo reset, yet time comes from predecessor.(exchange point).
  //CF:  (CATCHUP not used)
  if (type != BACKBONE_XCHG && type != BACKBONE_NODE && type != BACKBONE_UPDATE_NODE && type != BACKBONE_PHRASE_START && type != CATCH_UP_NODE) {
    printf("can't handle this type in alloc_belief_node_pot_rat\n");
    exit(0);
  }
  temp = (BNODE *) malloc(sizeof(BNODE));
#ifdef CONSTRAINT_EXPERIMENT
  temp->constraint = (BNODE_CONSTRAINT *) malloc(dim*sizeof(BNODE_CONSTRAINT));
  set_constraint(temp->constraint,dim,type);
#endif
  temp->next.num = 0;
  temp->prev.num = 0;
  temp->neigh.num = 0;
  temp->node_num = bel_num_nodes++;  //CF:  unique ID for this node
  temp->dim = dim;
  temp->obs.observed = 0;  //CF:  bool, whether this node observed
  temp->note_type = type;
  temp->focus = 1; //CF:   not used?
  temp->train_dist = NULL;  //CF:  if this node corresponds to a trainable distro (eg.updates), this will hold a ponter to it
  temp->trained = 0;
  temp->trainable = (type == BACKBONE_XCHG || type ==  BACKBONE_UPDATE_NODE || type == BACKBONE_PHRASE_START);

  // muffin
  //  temp->trainable = is_node_trainable(type,rat);

  //CF:  makes a text string for human viewing (and training files) of this node
  temp->trainable_tag = (temp->trainable) ? trainable_tag_rat(type,rat) : NULL;

  //CF:  ----------- this section not used...
  if (type ==  BACKBONE_UPDATE_NODE) {
    //    temp->prior_mean = Mperm(Mzeros(BACKBONE_DIM,1));   //CF:  not used
    //    temp->prior_cov = Mperm(prior_update_cov(whole_len));
    e = prior_update_dist(whole_len);  /* maybe should pass e instead of whole_len */
    temp->prior_mean = Mperm(e.m);
    temp->prior_cov = Mperm(e.cov);
#ifdef    ROMEO_JULIET
  si = coincident_solo_index(rat);  
  if (si > -1) score.solo.note[si].update_node = temp;
#endif
  }
  if (type ==  BACKBONE_PHRASE_START) {
    //    temp->prior_mean = Mperm(prior_init_mean());
    //    temp->prior_cov = Mperm(prior_init_cov());
    e = prior_cue_dist();
    temp->prior_mean = Mperm(e.m);
    temp->prior_cov = Mperm(e.cov);
  }
  if (type ==  BACKBONE_XCHG) {
    //    temp->prior_mean = Mperm(prior_init_mean());
    //    temp->prior_cov = Mperm(prior_init_cov());
    e = prior_xchg_dist();
    temp->prior_mean = Mperm(e.m);
    temp->prior_cov = Mperm(e.cov);
  }
  //CF: ... ---------------

  temp->observable_tag = NULL;   //CF:  this is a backbone node, so not observable.
  temp->wholerat = rat;
  temp->meas_time = (float) rat.num / (float) rat.den;
  if (bn->num < MAX_BELIEF_NODES) {
    bn->list[bn->num]  = temp;
    (bn->num)++;
  }
  else {
    printf("out of room in alloc_belief_node\n");
    exit(0);
  }
  return(temp);
}



alloc_belief_node_parallel(int dim, int type, RATIONAL rat, BELIEF_NET *bn, float whole_len) {
  BNODE *temp;
  QUAD_FORM e;
  int s,a,si;


  if (type !=  SOLO_NODE && type !=  SOLO_PHRASE_START &&  type != SOLO_UPDATE_NODE && type !=  SOLO_XCHG_NODE && 
      type != ACCOM_NODE && type != ACCOM_PHRASE_START && type != ACCOM_UPDATE_NODE && type != ACCOM_XCHG_NODE ) {
    printf("can't handle this type in alloc_belief_node_parallel\n");
    exit(0);
  }
  temp = (BNODE *) malloc(sizeof(BNODE));
  temp->next.num = 0;
  temp->prev.num = 0;
  temp->neigh.num = 0;
  temp->node_num = bel_num_nodes++;  //CF:  unique ID for this node
  temp->dim = dim;
  temp->obs.observed = 0;  //CF:  bool, whether this node observed
  temp->note_type = type;
  temp->focus = 1; //CF:   not used?
  temp->train_dist = NULL;  //CF:  if this node corresponds to a trainable distro (eg.updates), this will hold a ponter to it
  temp->trained = 0;
  temp->trainable = ( (type == SOLO_PHRASE_START || type ==  SOLO_UPDATE_NODE || type == SOLO_XCHG_NODE) );
  //		     (type == ACCOM_PHRASE_START || type ==  ACCOM_UPDATE_NODE || type == ACCOM_XCHG_NODE));


  //CF:  makes a text string for human viewing (and training files) of this node
  temp->trainable_tag = (temp->trainable) ? trainable_tag_rat(type,rat) : NULL;

  //CF:  ----------- this section not used...
  if (type ==  SOLO_UPDATE_NODE) {
    e = prior_update_dist(whole_len);  /* maybe should pass e instead of whole_len */
    temp->prior_mean = Mperm(e.m);
    temp->prior_cov = Mperm(e.cov);
  }
  if (type ==  SOLO_PHRASE_START) {
    e = prior_cue_dist();
    temp->prior_mean = Mperm(e.m);
    temp->prior_cov = Mperm(e.cov);
  }
  if (type ==  SOLO_XCHG_NODE) {
    e = prior_xchg_dist();
    temp->prior_mean = Mperm(e.m);
    temp->prior_cov = Mperm(e.cov);
  }


  temp->observable_tag = NULL;   //CF:  this is a backbone node, so not observable.
  temp->wholerat = rat;
  temp->meas_time = (float) rat.num / (float) rat.den;
  if (bn->num < MAX_BELIEF_NODES) {
    bn->list[bn->num]  = temp;
    (bn->num)++;
  }
  else {
    printf("out of room in alloc_belief_node\n");
    exit(0);
  }
  return(temp);
}




       
       
static int
add_undir_arc(BNODE *from, BNODE *to)      /* add a undirected arc */
                 {
  int i;
  char string[500];


  if (from == to) {
    printf("can't add arc to self\n");
    exit(0);
  }
  if (from->neigh.num >= MAX_BSUCC) {
    printf("can't add another undirected arc %d\n", MAX_BSUCC);

    exit(0);
  }
  for (i=0; i < from->neigh.num; i++) 
    if (from->neigh.arc[i] == to) break;
  if (i == from->neigh.num)  /* connexion not already there */
    from->neigh.arc[from->neigh.num++] = to;
  else return(0);
  if (to->neigh.num >= MAX_BSUCC) {
    wholerat2string(to->wholerat,string);
    printf("to position is %s\n",string);
    wholerat2string(from->wholerat,string);
    printf("from position is %s\n",string);
    printf("can't add another belief arc here (%d)\n",MAX_BSUCC);
    printf("node %s leads to too many others\n note type is %d\n",to->observable_tag,to->note_type);
    exit(0);
  }
  for (i=0; i < to->neigh.num; i++) 
    if (to->neigh.arc[i] == from) break;
  if (i == to->neigh.num) /* connextion not already there */
    to->neigh.arc[to->neigh.num++] = from;
  else return(0);
  return(1);
}
       

static void
add_dir_arc(BNODE *from, BNODE *to, MATRIX m)      /* add a directed arc */
                 {
		   /*   printf("ouch\n"); */
			 /*    exit(0);  */

  if (from->next.num >= MAX_BSUCC) {
    printf("can't add another belief arc\n");
    exit(0);
  }
  from->next.arc[from->next.num++] = to;

  if (to->prev.num >= MAX_BSUCC) {
    printf("can't add another belief arc\n");
    exit(0);
  }
  to->prev.arc[to->prev.num] = from;
  to->prev.A[to->prev.num] = Mperm(m);
  to->prev.num++;
  add_undir_arc(from,to);
}
       
       
static void
add_dir_arc_pot(BNODE *from, BNODE *to, MATRIX m, QUAD_FORM pot) {
     /* add a directed arc */             
  int i;


  if (from->next.num >= MAX_BSUCC) {
    printf("can't add another belief arc\n");
    exit(0);
  }
  for (i=0; i < from->next.num; i++) if (from->next.arc[i] == to) {
    printf("arc already exists\n");
    exit(0);
  }
    



  from->next.arc[from->next.num++] = to;

  if (to->prev.num >= MAX_BSUCC) {
    printf("can't add another belief arc\n");
    exit(0);
  }
  to->prev.arc[to->prev.num] = from;
  to->prev.A[to->prev.num] = Mperm(m);
  to->prev.e[to->prev.num] = QFperm(pot);
  to->prev.num++;
  add_undir_arc(from,to);
}
       
       
       
       


/*static JOINT_NORMAL 
  /*parms2jn(MATRIX mean, MATRIX std)  /* components independent */
/*                 {
  JOINT_NORMAL jn;
  int i,j;

  if (mean.cols != 1 || std.cols != 1) {
    printf("didn't pass vectors in parms2jn()\n");
    exit(0);
  }
  if (std.rows != mean.rows) {
    printf("dimensions don't agree in parms2jn()\n");
    exit(0);
  }
  jn.mean = mean;
  jn.var.cols = jn.var.rows = std.rows;
  for (i =0; i < std.rows; i++)   for (j =0; j < std.rows; j++) 
    jn.var.el[i][j] = (i == j) ? std.el[i][0]*std.el[i][0] : 0;
  return(jn);
}
    */

/*static void
init_evol_matrix(void) {

  evol = Miden(RANDOM_VECT_DIM);
  evol.el[0][0] = 1;
  evol.el[0][1] = 1;
  evol.el[0][2] = 0;
  evol.el[1][0] = 0;
  evol.el[1][1] = 1;
  evol.el[1][2] = 1;
  evol.el[2][0] = 0;
  evol.el[2][1] = 0;
  evol.el[2][2] = 1;
} */

MATRIX
evol_matrix(int dim, float length) {
  MATRIX temp;
  int i;

  temp = Miden(dim); //CF:  make identity matrix
  for (i = 0; i < dim-1; i++) temp.el[i][i+1] = length;  //CF:  overwrite one element with note_length
  return(temp);
}
  

#define ATEMPO_LAMBDA 1.



/*
  1     length    0
  0     lambda    1-lambda
  0     0         1
*/

MATRIX
evol_matrix_at(float length) {
  MATRIX temp;
  int i;

  temp = Miden(BACKBONE_ATEMPO_DIM);
  temp.el[0][1] = length;
  temp.el[1][1] = ATEMPO_LAMBDA;
  temp.el[1][2] = 1 - ATEMPO_LAMBDA;
  return(temp);
}
  

static MATRIX
evol_matrix_pred_pos(float length) {
  MATRIX temp;
  int i;

  temp = Mzeros(ACCOM_DIM,SOLO_DIM);
  temp.el[0][0] = 1;
  temp.el[0][1] = length;
  return(temp);
}
  
static MATRIX
evol_matrix_pred_pos2(float length) {
  MATRIX temp;
  int i;

  temp = Mzeros(SOLO_DIM,ACCOM_DIM);
  temp.el[0][0] = 1;
  temp.el[0][1] = length;
  return(temp);
}
  



static BNODE*
alloc_accomp_node(int dim, QUAD_FORM e, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;

  new = alloc_belief_node(ACCOM_DIM, e, type,  index, bn);
  qf = QFpoint(Mzeros(1,1));
  hang = alloc_belief_node(1, qf, ACCOM_OBS_NODE, index, bn);
  A = Mleft_corner(1,ACCOM_DIM);
  add_dir_arc(new,hang,A);   
  return(new);
}



static BNODE*
exper_alloc_accomp_node(int dim, QUAD_FORM e, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;

  new = alloc_belief_node(ACCOM_DIM, e, type,  index, bn);
  qf = QFpoint(Mzeros(1,1));
  hang = alloc_belief_node(1, QFunif(1), ACCOM_OBS_NODE, index, bn);
  A = Mleft_corner(1,ACCOM_DIM);
  add_dir_arc_pot(new,hang,A,qf);   
  return(new);
}

static QUAD_FORM
observation_variance(int i) {
  float var,scale;
  MATRIX m;
  QUAD_FORM qf;

  m = Mzeros(OBS_DIM,1);
  /*  var = .05; */
  /*                     return(QFpoint(m));          */
  scale = TOKENLEN/(float)SR;
    var = score.solo.note[i].obs_var*(scale*scale); 


	  
            qf = QFsphere(OBS_DIM, m, var); 
	    /*       qf = QFindep(OBS_DIM, m, &var);*/

     return(qf);
}

static QUAD_FORM
solo_observation_variance(int i, int ignore_solo, int solo_cue) {
  float var,scale;
  MATRIX m;
  QUAD_FORM qf;



  m = Mzeros(OBS_DIM,1);
  scale = TOKENLEN/(float)SR;
  var = score.solo.note[i].obs_var*(scale*scale); 
#ifdef PARAM_EXPERIMENT
  var *= 5;
#endif
  //  printf("obs var = %f\n",score.solo.note[i].obs_var);
  //  printf("observation variance = %f at %s\n",var,score.solo.note[i].observable_tag);


  if (var <= 0) { printf("bad variance in solo_observation_variance (%f) \n",var); exit(0); }

  //      if (solo_cue) qf = QFsphere(OBS_DIM, m, 1000.);   /* a new experiment */
  //           if (solo_cue) qf = QFsphere(OBS_DIM, m, .01);   /* a new experiment */
  /*             if (solo_cue) qf = QFsphere(OBS_DIM, m, 100.01);   /* a new experiment */
  if (solo_cue) qf = QFsphere(OBS_DIM, m, .01);   /* a new experiment */

  else if (ignore_solo  || var == HUGE_VAL)   qf = QFunif(OBS_DIM);

  /*if (ignore_solo  || var == HUGE_VAL) qf = QFunif(OBS_DIM);
  else if (solo_cue) qf = QFsphere(OBS_DIM, m, 1000.);   /* think about this ---
							    maybe better to disregard
							    the latency in recognizing
							    onset */
  else   qf = QFsphere(OBS_DIM, m, var); 
	     //	     if (ignore_solo) printf("ignoring solo note = %s\n",score.solo.note[i].observable_tag);
	     
  return(qf);
}


#ifdef PARAM_EXPERIMENT
#define MIDI_OBSERVATION_PASSIVE_VARIANCE  .01// .2 //5. /*1.  /*   .01  /*1. */
#else
  #define MIDI_OBSERVATION_PASSIVE_VARIANCE   5. /*1.  /*   .01  /*1. */
#endif
#define MIDI_OBSERVATION_LEAD_VARIANCE .001 // .01  /*1. */


#define MIDI_TRU_OBSERVATION_VARIANCE .00001  /*1. */

static QUAD_FORM
midi_observation_variance(int cue, int accomp_lead, int midi_is_true) {
  float var,scale;
  MATRIX m;
  QUAD_FORM qf;

  m = Mzeros(OBS_DIM,1);
  if (midi_is_true)     var = MIDI_TRU_OBSERVATION_VARIANCE;
  else if (accomp_lead) var = MIDI_OBSERVATION_LEAD_VARIANCE;
  else var = MIDI_OBSERVATION_PASSIVE_VARIANCE;

  /*  var = 5.;*/
  qf = QFsphere(OBS_DIM, m, var);
  return(qf);
}


static QUAD_FORM
midi_observation_variance_parallel(int cue, int accomp_lead, int midi_is_true) {
  float var,scale;
  MATRIX m;
  QUAD_FORM qf;

  m = Mzeros(OBS_DIM,1);
  if (midi_is_true)     var = MIDI_TRU_OBSERVATION_VARIANCE;
  else if (accomp_lead) var = MIDI_OBSERVATION_LEAD_VARIANCE;
  else var = .01;
  /*  var = 5.;*/
  qf = QFsphere(OBS_DIM, m, var);
  return(qf);
}


static BNODE*
alloc_solo_node(int dim, QUAD_FORM e, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;

  new = alloc_belief_node(SOLO_DIM, e, type,  index, bn);
  qf = observation_variance(index);
  hang = alloc_belief_node(OBS_DIM, qf, OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,SOLO_DIM);
  add_dir_arc(new,hang,A);
  return(new);
}

static BNODE*
exper_alloc_solo_node(int dim, QUAD_FORM e, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;

  new = alloc_belief_node(SOLO_DIM, e, type,  index, bn);
  qf = observation_variance(index);
  hang = alloc_belief_node(OBS_DIM, QFunif(OBS_DIM), OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,SOLO_DIM);
  add_dir_arc_pot(new,hang,A,qf);
  return(new);
}


/*static QUAD_FORM
linear_update(A,jn) 
MATRIX A;
JOINT_NORMAL jn; { /* if y = Ax + e (e ~ jn) what is cond dist of y|x
			 expressed as a quadratic form */
/*  QUAD_FORM qf;
  MATRIX var,s11,s12,s21,s22,vcombine(),Mi(),Mm(),Msc(),mcombine(),Mzeros();
  float Mdet();

  qf.m = vcombine(Mzeros(RANDOM_VECT_DIM,1),jn.mean);
  var = Mi(Mm(Mm(A,jn.var),Mt(A)));
  s11 = Mm(Mm(Mt(A),var),A);
  s12 = Msc(Mm(Mt(A),var),-1.);
  s21 = Msc(Mm(var,A),-1.);
  s22 = var;
  qf.S = mcombine(s11,s12,s21,s22);
  Msc(qf.S,-.5);
  qf.c = -( RANDOM_VECT_DIM*my_log(2*PI)+ my_log(Mdet(jn.var))) / 2 ;
  return(qf);
}
*/


       
/*static BNODE*
make_rect_graph(void) {
  int i,j;
  BNODE *ar[2][3];

  bel_num_nodes = 0;
  for (i=0; i < 2; i++)
    for (j=0; j < 3; j++)
      ar[i][j] = alloc_bel_node(RANDOM_VECT_DIM,0.,0,0);
  add_dir_arc(ar[0][0],ar[0][1],evol);
  add_dir_arc(ar[0][0],ar[1][0],evol);
  add_dir_arc(ar[1][0],ar[1][1],evol);
  add_dir_arc(ar[0][1],ar[0][2],evol);
  add_dir_arc(ar[0][2],ar[1][2],evol);
  add_dir_arc(ar[1][1],ar[1][2],evol);
  return(ar[0][0]);
}*/
  
#define PARENT_UNSET -1

int
right_parent(float time, int start, int end) {
  int i=0,bot,top,mid;
  float t;

  bot = start;
  top=end;
  if ((score.solo.note[bot].time + WEENY) > time) return(start);
  if ((score.solo.note[top].time + WEENY) < time) return(PARENT_UNSET);
  while ((top-bot) > 1) {
    mid = (top+bot)/2;
    t = score.solo.note[mid].time;
    if (fabs(t-time) < WEENY) return(mid);
    else if (score.solo.note[mid].time > time) top = mid;
    else bot = mid;
  }
  return(top);
}

int
left_parent(float time, int start, int end) {
  int i=0,bot,top,mid;
  float t;

  bot = start;
  top=end;
  if ((score.solo.note[top].time - WEENY) < time) return(top);
  if ((score.solo.note[bot].time - WEENY) > time) return(PARENT_UNSET);
  while ((top-bot) > 1) {
    mid = (top+bot)/2;
    t = score.solo.note[mid].time;
    if (fabs(t-time) < WEENY) return(mid);
    else if (score.solo.note[mid].time > time) top = mid;
    else bot = mid;
  }
  return(bot);
}

#define ANCHOR 0
#define INTERIOR 1
#define PHANTOM 2



void 
init_score() {
  int i;
  float var[10],t,len;
  QUAD_FORM solo_update,accom_update,qf,accom_update2;
  MATRIX init_solo, init_accom;

    var[0] = .1;
  var[1] = .1;
     solo_update = QFindep(SOLO_DIM,Mzeros(SOLO_DIM,1),var);  
  var[0] = .1;
  var[1] = .3;
  var[2] = 0.;
  var[0] = .1;
  var[1] = 1.;
  var[2] = 0.;  
  /*  var[0] = 0.;
  var[1] = 0.;
  var[2] = 1.;  */

     accom_update = QFindep(ACCOM_DIM,Mzeros(ACCOM_DIM,1),var);  
     accom_update2 = QFsphere(ACCOM_DIM,Mzeros(ACCOM_DIM,1),.01);  
  /*   solo_update = QFunif(SOLO_DIM);  
   accom_update = QFunif(ACCOM_DIM);  */
  score.solo.num = 10;
  for (i=0; i < score.solo.num; i++) {
    len = score.solo.note[i].length = 1;
    if (i == 0) {
      score.solo.note[i].time = 0;
      init_solo = Mzeros(SOLO_DIM,1);
      init_solo.el[0][0] = 5;
      init_solo.el[1][0] = 1;
      score.solo.note[i].qf_update = QFperm(QFunif(SOLO_DIM));
	/*		QFperm(QFpoint(init_solo));*/
      score.solo.note[i].kalman_gain = Mperm(evol_matrix(SOLO_DIM,1.));
    }
    else {
      score.solo.note[i].time = score.solo.note[i-1].time +  len;
      score.solo.note[i].qf_update = QFperm(solo_update);
      score.solo.note[i].kalman_gain = Mperm(evol_matrix(SOLO_DIM,len));
    }
  }
  score.midi.num = 28;
  for (i=0; i < score.midi.num; i++) {
    len = score.midi.burst[i].length = (score.solo.num-1)/(float)(score.midi.num-1); /*.25; */
    if (i == 0) {
      init_accom = Mzeros(ACCOM_DIM,1);
      init_accom.el[1][0] = 10;
      var[0] = /*0.*/HUGE_VAL;
      var[1] = 1.;
      var[2] = 0.;
      score.midi.burst[i].time = 0;
            score.midi.burst[i].qf_update = QFperm(QFindep(ACCOM_DIM,init_accom,var)); 
      /*     score.midi.burst[i].qf_update = QFperm(QFunif(ACCOM_DIM));*/
      /*            score.midi.burst[i].qf_update = QFperm(QFpos_near(ACCOM_DIM));  */
      score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,len));
    }
    else {
      t = score.midi.burst[i].time = score.midi.burst[i-1].time +  len;
      /*      if ((i%4) == 1) score.midi.burst[i].qf_update = QFperm(accom_update); */
      if (i)
	/*      if (i == 1 ||  i == 5 || i == 6)  */
	score.midi.burst[i].qf_update = QFperm(accom_update);
      /*      else if  (i == 5) score.midi.burst[i].qf_update = QFperm(accom_update2); */
      else  score.midi.burst[i].qf_update = QFperm(QFpoint(Mzeros(ACCOM_DIM,1)));
      score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,len));
    }
  }
}

old_init_score() {
  int i;
  float var[10],t,len;
  QUAD_FORM solo_update,accom_update,qf;
  MATRIX init_solo, init_accom;

  var[0] = .1;
  var[1] = .1;
  solo_update = QFindep(SOLO_DIM,Mzeros(SOLO_DIM,1),var);
  var[0] = .1;
  var[1] = .3;
  var[2] = 0.;
  accom_update = QFindep(ACCOM_DIM,Mzeros(ACCOM_DIM,1),var);
  score.solo.num = 6;
  for (i=0; i < score.solo.num; i++) {
    len = score.solo.note[i].length = 1;
    if (i == 0) {
      score.solo.note[i].time = 0;
      init_solo = Mzeros(SOLO_DIM,1);
      init_solo.el[0][0] = 4;
      init_solo.el[1][0] = 1;
      score.solo.note[i].qf_update = /*QFunif(SOLO_DIM);*/
	QFperm(QFpoint(init_solo));
      score.solo.note[i].kalman_gain = Mperm(evol_matrix(SOLO_DIM,1.));
    }
    else {
      score.solo.note[i].time = score.solo.note[i-1].time +  len;
      score.solo.note[i].qf_update = QFperm(solo_update);
      score.solo.note[i].kalman_gain = Mperm(evol_matrix(SOLO_DIM,len));
    }
  }
  score.midi.num = 21;
  for (i=0; i < score.midi.num; i++) {
    len = score.midi.burst[i].length = .25;
    if (i == 0) {
      init_accom = Mzeros(ACCOM_DIM,1);
      init_accom.el[1][0] = 3;
      init_accom.el[2][0] = 3;
      var[0] = .1;
      var[1] = .1;
      var[2] = 0.;
      score.midi.burst[i].time = 0;
      score.midi.burst[i].qf_update = QFperm(QFindep(ACCOM_DIM,init_accom,var));
      score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,len));
    }
    else {
      t = score.midi.burst[i].time = score.midi.burst[i-1].time +  len;
      if ((i%4) == 1) score.midi.burst[i].qf_update = QFperm(accom_update);
      else  score.midi.burst[i].qf_update = QFperm(QFpoint(Mzeros(ACCOM_DIM,1)));
      score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,len));
    }
  }
}

static QUAD_FORM
phantom_variance() {
  float var;
  int i;
  MATRIX m;

    return(QFpoint(Mzeros(PHANTOM_DIM,1)));  
  m = Mzeros(PHANTOM_DIM,1);
  var = .003;
  return(QFsphere(PHANTOM_DIM, m, var));
}

static void 
phantom_matrices(float t1, float t2, float t3, MATRIX *A1, MATRIX *A2) {
  float p,q;

  q = (t3-t1)/(t2-t1);
  p = 1-q;
  *A1 =  Mzeros(PHANTOM_DIM,SOLO_DIM);
  A1->el[0][POS_INDEX] = p;
  *A2 =  Mzeros(PHANTOM_DIM,SOLO_DIM);
  A2->el[0][POS_INDEX] = q;
  /*printf("phantom stuff\n");
Mp(*A1);
Mp(*A2); */
}


static void
diffusion(int start, int end, MATRIX *rA, QUAD_FORM *rqf) {
  /* the dist of the end accomp note given the start accomp note
     has form Ax+e where x is start state and e is a quad form.
     return A and e */
  int i;
  QUAD_FORM qf,e;
  MATRIX A,B;

  A = score.midi.burst[start].kalman_gain;
  qf = score.midi.burst[start+1].qf_update;




  for (i = start+1; i < end; i++) {
    /*    printf("diffusion %d\n",i);
QFprint(qf);  */
    B = score.midi.burst[i].kalman_gain;
    e = score.midi.burst[i+1].qf_update;
    /*printf("i = %d\n",i);
QFprint(qf);
QFprint(e); */
    /*        printf("A = \n");
    Mp(A);
    printf("B = \n");
    Mp(B); */
    A = Mm(B,A);
    /*        printf("afterward A = \n");
    Mp(A); 
            printf("qf before\n");
    QFprint(qf);  */
    qf = QFxform(qf,B);
    /*        printf("qf mid\n");
    QFprint(qf);  */
    qf = QFindep_sum(qf,e);
    /*        printf("qf e\n");
    QFprint(e);*/
    /*    printf("qf after\n");
    QFprint(qf);  */
  }
  *rA = A; 
  *rqf = qf; 
}


static QUAD_FORM
two_opinion(QUAD_FORM qf_left, QUAD_FORM qf_above, MATRIX C_left, MATRIX C_above, 
	    MATRIX *B_left, MATRIX *B_above, int n1, int n2) {
  /* this is taken from split_opinion.  this should eventually be factored about of 
     split_opinion.  

     there are two opinions about the distribution of a node.  
     one opinion is:

     C_left*x + qf_left    where x is some other node.   

     the other opinion is:

     C_above*y + qf_above  where y is some other node.  combine these opinions
     so that the distribution of the new node will be

     B_left*x + B_above*y + qf

     where qf is returned by this routine and B_left and B_above are set by it */
     
    MATRIX A_above,A_left,inter,temp;
    QUAD_FORM qf;
  

    Mint_comp(qf_left.null,qf_above.null,&inter,&temp); 
    if (inter.cols != 0) {
      printf("two intersecting null spaces in two_opinion()\nparent nodes are %d %d\n",n1,n2);
      printf("dimension of intersection is %d\n",inter.cols);
      Mp(qf_left.null);
      Mp(qf_above.null);
      exit(0);
    } 
    qf = matrix_QFplus(qf_above,qf_left,&A_above,&A_left);
    *B_above = Mm(A_above,C_above);
    *B_left = Mm(A_left,C_left);

    /*printf("B_above =\n");
Mp(*B_above);
printf("A_above =\n");
Mp(A_above);
printf("C_above =\n");
Mp(C_above);
exit(0);
printf("B_left =\n");
Mp(*B_left);
printf("qf_above =\n");
QFprint(qf_above);
printf("qf_left =\n");
QFprint(qf_left);
exit(0); */

    qf.m = Ma(Mm(A_above,qf_above.m),Mm(A_left,qf_left.m));
    return(qf);
}





static BNODE*
split_opinion(BNODE *left, BNODE *above,  int acomp_index, int type, BELIEF_NET *bn) {
    QUAD_FORM qf_above,qf_left,qf,e;
    MATRIX A_above,A_left,C_above,C_left,B_above,B_left,A,inter,temp;
    float length;
    BNODE *new;
    int prev_index,i;

    /*printf("split opinion\n"); */
    /* Mp(score.solo.note[1].qf_update.m);*/
                qf_above = QFpos_near(ACCOM_DIM);        
		/*	    qf_above = QFpos_same(ACCOM_DIM); */
    prev_index = left->index; /* index of previous non-interior acc note in score */
    diffusion(prev_index, acomp_index, &C_left, &qf_left);

    /*   printf("index = %d qf_left = \n",acomp_index);
	 QFprint(qf_left);   */

printf("c_left = \n");
Mp(C_left);
printf("qf_left = \n");
QFprint(qf_left); 
printf("qf_above\n");
QFprint(qf_above);
printf("qf_left\n");
QFprint(qf_left); 

 exit(0);
    
    /*          printf("split opinion:\n");
printf("above:\n");
QFprint(qf_above);
printf("left:\n");
QFprint(qf_left);    */
 
   Mint_comp(qf_left.null,qf_above.null,&inter,&temp); 
  if (inter.cols != 0) {
    printf("two intersecting null spaces in split_opinion()\n");
    printf("left = %d above = %d\n",left->node_num, above->node_num);
    printf("dimension of intersection is %d\n",inter.cols);
    Mp(qf_left.null);
    Mp(qf_above.null);
    exit(0);
  } 


    qf = matrix_QFplus(qf_above,qf_left,&A_above,&A_left);


    /*printf("result\n");
QFprint(qf); */


/*if (qf.fin.rows >s 0 && Mnorm(qf.cov) < .01) {
  qf.m = Mzeros(0,0);
  printf("qf\n");
  QFprint(qf);
  exit(0);
}*/
      
    C_above = Mleft_corner(ACCOM_DIM,above->dim);
    B_above = Mm(A_above,C_above);
    B_left = Mm(A_left,C_left);
    qf.m = Ma(Mm(A_above,qf_above.m),Mm(A_left,qf_left.m));
    /*    new = alloc_belief_node(ACCOM_DIM, qf, type,acomp_index,bn);  */
        new = alloc_accomp_node(ACCOM_DIM, qf, type,acomp_index,bn); 
	    add_dir_arc(above,new,B_above);
    add_dir_arc(left,new,B_left); 
    if (B_left.el[0][0] == 0.)
      printf("prev accomp state ignored in cond. dist. for state %d\n",new->node_num);
    if (B_above.el[0][0] == 0.)
      printf("coincident solo state ignored in cond. dist. for state %d\n",new->node_num);

	 /*       printf("left is %d above is %d\n", left->node_num, above->node_num); 
        printf("qf is\n");   
QFprint(qf);  */
    /*       if (left->node_num == 75 && above->node_num == 50) {
printf("qf_above\n");
QFprint(qf_above);
printf("qf_left\n");
QFprint(qf_left); 

printf("A_above = \n");
Mp(A_above);
printf("A_left = \n");
Mp(A_left);
printf("B_above = \n");
Mp(B_above);
printf("B_left = \n");
Mp(B_left); 
exit(0);   
       } */
    return(new);
}







static BNODE*
make_phantom_note(BNODE *lsolo, BNODE *rsolo, BNODE *laccom, int i, BELIEF_NET *bn) {
  float tl,tr,ta,t;
  int il,ir,ia;
  BNODE *phantom,*acc;
  MATRIX A1,A2;
  QUAD_FORM e;
  

  il = lsolo->index;
  ir = rsolo->index;
  ia = laccom->index;
  tl = score.solo.note[il].time;
  tr = score.solo.note[ir].time;
  /*  ta = score.midi.burst[ia].time; */
  t = score.midi.burst[i].time; 
  e = phantom_variance();
  phantom = alloc_belief_node(PHANTOM_DIM, e,PHANTOM_NODE, i,bn);
  phantom_matrices(tl,tr,t,&A1,&A2);
  add_dir_arc(lsolo,phantom,A1);
  add_dir_arc(rsolo,phantom,A2);
  acc = split_opinion(laccom, phantom, i, PHANTOM_CHILD_NODE, bn);
  return(acc);
}
  
static BNODE*
make_straggling_note(BNODE *pred, int i, BELIEF_NET *bn) {
  QUAD_FORM e;
  BNODE *strag;
  MATRIX A;
  

  e = score.midi.burst[i].qf_update;
  /*    printf("straggling e:\n");
  QFprint(e); */
    /*QFpoint(Mzeros(ACCOM_DIM,1));*/
  strag = alloc_accomp_node(ACCOM_DIM, e,STRAGGLE_ACCOM_NODE, i,bn);
  A = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
  /*  printf("length is %f\n", score.midi.burst[i-1].length);
  printf("matrix A is\n");
  Mp(A); */
  add_dir_arc(pred,strag,A);
  return(strag);
}
  
  



#define MAX_INTERIOR_CHAIN 100

static void
make_interior_chain(BNODE *la, BNODE *ra, BELIEF_NET *bn) {
  /* assume the global score named "score".  first and last are the
     accompaniment notes that bound the interior section of the graph.
     la and ra are the left and right anchor nodes */

  /* assume x(n) = A(n-1)*x(n-1) + e(n)  ==> p(x(n)|x(n-1)) viewed as
   a function of x(n-1) is N(inv(A)(x(n)-m(n), inv(A)*sig*inv(A')) */
  QUAD_FORM qf[MAX_INTERIOR_CHAIN],*beta,ud,temp,alpha,eqf;
  MATRIX mat[MAX_INTERIOR_CHAIN],*C,A,B1,B2;
  int i,first,last;
  BNODE *new,*prev;
  /* the backward probs beta(i) are N(beta[i].m+C[i]*x(last),beta[i].sigma) */


  /*printf("make_interior: left = %d right = %d\n",la->node_num,ra->node_num);*/


  first = la->index;
  last = ra->index;

  if (last+1-first > MAX_INTERIOR_CHAIN) {
    printf("can handle interior chain > %d\n",MAX_INTERIOR_CHAIN);
    exit(0);
  }
  beta = qf - first;
  C = mat - first;
  C[last] = Miden(ACCOM_DIM); /* really should get this dim from score note */
  beta[last] = QFpoint(Mzeros(ACCOM_DIM,1));
  for (i = last-1; i > first; i--) { /* compute backward probs */
    A = Mi(score.midi.burst[i].kalman_gain);
    ud = score.midi.burst[i+1].qf_update;
    C[i] = Mm(A,C[i+1]);
    ud.m = Msc(ud.m,-1.);
    temp = QFindep_sum(beta[i+1],ud);
    /*    printf("i = %d temp = \n",i);
QFprint(temp);  */

    beta[i] = QFxform(temp,A);
    /*    printf("beta[%d] = \n",i);
QFprint(beta[i]);  */
  }
  prev = la;
  for (i=first+1; i < last; i++) {
    A = score.midi.burst[i-1].kalman_gain;
    alpha = score.midi.burst[i].qf_update;
    eqf = matrix_QFplus(alpha,beta[i], &B1, &B2);
    eqf.m = Ma(Mm(B1,alpha.m),Mm(B2,beta[i].m));
    new = alloc_accomp_node(ACCOM_DIM, eqf,INTERIOR_NODE, i,bn); 
      /*    new = alloc_belief_node(ACCOM_DIM, eqf,INTERIOR_NODE, i,bn); */
    add_dir_arc(prev,new,Mm(B1,A));
    add_dir_arc(ra,new,Mm(B2,(C[i])));
    /*printf("left matrix\n");
Mp(Mm(B1,A));
printf("right matrix\n");
Mp(Mm(B2,C[i])); */
    prev = new;
  }
}
  
static void
make_tail(BNODE *left, int start, int end, BELIEF_NET *bn) {
  float length;
  int i;
  BNODE *last,*new;
  QUAD_FORM qf;
  MATRIX A;

  last = left;
  for (i= start; i <= end; i++) {
    qf = score.midi.burst[i].qf_update;
    A = score.midi.burst[i-1].kalman_gain;
    new = alloc_belief_node(ACCOM_DIM, qf, TAIL_NODE,i,bn);
    add_dir_arc(last,new,A);
  }
}
    
static void
make_head(BNODE *right, int start, int end, BELIEF_NET *bn) {
  int i;
  BNODE *last,*new;
  QUAD_FORM qf;
  MATRIX A;

  last = right;
  for (i= end; i >= start; i--) {
    qf = score.midi.burst[i+1].qf_update;
    qf.m = Msc(qf.m,-1.);
    A = score.midi.burst[i].kalman_gain;
    A = Mi(A);
    qf = QFxform(qf,A);
    new = alloc_belief_node(ACCOM_DIM, qf, HEAD_NODE,i,bn);
    add_dir_arc(last,new,A);
  }
}
    
static BNODE*
accom_hang_off(BNODE *solo, int index, BELIEF_NET *bn) {
  int i,n1,n2;
  BNODE *last,*new,*phant,*init;
  QUAD_FORM qf,near,qf_left,qf_above,e,qf_init;
  MATRIX A,I,A_above,A_left,B_left,B_above;

  
  qf_left = score.midi.burst[index].qf_update;
  /* printf("qf_left\n");
QFprint(qf_left);
printf("index = %d\n",index);
exit(0);  */
  qf_init = QFpoint(Mzeros(ACCOM_DIM,1));
  /*  init = alloc_belief_node(ACCOM_DIM, qf_init, PHANTOM_NODE, index,bn); */
    init = alloc_belief_node(ACCOM_DIM, qf_init, ACCOM_INIT_NODE, index,bn); 
/* this root influences the accomp note */


  /*  A_above = Mleft_corner(ACCOM_DIM,solo->dim);
  qf_above = QFpos_near(ACCOM_DIM);   */



  A_above = Mpartial_iden(ACCOM_DIM, SOLO_DIM);
  qf_above = QFpos_tempo_near();



    /*         qf_above = QFpos_same(ACCOM_DIM);   */
  A_left = Miden(ACCOM_DIM);
  n1 = solo->node_num;
  n2 = init->node_num;
  e = two_opinion(qf_left, qf_above,  A_left, A_above,  &B_left, &B_above,n1,n2);
  /*   printf("result of accom_hang_off\n");
  QFprint(e);
printf("B_left\n");
Mp(B_left);
printf("B_above\n");
Mp(B_above);
exit(0);   */
  new = alloc_accomp_node(ACCOM_DIM, e, ANCHOR_NODE,index, bn);
  add_dir_arc(solo,new,B_above);
  add_dir_arc(init,new,B_left); 
  return(new);


  /*  near = QFpos_near(ACCOM_DIM);
  qf = score.midi.burst[index].qf_update;
  qf = QFplus(qf,near);
  new = alloc_accomp_node(ACCOM_DIM, qf, ANCHOR_NODE,index, bn);
  A = Mleft_corner(ACCOM_DIM,solo->dim);
  add_dir_arc(solo,new,A);
  return(new); */
}
    

static void
init_belief_net(BELIEF_NET *bn) {
     bn->num = 0;
     bn->list = (BNODE **) malloc(MAX_BELIEF_NODES*sizeof(BNODE *));
     bn->potential.num = 0;
     bn->potential.el = (CLIQUE **) malloc(MAX_BELIEF_NODES*sizeof(CLIQUE *));
}


static int
is_interior_boundary(BNODE *b) {
  return(b->note_type == ANCHOR_NODE || b->note_type == PHANTOM_CHILD_NODE || 
	 b->note_type == STRAGGLE_ACCOM_NODE);
} 


void
add_interior_nodes(BELIEF_NET *bn) {
  int i,j;
  BNODE *b,*c;

  for (i=0; i < bn->num; i++) { /* ???? this is running over both solo and accomp */
    /*printf("node type of %d is %d\n",i ,    bn->list[i]->note_type);*/
    b = bn->list[i];
    if (is_interior_boundary(b)) {
      for (j=0; j < b->next.num; j++) {
	c = b->next.arc[j];
	if (is_interior_boundary(c)) {
	  if (b->index+1 != c->index) {
	    make_interior_chain(b,c, bn); 
	  }
	}
      }
    }
  }
}



static void
connect_belief_net_with_score(BELIEF_NET bn) {
  int i,s;

  /* i think just observation nodes are connected currently */

  for (i=0; i < bn.num; i++) {
    s = bn.list[i]->index;
    if (s > score.solo.num) {
      printf("solo index out of range s = %d\n",s);
      exit(0);
    }
    /*    printf("i = %d type = %d index = %d \n",i,bn.list[i]->note_type,s);  */
    if (bn.list[i]->note_type == OBS_NODE) {
      score.solo.note[s].belief = bn.list[i];
      /*     printf("i = %d s = %d belief = %d\n",i,s,bn.list[i]); */
      score.solo.note[s].observe = bn.list[i];
    }
    if (bn.list[i]->note_type == ACCOM_OBS_NODE) {
      score.midi.burst[s].belief = bn.list[i];
    }
    if  (bn.list[i]->note_type == SOLO_NODE) {
      /*        printf("i = %d type = %d index = %d  time = %f\n",i,bn.list[i]->note_type,s,score.solo.note[s].time);   */
      /*      score.solo.note[s].update_node = bn.list[i]; */
      score.solo.note[s].hidden = bn.list[i];
    }
    if  (bn.list[i]->note_type == SOLO_UPDATE_NODE) 
      score.solo.note[s].update_node = bn.list[i];
    if  (bn.list[i]->note_type == SOLO_PHRASE_START) {
      score.solo.note[s].update_node = bn.list[i];
    }
    if  (bn.list[i]->note_type == ANCHOR_NODE  || bn.list[i]->note_type == INTERIOR_NODE || bn.list[i]->note_type == STRAGGLE_ACCOM_NODE || bn.list[i]->note_type ==  PHANTOM_CHILD_NODE) 
{
      score.midi.burst[s].hidden = bn.list[i];
    }
  }
}
  

static void
connect_belief_net_with_the_score(BELIEF_NET bn) {
  int i,s;


  for (i=0; i < bn.num; i++) {
    if (bn.list[i]->note_type != OBS_NODE &&
	bn.list[i]->note_type != ACCOM_OBS_NODE) continue;
	s = bn.list[i]->index;
	/*    printf("i = %d type = %d index = %d \n",i,bn.list[i]->note_type,s);  */
    if (bn.list[i]->note_type == OBS_NODE) {
      if (s > score.solo.num) {
	printf("the solo index out of range s = %d\n",s);
	exit(0);
      }
      score.solo.note[s].belief = bn.list[i];
      //      printf("set %d to %d\n",s,bn.list[i]);
      score.solo.note[s].observe = bn.list[i];
    }
	if (bn.list[i]->note_type == ACCOM_OBS_NODE)
      score.midi.burst[s].belief = bn.list[i];
  }
}
  

  

    
static BELIEF_NET
make_score_graph() {
  int i,r,l,rr,ll;
  BNODE *solo[MAX_NOTES_PER_PHRASE],*accom[MAX_NOTES_PER_PHRASE],*obs,*phantom,*last;
  BNODE *next,*first_accomp;
  int type[MAX_NOTES_PER_PHRASE];
  int lp[MAX_NOTES_PER_PHRASE];
  int rp[MAX_NOTES_PER_PHRASE],typ;
  float time,length;
  MATRIX first_coord,mat,A1,A2,C,C1,B1,C2,B2;
  QUAD_FORM qf1,qf2,qf,e;
  BELIEF_NET bn;


  init_belief_net(&bn);
  for (i=0; i < score.midi.num; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,0,score.solo.num-1);
    lp[i] = left_parent(score.midi.burst[i].time,0,score.solo.num-1);
    /*    printf("%d %d %d\n",i,lp[i],rp[i]); */
  }


  bel_num_nodes = 0; /* maybe not needed */
  for (i=0; i < score.solo.num; i++) {
    e = score.solo.note[i].qf_update; /* both first and others */
    /*   solo[i] = alloc_belief_node(SOLO_DIM, e,SOLO_NODE, i, &bn);*/
    solo[i] = alloc_solo_node(SOLO_DIM, e,SOLO_NODE, i, &bn);
    /*    e = observation_variance();
    obs = alloc_belief_node(OBS_DIM, e, OBS_NODE,i, &bn);
    first_coord = Mleft_corner(OBS_DIM,SOLO_DIM);
    add_dir_arc(solo[i],obs,first_coord);*/
    if (i > 0)  {
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i].length);
      add_dir_arc(solo[i-1],solo[i],mat);
    }
  }

  if (rp[0]==rp[1]) {
    printf("haven't implemented accomp note at start of phrase\n");
    exit(0);
  }
  else if (rp[0]==lp[0]) first_accomp = accom_hang_off(solo[rp[0]], 0, &bn);
  else {
    printf("haven't implemented phantom at start of phrase\n");
    exit(0);
  }
  last = first_accomp;
  for (i=1; i < score.midi.num; i++) {
    if (rp[i]==lp[i])  /* coincident with solo note (an anchor) */
      last = split_opinion(last, solo[rp[i]], i, ANCHOR_NODE, &bn);
    else if (rp[i] == rp[i+1] && lp[i] == lp[i-1]) { /* an interior node */
      /* yes this is blank */
    }
    else /* a phantom node */ {
      last = make_phantom_note(solo[lp[i]], solo[rp[i]], last, i, &bn);
    }
  }
  
  add_interior_nodes(&bn);
  /*  last = first_accomp;
  while (last->next.num != 0) {
    for (i=0; i < last->next.num; i++) {
      next = last->next.arc[i];
      if (next->note_type == ANCHOR_NODE) {
	printf("true\n");
	make_interior_chain(last,next, &bn); 
	last = next;
      }
    }
  }*/
  return(bn);
}


static BELIEF_NET
make_phrase_graph(int phrase) {
  int firstnote,lastnote,i,firsta,lasta;
  int lpx[MAX_NOTES_PER_PHRASE],*lp;
  int rpx[MAX_NOTES_PER_PHRASE],*rp;
  BNODE *solox[MAX_NOTES_PER_PHRASE],**solo,*first_accomp,*last;
  MATRIX mat;
  BELIEF_NET bn;
  QUAD_FORM e;


  init_belief_net(&bn);
  phrase_span(phrase,&firstnote,&lastnote,&firsta,&lasta);
  /*lastnote = 1;
lasta = 1;*/
printf("solo = %d %d accomp = %d %d\n",firstnote,lastnote,firsta,lasta);
  if (lastnote+1-firstnote > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in phrase %d\n",phrase);
    exit(0);
  }
  if (lasta+1-firsta > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in phrase %d\n",phrase);
    exit(0);
  }
  lp = lpx - firsta;
  rp = rpx - firsta;
  solo = solox - firstnote;
  for (i=firsta; i <= lasta; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,firstnote,lastnote);
    lp[i] = left_parent(score.midi.burst[i].time,firstnote,lastnote);
    /*    printf("rp[%d] = %d lp[%d] = %d\n",i,rp[i],i,lp[i]); */
  }
  for (i=firstnote; i <= lastnote; i++) {
    e = score.solo.note[i].qf_update; /* both first and others */
    if (score.solo.note[i].cue) e = QFmake_pos_unif(e);
    solo[i] = exper_alloc_solo_node(SOLO_DIM, e,SOLO_NODE, i, &bn);
    if (i > firstnote)  {
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      add_dir_arc(solo[i-1],solo[i],mat);
    }
  }
  /*  return(bn); for only solo*/
  if (rp[firsta]==rp[firsta+1]) {
    printf("haven't implemented accomp note at start of phrase\n");
    exit(0);
  }
  else if (rp[firsta]==lp[firsta]) 
    first_accomp = accom_hang_off(solo[rp[firsta]], firsta, &bn);
  else {
    printf("haven't implemented phantom at start of phrase\n");
    exit(0);
  }
  last = first_accomp;
  for (i=firsta+1; i <= lasta; i++) {
    if (rp[i]==lp[i])  {/* coincident with solo note (an anchor) */
      last = split_opinion(last, solo[rp[i]], i, ANCHOR_NODE, &bn);
      /*      printf("%d is an anchor\n",i); */
    }
    else if (rp[i] == -1) {/* straggling accompaiment notes */
      last = make_straggling_note(last, i, &bn);
      /*      printf("%d is a straggler\n",i);*/
    }
    else if (rp[i] == rp[i+1] && lp[i] == lp[i-1]) { /* an interior node */
      /* yes this is blank */
      /*      printf("%d is an interior\n",i);*/
    }
    else /* a phantom node */ {
      last = make_phantom_note(solo[lp[i]], solo[rp[i]], last, i, &bn);
      /*      printf("%d is a phantom\n",i); */
    }
  }
    add_interior_nodes(&bn); 
  return(bn);
}


static int 
long_rest(i) { /* is ith solo note a "long rest" */
  //  if (score.solo.note[i].num != RESTNUM) return(0);
  if (is_a_rest(score.solo.note[i].num) == 0) return(0);
  if (score.solo.note[i].length < 1.) return(0);
  return(1);
}

static BELIEF_NET
make_indep_phrase_graph(int phrase) {  /* graph for network in which accomp and solo are indep */
  int firstnote,lastnote,i,firsta,lasta,coincides,type;
  int lpx[MAX_NOTES_PER_PHRASE],*lp;
  int rpx[MAX_NOTES_PER_PHRASE],*rp;
  BNODE *solox[MAX_NOTES_PER_PHRASE],**solo,*first_accomp,*last;
  BNODE *accox[MAX_NOTES_PER_PHRASE],**acco;
  MATRIX mat;
  BELIEF_NET bn;
  QUAD_FORM e,prior;
  float len;

  init_belief_net(&bn);
  phrase_span(phrase,&firstnote,&lastnote,&firsta,&lasta);
  printf("solo = %d %d accomp = %d %d\n",firstnote,lastnote,firsta,lasta);
  if (lastnote+1-firstnote > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in phrase %d\n",phrase);
    exit(0);
  }
  if (lasta+1-firsta > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in phrase %d\n",phrase);
    exit(0);
  }
  lp = lpx - firsta;
  rp = rpx - firsta;
  solo = solox - firstnote;
  acco = accox - firsta;
  for (i=firstnote; i <= lastnote; i++) {
    if (score.solo.note[i].cue && i != firstnote)  {
      printf("assumption not met\n");
      exit(0);
    }
    e = score.solo.note[i].qf_update; /* both first and others */
    if (score.solo.note[i].cue)  {
      /*      printf(" ii = %d\n",i); */
      prior = QFmake_pos_unif(e);
      solo[i] = exper_alloc_solo_node(SOLO_DIM, prior,SOLO_NODE, i, &bn);
    }
    else if (0/*long_rest(i-1)*/) {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
    }
    else {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      add_dir_arc_pot(solo[i-1],solo[i],mat,e);
    }
  }
  for (i=firsta; i <= lasta; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,firstnote,lastnote);
    lp[i] = left_parent(score.midi.burst[i].time,firstnote,lastnote);
    coincides =  (rp[i] == lp[i]); 
    e = score.midi.burst[i].qf_update; /* both first and others */
    prior = score.midi.burst[i].prior; 
    if (i == firsta) e = QFmake_pos_unif(e);
    type = (coincides) ? ANCHOR_NODE : INTERIOR_NODE;
    
    acco[i] = exper_alloc_accomp_node(ACCOM_DIM, prior/*QFunif(ACCOM_DIM)*/, type,i, &bn);
    if (i > firsta)  {
      mat = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
      add_dir_arc_pot(acco[i-1],acco[i],mat,e);
    }
    if (coincides) {
      mat = Mzeros(ACCOM_DIM,  SOLO_DIM);
      mat.el[0][0] = 1;
      e = QFpos_near(ACCOM_DIM);
      add_dir_arc_pot(solo[rp[i]],acco[i],mat,e);
    }
  }
  return(bn);
}


static int
sandwich(int i, int left, int rite) {
  int one,two;

  if (left < 0) return(0);
  if (i==0 || i == (score.midi.num-1)) return(0);
  if (left == rite) return(0);
  if (score.midi.burst[i-1].coincides) return(0);
  one = (score.midi.burst[i-1].time < score.solo.note[left].time);
  two = (score.midi.burst[i+1].time > score.solo.note[rite].time);
  return(one);
}

static int
right_sandwich(int i, int left, int rite) {
  int one,two;

  if (rite < 0) return(0);
  if (i==0 || i == (score.midi.num-1)) return(0);
  if (left == rite) return(0);
  if (score.midi.burst[i-1].coincides) return(0);
  if (is_a_rest(score.solo.note[rite].num)) return(0);
  one = (score.midi.burst[i-1].time < score.solo.note[left].time);
  two = (score.midi.burst[i+1].time > score.solo.note[rite].time);
  return(two);
}

static int
vertical_connect(int i, int left, int rite) {

  if (left != rite) return(0);
  if (is_a_rest(score.solo.note[left].num) == 0) return(1);
  if (score.solo.note[left].cue) return(1);
  return(0);
}


static BELIEF_NET
make_indep_graph() {  /* graph for network in which accomp and solo are indep */
  int firstnote,lastnote,i,firsta,lasta,coincides,type;
  int lpx[MAX_NOTES_PER_PHRASE],*lp;
  int rpx[MAX_NOTES_PER_PHRASE],*rp;
  BNODE *solox[MAX_NOTES_PER_PHRASE],**solo,*first_accomp,*last;
  BNODE *accox[MAX_NOTES_PER_PHRASE],**acco;
  MATRIX mat;
  BELIEF_NET bn;
  QUAD_FORM e,prior;
  float len,gap;

  init_belief_net(&bn);
  firstnote = 0; 
  lastnote = score.solo.num-1;
  firsta = 0;
  lasta = score.midi.num-1;
  if (score.solo.num > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in score\n");
    exit(0);
  }
  if (score.midi.num > MAX_NOTES_PER_PHRASE) {
    printf("too many accomp notes in score\n");
    exit(0);
  }
  lp = lpx;
  rp = rpx;
  solo = solox;
  acco = accox;
  for (i=0; i < score.solo.num; i++) {
    e = score.solo.note[i].qf_update; /* both first and others */
    if (score.solo.note[i].cue)  {
      prior = QFmake_pos_unif(e);
      solo[i] = exper_alloc_solo_node(SOLO_DIM, prior,SOLO_NODE, i, &bn);
    }
    else if (0/*long_rest(i-1)*/) {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
    }
    else {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      add_dir_arc_pot(solo[i-1],solo[i],mat,e);
    }
  }
  for (i=0; i < score.midi.num; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,firstnote,lastnote);
    lp[i] = left_parent(score.midi.burst[i].time,firstnote,lastnote);
    coincides =  (rp[i] == lp[i]); 
    e = score.midi.burst[i].qf_update; /* both first and others */
    prior = score.midi.burst[i].prior; 
    /*    if (i == firsta) e = QFmake_pos_unif(e); */
    type = (coincides) ? ANCHOR_NODE : INTERIOR_NODE;
    
    acco[i] = exper_alloc_accomp_node(ACCOM_DIM, prior/*QFunif(ACCOM_DIM)*/, type,i, &bn);
    if (i > firsta && score.midi.burst[i].connect)  {
      mat = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
      add_dir_arc_pot(acco[i-1],acco[i],mat,e);
    }
    if (vertical_connect(i,lp[i],rp[i])) {
	  /*    if (coincides ) {*/
      mat = Mzeros(ACCOM_DIM,  SOLO_DIM);
      mat.el[0][0] = 1;
      e  = QFpos_near(ACCOM_DIM);
      add_dir_arc_pot(solo[rp[i]],acco[i],mat,e);
    }
    if (sandwich(i,lp[i],rp[i])) {
         printf("sandwich at %f\n",score.midi.burst[i].time);
      gap = score.midi.burst[i].time - score.solo.note[lp[i]].time;
      mat = evol_matrix_pred_pos(gap);
      e = QFpos_near(ACCOM_DIM);
            e = QFpos_var(ACCOM_DIM, gap*5.);
      add_dir_arc_pot(solo[lp[i]],acco[i],mat,e);
      }
    if (right_sandwich(i,lp[i],rp[i])) {
      /*      printf("right sandwich at %f\n",score.midi.burst[i].time);*/
      gap =  score.solo.note[rp[i]].time - score.midi.burst[i].time;
      mat = evol_matrix_pred_pos2(gap);
      /*      e = QFpos_near(SOLO_DIM);*/
      e = QFpos_var(SOLO_DIM, gap*5.);
      /*      QFpos_var(gap*5.);*/
      /*      printf("here is e1\n");
      QFprint(e);
      printf("here is e2\n");
      QFprint(QFpos_var(gap*5.));*/

      /*      printf("time = %f\n",score.solo.note[rp[i]].time); */
	      
      add_dir_arc_pot(acco[i],solo[rp[i]],mat,e);

      printf("made change here wiht above\n");
      /*      if (strcmp("habanera",scorename) != 0)  
	add_dir_arc_pot(acco[i],solo[rp[i]],mat,e);
      else if  (fabs(score.solo.note[rp[i]].time-28.75) > .001)
      add_dir_arc_pot(acco[i],solo[rp[i]],mat,e);*/
      }
  }
  return(bn);
}


static BELIEF_NET
composite_make_indep_graph() {  /* graph for network in which accomp and solo are indep */
  int firstnote,lastnote,i,firsta,lasta,coincides,type;
  int lpx[MAX_NOTES_PER_PHRASE],*lp;
  int rpx[MAX_NOTES_PER_PHRASE],*rp,pred,next;
  BNODE *solox[MAX_NOTES_PER_PHRASE],**solo,*first_accomp,*last;
  BNODE *accox[MAX_NOTES_PER_PHRASE],**acco;
  MATRIX mat;
  BELIEF_NET bn;
  QUAD_FORM e,prior;
  float len,gap;

  init_belief_net(&bn);
  firstnote = 0; 
  lastnote = score.solo.num-1;
  firsta = 0;
  lasta = score.midi.num-1;
  if (score.solo.num > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in score\n");
    exit(0);
  }
  if (score.midi.num > MAX_NOTES_PER_PHRASE) {
    printf("too many accomp notes in score\n");
    exit(0);
  }
  lp = lpx;
  rp = rpx;
  solo = solox;
  acco = accox;
  for (i=0; i < score.solo.num; i++) {
    e = score.solo.note[i].qf_update; /* both first and others */
    if (score.solo.note[i].cue)  {
      prior = QFmake_pos_unif(e);
      solo[i] = exper_alloc_solo_node(SOLO_DIM, prior,SOLO_NODE, i, &bn);
    }
    else if (0/*long_rest(i-1)*/) {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
    }
    else {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      add_dir_arc_pot(solo[i-1],solo[i],mat,e);
    }
  }
  for (i=0; i < score.midi.num; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,firstnote,lastnote);
    lp[i] = left_parent(score.midi.burst[i].time,firstnote,lastnote);
    coincides =  (rp[i] == lp[i]); 
    e = score.midi.burst[i].qf_update; /* both first and others */
    prior = score.midi.burst[i].prior; 
    /*    if (i == firsta) e = QFmake_pos_unif(e); */
    type = (coincides) ? ANCHOR_NODE : INTERIOR_NODE;
    
    acco[i] = exper_alloc_accomp_node(ACCOM_DIM, prior/*QFunif(ACCOM_DIM)*/, type,i, &bn);
    if (vertical_connect(i,lp[i],rp[i])) {
      mat = Mzeros(ACCOM_DIM,  SOLO_DIM);
      mat.el[0][0] = 1;
      e  = QFpos_near(ACCOM_DIM);
      add_dir_arc_pot(solo[rp[i]],acco[i],mat,e);
    }
    pred = (lp[i] == rp[i]) ? lp[i] - 1 : lp[i];
    next = (lp[i] == rp[i]) ? rp[i] + 1 : rp[i];
    if (i > 0 && score.midi.burst[i-1].time  > score.solo.note[pred].time-.001) {
      mat = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
      add_dir_arc_pot(acco[i-1],acco[i],mat,e);
    }
    else if (rp[i] != lp[i]) {
      gap = score.midi.burst[i].time - score.solo.note[pred].time;
      mat = evol_matrix_pred_pos(gap);
      e = QFpos_near(ACCOM_DIM);
      add_dir_arc_pot(solo[pred],acco[i],mat,e);
      /*      printf("gap = %f time = %f\n", gap,score.midi.burst[i].time);
      Mp(mat);
      QFprint(e);
      exit(0); */
    }
    if (i < score.midi.num-1 && rp[i] > 0 && rp[i] != lp[i]) {
      if ( score.midi.burst[i+1].time > score.solo.note[next].time+.001) {
	gap =  score.solo.note[next].time - score.midi.burst[i].time;
	mat = evol_matrix_pred_pos2(gap);
	e = QFpos_near(SOLO_DIM);
	add_dir_arc_pot(acco[i],solo[next],mat,e);
      }
    }
  }
  return(bn);
}



BELIEF_NET
old_make_solo_graph(int start_note, int end_note) {
  int i,nl[SOLO_DIM];
  BNODE *last,*this,*update;
  MATRIX mat,tempo;
  QUAD_FORM e,z,ni;
  BELIEF_NET bn;
  float x = 1,v=1000;


  init_belief_net(&bn);
  for (i=0; i < SOLO_DIM; i++) nl[i] = (i != POS_INDEX);
  z = QFpoint(Mzeros(SOLO_DIM,1));
  ni = QFnull_inf(SOLO_DIM, nl);
  tempo = Mzeros(SOLO_DIM,SOLO_PHRASE_UPDATE_DIM);
  tempo.el[1][0] = 1; /* this matrix selects the 1st comp to the 2nd component */
  /*  tempo.el[0][0] = 1; /* this matrix selects the 1st comp to the 2nd component */
  for (i=start_note; i <= end_note; i++) {
    if (i > start_note)  {
      e = score.solo.note[i].qf_update; /* both first and others */
      /*      e.cov = Msc(e.cov,100000.);
      e.S = Msc(e.S,.00001);

printf("need to undo this\n"); */
      this  = alloc_solo_node(SOLO_DIM, z,SOLO_NODE, i, &bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      add_dir_arc(last,this,mat);
      update  = alloc_belief_node(SOLO_DIM, e,SOLO_UPDATE_NODE, i, &bn);
      add_dir_arc(update,this,Miden(SOLO_DIM));
    }
    else {
      /*      this  = alloc_solo_node(SOLO_DIM, ni,SOLO_NODE, i, &bn); */
      /*      e = QFunif(SOLO_PHRASE_UPDATE_DIM);  */
      e = score.solo.note[i].qf_update; /* both first and others */
      this  = alloc_solo_node(SOLO_DIM, e,SOLO_PHRASE_START, i, &bn);
      /*      update  = alloc_belief_node(SOLO_PHRASE_UPDATE_DIM, e,SOLO_UPDATE_NODE, i, &bn); */
      /*      add_dir_arc(update,this,tempo);*/
    }
    last = this;
  }
  return(bn);
}




      

void
simulate_from_graph(BELIEF_NET bn) {
  int i,done = 0,ready,j;
  MATRIX u,A;
  BNODE *b;
 
  if (bn.num > MAX_BELIEF_NODES) {
    printf("too many nodes in belief net\n");
    exit(0);
  }
  for (i=0; i < bn.num; i++) bn.list[i]->obs.observed = 0;
  while (done == 0) {
    done = 1;
    for (i=0; i < bn.num; i++) if (bn.list[i]->obs.observed == 0) {
      ready = 1;
      for (j=0; j < bn.list[i]->prev.num; j++)
	if (bn.list[i]->prev.arc[j]->obs.observed == 0) ready = 0;
      if (ready) {
	/*	printf("I is %d\n",bn.list[i]->node_num);*/
	u = rand_gauss(bn.list[i]->e);
	/*	printf("initial value\n");
	Mp(u);*/
	for (j=0; j < bn.list[i]->prev.num; j++) {
	  /*	  printf("I am %d dad is %d\n",bn.list[i]->node_num, bn.list[i]->prev.arc[j]->node_num); */
	  A =  bn.list[i]->prev.A[j]; 
	  /*	  Mp(A);  */
	  b =  bn.list[i]->prev.arc[j];
	  u = Ma(u,Mm(A,b->obs.value));
	  /*	  Mp(u); */
	}
	bn.list[i]->obs.value = Mperm(u);
	/*printf("u = \n");
Mp(u); */
	bn.list[i]->obs.observed = 1;
	done = 0;
      }
    }
  }
}
	  
	  
    
	
	
    
  



#define NUM_MUSIC_NODES  30  

static BNODE*
make_music_graph(void) {
  int i,j,k;
  BNODE *solo[NUM_MUSIC_NODES];
  BNODE *obs[NUM_MUSIC_NODES];
  SUPERMAT mean,cov;
  JOINT_NORMAL jn;
  SUPER_QUAD sqf;
  MATRIX m,c,v,U,D,solo_update,first_coord,null,inf,fin;
  QUAD_FORM con,temp;
  

  bel_num_nodes = 0;
  first_coord = Mzeros(1,SOLO_DIM);
  first_coord.el[0][0] = 1;
  solo_update = Miden(SOLO_DIM);
  solo_update.el[0][1] = 1;
  for (i=0; i < NUM_MUSIC_NODES; i++) {
    solo[i] = alloc_bel_node(SOLO_DIM, (float)i,SOLO_NOTE_TYPE,0);
    if (i == 0) {
      /*            m =  Mzeros(SOLO_DIM,1);*/
      /*      m = Mperm_a
lloc(SOLO_DIM,1);
      Mset_zero(m);*/
	    /*      m.el[1][0] = 0.;  
      U = Miden(SOLO_DIM); 
      
      D = Mzeros(SOLO_DIM,SOLO_DIM);
      D.el[1][1] = D.el[0][0] = HUGE_VAL;  
      solo[i]->e = QFset(m,U,D); */



      solo[i]->e = QFinit(SOLO_DIM);         
      QFcopy(QFunif(SOLO_DIM), &(solo[i]->e)); 
    }
    else  {
      add_dir_arc(solo[i-1],solo[i],solo_update);
      m =  Mzeros(SOLO_DIM,1);
      /*      U = Miden(SOLO_DIM);
      D = Mzeros(SOLO_DIM,SOLO_DIM);
      if ((i%10) == 0) D.el[1][1] = D.el[0][0] = HUGE_VAL;  
      else D.el[1][1] = D.el[0][0] = 0.; 
      solo[i]->e = QFset(m,U,D); */


      temp  = ((i%10) == 0) ? QFunif(SOLO_DIM) : QFpoint(m);
      solo[i]->e = QFinit(SOLO_DIM);         
      QFcopy(temp, &(solo[i]->e)); 
    }
  }
  for (i=0; i < NUM_MUSIC_NODES; i++) {
    obs[i] = alloc_bel_node(1,(float)i,OBS_NOTE_TYPE,0);
    add_dir_arc(solo[i],obs[i],first_coord);
    m =  Mzeros(1,1);
    /*    U = Miden(1);
    D = Miden(1);
    obs[i]->e = QFset(m,U,D); */
    obs[i]->obs.observed = 1; /*((i%5) == 0);*/
    obs[i]->obs.pos = sin(.1*i);

    obs[i]->e = QFinit(1);         
    QFcopy(QFsphere(1,m,1.), &(obs[i]->e));  

    
  }
  return(solo[0]);
}
  


/*#define HMM_LEN 10



static BNODE*
make_hmm_graph(void) {
  int i;
  BNODE *cur,*last=NULL,*out,*root;

  bel_num_nodes = 0;
  for (i=0; i < HMM_LEN; i++) {
    cur = alloc_bel_node(RANDOM_VECT_DIM,0.,0,0);
    if (i == 0) root = cur;
    out = alloc_bel_node(RANDOM_VECT_DIM,0.,0,0);
    add_dir_arc(cur,out,evol);
    if (i > 0)  add_dir_arc(last,cur,evol);
    last = cur;
  }
  return(root);
}

*/

static int
count_dag_graph_nodes(BNODE *root)
             { 
  int i,sum;

  if (root->next.num == 0) return(1);
  sum = 1;
  for (i=0; i < root->next.num; i++)
    sum += count_dag_graph_nodes(root->next.arc[i]);
  return(sum);
}

static void
add_dag_graph_nodes(BNODE *root, int *i, BNODE **list)
{
  int j;

  list[(*i)++] = root;
  for (j=0; j < root->next.num; j++)
    add_dag_graph_nodes(root->next.arc[j],i,list);
}


static void
visit_dag_graph(BNODE *root)
             { 
  int i;

  printf("visiting %d\n",root->node_num);
  for (i=0; i < root->next.num; i++)
    visit_dag_graph(root->next.arc[i]);
}

static BELIEF_NET
set_belief_net(BNODE *root) {
  int count,i=0;
  BELIEF_NET bn;

  count = count_dag_graph_nodes(root);
  bn.num = count;
  /*   bn.root = root; */
  /*   bn.list = (BNODE **) malloc(sizeof(BNODE *)*count); */
  add_dag_graph_nodes(root,&i,bn.list);
  return(bn);
}
  


static void
make_moral_graph(BNODE *root)  /* Marry unMarried parents and add undirected arcs */
             { 
  int i,j;

  for (i=0; i < root->next.num; i++) {
    add_undir_arc(root,root->next.arc[i]);
    make_moral_graph(root->next.arc[i]);
  }
  for (i=0; i < root->prev.num; i++) 
    for (j=i+1; j < root->prev.num; j++) {
#ifdef VERBOSE      
      /*      printf("marrying %d and %d\n",root->prev.arc[i]->node_num,root->prev.arc[j]->node_num); */
#endif
      add_undir_arc(root->prev.arc[i],root->prev.arc[j]);
    }

}

new_make_moral_graph(BELIEF_NET *bn) {
  int i,j,k,added;
  BNODE *b;

  for (i=0; i < bn->num; i++) {
    b = bn->list[i];
    for (j=0; j < b->prev.num; j++) 
      for (k=j+1; k < b->prev.num; k++) {
	added = add_undir_arc(b->prev.arc[j],b->prev.arc[k]);
#ifdef VERBOSE      
	if (added) printf("marrying %d and %d\n",b->prev.arc[j]->node_num,b->prev.arc[k]->node_num); 
#endif
      }
  }
}


static void
vug(BNODE *cur, int *visit, void (*func) (/* ??? */))
{ 
  int i;

  if (visit[cur->node_num]) return;
  visit[cur->node_num] = 1;
  func(cur);
  for (i=0; i < cur->neigh.num; i++) vug(cur->neigh.arc[i],visit,func);
}

static void
visit_undir_graph(BNODE *cur, void (*func) (/* ??? */))
{ 
  int *visit,i;

  visit = (int *) malloc(sizeof(int)*bel_num_nodes);
  for (i=0; i < bel_num_nodes; i++) visit[i] = 0;
  vug(cur,visit,func);
}


//CF:  ci is the component index (ie. which phrase BNODE belongs to)
static void
make_component(BELIEF_NET *bn, BNODE *b, int ci) {
  int i;

  if (b->visit) return;
  b->visit =1;
  bn->list[bn->num++] = b;  //CF:  place a pointer to the BNODE into the phrase
  b->comp_index = ci;  //CF:  which phrase is this BNODE a member of
  for (i=0; i < b->neigh.num; i++) make_component(bn,b->neigh.arc[i],ci); //CF:  recurse
}

//CF:  look at a belief net, and identify connected componets
//CF:  (ie. maximal subsets of the graph nodes; that are totally disconnected from one another)
static void
make_connected_components(BELIEF_NET *bn, PHRASE_LIST *pl) {
  int i,index,t=0;
  NETWORK *net;
  BELIEF_NET *nbn;
  CLIQUE *po;
  BNODE *b;

  printf("temporarily increased MAX_PHRASES\n");
  pl->num = 0;
  for (i=0; i < bn->num; i++) bn->list[i]->visit = 0;

  //CF:  assign BNODEs to phrases.
  //CF:  visit each node; if it hasnt been visited, then look at it and its children (recursive traversal)
  //CF:  (each iteration of this loop will find one isolated component)
  for (i=0; i < bn->num; i++) if (bn->list[i]->visit == 0) {
    nbn = &(pl->list[pl->num].net.bn);  //CF:  pointer to current phrase in phrase list, to write to
    init_belief_net(nbn); //CF:  new belief net; next line creates it
    make_component(nbn,bn->list[i],pl->num); //CF:  call to the recurive exploration function; writes to cur phrase
    pl->num++;
    if (pl->num >= MAX_PHRASES) {
      printf("no more room for phrase\n");
      exit(0);
    }
  }
  //CF:  now assign the corresponding potentials to the appropriate phrases
  for (i=0; i < bn->potential.num; i++) { //CF:  for every potential in the whole net
    po = bn->potential.el[i];  //CF:  pointer to the ith mini-clique (ie, P(node) or P(node|par)) in the whole net
    index = po->member[0]->comp_index; //CF:  phrase number for this clique
    //    printf("potential %d is in component %d at time %f\n",i,index,bn->potential.el[i]->member[0]->meas_time);
    nbn = &(pl->list[index].net.bn);
    nbn->potential.el[nbn->potential.num++] = po;  //CF:  place copy of the potential pointer, into the phrase
  }


  //CF:  print stuff
  for (i=0; i < pl->num; i++) {
    printf("component %d has %d nodes and %d potentials\n",i,pl->list[i].net.bn.num,pl->list[i].net.bn.potential.num);
    t += pl->list[i].net.bn.potential.num;
  }
  printf("a total of %d potentials\n",t); 
  /*    for (i=860; i < 890; i++)
      printf("i = %d null = %d\n",i,pl->list[2].net.bn.potential.el[i]->qf.null.el);
  */

    /*    printf("exiting here\n");
    exit(0);*/
}		   
  

static void
unset_mcs_num(BNODE *node)
{
  node->mcs_num = MCS_UNSET;
}

static xyz = 0;

static void
count_off(BNODE *node)
{
  if (node->note_type == SOLO_NODE)  printf("time = %f\n",score.solo.note[node->index].time);
}

static void
add_frontier_list(BNODE *node)
{
  int i;

  for (i=0; i < frontier.num; i++) if (frontier.list[i] == node) return;
  if (frontier.num >= MAX_FRONTIER) {
    printf("frontier list full at %d\n",frontier.num);
    exit(0);
  }
  frontier.list[frontier.num++] = node;
}
  
static void
del_frontier_list(BNODE *node)
{
  int i;

  for (i=0; i < frontier.num; i++) if (frontier.list[i] == node) break;
  if (i == frontier.num) {
    printf("couldn't find node to delete\n");
    exit(0); /*return;*/
  }
  frontier.list[i] = frontier.list[--frontier.num];
}

static void
visit_function(BNODE *node)
{
  printf("cur node = %d max_card_num = %d\n",node->node_num,node->mcs_num);
}
       

static int
num_set_neighbors(BNODE *node)
{
  int i,count=0;

  for (i=0; i < node->neigh.num; i++) {
    if (node->neigh.arc[i]->mcs_num != MCS_UNSET) count++;
    /*    printf("node = %d neigh = %d count = %d Msc_num = %d\n",node->node_num,node->neigh.arc[i]->node_num,count,node->neigh.arc[i]->mcs_num); */
  }
  return(count);
}

static int
is_connected(BNODE *n1, BNODE *n2)  /* assumes symmetric connections in neighbor structure */
               {
  int i;

  for (i=0; i < n1->neigh.num; i++) 
    if (n1->neigh.arc[i] == n2) return(1);
  return(0);
}
  
static int
is_connected_to_all(BNODE *n, CLIQUE *cl) { /* is n connected to everything in cl? */
  int i;

  for (i=0; i < cl->num; i++) 
    if (!is_connected(n,cl->member[i])) return(0);
  return(1);
}
	


static int
labeled_neighbors_connected(BNODE *node)  /* boolean. are neighbors of node connected? */
     /* connects the first pair of unconnected labeled neighbors found */
             {
  int i,j;

  for (i=0; i < node->neigh.num; i++) {
    if (node->neigh.arc[i]->mcs_num == MCS_UNSET) continue;
    for (j=i+1; j < node->neigh.num; j++) {
      if (node->neigh.arc[j]->mcs_num == MCS_UNSET) continue;
      if (!is_connected(node->neigh.arc[i],node->neigh.arc[j])) {
	add_undir_arc(node->neigh.arc[i],node->neigh.arc[j]);
#ifdef VERBOSE
	printf("connecting %d %d\n",node->neigh.arc[i]->node_num,node->neigh.arc[j]->node_num); 
#endif
	return(0);
      }
    }
  }
  return(1);
}


static int
max_card_search(BNODE *node)
{
  int i,cur_max,j,temp;
  BNODE *best;

  visit_undir_graph(node,unset_mcs_num);
  node->mcs_num = 0;
  frontier.num = 0;
  for (i=0; i < node->neigh.num; i++) 
    add_frontier_list(node->neigh.arc[i]);


  for (i=1; i < bel_num_nodes; i++) {  /* the nodes in belief net */
    cur_max = -1;
    for (j=0; j < frontier.num; j++) {
      temp = num_set_neighbors(frontier.list[j]);
      if (temp > cur_max) {
	cur_max = temp;
	best = frontier.list[j];
      }
    }
    if (!labeled_neighbors_connected(best)) return(0);
    else {
      best->mcs_num = i;
      del_frontier_list(best);
      for (j=0; j < best->neigh.num; j++) 
	if (best->neigh.arc[j]->mcs_num == MCS_UNSET)
	  add_frontier_list(best->neigh.arc[j]);
    }
  }
  return(1);
} 
    

static void
triangulate_graph(BNODE *root) 
{
    while(max_card_search(root) == 0);
}


static void
new_triangulate_graph(BELIEF_NET *bn)
{
  int i,cur_max,j,temp,done=0,k;
  BNODE *best,*first;

  
  while (done == 0) {  /* until graph triangulated */
    first = bn->list[0];
    visit_undir_graph(first,unset_mcs_num);
    /*        visit_undir_graph(first,count_off);
          printf("num in list is %d ",bn->num);
	  exit(0);  */
    first->mcs_num = 0;
    frontier.num = 0;
    for (i=0; i < first->neigh.num; i++) 
      add_frontier_list(first->neigh.arc[i]);



    for (i=1; i < bn->num; i++) {  /* the nodes in belief net */

      /*      printf("num in list is %d bn.num = %d\n",frontier.num,bn->num);
      for (j=0; j < frontier.num; j++) 
      printf("i = %d j = %d %d\n",i,j,frontier.list[j]->node_num);  */


      done = 1;  /* unless we find out otherwise */
      cur_max = -1;
      for (j=0; j < frontier.num; j++) {
	temp = num_set_neighbors(frontier.list[j]);
	if (temp > cur_max) {
	  cur_max = temp;
	  best = frontier.list[j];
	}
      }
      /*      printf("most = %d num = %d\n",best->node_num,cur_max); */
      if (!labeled_neighbors_connected(best)) {
	done = 0;
	break;
      }
      else {
	best->mcs_num = i;
	/*printf("labeled node_num %d with mcs num %d\n",best->node_num,i); */
	del_frontier_list(best);
	for (j=0; j < best->neigh.num; j++) 
	  if (best->neigh.arc[j]->mcs_num == MCS_UNSET)
	    add_frontier_list(best->neigh.arc[j]);
      }
    }
  }
}
  



static void
perfect_func(BNODE *node)
{
  perfect[node->mcs_num] = node;
}

static CLIQUE *
alloc_clique(void) {
  CLIQUE *temp;

  temp = (CLIQUE *) malloc(sizeof(CLIQUE));
  temp->num = 0;
  temp->qf.m.el = 0;
  return(temp);
}

static CLIQUE *
copy_clique(CLIQUE *cl) {
  CLIQUE *temp;

  temp = (CLIQUE *) malloc(sizeof(CLIQUE));
  *temp = *cl;
  return(temp);
}


static CLIQUE_NODE*
alloc_clique_node(void) {
  CLIQUE_NODE *temp;

  temp = (CLIQUE_NODE *) malloc(sizeof(CLIQUE_NODE));
  temp->clique = alloc_clique();
  temp->neigh.num  = 0;
  return(temp);
}


static void
add_to_clique(CLIQUE *clique, BNODE *node)
{
  if (clique->num == MAX_CLIQUE) {
    printf("out of room in clique\n");
    return;
  }
  clique->member[(clique->num)++] = node;
}


static void
build_add_to_clique(CLIQUE *clique, BNODE *node)  { /* call this when building clique tree */
  add_to_clique(clique,node);
  node->clique = clique;  /* chris */
}


static CLIQUE_NODE *
build_clique_from_top(int index)
{
  int i;
  CLIQUE_NODE *temp;
  BNODE *node;
  CLIQUE *cl;

  node  = perfect[index];
  temp = alloc_clique_node();
  temp->clique_id = index;
  cl = temp->clique;
  build_add_to_clique(cl,node);
   for (i = 0; i < node->neigh.num; i++) 
    if (node->neigh.arc[i]->mcs_num < node->mcs_num) {
      build_add_to_clique(temp->clique,node->neigh.arc[i]);
    }


  return(temp);
}
    


static void
connect_clique(CLIQUE *cl) {
  int i,j;

  for (i=0; i < cl->num; i++) for (j= i+1; j < cl->num; j++)
    add_undir_arc(cl->member[i],cl->member[j]);
}





static int
fast_num_set_neighbors(BNODE *node)
{
  int i,count=0;
  BNODE *next;

  for (i=0; i < node->neigh.num; i++) {
    next = node->neigh.arc[i];
    if (next->focus == 0) continue;
    if (next->mcs_num != MCS_UNSET) count++;
    /*    printf("node = %d neigh = %d count = %d Msc_num = %d\n",node->node_num,node->neigh.arc[i]->node_num,count,node->neigh.arc[i]->mcs_num); */
  }
  return(count);
}

//CF:  add unidirected connection, BNODE.neigh
static void
connect_lower_neighbors(BNODE *node)        {
  int i,j;
  BNODE *n1,*n2;

  for (i=0; i < node->neigh.num; i++) {
    n1 = node->neigh.arc[i];
    if (n1->mcs_num == MCS_UNSET) continue;
    if (n1->focus == 0 || n1->mcs_num >= node->mcs_num) continue;
    for (j=i+1; j < node->neigh.num; j++) {
      n2 = node->neigh.arc[j];
      if (n2->mcs_num == MCS_UNSET) continue;
      if (n2->focus == 0 || n2->mcs_num >= node->mcs_num) continue;
      if (!is_connected(n1,n2)) add_undir_arc(n1,n2);
    }
  }
}

static int
lower_neighbors_connected(BNODE *node)        {
  int i,j;
  BNODE *n1,*n2;

  for (i=0; i < node->neigh.num; i++) {
    n1 = node->neigh.arc[i];
    if (n1->mcs_num == MCS_UNSET) continue;
    if (n1->focus == 0 || n1->mcs_num >= node->mcs_num) continue;
    for (j=i+1; j < node->neigh.num; j++) {
      n2 = node->neigh.arc[j];
      if (n2->mcs_num == MCS_UNSET) continue;
      if (n2->focus == 0 || n2->mcs_num >= node->mcs_num) continue;
      if (!is_connected(n1,n2)) return(0);
    }
  }
  return(1);
}

//CF:  this does Max Cardinality Search (MCS) triangulation (See Ripley book, or LauritenSpeilgelhalterCowell tutorial)
//CF:  eventually we add BNODE.neigh undirected arcs
static void
fast_triangulate_graph(BELIEF_NET *bn)
{
  int i,cur_max,j,temp,done=0,k;
  BNODE *best,*first,*next;
  char string[500];

  
  first = bn->list[0];
  for (i=0; i < bn->num; i++)  {
    if (bn->list[i]->focus == 0) continue;   //CF:  focus is not used; its always 1.
    if (rat_cmp(bn->list[i]->wholerat,first->wholerat) < 0)  first = bn->list[i];
    bn->list[i]->mcs_num = MCS_UNSET;
  }
  first->mcs_num = 0;
  frontier.num = 0;
#ifdef GRADUAL_CATCH_UP_EXPERIMENT
  if (bn->num == 1) return;  /* a kludge.  probably shouldn't have isolated node */
#endif

  //CF:  init frontier to neighbors of the first node
  for (i=0; i < first->neigh.num; i++) {
    next = first->neigh.arc[i];
    if (next->focus) add_frontier_list(next);
  }

  //CF:  main loop.; Note we're not iterating over the node list, just over int MSC numbers.
  //CF:  frontier is the set of neighbors of MCS-labelled nodes.
  for (i=1; i < bn->num; i++) {  /* will only label focus nodes */  //CF: for each available number (ie num of nodes)
    cur_max = -1;
    for (j=0; j < frontier.num; j++) {
      temp = fast_num_set_neighbors(frontier.list[j]); //CF:  choose e frontier node with most labelled neighbors
      //      if (temp > cur_max) {
      if ((temp > cur_max) || (temp == cur_max && rat_cmp(frontier.list[j]->wholerat,best->wholerat) < 0)) {
	cur_max = temp;
	best = frontier.list[j];
      }
    }
    best->mcs_num = i;  //CF:  the chosen node gets labelled with current MCS number
    wholerat2string(best->wholerat,string);
    /*    printf("frontier.num = %d lableing type %d with %d at %s\n",frontier.num, best->note_type,i,string); 
    for (j=0; j < frontier.num; j++) {  
      wholerat2string(frontier.list[j]->wholerat,string);
      printf("%d %s\n",j,string);
      }*/
    del_frontier_list(best);  //CF:  remove it from the fronter.
    //CF:  add unlabelled neighbors of the newly-labelled node to the fronter
    for (j=0; j < best->neigh.num; j++) {
      next = best->neigh.arc[j];
      if (next->focus  && (next->mcs_num == MCS_UNSET)) add_frontier_list(next);
    }
    if (frontier.num == 0) break;  //CF:  this should never happen now focus is not used

  }





  //CF:  labelling has been done. Now trainglulate.
  //CF:  assert: i=bn->num here.
  for ( ; i > 0; i--) {
    //CF:  look for the node that has MCS number i.
    for (j=0; j < bn->num; j++) {
      //      printf("focus = %d i = %d mcs_num = %d\n",bn->list[j]->focus,i,bn->list[j]->mcs_num);
      if (bn->list[j]->focus && (bn->list[j]->mcs_num == i)) break;
    }
    //CF:  assert: now j=i, ie. (bn->list[j]->mcs_num = i)
    connect_lower_neighbors(bn->list[j]); //CF:  marry the lower-numbered neighbors of this node to each other.
  }
}
  

static void
working_fast_triangulate_graph(BELIEF_NET *bn)
{
  int i,cur_max,j,temp,done=0,k;
  BNODE *best,*first,*next;
  char string[500];

  
  for (i=0; i < bn->num; i++)  {
    if (bn->list[i]->focus == 0) continue;   //CF:  focus is not used; its always 1.
    first = bn->list[i];
    bn->list[i]->mcs_num = MCS_UNSET;
  }
  first->mcs_num = 0;
  frontier.num = 0;
#ifdef GRADUAL_CATCH_UP_EXPERIMENT
  if (bn->num == 1) return;  /* a kludge.  probably shouldn't have isolated node */
#endif

  //CF:  init frontier to neighbors of the first node
  for (i=0; i < first->neigh.num; i++) {
    next = first->neigh.arc[i];
    if (next->focus) add_frontier_list(next);
  }

  //CF:  main loop.; Note we're not iterating over the node list, just over int MSC numbers.
  //CF:  frontier is the set of neighbors of MCS-labelled nodes.
  for (i=1; i < bn->num; i++) {  /* will only label focus nodes */  //CF: for each available number (ie num of nodes)
    cur_max = -1;
    for (j=0; j < frontier.num; j++) {
      temp = fast_num_set_neighbors(frontier.list[j]); //CF:  choose frontier node with most labelled neighbors
      if (temp > cur_max) {
	cur_max = temp;
	best = frontier.list[j];
      }
    }
    best->mcs_num = i;  //CF:  the chosen node gets labelled with current MCS number
    wholerat2string(best->wholerat,string);
    printf("frontier.num = %d lableing type %d with %d at %s\n",frontier.num, best->note_type,i,string); 
    for (j=0; j < frontier.num; j++) {  // this makes the compiler hang ...????
      wholerat2string(frontier.list[j]->wholerat,string);
      printf("%d %s\n",j,string);
    }
    del_frontier_list(best);  //CF:  remove it from the fronter.
    //CF:  add unlabelled neighbors of the newly-labelled node to the fronter
    for (j=0; j < best->neigh.num; j++) {
      next = best->neigh.arc[j];
      if (next->focus  && (next->mcs_num == MCS_UNSET)) add_frontier_list(next);
    }
    if (frontier.num == 0) break;  //CF:  this should never happen now focus is not used

  }





  //CF:  labelling has been done. Now trainglulate.
  //CF:  assert: i=bn->num here.
  for ( ; i > 0; i--) {
    //CF:  look for the node that has MCS number i.
    for (j=0; j < bn->num; j++) {
      //      printf("focus = %d i = %d mcs_num = %d\n",bn->list[j]->focus,i,bn->list[j]->mcs_num);
      if (bn->list[j]->focus && (bn->list[j]->mcs_num == i)) break;
    }
    //CF:  assert: now j=i, ie. (bn->list[j]->mcs_num = i)
    connect_lower_neighbors(bn->list[j]); //CF:  marry the lower-numbered neighbors of this node to each other.
  }
}
  

static int
graph_triangulated(BELIEF_NET *bn)
{
  int i,cur_max,j,temp,done=0,k;
  BNODE *best,*first,*next;

  
  for (i=0; i < bn->num; i++)  {
    if (bn->list[i]->focus == 0) continue;
    first = bn->list[i];
    bn->list[i]->mcs_num = MCS_UNSET;
  }
  first->mcs_num = 0;
  frontier.num = 0;
  for (i=0; i < first->neigh.num; i++) {
    next = first->neigh.arc[i];
    if (next->focus) add_frontier_list(next);
  }
  for (i=1; i < bn->num; i++) {  /* will only label focus nodes */
    cur_max = -1;
    for (j=0; j < frontier.num; j++) {
      temp = fast_num_set_neighbors(frontier.list[j]);
      if (temp > cur_max) {
	cur_max = temp;
	best = frontier.list[j];
      }
    }
    best->mcs_num = i;
    if (lower_neighbors_connected(best) == 0) return(0);
    /*    printf("lableing type %d at %f with %d\n",best->note_type,best->meas_time,i); */
    del_frontier_list(best);
    for (j=0; j < best->neigh.num; j++) {
      next = best->neigh.arc[j];
      if (next->focus  && (next->mcs_num == MCS_UNSET)) add_frontier_list(next);
    }
    if (frontier.num == 0) break;
  }
  return(1);
}
  


static CLIQUE*
focus_possible_clique(BNODE *bn) {
/* returns subset of nodes containing bn and all neighbors
   of bn with lower max card search number.  these nodes
   are all connected, but might be a subset of a clique.
   (a clique is maximal */
     
  CLIQUE *cl;     
  int i;
  BNODE *next;

  cl = alloc_clique();
  build_add_to_clique(cl,bn);
  for (i=0; i < bn->neigh.num; i++) {
    next = bn->neigh.arc[i];
    if (next->focus && next->mcs_num < bn->mcs_num) 
      build_add_to_clique(cl,next);
  }
  return(cl);
}


static void//CF:  and copy pointers of BNODEs and Potentials into these phrases
add_to_clique_tree(CLIQUE_NODE *cl, CLIQUE_TREE *ct) {
  int i;
  BNODE *bn;

  ct->list[(ct->num)++] = cl;
  for (i=0; i < cl->clique->num; i++) {
    bn = cl->clique->member[i];
    bn->clnode = cl;
  }
}
    
static int
is_subset(CLIQUE *c1, CLIQUE *c2)
{
  int i,j;

  for (i=0; i < c1->num; i++) {
    for (j=0; j < c2->num; j++) {
      if (c1->member[i] == c2->member[j]) break;
    }
    if (j == c2->num) return(0);
  }
  return(1);
}


//CF:  build clique tree!
static CLIQUE_TREE
focus_build_clique_tree(BELIEF_NET *bn)
{
  CLIQUE_NODE *cur,*clique_node[MAX_BELIEF_NODES];
  CLIQUE *intersect,*cl,*cln;
  int i,j,low_clique,last_perfect,p,lastp,count=0;
  CLIQUE_TREE ct;
  BNODE *perfect[MAX_BELIEF_NODES];

  //CF:  build an array of pointers to the nodes in the order their MCS numbers, called 'perfect'
  //CF:  then cliques can be read off it (somehow?)
  for (i=0; i < bn->num; i++) if (bn->list[i]->focus) {
    count++;
    perfect[bn->list[i]->mcs_num] = bn->list[i];
    /*    printf("bn->list[%d]->mcs_num = %d\n",i,bn->list[i]->mcs_num);
    if (bn->list[i]->mcs_num == MCS_UNSET) {
      for (j=0; j < bn->list[i]->neigh.num; j++)
	printf("focus = %d\n",bn->list[i]->neigh.arc[j]->focus);
	}*/
  }
  //CF:  construct an empty clique tree, with space for max possible cliques = num of bnodes (upper bound)
  ct.list = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * count); 
  ct.num = 0;
  p = 0;

  while (p < count) {  //CF:  for node in this phrase
     cl = alloc_clique();  //CF:  malloc for a clique
     lastp = p;
     for (; p < count; p++) {
       cln = focus_possible_clique(perfect[p]); //CF:  collection of lower-numbered neighbours, might be a clique
       if (is_subset(cl,cln)) cl = cln;
       else break;
     }
     cur = alloc_clique_node();
     cur->clique = cl;

     //CF:  we have creates the CLIQUE_NODE; now link to (only ever one) neighbor and add seperator.
     if (ct.num > 0) {
       intersect = alloc_clique();
       for (i = 0; i < cl->num; i++) if (cl->member[i]->mcs_num < lastp)
	 add_to_clique(intersect,cl->member[i]);  //CF:  adds one BNODE to the intersection
       if (intersect->num == 0) {
	 printf("0 sized intersection\n");
	 printf("cl->num = %d\n",cl->num);
	 exit(0);
       }
       //CF:  There will be exactly one already-clique that shares this seperator.  Link to it.
       for (j=ct.num-1; j >= 0; j--) {
	 if (is_subset(intersect, ct.list[j]->clique)) {    //CF:  is the intersection a subset of this clique
	   connect_clique_nodes(cur, ct.list[j],intersect); //CF:  make the connection **
	   break;
	 }
       }
       if (j < 0) {
	 printf("couldn't find connecting clique\n");
	 exit(0);
       }
     }
     add_to_clique_tree(cur,&ct);
  }
  ct.root = perfect[count-1]->clnode;  //CF:  store an arbitary clique pointer as the 'root'
  return(ct);
}
  

static int
check_potential(CLIQUE *pot) {
  int i,dim;

  dim = 0;
  for (i=0; i < pot->num; i++) dim += pot->member[i]->dim;
  if (dim != pot->qf.cov.cols) {
    printf("misspecified clique potential (%d != %d)\n",dim,pot->qf.cov.cols);
    return(0);
    //  exit(0);
  }
  if (Mnorm(pot->qf.S) > 10000000) {
  printf("ay carumba\n"); Mp(pot->qf.S); exit(0);}
  return(1);
}

static int
add_potential(BELIEF_NET *bn, CLIQUE *pot) {

  if (bn->potential.num >= MAX_BELIEF_NODES) {
    printf("out of room in add_potential\n");
     return(0);
    //    exit(0);
  }
  check_potential(pot);
  pot->qf = QFperm_id(pot->qf);
  bn->potential.el[bn->potential.num++] = pot;
  if (pot->num == 1 && pot->member[0]->trainable) 
    pot->member[0]->train_dist = &(pot->qf);
  connect_clique(pot);
  return(1);
}

static CLIQUE*
make_conditional_potential(BNODE *fr, BNODE *to, MATRIX A, QUAD_FORM e) {
  CLIQUE *temp;
  float d;
  RATIONAL diff;

  if (e.cov.rows != A.rows)  
    {    printf("dimension disagreement in make_conditional_potential\n"); exit(0); }
  if (fr->dim != A.cols)  
    {    printf("dimension disagreement in make_conditional_potential\n"); exit(0); }
  if (to->dim != e.cov.rows)  
    {    printf("dimension disagreement in make_conditional_potential\n"); exit(0); }


  diff = sub_rat(to->wholerat,fr->wholerat);
  d = diff.num/(float)diff.den;

  /*  if (d > 5 || d < 0) {
    printf("this is a long arc (d = %f) \n",d);
    exit(0);
    }*/

  temp = alloc_clique();
  add_to_clique(temp,fr);
  add_to_clique(temp,to);
  temp->qf = QFcond_dist(A,e);
  temp->polarity = NUMERATOR;
  return(temp);
}

//CF:  to = A1*from1 + A2*from2 + e
static CLIQUE*
make_conditional_potential2(BNODE *fr1, BNODE *fr2, BNODE *to, MATRIX A1, MATRIX A2, QUAD_FORM e) {
  CLIQUE *temp;
  MATRIX A;
  RATIONAL diff;
  float d;
  char string[500];
  
  if (to->dim != e.cov.rows)  
    {    printf("dimension disagreement in make_conditional_potential2\n"); exit(0); }
  if (e.cov.rows != A1.rows)  
    {    printf("dimension disagreement in make_conditional_potential2\n"); exit(0); }
  if (A1.rows != A2.rows)  
    {    printf("dimension disagreement in make_conditional_potential2\n"); exit(0); }
  if (fr1->dim != A1.cols)
    {    printf("dimension disagreement in make_conditional_potential2\n"); exit(0); }
  if (fr2->dim != A2.cols)
    {    printf("dimension disagreement in make_conditional_potential2\n"); exit(0); }


  diff = sub_rat(to->wholerat,fr1->wholerat);
  d = diff.num/(float)diff.den;
  if (d > 5 || d < 0) {
    wholerat2string(fr1->wholerat,string);
    printf("this is a long arc (d = %f) from %s \n",d,string);
  //  exit(0);
  }
  temp = alloc_clique();
  add_to_clique(temp,fr1);
  add_to_clique(temp,fr2);
  add_to_clique(temp,to);
  A = Mcat(A1,A2);   //CF:  horizontal concatenation
  temp->qf = QFcond_dist(A,e);
  temp->polarity = NUMERATOR; //CF:  not used
  return(temp);
}

make_conditional_potential3(BNODE *fr1, BNODE *fr2, BNODE *fr3, BNODE *to, MATRIX A1, MATRIX A2, MATRIX A3, QUAD_FORM e) {
  CLIQUE *temp;
  MATRIX A;
  RATIONAL diff;
  float d;
  char string[500];
  
  if (to->dim != e.cov.rows)  
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }
  if (e.cov.rows != A1.rows)  
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }
  if (A1.rows != A2.rows)  
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }
  if (A1.rows != A3.rows)  
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }
  if (fr1->dim != A1.cols)
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }
  if (fr2->dim != A2.cols)
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }
  if (fr3->dim != A3.cols)
    {    printf("dimension disagreement in make_conditional_potential3\n"); exit(0); }


  temp = alloc_clique();
  add_to_clique(temp,fr1);
  add_to_clique(temp,fr2);
  add_to_clique(temp,fr3);
  add_to_clique(temp,to);
  A = Mcat(Mcat(A1,A2),A3);   //CF:  horizontal concatenation
  temp->qf = QFcond_dist(A,e);
  temp->polarity = NUMERATOR; //CF:  not used
  return(temp);
}

//CF:  associate a quadform with a node, by creating a mini clique including only that node and the qf on it.
//CF:  (this could be inserted into 
static CLIQUE*
make_marginal_potential(BNODE *nd, QUAD_FORM e) {
  CLIQUE *temp;

  temp = alloc_clique();
  add_to_clique(temp,nd);
  temp->qf = e;
  temp->polarity = NUMERATOR;
  return(temp);
}

static CLIQUE*
make_null_potential(BNODE *nd1, BNODE *nd2) {
  CLIQUE *temp;

  temp = alloc_clique();
  add_to_clique(temp,nd1);
  add_to_clique(temp,nd2);
  temp->qf = QFunif(nd1->dim+nd2->dim);
  temp->polarity = NUMERATOR;
  return(temp);
}


static CLIQUE*
make_potential(CLIQUE *cl, QUAD_FORM qf) {
  CLIQUE *temp;

  temp = copy_clique(cl);
  temp->qf = qf;
  temp->polarity = NUMERATOR;
  return(temp);
}


//CF:  construct a solo BNODE      
static BNODE*
alloc_solo_node_pot(int dim, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;

  new = alloc_belief_node_pot(SOLO_DIM, type,  index, bn);
  qf = observation_variance(index);
  hang = alloc_belief_node_pot(OBS_DIM, OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,SOLO_DIM);
  cl = make_conditional_potential(new, hang, A, qf);
  add_potential(bn,cl);
  score.solo.note[index].hidden = new;
  return(new);
}
//CF:  
static void
add_solo_observation_node_pot(BNODE *bone, int index, BELIEF_NET *bn, int ignore_solo, int solo_cue) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;

  qf = solo_observation_variance(index, ignore_solo, solo_cue);
  hang = alloc_belief_node_pot(OBS_DIM, OBS_NODE,index, bn); //CF:  OBS_NODE is type of this node, an observation 
  //  printf("node = %s\n",hang->observable_tag);
  //	QFprint(qf);  
  A = Mleft_corner(OBS_DIM,SOLO_DIM);
  cl = make_conditional_potential(bone, hang, A, qf);
  //  if (qf.inf.cols) { QFprint(cl->qf); }
  add_potential(bn,cl);
  score.solo.note[index].hidden = bone;
}

static void
add_solo_observation_node_at(BNODE *bone, int index, BELIEF_NET *bn, int ignore_solo, int solo_cue) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;

  qf = solo_observation_variance(index, ignore_solo, solo_cue);
  hang = alloc_belief_node_pot(OBS_DIM, OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,BACKBONE_ATEMPO_DIM);
  cl = make_conditional_potential(bone, hang, A, qf);
  add_potential(bn,cl);
  score.solo.note[index].hidden = bone;
}


static void
add_midi_observation_node_pot(BNODE *bone, int index, BELIEF_NET *bn, 
			      int cue, int accomp_lead, int midi_is_true) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;

  //  qf = midi_observation_variance_parallel(cue, accomp_lead, midi_is_true);
  qf = midi_observation_variance(cue, accomp_lead, midi_is_true);
  hang = alloc_belief_node_pot(OBS_DIM, ACCOM_OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,BACKBONE_DIM);
  cl = make_conditional_potential(bone, hang, A, qf);
  add_potential(bn,cl);
  score.midi.burst[index].hidden = bone;
}

static void
add_midi_observation_node_pot_catch(BNODE *bone, int index, BNODE *catch, BELIEF_NET *bn, 
			      int cue, int accomp_lead, int midi_is_true) {
  BNODE *new,*hang;
  MATRIX A,I;
  QUAD_FORM qf;
  CLIQUE *cl;

  qf = midi_observation_variance(cue, accomp_lead, midi_is_true);
  hang = alloc_belief_node_pot(OBS_DIM, ACCOM_OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,BACKBONE_DIM);
  I = Miden(1);
  cl = make_conditional_potential2(bone, catch, hang, A, I, qf);
  add_potential(bn,cl);
  score.midi.burst[index].hidden = bone;
  score.midi.burst[index].catchit = catch;
}

static void
add_midi_observation_node_at(BNODE *bone, int index, BELIEF_NET *bn, 
			      int cue, int accomp_lead, int midi_is_true) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;

  qf = midi_observation_variance(cue, accomp_lead, midi_is_true);
  hang = alloc_belief_node_pot(OBS_DIM, ACCOM_OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,BACKBONE_ATEMPO_DIM);
  cl = make_conditional_potential(bone, hang, A, qf);
  add_potential(bn,cl);
  score.midi.burst[index].hidden = bone;
}


#define PIANO_ERROR_SECS .05

static BNODE*
alloc_accomp_train_node_pot(int dim, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;

  new = alloc_belief_node_pot(ACCOM_DIM, type,  index, bn);
  qf = QFpos_var(OBS_DIM, PIANO_ERROR_SECS*PIANO_ERROR_SECS);
  hang = alloc_belief_node_pot(OBS_DIM, ACCOM_OBS_NODE,index, bn);
  A = Mleft_corner(OBS_DIM,ACCOM_DIM);
  cl = make_conditional_potential(new, hang, A, qf);
  add_potential(bn,cl);
  return(new);
}

static BNODE*
alloc_accomp_node_pot(int dim, int type, int index, BELIEF_NET *bn) {
  BNODE *new,*hang;
  MATRIX A;
  QUAD_FORM qf;
  CLIQUE *cl;


  new = alloc_belief_node_pot(ACCOM_DIM, type,  index, bn);
  qf = QFpoint(Mzeros(1,1));
  hang = alloc_belief_node_pot(1, ACCOM_OBS_NODE, index, bn);
  A = Mleft_corner(1,ACCOM_DIM);
  cl = make_conditional_potential(new, hang, A, qf);
  add_potential(bn,cl);
  score.midi.burst[index].hidden = new;
  return(new);
}



BELIEF_NET
make_solo_graph_pot(int start_note, int end_note) {
  int i;
  BNODE *last,*this,*update;
  MATRIX mat,tempo;
  QUAD_FORM e,z,nullqf;
  BELIEF_NET bn;
  CLIQUE *cl;


  init_belief_net(&bn);
  nullqf = z = QFpoint(Mzeros(SOLO_DIM,1));
  tempo = Mzeros(SOLO_DIM,SOLO_PHRASE_UPDATE_DIM);
  tempo.el[1][0] = 1; /* this matrix selects the 1st comp to the 2nd component */
  for (i=start_note; i <= end_note; i++) {
    if (i > start_note)  {
      e = score.solo.note[i].qf_update; /* both first and others */
      this  = alloc_solo_node_pot(SOLO_DIM, SOLO_NODE, i, &bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      update  = alloc_belief_node(SOLO_DIM, nullqf ,SOLO_UPDATE_NODE, i, &bn);
      cl = make_marginal_potential(update,e);
      add_potential(&bn,cl);
      cl = make_conditional_potential2(last, update, this, mat, Miden(SOLO_DIM), z);
      add_potential(&bn,cl);
    }
    else {
      e = score.solo.note[i].qf_update; /* both first and others */
      this  = alloc_solo_node_pot(SOLO_DIM, SOLO_PHRASE_START, i, &bn);
      cl = make_marginal_potential(this,e);
      add_potential(&bn,cl);
    }
    last = this;
  }
  return(bn);
}






add_solo_graph(BELIEF_NET *bn) {
  int i;
  BNODE *last,*this,*update;
  MATRIX mat,tempo;
  QUAD_FORM e,z;
  CLIQUE *cl;

  
  for (i=0; i < score.solo.num; i++) {
    if (i > 0 && score.solo.note[i].cue == 0)  {
      z = QFpoint(Mzeros(SOLO_DIM,1));
      e = score.solo.note[i].qf_update; /* both first and others */
      this  = alloc_solo_node_pot(SOLO_DIM, SOLO_NODE, i, bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      update  = alloc_belief_node_pot(SOLO_DIM, SOLO_UPDATE_NODE, i, bn);
      cl = make_marginal_potential(update,e);
      add_potential(bn,cl);
      cl = make_conditional_potential2(last, update, this, mat, Miden(SOLO_DIM), z);
      add_potential(bn,cl);
    }
    else {
      e = score.solo.note[i].qf_update; /* both first and others */
      this  = alloc_solo_node_pot(SOLO_DIM,  SOLO_PHRASE_START, i, bn);
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }
    last = this;
    clear_matrix_buff();
  }
}

#define MAX_COMPOSITE 20000 //10000 

//CF:  an event in the COMPOSITE score
typedef struct {
  int solo;  /* boolean */ //CF:  is it a solo or accomp event?
  int index;  
  int cue;    //CF:  bool. Is this note a cue? (eg. of a cadenza)
  RATIONAL pos;  //CF:  this is in whole notes (not bars!)
  BNODE *bnode;
  //CF:  xchg points also tempo settings (but with trainable tempos)
  int axchg;  /* bool; exchange control to accompaniment */ //CF:  changes weights, using acc variances
  int sxchg;  /* bool; exchange control to solo */ //CF:  the usual mode
  int accomp_lead; /* boolean: 1 = ignore solo input */
  int tempo_setting;  //CF:  point where tempo is reestablish completely anew. (new tempo independent of prev tempo)
  float whole_secs; //CF:  not used? (invtempo)
  int phantom;  // is there an associated observation?
  int solo_parent;  // for accompaniment note that is coincident with beat
} COMPOSITE_DATA;

typedef struct {
  int num;
  //  COMPOSITE_DATA list[MAX_COMPOSITE];
  COMPOSITE_DATA *list;
} COMPOSITE_LIST;


static void
alloc_composite_list(COMPOSITE_LIST *comp) {
  comp->num = 0;
  comp->list = (COMPOSITE_DATA *) malloc(sizeof(COMPOSITE_DATA)*MAX_COMPOSITE);
}

static int
composite_compare(const void *p1, const void *p2) {
  RATIONAL r1,r2;
  COMPOSITE_DATA *d1, *d2;

  d1 = (COMPOSITE_DATA *) p1;
  d2 = (COMPOSITE_DATA *) p2;

  r1 = (d1->solo) ? score.solo.note[d1->index].wholerat : score.midi.burst[d1->index].wholerat;
  r2 = (d2->solo) ? score.solo.note[d2->index].wholerat : score.midi.burst[d2->index].wholerat;
  return(rat_cmp(r1,r2));
}



static int
composite_compare_pos(const void *p1, const void *p2) {
  COMPOSITE_DATA *d1, *d2;

  d1 = (COMPOSITE_DATA *) p1;
  d2 = (COMPOSITE_DATA *) p2;
  return(rat_cmp(d1->pos,d2->pos));
}




del_composite_list(COMPOSITE_LIST *l, int i) {
  int j;

  for (j=i; j < l->num-1; j++) l->list[j] = l->list[j+1];
  l->num--;
}

uniq_composite_list(COMPOSITE_LIST *l) {
  int i,d;

  for (i=0; i < l->num-1; i++) if (composite_compare(&l->list[i],&l->list[i+1]) == 0) {
    if (l->list[i].cue && l->list[i+1].cue) 
      { printf("don't know which to choose\n"); exit(0); }
    if (l->list[i].cue) d = i+1;
    else if (l->list[i+1].cue) d = i;
    else d = (l->list[i].solo) ? i+1 : i;
    del_composite_list(l,d);
  }
}

uniq_composite_list_xchg(COMPOSITE_LIST *l) {
  int i;

  for (i=0; i < l->num-1; i++)  if (rat_cmp(l->list[i].pos,l->list[i+1].pos) == 0) {


    if (l->list[i].cue && l->list[i+1].cue) printf("bad mojo here\n");
    /* only one of the two is set so okay to '|' in next statement */
    l->list[i].cue = (l->list[i].cue | l->list[i+1].cue);
    //    l->list[i].cue = (l->list[i].cue || l->list[i+1].cue);
    l->list[i].axchg = (l->list[i].axchg || l->list[i+1].axchg);
    l->list[i].sxchg = (l->list[i].sxchg || l->list[i+1].sxchg);
    del_composite_list(l,i+1);
  }
}

set_accomp_lead(COMPOSITE_LIST *l) {
  int i,accomp_lead=0,si;
  char name[200];
  
  for (i=0; i < l->num; i++) {
    if (strcmp("marcello",scorename) == 0) {
      if ((i > 0) && (l->list[i-1].axchg)) accomp_lead = 1;
      if ((i == 0) && (l->list[i-1].axchg)) { printf("this xchg will be missed\n"); exit(0); }
    }
    else if (l->list[i].axchg || (l->list[i].cue == ACCOM_CUE_NUM)) 
      accomp_lead = 1;


    //    if (l->list[i].cue == ACCOM_CUE_NUM) {printf("xxx\n"); exit(0); } 

    if (l->list[i].sxchg || (l->list[i].cue == SOLO_CUE_NUM)) accomp_lead = 0;
    if ((l->list[i].sxchg || l->list[i].cue)  && (l->list[i].axchg)) {
      printf("weird conflict in set_acomp_lead sxgh = %d cue = %d axchg = %d\n",l->list[i].sxchg, l->list[i].cue, l->list[i].axchg);
      wholerat2string(l->list[i].pos,name);

      printf("i = %d place = %s  pos = %d/%d\n",i,name,l->list[i].pos.num,l->list[i].pos.den);
      exit(0);
    }
    
    si = coincident_solo_index(l->list[i].pos); 
    //    l->list[i].accomp_lead = accomp_lead;
    l->list[i].accomp_lead = (l->list[i].axchg && si != -1) ? 0 : accomp_lead; // 2-10 solo leads for the coincident note, if there is one
  }
}


add_composite_list(COMPOSITE_LIST *l, COMPOSITE_DATA d) {
  if (l->num == MAX_COMPOSITE) { printf("no composite room\n"); exit(0); }
  l->list[l->num] = d;
  l->num++;
}


static int
contains_noteon(MIDI_EVENT_LIST *m) {
  int i;

  for (i=0; i < m->num; i++) 
    if (m->event[i].command == NOTE_ON && m->event[i].volume > 0) return(1);
  return(0);
}

static int
treat_midi_as_true_time(int si, int mi) {
  int i;
  /* if solo note is a cue and an accomp is coincident and the accompaniment plays
     the next note by itself (or with solo) , treat the accomp time more like the true onset
     time.   this keeps the accompaniment from hicupping in trying to catch up */
  if (si < 0 || mi < 0) return(0);
  if (score.solo.note[si].cue == 0) return(0);
  if (rat_cmp(score.solo.note[si].wholerat,score.midi.burst[mi].wholerat)) return(0);
  if (mi == (score.midi.num -1) || si == (score.solo.num-1)) return(0);
  //  if (rat_cmp(score.midi.burst[mi+1].wholerat, score.solo.note[si+1].wholerat) >= 0) 
  if (rat_cmp(score.midi.burst[mi+1].wholerat, score.solo.note[si+1].wholerat) > 0)  return(0);
  if (contains_noteon(&score.midi.burst[mi+1].action) == 0) return(0);
  return(1);
}
  

//CF:  Constructs solo and/or accomp observations nodes hanging off the current backbone node (bone).
//CF:  accomp_lead is bool, whether the accomp is currently leading (in which case, accomp nodes are created with lower var)
//CF:     (the effect of the lower variance is to weight the accomp events more in future predictions)
static void
add_backbone_observations(BNODE *bone, RATIONAL rat, BELIEF_NET *bn, int accomp_lead) {
  int si,mi,cue,midi_is_true,solo_cue,xxx;

  si = coincident_solo_index(rat);  //CF:  look through solo notes to find one that matches this backbone node (-1 if none)
  mi = coincident_midi_index(rat);  //CF:  same for acc
  solo_cue = score.solo.note[si].cue;  /* could be either SOLO_CUE_NUM or ACCOM_CUE_NUM */
  cue = (si >=0 && mi >=0);
  midi_is_true = treat_midi_as_true_time(si,mi);
  //    if (midi_is_true) printf("midi is true at %s %s\n",score.solo.note[si].observable_tag,score.midi.burst[mi].observable_tag);

  /*  if (si >=0) if (strcmp(score.solo.note[si].observable_tag,"9+0/1_a6") == 0) {
    if (si >=0) add_solo_observation_node_pot(bone, si, bn, 0, solo_cue);
    if (mi >=0) add_midi_observation_node_pot(bone,  mi, bn,cue, 1, midi_is_true);
    printf("helloolo\n");
    return;
    }*/


  if (si >=0) add_solo_observation_node_pot(bone, si, bn, accomp_lead, solo_cue);     //CF:  if there a solo note, create it
  if (mi >=0) add_midi_observation_node_pot(bone,  mi, bn,cue, accomp_lead,midi_is_true); //CF:  ditto for acc note
}

static void
add_accm_observation(BNODE *bone, RATIONAL rat, BELIEF_NET *bn, int accomp_lead) {
  int si,mi,cue,midi_is_true,solo_cue,xxx;

  si = coincident_solo_index(rat);  //CF:  look through solo notes to find one that matches this backbone node (-1 if none)
  mi = coincident_midi_index(rat);  //CF:  same for acc

  /* this is probably all wrong for the parallel model where it is currently being used */
  midi_is_true = treat_midi_as_true_time(si,mi);
  if (mi >=0) add_midi_observation_node_pot(bone,  mi, bn,cue, accomp_lead,midi_is_true); //CF:  ditto for acc note
}

static void
add_solo_observation(BNODE *bone, RATIONAL rat, BELIEF_NET *bn, int accomp_lead) {
  int si,mi,cue,midi_is_true,solo_cue,xxx;

  si = coincident_solo_index(rat);  //CF:  look through solo notes to find one that matches this backbone node (-1 if none)
  if (si == -1) return;  // no corresponding solo note (could be phantom)
  solo_cue = score.solo.note[si].cue;  /* could be either SOLO_CUE_NUM or ACCOM_CUE_NUM */
  add_solo_observation_node_pot(bone, si, bn, accomp_lead, solo_cue);     //CF:  if there a solo note, create it
}

static void
add_backbone_observations_catch(BNODE *bone, RATIONAL rat, BELIEF_NET *bn, BNODE *catch, int accomp_lead) {
  int si,mi,cue,midi_is_true,solo_cue,xxx;

  si = coincident_solo_index(rat);
  mi = coincident_midi_index(rat);
  solo_cue = score.solo.note[si].cue;  /* could be either SOLO_CUE_NUM or ACCOM_CUE_NUM */
  cue = (si >=0 && mi >=0);
  midi_is_true = treat_midi_as_true_time(si,mi);
  //    if (midi_is_true) printf("midi is true at %s %s\n",score.solo.note[si].observable_tag,score.midi.burst[mi].observable_tag);

  /*  if (si >=0) if (strcmp(score.solo.note[si].observable_tag,"9+0/1_a6") == 0) {
    if (si >=0) add_solo_observation_node_pot(bone, si, bn, 0, solo_cue);
    if (mi >=0) add_midi_observation_node_pot(bone,  mi, bn,cue, 1, midi_is_true);
    printf("helloolo\n");
    return;
    }*/


  if (si >=0) add_solo_observation_node_pot(bone, si, bn, accomp_lead, solo_cue);
  if (mi >=0) add_midi_observation_node_pot_catch(bone,  mi, catch, bn,cue, 
						  accomp_lead,midi_is_true);
}

static void
add_backbone_observations_at(BNODE *bone, RATIONAL rat, BELIEF_NET *bn, int accomp_lead) {
  int si,mi,cue,midi_is_true,solo_cue;

  si = coincident_solo_index(rat);
  mi = coincident_midi_index(rat);
  solo_cue = score.solo.note[si].cue;
  cue = (si >=0 && mi >=0);
  midi_is_true = treat_midi_as_true_time(si,mi);
  /*  if (midi_is_true) printf("midi is true at %s %s\n",score.solo.note[si].observable_tag,score.midi.burst[mi].observable_tag);*/
  if (si >=0) add_solo_observation_node_at(bone, si, bn, accomp_lead, solo_cue);
  if (mi >=0) add_midi_observation_node_at(bone,  mi, bn,cue, accomp_lead, midi_is_true);
}



static void
make_composite_list(COMPOSITE_LIST *compl) {
  COMPOSITE_DATA d;
  int i;
 
  alloc_composite_list(compl);
  for (i=0; i < score.solo.num; i++) { 
    d.solo = 1; 
    d.index = i; 
    d.pos = score.solo.note[i].wholerat;
    d.cue = score.solo.note[i].cue;
    add_composite_list(compl,d); 
  }
  for (i=0; i < score.midi.num; i++) { 
    d.solo = 0; 
    d.index = i; 
    d.pos = score.midi.burst[i].wholerat;
    d.cue = score.midi.burst[i].acue;
    add_composite_list(compl,d); 
  }
  qsort(compl->list,compl->num,sizeof(COMPOSITE_DATA),composite_compare);
    uniq_composite_list(compl);  
    /*      for (i=0; i < compl->num; i++) printf("%d %d %d %d/%d\n",i,compl->list[i].solo,compl->list[i].index,compl->list[i].pos.num,compl->list[i].pos.den);  */
}



static void
add_tempo_settings_to_composite(COMPOSITE_LIST *compl) {
  int i,j;
  RATIONAL r;

  for (i=0; i < tempo_list.num; i++) {
    for (j=0; j < compl->num; j++) 
      if (rat_cmp(compl->list[j].pos,tempo_list.el[i].wholerat) >= 0) break;
	if (j == compl->num) {
	  r = tempo_list.el[i].wholerat;
	  printf("ignoring weird tempo setting at %d/%d\n",r.num,r.den);
	  continue;
	 // exit(0);
	}
    compl->list[j].tempo_setting = 1;
    compl->list[j].whole_secs = tempo_list.el[i].whole_secs;
    r = compl->list[j].pos;
    //        printf("tempo (%d) of %f at %d/%d\n",i, compl->list[j].whole_secs,r.num,r.den);
  }
}

static void
make_composite_list_xchg(COMPOSITE_LIST *compl) {
  COMPOSITE_DATA d;
  int i;
  RATIONAL mp;
 
  alloc_composite_list(compl);
  for (i=0; i < score.solo.num; i++) { 
    d.pos = score.solo.note[i].wholerat;
    d.cue = score.solo.note[i].cue;
    d.sxchg = score.solo.note[i].xchg;
    d.axchg = 0;
    d.tempo_setting = 0;
    d.accomp_lead = 0;
    add_composite_list(compl,d); 
  }
  for (i=0; i < score.midi.num; i++) { 
    d.pos = score.midi.burst[i].wholerat;
    d.cue = 0;  /* can't have accompaniment cues */
    d.sxchg = 0;
    d.axchg  =score.midi.burst[i].xchg;
    //    if (score.midi.burst[i].xchg) printf("xchg at %s\n",score.midi.burst[i].observable_tag);
    d.tempo_setting = 0;
    d.accomp_lead = 0;
    add_composite_list(compl,d); 
  }
  //  exit(0);
  qsort(compl->list,compl->num,sizeof(COMPOSITE_DATA),composite_compare_pos);
  uniq_composite_list_xchg(compl);  
  add_tempo_settings_to_composite(compl);
  set_accomp_lead(compl);  
  /*         for (i=0; i < compl->num; i++) printf("%d %d %d %d/%d\n",i,compl->list[i].solo,compl->list[i].index,compl->list[i].pos.num,compl->list[i].pos.den);  
	     exit(0);*/
}



static int
comp_index(COMPOSITE_LIST *compl, RATIONAL rat) {
  int j;

  for (j=0; j < compl->num; j++) if (rat_cmp(compl->list[j].pos,rat) == 0)  return(j);
  return(-1);
}


add_composite_list_args(COMPOSITE_LIST *compl, RATIONAL pos, int cue, int sxchg, int axchg, int tempo_setting, int accomp_lead, 
			int phantom, float whole_secs, int solo_parent) {
  COMPOSITE_DATA d;

  d.pos = pos;
  d.cue =  cue;
  d.sxchg = sxchg;
  d.axchg = axchg;
  d.tempo_setting = tempo_setting;
  d.accomp_lead = accomp_lead;
  d.phantom = phantom;
  d.whole_secs = whole_secs;
  d.solo_parent = solo_parent;
  add_composite_list(compl,d);
}




static void
remove_false_phantom(COMPOSITE_LIST *compl) {
  int i;
  char string[500];

  for (i=compl->num-1; i >= 0; i--) {
    if (compl->list[i].phantom && compl->list[i].accomp_lead && (compl->list[i].axchg == 0)) {
      wholerat2string(compl->list[i].pos,string);
      printf("removing false phantom %s\n",string);
      compl->list[i] = compl->list[--(compl->num)];
    }
  }
  qsort(compl->list,compl->num,sizeof(COMPOSITE_DATA),composite_compare_pos);
}

static void
make_solo_list(COMPOSITE_LIST *compl) {  // 9-09 the collection of events for the solo part of the two parallel models
  COMPOSITE_DATA d;
  int i,j,k,si,sxchg,axchg,tempo_setting,accomp_lead,phantom,cue,solo_parent;
  RATIONAL mp,rat,beat,meas,pos,ts;
  float ws;
  char tag[500];
  float q;
 
  compl->num = 0;
  for (i=0; i < score.solo.num; i++) { 
    add_composite_list_args(compl, score.solo.note[i].wholerat, score.solo.note[i].cue, sxchg = score.solo.note[i].xchg, axchg = 0, 
			    tempo_setting = 0, accomp_lead=0, phantom = 0, ws = 0., solo_parent = 0);
  }
  for (i=0; i < score.midi.num; i++) { 
    if (score.midi.burst[i].xchg == 0) continue;  // add xchg points
    rat = score.midi.burst[i].wholerat;
    j = comp_index(compl, rat);
    if (j != -1)  compl->list[j].axchg = 1;
    else   add_composite_list_args(compl, score.midi.burst[i].wholerat, cue = 0,sxchg = 0, axchg = 1, tempo_setting = 0, 
				   accomp_lead= 0, phantom = 1,  ws = 0., solo_parent = 0);
  }
  for (i=0; i < tempo_list.num; i++) {  // add tempo settings
    j = comp_index(compl, tempo_list.el[i].wholerat);
    if (j != -1)  {
      compl->list[j].tempo_setting = 1;
      compl->list[j].whole_secs = tempo_list.el[i].whole_secs;
    }
    else   add_composite_list_args(compl, tempo_list.el[i].wholerat, cue = 0,sxchg = 0, axchg = 0, tempo_setting = 1, 
				   accomp_lead= 0, phantom = 1,  ws = tempo_list.el[i].whole_secs, solo_parent = 0);
  }
  for (i=1; i <= score.measdex.num; i++)  {
    if (solo_measure_empty(i)) continue;
    meas = score.measdex.measure[i].wholerat;
    beat = score.measdex.measure[i].beat;
    ts = score.measdex.measure[i].time_sig;
    for (k=0; ; k++) {
      pos = beat;
      pos.num *= k;
      if (rat_cmp(pos,ts) >= 0) break;  
      pos = add_rat(pos,meas);
      j = comp_index(compl, pos);
      if (j != -1);
      else add_composite_list_args(compl, pos, cue = 0,sxchg = 0, axchg = 0, tempo_setting = 0, 
				   accomp_lead= 0, phantom = 2,  ws = 0, solo_parent = 0);
    }
  }

  qsort(compl->list,compl->num,sizeof(COMPOSITE_DATA),composite_compare_pos);
  set_accomp_lead(compl);  
  remove_false_phantom(compl);


  /*      for (i=0; i < compl->num; i++) {
    rat = compl->list[i].pos;
    j = coincident_solo_index(rat);
    if (j >= 0) strcpy(tag,score.solo.note[j].observable_tag);
    else {
      j = coincident_midi_index(rat);
    if (j >= 0) strcpy(tag,score.midi.burst[j].observable_tag);
    else strcpy(tag,"none");
    }
    if (i > 0) {
      rat = sub_rat(compl->list[i].pos,compl->list[i-1].pos);
      q = rat.num/(float)rat.den;
      if (q < 0) printf("christopher\n");
    }
    printf("%d\t%s\taxchg = %d\ttempo = %d\tphantom=%d\taccomp_lead = %d\n",i,tag,compl->list[i].axchg,compl->list[i].tempo_setting, compl->list[i].phantom, compl->list[i].accomp_lead);  
  }
  exit(0); */
}



add_accom_cues(COMPOSITE_LIST *compl) { 
  int i,j;

  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].cue == 0) continue;
    //    printf("cue at %s\n",score.solo.note[i].observable_tag);
    for (j=0; j < compl->num; j++) if (rat_cmp(compl->list[j].pos,score.solo.note[i].wholerat) >= 0) break;
    compl->list[j].cue = 1;
  }
}


static int
pos_is_beat(RATIONAL pos) {
  int m;
  RATIONAL beat,meas,q;

  m = wholerat2measnum(pos);
  beat = score.measdex.measure[m].beat;
  meas = wholerat2measrat(pos);
  q = div_rat(meas,beat);
  return(q.den == 1);
}


static void
make_accm_list(COMPOSITE_LIST *compl, COMPOSITE_LIST *scompl) {  // 9-09 the collection of events for the accomp part of the two parallel models
  COMPOSITE_DATA d;
  int i,j,k,si,sxchg,axchg,tempo_setting,accomp_lead,phantom,cue,solo_parent,coinc;
  RATIONAL mp,rat,beat,meas,pos,ts,wr;
  float ws;
  char tag[500];
 
  compl->num = 0;
  for (i=0; i < score.midi.num; i++) { 
    wr = score.midi.burst[i].wholerat;
    coinc =  (comp_index(scompl, wr) != -1);
    solo_parent = (pos_is_beat(wr) && coinc);
    add_composite_list_args(compl, wr , cue=0, sxchg = 0, axchg = score.midi.burst[i].xchg, 
			    tempo_setting = 0, accomp_lead=0, phantom = 0, ws = 0., solo_parent);
  }
  for (i=0; i < score.solo.num; i++) { 
    if (score.solo.note[i].xchg == 0 && score.solo.note[i].cue == 0) continue;  // add xchg points
    rat = score.solo.note[i].wholerat;
    j = comp_index(compl, rat);
    if (j != -1)  compl->list[j].sxchg = 1;
    else  add_composite_list_args(compl, score.solo.note[i].wholerat, cue = 0,sxchg = 1, axchg = 0, tempo_setting = 0, 
				  accomp_lead= 0, phantom = 1,  ws = 0., solo_parent = 0);
  }
  for (i=0; i < tempo_list.num; i++) {  // add tempo settings
    j = comp_index(compl, tempo_list.el[i].wholerat);
    if (j != -1)  {
      compl->list[j].tempo_setting = 1;
      compl->list[j].whole_secs = tempo_list.el[i].whole_secs;
    }
    else   add_composite_list_args(compl, tempo_list.el[i].wholerat, cue = 0,sxchg = 0, axchg = 0, tempo_setting = 1, 
				   accomp_lead= 0, phantom = 1,  ws = tempo_list.el[i].whole_secs, solo_parent = 0);
  }
  for (i=1; i <= score.measdex.num; i++)  {
    if (solo_measure_empty(i)) continue;
    meas = score.measdex.measure[i].wholerat;
    beat = score.measdex.measure[i].beat;
    ts = score.measdex.measure[i].time_sig;
    for (k=0; ; k++) {
      pos = beat;
      pos.num *= k;
      if (rat_cmp(pos,ts) >= 0) break;  
      pos = add_rat(pos,meas);
      coinc =  (comp_index(scompl, pos) != -1);
      j = comp_index(compl, pos);
      if (j != -1);
      else add_composite_list_args(compl, pos, cue = 0,sxchg = 0, axchg = 0, tempo_setting = 0, 
				   accomp_lead= 0, phantom = 2,  ws = 0, solo_parent = coinc);
    }
  }

  qsort(compl->list,compl->num,sizeof(COMPOSITE_DATA),composite_compare_pos);
  set_accomp_lead(compl);  
  remove_false_phantom(compl);
  add_accom_cues(compl);

  /*  for (i=0; i < compl->num; i++) {
    rat = compl->list[i].pos;
    j = coincident_midi_index(rat);
    if (j >= 0) strcpy(tag,score.midi.burst[j].observable_tag);
    else {
      j = coincident_solo_index(rat);
    if (j >= 0) strcpy(tag,score.solo.note[j].observable_tag);
    else strcpy(tag,"none");
    }
    printf("%d\t%s\taxchg = %d\ttempo = %d\tphantom=%d\taccomp_lead = %d\tcue=%d\n",i,tag,compl->list[i].axchg,compl->list[i].tempo_setting, compl->list[i].phantom, compl->list[i].accomp_lead,compl->list[i].cue);
  }
  exit(0); */
}


int
training_tag(char *pos, char *prefix, float *wholesecs) { // pos is string of position.  set prefix on return
  int i,len,f,j;
  char *ptr;

  prefix[0] = 0;
  for (i=0; i < glob_tr.num; i++) {
    len = strlen(glob_tr.tag[i]); 
    for (j =len; j > 0; j--) if (glob_tr.tag[i][j] == '_')  break;
    if (j == 0) { printf("bad tag %s\n",glob_tr.tag[i]); exit(0); }
    if (strcmp(glob_tr.tag[i] + j + 1,pos) != 0) continue;
    strcpy(prefix,glob_tr.tag[i]);
    prefix[j] = 0;
    *wholesecs = glob_tr.qf[i].m.el[1][0];  /* I'm assuming the 1st element is always tempo but this 
					      isn't guaranteed */
    return(1);
  }
  return(0);
} 

int
train_tag(int type, RATIONAL rat, float *wholesecs) { // pos is string of position.  set prefix on return
  int i,len,f,j;
  char *ptr,*tag;

  tag = trainable_tag_rat(type,rat);

  for (i=0; i < glob_tr.num; i++) {
    if (strcmp(glob_tr.tag[i],tag) != 0) continue;
    *wholesecs = glob_tr.qf[i].m.el[1][0];  /* I'm assuming the 1st element is always tempo but this 
					      isn't guaranteed */
    return(1);
  }
  return(0);
} 

static QUAD_FORM 
extract_trained_qf(char *tag, TRAINING_RESULTS tr) {
  int i;

  //    printf("%s\n",tag);
  for (i=0; i < tr.num; i++) {
    if (strcmp(tr.tag[i],tag) != 0) continue;
    //    if (strcmp(tag,"bbone_update_236+0/1") == 0) exit(0); 
    return(tr.qf[i]);
  }
  if (i == tr.num) {
    printf("uh... no match for %s found in results\n",tag);
    exit(0);
  }
} 

static int 
trained_qf_index(BNODE *bn, TRAINING_RESULTS tr) {
  int i;

  if (bn->trainable == 0 || bn->trainable_tag == NULL)
    if (bn->trainable != 0 || bn->trainable_tag != NULL) {
      printf("probllem in trained_qf_index\n"); exit(0);
    }

  if (bn->trainable == 0) return(-1);
  for (i=0; i < tr.num; i++) if (strcmp(tr.tag[i],bn->trainable_tag) == 0) {
    bn->trained = 1;  // 7-06  a little obscure to bury this detail here, but can't see harm
    return(i);
  }
  if (tr.num) printf("substituting for %s\n",bn->trainable_tag);
  return(-1);
} 



add_composite_graph(BELIEF_NET *bn, TRAINING_RESULTS tr) {
  int i,num,index,solo,si,mi,cue,type,utype,sightread_cue,accomp_lead=0;
  BNODE *last,*this,*update;
  MATRIX mat,tempo,evol;
  QUAD_FORM e,z;
  CLIQUE *cl;
  COMPOSITE_LIST compl;
  float length;
  RATIONAL rat,r1,r2;

  
  make_composite_list(&compl);
  for (i=0; i < compl.num; i++) {
    index = compl.list[i].index;
    rat = compl.list[i].pos;
    cue = compl.list[i].cue;
    type = (compl.list[i].solo) ? BACKBONE_SOLO_NODE : BACKBONE_MIDI_NODE;
    sightread_cue = (compl.list[i].solo) ? (score.solo.note[index].wholerat.den == 2) : 0;
    /*    if (sightread_cue)  printf("note time = %s\n",score.solo.note[index].observable_tag);*/
    utype = (compl.list[i].solo) ? BACKBONE_UPDATE_SOLO_NODE : BACKBONE_UPDATE_MIDI_NODE;
    if (i) length = rat2f(rat) - rat2f(compl.list[i-1].pos);
    if (i > 0 && cue == 0/* sightread_cue == 0*/)  {
      z = QFpoint(Mzeros(SOLO_DIM,1));
	    /*      e = QFtempo_change(.01*length); /* best yet with training */


      /*            e = QFtempo_change_pos_slop_parms(length*.5, length*1.);   */
      /*      e = QFtempo_change_pos_slop_parms(length*.0005, length*.05);   */
      this  = alloc_belief_node_pot(BACKBONE_DIM, type, index, bn);
      add_backbone_observations(this, rat, bn, accomp_lead);
      mat = evol_matrix(BACKBONE_DIM,  length);
      update  = alloc_belief_node_pot(BACKBONE_DIM, utype, compl.list[i].index, bn);
      if (tr.num == 0)   e = QFtempo_change_pos_slop(length);   
      else  e = extract_trained_qf(update->trainable_tag, tr);     
      cl = make_marginal_potential(update,e);
      add_potential(bn,cl);
      cl = make_conditional_potential2(last, update, this, mat, Miden(BACKBONE_DIM), z);
      add_potential(bn,cl);
    }
    else if (i == 0 || (cue  && compl.list[i].solo)) {
      accomp_lead = 0;
      this  = alloc_belief_node_pot(SOLO_DIM,  SOLO_PHRASE_START, compl.list[i].index, bn);
      if (tr.num == 0)  e = QFmeas_size(score.meastime,1.);                          
      else  e = extract_trained_qf(this->trainable_tag, tr);       
      add_backbone_observations(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }
    else {  /* an accompaniment cue: handing off to accompaniment */
      accomp_lead = 1;
      z = QFpoint(Mzeros(BACKBONE_DIM,1));
      this  = alloc_belief_node_pot(BACKBONE_DIM, type, index, bn);
      add_backbone_observations(this, rat, bn, accomp_lead);
      update  = alloc_belief_node_pot(1 /* dim is 1 */, utype, index, bn);
      if (tr.num == 0)  e = QFmv(Mconst(1,1,score.meastime),Miden(1));
      else e = extract_trained_qf(update->trainable_tag, tr);    
      cl = make_marginal_potential(update,e);
      add_potential(bn,cl);
      mat = Mzeros(BACKBONE_DIM,1); mat.el[1][0] = 1;
      evol = evol_matrix(BACKBONE_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0;
      cl = make_conditional_potential2(last, update, this, evol,mat, z);
      add_potential(bn,cl);
    }
    last = this;
    clear_matrix_buff();
  }
}

static void
fix_tempo(char *pos, RATIONAL unit, float upm, BELIEF_NET *bn) {
  RATIONAL rat;
  int i,j;
  float whole_secs;
  QUAD_FORM e;
  CLIQUE *cl;



  rat = string2wholerat(pos);
  //  printf("pos = %s\n",pos);

  for (i=0; i < bn->num; i++) 
    if (bn->list[i]->note_type == BACKBONE_NODE && rat_cmp(bn->list[i]->wholerat,rat) == 0) break;
  if (i == bn->num) { printf("couldn't find node with pos %s\n",pos); exit(0); }
  whole_secs =  (float) (60*unit.den) / (float) (unit.num*upm); 
  /*  printf("whole_secs = %f\n",whole_secs);*/
  e = QFatempo(whole_secs);
  /*    QFprint(e);    exit(0);*/
  cl = make_marginal_potential(bn->list[i], e);
  add_potential(bn,cl);
}

static void
set_tempo(RATIONAL pos, float whole_secs, BELIEF_NET *bn) {
  int i,j,t;
  QUAD_FORM e;
  CLIQUE *cl;
  RATIONAL wr,lwr,wrn;


  /*  for (i=0; i < bn->num; i++) 
    if (bn->list[i]->note_type == BACKBONE_XCHG && rat_cmp(bn->list[i]->wholerat,pos) == 0) break;
  if (i < bn->num) bn->list[i].trainable = 0;  /* this node no longer trainiable */

  //  printf("pos = %d/%d\n",pos.num,pos.den);


  for (i=0; i < bn->num; i++) {
    t = bn->list[i]->note_type;
    if (t != BACKBONE_NODE && t != BACKBONE_PHRASE_START) continue;
    wr = bn->list[i]->wholerat;
    lwr = (i==0) ? wr : bn->list[i-1]->wholerat;
        if ((rat_cmp(lwr,pos) <= 0) && (rat_cmp(wr,pos) >= 0)) break;
	//    if (rat_cmp(wr,pos) == 0) break;
  }
  if (i == bn->num) { printf("couldn't find node with pos %d/%d\n",pos.num,pos.den); exit(0); }
  printf("i = %d pos = %d/%d whole_secs = %f\n",i, pos.num,pos.den,whole_secs);
  e = QFatempo(whole_secs);
  /*    QFprint(e);    exit(0);*/
  cl = make_marginal_potential(bn->list[i], e);
  add_potential(bn,cl);
}

float
get_whole_secs(RATIONAL r) {
  int i;
  RATIONAL w;

  if (tempo_list.num == 0) {
    printf("no tempo information\n"); exit(0);
    // return(score.meastime);
  }
  for (i=0; i < tempo_list.num; i++) {
    w = tempo_list.el[i].wholerat;
    //    if (r.num == 93)    printf("the tempo for %d/%d = %f\n",w.num,w.den,tempo_list.el[i].whole_secs);
    if (rat_cmp(tempo_list.el[i].wholerat,r) > 0) {
      //            printf("tempo for %d/%d = %f\n",r.num,r.den,tempo_list.el[i-1].whole_secs);

      if (i == 0) { printf("no tempo for %d/%d\n",r.num,r.den); exit(0);  }

      return(tempo_list.el[i-1].whole_secs);
    }
  }
  //  printf("tempo is %f num = %d\n",tempo_list.el[i-1].whole_secs,tempo_list.num); exit(0);
  return(tempo_list.el[i-1].whole_secs);
  printf("couldn't find tempo for %d/%d\n",r.num,r.den);
  exit(0);
}


is_hand_set_tempo(RATIONAL r) {
  int i;

  for (i=0; i < tempo_list.num; i++) 
    if (rat_cmp(tempo_list.el[i].wholerat,r) == 0) return(tempo_list.el[i].hand);
  return(0);
}

#ifdef ROMEO_JULIET
  //CF:  very important constants!  used in get_untrained_update
  #define AGOGIC_CONST .04 //.04 // .0004 //.4 //.00005 //.4 //.020833
  #define ACCEL_CONST  .01 // .001 // .01//  .0001 // .1 //.0002 // .1 //.020833
#else
  //CF:  very important constants!  used in get_untrained_update
  #define AGOGIC_CONST .04 // .0004 //.4 //.00005 //.4 //.020833
  #define ACCEL_CONST  .01//  .0001 // .1 //.0002 // .1 //.020833
#endif

/*#define INIT_MEAS_SIZE_CONST .25  // this seems high, but downside very slim
static 
float meas_size_var(float ms) {  // assuming std should be prop to mean for init meas size
  float std;

  std = INIT_MEAS_SIZE_CONST*ms;
  return(std*std);
  }*/

/* might have had beeter results in hindemith with .004 and .001 */


QUAD_FORM
get_untrained_accm_update(float ws, float len) { // for accm line
  QUAD_FORM e;
  float t,ms_var,stretch_var;

  t = ws*len;
  stretch_var = (t > .5) ? .1*t : 0;
  if (t > 1.) stretch_var = t*t;
  ms_var = ACCEL_CONST*t;
  //  e  =  QFpos_meassize_update(stretch_var,ms_var);
  //  e  =  QFpos_meassize_update(AGOGIC_CONST*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal 

  e  =  QFpos_meassize_update(stretch_var,50*ACCEL_CONST*t);  //CF:  zero mean, diagonal 
  return(e);
}

QUAD_FORM
get_untrained_update(float ws, float len) {
  QUAD_FORM e;
  float t,ms_var,stretch_var;

  t = ws*len;
  /*
  stretch_var = (t > .5) ? .1*t : 0;
  ms_var = ACCEL_CONST*t;
  //  e  =  QFpos_meassize_update(stretch_var,ms_var);
  e  =  QFpos_meassize_update(AGOGIC_CONST*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal  */
#ifdef PARAM_EXPERIMENT
  stretch_var = (t > .5) ? .1*t : 0;
  if (t > 1.) stretch_var = t*t;
  ms_var = ACCEL_CONST*t;
  //  e  =  QFpos_meassize_update(stretch_var,ms_var);
  //  e  =  QFpos_meassize_update(AGOGIC_CONST*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal 
  e  =  QFpos_meassize_update(stretch_var,.5*ACCEL_CONST*t);  //CF:  zero mean, diagonal 
#else
  e  =  QFpos_meassize_update(AGOGIC_CONST*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal 
#endif
  return(e);
}

QUAD_FORM
get_untrained_update_var(float ws, float len, RATIONAL rat) {
  QUAD_FORM e;
  float t,ms_var,stretch_var;
  RATIONAL h,quot,mp;
  static int last=0;
  char text[200];

  mp = wholerat2measrat(rat);
  h.num = 3; h.den = 8;
  t = ws*len;
  stretch_var = (t > .5) ? .1*t : 0;
  ms_var = ACCEL_CONST*t;
  quot = div_rat(rat,h);
  if (quot.num % quot.den == 0) {
  //  if (mp.den == 8 && (mp.num == 1 || mp.num == 4)) {
    wholerat2string(rat,text);
    printf("r+j: pos = %s\n",text);
    //  e = QFpos_meassize_update(0*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal 
    //    e = QFpos_meassize_update(AGOGIC_CONST*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal 
    //    e = QFpos_meassize_update(AGOGIC_CONST*t,0*t);  //CF:  zero mean, diagonal 
    e = QFpos_meassize_update(AGOGIC_CONST*t,.0*t);  //CF:  zero mean, diagonal 
  }
  else    e = QFpos_meassize_update(0,.0*t);  //CF:  zero mean, diagonal 
  //  else e = QFpos_meassize_update(0,0);

  e = QFpos_meassize_update(AGOGIC_CONST*t,ACCEL_CONST*t);  //CF:  zero mean, diagonal 
    
  
  //  e  =  QFpos_meassize_update(stretch_var,ms_var);
  /*  if (tc_near) e  =  QFpos_meassize_update(AGOGIC_CONST*t,.05*t);  //CF:  zero mean, diagonal 
      else e  =          QFpos_meassize_update(AGOGIC_CONST*t,0*t);  //CF:  zero mean, diagonal  */

  last = (mp.num == 0);
  return(e);
}


#define WS_STD_CONST .25 // .05  // 2-10 changed to .025

static float
whole_secs_var(float w) { // w is wholenote secs
  float ws_std;

  ws_std = w*WS_STD_CONST;
  return(ws_std*ws_std);
}


/*QUAD_FORM
init_trainable_dist(RATIONAL rat, int type) {  
// this is the initial dists of the trainiable cue and xchg dists
//						  copied from add_composite_graph_xchg().  Better programming
//						  would have that routine use init_trainiable_dist(),
//						  since there are some important constants that should
//						  be shared.  I hope to do this later, but it is minor

  float whole_secs,var;
  MATRIX m,v;

  whole_secs = get_whole_secs(rat);
  var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
  if (type == BACKBONE_PHRASE_START) {
    return(QFmeas_size(whole_secs,var));
  }
  if (type == BACKBONE_XCHG) {
    m = Mconst(BACKBONE_DIM,1, whole_secs);  m.el[0][0] = 0;
    v = Miden(BACKBONE_DIM); v.el[0][0] = .1; v.el[1][1] = var;  
    return(QFmv(m,v));
  }
}
*/

static int
is_tempo_change_near(int index, COMPOSITE_LIST *compl) {
  int i,ret=0;
  RATIONAL here,there,diff,limit;

  limit.num = 6; limit.den = 8;  // one 6/8 measure
  here = compl->list[index].pos;
  for (i=index; i < compl->num; i++) {
    there = compl->list[i].pos;
    diff = sub_rat(there,here);
    if (rat_cmp(diff,limit) > 0) break;
    if (compl->list[i].axchg) ret=1;
    if (compl->list[i].sxchg) ret=1;
    if (compl->list[i].tempo_setting) ret=1;
    if (compl->list[i].cue) ret=1;
  }
  return(ret);  
}




static int
make_solo_line(BELIEF_NET *bn, TRAINING_RESULTS tr,COMPOSITE_LIST *compl) {
  int i,index,cue,axchg,sxchg,tempo_set,hand_set_tempo,accomp_lead,ti,long_gap;
  RATIONAL rat;
  float length,whole_secs,var;
  BNODE *this,*last,*update;
  QUAD_FORM e,z;
  CLIQUE *cl;
  MATRIX evol,m,v,mat,A,A1,A2;
  char string[500];
  int null_inf[2] = {0,1};  // to make potential where 2nd compoenent (tempo) is deterministically 0 and 1st compoenent (pos) is uniform

  for (i=0; i < compl->num; i++) {
    index = compl->list[i].index;
    rat = compl->list[i].pos;
    whole_secs = get_whole_secs(rat);
    cue = compl->list[i].cue;
    axchg = compl->list[i].axchg;
    sxchg = compl->list[i].sxchg;
    tempo_set = compl->list[i].tempo_setting;
    hand_set_tempo = is_hand_set_tempo(rat);
    accomp_lead = compl->list[i].accomp_lead;
    if (i) length = rat2f(rat) -  rat2f(compl->list[i-1].pos);  //CF:  length of note in musical time, as float
    long_gap = (length*whole_secs > 5);  // greater than 5 secs don't connect


    if (i == 0 || cue) { //eg. this event is the start of a phrase
      compl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, SOLO_PHRASE_START, rat, bn, 0.); //CF:  graph BNODE element
      ti = trained_qf_index(this, tr);       //CF:  look for a training data for this (cue) node
      //CF:  if no training data, get tempo prior direct from score; else used the trained prior from tr
      //CF:  get_whole_secs uses a table of score_positions*tempo_changes
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      e =  (ti == -1 || hand_set_tempo)  ?  QFmeas_size(whole_secs,var) : tr.qf[ti];   
      //CF:  make BNODE's potential
      add_solo_observation(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl); //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
    }  
    //CF:  spec cases for exchanges
    else if (long_gap || axchg || sxchg || tempo_set) {  /* handing off from solo to accomp or vice-versat */
      /*	wholerat2string(rat,string);
		printf("at %s long_gap = %d tempo_set = %d axchg = %d sxchg = %d\n",string,long_gap,tempo_set,axchg,sxchg);  */
      /* if previous an axchg ignore the prediction of position but take the new tempo.  otherwise take both */
      compl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, SOLO_NODE, rat, bn,length);
      add_solo_observation(this, rat, bn, accomp_lead);
      update  = alloc_belief_node_parallel(SOLO_DIM, SOLO_XCHG_NODE, rat, bn,length); //CF:only 1D update for tempo only --- looks like 2D???

      //      whole_secs = (tempo_set) ? compl->list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      if (tempo_set && compl->list[i].whole_secs != whole_secs) {
	printf("this shouldn't happen, go back to setting whole_secs in this else if block as commented out above\n");
	exit(0);
      }

      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      ti = trained_qf_index(update, tr);       
      //      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Msc(Miden(1),.01)) : tr.qf[ti];  // really haven't figured out variance yet.
      if (ti == -1 || hand_set_tempo) {  // || hand_set_tempo    added 4-05  overides training
	wholerat2string(rat,string);
	printf("tempo fixed to %f at %d/%d %s %s\n",whole_secs,rat.num,rat.den,update->trainable_tag,string);
	m = Mconst(SOLO_DIM,1, whole_secs);  m.el[0][0] = 0;
	v = Miden(SOLO_DIM); v.el[0][0] = .1; v.el[1][1] = var;  
	// maybe want large variance so this tempo setting will not be influenced by prior, 
	// hence can override
	e = QFmv(m,v);
      }
      else e = tr.qf[ti];   //CF:  trained prior for update node
      cl = make_marginal_potential(update,e); /* the new tempo */
      if ( add_potential(bn,cl) == 0) return(0);
      /* get new tempo from the update node */
      evol = evol_matrix(SOLO_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0; 
      /* evol: what previous pos and tempo predict */
      /* new pos taken as deterministic fn of previous tempo and pos --- might want to rethink this */ 
      if (long_gap || compl->list[i-1].axchg ) cl = make_conditional_potential(update,this,A = Miden(SOLO_DIM), e =  QFnull_inf(SOLO_DIM,null_inf));
      else cl = make_conditional_potential2(last, update, this, A1=evol,A2=Miden(SOLO_DIM), e = QFpoint(Mzeros(SOLO_DIM,1)) );
      add_potential(bn,cl);
    }
    //CF:  usual case
    else {
      z = QFpoint(Mzeros(SOLO_DIM,1)); //CF:  point distribution on e in y=Ax+e; ie. y is conditionally deterministic
      compl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, SOLO_NODE, rat, bn,length);  //CF:  construct BNODE
      add_solo_observation(this, rat, bn, accomp_lead);  //CF: construct BNODEs and potentials for hanging-off obs nodes
      mat = evol_matrix(SOLO_DIM,  length);  //CF:  state evolution matrix for this note, [[1 note_length] [0 1]]
      update  = alloc_belief_node_parallel(SOLO_DIM, SOLO_UPDATE_NODE, rat, bn, length); //CF: construct update node
      ti = trained_qf_index(update, tr);       //CF:  index into training list (-1 if not found)
      //CF:  if update has training data use it; else use an approximate (but important) standard prior 
      e = (ti == -1) ?  get_untrained_update(whole_secs,length) : tr.qf[ti];   //CF:  untrained prior for update node
      cl = make_marginal_potential(update,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl);   //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.

      //CF: x_{n+1}= mat*x_n + Identity*update + z  (where z is always zero in this case)
      cl = make_conditional_potential2(last, update, this, mat, Miden(SOLO_DIM), z);  //CF:potential fn for backbone node
      add_potential(bn,cl);  //CF:  store associated (BNODEs,quadnorm) pair in potential list 
    }
    last = this; //CF:  the previously created backbone node
    clear_matrix_buff();
  }
  return(1);
}


static void
Mpos_mix(MATRIX *m1, MATRIX *m2, float p) {  /* pos is convex combination of that predicted by m1,m2.  tempo comes from m1*/
  int i;

  for (i=0; i < m1->cols; i++) {
    m1->el[0][i] *= p;
    m2->el[0][i] *= (1-p);
    m2->el[1][i] = 0;
  }
}

static int
exper_make_accm_line(BELIEF_NET *bn, TRAINING_RESULTS tr, COMPOSITE_LIST *acompl, COMPOSITE_LIST *scompl) {
  int i,index,cue,axchg,sxchg,tempo_set,hand_set_tempo,accomp_lead,ti,solo_parent,si;
  RATIONAL rat;
  float length,whole_secs,var;
  BNODE *this,*last,*update,*above;
  QUAD_FORM e,z;
  CLIQUE *cl;
  MATRIX evol,m,v,mat,id;
  char string[500];
  int null_inf[2] = {0,1};  // to make potential where 2nd compoenent (tempo) is deterministically 0 and 1st compoenent (pos) is uniform

  for (i=0; i < acompl->num; i++) {
    index = acompl->list[i].index;
    rat = acompl->list[i].pos;
    cue = acompl->list[i].cue;
    axchg = acompl->list[i].axchg;
    sxchg = acompl->list[i].sxchg;
    tempo_set = acompl->list[i].tempo_setting;
    hand_set_tempo = is_hand_set_tempo(rat);
    accomp_lead = acompl->list[i].accomp_lead;
    solo_parent = acompl->list[i].solo_parent;
    if (i) length = rat2f(rat) -  rat2f(acompl->list[i-1].pos);  //CF:  length of note in musical time, as float
    if (i == 0 || cue) { //eg. this event is the start of a phrase
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_PHRASE_START, rat, bn, 0.); //CF:  graph BNODE element
      ti = trained_qf_index(this, tr);       //CF:  look for a training data for this (cue) node
      //CF:  if no training data, get tempo prior direct from score; else used the trained prior from tr
      //CF:  get_whole_secs uses a table of score_positions*tempo_changes
      whole_secs = get_whole_secs(rat);
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      e =  (ti == -1 || hand_set_tempo)  ?  QFmeas_size(whole_secs,var) : tr.qf[ti];   
      //CF:  make BNODE's potential
      add_accm_observation(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl); //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
    }  
    //CF:  spec cases for exchanges
    else if (axchg || sxchg || tempo_set) {  /* handing off from solo to accomp or vice-versat */
      //      printf("axchg = %d sxchg = %d\n",axchg,sxchg); 
      z = QFpoint(Mzeros(SOLO_DIM,1));
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_NODE, rat, bn,length);
      add_accm_observation(this, rat, bn, accomp_lead);
      update  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_XCHG_NODE, rat, bn,length); //CF:only 1D update for tempo only --- looks like 2D???
      whole_secs = (tempo_set) ? acompl->list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      ti = trained_qf_index(update, tr);       
      // should never find training at present
      //      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Msc(Miden(1),.01)) : tr.qf[ti];  // really haven't figured out variance yet.
      if (ti == -1 || hand_set_tempo) {  // || hand_set_tempo    added 4-05  overides training
	wholerat2string(rat,string);
	printf("tempo fixed to %f at %d/%d %s %s\n",whole_secs,rat.num,rat.den,update->trainable_tag,string);
	m = Mconst(SOLO_DIM,1, whole_secs);  m.el[0][0] = 0;
	v = Miden(SOLO_DIM); v.el[0][0] = .1; v.el[1][1] = var;  
	// maybe want large variance so this tempo setting will not be influenced by prior, 
	// hence can override
	e = QFmv(m,v);
      }
      else e = tr.qf[ti];   //CF:  trained prior for update node
      cl = make_marginal_potential(update,e); /* the new tempo */
      if ( add_potential(bn,cl) == 0) return(0);
      /* get new tempo from the update node */
      evol = evol_matrix(SOLO_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0; 
      /* evol: what previous pos and tempo predict */
      /* new pos taken as deterministic fn of previous tempo and pos --- might want to rethink this */ 
      cl = make_conditional_potential2(last, update, this, evol,Miden(SOLO_DIM), z);
      add_potential(bn,cl);
    }
    else if (solo_parent) {
      z = QFpoint(Mzeros(SOLO_DIM,1)); //CF:  point distribution on e in y=Ax+e; ie. y is conditionally deterministic
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_NODE, rat, bn,length);  //CF:  construct BNODE
      add_accm_observation(this, rat, bn, accomp_lead);  //CF: construct BNODEs and potentials for hanging-off obs nodes
      mat = evol_matrix(SOLO_DIM,  length);  //CF:  state evolution matrix for this note, [[1 note_length] [0 1]]
      update  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_UPDATE_NODE, rat, bn, length); //CF: construct update node
      ti = trained_qf_index(update, tr);       //CF:  index into training list (-1 if not found)
      // should never find in training at present
      whole_secs = get_whole_secs(rat);   //CF:  current inverse tempo
      //CF:  if update has training data use it; else use an approximate (but important) standard prior 
      e = (ti == -1) ?  get_untrained_accm_update(whole_secs,length) : tr.qf[ti];   //CF:  untrained prior for update node
      cl = make_marginal_potential(update,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl);   //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
      si = comp_index(scompl, rat);
      if (si == -1) { 
	wholerat2string(rat,string);
	printf("no not this at %s\n",string); exit(0); 
      }
      above = scompl->list[si].bnode;
      id = Miden(SOLO_DIM);
      Mpos_mix(&mat,&id,0.);
      //CF: x_{n+1}= mat*x_n + Identity*update + z  (where z is always zero in this case)
      cl = make_conditional_potential3(last, above, update, this, mat, id, Miden(SOLO_DIM), z);  //CF:potential fn for backbone node
      add_potential(bn,cl);  //CF:  store associated (BNODEs,quadnorm) pair in potential list 
    }
    else { // the usual case
      z = QFpoint(Mzeros(SOLO_DIM,1)); //CF:  point distribution on e in y=Ax+e; ie. y is conditionally deterministic
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_NODE, rat, bn,length);  //CF:  construct BNODE
      add_accm_observation(this, rat, bn, accomp_lead);  //CF: construct BNODEs and potentials for hanging-off obs nodes
      mat = evol_matrix(SOLO_DIM,  length);  //CF:  state evolution matrix for this note, [[1 note_length] [0 1]]
      update  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_UPDATE_NODE, rat, bn, length); //CF: construct update node
      ti = trained_qf_index(update, tr);       //CF:  index into training list (-1 if not found)
      // should never find in training at present
      whole_secs = get_whole_secs(rat);   //CF:  current inverse tempo
      //CF:  if update has training data use it; else use an approximate (but important) standard prior 
      e = (ti == -1) ?  get_untrained_accm_update(whole_secs,length) : tr.qf[ti];   //CF:  untrained prior for update node
      cl = make_marginal_potential(update,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl);   //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.

      //CF: x_{n+1}= mat*x_n + Identity*update + z  (where z is always zero in this case)
      cl = make_conditional_potential2(last, update, this, mat, Miden(SOLO_DIM), z);  //CF:potential fn for backbone node
      add_potential(bn,cl);  //CF:  store associated (BNODEs,quadnorm) pair in potential list 
    }
    last = this; //CF:  the previously created backbone node
    clear_matrix_buff();
  }
  return(1);
}


static int
make_accm_line(BELIEF_NET *bn, TRAINING_RESULTS tr, COMPOSITE_LIST *acompl, COMPOSITE_LIST *scompl) {
  int i,index,cue,axchg,sxchg,tempo_set,hand_set_tempo,accomp_lead,ti;
  RATIONAL rat;
  float length,whole_secs,var;
  BNODE *this,*last,*update;
  QUAD_FORM e,z;
  CLIQUE *cl;
  MATRIX evol,m,v,mat;
  char string[500];
  int null_inf[2] = {0,1};  // to make potential where 2nd compoenent (tempo) is deterministically 0 and 1st compoenent (pos) is uniform

  for (i=0; i < acompl->num; i++) {
    index = acompl->list[i].index;
    rat = acompl->list[i].pos;
    cue = acompl->list[i].cue;
    axchg = acompl->list[i].axchg;
    sxchg = acompl->list[i].sxchg;
    tempo_set = acompl->list[i].tempo_setting;
    hand_set_tempo = is_hand_set_tempo(rat);
    accomp_lead = acompl->list[i].accomp_lead;
    if (i) length = rat2f(rat) -  rat2f(acompl->list[i-1].pos);  //CF:  length of note in musical time, as float
    if (i == 0 || cue) { //eg. this event is the start of a phrase
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_PHRASE_START, rat, bn, 0.); //CF:  graph BNODE element
      ti = trained_qf_index(this, tr);       //CF:  look for a training data for this (cue) node
      //CF:  if no training data, get tempo prior direct from score; else used the trained prior from tr
      //CF:  get_whole_secs uses a table of score_positions*tempo_changes
      whole_secs = get_whole_secs(rat);
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      e =  (ti == -1 || hand_set_tempo)  ?  QFmeas_size(whole_secs,var) : tr.qf[ti];   
      //CF:  make BNODE's potential
      add_accm_observation(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl); //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
    }  
    //CF:  spec cases for exchanges
    else if (axchg || sxchg || tempo_set) {  /* handing off from solo to accomp or vice-versat */
      //      printf("axchg = %d sxchg = %d\n",axchg,sxchg); 
      z = QFpoint(Mzeros(SOLO_DIM,1));
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_NODE, rat, bn,length);
      add_accm_observation(this, rat, bn, accomp_lead);
      update  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_XCHG_NODE, rat, bn,length); //CF:only 1D update for tempo only --- looks like 2D???
      whole_secs = (tempo_set) ? acompl->list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      ti = trained_qf_index(update, tr);       
      // should never find training at present
      //      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Msc(Miden(1),.01)) : tr.qf[ti];  // really haven't figured out variance yet.
      if (ti == -1 || hand_set_tempo) {  // || hand_set_tempo    added 4-05  overides training
	wholerat2string(rat,string);
	printf("tempo fixed to %f at %d/%d %s %s\n",whole_secs,rat.num,rat.den,update->trainable_tag,string);
	m = Mconst(SOLO_DIM,1, whole_secs);  m.el[0][0] = 0;
	v = Miden(SOLO_DIM); v.el[0][0] = .1; v.el[1][1] = var;  
	// maybe want large variance so this tempo setting will not be influenced by prior, 
	// hence can override
	e = QFmv(m,v);
      }
      else e = tr.qf[ti];   //CF:  trained prior for update node
      cl = make_marginal_potential(update,e); /* the new tempo */
      if ( add_potential(bn,cl) == 0) return(0);
      /* get new tempo from the update node */
      evol = evol_matrix(SOLO_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0; 
      /* evol: what previous pos and tempo predict */
      /* new pos taken as deterministic fn of previous tempo and pos --- might want to rethink this */ 
      cl = make_conditional_potential2(last, update, this, evol,Miden(SOLO_DIM), z);
      add_potential(bn,cl);
    }
    //CF:  usual case
    else {
      z = QFpoint(Mzeros(SOLO_DIM,1)); //CF:  point distribution on e in y=Ax+e; ie. y is conditionally deterministic
      acompl->list[i].bnode = this  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_NODE, rat, bn,length);  //CF:  construct BNODE
      add_accm_observation(this, rat, bn, accomp_lead);  //CF: construct BNODEs and potentials for hanging-off obs nodes
      mat = evol_matrix(SOLO_DIM,  length);  //CF:  state evolution matrix for this note, [[1 note_length] [0 1]]
      update  = alloc_belief_node_parallel(SOLO_DIM, ACCOM_UPDATE_NODE, rat, bn, length); //CF: construct update node
      ti = trained_qf_index(update, tr);       //CF:  index into training list (-1 if not found)
      // should never find in training at present
      whole_secs = get_whole_secs(rat);   //CF:  current inverse tempo
      //CF:  if update has training data use it; else use an approximate (but important) standard prior 
      e = (ti == -1) ?  get_untrained_accm_update(whole_secs,length) : tr.qf[ti];   //CF:  untrained prior for update node
      cl = make_marginal_potential(update,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl);   //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.

      //CF: x_{n+1}= mat*x_n + Identity*update + z  (where z is always zero in this case)
      cl = make_conditional_potential2(last, update, this, mat, Miden(SOLO_DIM), z);  //CF:potential fn for backbone node
      add_potential(bn,cl);  //CF:  store associated (BNODEs,quadnorm) pair in potential list 
    }
    last = this; //CF:  the previously created backbone node
    clear_matrix_buff();
  }
  return(1);
}


static void
connect_solo_accm(BELIEF_NET *bn,COMPOSITE_LIST *solo_compl, COMPOSITE_LIST *accm_compl) {
  int i,js,ja,k,m,at,st;
  BNODE *ba,*bs;
  RATIONAL meas,beat,ts,pos,q;
  MATRIX I;
  QUAD_FORM e,ee;
  CLIQUE *cl;
  char string[500];


  I = Miden(SOLO_DIM);
  e = QFpos_var(SOLO_DIM,.1);
  ee = QFhold_steady();
   for (i=0; i < solo_compl->num; i++) {
    pos = solo_compl->list[i].pos;
    m = wholerat2measnum(pos);
    beat = score.measdex.measure[m].beat;
    meas = wholerat2measrat(pos);
    q = div_rat(meas,beat);
    if (q.den != 1) continue;  // (q.den == 1) <==> beat divides meas
    // should make a connection here
    wholerat2string(pos,string);
    bs = solo_compl->list[i].bnode;
    ja = comp_index(accm_compl, pos);
    if (ja == -1) { printf("couldn't find %s there\n",string); exit(0); }
    ba = accm_compl->list[ja].bnode; 
    at = ba->note_type;
    st = bs->note_type;
    /*if (st == SOLO_PHRASE_START || st == SOLO_XCHG_NODE || at == ACCOM_XCHG_NODE) cl = make_conditional_potential(bs,ba,I,ee);
      else */  
    cl = make_conditional_potential(bs,ba,I,e);
    add_potential(bn,cl);
    printf("connecting nodes at %s\n",string);
  }
}


static int
add_parallel_solo_accom_graph(BELIEF_NET *bn, TRAINING_RESULTS tr) {
  static COMPOSITE_LIST solo_compl,accm_compl;

  if (solo_compl.list == NULL) alloc_composite_list(&solo_compl);
  if (accm_compl.list == NULL) alloc_composite_list(&accm_compl);
  make_solo_list(&solo_compl);
  make_solo_line(bn,tr,&solo_compl);
  make_accm_list(&accm_compl, &solo_compl);
  make_accm_line(bn,tr,&accm_compl,&solo_compl);
  connect_solo_accm(bn,&solo_compl,&accm_compl);
  return(1);
}


static int
use_trained_update(int ti, BNODE *last, BNODE *last_update, TRAINING_RESULTS tr) {
  if (ti == -1) return(0);
  if (ti == 0) return(1);
  if (last != NULL && last->trainable && strcmp(tr.tag[ti-1],last->trainable_tag) == 0) return(1);
  if (last_update != NULL && last_update->trainable && strcmp(tr.tag[ti-1],last_update->trainable_tag) == 0) return(1);
  /* if there is a change of the score (eg adding and/or deleting notes)  most of the trained model is still okay.  But
     the trained model doesn't make sense at any place where the new preceding note position is not the same as the old (trained) preceding note position. 
  Here we also won't use the training when the previous notes (in the old and current scores) have different roles (cue, xchg) */
  printf("appears to be change in score --- not using trained model for %s\n",tr.tag[ti]);
  
  return(0);
}



//CF:  Meat to build the BN, into *bn
int
add_composite_graph_xchg(BELIEF_NET *bn, TRAINING_RESULTS tr) {
  int ti,i,num,index,solo,si,mi,cue,type,utype,sightread_cue,accomp_lead=0,axchg,sxchg,tempo_set;
  int hand_set_tempo,tempo_chg_near;
  BNODE *last,*this,*update,*this_catch,*last_catch,*last_update=NULL;
  MATRIX mat,tempo,evol,m,v;
  QUAD_FORM e,z;
  CLIQUE *cl;
  COMPOSITE_LIST compl;
  float length,whole_secs,var;
  RATIONAL rat,r1,r2;
  char string[500];
  


  make_composite_list_xchg(&compl);  //CF:  collect events into composite list
  
  for (i=0; i < compl.num; i++) {    //CF:  for compositite event; build the backbone
    index = compl.list[i].index;
    rat = compl.list[i].pos;
    cue = compl.list[i].cue;
    axchg = compl.list[i].axchg;
    sxchg = compl.list[i].sxchg;
    tempo_set = compl.list[i].tempo_setting;
    hand_set_tempo = is_hand_set_tempo(rat);
    accomp_lead = compl.list[i].accomp_lead;

    wholerat2string(rat,string);
    if (accomp_lead) /*printf("accomp lead %s\n", string)*/;

#ifdef ROMEO_JULIET    
    tempo_chg_near = is_tempo_change_near(i,&compl);
    if (tempo_chg_near) {
	wholerat2string(rat,string);
	//	printf("tempo near at %s\n",string);
    }
#endif
    /*            wholerat2string(rat,string);
		  printf("i = %d accomp_lead = %d place = %s\n",i,accomp_lead,string);*/
    type = (compl.list[i].solo) ? BACKBONE_SOLO_NODE : BACKBONE_MIDI_NODE; /* changes these */
    utype = (compl.list[i].solo) ? BACKBONE_UPDATE_SOLO_NODE : BACKBONE_UPDATE_MIDI_NODE;
    if (i) length = rat2f(rat) -  rat2f(compl.list[i-1].pos);  //CF:  length of note in musical time, as float
    //    printf("i = %d cue = %d\n",i,cue);

    //CF:  real construction starts here:

    //CF:  spec case for cue point
    //CF:    prior on tempo comes form tempo list if untrained; or training data file if trained
    //CF:    because this is a cue point, the starttime is flat, degenerate.
    if (i == 0 || cue) { /*either cue*/                 //type,eg. this event is the start of a phrase
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_PHRASE_START, rat, bn, 0.); //CF:  graph BNODE element

      ti = trained_qf_index(this, tr);       //CF:  look for a training data for this (cue) node
      //CF:  if no training data, get tempo prior direct from score; else used the trained prior from tr
      //CF:  get_whole_secs uses a table of score_positions*tempo_changes
      //      whole_secs = get_whole_secs(rat);
      //      var = whole_secs_var(whole_secs);  // variance for tempo reset as function og mean tempo.
      //      e =  (ti == -1)  ?  QFmeas_size(whole_secs,var)	:  tr.qf[ti];  // changed 1-06  //CF:  make BNODE's potential

      whole_secs = get_whole_secs(rat);
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function of mean tempo.
      //      e =  (ti == -1 || hand_set_tempo)  ?  QFmeas_size(get_whole_secs(rat)/*score.meastime*/,.1) :tr.qf[ti];   

      e =  (ti == -1 || hand_set_tempo)  ?  QFmeas_size(whole_secs,var) : tr.qf[ti];   // commenting out hand_set_tempo 1-10 so that the hand set tempos will *change* with training
      //CF:  make BNODE's potential
      add_backbone_observations(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
	  add_potential(bn,cl); //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
    }  
    //CF:  spec cases for exchanges
    else if (axchg || sxchg || tempo_set) {  /* handing off from solo to accomp or vice-versat */

      //      printf("axchg = %d sxchg = %d\n",axchg,sxchg); 
      z = QFpoint(Mzeros(BACKBONE_DIM,1));
      /*      this  = alloc_belief_node_pot_rat(BACKBONE_DIM, BACKBONE_NODE, rat, bn);*/
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_NODE, rat, bn,length);


      add_backbone_observations(this, rat, bn, accomp_lead);
      update  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_XCHG, rat, bn,length); //CF:only 1D update for tempo only
      //      printf("trainable muffin at %s\n",update->trainable_tag);

      whole_secs = (tempo_set) ? compl.list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function og mean tempo.
      ti = trained_qf_index(update, tr);       
      //      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Msc(Miden(1),.01)) : tr.qf[ti];  // really haven't figured out variance yet.

      //      if (ti == -1 || hand_set_tempo) {  // || hand_set_tempo    added 4-05  overides training  // changed back 1-10
      if (use_trained_update(ti,last,last_update,tr) == 0 || hand_set_tempo) {  //  changed 10-10
	wholerat2string(rat,string);
	printf("tempo fixed to %f at %d/%d %s %s\n",whole_secs,rat.num,rat.den,update->trainable_tag,string);
	m = Mconst(BACKBONE_DIM,1, whole_secs);  m.el[0][0] = 0;
	v = Miden(BACKBONE_DIM); v.el[0][0] = .1; v.el[1][1] = var;  
	// maybe want large variance so this tempo setting will not be influenced by prior, 
	// hence can override

	e = QFmv(m,v);
      }
      else e = tr.qf[ti];   //CF:  trained prior for update node

      cl = make_marginal_potential(update,e); /* the new tempo */
      if ( add_potential(bn,cl) == 0) return(0);
      /* get new tempo from the update node */
      evol = evol_matrix(BACKBONE_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0; 
      /* evol: what previous pos and tempo predict */
      /* new pos taken as deterministic fn of previous tempo and pos --- might want to rethink this */  
      // this seems to be no longer the case
      cl = make_conditional_potential2(last, update, this, evol,Miden(BACKBONE_DIM), z);
      add_potential(bn,cl);
    }
    //CF:  usual case
    else {
      z = QFpoint(Mzeros(SOLO_DIM,1)); //CF:  point distribution on e in y=Ax+e; ie. y is conditionally deterministic
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_NODE, rat, bn,length);  //CF:  construct BNODE

      add_backbone_observations(this, rat, bn, accomp_lead);  //CF: construct BNODEs and potentials for hanging-off obs nodes

      mat = evol_matrix(BACKBONE_DIM,  length);  //CF:  state evolution matrix for this note, [[1 note_length] [0 1]]
      update  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_UPDATE_NODE, rat, bn, length); //CF: construct update node
      ti = trained_qf_index(update, tr);       //CF:  index into training list (-1 if not found)
      whole_secs = get_whole_secs(rat);   //CF:  current inverse tempo
      //      e = (ti == -1) ?  QFpos_meassize_update(.01*length,.05*length) : tr.qf[ti];
      //CF:  if update has training data use it; else use an approximate (but important) standard prior 
#ifdef ROMEO_JULIET    
      e = (ti == -1) ?  get_untrained_update_var(whole_secs,length,rat) : tr.qf[ti];   //CF:  untrained prior for update node
#else
      e = (use_trained_update(ti,last,last_update,tr)) ? tr.qf[ti] : get_untrained_update(whole_secs,length);  // changed 10-10.
      //      e = (ti == -1) ?  get_untrained_update(whole_secs,length) : tr.qf[ti];   //CF:  untrained prior for update node
#endif
      cl = make_marginal_potential(update,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl);   //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
      //CF: x_{n+1}= mat*x_n + Identity*update + z  (where z is always zero in this case)
      cl = make_conditional_potential2(last, update, this, mat, Miden(BACKBONE_DIM), z);  //CF:potential fn for backbone node
      add_potential(bn,cl);  //CF:  store associated (BNODEs,quadnorm) pair in potential list 
    }
    last = this; //CF:  the previously created backbone node
    last_update = update; 
    clear_matrix_buff();
  }
  return(1);
}


//CF:  Meat to build the BN, into *bn
working_add_composite_graph_xchg(BELIEF_NET *bn, TRAINING_RESULTS tr) {  
  /* this worked fine in 3-06 but made minor improvement so that can have agogic accent coming into
     exchange node */
  int ti,i,num,index,solo,si,mi,cue,type,utype,sightread_cue,accomp_lead=0,axchg,sxchg,tempo_set;
  BNODE *last,*this,*update,*this_catch,*last_catch;
  MATRIX mat,tempo,evol,m,v;
  QUAD_FORM e,z;
  CLIQUE *cl;
  COMPOSITE_LIST compl;
  float length,whole_secs,var;
  RATIONAL rat,r1,r2;
  char string[500];


  make_composite_list_xchg(&compl);  //CF:  collect events into composite list
  
  for (i=0; i < compl.num; i++) {    //CF:  for compositite event; build the backbone
    index = compl.list[i].index;
    rat = compl.list[i].pos;
    cue = compl.list[i].cue;
    axchg = compl.list[i].axchg;
    sxchg = compl.list[i].sxchg;
    tempo_set = compl.list[i].tempo_setting;
    accomp_lead = compl.list[i].accomp_lead;
    /*            wholerat2string(rat,string);
		  printf("i = %d accomp_lead = %d place = %s\n",i,accomp_lead,string);*/
    type = (compl.list[i].solo) ? BACKBONE_SOLO_NODE : BACKBONE_MIDI_NODE; /* changes these */
    utype = (compl.list[i].solo) ? BACKBONE_UPDATE_SOLO_NODE : BACKBONE_UPDATE_MIDI_NODE;
    if (i) length = rat2f(rat) -  rat2f(compl.list[i-1].pos);  //CF:  length of note in musical time, as float
    //    printf("i = %d cue = %d\n",i,cue);

    //CF:  real construction starts here:

    //CF:  spec case for cue point
    //CF:    prior on tempo comes form tempo list if untrained; or training data file if trained
    //CF:    because this is a cue point, the starttime is flat, degenerate.
    if (i == 0 || cue) { /*either cue*/                 //type,eg. this event is the start of a phrase
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_PHRASE_START, rat, bn, 0.); //CF:  graph BNODE element
      ti = trained_qf_index(this, tr);       //CF:  look for a training data for this (cue) node
      //CF:  if no training data, get tempo prior direct from score; else used the trained prior from tr
      //CF:  get_whole_secs uses a table of score_positions*tempo_changes
      //      whole_secs = get_whole_secs(rat);
      //      var = whole_secs_var(whole_secs);  // variance for tempo reset as function og mean tempo.


      //      e =  (ti == -1)  ?  QFmeas_size(whole_secs,var)	:  tr.qf[ti];  // changed 1-06  //CF:  make BNODE's potential
      e =  (ti == -1)  ?  QFmeas_size(get_whole_secs(rat)/*score.meastime*/,.1)	:  tr.qf[ti];   //CF:  make BNODE's potential
      add_backbone_observations(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl); //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
    }  
    //CF:  spec cases for exchanges
    else if (axchg || sxchg || tempo_set) {  /* handing off from solo to accomp or vice-versat */
   
      //      printf("axchg = %d sxchg = %d\n",axchg,sxchg); 
      //                  printf("here tag = %s\n",this->trainable_tag);
      //            if (strcmp(this->trainable_tag,"bbone_xchg_400+0/1")) exit(0);


      z = QFpoint(Mzeros(BACKBONE_DIM,1));
      /*      this  = alloc_belief_node_pot_rat(BACKBONE_DIM, BACKBONE_NODE, rat, bn);*/
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_NODE, rat, bn,length);
      add_backbone_observations(this, rat, bn, accomp_lead);

#ifdef IMP
      update  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_XCHG, rat, bn,length); //CF:only 1D update for tempo only
#else 
      update  = alloc_belief_node_composite(1 /* dim = 1 */, BACKBONE_XCHG, rat, bn,length); //CF:only 1D update for tempo only
#endif

      //      printf("trainable muffin at %s\n",update->trainable_tag);

      whole_secs = (tempo_set) ? compl.list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      var = whole_secs_var(whole_secs);  // variance for tempo reset as function og mean tempo.
      ti = trained_qf_index(update, tr);       
      //      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Msc(Miden(1),.01)) : tr.qf[ti];  // really haven't figured out variance yet.

      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Msc(Miden(1),var)) : tr.qf[ti];  // really haven't figured out variance yet.
#ifdef IMP
      if (ti == -1) {
	m = Mconst(BACKBONE_DIM,1, whole_secs);  m.el[0][0] = 0;
	v = Miden(BACKBONE_DIM); v.el[0][0] = .1; v.el[1][1] = var;
	e = QFmv(m,v);
      }
      else e = tr.qf[ti];   //CF:  trained prior for update node
#endif
      cl = make_marginal_potential(update,e); /* the new tempo */
      add_potential(bn,cl);
      mat = Mzeros(BACKBONE_DIM,1); mat.el[1][0] = 1;
      /* get new tempo from the update node */
      evol = evol_matrix(BACKBONE_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0; 
      /* evol: what previous pos and tempo predict */
      /* new pos taken as deterministic fn of previous tempo and pos --- might want to rethink this */ 
#ifdef IMP
      cl = make_conditional_potential2(last, update, this, evol,Miden(BACKBONE_DIM), z);
#else
      cl = make_conditional_potential2(last, update, this, evol,mat, z);
#endif
      add_potential(bn,cl);
    }
    //CF:  usual case
    else {
      z = QFpoint(Mzeros(SOLO_DIM,1)); //CF:  point distribution on e in y=Ax+e; ie. y is conditionally deterministic
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_NODE, rat, bn,length);  //CF:  construct BNODE

      add_backbone_observations(this, rat, bn, accomp_lead);  //CF: construct BNODEs and potentials for hanging-off obs nodes

      mat = evol_matrix(BACKBONE_DIM,  length);  //CF:  state evolution matrix for this note, [[1 note_length] [0 1]]
      update  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_UPDATE_NODE, rat, bn, length); //CF: construct update node
      ti = trained_qf_index(update, tr);       //CF:  index into training list (-1 if not found)
      whole_secs = get_whole_secs(rat);   //CF:  current inverse tempo
      //      e = (ti == -1) ?  QFpos_meassize_update(.01*length,.05*length) : tr.qf[ti];
      //CF:  if update has training data use it; else use an approximate (but important) standard prior 
      e = (ti == -1) ?  get_untrained_update(whole_secs,length) : tr.qf[ti];   //CF:  untrained prior for update node
      cl = make_marginal_potential(update,e);  //CF:  associate the potnetial with the BNODE (it's not stored IN the BNODE)
      add_potential(bn,cl);   //CF:  add the mini-clique to the POTENTIAL_LIST in BELIEF_NET bn.
      //CF: x_{n+1}= mat*x_n + Identity*update + z  (where z is always zero in this case)
      cl = make_conditional_potential2(last, update, this, mat, Miden(BACKBONE_DIM), z);  //CF:potential fn for backbone node
      add_potential(bn,cl);  //CF:  store associated (BNODEs,quadnorm) pair in potential list 
    }
    last = this; //CF:  the previously created backbone node
    clear_matrix_buff();
  }
}







stable_add_composite_graph_xchg(BELIEF_NET *bn, TRAINING_RESULTS tr) {
  int ti,i,num,index,solo,si,mi,cue,type,utype,sightread_cue,accomp_lead=0,axchg,sxchg,tempo_set;
  BNODE *last,*this,*update,*this_catch,*last_catch;
  MATRIX mat,tempo,evol,m,v;
  QUAD_FORM e,z;
  CLIQUE *cl;
  COMPOSITE_LIST compl;
  float length,whole_secs;
  RATIONAL rat,r1,r2;
  char string[500];
  

  make_composite_list_xchg(&compl);
  for (i=0; i < compl.num; i++) {
    index = compl.list[i].index;
    rat = compl.list[i].pos;
    cue = compl.list[i].cue;
    axchg = compl.list[i].axchg;
    sxchg = compl.list[i].sxchg;
    tempo_set = compl.list[i].tempo_setting;
    accomp_lead = compl.list[i].accomp_lead;
    /*            wholerat2string(rat,string);
		  printf("i = %d accomp_lead = %d place = %s\n",i,accomp_lead,string);*/
    type = (compl.list[i].solo) ? BACKBONE_SOLO_NODE : BACKBONE_MIDI_NODE; /* changes these */
    utype = (compl.list[i].solo) ? BACKBONE_UPDATE_SOLO_NODE : BACKBONE_UPDATE_MIDI_NODE;
    if (i) length = rat2f(rat) -  rat2f(compl.list[i-1].pos);

    printf("i = %d cue = %d\n",i,cue);


    if (i == 0 || cue) { /*either cue*/
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_PHRASE_START, rat, bn, 0.);
      //      printf("tag = %s\n",this->trainable_tag); exit(0);

      //      ti = trained_qf_index(this, tr);       
      //      e =  (ti == -1)  ?  QFmeas_size(get_whole_secs(rat)/*score.meastime*/,.1)	:  tr.qf[ti];


      if (tr.num == 0 || this->trainable_tag == NULL) 
	e = /*prior_cue_dist(); */ QFmeas_size(get_whole_secs(rat)/*score.meastime*/,.1); 
      else  e = extract_trained_qf(this->trainable_tag, tr);        

      add_backbone_observations(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }


    else if (axchg || sxchg || tempo_set) {  /* handing off from solo to accomp or vice-versat */

      //      printf("axchg = %d sxchg = %d\n",axchg,sxchg); 
      z = QFpoint(Mzeros(BACKBONE_DIM,1));
      /*      this  = alloc_belief_node_pot_rat(BACKBONE_DIM, BACKBONE_NODE, rat, bn);*/
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_NODE, rat, bn,length);

      add_backbone_observations(this, rat, bn, accomp_lead);
      //  this was here catch    add_backbone_observations(this, rat, bn, accomp_lead);
      /*  update  = alloc_belief_node_pot_rat(1 , BACKBONE_UPDATE_NODE, rat, bn);*/


      update  = alloc_belief_node_composite(1 /* dim = 1 */, BACKBONE_XCHG, rat, bn,length);

      

      //      whole_secs = (tempo_set) ? compl.list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      //      ti = trained_qf_index(update, tr);       
      //      e = (ti == -1) ?  QFmv(Mconst(1,1, whole_secs),Miden(1)) : tr.qf[ti];


      if (tr.num == 0 || update->trainable == 0) {
      	whole_secs = (tempo_set) ? compl.list[i].whole_secs : get_whole_secs(rat); //score.meastime;
      	e = /*prior_xchg_dist();*/  QFmv(Mconst(1,1,/*score.meastime*/ whole_secs),Miden(1));
	/*	#ifndef CLARINET_EXPERIMENT
	      	e =  QFmv(Mconst(1,1,whole_secs),Mzeros(1,1));
	#endif
	printf("need to resolve this issue: first line is the established way\n");*/
      }
      /*      update  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_XCHG, rat, bn,length);
      if (tr.num == 0)  {
	m = Mzeros(2,1); m.el[1][0] = score.meastime;
	e = QFmv(m,Miden(BACKBONE_DIM));      
	}*/
      else e = extract_trained_qf(update->trainable_tag, tr);    


      //      printf("axchg = %d sxsgh = %d tempos_et = %d\n",axchg,sxchg,tempo_set);
      //           QFprint(e);
      cl = make_marginal_potential(update,e); /* the new tempo */
      add_potential(bn,cl);

      mat = Mzeros(BACKBONE_DIM,1); mat.el[1][0] = 1;
      /* get new tempo from the update node */

      // mat = Miden(BACKBONE_DIM);



      evol = evol_matrix(BACKBONE_DIM,  length); evol.el[1][0]= evol.el[1][1] = 0; 
      /* evol: what previous pos and tempo predict */
      cl = make_conditional_potential2(last, update, this, evol,mat, z);
      add_potential(bn,cl);
    }

    else {
      z = QFpoint(Mzeros(SOLO_DIM,1));
      /*      e = QFtempo_change(.01*length); /* best yet with training */
      /*            e = QFtempo_change_pos_slop_parms(length*.5, length*1.);   */
      /*      e = QFtempo_change_pos_slop_parms(length*.0005, length*.05);   */
      /*      this  = alloc_belief_node_pot_rat(BACKBONE_DIM, BACKBONE_NODE, rat, bn);*/
      this  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_NODE, rat, bn,length);
      add_backbone_observations(this, rat, bn, accomp_lead);
      // this was here catch      add_backbone_observations(this, rat, bn, accomp_lead);
      mat = evol_matrix(BACKBONE_DIM,  length);
      /* update  = alloc_belief_node_pot_rat(BACKBONE_DIM, BACKBONE_UPDATE_NODE, rat, bn);*/
      update  = alloc_belief_node_composite(BACKBONE_DIM, BACKBONE_UPDATE_NODE, rat, bn, length);
      //      if (tr.num == 0) {


      //      if (this->trainable_tag != NULL) printf("%s %s\n",this->observable_tag,this->trainable_tag);

      //      printf("i = %d trainable = %d rat = %d/%d index = %d %s %s \n",i,update->trainable,rat.num,rat.den,index,this->trainable_tag,this->observable_tag);

      //      ti = trained_qf_index(update->trainable_tag, tr);       
      if (tr.num == 0 || /*this*/update->trainable_tag == NULL) {
	//			e = QFtempo_change_pos_slop(length);   /* for schumann 1 */
	/*		e = QFtempo_change_pos_slop_parms(.001*length,.0001*length);   /* worked okay ... nick */
	/*	e = QFtempo_change_pos_slop_parms(.001*length,.001*length);   /* this is what was used n fugue recording  and current 1st movment */
	//		e  = QFtempo_change_pos_slop_parms(0.,.001*length);   /* flight of bumblebee */
	//			e  = QFtempo_change_pos_slop_parms(.01*length,.001*length);   /* marcello */
	//	e  = QFtempo_change_pos_slop_parms(.1*length,.01*length);   /* poulenc and current romance1! */
	//	e  = QFtempo_change_pos_slop_parms(.5*length,.001);   /* mozart quartet */
	//			e  = QFtempo_change_pos_slop_parms(1000.*length,0.);   /* mozart quartet */
	//	e  = QFpos_meassize_update(.01*length,.001*length);   /* brahms vc */
	//	e  = /*prior_update_dist(length); */  QFpos_meassize_update(.01*length,.01*length); 
#ifdef VIOLIN_EXPERIMENT	
	e  = /*prior_update_dist(length); */  QFpos_meassize_update(.01*length,.01*length); 
#else
	e  = /*prior_update_dist(length); */  QFpos_meassize_update(.01*length,.05*length); /* mozart 4tet */
	if (strcmp("schubert_violin_son_mvmt1",scorename) == 0) 
	  e  = QFpos_meassize_update(.001*length,.001*length); 
	//		if (Mnorm(e.m) > 0.) exit(0);
#endif

	//				e  = QFtempo_change_pos_slop_parms(.05*.05,1.*length);   /* ravel sonatine */
	//				e  = QFtempo_change_pos_slop_parms(.05*.05,1.*length);   /* testing ravel sonatine */
	//		e  = QFtempo_change_pos_slop_parms(.05*.05,.0001*length);   /* schumann horn */
	//		e  = QFtempo_change_pos_slop_parms(.05*.05,1.*length);   /* schumann horn */
		/*	e = QFtempo_change_pos_slop_parms(100.*length,100.*length);   /* worked badly ... nick */
	/*	if (rat.den == 1) e = QFtempo_change_pos_slop(.1*length);    */

	/*			e = QFpos_slop(length);    /* had this for nick 2-5-02
	/*	if (rat.den == 1) e = QFtempo_change_pos_slop(.1*length);   
		else e = QFpos_slop(length);   */
      }
      else  e = extract_trained_qf(update->trainable_tag, tr);     
      //      else e = tr.qf[ti];

      //      if (Mnorm(e.m) > 0.) { Mp(e.m); exit(0); }

      cl = make_marginal_potential(update,e);
      add_potential(bn,cl);


      cl = make_conditional_potential2(last, update, this, mat, Miden(BACKBONE_DIM), z);
      add_potential(bn,cl);
    }
    last = this;
    clear_matrix_buff();
  }
  /*  rat.num = 1; rat.den = 4;
  fix_tempo("139",rat,100., bn);
  fix_tempo("142",rat,103., bn);
  fix_tempo("145",rat,108., bn);
  fix_tempo("147+1/4",rat,113., bn);
  fix_tempo("150",rat,120., bn);
  fix_tempo("152+1/4",rat,127., bn);
  fix_tempo("155",rat,134., bn);
  fix_tempo("157+1/4",rat,141., bn);*/
}




add_composite_graph_atempo(BELIEF_NET *bn, TRAINING_RESULTS tr) {
  int i,num,index,solo,si,mi,cue,type,utype,sightread_cue,accomp_lead=0,axchg,sxchg,dim;
  BNODE *last,*this,*update,*atempo;
  MATRIX mat,tempo,evol;
  QUAD_FORM e,z;
  CLIQUE *cl;
  COMPOSITE_LIST compl;
  float length;
  RATIONAL rat,r1,r2;
  

  make_composite_list_xchg(&compl);
  for (i=0; i < compl.num; i++) {
    index = compl.list[i].index;
    rat = compl.list[i].pos;
    cue = compl.list[i].cue;
    axchg = compl.list[i].axchg;
    sxchg = compl.list[i].sxchg;
    accomp_lead = compl.list[i].accomp_lead;
    type = (compl.list[i].solo) ? BACKBONE_SOLO_NODE : BACKBONE_MIDI_NODE; /* changes these */
    utype = (compl.list[i].solo) ? BACKBONE_UPDATE_SOLO_NODE : BACKBONE_UPDATE_MIDI_NODE;
    if (i) length = rat2f(rat) -  rat2f(compl.list[i-1].pos);


    if (i == 0 || cue) {

      this  = alloc_belief_node_composite(BACKBONE_ATEMPO_DIM,  BACKBONE_PHRASE_START, rat, bn, 0.);
      if (tr.num == 0)  e = QFmeas_size_at(score.meastime,1.);  
      else  e = extract_trained_qf(this->trainable_tag, tr);       
      add_backbone_observations_at(this, rat, bn, accomp_lead);
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }

    else if (axchg || sxchg) {  /* handing off from solo to accomp or vice-versat */
      z = QFpoint(Mzeros(BACKBONE_ATEMPO_DIM,1));
      this  = alloc_belief_node_composite(BACKBONE_ATEMPO_DIM, BACKBONE_NODE, rat, bn,length);
      add_backbone_observations_at(this, rat, bn, accomp_lead);
      update  = alloc_belief_node_composite(dim = 2, BACKBONE_UPDATE_NODE, rat, bn,length);
      if (tr.num == 0)  e = QFmv(Mconst(2,1,score.meastime),Miden(2)); /* maybe smaller variance ? */
      else e = extract_trained_qf(update->trainable_tag, tr);    
      cl = make_marginal_potential(update,e); /* the new tempo(s!) */
      add_potential(bn,cl);
      evol = evol_matrix_at(length); evol.el[1][1]= evol.el[1][2] = evol.el[2][2] = 0;
      /* first component of "this" is 1st row of evol matrix applied to last */
      mat = Mzeros(BACKBONE_ATEMPO_DIM,BACKBONE_UPDATE_DIM); mat.el[1][0] = mat.el[2][1] = 1;
      /* 2nd two components of "this" are the two components of update */
      cl = make_conditional_potential2(last, update, this, evol,mat, z);
      add_potential(bn,cl);
    }
    else {
      z = QFpoint(Mzeros(BACKBONE_ATEMPO_DIM,1));
      this  = alloc_belief_node_composite(BACKBONE_ATEMPO_DIM, BACKBONE_NODE, rat, bn,length);
      add_backbone_observations_at(this, rat, bn, accomp_lead);
      evol = evol_matrix_at(length);
      mat = Mzeros(BACKBONE_ATEMPO_DIM,BACKBONE_UPDATE_DIM); mat.el[0][0] = mat.el[1][1] = 1;
      update  = alloc_belief_node_composite(BACKBONE_UPDATE_DIM, BACKBONE_UPDATE_NODE, rat, bn, length);
      //       if (tr.num == 0) 	e  = QFtempo_change_pos_slop_parms(.01*length,.001*length);   /* flight of bumblebee */
       if (tr.num == 0) 	e  = QFtempo_change_pos_slop_parms(100.*length,10.*length);   /* flight of bumblebee */
      else  e = extract_trained_qf(update->trainable_tag, tr);     
      cl = make_marginal_potential(update,e);
      add_potential(bn,cl);
      cl = make_conditional_potential2(last, update, this, evol, mat, z);
      add_potential(bn,cl);
    }
    last = this;
    clear_matrix_buff();
  }
}



static void
get_pos_in_meas(char *s1, char *s2) {
  char *s;

  while (*s1 != '+') s1++;
  s1++;
  strcpy(s2,s1);
}


static void
add_accomp_train_graph(BELIEF_NET *bn) {
  int i,strong_beat;
  BNODE *last,*this,*update;
  MATRIX mat,tempo;
  QUAD_FORM e,z;
  CLIQUE *cl;
  float length;
  char meas_pos[500];


  init_belief_net(bn);
  for (i=0; i < score.midi.num; i++) {
    get_pos_in_meas(score.midi.burst[i].observable_tag, meas_pos);
    strong_beat =  (strcmp(meas_pos,"0/1") == 0 || strcmp(meas_pos,"1/2") == 0 || strcmp(meas_pos,"1/4") == 0 || strcmp(meas_pos,"3/4") == 0);
    /*    if (!strong_beat) printf("%s\n",score.midi.burst[i].observable_tag); */

    if (i > 0 && score.midi.burst[i].acue == 0)  {
      length = score.midi.burst[i-1].length;
      z = QFpoint(Mzeros(ACCOM_DIM,1));     /* contribution from prev note is deterministic */
      /*      e = (strong_beat) ? QFtempo_change_pos_slop_parms(length*.5, length*1.) : z;     /* the trainable increment */
      e = QFtempo_change_pos_slop_parms(length*.5, length*1.);     /* the trainable increment */
      this  = alloc_accomp_train_node_pot(ACCOM_DIM,ACCOM_NODE, i, bn);
      mat = evol_matrix(ACCOM_DIM,  length);
      update  = alloc_belief_node_pot(ACCOM_DIM, ACCOM_UPDATE_NODE, i, bn);
      cl = make_marginal_potential(update,e);
      add_potential(bn,cl);
      cl = make_conditional_potential2(last, update, this, mat, Miden(ACCOM_DIM), z);
      add_potential(bn,cl);
    }
    else {
      e = QFmeas_size2(score.meastime,1.);  /* assumes dim of accomp note is 2 */
      this  = alloc_accomp_train_node_pot(ACCOM_DIM,  ACCOM_PHRASE_START, i, bn);
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }
    last = this;
    clear_matrix_buff();
  }
}


static BNODE*
boundary_node(int i) {  /* index of solo node */
  BNODE *n,*nn;
  int j;

  n = score.solo.note[i].hidden;
  for (j=0; j < n->neigh.num; j++) {
    nn = n->neigh.arc[j];
    if (nn->note_type == ANCHOR_NODE  || nn->note_type == PHANTOM_NODE) break;
  }
  if (j == n->neigh.num)  return(0);
  return(nn);
}

static int
same_component(BNODE *n1, BNODE *n2) { /* lots of hidden assumps */
  int i;
  BNODE *next;

  while (n1->meas_time < n2->meas_time) {
    if (is_connected(n1,n2)) return(1);
    for (i=0; i < n1->neigh.num; i++) {
      next = n1->neigh.arc[i];
      if (next->note_type != ANCHOR_NODE && next->note_type != PHANTOM_NODE &&
	  next->note_type != INTERIOR_NODE) continue;
      if (next->meas_time > n1->meas_time) break;
    }
    if (i == n1->neigh.num) return(0);
    n1 = n1->neigh.arc[i];
  }
  return(0);
}

add_accomp_graph(BELIEF_NET *bn) {
  int lp[MAX_ACCOMP_NOTES];
  int rp[MAX_ACCOMP_NOTES];
  int cuepoint[MAX_ACCOMP_NOTES];
  int i,type,coincides,no_connect,left_phantom,rite_phantom,solo_next,connect,j;
  QUAD_FORM e;
  BNODE *last,*this,*update,*phant,*left,*rite;
  MATRIX mat;
  CLIQUE *cl;
  float gap;

  for (i=0; i < score.midi.num; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,0,score.solo.num-1);
    lp[i] = left_parent(score.midi.burst[i].time,0,score.solo.num-1);
  }
  for (i=0; i < score.midi.num; i++) {
    coincides =  (rp[i] == lp[i]); 
    type = (coincides) ? ANCHOR_NODE : INTERIOR_NODE;
    if (lp[i] == PARENT_UNSET || rp[i-1] == PARENT_UNSET)
      no_connect = rite_phantom = left_phantom = 0;
    else {
      no_connect = ((i == 0)  || 
		  ((lp[i] == rp[i-1]) && (lp[i-1] != rp[i-1]) &&  (score.solo.note[lp[i]].cue)) ||
		  ((rp[i] == lp[i])  && (rp[i-1] == lp[i-1]))   ||
		    ((rp[i-1] != lp[i-1]) && (score.solo.note[rp[i-1]].cue)) );
      left_phantom = ((i > 0) && (score.solo.note[lp[i]].time - .001 > score.midi.burst[i-1].time)
			&& (rp[i] != lp[i]) && (score.midi.burst[i].acue == 0));
      rite_phantom = ((i > 0) && (score.solo.note[rp[i-1]].time + .001 < score.midi.burst[i].time)
		      && (rp[i-1] != lp[i-1])  && (score.solo.note[rp[i-1]].cue == 0));
      connect = ((i > 0) &&  
		  ( (lp[i] == lp[i-1])    || 
		       ((rp[i] == rp[i-1]) && (score.solo.note[rp[i]].cue == 0)))); 
      no_connect = !connect;
    }
    this  = alloc_accomp_node_pot(ACCOM_DIM, type,i, bn);
    if (coincides) {
      if (cuepoint[i]) e = QFpos_sim();
      else e = QFpos_tempo_sim();
      if (cuepoint[i]) printf("cue point at %d\n",i);
      cl = make_conditional_potential(score.solo.note[lp[i]].hidden,this, Miden(SOLO_DIM), e);
      add_potential(bn,cl);
    }
    if (rite_phantom) {
      phant  = alloc_belief_node_pot(ACCOM_DIM, PHANTOM_NODE,rp[i-1], bn);
      e = QFpos_tempo_sim();
      cl = make_conditional_potential(score.solo.note[rp[i-1]].hidden,phant, Miden(SOLO_DIM), e);
      add_potential(bn,cl); 
      gap =  score.solo.note[rp[i-1]].time - score.midi.burst[i-1].time;
      mat = evol_matrix(ACCOM_DIM,  gap);
      e = QFpos_tempo_sim();
      cl = make_conditional_potential(last ,phant, mat , e);
      add_potential(bn,cl); 
    }
    if (left_phantom) {  /* could already be there */
      phant = boundary_node(lp[i]);
      if (phant == 0) phant  = alloc_belief_node_pot(ACCOM_DIM, PHANTOM_NODE,lp[i], bn);
      e = QFpos_tempo_sim();
      cl = make_conditional_potential(score.solo.note[lp[i]].hidden,phant, Miden(SOLO_DIM), e);
      add_potential(bn,cl); 
      gap =  score.midi.burst[i].time - score.solo.note[lp[i]].time;
      mat = evol_matrix(ACCOM_DIM,  gap);
      e = QFpos_tempo_sim();
      cl = make_conditional_potential(phant, this, mat , e);
      add_potential(bn,cl); 
    }
    if (!no_connect && !rite_phantom && !left_phantom)  {
      mat = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
      e = score.midi.burst[i].qf_update; 
      cl = make_conditional_potential(last, this, mat, e);
      add_potential(bn,cl);
    } 
    if (score.midi.burst[i].acue) {
      e  = QFatempo(score.meastime);
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }      
    last = this;
    clear_matrix_buff();
  }
  for (i=0; i < score.midi.num; i++) {
    this = score.midi.burst[i].hidden;
    if (this->note_type != INTERIOR_NODE) continue;
    if (rp[i] == PARENT_UNSET) continue;
    if (lp[i] == PARENT_UNSET) continue;
    if (score.solo.note[rp[i]].cue) continue;
    left = boundary_node(lp[i]);
    if (left == 0) continue;
    rite = boundary_node(rp[i]);
    /*    if (rite == 0) continue; */
    /*    printf("left = %f rite = %f\n",left->meas_time,rite->meas_time); */
    if (is_connected(left,rite)) continue;
    if (same_component(left,rite) == 0) continue;
    add_undir_arc(left,rite);
    /*    mat = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
    cl = make_null_potential(left,rite);
    add_potential(bn,cl); */
  }
}


/*
static int
belief_compare(void *i, void *j) {
  BNODE  *ii,*jj;

  ii = (BNODE *) i;
  jj = (BNODE *) j;
  if (ii->note_type == ANCHOR_NODE || ii->note_type == PHANTOM_NODE) {
    if (jj->note_type == ANCHOR_NODE || jj->note_type == PHANTOM_NODE) return(jj->meas_time < ii->meas_time);
    else return(1);
  }
  else {
    if (jj->note_type == ANCHOR_NODE || jj->note_type == PHANTOM_NODE) return(-1);
    else return(0);
  }
}
*/

static void
foss(BNODE *cur, BELIEF_NET *bn, CLIQUE **cl) {
  int i;

  
  if (cur->mark) return;
  if (cur->note_type != ANCHOR_NODE && cur->note_type != PHANTOM_NODE &&
      cur->note_type != INTERIOR_NODE && cur->note_type) return;

  if (cur->note_type == ANCHOR_NODE || cur->note_type == PHANTOM_NODE)
    add_to_clique(*cl,cur);
  cur->focus = 1;
  if (cur->note_type == INTERIOR_NODE) {
    cur->mark = 1;
    for (i=0; i < cur->neigh.num; i++) foss(cur->neigh.arc[i],bn,cl);
  }
}

static void
focus_on_sub_structure(BNODE *start, BELIEF_NET *bn, CLIQUE **cl) {
  int i;
  
  *cl = alloc_clique();
  for (i=0; i < bn->num; i++) bn->list[i]->focus = 0;
  foss(start,bn,cl);
  /*        for (i=0; i < bn->num; i++) if(bn->list[i]->focus) printf("%d at %f\n",bn->list[i]->note_type,bn->list[i]->meas_time);
      exit(0);  */
}








#ifdef XXXX
xxxmake_indep_graph() {  /* graph for network in which accomp and solo are indep */
  int firstnote,lastnote,i,firsta,lasta,coincides,type;
  int lpx[MAX_NOTES_PER_PHRASE],*lp;
  int rpx[MAX_NOTES_PER_PHRASE],*rp;
  BNODE *solox[MAX_NOTES_PER_PHRASE],**solo,*first_accomp,*last;
  BNODE *accox[MAX_NOTES_PER_PHRASE],**acco;
  MATRIX mat;
  BELIEF_NET bn;
  QUAD_FORM e,prior;
  float len,gap;

  init_belief_net(&bn);
  firstnote = 0; 
  lastnote = score.solo.num-1;
  firsta = 0;
  lasta = score.midi.num-1;
  if (score.solo.num > MAX_NOTES_PER_PHRASE) {
    printf("too many solo notes in score\n");
    exit(0);
  }
  if (score.midi.num > MAX_NOTES_PER_PHRASE) {
    printf("too many accomp notes in score\n");
    exit(0);
  }
  lp = lpx;
  rp = rpx;
  solo = solox;
  acco = accox;
  for (i=0; i < score.solo.num; i++) {
    e = score.solo.note[i].qf_update; /* both first and others */
    if (score.solo.note[i].cue)  {
      prior = QFmake_pos_unif(e);
      solo[i] = exper_alloc_solo_node(SOLO_DIM, prior,SOLO_NODE, i, &bn);
    }
    else if (0/*long_rest(i-1)*/) {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
    }
    else {
      solo[i] = exper_alloc_solo_node(SOLO_DIM, QFunif(SOLO_DIM),SOLO_NODE, i, &bn);
      mat = evol_matrix(SOLO_DIM,  score.solo.note[i-1].length);
      add_dir_arc_pot(solo[i-1],solo[i],mat,e);
    }
  }
  for (i=0; i < score.midi.num; i++) {
    rp[i] = right_parent(score.midi.burst[i].time,firstnote,lastnote);
    lp[i] = left_parent(score.midi.burst[i].time,firstnote,lastnote);
    coincides =  (rp[i] == lp[i]); 
    e = score.midi.burst[i].qf_update; /* both first and others */
    prior = score.midi.burst[i].prior; 
    /*    if (i == firsta) e = QFmake_pos_unif(e); */
    type = (coincides) ? ANCHOR_NODE : INTERIOR_NODE;
    
    acco[i] = exper_alloc_accomp_node(ACCOM_DIM, prior/*QFunif(ACCOM_DIM)*/, type,i, &bn);
    if (i > firsta && score.midi.burst[i].connect)  {
      mat = evol_matrix(ACCOM_DIM,  score.midi.burst[i-1].length);
      add_dir_arc_pot(acco[i-1],acco[i],mat,e);
    }
    if (vertical_connect(i,lp[i],rp[i])) {
	  /*    if (coincides ) {*/
      mat = Mzeros(ACCOM_DIM,  SOLO_DIM);
      mat.el[0][0] = 1;
      e  = QFpos_near(ACCOM_DIM);
      add_dir_arc_pot(solo[rp[i]],acco[i],mat,e);
    }
    if (sandwich(i,lp[i],rp[i])) {
         printf("sandwich at %f\n",score.midi.burst[i].time);
      gap = score.midi.burst[i].time - score.solo.note[lp[i]].time;
      mat = evol_matrix_pred_pos(gap);
      e = QFpos_near(ACCOM_DIM);
            e = QFpos_var(ACCOM_DIM, gap*5.);
      add_dir_arc_pot(solo[lp[i]],acco[i],mat,e);
      }
    if (right_sandwich(i,lp[i],rp[i])) {
      /*      printf("right sandwich at %f\n",score.midi.burst[i].time);*/
      gap =  score.solo.note[rp[i]].time - score.midi.burst[i].time;
      mat = evol_matrix_pred_pos2(gap);
      /*      e = QFpos_near(SOLO_DIM);*/
      e = QFpos_var(SOLO_DIM, gap*5.);
      /*      QFpos_var(gap*5.);*/
      /*      printf("here is e1\n");
      QFprint(e);
      printf("here is e2\n");
      QFprint(QFpos_var(gap*5.));*/

      /*      printf("time = %f\n",score.solo.note[rp[i]].time); */
	      
      if (strcmp("habanera",scorename) != 0)  
	add_dir_arc_pot(acco[i],solo[rp[i]],mat,e);
      else if  (fabs(score.solo.note[rp[i]].time-28.75) > .001)
	add_dir_arc_pot(acco[i],solo[rp[i]],mat,e);
      }
  }
  return(bn);
}

#endif












static QUAD_FORM
zero_dist(CLIQUE *c) {  /* this is allocated to permanent space */
  SUPERMAT m;
  SUPERMAT S;
  int i,j,di,dj,dim=0;
  QUAD_FORM temp,z;

  /* xxx fix this.  why am i allocating to permanent sapce? */
  for (i=0; i < c->num; i++) 
    dim += c->member[i]->dim;
  z = QFinit(dim);
  /* temp.m = Mzeros(dim,1); */
  /*  temp.U = Miden(dim);
      /*  temp.D = Mzeros(dim,dim);*/
  /* temp.D = Mdiag_invert(temp.D); */
  /*  temp.S = Mzeros(dim,dim);
  temp.cov = Mzeros(dim,dim);
  temp.fin = Mzeros(dim,0);
  temp.inf = Miden(dim);
  temp.null = Mzeros(dim,0); */
  /*  temp.minf = Mzeros(dim,1); */
  QFcopy(QFunif(dim), &z);
  return(z);
}

static QUAD_FORM
new_zero_dist(CLIQUE *c) {  /* this is allocated to permanent space */
  SUPERMAT m;
  SUPERMAT S;
  int i,j,di,dj,dim=0;
  QUAD_FORM temp,z;

  for (i=0; i < c->num; i++) 
    dim += c->member[i]->dim;
  if (dim == 0) {
    printf("assdfasdf\n");
    exit(0);
  }
  return(QFunif(dim));  //CF:  uniform (flat) distro
}

  


static SUPER_QUAD 
old_zero_dist(CLIQUE *c) { 
  int i,j,di,dj;
  SUPER_QUAD temp;
  SUPERMAT m;
  SUPERMAT S;
  
  m = Salloc(c->num,1);
  S = Salloc(c->num,c->num);
  for (i=0; i < c->num; i++) {
    di = c->member[i]->dim;
    m.sub[i][0] = Mzeros(di,1);
    for (j=0; j < c->num; j++) {
      dj = c->member[j]->dim;
      S.sub[i][j] = Mzeros(di,dj);
    }
  }
  temp = SQset(m, S, 0.);
  return(temp);
}
    
static SUPERMAT
zero_clique_var(CLIQUE *c) { 
  int i,j,di,dj;
  SUPER_QUAD temp;
  SUPERMAT m;
  SUPERMAT S;
  
  S = Salloc(c->num,c->num);
  for (i=0; i < c->num; i++) {
    di = c->member[i]->dim;
    for (j=0; j < c->num; j++) {
      dj = c->member[j]->dim;
      S.sub[i][j] = Mzeros(di,dj);
    }
  }
  return(S);
}
    
    
static CLIQUE*
running_intersect(CLIQUE *clique, BNODE *node)  /* should take clique * */
{
  int i;
  CLIQUE *temp;
  int found = 0;


  temp = alloc_clique();
  
  for (i=0; i < clique->num; i++) {
    if (clique->member[i] != node)  add_to_clique(temp,clique->member[i]);
    else found = 1;
  }
  if (found == 0)
    printf("couldn't find clique member\n");
  return(temp);
}



int
connect_clique_nodes(CLIQUE_NODE *n1, CLIQUE_NODE *n2, CLIQUE *intersect)
{
  int i;

  if (n1->neigh.num == MAX_CLIQUE_NEIGH) {
    printf("cannot add clique neighbor\n");
    return(0);
  }
  i = (n1->neigh.num)++;
  n1->neigh.bond[i].ptr = n2;
  n1->neigh.bond[i].intersect = intersect;
  n1->neigh.bond[i].flow_tab.num = 0; 


  if (n2->neigh.num == MAX_CLIQUE_NEIGH) {
    printf("cannot add clique neighbor\n");
    return(0);
  }
  i = (n2->neigh.num)++;
  n2->neigh.bond[i].ptr = n1;
  n2->neigh.bond[i].intersect = intersect;
  n2->neigh.bond[i].flow_tab.num = 0; 
}

  
  
static void
print_clique(CLIQUE *c)
{
  int i;
  
  printf("printing clique\n"); 
  for (i=0; i < c->num; i++) {
    printf("member = %d mcs_num = %d dim = %d meas = %f type = %d\n",
	   c->member[i]->node_num,c->member[i]->mcs_num,c->member[i]->dim,c->member[i]->meas_time,c->member[i]->note_type);
  }
  printf("\n");
}
  

static CLIQUE_TREE
build_clique_tree(BNODE *node)
{
  CLIQUE_NODE *cur,**clique_node;
  CLIQUE *intersect;
  int i,j,low_clique;
  CLIQUE_TREE ct;

  perfect = (BNODE **) malloc(sizeof(BNODE *) * bel_num_nodes);
  clique_node = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * bel_num_nodes);
  visit_undir_graph(node,perfect_func);
  
  /* build first clique */
  cur =  alloc_clique_node();
  build_add_to_clique(cur->clique,perfect[0]);
  for (i=1; i < bel_num_nodes; i++) {
    if (is_connected_to_all(perfect[i],cur->clique)) 
      build_add_to_clique(cur->clique,perfect[i]);
    else break;
  }
  low_clique = i-1;

  ct.num = bel_num_nodes-low_clique;
  ct.list = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * ct.num);
  ct.root = ct.list[0] = cur;
  

  cur->clique_id = low_clique;
  clique_node[low_clique] = cur;
  /*printf("low_clique = %d\n",low_clique);*/
  for (i = low_clique+1; i < bel_num_nodes; i++) {
    ct.list[i-low_clique] = clique_node[i] = build_clique_from_top(i);
    intersect = running_intersect(clique_node[i]->clique,perfect[i]);
    /*printf("i = %d\n",i);
print_clique(clique_node[i]->clique);
print_clique(intersect); */
    for (j=i-1; j >= low_clique; j--) {
      /*printf("j = %d\n",j);
print_clique(clique_node[j]->clique); */
      if (is_subset(intersect, clique_node[j]->clique)) {
	/*printf("connecting cliques:\n");
print_clique(clique_node[i]->clique);
print_clique(clique_node[j]->clique); */
	connect_clique_nodes(clique_node[i],clique_node[j],intersect);
	break;
      }
    }
    if (j < low_clique) {
      printf("couldn't find connecting clique\n");
      return(ct);
    }
  }
  return(ct);
}
  
	
static CLIQUE*
possible_clique(BNODE *bn) {
/* returns subset of nodes containing bn and all neighbors
   of bn with lower max card search number.  these nodes
   are all connected, but might be a subset of a clique.
   (a clique if maximal */
     
  CLIQUE *cl;     
  int i;

  cl = alloc_clique();
  build_add_to_clique(cl,bn);
  for (i = 0; i < bn->neigh.num; i++) 
    if (bn->neigh.arc[i]->mcs_num < bn->mcs_num) 
      build_add_to_clique(cl,bn->neigh.arc[i]);
  return(cl);
}
     
	


    
static CLIQUE_TREE
new_new_build_clique_tree(BELIEF_NET *bn)
{
  CLIQUE_NODE *cur,**clique_node;
  CLIQUE *intersect,*cl,*cln;
  int i,j,low_clique,last_perfect,p,lastp;
  CLIQUE_TREE ct;

  perfect = (BNODE **) malloc(sizeof(BNODE *) * bn->num);
  clique_node = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * bn->num);
  visit_undir_graph(bn->list[0],perfect_func);
  ct.list = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * bn->num); 
  ct.num = 0;
  p = 0;

  while (p < bn->num) {
     cl = alloc_clique();
     lastp = p;
     for (; p < bn->num; p++) {
       cln = possible_clique(perfect[p]);
       if (is_subset(cl,cln)) cl = cln;
       else break;
     }
     /*print_clique(cl);*/
     cur = alloc_clique_node();
     cur->clique = cl;
     if (ct.num > 0) {
       intersect = alloc_clique();
       for (i = 0; i < cl->num; i++) if (cl->member[i]->mcs_num < lastp)
	 add_to_clique(intersect,cl->member[i]);
       for (j=ct.num-1; j >= 0; j--) {
	 if (is_subset(intersect, ct.list[j]->clique)) {
	   connect_clique_nodes(cur, ct.list[j],intersect);
	   break;
	 }
       }
       if (j < 0) {
	 printf("couldn't find connecting clique\n");
	 exit(0);
       }
     }
     /*     ct.list[ct.num++] = cur;*/
     add_to_clique_tree(cur,&ct);
  }
  ct.root = bn->list[bn->num-1]->clnode;
  return(ct);
}
  

    
static CLIQUE_TREE
new_build_clique_tree(BELIEF_NET *bn)
{
  CLIQUE_NODE *cur,**clique_node;
  CLIQUE *intersect;
  int i,j,low_clique,last_perfect;
  CLIQUE_TREE ct;

  perfect = (BNODE **) malloc(sizeof(BNODE *) * bn->num);
  clique_node = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * bn->num);
  visit_undir_graph(bn->list[0],perfect_func);
  
  /* build first clique */
  cur =  alloc_clique_node();
  build_add_to_clique(cur->clique,perfect[0]);
  for (i=1; i < bn->num; i++) {
    if (is_connected_to_all(perfect[i],cur->clique)) 
      build_add_to_clique(cur->clique,perfect[i]);
    else break;
  }
  last_perfect = low_clique = i-1;

  ct.num = bn->num-low_clique;
  ct.list = (CLIQUE_NODE **) malloc(sizeof(CLIQUE_NODE *) * bn->num); 
  /* might use less */
  ct.root = ct.list[0] = cur;
  ct.num = 1;
  
  /*printf("1st clique\n");
print_clique(ct.list[0]->clique); */

  cur->clique_id = low_clique;
  clique_node[low_clique] = cur;
  

  while (last_perfect < bn->num) {
    cur =  alloc_clique_node();
    for (; last_perfect < bn->num; last_perfect++) {
print_clique(cur->clique);
      if (is_connected_to_all(perfect[last_perfect],cur->clique)) 
	build_add_to_clique(cur->clique,perfect[last_perfect]);
      else break;
    }
printf("new clique\n");
print_clique(cur->clique);
    intersect = running_intersect(cur->clique,perfect[last_perfect-1]);
printf("intersect\n");
print_clique(intersect);
    for (j=ct.num-1; j >= 0; j--) {
      if (is_subset(intersect, ct.list[j]->clique)) {
	connect_clique_nodes(cur, ct.list[j],intersect);
	break;
      }
    }
    if (j < 0) {
      printf("couldn't find connecting clique\n");
      return(ct);
    }
    ct.list[ct.num++] = cur;
  }
  return(ct);



  for (i = low_clique+1; i < bn->num; i++) {
    ct.list[i-low_clique] = clique_node[i] = build_clique_from_top(i);
    intersect = running_intersect(clique_node[i]->clique,perfect[i]);
    /*printf("i = %d\n",i);
print_clique(clique_node[i]->clique);
print_clique(intersect); */
    for (j=i-1; j >= low_clique; j--) {
      /*printf("j = %d\n",j);
print_clique(clique_node[j]->clique); */
      if (is_subset(intersect, clique_node[j]->clique)) {
	/*printf("connecting cliques:\n");
print_clique(clique_node[i]->clique);
print_clique(clique_node[j]->clique); */
	connect_clique_nodes(clique_node[i],clique_node[j],intersect);
	break;
      }
    }
    if (j < low_clique) {
      printf("couldn't find connecting clique\n");
      return(ct);
    }
  }
  return(ct);
}
    
	
    

 
static void
tct(CLIQUE_NODE *node, int *visit, void func())
{
  int i;
  
  if (visit[node->clique_id]) return;
  print_clique(node->clique);
  visit[node->clique_id] = 1;
  for (i=0; i < node->neigh.num; i++) {
    func();
    printf("bond(%d %d)\n",node->clique_id,node->neigh.bond[i].ptr->clique_id);
    tct(node->neigh.bond[i].ptr,visit,func);
  }
}
      

 
static void
trace_clique_tree(CLIQUE_NODE *node, void func())
{
  int i,*visit;

  visit = (int *) malloc(sizeof(int)*bel_num_nodes);
  for (i=0; i < bel_num_nodes; i++) visit[i] = 0;
  
  tct(node,visit,func);
}

static MATRIX 
make_sub_vector(CLIQUE *set, CLIQUE *sub, MATRIX m)
{
  int i,j;
  MATRIX temp;

  temp.cols = 1;
  temp.rows = sub->num;
  for (i=0; i < sub->num; i++) {
    for (j=0; j < set->num; j++) {
      if (sub->member[i] == set->member[j]) {
	temp.el[i][0] = m.el[j][0];
	break;
      }
    }
    if (j == set->num) {
      printf("problem in make_sub_vector\n");
      exit(0);
    }
  }
  return(temp);
}


/*#define MAXDIM 100


static MATRIX 
extend_sub_vector(CLIQUE *set, CLIQUE *sub, MATRIX m)
{
  int i,ii, map[MAXDIM];
  MATRIX temp,Mzeros(int, int);

  temp = Mzeros(set->num,1);
  for (i=0; i < sub->num; i++) {
    for (ii=0; ii < set->num; ii++) {
      if (sub->member[i] == set->member[ii]) {
	map[i] = ii;
	break;
      }
    }
  }
  for (i=0; i < sub->num; i++) temp.el[map[i]][0] = m.el[i][0];
}


static MATRIX 
make_sub_matrix(CLIQUE *set, CLIQUE *sub, MATRIX m)
{
  int i,j,ii,jj,map[MAXDIM];
  MATRIX temp;

  temp.cols =  temp.rows = sub->num;
  for (i=0; i < sub->num; i++) {
    for (ii=0; ii < set->num; ii++) {
      if (sub->member[i] == set->member[ii]) {
	map[i] = ii;
	break;
      }
    }
  }
  for (i=0; i < sub->num; i++) 
    for (j=0; j < sub->num; j++) 
      temp.el[i][j] = m.el[map[i]][map[j]];
  return(temp);
}


static MATRIX 
extend_sub_matrix(CLIQUE *set, CLIQUE *sub, MATRIX m)
{
  int i,j,ii,jj,map[MAXDIM];
  MATRIX temp,Mzeros(int, int);

  temp = Mzeros(set->num,set->num);
  for (i=0; i < sub->num; i++) {
    for (ii=0; ii < set->num; ii++) {
      if (sub->member[i] == set->member[ii]) {
	map[i] = ii;
	break;
      }
    }
  }
  for (i=0; i < sub->num; i++) 
    for (j=0; j < sub->num; j++) 
      temp.el[map[i]][map[j]] = m.el[i][j];
  return(temp);
}



*/
      


static QUAD_FORM
integrate_out(CLIQUE *inter, CLIQUE *snd)
{ 
  QUAD_FORM temp;
  MATRIX inv,Mi(MATRIX);
  float diff,det_rat;

  /*  temp.m = make_sub_vector(snd,inter,snd->quad.m); */
  /*  temp.S = Mi(make_sub_matrix(snd,inter,Mi(snd->quad.S))); */
  diff = snd->num-inter->num;
  /*  det_rat = Mdet(temp.S)/Mdet(snd->quad.S); */
  temp.c = snd->quad.c + (diff*my_log(2*PI)+my_log(det_rat))/2;  /* this isnt rignt */
}

static float
qf(MATRIX S, MATRIX x)
{ /* S is Matrix x is vector */
  MATRIX temp,Mm(MATRIX, MATRIX),Mt(MATRIX); 

  temp = Mm(Mm(Mt(x),S),x);
  return(temp.el[0][0]);
}
	 


static QUAD_FORM
complete_the_square(QUAD_FORM q1, QUAD_FORM q2)
{  /* add the two quad forms and complete the
		       square to return a quad form */
  QUAD_FORM temp;
  MATRIX Ma(MATRIX, MATRIX),Mm(MATRIX, MATRIX),Mi(MATRIX);

  temp.S = Ma(q1.S,q2.S);
  temp.m = Mm(Mi(temp.S),Ma(Mm(q1.S,q1.m),Mm(q2.S,q2.m)));
  temp.c = q1.c + q2.c;
  temp.c += qf(q1.S,q1.m) + qf(q2.S,q2.m);
  temp.c -=  qf(temp.S,temp.m);
}



/*static void
init_belief(void) {
  init_evol_matrix();
} */




int 
clique_index(CLIQUE *cl, BNODE *node) { /* find index of node in clique */
  int i;

  for (i = 0; i < cl->num; i++)
    if (cl->member[i] == node) return(i);
  printf("couldn't find node in clique\n");
  return(0);
}

/*static SUPER_QUAD
old_init_clique_prob(CLIQUE *cl, BNODE *node) {
  /* express the conditional distribution used in generating node in terms
     of a clique potential.  the clique cl contains node and all of its
     ancestors.  this routine sets cl->quad */
/*  SUPER_QUAD temp,quad;
  SUPERMAT m,s;
  int k,i,j,n,r,c,a,b,da,db,cur,rank;
  float det;
  QUAD_FORM qf;
  BNODE *prva,*prvb;
  MATRIX ma,mb;
    
  qf = JNtoQF(node->e);
  n = cl->num;
  k = node->prev.num;
  quad = old_zero_dist(cl);
  cur = clique_index(cl,node);
  quad.m.sub[cur][0] = qf.m;
  quad.S.sub[cur][cur] = qf.S;
  for (i=0; i < k; i++) {
    prva = node->prev.arc[i];
    ma = node->prev.A[i];
    a = clique_index(cl,prva);
    da = prva->dim;
    quad.m.sub[a][1] = Mzeros(da,1);
    quad.S.sub[cur][a] = Msc(Mm(qf.S,ma),-1.);
    quad.S.sub[a][cur] = Msc(Mm(Mt(ma),qf.S),-1.);
    for (j=0; j < k; j++) {
      prvb = node->prev.arc[j];
      mb = node->prev.A[j];
      b = clique_index(cl,prvb);
      db = prvb->dim;
      /*     quad.S.sub[a][b] = sq.S.sub[i][j]; */
/*      quad.S.sub[a][b] = Mm(Mm(Mt(ma),qf.S),mb);
    }
  }      
  quad.c = qf.c;
  return(quad);
}
*/


static QUAD_FORM
exper_init_clique_prob(CLIQUE *cl, BNODE *node) {
  /* express the conditional distribution used in generating node in terms
     of a clique potential.  the clique cl contains node and all of its
     ancestors. there may be other nodes in the clique though*/

  SUPERMAT Big,Tall,Perm;
  int i,n,r,a,b,self,dim[MAX_CLIQUE],dimsum=0,prev;
  BNODE *prva;
  MATRIX A[MAX_CLIQUE],big,tall,xxx,null,comp,perm;
  QUAD_FORM update,tall_update,trans,quad,acc,cond,cond_prom;

  /*       if (Mnormsq(node->e.m) > .01) {
      printf("meas time = %f node->index = %d\n",score.midi.burst[node->index].time,node->index);
            printf("prior: node is %d\n",node->mcs_num);
	         QFprint(node->e);
      }    */

  /*    if (cl->num == 3) {
    print_clique(cl);
    printf("node num = %d mcs = %d\n",node->node_num,node->mcs_num);
    QFprint(node->e);
    }*/

  /*  print_clique(cl);*/
  self = clique_index(cl,node);  /* clique index referring to node */
  for (a=0; a < cl->num; a++)  dim[a] = cl->member[a]->dim;  
  n = cl->num;
  Perm  = Salloc(cl->num,1);
  for (a=0; a < cl->num; a++) 
    Perm.sub[a][0] = (a == self) ? Miden(dim[a]) : Mzeros(dim[a],dim[self]);
  perm = expand(Perm);
  acc = QFpromote(node->e,perm);
  for (i=0; i < node->prev.num; i++) {
    /*  node->prev.e[i].S = Miden(node->prev.e[i].m.rows);
  node->prev.e[i].cov = Miden(node->prev.e[i].m.rows);
  node->prev.e[i].fin = Miden(node->prev.e[i].m.rows);
  node->prev.e[i].null  = Mzeros(node->prev.e[i].m.rows,0);
  node->prev.e[i].inf  = Mzeros(node->prev.e[i].m.rows,0);
  if (Mnormsq(node->prev.e[i].m) > .01) {
    printf("cond: node is %d\n",node->mcs_num);
    QFprint(node->prev.e[i]);
    }*/
    cond = QFcond_dist(node->prev.A[i],node->prev.e[i]);
    prev = clique_index(cl,node->prev.arc[i]);  /* clque indx referring to prev node */
    Perm  = Salloc(cl->num,2);
    for (a=0; a < cl->num; a++) {
      Perm.sub[a][0] = (a == prev) ? Miden(dim[a]) : Mzeros(dim[a],dim[prev]);
      Perm.sub[a][1] = (a == self) ? Miden(dim[a]) : Mzeros(dim[a],dim[self]);
    }
    perm = expand(Perm);
    cond_prom = QFpromote(cond,perm);
    /*    if (cl->num == 3) {
      printf("cond\n");
     QFprint(cond);
      printf("cond_prom\n");
     QFprint(cond_prom);
     } */
    acc = QFplus(acc,cond_prom);
    /*    if (cl->num == 3) {
      printf("acc\n");
      QFprint(acc);
      } */
    /*    printf("Matrix A\n");
    Mp(node->prev.A[i]);
    printf("update\n");
    QFprint(node->prev.e[i]);
    printf("conditional\n");
    QFprint(cond);*/
  }
  /*  if (cl->num == 3) {
    QFprint(acc);
    } */
  return(acc);
}





static QUAD_FORM
new_new_init_clique_prob(CLIQUE *cl, BNODE *node) {
  /* express the conditional distribution used in generating node in terms
     of a clique potential.  the clique cl contains node and all of its
     ancestors. there may be other nodes in the clique though*/

  SUPERMAT Perm,Amat;
  int i,a,self,dim[MAX_CLIQUE],ax[MAX_CLIQUE];
  MATRIX perm,A;
  QUAD_FORM cond,ret;


  self = clique_index(cl,node);  /* clique index referring to node */
  for (i=0; i < node->prev.num; i++) 
    ax[i]  = clique_index(cl,node->prev.arc[i]);  /* clique index of ith predecessor */
  for (a=0; a < cl->num; a++)  dim[a] = cl->member[a]->dim;  
  if (node->prev.num) {
    Amat  = Salloc(1,node->prev.num);
    for (i=0; i < node->prev.num; i++) Amat.sub[0][i] = node->prev.A[i];
    A = expand(Amat);    
    cond = QFcond_dist(A,node->e); /* node->prev.num + 1 (vector) components (last comp is for self) */
  }
  else cond = node->e;
  Perm = Salloc(cl->num,node->prev.num+1);  /* permutation cond space to clique space  */
  for (a=0; a < cl->num; a++) for (i=0; i < node->prev.num; i++) 
    Perm.sub[a][i] =  Mzeros(dim[a],dim[ax[i]]);
  for (a=0; a < cl->num; a++) 
    Perm.sub[a][node->prev.num] =  Mzeros(dim[a],dim[self]);
  for (i=0; i < node->prev.num; i++) 
    Perm.sub[ax[i]][i] =  Miden(dim[ax[i]]);
  Perm.sub[self][node->prev.num] =  Miden(dim[self]);
  perm = expand(Perm);
  ret = QFpromote(cond,perm);
  return(ret);
}




static QUAD_FORM
new_init_clique_prob(CLIQUE *cl, BNODE *node) {
  /* express the conditional distribution used in generating node in terms
     of a clique potential.  the clique cl contains node and all of its
     ancestors. there may be other nodes in the clique though*/

  SUPERMAT Big,Tall;
  int k,i,n,r,a,b,self,dim[MAX_CLIQUE],dimsum=0;
  BNODE *prva;
  MATRIX A[MAX_CLIQUE],big,tall,xxx,null,comp;
  QUAD_FORM update,tall_update,trans,quad;



  /*  if (cl->num == 3) {
    print_clique(cl);
    printf("node num = %d mcs = %d\n",node->node_num,node->mcs_num);
    QFprint(node->e);
    }*/

  self = clique_index(cl,node);  /* clique index referring to node */
  update = node->e;    

  n = cl->num;
  k = node->prev.num;
  /*printf("\n\n\nk = %d I am %d\n",k,node->node_num); 
print_clique(cl); */
  for (a=0; a < cl->num; a++)  dim[a] = cl->member[a]->dim;  
  /*for (a=0; a < cl->num; a++)  printf("nodenum is %d\n", cl->member[a]->node_num);  */
  dim[self] = node->dim;
  for (a=0; a < cl->num; a++)  A[a] = Mzeros(dim[self],dim[a]);  
  /* clique els not ancestors of node are   initalized */
  A[self] = Mzeros(node->dim,node->dim);
  Big = Salloc(n,n);
  Tall = Salloc(n,1);
  /*  printf("k = %d\n",k);*/
  for (i=0; i < k; i++) {
    prva = node->prev.arc[i];
    a = clique_index(cl,prva);
    A[a] = node->prev.A[i];
    /*       printf("A[%d] = \n",i);
	 Mp(A[a]);  */

    dim[a] =  prva->dim;
  } /* A[a],dim[a] are matrix and dimension assoc with ath clique member */
  for (a=0; a < cl->num; a++) dimsum += dim[a];


  for (a=0; a < cl->num; a++) 
    Tall.sub[a][0] = (a == self) ? Miden(dim[a]) : Mzeros(dim[a],dim[self]);
  for (a=0; a < cl->num; a++) 
    for (b=0; b < cl->num; b++) {
      Big.sub[a][b] = (a == self) ? A[b] : (a == b) ? Miden(dim[a]) : Mzeros(dim[a],dim[b]);
    }
  big = expand(Big);
  tall = expand(Tall);
  trans = QFxform(QFunif(dimsum),big);
  tall_update = QFxform(update,tall);
  quad = QFindep_sum(trans,tall_update);
  /*  if (cl->num == 3) {
    Mp(tall);
    Mp(big);
    QFprint(trans);
      for (a=0; a < cl->num; a++) {
    printf("A[%d] = \n",a);
    Mp(A[a]);
      } 
         exit(0);
	 }*/

  /*    for (a=0; a < cl->num; a++) {
    printf("A[%d] = \n",a);
    Mp(A[a]);
  } 
    Sprint(Big);
  printf("self = %d\n",self);
  printf("big = \n");
  Mp(big);
  printf("tall = \n");
  Mp(tall);
  printf("tall_update\n");
  QFprint(tall_update);
  printf("update\n");
  QFprint(update); */
  /*xxx = Mzeros(quad.null.rows,1);
for (i=0; i < quad.null.rows; i++) xxx.el[i][0] = (i%3 != 2) ?  3+i : 0; 
printf("here is a check\n");
printf("x is\n");
Mp(xxx);
printf("proj is \n");
Mp(Mm(Mm(trans.null,Mt(trans.null)),xxx));
Mnull_decomp(Mt(big),&null,&comp);
printf("null of t(A)\n");
Mp(null);
Mp(Mm(Mt(big),null)); */

  /*  printf("trans\n");
  QFprint(trans);
  printf("quad\n");
  QFprint(quad);  */
  /*  if (node->node_num == 24) exit(0); */
  return(quad); 
}


static QUAD_FORM
init_clique_prob(CLIQUE *cl, BNODE *node) {
  /* express the conditional distribution used in generating node in terms
     of a clique potential.  the clique cl contains node and all of its
     ancestors. there may be other nodes in the clique though*/

  SUPERMAT M_inf,M_fin,mean,Constr,nn;
  int k,i,j,n,r,a,b,self,dim[MAX_CLIQUE],d;
  QUAD_FORM quad;
  BNODE *prva;
  MATRIX N[MAX_CLIQUE],upd_inf,trash,A[MAX_CLIQUE],restr,tmp,Minf,Mfin,MU,MD,P,F;
  MATRIX Pz,Pzc,Uz,mzc,mz,B[MAX_CLIQUE],V1,V2;
  MATRIX prod,mu,c,constr,particular,temp,null_inf,null,new_null;
  QUAD_FORM update,alt;

  self = clique_index(cl,node);  /* clique index referring to node */
  update = node->e;    
  /*  Uz = upd_inf = Meigen_space(update.U,update.D,0.);*/
  Uz = upd_inf = update.null;
  Pz = Mm(Uz,Mt(Uz));           /* projection onto null space of covariance of update */
  Pzc = Ms(Miden(Pz.rows),Pz);   /* projection onto complement of above */
  mz =  Mm(Pz,update.m);        /* projection of mu onto constrained space */
  /*  restr = Mconcentration(update.U,update.D);  */
  restr = (update.S.cols != 0) ? update.S : Msym_range_inv(update.cov,update.fin);
  n = cl->num;
  k = node->prev.num;
  for (a=0; a < cl->num; a++)  dim[a] = cl->member[a]->dim;  
  dim[self] = node->dim;
  for (a=0; a < cl->num; a++)  A[a] = Mzeros(dim[self],dim[a]);  /* clique els not ancestors of node are
								    initalized */
  A[self] = Miden(node->dim);
  M_fin = Salloc(n,n);
  Constr = Salloc(1,n);
  mean = Salloc(n,1);
  for (i=0; i < k; i++) {
    prva = node->prev.arc[i];
    a = clique_index(cl,prva);
    A[a] = Msc(node->prev.A[i],-1.);
    dim[a] =  prva->dim;
  } /* A[a],dim[a] are matrix and dimension assoc with ath clique member */
  for (a=0; a < cl->num; a++) {
    B[a] = Mm(Pzc,A[a]);
    Constr.sub[0][a] = Mm(Pz,A[a]);
    mean.sub[a][0] = (a == self) ? update.m : Mzeros(dim[a],1);
  }
  for (a=0; a < cl->num; a++) 
    for (b=0; b < cl->num; b++)  M_fin.sub[a][b] = Mm(Mm(Mt(B[a]),restr),B[b]);
  quad.m = expand(mean);
  quad.S = expand(M_fin);
  constr = expand(Constr);

  /*printf("origianal S\n");
Mp(quad.S);u
printf("restr\n");
Mp(restr);
printf("update\n");
QFprint(update);
for (b=0; b < cl->num; b++)  Mp(B[b]);
  Mnull_decomp(constr,&V1,&V2); /* V1 spans N(constr);  V2 spans N(constr) comp */
  /*  particular  = Msolve(constr,mz);
  c  = Mm(Mt(V2),particular);       /*  {x:constr x = mz} = {x:V2'x = c} */
  /*  quad = restrict(quad, V1, V2, c);
printf("before quad_set\n");
printf("U = \n");
Mp(quad.U); 
printf("D = \n");
Mp(quad.D);
  quad = QFset(quad.m,quad.U,quad.D); 
  printf("quad = \n");
QFprint(quad);  
  return(quad); */

  Mnull_decomp(quad.S,&null_inf,&quad.fin);  /* null_inf has both null and inf spaces */


      Mnull_decomp(constr,&temp,&null); 
    Mint_comp(null_inf, null, &quad.null, &quad.inf); 




  quad.cov = Msym_range_inv(quad.S,quad.fin);

  /*
printf("constr\n");
Mp(constr);
printf("quad.S\n");
Mp(quad.S);
printf("quad.fin\n");
Mp(quad.fin);
printf("null_inf\n");
Mp(null_inf);
printf("quad.null\n");
Mp(quad.null);
printf("temp\n");
Mp(temp);
printf("quad.inf\n");
Mp(quad.inf); */

  /*  if (quad.fin.cols > 0 && Mnorm(quad.cov) < .01) {
    printf("cov = \n");
    Mp(quad.cov);
    printf("S = \n");
    Mp(quad.S);
    printf("restr = \n");
    Mp(restr);
    printf("update.cov = \n");
    Mp(update.cov);
    
    exit(0);
  } */
  /*  printf("mean\n");
  Sprint(mean);
  printf("S\n");
  Sprint(M_fin);
  printf("constr\n");
  Sprint(Constr);
  exit(0); */

  printf("mycheck\n");
print_clique(cl);
printf("node_num is %d\n",node->node_num);
printf("quad\n\n");
QFprint(quad);

printf("update = \n\n");
QFprint(update); 
printf("constraint = \n\n");
Mp(constr); 
printf("null = \n\n");
Mp(null);
printf("null_inf = \n\n");
Mp(null_inf);
printf("is null_inf orthogonal to quad.fin?\n");
Mp(Mm(Mt(null_inf),quad.fin));
printf("quad.s*null\n");
Mp(Mm(quad.S,null));

  alt = new_init_clique_prob(cl, node);
  QFcompare(alt,quad);


  return(quad); 


}

//CF:  malloc a qf for one clique
void
alloc_clique_qf(CLIQUE *c)
{
  int i,dim=0;
  QUAD_FORM temp;
  
  if (c->qf.m.el) return;  /* already allocated */
  for (i=0; i < c->num; i++) 
    dim += c->member[i]->dim;  
  c->qf = QFinit(dim);
  c->memory = QFinit(dim);  /* take this out in realtime to make program smaller to fit in
  /*				memory */
}







static void
save_separators(CLIQUE_TREE ct) {
  int i,j;
  CLIQUE_NEIGH cn;
  CLIQUE *clq;

  for (i=0; i < ct.num; i++) {
    cn =  ct.list[i]->neigh;
    for (j = 0; j < cn.num; j++) {
      clq = cn.bond[j].intersect;
      QFcopy(clq->qf,&clq->memory); 
    }
  }
}

static void
recall_separators(CLIQUE_TREE ct) {
  int i,j;
  CLIQUE_NEIGH cn;
  CLIQUE *clq;

  for (i=0; i < ct.num; i++) {
    cn =  ct.list[i]->neigh;
    for (j = 0; j < cn.num; j++) {
      clq = cn.bond[j].intersect;
      QFcopy(clq->memory,&clq->qf); 
    }
  }
}



  
//CF:  
static void
init_cliques(CLIQUE_TREE ct) {
  int i;
  CLIQUE *clq;
  QUAD_FORM temp;

  for (i=0; i < ct.num; i++) {
      clq = ct.list[i]->clique;
      /*      clq->quad = old_zero_dist(clq);
            clq->qf = zero_dist(clq); */
      /*      alloc_clique_qf(clq); */
      //CF:  create a qf flat distro using matrices from the temporary matrix buffer; then deep-copy it to permanent mem 
      QFcopy(new_zero_dist(clq), &clq->qf);   
      clear_matrix_buff(); 
  }
}

static void
init_separators(CLIQUE_TREE ct) {
  int i,j,x,y;
  CLIQUE_NEIGH cn;
  CLIQUE *clq;
  /*  QUAD_FORM qf;*/

  for (i=0; i < ct.num; i++) {
    cn =  ct.list[i]->neigh;
    for (j = 0; j < cn.num; j++) {
      clq = cn.bond[j].intersect;
      /*      clq->quad = old_zero_dist(clq); */
      /*                  clq->qf = zero_dist(clq); */
      /*      alloc_clique_qf(clq); */
      /*    x = malloc(CHUNK);
      
    qf = new_zero_dist(clq);
    y = malloc(CHUNK);
    printf("now here used %d\n",(y-x)-(CHUNK));*/


      QFcopy(new_zero_dist(clq), &clq->qf); 
      clear_matrix_buff();
    }
  }
}

init_clique_tree_qfs(CLIQUE_TREE ct) {
  init_cliques(ct);
  init_separators(ct);
}

static void
alloc_clique_qfs(CLIQUE_TREE ct) {
  int i;
  CLIQUE *clq;
  QUAD_FORM temp;

  for (i=0; i < ct.num; i++) {  //CF:  for each clique in clique tree
      clq = ct.list[i]->clique;
      alloc_clique_qf(clq);
  }
}
//CF:  
static void
alloc_separator_qfs(CLIQUE_TREE ct) {
  int i,j;
  CLIQUE_NEIGH cn;
  CLIQUE *clq;

  for (i=0; i < ct.num; i++) {
    cn =  ct.list[i]->neigh;
    for (j = 0; j < cn.num; j++) {
      clq = cn.bond[j].intersect;
      /*       printf("clq = %x mem = %x\n",clq,clq->qf.m.el);*/
      alloc_clique_qf(clq);
    }
  }
}
//CF:  mallocs
static void
alloc_clique_tree_qfs(CLIQUE_TREE ct) {
  alloc_clique_qfs(ct);
  alloc_separator_qfs(ct);
}

static void
save_cliques(CLIQUE_TREE ct) {
  int i;
  CLIQUE *clq;

  for (i=0; i < ct.num; i++) {
      clq = ct.list[i]->clique;
      QFcopy(clq->qf, &clq->memory); 
  }
}

static void
recall_cliques(CLIQUE_TREE ct) {
  int i;
  CLIQUE *clq;

  for (i=0; i < ct.num; i++) {
      clq = ct.list[i]->clique;
      QFcopy(clq->memory, &clq->qf); 
  }
}

static void
save_state(CLIQUE_TREE ct) {
  printf("saving state\n");
  save_cliques(ct);
  save_separators(ct);
}

static void
at_equil(CLIQUE_TREE ct) {
  int i,j;

  for (i=0; i < ct.num; i++) 
    for (j=0; j < ct.list[i]->neigh.num; j++)
      ct.list[i]->neigh.bond[j].dry = 0;
}


static void
recall_state(CLIQUE_TREE ct) {
  recall_cliques(ct);
  recall_separators(ct);
  at_equil(ct);
}

void
recall_global_state() {
  int i,overlap;
  
  printf("recalling the global belief state\n");
  for (i=0; i < component.num; i++)  recall_state(component.list[i].net.ct);
}

  
void
recall_partial_global_state() {
  int i,overlap;
  

  for (i=0; i < component.num; i++) {
   overlap =  does_phrase_overlap(component.list[i].net);
   if (overlap) recall_state(component.list[i].net.ct);
  }

}

  

 static void
init_cliques_with_probs(CLIQUE_TREE ct, BELIEF_NET bn) {
  int i,j,k,n,perm[MAX_BSUCC]; /* perm[i] is index of assoc clique el */
  CLIQUE c,*cl;
  MATRIX A[MAX_BSUCC]; 
  BNODE *b;
  SUPER_QUAD sq;
  QUAD_FORM qf,temp;



   init_cliques(ct);
      init_separators(ct); 
  for (i=0; i < bn.num; i++) {
    /*printf("i =           %d\n",i); */


    c.num = 0;
    b = bn.list[i];
    /*printf("node num is %d\n",b->node_num);
QFprint(b->e); */
    add_to_clique(&c,b);
    n = b->prev.num;
    for (j = 0; j < n; j++) {
      A[j] = b->prev.A[j];
      add_to_clique(&c,b->prev.arc[j]);
    } /* c contains b and its ancestors */
    for (k=0; k < ct.num; k++) { /* find a clique tree element containing c */
      cl =  ct.list[k]->clique;
      if (is_subset(&c,cl))  	break;
    }
    if (k == ct.num) { printf("problem in init_cliques_with_probs()\n"); exit(0); }
    else {
      /*printf("node type is %d\n",b->note_type);*/
      /*                 qf = new_init_clique_prob(cl,b);*/
                  qf = new_new_init_clique_prob(cl,b); 
	    /*       qf = exper_init_clique_prob(cl,b);*/



      /*      printf("hoho1\n");
      QFcheck(qf); */
      /*      printf("arg1\n");
      QFprint(cl->qf);
      printf("arg2\n");
      QFprint(qf); */
      
      /*      temp =  QFplus(cl->qf,qf); */
      QFcopy(QFplus(cl->qf,qf),&(cl->qf));




      /*printf("clique\n");
print_clique(cl);
printf("adding:\n");
QFprint(qf);
printf("result\n");
QFprint(cl->qf);    */
/*      printf("hoho2\n");
      QFcheck(cl->qf); */
      clear_matrix_buff();
      /*      cl->qf = QFplus(cl->qf,qf); */
      /*      printf("answer1\n");
      QFprint(cl->qf);*/
      /*      printf("answer2\n");
      QFprint(temp); */
      
      /*      sq = init_clique_prob(cl,b);
            cl->quad = SQadd(sq,cl->quad); */
    }
    /*    SQprint(cl->quad); */
  }
}

static QUAD_FORM
permute_potential(CLIQUE *new, CLIQUE *old, QUAD_FORM qf) {
  SUPERMAT Perm;   //CF:  this is the only place SUPERMAT gets used
  int i,a,d;
  MATRIX perm;
  QUAD_FORM ret;

  Perm = Salloc(new->num,old->num);
  for (a=0; a < new->num; a++) for (i=0; i < old->num; i++) 
    Perm.sub[a][i] =  Mzeros(new->member[a]->dim,old->member[i]->dim);
  for (i=0; i < old->num; i++) {
    d  = clique_index(new,old->member[i]);
    Perm.sub[d][i] =  Miden(new->member[d]->dim);
  }
  perm = expand(Perm);
  ret = QFpromote(qf,perm);
  return(ret);
}


static int
clique_in_focus(CLIQUE *cl) {
  int i;
  
  for (i=0; i < cl->num; i++)
    if (cl->member[i]->focus == 0) return(0);
  return(1);
}


//CF:  set init probs for cliques
static void
init_cliques_using_potentials(CLIQUE_TREE ct, BELIEF_NET bn) {
  int i,k,l,x,y,j;
  CLIQUE *cl,*po;
  QUAD_FORM qf;
  CLIQUE_NEIGH cn;

  init_clique_tree_qfs(ct);   //CF:  everything set to flat distros

  /*  init_cliques(ct);*/
    /*  init_separators(ct); */

   //CF:  for every BNODE's potential, find any ONE clique containing it, and mutiply it into that clique potential
  for (i=0; i < bn.potential.num; i++) { 
    po = bn.potential.el[i];        //CF:  not used; always =NUMERATOR
    /*    printf("i = %d null = %d\n",i,po->qf.null.el);*/
    if (clique_in_focus(po) == 0) continue;
    if (po->polarity == NUMERATOR) {
      for (k=0; k < ct.num; k++) { /* find a clique tree element containing c */
	cl =  ct.list[k]->clique;
	if (is_subset(po,cl)) break;
      }
    }
    //CF:  not used
    else if (po->polarity == DENOMINATOR) {
      for (k=0; k < ct.num; k++) { /* find a clique tree element containing c */
	cn =  ct.list[k]->neigh;
	for (l=0; l < cn.num; l++) {
	  cl = cn.bond[l].intersect;
	  if (is_subset(po,cl)) break;
	}
	if (l < cn.num) break;
      }
    }
    //CF:  should never happen:
    else { printf("unknown polarity (%d)\n",po->polarity); exit(0); }

    //CF:  should never happen:
    if (k == ct.num) { 
      printf("couldn't find a clique contining potential out of %d cliques\n",ct.num);
      if (po->polarity == NUMERATOR) printf("polarity = numerator\n");
      else printf("polarity = denominator\n");
      print_clique(po);
      exit(0);
    }


    /*    else print_clique(cl); */
    /*  QFprint(po->qf); */
    /*    for (j=0; j < po->num; j++) printf("dim[%d] = %d\n",j,po->member[j]->dim);
    for (j=0; j < cl->num; j++) printf("dim[%d] = %d\n",j,cl->member[j]->dim);
    printf("qf.dim = %d (%d)\n",qf.cov.rows,qf.cov.cols);*/
    qf = permute_potential(cl,po,po->qf);   //CF: spin the clique's qf so dimensions match its BNODE list, 'member'.
    /*    QFprint(qf);*/
    QFcopy(QFplus(cl->qf,qf),&(cl->qf));    //CF:  multiply in the new potention (and copy into permanant memory)
    clear_matrix_buff();
  }
}



static void
make_connections(CLIQUE_TREE ct, BELIEF_NET bn) {
  int i,j,k,n,perm[MAX_BSUCC]; /* perm[i] is index of assoc clique el */
  CLIQUE c,*cl;
  MATRIX A[MAX_BSUCC]; 
  BNODE *b;
  SUPER_QUAD sq;
  QUAD_FORM qf,temp;


  for (i=0; i < bn.num; i++) {
    c.num = 0;
    b = bn.list[i];
    if (b->note_type != ANCHOR_NODE) continue;
    b->e = QFpos_near(b->dim);
    /*  printf("node num is %d\n",b->node_num);
	QFprint(b->e); */
    add_to_clique(&c,b);
    n = b->prev.num;
    for (j = 0; j < n; j++) {
      A[j] = b->prev.A[j];
      if (Mnormsq(b->prev.A[j]) == 0.) {
	b->prev.A[j].el[POS_INDEX][POS_INDEX]= 1.;
      }
      else {
	b->prev.A[j] = Mperm(Mzeros(A[j].rows,A[j].cols));
      }
      add_to_clique(&c,b->prev.arc[j]);
    } /* c contains b and its ancestors */
    for (k=0; k < ct.num; k++) { /* find a clique tree element containing c */
      cl =  ct.list[k]->clique;
      if (is_subset(&c,cl))  	break;
    }
    print_clique(cl);
    if (k == ct.num) printf("problem in init_cliques_with_probs()\n");
    else {
      /*printf("node type is %d\n",b->note_type);*/
      qf = new_init_clique_prob(cl,b);
      /*      printf("hoho1\n");
      QFcheck(qf); */
      /*                printf("arg1\n"); 
      QFprint(cl->qf);
            printf("arg2\n");
	    QFprint(qf);   */
      
      /*      temp =  QFplus(cl->qf,qf); */
      QFprint(cl->qf);
      QFcopy(QFplus(cl->qf,qf),&(cl->qf));
      /*                        printf("clique\n");
print_clique(cl);
printf("adding:\n");
QFprint(qf);
printf("result\n");
QFprint(cl->qf);     
      
/*      printf("hoho2\n");
      QFcheck(cl->qf); */
      clear_matrix_buff();
      /*      cl->qf = QFplus(cl->qf,qf); */
      /*      printf("answer1\n");
      QFprint(cl->qf);*/
      /*      printf("answer2\n");
      QFprint(temp); */
      
      /*      sq = init_clique_prob(cl,b);
            cl->quad = SQadd(sq,cl->quad); */
    }
    /*    SQprint(cl->quad); */
  }
}



/*static SUPER_QUAD
old_marginalize(CLIQUE *cl1, CLIQUE *cl2) {
  /* cl1 is a clique containing cl2.  integrate the "prob" dist of cl1
     to marginalize on the clique cl2.  set cl2 to that marginal
     dist */
/*  SUPER_QUAD fullsq,subsq;
  SUPER_JN fullsjn,subsjn;
  int i1,j1,i2,j2,r;
  BNODE *nd;
  JOINT_NORMAL jn;

  fullsq = cl1->quad;
  fullsjn = SQtoSJN(fullsq);
  /*printf("full jn:\n");
SJNprint(fullsjn); */

/*  r = cl2->quad.m.rows; /* just inherit shape */
/*  subsjn.mean = Salloc(r,1);
  subsjn.var = Salloc(r,r);
  


  for (i1=0; i1 < cl2->num; i1++) {
    j1 = clique_index(cl1, cl2->member[i1]); 
    subsjn.mean.sub[i1][0] = fullsjn.mean.sub[j1][0];
    for (i2=0; i2 < cl2->num; i2++) { 
      j2 = clique_index(cl1, cl2->member[i2]); 
      subsjn.var.sub[i1][i2] = fullsjn.var.sub[j1][j2];
    }
  }
  jn = JNset(expand(subsjn.mean),expand(subsjn.var));
  subsjn.lc = jn.lc;
  subsq = SJNtoSQ(subsjn);
  /*  subsq.c = fullsq.c; a modific of this */
  
  
  /* subsjn = SQtoSJN(subsq);
  printf("marginal:\n");
  SJNprint(subsjn);  */

/*  return(subsq);
}*/

  

static MATRIX
permute_matrix(CLIQUE *cl1, CLIQUE *cl2) {
  /* cl1 contains cl2.  return matrix that permutes the components of cl2 
     into the corresponding components of cl1 */
  MATRIX A;
  SUPERMAT AA;
  int i,j,dim[MAX_CLIQUE],perm[MAX_CLIQUE];

  for (i = 0; i < cl1->num; i++) dim[i] = cl1->member[i]->dim;
  for (i=0; i < cl2->num; i++) perm[i] = clique_index(cl1, cl2->member[i]); 
  /* perm[i] is index in cl1 of ith member of cl2 */

  AA = Salloc(cl1->num,cl2->num);
  for (i = 0; i < cl2->num; i++) for (j = 0; j < cl1->num; j++) 
    AA.sub[j][i] = Mzeros(dim[j],dim[perm[i]]);
  for (i=0; i < cl2->num; i++) AA.sub[perm[i]][i] = Miden(dim[perm[i]]);
  A = expand(AA);
  return(A);
}




static QUAD_FORM
marginalize_sep(CLIQUE *cl1, CLIQUE *cl2) {
  /* cl1 is a clique containing cl2.  integrate the "prob" dist of cl1
     to marginalize on the clique cl2.  set cl2 to that marginal
     dist */
  MATRIX A;
  QUAD_FORM ret;



  A = permute_matrix(cl1,cl2);
  
  ret = QFmargin(cl1->qf,Mt(A));
  return(ret);
}


static QUAD_FORM
new_marginalize(CLIQUE *cl1, CLIQUE *cl2) {
  /* cl1 is a clique containing cl2.  integrate the "prob" dist of cl1
     to marginalize on the clique cl2.  set cl2 to that marginal
     dist */
  int i,j,dim[MAX_CLIQUE],perm[MAX_CLIQUE];
  MATRIX A;
  SUPERMAT AA;
  QUAD_FORM qf,test_QFxform(),temp;

  for (i = 0; i < cl1->num; i++) dim[i] = cl1->member[i]->dim;
  for (i=0; i < cl2->num; i++) perm[i] = clique_index(cl1, cl2->member[i]); 
  /* perm[i] is index in cl1 of ith member of cl2 */

  if (cl1->qf.cov.cols == 0) cl1->qf.cov  = Msym_range_inv(cl1->qf.S,cl1->qf.fin);
  AA = Salloc(cl2->num,cl1->num);
  for (i = 0; i < cl2->num; i++) for (j = 0; j < cl1->num; j++) 
    AA.sub[i][j] = Mzeros(dim[perm[i]],dim[j]);
  for (i=0; i < cl2->num; i++) AA.sub[i][perm[i]] = Miden(dim[perm[i]]);
  A = expand(AA);
  qf = QFxform(cl1->qf, A);

  /*  temp = marginalize_sep(cl1,cl2);
      QFcompare(temp,qf); */

  return(qf);
}
  








  

static QUAD_FORM
new_promote(CLIQUE *cl1, CLIQUE *cl2, QUAD_FORM qf) {
  /* cl1 is a clique containing cl2.  express the dist q2
     of cl2 in terms of the elements (and order) of cl1 */
  int i,j,dim[MAX_CLIQUE],perm[MAX_CLIQUE],in_range[100];
  MATRIX A;
  SUPERMAT AA;
  QUAD_FORM ret,base;

#ifdef DEBUG
  QFcheck(cl2->qf);
#endif
  for (i = 0; i < cl1->num; i++) dim[i] = cl1->member[i]->dim;
  for (i=0; i < cl2->num; i++) perm[i] = clique_index(cl1, cl2->member[i]); 
  /* perm[i] is index in cl1 of ith member of cl2 */

  /*  if (cl1->qf.cov.cols == 0) cl1->qf.cov  = Msym_range_inv(cl1->qf.S,cl1->qf.fin);*/
  AA = Salloc(cl1->num,cl2->num);
  for (i = 0; i < cl2->num; i++) for (j = 0; j < cl1->num; j++) 
    AA.sub[j][i] = Mzeros(dim[j],dim[perm[i]]);
  for (i=0; i < cl2->num; i++) AA.sub[perm[i]][i] = Miden(dim[perm[i]]);
  A = expand(AA);
  if (A.rows > 100) {
    printf("strange problem in new_promote\n");
    exit(0);
  }
  for (i=0; i < A.rows; i++) {
    for (j=0; j < A.cols; j++) if (A.el[i][j] != 0.) break;
    in_range[i] =  (j != A.cols);
  }
  base = QFnull_inf(A.rows, in_range);
  ret = QFxform(qf, A);
  ret = QFindep_sum(ret,base);
  return(ret);
}
  



static QUAD_FORM
promote_sep(CLIQUE *cl1, CLIQUE *cl2, QUAD_FORM qf) {
  /* cl1 is a clique containing cl2.  express the dist q2
     of cl2 in terms of the elements (and order) of cl1 */
   MATRIX A;
   QUAD_FORM ret;

#ifdef DEBUG
  QFcheck(cl2->qf);
#endif

  A = permute_matrix(cl1,cl2);
  ret = QFpromote(qf,A);
  return(ret);
}
  







static QUAD_FORM
zero_subset(CLIQUE *cl1, CLIQUE *cl2) {
  /* cl1 is a clique containing cl2.  return cl1 with 
     the part of cl1 corresponding
     to cl2 set to zero. */
  int i,j,dim[MAX_CLIQUE],yes[MAX_CLIQUE];
  MATRIX A;
  SUPERMAT AA;
  QUAD_FORM qf,test_QFxform();


  for (i = 0; i < cl1->num; i++) dim[i] = cl1->member[i]->dim;
  for (i = 0; i < cl1->num; i++) yes[i] = 1;
  for (i=0; i < cl2->num; i++) {
    j = clique_index(cl1, cl2->member[i]); 
    /* j is index in cl1 of ith member of cl2 */
    yes[j] = 0;
  }

  AA = Salloc(cl1->num,cl1->num);
  for (i = 0; i < cl1->num; i++) for (j = 0; j < cl1->num; j++) 
    AA.sub[i][j] = Mzeros(dim[i],dim[j]);
  for (i=0; i < cl1->num; i++) 
    if (yes[i])  AA.sub[i][i] = Miden(dim[i]);
  A = expand(AA);
  qf = QFxform(cl1->qf, A);
  return(qf);
}
  

static QUAD_FORM
marginalize(CLIQUE *cl1, CLIQUE *cl2) {
  /* cl1 is a clique containing cl2.  integrate the "prob" dist of cl1
     to marginalize on the clique cl2.  set cl2 to that marginal
     dist */
  QUAD_FORM qf,alt;
  int i,j,dim[MAX_CLIQUE],perm[MAX_CLIQUE];
  MATRIX temp,t1,t2,inf_ortho,tmp,inf_space,P,sigma,not_inf_ortho,reduce,null_inf,D,U;
  SUPERMAT sup_inf,sub_inf,sub_fin,sup_fin,sup_mean,sub_mean;



QFcheck(cl1->qf);

  sub_inf = Salloc(cl2->num,1);
  sub_mean = Salloc(cl2->num,1);
  sub_fin = Salloc(cl2->num,cl2->num);
  for (i = 0; i < cl1->num; i++) dim[i] = cl1->member[i]->dim;
  for (i=0; i < cl2->num; i++) perm[i] = clique_index(cl1, cl2->member[i]); 
  /* perm[i] is index in cl1 of ith member of cl2 */
  /*  inf_space = Meigen_space(cl1->qf.U,cl1->qf.D,HUGE_VAL); */
  inf_space = cl1->qf.inf; /* xxx */


  sup_inf = row_partition(inf_space,dim,cl1->num);
  sup_mean = row_partition(cl1->qf.m,dim,cl1->num);
  for (i=0; i < cl2->num; i++) {
    sub_inf.sub[i][0] = sup_inf.sub[perm[i]][0];
    sub_mean.sub[i][0] = sup_mean.sub[perm[i]][0];
  }
  temp = expand(sub_inf);
  qf.m = expand(sub_mean);

printf("qf.m is\n");
Mp(qf.m);
  Mnull_decomp(Mt(temp),&not_inf_ortho,&inf_ortho); 
  /* inf_ortho is orthogonal basis for inf space in cl2 */
  /*  temp = Mcovariance(cl1->qf.U,cl1->qf.D); */
  if (cl1->qf.cov.cols == 0) temp = Msym_range_inv(cl1->qf.S,cl1->qf.fin);
  else temp = cl1->qf.cov; /* xxx */


  sup_fin = two_way_partition(temp,dim,cl1->num);
  for (i=0; i < cl2->num; i++)   for (j=0; j < cl2->num; j++) 
    sub_fin.sub[i][j] = sup_fin.sub[perm[i]][perm[j]];
  sigma = expand(sub_fin);
  P = Mm(inf_ortho,Mt(inf_ortho)); /* could do this with not_inf_ortho */
  P = Ms(Miden(inf_ortho.rows),P);
  sigma = Mm(Mm(P,sigma),P);  /* projection onto orth comp of inf_ortho */
  /* Msym_decomp(sigma,&qf.U,&qf.D); */
  /*printf("original matrix\n");
Mp(expand(sub_fin));
printf("finite part:\n");
Mp(qf.U);
Mp(qf.D); */
  /*  Mset_inf(&qf.U,&qf.D,inf_ortho); */

  qf.m = Mm(P,qf.m);
printf("qf.m is\n");
Mp(qf.m);
printf("inf_ortho is\n");
Mp(inf_ortho);
printf("P is\n");
Mp(P);
  qf.cov = sigma;
  /*  qf.S = Munset(); */
  qf.inf = inf_ortho;

  /*  reduce = Mm(Mm(Mt(not_inf_ortho),sigma),not_inf_ortho);
  Mnull_decomp(reduce,&(qf.null),&(qf.fin)); 
  qf.null = Mm(not_inf_ortho,qf.null);
  qf.fin = Mm(not_inf_ortho,qf.fin); */

  Mnull_decomp(sigma,&null_inf,&(qf.fin)); 
  Mint_comp(null_inf, qf.inf, &qf.inf, &qf.null);
  qf.S = Msym_range_inv(qf.cov,qf.fin);

  /*#ifdef DEBUG 
 compare_reps(qf); 
#endif */

  /*printf("hello\n");
Mp(Mm(sigma,qf.inf));*/


  QFcheck(qf);

  alt = new_marginalize(cl1, cl2);
  QFcompare(qf,alt);
  printf("loree\n");

  return(qf);
}  





static MATRIX 
make_basis(SUPERMAT m, int *bool, int d) {
     /* the cols of m are bases for spaces.  bool[i] = 1 means  the sub matrix
	was part of the basis for the original space. return basis for product space */
  MATRIX temp; 
  int i;


  temp = m.sub[0][0];
Mp(temp);
  for (i=1; i < d; i++) {
    if (bool[i]) temp = Mvcat(temp,m.sub[i][0]);
    else temp = Mdirect_sum(temp,m.sub[i][0]);
Mp(temp);
  }
  return(temp);
}


static QUAD_FORM
old_promote(CLIQUE *cl1, CLIQUE *cl2, QUAD_FORM q2) {
  /* cl1 is a clique containing cl2.  express the dist q2
     of cl2 in terms of the elements (and order) of cl1 */
  int i,j,dim1[MAX_CLIQUE],perm[MAX_CLIQUE],dim2[MAX_CLIQUE];
  QUAD_FORM qf,xxx;
  SUPERMAT D,U,m,D2,U2,m2,S,S2,null2,null1,fin1,fin2,inf1,inf2,infc;
  MATRIX t1,t2;

  if (q2.S.cols == 0) q2.S = Msym_range_inv(q2.cov,q2.fin);


  if (isnanf(q2.S.el[0][0]))
    exit(0);

  /*  U = Salloc(cl1->num,cl1->num);
  D = Salloc(cl1->num,cl1->num); */
  S = Salloc(cl1->num,cl1->num);
  infc = Salloc(cl1->num,cl1->num);
  m = Salloc(cl1->num,1);
  null1 = Salloc(cl1->num,1);
  fin1 = Salloc(cl1->num,1);
  inf1 = Salloc(cl1->num,1);
  for (i = 0; i < cl1->num; i++) dim1[i] = cl1->member[i]->dim;
  for (i = 0; i < cl1->num; i++) for (j = 0; j < cl1->num; j++) {
    /*    U.sub[i][j] = (i == j) ? Miden(dim1[i]) : Mzeros(dim1[i],dim1[j]);*/
    infc.sub[i][j] = (i == j) ? Miden(dim1[i]) : Mzeros(dim1[i],dim1[j]);
    /*    D.sub[i][j] = (i == j) ? Minf(dim1[i]) : Mzeros(dim1[i],dim1[j]); */
    S.sub[i][j] = Mzeros(dim1[i],dim1[j]);
  }
  for (i = 0; i < cl1->num; i++) m.sub[i][0] = Mzeros(dim1[i],1);
  for (i = 0; i < cl2->num; i++) dim2[i] = cl2->member[i]->dim;
  for (i=0; i < cl2->num; i++) perm[i] = clique_index(cl1, cl2->member[i]); 
  /* perm[i] is index in cl1 of ith member of cl2 */


  /*  D2 = two_way_partition(q2.D,dim2,cl2->num);
  U2 = two_way_partition(q2.U,dim2,cl2->num); */
  S2 = two_way_partition(q2.S,dim2,cl2->num);
  m2 = row_partition(q2.m,dim2,cl2->num);
  null2 = row_partition(q2.null,dim2,cl2->num);
  fin2 = row_partition(q2.fin,dim2,cl2->num);
  inf2 = row_partition(q2.inf,dim2,cl2->num);
  for (i=0; i < cl2->num; i++)   for (j=0; j < cl2->num; j++) {
    /*    D.sub[perm[i]][perm[j]] = D2.sub[i][j];
    U.sub[perm[i]][perm[j]] = U2.sub[i][j]; */
    S.sub[perm[i]][perm[j]] = S2.sub[i][j];
  }
  for (i=0; i < cl2->num; i++)   for (j=0; j < cl1->num; j++) 
    infc.sub[j][perm[i]].cols  = 0;
  for (i=0; i < cl1->num; i++) {
    null1.sub[i][0] = Mzeros(dim1[i],q2.null.cols);
    fin1.sub[i][0] = Mzeros(dim1[i],q2.fin.cols);
    inf1.sub[i][0] = Mzeros(dim1[i],q2.inf.cols);
  }
  for (i=0; i < cl2->num; i++) {
    m.sub[perm[i]][0] = m2.sub[i][0];
    null1.sub[perm[i]][0] = null2.sub[i][0];
    fin1.sub[perm[i]][0] = fin2.sub[i][0];
    inf1.sub[perm[i]][0] = inf2.sub[i][0];
  }
  qf.m = expand(m);
  qf.fin = expand(fin1);
  qf.inf = Mcat(expand(inf1),expand(infc));
  qf.null = expand(null1);
  /*  qf.U = expand(U);
  qf.D = expand(D); */
  qf.S = expand(S);
  /*  qf.cov = Munset();*/
  qf.cov = Msym_range_inv(qf.S,qf.fin);
  /*#ifdef DEBUG 
 compare_reps(qf); 
#endif */

  /* xxx = new_promote(cl1, cl2, q2); 
printf("this is xxx\n");
QFprint(xxx);
printf("this is qf\n");
QFprint(qf);
QFcompare(qf,xxx);
printf("okay here\n"); */
  return(qf);
}


/*static SUPER_QUAD
old_update_ratio(SUPER_QUAD old, SUPER_QUAD margin) {
  QUAD_FORM exp_old,exp_mar,sum;
  SUPER_QUAD ret;

  exp_old = SQtoQF(old);
  exp_mar = SQtoQF(margin);
  sum = QFadd(exp_mar, QFneg(exp_old));
  ret = QFtoSQ(sum,old);
  return(ret);
}*/


static QUAD_FORM
promote(CLIQUE *cl1, CLIQUE *cl2, QUAD_FORM q2) {
  QUAD_FORM qfo,qfn;

  qfo = old_promote(cl1,cl2,q2);
#ifdef SUPER_DEBUG  
  qfn = new_promote(cl1,cl2,q2);
  QFcompare(qfo,qfn);
#endif

  /*    qfn = promote_sep(cl1,cl2,q2);
	QFcompare(qfo,qfn); */

  return(qfo);
}



static QUAD_FORM
update_ratio(QUAD_FORM old, QUAD_FORM margin) {
  QUAD_FORM diff,sum;

  /*  sum = QFplus(margin, QFneg(old)); */
  if (old.inf.cols == old.inf.rows) return(margin);
  diff = QFminus(margin, old);
  /*  return(sum); */
    return(diff); 
}


oxox() {
  printf("hlh\n");
}

static void
check_clique_memory(CLIQUE_TREE ct) {
  /* the cliques prob distributions should be in permanent memory */
  int i;
  CLIQUE *cl;
  char *high,*min;
  QUAD_FORM qf;

  high = highest_address();
  for (i=0; i < ct.num; i++) {
    qf = ct.list[i]->clique->qf;
    min = (char *) &qf.cov.el[0][0];
    if ((char*) &qf.S.el[0][0] < min) min = (char*) &qf.S.el[0][0];
    if ((char*) &qf.inf.el[0][0] < min) min = (char*) &qf.inf.el[0][0];
    if ((char*) &qf.fin.el[0][0] < min) min = (char*) &qf.fin.el[0][0];
    if ((char*) &qf.null.el[0][0] < min) min =(char*)  &qf.null.el[0][0];
    if ((char*) &qf.m.el[0][0] < min) min = (char*) &qf.m.el[0][0];
    if (min < high) {
      printf("memory problem in permanent cliques \n");
      exit(0);
    }
  }
}

static void
check_quad_memory(QUAD_FORM qf) {
  /* the cliques prob distributions should be in permanent memory */
  int i;
  CLIQUE *cl;
  char *high,*min;

  high = highest_address();
  min = (char *) &qf.cov.el[0][0];
  if ((char*) &qf.S.el[0][0] < min) min = (char*) &qf.S.el[0][0];
  if ((char*) &qf.inf.el[0][0] < min) min = (char*) &qf.inf.el[0][0];
  if ((char*) &qf.fin.el[0][0] < min) min = (char*) &qf.fin.el[0][0];
  if ((char*) &qf.null.el[0][0] < min) min =(char*)  &qf.null.el[0][0];
  if ((char*) &qf.m.el[0][0] < min) min = (char*) &qf.m.el[0][0];
  if (min < high) {
    printf("memory problem in permanent cliques \n");
    exit(0);
  }
}







static void
flow_guts(CLIQUE *snd, CLIQUE *mid, CLIQUE *rcv, QUAD_FORM *newmid, QUAD_FORM *newrcv, QUAD_FORM *prommid,QUAD_FORM *prommarg , QUAD_FORM *summ) {
  /* flow from snd to rcv with mid a subclique of both */
  SUPER_QUAD urp,new;
  QUAD_FORM exp_urp,exp_rcv,sum,margin,ur,ur_prom,result,margin_prom,old_prom;
  int flag,i;
  extern int print_all;

  /*  if (cur_accomp == 39) {
  printf("entering flow_guts\n");
  getchar();
  getchar();
  }
  */

  /*  printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv); */

  /* if (snd->member[0]->mcs_num == 148) {
  printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  printf("send is:\n");
  QFprint(snd->qf);
  printf("rcv is:\n");
  QFprint(rcv->qf);
  printf("mid is:\n");
  QFprint(mid->qf);
  }
  */

  //CF:  not used
  if(global_flag) {
       printf("entering flow\n");
        printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  printf("snd on entering\n");
  QFprint(snd->qf);
  printf("rcv on entering\n");
  QFprint(rcv->qf);
  printf("\n\n");      
  }

#ifdef VERBOSE
  printf("entering flow\n");
  printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  printf("\n\n"); 
  printf("snd on entering\n");
  QFprint(snd->qf);
#endif

  /*  if (print_all) {
  printf("entering flow\n");
  printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  printf("\n\n"); 
  printf("snd on entering\n");
  QFprint(snd->qf);
  printf("rcv on entering\n");
  QFprint(rcv->qf);
  }*/



  margin = marginalize_sep(snd,mid);     //CF:  marginalise out the sender pdf, to the dims of the seperator

  /*  margin = new_marginalize(snd, mid);*/





#ifdef VERBOSE
  printf("margin is\n");
  QFprint(margin);
  printf("mid->qf is\n");
  QFprint(mid->qf);
#endif


  //CF:  for X->S->Y,  we will do:
//CF:   S*=marginalize X
//CF:   Y<- 
//CF:  

  margin_prom = promote_sep(rcv,mid,margin);      //CF: promote S*
  /*  margin_prom = promote(rcv,mid,margin); */


  old_prom = promote_sep(rcv,mid,mid->qf);        //CF:  promote S_old
  /*  old_prom = promote(rcv,mid,mid->qf); */

#ifdef VERBOSE
   printf("promoted mid is\n");
   QFprint(old_prom); 
   printf("promoted margin\n");
   QFprint(margin_prom);   
#endif


#ifdef VERBOSE
   printf("before anything added to rcv:\n");
   QFprint(rcv->qf); 
#endif

   //CF:  not used
   if (global_flag) {
     printf("before anything added to rcv:\n");
     QFprint(rcv->qf); 
   }

   sum = QFplus_sep(margin_prom,rcv->qf);  //CF:  Y times S*



   /*       if(global_flag) {
       printf("entering flow\n");
        printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  QFprint(margin_prom);
    QFprint(rcv->qf);
  printf("\n\n");     
  global_flag = 0;
  }
   */


   /*  sum = QFplus(margin_prom,rcv->qf);  */

#ifdef VERBOSE
  printf("after adding promoted marginal\n");
  QFprint(sum); 
#endif


  result = QFminus_sep(sum, old_prom);     //CF:   new Y divided by S_old 
  /*  result = QFminus(sum, old_prom);*/

  
#ifdef VERBOSE
  printf("after subtracting promoted mid\n");
  QFprint(result); 
#endif


#ifdef VERBOSE
  printf("after result is\n");
  QFprint(result);
#endif

  if (global_flag) {
    printf("after result is\n");
    QFprint(result); 
  }
  
  /*  if (rcv->member[0]->node_num == 26 && snd->member[0]->node_num == 2)  
    {  QFprint(sum); exit(0);} */

  /*printf("margin(5) = %x\n",margin.S.el[1]); */

  *newrcv = result;
  *newmid = margin;
  *prommid = old_prom;
  *prommarg = margin_prom;
  *summ = sum;



}





//CF:   note used?
FLOW_SPACE *
search_flow_tab(CLIQUE_NODE *send, CLIQUE_NODE *recv) {
  int i;
  CLIQUE sndvar,midvar,rcvvar;
  CLIQUE_BOND *bnd;
  FLOW_SPACE_TAB *tab;
  QUAD_FORM snd,sep,rcv;
  FLOW_SPACE *cur;

  for (i=0; i < send->neigh.num; i++) if (send->neigh.bond[i].ptr == recv) {
    snd = send->clique->qf;
    bnd = send->neigh.bond + i;
    rcv = bnd->ptr->clique->qf;
    sep = bnd->intersect->qf;
    tab = &(bnd->flow_tab); 
    break;
  }
  if (i == send->neigh.num) {
    printf("couldn't find connection in search_flow_tab\n");
    exit(0);
  }
  for (i=0; i < tab->num; i++) {
    cur = tab->space + i;
    if (Mspaces_equal(cur->old_snd.null,snd.null) == 0) continue;
    if (Mspaces_equal(cur->old_snd.inf,snd.inf) == 0) continue;
    if (Mspaces_equal(cur->old_snd.fin,snd.fin) == 0) continue;
    if (Mspaces_equal(cur->old_sep.null,sep.null) == 0) continue;
    if (Mspaces_equal(cur->old_sep.inf,sep.inf) == 0) continue;
    if (Mspaces_equal(cur->old_sep.fin,sep.fin) == 0) continue;
    if (Mspaces_equal(cur->old_rcv.null,rcv.null) == 0) continue;
    if (Mspaces_equal(cur->old_rcv.inf,rcv.inf) == 0) continue;
    if (Mspaces_equal(cur->old_rcv.fin,rcv.fin) == 0) continue;
    return(cur);
  }

  /*  if (send->clique->member[i]->node_num == 0) {
    printf("config:\n");
    QFprint(snd);
    QFprint(sep);
    QFprint(rcv);

    }*/

  return(NULL);
}

SPACE_DECOMP
perm_space_decomp(QUAD_FORM qf) {
  SPACE_DECOMP temp;

  temp.null = Mperm_id(qf.null);
  temp.inf = Mperm_id(qf.inf);
  temp.fin = Mperm_id(qf.fin);
  return(temp);
}


static void
add_flow_tab(CLIQUE_NODE *send, CLIQUE_NODE *recv, QUAD_FORM snd, 
	     QUAD_FORM sep,  QUAD_FORM rcv, QUAD_FORM newsep, 
	     QUAD_FORM newrcv) {
  int i,d;
  FLOW_SPACE_TAB *tab;
  CLIQUE_BOND *bnd;
  

  for (i=0; i < send->neigh.num; i++) if (send->neigh.bond[i].ptr == recv) {
    bnd = send->neigh.bond + i;
    tab = &(bnd->flow_tab); 
    break;
  }
  if (tab->num >= MAX_FLOW_SPACE) {
    printf("out of room in add_flow_tab\n");
    for (i=0; i < tab->num; i++) {
      printf("i = %d\n",i);
      Mp(tab->space[i].old_rcv.null);
      Mp(tab->space[i].old_rcv.inf);
      Mp(tab->space[i].old_rcv.fin);
      Mp(tab->space[i].old_sep.null);
      Mp(tab->space[i].old_sep.inf);
      Mp(tab->space[i].old_sep.fin);
      Mp(tab->space[i].old_snd.null);
      Mp(tab->space[i].old_snd.inf);
      Mp(tab->space[i].old_snd.fin);
    }
    exit(0);
  }
  i = (tab->num)++;
  tab->space[i].old_snd = perm_space_decomp(snd);
  tab->space[i].old_sep = perm_space_decomp(sep);
  tab->space[i].old_rcv = perm_space_decomp(rcv);
  tab->space[i].new_rcv = perm_space_decomp(newrcv);
  tab->space[i].new_sep = perm_space_decomp(newsep); 
}





static int
enforce_constraints(CLIQUE_NODE *cn) {
  int i,j,t=0;
  BNODE *mem;
  BNODE_CONSTRAINT *bnc;
  CLIQUE *clq;
  float val;
  
  clq = cn->clique;
  for (i=0; i < clq->num; i++) {
    mem = clq->member[i];
    bnc = mem->constraint;
    for (j=0; j < mem->dim; j++,t++) {
      if (bnc == NULL) continue;
      if (bnc[j].active == 0) continue;
      val = clq->qf.m.el[t][0];
      if (val >= bnc[j].lim) continue;
      printf("enforching constraing j = %d lim = %f val = %f type = %d\n",j,bnc[j].lim,val,mem->note_type);
      QFprint(clq->qf);
      exit(0);
      fix_clique_value(cn, t, bnc[j].lim);
    }
  }
}

//CF:  one msg passing action
static void
flow(CLIQUE_NODE *send, CLIQUE_NODE *recv) {
  /* flow from snd to rcv with mid a subclique of both */
  QUAD_FORM newmid,newrcv,newmidvar,newrcvvar,prommid,prommidvar,prommarg,prommargvar,sum,sumvar;
  int i;
  CLIQUE *snd, *mid, *rcv,sndvar,midvar,rcvvar;
  CLIQUE_BOND *bnd;
  FLOW_SPACE *space;
  CLIQUE_NODE *middle;

  //  printf("entering flow\n");

  //CF:  find the seperator between sender and receiver cliques
  for (i=0; i < send->neigh.num; i++) if (send->neigh.bond[i].ptr == recv) {
    snd = send->clique;
    bnd = send->neigh.bond + i;  //CF:  pointer to the CLIQUE_BOND 
    rcv = bnd->ptr->clique;
    middle = bnd->ptr;    //CF:  should equal recv??     not used?
    mid = bnd->intersect; //CF:  pointer to the seperator CLIQUE of the CLIQUE_BOND
    bnd->dry = 0; 
    break;
  }

  /*    space = search_flow_tab(send,recv); 
    if (send->clique->member[i]->node_num == 0) {
      printf("space = %d\n",space);
      for (i=0; i < send->neigh.num; i++) printf("num = %d\n",send->neigh.bond[i].flow_tab.num);
    } */

  //CF:  prom=promoted (ie extended to higher-dims)
  flow_guts(snd,mid,rcv,&newmid,&newrcv,&prommid,&prommarg,&sum);     //CF:  MEAT

//CF:  not usually used
#ifdef SUPER_DEBUG
  sndvar = *snd;
  sndvar.qf = nondegenerate(snd->qf);
  rcvvar = *rcv;
  rcvvar.qf = nondegenerate(rcv->qf);
  midvar = *mid;
  midvar.qf = nondegenerate(mid->qf);

  flow_guts(&sndvar,&midvar,&rcvvar,&newmidvar,&newrcvvar,&prommidvar,&prommargvar,&sumvar); 

  if (approx_equal(prommarg,prommargvar) == 0) {
    printf("prommargs don't appear to be equal\n");
    exit(0);
  }
  if (approx_equal(sum,sumvar) == 0) {
    printf("sums don't appear to be equal\n");
    exit(0);
  }
  if (approx_equal(prommid,prommidvar) == 0) {
    printf("prommids don't appear to be equal\n");
    exit(0);
  }
  if (approx_equal(newmid,newmidvar) == 0) {
    printf("mids don't appear to be equal\n");
    exit(0);
  }
  if (approx_equal(newrcv,newrcvvar) == 0) {
    printf("rcvs don't appear to be equal\n");
    exit(0);
  }
#endif  

  /*    if (space == 0) add_flow_tab(send,recv,snd->qf,mid->qf,rcv->qf,newmid,newrcv);  */

  //CF:  all of the above was done in temp memory; copy to perm memory
  QFcopy(newrcv, &(rcv->qf));  
  QFcopy(newmid, &(mid->qf));    

#ifdef CONSTRAINT_EXPERIMENT
  enforce_constraints(recv);
  enforce_constraints(middle);
#endif

  /*  QFcopy(newrcvvar, &(rcv->qf));  
  QFcopy(newmidvar, &(mid->qf));      */

  clear_matrix_buff();
  //  printf("leaving flow\n");
}





recent_flow(CLIQUE_NODE *send, CLIQUE_NODE *recv) {
  /* flow from snd to rcv with mid a subclique of both */
  SUPER_QUAD urp,new;
  QUAD_FORM exp_urp,exp_rcv,sum,margin,ur,ur_prom,result,margin_prom,old_prom;
  int flag,i;
  CLIQUE *snd, *mid, *rcv;
  CLIQUE_BOND *bnd;

  for (i=0; i < send->neigh.num; i++) if (send->neigh.bond[i].ptr == recv) {
    snd = send->clique;
    bnd = send->neigh.bond + i;
    rcv = bnd->ptr->clique;
    mid = bnd->intersect;
    bnd->dry = 0; 
    break;
  }
    
    

  /*check_quad_memory(rcv->qf);
check_quad_memory(mid->qf);
check_quad_memory(snd->qf); */


#ifdef VERBOSE
  printf("entering flow\n");
  printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  printf("\n\n"); 
  printf("snd on entering\n");
  QFprint(snd->qf);
#endif


  margin = new_marginalize(snd, mid);

#ifdef VERBOSE
  printf("margin is\n");
  QFprint(margin);
  printf("mid->qf is\n");
  QFprint(mid->qf);
#endif

  margin_prom = promote(rcv,mid,margin); 
  old_prom = promote(rcv,mid,mid->qf); 

#ifdef VERBOSE
   printf("promoted mid is\n");
   QFprint(old_prom); 
   printf("promoted margin\n");
   QFprint(margin_prom);   
#endif


#ifdef VERBOSE
   printf("before anything added to rcv:\n");
   QFprint(rcv->qf); 
#endif



  result = QFplus(margin_prom,rcv->qf);  

#ifdef VERBOSE
  printf("after adding promoted marginal\n");
  QFprint(result); 
#endif


  result = QFminus(result, old_prom);
  
#ifdef VERBOSE
  printf("after subtracting promoted mid\n");
  QFprint(result); 
#endif


  QFcopy(result, &(rcv->qf));  
  QFcopy(margin, &(mid->qf));   
/*  QFcopy(result, new_rcv);
  QFcopy(margin, new_mid); */


#ifdef VERBOSE
  printf("after result is\n");
  QFprint(result);
#endif
 
  clear_matrix_buff();
}


static void
old_flow(CLIQUE_NODE *send, CLIQUE_NODE *recv) {
  /* flow from snd to rcv with mid a subclique of both */
  SUPER_QUAD urp,new;
  QUAD_FORM exp_urp,exp_rcv,sum,margin,ur,ur_prom,result;
  int flag,i;
  CLIQUE *snd, *mid, *rcv;
  CLIQUE_BOND *bnd;

  for (i=0; i < send->neigh.num; i++) if (send->neigh.bond[i].ptr == recv) {
    snd = send->clique;
    bnd = send->neigh.bond + i;
    rcv = bnd->ptr->clique;
    mid = bnd->intersect;
    bnd->dry = 0; 
    break;
  }
    
    

  /*check_quad_memory(rcv->qf);
check_quad_memory(mid->qf);
check_quad_memory(snd->qf); */


#ifdef VERBOSE
  printf("entering flow\n");
  printf("send to receive:\n");
  print_clique(snd);
  print_clique(rcv);
  printf("\n\n"); 
  printf("snd on entering\n");
  QFprint(snd->qf);
#endif


  margin = new_marginalize(snd, mid);

#ifdef VERBOSE
  printf("margin is\n");
  QFprint(margin);
  printf("mid->qf is\n");
  QFprint(mid->qf);
#endif

   ur =  update_ratio(mid->qf,margin);

#ifdef VERBOSE
   printf("mid_qf is\n");
   QFprint(mid->qf); 
   printf("update ratio\n");
   QFprint(ur);   
#endif





  ur_prom = promote(rcv,mid,ur); 

#ifdef VERBOSE
  printf("promted marginal is\n");
  QFprint(ur_prom); 
  printf("before result is\n");
  QFprint(rcv->qf);
#endif


  result = QFplus(ur_prom,rcv->qf);  


  QFcopy(result, &(rcv->qf));  
  QFcopy(margin, &(mid->qf));  

#ifdef VERBOSE
  printf("after result is\n");
  QFprint(rcv->qf);
#endif
 
  clear_matrix_buff();
}



/*static void
high_flow(CLIQUE_NODE *orig, int dest) {
  CLIQUE *mid;
  CLIQUE_NODE *son;
  CLIQUE_BOND *bnd;
  
  bnd = orig->neigh.bond + dest;
  son = bnd->ptr;
  mid = bnd->intersect;
  flow(orig->clique,mid,son->clique);   
  bnd->dry = 0;
}


static void
local_equilib(CLIQUE_NODE *node) {
  int i,j;
  CLIQUE_NODE *next,*prev;
  CLIQUE_BOND *bnd;
  
  for (i = 0; i < node->neigh.num; i++) {
    next = node->neigh.bond[i].ptr;
    for (j = 0; j < next->neigh.num; j++) { 
      bnd = next->neigh.bond + j;
      prev = bnd->ptr;
      if (prev == node && bnd->dry) {
	local_equilib(prev);
	high_flow(prev,j);
      }
    }
  }
}	
	
*/

//CF:  recurisvely move through the tree; grampa is passed to prevent us visiting the way we came from.
//CF:  given a dad, we visit all its sons but not the grampa.
static void
collect(CLIQUE_NODE *dad, CLIQUE_NODE *gramp) {
  int i,j,k,dry;
  CLIQUE_NODE *son;
  static int depth;
  float t1,t2;

  //  t1 = now();
  depth++;
  /*  printf("collect depth = %d dad = %d\n",depth,dad);
      printf("collect depth = %d sons = %d\n",depth,dad->neigh.num); */
/*  QUAD_FORM new_mid,new_rcv;

  new_rcv = QFperm(dad->clique->qf);
  new_mid = QFperm(dad->clique->qf); */
  for (i = 0; i < dad->neigh.num; i++) {  //CF:  for each son
    //    printf("son number %d\n",i);
   son = dad->neigh.bond[i].ptr;

    //CF:  not important: (check for symettrical pointers)
    for (j = 0; j < son->neigh.num; j++) 
      if (son->neigh.bond[j].ptr == dad) break;
    if (j == son->neigh.num) {
      printf("asymetrical pointers in collect\n");
      exit(0);
    }
    
    /*    dry = dad->neigh.bond[i].dry;*/
    dry = son->neigh.bond[j].dry;
    if (son != gramp && dry) {
      collect(son,dad);                //CF:  recurse...
      //      printf("flow %d %d\n",son,dad);
      flow(son,dad);                   //CF:  ...then the action   (depth first traversal)   ;THE MESSAGE PASSING ACTION!
      //      printf("returned from flow in collect\n");
/*      flow(son,dad);   
      QFcompare(new_rcv,dad->clique->qf);*/
    }
  }

  depth--;
  
}

static void
disseminate(CLIQUE_NODE *dad, CLIQUE_NODE *gramp) {
  int i,dry;
  CLIQUE_NODE *son;
  /*  QUAD_FORM new_mid,new_rcv;*/

  for (i = 0; i < dad->neigh.num; i++) {
    son = dad->neigh.bond[i].ptr;
    /*    new_rcv = QFperm(son->clique->qf);
    new_mid = QFperm(son->clique->qf); */
    dry = dad->neigh.bond[i].dry;
    if (son != gramp && dry) {
      flow(dad,son);
      /*      QFcompare(new_rcv,son->clique->qf); */
      disseminate(son,dad);
    }
  }
}

static void
all_dry(CLIQUE_TREE ct) {
  int i,j;

  for (i=0; i < ct.num; i++) 
    for (j=0; j < ct.list[i]->neigh.num; j++)
      ct.list[i]->neigh.bond[j].dry = 1;
}


//CF:  run a complete round of msg passing
static void
equilibrium(CLIQUE_TREE ct) {
  int i;

  if (ct.root == NULL) { printf("calling equilibrium with null root\n"); exit(0); }
  all_dry(ct);  //CF:  make everything dry
  /*  equilb(ct.root,0); */
  /*printf("root distribution before collect\n");
  QFprint(ct.root->clique->qf); */
  /*printf("entering collect\n");*/
  /*  collect(ct.list[0],0);*/
  collect(ct.root,0);                        //CF:  head-recurse from the root, depth-first, flow towards root
  /*printf("entering diseminate\n");*/
/*for (i=0; i < ct.root->neigh.num; i++) {
  printf("i = %d\n",i);
  printf("clique\n");
  QFprint(ct.root->clique->qf);
  printf("mid\n");
  QFprint(ct.root->neigh.bond[i].intersect->qf);
}
exit(0); */

  /*  disseminate(ct.list[0],0); */
  disseminate(ct.root,0);   //CF:  tail-recurision to flow outwards from root
}
  

static void
add_constraints(CLIQUE_TREE ct, BELIEF_NET bn) {
  int i,k,j,l;
  CLIQUE c,*cl;
  QUAD_FORM qf,temp;
  float pos;
  BNODE *b;

  for (i = 0; i < bn.num; i++) {
    b = bn.list[i];
    if (b->obs.observed) {  
      pos = bn.list[i]->obs.pos;
      c.num = 0;
      add_to_clique(&c,b); /* clique contains only 1 element */
      for (k=0; k < ct.num; k++) { 
	cl =  ct.list[k]->clique;
	if (is_subset(&c,cl))  break;
      }
      if (k == ct.num) printf("problem in add_constraints()\n");
      else {
	qf = QFobs(b->dim, POS_INDEX, pos);
	/*	qf.m = Mzeros(b->dim,1);
	qf.m.el[POS_INDEX][0] = pos;
	qf.U = Miden(b->dim);
	qf.D = Mzeros(b->dim,b->dim);
	qf.cov = Mzeros(b->dim,b->dim);
	qf.S = Munset();
	qf.null = Mzeros(b->dim,b->dim-1);
	for (j=0; j < b->dim; j++) {
	  qf.D.el[j][j] = (j == POS_INDEX) ? 0 : HUGE_VAL;
	} */
	qf = promote(cl,&c,qf); 
	/*			printf("before plus:\n");
			QFprint(cl->qf);	
printf("adding:\n");
QFprint(qf);	     */
	QFcopy(QFplus(cl->qf,qf),&(cl->qf));
	clear_matrix_buff();

	/*		printf("after QFplus:\n");
if (cl->qf.cov.rows > 100) exit(0);
QFprint(cl->qf);	  */
/*exit(0); */
      }
    }
  }
}
      
static void 
spr_dr(CLIQUE_NODE *node, CLIQUE_NODE *dad) {
  int i;
  CLIQUE_NODE *next;
 
  /*  printf("have %d neighs\n",node->neigh.num);*/
  for (i=0; i < node->neigh.num; i++) {
    next = node->neigh.bond[i].ptr;
    if (next != dad) {
      node->neigh.bond[i].dry = 1;
       /*  print_clique(node->neigh.bond[i].ptr->clique);*/
      spr_dr(next,node);
    }
  }
}

static void 
spread_drought(CLIQUE_NODE *node) {
  spr_dr(node,node);
}


 
static void      
fix_solo_obs(int n, float val) {
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf,sum;

  /*  printf("note %d fixed at %f\n",n,val);
      exit(0); */

  bn = score.solo.note[n].belief;
  cl = bn->clnode->clique; 
  c.num = 0;
  add_to_clique(&c,bn); /* clique contains only 1 element */
  qf = QFobs(bn->dim, POS_INDEX, val);
  /*qf = QFpos_near(bn->dim);
qf.m.el[0][0] = val;     */
  qf = promote(cl,&c,qf); 
  sum = QFplus(cl->qf,qf);
  QFcopy(sum,&(cl->qf));
  spread_drought(bn->clnode);
}





/*static   collect(score.midi.burst[0].belief->clnode,0);  */
  

static QUAD_FORM 
post_dist(BNODE *node) {
  CLIQUE c;
  QUAD_FORM dist;

  c.num = 0;
  add_to_clique(&c,node); /* clique contains only 1 element */

  /*  printf("node is %d\n",node->node_num);
  print_clique(node->clique);

  QFprint(node->clique->qf); */
  dist = new_marginalize(node->clnode->clique, &c); /*chris */   //CF:  TODO: why not use QFmarg? (see comment inside fn)
  return(dist);
}

static void      
fix_accom_obs(int n, float val) {
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;


  bn = score.midi.burst[n].belief;
  cl = bn->clnode->clique; 
  c.num = 0;
  add_to_clique(&c,bn); /* clique contains only 1 element */
  qf = QFobs(bn->dim, POS_INDEX, val);
  qf = promote(cl,&c,qf); 
  QFcopy(QFplus(cl->qf,qf),&(cl->qf));
  spread_drought(bn->clnode);
}

//CF:  
static void
get_accom_mean(int n, float *m, float *v) {
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;

  bn = score.midi.burst[n].belief;   //CF:  belief node for pending acc event
  qf = post_dist(bn);                //CF:  posteior dist (from marginalising its clique)
  if (qf.m.rows > 1) {
    printf("problem in get_accom_mean()\n");
    exit(0);
  }

  *m = qf.m.el[0][0];
  //CF:  it may the case that the pending acc note is the start of a new phrase (ie at at cue point)
  //CF:  in this case, we have no infomration about its time, its a flat posterior, so report var=HUGE_VAL to indicate this.
  if (qf.inf.cols > 0) *v = HUGE_VAL;
  else *v = qf.cov.el[0][0];
}

  
void
get_solo_mean(int n, float *m, float *v) {
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;

#ifdef PARALLEL_GRAPH_EXPERIMENT  
  collect(score.solo.note[n].belief->clnode,0); 
#endif

  bn = score.solo.note[n].observe;
  if (bn == NULL)  { printf("trying to compute with null belief_node\n");   exit(0); }
  qf = post_dist(bn);
  if (qf.m.rows > 1) {
    printf("problem in get_accom_mean()\n");
    exit(0);
  }
  *m = qf.m.el[0][0];
  if (qf.inf.cols > 0) *v = HUGE_VAL;
  else *v = qf.cov.el[0][0];
}

  
static void
get_hidden_solo_mean(int n, MATRIX *m, MATRIX *v) {
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;

  bn = score.solo.note[n].hidden;
  qf = post_dist(bn);
  *m = qf.m;
  *v = qf.cov;
}

  


void
fix_solo_simulations() {
  int i;
  BNODE *bn;
  float val;

  for (i=0; i < score.solo.num; i++) {
    bn = score.solo.note[i].belief;
    val = bn->obs.value.el[0][0];
    fix_solo_obs(i,val);
  }
}

//CF:  
void recompute_accomp(i) {
  /*  float x,y;
      float now();*/
  /* recomputes the time of the ith accomp node */
  /*  x = now();*/
    collect(score.midi.burst[i].belief->clnode,0);     //CF:  find the clique correspondoing to next accomp score event
    /*  y = now(); */
    /*  printf("note %d took %f\n",i,y-x); */
}

void recompute_hidden_accomp(int i, MATRIX *m, MATRIX *v) {
  /* recomputes the time of the hidden parent of ith accomp node */
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;

  collect(score.midi.burst[i].hidden->clnode,0);   
  bn = score.midi.burst[i].hidden;
  qf = post_dist(bn);


  *m = qf.m;
  *v = qf.cov;
  /*  if (qf.inf.cols > 0) {
    *v = Mzeros(qf.m.rows,qf.m.rows);
    *m = Mzeros(qf.m.rows,1);
    }*/
}

void recompute_hidden_accomp_var(int i) {
  /* recomputes the time of the hidden parent of ith accomp node */
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;

  collect(score.midi.burst[i].hidden->clnode,0);   
  bn = score.midi.burst[i].hidden;
  qf = post_dist(bn);
  QFprint(qf);
}

void recompute_catch(int i, float *m, float*v) {
  BNODE *bn;
  CLIQUE *cl,c;
  QUAD_FORM qf;

  collect(score.midi.burst[i].catchit->clnode,0);   
  bn = score.midi.burst[i].catchit;
  qf = post_dist(bn);
  *m = qf.m.el[0][0];
  *v = qf.cov.el[0][0];
}

   
void recompute_solo(i) {
  /* recomputes the time of the ith solo node */
    collect(score.solo.note[i].observe->clnode,0);   
}


//CF:  fix an onset observation into the BN, and spread drought
static void      
fix_obs(BNODE *bn, float val) {
  CLIQUE *cl,c;
  QUAD_FORM qf,sum;

  /*#ifdef ROMEO_JULIET
  //  printf("recomputing dist of solo note %d\n",cur_note);
  //  recompute_solo(cur_note); 
  #endif*/


  cl = bn->clnode->clique;   //CF:  BNODE stores pointer to its clique
  c.num = 0;
  add_to_clique(&c,bn); /* clique contains only 1 element */
  qf = QFobs(bn->dim, POS_INDEX, val);   //CF:  make a point in 1D (to be promoted later) (bn->dim is always 1 for obs nodes)
  qf = promote(cl,&c,qf);                //CF:  promote it to dimension of its clique
  sum = QFplus(cl->qf,qf);               //CF:  multiply the observation into the clique joint potential

  /*  printf("observation\n");
  QFprint(qf);
  printf("current\n");
  QFprint(cl->qf); */
  /*  printf("result\n");
      QFprint(cl->qf);*/
  QFcopy(sum,&(cl->qf));                 //CF:  copy into perm memory
  spread_drought(bn->clnode);    //CF:  recursively propogate dryness away from this clique (not msg passing, just dry bits)
}


static void      
fix_bn_value(BNODE *bn, int component, float val) {
  CLIQUE *cl,c;
  QUAD_FORM qf,sum;

  cl = bn->clnode->clique; 
  c.num = 0;
  add_to_clique(&c,bn); /* clique contains only 1 element */
  qf = QFobs(bn->dim, component, val);
  qf = promote(cl,&c,qf); 
  sum = QFplus(cl->qf,qf);
  QFcopy(sum,&(cl->qf));
  spread_drought(bn->clnode);
}

static void      
fix_clique_value(CLIQUE_NODE *clnode, int component, float val) {
  CLIQUE *cl,c;
  QUAD_FORM qf,sum;

  cl = clnode->clique; 
  qf = QFobs(cl->qf.m.rows, component, val);
  sum = QFplus(cl->qf,qf);
  QFcopy(sum,&(cl->qf));
  spread_drought(clnode);
}


//CF:  just a history record of when notes were scheduled
static void
add_sched(SCHEDULE_LIST *sched, float exec, float set) {
  float now();

  /*  printf("now = %f\n",now());*/
  if (sched->num == MAX_SCHEDULE) return;
  sched->exec_secs[sched->num] = exec;
  sched->set_secs[sched->num] = set;
  sched->num++;
}



static void
queue_midi_if_needed() {
  float m,v;
  int gate,gate_passed;

  get_accom_mean(cur_accomp,&m,&v);    //CF:  get the mean and variance times for the pending note (from its clique)
  gate = score.midi.burst[cur_accomp].gate;
  gate_passed = (cur_note >  gate);
  if ((v != HUGE_VAL) && (gate_passed)) {
    queue_event();  
  }
}


//CF:  recompute time of next pending (cur_accomp) accomp note only, from new observation
void
recalc_next_accomp() {
  float m,v,now(),ideal,time,mc,vc,t1,t2;
  MATRIX mu,var;
  int coincides,gate_passed,gate,n;
  QUAD_FORM qf;

  /*    global_flag = 1; */

  if (cur_accomp > last_accomp) return;


  
  
  recompute_accomp(cur_accomp);  //CF:  MEAT ***

  /*    printf("hidden solo var: %d\n",cur_note);
	Mp(score.solo.note[cur_note].hidden->clnode->clique->qf.cov); */

  //  get_hidden_solo_mean(cur_note, &mu,&var);
  /*  get_solo_mean(cur_note,&m,&v);
      printf("hidden solo mean for %s at %f (%f)\n",score.solo.note[cur_note].observable_tag,score.solo.note[cur_note].off_line_secs,m); */
  //  Mp(mu);

  //      Mp(score.solo.note[cur_note].hidden->clnode->clique->qf.m); 


  /*    printf("solo note %s mcs = %d\n",score.solo.note[69].observable_tag,score.solo.note[69].hidden->mcs_num);
    print_clique(score.solo.note[69].hidden->clnode->clique);
       Mp(score.solo.note[69].hidden->clnode->clique->qf.m);
       exit(0); */
  get_accom_mean(cur_accomp,&m,&v);    //CF:  get the mean and variance times for the pending note (from its clique)
  
  

  //CF:  for debugging
  recompute_hidden_accomp(cur_accomp,&mu,&var);     
  printf("recalc: cur_accomp = %s mean = %f  var = %f tempo = %f\n",score.midi.burst[cur_accomp].observable_tag,m,v,mu.el[1][0]);  

  if (v != HUGE_VAL && mu.el[1][0] < 0.) {
    printf("negative tempo: prepare for disater in midi mode\n");
    Mp(mu);
    Mp(var);
    //    exit(0); 
  }
  /*              printf("accomp = %d %s\n",cur_accomp,score.midi.burst[cur_accomp].observable_tag);
	          	  printf("mean is:\n");
			  Mp(mu);  

	          	  printf("var is:\n");
			  Mp(var);     */
  
#ifdef GRADUAL_CATCH_UP_EXPERIMENT
    recompute_catch(cur_accomp,&mc,&vc);
    //    printf("cur_accomp = %s catch mean = %f var = %f\n",score.midi.burst[cur_accomp].observable_tag,mc,vc);
#endif
  //               printf("cur_accomp = %s m = %f var = %f\n",score.midi.burst[cur_accomp].observable_tag,m,v);    
  //                printf("cur_accomp = %s m = %f var = %f\n",score.midi.burst[cur_accomp].observable_tag,m,v);    
  coincides =  score.midi.burst[cur_accomp].coincides; //CF:   not used
  /*   if (v != HUGE_VAL) ideal = score.midi.burst[cur_accomp].ideal_secs = (coincides) ? m +  0*sqrt(v) : m; */

  if (v != HUGE_VAL) {
    ideal = score.midi.burst[cur_accomp].ideal_secs =  m;    //CF:  ideal time for pending note to get played
 //   add_sched(&score.midi.burst[cur_accomp].schedule, ideal, now());  //CF:  for MIDI; schedule note
  }
  else score.midi.burst[cur_accomp].ideal_secs = HUGE_VAL;   //CF:  if no info about acc time, log it as time inf (will change later)


  time = score.midi.burst[cur_accomp].time;    //CF:  not used
  /*    if (fabs(score.midi.burst[cur_accomp].time - 18.) < .001)  */
  /*         printf("setting note at %f for %f (var = %f) at %f \n",time,m,v,now());     */




  /*if (cur_accomp == 5) printf("m = %f\n",m);*/

  /*  recompute_hidden_accomp(cur_accomp,&mu,&var);
  printf("accomp = %d\n",cur_accomp);
  printf("mean and var are:\n");
  Mp(mu);
  Mp(var);  
  */

  /*       printf("for accomp %d m = %f v = %f now is %f queing = %d\n",cur_accomp,m,v,now(),(v!=HUGE_VAL));  */


  //    printf("note = %d\n",score.solo.note[cur_accomp].num); 

  //CF:  accompaniment gates (see docs)
  gate = score.midi.burst[cur_accomp].gate;
  gate_passed = (cur_note >  gate);

#ifdef ORCHESTRA_EXPERIMENT   //CF:  always true when playing with audio orchestra
  if (midi_accomp == 0) {
    if (v != HUGE_VAL) plan_orchestra(gate_passed && v != HUGE_VAL);     //CF:  tweaks the voocder rate ***
    else set_netural_vocoder_rate(); // given no knowledge, at least play orchestra at natural rate
  }

#endif    

  /*   printf("cur_note = %d mid  = %d gate = %d can play %s? %d  gate = %s\n",cur_note,cur_accomp,gate,score.midi.burst[cur_accomp].observable_tag,gate_passed,score.solo.note[gate].observable_tag);*/

    //  if (gate_passed == 0) printf("holding up on queing %s (cur_note = %s)\n",score.midi.burst[cur_accomp].observable_tag,score.solo.note[cur_note].observable_tag);


  if ((v != HUGE_VAL) && (gate_passed)) {
    if (midi_accomp) queue_event(); //CF:  if MIDI orchestra, schedule the pending MIDI event  
    }

  /*   if (score.midi.burst[cur_accomp].time == 45.) {
     printf("accomp note %d m = %f v = %f\n",cur_accomp,m,v); 
     Mp(score.solo.note[18].hidden->clnode->clique->qf.m);
     Mp(score.solo.note[18].hidden->clnode->clique->qf.cov);
     } */

       /*     Mp(score.solo.note[17].hidden->clnode->clique->qf.m);
     Mp(score.solo.note[18].hidden->clnode->clique->qf.m);*/
     /*     Mp(score.solo.note[18].hidden->clnode->clique->qf.cov); */


}




static void
make_belief_current(BNODE *bn, float val) {
  float t1,t2;

  
  /*  printf("before anything backbone %d has:\n",cur_accomp);
      recompute_hidden_accomp_var(cur_accomp);     */

  fix_obs(bn,val);        //CF:  incorporate observation into clique potential (ie make its a dimension into a spike)
  //  t1 = now();
  recalc_next_accomp();	  //CF:  runs msg passings, and tweaks vocoder rate (or schedule MIDI events)
  

}


static UPDATE_QUEUE uq;
static int belief_blocked=0;

//CF:  uq is the update queue
void
flush_belief_queue() {
  int i;
  float now(),t1,t2;

  //CF:  semaphor locking mechanism -- in case 2 obs arrive at once and callback
  /*  if (cur_accomp == 5) printf("flushing at %f\n",now()); */
  if (belief_blocked) {
    printf("a collsion avoided\n");
    return;
  }
  belief_blocked = 1;
  /*  for (i=0; i < uq.num; i++) {
    make_belief_current(uq.update[i].bn,uq.update[i].val);
    }*/


  while (uq.num > 0) {  //CF:  while things in queue left to do...
    
  
    make_belief_current(uq.update[0].bn,uq.update[0].val);  //CF:  run msg passing from this clique to cur_accomp's clique***
  
    for (i=1; i < uq.num; i++) uq.update[i-1] = uq.update[i]; //CF:  remove completetion action from queue
    uq.num--;
  }
  uq.num = 0;  /* won't be necessary */
  belief_blocked = 0; //CF:  unlock semaphor
}


//CF:  A request to run the BBN, when a note has been observed.  
//CF:  (add a update request to the belief queue; then flush the queue)
static void
add_belief_queue(BNODE *bn, float val, int flush) {
  float now();

  /*  if (cur_accomp == 5) printf("add_belief_queue at %f\n",now());*/
  if (uq.num == MAX_BELIEF_QUEUE) {
    printf("out of room in add_belief_queue()\n");
    exit(0);
  }
  uq.update[uq.num].bn = bn;
  uq.update[uq.num].val = val;
  uq.num++;

  if (flush) flush_belief_queue();   //CF:  do the computation
}
 

    
  
  
static void 
hello() {
  printf("hello\n");
}


void
write_estimates(BELIEF_NET bn) {
  int i,d;
  FILE *fpo,*fps,*fpa;
  BNODE *b;
  
  fpo = fopen("observations.dat","w");
  fps = fopen("solo.dat","w");
  fpa = fopen("accomp.dat","w");
  for (i=0; i < bn.num; i++) {
    b = bn.list[i];
    d = b->index;
    if (b->note_type == OBS_NODE) 
      fprintf(fpo,"%f %f\n",score.solo.note[d].time,post_dist(b).m.el[0][0]);
    else if (b->note_type == SOLO_NODE) 
      fprintf(fps,"%f %f\n",score.solo.note[d].time, post_dist(b).m.el[0][0]);
    else if (b->note_type == /*ANCHOR_NODE*//*PHANTOM_NODE*/ACCOM_OBS_NODE/*INTERIOR_NODE*/) 
      fprintf(fpa,"%f %f\n",score.midi.burst[d].time, post_dist(b).m.el[0][0]);
  }
  fclose(fpo);
  fclose(fps);
  fclose(fpa);
}

void
write_simulation_estimates(BELIEF_NET bn) {
  int i,d;
  FILE *fpo,*fps,*fpa;
  BNODE *b;
  
  fpo = fopen("observations.dat","w");
  fps = fopen("solo.dat","w");
  fpa = fopen("accomp.dat","w");
  for (i=0; i < bn.num; i++) {
    b = bn.list[i];
    d = bn.list[i]->index;
    if (b->note_type == OBS_NODE) 
      fprintf(fpo,"%f %f\n",score.solo.note[d].time,b->obs.value.el[0][0]);
    else if (b->note_type == SOLO_NODE) 
      fprintf(fps,"%f %f\n",score.solo.note[d].time,b->obs.value.el[0][0]);
    /*           else if (b->note_type == INTERIOR_NODE || b->note_type == PHANTOM_CHILD_NODE|| b->note_type == ANCHOR_NODE)  */
              else if (b->note_type == ACCOM_OBS_NODE )  
                    fprintf(fpa,"%f %f\n",score.midi.burst[d].time,b->obs.value.el[0][0]); 
  }
  fclose(fpo);
  fclose(fps);
  fclose(fpa);
}

#define TEX_FILE "pic.tex"
static FILE *tex_fp;

#define INIT_WIDTH (1./16.)

static void
type2string(int type, char *c) {
  if (type == INTERIOR_NODE) strcpy(c,"int");
  if (type == SOLO_NODE) strcpy(c,"solo");
  if (type == OBS_NODE) strcpy(c,"obs");
  if (type == ANCHOR_NODE) strcpy(c,"anch");
  if (type == PHANTOM_NODE) strcpy(c,"phan");
  if (type == ACCOM_OBS_NODE) strcpy(c,"accom");
}

void
tex_attrib(BNODE *b, float *time, int *type) {
  int d;

  d = b->index;
  *type = b->note_type;
  
  if (*type == STRAGGLE_ACCOM_NODE) *type = INTERIOR_NODE;
  if (*type == SOLO_NODE || *type == OBS_NODE || 
      *type == PHANTOM_NODE || *type == SOLO_PHRASE_START) 
    *time = score.solo.note[d].time;
  else
    *time = score.midi.burst[d].time;
  /*  if (*type == ACCOM_INIT_NODE) {
    printf("was here\n");
      *time -= INIT_WIDTH;
    *type = ANCHOR_NODE; 
  } */
  /*  *time += INIT_WIDTH; */
}

void
draw_arc(float x1, float y1, float x2, float y2, float rad, 
	 float vexp, float hexp) {
  float dx,dy,norm;
  
  dx = x2-x1;
  dy = y2-y1;
  norm = sqrt(dx*dx+dy*dy);
  dx /= norm;
  dy /= norm;
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*x1 + rad*dx,(hexp*y1 + rad*dy));
  fprintf(tex_fp,"\\avec(%3.3f %3.3f)\n",vexp*x2-rad*dx,(hexp*y2 - rad*dy));
}


void
fig_print_matrix(float x, float y, MATRIX A) {
  int i,j;

  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",x,y);
  /*  fprintf(tex_fp,"\\htext{$\\tiny ( \\begin{array}{ll} 1 2 \\\\ 3 4 \\end{array} )$}\n"); */
  fprintf(tex_fp,"\\htext{$\\tiny ( \\begin{array}{");
  for (j = 0; j < A.cols; j++) 
    fprintf(tex_fp,"l");
  fprintf(tex_fp,"}"); 
  for (i = 0; i < A.rows; i++) {
    for (j = 0; j < A.cols; j++)  { 
      fprintf(tex_fp,"%3.2f",A.el[i][j]); 
      if (j < A.cols-1) fprintf(tex_fp,"&"); 
    }
    if (i < A.rows-1) fprintf(tex_fp,"\\\\"); 
  }
  fprintf(tex_fp,"\\end{array} )$}\n");
}


void
new_draw_arc(float x1, float y1, float x2, float y2, float rad, 
	 float vexp, float hexp, MATRIX A) {
  float dx,dy,norm,midx,midy;
  
  dx = x2-x1;
  dy = y2-y1;
  norm = sqrt(dx*dx+dy*dy);
  dx /= norm;
  dy /= norm;
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*x1 + rad*dx,(hexp*y1 + rad*dy));
  fprintf(tex_fp,"\\lvec(%3.3f %3.3f)\n",vexp*x2-rad*dx,(hexp*y2 - rad*dy));
  midx = (x1 + x2)/2;
  midy = (y1 + y2)/2;
  /*  fig_print_matrix(vexp*midx,hexp*midy,A); */
}


static float 
type_height(int type) {
  if (type == PHANTOM_CHILD_NODE) type = ANCHOR_NODE;
  return((float) type);
}


static float 
new_type_height(int type) {


  if (type ==  BACKBONE_SOLO_NODE  || type == BACKBONE_MIDI_NODE || type == SOLO_PHRASE_START ) return(10.);

  /*    if (type == PHANTOM_CHILD_NODE) type = ANCHOR_NODE; */

  /*    if (type ==  BACKBONE_SOLO_NODE  || type == BACKBONE_MIDI_NODE) return(3.);
  if (type ==  OBS_NODE) return(1.);
  if (type ==  ACCOM_OBS_NODE) return(4.);
  printf("unknown type %d\n",type);
  exit(0);*/


  if (type == SOLO_PHRASE_START) type = SOLO_NODE; 
    printf("%d\n",type); 
  return((float) (20-type));
}


void
make_kalman_graph(BELIEF_NET bn, float start, float end, char *name) {
  int i,d,type,j,newtype;
  float time,rad=.3,vexp=8,hexp=2.,x,y,newtime;
  BNODE *b;


  tex_fp = fopen(name,"w");
  vexp = 20./(INIT_WIDTH+end-start);
  fprintf(tex_fp,"\\drawdim cm \\setunitscale 1. \\linewd 0.02\n"); 
  fprintf(tex_fp,"\\textref h:C v:C\n"); 
  fprintf(tex_fp,"\\arrowheadtype t:F\n");
  fprintf(tex_fp,"\\arrowheadsize l:.2 w:.2\n");
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (time < start || time > end) continue;
    if (type > 2) continue;
    time -= start;
    /*    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*type_height(type),vexp*time); */
    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,hexp*new_type_height(type));

    /*    fprintf(tex_fp,"\\vtext{%d}\n",bn.list[i]->node_num);*/
    fprintf(tex_fp,"\\lcir r:%3.3f\n",rad);
  }
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (type > 2) continue;
    if (time < start || time > end) continue;
    time -= start;
    for (j = 0; j < bn.list[i]->next.num; j++) {
      b = bn.list[i]->next.arc[j];
      tex_attrib(b,&newtime,&newtype);
      if (newtime < start || newtime > end) continue;
      if (newtype > 2) continue;
      newtime -= start;
      /*            draw_arc(time, type_height(type), newtime, type_height(newtype), rad,vexp,hexp); */
      /*      draw_arc(type_height(type), time,  type_height(newtype), newtime,rad,hexp,vexp);  */
      draw_arc(time,  new_type_height(type),  newtime,new_type_height(newtype),rad,vexp,hexp);   
    }
  }
  fclose(tex_fp);
}

void
make_excerpt_graph(BELIEF_NET bn, float start, float end, char *name, int lowline, int highline) {
  int i,d,type,j,newtype,index,ii;
  float time,rad=.35,vexp=8,hexp=1.8/*2.*/,x,y,newtime;
  BNODE *b;
  char style[500];
  MATRIX A;


  tex_fp = fopen(name,"w");
  vexp = 18./(end-start);
  fprintf(tex_fp,"\\drawdim cm \\setunitscale .90 \\linewd 0.02\n"); 
  fprintf(tex_fp,"\\textref h:C v:C\n"); 
  fprintf(tex_fp,"\\arrowheadtype t:F\n");
  fprintf(tex_fp,"\\arrowheadsize l:.2 w:.2\n");
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (type == PHANTOM_NODE) { printf("phantom here\n"); }

    if (time < start || time > end) continue;
    if (type < lowline || type > highline) continue; 
    if (type == ACCOM_INIT_NODE) continue;
    time -= start;
    /*    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*type_height(type),vexp*time); */
    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,hexp*new_type_height(type));
    index = bn.list[i]->index;
    type2string(type,style);
    /*    fprintf(tex_fp,"\\htext{$x_{%d}^{\rm %s}$}\n",index,style); */
    /*    fprintf(tex_fp,"\\htext{$x_{%d}^{\\rm %s}$}\n",index,style);*/

    fprintf(tex_fp,"\\htext{%d}\n", bn.list[i]->mcs_num); 
    if ((type == OBS_NODE && time < 0. /*.35*/) || (type == ACCOM_OBS_NODE && time < 0. /*.4*/))
      fprintf(tex_fp,"\\fcir f:0 r:%3.3f\n",rad);
    else fprintf(tex_fp,"\\lcir r:%3.3f\n",rad);
  }
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (time < start || time > end) continue;
    if (type < lowline || type > highline) continue;
    if (type == ACCOM_INIT_NODE) continue;
    time -= start;
    for (j = 0; j < bn.list[i]->next.num; j++) {
      b = bn.list[i]->next.arc[j];
      tex_attrib(b,&newtime,&newtype);
      if (newtime < start || newtime > end) continue;
      if (newtype < lowline || newtype > highline) continue;
      if (newtype == ACCOM_INIT_NODE) continue;
      newtime -= start;
      for (ii=0; ii < b->prev.num; ii++) if (b->prev.arc[ii] == bn.list[i]) break;
      A = b->prev.A[ii];
      /*            draw_arc(time, type_height(type), newtime, type_height(newtype), rad,vexp,hexp); */
      /*    draw_arc(type_height(type), time,  type_height(newtype), newtime,rad,hexp,vexp);   */
	           new_draw_arc(time,  new_type_height(type),  newtime,new_type_height(newtype),rad,vexp,hexp,A);    
    }
  }
  fclose(tex_fp);
}

void
new_make_excerpt_graph(PHRASE_LIST pl, float start, float end, char *name, int lowline, int highline) {
  int i,d,type,j,newtype,index,ii,k;
  float time,rad=.35,vexp=8,hexp=.4 /*1.8*//*2.*/,x,y,newtime,top,bot;
  BNODE *b;
  char style[500];
  MATRIX A;
  BELIEF_NET bn;


  tex_fp = fopen(name,"w");
  vexp = 18./(end-start);
  fprintf(tex_fp,"\\drawdim cm \\setunitscale .90 \\linewd 0.02\n"); 
  fprintf(tex_fp,"\\textref h:C v:C\n"); 
  fprintf(tex_fp,"\\arrowheadtype t:F\n");
  fprintf(tex_fp,"\\arrowheadsize l:.2 w:.2\n");
  for (k=0; k < pl.num; k++) {
    bn = pl.list[k].net.bn;
    for (i=0; i < bn.num; i++) {
      if (bn.list[i]->focus == 0) continue;
      tex_attrib(bn.list[i],&time,&type);
      if (type == PHANTOM_NODE) { printf("phantom here\n"); }

      if (time < start || time > end) continue;
      if (type < lowline || type > highline) continue; 
      if (type == ACCOM_INIT_NODE) continue;
      if (type != BACKBONE_MIDI_NODE && type != BACKBONE_SOLO_NODE ) continue;
      if (type != SOLO_NODE && type != SOLO_PHRASE_START && type != OBS_NODE  && 
	  type != PHANTOM_NODE && type != INTERIOR_NODE && type != ANCHOR_NODE  &&
	  type != ACCOMP_LEAD_NODE && type != LEAD_CONNECT_NODE &&
	  type != ACCOM_OBS_NODE && type != BACKBONE_SOLO_NODE  &&
	  type != BACKBONE_MIDI_NODE && type != BACKBONE_UPDATE_SOLO_NODE  &&
	  type != BACKBONE_UPDATE_MIDI_NODE) continue;
      time -= start;
      /*printf("time = %f\n",time);       */
      /*    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*type_height(type),vexp*time); */
      fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,hexp*new_type_height(type));
      index = bn.list[i]->index;
      type2string(type,style);
      /*    fprintf(tex_fp,"\\htext{$x_{%d}^{\rm %s}$}\n",index,style); */
      /*    fprintf(tex_fp,"\\htext{$x_{%d}^{\\rm %s}$}\n",index,style);*/
      
         fprintf(tex_fp,"\\htext{%d}\n", bn.list[i]->mcs_num);  
      if ((type == OBS_NODE && time < 0. /*.35*/) || (type == ACCOM_OBS_NODE && time < 0. /*.4*/))
	fprintf(tex_fp,"\\fcir f:0 r:%3.3f\n",rad);
      else fprintf(tex_fp,"\\lcir r:%3.3f\n",rad);
    }
    for (i=0; i < bn.num; i++) {
      if (bn.list[i]->focus == 0) continue;
      tex_attrib(bn.list[i],&time,&type);
      if (time < start || time > end) continue;
      if (type < lowline || type > highline) continue;
      if (type == ACCOM_INIT_NODE) continue;
      if (type != BACKBONE_MIDI_NODE && type != BACKBONE_SOLO_NODE ) continue;
      if (type != SOLO_NODE && type != SOLO_PHRASE_START && type != OBS_NODE  && 
	  type != PHANTOM_NODE && type != INTERIOR_NODE && type != ANCHOR_NODE  &&
	  type != ACCOMP_LEAD_NODE && type != LEAD_CONNECT_NODE &&
	  type != ACCOM_OBS_NODE && type != BACKBONE_SOLO_NODE  &&
	  type != BACKBONE_MIDI_NODE && type != BACKBONE_UPDATE_SOLO_NODE  &&
	  type != BACKBONE_UPDATE_MIDI_NODE) continue;
      time -= start;
      for (j = 0; j < bn.list[i]->neigh.num; j++) {
	b = bn.list[i]->neigh.arc[j];
	if (b->focus == 0) continue;
	tex_attrib(b,&newtime,&newtype);
	if (newtype == SOLO_UPDATE_NODE) continue;
	if (newtime < start || newtime > end) continue;
	if (newtype < lowline || newtype > highline) continue;
	if (newtype == ACCOM_INIT_NODE) continue;
	newtime -= start;
	/*	for (ii=0; ii < b->neigh.num; ii++) if (b->neigh.arc[ii] == bn.list[i]) break;
	A = b->prev.A[ii];*/
	/*            draw_arc(time, type_height(type), newtime, type_height(newtype), rad,vexp,hexp); */
	/*    draw_arc(type_height(type), time,  type_height(newtype), newtime,rad,hexp,vexp);   */
	new_draw_arc(time,  new_type_height(type),  newtime,new_type_height(newtype),rad,vexp,hexp,A);    
      }
    }
  }
  fclose(tex_fp);
}



void
make_tex_graph(BELIEF_NET bn) {
  int i,d,type,j,newtype;
  float time,rad=.3,vexp=8,hexp=2.,x,y,newtime,mint=100000,maxt=0;
  BNODE *b;


  tex_fp = fopen(TEX_FILE,"w");
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (time > maxt) maxt = time;
    if (time < mint) mint = time;
  }
  vexp = /*20.*/ 25./(maxt-mint);
  fprintf(tex_fp,"\\drawdim cm \\setunitscale 1. \\linewd 0.02\n"); 
  fprintf(tex_fp,"\\textref h:C v:C\n"); 
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    time -= mint;
/*    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,-hexp*type_height(type));*/
    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*type_height(type),vexp*time);
    /*       fprintf(tex_fp,"\\htext{%d,%d}\n",bn.list[i]->node_num,bn.list[i]->mcs_num);  */

    /*if (bn.list[i]->e.m.rows > 1) */fprintf(tex_fp,"\\vtext{%d}\n",bn.list[i]->/*note_type*/node_num/*index*//*e.m.el[1][0]*/); 
    fprintf(tex_fp,"\\lcir r:%3.3f\n",rad);
  }
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    time -= mint;
    for (j = 0; j < bn.list[i]->next.num; j++) {
      b = bn.list[i]->next.arc[j];
      tex_attrib(b,&newtime,&newtype);
      newtime -= mint;
      /*            draw_arc(time, type_height(type), newtime, type_height(newtype), rad,vexp,hexp); */
      draw_arc(type_height(type), time,  type_height(newtype), newtime,rad,hexp,vexp); 
    }
  }
  fclose(tex_fp);
}

void
talk_make_tex_graph(BELIEF_NET bn) {
  int i,d,type,j,newtype;
  float time,rad=.3,vexp=8,hexp=2.,x,y,newtime,mint=100000,maxt=0,margin = 1;
  BNODE *b;


  tex_fp = fopen(TEX_FILE,"w");
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (time > maxt) maxt = time;
    if (time < mint) mint = time;
  }
  vexp = 20./(maxt-mint);
  fprintf(tex_fp,"\\drawdim cm \\setunitscale 1. \\linewd 0.02\n"); 
  fprintf(tex_fp,"\\textref h:C v:C\n"); 
  fprintf(tex_fp,"\\arrowheadtype t:F\n");
  fprintf(tex_fp,"\\arrowheadsize l:.2 w:.2\n");

  /*  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*1,vexp*0);
  fprintf(tex_fp,"\\textref h:R v:C\n");
  fprintf(tex_fp,"\\vtext{solo observations}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*2,vexp*0);
  fprintf(tex_fp,"\\vtext{solo nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*3,vexp*0);
  fprintf(tex_fp,"\\vtext{phantom nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*4,vexp*0);
  fprintf(tex_fp,"\\vtext{anchor nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*5,vexp*0);
  fprintf(tex_fp,"\\vtext{interior accomp nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*6,vexp*0);
  fprintf(tex_fp,"\\vtext{accomp observations}\n");  */
    
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    time -= mint;
/*    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,-hexp*type_height(type));*/
    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,hexp*new_type_height(type));
    /*       fprintf(tex_fp,"\\htext{%d,%d}\n",bn.list[i]->node_num,bn.list[i]->mcs_num);  */

    /*           fprintf(tex_fp,"\\vtext{%d}\n",bn.list[i]->index); */
    fprintf(tex_fp,"\\lcir r:%3.3f\n",rad);
  }
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    time -= mint;
    for (j = 0; j < bn.list[i]->next.num; j++) {
      b = bn.list[i]->next.arc[j];
      tex_attrib(b,&newtime,&newtype);
      newtime -= mint;
      /*            draw_arc(time, type_height(type), newtime, type_height(newtype), rad,vexp,hexp); */
       draw_arc(time,  new_type_height(type),  newtime,new_type_height(newtype),rad,vexp,hexp);   
    }
  }
  fclose(tex_fp);
}


void
xxx_talk_make_tex_graph(BELIEF_NET bn) {
  int i,d,type,j,newtype;
  float time,rad=.3,vexp=8,hexp=2.,x,y,newtime,mint=100000,maxt=0;
  BNODE *b;


  tex_fp = fopen(TEX_FILE,"w");
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    if (time > maxt) maxt = time;
    if (time < mint) mint = time;
  }
  vexp = 20./(maxt-mint);
  fprintf(tex_fp,"\\drawdim cm \\setunitscale 1. \\linewd 0.02\n"); 
  fprintf(tex_fp,"\\textref h:C v:C\n"); 
  fprintf(tex_fp,"\\arrowheadtype t:F\n");
  fprintf(tex_fp,"\\arrowheadsize l:.2 w:.2\n");

  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*1,vexp*0);
  fprintf(tex_fp,"\\textref h:R v:C\n");
  fprintf(tex_fp,"\\vtext{solo observations}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*2,vexp*0);
  fprintf(tex_fp,"\\vtext{solo nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*3,vexp*0);
  fprintf(tex_fp,"\\vtext{phantom nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*4,vexp*0);
  fprintf(tex_fp,"\\vtext{anchor nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*5,vexp*0);
  fprintf(tex_fp,"\\vtext{interior accomp nodes}\n"); 
  fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*6,vexp*0);
  fprintf(tex_fp,"\\vtext{accomp observations}\n"); 
    
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    time -= mint;
/*    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",vexp*time,-hexp*type_height(type));*/
    fprintf(tex_fp,"\\move(%3.3f %3.3f)\n",hexp*type_height(type),vexp*time);
    /*       fprintf(tex_fp,"\\htext{%d,%d}\n",bn.list[i]->node_num,bn.list[i]->mcs_num);  */

    /*           fprintf(tex_fp,"\\vtext{%d}\n",bn.list[i]->index); */
    fprintf(tex_fp,"\\lcir r:%3.3f\n",rad);
  }
  for (i=0; i < bn.num; i++) {
    tex_attrib(bn.list[i],&time,&type);
    time -= mint;
    for (j = 0; j < bn.list[i]->next.num; j++) {
      b = bn.list[i]->next.arc[j];
      tex_attrib(b,&newtime,&newtype);
      newtime -= mint;
      /*            draw_arc(time, type_height(type), newtime, type_height(newtype), rad,vexp,hexp); */
      draw_arc(type_height(type), time,  type_height(newtype), newtime,rad,hexp,vexp); 
    }
  }
  fclose(tex_fp);
}




static void
check_clique_dists(CLIQUE_TREE ct) {
  int i;
  CLIQUE *cl;

  for (i=0; i < ct.num; i++) {
    cl = ct.list[i]->clique;
    QFcheck(cl->qf);
    /*    printf("%d okay\n",i); */
  }
}

static void 
print_clique_tree(CLIQUE_TREE ct) {
  int i;
  for (i=0; i < ct.num; i++) {
    printf("clique = %d\n",ct.list[i]->clique_id); 
    print_clique(ct.list[i]->clique);
  }  
}

CLIQUE_TREE 
bn2ct(BELIEF_NET *bn) {
  CLIQUE_TREE ct;

  connect_belief_net_with_score(*bn);
  new_make_moral_graph(bn);
  new_triangulate_graph(bn);
  ct = new_new_build_clique_tree(bn); 
  init_cliques_with_probs(ct, *bn); 
  
  return(ct);
}

CLIQUE_TREE 
old_bn2ct_pot(BELIEF_NET *bn) {
  CLIQUE_TREE ct;

  connect_belief_net_with_score(*bn);
  new_triangulate_graph(bn);
   ct = new_new_build_clique_tree(bn); 
  init_cliques_using_potentials(ct, *bn);  
  return(ct);
}

CLIQUE_TREE 
bn2ct_pot(BELIEF_NET *bn) {
  CLIQUE_TREE ct;
  int i,j;

  /*  for (i=0; i < bn->num; i++) if (bn->list[i]->focus) {
    printf("bn->list[%d]->mcs_num = %d\n",i,bn->list[i]->mcs_num);
    for (j=0; j < bn->list[i]->neigh.num; j++)
      printf("focus = %d\n",bn->list[i]->neigh.arc[j]->focus);
      } */
  connect_belief_net_with_the_score(*bn);

  fast_triangulate_graph(bn);                 //CF:  ****  //CF:  bn how has extra BNODE.neigh undir arcs
  ct = focus_build_clique_tree(bn);           //CF:  ****  builds the clique tree (no probabilities yet)

  /*  printf("stage 1 \n");
  for (i=0; i < ct.num; i++) {
    printf("clique %d has %d els\n",i,ct.list[i]->clique->num);
    for (j=0; j < ct.list[i]->neigh.num; j++)
      printf("sep has %d els\n",ct.list[i]->neigh.bond[j].intersect->num);
      }*/
  alloc_clique_tree_qfs(ct);  //CF:  mallocs for potentials

  /*  printf("stage 2 \n");
  for (i=0; i < ct.num; i++) {
    printf("clique %d has %d els\n",i,ct.list[i]->clique->num);
    for (j=0; j < ct.list[i]->neigh.num; j++)
      printf("sep has %d els\n",ct.list[i]->neigh.bond[j].intersect->num);
      }*/

  init_cliques_using_potentials(ct, *bn);  //CF:  init potentials.  Seperators are flat. Nodes get product of clique members.
  /*  printf("leavinmg out equil\n"); */
  //  global_flag = 1;
  equilibrium(ct);   /* do not take this out.  ct2bn assumes clique tree in equil */  //CF:  A ROUND OF MSG PASSING !
  //  exit(0);
  return(ct);
}




CLIQUE_TREE 
exper_bn2ct(BELIEF_NET *bn) {
  CLIQUE_TREE ct;
 
  connect_belief_net_with_score(*bn);
  /*  printf("%d\n",score.solo.note[0].belief);
      exit(0); */
  new_make_moral_graph(bn);
  new_triangulate_graph(bn);
  ct = new_new_build_clique_tree(bn); 
  init_cliques_with_probs(ct, *bn); 
  /*  equilibrium(ct);
      init_cliques_with_probs(ct, *bn);  */

  /*  make_connections(ct, *bn); */

  return(ct);
}


static void
compute_boundary_marginal(CLIQUE *po, CLIQUE_TREE ct) {
  int k;
  CLIQUE *cl;

  for (k=0; k < ct.num; k++) { /* find a clique tree element containing c */
    cl =  ct.list[k]->clique;
    if (is_subset(po,cl)) break;
  }
  if (k == ct.num) {
    printf("could't find clique in clique tree\n");
    exit(0);
  }
  po->qf = marginalize_sep(cl,po);
  po->polarity = DENOMINATOR;
}


static void
xct2bnx(CLIQUE_NODE *dad, CLIQUE_NODE *gramp, BELIEF_NET *bn) {
  int i;
  CLIQUE_NODE *son;
  CLIQUE *soncl,*midcl,*temp;
  QUAD_FORM midqf,sonqf;
  MATRIX I;
    
  /*  print_clique(dad->clique); */
  clear_matrix_buff();
  for (i = 0; i < dad->neigh.num; i++) {
    son = dad->neigh.bond[i].ptr;
    if (son != gramp) {
      soncl = son->clique;
      midcl = dad->neigh.bond[i].intersect;
      temp = copy_clique(soncl);
      I = permute_matrix(soncl,midcl);

      /*      soncl->qf = QFconditional(soncl->qf,I);
      soncl->polarity = NUMERATOR;
      add_potential(bn, soncl); */
      
      temp->qf = QFconditional(soncl->qf,I);
      temp->polarity = NUMERATOR;
      add_potential(bn, temp);

      xct2bnx(son,dad,bn);
    }
  }
}


static void
ct2bn(CLIQUE_NODE *root, BELIEF_NET *bn) {
  /* assumes that clique tree is in equilibrium */

  root->clique->polarity = NUMERATOR;
  add_potential(bn, root->clique);
  xct2bnx(root,0,bn);
}

static void
clear_focus_potentials(BELIEF_NET *bn) {
  int i;
  CLIQUE *po;

  for (i=bn->potential.num-1; i >= 0; i--) {
    po = bn->potential.el[i];
    if (clique_in_focus(po) == 0) continue;
    bn->potential.el[i] = bn->potential.el[--(bn->potential.num)];
  }
}


static void
condition_on_boundary(CLIQUE *bnd, BELIEF_NET *bn) {
  int k;
  CLIQUE *rootcl,*cond;
  QUAD_FORM qf;
  MATRIX I;
  CLIQUE_TREE ct;
  CLIQUE_NODE *root;

  ct = bn2ct_pot(bn); 
  for (k=0; k < ct.num; k++) { /* find a clique tree element containing c */
    root =  ct.list[k];
    rootcl =  root->clique; 
    if (is_subset(bnd,rootcl)) break;
  }
  if (k == ct.num) {
    printf("could't find clique in clique tree\n");
    exit(0);
  }
  I = permute_matrix(rootcl,bnd);
  qf = QFconditional(rootcl->qf,I);
  /*  cond = make_potential(rootcl,qf);*/
  rootcl->qf = QFperm(qf); 
  clear_focus_potentials(bn);
  ct2bn(root,bn);
}


static void
bisect_boundary(BELIEF_NET *bn, CLIQUE *cl) {
  int i,type;
  BNODE *nd;


  if (cl->num > 2) {
    printf("boundary clique has too many elements (%d)\n",cl->num);
    exit(0);
  }
  if (cl->num == 1) return;
  if (cl->num == 0) {
    printf("empty boundary clique\n");
    exit(0);
  }
  nd = cl->member[0];
  for (i=0; i < nd->neigh.num; i++) {
    type = nd->neigh.arc[i]->note_type;
    if (type == SOLO_NODE || type == SOLO_PHRASE_START) break;
  }
  if (i == nd->neigh.num) {
    printf("couldn't find SOLO_NODE parent\n");
    exit(0);
  }
  add_undir_arc(nd->neigh.arc[i],cl->member[1]);
}


//CF:  not used?
static void
condition_graph_on_solo(BELIEF_NET *bn) {
  int i,j=0,type,ntype,k=0,l=0;
  BELIEF_NET sub;
  BNODE *cur,*next;
  float t0=10000;
  CLIQUE *boundary;
  CLIQUE_TREE ct;
  QUAD_FORM marg;


  for (i=0; i < bn->num; i++) bn->list[i]->mark = 0;
  for (i=0; i < bn->num; i++) {
    cur = bn->list[i];
    type = cur->note_type;
    if (type == INTERIOR_NODE && cur->mark == 0) {
      /*      l++; */
      focus_on_sub_structure(cur, bn, &boundary); 
      /*                print_clique(boundary);   */
      condition_on_boundary(boundary,bn);
      bisect_boundary(bn,boundary);


      /*   old way ....
	   ct = bn2ct_pot(bn); 
      compute_boundary_marginal(boundary,ct);
      add_potential(bn, boundary);
      bisect_boundary(bn,boundary);*/

      /*        t0 = HUGE_VAL;
	for (k=0; k < bn->num; k++) if ((bn->list[k]->focus) && (bn->list[k]->meas_time < t0)) 
	  t0 = bn->list[k]->meas_time;
	printf("t0 = %f\n",t0);
	make_connected_components(bn,&component); 
	anew_make_excerpt_graph(component,t0,t0+5,"weinen_frag.tex",0,20);  */
      /*      j=0;
      for (k=0; k < bn->num; k++) if (bn->list[k]->focus && (bn->list[k]->note_type == PHANTOM_NODE ||bn->list[k]->note_type == ANCHOR_NODE)) j++;
      printf("%d (%d) anchors\n",j,boundary->num);
      if (j == 0) {
	for (k=0; k < bn->num; k++) if ((bn->list[k]->focus) && (bn->list[k]->meas_time < t0)) 
	  t0 = bn->list[k]->meas_time;
	printf("t0 = %f\n",t0);
	make_connected_components(bn,&component); 
	new_make_excerpt_graph(component,t0,t0+5,"weinen_frag.tex",0,20); 
	exit(0);
      }*/
    }
  }
  for (i=0; i < bn->num; i++) bn->list[i]->focus = 1;
}




static void
fix_examp_obs(EXAMPLE *ex, int firstnote, int lastnote) {
  int i;
  float time;

  for (i = ex->start; i <= ex->end; i++) {
    if (i > lastnote || i < firstnote) continue;
    time = ex->time[i-firstnote];
    fix_solo_obs(i,time);
  }
}

static float /* additive and multiplicative constants are discarded */
gaussian_log_like(QUAD_FORM qf, MATRIX obs) {
  MATRIX d,m;
  float v;

  if (qf.null.cols != 0 || qf.inf.cols != 0) return(-HUGE_VAL);
  d = Ms(qf.m,obs);
  m = Mm(Mt(d),Mm(qf.S,d));
  v = my_log(fabs(Mdet(qf.S))) - m.el[0][0];
  return(v);
}


static float 
normal_log_like(float mean, float var, float obs) {
  float d,m,v;


  if (var == 0.)  return(-HUGE_VAL);
  d = mean - obs;
  v = -.5*(my_log(var) + d*d/var);
  return(v);
}



static float  /* assumes equilibrium is stored */
belief_log_like(CLIQUE_TREE ct, EXAMPLE *ex, int firstnote, int lastnote) {
  /* pass this routine the file name that has the list of 
     tag observation
     tag observation 
     ....
     */
  int i,one_set=0;
  BNODE *bn;
  QUAD_FORM qf;
  float sqe = 0,total = 0,time,predict,x;
  

  recall_state(ct);  /* gives me equilibrium */
  for (i = ex->start; i <= ex->end; i++) {
    if (i > lastnote || i < firstnote) continue;
    time = ex->time[i-firstnote]; /* the actual observation */
    bn = score.solo.note[i].belief;
    if (one_set)  collect(bn->clnode,0);  /* bn->clnode is a clique that contains bn */
    qf = post_dist(bn);     /* this posterior distribution on node bn */
    if (qf.cov.el[0][0] == 0.) {  
      printf("0 cova in note %d\n",i);
      QFprint(qf);
    }
    predict = qf.m.el[0][0]; /* this is posterion mean of TIME of bn */
    /* qf.m is mean of qf (which is the posterior distribution on the bn node */
    /* qf.m is nx1 matrix so has components qf.el[0][0],qf.el[1][0] ... qf.el[n-1][0] */
    x = normal_log_like(qf.m.el[0][0],qf.cov.el[0][0],time);
    printf("error = %f\n",qf.m.el[0][0]-time);
    total += x;
    /*      total += (predict-time)*(predict-time); squared error*/
    /*      printf("i = %d var = %f\n",i,qf.cov.el[0][0]); */
    fix_solo_obs(i,time);  /* changes clique tree rep by multiplying
			      by $1{bn = obs}$ */
    /*     change this to fix_obs(bn, float val) */
    one_set = 1;
  }
  return(total);
}


static int
tag_obs_comp(const void *p1, const void *p2) {
  TAGGED_OBS *t1, *t2;

  t1 = (TAGGED_OBS *) p1;
  t2 = (TAGGED_OBS *) p2;
  return(strcmp(t1->name,t2->name));
}





//static DATA_OBSERVATION
static int
read_data_observation(char *file, DATA_OBSERVATION *dato) {
  FILE *fp;
  int i,lines=0,n;
  char name[500];
  float val;
  //  DATA_OBSERVATION dato;

  fp = fopen(file,"r");
  if (fp == NULL) {
    printf("couldn't open %s in read_data_observation()\n",file);
    /*    fp = fopen("xxx.c", "r");
	  printf("fp = %d\n",fp);*/
    return(0);
    //    exit(0);
  }
  //  else printf("successfully opened %s\n",file);
  n = 0;
  while (feof(fp) == 0) if (fgetc(fp) == '\n') lines++;
  fseek(fp,0,SEEK_SET);  /* start at beginning */
  dato->tag_obs = (TAGGED_OBS *) malloc(lines*sizeof(TAGGED_OBS));
  for (i=0; i < lines; i++) {
    fscanf(fp,"%s %f",name,&val);
    //    printf("%s %f\n",name,val); 

    dato->tag_obs[i].name = (char *) malloc(strlen(name)+1);
    strcpy(dato->tag_obs[i].name,name);
    dato->tag_obs[i].val = val;
  }
  dato->num = lines;
  qsort(dato->tag_obs,dato->num,sizeof(TAGGED_OBS),tag_obs_comp);
  /*      for (i=0; i < dato->num; i++) printf("%s %f\n",dato->tag_obs[i].name,dato->tag_obs[i].val);   */
  fclose(fp);
  return(1);
  //  return(dato);
}


static int
assoc_val(DATA_OBSERVATION dato, char *tag, TAGGED_OBS *t) {
  int hi,lo,mid,d;

  lo = 0;
  hi = dato.num-1;
  if (strcmp(dato.tag_obs[lo].name,tag) == 0) {
    *t = dato.tag_obs[lo];
    return(1);
  }
  if (strcmp(dato.tag_obs[hi].name,tag) == 0) {
    *t = dato.tag_obs[hi];
    return(1);
  }
  while (lo < hi-1) {
    /*    printf("lo = %s hi = %s tag = %s\n",dato.tag_obs[lo].name,dato.tag_obs[hi].name,tag); */
    mid = (hi+lo)/2;
    d = strcmp(dato.tag_obs[mid].name,tag);
    if (d == 0) {
      *t = dato.tag_obs[mid];
      return(1);
    }
    if (d < 0) lo = mid;
    else hi = mid;
  }
  return(0);
}

static float  /* assumes equilibrium is stored */
old_data_log_like(NETWORK net, DATA_OBSERVATION dato) {
  int i,j,one_set=0;
  BNODE *bn;
  QUAD_FORM qf;
  float sqe = 0,total = 0,val,predict,x;
  TAGGED_OBS t;


  recall_state(net.ct);  /* gives equilibrium */

  for (j=0; j < net.bn.num; j++) {
    bn = net.bn.list[j];
    /*    printf("type = %d observable tag = %s\n",bn->note_type,bn->observable_tag);*/
    if (bn->observable_tag == NULL) continue;
    if (assoc_val(dato,bn->observable_tag,&t) == 0)   continue;
    /*    printf("j = %d tag = %s tag = %s val = %f \n",j,bn->observable_tag,t.name,t.val);*/
    if (one_set)  {
      collect(bn->clnode,0);  /* bn->clnode is a clique that contains bn */
      qf = post_dist(bn);     /* this posterior distribution on node bn */
      if (qf.cov.el[0][0] == 0.)  {
	printf("0 cova in note %d\n",j);
	QFprint(qf);
      }
      predict = qf.m.el[0][0]; /* assuming 0th position is observable, this is posterior mean of observable */
      /* qf.m is mean of qf (which is the posterior distribution on the bn node */
      /* qf.m is nx1 matrix so has components qf.el[0][0],qf.el[1][0] ... qf.el[n-1][0] */
      x = normal_log_like(qf.m.el[0][0],qf.cov.el[0][0],t.val);
      total += x; 
      /*      total += (predict-t.val)*(predict-t.val); /* squared error*/
      /*      printf("i = %d var = %f\n",i,qf.cov.el[0][0]); */
    }
    fix_obs(bn, t.val);  /* changes clique tree rep by multiplying by $1{bn = obs}$ */
    one_set = 1;
  }
  return(total);
}

//CF:  
static float  /* assumes equilibrium is stored */
data_log_like(NETWORK net, DATA_OBSERVATION dato) {
  int i,j,one_set=0;
  BNODE *bn;
  QUAD_FORM qf;
  float sqe = 0,total = 0,val,predict,x;
  TAGGED_OBS t;


  recall_state(net.ct);  /* gives equilibrium */

  for (j=0; j < net.bn.num; j++) {   //CF:  for each observed node...   (continue if not observed)
    bn = net.bn.list[j];
    //    printf("type = %d observable tag = %s\n",bn->note_type,bn->observable_tag);
    if (bn->observable_tag == NULL) continue;
    if (assoc_val(dato,bn->observable_tag,&t) == 0)    continue;
    /*    printf("j = %d tag = %s tag = %s val = %f \n",j,bn->observable_tag,t.name,t.val);*/


    collect(bn->clnode,0);  /* bn->clnode is a clique that contains bn */
    qf = post_dist(bn);     /* this posterior distribution on node bn */
    /*      if (qf.inf.cols) printf("inf tag = %s\n",bn->observable_tag); 
	    else  printf("fin tag = %s\n",bn->observable_tag); */
    if (qf.inf.cols == 0)  { /* finite var on observation */
      if (qf.cov.el[0][0] == 0.)  {
	printf("0 cova in note %d\n",j);
	QFprint(qf);
      }
      predict = qf.m.el[0][0]; /* assuming 0th position is observable, this is posterior mean of observable */
      /* qf.m is mean of qf (which is the posterior distribution on the bn node */
      /* qf.m is nx1 matrix so has components qf.el[0][0],qf.el[1][0] ... qf.el[n-1][0] */
      /*        printf("name = %s predict = %f var = %f val = %f error = %f\n",t.name,qf.m.el[0][0],qf.cov.el[0][0],t.val,t.val-qf.m.el[0][0]);*/
      x = normal_log_like(qf.m.el[0][0],qf.cov.el[0][0],t.val);     //CF: lnP(y_i | y_{j<i}, M)  :where M includes priors
	      //    printf("error = %f\n",qf.m.el[0][0]-t.val);
	      /*	      if (x < -100) {
		printf("node = %s val = %f\n",bn->observable_tag,t.val);
		QFprint(bn->clnode->clique->qf);
		exit(0);
		}*/
	      
      total += x; 
      /*      total += (predict-t.val)*(predict-t.val); /* squared error*/
      /*      printf("i = %d var = %f\n",i,qf.cov.el[0][0]); */
    }
    fix_obs(bn, t.val);  /* changes clique tree rep by multiplying by $1{bn = obs}$ */
  }
  return(total);
}


static float
phrase_log_likelihood(CLIQUE_TREE ct, int firstnote, int lastnote) {
  EXAMPLE *ex;
  float total = 0;
  int i;

  for (i=0; i < score.example.num; i++) {  /* go through TRAIN_FILE_LIST instead */
    ex = score.example.list + i;
    if (ex->start < lastnote && ex->end > firstnote)  
      total += belief_log_like(ct, ex, firstnote, lastnote);
    if (total == -HUGE_VAL) return(total);
  }
  return(total);
}
//CF:  
static float
all_data_log_likelihood(NETWORK net, TRAIN_FILE_LIST tfl) {
  EXAMPLE *ex;
  float total = 0;
  int i;
  char *file;

  for (i=0; i < tfl.num; i++) {    //CF:  for each file
    file = tfl.name[i];
    total += data_log_like(net, tfl.dato[i]); //CF:  meat -- P(D|M) for this training file
    if (total == -HUGE_VAL) return(total);     //CF:  stop if getting silly values
  }
  return(total);
}

#define MAX_NOTES_INA_PHRASE 1000

train_test(int phrase) {  /* new input will be (BELIEF_NET bn, TRAIN_FILE_LIST tfl) */
  int firstnote,lastnote,firsta,lasta,i,j,exnum,iter;
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  EXAMPLE *ex;
  QUAD_FORM qf;
  MATRIX sl[MAX_NOTES_INA_PHRASE],*suff_lin;
  MATRIX sq[MAX_NOTES_INA_PHRASE],*suff_quad;
  MATRIX xx,m,sig,mm,xbar,xhat;
  float like;



  /*    score.example.num = 40;   */
  phrase_span(phrase,&firstnote,&lastnote,&firsta,&lasta);  /* this no longer relevant */
  suff_lin = sl-firstnote;  /* add fields to BNODE for these accumlators */
  suff_quad = sq-firstnote;
  for (i=firstnote; i <= lastnote; i++) {  /* look through DAG graph nodes to
					      find trainable variables (nodes with
					      no parents and initialize accumulators */
    suff_lin[i] = Mperm(score.solo.note[i].qf_update.m);
    Mset_zero(suff_lin[i]);
    suff_quad[i] = Mperm(score.solo.note[i].qf_update.cov);
    Mset_zero(suff_quad[i]);
  }
  bn = old_make_solo_graph(firstnote,lastnote);  /* this is already passed */


  ct = bn2ct(&bn); /* makes clique tree.  you still need to do this */

  printf("starting training\n");
  for (iter = 0; iter < 25; iter++) {  /* need more intelligence stopping criterion */
    for (i=firstnote; i <= lastnote; i++) { /* look through DAG nodes instead */
      Mset_zero(suff_lin[i]);
      Mset_zero(suff_quad[i]);
    }
    init_cliques_with_probs(ct, bn); /* assigns probabilites */
    equilibrium(ct);    
    save_state(ct); /* save the equilibrium representation for subsequent reinitialization
		       to equilibrium */
    printf("starting log likelihood\n");
    like = phrase_log_likelihood(ct,firstnote,lastnote);  
    printf("iter = %d like = %f\n",iter,like);
    exnum = 0;  /* number of observations.  should really have separate counter
		   for each observable node.  this would be a variable in BNODE */
    for (i=0; i < score.example.num; i++) { /* loop through files in TRAIN_FILE_LIST */
      /*      printf("ex = %d\n",i);*/
      ex = score.example.list + i;
      if (ex->start < lastnote && ex->end > firstnote)  { /* any overlap */
	printf("example %d\n",i);
	exnum++;  /* not really.  each observed variable should have associated
		     counter incremented */
	recall_state(ct);  /* sets clique tree to equilibrium again */

	fix_examp_obs(ex, firstnote, lastnote);  /* need to rewrite this
						    for generic DAG */
	equilibrium(ct);    
	for (j=firstnote; j <= lastnote; j++) { /* do this for trainable (no parents)
						   variables */
	  /* 	  printf("note = %d\n",j); */
	  qf = post_dist(score.solo.note[j].update_node);  

	  /* post_dist(bn) is marginal dist of variables assoc. with bn */
	  Mcopymat(Ma(suff_lin[j],qf.m),&suff_lin[j]);
	  xx = Ma(Mm(qf.m,Mt(qf.m)),qf.cov);  
		  /*	  	  xx = Mm(qf.m,Mt(qf.m));   */
	  /*	  if (j == 4)
		  { printf("m = \n");
		  /*printf("start = %d end = %d\n",ex->start,ex->end); */
	  /*Mp(qf.m); */
	  /*	} */
	  Mcopymat(Ma(suff_quad[j],xx),&suff_quad[j]);
	}
      }
    }
    for (j=firstnote; j <= lastnote; j++) {  /* reestimating trainable distributions */
      /*      xhat = Msc(suff_lin[j],1./(float)exnum);
      sig = Msc(suff_quad[j],1./(float)exnum);
      mm = Mm(xhat,Mt(xhat)); 
      sig = Ms(sig,mm); */

      xbar = Msc(suff_lin[j],1./(float)exnum);
      xhat = Mcopy(xbar);  /* !!! */
      if (iter < 6) if (j > firstnote) {
	xhat.el[0][0] = 0; 
	sig.el[0][0] = sig.el[0][1] = sig.el[1][0] = 0;
      }
      sig = Msc(suff_quad[j],1./(float)exnum);
      sig= Ms(Ma(sig,Mm(xhat,Mt(xhat))),Ma(Mm(xbar,Mt(xhat)),Mm(xhat,Mt(xbar))));
         

      /*            qf = QFmv_simplify(xhat,sig); */
	          qf = QFmv(xhat,sig);
      QFcopy(qf,&(score.solo.note[j].update_node->e)); 
      QFcopy(qf,&(score.solo.note[j].qf_update));
      /*                             printf("after %d\n",j);
				     QFprint(score.solo.note[j].qf_update);  */
    }   
    /*    exit(0); */
  }
  write_solo_training();
}

//CF: allocates (permanent) memory for the .suff_lin and .suff_quad   statistics of trainable nodes in a net
//CF:  eg.  suff_line = \sum mu_i
//CF:       suff_quad = \sum mu_i mu_i^T + \Sigma_i
//CF:  
//CF:  mu_i is the mean of this node given the ith training example 
//CF:  Sigma_i is the covariance given the ith training example
//CF:  These will be used to compute the ML mean and covariance for this node.
static void
init_bbn_train(BELIEF_NET bn) {
  BNODE *b;
  int i;

  for (i=0; i < bn.num; i++) {    //CF:  look for trainable nodes
    b = bn.list[i];               //CF:  each node b in net bn
    if (b->trainable == 0) continue;
    /*    printf("tag = %s\n",b->trainable_tag);*/
    b->suff_lin = Mperm(Mzeros(b->dim,1));  /* just taking dimensions */
    b->suff_quad = Mperm(Mzeros(b->dim,b->dim));
  }
}

static void
zero_sufficient_stats(BELIEF_NET bbn) {
  int i;
  BNODE *bn;
  
  for (i=0; i < bbn.num; i++) {
    bn = bbn.list[i];
    if (bn->trainable == 0) continue;
    Mset_zero(bn->suff_lin);
    Mset_zero(bn->suff_quad);
  }
}

static void
accumulate_sufficient_stats(BELIEF_NET bbn) {
  int j;
  BNODE *bn;
  QUAD_FORM qf;
  MATRIX xx;
  
  for (j=0; j < bbn.num; j++) {
	bn = bbn.list[j];
    if (bn->trainable == 0) continue;
	/*	  printf("note = %d\n",j);*/

	qf = post_dist(bn);  /* qf is marginal dist of variables assoc. with trainable node bn */

    
    /*    if (qf.null.cols || qf.inf.cols) {
      printf("degeneracy in trainable %s\n",bn->trainable_tag);
      QFprint(qf);
      }*/



    /*  printf("tag = %s\n",bn->trainable_tag);
	QFprint(qf);*/
    /*    if (strcmp(bn->trainable_tag,"bbone_xchg_337+0/1") == 0) {
      printf("mean of update is \n");
      Mp(qf.m);
      }*/

    Mcopymat(Ma(bn->suff_lin,qf.m),&(bn->suff_lin));   //CF:  suff_lin += mu
    //    printf("%s\n",bn->trainable_tag);
    //   Mp(qf.m);
    //    Mp(qf.cov);
    xx = Ma(Mm(qf.m,Mt(qf.m)),qf.cov);                 //CF:  suff_quad += (mu*mu^T + Sigma)
	/*    	  	  xx = Mm(qf.m,Mt(qf.m));   */
    Mcopymat(Ma(bn->suff_quad,xx),&(bn->suff_quad));
  }
}



static void
set_data_observation(BELIEF_NET bbn, DATA_OBSERVATION dato) {
  int i,j;
  BNODE *bn;
  float val;
  TAGGED_OBS t;

  for (j=0; j < bbn.num; j++) {
    bn = bbn.list[j];
    if (bn->observable_tag == NULL) continue;
    if (assoc_val(dato,bn->observable_tag,&t) == 0)   continue;
    val = t.val;
    //    printf("name = %s val = %f\n",t.name,t.val);
	fix_obs(bn, val);  /* changes clique tree rep by multiplying by $1{bn = obs}$ */
	clear_matrix_buff();
    //       printf("setting %s to %f\n",bn->observable_tag,val);
  }
}


QUAD_FORM
temp_QFmv_simple(MATRIX m, MATRIX cov) { 
  QUAD_FORM temp;
  int dim;
  MATRIX P;

  if (m.cols != 1 || m.rows != cov.rows || m.rows != cov.cols) {
    printf("bad input to QFmv\n");
    exit(0);
  }
  dim = m.rows;
  temp.m =  m;
  printf("cov is\n");
  Mp(cov);
  printf("%e\n",cov.el[0][0]);
  Mnull_decomp_thresh(cov,&temp.null,&temp.fin,.0001);
  temp.inf  = Mzeros(dim,0);
  P = Mm(temp.fin,Mt(temp.fin));
  temp.cov = Mm(Mm(P,cov),P);
  temp.S = Msym_range_inv(temp.cov,temp.fin);
  /*  temp.cov = cov;
  temp.null = Mzeros(dim,0);
  temp.fin  = Miden(dim);
  temp.S =   Mi(cov); */
  exit(0);
  return(temp);
}




#define PRIOR_COUNT 20.  /* a floating point value */

static void
reestimate_trainable_dists(BELIEF_NET bbn, int iter, int exnum, int mode) {
  int j,d;
  BNODE *bn;
  MATRIX sig,xbar,xhat;
  QUAD_FORM qf,temp;
  float p,q;
  
  //CF:  each trainable BNODE
  for (j=0; j < bbn.num; j++) { /* reestimating trainable distribution"s */
    bn = bbn.list[j];
	if (bn->trainable == 0) 
	  continue;

    
	p = PRIOR_COUNT / (float) (PRIOR_COUNT + exnum);   //CF:  not used?
    q = exnum / (float) (PRIOR_COUNT + exnum);         //CF:  not used?
    xbar = Msc(bn->suff_lin,1./(float)exnum);         //CF:  mu_hat = (1/n)*suff_lin

    /*    if (strcmp(bn->trainable_tag,"bbone_xchg_337+0/1") == 0) {
      printf("here is 337: exnum = %d\n",exnum);
      
      Mp(xbar);
      }*/



    xhat = Mcopy(xbar);  /* !!! */                      //CF:  ? not needed now?

    //    if (xhat.el[0][0] < 0) xhat.el[0][0] = 0;  only length notes.  didn't really notice much affect, but maybe good?

    if (mode == MEANS_ONLY) sig = bn->train_dist->cov;  // what was already there
    else {
      sig = Msc(bn->suff_quad,1./(float)exnum);
      //CF:  ?   this is just doing    sig_hat = sig - xx^t    ?      
      sig= Ms(Ma(sig,Mm(xhat,Mt(xhat))),Ma(Mm(xbar,Mt(xhat)),Mm(xhat,Mt(xbar))));
    }

     /*   xhat = Ma(Msc(xhat,q),Msc(bn->prior_mean,p));
	printf("p = %f q = %f\n",p,q);
	printf("sig before = \n");
	Mp(sig);
	printf("prior cov = \n");
	Mp(bn->prior_cov);
	sig = Ma(Msc(sig,q),Msc(bn->prior_cov,p));
	printf("sig after = \n");
	Mp(sig);*/


    /*    printf("exnum = %d sig = \n",exnum);
	  Mp(sig); */
    /*        qf = QFmv_t_simplify(xhat,sig); */
    /*    if (j == 0) {
           Mp(xhat);
	      Mp(sig); 
	      } */
    /*    xhat = bn->prior_mean;
	  sig = bn->prior_cov;*/

    /*    if (bn->note_type == BACKBONE_UPDATE_NODE)  {
        qf = QFmv_simple(xhat,bn->prior_cov);   
	      printf("start pair\n");
      Mp(sig);
      Mp(bn->prior_cov);
    }
    else */

    /*#ifdef CLARINET_EXPERIMENT
    if (Mnormsq(sig) < .00001) {
      printf("resetting\n");
      Mp(sig);
      sig = Msc(Miden(sig.rows),.001);
      printf("to\n");
      Mp(sig);
    }
    #endif    */

    //CF:  make a quad form to store the estimated pdf
    qf = QFmv_simple(xhat,sig);   /* for training satruated model with nick */

    /*    if (strcmp(bn->trainable_tag,"bbone_xchg_337+0/1") == 0) {
      printf("bbone_xchg_337+0/1 = \n");
      Mp(xhat);
      Mp(qf.S);
      Mp(sig);
      

      printf("var = %10.20f\n",sig.el[0][0]);
      }*/


    /*	            qf = QFmv(xhat,sig);   /* for training satruated model 
					      (but very small variances are set
					      to null space) */
    if (bn->note_type == BACKBONE_PHRASE_START  || 
	bn->note_type == SOLO_PHRASE_START || 
	bn->note_type == ACCOM_PHRASE_START)   {
      qf = QFmake_pos_unif(qf);
      //      printf("making pos uniform for %s\n",bn->trainable_tag);
    }

    //    Mp(bn->train_dist->cov);
    QFcopy(qf,bn->train_dist); 
    bn->trained = 1;
    /*    if (strcmp(bn->trainable_tag,"bbone_xchg_337+0/1") == 0)  {
      printf("bbone_xchg_337+0/1\n");
      QFprint(qf);
      }*/

    //        printf("tag = %s\n",bn->trainable_tag);
    //       QFprint(*(bn->train_dist));
  }   
}


static int
is_overlap(NETWORK net,  DATA_OBSERVATION obs) {
  int i,j;

  for (i=0; i < net.bn.num; i++)  {
    if (net.bn.list[i]->observable_tag == NULL) continue;
    for (j=0; j < obs.num; j++) {
      if (strcmp(net.bn.list[i]->observable_tag,obs.tag_obs[j].name) == 0) return(1);
    }
  }
  return(0);
}

#ifdef WINDOWS
#define BBN_TRAIN_ITERS 1
#else
#define BBN_TRAIN_ITERS 5
#endif 


//CF:  main BN training fn
//CF:  net has a blief net and a clique tree, for one phrase.
//CF:  tfl is list of string names of training files
bbn_train(NETWORK net, TRAIN_FILE_LIST tfl, int iterations, int mode) {
  int i,iter,x,y,overlap[MAX_TRAIN_FILES],num_ex = 0;
  BNODE *bn;
  /*  MATRIX xx,m,sig,mm,xbar,xhat;*/
  float like,val,rat,last_like;

  //CF:  count num of useful files, ie. those which have info for this phrase.
  for (i=0; i < tfl.num; i++) if (overlap[i] = is_overlap(net,tfl.dato[i])) num_ex++;  
  printf("num examples = %d\n",num_ex);
  /* yes, that really should be just = and not == above! */
  //  for (i=0; i < tfl.num; i++) printf("%d ", overlap[i]); printf("\n");
  //  printf("num_ex = %d\n",num_ex);
  if (num_ex == 0) return(0);


  init_bbn_train(net.bn);   //CF:  allocate memory
  printf("starting training\n");
  //     net.ct = bn2ct_pot(&(net.bn));   /* this seems to be the problem */

  //CF:  usually 5 EM iterations
  for (iter = 0; iter <= /*BBN_TRAIN_ITERS*/iterations; iter++) {  /* need more intelligence stopping criterion */
    zero_sufficient_stats(net.bn);   //CF:  sets suff_lin and suff_quad to zero
    
    //CF:  init and equilibriate the clique tree
    init_cliques_using_potentials(net.ct, net.bn);  
    equilibrium(net.ct);    
    save_state(net.ct); /* save the equilibrium representation for subsequent 
			   reinitialization  to equilibrium (saved in shadow parts of BN structs) */
    if (iter == iterations/*BBN_TRAIN_ITERS*/) break;  // this is right
    printf("starting log likelihood\n");
    like = all_data_log_likelihood(net,tfl);        //CF:  sanity check that likeihood always increases durign EM
    printf("iter = %d (out of %d) like = %f\n",iter,iterations,like);
    //    printf("need to test that all examples intersect the current phrase\n");

    //CF:  for each training file
    for (i=0; i < tfl.num; i++) { 
      background_info.fraction_done = i/(float)tfl.num;
      if (overlap[i] == 0) continue;
      recall_state(net.ct);  /* sets clique tree to equilibrium again */

      //      printf(" setting observation %s\n",tfl.name[i]);
      set_data_observation(net.bn, tfl.dato[i]);  //CF:  instantiate all observations from training file
      equilibrium(net.ct);    //CF:  run full round of msg passing
      accumulate_sufficient_stats(net.bn);   //CF:  update bn.suff_lin and .suff_quad for each trainable (parentless) BNODE
    }
    background_info.fraction_done = 1;  // user can't close this.
	reestimate_trainable_dists(net.bn, iter,num_ex,mode);   //CF:  get quadforms for trained nodes; store in BNODE.trained_dist
    //    reestimate_trainable_dists(net.bn, iter,tfl.num,mode);
    if (iter > 0 && last_like > 0) {  // last_like == 0 only when a phrase only has a cue note (infinite variance)
       rat = like/last_like - 1.;
       printf("rat = %f\n",rat);
       if (rat  < .01 /*&& iter > 5*/) /*break*/;
    }
    last_like = like;
  }
}
  



static PHRASE_LIST
make_solo_bbn_graph() {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i;
  PHRASE_LIST pl;

  pl.list = (PHRASE_LIST_EL *) malloc(MAX_PHRASES*sizeof(PHRASE_LIST_EL));
  init_belief_net(&bn);
  add_solo_graph(&bn);
  make_connected_components(&bn,&pl);
  for (i=0; i < pl.num; i++)  pl.list[i].net.ct = bn2ct_pot(&(pl.list[i].net.bn));   
  return(pl);
}

static PHRASE_LIST
make_accomp_bbn_graph() {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i;
  PHRASE_LIST pl;

  pl.list = (PHRASE_LIST_EL *) malloc(MAX_PHRASES*sizeof(PHRASE_LIST_EL));
  init_belief_net(&bn);
  add_accomp_train_graph(&bn);
  make_connected_components(&bn,&pl);
  for (i=0; i < pl.num; i++)  {
    printf("creating network %d\n",i);
    pl.list[i].net.ct = bn2ct_pot(&(pl.list[i].net.bn));   
  }
  return(pl);
}


void
bbn_training_name(char *s, char *name) {
  strcpy(name,user_dir);
   strcat(name,AUDIO_DIR);
  strcat(name,"/");
  strcat(name,player);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"_");
  strcat(name,s);
  strcat(name,".bnt");
}

void
bbn_accomp_training_name(char *name) {
  strcpy(name,user_dir);
  strcat(name,"scores");
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"_");
  strcat(name,ACCOMP_TRAIN_ONLY);
  strcat(name,".bnt");
}

void
write_bbn_training(PHRASE_LIST pl, char *s) {
  char name[500];
  FILE *fp;
  int i,j;
  BELIEF_NET bbn;
  BNODE *bn;
  
#ifdef WINDOWS
  if (strcmp(s, ACCOMP_TRAIN_ONLY) == 0)  bbn_accomp_training_name(name);
  else bbn_training_name(s, name);
#else
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  //  strcpy(name,scorename);
  strcat(name,"_");
  strcat(name,s);
  strcat(name,".bnt");
#endif
  fp = fopen(name,"w");
  if (fp == NULL) {
    printf("could'nt open %s\n",name);
    exit(0);
  }
  printf("pl.num = %d\n",pl.num);
  for (i=0; i < pl.num; i++) {
    bbn = pl.list[i].net.bn;
    for (j=0; j < bbn.num; j++) { 
      bn = bbn.list[j];
	  if (bn->trainable == 0 /*|| bn->trained == 0*/)  continue; // 7-06 added the || -- only write the actually trained ones
			       // 2-10 took the || out again.
      fprintf(fp,"tag = %s\n",bn->trainable_tag);
      QFprint_file(*(bn->train_dist),fp);
    }
  }
  fclose(fp);
}



void  // for external call to write training
write_the_bbn_training() {
  write_bbn_training(component, SOLO_TRAIN);
}



/*void
modify_trainable_dist(RATIONAL wholerat, char *trainable_tag, QUAD_FORM qf) {
  int i,j;
  BELIEF_NET bbn;
  BNODE *bn;
  
  for (i=0; i < component.num; i++) {
    bbn = component.list[i].net.bn;
    for (j=0; j < bbn.num; j++) { 
      bn = bbn.list[j];
      if (rat_cmp(bn->wholerat,wholerat) != 0) continue;
      bn->trainable_tag = trainable_tag;
      QFcopy(qf,bn->train_dist); 
    }
  }
}

*/

void
read_bbn_training_dists(PHRASE_LIST pl, char *s) {
  char name[500],string[500];
  FILE *fp;
  int i,j,count=0,k;
  BELIEF_NET bbn;
  BNODE *bn;
  TRAINING_RESULTS tr;
  QUAD_FORM qf;
  
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  //  strcpy(name,scorename);
  strcat(name,"_");
  strcat(name,s);
  strcat(name,".bnt");
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("could'nt open %s\n",name);
    exit(0);
  }
  while (feof(fp) == 0) {
    fscanf(fp,"%s",string);
    if (strcmp(string,"tag") == 0) count++;
  }
  fseek(fp,0,SEEK_SET);  /* start at beginning */
  tr.num = count;
  tr.tag = (char **) malloc(tr.num*sizeof(char *));
  tr.qf = (QUAD_FORM *) malloc(tr.num*sizeof(QUAD_FORM));
  for (i=0; i < tr.num; i++) {
    fscanf(fp,"tag = %s\n",string);
    tr.tag[i] = (char *) malloc(strlen(string)+1);
    strcpy(tr.tag[i],string);
    qf = QFread_file(fp);
    tr.qf[i] = QFperm(qf);
  }
  for (i=0; i < pl.num; i++) {
    bbn = pl.list[i].net.bn;
    for (j=0; j < bbn.num; j++) { 
      bn = bbn.list[j];
      if (bn->trainable == 0) continue;
      for (k=0; k < tr.num; k++) {
	if (strcmp(tr.tag[k],bn->trainable_tag) != 0) continue;
	QFcopy(tr.qf[k],bn->train_dist); 
	break;
      }
      if (k == tr.num) printf("uh... no match for %s found\n",bn->trainable_tag);
    }
  }
  fclose(fp);
}

static FILE *
open_bbn_training_file_linux(char *s) {
  char name[500],full_name[500];
  FILE *fp;

  strcpy(name,audio_data_dir);

  strcpy(name,scoretag);
  strcat(name,"_");
  strcat(name,s);
  strcat(name,".bnt");

  strcpy(full_name,audio_data_dir);
  strcat(full_name,name);
  
  fp = fopen(full_name,"r");
  if (fp) {
    printf("found a %s\n",full_name);
    return(fp);
  }
  else printf("very sorry ... could'nt open %s\n",full_name);

  strcpy(full_name,score_dir);
  strcat(full_name,name);

  fp = fopen(full_name,"r");
  if (fp) {
    printf("found b %s\n",full_name);
    printf("xxx\n");
    return(fp);
  }
  else printf("could'nt open %s\n",full_name);
  return(fp);
}


static FILE *
open_bbn_training_file_windows(char *s) { /* arg unused */
  char name[500],full_name[500];
  FILE *fp;


  bbn_training_name(SOLO_TRAIN, name);  /* both */
  fp = fopen(name,"r");
  if (fp) {
    printf("found c %s\n",name);
    return(fp);
  }

  bbn_accomp_training_name(name);
  
  fp = fopen(name,"r");
  if (fp) {
    printf("found d %s\n",name);
    return(fp);
  }
  printf("no training found ... using null model\n");
  return(fp);
}


static FILE *
open_bbn_training_file(char *s) {
  FILE *fp;

#ifdef WINDOWS
  fp = open_bbn_training_file_windows(0);  /* argument unused */
#else
  fp = open_bbn_training_file_linux(s);
#endif
  return(fp);
}





static TRAINING_RESULTS  
get_bbn_training_dists_name(char *name) {
 char string[500];
  FILE *fp;
  int i,j,count=0,k;
  BELIEF_NET bbn;
  BNODE *bn;
  TRAINING_RESULTS tr;
  QUAD_FORM qf;
  
  fp = fopen(name,"r");
  if (fp == NULL) { printf("couldn't open %s\n",name); exit(0); }

  while (feof(fp) == 0) {
    fscanf(fp,"%s",string);
    if (strcmp(string,"tag") == 0) count++;
  }
  fseek(fp,0,SEEK_SET);  /* start at beginning */
  tr.num = count;
  tr.tag = (char **) malloc(tr.num*sizeof(char *));
  tr.qf = (QUAD_FORM *) malloc(tr.num*sizeof(QUAD_FORM));
  for (i=0; i < tr.num; i++) {
    fscanf(fp,"tag = %s\n",string);
    tr.tag[i] = (char *) malloc(strlen(string)+1);
    strcpy(tr.tag[i],string);
    qf = QFread_file(fp);
    tr.qf[i] = QFperm(qf);
    //    printf("tag = %s\n",tr.tag[i]);
    //    Mp(tr.qf[i].m);
  }
  fclose(fp);
  return(tr);
}


TRAINING_RESULTS  
get_bbn_training_dists(char *s, int *flag) {
  char name[500],string[500];
  FILE *fp;
  int i,j,count=0,k;
  BELIEF_NET bbn;
  BNODE *bn;
  TRAINING_RESULTS tr;
  QUAD_FORM qf;
  
  glob_tr.num = tr.num = 0;
  fp = open_bbn_training_file(s);
  if (fp == NULL) { *flag = 0; return(tr); }


  /*  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,"_");
  strcat(name,s);
  strcat(name,".bnt");
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("could'nt open %s\n",name);
    *flag = 0;
    return(tr);
  }
  */

  while (feof(fp) == 0) {
    fscanf(fp,"%s",string);
    if (strcmp(string,"tag") == 0) count++;
  }
  fseek(fp,0,SEEK_SET);  /* start at beginning */
  tr.num = count;
  tr.tag = (char **) malloc(tr.num*sizeof(char *));
  tr.qf = (QUAD_FORM *) malloc(tr.num*sizeof(QUAD_FORM));
  glob_tr = tr;  // so training is accessible elsewhere --- kludge
  for (i=0; i < tr.num; i++) {
    fscanf(fp,"tag = %s\n",string);
    tr.tag[i] = (char *) malloc(strlen(string)+1);
    strcpy(tr.tag[i],string);
    qf = QFread_file(fp);
    tr.qf[i] = QFperm(qf);
    //    printf("tag = %s\n",tr.tag[i]);
    //    Mp(tr.qf[i].m);
  }
  fclose(fp);
  *flag = 1;
  return(tr);
}





void
read_bbn_training_dists_to_score(char *s) {
  char name[500],string[500];
  FILE *fp;
  int i,j,count=0,k;
  BELIEF_NET bbn;
  BNODE *bn;
  TRAINING_RESULTS tr;
  QUAD_FORM qf;
  

  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  //  strcpy(name,scorename);
  strcat(name,"_");
  strcat(name,s);
  strcat(name,".bnt");
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("could'nt open %s\n",name);
    exit(0);
  }
  while (feof(fp) == 0) {
    fscanf(fp,"%s",string);
    if (strcmp(string,"tag") == 0) count++;
  }
  fseek(fp,0,SEEK_SET);  /* start at beginning */
  tr.num = count;
  tr.tag = (char **) malloc(tr.num*sizeof(char *));
  tr.qf = (QUAD_FORM *) malloc(tr.num*sizeof(QUAD_FORM));
  for (i=0; i < tr.num; i++) {
    fscanf(fp,"tag = %s\n",string);
    tr.tag[i] = (char *) malloc(strlen(string)+1);
    strcpy(tr.tag[i],string);
    qf = QFread_file(fp);
    tr.qf[i] = QFperm(qf);
  }
  for (i=0; i < score.midi.num; i++) {
    for (k=0; k < tr.num; k++) {
      if (strcmp(tr.tag[k],score.midi.burst[i].trainable_tag) != 0) continue;
      score.midi.burst[i].qf_update = QFperm(tr.qf[k]);
      break;
    }
    if (k == tr.num) {
      printf("xx no match for %s found\n",score.midi.burst[i].trainable_tag);
      exit(0);
    }
  }
  fclose(fp);
}





TRAIN_FILE_LIST 
read_bbn_training_file(char *name) {
  FILE *fp;
  int i,x,ok;
  char fname[500];
  TRAIN_FILE_LIST tfl;

  tfl.name = (char **) malloc(sizeof(char *)*MAX_TRAIN_FILES);
  tfl.dato = (DATA_OBSERVATION *) malloc(sizeof(DATA_OBSERVATION)*MAX_TRAIN_FILES);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    exit(0);
  }
  for (i=0; i < MAX_TRAIN_FILES; i++) {
    fscanf(fp,"%s",fname);
    tfl.name[i] = (char *) malloc(strlen(fname)+1);
    strcpy(tfl.name[i],fname);
    //    tfl.dato[i] = read_data_observation(tfl.name[i]);
    ok = read_data_observation(tfl.name[i],tfl.dato + i);
    if (ok == 0) i--;
    if (feof(fp)) break;
  }
  tfl.num = i;
  /*  for (i=0; i < tfl.num; i++) 
      printf("%s\n",tfl.name[i]); */
  fclose(fp);
  return(tfl);
}

#define MAX_USEFUL_EXAMPS 3 //5 


static TRAIN_FILE_LIST 
shorten_train_file_list(TRAIN_FILE_LIST tfl) {
  /* get rid of less recent  (higher indexed) examples that don't have useful information */
  int i,j,count[MAX_SOLO_NOTES],useful[MAX_TRAIN_FILES];
  TAGGED_OBS t;
  TRAIN_FILE_LIST tf;

  for (j=0; j < score.solo.num; j++) count[j] =0;
  for (i=0; i < tfl.num; i++) {  // assuming sorted so that most recent examples come first 
    useful[i] = 0;
    for (j=0; j < score.solo.num; j++) {  // if example i is useful contributor to *any* note, take it
      if (tfl.dato[i].num == 0) continue;
      if (assoc_val(tfl.dato[i], score.solo.note[j].observable_tag, &t)) 
	if (count[j] < MAX_USEFUL_EXAMPS && useful[i] == 0) { 
	  useful[i] = 1; 
	  //  printf("%s usefual at %s\n",tfl.name[i],score.solo.note[j].observable_tag);
	} 
    }
    if (useful[i] == 0) continue;
    for (j=0; j < score.solo.num; j++) 
      if (assoc_val(tfl.dato[i], score.solo.note[j].observable_tag, &t)) count[j]++;
  }

  tf.num = 0;
  tf.name = (char **) malloc(sizeof(char *)*tfl.num);
  tf.dato = (DATA_OBSERVATION *) malloc(sizeof(DATA_OBSERVATION)*tfl.num);
  for (i=0; i < tfl.num; i++) if (useful[i]) {
    tf.name[tf.num] = tfl.name[i];
    tf.dato[tf.num] = tfl.dato[i];
    tf.num++;
  }
  return(tf);
}

static int
old_is_current(char *name, HAND_MARK_LIST_LIST *hml) {
  int i,k,j,m;
  char *num,times[200],string[500],s2[500];
  FILE *fp;
  RATIONAL  start,end;
  HAND_MARK_LIST *lcur,*lold;
  
  for (i=0; i < hml->num; i++) if (strcmp(name,hml->examp[i]) < 0) break;
  num = name + strlen(name) - 3;
  get_data_file_name(times, num, "times");
  fp = fopen(times,"r");
  if (fp == NULL) { printf("couldn't find %s for some reason\n",times); exit(0); }
  fscanf(fp,"%s = %s\n",s2,string);
  start = string2wholerat(string);
  fscanf(fp,"%s = %s\n",s2,string);
  end = string2wholerat(string);
  fclose(fp);
  lold = hml->snap + i;
  lcur = hml->snap + hml->num-1;  // the last list is the current setting
  for (j= lold->num-1; j >= 0; j--) if (rat_cmp(lold->list[j].pos,start) <= 0) break;
  for (k= lcur->num-1; k >= 0; k--) if (rat_cmp(lold->list[k].pos,start) <= 0) break;
  for (m=0; ; m++) {
    if ((j+m == lold->num) && (k+m == lcur->num)) break;  // done with both lists at same time
    if ((j+m == lold->num) || (k+m == lcur->num)) return(0);  // one list longer than other
    if ((rat_cmp(lold->list[j+m].pos,end) >= 0) && (rat_cmp(lcur->list[k+m].pos,end) >= 0)) break;
    if (rat_cmp(lold->list[j+m].pos,lcur->list[k+m].pos) != 0) return(0);
    if (strcmp(lold->list[j+m].type,lcur->list[k+m].type) != 0) return(0);
    if (strcmp(lold->list[j+m].type,"tempo_change") != 0) continue;
    if (rat_cmp(lold->list[j+m].unit,lcur->list[k+m].unit) != 0) return(0);
    if (lold->list[j+m].bpm != lcur->list[k+m].bpm)  return(0);
  }
  /*  printf("using file %s for %s num = %s\n",hml->examp[i],name,num);
  printf("%d/%d\n",start.num,start.den);
  printf("%d/%d\n",end.num,end.den);*/
  return(1);
}


static int
is_current(char *name, HAND_MARK_LIST *cur_hnd) {
  int i,k,j,m,jj,kk;
  char *num,times[200],string[500],s2[500],hnd[200],cur[200];
  FILE *fp;
  RATIONAL  start,end;
  HAND_MARK_LIST *old_hnd,lold;
  
  num = name + strlen(name) - 3;  // the three digit suffix

  get_data_file_name(hnd, num, "hnd");
  fp = fopen(hnd,"r");
  if (fp == 0) return(1);  // if no .hnd file assume match
  else fclose(fp);

  old_hnd = &lold;
  read_hand_mark(old_hnd, hnd);
  //  get_player_file_name(cur,"hnd");
  //  read_hand_mark(&lcur, cur);  // this should be passed rather than read each time ...


  get_data_file_name(times, num, "times");
  fp = fopen(times,"r");
  if (fp == NULL) { printf("couldn't find %s for some reason\n",times); exit(0); }
  fscanf(fp,"%s = %s\n",s2,string);
  start = string2wholerat(string);
  fscanf(fp,"%s = %s\n",s2,string);
  end = string2wholerat(string);
  fclose(fp);
  
  for (j= old_hnd->num-1; j >= 0; j--) if (rat_cmp(old_hnd->list[j].pos,start) <= 0) break;
  for (jj=0; jj < old_hnd->num; jj++) if (rat_cmp(old_hnd->list[jj].pos,end) >= 0) break;
  for (k= cur_hnd->num-1; k >= 0; k--) if (rat_cmp(old_hnd->list[k].pos,start) <= 0) break;
  for (kk=0; kk < cur_hnd->num; kk++) if (rat_cmp(cur_hnd->list[kk].pos,end) >= 0) break;
  if ((jj - j) != (kk - k)) return(0);  // must have same number of relevant entries
  
  for (m=0; m < (jj -j); m++) {
    //    if ((j+m == old_hnd->num) && (k+m == lcur.num)) return(1);  // done with both lists at same time
    //    if ((rat_cmp(old_hnd->list[j+m].pos,end) >= 0) && (rat_cmp(cur_hnd->list[k+m].pos,end) >= 0)) return(1);
    if (rat_cmp(old_hnd->list[j+m].pos,cur_hnd->list[k+m].pos) != 0) return(0);
    if (strcmp(old_hnd->list[j+m].type,cur_hnd->list[k+m].type) != 0) return(0);
    if (strcmp(old_hnd->list[j+m].type,"tempo_change") != 0) continue;
    if (rat_cmp(old_hnd->list[j+m].unit,cur_hnd->list[k+m].unit) != 0) return(0);
    if (old_hnd->list[j+m].bpm != cur_hnd->list[k+m].bpm)  return(0);
  }
  return(1);
}


static void
get_hand_marks(HAND_MARK_LIST *cur_hnd) {
  char name[300];

  get_player_file_name(name,"hnd");  // first check the player's personal directory
  if (read_hand_mark(cur_hnd, name)) return; 
  get_score_file_name(name, ".hnd");  // otherwise check the score directory
  if (read_hand_mark(cur_hnd, name)) return; 
  cur_hnd->num = 0;   // otherwise assume no hand marks
}


TRAIN_FILE_LIST 
get_bbn_training_file_max() {
  /* chooses a subset of the whole collection of files.  The subset prefers later examples than
     earlier ones and skips examples if they offer no note examples that have already been seen
     at least MAX_USEFUL_EXAMPS times.  At present each example is either chosen or not with the
     decision being the same for all phrases.  May want to refine this so that an example could be
     used for one phrase but not another.  */
  FILE *fp;
  int i,x,n,j,ok;
  char fname[200],name[200],*num,*tempc,cur[200];
  TRAIN_FILE_LIST tfl;
  DATA_OBSERVATION tempd;
  HAND_MARK_LIST cur_hnd;
  //  HAND_MARK_LIST_LIST hml;

  //  read_hand_mark_lists(&hml);

#ifdef WINDOWS
  get_player_file_name(name,BBN_EX_SUFF);
#else
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".sex");
#endif

  tfl.name = (char **) malloc(sizeof(char *)*MAX_TRAIN_FILES);
  tfl.dato = (DATA_OBSERVATION *) malloc(sizeof(DATA_OBSERVATION)*MAX_TRAIN_FILES);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    tfl.num = 0;
    return(tfl);
  }


  get_hand_marks(&cur_hnd);
  //  get_player_file_name(cur,"hnd");
  //  read_hand_mark(&cur_hnd, cur); 
  for (n=0; n < MAX_TRAIN_FILES; ) {
    //    fscanf(fp,"%[^n]",fname);  // do I need another fscanf for remainder of line?
    fscanf(fp,"%s",fname);  // do I need another fscanf for remainder of line?
    if (feof(fp)) break;
    /*    if (is_current(fname,&cur_hnd) == 0) {
      printf("not using %s since not consistent with current hand settings\n",fname);
      continue;
      }*/
    num = fname + strlen(fname) - 3;
    get_data_file_name(name, num, "atm");
    tfl.name[n] = (char *) malloc(strlen(name)+1);
    strcpy(tfl.name[n],name);
    //    tfl.dato[n] = read_data_observation(tfl.name[n]);
    ok = read_data_observation(tfl.name[n], tfl.dato + n);
    if (ok) n++;
  }
  fclose(fp);
  for (j = 0; j < n/2; j++) {  // reverse the list
    tempc = tfl.name[j]; tfl.name[j] = tfl.name[n-1-j]; tfl.name[n-1-j] = tempc; 
    tempd = tfl.dato[j]; tfl.dato[j] = tfl.dato[n-1-j]; tfl.dato[n-1-j] = tempd; 
  }
  tfl.num = n;
  tfl = shorten_train_file_list(tfl);
  for (j=0; j < tfl.num; j++) printf("training on %s\n",tfl.name[j]);
  return(tfl);
}


static TRAIN_FILE_LIST 
get_last_bbn_training_file() {
  /* makes TRAIN_FILE_LIST using only last training example  */
  FILE *fp;
  int i,x,n,j,ok;
  char fname[200],name[200];
  TRAIN_FILE_LIST tfl;

#ifdef WINDOWS
  get_player_file_name(name,BBN_EX_SUFF);
#else
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".sex");
#endif
  tfl.name = (char **) malloc(sizeof(char *)*MAX_TRAIN_FILES);
  tfl.dato = (DATA_OBSERVATION *) malloc(sizeof(DATA_OBSERVATION)*MAX_TRAIN_FILES);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    tfl.num = 0;
    return(tfl);
  //  exit(0);
  }
  for (n=0; ; n++) {
    fscanf(fp,"%s",fname);
    if (feof(fp)) break;
  }
  fclose(fp);
  if (strlen(fname) < 3) { printf("this can't be right: fname = %s\n",fname); exit(0); }
#ifdef WINDOWS
  strcat(fname,".atm");
  strcpy(name,user_dir);
   strcat(name,AUDIO_DIR);
  strcat(name,"/");
  strcat(name,player);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,fname);
  strcpy(fname,name);
#endif
  tfl.name[0] = (char *) malloc(strlen(fname)+1);
  strcpy(tfl.name[0],fname);
  //  tfl.dato[0] = read_data_observation(tfl.name[0]);
  ok = read_data_observation(tfl.name[0],tfl.dato + 0);
  tfl.num = (ok) ? 1 : 0;
  return(tfl);
}




void
play_accomp_performance() {
  char accom_times[500],tag[500],vel_file[500],start[500],end[500];
  FILE *fp;
  int i,vel,j;
  float t,t0;
  RATIONAL s,e;
  

  printf("enter the name of the accompaniment times file (with .atm suffix): ");
  scanf("%s",accom_times);
  fp = fopen(accom_times,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",accom_times);
    exit(0);
  }
  for (i=0; i < score.midi.num; i++) {
    fscanf(fp,"%s\t%f\n",tag,&t);
    /*    printf("%s %s\n",tag,score.midi.burst[i].observable_tag); */
    if (strcmp(tag,score.midi.burst[i].observable_tag) != 0) { printf("bad match here\n"); exit(0); }
    score.midi.burst[i].ideal_secs = t;
  }
  fclose(fp);
  /*  read_midi_velocities(); */



  /*printf("enter the full name of the velocity file: ");
  scanf("%s",vel_file);
  fp = fopen(vel_file,"r");
  if (fp == NULL)  printf("couldn't open %s\n",accom_times);
  else while (!feof(fp)) {
    fscanf(fp,"%s %d\n",tag,&vel);
    for (i=0; i < score.accompaniment.num; i++)
      if (strcmp(tag,score.accompaniment.note[i].observable_tag) == 0) break;
    if (i == score.accompaniment.num) {
      printf("no match for %s\n",tag);
      exit(0);
    }
    score.accompaniment.note[i].note_on->volume = vel;
  }
  fclose(fp); */
  printf("enter the measure position (d+d/d) where you would like to begin: \n");
  scanf("%s",start);
  s = string2wholerat(start);
  printf("enter the measure position (d+d/d) where you would like to end: \n");
  scanf("%s",end);
  e = string2wholerat(end);
  for (i=0; i < score.midi.num; i++) {
    if (rat_cmp(s,score.midi.burst[i].wholerat) == 0) t0 = score.midi.burst[i].ideal_secs -1;
    if (rat_cmp(s,score.midi.burst[i].wholerat) <= 0) score.midi.burst[i].ideal_secs -= t0;
    /*     for (j=0; j < score.midi.burst[i].action.num; j++) 
	   printf("%x %d %d\n",score.midi.burst[i].action.event[j].command,score.midi.burst[i].action.event[j].volume,score.midi.burst[i].action.event[j].notenum); */
   }
  mode = NO_SOLO_MODE;
  newer_start_cond(s,e);
  printf("cur_accomp = %d last_accomp = %d\n",cur_accomp,last_accomp);

  queue_event();
  while (cur_accomp <= last_accomp) Sleep(1);/* printf("cur accomp = %d last_accomp = %d\n",cur_accomp,last_accomp); */
  end_midi();

}


void synthesize_mean_accomp() {
  int i,j,index;
  PHRASE_LIST pl;
  NETWORK net;
  BNODE *b;
  float m,v,t;
  QUAD_FORM qf;
  FILE *fp;
  char fname[500];

  pl = make_accomp_bbn_graph();
  read_bbn_training_dists(pl,"accom");  
  for (i=0; i < pl.num; i++) {
    net = pl.list[i].net;
    net.ct = bn2ct_pot(&(net.bn));   
    for (j=0; j < net.bn.num; j++) {
      b = net.bn.list[j];
      index = b->index;
      if (score.midi.burst[index].acue == 0) continue;
      if (i == 0) m = 0;
      else get_accom_mean(index-1,&m,&v); 
      fix_obs(b, m+1);  
      break;
    }
    if (j == net.bn.num) { printf("couldn't find start of phrase\n"); exit(0); }
    equilibrium(net.ct);    
  }
  strcpy(fname,scorename);
  strcat(fname,".atm");
  fp = fopen(fname,"w");
  for (i=0; i < score.midi.num; i++) {
    get_accom_mean(i,&m,&v); 
    score.midi.burst[i].ideal_secs = m;
    fprintf(fp,"%s\t%f\n",score.midi.burst[i].observable_tag,m);
    /*    if (i < 10)    printf("event %d at %f (v = %f)\n",i,m,v);*/
  }
  fclose(fp);
  /*  synthesize_accomp_performance(); */
  return;
  /*  write_seq("temp");
      return; */
  mode = NO_SOLO_MODE;
  last_accomp = score.midi.num-1;
  new_start_cond(0.,1000.,1.) ;
  cur_accomp = 0;
  queue_event();
  while (cur_accomp <= last_accomp) Sleep(1);/* printf("cur accomp = %d last_accomp = %d\n",cur_accomp,last_accomp); */
  end_midi();
}

void bbn_training_test() {
  TRAIN_FILE_LIST tfl;
  int i;
  PHRASE_LIST pl;
  char train_file[500];

  /*                synthesize_mean_accomp();
		    exit(0);       */
  pl = make_accomp_bbn_graph();
  /*  read_bbn_training_dists(pl,"accom");   */
  printf("enter the name of the training file\n");
  scanf("%s",train_file);
  printf("training file is %s\n",train_file);
  tfl = read_bbn_training_file(train_file);
  for (i=0; i < pl.num; i++) {
    printf("training phrase %d\n",i);
    bbn_train(pl.list[i].net,tfl,BBN_TRAIN_ITERS,MEANS_AND_VARS);
  }
  write_bbn_training(pl,"accom");
}


static NETWORK
init_network(int phrase) {
  NETWORK n;
  QUAD_FORM jn;
  
  printf("creating network %d\n",phrase);
  /*  n.bn = make_phrase_graph(phrase);
      n.ct = bn2ct(&(n.bn)); */

  printf("experimental network\n");
  n.bn = make_indep_phrase_graph(phrase);
  /*  n.bn = make_indep_graph();*/
  n.ct = exper_bn2ct(&(n.bn)); 
  


  /*  make_tex_graph(n.bn);
exit(0);               */

  /*        fix_solo_obs(141,0.);     */
  equilibrium(n.ct);
  /*  print_clique(n.ct.list[1]->clique); */
  /*    QFprint(n.ct.list[1]->clique->qf);  */
  /*            fix_solo_obs(0,0.);       */
      /*      equilibrium(n.ct); 
      QFprint(n.ct.list[1]->clique->qf);
      exit(0); */


  


  /*             jn = post_dist(n.bn.list[0]);
  QFprint(jn);
  exit(0);     */
  return(n);
}



void
compute_flow_spaces(void) { 
  int i,num,flush;
  float m,v,time,next_time,error,meas;
  char update[500],word[500],name[500];
  FILE *fp;

  cur_accomp = 0;
  fp = fopen("habanera.ut","r");
  while (!feof(fp)) {
    fscanf(fp,"%s %s %d %f\n",update,word,&num,&time);
    printf("%s %s %d %f\n",update,word,num,time);
    if (strcmp(update,"solo") == 0)  update_belief_solo(num, time);
      
    else if (strcmp(update,"accom") == 0) {
      cur_accomp = num+1;
      update_belief_accom(num, time, flush=1);
    }
    else { printf("unknown update\n"); exit(0); }
  }
}



static NETWORK
exper_init_network() {
  NETWORK n,*np;
  QUAD_FORM jn;
  PHRASE_LIST pl;
  int i;
  
  printf("creating network\n");
  printf("experimental network\n");
  n.bn = make_indep_graph();
    make_connected_components(&n.bn,&component);
  for (i=0; i < component.num; i++) {
    /*    n = component.list[i].net;
    n.ct = exper_bn2ct(&(n.bn)); 
    equilibrium(n.ct);
    print_clique(n.ct.list[0]->clique);  */

    component.list[i].net.ct = exper_bn2ct(&(component.list[i].net.bn)); 
    equilibrium(component.list[i].net.ct);
  }

  /*  if (strcmp("habanera",scorename) == 0)  compute_flow_spaces(); 
  for (i=0; i < component.num; i++) {
    init_cliques_with_probs(component.list[i].net.ct, component.list[i].net.bn); 
    equilibrium(component.list[i].net.ct);
    }*/


  return(n);  /* return garbage */

}


static void
check_forest() {
  int i,firsta,firsts,lasta,lasts;

  printf("starting forset\n");
  for (i=0; i < cue.num; i++) {
    phrase_span(i,&firsts,&lasts,&firsta,&lasta);
    printf("phrase is %d--%d  %d--%d\n",firsts,lasts,firsta,lasta);
    cue.list[i].net = init_network(i);
  }
  printf("ending forset\n");
}


static int
phrase_active(int phrase) {
  int firsta,firsts,lasta,lasts;


  phrase_span(phrase,&firsts,&lasts,&firsta,&lasta);
  printf("phrase = %d firsts = %d lasts = %d firstnote = %d lastnote = %d\n",phrase,firsts,lasts,firstnote,lastnote); 
  return(firsts < lastnote && lasts > firstnote);
} 

void
init_forest(int fnote, int lnote) {
  int i,firsta,firsts,lasta,lasts;

 
 for (i=0; i < cue.num; i++) {
    if (phrase_active(i))  cue.list[i].net = init_network(i);
  }
}  

void
exper_init_forest() {
  int i,firsta,firsts,lasta,lasts;

  exper_init_network();
}  


static void
fast_recalc_next_accomp(float ideal) {
  float m,v,now();

  score.midi.burst[cur_accomp].ideal_secs = ideal;
    queue_event(); 
    /*   async_queue_event();  */
}


//CF:  num= index of solo note in score
//CF:  secs = time that note onset was observed
void 
update_belief_solo(int num, float secs) {  /* update belief net in response to 
					      fsolo note detection */
  int hurry = 0,flush;
  float m,v;
  float t1,t2,now(),next_time;
  BNODE *bn;
  QUAD_FORM qf;

  /*    static int xxx; */

  /*  printf("solo update %d %f\n",num,secs);*/

  /*    if (fabs(score.solo.note[num].time - 18.) < .001) 
	printf("updating at %f\n",now());  */
  /*  printf("  note = %d on line = %f off_line = %f\n",num,secs,score.solo.note[num].off_line_secs); */

  /*    if (xxx) return; */
/*  printf("note %d detected at %f cur_accomp = %d\n",num,secs,cur_accomp);     */
  score.midi.burst[cur_accomp].ideal_set = now();   //CF:  storing debug/inspection information


  /*  fix_solo_obs(num,secs); */

  /*  fix_obs(score.solo.note[num].belief,secs);
  t1 = score.midi.burst[cur_accomp].time;
  t2 = score.solo.note[num].time;
  if (score.solo.note[num].cue && t1 == t2) fast_recalc_next_accomp(secs);
  else  if (cur_accomp <= last_accomp) recalc_next_accomp();   */

  add_belief_queue(score.solo.note[num].belief, secs, flush=1);  //CF:  semaphor-like mechanism -- RUN THE BBN!
#ifdef ROMEO_JULIET
  /*        bn = score.solo.note[num].update_node;
  if (bn == NULL) return;
  collect(bn->clnode,0);   
  qf = post_dist(bn);
  if (qf.m.el[0][0] < 0) {
    printf("before solo note %d update qf = \n",num);
    QFprint(qf);
    fix_obs(bn,0.);  // fix the POS (agogic) variable to 0
    qf = post_dist(bn);
    printf("after solo note %d update qf = \n",num);
    QFprint(qf);
    }*/
#endif


  /*  recompute_solo(num+1);
    get_solo_mean(num+1,&m,&v);
    next_time = toks2secs(score.solo.note[num+1].realize); */
    /*    printf("note = %d predict = %f std = %f actual = %f\n",num+1,m,sqrt(v),next_time);*/


    /*  collect(score.solo.note[num+1].hidden->clnode,0);  
  get_hidden_solo_mean(num+1,&m,&v);
  printf("next solo predicted at %f\n",m); */
     /*         xxx = 1; */
  //  if (midi_accomp) queue_midi_if_needed();

}


void 
update_belief_accom(int num, float secs, int flush) {  /* update belief net in response to 
					       a played accomp note */
    /*    fix_accom_obs(num, secs);*/

  /*  printf("accom update %d %f\n",num,secs);*/

  /*    fix_obs(score.midi.burst[num].belief,secs);
  recalc_next_accomp();  */
  add_belief_queue(score.midi.burst[num].belief, secs, flush);  
    //  if (midi_accomp) queue_midi_if_needed();

}


/*#define THE_PHRASE 4 */
#define THE_PHRASE 2




static void
test_solo_predict(void) {  /* seems to assume that an audio file has been parsed. */
  int i;
  float m,v,time,next_time,error,meas;
  char name[500];

  /*  init_forest(firstnote,lastnote);*/
  exper_init_forest();
  for (i=firstnote; i <= lastnote-1; i++) {
    time = toks2secs(score.solo.note[i].realize);
    if (i == firstnote) 
      printf("time of first note is %f\n",toks2secs(score.solo.note[i].realize));
    next_time = toks2secs(score.solo.note[i+1].realize);
    /*    printf("%f %f\n",score.solo.note[i+1].realize,next_time);*/
    fix_solo_obs(i,time);
    recompute_solo(i+1);
    get_solo_mean(i+1,&m,&v);
    error = (next_time-m); 
    num2name(score.solo.note[i+1].num,name);
    meas = score.solo.note[i+1].time;
    printf("meas = %5.2f\tname = %s\tpredict = %5.3f\tactual = %5.3f\terror  = %5.3f\n",meas,name,m,next_time,error);
  } 
}


void
posterior_test(void) {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i;
  BNODE *one,*two;
  CLIQUE_NODE *cur;
  CLIQUE *snd,*mid;
  QUAD_FORM qf,prom;
  

  bn = old_make_solo_graph(0,8); 
  for (i=0; i < bn.num; i++) {
    if (bn.list[i]->note_type == SOLO_PHRASE_START && bn.list[i]->index == 0)
      one = bn.list[i];
    if (bn.list[i]->note_type == SOLO_NODE && bn.list[i]->index == 8)
      two = bn.list[i];
  }
  add_dir_arc(one,two,Mzeros(SOLO_DIM,SOLO_DIM));
  ct = bn2ct(&bn); 
  init_cliques_with_probs(ct, bn); /* assigns probabilites */
  equilibrium(ct);    
  cur = ct.list[0];
  snd = cur->clique;
  mid = cur->neigh.bond[0].intersect;
  prom = promote_sep(snd,mid,mid->qf);  
  qf = QFminus(snd->qf,prom);
  QFprint(qf);

}
  

    
  
static void
find_lead_sections(int *lp, int *rp) {
  int i,j;

  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].acue) {
    for (j=i; j < score.midi.num; j++) {
      if (score.solo.note[lp[j]].cue) break;
      score.midi.burst[j].node_type = ACCOMP_LEAD_NODE;
      printf("accomp_lead is %d\n",j);
    }
    for (j=i; j < score.midi.num; j++) if (fabs(score.solo.note[rp[i]].time - score.midi.burst[j].time) < .0001) {
      score.midi.burst[j].node_type = LEAD_CONNECT_NODE;
      printf("lead_connect is %d\n",j);
      break;
    }
  }      
}    


#define SANDWICH_PATCH 0       /* accompaniment notes sandwiched between two solo notes */
#define ACCOMP_LEAD_PATCH 1    /* cue the accompaniment off of shead, but use the tempo from accomp model */
#define SINGLE_NOTE_PATCH 2    /* a single accompaniment note hanging off shead */
#define PHRASE_TRAIL_PATCH 3   /* accomp patch hangs off shead */



typedef struct {
  int ahead;
  int atail;
  int shead;
  int stail;
  int type;
} ACCOMP_PATCH;


typedef struct {
  int num;
  ACCOMP_PATCH list[MAX_ACCOMP_NOTES];
} PATCH_LIST;



static void
add_patch_list(ACCOMP_PATCH p, PATCH_LIST *l) {

  if (l->num == MAX_ACCOMP_NOTES) {
    printf("out of room in add_patch_list\n");
    exit(0);
  }
  l->list[l->num++] = p;
}


static int
first_accomp_ge_solo(int i) {
  int j;

  for (j=0; j < score.midi.num; j++) 
    if (rat_cmp(score.midi.burst[j].wholerat,score.solo.note[i].wholerat) >= 0) break;
  return(j);
}

static int
first_solo_ge_accomp(int i, int *flag) {
  int j;

  *flag = 0;
  for (j=0; j < score.solo.num; j++) 
    if (rat_cmp(score.solo.note[j].wholerat,score.midi.burst[i].wholerat) >= 0) break;
  if (j == score.solo.num) *flag = 1;
  return(j);
}

static int
last_accomp_le_solo(int i) {
  int j;

  for (j=score.midi.num-1; j >= 0; j--) 
    if (rat_cmp(score.midi.burst[j].wholerat, score.solo.note[i].wholerat) <= 0) break;
  return(j);
}

static int
solo_accomp_eq(int i,int j) {
  if (i < 0 || i >= score.solo.num) {
    printf("solo index out of range\n");
    exit(0);
  }
  if (j < 0 || j >= score.midi.num) {
    printf("accomp index out of range (%d %d)\n",j,score.midi.num);
    exit(0);
  }
  return((rat_cmp(score.midi.burst[j].wholerat, score.solo.note[i].wholerat) == 0));
}




static void
set_lead_list(PATCH_LIST *pl) {
  int i,j,k,l,flag;
  ACCOMP_PATCH p;
  
  p.type = ACCOMP_LEAD_PATCH;
  for (i=0; i < score.midi.num; i++) {
    if (score.midi.burst[i].acue == 0) continue;
    j = first_solo_ge_accomp(i,&flag);
    if (flag == 0 && score.solo.note[j].cue && solo_accomp_eq(j,i)) continue;  
    /* if solo cue coincides with accomp cue then accomp not leading */
    printf("cue at %d time = %d/%d\n",i,score.midi.burst[i].wholerat.num,score.midi.burst[i].wholerat.den);
    for (j=0; j < score.solo.num; j++) {
      if (score.solo.note[j].cue == 0) continue;   /* just brought this back */
      if (rat_cmp(score.solo.note[j].wholerat, score.midi.burst[i].wholerat) <= 0) continue;
      /*      if (score.solo.note[j].time - .001 < score.midi.burst[i].time) continue;*/
      break;  /* first solo cue > accomp cue */
    }
    
    if (j == score.solo.num) k = score.midi.num-1;
    else for (k=score.midi.num-1; k >= 0; k--) 
      /*      if (score.midi.burst[k].time -.0001 < score.solo.note[j].time) break;*/
      if (rat_cmp(score.midi.burst[k].wholerat, score.solo.note[j].wholerat) <= 0) break;
    if ((strcmp(scorename,"reverie") == 0)  && (strcmp(score.solo.note[j].observable_tag,"59+0/1_fs6") == 0))
      k--;  /* kludge for cue at measure 59 */
    for (l=0; l < score.solo.num; l++) 
      if (score.solo.note[l].time + .0001 > score.midi.burst[i].time) break;
    p.ahead = i;
    p.atail = k;
    p.shead = l;
      printf("ahead = %f atail = %f shead = %f\n",score.midi.burst[p.ahead].time,score.midi.burst[p.atail].time,score.solo.note[p.shead].time); 
      printf("ahead = %s atail = %s shead = %s\n",score.midi.burst[p.ahead].observable_tag,score.midi.burst[p.atail].observable_tag,score.solo.note[p.shead].observable_tag); 
    add_patch_list(p,pl);
    if (p.ahead > p.atail) {
      printf("lead list patch ahead = %d atail = %d shead = %d stail = %d\n",p.ahead,p.atail,p.shead,p.stail);
      exit(0);
    }
  }
}



static void
recent_set_non_lead_list(PATCH_LIST *pl) {
  int i,j,k,b1,b2;
  ACCOMP_PATCH p;

  for (i=0; i < score.solo.num-1; i++) {
    p.shead = i;
    p.stail = i+1;
    for (j=0; j < score.midi.num; j++) if (score.midi.burst[j].time + .0001 > score.solo.note[i].time) break;
    p.ahead = j;
    if (score.midi.burst[p.ahead].time + .0001  > score.solo.note[i+1].time) continue;   /* nothing between */
    for (j=score.midi.num-1; j >= 0; j--) if (score.midi.burst[j].time - .0001 < score.solo.note[i+1].time) break;
    p.atail = j;
    for (k=0; k < pl->num; k++) {
      if (pl->list[k].type != ACCOMP_LEAD_PATCH) continue;
      if (p.ahead <= pl->list[k].ahead && p.atail >= pl->list[k].ahead) 
	while (p.atail >=  pl->list[k].ahead) p.atail--;
      if (p.ahead <= pl->list[k].atail && p.atail >= pl->list[k].atail) 
	while (p.ahead <=  pl->list[k].atail) p.ahead++;
      if (p.ahead > pl->list[k].atail || p.atail < pl->list[k].ahead)  continue;
      while (p.atail >=  pl->list[k].ahead) p.atail--; 
      while (p.ahead <=  pl->list[k].atail) p.ahead++;
    }
    /*    for (k=0; k < pl->num; k++) {
      if (pl->list[k].type != ACCOMP_LEAD_PATCH) continue;
      if (p.ahead > pl->list[k].atail || p.atail < pl->list[k].ahead)  continue;
      while (p.atail >=  pl->list[k].ahead) p.atail--; 
      while (p.ahead <=  pl->list[k].atail) p.ahead++;
    }*/
    b1 = (fabs(score.midi.burst[p.ahead].time - score.solo.note[p.shead].time) < .0001);
    b2 = (fabs(score.midi.burst[p.atail].time - score.solo.note[p.stail].time) < .0001);
    if (p.ahead > p.atail) continue;
    if ((p.ahead == p.atail) && b2) continue;
    if (p.ahead == p.atail && (score.midi.burst[p.ahead].time - .0001 < score.solo.note[i].time)) 
      p.type = SINGLE_NOTE_PATCH;
    else if ((b1 && b2) && ((p.ahead+1) == p.atail)) p.type = SINGLE_NOTE_PATCH;
    else if (score.solo.note[i+1].cue)     p.type =  PHRASE_TRAIL_PATCH;
    else p.type = SANDWICH_PATCH;
    add_patch_list(p,pl);
  }
}

static ACCOMP_PATCH
separate_from_lead_patches(ACCOMP_PATCH p, PATCH_LIST *pl) {
  int k,lhead,ltail;

  for (k=0; k < pl->num; k++) {
    if (pl->list[k].type != ACCOMP_LEAD_PATCH) continue;
    lhead = pl->list[k].ahead;
    ltail = pl->list[k].atail;
    if (p.ahead <= lhead && p.atail >= lhead) /* p contains lhead */
	while (p.atail >=  lhead) p.atail--;
    if (p.ahead <= ltail && p.atail >= ltail) /* p contains ltail */
      while (p.ahead <=  ltail) p.ahead++;
    if (p.ahead > ltail || p.atail < lhead)  continue;  /* no intersection anymore */
    while (p.atail >=  lhead) p.atail--; 
    while (p.ahead <=  ltail) p.ahead++;
  }
  return(p);
}


static void
set_non_lead_list(PATCH_LIST *pl) {
  int i,j,k,b1,b2,a,b,lasti;
  ACCOMP_PATCH p;

  for (i=0; i < score.solo.num; i++) {
    p.shead = i;
    p.stail = i+1;
    p.ahead = first_accomp_ge_solo(i);
    if (p.ahead == score.midi.num) continue; 
    if (i != score.solo.num-1) {
      if (rat_cmp(score.midi.burst[p.ahead].wholerat, score.solo.note[i+1].wholerat) >= 0) continue;   /* nothing between */
      else p.atail = last_accomp_le_solo(i+1);
    }
    else p.atail = score.midi.num-1;

    /*    printf("i = %d atail = %d\n",i,p.atail); */

    /*   for (k=0; k < pl->num; k++) {
	  if (pl->list[k].type != ACCOMP_LEAD_PATCH) continue;
       if (p.ahead <= pl->list[k].ahead && p.atail >= pl->list[k].ahead)  
	 while (p.atail >=  pl->list[k].ahead) p.atail--;
       if (p.ahead <= pl->list[k].atail && p.atail >= pl->list[k].atail) 
 	while (p.ahead <=  pl->list[k].atail) p.ahead++;
       if (p.ahead > pl->list[k].atail || p.atail < pl->list[k].ahead)  continue;
       while (p.atail >=  pl->list[k].ahead) p.atail--; 
       while (p.ahead <=  pl->list[k].atail) p.ahead++;
       } */

    p = separate_from_lead_patches(p,pl); 
    if (p.ahead > p.atail) continue;





    /*    printf("i = %d %d %d %d %d\n",i,p.shead,p.stail,p.ahead,p.atail);*/
    b1 = solo_accomp_eq(p.shead, p.ahead);
    b2 = (i < score.solo.num-1) ? solo_accomp_eq(p.stail, p.atail) : 0;
    /*  if (i == score.solo.num-1) {
      printf("b2 = %d\n",b2);
      printf("p.stail = %d solo num = %d\n",p.stail,score.solo.num);
      printf("%d/%d\n",score.solo.note[p.stail].wholerat.num,score.solo.note[p.stail].wholerat.den);
      printf("%d/%d\n",score.midi.burst[p.atail].wholerat.num,score.midi.burst[p.atail].wholerat.den);
      exit(0);
    }
    */

    /*    if (p.ahead > p.atail) continue;*/
    if ((p.ahead == p.atail) && b2) continue;
    if (p.ahead == p.atail && (rat_cmp(score.midi.burst[p.ahead].wholerat, score.solo.note[i].wholerat) <= 0))
      p.type = SINGLE_NOTE_PATCH;
    else if ((b1 && b2) && ((p.ahead+1) == p.atail)) p.type = SINGLE_NOTE_PATCH;
    else if (score.solo.note[i+1].cue || i == (score.solo.num-1))   p.type =  PHRASE_TRAIL_PATCH;
    else p.type = SANDWICH_PATCH;
    add_patch_list(p,pl);
  }
}

set_patch_list(PATCH_LIST *pl) {
  int i,a,b;

  pl->num = 0;
  set_lead_list(pl);
  set_non_lead_list(pl);
  for (i=0; i < pl->num; i++) {
    a = pl->list[i].ahead;
    b = pl->list[i].atail;
    /*                 printf("type = %d ahead = %d atail = %d shead = %d stail = %d (%f -- %f) shead_time = %f\n",pl->list[i].type,pl->list[i].ahead,pl->list[i].atail,pl->list[i].shead,pl->list[i].stail,score.midi.burst[a].time,score.midi.burst[b].time,score.solo.note[pl->list[i].shead].time);      */
  }
}

static BNODE*
solo_anchor(int solo_num) {
  BNODE *hidden,*node;
  int i,type;

  hidden = score.solo.note[solo_num].hidden;
  for (i=0; i < hidden->neigh.num; i++) {
    node = hidden->neigh.arc[i];
    type = node->note_type;
    if (type == ANCHOR_NODE || type == PHANTOM_NODE) return(node);
  }
  return(NULL);
}


static void
create_single_note_patch(ACCOMP_PATCH p, BELIEF_NET *bn) {
  QUAD_FORM e;
  BNODE *hang;
  CLIQUE *cl;

  if (solo_anchor(p.shead)) return;
  hang  = alloc_accomp_node_pot(ACCOM_DIM, ANCHOR_NODE,p.ahead, bn);
  e = QFpos_tempo_sim();
  cl = make_conditional_potential(score.solo.note[p.shead].hidden,hang, Miden(SOLO_DIM), e);
  add_potential(bn,cl); 
}


static BNODE*
add_solo_connect(int sn, int an, BELIEF_NET *bn) {
  int coincides;
  QUAD_FORM e;
  BNODE *hang;
  CLIQUE *cl;

  hang = solo_anchor(sn);
  if (hang) return(hang); /* already there */

  coincides = (fabs(score.solo.note[sn].time - score.midi.burst[an].time) < .0001); 
  /*  printf("solo = %s acc = %s conincides = %d\n",score.solo.note[sn].observable_tag,score.midi.burst[an].observable_tag,coincides); */
  if (coincides)   hang  = alloc_accomp_node_pot(ACCOM_DIM, ANCHOR_NODE,an, bn);
  else  hang  = alloc_belief_node_pot(ACCOM_DIM, PHANTOM_NODE,sn, bn);

  e = QFpos_tempo_sim();   /*most recent*/
      /*      e = QFpos_tempo_very_sim();  */
	/*          e = QFhold_steady();    */
  cl = make_conditional_potential(score.solo.note[sn].hidden,hang, Miden(SOLO_DIM), e);
  add_potential(bn,cl); 
}


static void
condition_patch(CLIQUE *bound, ACCOMP_PATCH p, BELIEF_NET *bn) {
  int i,j,flag;

  for (i=0; i < bn->num; i++) bn->list[i]->focus = 0;
  for (i=p.ahead; i <= p.atail; i++) score.midi.burst[i].hidden->focus = 1;
  for (i=0; i < bound->num; i++)  bound->member[i]->focus = 1;
  for (i=0; i < bound->num; i++) {
    flag = 0;
    for (j=0; j < bound->member[i]->neigh.num; j++)
      if (bound->member[i]->neigh.arc[j]->focus) flag = 1;
    if (flag == 0) {
      printf("isolated node\n");
      exit(0);
    }
  }

  condition_on_boundary(bound,bn);
  for (i=0; i < bn->num; i++) bn->list[i]->focus = 1;
}


static void
create_sandwich_patch(ACCOMP_PATCH p, BELIEF_NET *bn) {
  QUAD_FORM e;
  BNODE *hang,*this,*last,*left_anch,*rite_anch;
  CLIQUE *cl;
  int i,a,b;
  MATRIX mat;
  float gap;
  
  
  add_solo_connect(p.shead,p.ahead,bn);
  add_solo_connect(p.stail,p.atail,bn);
  left_anch = last = solo_anchor(p.shead);
  rite_anch = solo_anchor(p.stail);
  /*    printf("last type = %d next type = %d time  %f\n",last->note_type,rite_anch->note_type,last->meas_time);*/
  a = (left_anch->note_type == PHANTOM_NODE) ? p.ahead : p.ahead+1;
  b = (rite_anch->note_type == PHANTOM_NODE) ? p.atail+1 : p.atail;
  /*  printf("a = %d (%s) b = %d (%s) \n",a,score.midi.burst[a].observable_tag,b,score.midi.burst[b].observable_tag);*/
  for (i=a; i <= b; i++) {
    if (i == b)  this = rite_anch;
    else this  = alloc_accomp_node_pot(ACCOM_DIM, INTERIOR_NODE,i, bn);
    gap = this->meas_time - last->meas_time;
    mat = evol_matrix(ACCOM_DIM,  gap);
    if (score.midi.burst[i].acue) printf("need untrained update distriubution at %s\n",score.midi.burst[b].observable_tag);
    /*    e = QFtempo_change_pos_slop_parms(gap*.5, gap*1.); */
        e = (score.midi.burst[i].acue) ? QFtempo_change_pos_slop_parms(gap*.5, gap*1.)
            : score.midi.burst[i].qf_update;  /* this is not correct in  phantom case since the update comes from a longer length note
					   than the distance to the phantom */
    if (score.midi.burst[i].acue) {
      /*      printf("can't handle accomp cue (%d) sandwhiched between solo cues at %s in create_sandwich_patch (%d %d)\n",i,score.midi.burst[i].observable_tag,p.ahead,p.atail);
	      printf("maybe this is okay\n"); */
    }
    cl = make_conditional_potential(last, this, mat, e);
    add_potential(bn,cl);
    last = this;
  }
  add_undir_arc(left_anch,rite_anch);
  cl = alloc_clique();
  add_to_clique(cl,left_anch);
  add_to_clique(cl,rite_anch);
  condition_patch(cl,p,bn);
  bisect_boundary(bn, cl);

  
}


static void
create_lead_patch(ACCOMP_PATCH p, BELIEF_NET *bn) {
  QUAD_FORM e;
  BNODE *hang,*this,*last,*pont,*mid;
  CLIQUE *cl;
  int i,a,b,coincides;
  MATRIX mat;
  float gap;
  
  
  for (i=p.ahead; i <= p.atail; i++) {
    /*    printf("acc = %d %f (%f)\n",i,score.midi.burst[i].time,score.midi.burst[score.midi.num-1].time);*/
    coincides = (fabs(score.solo.note[p.shead].time - score.midi.burst[i].time) < .0001);
    this  = alloc_accomp_node_pot(ACCOM_DIM, ACCOMP_LEAD_NODE,i, bn);
    if (coincides) {
      mid  = alloc_belief_node_pot(1, LEAD_CONNECT_NODE,i, bn);
      mat = Mleft_corner(1,SOLO_DIM);
      e = QFpos_same(1);
      cl = make_conditional_potential(score.solo.note[p.shead].hidden,mid, mat, e);
      add_potential(bn,cl); 
      mat = Mleft_corner(ACCOM_DIM,1);
      e = QFpos_sim();
      cl = make_conditional_potential(mid,this, mat, e);
      add_potential(bn,cl); 
    }
    if (i > p.ahead) {
      gap = this->meas_time - last->meas_time;
      mat = evol_matrix(ACCOM_DIM,  gap);
      e = score.midi.burst[i].qf_update; 
      cl = make_conditional_potential(last, this, mat, e);
      add_potential(bn,cl);
    }
    else {
      e = score.midi.burst[i].qf_update; 
      cl = make_marginal_potential(this,e);
      add_potential(bn,cl);
    }
    last = this;
  }
  cl = alloc_clique();
  add_to_clique(cl,mid);
  condition_patch(cl,p,bn);
}


static void
create_trail_patch(ACCOMP_PATCH p, BELIEF_NET *bn) {
  QUAD_FORM e;
  BNODE *hang,*this,*last,*pont,*mid;
  CLIQUE *cl;
  int i,a,b,coincides;
  MATRIX mat;
  float gap;

  printf("trail from %f to %f\n",score.midi.burst[p.ahead].time,score.midi.burst[p.atail].time);
  for (i=p.ahead; i <= p.atail; i++) {
    coincides = (fabs(score.solo.note[p.shead].time - score.midi.burst[i].time) < .0001);
    this  = alloc_accomp_node_pot(ACCOM_DIM, INTERIOR_NODE,i, bn);
    if (coincides) {
      e = QFpos_tempo_sim();
      cl = make_conditional_potential(score.solo.note[p.shead].hidden,this, Miden(SOLO_DIM), e);
      add_potential(bn,cl);
    }
    if (i > p.ahead) {
      gap = this->meas_time - last->meas_time;
      mat = evol_matrix(ACCOM_DIM,  gap);
      e = score.midi.burst[i].qf_update; 
      cl = make_conditional_potential(last, this, mat, e);
      add_potential(bn,cl);
    }
    last = this;
  }
  gap =   score.midi.burst[p.ahead].time - score.solo.note[p.shead].time;
  if (gap > .001) {
    printf("did this solo = %f acc = %f\n",score.solo.note[p.shead].time,score.midi.burst[p.ahead].time);
    hang  = alloc_belief_node_pot(ACCOM_DIM, PHANTOM_NODE,p.shead, bn);
    e = QFpos_tempo_sim();
    cl = make_conditional_potential(score.solo.note[p.shead].hidden,hang, Miden(SOLO_DIM), e);
    add_potential(bn,cl);
    mat = evol_matrix(ACCOM_DIM,  gap);
    e = QFhold_steady();
    cl = make_conditional_potential(hang,score.midi.burst[p.ahead].hidden, mat, e);
    add_potential(bn,cl);
  }
}


static void
create_patch(ACCOMP_PATCH p, BELIEF_NET *bn) {

  /*  printf("creating patch solo = (%d %d) accom = (%d %d)\n",p.shead,p.stail,p.ahead,p.atail); */
  if (p.type == SINGLE_NOTE_PATCH) {
    create_single_note_patch(p,bn);
    return;
  }
  if (p.type == SANDWICH_PATCH) {
    create_sandwich_patch(p,bn);
    return;
  }
  if (p.type == ACCOMP_LEAD_PATCH) {
    create_lead_patch(p,bn);
    return;
  }
  if (p.type == PHRASE_TRAIL_PATCH) {
    create_trail_patch(p,bn);
    return;
  }
}


static void
add_accomp_patches(BELIEF_NET *bn) {
  PATCH_LIST pl;
  int i;

  set_patch_list(&pl);
  for (i=0; i < pl.num; i++) create_patch(pl.list[i],bn);
}



static void
add_tempo_settings(BELIEF_NET *bn) {
  char name[500],str[500];
  FILE *fp; 
  int pitch,vel,i,m1,m2,j;
  RATIONAL r,rr,val;
  float len,bpm;


  for (i=0; i < tempo_list.num; i++) {
    //    printf("%d/%d %f\n",tempo_list.el[i].wholerat.num,tempo_list.el[i].wholerat.den,tempo_list.el[i].whole_secs);
    set_tempo(tempo_list.el[i].wholerat,tempo_list.el[i].whole_secs,bn);
  }
  return;




  strcpy(name,full_score_name);
  strcat(name,".tmp");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("no tempo settings (%s)\n",name);
    return; 
  }
  while (1) {
    fscanf(fp,"%s %d/%d = %f",str,&val.num,&val.den,&bpm);
    if (feof(fp)) break;
    printf("setting %s %d/%d = %f\n",str,val.num,val.den,bpm);
    fix_tempo(str,val,bpm,bn);
  }
  fclose(fp);
}

 int
does_phrase_overlap(NETWORK n) {
  int i,t,overlap;
  RATIONAL r;
  char pos[500];

  for (i=0; i < n.bn.num; i++) {
    r = n.bn.list[i]->wholerat;
    t = n.bn.list[i]->note_type;
    if (n.bn.list[i]->observable_tag == 0) continue;
    overlap = ((rat_cmp(r,start_pos) >= 0) &&  (rat_cmp(r,end_pos) <= 0));
    if (overlap) return(1);
    //    wholerat2string(r,pos);
    //    printf("i = %d %s %d %d/%d %d end = %d/%d\n",i,pos,t,r.num,r.den,overlap,end_pos.num,end_pos.den);
  }
  return(0);
}



//CF:  builds the networks (after training files have been read in, and their priors
//CF:  on the update nodes parsed into tr)
void
make_complete_bbn_graph_composite(TRAINING_RESULTS tr) {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i,flag,overlap;
  

  /*    read_bbn_training_dists_to_score("composite");         */
  /*  tr = get_bbn_training_dists("composite", &flag); */
  /*  if (flag == 0)  printf("couldn't find trained composite model ... not sure what happens here\n");*/

  free_perm_space();  // expreiment 1-06   if reading in 2nd score need to free old perm memory
  printf("memory = %d\n",malloc(1));
  init_belief_net(&bn);
  printf("memory = %d\n",malloc(1));
  /*  add_composite_graph(&bn,tr); */
  //  printf("note this switch!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");


  add_composite_graph_xchg(&bn,tr);  //CF:  MEAT -- BUILD DAG
	  
  //  add_composite_graph_atempo(&bn,tr);
  //  add_tempo_settings(&bn); /*using version with composite list */
  printf("memory = %d\n",malloc(1));
  printf("%d nodes x %d bytes per node =%d bytes alloted in add_composite_graph_xchg\n",
	 bn.num,sizeof(BNODE),bn.num*sizeof(BNODE));
  component.list = (PHRASE_LIST_EL *) malloc(MAX_PHRASES*sizeof(PHRASE_LIST_EL));
  printf("memory = %d\n",malloc(1));

  //CF:  identify completely separated phrases of graph ('component' is global PHRASE_LIST)
  //CF:  and copy pointers of BNODEs and Potentials into these phrases
  make_connected_components(&bn,&component);    
                                                
  printf("memory = %d\n",malloc(1));
  printf("components = %d cues = %d\n",component.num,cue.num);
  printf("building all phrases of  bbn graph\n");

  //CF:  for each phrase
  for (i=0; i < component.num; i++) { /* 6 secs for whole strauss 1 */
    overlap =  does_phrase_overlap(component.list[i].net);
    //   if (overlap == 0) continue;
    //   printf("now = %f\n",now());
    printf("memory = %d\n",malloc(1));
    ct = bn2ct_pot(&(component.list[i].net.bn));      //CF:  BELIEF NET TO (initialised) CLIQUE TREE *** for ith phrase
    component.list[i].net.ct = ct;                    //CF:  add the clique tree to the phrase
    save_state(ct);  /* added this on 9-12-02 */
    printf("memory = %d\n",malloc(1));
    printf("component %d at equilibrium\n",i);
  }
}


int
make_complete_bbn_graph_composite_no_ct(TRAINING_RESULTS tr) {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i,flag,overlap;
  

  /*    read_bbn_training_dists_to_score("composite");         */
  /*  tr = get_bbn_training_dists("composite", &flag); */
  /*  if (flag == 0)  printf("couldn't find trained composite model ... not sure what happens here\n");*/

  printf("memory = %d\n",malloc(1));
  init_belief_net(&bn);
  printf("memory = %d\n",malloc(1));
  /*  add_composite_graph(&bn,tr); */
  //  printf("note this switch!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
#ifdef PARALLEL_GRAPH_EXPERIMENT
  if (add_parallel_solo_accom_graph(&bn,tr) == 0) return(0);

#else
  if  (add_composite_graph_xchg(&bn,tr) == 0) return(0);
#endif
	  //	          add_composite_graph_atempo(&bn,tr);
	  //  add_tempo_settings(&bn); /*using version with composite list */
  printf("memory = %d\n",malloc(1));
  printf("%d nodes x %d bytes per node =%d bytes alloted in add_composite_graph_xchg\n",
	 bn.num,sizeof(BNODE),bn.num*sizeof(BNODE));
  component.list = (PHRASE_LIST_EL *) malloc(MAX_PHRASES*sizeof(PHRASE_LIST_EL));
  printf("memory = %d\n",malloc(1));
  make_connected_components(&bn,&component);
  printf("memory = %d\n",malloc(1));
  printf("components = %d cues = %d\n",component.num,cue.num);

  printf("reset this in make_complete_bbn_graph_composite ..................\n");


  printf("built all phrases of  bbn graph without clique trees\n");
  for (i=0; i < component.num; i++)   component.list[i].net.ct.num = 0;
  return(1);
}


void
add_needed_clique_trees() {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i,flag,overlap;
  
  for (i=0; i < component.num; i++) { 
    if (component.list[i].net.ct.num) continue;
    overlap =  does_phrase_overlap(component.list[i].net);
    if (overlap == 0) continue;
   //   printf("now = %f\n",now());
  printf("memory = %d\n",malloc(1));
    ct = bn2ct_pot(&(component.list[i].net.bn));   
    component.list[i].net.ct = ct;
    save_state(ct);  /* added this on 9-12-02 */
  printf("memory = %d\n",malloc(1));
    printf("component %d at equilibrium\n",i);
  }
}


static void
override_training_means(TRAINING_RESULTS tr) {
  char name[500],string[500];
  FILE *fp;
  int i,j,comp;
  BELIEF_NET bbn;
  BNODE *bn;
  float val;
  
  
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  //  strcpy(name,scorename);
  strcat(name,".chg");
  fp = fopen(name,"r");
  if (fp == NULL) return;
  while (1) {
    fscanf(fp, "%s %d %f",string,&comp,&val);
    if (feof(fp)) break;
    printf("changing %s mean[%d] to %f\n",string,comp,val);
    for (i=0; i < tr.num; i++) {
      if (strcmp(string,tr.tag[i]) == 0) break;
    }
    if (i == tr.num) { 
      printf("couldn't match tag %s in override_training_means()\n",string);
      exit(0);
    }
    tr.qf[i].m.el[comp][0] = val;
  }
  fclose(fp);
}






//CF:  Construct the belief net ****
//CF:  'answer' is: whether to use solo/accomp/both training data from existing files
//CF:  (ie. load the priors on the update nodes from these files)
void
make_complete_bbn_graph_arg(char *answer) {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i,success;
  TRAINING_RESULTS tr;



  if (strcmp(answer,"null") == 0) tr.num = 0;
  else if (strcmp(answer,"accomp") == 0) {
    tr = get_bbn_training_dists(ACCOMP_TRAIN_ONLY, &success);  
    if (success == 0) { printf("couldn't read training %s\n",ACCOMP_TRAIN_ONLY); exit(0); }
  }
  else if (strcmp(answer,"both") == 0) {
    tr = get_bbn_training_dists(SOLO_TRAIN, &success);  
    if (success == 0) { printf("couldn't read training %s\n",SOLO_TRAIN); exit(0); }
  }
  else if (strcmp(answer,"solo_only") == 0) {
    tr = get_bbn_training_dists(SOLO_ONLY_TRAIN, &success);  
    if (success == 0) { 
      printf("couldn't read training %s\n",SOLO_ONLY_TRAIN); exit(0); 
    }
  }
  else {printf("didn't recognize option %s\n",answer); exit(0); }
  if (tr.num) override_training_means(tr);
  make_complete_bbn_graph_composite(tr); //CF:  meat
}

int
make_complete_bbn_graph_windows() {
  int i,success;
  TRAINING_RESULTS tr;

  free_perm_space();  // expreiment 1-06   if reading in 2nd score need to free old perm memory
  tr = get_bbn_training_dists(0, &success);  
  if (success == 0) tr.num = 0;
  //   if (tr.num) override_training_means(tr);   /* old way I think */
  //    make_complete_bbn_graph_composite(tr);

  if (make_complete_bbn_graph_composite_no_ct(tr) == 0) return(0);
  return(1);
}


//CF:  entry for BN construction
void
make_complete_bbn_graph() {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i,success;
  TRAINING_RESULTS tr;
  char answer[500];


  printf("chaning to composite graph\n");
  printf("enter the training you want to use (null/accomp/both/solo_only)\n");
  scanf("%s",answer);
  if (strcmp(answer,"null") == 0) tr.num = 0;
  else if (strcmp(answer,"accomp") == 0) {
    tr = get_bbn_training_dists(ACCOMP_TRAIN_ONLY, &success);  
    if (success == 0) { printf("couldn't read training %s\n",ACCOMP_TRAIN_ONLY); exit(0); }
  }
  else if (strcmp(answer,"both") == 0) {
    tr = get_bbn_training_dists(SOLO_TRAIN, &success);  
    if (success == 0) { printf("couldn't read training %s\n",SOLO_TRAIN); exit(0); }
  }
  else if (strcmp(answer,"solo_only") == 0) {
    tr = get_bbn_training_dists(SOLO_ONLY_TRAIN, &success);  
    if (success == 0) { printf("couldn't read training %s\n",SOLO_ONLY_TRAIN); exit(0); }
  }
  else {printf("didn't recognize option %s\n",answer); exit(0); }
  if (tr.num) override_training_means(tr);

  make_complete_bbn_graph_composite(tr);  //CF:  meat


  return;


//CF:  following is not used:

  read_bbn_training_dists_to_score("accom");       
  /*  printf("leaving out trainined model --- using default\n");        */
  init_belief_net(&bn);
  printf("adding solo graph\n");
  add_solo_graph(&bn);
  printf("adding accomp graph\n");
  add_accomp_patches(&bn);
  component.list = (PHRASE_LIST_EL *) malloc(MAX_PHRASES*sizeof(PHRASE_LIST_EL));
    /*   condition_graph_on_solo(&bn);   
	 printf("making connected components\n");  */
  make_connected_components(&bn,&component);
  for (i=0; i < component.num; i++) {
    /*        global_flag = 1; */
    /*    i = 5;  */
    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn));   
    printf("component %d at equilibrium\n",i);
    /*    exit(0); */
  }

  /*  make_connected_components(&bn,&component);
  new_make_excerpt_graph(component,40.,42.,"weinen_frag.tex",0,20);*/
  
  
}








static void
new_test_solo_predict(void) {  /* seems to assume that an audio file has been parsed. */
  int i,/*firstnote,lastnote,*/firsta,lasta;
  float m,v,time,next_time,error,meas,tempo,tvar;
  char name[500],answer[500];
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  FILE *fp_true,*fp_pred,*fp_error;
  MATRIX hm,hvar;



  printf("enter the audio data directory: ");
  scanf("%s",audio_data_dir);


  fp_true = fopen("solo_true.dat","w");
  fp_pred = fopen("solo_pred.dat","w");
  fp_error = fopen("solo_error.dat","w");


  /*     read_solo_training();   */


  read_audio_indep();
  /*  phrase_span(0,&firstnote,&lastnote,&firsta,&lasta);  */


  /*     bn = make_solo_graph(firstnote,lastnote); 
	 ct = bn2ct(&bn);   */


  printf("enter the training you want to use (null/accomp/both)\n");
  scanf("%s",answer);
  make_complete_bbn_graph_arg(answer);

  //      make_complete_bbn_graph();   


  get_solo_mean(firstnote,&m,&v);
  printf("before any info first note has m = %f v  = %f\n",m,v);
  for (i=firstnote; i <= lastnote-1; i++) {
    time = toks2secs(score.solo.note[i].realize);
    /*    time = score.solo.note[i].realize;*/
    /*    if (i == firstnote) 
      printf("time of first note is %f\n",toks2secs(score.solo.note[i].realize));*/
    next_time = toks2secs(score.solo.note[i+1].realize);
    /*    next_time = score.solo.note[i+1].realize;*/
    /*    printf("%f %f\n",score.solo.note[i+1].realize,next_time);*/
    fix_solo_obs(i,time);
    recompute_solo(i+1);
    get_solo_mean(i+1,&m,&v);
    error = (next_time-m); 
    get_hidden_solo_mean(i+1, &hm,&hvar);
    num2name(score.solo.note[i+1].num,name);
    meas = score.solo.note[i+1].time;
    /*    printf("meas = %5.2f\tname = %s\tpredict = %5.3f\tactual = %5.3f\terror  = %5.3f\n",meas,name,m,next_time,error); */
    printf("note = %10s\tpredict = %6.3f\tactual = %5.3f\terror  = %5.3f\ttempo = %f\n",score.solo.note[i+1].observable_tag,m,next_time,error,hm.el[1][0]);
    printf("var = %f\n",v);
    if (1/*meas >= 3 &&  meas < 5*/) {
      fprintf(fp_true,"%f %f\n",meas,next_time);
      fprintf(fp_pred,"%f %f\n",meas,m);
      fprintf(fp_error,"%f %f\n",meas,error);
    }
  } 
  fclose(fp_true);
  fclose(fp_pred);
  fclose(fp_error);
}

static void
bug_catch(void) {
  int i,firstnote,lastnote,firsta,lasta;
  float m,v,time,next_time,error,meas;
  char name[500];
  BELIEF_NET bn;
  CLIQUE_TREE ct;



  read_solo_training();  


  read_audio_indep();
  phrase_span(/*phrase*/ 1,&firstnote,&lastnote,&firsta,&lasta);


   make_complete_bbn_graph();   

   time = toks2secs(score.solo.note[0].realize);
   fix_solo_obs(0,time);

  for (cur_accomp=0; cur_accomp <= 1000; cur_accomp++) {
    printf("cur_accomp = %d\n",cur_accomp);
    recalc_next_accomp();  
  }
}

void
synthetic_updates(void) {  /* seems to assume that an audio file has been parsed. */
  int i,num,flush;
  float m,v,time,next_time,error,meas;
  char update[500],word[500],name[500];
  FILE *fp;

  cur_accomp = 0;
  exper_init_forest();
  fp = fopen("habanera.ut","r");
  while (!feof(fp)) {
    fscanf(fp,"%s %s %d %f\n",update,word,&num,&time);
    if (strcmp(update,"solo") == 0)  update_belief_solo(num, time);
      
    else if (strcmp(update,"accom") == 0) {
      cur_accomp = num+1;
      update_belief_accom(num, time,flush=1);
    }
    else { printf("unknown update\n"); exit(0); }
  }
    


  
  for (i=firstnote; i <= lastnote-1; i++) {
    time = toks2secs(score.solo.note[i].realize);
    if (i == firstnote) 
      printf("time of first note is %f\n",toks2secs(score.solo.note[i].realize));
    next_time = toks2secs(score.solo.note[i+1].realize);
    /*    printf("%f %f\n",score.solo.note[i+1].realize,next_time);*/
    fix_solo_obs(i,time);
    recompute_solo(i+1);
    get_solo_mean(i+1,&m,&v);
    error = (next_time-m); 
    num2name(score.solo.note[i+1].num,name);
    meas = score.solo.note[i+1].time;
    printf("meas = %5.2f\tname = %s\tpredict = %5.3f\tactual = %5.3f\terror  = %5.3f\n",meas,name,m,next_time,error);
  } 
}



#define TRAIN_RUNS 4
#define CUTOFFS 5

void
belief_test(void) {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int fnote,lnote,firsta,lasta,i,ex,j;
  float time,v,m;
  QUAD_FORM jn;

  new_test_solo_predict(); 
  exit(0);
  bug_catch();
  exit(0);
  /*  test_solo_predict();  */
  new_test_solo_predict(); 
  return; 
  /*  make_table_for_paper();*/
  //  make_table_for_paper_2();

  init_forest(firstnote,lastnote);
  for (i=firstnote; i <= lastnote; i++) {
    time = (TOKENLEN/(float)SR)*score.solo.note[i].realize;
    fix_solo_obs(i,time);
  } 
  for (i=0; i < cue.num; i++) if (phrase_active(i)) equilibrium(cue.list[i].net.ct);
  

  for (i=0; i <= score.midi.num; i++) {
    time = score.midi.burst[i].time;
    if (time >= score.solo.note[firstnote].time)
      if (time <= score.solo.note[lastnote].time) {
	get_accom_mean(i,&m,&v);
	score.midi.burst[i].ideal_secs = m;/**TOKENLEN/(float)SR; */
	printf("accom num = %d time = %m var = %f\n",i,m,v);
      }
  } 

  while (1) play_accomp_with_recording(); 

  /*  check_forest(); */

  /*  for (i=0; i < 5; i++) train_test(THE_PHRASE);
  bel_write_updates();


exit(0); */

  phrase_span(THE_PHRASE,&fnote,&lnote,&firsta,&lasta);
  for (ex=0; ex < score.example.num; ex++) {
    /*    printf("%d %d %d %d\n",score.example.list[i].start,fnote,score.example.list[i].end,lnote); */
    if (score.example.list[ex].start <= fnote && score.example.list[ex].end >= lnote) 
      break;
      /*      printf("example %d matches\n",i);*/
      
  }

    bn = make_phrase_graph(THE_PHRASE); 
    ct = bn2ct(&bn);

    /*    for (i=0; i < bn.num; i++) {
        printf("nodenum = %d\n",bn.list[i]->node_num);
    QFprint(bn.list[i]->e);
    getchar();
    getchar();
  } */

    /*                        make_tex_graph(bn);
  exit(0);                       */
/*  connect_belief_net_with_score(bn);
  new_make_moral_graph(&bn);
  new_triangulate_graph(&bn);
  ct = new_new_build_clique_tree(&bn); 
  init_cliques_with_probs(ct, bn); 
  check_clique_dists(ct); */
    /*exit(0);*/


        for (i=fnote; i <= lnote; i++) {
      time = (TOKENLEN/(float)SR)*score.solo.note[i].realize;
      fix_solo_obs(i,time);
    } 
    equilibrium(ct);     

    /*    printf("node 5 is\n");
    jn = post_dist(bn.list[5]);
    QFprint(jn);
    printf("node 4 is\n");
    jn = post_dist(bn.list[4]);
    QFprint(jn);
    printf("node 47 is\n");
    jn = post_dist(bn.list[47]);
    QFprint(jn);
    printf("node 48 is\n");
    jn = post_dist(bn.list[48]);
    QFprint(jn);
exit(0); */
      
    /*    
	  for (i=fnote; i <= lnote; i++) {
	  time = score.solo.note[i].realize;
    fix_solo_obs(i,time);
    collect(score.solo.note[i+1].observe->clnode,0);  
    jn = post_dist(score.solo.note[i+1].observe);
    printf("realize = %f\n\n",time);
    printf("predict = %f (var = %f) \n",jn.m.el[0][0],jn.cov.el[0][0]);
  }
  exit(0);



    equilibrium(ct);     
  for (i=0; i < 10; i++) {
    printf("i = %d\n",i);
    QFprint(score.solo.note[i].hidden->e); 
    
    jn = post_dist(score.solo.note[i].hidden);
    QFprint(jn); 
    getchar();
    getchar();
  }
exit(0);
*/


    /*    for (i=0; i < 2; i++) {
      printf("i = %d\n",i);
      jn = post_dist(score.solo.note[i].observe);
      QFprint(jn); 
      jn = post_dist(score.solo.note[i].hidden);
      QFprint(jn); 
    }
      exit(0);
      */

    
    /*        for (i=fnote; i <= lnote; i++) {
      time = score.solo.note[i].realize *TOKENLEN/(float)SR; 
      fix_solo_obs(i,time);
      printf("fixing solo %d at %f\n",i,time);
      for (j=firsta; j <= lasta; j++) {
	if (score.midi.burst[j].time > score.solo.note[i].time || i == fnote) {
	  if (score.midi.burst[j].time < score.solo.note[i+1].time + .01) {
	    recompute_accomp(j);
	    get_accom_mean(j,&time,&v);
	    fix_accom_obs(j, time);
	    printf("fixing accomp %d at %f\n",j,time);
	    score.midi.burst[j].secs = time;
	  }
	}
      }
    }
    */
	    
	
      
    
    for (i=fnote; i <= lnote; i++) printf("realize(%d) = %f\n",i,score.solo.note[i].realize*TOKENLEN/(float)SR);
  for (i=firsta; i <= lasta; i++) {
    get_accom_mean(i,&time,&v);
    score.midi.burst[i].ideal_secs = time;/**TOKENLEN/(float)SR; */
       printf("accom num = %d time = %f var = %f\n",i,time,v);
  } 
  while (1) { 
    play_accomp_phrase_with_recording(THE_PHRASE);
  } 
}





belief_main(void) {
  BNODE *root;
  int i,j;
  CLIQUE_NODE *top;
  CLIQUE snd,rcv,inter;
  
  

  QUAD_FORM qf,new,quad;
  JOINT_NORMAL jn;
  MATRIX m,U,D,b,x,S;
  MATRIX V,C,A;
  int rank,dim[3];
  float det,theta=PI/4;
  CLIQUE_TREE ct;
  BELIEF_NET bn;
  SUPER_QUAD sq;
  SUPERMAT sm;
  float buff[100][2],d[3];


  /*  init_belief(); */
  init_score();
  bn = make_score_graph();

  /*  for (i=0; i < bn.num; i++) printf("%d %d %d\n",i,bn.list[i]->note_type,bn.list[i]->index);*/

  connect_belief_net_with_score(bn);


  new_make_moral_graph(&bn);
  new_triangulate_graph(&bn);


  /*      make_tex_graph(bn);
  exit(0);          */
  ct = new_new_build_clique_tree(&bn); 
  /*  print_clique_tree(ct); */
  ctree = ct;
  init_cliques_with_probs(ct, bn); 
  check_clique_dists(ct);
  printf("adding constraints\n");
  simulate_from_graph(bn);
  /*       write_simulation_estimates(bn);
exit(0);       */
            equilibrium(ct);   


	    /*    fix_solo_simulations();*/
  check_clique_dists(ct);
  printf("computing equilibrium\n");
  /*  fix_solo_obs(0,0);
    fix_solo_obs(1,1);
  fix_solo_obs(2,2);    
printf("starting to collect\n");
  collect(score.midi.burst[0].belief->clnode,0);  
  collect(score.midi.burst[1].belief->clnode,0);  
  collect(score.midi.burst[2].belief->clnode,0);  
  collect(score.midi.burst[3].belief->clnode,0);  
  collect(score.midi.burst[4].belief->clnode,0);   */
  /*      fix_solo_obs(0,10);
  fix_solo_obs(1,11);
  fix_solo_obs(2,12);       */
  /*      fix_solo_simulations();   */
  /*   equilibrium(ct);  */
  for (i=0; i < 10; i++)   fix_solo_obs(i,10+i);
  for (i=0; i < 27; i++)   recompute_accomp(i);

  printf("writing estimates\n");
  write_estimates(bn);
  exit(0);

  
  

  


/*  root = make_music_graph(); */
  root = make_music_graph(); 
  bn = set_belief_net(root);
  make_moral_graph(root);
  /*  visit_dag_graph(root);  */
  triangulate_graph(root);
  /*  visit_undir_graph(root,visit_function);*/
  ct = build_clique_tree(root);
  ctree = ct;
/*  trace_clique_tree(ct.root,hello); */
  /*  for (i=0; i < bn.num; i++) printf("bnode = %d\n",bn.list[i]->node_num); */
  /*  for (i=0; i < ct.num; i++) {
    printf("clique = %d\n",ct.list[i]->clique_id); 
    print_clique(ct.list[i]->clique);
  }  */
  init_cliques_with_probs(ct, bn);


  /*  equilibrium(ct); */
    add_constraints(ct, bn);
    /*    for (i=0; i < ctree.num; i++) {
 printf("i = %d\n",i);
 QFprint(ctree.list[i]->clique->qf);
}  */

    check_clique_memory(ct);
    equilibrium(ct);
    write_estimates(bn);
    /*    global_flag = 1;
    equilibrium(ct); */
  for (i=0; i < bn.num; i++) {
    printf("%d %f\n",bn.list[i]->node_num,post_dist(bn.list[i]).m.el[0][0]);
    /*    QFprint(post_dist(bn.list[i]));*/
    /*    buff[i][j] = post_dist(bn.list[i]).m.el[0][0]; */
  }
  /*    for (i=0; i < bn.num; i++) if (fabs(buff[i][0]-buff[i][1]) > .01) printf("%d %f %f\n",i,buff[i][0],buff[i][1]); */
  /*  for (i=0; i < ct.num; i++) {
    print_clique(ct.list[i]->clique);
    QFprint(ct.list[i]->clique->qf);
  } */
}
    





/*  testing shit 
  d[0] = 1;
  d[1] = .001;
  d[2] = 10.;
  U = Miden(3);
  A = Mzeros(3,3);
  D = Miden(3);
  for (i=0; i < 3; i++) D.el[i][i] = d[i];
    for (i=0; i < 3; i++) for (j=0; j < 3; j++) if (j == 1 || i == 1) A.el[i][j] = 1;
  U = Morthogonalize(U);
  quad.fin = Mzeros(3,1);
  quad.fin.el[0][0] = U.el[0][0];
  quad.fin.el[1][0] = U.el[1][0];
  quad.fin.el[2][0] = U.el[2][0];
  quad.null = Mzeros(3,1);
  quad.null.el[0][0] = U.el[0][1];
  quad.null.el[1][0] = U.el[1][1];
  quad.null.el[2][0] = U.el[2][1];
  quad.inf = Mzeros(3,1);
  quad.inf.el[0][0] = U.el[0][2];
  quad.inf.el[1][0] = U.el[1][2];
  quad.inf.el[2][0] = U.el[2][2];
  quad.m = Mzeros(3,1);
  quad.S = Miden(3);
  quad.cov = Miden(3);
  new = QFxform(quad,A);
  printf("U is \n");
  Mp(U);
  S = Mm(U,Mm(D,Mt(U)));
  printf("A is \n");
  Mp(A);

  S = Mm(Mm(A,S),Mt(A));
  Msym_decomp(S, &V, &C);
  printf("C = \n");
  Mp(C);
  printf("V = \n");
  Mp(V);
  QFprint(new);
  exit(0);
  */



/*#define UPDATE_STD_CNST 1 


QUAD_FORM belief_init_update(float length) { /* approx length of note in  measures */ 
/*
    float var,secs2tokens(),val[SOLO_DIM];
    QUAD_FORM init;
    

    length *= score.meastime*UPDATE_STD_CNST;
    length = secs2tokens(length);
    var = length*length;
    for (i=0; i < SOLO_DIM; i++) val[i] = var;
    init = QFindep(SOLO_DIM,Mzeros(SOLO_DIM,1),val);  
    return(init);
}
*/


void 
make_belief_network_graphs() {
  NETWORK n;
  QUAD_FORM jn;
  TRAINING_RESULTS tr;
  
  /*  n.bn = make_phrase_graph(0);*/
  /*    n.bn = make_indep_graph();  old way */
  /*  exper_init_network();  /* new way */


  tr.num = 0;
  make_complete_bbn_graph_composite(tr);


  /*    n.bn = make_indep_phrase_graph(0); */
  /*  global_flag = 1; */
    /*                    n.ct = exper_bn2ct(&(n.bn)); 
			  equilibrium(n.ct);      */
    /*      exper_init_forest();
  printf("have %d components\n",component.num);
  n = component.list[1].net; */
  n = component.list[0].net; /* new way */
  /*  make_excerpt_graph(n.bn,27.,30.,"weinen_frag.tex",0,20); */
  new_make_excerpt_graph(component,0.,2.,"romance2_frag.tex",0,20);
  exit(0);
  make_excerpt_graph(n.bn,17.,20.,"weinen_frag.tex",0,20);
  exit(0);
  make_excerpt_graph(n.bn,0.,2.5,"schubert_frag.tex",1,2);
  make_kalman_graph(n.bn,0.,.85,"kalman_frag.tex");
}




int
bbn_train_composite_model_mem() {
  /* do training procedure starting with the model that is currently in memory */
  TRAIN_FILE_LIST tfl;
  int i,j;

   tfl = get_bbn_training_file_max();
   printf("components = %d\n",component.num);
  for (i=0; i < component.num; i++) {
    printf("training phrase %d\n",i);
    if (count_overlapping_examples(component.list[i].net,tfl) == 0) {
      printf("skipping training phrase %d\n",i); 
      continue; 
    }

#ifdef WINDOWS  /* only have clique tree created and only want to train for phrases of most recent example.
		   training on all examples would allow a single example for a phrase 
		   to overtrain model */
    if (does_phrase_overlap(component.list[i].net) == 0) {
      printf("skipping training phrase %d since doesn't intersect cur examp\n",i); 
      continue;
    }
#endif


    /*    for (j=0; j < component.list[i].net.bn.num; j++)
      if (component.list[i].net.bn.list[j]->observable_tag)
      printf("%s\n",component.list[i].net.bn.list[j]->observable_tag); */
#ifndef WINDOWS
    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn));
#endif
    bbn_train(component.list[i].net,tfl,1,/*MEANS_AND_VARS*/MEANS_ONLY);  /* 1 training iteration */
  }


  write_bbn_training(component,SOLO_TRAIN);
  return(0);
}

void 
bbn_train_composite_model_mem_one_ex() {
  /* do training procedure starting with the model that is currently in memory */
  TRAIN_FILE_LIST tfl;
  int i,j;

  tfl = get_last_bbn_training_file();  // only use the most recent example.
   printf("components = %d\n",component.num);
  for (i=0; i < component.num; i++) {
    printf("training phrase %d\n",i);
    if (count_overlapping_examples(component.list[i].net,tfl) == 0) {
      printf("skipping training phrase %d\n",i); 
      continue; 
    }

#ifdef WINDOWS  /* only have clique tree created and only want to train for most recent example.
		   training on all examples would allow a single example for a phrase 
		   to overtrain model */
    if (does_phrase_overlap(component.list[i].net) == 0) {
      printf("skipping training phrase %d since doesn't intersect cur examp\n",i); 
      continue;
    }
#endif


    /*    for (j=0; j < component.list[i].net.bn.num; j++)
      if (component.list[i].net.bn.list[j]->observable_tag)
      printf("%s\n",component.list[i].net.bn.list[j]->observable_tag); */
#ifndef WINDOWS
    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn));
#endif
    bbn_train(component.list[i].net,tfl,1,/*MEANS_AND_VARS*/MEANS_ONLY);  /* 1 training iteration */
  }


  write_bbn_training(component,SOLO_TRAIN);
}


void 
bbn_train_from_scratch_composite_model() {
  /* do training procedure starting with the model that is currently in memory */
  TRAIN_FILE_LIST tfl;
  int i,j;
  TRAINING_RESULTS tr;
  char name[500];
  FILE *fp;

  bbn_accomp_training_name(name);
  fp = fopen(name,"r");
  if (fp) {
    fclose(fp);
    tr = get_bbn_training_dists_name(name);
    //    printf("read %d dists\n",tr.num); exit(0);
  }
  else tr.num=0;

  make_complete_bbn_graph_composite(tr);   

  //    make_complete_bbn_graph_windows();

   tfl = get_bbn_training_file_max();
   if (tfl.num == 0) { printf("can't train with no examples\n"); return; }
   printf("components = %d\n",component.num);
  for (i=0; i < component.num; i++) {
    printf("training phrase %d\n",i);
    if (count_overlapping_examples(component.list[i].net,tfl) == 0) {
      printf("skipping training phrase %d\n",i); 
      continue; 
    }
    //    bbn_train(component.list[i].net,tfl,5,MEANS_AND_VARS);  /* 5 iterations */
    bbn_train(component.list[i].net,tfl,5,MEANS_ONLY);  /* 5 iterations */
  }

  write_bbn_training(component,SOLO_TRAIN);
}


void 
bbn_train_accomp_composite_model() {
  TRAIN_FILE_LIST tfl;
  int i,j,ok;
  TRAINING_RESULTS tr;
  char name[500];
  

  tr.num = 0;
  make_complete_bbn_graph_composite(tr);   


  strcpy(name,user_dir);
  strcat(name,SCORE_DIR);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,".atm");
  printf("%s\n",name);

  tfl.num = 1;
  tfl.name = (char **) malloc(sizeof(char *));
  tfl.dato = (DATA_OBSERVATION *) malloc(sizeof(DATA_OBSERVATION));
  tfl.name[0] = (char *) malloc(strlen(name)+1);
  strcpy(tfl.name[0],name);
  //  tfl.dato[0] = read_data_observation(tfl.name[0]);
  ok = read_data_observation(tfl.name[0],tfl.dato + 0);
  if (ok == 0) { printf("failed reading data observation\n"); exit(0); }

  for (i=0; i < tfl.dato[0].num; i++)
	if (tfl.dato[0].tag_obs[i].val == 0)  {
		printf(" 0 value at %s in %s\n",tfl.dato[0].tag_obs[i].name,name);
	  //	printf("%f %f\n",tfl.dato[0].tag_obs[i-1].val,tfl.dato[0].tag_obs[i].val);
		exit(0);
    }

  for (i=0; i < component.num; i++) {
    printf("training phrase %d\n",i);
    if (count_overlapping_examples(component.list[i].net,tfl) == 0) {
      printf("skipping training phrase %d\n",i); 
      continue; 
    }
    bbn_train(component.list[i].net,tfl,5,MEANS_ONLY);  /* 5 iterations */
  }

  write_bbn_training(component,ACCOMP_TRAIN_ONLY);
}




int
count_overlapping_examples(NETWORK net, TRAIN_FILE_LIST tfl) {
  int total=0,i;

  for (i=0; i < tfl.num; i++) if (is_overlap(net,tfl.dato[i])) total++;
  return(total);
}

//CF:  top level , train BBN.  Offline -- load performance files from existing performances.
//CF:  can train on accopnaiment or solo or both
void bbn_train_composite_model() {
  TRAIN_FILE_LIST tfl;
  int i,success,j;
  PHRASE_LIST pl;
  char train_file[500],answer[500],output[500];
  TRAINING_RESULTS tr;

  printf("enter the audio data directory: ");
  scanf("%s",audio_data_dir);
  /*                synthesize_mean_accomp();
		    exit(0);       */
  printf("enter\n0) solo_only\tto train on solo examples from scratch\n");  
  printf("1) accomp\tto train on accompaniment examples\n");  
  printf("2) solo\tto train on solo examples starting from result of 1)\n");
  printf("3) incremental\tto train on solo examples from result of 2)\n");
  scanf("%s",answer);
  if (strcmp(answer,"solo_only") == 0)  { tr.num = 0; strcpy(output,SOLO_ONLY_TRAIN); } //CF:  from scratch using solo only
  else if (strcmp(answer,"accomp") == 0)  { tr.num = 0; strcpy(output,ACCOMP_TRAIN_ONLY); } //CF: from scratch using acc only
  else if (strcmp(answer,"solo") == 0)  {   //CF:  from trained acc run (ie previous option), now retrain using solo
    //CF:  load previous accompaniment-trained distributions (.bnt file)
    tr = get_bbn_training_dists(ACCOMP_TRAIN_ONLY, &success);  
    if (success == 0) 
      { printf("couldn't find training for %s\n",ACCOMP_TRAIN_ONLY); exit(0); }
    strcpy(output,SOLO_TRAIN); 
  }
  else if (strcmp(answer,"incremental") == 0)  {   //CF:  not used much -- retrain using old solo and acc, with new solo
    tr = get_bbn_training_dists(SOLO_TRAIN, &success); //CF: SOLO_TRAIN is fn str suffix,_solo_only, _acc_only, _solo_acc etc
    if (success == 0) 
      { printf("couldn't find training for %s\n",SOLO_TRAIN); exit(0); }
    strcpy(output,SOLO_TRAIN); 
  }
  else { printf("couldn't handle option %s\n",answer); exit(0); }

    

  make_complete_bbn_graph_composite(tr);   //CF:  construct the DAG and clique tree, using the existing trained priors
  
  /*  read_bbn_training_dists(pl,"accom");   */
  printf("enter the name of the training file\n");    //CF:  file containing names of .atm training files
  scanf("%s",train_file);
  printf("training file is %s\n",train_file);
  tfl = read_bbn_training_file(train_file);           //CF:  read in that list of names
  /*  tfl = get_bbn_training_file_max(); /*I think I want this in here */

  for (i=0; i < component.num; i++) {               //CF:  for each phrase (global 'component' was set during DAG building)
    printf("training phrase  here %d\n",i);
    /*    printf("zoickes fix the\n"); */

    if (count_overlapping_examples(component.list[i].net,tfl) == 0) { //CF: if this phrase has no train data, skip the phrase
      printf("skipping training phrase %d\n",i); continue; 
    }

    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn));   //CF:  redundant code - makes clique tree again!
    /*    for (j=0; j < component.list[i].net.bn.num; j++) 
      if (component.list[i].net.bn.list[j]->observable_tag) {
	printf("%s\n",component.list[i].net.bn.list[j]->observable_tag);
	break; 
	} */
    bbn_train(component.list[i].net,tfl,BBN_TRAIN_ITERS,MEANS_AND_VARS);
    //CF:  ***** train this phrase ******
    /*    break;*/
  }
  /*  write_bbn_training(component,"composite"); */
    write_bbn_training(component,output);   //CF:  write out new .bnt
}




void
potential_test() {
  int firstnote,lastnote,firsta,lasta;
  BELIEF_NET bn;
 
  int i;
  float m,v,time,next_time,error,meas;
  char name[500];
  CLIQUE_TREE ct;

  phrase_span(0,&firstnote,&lastnote,&firsta,&lasta);  

  read_solo_training(); 

  /*    bn = make_solo_graph_pot(firstnote,lastnote);
  ct = bn2ct_pot(&bn);  */



  /*  bn = make_solo_graph_pot(firstnote,lastnote);
  make_connected_components(&bn,&component);
  for (i=0; i < component.num; i++) {
    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn)); 
    printf("component %d at equilibrium\n",i);
  }*/

  /*    printf("x\n");
  init_belief_net(&bn);
  add_solo_graph(&bn);
  printf("x\n");
  make_connected_components(&bn,&component);
  printf("x\n");
  for (i=0; i < component.num; i++) {
    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn)); 
    printf("component %d at equilibrium\n",i);
  }
  */
  make_complete_bbn_graph();



  for (i=firstnote; i <= lastnote-1; i++) {
    time = toks2secs(score.solo.note[i].realize);
    time = score.solo.note[i].realize;
    if (i == firstnote) 
      printf("time of first note is %f\n",toks2secs(score.solo.note[i].realize));
    next_time = toks2secs(score.solo.note[i+1].realize);
    next_time = score.solo.note[i+1].realize;
    /*    printf("%f %f\n",score.solo.note[i+1].realize,next_time);*/
    fix_solo_obs(i,time);
    recompute_solo(i+1);
    get_solo_mean(i+1,&m,&v);
    error = (next_time-m); 
    num2name(score.solo.note[i+1].num,name);
    meas = score.solo.note[i+1].time;
    printf("meas = %5.2f\tname = %s\tpredict = %5.3f\tactual = %5.3f\terror  = %5.3f\n",meas,name,m,next_time,error);
  }
}


void
belief_graph_test() {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
  int i,sm,em;
  float fsm,fem;


  make_complete_bbn_graph(); 
  sm = 55;  /* start measure */
  em = 61;  /* end measure */
  fsm = score.measdex.measure[sm].pos;
  fem = score.measdex.measure[em].pos;
  new_make_excerpt_graph(component,fsm,fem,"weinen_frag.tex",0,20); 
  exit(0);
  new_make_excerpt_graph(component,25.,29.,"weinen_frag.tex",0,20); 


   init_belief_net(&bn);
  printf("adding solo graph\n");
  add_solo_graph(&bn);
  printf("adding accomp graph\n");
  add_accomp_graph(&bn);
  make_connected_components(&bn,&component);
  new_make_excerpt_graph(component,6.,10.,"weinen_frag.tex",0,20); 
  exit(0);


  printf("conditioning\n");
  condition_graph_on_solo(&bn); 
  printf("done\n");
  exit(0);
  
  make_connected_components(&bn,&component);
  for (i=0; i < component.num; i++) 
    component.list[i].net.ct = bn2ct_pot(&(component.list[i].net.bn));  
      if (graph_triangulated(&(component.list[i].net.bn)) == 0)  
      printf("graph not triangluated\n");
      else printf("graph triangulated\n"); 


  
  
}




void 
paste_on_accomp(void) {  /* assumes firstnote and lastnote set and parsed soundfile has been
			    readin and parsed */
  int i,firstnote,latenote,firsta,lasta;
  float time,m,v;
  FILE *fp_solo,*fp_accomp;

  fp_solo = fopen("solo_times.dat","w");
  fp_accomp = fopen("accomp_times.dat","w");


  make_complete_bbn_graph();
  phrase_span(/*phrase*/ 0,&firstnote,&lastnote,&firsta,&lasta);

  for (i=firstnote; i <= lastnote; i++) {
    time = score.solo.note[i].realize;  /* all done in tokens */
    fix_solo_obs(i,time);
    fprintf(fp_solo,"%f %f\n",score.solo.note[i].time,time);
  } 
  
  equilibrium(component.list[0].net.ct);

  for (i=0; i < score.midi.num; i++) {
    time = score.midi.burst[i].time;
    if (time >= score.solo.note[firstnote].time) {
      if (time <= score.solo.note[lastnote].time) {
	get_accom_mean(i,&m,&v);
	fprintf(fp_accomp,"%f %f\n",time,m);
      }
    }
  } 
  fclose(fp_solo);
  fclose(fp_accomp);
}


void
recall_global_state_supposed_uneeded_computation() {
  int i;
  CLIQUE_TREE ct;
  BELIEF_NET bn;  

  printf("recalling the global belief state\n");
  for (i=0; i < component.num; i++)  {
    ct = component.list[i].net.ct;
    bn = component.list[i].net.bn;
    init_cliques_using_potentials(ct, bn);  
    equilibrium(ct);    
  }
}


void
set_conditional_times( char *name) {
  int i,iter,x,y,overlap[MAX_TRAIN_FILES],num_ex = 0,j,midi_bytes,ok;
  BNODE *bn;
  float like,val,rat,last_like,ws;
  QUAD_FORM qf;
  char buff[1000];
  TRAIN_FILE_LIST tfl;
  DATA_OBSERVATION dobs;
  RATIONAL r;

  //  dobs = read_data_observation(name);
  ok = read_data_observation(name,&dobs);
  if (ok == 0) { printf("failed reading data observation\n",name); exit(0); }
  if (dobs.num == 0) { printf("couldn't read %s\n",name); exit(0); }
  //  for (i=0; i < dobs.num; i++) printf("%s %f\n",dobs.tag_obs[i].name,dobs.tag_obs[i].val);
 // for (i=0; i < 5; i++) printf("wawa  %s cue = %d\n",score.solo.note[i].observable_tag,score.solo.note[i].cue);
 // for (i=0; i < 5; i++) printf("wawa  %s accomp xchg  %d\n",score.midi.burst[i].observable_tag,score.midi.burst[i].xchg);






  add_needed_clique_trees();








  for (j=0; j < component.num; j++) {
    if (component.list[j].net.ct.num == 0) continue;
	recall_state(component.list[j].net.ct);  /* sets clique tree to equilibrium again */


	/* in case the first phrase has no assoicated solo observations need to set the first
		accomp note time.  This isn't really the right thing to do -- what happens is that the
		accomp plays according to to the current model and then waits for the solo note.  this
		wouldn't make sense if the solo part wasn't played listening to accomp, or if tempos
		have changed */
	if (j == 0 && score.midi.burst[0].xchg) {
		for (i=0; i < score.solo.num; i++) if (score.solo.note[i].xchg || score.solo.note[i].cue) break;
		if (score.solo.note[i].cue) fix_obs(score.midi.burst[0].belief,.1);
	  /* not very safe ... should see where first accomp note actually played in real time */
	}


	set_data_observation(component.list[j].net.bn, dobs);
    printf("beginning eqilibrium %d\n",j);
    equilibrium(component.list[j].net.ct);    
   printf("ending eqilibrium %d\n",j);
  }

  set_accomp_range();



  for (cur_accomp=first_accomp; cur_accomp <= last_accomp; cur_accomp++) {
    bn = score.midi.burst[cur_accomp].belief;
    qf = post_dist(bn);  /* qf is marginal dist of variables assoc. with trainable node bn */
        printf("cur_accomp = %s, time = %f\n",score.midi.burst[cur_accomp].observable_tag,qf.m.el[0][0]);
	score.midi.burst[cur_accomp].actual_secs = qf.m.el[0][0];
	score.midi.burst[cur_accomp].ideal_secs = qf.m.el[0][0];
#ifdef JAN_BERAN
	score.midi.burst[cur_accomp].ideal_secs += .050;
#endif

  }

  /*  for (cur_accomp=first_accomp; cur_accomp <= last_accomp; cur_accomp++) {
    r = score.midi.burst[cur_accomp].wholerat;
    score.midi.burst[cur_accomp].ideal_secs = 2*r.num/(float)r.den;
    score.midi.burst[cur_accomp].actual_secs = 2*r.num/(float)r.den;
    }*/
}




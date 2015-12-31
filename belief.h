#ifndef BELIEF
#define BELIEF

#include "matrix_util.h"
#include "joint_norm.h"
#include "new_score.h"

#define OBS_DIM 1
#define PHANTOM_DIM 1
#define SOLO_DIM 2
#define ACCOM_DIM 2 /* 3 */
#define BACKBONE_DIM 2 /* 3 */
#define CATCH_UP_DIM 1
#define SOLO_PHRASE_UPDATE_DIM 1
#define BACKBONE_ATEMPO_DIM 3
#define BACKBONE_UPDATE_DIM 2


#define ACCOMP_TRAIN_ONLY "accomp_only"
#define SOLO_TRAIN "accomp+solo_train"
#define SOLO_ONLY_TRAIN "solo_only_train"


#define OBS_NODE 1
#define SOLO_NODE 2
#define PHANTOM_NODE 3   
#define ANCHOR_NODE 4
#define INTERIOR_NODE 5
#define LEAD_CONNECT_NODE 6
#define ACCOMP_LEAD_NODE 7
#define ACCOM_OBS_NODE 8
#define PHANTOM_CHILD_NODE 7
#define TAIL_NODE 8
#define HEAD_NODE 9
#define PHANTOM_ACCOM_NODE 10
#define STRAGGLE_ACCOM_NODE 11
#define SOLO_UPDATE_NODE 12
#define ACCOM_INIT_NODE 13
#define SOLO_PHRASE_START 14
#define ACCOM_UPDATE_NODE 15
#define ACCOM_PHRASE_START 16
#define ACCOM_NODE 17
#define BACKBONE_SOLO_NODE 18
#define BACKBONE_MIDI_NODE 19
#define BACKBONE_UPDATE_SOLO_NODE 20
#define BACKBONE_UPDATE_MIDI_NODE 21
#define BACKBONE_UPDATE_NODE 22
#define BACKBONE_NODE 23
#define BACKBONE_PHRASE_START 24
#define BACKBONE_XCHG 25
#define ATEMPO 25
#define CATCH_UP_NODE 26
#define SOLO_XCHG_NODE 27
#define ACCOM_XCHG_NODE 28




typedef struct {
  int observed;  /* boolean.  is the value observed? */
  float pos;    /* only meaninful if boolean is true */
  MATRIX value;  /* the value of the state.  for use in simulaing */
} OBSERVATION;

#define MAX_BSUCC 30  // /*15 /*6/*20 /*200*/

typedef struct {
  int num;
  struct bnode *arc[MAX_BSUCC];
  MATRIX A[1/*MAX_BSUCC*/];  /* only applies to prev list */  //CF:  tranform from set of ALL parent dims to my value
  QUAD_FORM e[1/*MAX_BSUCC*/];  /* not used now  (0 is trying to save space */
  /* y = Ax + e defines a potential p(x,y) on x and y .  the local contribution
     to the joint dist is prod p_i(x_i,y) over all predecessor i */
} BEL_NEXT_LIST;




typedef struct {
  int active;     /* is constraint active */
  float lim;
} BNODE_CONSTRAINT;





typedef struct bnode {
  BEL_NEXT_LIST next;    //CF:  children
  BEL_NEXT_LIST prev;  /* only list used for DAG representation */   //CF:  parents
  BEL_NEXT_LIST neigh;  /* neighbor */
  int dim;  /* dimension of the Gaussian rv assoc with this node */ //CF:  random vector
  BNODE_CONSTRAINT *constraint;
  /*  SUPER_QUAD qf; // if node is root, this is Marg dist, ow this is a
		   conditional dist of this node  given prev  nodes */
  OBSERVATION obs;
  QUAD_FORM e;  /* the dist of this node is sum over predecessors of
		    Ak xk + e where Ak are the matrices for the
		    predecessors and xk are the rv of the predecessors.
		    if node has no predecessors, this is the initial
		    distribution of node */
  
  int node_num;
  int mcs_num;  /* Maximum cardinality search number */
  struct clique *clique;  /* node is a member of the clique */ /*phase this out */
  struct clique_node *clnode; /* node is a member of the clique */
  float meas_time; /* time of node in measures */
  int note_type;  /* (node_type) solo, accomp or observation */
  int index;     /* index into score */
  int visit;
  int mark;
  int focus;  /* boolean.  if 0 then DO NOT regard node as in graph.  */  //CF:  not used?
  int comp_index;  /* which (phrase) component is this in */
  int trainable;  /* boolean */
  int trained; // added 7-06 so we can jusrt write out trained distrbutions
  char *trainable_tag;
  QUAD_FORM *train_dist;
  RATIONAL wholerat;
  char *observable_tag;
  MATRIX suff_lin; /* used in training */
  MATRIX suff_quad;
  MATRIX prior_cov;  /* only used for trainable nodes */
  MATRIX prior_mean; 
} BNODE;




#define MAX_CLIQUE 6
#define MAX_CLIQUE_NEIGH 5
#define NUMERATOR 1
#define DENOMINATOR 2

typedef struct clique {
  int num;   //CF:   number of BNODEs in this clique
  BNODE *member[MAX_CLIQUE];   //CF:  pointers to those BNODEs
  SUPER_QUAD quad;  /* e^gauss is scaled prob dist */  //CF:  not used
  QUAD_FORM qf;  //CF:  The actual quad form for the clique potential.
  QUAD_FORM memory;  /* a stored qf.  this saves a states like e.g. equilibrium */
  int polarity;  /* NUMERATOR or DENOMINATOR */   //CF:  not used
} CLIQUE;


typedef struct {
  MATRIX null;
  MATRIX inf;
  MATRIX fin;
} SPACE_DECOMP;

typedef struct {
  SPACE_DECOMP old_snd;
  SPACE_DECOMP old_sep;
  SPACE_DECOMP old_rcv;
  SPACE_DECOMP new_sep;
  SPACE_DECOMP new_rcv;
} FLOW_SPACE;

#define MAX_FLOW_SPACE 1 /* 0 to compile in Borland */  
/* temporarily getting rid of this */

typedef struct {
  int num;
  FLOW_SPACE space[MAX_FLOW_SPACE];
}  FLOW_SPACE_TAB;


//CF:  a directional link between two cliques, with another clique used as the seperator potential
//CF:  (in practice, this always occurs in bidirectional pairs)
typedef struct {
  FLOW_SPACE_TAB flow_tab;      //CF:  not used?
  CLIQUE *intersect ;           //CF:  the seperator potential (stores node pointers and joint seperator potential)
  struct clique_node *ptr;      //CF:  pointer to destination clique node  
  int dry;  /* if true, outgoing flow must be performed before equilibrium reached */ //CF:  bool. See 'dryness' docs.
} CLIQUE_BOND;

typedef struct {
  int num;
  CLIQUE_BOND bond[MAX_CLIQUE_NEIGH];
} CLIQUE_NEIGH;
  
typedef struct clique_node {
  int clique_id;
  CLIQUE_NEIGH neigh;  //CF:  arcs to neighbouring cliques
  CLIQUE *clique;
} CLIQUE_NODE;
  


typedef struct {
  int num;
  CLIQUE_NODE *root;
  CLIQUE_NODE **list;
} CLIQUE_TREE;


typedef struct {
  int num;
  CLIQUE **el;
} POTENTIAL_LIST;


#define MAX_BELIEF_NODES  40000 //20000 //10000  /*5000 /*1000  /* 5000 messes all up */

typedef struct {
  int num;
  /*  BNODE *list[MAX_BELIEF_NODES];*/
  BNODE **list;
  POTENTIAL_LIST potential;
} BELIEF_NET;

typedef struct {
  BELIEF_NET bn;
  CLIQUE_TREE ct;
} NETWORK;



typedef struct {
  BNODE *bn;
  float val;
} BELIEF_UPDATE;

#define MAX_BELIEF_QUEUE 10

typedef struct {
  int num;
  BELIEF_UPDATE update[MAX_BELIEF_QUEUE];
} UPDATE_QUEUE;


//CF:  stores results of training the BN.  For reading/writing to files.
typedef struct {
  int num;
  char **tag;      //CF:  node name
  QUAD_FORM *qf;   //CF:  node's distribution
} TRAINING_RESULTS;


 


void flush_belief_queue();
void set_conditional_times( char *name);
BELIEF_NET make_solo_graph(int start_note, int end_note);
QUAD_FORM belief_init_update(float length);
void belief_test(void);
MATRIX evol_matrix(int dim, float length);
void paste_on_accomp(void);
void update_belief_solo(int num, float secs);
void update_belief_accom(int num, float secs, int flush);
void init_forest(int fnote, int lnote);
void make_belief_network_graphs();
void exper_init_forest();
void synthesize_mean_accomp();
void play_accomp_performance();
char *trainable_tag(int type, int index);
char *trainable_tag_rat(int type,  RATIONAL rat);
void read_bbn_training_dists_to_score(char *s);
void bbn_train_composite_model();
void make_complete_bbn_graph_arg(char *answer);
void recall_global_state();
void recall_partial_global_state();
int bbn_train_composite_model_mem();
void recall_global_state_supposed_uneeded_computation();
float get_whole_secs(RATIONAL r);
void bbn_training_name(char *s, char *name);
int make_complete_bbn_graph_windows();
void add_needed_clique_trees();
void bbn_train_from_scratch_composite_model();
void bbn_train_accomp_composite_model();
int training_tag(char *pos, char *prefix, float *wholesecs);
void  write_the_bbn_training();
TRAINING_RESULTS  get_bbn_training_dists(char *s, int *flag);
void bbn_train_composite_model_mem_one_ex();
int train_tag(int type, RATIONAL rat, float *wholesecs);
void recalc_next_accomp();
void get_solo_mean(int n, float *m, float *v);

#endif




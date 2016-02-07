#ifndef SHARE
#define SHARE

#define WINDOWS


#include "local_defs.h"

//#define DEBUG
/*#define SUPER_DEBUG*/
/*#define VERBOSE */

/* #define SUN_COMPILE */


#include "new_score.h"
#include "kalman.h"
#include "belief.h"
#include "matrix_util.h"
#include "dp.h"

#include "cdread.h"

//#define MIREX
//#define BUG_SEARCH
#define POLYPHONIC_INPUT_EXPERIMENT
#define ORCHESTRA_EXPERIMENT
//#define NOTATION
//#define JAN_BERAN
//#define START_ANYWHERE_EXPT

//#define VISTA
/*#ifdef JAN_BERAN
  #define BOSENDORFER
  #endif*/
/*#define PIANO_RECOG
#define GRAPH_EXPERIMENT
#define PROFILE_EXPERIMENT   */

//#define SKIP_APP_STORE

//#define DISTRIBUTION
//#define COPY_PROTECTION

#define PROGRAM_KEY "Music+One"

//#define ROMEO_JULIET
//#define SPECT_TRAIN_EXPT

//#define ALIASING_EXPERIMENT  
/* this filters 48Khz data before downsampling to 8Khz at present this makes things worse ... don't know why*/
//#define TIMING_KLUDGE
//#define ATTACK_SPECT  // this shows very small improvement on sibelius data
//#define GAMMA_EXPERIMENT
#define NOEXIT
//#define PRE_VOCODED_AUDIO
//#define REAL_TIME_READ_VOC







/*changes   

  rachmaninov has

  #define POLYPHONIC_INPUT_EXPERIMENT
  training in local_defs.c

  but not
  #define ACCOUNT_FOR_DELAY_EXPERIMENT

*/



//#define POLYPHONIC_INPUT_EXPERIMENT
//#define ORCHESTRA_EXPERIMENT



/*#define RUNNING_GUI        using graphical user interface */
#define TUTORIAL_PIECE "saintsaens_intro+rondo"
#define SA_PITCH_TABLE_IMPROV  /* need to test this modification with
				  all versions that call sounding_accomp().  currently
				  just tested with synth mode.  comment out the
				  define to be in test mode where both old and
				  new wayrs are tried and compared */
		  

#define BOSENDORFER_LAG /*-.15*/ .2  // if you change this check out oddity in frame2spect /* seconds from receiving midi to key strike*/
#define LIVE_ERROR 2  /* if (performance_interrupted == 1) person clicked stop button.  
			 if (performance_interrupted == LIVE_ERROR) write to live_failure_string */

#define HIRES_OUT_NAME  "temp.48k"
#define QUANTFILE "quant.dat"
#define DISTRIBUTION_FILE "distribution.dat"

#define LOG_FILE "log.txt"
#define FRAMELEN 512     /* size of data chunk for evaluation */
#define SKIPLEN  256     /* samples between the starts of sucessive frames */
#define TOKENLEN  SKIPLEN //256     /* size of data chunk for evaluation */
/* kth frame includes samples from  

    (k+1)*SKIPLEN - FRAMELEN -->  (k+1)*SKIPLEN

   the starting point could be negative */

  #define FREQDIM   1024 //512       /* size of fft */


#define SAMPLE_SR    8000
#define NOMINAL_SR    8000

#define OUTPUT_SR  48000
#define TRUE_OUTPUT_SR 48000 //48002.93 
#define NOMINAL_OUT_SR 48000

/*#define OUTPUT_SR  48010
#define TRUE_OUTPUT_SR 48010 //48002.93 
#define NOMINAL_OUT_SR 48000 */

#define IO_FACTOR (NOMINAL_OUT_SR/NOMINAL_SR)

#define MIDI_MIDDLE_C 60

#define BITS_PER_SAMPLE 16  /* this constant is hardwired into sample conversion */
/*#define SR       8000     sampling rate in Hz */
#define CHANNELS 1
#define BYTES_PER_SAMPLE (CHANNELS*BITS_PER_SAMPLE/8)
#define BYTES_PER_FRAME (TOKENLEN*BYTES_PER_SAMPLE) 
#ifdef BUG_SEARCH
#define MAX_AUDIO_SECS 4200
#else
#define MAX_AUDIO_SECS 2000 //1300 //1000 //600 //420
#endif
#define MAXAUDIO ((int) (MAX_AUDIO_SECS*NOMINAL_SR*BYTES_PER_SAMPLE))
#define MAX_FRAMES MAXAUDIO/BYTES_PER_FRAME


//#define AUDIO_DIR "C:/Users/Public/Documents/audio"
#define AUDIO_DIR "audio"
#define SCORE_DIR "scores"
#define WIN_TRAIN_DIR "train"
#define MISC_DIR "data"
/*#define AUDIO_DIR "audio"
#define SCORE_DIR "scores"
#define MISC_DIR "data"
#define WIN_TRAIN_DIR "train" */
#define DP_EX_SUFF "ex"
#define BBN_EX_SUFF "bex"


#define MAX_ACCOMP_NOTES 15000 //11000 //8000 //4000     /* maximum accompaniment notes in note list */
// linker fails with max_accomp_notes = 12000,  maybe should (m)allocate at runtime?
//#define MAX_ACCOMPANIMENT_NOTES 4000     /* maximum accompaniment notes in note list */
#define MAX_SOLO_NOTES 5000 //2000     /* maximum accompaniment notes in note list */
//#define MAX_POLY_SOLO_EVENTS 20000     /* maximum accompaniment notes in note list */
#define PI       3.14159  
/*#define MAXAUDIO 6000000   1 meg is about a minute  */
  /* if using graph many nodes required in backward direction of baum selch */
#define SCALE     .2  /* relates mean to std  3std's gets most mass  */
#define OCTAVES 10
#define PITCHNUM 12*OCTAVES+1 
#define RESTNUM 12*OCTAVES
#define RHYTHMIC_RESTNUM 12*OCTAVES+4
#define TIENUM  12*OCTAVES+2
#define ATTACKNUM  12*OCTAVES+3
/*#define FERMATA  12*OCTAVES+4     the fermata cue currently is hiding in the midi event list */

#define NOTE_ON   0x90
#define NOTE_OFF  0x80
#define MIDI_PITCH_NUM 128 // total # of midi pitches



#define FERMATA 1
#define MIDI_COM 2
#define UPDATE 3

/*#define MAX_FRAMES 10000*/


#define TIMELAG .2   /* run dsp TIMELAG secs behind conductor */
/*#define ZTIME .75      a fn of TIMELAG.  if the conductor waits ZTIME  */
/*#define ZTIME .90      a fn of TIMELAG.  if the conductor waits ZTIME
                        before starting recording, then start of recording
                        coincides with start of music kit output */

#define MIDI_MIDDLE_C 60
#define IRLVNT 0  /* field is irrelevant */
#define PARSE_MODE     -1
#define SYNTH_MODE -2
#define FORWARD_MODE -3
#define BACKWARD_MODE -4
#define NO_SOLO_MODE -5
#define LIVE_MODE -6
#define GAUSS_DP_MODE -11
#define SOUND_CHECK_MODE -9
#define CALLIB_MODE -10
#define BELIEF_TEST_MODE -7
#define SIMULATE_MODE -8  /* the realtime dp is simulated with an existing data file, but not waiting for 
			     data to be ready as it would in LIVE_MODE or SYNTH_MODE */
#define SIMULATE_FIXED_LAG_MODE -12    // like SIMULATE_MODE but estimating onset times at saved lags
#define PV_MODE -13   
#define MIDI_MODE -14   
#define BASIC_PLAY_MODE -15   
#define TEST_BALANCE_MODE -16  
#define INTRO_CLIP_MODE -17 
#define HALF_STEP 1.059463
#define LOWEST_NOTE 0 //(MIDI_MIDDLE_C - 2)
#define HIGHEST_NOTE (MIDI_MIDDLE_C  + 2*12 + 7)
//#define LOWEST_NOTE (MIDI_MIDDLE_C - 3*12 + 2)
//#define HIGHEST_NOTE (MIDI_MIDDLE_C  + 2*12 + 7)


/************************** macros ******************************************/

#define min(x,y) ((x < y) ? x : y)
#define max(x,y) ((x > y) ? x : y)
/*#define abs(x)  ( ((x) >= 0) ? (x) : -(x))*/
#define square(x) ((x)*(x))
//#define round(x)  ( (int) ((x) + .5))
#define token2secs(t) ((t) * TOKENLEN / (float) SR)

/*******************************************************************************/


#define MS124T 1
#define MS124W 2
#define DIRECT_SERIAL 3



/********************************* states ********************************************/
#define STATE_NUM  8    /* total number of different states */
#define STAT_NUM   8    /* total number of different statistics */

#define NOTE_STATE 0
#define REST_STATE 1
#define ATTACK_STATE 2
#define ATTACK2_STATE 3
#define REARTIC_STATE 4
#define REARTIC2_STATE 5
#define SLUR_STATE 6
#define SLUR2_STATE 7
#define SETTLE_STATE 8
#define FALL_STATE 9   /* immediately after attack there is drop in intensity */
#define RISE_STATE 10
#define TRILL_HALF_STATE 11 /* these are not used now  half step trill */
#define TRILL_WHOLE_STATE 12

#define PITCH_STAT 0
#define ENERGY_STAT 1
#define BURST_STAT 2
#define OCTAVE_STAT 3
#define SIGNED_BURST_STAT 4
#define FUND_STAT 5
#define COLOR_STAT 6
#define TWO_PITCH_STAT 7



/*#define SCORE_DIR "/home/users/raphael/projects/sound/accomp/scores/folk/"*/
#define BASE_DIR "/home/raphael/accomp/"
  
/**************************************************************************************/


   /* typedefs */

/*typedef struct gnode;  */

typedef struct {
  struct gnode *ptr;
  float prob;
} ARC;

typedef struct {
  int num;
  int limit;
  ARC  *list;
} ARC_LIST;

#define ARG_NUM 2
#define THIS_PITCH 0  /* indicies into the desribed arguments */
#define LAST_PITCH 1

typedef struct { /* complete id of statistic.  arg[0] will probably 
                    contain pitch if needed and arg[1] will contain 2nd pitch */

  int statnum;  /* number of statistic */
  int arg[ARG_NUM];
  float val;
} STAT_ID;




typedef struct {
  int statenum;    /* type of node ie. note, rest, etc */
  int arg[ARG_NUM];
} FULL_STATE;

typedef struct {
  int statnum;    /* number of stat */
  int arg[ARG_NUM];
} STAT_INDEX;



typedef struct {
  STAT_INDEX si;
  float val;
} STAT_REC;


typedef struct gnode {   /* a dynamic programming graph node */
  /* int type;     type of node ie. note, rest, etc */
  /*  int pitch;    in midi numbering */
  /*  int last_pitch;    in midi numbering */
  FULL_STATE state;  //CF:  statnum is same as 'shade', eg. rest, attack, etc.
  //  int is_trill;  /* is node the non-principal note of trill */
  int note;    /* this node represents the note(th) note in solo part */
  int pos;     /* how far into this note */
  int visit;   /* has this node been visited (for check_graph() only */ //CF: dirty bit for tree visiting algorithms
  float token_done;   //CF:  not used
  ARC_LIST next;     //CF:  links to children
  ARC_LIST prev;     //CF:  links to parents
  int row;    /* for making tex graph only.  what row should node appear in */
  int xpos;   /* also for tex graph only */
  int ypos;  
} GNODE;



//CF:  a NODE is a hypothesis about which score state (GNODE) we are in at the CURRENT time.
//CF:  NODE is a misnomer now -- the GNODE is a node in the state graph (ie one node per state)
//CF:  NB. Only 'place' and 'prob' are used - the other variables are not used.
//CF:  NODEs exist in the hypothesis list 'active'.
typedef struct nd  {   /* the dynamic programming search tree is made of these */
   float      prob;    /* probability of a node */
/*   float      *ptr;     points to current place in table */
/*   int        mean;     expected number of tokens for the note rounded */
/*   int        begin;    time in tokens  when this node began */
   int        note;    /* this node represents the note(th) note in the note list */
   int        active;  /* flag for: is in active list ? */
   struct nd  *mom;    /* parent node */
   struct nd  *son;    /* child node */
   int        start;    /* note can change only ofter start iterations */
   int        rest;     /* in rest mode? */
   GNODE *orig;     //CF:  not used in active list.  (It's used later in the mini-backward pass's 'link' array)
   GNODE *place;    //CF:  the hypothesised score state 
  GAUSS_STATE gs;
} NODE,*NODEPTR;



typedef struct  {
  int pitch;
  float meas;
  int found;
  int wait;
  float start_time;
  float end_time;
  int vel;
  char observable_tag[50];  /* char string that identifies the observable data assoc with this event */
  struct midi_event *note_on;
  RATIONAL wholerat;  /* rational number giving score position in whole notes */
} ACCOMPANIMENT_NOTE;




typedef struct midi_event {
  int tag;  /* MIDI_COM , FERMATA , UPDATE */
  int trill;  /* 0 means no trill, ow the other pitch */
  unsigned char notenum;  /* midi note num  */
  unsigned char command; /* NOTE_ON / NOTE_OFF */
  unsigned char volume;
  float meas;   /* beats */
  float secs;
  float tempo;
  float realize;  /* time this event occured in secs for latest run */
  ACCOMPANIMENT_NOTE *accomp_note;
  RATIONAL wholerat;
} MIDI_EVENT;

typedef struct {
  int num;
  MIDI_EVENT *event;
} MIDI_EVENT_LIST;


#define MAX_EVENTS 40000 //20000//10000


typedef struct {
  int num;
  MIDI_EVENT list[MAX_EVENTS];
} EVENT_LIST;

typedef struct {
  int note_num;  /* number of note for this PHRASE */
  int trained;   /* is the phrase starting with this PHRASE current? */ 
  JN state_init;  //CF:  not used
  NETWORK net;
} PHRASE_LIST_EL;

#define MAX_PHRASES /*1000*/ 100 //40 

typedef struct {
  int num;
  int cur;
  /*  PHRASE_LIST_EL list[MAX_PHRASES];*/
  PHRASE_LIST_EL *list;
} PHRASE_LIST;

#ifdef JAN_BERAN
#define MAX_SOUNDING_NOTES 30
#else
#define MAX_SOUNDING_NOTES 15 //25  // I suspect this can be much smaller 
#endif

typedef struct {
  int num; // total # of sounding notes
  int vel[MAX_SOUNDING_NOTES];   /* inital velocity */
  float age[MAX_SOUNDING_NOTES];  /* age in secs before this "chord" */
  int snd_nums[MAX_SOUNDING_NOTES]; //midi pitch
  int attack[MAX_SOUNDING_NOTES];  /* boolean just started this midi burst? */
  int trill[MAX_SOUNDING_NOTES];  /* boolean just started this midi burst? */
} SOUND_NUMBERS;

#define MAX_SCHEDULE 5

typedef struct {
  int num;
  float exec_secs[MAX_SCHEDULE];
  float set_secs[MAX_SCHEDULE];
} SCHEDULE_LIST;

typedef struct  {
  int frames;                //CF:  corresponding frame time of this event in the prerecorded vocoder acc file
  RATIONAL wholerat;         //CF:  position in score time (in whole notes)
  //SCHEDULE_LIST schedule;//CF: not used? (an acc event can get rescheduled many times as new solo data comes in; logged here)
  float secs;
  float time;               /* start time in measures */   //CF:  not used?
  float ideal_secs;         /* time in seconds this should happen */  //CF:  schedular writes to this.
  float ideal_timer_ret;    /* max(now(),ideal_secs) when note is scheduled for
			     callback */ //CF:  just for logging
  float ideal_set;         /* time at which above ideal was decided on */
  float length;       /* length in measures */
  float mean;         /* mean length in token */
  float std;          /* std of length is tokens */ //CF:  not used
  float size;         /* expected  measure size in seconds for this note */ //CF:  not used?
  int   click;        /* boolean.  is this a reference point */ //CF:  not used
  float clicklen;     /* length in measure until next reference point */ //CF:  not used
  float clickmean;    /* mean length in tokens until next reference point */ //CF:  not used
  int   count;        /* mean and std based on count observations */ //CF:  not used
  int   num;          /* index into sol */ //CF:  not used
  int node_type;    //CF:  not used
  float actual_secs; /*realize;       when played this last performance (in seconds) */
  float orchestra_secs; /* time when played in orchestra recording (measures same thing as 'frames' but in secs) */
  float recorded_secs;  // in the midi-output performance we just read in, where was onset
  int coincides;  /* coincident with a solo note ? */  //CF:  not used
  int connect; /* connect to previous note  in dependency graph? */ //CF:  not used
  int acue;        //CF:  bool. is this an acc cue. Not used.

  int xchg;  /* bool, exchange "control" to accomp part */
  int gate;  /* bool, index of solo note which must be heard before playing burst (-1 if none) */
  int pedal_on;   //CF:  sustain pedal commands
  int pedal_off;  //CF:  ""
  int mark;
  SOUND_NUMBERS snd_notes; /* list of all currently sounding accomp notes */ //CF:  for MIDI sound feedback adjustment
  //  JN update;  /* update or initial dist if first note */ //CF:  not used
  QUAD_FORM qf_update;    //CF:  not used
  QUAD_FORM prior;        //CF:  not used
  MATRIX kalman_gain; /* kalman gain matrix */ //CF:  not used
  
  //CF:  important:
  BNODE *belief;  //CF:  pointer to observable acc node in bayes net
  BNODE *hidden;  //CF:  "" to backbone parent
  BNODE *catchit; //CF:  not used
  char observable_tag[50];  /* char string that identifies the observable time assoc with this note */
  char trainable_tag[50];  /* char string that identifies the observable time assoc with this note */

  MIDI_EVENT_LIST action;   /* the events happening at this time */ //CF:  ie MIDI commands to play

  float *spect;      //CF:  model of MIDI feedback contribution to audio
  float *atck_spect; // the spectrum of the attacking notes --- same as spect if no hold over notes
  int set_by_hand;   //CF:  bool, was the frame location set by hand.
} MIDI_BURST;     //CF:  the BURST is what Dovey calls a 'simultaneity', ie a group of MIDI commands
                   //CF:  that all occur simultaneously.  Every time point in the acc that something happens
                   //CF:  gets modelled as a separate MIDI_BURST.    
                  //CF:  NB this is still all used in the audio version; the MIDI commands arent actually played
                  //CF:  but their representations are matched to the audio by the 'frames' variable.


#define MAX_TRAIN 100


typedef struct {
  int num;
  int list[MAX_TRAIN];
} START_HIST;

#define MAX_SOLO_LENGTH_EXAMPS 25

typedef struct {
  int num;
  float secs[MAX_SOLO_LENGTH_EXAMPS];
} SOLO_LENGTH_LIST;

#define MELD_LEN 1 /* 1 to compile in Borland */

#define TUNE_LEN 3  // number of different tunings
#define SPECT_TRAIN_NUM 4


typedef struct  {

#ifdef SPECT_TRAIN_EXPT
  int positions;
#endif

#ifdef MIREX
  int index;
#endif
  float orchestra_secs; //CF:  only for removng soloist experiment -- not used now
   float time;         /* start time in measures */ //CF:  deprecated
   float length;       /* length in measures */ //CF:  not used
   float mean;         /* mean length in token */
   float std;          /* std of length is tokens */
   float size;         /* expected  measure size in seconds for this note */ //CF:  expected time per whole note
   int   click;        /* boolean.  is this a reference point */   //CF:  not used
   float clicklen;     /* length in measure until next reference point */ //CF:  not used
   float clickmean;    /* mean length in tokens until next reference point */ //CF:  not used
   int   count;        /* mean and std based on count observations */  //CF:  not used?
   int   num;          /* index into sol */  //CF:  not used
  int   trill;         /* the upper midi pitch of the trill (0 means no trill) */ //CF:  not used
   float realize;      /* frame in which this note was realized this (real-time)  iteration */ //CF:  for logging only
  int frames;  /* maybe this is redundant with realize */ //CF:  time the solo note was played (in most recent performance)
  int saved_frames;  /* for gauss parse */ //CF:  not used
   int lag;  /* how many tokens in past was note detected */ //CF:  for logging.  Latency in making the detection.
  int detect_frame;
   float off_line_secs;  /* off line esimate of note onset time */
   float on_line_secs;  /*  on line esimate of note onset time */
  float std_err;         /* standard error in estimate of note onset pos  in seconds */
  float  detect_time;   /* time when note detected (not estimated time) in general several frames late */
  float obs_var;       /* variance of the observation */  //CF:  ie observation noise (not performaer deviation)
  int   cue;          /* boolean.  is this note a cue note ? */ //CF:  use different model for cues
  int xchg;   /* exchange "control" to solo part */ //CF:  different performance modes, soloist or acc leading.
  int rhythmic_rest;  // solo rest treated as rhythmic event  --- somewhat rare
  int trigger; //CF:  bool, not used
  int tempo_change;   //CF:  bool, is this a tempo_change point (ie forward tempo probanility ignored at these points)
   float meas_size;    /* the expected measure size for this note */    //CF:  not used
   START_HIST examp;     /* start times for training examples */
   JN update;  /* describes change of state for kalman filter */ //CF:  not used
  char observable_tag[50];  /* char string that identifies the observable time assoc with this note */ //CF:  humanreadable string name for note
   MX state_error; /* state error covariance matrix */ //CF:  not used
   MATRIX kalman_gain; /* kalman gain matrix */  //CF:  not used
   QUAD_FORM qf_update; 
  //CF:  IMPORTANT BITS:
  BNODE *belief;           //CF:  pointer to Bayes Net observable node for this note (ie the solo node)
  BNODE *update_node;      //CF:  pointer to corresponding update node (ie delta time, delta speed)
  BNODE *hidden;           //CF:  pointer to corresponding backbone (hidden) node   
  BNODE *observe;          //CF:  same as belief (not used?)
  RATIONAL wholerat;       //CF:  musical postion in ratios of whole notes (semibreves) - as in the score files
  //  SOLO_LENGTH_LIST length_hist;
  float *spect;  //CF:  ideal spectrum model for this note
  int   peaks;  // number of peaks in spectrum
  float *trill_spect;  /* for non-principal note*/ //CF:  not used
  float *attack_spect; 
  float *meld_spect[MELD_LEN];  //CF:  used?   combined spectrum with nearby notes (done at score read time)
#ifdef SPECT_TRAIN_EXPT
  float *train_spect[SPECT_TRAIN_NUM];
#endif
  //    float *tune_spect[TUNE_LEN]; 
  MIDI_EVENT_LIST action;   /* for polyphonic solo */ //CF:  not used
  //CF:  important:
  SOUND_NUMBERS snd_notes; /* list of all currently sounding solo notes (for polyphonic solo)*/
  int gate;  /* can't detect solo note until accomp plays note indexed by this  (-1 if none)*/
  int set_by_hand; //CF:  bool, true if user has hand-set the position of this note. (eg. from GUI)
  int treating_as_observed;  // in online this was not ignored
} SOLO_NOTE;



typedef struct  {
   int   num;            /* total number of notes */ 
   SOLO_NOTE  note[MAX_SOLO_NOTES]; /* actual note array */
} SOLO_PART;

typedef struct  {
   int   num;            /* total number of notes */ 
   MIDI_BURST  burst[MAX_ACCOMP_NOTES]; /* actual note array */
} MIDI_PART;

typedef struct  {
   int   num;            /* total number of notes */ 
  //   ACCOMPANIMENT_NOTE  note[MAX_ACCOMPANIMENT_NOTES]; /* actual note array */
   ACCOMPANIMENT_NOTE  note[MAX_ACCOMP_NOTES]; /* actual note array */
} ACCOMPANIMENT_PART;


#define MAX_EXAMP_LENGTH 2100 //1500

typedef struct {
  int start; 
  int end;
  //    float time[MAX_EXAMP_LENGTH];
  float *time;  // changed this 11-06  doesn't seem to cost much to allocated as needed
} EXAMPLE;


#define MAX_EXAMPS 400

typedef struct {
    int num;
    EXAMPLE list[MAX_EXAMPS];
} EXAMP_DATA;

#define MAX_MEASURES 2000


typedef struct {
  RATIONAL wholerat;
  RATIONAL time_sig;
  RATIONAL beat;
  float pos;
  int meas;
  int visit; /* number of times through this meas */
} MEASURE_POS;

typedef struct {
  int num;
  MEASURE_POS  measure[MAX_MEASURES];  /* score pos of each measure (starting with measure 1  */
  RATIONAL pickup;
} MEASURE_INDEX;


#define MAX_REPEATS 100

typedef struct {
  int num;
  int left[MAX_REPEATS];
  int rite[MAX_REPEATS];
} REPEAT_LIST;


typedef struct  {
/*   float tempo;           initial tempo in beats per minute */
/*   float meter;           actually quarter notes  per bar */
   float measure;        /* length of measure in notevalues */ //CF:  not used
   float meastime;       /* time in secs for a measure */  //CF:  deprecated!
   SOLO_PART  solo;
   MIDI_PART  midi; //CF ac part
   ACCOMPANIMENT_PART  accompaniment; //CF not used
  EXAMP_DATA example;  /* phasing this out */ //CF:  list of performances and time at which notes occured in them, for training
  REPEAT_LIST repeat; //CF: for human score positions in tems of notaed repeats and jumps
  MEASURE_INDEX measdex; //CF:  time sig of each measure (also tells us if there's a pickup)
} SCORE;

typedef struct   {
   char name[6];          /* eg c#''' or ~ for rest */ 
   float omega;          /* freq in cycles per token */
} PITCH,*PITCHPTR;



/*typedef struct sgn {   //  state graph node 
  int num;      //an index into sol 
  float  samelp;   //log probabilities 
  float  newlp;
  struct sgn *samenote;   // left child 
  struct sgn *newnote;  // right child 
} SGNODE;     */



/*typedef struct {
  float parm;      // parameter value 
 int   count;   // number of values used for training 
} PARM; */

/*typedef struct {
  PARM rest;      // parm for rest 
  PARM change;    // parm for note change 
  PARM noterat;   // parm for ratio of powers 
  PARM noteext;   // parm for exterior of note 
} MODEL; */



/*typedef struct {
  float mean;
  float std;
} NORM; */



#define HIST_LIM 200

typedef struct {
  int stat_num;
  int pitch;
  float stat;
} HIST;

typedef struct {
  int num;
  STAT_REC list[HIST_LIM];
} STAT_HIST;


//CF:  this is like a NODE (ie a current state hypothesis)
//CF:  but it stores less data, and is used for keeping historical records of hypotheses.
//CF:  We find them used in the PROB_HIST structure. 
typedef struct {
  GNODE *place;
  float prob;
} SNAPSHOT,*SSPTR; 

/*typedef struct {
  int num;
  SNAPSHOT list[HIST_LIM];
} PROB_HIST;  */

#define AVE_SNAPS_PER_FRAME 100 // /*40 /*20 */ /* this should be connected to the minimum number of snapshots per frame */
#define MAX_SNAPS (MAX_FRAMES*AVE_SNAPS_PER_FRAME)


//CF:  history-record counterpart of the active hypothesis set.  A set of hyps that exisited at one point in time.
//CF:  forward and gamma(posterior) probabilities will be stored in these as a result of fwd and bkwd passes.
typedef struct {
  int num;
  SNAPSHOT *list;
} PROB_HIST; 


//#define VOL_LEVELS "vol_levels"
#define SOUND_INFO "sound_info.dat"
//#define VOL_QUANTA 20  // phasing this out.
#define VOL_TEST_QUANTA 28
#define FRAMES_PER_LEVEL 20
 


typedef struct {
  float input_sr;
  float output_sr;
  float volume_levels[VOL_TEST_QUANTA];
  float samp_zero_secs;  /* time at which sampling starts*/
  int padding1;
  int padding2;
  int padding3;
} LOCAL_SOUND_INFO;

enum structure_state  {s_cue, s_to_solo, s_to_accomp, s_set_tempo};

typedef struct {
   char tag[100];
   char desc[100];
   int index;
  int solo_control;  // boolean
  int changed;  // boolean
  enum structure_state state;
   RATIONAL wholerat;
  RATIONAL unit;
  int bpm;
}  TAG_EL;


#define MAX_TAG_EL 2000 //500

typedef struct {
  int num;
  TAG_EL el[MAX_TAG_EL];
} TAG_EL_LIST;

typedef struct {
  float fraction_done;  // in [0,1]
  int running;
  int ret_val;
    int indeterminate;
  char name[500];
    char *label;
  int   (*function)(void);
} BACKGROUND_INFO;


#define CIRC_CHUNKS 200 //30  // 30 caused crash on *very* slow test machine

typedef struct {
  FILE *fp;
  int chunks;
  int size;
  int cur;
  int seam;
  unsigned char **ptr;
} CIRC_CHUNK_BUFF;


#define NUM_MIXES 4  // solo+click, solo+accomp, live, solo alone

typedef struct {
  float volume;
  float pan;
  float balance;
} MIX;

#define SIMPLE_MIXES 3


typedef struct {
  float volume;
  float pan;
  int mute;
} SIMPLE_MIX;

typedef struct {
  RATIONAL start;
  RATIONAL end;
  int firstnote;
  int lastnote;
    int frames;
    char current_examp[500];
} PERFORMANCE_STATE;  // for simple toggling back and forth between the navigation panel and the live panel


#define SHORT_STRING_MAX 25

typedef struct {
    char inst[SHORT_STRING_MAX];
    char person[SHORT_STRING_MAX];
} PLAYER_INFO;


#define MAX_PLAYERS 100

typedef struct {
    int num;
    PLAYER_INFO player[MAX_PLAYERS];
} PLAYER_LIST;


#define MAX_INSTRUMENTS 10

typedef struct {
    int num;
    char *instrument[MAX_INSTRUMENTS];
}  INSTRUMENT_LIST;


#define INC_NUMS 10

typedef struct {
    int num;
    int cur;
    RATIONAL inc[INC_NUMS];
} INC_VALS;


#endif


 

















#include "share.h"

   /* global variables */


int upgrade_purchased = 0;
int buffer_shift = 0; // interpret the incoming audio and outgoing audio as differering by this many buffers.  a positive number will make the orchestra play earlier
unsigned int unique_id;  // an identification for this particular machine
int show_rests;
int clicks_in = 0;
int ensemble_in = 0;
int currently_playing;
int playing_opening_clip;
char full_score_name[500]; /* path and filename of score name without suffix  */
char score_dir[500]; 
int mode;     /* real-time or training */
int frames=0;   /* total number of frames */
int accomp_on_speakers=1;  /* the default */
EVENT_LIST   event;
EVENT_LIST   sevent;  /* for polyphonic solo */
PHRASE_LIST   cue;
SCORE score;   /* the musical score */
GNODE *start_node,*end_node;

PROB_HIST  prob_hist[MAX_FRAMES]; //CF:  for every frame, we record a PROB_HIST structure (the set of NODE hypotheses)

int new_data;  /* boolean.  have we read in new data yet this iteration? */
float *spect;        /* the fft'ed and processed token */
JN state_hat;  /* current kalman state */
float phrase_start;
int print_level = 1;
int firstnote,lastnote;
int first_accomp,last_accomp;
int   cur_note;
int cur_accomp;  /* the current index into the accompaniment part of score */
char audio_file[500];
char audio_data_dir[500];
int thread_nesting = 0;  /* every time code reentered through timer interrupt this is
			   incremented.  decremented when timer exits */
float start_meas;  /* the starting time of the excerpt in measures */
float end_meas;
RATIONAL start_pos;  /* the time where the performance starts -- could have solo, orch or both */
RATIONAL end_pos;
unsigned char *orchdata;  /* raw sound data from orchestra */
float restspect[FREQDIM/2];
float orchestra_spect[FREQDIM/2];
float unscaled_orchestra_spect[FREQDIM/2];
float *orch_data_frame;   /* one frame of orchestra data */
unsigned char *audiodata;  
char scoretag[500] = {0};
TEMPO_LIST tempo_list;
int stop_playing_flag=1;
char instrument[500];
char player[500] = "user";   // generic name for single-user system
char current_examp[500];
int performance_interrupted; /* boolean for when performanced stopped prematurely */
int done_recording;
int live_locked;
int audio_skew;  /* if both input and output used record sr, record samples- play samples */
int mic_vol;
LOCAL_SOUND_INFO sound_info;  // don't think this is used currently
float SR = 8000;  /* used to be define */
int vol_test_levels[VOL_TEST_QUANTA] = { 1,2,3,4,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,96,97,98,99,100};
RATIONAL spect_inc;
int is_hires=1;  // using hi resolution data? 
float mixlev=.5;
float pan_value = .5;  // in [0=all left,1=all right]
float master_volume = 1.;  
RANGE_LIST range_hist;
float room_response[FREQDIM/2];
int hyp_mask[FREQDIM/2];
int delay_seconds = 0;
//int timer_position;  
BACKGROUND_INFO background_info;
char score_title[500];
char asio_driver[500];
int freq_cutoff = 0;   //  only frequencies about this will be used in spectrum calculations (to filter our orch)
/* have 3 versions of time output, input and clock.  measure sr on both output and input.
   set clock to agree with ouput version of t=0.  measure input offset and account for this
   in conversion to secs */
int play_thread_active = 0; 
CD_INFO cd_info;
int play_frames_buffered;
#ifdef JAN_BERAN
int midi_accomp = 1; // 1 for midi accompaniment
#else
int midi_accomp = 0; // 1 for midi accompaniment
#endif
int using_asio = 0;
CIRC_CHUNK_BUFF orch_out;
CIRC_CHUNK_BUFF solo_out;
int bug_flag = 0;
int use_pluck;
MIX audio_mix[NUM_MIXES];
SIMPLE_MIX simple_mix[SIMPLE_MIXES] = {{.5,.5,0}, {.5,.5,0}, {.5,.5,0}};
float mic_input_level;
/*MIX live_mix = {.25, .5, .5};
MIX solo_accomp_mix= {.25, .5, .5};
MIX solo_click_mix = {.25, .5, .5};
MIX solo_only_mix = {.25, .5, .5};*/
int focus_on_live = 1;  // the live panel is enabled and the mixer controls refer to live setting
PERFORMANCE_STATE live_range = {{-1,0},{-1,0}, 0, 0};
PERFORMANCE_STATE take_range = {{-1,0},{-1,0}, 0, 0};
char user_dir[500];
char live_failure_string[500];
int mmo_mode = 0;  // only follow soloist on cues.
int orchestra_shift_ms = 0;  // shift orchestra times by this much.  positive value makes the orchestra play more "on top" of soloist
PLAYER_LIST player_list;
INSTRUMENT_LIST instrument_list;
int spect_wd;
int current_parse_changed_by_hand=0;



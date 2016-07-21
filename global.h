


/* external variables */


extern int upgrade_purchased;
extern int buffer_shift;
extern unsigned int unique_id;
extern int show_rests;
extern int clicks_in;
extern int ensemble_in;
extern int currently_playing;
extern int playing_opening_clip;
extern char full_score_name[]; /* path and filename of score name without suffix  */
extern char score_dir[];
extern int mode;     /* real-time etc. */
extern int frames;   /* number of frames in total sample */
extern int frames_target;
extern int accomp_on_speakers;
extern EVENT_LIST event;
extern EVENT_LIST   sevent;  /* for polyphonic solo */
extern PHRASE_LIST cue;
extern SCORE score;   /* the musical score */
extern GNODE *start_node,*end_node; /* for state graph */
extern PROB_HIST  prob_hist[MAX_FRAMES];
extern int new_data;  /* boolean.  have we read in new data yet this iteration? */

extern float *spect;        /* the fft'ed and processed token */
extern float *data_48k;
extern float *data;
extern int freqs;
extern PITCH *sol;   /* solfege array */
extern int  *notetime;       /* notetime[i] is time in beats the ith note starts */
extern int  token;
extern char scorename[];
extern char audio_data_dir[];
extern int  firstnote;
extern int  lastnote;
extern int first_accomp,last_accomp;
extern char score_title[];
extern char asio_driver[];

extern void write_features(char *name);

extern JN state_hat;  /* current kalman state */
extern float phrase_start;
extern int   cur_note;
extern int cur_accomp;  /* the current index into the accompaniment part of score */


extern int print_level;


extern char audio_file[];

extern int thread_nesting;



extern float start_meas;  /* the starting time of the excerpt in measures */
extern float end_meas;

extern RATIONAL start_pos;
extern RATIONAL end_pos;
extern unsigned char *orchdata;  /* raw sound data from orchestra */
extern float restspect[];
extern float orchestra_spect[];
extern float unscaled_orchestra_spect[];
extern float *orch_data_frame;
extern unsigned char *audiodata;
extern unsigned char *audiodata_target;
extern char scoretag[];
extern TEMPO_LIST tempo_list;
extern int stop_playing_flag;
extern char instrument[];
extern char player[];
extern char current_examp[];
extern int performance_interrupted; /* boolean for when performanced stopped prematurely */
extern int done_recording;
extern int live_locked;
extern int audio_skew;  /* if both input and output used record sr, record samples- play samples */
extern int mic_vol;
extern LOCAL_SOUND_INFO sound_info;
extern float SR;  /* used to be define */
extern int vol_test_levels[];
extern RATIONAL spect_inc;
extern int is_hires;
extern float mixlev;
extern float pan_value;
extern float master_volume;
extern RANGE_LIST range_hist;
extern float room_response[FREQDIM/2];
extern int hyp_mask[FREQDIM/2];
extern int delay_seconds;
//extern int timer_position;
extern BACKGROUND_INFO background_info;
extern int freq_cutoff;
extern int play_thread_active;
extern CD_INFO cd_info;
extern int midi_accomp;
extern int play_frames_buffered;
extern int using_asio;
extern CIRC_CHUNK_BUFF orch_out;
extern CIRC_CHUNK_BUFF solo_out;
extern int bug_flag;
extern int use_pluck;
extern MIX live_mix;
extern MIX solo_accomp_mix;
extern MIX solo_click_mix;
extern MIX solo_only_mix;
extern MIX audio_mix[];
extern SIMPLE_MIX simple_mix[];
extern float mic_input_level;
extern int focus_on_live;
extern PERFORMANCE_STATE live_range;
extern PERFORMANCE_STATE take_range;
extern char user_dir[];
extern char live_failure_string[];
extern int mmo_mode;
extern int orchestra_shift_ms;
extern PLAYER_LIST player_list;
extern INSTRUMENT_LIST instrument_list;
extern int spect_wd;
extern int current_parse_changed_by_hand;

extern float inst_freq[];
extern int inst_fbin[];
extern float inst_amp[];
extern float window[FRAMELEN];
extern float coswindow[FRAMELEN];
extern float coswindow2[FRAMELEN_PITCH];
extern float coswindow_1024[FREQDIM];
extern init_window();
extern init_cos_window();
extern init_cos_window2();
extern init_cos_window_1024();
extern float princarg(float phasein);
extern unsigned char *synth_pitch;
extern float *cumsum_freq;
extern int play_synthesis;
extern int test_audio_wave;
extern int last_analyzed_frame;
extern int first_analyzed_frame;
extern int frame_resolution;
void resynth_solo_phase_vocoder();
int count_database_intervals(char *directory, char *map_file_name);
int count_intervals(char *name, int transp);
extern void transpose_features(char *name, char *new_file, int semitones);
int get_score_transposition(char* name);
extern int tps;
extern int *database_pitch;







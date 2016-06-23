
#ifndef DP
#define DP

extern int performance_interrupted;


#define MAX_STD_SECS_DETECT  .05 /*.03*/  /* .05 */

void make_current_examp(char *name, int n);
int audio_takes_exist();
void init_live_state();
void get_data_file_name_piece(char *name, char *piece, char *num, char *suff);
int mirex_parse();
void alloc_node_buff();
void alloc_snapshot_buffer();
int forward_backward();
int get_parse_type(char *s);
int parse();
void save_labelled_audio();
void write_orch_parse();
void write_current_parse();
void next_free_examp(char *name);
int after_live_perform();
float next_frame_avail_secs();
int start_live(int md);
int  read_parse();
int read_audio_indep();
float  secs2tokens(float s);
float  tokens2secs(float t);
void async_dp();
void dpiter();
void synthetic_play();
void live_rehearse();
void synthetic_play_guts(char *type);
void live_rehearse_guts(char *type);
void good_example();
void  review_parse();
void read_orchestra_audio();
float hz2omega(float hz);
float omega2hz(int omega);
void get_rgb_spect(unsigned char **rgb, int *rows, int *cols);
int read_parse_arg(char *file);
void prepare_live(int m);
void audio_listen();
void get_example_file_name(char *examps);
void get_player_file_name(char *examps, char *suffix);
int simple_read_parse(int *fn, int *ln);
void finish_live_mode();
int is_mic_working(float freq);
void windows_simulate_play();
void windows_simulate_play_fixed_lag();
void write_current_parse_stump();
void make_current_dp_graph();
int is_on_speakers();
int is_parse_okay(int *pos);
void write_accomp_times();
void get_data_file_name(char *name, char *num, char *suff);
void get_score_file_name(char *name, char *suff);
void write_current_accomp_times();
void write_ideal_accomp_times();
void set_audio_file_num_string(char *num);
void store_live_state();
void store_take_state();
void restore_live_state();
void restore_take_state();


typedef struct {
  float h;
  float m;
  float v;
} ONED_KERNEL;


typedef struct gauss_state {
  //  GAUSS_KERNEL tempo;
  ONED_KERNEL tempo;
  ONED_KERNEL pos;
  float opt_scale;
  float alth;
  int note;
  int first_frame;
  int frame;
  int status;  /* BEFORE_START, AFTER_END, IN_NOTES */
  struct gauss_state *parent;
} GAUSS_STATE;



#endif


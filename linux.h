


#ifndef LINUX
#define LINUX

void set_audio_to_start();
int num_playback_samples_played();
void start_playback();
void end_playback();
float callback_time();
int begin_apple_duplex();
int stop_apple_duplex();
int faust_release();
int audio_warm_up_done();
void warm_up_audio();
void add_piece_to_verified(char *tag);
int already_verified(char *tag);
unsigned int str2code(char *s);
float  input_lag_secs();
int frames_behind();
int  start_play_callbacks();
int record_frame_ready(int f);
int start_audio_callbacks();
void duplex_test();
int end_playing();
int end_sampling();
float now();
int wave_write_samples(unsigned char *buff, int samples);
int get_playback_cursor();
void move_playback_cursor(int pos);
int write_audio_channels(float *left, float *rite, int num);
void set_async_event(float time);
void init_timer();
void begin_playing();
void dequeue_event();
void SetMasterVolumeMax();
void set_os_version();
void play_audio_buffer();
int play_frames_played();


int ready_for_play_frame(int frame);
int wave_set_out_volume(float vol);
int wave_turn_off_mic_boost();
int wave_set_mic_volume(float vol);
int SetMic(unsigned long vvolume);
int SetVolume(unsigned long vvolume);
int is_vista_running();
float system_secs();
float wave_audio_out_now();
void close_hires_output_file();
void prepare_hires_solo_output(int chunk);
int wave_write_channels(float *left, float *rite, int num);
void wave_set_audio_skew();
void play_midi_chunk(unsigned char *buff, int len);
void wait_ms(int ms);
void  get_live_midi_performance();
void end_midi();
void SetMasterVolumeMax(); 
int wave_play_frames_played();
int play_frames_queued();
void synchronize_clocks();
void set_audio_skew();
int samples_recorded();
int samples_played();
int SetMic(unsigned long vvolume);
void wave_send_in_buffer();
int wave_record_frame_ready(int f);
int is_record_frame_ready();
int wave_ready_for_play_frame();
void music_plus_one();
//void record_in_background();
//float now();
void set_timer_signal(float interval);
void write_audio_section(char *af, int start, int end);
//void  midi_change_program(int channel, int program);
void end_audio();
//void premature_end_action();
char **get_xpm_spect();
void play_in_background();
//int sample2int(unsigned char *temp);
//void int2sample(int v, unsigned char *samp);
//void float2sample(float x, unsigned char *samp);
//float sample2float(unsigned char *temp);
void prepare_playing_and_sampling();
int wave_prepare_playing(int sr);
int wave_write_samples(unsigned char *buff, int samples);
int wave_prepare_sampling();
int read_samples(unsigned char *buff, int samples);
void wave_end_playing();
int wave_begin_sampling();
void wave_end_sampling();
void cancel_outstanding_events();
//void play_audio_buffer_one_channel(int first_frame);
void start_play_in_background_gui(int start_frame);
//void get_rgb_spect(unsigned char **rgb, int *rows, int *cols);
//void samples2floats(unsigned char *audio, float *buff, int n);
void add_vert_function_to_spect(unsigned char **rgb, float *f, int lo, int hi, int col);
int  get_delay();
void rewind_playing(int mcs);
void install_signal_action(void (*func)(int));
void  wait_for_samples();
void init_clock();
void callibrate_clocks(); 
float callibrate_fraction(); 
void callibrate_calc(); 
void filtered_downsample(unsigned char *fr, unsigned char *to, int samps, int frame, int *produced);
void local_downsample_audio(unsigned char *fr, unsigned char *to, int factor);
void make_downsampling_filter();
void float_local_downsample_audio(unsigned char *fr, unsigned char *to, int factor);
int init_midi();
void wave_wait_until_play_time();

#endif




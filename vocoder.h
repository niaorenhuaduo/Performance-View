
#ifndef VOCODER
#define VOCODER

#include "share.c"
//#include "Resynthesis.h"

#define VOC_TOKEN_LEN  4096
#define VCODE_FREQS  (VOC_TOKEN_LEN/2)
#define HOP 4                  //CF:  number of grains getting mixed into each output frame
#define HOP_LEN (VOC_TOKEN_LEN/HOP)

float vcode_frames2secs(float f);
void set_netural_vocoder_rate();
float vcode_cur_pos_secs();
void set_vcode_rate(float rate);
int oracle_vcode();
void save_hires_audio_files();
int vcode_samples_queued();
void close_hires_orch_output();
float next_callback_seconds();
void strip_start_playing_orchestra();
void vocoder_test();
int  vcode_init();
void vcode_synth_frame_rate();
void vcode_init_raw_audio();
void start_playing_orchestra();
void vcode_action();
void reset_orchestra();
void plan_orchestra(int free_to_go);
void stall_orchestra();
float next_frame_time();
void adjust_vring_rate(float inc);
void get_vcode_rgb_spect(unsigned char **rgb, int *rows, int *cols);
void vcode_test();
void set_phase_func(float f, float *func);
void set_modulus_phase_func(float f, float *mod, float *phase);
void interleave_with_null_channel(unsigned char *out, unsigned char *inter, int n);
void interleave_mono_channels(unsigned char *left, unsigned char *right, unsigned char *mix, int n);

void set_orch_pitch(float p);
float get_orch_pitch();
unsigned int read_orchestra_times_name(char *name);
int read_48khz_raw_audio_name(char *name);
void read_vcode_data();
void vcode_initialize();
void set_vcode_data_type(int i);
void write_click_frame();
void orchestra_audio_info(unsigned char **ptr, int *samps);
void read_mmo_taps_name(char *name);
int vcode_samples_per_frame();
int vcode_audio_frames();
void check_if_cur_accomp_played();
void flush_hires_orch_data();
void init_orchestra();
int have_orchestral_intro();
void read_orchestra_shift();
void write_orchestra_shift();
void temp_rewrite_audio();
void temp_append_audio();
void write_features(char *name);
//void cal_vcode_features(AUDIO_FEATURE_LIST *flist);

//------------------------
// might want to move these elsewhere along with the #include <stdio.h>

int init_io_circ_buff(CIRC_CHUNK_BUFF *buff, char *name, int size);
void flush_io_buff(CIRC_CHUNK_BUFF *buff);
void write_io_buff(CIRC_CHUNK_BUFF *buff, unsigned char *out);
void queue_io_buff(CIRC_CHUNK_BUFF *buff, unsigned char *out);
void close_io_buff(CIRC_CHUNK_BUFF *buff);

#endif

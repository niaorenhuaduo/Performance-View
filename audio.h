#ifndef AUDIO_STUFF
#define AUDIO_STUFF


void reset_orch_pitch();
void triple_float_mix(float *s1, float *s2, *s3, float *o1, float *o2, int num);
void triple_mix(float c[], float *o1, float *o2);
float audio_power();
void add_channels(float *in1, float *in2, float *out, int n);
void mix_single_channel(unsigned char *left, unsigned char *mix, int n);
void  mix_channels(float c1, float c2, float *o1, float *o2);
void interleave_channels(unsigned char *left, unsigned char *rite, unsigned char *inter, int n);
void float_mix(float *s1, float *s2, float *o1, float *o2, int num);
int read_wav(char *af, unsigned char *adata);
void combine_hires_parts();
void float2sample(float x, unsigned char *samp);
void floats2samples(float *buff, unsigned char *audio, int n);
void int2sample(int v, unsigned char *samp);
int sample2int(unsigned char *temp);
float sample2float(unsigned char *temp);
void samples2floats(unsigned char *audio, float *buff, int n);
void samples2data();
int read_audio(char *af);
void write_audio(char *af);
int readaudio();
void play_audio_buffer_one_channel(int first_frame);
void combine_parts();
void record_in_background();
void set_sine_wave(char *ptr, float f, int sr, float secs);
void set_clicks(char *ptr, int sr, int secs);
int index2micvol(int vq);
int micvol2index(int vol);
void write_mic_level(int v);
int read_mic_level();
void init_sample_meas();
void make_sample_meas();
void analyze_sample_meas();
void analyze_room_response(int lo, int hi);
void write_sound_info();
int read_sound_info();
void downsample_audio(unsigned char *fr, unsigned char *to, int samps, int factor);
void upsample_audio(unsigned char *fr, unsigned char *to, int samps, int factor, int sample_bytes);
float max_audio_val();
void mix_mono_channels(unsigned char *left, unsigned char *right, unsigned char *mix, int n, float p1, float p2);
int create_wav();
void get_mix();
void save_mix();
void set_white_noise(char *ptr, int sr, float secs);
void set_silence(char *ptr, int sr, float secs);
void downsample_48k_solo();
void write_wav(char *af, short *buff, int samples,int channels);
void get_preference_file_name(char *examps);




#endif

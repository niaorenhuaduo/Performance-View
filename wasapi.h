#ifndef WASAPI_HEAD
#define WASAPI_HEAD

void PlayExclusiveStream();
void wasapi_test();
int test_wasapi_mixing();
//int wasapi_set_mic_volume(float vol);
//int wasapi_set_out_volume(float vol);
int wasapi_get_mic_volume(float *vol);
//int turn_off_mic_boost();


int set_mic_volume(float vol);
//int wasapi_set_out_volume(float vol);
int wasapi_get_mic_volume(float *vol);
int set_out_volume(float vol);
int turn_off_mic_boost();




/*void PlayAudioStream();
void RecordAudioStream();*/

//float wasapi_play_secs_buffered();
//int wasapi_record_frame_ready();
//void wasapi_send_in_buffer();
//void wasapi_end_sampling();

void wasapi_set_priority();


#ifdef __cplusplus
extern "C" {
#endif
float play_secs_buffered();
int  prepare_playing(int);
int  write_samples(unsigned char *interleaved ,int num);
int ready_for_play_frame(int frame);
int play_frames_played();
int end_playing();
int prepare_sampling();
int begin_sampling();
int end_sampling();
int record_frame_ready(int f);
int send_in_buffer();
void set_audio_skew();
int write_audio_channels(float *left, float *rite, int samps);
void wait_until_play_time();  
float now();
void set_os_version();
#ifdef __cplusplus
}
#endif 


/*#ifdef __cplusplus
extern "C" int  prepare_playing(int);
#else
int  prepare_playing(int);
#endif */


/* these functions use an ifdef to cover both wasapi and wave api's */

//int  prepare_playing(int sr);



#endif

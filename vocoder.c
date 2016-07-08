//#define ONLINE_READ  // read orchestra from disc during live performance
//#define TIMING_KLUDGE


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "share.c"
#include "dirent.h"

#include <signal.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//#include <sys/time.h>
#include "conductor.h"
#include "linux.h"
#include "vocoder.h"
#include "timer.h"
#include "global.h"
#include "times.h"
#include "new_score.h"
#include "audio.h"
#include "matrix_util.h"
#include "wasapi.h"
#include "belief.h"
#include "new_score.h"
#include "Resynthesis.h"


//#define ORCH_PITCH  444  /* assuming that the orch recorded at a=440 */
#define ORCH_PITCH  440 //442  /* assuming that the orch recorded at a=440 */



//CF:  a complex number
typedef struct {
  float modulus;
  float phase;
} VCODE_ELEM;


//CF:  the crcular output buffer
//CF:  (also the length of the grains produced)
typedef struct {
  float file_pos_secs;          //CF:  position in underlying audio file, in secs
  float obuff[VOC_TOKEN_LEN];   //CF:  the circular buffer
  float phase[VOC_TOKEN_LEN/2]; //phi,phase vector for latest generated grain (size=length/2 as symettric; audio is all real)
  VCODE_ELEM polar1[VOC_TOKEN_LEN/2]; 
  VCODE_ELEM polar2[VOC_TOKEN_LEN/2]; 
  /* these are polar audio represenations of the last two relevant spectra, used when interpolating over a "tap" */
} VCODE_STATE;


#define VCODE_STATE_HISTORY_LEN 10
																								   
//CF:  there will be one of these, the main structure.
typedef struct {
  VCODE_STATE state_hist[VCODE_STATE_HISTORY_LEN];  /* state_hist[i % xxx] is state before  //CF:  not used?
						       ith   frame computed */
  VCODE_STATE cur_state;  //CF:  ** the state
  float gain;             //CF:  master volume
  float rate;  /* secs of audio file / secs of real time */    //CF:  *** playback rate -- this is what we tweak! 
  //  float file_pos_secs;                                     //CF:  (occasianlly we make a skip)
  int  frame;   /* output frame */   //CF:  (not circular)
  FILE *fp;     //CF:  the underlying audio file
  //  VCODE_ELEM data[RING_SIZE][VOC_TOKEN_LEN/2];
  //  float obuff[VOC_TOKEN_LEN];
  //  float phase[VOC_TOKEN_LEN/2];
  //  float timing_interval;
  unsigned char *audio;   //CF:  the audio data
  int audio_frames;  /* lenght of audio data in frames */
  /*    float goal_record_secs;  /* want to be t goal_record_secs position in the recording
			    *at goal_real_secs */
  /*    float goal_real_secs;*/
}  VCODE_RING;

typedef struct {
  int num;
  int index[VCODE_FREQS];
} INDEX_LIST;


#define MAX_TAPS 300

typedef struct { 
  int num;
  float tap[MAX_TAPS];  /* in secs */
} TAP_STRUCT;


#define ORCH_BUFF_SECS 30


typedef struct {  // for online reading of orchestra audio
  FILE *fp;
  int seam;  /* the samp at seam is the "sample" samp */
  int sample;
  unsigned char buff[ORCH_BUFF_SECS*OUTPUT_SR*BYTES_PER_SAMPLE];
} ORCH_IO_STRUCT;


static VCODE_RING vring;
static float voc_window[VOC_TOKEN_LEN];   //CF:  shape of a smoothing window (eg. raised cosine, (1+cos t)/2  )
static float orch_pitch = ORCH_PITCH;     //CF:  not usually used; for pitch shifting the audio.
static float smooth_window[VOC_TOKEN_LEN];//CF:  window for FFT analysis (eg. Hamming etc. We use raised cosine again)
  //CF:  ignore this for now -- used for 'remove solist' experiments
static MATRIX spect_pairs;
static MATRIX spect_mean;
static MATRIX concentration;
static MATRIX var_pairs;
static MATRIX spectrogram;
static int rel_freqs;  /* only consider this many freqs ... others to high to bother with */
extern int Audio_fd;
extern unsigned char *audiodata;   //CF:  pointer to output audio buffer
static VCODE_ELEM **vcode_data;
static int num_vars;
static int first_frame = 0;   /* number of 1st frame played */
static int vcode_fd;   
static TAP_STRUCT mmotap;

static int is_pre_vocoded=0;   /* boolean. if yes then using data that is
				  already fourier transformed. owise use
				  raw audio data */


static FILE *hires_fp;
static FILE *hires_orch_in_fp;
#define HIRES_FRAMES 20
static unsigned char hires_solo_buff[HIRES_FRAMES*(VOC_TOKEN_LEN/HOP)*BYTES_PER_SAMPLE];
static ORCH_IO_STRUCT orch_in;








#define VOC_RAW_INPUT "mi_chiamano_mimi.raw"
#define VOC_TIMES "mi_chiamano_mimi_long.times"

/*#define VOC_RAW_INPUT "signore_ascolta.raw"

#define VOC_TIMES "signore_ascolta.times"*/

#define  SECS_PER_FRAME  (VOC_TOKEN_LEN/((float)(HOP*OUTPUT_SR)))
#define  SAMPLES_PER_FRAME  (VOC_TOKEN_LEN/((float)HOP))
#define  SAMPS_PER_FRAME  (VOC_TOKEN_LEN/HOP)


void
set_vcode_data_type(int i) {  /* 0 means raw data and 1 means spectrogram data */
  is_pre_vocoded = i;
}


void
set_orch_pitch(float p) {
  orch_pitch = p;
}

float
get_orch_pitch() {
  return(orch_pitch);
}

static float
nominal_tokens2secs(float t) {
  return(t*(SKIPLEN/(float)NOMINAL_SR));
}



float
vcode_frames2secs(float f) {
  return(f*VOC_TOKEN_LEN / ((float)HOP*OUTPUT_SR));
}

static float
vcode_secs2frames(float secs) {
  return(secs*HOP*OUTPUT_SR/(float)VOC_TOKEN_LEN);
}

void
write_orchestra_shift() {
  char file[500];
  FILE *fp;

  get_player_file_name(file,"sft");
  fp = fopen(file,"w");
  fprintf(fp,"%d",orchestra_shift_ms);
  fclose(fp);
}

void
read_orchestra_shift() {
  char file[500];
  FILE *fp;

  get_player_file_name(file,"sft");
  fp = fopen(file,"r");
  orchestra_shift_ms = 0;
  if (fp == NULL) return;
  fscanf(fp,"%d",&orchestra_shift_ms);
  fclose(fp);
}



unsigned int
read_orchestra_times_name(char *name) {
  FILE *fp;
  char s1[500],s2[500],s3[500],s[500],tag[500];
  int i,j,k;
  unsigned int key=0;
  float f,a,a1,a2,t1,t2,t;

  read_orchestra_shift();  // this is okay since player must be chosen before piece.
  //  fp = fopen(VOC_TIMES,"r");
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    return(0);  // returning key of 0 will certainly be rejected by key verification
  }
  fscanf(fp,"%s %s %s",s1,s2,s3);

  fscanf(fp,"%s %s %s",s1,s2,s3);

  fscanf(fp,"%s %s %s",s1,s2,s3);

  fscanf(fp,"%s %s %s",s1,s2,s3);

  for (i=0; i < score.midi.num; i++) score.midi.burst[i].orchestra_secs = 0.;
  while (1) {
    fscanf(fp,"%s %f %d",s,&f,&j);
    
    //    f -= 35;
    //    fscanf(fp,"%s %f",s,&f);

    /* sscanf(s,"atm_%d+%s",&k,s1);  // kluding repeat
    if (k < 80) printf("%s\t\t%f\t%d\n",s,f,j);
    else if (k >=80 && k < 159)  printf("atm_%d+%s(2)\t\t%f\t%d\n",k-79,s1,f,j);
    else printf("atm_%d+%s\t\t%f\t%d\n",k-79,s1,f,j);  */


    if (feof(fp)) break;
    if (s[0] != 'a') { printf("bad input in .times file %s\n",name); return(0); }
    
    for (i=0; i < score.midi.num; i++)  if (strcmp(score.midi.burst[i].observable_tag,s) == 0) break;
    if (i == score.midi.num) /* printf("couldn't find match for data %s\n",s)*/;
    else {
      score.midi.burst[i].frames = (int) f;
      key = 137*key+score.midi.burst[i].frames;
      score.midi.burst[i].set_by_hand = j;

      score.midi.burst[i].orchestra_secs = nominal_tokens2secs(f) + orchestra_shift_ms/1000.;
#ifdef TIMING_KLUDGE
      score.midi.burst[i].orchestra_secs = nominal_tokens2secs(f)+.040; //.015;  
#endif
      //            printf("%s = %f %f\n",score.midi.burst[i].observable_tag,f,score.midi.burst[i].orchestra_secs );
    }
    for (i=0; i < score.solo.num; i++) {
      if (strcmp(score.solo.note[i].observable_tag+5,s+4) == 0) break;
    }
    if (i != score.solo.num) {
      score.solo.note[i].orchestra_secs = nominal_tokens2secs(f); 
      //      printf("----------------------%s %f\n",score.solo.note[i].observable_tag,nominal_tokens2secs(f));
    }
    




    /* maybe this should be real rather than nomial seconds */
  }
  fclose(fp);
  if (strcmp(scoretag,"strauss_oboe_mvmt2") == 0)  return(key);
  
  /*    if (strcmp(scoretag,"strauss_oboe_mv1") == 0)  {
    return(key);
    } */


  for (i=0; i < score.midi.num; i++) {
	if (score.midi.burst[i].orchestra_secs <= 0) { printf("0 in times file at %s\n",score.midi.burst[i].observable_tag);
	return(0); }
    if (i == 0) continue;
    if (score.midi.burst[i].orchestra_secs  <= score.midi.burst[i-1].orchestra_secs) {
      printf("non increasing times in times file at %s\n",score.midi.burst[i].observable_tag);
      return(0);
    }
  }

  //  if (strcmp(full_score_name,"/home/raphael/accomp/data/midi/strauss_oboe_concerto/all") == 0)
  //    return;
  for (i=1; i < score.midi.num; i++) if (score.midi.burst[i].orchestra_secs == 0.) {
    /*    	   printf("skipping interpolation of missing index time\n");
		   continue;*/
    for (j=i+1; j < score.midi.num; j++) 
      if (score.midi.burst[j].orchestra_secs != 0.) break;
    a2 = rat2f(score.midi.burst[j].wholerat);
    t2 = score.midi.burst[j].orchestra_secs;
    for (j=i-1; j >= 0; j--) if (score.midi.burst[j].orchestra_secs != 0.) break;
    a1 = rat2f(score.midi.burst[j].wholerat);
    t1 = score.midi.burst[j].orchestra_secs;
    a = rat2f(score.midi.burst[i].wholerat);
    t = t1 + (t2-t1)*(a-a1)/(a2-a1);
    printf("couldn't find match for score %s interpolated to %f a2 = %f\n",
	   score.midi.burst[i].observable_tag,t,a2);

    score.midi.burst[i].orchestra_secs = t;
    //    score.midi.burst[i].frames = (int) (secs2tokens(t) + .5);
  }

  return(1);  // not doing the key now 


  /*  for (i=0; i < score.midi.num; i++) {
    printf("%d ",score.midi.burst[i].snd_notes.num);
    for (j=0; j < score.midi.burst[i].snd_notes.num; j++) 
      printf("%d ",score.midi.burst[i].snd_notes.snd_nums[j]);
    printf("\t%f\n",score.midi.burst[i].orchestra_secs);
  }
  exit(0);*/
  /*    if (strcmp(score.midi.burst[i].observable_tag,s) != 0) {
      printf("%s not equal %s\n",score.midi.burst[i].observable_tag,s);
      exit(0);
    }
    score.midi.burst[i].orchestra_secs = nominal_tokens2secs(f);
             printf("%s at %f (frames = %f)\n",score.midi.burst[i].observable_tag,score.midi.burst[i].orchestra_secs,f);
	     }*/
  
  /*  for (i=1; i < score.midi.num; i++) if (score.midi.burst[i].frames <= score.midi.burst[i-1].frames) {
    printf("inconsistent times file\n");
    printf("%s %f %s %f\n",score.midi.burst[i].observable_tag,score.midi.burst[i].frames,score.midi.burst[i-1].observable_tag,score.midi.burst[i-1].frames);
    exit(0);
    }*/
  printf("read %s\n",name);
  return(key);
}

static void
read_orchestra_times() {
  FILE *fp;
  char s1[500],s2[500],s3[500],s[500],name[500],tag[500];
  int i,j;
  float f,a,a1,a2,t1,t2,t;

  printf("enter the 'times' file for audio:\n");
  scanf("%s",name);
  read_orchestra_times_name(name);
  return;


  //  fp = fopen(VOC_TIMES,"r");
  fp = fopen(name,"r");
  if (fp == NULL) { printf("couldn't open %s\n",name); exit(0); }
  fscanf(fp,"%s %s %s",s1,s2,s3); 
  fscanf(fp,"%s %s %s",s1,s2,s3); if (strcmp(s1,"end_pos" != 0)) exit(0);
  fscanf(fp,"%s %s %s",s1,s2,s3); if (strcmp(s1,"firstnote" != 0)) exit(0);
  fscanf(fp,"%s %s %s",s1,s2,s3); if (strcmp(s1,"lastnote" != 0)) exit(0);
  for (i=0; i < score.midi.num; i++) score.midi.burst[i].orchestra_secs = 0.;
  while (1) {
    fscanf(fp,"%s %f %d",s,&f,&j);
    if (feof(fp)) break;
    if (s[0] != 'a') { printf("bad input in .times file %s\n",name); exit(0); }
    
    for (i=0; i < score.midi.num; i++) 
      if (strcmp(score.midi.burst[i].observable_tag,s) == 0) break;
    if (i == score.midi.num) /*printf("couldn't find match for data %s\n",s)*/; 
    else {
      score.midi.burst[i].orchestra_secs = nominal_tokens2secs(f); 
    }


    for (i=0; i < score.solo.num; i++) {
      if (strcmp(score.solo.note[i].observable_tag+5,s+4) == 0) break;
    }
    if (i != score.solo.num) {
      score.solo.note[i].orchestra_secs = nominal_tokens2secs(f); 
      //      printf("----------------------%s %f\n",score.solo.note[i].observable_tag,nominal_tokens2secs(f)); 
    }
    




    /* maybe this should be real rather than nomial seconds */
  }
  if (strcmp(full_score_name,"/home/raphael/accomp/data/midi/strauss_oboe_concerto/all") == 0) return;



  for (i=1; i < score.midi.num; i++) if (score.midi.burst[i].orchestra_secs == 0.) {
    //    printf("skipping interpolation of missing index time\n");
    //    continue;
    for (j=i+1; j < score.midi.num; j++) 
      if (score.midi.burst[j].orchestra_secs != 0.) break;
    a2 = rat2f(score.midi.burst[j].wholerat);
    t2 = score.midi.burst[j].orchestra_secs;
    for (j=i-1; j >= 0; j--) if (score.midi.burst[j].orchestra_secs != 0.) break;
    a1 = rat2f(score.midi.burst[j].wholerat);
    t1 = score.midi.burst[j].orchestra_secs;
    a = rat2f(score.midi.burst[i].wholerat);
    t = t1 + (t2-t1)*(a-a1)/(a2-a1);
    printf("couldn't find match for score %s interpolated to %f a2 = %f\n",
	   score.midi.burst[i].observable_tag,t,a2);

    score.midi.burst[i].orchestra_secs = t;
  }


  /*  for (i=0; i < score.midi.num; i++) {
    printf("%d ",score.midi.burst[i].snd_notes.num);
    for (j=0; j < score.midi.burst[i].snd_notes.num; j++) 
      printf("%d ",score.midi.burst[i].snd_notes.snd_nums[j]);
    printf("\t%f\n",score.midi.burst[i].orchestra_secs);
  }
  exit(0);*/
  /*    if (strcmp(score.midi.burst[i].observable_tag,s) != 0) {
      printf("%s not equal %s\n",score.midi.burst[i].observable_tag,s);
      exit(0);
    }
    score.midi.burst[i].orchestra_secs = nominal_tokens2secs(f);
             printf("%s at %f (frames = %f)\n",score.midi.burst[i].observable_tag,score.midi.burst[i].orchestra_secs,f);
	     }*/
}



#define READ_TEST_SIZE 1024
#define SLOPE_SECS 2.

#define PIT .8 //.8

static void
volume_correct() {
  int i,i0,i1,m;
  float s0,s1,x,p,q,f;
  RATIONAL r0,r1;

  r0 = string2wholerat("50+0/1");
  r1 = string2wholerat("58+1/4");
  //  r0 = string2wholerat("1+0/1");
  //  r1 = string2wholerat("6+0/1");
  for (i=0; i < score.midi.num; i++) {
    if (rat_cmp(r0,score.midi.burst[i].wholerat) == 0) 
      s0 = score.midi.burst[i].orchestra_secs;
    if (rat_cmp(r1,score.midi.burst[i].wholerat) == 0) 
      s1 = score.midi.burst[i].orchestra_secs;
  }
  m = SLOPE_SECS*OUTPUT_SR;
  //        printf("s1 = %f s0 = %f\n",s1,s0); exit(0);
  //            s0 = 5;
  //            s1 = 20;
  i0 = (int) (s0*OUTPUT_SR); 
  i1 = (int) (s1*OUTPUT_SR); 
  for (i=i0; i < i1; i++) {
    if ((i-i0) < m) { 
      p = (i-i0)/(float)m;
      q = 1-p;
      x = q + p*PIT;
    }
    else if ((i1-i) < m) { 
      p = (i1-i)/(float)m;
      q = 1-p;
      x = q + p*PIT;
    }
    else   x = PIT;
    //     printf("i = %d x = %f\n",i,x);
    f = x*sample2float(vring.audio + i*BYTES_PER_SAMPLE);
    float2sample(f,vring.audio + i*BYTES_PER_SAMPLE);
  }
}


int
vcode_audio_frames() {
  return(vring.audio_frames);
}

int
read_48khz_raw_audio_name(char *name) {
  FILE *fp;
  int i,total=0,samps,k,j;
  unsigned char temp[READ_TEST_SIZE];

  if (vring.audio) free(vring.audio);
  //    printf("reading 48kz audio %s\n",name); exit(0);
  //  fp = fopen(VOC_RAW_INPUT,"r");
  fp = fopen(name,"rb");
  if (fp == NULL) { printf("couldn't read %s\n",name); exit(0); }
  while (feof(fp) == 0) total +=  fread(temp,1,READ_TEST_SIZE,fp);

  samps = total/BYTES_PER_SAMPLE;
  vring.audio_frames = (int) (samps / SAMPLES_PER_FRAME);

#ifdef ONLINE_READ
  return(1);
#endif

  vring.audio = (unsigned char *) malloc(total);
  if (vring.audio == 0) { printf("couldn't alloc %d bytes in read_48khz_raw_audio_name\n",total); return(0);  }
  fseek(fp,0,SEEK_SET);
   fread(vring.audio,1,total,fp);  
     printf("read %d frames from %s\n",vring.audio_frames,name); 

    for (i=0; i < score.midi.num; i++) {
      k = (int)(score.midi.burst[i].orchestra_secs*48000);
      //      for (j=0; j < 500; j++) float2sample(sample2float(vring.audio + 2*(k+j)) + .1*sin(5000*j), vring.audio + 2*(k+j));
      //       for (j=k-5; j <= k+5; j++) vring.audio[2*j + 1] += 150; 
      //      vring.audio[2*k] += 150;
    }
  // exit(0);
  return(1);
}

static void append_features(char *name, AUDIO_FEATURE_LIST *list) {
    
    FILE *fp;
    fp = fopen(name, "r");
    if (fp == NULL) { printf("can't open %s\n",name); return; }
    
    int frames = 0;
    fscanf(fp,"Total number of frames: %d\n",&frames);
    
    AUDIO_FEATURE af;
    while (feof(fp) == 0) {
      fscanf(fp, "%d\t%f\t%f\t%f\n", &af.frame, &af.hz, &af.amp, &af.nominal);
      list->el[list->num++] = af;
    }
    
    fclose(fp);
}

#define MAX_DATA_NUM 10
int
read_48khz_raw_audio_data_base(char *directory, AUDIO_FEATURE_LIST *list) {
  FILE *fp;
  int i,total=0,samps,k,j;
  unsigned char temp[READ_TEST_SIZE];

  if (vring.audio) free(vring.audio);
  
  
  DIR *dir;
  struct dirent *ent;
  char feature_file_name[200];
  char audio_file_name_stub[200];
  char audio_file_name[200];
  
  int len[MAX_DATA_NUM];
  int cnt = 0;
  
  if ((dir = opendir (directory)) == NULL) return(0);
  
  while ((ent = readdir (dir)) != NULL) {
    if(strstr(ent->d_name, "_48k.raw") != 0){
      strcpy(audio_file_name, directory);
      strcat(audio_file_name, ent->d_name);
      fp = fopen(audio_file_name,"rb");
      if (fp == NULL) { printf("couldn't read %s\n",ent->d_name); exit(0); }
      int cur_total = 0;
      while (feof(fp) == 0) cur_total +=  fread(temp,1,READ_TEST_SIZE,fp);
      
      int temp_num_frame = (cur_total/BYTES_PER_SAMPLE)/SAMPLES_PER_FRAME;
      cur_total = temp_num_frame*SAMPLES_PER_FRAME*BYTES_PER_SAMPLE;
      len[cnt++] = cur_total;
      total += cur_total;
      fclose(fp);
      }
  }
  closedir (dir);
  
  
  samps = total/BYTES_PER_SAMPLE;
  vring.audio_frames = (int) (samps / SAMPLES_PER_FRAME);
  vring.audio = (unsigned char *) malloc(total);
  if (vring.audio == 0) { printf("couldn't alloc %d bytes in read_48khz_raw_audio_name\n",total); return(0);  }
  dir = opendir (directory);
 
  int offset = 0;
  cnt = 0;
  while ((ent = readdir (dir)) != NULL) {
    if(strstr(ent->d_name, "_48k.raw") != 0){
      strcpy(audio_file_name, directory);
      strcat(audio_file_name, ent->d_name);
      fp = fopen(audio_file_name,"rb");
      int file_frame_len = len[cnt++];
      fread(vring.audio+offset,1,file_frame_len,fp);
      fclose(fp);
      offset += file_frame_len;
  
      strcpy(audio_file_name_stub, ent->d_name);
      audio_file_name_stub[strlen(audio_file_name_stub) - 8] = 0; //remove "_48k.raw"
      strcpy(feature_file_name , directory);
      strcat(feature_file_name , audio_file_name_stub);
      strcat(feature_file_name, ".feature");
      append_features(feature_file_name, list);
    }
  }
  closedir (dir);
  return(1);
}

 
void
orchestra_audio_info(unsigned char **ptr, int *samps) {
  *ptr = vring.audio;
  *samps = vring.audio_frames * SAMPLES_PER_FRAME;
}

static void
read_48khz_raw_audio() {
  FILE *fp;
  int total=0,samps;
  unsigned char temp[READ_TEST_SIZE];
  char name[200];

  printf("enter the name of the raw 48KHz audio file:");
  scanf("%s",name);  
  read_48khz_raw_audio_name(name);
  return;
  //  printf("reading 48kz audio %s\n",name); exit(0);
  //  fp = fopen(VOC_RAW_INPUT,"r");
  fp = fopen(name,"r");
  if (fp == NULL) { printf("couldn't read %s\n",name); exit(0); }
  while (feof(fp) == 0) total +=  fread(temp,1,READ_TEST_SIZE,fp);
  vring.audio = (unsigned char *) malloc(total);
  fseek(fp,0,SEEK_SET);
  fread(vring.audio,1,total,fp);  
  samps = total/BYTES_PER_SAMPLE;
  vring.audio_frames = (int) (samps / SAMPLES_PER_FRAME);
  //  printf("read %d frames\n",vring.audio_frames); exit(0);
}


static float
vcode_hz2omega(float hz) {
  return(VOC_TOKEN_LEN*hz/(float) NOMINAL_OUT_SR);
}







static void
write_vcode_data(int start, int end) {
  char name[1000];
  FILE *fp;
  int f,i;

  printf("enter the name of the voc output file \n");
  scanf("%s",name);
  fp = fopen(name,"w");
  for (f=start; f < end; f++) fwrite(vcode_data[f],rel_freqs,sizeof(VCODE_ELEM),fp);
  fclose(fp);
}





void
read_vcode_data() {
  FILE *fp;
  int total=0,samps,f,n,i,b;
  float temp[VCODE_FREQS];
  char name[200];

#ifdef WINDOWS
  strcpy(name,user_dir);
  strcat(name,SCORE_DIR);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,".voc");
#else
  printf("enter the name of the prevocoded (.voc) audio data file:");
  scanf("%s",name);  
#endif
  fp = fopen(name,"rb");
  if (fp == NULL) { printf("couldn't read %s\n",name); exit(0); }
  for (n=0; ; n++) {
    b = fread(temp,rel_freqs,sizeof(VCODE_ELEM),fp);
    //    printf("b = %d n = %d\n",b,n);
    if (feof(fp)) break;
  }
  vring.audio_frames = n;
  fseek(fp,0,SEEK_SET);
  vcode_data = (VCODE_ELEM **) malloc(vring.audio_frames*sizeof(VCODE_ELEM *));
  printf("allocing %d bytes\n",vring.audio_frames*rel_freqs*sizeof(VCODE_ELEM));
  for (i=0; i < vring.audio_frames; i++) {
    vcode_data[i] = (VCODE_ELEM *) malloc(/*VCODE_FREQS*/rel_freqs*sizeof(VCODE_ELEM));
    if (vcode_data[i] == NULL) { printf("failed in mallocing vcode data\n"); exit (0); }
    fread(vcode_data[i],rel_freqs,sizeof(VCODE_ELEM),fp);  
  /* should be rel_freqs above*/
  }

  fclose(fp);

#ifdef  REAL_TIME_READ_VOC
  vcode_fd = open(name,O_RDONLY | O_NDELAY);
#endif
  

}



static void
init_vring_state() {
  int i,j;

  for (j=0; j < VOC_TOKEN_LEN; j++) vring.cur_state.obuff[j] = 0;
  for (j=0; j < VOC_TOKEN_LEN/2; j++) {
    vring.cur_state.phase[j] = 0; 
    vring.cur_state.polar1[j].modulus = vring.cur_state.polar1[j].phase = 0; 
    vring.cur_state.polar2[j].modulus = vring.cur_state.polar2[j].phase = 0; 
  }
}





#define MAX_FREQS_HZ 5000


void
vcode_initialize() {
  int i,j;
  float x;

  rel_freqs = (int) vcode_hz2omega(MAX_FREQS_HZ);
  num_vars = 2*rel_freqs;
  //  if (strcmp(scorename,"mimi") == 0) volume_correct();
  vring.frame = 0;
  init_vring_state();
  /*  for (j=0; j < VOC_TOKEN_LEN; j++) vring.cur_state.obuff[j] = 0;
      for (j=0; j < VOC_TOKEN_LEN/2; j++) vring.cur_state.phase[j] = 0; */
  for (j=0; j < VOC_TOKEN_LEN; j++) {
    x = 2*PI*(j - VOC_TOKEN_LEN/2)/(float)VOC_TOKEN_LEN;
    voc_window[j] = (1 + cos(x))/2;
    smooth_window[j] = (1 + cos(x))/2;
  }
  vring.cur_state.file_pos_secs = 0.;
  vring.rate = 1.;
}



float
vcode_cur_pos_secs() {
  return(vring.cur_state.file_pos_secs);
}



void
read_mmo_taps() {
  FILE *fp;
  char name[1000];
  float x;
  int i;

  mmotap.num = 0;
  strcpy(name,full_score_name);
  strcat(name,".tap");
  fp = fopen(name,"r");
  if (fp == NULL) return;
  for (i=0; i < MAX_TAPS; i++) { 
    fscanf(fp,"%f",&x);
    if (feof(fp)) break;
    mmotap.tap[i] =   toks2secs(x);
    //    printf("tap at %f\n",mmotap.tap[i]); 
  }
  mmotap.num = i;
}

void
read_mmo_taps_name(char *name) {
  FILE *fp;
  float x;
  int i;

  mmotap.num = 0;
  fp = fopen(name,"r");
  if (fp == NULL) return;
  for (i=0; i < MAX_TAPS; i++) { 
    fscanf(fp,"%f",&x);
    if (feof(fp)) break;
    mmotap.tap[i] =   toks2secs(x);
    //    printf("tap at %f\n",mmotap.tap[i]); 
  }
  mmotap.num = i;
}

void
vcode_buffer_hires_audio() {
  char stump[500];
  int f,i,j,b;

  b = (VOC_TOKEN_LEN/HOP)*BYTES_PER_SAMPLE;
  strcpy(stump,audio_data_dir);
  strcat(stump,current_examp);
  strcat(stump,".48k");
  hires_fp = fopen(stump,"rb");
  //  if (hires_fp == NULL) hires_fp = fopen(HIRES_OUT_NAME,"rb");
  if (hires_fp == NULL) { printf("vcode couldn't open %s\n",stump); return; /*exit(0);*/ }
  for (i=0; i < HIRES_FRAMES; i++)   fread(hires_solo_buff +  i*b , b, 1, hires_fp);
}


void
prepare_hires_orch_input_audio() {
  char stump[500],name[500];
  int f,i,j,b,t;

  strcpy(name,user_dir);
  strcat(name,SCORE_DIR);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,"/");
  strcat(name,scoretag);
  strcat(name,".raw");
  orch_in.fp = fopen(name,"rb");
  if (orch_in.fp == NULL) { printf("vcode couldn't open %s\n",name); exit(0); }
  t = (int) (score.midi.burst[cur_accomp].orchestra_secs - 1.);
  b = t*OUTPUT_SR;
  fseek(orch_in.fp,b*BYTES_PER_SAMPLE,SEEK_SET);
  fread(orch_in.buff , ORCH_BUFF_SECS*OUTPUT_SR*BYTES_PER_SAMPLE, 1, orch_in.fp);
  orch_in.seam = 0;
  orch_in.sample = b;
}



static void
get_input_audio(int samps) {
  int headroom,r1,r2,buff_len; 

  buff_len = ORCH_BUFF_SECS*OUTPUT_SR;  // in samples
  headroom = buff_len - orch_in.seam;
  r1 = (samps < headroom) ?  samps : headroom;
  r2 = samps - r1;
  fread(orch_in.buff + orch_in.seam*BYTES_PER_SAMPLE , r1*BYTES_PER_SAMPLE, 1, orch_in.fp);
  fread(orch_in.buff , r2*BYTES_PER_SAMPLE, 1, orch_in.fp);
  orch_in.seam = (orch_in.seam + samps) % buff_len;
  orch_in.sample += samps;
  //  printf("Read %d samps\n",samps);
}





#define BACK_SECS   5   // have this much audio behind current vring.cur_state.file_pos_secs

static void
replenish_orch_audio() {
  int need,new_samps;
  float t1,t2;

  t1 = now();
  new_samps = (int) ((vring.cur_state.file_pos_secs - BACK_SECS) * OUTPUT_SR);
  need = new_samps - orch_in.sample; 
  //  printf("need = %d new_samps = %d sample = %d\n",need,new_samps,orch_in.sample);
  if (need > 0) get_input_audio(need);
  t1 = now();
  if ((t2-t1) > .001) printf("waited %f secs at %f in replenish_orch_audio()\n",t2-t1,t2);
}


int
init_io_circ_buff(CIRC_CHUNK_BUFF *buff, char *name, int size) {
  int i;

  buff->fp = fopen(name,"wb");
  if (buff->fp == NULL) { printf("vcode couldn't open %s\n",name); return(0); }
  buff->cur = buff->seam = 0;
  buff->size = size;
  if (buff->ptr == NULL) {
    buff->ptr = (unsigned char **) malloc(sizeof(unsigned char *)*CIRC_CHUNKS);
    buff->chunks = CIRC_CHUNKS;
    for (i=0; i < CIRC_CHUNKS; i++) buff->ptr[i] = (unsigned char *) malloc(size);
  }
  return(1);
}


static int
prepare_hires_orch_output() {
  char stump[500];
  int f,i,j,b;

  strcpy(stump,audio_data_dir);
  strcat(stump,current_examp);
  strcat(stump,".48o");
  //  strcpy(stump,"temp.48o");

  return(init_io_circ_buff(&orch_out,stump,HOP_LEN*BYTES_PER_SAMPLE));
}


static int stalled_out=0;

//CF:  init vocoder
int
vcode_init() { 

  vring.frame = 0;
  vring.cur_state.file_pos_secs = 0.;
  vring.rate = 1.;
  vring.gain = 1.;
    stalled_out = 0;
  read_mmo_taps();
  init_vring_state();

#ifdef ONLINE_READ
  prepare_hires_orch_input_audio();
#endif

  if (mode != LIVE_MODE)  vcode_buffer_hires_audio();  // always want this if available (?)
  if (is_hires && mode == LIVE_MODE)  if (prepare_hires_orch_output() == 0) return(0);

  

#ifdef WINDOWS
  return(1);
#endif

  if (is_pre_vocoded) read_vcode_data();
  else read_48khz_raw_audio();

  /*#ifdef PRE_VOCODED_AUDIO
  read_vcode_data();
#else
read_48khz_raw_audio(); 

  //    vcode_data = (VCODE_ELEM **) malloc(vring.audio_frames*sizeof(VCODE_ELEM *));
  //  for (i=0; i < vring.audio_frames; i++) 
  //    vcode_data[i] = (VCODE_ELEM *) malloc(VCODE_FREQS*sizeof(VCODE_ELEM));
  //    vcode_data[i] = (VCODE_ELEM *) malloc(rel_freqs*sizeof(VCODE_ELEM));
  //    want to change vcode_data to rel_freqs eventually when we decide on appropriate
  //        value 
  #endif  */

  read_orchestra_times();  //CF:  read indices into audio (.times file)  
                           //CF:  The times file contains mappings from symbollic acconmpaniment events to 
                           //CF:  to when they occur within the audio file.   (events are almost all note-ons)
  return(1);
}


void
old_vcode_init() { 
  int i,j;
  float x;

  rel_freqs = (int) vcode_hz2omega(MAX_FREQS_HZ);
  num_vars = 2*rel_freqs;

#ifndef WINDOWS

  if (is_pre_vocoded) read_vcode_data();
  else read_48khz_raw_audio();

  /*#ifdef PRE_VOCODED_AUDIO
  read_vcode_data();
#else
  read_48khz_raw_audio();


  #endif   */

  read_orchestra_times();
#endif

  //  if (strcmp(scorename,"mimi") == 0) volume_correct();
  vring.frame = 0;
  for (j=0; j < VOC_TOKEN_LEN; j++) vring.cur_state.obuff[j] = 0;
  for (j=0; j < VOC_TOKEN_LEN/2; j++) vring.cur_state.phase[j] = 0;
  for (j=0; j < VOC_TOKEN_LEN; j++) {
    x = 2*PI*(j - VOC_TOKEN_LEN/2)/(float)VOC_TOKEN_LEN;
    voc_window[j] = (1 + cos(x))/2;
    smooth_window[j] = (1 + cos(x))/2;
  }
  vring.cur_state.file_pos_secs = 0.;
  vring.rate = 1.;
}




void
vcode_init_raw_audio() { 
  int i,j;
  float x;

  rel_freqs = (int) vcode_hz2omega(MAX_FREQS_HZ);
  num_vars = 2*rel_freqs;

  read_48khz_raw_audio();

  vcode_data = (VCODE_ELEM **) malloc(vring.audio_frames*sizeof(VCODE_ELEM *));
  for (i=0; i < vring.audio_frames; i++) 
    vcode_data[i] = (VCODE_ELEM *) malloc(VCODE_FREQS/*rel_freqs*/*sizeof(VCODE_ELEM));
  /*    want to change vcode_data to rel_freqs eventually when we decide on appropriate
        value */



  read_orchestra_times();
  //  if (strcmp(scorename,"mimi") == 0) volume_correct();
  vring.frame = 0;
  for (j=0; j < VOC_TOKEN_LEN; j++) vring.cur_state.obuff[j] = 0;
  for (j=0; j < VOC_TOKEN_LEN/2; j++) vring.cur_state.phase[j] = 0;
  for (j=0; j < VOC_TOKEN_LEN; j++) {
    x = 2*PI*(j - VOC_TOKEN_LEN/2)/(float)VOC_TOKEN_LEN;
    voc_window[j] = (1 + cos(x))/2;
    smooth_window[j] = (1 + cos(x))/2;
  }
  vring.cur_state.file_pos_secs = 0.;
  vring.rate = 1.;

}



static void
complex2polar(float *c, float *mod, float *phase) {
  if (c[0] == 0. && c[1] == 0.) { // domain error
    *phase = *mod = 0;
    return;
  }
  *phase = atan2(c[0],c[1]);
  *mod = sqrt(c[0]*c[0] + c[1]*c[1]);
}

static void
polar2complex(float mod, float phase, float *c) {
  c[0] = mod*sin(phase);
  c[1] = mod*cos(phase);
}



static float
interp_audio(unsigned char *base, float samp) {
  int lo,hi;
  float x,p,q;

  lo = (int) samp;
  hi = lo+1;
  p = samp-lo;
  q = 1-p;
  x = q*sample2float(base + lo*BYTES_PER_SAMPLE) + p*sample2float(base + hi*BYTES_PER_SAMPLE);
  return(x);
}




static void
cut_out_chiff(int i, VCODE_ELEM *v) {
  int p1,p2,temp,olo,ohi,h,k;
  float lohz,hihz;

  /*    p1 = 70;
  lohz = orch_pitch*pow(2., (p1-69)/12.);
  olo = (int) (.5 + VOC_TOKEN_LEN*lohz/(float) NOMINAL_OUT_SR);
   v[olo].modulus =  .01;
   printf("i = %d\n",i);*/



  if (score.solo.note[i].snd_notes.num == 0) return;
  if (score.solo.note[i+1].snd_notes.num == 0) return;
  p1 = score.solo.note[i].snd_notes.snd_nums[0];
  p2 = score.solo.note[i+1].snd_notes.snd_nums[0];
  printf("%d -- %d\n",p1,p2);
  if (p1 > p2) { temp = p1; p1 = p2; p2 = temp; }
  lohz = orch_pitch*pow(2., (p1-69)/12.);
  hihz = orch_pitch*pow(2., (p2-69)/12.);
  //  printf("%f -- %f\n",lohz,hihz);
  for (h=1; ; h++) {
    olo = (int) (.5 + VOC_TOKEN_LEN*h*lohz/(float) NOMINAL_OUT_SR);
    ohi = (int) (.5 + VOC_TOKEN_LEN*h*hihz/(float) NOMINAL_OUT_SR);
    if (ohi >= VOC_TOKEN_LEN/2) break;
    for (k= olo; k <= ohi; k++) v[k].modulus =  0;
  }
}

static void
mark_out_gliss(int i, int *mark) {
  int p1,p2,temp,olo,ohi,h,k;
  float lohz,hihz;

  /*    p1 = 70;
  lohz = orch_pitch*pow(2., (p1-69)/12.);
  olo = (int) (.5 + VOC_TOKEN_LEN*lohz/(float) NOMINAL_OUT_SR);
   v[olo].modulus =  .01;
   printf("i = %d\n",i);*/



  if (score.solo.note[i].snd_notes.num == 0) return;
  if (score.solo.note[i+1].snd_notes.num == 0) return;
  p1 = score.solo.note[i].snd_notes.snd_nums[0];
  p2 = score.solo.note[i+1].snd_notes.snd_nums[0];
  if (p1 > p2) { temp = p1; p1 = p2; p2 = temp; }
  lohz = orch_pitch*pow(2., (p1-69)/12.);
  olo = (int) (.5 + VOC_TOKEN_LEN*lohz/(float) NOMINAL_OUT_SR);
  for (k= olo; k < VCODE_FREQS; k++) mark[k] = 1;
}




static float
vcode_midi2omega(float midi) {
  float hz;

  hz = 440.*pow(2., (midi-69)/12.);
  return(VOC_TOKEN_LEN*hz/(float) NOMINAL_OUT_SR);
}
 


#define HOW_CLOSE_IS_CLOSE .2

static int
mark_transient_freqs(float secs, int *mark) {  /* n is first note index */
  SOUND_NUMBERS chrd;
  int i,j,low_midi,lo;

  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].snd_notes.num == 0) continue;
    if (fabs(score.solo.note[i].orchestra_secs - secs) < HOW_CLOSE_IS_CLOSE) break;
  }
  if (i == score.solo.num) return(0);

  low_midi = 1000;  /* impossibly high midi pitch */
  for (; i < score.solo.num; i++) {
    if (fabs(score.solo.note[i].orchestra_secs - secs) > HOW_CLOSE_IS_CLOSE) break;
    chrd = score.solo.note[i].snd_notes;
    for (j=0; j < chrd.num; j++) if (chrd.snd_nums[j] < low_midi) low_midi = chrd.snd_nums[j];
  }
  lo = (int) vcode_midi2omega(low_midi-.5) + .5;
  for (i=lo; i < rel_freqs; i++) mark[i] = 1;
  return(rel_freqs-1-lo);
}




#define FILTER_EXTENT 5
#define FILTER_HEIGHT 1
#define TRANS_WID 2
#define TRANS_NUM 4

static void
filter_out_transients(VCODE_ELEM **v) {  
  SOUND_NUMBERS chrd;
  int i,j,low_midi,lo,f,k,l,n,m;
  float t[100],**temp;
  float **filter,c,x,**matrix();

  
  c = 2*(FILTER_EXTENT-TRANS_WID)/ (float) (2*TRANS_WID+1);
  filter = matrix(-FILTER_EXTENT, FILTER_EXTENT,-FILTER_HEIGHT, FILTER_HEIGHT);
  temp = matrix(-TRANS_NUM, TRANS_NUM,0, rel_freqs);
  for (l=-FILTER_HEIGHT; l <= FILTER_HEIGHT; l++) {
    for (i=-FILTER_EXTENT; i <= FILTER_EXTENT; i++) filter[i][l] = 1;
    for (i=-TRANS_WID; i <= TRANS_WID; i++) filter[i][l] = -c;
  }
  filter[0][0] += 1;

  /*  for (l=-FILTER_HEIGHT; l <= FILTER_HEIGHT; l++) 
    for (j=-FILTER_EXTENT; j <= FILTER_EXTENT; j++) 
      filter[j][l] = 1/(float) ((2*FILTER_HEIGHT+1)*(2*FILTER_EXTENT+1));
  
    
  x = 0;
  for (l=-FILTER_HEIGHT; l <= FILTER_HEIGHT; l++) {
    for (j=-FILTER_EXTENT; j <= FILTER_EXTENT; j++) {
      printf("%3.1f\t",filter[j][l]);
      x += filter[j][l];
    }
    printf("\n");
    }*/
  //  printf("x = %f\n",x); exit(0);



  for (i=0; i < 20 /*score.solo.num*/; i++) {
    printf("i = %d\n",i);
    if (score.solo.note[i].snd_notes.num == 0) continue;
    f = vcode_secs2frames(score.solo.note[i].orchestra_secs) + .5;
    if (f < 10) continue;
    lo = (int) (vcode_midi2omega(score.solo.note[i].snd_notes.snd_nums[0]-.5) + .5);
    for (k=lo; k < rel_freqs-5; k++) {
      for (j=-TRANS_NUM; j <= TRANS_NUM; j++) {
	temp[j][k] = 0;
	for (n=-FILTER_EXTENT; n <= FILTER_EXTENT; n++) 
	  for (m=-FILTER_HEIGHT; m <= FILTER_HEIGHT; m++) 
	    temp[j][k]  += filter[n][m]*v[f+j+n][k+m].modulus;
	  temp[j][k] = max(temp[j][k],0);
      }
    }
    for (k=lo; k < rel_freqs-5; k++)  for (j=-TRANS_NUM; j <= TRANS_NUM; j++) 
      v[f+j][k].modulus = temp[j][k];
  }
}

static void
clip_highs(VCODE_ELEM *v) {
  int p1,p2,temp,olo,ohi,h,k,i;
  float lohz,hihz;

  //  p2 = VOC_TOKEN_LEN/2; /* high Bb */
  //  p2 = 82+13; /* high Bb */
  p1 = 65; /* octave f */
  lohz = orch_pitch*pow(2., (p1-69)/12.);
  hihz = orch_pitch*pow(2., (p2-69)/12.);
  olo = (int) (.5 + VOC_TOKEN_LEN*lohz/(float) NOMINAL_OUT_SR);
  ohi = (int) (.5 + VOC_TOKEN_LEN*hihz/(float) NOMINAL_OUT_SR);
  for (i=olo; i <  VOC_TOKEN_LEN/2; i++) v[i].modulus =  0;
}






#define ERASE_BEFORE_ONSET .1
#define INCLUDE_RANGE   1.25 // .5
#define PITCH_BAND .25 //.25 /* in semitones */
#define MIN_BAND_HZ 20.
#define SOLOIST_TUNING 442.


static int
mark_solo_cut(float secs, int *mark) {
  int i,j,p,o,k,h,lo_index,hi_index,olo,ohi,n,closei=0,b,count,change,marked;
  float hz,omega,hihz,lohz,mx,pshift;
  static int lasti;
  char chord[100];


  for (i=0; i < VCODE_FREQS; i++)  mark[i] = 0;

  /*   marked = mark_transient_freqs(secs, mark);
       if (marked) return(marked);*/


  /*  for (j=0; i < score.midi.num-1; j++) 
      if (score.midi.burst[j+1].orchestra_secs > secs) break;*/

  for (i=0; i < score.solo.num-1; i++) {
    if (score.solo.note[i].orchestra_secs == 0) continue;
    //    if (fabs(score.solo.note[i+1].orchestra_secs - secs) < .1) closei = i;
    if (score.solo.note[i+1].orchestra_secs > secs + ERASE_BEFORE_ONSET) break;
  }
  //  if (closei) mark_out_gliss(closei,mark);
  //  clip_highs(v);

  hi_index = i;
  while ((score.solo.note[i].orchestra_secs != 0.) && 
	 (fabs(score.solo.note[i].orchestra_secs-secs) < INCLUDE_RANGE) && 
	 (i > 0)) i--;
  lo_index = i;
  
  //  printf("hi_index = %d lo_index = %d %s %s\n",hi_index,lo_index,score.solo.note[lo_index].observable_tag,score.solo.note[lo_index-1].observable_tag); exit(0); 
  for (i = lo_index; i <= hi_index; i++) {
    for (n=0; n < score.solo.note[i].snd_notes.num; n++) {
      p = score.solo.note[i].snd_notes.snd_nums[n];
      hihz = orch_pitch*pow(2., (p+PITCH_BAND-69)/12.);
      lohz = orch_pitch*pow(2., (p-PITCH_BAND-69)/12.);
      /*      if (hi_index != lasti) {
	printf("pitch = %f\n",hz); 
	printf("%f %f\n",score.solo.note[i].orchestra_secs,secs);
	sndnums2string(score.solo.note[i].snd_notes,chord);
	printf("solo chord is %s\n",chord);
	sndnums2string(score.midi.burst[j].snd_notes,chord);
	printf("accomp chord is %s\n",chord);
      }
      lasti = hi_index;*/
      for (h=1; ; h++) {
	omega = VOC_TOKEN_LEN*h*hz/(float) NOMINAL_OUT_SR;
	o = omega + .5;
	olo = (int) (.5 + VOC_TOKEN_LEN*(h*lohz-MIN_BAND_HZ)/(float) NOMINAL_OUT_SR);
	ohi = (int) (.5 + VOC_TOKEN_LEN*(h*hihz+MIN_BAND_HZ)/(float) NOMINAL_OUT_SR);

	if (ohi >= VOC_TOKEN_LEN/2) break;
	for (k= olo; k <= ohi; k++) mark[k] = 1;
      }
      //      printf("pitch = %f o = %d\n",hz,o); 
      //      for (k= o-10; k < o+10; k++) printf("%d %f\n",k,v[k].modulus);
    }
  }
  for (i=count=0; i < rel_freqs; i++) count += mark[i]; 
  return(count);
}


static void  
mark_harm(float om, int *mark, VCODE_ELEM *v) {
  int i,o,maxi;
  float m = 0;

  o = (int) om+.5;
  for (i=o-2; i <= o+2; i++) if (v[i].modulus > m) { m = v[i].modulus; maxi = i; }
  
  //  printf("maxi = %d\n",maxi);
  for (i=maxi+1; i < rel_freqs; i++) {
    //    printf("%f %f\n",v[i].modulus, v[i-1].modulus);
    if (v[i].modulus < v[i-1].modulus) mark[i-1] = 1;
    else break;
  }
  //  printf("\n");
  for (i=maxi-1; i > 0; i--) {
    //    printf("%f %f\n",v[i].modulus, v[i+1].modulus);
    if (v[i].modulus < v[i+1].modulus) mark[i+1] = 1;
    else break;
  }
  //  exit(0);
}


static int
mark_solo_harms(float secs, int *mark, VCODE_ELEM *v) {
  int i,j,p,o,k,h,lo_index,hi_index,olo,ohi,n,closei=0,b,count,change,marked;
  float hz,omega,hihz,lohz,mx;
  static int lasti;
  char chord[100];


  for (i=0; i < VCODE_FREQS; i++)  mark[i] = 0;

  for (i=0; i < score.solo.num-1; i++) {
    if (score.solo.note[i+1].orchestra_secs > secs + ERASE_BEFORE_ONSET) break;
  }

  hi_index = i;
  while (fabs(score.solo.note[i].orchestra_secs-secs) < INCLUDE_RANGE && i > 0) i--;
  lo_index = i;
  
  for (i = lo_index; i <= hi_index; i++) {
    for (n=0; n < score.solo.note[i].snd_notes.num; n++) {
      p = score.solo.note[i].snd_notes.snd_nums[n];
      omega = vcode_midi2omega(p);
      for (h=1; ; h++) {
	if (h*omega >= rel_freqs) break;
	mark_harm(h*omega,mark,v);
      }
    }
  }
  for (i=count=0; i < rel_freqs; i++) count += mark[i]; 
  return(count);
}



#define MAX_HARMS 15

static int
has_harmonic_in_range(int midi, int lo, int hi, int *mark,  int *harm, int *visible) {  
  /* lo and hi are FFT indices  *harm is harmonic number *visible is number of visible harmonic*/
  float lolim,hilim,hlo,hhi;
  int h,a,b,j;
  
  *visible = 0;
  lolim = vcode_midi2omega(midi-.25);
  hilim = vcode_midi2omega(midi+.25);
  for (h=1; h < MAX_HARMS; h++) {
    hlo = h*lolim;
    hhi = h*hilim;
    //    printf("hlo = %f hhi = %f lo = %d hi = %d\n",hlo,hhi,lo,hi);
    if (hlo >  hi) return(0);  /* no overlap with any harmonic */
    if (hhi <  lo) continue;   /* no overlap with this harmonic */
    break;
  }
  if (h == MAX_HARMS) return(0);
  *harm = h;
  for (h=1; h < MAX_HARMS; h++) {
    a = (int) (h*lolim + .5);
    b = (int) (h*hilim + .5);
    for (j=a; j <= b; j++) if (mark[j]) break;
    if (j < b) continue;  /* harmonic h not completely visible */
    if (*visible == 0) *visible = h;
    if (abs(h- *harm) < abs(*visible - *harm)) *visible = h;  /* looking for closest visible harm */
  }
  if (*visible == 0) return(0);
  return(1);
}


static int
in_chord(int midi, SOUND_NUMBERS *notes) {
  int i;

  for (i=0; i < notes->num; i++) if (midi == notes->snd_nums[i]) return(1);
  return(0);
}

static void
find_orch_notes(float secs, SOUND_NUMBERS *notes) {
  int j,i,c,m;

  notes->num = 0;
  for (j=0; j < score.midi.num-1; j++) 
    if (score.midi.burst[j+1].orchestra_secs > secs) break;
  for (i=0; i < score.midi.burst[j].snd_notes.num; i++) { 
    m =  score.midi.burst[j].snd_notes.snd_nums[i];
    notes->snd_nums[notes->num++] = m;
  }
  //  printf("j = %d num = %d\n",j,notes->num);
  for (j--; j > 0; j--) {
    if (score.midi.burst[j+1].orchestra_secs == 0.) continue;  /* note unset */
    if ((secs - score.midi.burst[j+1].orchestra_secs) > INCLUDE_RANGE) break;
    for (i=0; i < score.midi.burst[j].snd_notes.num; i++) {
      m =  score.midi.burst[j].snd_notes.snd_nums[i];
      if (in_chord(m,notes)==0) notes->snd_nums[notes->num++] = m;
    }
  }
}


static void
fill_in_gaps(float secs, int *mark, VCODE_ELEM *v) {
  int i,j,ii,hi,lo=0,k,m,harm,visible,l;
  char chord[100];
  static int lastj;
  SOUND_NUMBERS notes;


  find_orch_notes(secs,&notes);
  for (j=0; j < score.midi.num-1; j++) 
    if (score.midi.burst[j+1].orchestra_secs > secs) break;
  //  sndnums2string(score.midi.burst[j].snd_notes,chord);
  if (j != lastj) {
    sndnums2string(notes,chord);
    printf("secs = %f, osecs = %f j = %d chord = %s\n",secs,score.midi.burst[j+1].orchestra_secs,j,chord);
    sndnums2string(score.midi.burst[j].snd_notes,chord);
    printf("cur chord is = %s\n",chord);
  }
  lastj = j;


  for (i=0; i < VCODE_FREQS; i++) if (mark[i]) {
    lo = i;
    for (i++; i < VCODE_FREQS; i++)  if (mark[i]==0) break;
    hi = i-1;
    for (k=0; k < notes/*score.midi.burst[j].snd_notes*/.num; k++) {
      m = notes/*score.midi.burst[j].snd_notes*/.snd_nums[k];
      if (has_harmonic_in_range(m,  lo,  hi, mark,  &harm, &visible))
	for (l=lo; l <= hi; l++) {
	  v[l].modulus = 0; //v[l*visible/harm].modulus/1;
	}
	//      	printf("%d -- %d has harmonic %d of %d (%f) with visible %d\n",lo,hi,harm,m,vcode_midi2omega((float)m),visible);
	    //            else printf("%d -- %d has no visible harmonic from %d\n",lo,hi,m);
    }
  }
}


static int
index_comp (int *a, int *b) {
  return( (*a) - (*b));
}


static void
get_relevant_freqs(float secs, INDEX_LIST *solo, INDEX_LIST *orch, int *mark) {
  int i,j,h,a,b,k,m,smark[VCODE_FREQS],omark[VCODE_FREQS];
  SOUND_NUMBERS notes;
  float lolim,hilim;


  for (i=0; i < VCODE_FREQS; i++) smark[i] = omark[i] = 0;
  solo->num = orch->num = 0;
  find_orch_notes(secs,&notes);
  for (k=0; k < notes.num; k++) {
    m = notes.snd_nums[k];
    //    lolim = vcode_midi2omega(m-.25);
    //    hilim = vcode_midi2omega(m+.25);
    lolim = vcode_midi2omega(m-.5);
    hilim = vcode_midi2omega(m+.5);
    for (h=1; h < MAX_HARMS; h++) {
      a = (int) (h*lolim + .5);
      b = (int) (h*hilim + .5);
      b = min(b,(rel_freqs-1));
      for (j=a; j <= b; j++) {
	if (mark[j]) smark[j] = 1;
	else  omark[j] = 1;
      }
    }
  }
  for (i=0; i < rel_freqs; i++) {
    //    if (smark[i]) solo->index[solo->num++] = i;
    if (mark[i]) solo->index[solo->num++] = i;  /* need to think about this */
    if (omark[i]) orch->index[orch->num++] = i;
  }
}




static void  /* assumes no duplicatiosn */
merge_index_lists(INDEX_LIST *l1, INDEX_LIST *l2, INDEX_LIST *l) {
  int i,i1,i2;
  
  l->num = l1->num + l2->num;
  for (i = i1 = i2 =0; i < l->num; i++) {
    if (i2 == l2->num) l->index[i] = l1->index[i1++];
    else if (i1 == l1->num) l->index[i] = l2->index[i2++];
    else if (l1->index[i1] < l2->index[i2])  l->index[i] = l1->index[i1++];
    else l->index[i] = l2->index[i2++];
  }
} 



static void
print_index_list(INDEX_LIST *l) {
  int i;

  printf("list has %d els\n",l->num);
  for (i=0; i < l->num; i++) printf("%d\n",l->index[i]);
  printf("\n");
}

static void
copy_index_list(INDEX_LIST *ls, INDEX_LIST *ld) {
  int i;

  ld->num = ls->num;
  for (i=0; i < ls->num; i++) ld->index[i] = ls->index[i];
}

static int
index_lists_same(INDEX_LIST *l1, INDEX_LIST *l2) {
  int i;

  if (l1->num != l2->num) return(0);
  for (i=0; i < l1->num; i++) if (l1->index[i] != l2->index[i]) return(0);
  return(1);
}

static int
new_situation(INDEX_LIST *a1, INDEX_LIST *b1, INDEX_LIST *a2, INDEX_LIST *b2, 
	      INDEX_LIST *a3, INDEX_LIST *b3) {

  if (index_lists_same(a1,b1) == 0) return(1);
  if (index_lists_same(a2,b2) == 0) return(1);
  if (index_lists_same(a3,b3) == 0) return(1);
  return(0);
}
 
static int
new_situation2(INDEX_LIST *a1, INDEX_LIST *b1, INDEX_LIST *a2, INDEX_LIST *b2) { 

  if (index_lists_same(a1,b1) == 0) return(1);
  if (index_lists_same(a2,b2) == 0) return(1);
  return(0);
}
 
static int
omega2vardex(int omega) {
  return(omega);
}

static int
lastomega2vardex(int omega) {
  return(rel_freqs + omega);
}

static void
vardex2maskdex(int var, int *n, int *o) {
  
  *o = var%rel_freqs;
  *n = var/rel_freqs;
}




static void
get_relevant_vars(int f, INDEX_LIST *past, INDEX_LIST *solo, INDEX_LIST *orch, int **mark) {
  int i,j,h,a,b,k,m,smark[VCODE_FREQS],omark[VCODE_FREQS];
  SOUND_NUMBERS notes;
  float lolim,hilim,secs;
  char chord[100];


  secs = vcode_frames2secs((float) f);
  for (i=0; i < VCODE_FREQS; i++) smark[i] = omark[i] = 0;
  past->num = solo->num = orch->num = 0;
  find_orch_notes(secs,&notes);
  sndnums2string(notes,chord);
  //  printf("orch chord is %s\n",chord);
  for (k=0; k < notes.num; k++) {
    m = notes.snd_nums[k];
    lolim = vcode_midi2omega(m-.25);
    hilim = vcode_midi2omega(m+.25);
    for (h=1; h < MAX_HARMS; h++) {
      a = (int) (h*lolim + .5);
      b = (int) (h*hilim + .5);
      b = min(b,(rel_freqs-1));
      for (j=a; j <= b; j++) {
	if (mark[f][j]) smark[j] = 1;
	else  omark[j] = 1;
      }
    }
  }
  for (i=0; i < rel_freqs; i++) {
    if (omark[i])  orch->index[orch->num++] = omega2vardex(i);
  }
  for (i=0; i < rel_freqs; i++) {
    if (mark[f][i]) solo->index[solo->num++] = omega2vardex(i);
    //    if ((f > 0) && mark[f-1][i]) past->index[past->num++] = lastomega2vardex(i);
  }
  /*  for (i=0; i < rel_freqs; i++)  printf("%d",omark[i]);
  printf("\n");
  for (i=0; i < rel_freqs; i++)  printf("%d",smark[i]);
  printf("\n");
  for (i=0; i < rel_freqs; i++)  printf("%d",mark[f][i]);
  printf("\n");*/
  //  print_index_list(orch);
}





static float
interp_freq(float omega, VCODE_ELEM *v) {
  float p,q;
  int a,b;

  a = (int) omega;
  b = a+1;
  q = omega - a;
  p = 1-q;
  return(p*v[a].modulus + q*v[b].modulus);
}


static void
find_harmonic_data(FILE *fp, float secs, int *mark, VCODE_ELEM *v) {
  int i,j,h,ii,hi,lo=0,k,m,harm,visible,l,d;
  char chord[100];
  static int lastj;
  SOUND_NUMBERS notes;
  float f,phase,mod,c[2];
  static float p[4] = {0,0,0,0};

  for (j=0; j < score.midi.num-1; j++) 
    if (score.midi.burst[j+1].orchestra_secs > secs) break;
  sndnums2string(score.midi.burst[j].snd_notes,chord);

    m = 46;
    f = vcode_midi2omega((float)m);

  /*  for (k=0; k < score.midi.burst[j].snd_notes.num; k++) {
    m = score.midi.burst[j].snd_notes.snd_nums[k];
    //    if ((m%12) != 2) continue;
    //        if (m != 62) continue;

  */

    /*        phase = v[(int)f].phase;
    for (h=2; h >= 0; h--) p[h+1] = p[h];
    p[0] = phase;
    fprintf(fp,"%f %f %f %f\n",p[0],p[1],p[2],p[3]);
    return;*/


    /*    fprintf(fp,"%f %f %f %f\n",v[(int)f-1].phase,v[(int)f].phase,v[(int)f+1].phase,v[(int)f+2].phase);
	  return;*/


    for (h=1; h < 5; h++) {
      d = (int) (h*f);
      if (mark[d] || mark[d+1]) fprintf(fp,"NA      \t");
      else {
	phase = v[(int)(h*f)].phase;
	mod = v[(int)(h*f)].modulus;
	//	fprintf(fp,"%f\t",(mod < .01) ? 0 : mod);
	fprintf(fp,"%f\t",mod);
	//	polar2complex(mod, phase, c);
	//	fprintf(fp,"%f\t",phase);
	//	fprintf(fp,"%f\t%f\t",c[0],c[1]);
      }
      
      /*    }*/
    }
    fprintf(fp,"\n");
}



static void
cut_out_solo(float secs, VCODE_ELEM *v) {
  int i,j,p,o,k,h,lo_index,hi_index,olo,ohi,n,closei=0,b,mark[VCODE_FREQS];
  float hz,omega,hihz,lohz,mx;
  static int lasti;
  char chord[100];


  mark_solo_cut(secs,mark);
  //  j = 0;
  //  for (i=0; i < VCODE_FREQS; i++) if (mark[i]) { printf("%f ",v[i].phase); if ((j++) == 5) return;}
  for (i=0; i < VCODE_FREQS; i++) if (mark[i]) v[i].modulus = 0;
  fill_in_gaps(secs, mark, v);
  

  return;
  for (i=0; i < score.solo.num-1; i++) {
    if (fabs(score.solo.note[i+1].orchestra_secs - secs) < .1) closei = i;
    if (score.solo.note[i+1].orchestra_secs > secs + INCLUDE_RANGE) break;
  }
  //  if (closei) cut_out_chiff(closei,v);
  //  clip_highs(v);

  hi_index = i;
  while (fabs(score.solo.note[i].orchestra_secs-secs) < INCLUDE_RANGE && i > 0) i--;
  lo_index = i;
  
  for (i = lo_index; i <= hi_index; i++) {
    for (n=0; n < score.solo.note[i].snd_notes.num; n++) {
      p = score.solo.note[i].snd_notes.snd_nums[n];
      hihz = orch_pitch*pow(2., (p+PITCH_BAND-69)/12.);
      lohz = orch_pitch*pow(2., (p-PITCH_BAND-69)/12.);
      /*      if (hi_index != lasti) {
	printf("pitch = %f\n",hz); 
	printf("%f %f\n",score.solo.note[i].orchestra_secs,secs);
	sndnums2string(score.solo.note[i].snd_notes,chord);
	printf("solo chord is %s\n",chord);
	sndnums2string(score.midi.burst[j].snd_notes,chord);
	printf("accomp chord is %s\n",chord);
      }
      lasti = hi_index;*/
      for (h=1; ; h++) {
	omega = VOC_TOKEN_LEN*h*hz/(float) NOMINAL_OUT_SR;
	o = omega + .5;
	olo = (int) (.5 + VOC_TOKEN_LEN*h*lohz/(float) NOMINAL_OUT_SR);
	ohi = (int) (.5 + VOC_TOKEN_LEN*h*hihz/(float) NOMINAL_OUT_SR);

	if (ohi >= VOC_TOKEN_LEN/2) break;
	for (k= olo; k <= ohi; k++) v[k].modulus = 0;
      }
      //      printf("pitch = %f o = %d\n",hz,o); 
      //      for (k= o-10; k < o+10; k++) printf("%d %f\n",k,v[k].modulus);
    }
  }
}





static void
old_vcode_interp(float f, VCODE_ELEM *v) { /* f is pos in VCODE frames */
  float sound1[VOC_TOKEN_LEN],sound2[VOC_TOKEN_LEN];
  float mod1,phi1,mod2,phi2,p,q,prate,a,b,pp,qq,div_const;
  unsigned char *orig,*temp,*center;
  int j,lo,hi;


  /* this was the old way of doing things that didn't try to mimick the strange
     relationship between frame number of audio chunk that is exhibited in 
     audio.c (and in rhythm) in samples2data.  When using the old way the
     estimated times from rhythm parsing don't match up with times of events
     in spectrogram  */


  div_const = sqrt((float) VOC_TOKEN_LEN);
  lo = (int) f;
  hi = lo + 1;
  q = f - lo;
  p = 1-q;
  prate = ((float) orch_pitch) / 440.;
  //  orig = temp = vring.audio +  (int) (f*VOC_TOKEN_LEN*BYTES_PER_SAMPLE/HOP);
  orig = temp = vring.audio +  (int) (lo*VOC_TOKEN_LEN*BYTES_PER_SAMPLE/HOP);




  //  ((lo+1)*SAMPLES_PER_FRAME - VOC_TOKEN_LEN) * BYTES_PER_SAMPLE;
  /* mimicking of strange convention in samples2data of audio.c */



  /*  for (j=0; j < VOC_TOKEN_LEN; j++, temp+= BYTES_PER_SAMPLE) 
      sound1[j] = voc_window[j]*sample2float(temp);*/
  

  for (j=0; j < VOC_TOKEN_LEN; j++) 
    //    sound1[j] = smooth_window[j] * (temp < vring.audio) ? 0 : interp_audio(temp,j*prate);
    sound1[j] = smooth_window[j] * interp_audio(temp,j*prate);
  //    sound1[j] = voc_window[j] * interp_audio(temp,j*prate);

  realft(sound1-1,VOC_TOKEN_LEN/2,1);

  //  temp = orig + (int) (prate*VOC_TOKEN_LEN*BYTES_PER_SAMPLE/HOP);

      temp = orig + ((int) (prate*VOC_TOKEN_LEN/HOP)) *BYTES_PER_SAMPLE;

  /*  for (j=0; j < VOC_TOKEN_LEN; j++, temp+= BYTES_PER_SAMPLE) 
      sound2[j] = voc_window[j]*sample2float(temp);*/

  for (j=0; j < VOC_TOKEN_LEN; j++) 
    sound2[j] = smooth_window[j] * interp_audio(temp,j*prate);
  //    sound2[j] = voc_window[j] * interp_audio(temp,j*prate);

  realft(sound2-1,VOC_TOKEN_LEN/2,1);

  for (j=0; j < VOC_TOKEN_LEN/2; j++) {
    complex2polar(sound1+2*j,&mod1,&phi1);  /* maybe very minor error due to way freqs are stored */
    complex2polar(sound2+2*j,&mod2,&phi2);
    v[j].modulus = (p*mod1 + q*mod2)/ /*VOC_TOKEN_LEN*/ div_const;
    //    v[j].modulus =  log(1 + v[j].modulus); 
    v[j].phase = phi2-phi1;
    while (v[j].phase < -PI) v[j].phase += 2*PI;
    while (v[j].phase >  PI) v[j].phase -= 2*PI;
    //    if (j == 100) printf("%d %f\n",vring.frame,v[j].phase);
  }
}


static void
vcode_interp(float f, VCODE_ELEM *v) { /* f is pos in VCODE frames */
  float sound1[VOC_TOKEN_LEN],sound2[VOC_TOKEN_LEN];
  float mod1,phi1,mod2,phi2,p,q,prate,a,b,pp,qq,div_const;
  unsigned char *orig,*temp,*center;
  int j,lo,hi;

  //  printf("vcode_interp at %f\n",now()); 


  div_const = sqrt((float) VOC_TOKEN_LEN);
  lo = (int) f;
  hi = lo + 1;
  q = f - lo;
  p = 1-q;
  prate = ((float) orch_pitch) / 440.;
  orig = temp = vring.audio +  (int) 
    (((lo+1)*SAMPLES_PER_FRAME - VOC_TOKEN_LEN) * BYTES_PER_SAMPLE);
  /* mimicking of strange convention in samples2data of audio.c */

  for (j=0; j < VOC_TOKEN_LEN; j++) 
    sound1[j] = smooth_window[j] * ((temp < vring.audio) ? 0 : interp_audio(temp,j*prate));
  realft(sound1-1,VOC_TOKEN_LEN/2,1);
  temp = orig + ((int) (prate*VOC_TOKEN_LEN/HOP)) *BYTES_PER_SAMPLE;

  for (j=0; j < VOC_TOKEN_LEN; j++) 
    sound2[j] = smooth_window[j] * ((temp < vring.audio) ? 0 : interp_audio(temp,j*prate));
  realft(sound2-1,VOC_TOKEN_LEN/2,1);

  for (j=0; j < VOC_TOKEN_LEN/2; j++) {
    complex2polar(sound1+2*j,&mod1,&phi1);  /* maybe very minor error due to way freqs are stored */
    complex2polar(sound2+2*j,&mod2,&phi2);
    v[j].modulus = (p*mod1 + q*mod2)/ /*VOC_TOKEN_LEN*/ div_const;
    v[j].phase = phi2-phi1;
    //    if (j == 20) printf("mod1 = %f mod2 = %f p = %f q = %f ans = %f\n",mod1,mod2,p,q,v[j].modulus );
    while (v[j].phase < -PI) v[j].phase += 2*PI;
    while (v[j].phase >  PI) v[j].phase -= 2*PI;
  }
}


vcode_fast_interp(float f, VCODE_ELEM *v) { /* f is pos in VCODE frames */
  float sound1[VOC_TOKEN_LEN],sound2[VOC_TOKEN_LEN];
  float mod1,phi1,mod2,phi2,p,q,prate,a,b,pp,qq,t1,t2;
  unsigned char *orig,*temp,*center;
  int j,lo,hi,bt;
  

#ifdef  REAL_TIME_READ_VOC
  VCODE_ELEM voc[VCODE_FREQS];
  t1 = now();
  bt = read(vcode_fd, v, VCODE_FREQS*sizeof(VCODE_ELEM));
  t2 = now();
  if (bt < 16384) printf("read %d\n",bt);
if ((t2-t1) > .0001) printf("waited %f\n",t2-t1); 
#endif


/*  t1 = now();
fread(v,rel_freqs,sizeof(VCODE_ELEM),vcode_fp);  
t2 = now();


if ((t2-t1) > .01) printf("waited %f\n",t2-t1); */

  //  printf("fast rate = %f\n",vring.rate);
  lo = (int) f;
  hi = lo + 1;
  q = f - lo;
  p = 1-q;
  for (j=0; j < VCODE_FREQS; j++) v[j].modulus = v[j].phase = 0;
  for (j=0; j < rel_freqs; j++) {
    v[j].modulus = (p*vcode_data[lo][j].modulus + q*vcode_data[hi][j].modulus); /// VOC_TOKEN_LEN;
    v[j].phase = (p > q) ? vcode_data[lo][j].phase : vcode_data[hi][j].phase;
  }
  //  printf("phase = %f\n",v[30].phase);
}



static void
get_hires_solo_audio_frame(unsigned char *obuff) {
  int b,i,j,k,hi_samps,lo_samps,l=0,m,n,base;

  hi_samps = VOC_TOKEN_LEN/HOP;
  b = hi_samps*BYTES_PER_SAMPLE;
  j = vring.frame % HIRES_FRAMES;
  memcpy(obuff, hires_solo_buff + j*b, b);
  fread(hires_solo_buff +  j*b , b, 1, hires_fp);
}


static void
get_upsampled_solo_audio_frame(unsigned char *obuff) {
  int i,j,k,hi_samps,lo_samps,l=0,m,n,base;
  unsigned char *ptr;
  float x1,x2,p,q,x,r,rep;


  rep =  NOMINAL_OUT_SR/(float)NOMINAL_SR;  
  hi_samps = VOC_TOKEN_LEN/HOP;
  ptr = audiodata;
  base = vring.frame*hi_samps;
  for (i=0,l=0; i < hi_samps; i++,l+=BYTES_PER_SAMPLE) {
    r = (base+i)/rep;
    m = (int) r;
    n = m+1;
    q = r-m;
    p = 1-q;
    x1 = sample2float(&ptr[m*BYTES_PER_SAMPLE]);
    x2 = sample2float(&ptr[n*BYTES_PER_SAMPLE]);
    float2sample(p*x1 + q*x2,&obuff[l]);
  }
}

static int orchestra_s=0;   /* high rate sample number */
static long int orchestra_acc=0;

static void
save_orch_data(unsigned char *out, int n) {
  int i,j,rep,d;
  //  static int s=0;   /* high rate sample number */
  //  static long int acc=0;
  
  /*  return;
  printf("audiodata = %x orchdata = %x diff = %d\n",audiodata,orchdata,orchdata-audiodata);
  exit(0); */

  rep =  NOMINAL_OUT_SR/NOMINAL_SR;  
  if (NOMINAL_SR*rep != NOMINAL_OUT_SR) { printf("conversion mismatch\n"); exit(0); }
  for (i=0; i < n; i++, orchestra_s++) {
    orchestra_acc += sample2int(out + BYTES_PER_SAMPLE*i);
    if (((orchestra_s+1) % rep) != 0) continue;
    d = (orchestra_s+1) / rep;
    int2sample(orchestra_acc/rep, orchdata+d*BYTES_PER_SAMPLE);
    orchestra_acc = 0;
  }
}

void
interleave_with_null_channel(unsigned char *out, unsigned char *inter, int n) {
  int i,j;

  for (i=0; i < n; i++) {
    for (j=0; j < BYTES_PER_SAMPLE; j++) {
      inter[2*i*BYTES_PER_SAMPLE + j] = out[BYTES_PER_SAMPLE*i + j];
      inter[(2*i+1)*BYTES_PER_SAMPLE + j] = out[BYTES_PER_SAMPLE*i + j];
      /* this channel doesn't seem to produce anaything */
    }
  }
}

void
interleave_mono_channels(unsigned char *left, unsigned char *right, unsigned char *mix, int n) {
  int i,j;
  float a,b,o1,o2;

  for (i=0; i < n; i++) {
    a = sample2float(left + BYTES_PER_SAMPLE*i);
    b = sample2float(right + BYTES_PER_SAMPLE*i);
    mix_channels(a,b, &o1,&o2);
    float2sample(o1,mix + 2*i*BYTES_PER_SAMPLE);
    float2sample(o2,mix + (2*i+1)*BYTES_PER_SAMPLE);
    /*    for (j=0; j < BYTES_PER_SAMPLE; j++) {
      mix[2*i*BYTES_PER_SAMPLE + j] = left[BYTES_PER_SAMPLE*i + j];
      mix[(2*i+1)*BYTES_PER_SAMPLE + j] = right[BYTES_PER_SAMPLE*i + j];
      }*/
  }
}




float  /* this keeps the number of queued samples (delay) much more
	  constant.  I don't know if this helps anythng though */
my_next_frame_time() {
  return(now() + get_delay()/(float)OUTPUT_SR);
}

float
old_next_frame_time() {
  return(vring.frame*VOC_TOKEN_LEN/ ((float) TRUE_OUTPUT_SR * HOP));
}

#ifdef ACCOUNT_FOR_DELAY_EXPERIMENT

//float  /* this keeps the number of queued samples (delay) much more
//	  constant.  I don't know if this helps anythng though */
//next_frame_time() {
//  return(now() + get_delay()/(float)OUTPUT_SR);
//}

float
next_frame_time() {
  return(vring.frame*VOC_TOKEN_LEN/ ((float) TRUE_OUTPUT_SR * HOP));
  // correct this to use measured sr and offset
}






#else
float
next_frame_time() {
  return(vring.frame*VOC_TOKEN_LEN/ ((float) TRUE_OUTPUT_SR * HOP));
}
#endif

int
vcode_samples_queued() {
  return(vring.frame*(VOC_TOKEN_LEN/ HOP));
}

int
vcode_samples_per_frame() {
  return(VOC_TOKEN_LEN/ HOP);
}

#ifdef ACCOUNT_FOR_DELAY_EXPERIMENT
  #define FUDGE_SECS  .1 //.03 
#else
  #define FUDGE_SECS   .02 // .05 //.02
#endif


float
next_callback_seconds() {
  return(next_frame_time() - FUDGE_SECS);
}


static void
save_vcode_state() {
  int i,d;

  d = vring.frame%VCODE_STATE_HISTORY_LEN;
  vring.state_hist[d] = vring.cur_state;
  
}


static void
install_state() {
  int i,d;

  d = vring.frame%VCODE_STATE_HISTORY_LEN;
  vring.cur_state = vring.state_hist[d];
}


static void
which_solo_note(float secs) {
  int i,j;
  RATIONAL r,s;
  static lastj=-1,lasti=-1;


  for (i=0; i < score.solo.num-1; i++) if (score.solo.note[i+1].orchestra_secs > secs) break;
  if (i != lasti) {
        printf("i = %d r = %d/%d\n",i,r.num,r.den);
	for (j=0; j < freqs; j++) printf("%d %f\n",j,score.solo.note[i].spect[j]);
  }
  lasti =i;
  return;
  for (i=0; i < score.midi.num-1; i++) if (score.midi.burst[i+1].orchestra_secs > secs) break;
  if (i == score.solo.num) { printf("couldn't find place\n"); exit(0); }
  r = score.midi.burst[i].wholerat;
  if (i != lasti) {
        printf("i = %d r = %d/%d\n",i,r.num,r.den);
  }
  lasti =i;
  for (j=0; j < score.solo.num-1; j++) {
    s = score.solo.note[j+1].wholerat;
    if (rat_cmp(s,r) > 0) break; 
  }
  if (j != lastj) {
    printf("j = %d\n",j);
    //    printf("r = %d/%d s = %d/%d\n",r.num,r.den,s.num,s.den);
  }
  lastj = j;
}


static void
get_floats(int f, float ff, float *b,int num) {
  float mod,phi,div_const,prate,x;
  int j,offset,samp,marg,jj,blen,n1,n2,bpos,clear;
  unsigned char *temp,*temp2;

  //   samp =  (f+1)*SAMPS_PER_FRAME - VOC_TOKEN_LEN;  // integral frame pos
  samp =  (ff+1)*SAMPS_PER_FRAME - VOC_TOKEN_LEN;  // continuous frame pos
 
   /* can't hear diff between above */

  
#ifdef ONLINE_READ
  blen = ORCH_BUFF_SECS*OUTPUT_SR;
  bpos =  (orch_in.seam + samp -orch_in.sample) % blen;  // pos of sample in circular buffer
  clear = blen-bpos;   // how many avail samples without having to wrap around
  n1 = (clear > num) ? num : clear;   // number taken from end
  n2 = (clear > num) ? 0 : num-n1;    // number of wrap around samps
  samples2floats(orch_in.buff + bpos*BYTES_PER_SAMPLE,b,n1);
  samples2floats(orch_in.buff,b+n1,n2);
#else
  offset = samp*BYTES_PER_SAMPLE;
  //  temp = vring.audio +  (int)     (((f+1)*SAMPLES_PER_FRAME - VOC_TOKEN_LEN) * BYTES_PER_SAMPLE);
   //CF:  vring.audio is pointer to the start of the audio.  temp points to start of this grain 
  /* mimicking of strange convention in samples2data of audio.c */
  temp = vring.audio +  offset;
  samples2floats(temp,b,num);

  //  for (j=0; j < 50; j++) printf("base =%d %d %f\n",temp, j,b[j]);
  //  for (j=0; j < 50; j++) printf("base =%d %d %f\n",temp, j,sample2float(temp+j*BYTES_PER_SAMPLE));
#endif
}


static void
resample_floats(float *b, float *r, int n, int m, float prate) {
  float mod,phi,div_const,x,p,q;
  int i,l,h;

  for (i=0; i < n; i++) {
    x = i*prate;
    l = (int) x;
    q = x-l;
    p = 1-q;
    if (l+1 >= m) { printf("out of bounds in resample_floats()\n"); exit(0); }
    r[i] = p*b[l] + q*b[l+1];
  }
}




static void
complex_spectrum(float *x, VCODE_ELEM *v) {
  float sound[VOC_TOKEN_LEN];
  float mod,phi,div_const,prate;
  int j;

  div_const = (float) VOC_TOKEN_LEN;
  for (j=0; j < VOC_TOKEN_LEN; j++)  sound[j] = smooth_window[j] * x[j];



  realft(sound-1,VOC_TOKEN_LEN/2,1);      //CF:  do the (real) FFT
  v[0].modulus = sound[0] / div_const;  /* discarding highest freq (sound[1] */
  v[0].phase = 0;            //CF:  FFT results format convention: [R0 RN R1 I1 R2 I2 R3 I3 ...].
                             //CF:  0th and Nth componets are always real, so are packed into a space
                             //CF:  that would normally accomodate just one complex number.
                             //CF:  Fix this by overwriting RN by 0, so we treat is as I0.

  for (j=1; j < VOC_TOKEN_LEN/2; j++) {
    complex2polar(sound+2*j,&mod,&phi);     //CF:  store mod and phi in output variables
    v[j].modulus = mod/ div_const;          //CF:  normalize (remove two lots of 1/sqrt(N) gain, so IFFT will be normalized )
    v[j].phase = phi;
    while (v[j].phase < -PI) v[j].phase += 2*PI;
    while (v[j].phase >  PI) v[j].phase -= 2*PI;
  }

}









static void
vcode_polar(int f, VCODE_ELEM *v) { /* f is pos in VCODE frames */
  float sound[VOC_TOKEN_LEN];
  float mod,phi,div_const,prate,x;
  unsigned char *temp,*temp2;
  int j,offset,tsamps,marg,jj,blen;


  //  div_const = sqrt((float) VOC_TOKEN_LEN);
  //  prate = ((float) ORCH_PITCH) / 440.;


  prate = orch_pitch / 440.;             //CF:  resample audio if we want to change the pitch
  div_const = (float) VOC_TOKEN_LEN;
  offset = (int) (((f+1)*SAMPLES_PER_FRAME - VOC_TOKEN_LEN) * BYTES_PER_SAMPLE);
  //  temp = vring.audio +  (int)     (((f+1)*SAMPLES_PER_FRAME - VOC_TOKEN_LEN) * BYTES_PER_SAMPLE);
   //CF:  vring.audio is pointer to the start of the audio.  temp points to start of this grain 
  /* mimicking of strange convention in samples2data of audio.c */
  temp = vring.audio +  offset;


#ifdef ONLINE_READ
  tsamps = offset/BYTES_PER_SAMPLE;
  blen = ORCH_BUFF_SECS*OUTPUT_SR;
  for (j=0; j < VOC_TOKEN_LEN; j++) {
    if (offset < 0)  { sound[j] = 0; continue; }
    jj =  (orch_in.seam + tsamps + j -orch_in.sample);
    if (jj > blen) jj -= blen;
    if (jj == orch_in.seam) { printf("trying to access unavail data\n"); exit(0); }
    x = sample2float(orch_in.buff+BYTES_PER_SAMPLE*jj);
    sound[j] = smooth_window[j] * x;
       //CF:  resampling.  sound[] gets filled with the current resampled, windowed grain.
  }
#else
  for (j=0; j < VOC_TOKEN_LEN; j++) 
    sound[j] = smooth_window[j] * ((offset < 0) ? 0 : interp_audio(temp,j*prate));  
#endif

  //  if (f > 500) for (j=0; j < 100; j++) printf("old f = %d %d %f\n",f,j,sound[j]);


      //	sound[j] = smooth_window[j] * ((temp < vring.audio) ? 0 : interp_audio(temp,j*prate));  

  realft(sound-1,VOC_TOKEN_LEN/2,1);      //CF:  do the (real) FFT
  v[0].modulus = sound[0] / div_const;  /* discarding highest freq (sound[1] */
  v[0].phase = 0;            //CF:  FFT results format convention: [R0 RN R1 I1 R2 I2 R3 I3 ...].
                             //CF:  0th and Nth componets are always real, so are packed into a space
                             //CF:  that would normally accomodate just one complex number.
                             //CF:  Fix this by overwriting RN by 0, so we treat is as I0.

  for (j=1; j < VOC_TOKEN_LEN/2; j++) {
    complex2polar(sound+2*j,&mod,&phi);     //CF:  store mod and phi in output variables
    v[j].modulus = mod/ div_const;          //CF:  normalize (remove two lots of 1/sqrt(N) gain, so IFFT will be normalized )
    v[j].phase = phi;
    while (v[j].phase < -PI) v[j].phase += 2*PI;
    while (v[j].phase >  PI) v[j].phase -= 2*PI;
  }
}




static void
get_polar(int f, VCODE_ELEM *v) { 
  int j;

  for (j=0; j < rel_freqs; j++) v[j] = vcode_data[f][j];
  // for (j=0; j < rel_freqs; j++) v[j].modulus *= 5;  /* temporary */
  for (; j < VCODE_FREQS; j++) v[j].modulus = v[j].phase = 0;
}

#define MMO_TAP_FRONT_SECS .1
#define MMO_TAP_BACK_SECS .4

static int
is_near_mmo_tap(float f) {
  int i;
  float s;

  s = vcode_frames2secs(f);
  for (i=0; i < mmotap.num; i++) {
    if (mmotap.tap[i]-s >  MMO_TAP_FRONT_SECS) continue;
    if (s- mmotap.tap[i] >  MMO_TAP_BACK_SECS) continue;
    return(1);
  }
  return(0);

}


#define MAX_STRETCH_SAMPS  (2*VOC_TOKEN_LEN)

static void
vcode_two_polar(int lo, float f, VCODE_ELEM *v1, VCODE_ELEM *v2) {
  float prate,orig[MAX_STRETCH_SAMPS],resamp[VOC_TOKEN_LEN + SAMPS_PER_FRAME];
  int orig_num,resamp_num,j;

  prate = orch_pitch / 440.;             //CF:  resample audio if we want to change the pitch
  resamp_num = VOC_TOKEN_LEN + SAMPS_PER_FRAME;
  //each frame has VOC_TOKEN_LEN samples but the shift is SAMPLES_PER_FRAME so this is the
  // the number of samples needed for two successive overlapping Fourier transforms
  orig_num = prate *(resamp_num + 1) + 1;  
  // high estim of number of unresampled samples involved in two frame calculations.  
  if (orig_num > MAX_STRETCH_SAMPS) { printf("too many samples here\n");  exit(0); }
  get_floats(lo, f, orig, orig_num);
  resample_floats(orig , resamp, resamp_num , orig_num, prate);
  complex_spectrum(resamp, v1);
  complex_spectrum(resamp + SAMPS_PER_FRAME, v2);
  /*  complex_spectrum(orig, v1);
      complex_spectrum(orig + SAMPS_PER_FRAME); */
}




//CF:  make a new grain
static void
pv_new_grain(float f, float *sound) {
  VCODE_ELEM new[VOC_TOKEN_LEN/2+1];
  int i,j,w;
  int lo,hi;
  float p,q;
  VCODE_ELEM v[VOC_TOKEN_LEN/2+1];
  /*  static VCODE_ELEM v1[VOC_TOKEN_LEN/2];  // static cause 0 initialization in case we begin near an mmo tap.
      static VCODE_ELEM v2[VOC_TOKEN_LEN/2];*/
  VCODE_ELEM *v1;
  VCODE_ELEM *v2;


  v1 = vring.cur_state.polar1;
  v2 = vring.cur_state.polar2;
  //    printf("new grain at %f\n",f);
  //CF:  we take a weighted average of the two closest recorded audio frames
  //CF:  lo and hi are indices of those frames.  q and p are their weights.
  lo = (int) f;
  hi = lo + 1;
  q = f - lo;
  p = 1-q;


  if (is_pre_vocoded) {  get_polar(lo,v1);  get_polar(hi,v2); } //CF:  only if prevocoded spectrum is used
  else 
    if (is_near_mmo_tap(f) == 0) {  
      //      printf("near tap\n");
      vcode_two_polar(lo, f, v1,v2);
      /*      vcode_polar(lo,v1); 
	      vcode_polar(hi,v2); */
    } //CF:  gets the spectra of both adjacent audio frames





  /*#ifdef PRE_VOCODED_AUDIO
  get_polar(lo,v1);  get_polar(hi,v2);
#else
  vcode_polar(lo,v1); vcode_polar(hi,v2);
  #endif */


  //CF:  weighted avg of the two spectra
  v[0].modulus = p*v1[0].modulus + q*v2[0].modulus;
  for (j=1; j < VOC_TOKEN_LEN/2; j++) v[j].modulus = p*v1[j].modulus + q*v2[j].modulus;

  sound[0] = v[0].modulus;
  sound[1] = 0;

  for (j=1; j < VOC_TOKEN_LEN/2; j++) {
    //CF:  filling 'sound' with required complex spectrum 
    //CF:  using modulus from data, and the replacement accumulated Delta phase.
    polar2complex(v[j].modulus,vring.cur_state.phase[j],&sound[2*j]);   //CF:  cur_state.phase is current Phi

    //CF:  set up required phases for next iteration
    //CF:  (making use of the data phase information)
    vring.cur_state.phase[j] += (v2[j].phase-v1[j].phase);              //CF:  set phase <- phase+\Delta phase
  }
  realft(sound-1,VOC_TOKEN_LEN/2,-1); //CF:  inverse FFT to get the resulting grain
  // for (i=0; i < VOC_TOKEN_LEN; i++) sound[i] *= 2;  //CF:  heuristic to account roughly for the gain loss due to vocoding!
}






#define PEAK_RAD 2



static void
phase_lock_new_grain(float f, float *sound) {
  float sound1[VOC_TOKEN_LEN],sound2[VOC_TOKEN_LEN];
  float mod1,phi1,mod2,phi2,p,q,prate,a,b,pp,qq,ph_diff;  // not really right length
  float phase_diff[VOC_TOKEN_LEN];
  unsigned char *orig,*temp,*center;
  int i,j,lo,hi;
  int w,num=0,peak[VOC_TOKEN_LEN],pk;
  VCODE_ELEM v[VOC_TOKEN_LEN/2+1];
  VCODE_ELEM v1[VOC_TOKEN_LEN/2];
  VCODE_ELEM v2[VOC_TOKEN_LEN/2];



  /* 8-06  It seems that truncating the frame position to be an integer
     might give worse audio quality especially with very low (or really
     an non-integral) rate.  should experiment not truncating */


  if (f < HOP) f = HOP;  // ow can read data before start of audio.
  lo = (int) f;
  hi = lo + 1;
  q = f - lo;  // note these are reset for continuous frame pos to p=1,q=0.
  p = 1-q;


  if (is_pre_vocoded) {  get_polar(lo,v1);  get_polar(hi,v2); }
  else if (is_near_mmo_tap(f) == 0) {  
      /*                           vcode_polar(lo,v1); 
				   vcode_polar(hi,v2);         */
      p = 1; q = 0;
      vcode_two_polar(lo, f, v1,v2);
  }

  /*#ifdef PRE_VOCODED_AUDIO
  get_polar(lo,v1);  get_polar(hi,v2);
#else
  vcode_polar(lo,v1); vcode_polar(hi,v2);
  #endif */
  

  //  v[0].modulus = p*v1[0].modulus + q*v2[0].modulus;

  for (j=0; j < VOC_TOKEN_LEN/2; j++)  v[j].modulus = p*v1[j].modulus + q*v2[j].modulus;


  for (w = PEAK_RAD; w < (VOC_TOKEN_LEN/2 - PEAK_RAD); w++) {
    for (j = -PEAK_RAD; j <= PEAK_RAD; j++) if (v[w+j].modulus > v[w].modulus) break;
    if (j <= PEAK_RAD) continue;
    phase_diff[num] = vring.cur_state.phase[w] - v1[w].phase;
    peak[num++] = w;
  }

  //  for (j=1; j < VOC_TOKEN_LEN/2; j++)  vring.cur_state.phase[j] += (v2[j].phase-v1[j].phase);     
 // regular phase vocoding

  for (j=0; j < num; j++) {
    lo = (j > 0) ? (peak[j-1] + peak[j])/2 : 1;
    hi = (j < (num-1)) ? (peak[j] + peak[j+1])/2 : (VOC_TOKEN_LEN/2);
    //    pk = peak[j];
    //    ph_diff = vring.cur_state.phase[pk] - v1[pk].phase;
    /* v2[pk].phase + ph_diff is desired phase at pk for phase vocoding */
    //        for (w=lo; w < hi; w++)  vring.cur_state.phase[w] = v2[w].phase + ph_diff;
    for (w=lo; w < hi; w++)  vring.cur_state.phase[w] = v2[w].phase + phase_diff[j];
  }

  sound[0] = v[0].modulus;
  sound[1] = 0;
  for (j=1; j < VOC_TOKEN_LEN/2; j++) 
    polar2complex(v[j].modulus,vring.cur_state.phase[j],&sound[2*j]);
  realft(sound-1,VOC_TOKEN_LEN/2,-1);
  //  for (i=0; i < VOC_TOKEN_LEN; i++) sound[i] *= 2;  //CF:  heuristic to account roughly for the gain loss due to vocoding!
}

void
queue_io_buff(CIRC_CHUNK_BUFF *buff, unsigned char *out) {
  memcpy(buff->ptr[buff->cur], out,buff->size);
  buff->cur = (buff->cur+1) % buff->chunks;
  if (buff->cur == buff->seam) { 
      performance_interrupted = LIVE_ERROR;
    strcpy(live_failure_string,"computer can't keep up with audio processing");
    printf("output audio not written in time\n"); 
    return;
    //    exit(0); 
  }
}

void
flush_io_buff(CIRC_CHUNK_BUFF *buff) {
  int d;


  while (buff->cur != buff->seam) {
	d = fwrite(buff->ptr[buff->seam],buff->size,1,buff->fp);
	//  crahsed here 5-11  --- follow this up call stack: recording or playing?
    buff->seam = (buff->seam+1) % buff->chunks;
  }
}


void write_io_buff(CIRC_CHUNK_BUFF *buff, unsigned char *out) {
  int d;
  

  d = fwrite(out,1,buff->size,buff->fp);
  if (d != buff->size) { 
      performance_interrupted = LIVE_ERROR;
    strcpy(live_failure_string,"disk output failure");
    printf("output error in write_io_buff\n"); 
    return;
    //    exit(0); 
  }
}

void temp_rewrite_audio() {
    int d;
    char fileName[500];
    
    strcpy(fileName, user_dir);
    strcat(fileName, "audio/new/new.raw");
    //remove the content
    FILE *fp = fopen(fileName, "wb");
    fclose(fp);
}

void temp_append_audio(int size, unsigned char *out) {
    int d;
    char fileName[500];
    
    strcpy(fileName, user_dir);
    strcat(fileName, "audio/new/new.raw");
    FILE *fp = fopen(fileName, "ab");
    fwrite(out,1,size,fp);
    fclose(fp);
}

void cal_vcode_features(AUDIO_FEATURE_LIST *flist){
      flist->num = 0;
      if(flist->el == NULL) flist->el = malloc(vring.audio_frames * sizeof(AUDIO_FEATURE));
      for(int i = 0; i < vring.audio_frames; i++){
            //flist->el[i] = ;
      }
}

void 
close_io_buff(CIRC_CHUNK_BUFF *buff) {
  fclose(buff->fp);
}

void
save_hires_audio_files() {
  char file[500],next[500],ifile[500];

  strcpy(ifile,audio_data_dir);  /* for some reason can't rebame in 1 step ??? */
  strcat(ifile,HIRES_OUT_NAME);
  printf("renaming %s %s\n",HIRES_OUT_NAME,ifile);
  rename(HIRES_OUT_NAME,ifile);
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".48k");
  printf("renaming %s %s\n",ifile,file);
  rename(ifile,file);



  //  strcpy(ifile,audio_data_dir);  /* for some reason can't rebame in 1 step ??? */
  //  strcat(ifile,"temp.48o");
  //  rename("temp.48o",ifile);
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".48o");
  rename(ifile,file);

  rename("temp.48o",file);

}



//#define OLA_LOSS_CONSTANT 1.25  // lose a factor of this due to overlap add --- just guessng here

//CF:  guts.  Generate one new frame of vocoder output audio.
static void
vcode_play_solo_audio_frame() {
  VCODE_ELEM new[VOC_TOKEN_LEN/2+1];
  int i,j,w;
  float sound[VOC_TOKEN_LEN],f,frame_secs;
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],solo[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  static FILE *fp;
  

  if (hires_fp) get_hires_solo_audio_frame(solo);
  else  get_upsampled_solo_audio_frame(solo);
  interleave_mono_channels(solo,solo, interleaved, VOC_TOKEN_LEN/HOP);
  if (mode != SIMULATE_MODE) {
    w = write_samples(interleaved,VOC_TOKEN_LEN/HOP); /* two channels */  //CF:  call to ALSA to play output ***
    //  printf("wrote at %f\n",now());  
    if (w < VOC_TOKEN_LEN/HOP) {
	  printf("failed in audio write 1 (%d < %d)\n",w, VOC_TOKEN_LEN/HOP);
      //    exit(0); 
    }
  }
  vring.frame++;  /* these are overlapped frames */
}





//CF:  guts.  Generate one new frame of vocoder output audio.
void
vcode_synth_frame_rate() {
  VCODE_ELEM new[VOC_TOKEN_LEN/2+1];
  int i,j,w;
  float sound[VOC_TOKEN_LEN],f,frame_secs,fsolo[HOP_LEN],forch[HOP_LEN],t1,t2;
  float left[HOP_LEN],rite[HOP_LEN];
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],solo[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  static FILE *fp;
  
  /*  printf("----------------pos is %f cur = %d desired time is %f now = %f\n",vring.cur_state.file_pos_secs,cur_accomp,score.midi.burst[cur_accomp].ideal_secs,next_frame_time());  */

  /*    if (vring.cur_state.file_pos_secs > score.midi.burst[cur_accomp].orchestra_secs)
      {   printf("----------------pos is %f cur = %d desired time is %f now = %f\n",vring.cur_state.file_pos_secs,cur_accomp,score.midi.burst[cur_accomp].ideal_secs,next_frame_time()); }
  */
  //  printf("rate = %f\n",vring.rate);

  frame_secs = VOC_TOKEN_LEN/((float)(HOP*OUTPUT_SR));  /* secs per frame, constant */
  vring.cur_state.file_pos_secs += vring.rate*frame_secs;    //CF:  tweak position in underlying audio file, in secs
  
  f = vring.cur_state.file_pos_secs / frame_secs;  /* position in audio file in (float) frames (frames overlap by eg. 3/4) */ 
  //CF:  spec case when we hit the end of the audio file -- jump back 20 frames and repeat
  if (((int) f) >= vring.audio_frames-20) f = vring.audio_frames-20;
  /* 20 of course is right but 2 causes a seg fault.  might want to figure this out at some point */

  if (vring.rate < .3)  pv_new_grain(f,sound);  /* straight phase vocoding */
  else  phase_lock_new_grain(f,sound);  /* phase-locking phase vocoding */
  //CF:  now we have one new grain.
  //CF:  Each output frame is a weighted combination of the latest 4 (HOP) grains.
  //CF:  We use a clever circular buffer structure to compute this efficiently...

  //CF:  add the new grain into the circular buffer, with the appropriate offset
  for (i=0, j = vring.frame*HOP_LEN; i < VOC_TOKEN_LEN; i++, j++) 
    vring.cur_state.obuff[j%VOC_TOKEN_LEN] += vring.gain*voc_window[i]*sound[i];  
  
  //CF:  one of the frames of the circular buffer now contains the required output.  Write this to 'out'. 

  for (i=0, j = (vring.frame%HOP)*HOP_LEN; i < HOP_LEN; i++,j++) {
    forch[i] = vring.cur_state.obuff[j];
    float2sample(vring.cur_state.obuff[j], out+i*BYTES_PER_SAMPLE);
    vring.cur_state.obuff[j] = 0;   //CF:  zero out the frame of the circular buffer now we've finished with it.
  }
    if (mode == SYNTH_MODE) { //CF:  mode with pre-recorded solo part -- we want to hear the solo in the mix too.
    if (hires_fp) get_hires_solo_audio_frame(solo);  // this will cause disc read block on the audio output callback ... okay for synth mode I guess
    else  get_upsampled_solo_audio_frame(solo);
    samples2floats(solo, fsolo, HOP_LEN);
    float_mix(fsolo, forch,left,rite,HOP_LEN);
  }
  else  float_mix(forch, forch,left,rite,HOP_LEN);


  if (mode == LIVE_MODE) save_orch_data(out , HOP_LEN);  //CF:  save (downsampled) copy of out audio, for testing (no disk output)
  //if (mode != SIMULATE_MODE)  write_audio_channels(left,rite,HOP_LEN);  /* two channels */
  vring.frame++;  /* these are overlapped frames */
  save_vcode_state();  /* nth state is state before  nth frame played */

  if (mode == LIVE_MODE) queue_io_buff(&orch_out,out);   
  if (mode == SIMULATE_MODE) write_io_buff(&orch_out,out);   // for oracle
}


void
vcode_synth_frame_var(int cur_frame) {
  VCODE_ELEM new[VOC_TOKEN_LEN/2+1];
  int i,j,w;
  float sound[VOC_TOKEN_LEN],f,frame_secs,fsolo[HOP_LEN],forch[HOP_LEN],t1,t2;
  float left[HOP_LEN],rite[HOP_LEN];
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],solo[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  static FILE *fp;
  
  /*  printf("----------------pos is %f cur = %d desired time is %f now = %f\n",vring.cur_state.file_pos_secs,cur_accomp,score.midi.burst[cur_accomp].ideal_secs,next_frame_time());  */

  /*    if (vring.cur_state.file_pos_secs > score.midi.burst[cur_accomp].orchestra_secs)
      {   printf("----------------pos is %f cur = %d desired time is %f now = %f\n",vring.cur_state.file_pos_secs,cur_accomp,score.midi.burst[cur_accomp].ideal_secs,next_frame_time()); }
  */
  //  printf("rate = %f\n",vring.rate);

  frame_secs = VOC_TOKEN_LEN/((float)(HOP*OUTPUT_SR));  /* secs per frame, constant */
  vring.cur_state.file_pos_secs = cur_frame*frame_secs;
  
  f = vring.cur_state.file_pos_secs / frame_secs;  /* position in audio file in (float) frames (frames overlap by eg. 3/4) */
    
  //CF:  spec case when we hit the end of the audio file -- jump back 20 frames and repeat
  if (((int) f) >= vring.audio_frames-20) f = vring.audio_frames-20;
  /* 20 of course is right but 2 causes a seg fault.  might want to figure this out at some point */

  if (vring.rate < .3)  pv_new_grain(f,sound);  /* straight phase vocoding */
  else  phase_lock_new_grain(f,sound);  /* phase-locking phase vocoding */
  //CF:  now we have one new grain.
  //CF:  Each output frame is a weighted combination of the latest 4 (HOP) grains.
  //CF:  We use a clever circular buffer structure to compute this efficiently...

  //CF:  add the new grain into the circular buffer, with the appropriate offset
  for (i=0, j = vring.frame*HOP_LEN; i < VOC_TOKEN_LEN; i++, j++) 
    vring.cur_state.obuff[j%VOC_TOKEN_LEN] += vring.gain*voc_window[i]*sound[i];  
  
  //CF:  one of the frames of the circular buffer now contains the required output.  Write this to 'out'. 

  for (i=0, j = (vring.frame%HOP)*HOP_LEN; i < HOP_LEN; i++,j++) {
    forch[i] = vring.cur_state.obuff[j];
    float2sample(vring.cur_state.obuff[j], out+i*BYTES_PER_SAMPLE);
    vring.cur_state.obuff[j] = 0;   //CF:  zero out the frame of the circular buffer now we've finished with it.
  }
    if (mode == SYNTH_MODE) { //CF:  mode with pre-recorded solo part -- we want to hear the solo in the mix too.
    if (hires_fp) get_hires_solo_audio_frame(solo);  // this will cause disc read block on the audio output callback ... okay for synth mode I guess
    else  get_upsampled_solo_audio_frame(solo);
    samples2floats(solo, fsolo, HOP_LEN);
    float_mix(fsolo, forch,left,rite,HOP_LEN);
  }
  else  float_mix(forch, forch,left,rite,HOP_LEN);


  if (mode == LIVE_MODE) save_orch_data(out , HOP_LEN);  //CF:  save (downsampled) copy of out audio, for testing (no disk output)
  //if (mode != SIMULATE_MODE)  write_audio_channels(left,rite,HOP_LEN);  /* two channels */
  vring.frame++;  /* these are overlapped frames */
  save_vcode_state();  /* nth state is state before  nth frame played */

  if (mode == LIVE_MODE) queue_io_buff(&orch_out,out);   
  if (mode == SIMULATE_MODE) write_io_buff(&orch_out,out);   // for oracle
      temp_append_audio(HOP_LEN*BYTES_PER_SAMPLE, out);
}


static void
pre_wasapi_vcode_synth_frame_rate() {
  VCODE_ELEM new[VOC_TOKEN_LEN/2+1];
  int i,j,w;
  float sound[VOC_TOKEN_LEN],f,frame_secs;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],solo[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  static FILE *fp;
  

  /*    if (vring.cur_state.file_pos_secs > score.midi.burst[cur_accomp].orchestra_secs)
      {   printf("----------------pos is %f cur = %d desired time is %f now = %f\n",vring.cur_state.file_pos_secs,cur_accomp,score.midi.burst[cur_accomp].ideal_secs,next_frame_time()); }
  */

  //  printf("rate = %f\n",vring.rate);

  frame_secs = VOC_TOKEN_LEN/((float)(HOP*OUTPUT_SR));  /* secs per frame, constant */
  vring.cur_state.file_pos_secs += vring.rate*frame_secs;    //CF:  tweak position in underlying audio file, in secs
  f = vring.cur_state.file_pos_secs / frame_secs;  /* position in audio file in (float) frames (frames overlap by eg. 3/4) */ 
  //  printf("frame pos = %f\n",f);

  //CF:  spec case when we hit the end of the audio file -- jump back 20 frames and repeat
  if (((int) f) >= vring.audio_frames-20) f = vring.audio_frames-20;
  /* 20 of course is right but 2 causes a seg fault.  might want to figure this out at some point */

  //        printf("secs = %f  f = %f len = %f\n",vring.cur_state.file_pos_secs,f,frame_secs);

  //        pv_new_grain(f,sound);  /* straight phase vocoding */ //CF:  make a new grain, retured in 'sound' ******

  if (vring.rate < .3)  pv_new_grain(f,sound);  /* straight phase vocoding */
  else  phase_lock_new_grain(f,sound);  /* phase-locking phase vocoding */

  //CF:  now we have one new grain.
  //CF:  Each output frame is a weighted combination of the latest 4 (HOP) grains.
  //CF:  We use a clever circular buffer structure to compute this efficiently...

  //CF:  add the new grain into the circular buffer, with the appropriate offset
  for (i=0, j = vring.frame*VOC_TOKEN_LEN/HOP; i < VOC_TOKEN_LEN; i++, j++) 
    vring.cur_state.obuff[j%VOC_TOKEN_LEN] += vring.gain*voc_window[i]*sound[i];  //

  //CF:  one of the frames of the circular buffer now contains the required output.  Write this to 'out'. (to send to ALSA)
  for (i=0, j = (vring.frame%HOP)*VOC_TOKEN_LEN/HOP; i < VOC_TOKEN_LEN/HOP; i++,j++) {
    float2sample(vring.cur_state.obuff[j], out+i*BYTES_PER_SAMPLE);
    vring.cur_state.obuff[j] = 0;   //CF:  zero out the frame of the circular buffer now we've finished with it.
  }
  if (mode == SYNTH_MODE) { //CF:  mode with pre-recorded solo part -- we want to hear the solo in the mix too.
    if (hires_fp) get_hires_solo_audio_frame(solo);
    //      if (is_hires) get_hires_solo_audio_frame(solo);
    else  get_upsampled_solo_audio_frame(solo);
        interleave_mono_channels(solo,out, interleaved, VOC_TOKEN_LEN/HOP);
	//        interleave_mono_channels(out, solo, interleaved, VOC_TOKEN_LEN/HOP);
    //  interleave_mono_channels(out, out, interleaved, VOC_TOKEN_LEN/HOP);
 
    //      	        interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);
  }
  //  else    interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);   //CF:  convert output to stereo
  else  interleave_mono_channels(out, out, interleaved, VOC_TOKEN_LEN/HOP);
  //  if (mode == LIVE_MODE || mode || SYNTH_MODE) save_orch_data(out, VOC_TOKEN_LEN/HOP);   change made 5-05  in synth mode use the saved orchestra audio so dp features are the same every time note mistake in original line
  if (mode == LIVE_MODE) save_orch_data(out , VOC_TOKEN_LEN/HOP);  //CF:  save (downsampled) copy of out audio, for testing

  //  printf("out = %d\n",out[0]);

  if (mode != SIMULATE_MODE) {
    w = write_samples(interleaved,VOC_TOKEN_LEN/HOP); /* two channels */  //CF:  call to ALSA to play output ***
   
   //  printf("wrote at %f\n",now());  
    if (w < VOC_TOKEN_LEN/HOP) {
	  printf("failed in audio write 2 (%d < %d)\n",w, VOC_TOKEN_LEN/HOP);
      //    exit(0); 
    }
  }
  //  else printf("good write of %d\n",w);
  //    w = read(Audio_fd, interleaved,4096); 
  //    if (w != 4096) printf("short read %d bytes\n",w);
  /*  if (fp == NULL) fp = fopen("orchestra.raw","w");
  fwrite(out,VOC_TOKEN_LEN/HOP,2,fp);
  if (token == 700) { fclose(fp); exit(0); }*/
  vring.frame++;  /* these are overlapped frames */
  save_vcode_state();  /* nth state is state before  nth frame played */


  if (mode == LIVE_MODE) queue_io_buff(&orch_out,out);   
  if (mode == SIMULATE_MODE) write_io_buff(&orch_out,out);   
}




void
write_click_frame() {
  int i,j,w;
  float sound[VOC_TOKEN_LEN],f,frame_secs;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];

  for (i=0; i < VOC_TOKEN_LEN/HOP; i++)     sound[i] = -.9;
  //  if (((rand() % 1000) / 1000.) < .1) sound[0] = .7;
  if (vring.frame%10 == 0) sound[0] = sound[1] = sound[2] = sound[3] = .9;
  // for (i=0; i < VOC_TOKEN_LEN/HOP; i++)     sound[i] = sin(.01*i);
  for (i=0; i < VOC_TOKEN_LEN/HOP; i++)  float2sample(sound[i], out+i*BYTES_PER_SAMPLE);
  interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);
  save_orch_data(out, VOC_TOKEN_LEN/HOP);  
  write_samples(interleaved,VOC_TOKEN_LEN/HOP); /* two channels */
  //  printf("wote frame at %f\n",now());
  vring.frame++;  /* these are overlapped frames */
}



static void
stable_vcode_synth_frame_rate() {
  VCODE_ELEM new[VOC_TOKEN_LEN/2];
  int i,j,w;
  float sound[VOC_TOKEN_LEN],f,frame_secs,div_const;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],solo[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  static FILE *fp;
  

  /*    if (vring.cur_state.file_pos_secs > score.midi.burst[cur_accomp].orchestra_secs) 
	{   printf("----------------pos is %f cur = %d desired time is %f now = %f\n",vring.cur_state.file_pos_secs,cur_accomp,score.midi.burst[cur_accomp].ideal_secs,next_frame_time()); } */
  


  frame_secs = VOC_TOKEN_LEN/((float)(HOP*OUTPUT_SR));  /* secs per frame */
  vring.cur_state.file_pos_secs += vring.rate*frame_secs;
  div_const = sqrt((float) VOC_TOKEN_LEN);


  f = vring.cur_state.file_pos_secs / frame_secs;  /* position in frames */
  //  which_solo_note(vring.cur_state.file_pos_secs);
  if (((int) f) >= vring.audio_frames-20) f = vring.audio_frames-20;
  /* 20 of course is right but 2 causes a seg fault.  might want to figure this out at some point */

  //  printf("secs = %f  f = %f len = %f\n",vring.cur_state.file_pos_secs,f,frame_secs);



  /*          for (i=0; i < score.solo.num; i++) 
    if (fabs(score.solo.note[i].orchestra_secs - vring.cur_state.file_pos_secs) < .1) break;
  if (i < score.solo.num) {
    if ( vring.cur_state.file_pos_secs < score.solo.note[i].orchestra_secs ) f -= .1/frame_secs;
    else f += .1/frame_secs;
    }*/


  if (is_pre_vocoded)   vcode_fast_interp(f, new);
  else  vcode_interp(f, new);


  /*#ifdef PRE_VOCODED_AUDIO
  vcode_fast_interp(f, new);
#else
    vcode_interp(f, new);
    #endif */



  //    cut_out_solo(vring.cur_state.file_pos_secs , new);
  for (j=0; j < VOC_TOKEN_LEN/2; j++) {
    polar2complex(new[j].modulus,vring.cur_state.phase[j],&sound[2*j]);
    vring.cur_state.phase[j] += new[j].phase;
  }
  realft(sound-1,VOC_TOKEN_LEN/2,-1);
  for (i=0; i < VOC_TOKEN_LEN; i++) sound[i] /= div_const;
  
  for (i=0, j = vring.frame*VOC_TOKEN_LEN/HOP; i < VOC_TOKEN_LEN; i++, j++) 
    vring.cur_state.obuff[j%VOC_TOKEN_LEN] += voc_window[i]*sound[i];
  for (i=0, j = (vring.frame%HOP)*VOC_TOKEN_LEN/HOP; i < VOC_TOKEN_LEN/HOP; i++,j++) {
    //    int2sample((int) (vring.cur_state.obuff[j] * (float) 0x10000), out+i*BYTES_PER_SAMPLE);
    float2sample(vring.cur_state.obuff[j], out+i*BYTES_PER_SAMPLE);
    vring.cur_state.obuff[j] = 0;
  }
  if (mode == SYNTH_MODE) {
    get_upsampled_solo_audio_frame(solo);
    interleave_mono_channels(out, solo, interleaved, VOC_TOKEN_LEN/HOP);
		  //      	        interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);
  }
  else    interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);
  if (mode == LIVE_MODE /*|| mode == SYNTH_MODE*/) save_orch_data(out, VOC_TOKEN_LEN/HOP);  
   w = write_samples(interleaved,VOC_TOKEN_LEN/HOP); /* two channels */
   //  printf("wrote at %f\n",now());  
  if ( w < VOC_TOKEN_LEN/HOP) {
	printf("failed in audio write 3 (%d < %d)\n",w, VOC_TOKEN_LEN/HOP);
    //    exit(0); 
  }
  //  else printf("good write of %d\n",w);
  //    w = read(Audio_fd, interleaved,4096); 
  //    if (w != 4096) printf("short read %d bytes\n",w);
  /*  if (fp == NULL) fp = fopen("orchestra.raw","w");
  fwrite(out,VOC_TOKEN_LEN/HOP,2,fp);
  if (token == 700) { fclose(fp); exit(0); }*/
  vring.frame++;  /* these are overlapped frames */
  save_vcode_state();  /* nth state is state before  nth frame played */
  //  if (vring.cur_state.file_pos_secs > 6.) { printf("set\n"); vring.cur_state.file_pos_secs = 5.8; }
}



static void
vcode_sola_synth_frame() {  /* Shift OverLap Add */
  int i,j,w,samp;
  float sound[VOC_TOKEN_LEN],f,frame_secs,fps,prate;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],*base;
  unsigned char interleaved[VOC_TOKEN_LEN*BYTES_PER_SAMPLE],solo[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  static FILE *fp;
  

  prate = ((float) orch_pitch) / 440.;
  frame_secs = VOC_TOKEN_LEN/((float)(HOP*OUTPUT_SR));  /* secs per frame */
  fps = vring.cur_state.file_pos_secs += vring.rate*frame_secs;
  f = vring.cur_state.file_pos_secs / frame_secs;  /* position in frames */
  if (((int) f) >= vring.audio_frames-20) f = vring.audio_frames-20;
  /* 2 of course is right but 2 causes a seg fault so use 20.  
     might want to figure this out at some point */

  //  printf("secs = %f  f = %f len = %f\n",vring.cur_state.file_pos_secs,f,frame_secs);

  samp = (int) (fps*OUTPUT_SR);
  base = vring.audio +  samp*BYTES_PER_SAMPLE;
  for (i=0; i < VOC_TOKEN_LEN; i++) sound[i] =  interp_audio(base, i*prate);
  for (i=0, j = vring.frame*VOC_TOKEN_LEN/HOP; i < VOC_TOKEN_LEN; i++, j++) 
    vring.cur_state.obuff[j%VOC_TOKEN_LEN] += voc_window[i]*sound[i];
  for (i=0, j = (vring.frame%HOP)*VOC_TOKEN_LEN/HOP; i < VOC_TOKEN_LEN/HOP; i++,j++) {
    float2sample(vring.cur_state.obuff[j], out+i*BYTES_PER_SAMPLE);
    vring.cur_state.obuff[j] = 0;
  }
  if (mode == SYNTH_MODE) {
    get_upsampled_solo_audio_frame(solo);
    interleave_mono_channels(out, solo, interleaved, VOC_TOKEN_LEN/HOP);
		  //      	        interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);
  }
  else    interleave_with_null_channel(out, interleaved, VOC_TOKEN_LEN/HOP);
  if (mode == LIVE_MODE || mode || SYNTH_MODE) save_orch_data(out, VOC_TOKEN_LEN/HOP);  
   w = write_samples(interleaved,VOC_TOKEN_LEN/HOP); /* two channels */
   //  printf("wrote at %f\n",now());  
  if ( w < VOC_TOKEN_LEN/HOP) {
    printf("failed in audio write 4 (%d < %d)\n",w, VOC_TOKEN_LEN/HOP);  
    //    exit(0); 
  }
  vring.frame++;  /* these are overlapped frames */
  save_vcode_state();  /* nth state is state before  nth frame played */
}




static void  
vocode_in_background_action(int num) { 
  static float err,next_callback_secs,interval;
  static float next_frame_secs;
  int i,playback_pos,cursor;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  int j;




      cursor = get_playback_cursor();
      playback_pos = (int) (vring.cur_state.file_pos_secs*SR/TOKENLEN);
      if (cursor != playback_pos && (now() > .1)) {
	          move_playback_cursor(playback_pos);
      }
    
  

  err = now() - next_callback_secs;
  //  printf("error = %f\n",err);
  if (err > .01) printf("late by %f on signal in vocode_in_background_action\n",err);
  //  if (now() > 1) { printf("now = %f\n,frames = %d\n",now(),vring.frame); exit(0); }
  do {
      //      vcode_synth_and_write_frame((float) 1.52*vring.frame);
    

                vcode_synth_frame_rate();
	    //	    vcode_sola_synth_frame();


    
				   
    next_frame_secs = vring.frame*VOC_TOKEN_LEN/ ((float) OUTPUT_SR * HOP);
    next_callback_secs = next_frame_secs - FUDGE_SECS;
    interval = next_callback_secs - now();
    //    if (interval <= 0) printf("got behind in audio plaing\n"); 
  } while (interval <= 0);

  //  printf("inteval = %f\n",interval);
  set_timer_signal(interval);
}

static void  
vcode_background_action(int num) { 
  static float err,next_callback_secs,interval;
  static float next_frame_secs;
  int i,playback_pos,cursor;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  int j;
  static int block=0;
  //  extern int stop_playing_flag;

  if (stop_playing_flag) { printf("stopped\n"); return;}
  //  if (block) return;
  //  block = 1;
  cursor = get_playback_cursor();
  playback_pos = (int) (vring.cur_state.file_pos_secs*HOP*NOMINAL_OUT_SR/VOC_TOKEN_LEN) - first_frame;;
  if (cursor != playback_pos && (now() > .1)) {
    move_playback_cursor(playback_pos);
  }
  err = now() - next_callback_secs;
  if (err > .01) printf("late by %f on signal in vocode_in_background_action\n",err);
  do {
    vcode_synth_frame_rate();
    next_frame_secs = vring.frame*VOC_TOKEN_LEN/ ((float) OUTPUT_SR * HOP);
    next_callback_secs = next_frame_secs - FUDGE_SECS;
    interval = next_callback_secs - now();
  } while (interval <= 0);
  set_timer_signal(interval);
  //  block = 0;
}





static int
solo_lags_behind() { // if true the pending solo note should have sounded a while ago
  int recent_acc_time;
  RATIONAL r;
  float diff,ws;

  r = sub_rat(score.midi.burst[cur_accomp].wholerat,score.solo.note[cur_note].wholerat);
  ws = get_whole_secs(score.midi.burst[cur_note].wholerat);
  diff = ws*r.num/ (float) r.den;
  return(diff > .4);
  //  printf("ws = %f r = %d/%d\n",ws,r.num,r.den);
  //  printf("pending solo note is %f behind this accompaniment event\n",diff);
  //  return(0);
}




/*void
init_vocode_synth_in_background() { 
  struct sigaction sa;
 
  sa.sa_handler = vocode_in_background_action;
  sa.sa_restorer = 0;
  sa.sa_flags = 0;
  if (sigaction(SIGALRM,&sa,0) == -1) {
    printf("couldn't create timer\n");
    exit(0);
  }
  }*/


//static int abcdef[1000];

//CF:  Anticipate maintains the index of one 'pending' acc note; ie. the next note to be inferred and played.
//CF:  Here we check if this note has been played by the vocoder.  If so, we report this to Anticipate.
//CF:  Anticipate will then recalculate the time for the NEW pending note.
void
check_if_cur_accomp_played() {  /* should be called after vring updated */
  int i,solo_lags,flush;
  float this_frame,diff,play_secs;
  static int last;


  if (cur_accomp > last_accomp) return; // all notes played

  if (vring.rate == 0) return; // could be stalling on the first solo note
  diff = vring.cur_state.file_pos_secs - score.midi.burst[cur_accomp].orchestra_secs;
  //  printf("check_if_cur_accomp_played: %d %f %f %f\n",cur_accomp,score.midi.burst[cur_accomp].orchestra_secs,vring.cur_state.file_pos_secs,diff);
  if (diff < 0) return;  /* still waiting */

  //    solo_lags = solo_lags_behind();
     solo_lags = 0;
  if (solo_lags) printf("solo is lagging at %s --- won't report event\n",score.midi.burst[cur_accomp].observable_tag);


  //  if (abcdef[cur_accomp]) return;  /* take this out later */
  //  abcdef[cur_accomp] = 1;
  this_frame = next_frame_time();

  //  printf("this_frame = %f token time = %f diff = %f \n",this_frame,tokens2secs((float)token),this_frame-tokens2secs((float)token));


  play_secs = this_frame - diff/vring.rate; //CF:  time that the pending acc note was played, in real-time seconds
  printf("detected %s at %f late by %f\n",score.midi.burst[cur_accomp].observable_tag,play_secs,play_secs - score.midi.burst[cur_accomp].ideal_secs);
  //  printf("this_frame = %f diff = %f rate = %f\n",this_frame,diff,vring.rate);
  /*  return;*/
  if (cur_accomp == last_accomp)  { 
    printf("just played last note (%s)\n",score.midi.burst[cur_accomp].observable_tag); /* return; /* added return in 8-04*/ 
    vring.rate = 1.;   // need natural decay
  }
  score.midi.burst[cur_accomp].actual_secs = play_secs;
  
    if (cur_accomp == last_accomp)
      printf("hello\n");

    cur_accomp++;   //CF:  increment pending acc note (the note Aticipate is searching for) ***
    if (mode == PV_MODE || cur_accomp -1 > last_accomp) return;
    if (solo_lags == 0) update_belief_accom(cur_accomp-1, play_secs, flush=0);  //CF:  report to Anticipate *** --- but don't do actual calculation
    else  recalc_next_accomp();	
    vring.rate = 1.;   // have a reasonable play rate in unlikely event that the play rate is not reset for the next audio callback.  this would only happen if flush_belief_queue is not called for a long time






    /*    cur_accomp++;   //CF:  increment pending acc note (the note Aticipate is searching for) ***
	  if (solo_lags == 0 && cur_accomp - 1 <= last_accomp && mode != PV_MODE) update_belief_accom(cur_accomp-1, play_secs, flush=1);  //CF:  report to Anticipate ***  */

  /*  if (cur_accomp < last_accomp) update_belief_accom(cur_accomp, play_secs, flush=1); this might be better
      cur_accomp++;*/
}


static int
currently_gated() {
  int gate,am_gated;

  gate = score.midi.burst[cur_accomp].gate;
    //  printf("cur_accomp = %d gate = %d\n",cur_accomp,gate);
  //  am_gated = (cur_note <=  gate);
  am_gated = (cur_note <=  gate || cur_accomp > last_accomp);  /* changed 9-04 */
  //  if (am_gated)    printf("cur_accomp = %d gate = %d cur_note = %d (%s) am_gated = %d\n",cur_accomp,gate,cur_note,score.solo.note[cur_note].observable_tag,am_gated);
  //    exit(0);

  return(am_gated);
}

#define STALL_MARGIN .3 //.2 //  2.   /* stall when inside this much */
#define SOUND_MARGIN .1


static int
within_stall_margin() {
  float t_orch,opos;

  if (cur_accomp > last_accomp) return(0);
  opos = score.midi.burst[cur_accomp].orchestra_secs - SOUND_MARGIN; 
  /* leave a little space before next note */
  t_orch = opos  - vring.cur_state.file_pos_secs;
  return(t_orch < STALL_MARGIN);
}



static void
fade_in_out() {
    float t;
    
    /*//  still investigating this ...   */
    //#ifdef START_ANYWHERE_EXPT
    if (mode != PV_MODE  && cur_note == firstnote && have_orchestral_intro() == 0) return; // when soloist begins, keep sound off until first note played
    //#endif
    if (cur_accomp >  last_accomp || stalled_out) {
        vring.gain *= .95;    //CF:  fade out at the piece end
        return;
    }
    //  t = next_frame_time();
    if (vring.gain > .999) { vring.gain = 1; return; }
    vring.gain += .05*(1 - vring.gain);  //  fade in
    //  printf("time is %f gain is %f\n",t,vring.gain);
}



static void
stall_me() {
  float t_orch,opos;
   

  if (cur_accomp > last_accomp) return;
  if (within_stall_margin() == 0) return;
  opos = score.midi.burst[cur_accomp].orchestra_secs - SOUND_MARGIN; 
  /* leave a little space before next note */
  //  t_orch = score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs;
  t_orch = opos  - vring.cur_state.file_pos_secs;
  /*if (t_orch < STALL_MARGIN)*/
  vring.rate = t_orch;
    if (score.solo.note[cur_note].cue == 0) {
        stalled_out = 1;  // will fade out and then terminate
        if (vring.gain > .01) return;
        performance_interrupted = LIVE_ERROR;
        strcpy(live_failure_string,"can't detect solo notes");
    }
  //  if (t_orch < .05) vring.cur_state.file_pos_secs -= .2;
  /*printf("cur_accomp = %s\n",score.midi.burst[cur_accomp].observable_tag);
    printf("coming note secs = %f cur pos = %f\n",score.midi.burst[cur_accomp].orchestra_secs,vring.cur_state.file_pos_secs);*/
  /*      printf("stalling rate = %f pos = %f\n",vring.rate,vring.cur_state.file_pos_secs); */
}


static float
vcode_frame2secs(int f) {
  return(f*VOC_TOKEN_LEN/ ((float) TRUE_OUTPUT_SR * HOP));
}


static int 
full_frames_buffered() {
  int f;
  f = (get_delay()-500)/ (VOC_TOKEN_LEN/HOP);
  return((f > 0) ? f : 0);
}

static int 
alt_full_frames_buffered() {
  return((get_delay()-400)/ (VOC_TOKEN_LEN/HOP));
  
}

static int
first_accessible_frame() {
  float f;

  return(vring.frame-full_frames_buffered());

  f = now() * TRUE_OUTPUT_SR * HOP / (float) VOC_TOKEN_LEN;
  return((int) f + 1);
}


static void
reset_rate_and_maybe_pos(int first_unplayed_frame, int diff) {
  float new_rate,desired,cur,t_real,t_orch,next_frame_secs,alt;
  int pending_accomp,fr;


  /*  fr=first_accessible_frame();
  printf("1st accessible frame = %d time = %f next_frame_time = %f\n",
  fr,vcode_frame2time(fr),next_frame_time());*/

  next_frame_secs = vcode_frame2secs(first_unplayed_frame);
  //  next_frame_secs = my_next_frame_time();

  //  t_real = score.midi.burst[cur_accomp].ideal_secs - next_frame_time(); // - FUDGE_SECS;
  t_real = score.midi.burst[cur_accomp].ideal_secs - next_frame_secs;
  t_orch = score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs;
  if (t_real < 0) {
    printf("---------------------reset pos to %f\n",score.midi.burst[cur_accomp].orchestra_secs);
        vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs;
	vring.rate = 1;  /* should be reset right away */
  }
  else {
    /* orchestra file time until next note */
    vring.rate = t_orch/t_real;
    if (vring.rate <= 0.) { 
      printf("bad rate set (%f) t_orch = %f t_real = %f\n",vring.rate,t_orch,t_real); 
      printf("orch_secs = %f file_pos_secs = %f\n",score.midi.burst[cur_accomp].orchestra_secs, vring.cur_state.file_pos_secs);
    exit(0); }
    if (vring.rate > 2.) { printf("truncating vring.rate\n"); vring.rate = 2.; }
  }
  //     printf("ideal next note secs is %f (orch pos = %f)\n",score.midi.burst[cur_accomp].ideal_secs,score.midi.burst[cur_accomp].orchestra_secs);
//         printf("orch = %f real = %f rate = %f next_frame_time = %f\n",t_orch,t_real,vring.rate,next_frame_time());
  //               printf("leaving plan orchextra at %f orch pos = %f rate = %f\n",now(),vring.cur_state.file_pos_secs,vring.rate);
}

int
have_orchestral_intro() {
  RATIONAL meas;
  float ws;
  int i;

  for (i=firstnote; i < score.solo.num; i++) if (is_solo_rest(i) == 0) break;
  if (i == score.solo.num) return(1);
  meas = sub_rat(score.solo.note[/*firstnote*/i].wholerat,score.midi.burst[first_accomp].wholerat);
  ws = get_whole_secs(score.midi.burst[first_accomp].wholerat);
  return(rat2f(meas)*ws > 1.);
}




//CF:  Writes one output frame of vocoder audio; then schedules an event to get called back for the next one.
void  
vcode_action() { 
  static float err,next_callback_secs,interval;
  static float next_frame_secs;
  float next_access_frame_secs,t1,t2;
  int i,fr;
  unsigned char out[VOC_TOKEN_LEN*BYTES_PER_SAMPLE];
  int j,diff;
  static int id;
  TIMER_EVENT e;

  if (midi_accomp) { 
	if (mode == LIVE_MODE) return;
    vcode_play_solo_audio_frame(); return; 
  }


  //  if (cur_accomp >/*=*/ last_accomp) vring.gain *= .95;    //CF:  fade out at the piece end
  fade_in_out();
 // printf("gain = %f\n",vring.gain);
  if (mode != PV_MODE && currently_gated())
      stall_me();          //CF:  Xeno-like slowing down if waiting for a gate (eg. waiting for a cue)

  //    printf("cur_accomp = %d\n",cur_accomp);
  //  printf("cur_accomp = %d currently_gated = %d\n",cur_accomp,currently_gated()); exit(0);

  //CF:  a check on signal timings
  err = now() - next_callback_secs;
  //  printf("err = %f\n",err);
  //  if (err > .01) printf("late by %f on signal\n",err);  not using this criterion
  //printf("vcode action at %f orch pos = %f rate = %f gain = %f\n",now(),vring.cur_state.file_pos_secs,vring.rate,vring.gain);
  //  if (now() > 1) { printf("now = %f\n,frames = %d\n",now(),vring.frame); exit(0); }

  /*  if (vring.frame > 5) { 
    write_samples(out,VOC_TOKEN_LEN/HOP); 
    rewind_playing(VOC_TOKEN_LEN/HOP);
    }*/

//CF:  not used:
  //  printf("expect next frame at %f = %f + %f\n",my_next_frame_time(),get_delay()/(float)OUTPUT_SR,now());

      //  t1 = now();
   vcode_synth_frame_rate();    //CF:  create and write a frame of output audio ******
    /*    t2 = now();
        if ((t2-t1) > .002) printf("long time in vcode_synth_frame_rate: %f\n",t2-t1);*/
  //  printf("inteval = %f\n",interval);


    //CF:  set next timer event
  //    e.priority = e.type = PLAYING_AUDIO;
  //    e.time = next_callback_secs;
  //    e.id = id++;  /* will never want to reset this so don't need id */
    //        printf("settting event for %f\n",e.time);
  //    set_timer_event(e);
#ifdef ONLINE_READ
    replenish_orch_audio(); 
    // if this goes after check_if_cur_accomp_played then encounter frquent delays
    // while it reads 
    
#endif

    check_if_cur_accomp_played(); //CF:  report to Anticipate if its pending note was played
    //    printf("next time should be %f\n",next_callback_secs);
    //  set_timer_signal(interval);
}


static void
clear_output_buffer() {
  int i;

  for (i=0; i < VOC_TOKEN_LEN; i++) vring.cur_state.obuff[i] = 0;
  for (i=0; i < VOC_TOKEN_LEN/2; i++) vring.cur_state.phase[i] = 0;
}

float
interp_orch_times(int i, RATIONAL r) { 
  /* the times for i and i+1 orchestra events determine a linear mapping from score pos to secs.  return the time (secs) corresponding to
     score pos r */
 float t1,t2,p1,p2,p,x,a1,a2;

 t2 = score.midi.burst[i+1].orchestra_secs;
 p2 = rat2f(score.midi.burst[i+1].wholerat);
 t1 = score.midi.burst[i].orchestra_secs;
 p1 = rat2f(score.midi.burst[i].wholerat);
 p = rat2f(r);
 a2 = (p-p1)/(p2-p1);
 a1 = 1-a2;
 x =  a2*t2 + a1*t1;
 return(x);
}



float orchestra_start_secs() {
  int i;
  float ws,t,mn,mx,o1,o2;
  RATIONAL rat_diff;

	o2 =  score.midi.burst[cur_accomp].orchestra_secs;
	o1 = (cur_accomp == 0) ? 0. :  score.midi.burst[cur_accomp-1].orchestra_secs;
	if (o2 - o1 > 20.)   { // big gap like in cadenza.  calculate backward from target point
	  ws = get_whole_secs(start_pos);  // the length of whole note in secs at the first solo note time
	  rat_diff = sub_rat(score.midi.burst[cur_accomp].wholerat,start_pos);  // whole notes until accomp plays
	  t =  o2 - rat_diff.num*ws/(float)rat_diff.den;
	  mn = o1+.1;
	  if (t < mn) t = mn;
	  return(t);
    }
	i = (cur_accomp > 0) ? cur_accomp-1 : 0; // in latter case still want linear prediction of starting place
	return(interp_orch_times(i,start_pos));
}

void
init_orchestra() {
   int solo_before_orch,i;
   float ws,mn,mx,t;
   RATIONAL rat_diff;

  vring.gain = (cur_accomp == 0) ? 0 : 1; // will fade in
  vring.gain = 0;
  orchestra_s = orchestra_acc=0;
  clear_output_buffer();

  /* this is an experiment that might work with some work.  Idea is to stall the orchestra
   and "mute" it until soloist plays something */
  /*  if (have_orchestral_intro()) {
    vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - .1;
    if (vring.cur_state.file_pos_secs < 0) vring.cur_state.file_pos_secs = 0;
    vring.rate = 1;
  }
  else {
    i = (cur_accomp > 0) ? cur_accomp-1 : 0; // in latter case still want linear prediction of starting place
    vring.cur_state.file_pos_secs =  interp_orch_times(i,start_pos);
    if (vring.cur_state.file_pos_secs < 0) vring.cur_state.file_pos_secs = 0;
    vring.rate = 1;  // nothing can happen with rate until solo note detected
  }
  return;*/



  //CF:  move to (a little bit before) the first acc event time

  //  solo_before_orch = (rat_cmp(start_pos,score.midi.burst[cur_accomp].wholerat) < 0);
  if (have_orchestral_intro()==0) {

	vring.cur_state.file_pos_secs = orchestra_start_secs();
  //	printf("target pos = %s\n",score.midi.burst[cur_accomp].observable_tag);
	/* if start *on* the cur_accomp note then trigger a note observation on 1st frame.  the calucation for belief not invoves
	   dividing something by the current rate (0) giving error */
	/*	printf("ws = %f\n",ws);
	printf("rat_diff = %d/%d\n",rat_diff.num,rat_diff.den);
	printf("time to wait = %f\n",rat_diff.num*ws/(float)rat_diff.den);
	exit(0);*/

	//	vring.cur_state.file_pos_secs =  interp_orch_times(i,start_pos);
	vring.rate = 0.;
   // vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - .3;  // old way
  }
  else {  // orchestra intro case --- easy

	vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - .1;
	vring.rate = 1;
  }

  //  vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - .1/*.3*/;
  if (vring.cur_state.file_pos_secs < 0) vring.cur_state.file_pos_secs = 0;
  //  printf("cur_accomp = %d pos = %f\n",cur_accomp,vring.cur_state.file_pos_secs);

  // vring.cur_state.file_pos_secs = 0;
  printf("rate = %f\n",vring.rate);
}

void
start_playing_orchestra() {
   int solo_before_orch;


   init_orchestra();
   vcode_action();     //CF:  meat
}

void
strip_start_playing_orchestra() {
  //   printf("must undo this horrible thing\n");

  //  printf("starting pos = %s\n",score.midi.burst[cur_accomp].observable_tag); exit(0);
  orchestra_s = orchestra_acc=0;
  vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - .1/*.3*/;
 
  vring.rate = 1.;
  printf("rate = %f\n",vring.rate);

}



void
adjust_vring_rate(float inc) {
  vring.rate += inc;
  printf("rate = %f\n",vring.rate); 
}

void
vocoder_test() {
  //start_vocode_in_background() {
  int prepare_sampling();
  char c[500],x;


  vcode_init();
  //  vcode_init_ring();
  prepare_playing(NOMINAL_OUT_SR);
  init_clock();

  //  init_vocode_synth_in_background();
  install_signal_action(vocode_in_background_action);
  vocode_in_background_action(0);
  printf("u = faster d = slower\n");
  return;
    while (1)  {
    scanf("%s",c);
    printf("chard = %s\n",c);
    if (c[0] == 'u') { vring.rate += .1; printf("rate = %f\n",vring.rate); c[0] = 0;}
    if (c[0] == 'd') { vring.rate -= .1; printf("rate = %f\n",vring.rate); c[0] = 0; }
    pause();
    }
  //  while (1);  /* to hog processor  intentionally */ 
}


void
vcode_test() {
  //start_vocode_in_background() {
  int prepare_sampling();
  char c[500],x;
  //  extern int stop_playing_flag;



  vring.cur_state.file_pos_secs = vcode_frames2secs((float) first_frame);
  //  first_frame = 3000;
  //  printf("%f\n",vring.cur_state.file_pos_secs); exit(0);
  stop_playing_flag = 0;
  prepare_playing(NOMINAL_OUT_SR);
  init_clock();
  install_signal_action(vcode_background_action);
  vcode_background_action(0);
  printf("u = faster d = slower\n");
}



#define STALL_WINDOW 2.   /* stall when inside this much */
#define LOOP_SECS .3

static int waiting_for_accomp_num;

void
stall_orchestra() {
  float t_orch,half_time;
  TIMER_EVENT e;

  if (cur_accomp > waiting_for_accomp_num) return;  
  //  printf("time in stall is %f rate = %f cur_accomp = %d\n",now(),vring.rate,cur_accomp);
  t_orch = score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs;




  if (t_orch < STALL_WINDOW) vring.rate = t_orch;
  //    printf("t_orch = %f stall rate = %f file pos = %f\n",t_orch,vring.rate,vring.cur_state.file_pos_secs);
  e.priority = e.type = STALLING_ORCHESTRA;
  e.time = now() + (t_orch/vring.rate)/2;
  e.id = cur_accomp;   /* this deletes old timer event for cur_accomp */
  set_timer_event(e);

  printf("rate = %f\n",vring.rate);
  
}

#define LOOP_SECS .1
#define LOOP_MARGIN .1

void
new_stall_orchestra() {
  float t_orch,half_time;
  TIMER_EVENT e;
  static int parity;

  if (cur_accomp > waiting_for_accomp_num) return;  
  //  printf("time in stall is %f rate = %f cur_accomp = %d\n",now(),vring.rate,cur_accomp);
  t_orch = score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs;

  /*  if (t_orch < LOOP_MARGIN + .1) { */
    //    printf("torch = %f\n",t_orch); 
  //    vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - (LOOP_SECS+LOOP_MARGIN); 
    vring.rate = parity ? 1. : -1; 
    /*  }*/

  parity = (parity+1)%2;
  printf("in stall rate = %f t_orch is %f now = %f\n",vring.rate,t_orch,now()); 

  e.priority = e.type = STALLING_ORCHESTRA;
  e.time = now() + LOOP_SECS; //(t_orch/vring.rate)-LOOP_MARGIN;
  e.id = cur_accomp;   /* this deletes old timer event for cur_accomp */
  set_timer_event(e);


  //  if (t_orch < STALL_WINDOW) vring.rate = t_orch;
  //  printf("t_orch = %f stall rate = %f file pos = %f\n",t_orch,vring.rate,vring.cur_state.file_pos_secs);

  
}

static float
time_until_pos(float pos) { /* at current rate how long until orchestra gets to pos */

  return((pos - vring.cur_state.file_pos_secs)/vring.rate);
}


static void
init_stall_orchestra(int event) {
  waiting_for_accomp_num = event;
  stall_orchestra();
  /*
  vring.rate = 1;
  e.priority = e.type = STALLING_ORCHESTRA;
  e.time = now() + (t_orch/vring.rate)/2;
  e.id = cur_accomp;  
  set_timer_event(e);*/
  
}

static void
new_init_stall_orchestra(int event) {
  float t_orch;
  TIMER_EVENT e;

  waiting_for_accomp_num = event;

  //  printf("rate = %f entering stall\n",vring.rate);
  t_orch = score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs;
  e.priority = e.type = STALLING_ORCHESTRA;
  e.time = now() + (t_orch-LOOP_MARGIN)/vring.rate;
  e.id = cur_accomp;  
  set_timer_event(e);
  
}


static float
prerec_time_to_onset() {
  float t1,t2,tmin;

  t1 = fabs(score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs);
  t2 = (cur_accomp > 0) ? 
    fabs(score.midi.burst[cur_accomp-1].orchestra_secs - vring.cur_state.file_pos_secs) : HUGE_VAL;
  tmin = (t1 < t2) ? t1 : t2;
  return(tmin);
}



void
set_netural_vocoder_rate() {
  vring.rate = 1;
}


#define MAX_VCODE_RATE  3. // 2.

#ifdef ACCOUNT_FOR_DELAY_EXPERIMENT
void
plan_orchestra(int free_to_go) {
  if (currently_gated()) return;
  if (cur_accomp > last_accomp) return;
  vring.rate = 0.;  /* means must be reset in next visit to vcode_action() */
}
#else


//CF:  called from Anticipate when belief in pending event changes -- tweaks vocoder. **
void
plan_orchestra(int free_to_go) {                     //CF:  free_to_go is not used
  float new_rate,desired,cur,t_real,t_orch;
  int pending_accomp,fr;

  //  printf("plan orch\n");

  /*  fr=first_accessible_frame();
  printf("1st accessible frame = %d time = %f next_frame_time = %f\n",
  fr,vcode_frame2time(fr),next_frame_time());*/

  if (mode != SIMULATE_MODE && currently_gated() &&  within_stall_margin()) {
    printf("holding up on queing %s (cur_note = %s)\n",score.midi.burst[cur_accomp].observable_tag,
	   score.solo.note[cur_note].observable_tag);
    return;  // if gated, tweaking is handled  elsewhere (by stall_me, called by called-back function)
  }

  // if (prerec_time_to_onset() < .05) { printf("onset correction for %d\n",cur_accomp); vring.rate = 1;  return; }  // for onsets
 


  //  printf("plan orchestra cur accomp = %s ideal = %f free_to_go = %d\n",score.midi.burst[cur_accomp].observable_tag,score.midi.burst[cur_accomp].ideal_secs,free_to_go);
  //  if (cur_accomp == 0) return;  /* haven't dealt with this yet */
//  if (cur_accomp >= last_accomp) return;
  if (cur_accomp > last_accomp) return;        //CF:  if out of notes -- end of score
  //    if (free_to_go == 0)  { init_stall_orchestra(cur_accomp); return; }

//  if (cur_accomp == 1) return;
//  printf("cur orch secs = %f orch time of next event = %f desired time of next event = %f\n",vring.cur_state.file_pos_secs,score.midi.burst[cur_accomp].orchestra_secs,score.midi.burst[cur_accomp].ideal_secs);
  //  t_real = score.midi.burst[cur_accomp].ideal_secs - (FUDGE_SECS + now());


  //  printf("ideal next time is %f now = %f\n",score.midi.burst[cur_accomp].ideal_secs,now());

  if (score.midi.burst[cur_accomp].ideal_secs == HUGE_VAL) exit(0);

  //CF: real-time until pending should be played:
  t_real = score.midi.burst[cur_accomp].ideal_secs - next_frame_time(); // - FUDGE_SECS; 

  //CF:  prerecorded-audio-time until pending should be played:
  t_orch = score.midi.burst[cur_accomp].orchestra_secs - vring.cur_state.file_pos_secs;

    printf("t_real = %f t_orch = %f ideal = %f next = %f\n",t_real,t_orch, score.midi.burst[cur_accomp].ideal_secs, next_frame_time()); 
  //CF:  basic plan:  set rate to ratio of these.  (but some annoying spec cases to handle)

  //    printf("orchestra secs = %f vring.cur_state.file_pos_secs = %f\n",score.midi.burst[cur_accomp].orchestra_secs, vring.cur_state.file_pos_secs);

  //CF:  If the desired time of the pending event is in the past, then we are late.
  //CF:  So SKIP AHEAD straight to the pending event's position in the prerec file.
  if (t_real < 0) {         
	printf("resetting orch file pos to %f --- cur accomp = %s ideal next orch = %f next_frame_time = %f\n",
	score.midi.burst[cur_accomp].orchestra_secs , score.midi.burst[cur_accomp].observable_tag, score.midi.burst[cur_accomp].ideal_secs , next_frame_time());
	vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs;  //CF:  SKIP to desired prerec location **
    vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs - t_real;  
    //CF:  SKIP to desired prerec location **
    
    vring.rate = 1;  /* should be reset right away */
    /*    printf("setting rate to 1 here\n");*/
    //    printf("orch pos = %f at %f\n",vring.cur_state.file_pos_secs,now()); 
  }
  else {  //CF:  regular case:
    /* orchestra file time until next note */
    //    vring.rate = t_orch/t_real;
    new_rate = t_orch/t_real;  //CF:  MEAT: ratio of real to prerec time
    //CF:  if the pending note has already been played by vocoder:  
    //CF:  Note that the vocoder-callback will therefore choose the new pending note and update its time very soon 
    //CF:  So basically do nothing for now until this update gives us more info.
    //    printf("t_orch = %f t_real = %f\n",t_orch,t_real); 
    if (new_rate <= 0.) { //CF:  (only happens rarely; a concurrency quirk)
      printf("tried to set bad rate (%f) t_orch = %f t_real = %f\n",new_rate,t_orch,t_real); 
      printf("next orch secs = %f file_pos_secs = %f\n",score.midi.burst[cur_accomp].ideal_secs ,next_frame_time());
      printf("cur_accomp = %s orch secs = %f file_pos_secs = %f\n",score.midi.burst[cur_accomp].observable_tag,score.midi.burst[cur_accomp].orchestra_secs, vring.cur_state.file_pos_secs);
      // exit(0); 
      //CF:  TODO: Chris thinks these print statements are too derogatory!
      return;
    }

    vring.rate = new_rate; //t_orch/t_real;


    if (vring.rate > MAX_VCODE_RATE) vring.rate = MAX_VCODE_RATE;    //CF:  dont allow rate>MAX_VCODE_RATE, because it starts sounding bad then.
    //    if (vring.rate > 2.) { printf("truncating vring.rate\n"); vring.rate = 2.; }
    //    if (vring.rate < .5) vring.rate = .5;
    //    if (cur_accomp == score.midi.num-1) vring.rate = 1.;  // need natural rate for final decay.
    //        if (cur_accomp >  last_accomp) vring.rate = 1.;  // 2-10 commented out  //need natural rate for final decay.
    printf("cur_accomp = %d last_accomp = %d t_orch = %f t_real = %f mollified rate = %f\n",cur_accomp,last_accomp,t_orch,t_real,vring.rate);
    //    printf("actual rate = %f\n",vring.rate);
  }



  //      printf("cur_accomp = %s rate = %f\n",score.midi.burst[cur_accomp].observable_tag,vring.rate);
  //    printf("ideal next note secs is %f (orch pos = %f)\n",score.midi.burst[cur_accomp].ideal_secs,score.midi.burst[cur_accomp].orchestra_secs);
  //     printf("orch = %f real = %f rate = %f next_frame_time = %f\n",t_orch,t_real,vring.rate,next_frame_time());
  //    printf("leaving plan orchextra at %f orch pos = %f rate = %f\n",now(),vring.cur_state.file_pos_secs,vring.rate);
  //  printf("next goal is %f\n",score.midi.burst[cur_accomp].ideal_secs);
}
#endif



int
new_oracle_vcode() {
  int i=0,j;
  char file[500];
  

  mode = SIMULATE_MODE;
  set_accomp_range();
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".48o");
  init_io_circ_buff(&orch_out,file,HOP_LEN*BYTES_PER_SAMPLE);
  vcode_init();
  vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs  - score.midi.burst[cur_accomp].ideal_secs;
  while ( next_frame_time() < nominal_tokens2secs((float)frames)) {
    background_info.fraction_done = next_frame_time()/ nominal_tokens2secs((float)frames);
    vcode_synth_frame_rate();

    if (cur_accomp <= last_accomp) check_if_cur_accomp_played();
    plan_orchestra(0);  
  } 
  close_io_buff(&orch_out);
}



int
oracle_vcode() {
  int i=0;
  char file[500];
  RATIONAL r;
  float diff;

  mode = SIMULATE_MODE;
  set_accomp_range();
  strcpy(file,audio_data_dir);
  strcat(file,current_examp);
  strcat(file,".48o");


  init_io_circ_buff(&orch_out,file,HOP_LEN*BYTES_PER_SAMPLE);
  
  vcode_initialize();
  vring.cur_state.file_pos_secs = score.midi.burst[cur_accomp].orchestra_secs  - score.midi.burst[cur_accomp].ideal_secs;
  while ( next_frame_time() < nominal_tokens2secs((float)frames)) {
    fade_in_out();  //maybe need this?
	i++;
	  background_info.fraction_done = next_frame_time()/ nominal_tokens2secs((float)frames);
       vcode_synth_frame_rate();

       /* on 12-13-07 substituted the folowing lines.  the difference is that check_if_cur_accomp_played
	uses the recently played accompaniment notes to reanticipate with a call to the belief net */

       //       if (cur_accomp <= last_accomp) check_if_cur_accomp_played();

       if (cur_accomp <= last_accomp) {
	 diff = vring.cur_state.file_pos_secs - score.midi.burst[cur_accomp].orchestra_secs;
	 if (diff >=0) cur_accomp++;
       }
       plan_orchestra(0);  
       printf("%d pos = %f rate = %f cur_accomp = %d last = %d target = %f\n",i, vring.cur_state.file_pos_secs,vring.rate,cur_accomp,last_accomp,score.midi.burst[cur_accomp].ideal_secs);
      //              if (i == 1000) break;
  } 
  close_io_buff(&orch_out);

  background_info.fraction_done = 1;
  return(0);
}





void
set_phase_func(float f, float *func) {
  VCODE_ELEM new[VCODE_FREQS];
  int i;

  vcode_interp((float) f, new);
  for (i=0; i < VCODE_FREQS; i++) {
    //    printf("i = %d\n",i);
    func[i] = new[i].phase;
  }
}

void
set_modulus_phase_func(float f, float *mod, float *phase) {
  VCODE_ELEM new[VCODE_FREQS];
  int i;

  vcode_interp((float) f, new);
  for (i=0; i < VCODE_FREQS; i++) {
    //    printf("i = %d\n",i);
    mod[i] = new[i].modulus;
    phase[i] = new[i].phase;
  }
}



#define PAIRS_FILE "spect_pairs.dat"
#define VAR_PAIRS_FILE "var_pairs.dat"

static void
compute_var_pairs() {
  int vf,f,i,j,total=0;
  int mark[2][VCODE_FREQS],marked,ni,oi,nj,oj;
  VCODE_ELEM new[2][VCODE_FREQS];
  FILE *fp;
  float secs,m=0;
  MATRIX count,U,V,D;
 

  fp = fopen(VAR_PAIRS_FILE,"w");

  var_pairs = Mperm_alloc(num_vars,num_vars);
  count = Mperm_alloc(num_vars,num_vars);
  for (i=0; i < num_vars; i++)  for (j=0; j < num_vars; j++)
    var_pairs.el[i][j]  = count.el[i][j] = 0;
  for (i=0; i < rel_freqs; i++)  mark[1][i] = 0;

  //    for (f=0; f < vring.audio_frames-10; f++) { /* I don't know why the kludge */
  for (f=0; f < 500; f++) { 
    if ((f % 100) == 0) printf("f = %d\n",f);
    secs = vcode_frames2secs((float) f);
    marked = mark_solo_cut(secs,mark[0]);
    vcode_interp((float) f, new[0]);

    for (i=0; i < num_vars; i++) {
      vardex2maskdex(i, &ni, &oi);
      if (mark[ni][oi]) continue;
      for (j=0; j < num_vars; j++) {
	vardex2maskdex(j, &nj, &oj);
	if (mark[nj][oj]) continue; 
	var_pairs.el[i][j] += new[ni][oi].modulus*new[nj][oj].modulus;
	count.el[i][j] += 1;
      }
    }
    for (i=0; i < rel_freqs; i++) { new[1][i] = new[0][i]; mark[1][i] = mark[0][i]; }
  }
  for (i=0; i < num_vars; i++)  for (j=0; j < num_vars; j++) 
    var_pairs.el[i][j] /= count.el[i][j];

  for (i=0; i < num_vars; i++)  for (j=0; j < num_vars; j++) {
    if  (var_pairs.el[i][j] > m) {
      m = var_pairs.el[i][j];
      //            printf("maxer at %d %d %f %e\n",i,j,m,m);
    }
  }

  for (i=0; i < num_vars; i++)  for (j=0; j < num_vars; j++) {
    var_pairs.el[i][j] /= m;
    if (var_pairs.el[i][j] > .7) {
      vardex2maskdex(i, &ni, &oi);
      vardex2maskdex(j, &nj, &oj);
      printf("high val at %d %d -- %d %d\n",ni,oi,nj,oj);
    }
  }
  //      spect_pairs = Mm(spect_pairs,Mi(spect_pairs));
  //      spect_pairs = Mi(spect_pairs);
  for (i=0; i < num_vars; i++)  fwrite(var_pairs.el[i]+i,num_vars-i,sizeof(FLOAT_DOUBLE),fp);



  fclose(fp);
  //  var_pairs.rows = var_pairs.cols = 5;
  //  Mp(var_pairs);
}



static void
read_var_pairs() {
  int i,j;
  FILE *fp;

  var_pairs = Mperm_alloc(num_vars,num_vars);
  fp = fopen(VAR_PAIRS_FILE,"r");
  for (i=0; i < num_vars; i++)  {
    fread(var_pairs.el[i]+i,num_vars-i,sizeof(FLOAT_DOUBLE),fp);
    for (j=i; j < num_vars; j++) var_pairs.el[j][i] = var_pairs.el[i][j];
  }
  /*    var_pairs.rows = var_pairs.cols = 4;
    Mp(var_pairs);
    exit(0);*/
  fclose(fp);
}

static void
read_spect_pairs() {
  int i,j;
  FILE *fp;
  MATRIX temp;

  spect_pairs = Mperm_alloc(rel_freqs,rel_freqs);
  spect_mean = Mperm_alloc(rel_freqs,1);
  concentration = Mperm_alloc(rel_freqs,rel_freqs);
  fp = fopen(PAIRS_FILE,"r");
  for (i=0; i < rel_freqs; i++) fread(spect_mean.el[i],1,sizeof(FLOAT_DOUBLE),fp);
  for (i=0; i < rel_freqs; i++)  {
    fread(spect_pairs.el[i]+i,rel_freqs-i,sizeof(FLOAT_DOUBLE),fp);
    for (j=i; j < rel_freqs; j++) {
      spect_pairs.el[j][i] = spect_pairs.el[i][j];
      //      printf("%f\n",spect_pairs.el[j][i]);
    }
  }
  /*  for (i=0; i < rel_freqs; i++)  {
    fread(concentration.el[i]+i,rel_freqs-i,sizeof(FLOAT_DOUBLE),fp);
    for (j=i; j < rel_freqs; j++) concentration.el[j][i] = concentration.el[i][j];
  }
  temp = concentration;*/


  //  temp = spect_pairs;
  //  temp = Mm(concentration,spect_pairs);
  /*  temp.rows = temp.cols = 10;
  Mp(temp);
  exit(0);*/
  fclose(fp);
  //        for (i=0; i < rel_freqs; i++)  printf("%d %f\n",i,spect_mean.el[i][0]); 
  //        exit(0);
}


#define PAIRS_FRAMES 15000

static void
compute_spect_pairs() {
  int vf,f,i,j,total=0,a,b;
  //  int mark[VCODE_FREQS],marked;
  int marked;
  int **mark,**imatrix();
  VCODE_ELEM new[VCODE_FREQS];
  FILE *fp;
  float secs,m=0;
  MATRIX count,U,V,D,mcount;
    INDEX_LIST past,solo,orch;

  fp = fopen(PAIRS_FILE,"w");

  spect_pairs = Mperm(Mzeros(rel_freqs,rel_freqs));
  spect_mean = Mperm(Mzeros(rel_freqs,1));
  count = Mperm(Mzeros(rel_freqs,rel_freqs));
  mcount = Mperm(Mzeros(rel_freqs,1));
 
  clear_matrix_buff();
  mark = imatrix(0,PAIRS_FRAMES,0,VCODE_FREQS-1);
  for (i=0; i < rel_freqs; i++)  for (j=0; j < rel_freqs; j++) count.el[i][j] = .5;


  //       for (f=0; f < vring.audio_frames-10; f++) { /* I don't know why the kludge */
	    for (f=0; f < PAIRS_FRAMES; f++) { /* I don't know why the kludge */
    if ((f % 100) == 0) printf("f = %d\n",f);
    //    if (f == 500) break;
    secs = vcode_frames2secs((float) f);
    marked = mark_solo_cut(secs,mark[f]);
    //    if (marked) continue;

    //    for (i=0; i < rel_freqs; i++) mark[i] = 0;

    vcode_interp((float) f, new);
    get_relevant_vars(f, &past, &solo, &orch, mark);
    //    for (i=0; i < rel_freqs; i++) new[i].modulus = log(new[i].modulus);
    //    new[0].modulus = 1;

    /*mark[0] = mark[1] = mark[2] = 0;
 new[0].modulus = drand48();
 new[1].modulus = drand48();
 new[2].modulus =  new[0].modulus +  new[1].modulus + .1*(drand48()-.5);
 printf("%f %f %f\n",new[0].modulus,new[1].modulus,new[2].modulus);*/



    /*        for (i=0; i < rel_freqs; i++) {
      if (mark[f][i]) continue;
      spect_mean.el[i][0] += new[i].modulus;
      mcount.el[i][0] += 1;
      for (j=0; j < rel_freqs; j++) {
	if (mark[f][j]) continue; 
	spect_pairs.el[i][j] += new[j].modulus*new[i].modulus;
	count.el[i][j] += 1;
      }
      }*/


           for (a=0; a < orch.num; a++) {
      i = orch.index[a];
      if (mark[f][i]) continue;
      spect_mean.el[i][0] += new[i].modulus;
      mcount.el[i][0] += 1;
      for (b=0; b < orch.num; b++) {
	j = orch.index[b];
	if (mark[f][j]) continue; 
	spect_pairs.el[i][j] += new[j].modulus*new[i].modulus;
	count.el[i][j] += 1;
      }
      }


  }

  for (i=0; i < rel_freqs; i++)  {
    spect_mean.el[i][0] /= mcount.el[i][0];
    for (j=0; j < rel_freqs; j++) 
    spect_pairs.el[i][j] /= count.el[i][j];
  }


  /*  for (i=0; i < rel_freqs; i++)  
    for (j=0; j < rel_freqs; j++) 
    spect_pairs.el[i][j] -= spect_mean.el[i][0] *  spect_mean.el[j][0];*/




  /*    for (i=0; i < rel_freqs; i++)  for (j=0; j < rel_freqs; j++) {
    if (fabs(spect_pairs.el[i][j]) > m) {
      m = spect_pairs.el[i][j];
      printf("maxer at %d %d %f %e\n",i,j,m,m);
    }
  }

    for (i=0; i < rel_freqs; i++)  for (j=0; j < rel_freqs; j++) {
      spect_pairs.el[i][j] /= m;
      }*/


  //      spect_pairs = Mm(spect_pairs,Mi(spect_pairs));
  //      spect_pairs = Mi(spect_pairs);
  //  Mp_file( spect_pairs, fp);

  for (i=0; i < rel_freqs; i++)  fwrite(spect_mean.el[i],1,sizeof(FLOAT_DOUBLE),fp);
  for (i=0; i < rel_freqs; i++)  fwrite(spect_pairs.el[i]+i,rel_freqs-i,sizeof(FLOAT_DOUBLE),fp);

  //    printf("computing inverse\n");
    //          Msvd_inv_in_place(spect_pairs);


  //	    spect_pairs = Mi(spect_pairs);
  //  for (i=0; i < rel_freqs; i++)  fwrite(spect_pairs.el[i]+i,rel_freqs-i,sizeof(FLOAT_DOUBLE),fp);

  fclose(fp);
  //  for (i=0; i < rel_freqs; i++)  printf("%d %f\n",i,spect_mean.el[i][1]);
  //  exit(0);
}



set_sub_matrices(MATRIX *within, MATRIX *between, int *mark) {
  int i,j,bj,wj,b,w,wi;
  
  wi = b = w = 0;
  for (i=0; i < rel_freqs; i++) if (mark[i]) b++; else w++;
  within->rows = between->rows =  within->cols = w;
  between->cols = b;
  for (i=0; i < rel_freqs; i++) {
    if (mark[i]) continue;
    bj = wj = 0;
    for (j=0; j < rel_freqs; j++) {
      if (mark[j]) between->el[wi][bj++] = spect_pairs.el[i][j];
      else within->el[wi][wj++] = spect_pairs.el[i][j];
    }
    wi++;
  }
}


extract_cov(MATRIX *S, MATRIX cov, INDEX_LIST *rows, INDEX_LIST *cols) {
  int i,j,ii,jj;
  

  S->rows = rows->num;
  S->cols = cols->num;
  for (i=0; i < rows->num; i++)  {
    ii = rows->index[i];
    for (j=0; j < cols->num; j++) {
      jj = cols->index[j];
      S->el[i][j] = cov.el[ii][jj];
    }
  }
  /*  print_index_list(rows);
  print_index_list(cols);
  printf("cov[0][0] = %f\n",cov.el[0][0]);
  Mp(*S);
  exit(0);*/
}

static void
get_observed(VCODE_ELEM *all, int marked, int *mark, MATRIX *obs) {
  int i,j;

  obs->rows = 1;
  obs->cols = rel_freqs-marked;
  for (i=j=0; i < rel_freqs; i++) if (mark[i]==0) {
    obs->el[0][j++] = all[i].modulus;
  }
}

static void
get_orch(VCODE_ELEM *all, INDEX_LIST *list, MATRIX *orch) {
  int i,j;

  orch->rows = 1;
  orch->cols = list->num;
  for (i=0; i < list->num; i++) {
    j = list->index[i];
    orch->el[0][i] = all[j].modulus; // - spect_mean.el[j][0];
  }
}

static void
get_predictors(int f, int **mark, VCODE_ELEM **all, INDEX_LIST *list, MATRIX *orch) {
  int i,j,nj,oj,ff;

  orch->rows = 1;
  orch->cols = list->num;
  for (i=0; i < list->num; i++) {
    j = list->index[i];
    vardex2maskdex(j, &nj, &oj);
    ff = f;
    if (nj) for (; ff > 0; ff--) if (mark[ff][oj] == 0) break; 
    orch->el[0][i] = all[/*f-nj*/ ff][oj].modulus;
  }
}

static void
set_hidden(VCODE_ELEM *all, int marked, int *mark, MATRIX hidden) {
  int i,j;
  float v;

  for (i=j=0; i < rel_freqs; i++) if (mark[i]==1) {
    //    if (hidden.el[0][j] < 0) printf("truncating %f\n", hidden.el[0][j]);
    v = hidden.el[0][j++];
    //    all[i].modulus = (v > 0) ? v : 0;
            all[i].modulus = v;
	//	printf("v = %f\n",v);
  }
}


static void
set_solo(VCODE_ELEM *all, INDEX_LIST *list, MATRIX hidden) {
  int i,j;
  float v;


  for (i=0; i < list->num; i++) {
    j = list->index[i];
    v = hidden.el[0][i]; // + spect_mean.el[j][0];
        if (v < 0) v = 0;
	    all[j].modulus = v/12;
	    //    all[j].modulus = v/4;
  }
}


     
static void
set_solo_shrinkage(VCODE_ELEM *all, INDEX_LIST *list, MATRIX hidden, MATRIX cond_cov) {
  int i,j;
  float v;


  for (i=0; i < list->num; i++) {
    v = hidden.el[0][i]/* - 1.5*sqrt(cond_cov.el[i][i])*/;
    if (v < 0) v = 0;
    j = list->index[i];
    all[j].modulus = v/4;
    //    printf("j = %d m = %f std = %f\n",j,hidden.el[0][i], sqrt(cond_cov.el[i][i]));
  }
}


     


void
old_get_vcode_rgb_spect(unsigned char **rgb, int *rows, int *cols) {
  float **s,secs;
  int f,i,j,t,v;
  int start,end,k,**mark,**imatrix(),mask,marked,last_marked,relevant[VCODE_FREQS];
  float spread,mmax= -10000,mmin=10000,m,band,**matrix(),**lp,pmax,pmin,*sp,temp,total=0;
  VCODE_ELEM new[VCODE_FREQS];
  FILE *fp,*fph;
  MATRIX sub_pred,sub_resp,theta,observed,hidden,Sxx,Sxy,U,D,V;
  INDEX_LIST rsolo,rorch;


  //  rel_freqs = 4;

  //         compute_spect_pairs(); exit(0);
  //    compute_var_pairs(); exit(0);
  //  fph = fopen("data","w");
  fp = fopen(PAIRS_FILE,"r");
  spect_pairs = Mperm(Mr_file(fp));


  
  
  start = 0;
  end =  5000; //vring.audio_frames;


  sub_pred = Mperm_alloc(rel_freqs,rel_freqs);
  sub_resp = Mperm_alloc(rel_freqs,rel_freqs);
  Sxx = Mperm_alloc(rel_freqs,rel_freqs);
  Sxy = Mperm_alloc(rel_freqs,rel_freqs);
  U = Mperm_alloc(rel_freqs,rel_freqs);
  D = Mperm_alloc(rel_freqs,rel_freqs);
  V = Mperm_alloc(rel_freqs,rel_freqs);
  theta = Mperm_alloc(rel_freqs,rel_freqs);
  observed = Mperm(Mzeros(1,rel_freqs));
  

  *rgb = (unsigned char *) malloc(3*(end-start)*VCODE_FREQS);
  s = matrix(start,end,0,rel_freqs);
  mark = imatrix(start,end,0,VCODE_FREQS-1);
  for (f=start; f < end; f++) {


    vcode_interp((float) f, new);

    
    //    for (i=0; i < rel_freqs; i++) new[i].modulus = log(new[i].modulus);
    //    new[0].modulus = 1;


    secs = vcode_frames2secs((float) f);
    marked = mark_solo_cut(secs,mark[f]);

    //    temp = new[3].modulus;

    /*    for (i=0; i < 5; i++) mark[f][i] = 1;
    set_sub_matrices(&sub_pred,&sub_resp,mark[f]);
    theta = Mm(Mi(sub_pred),sub_resp);
    Mp(spect_pairs);
    Mp(sub_pred);
    Mp(sub_resp);
    Mp(theta);
    exit(0);*/

    //    find_harmonic_data(fph,secs, mark[f], new);

    //    printf("f = %d marked = %d\n",f,marked);
    if (marked) {
      if (last_marked != marked) {
	clear_matrix_buff();
	cut_out_solo(vcode_frames2secs((float) f) , new);

	get_relevant_freqs(secs, &rsolo ,&rorch, mark[f]);
	extract_cov(&Sxx, spect_pairs, &rorch, &rorch);
	extract_cov(&Sxy, spect_pairs, &rorch, &rsolo);


	Msvd_inv_in_place(Sxx);

	Mma(Sxx,Sxy,&theta);

	//	theta = Mm(Mi(Sxx),Sxy);
	//	Mp(Sxx);
	  //	Mp(theta);
	//	exit(0);
	//print_index_list(&rsolo);
	//	print_index_list(&rorch);
	//	print_index_list(&rsolo);
	//	set_sub_matrices(&sub_pred,&sub_resp,mark[f]);
	//	printf("hello sub_pred = %dx%d\n",sub_pred.rows,sub_pred.cols);
	//	printf("sub_resp = %dx%d\n",sub_resp.rows,sub_resp.cols);
	//	theta = Mm(Mi(sub_pred),sub_resp);
	//	Mp(theta);
	//	printf("\n");
      }
      get_orch(new, &rorch, &observed);
      //      get_observed(new,marked,mark[f],&observed);
      hidden = Mm(observed,theta);
      //      set_hidden(new, marked, mark[f],hidden);

      for (i=0; i < VCODE_FREQS; i++) if (mark[f][i]) new[i].modulus = 0;
      set_solo(new, &rsolo, hidden);
      

      last_marked = marked;
    }


    //    printf("err = %f\n",new[3].modulus-temp);
    //    total += new[3].modulus-temp;
    //    printf("total = %f\n",total);
    /*  for (i=0; i < VCODE_FREQS; i++) if (mark[f][i]) new[i].modulus = 0;
	fill_in_gaps(secs, mark[f], new);*/

    //    for (i=0; i < rel_freqs; i++) new[i].modulus = exp(new[i].modulus) - 1;

  
    //            printf("%d %f\n",50,new[50].modulus );

    /*            for (j=0; j < rel_freqs; j++)  {
      new[j].modulus = exp(new[j].modulus);
      //      printf("%d %f\n",j,new[j].modulus );
      
      }*/

    //    new[0].modulus =new[1].modulus;
    //    	printf("%d at 50 = %f %f\n",f,new[50].modulus,exp(new[50].modulus));


    for (j=0; j < rel_freqs; j++)  vcode_data[f][j] = new[j];
    for (j=0; j < rel_freqs; j++)  s[f][j] =  pow(log(1+square(new[j].modulus)),/*.4*/ .2);
  }




  for (i=0; i < rel_freqs; i++)   for (j=0; j < rel_freqs; j++) spect_pairs.el[i][j] /= end;
  for (i=0; i < 8; i++) {  
    for (j=0; j < 8; j++) printf("%e\t",spect_pairs.el[i][j]);
    printf("\n");
  }
  m=HUGE_VAL;
  for (i = 0; i < rel_freqs; i++) 
    for (j=start; j < end; j++) {
    if (s[j][i] > mmax) {
      mmax = s[j][i];  
      //            printf("max = %d %d %f\n",j,i,mmax);
    }
    if (s[j][i] < mmin) {
      mmin = s[j][i];  
      //            printf("min = %d %d %f\n",j,i,mmin);
    }
  }
  printf("min = %f max = %f\n",mmin,mmax);
  //  exit(0);
  spread = mmax - mmin;
  k= 0;
  for (i = 0; i < rel_freqs; i++) {
    for (j=start; j < end; j++) {
      v = (int) (256*(s[j][rel_freqs-i-1] - mmin)/spread);
      mask = 1-mark[j][rel_freqs-i-1];
      (*rgb)[k++] = v*mask;      (*rgb)[k++] = v/**mask*/;      (*rgb)[k++] = v/**mask*/;
    }
  }
  *cols = end-start;
  *rows = rel_freqs;
  //    fclose(fph);
}


static float
first_event_secs_before(RATIONAL r) {
  int k,i;
  RATIONAL rk,ri;

  for (k=0; k < score.midi.num-1; k++) 
    if (rat_cmp(score.midi.burst[k+1].wholerat,r) >= 0) break;
  for (i=0; i < score.solo.num-1; i++) 
    if (rat_cmp(score.solo.note[i+1].wholerat,r) >= 0) break;
  rk  = score.midi.burst[k].wholerat;
  ri = score.solo.note[i].wholerat;
  return((rat_cmp(rk,ri) > 0) ? score.midi.burst[k].orchestra_secs : 
	 score.solo.note[i].orchestra_secs); 
}


static float
first_event_secs_after(RATIONAL r) {
  int k,i;
  RATIONAL rk,ri;

  for (k=score.midi.num-1; k > 0; k--)
    if (rat_cmp(score.midi.burst[k-1].wholerat,r) <= 0) break;

  for (i=score.solo.num-1; i > 0; i--)
    if (rat_cmp(score.solo.note[i-1].wholerat,r) <= 0) break;
  rk  = score.midi.burst[k].wholerat;
  ri = score.solo.note[i].wholerat;
  return((rat_cmp(rk,ri) < 0) ? score.midi.burst[k].orchestra_secs : 
	 score.solo.note[i].orchestra_secs); 
}



#define SEARCH_WID 5 //2
#define FIX_FRAMES 8
#define FIX_WIDTH 3
#define FIX_LEFT 2
#define FIX_RIGHT 5

void
blur_transients(int start, int end, int **mark, VCODE_ELEM **v) {
  int f,fl,fr,i,j,w,lo,jm,dl,dr,low_midi,k,flast,safe,al,ar;
  float pl,pr,pc,tot,s,last_tot,rat,m;
  SOUND_NUMBERS chrd;
  RATIONAL ri,rk,r;
    




  for (i=0; i < score.solo.num; i++) {
    m = -HUGE_VAL;
    if (score.solo.note[i].snd_notes.num == 0) continue;
    ri  = score.solo.note[i].wholerat;
    f = vcode_secs2frames(score.solo.note[i].orchestra_secs) + .5;
    if (f < start || f > end) continue;
    if (f < 10) continue;

    low_midi = 1000;  /* impossibly high midi pitch */
    chrd = score.solo.note[i].snd_notes;
    for (j=0; j < chrd.num; j++) if (chrd.snd_nums[j] < low_midi) low_midi = chrd.snd_nums[j];
    chrd = score.solo.note[(i ==0) ? 0 : i-1].snd_notes;
    for (j=0; j < chrd.num; j++) if (chrd.snd_nums[j] < low_midi) low_midi = chrd.snd_nums[j];
    lo = (int) (vcode_midi2omega(low_midi-.5) + .5);
    m = -HUGE_VAL;
    for (j=f-SEARCH_WID; j <= f+SEARCH_WID; j++) {
      tot = 0;
      for (w=lo; w < rel_freqs; w++) {
	tot += (v[j][w].modulus - v[j-1][w].modulus);
	//	printf("%f\n",v[j][w].modulus);
      }
      if (tot > m) { m= tot; jm = j; }
      printf("%d %f\n",j,tot);
    }
    printf("\n");

    for (k=0; k < score.midi.num-1; k++) 
      if (rat_cmp(score.midi.burst[k+1].wholerat,ri) >= 0) break;
    printf("solo = %s midi = %s\n",score.solo.note[i].observable_tag,score.midi.burst[k].observable_tag);
    rk  = score.midi.burst[k].wholerat;
    //    ri = score.solo.note[i-1].wholerat;
    flast = (rat_cmp(rk,ri) > 0) ? vcode_secs2frames(score.midi.burst[k].orchestra_secs) : 
      vcode_secs2frames(score.solo.note[i-1].orchestra_secs);
    
    al = (int) vcode_secs2frames(first_event_secs_before(ri));
    ar = (int) vcode_secs2frames(first_event_secs_after(ri));
    if (al >= jm) al = jm-1;
    if (ar <= jm) ar = jm+1;
    if (al >= jm || ar <= jm) { printf(" al = %d ar = %d jm = %d bad something\n",al,ar,jm); exit(0); }
    dl = (al + jm)/2;
    dr = (ar + jm)/2;

    //    printf("al = %d jm = %d ar = %d\n",al,jm,ar);
  
    for (j = jm-FIX_LEFT; j <= jm+FIX_RIGHT; j++) {
      //      dl = jm-1;
      //      dl = safe;
      //      dr = jm + FIX_FRAMES;
      pl = exp(-.1*(j-dl));
      pr = exp(-.1*(dr-j));
      tot = pl + pr;
      pl = pl/tot;
      pr = pr/tot;
      for (w=lo; w < rel_freqs; w++) 
	v[j][w].modulus = pl*v[dl][w].modulus+ pr*v[dr][w].modulus;
    }
    //                        for (w=lo; w < rel_freqs; w++) v[jm][w].modulus = 0;
  }      
  return;



  for (f=start; f < end; f++) for (i=0; i < rel_freqs; i++) {
    if (mark[f][i]==0) continue;
    for (fl = f; fl > 0; fl--)  if (mark[fl][i] == 0) break;
    for (fr = f; fr < end; fr++)  if (mark[fr][i] == 0) break;
    s =   vcode_frames2secs((float) (fr-fl));
    if (s > .5) continue;
    dl = fabs(f-fl);
    dr = fabs(f-fr);
    pl = exp(-.3*dl);
    pr = exp(-.3*dr);
    tot = pl + pr;
    pl = pl/tot;
    pr = pr/tot;
    v[f][i].modulus = pl*v[fl][i].modulus+ pr*v[fr][i].modulus;
  }
}


void
get_vcode_rgb_spect(unsigned char **rgb, int *rows, int *cols) {
  float **s,secs;
  int f,i,j,t,v,flo =0,fhi= rel_freqs;
  int start,end,k,**mark,**imatrix(),mask,marked,last_marked,relevant[VCODE_FREQS];
  float spread,mmax= -10000,mmin=10000,m,band,**matrix(),**lp,pmax,pmin,*sp,temp,total=0;
  //  VCODE_ELEM new[VCODE_FREQS];
  FILE *fp,*fph;
  MATRIX sub_pred,sub_resp,theta,observed,hidden,Sxx,Sxy,U,D,V,Cyy;
  INDEX_LIST rpast,rpred,rsolo,rorch,last_rpast,last_rsolo,last_rorch;




  //    compute_spect_pairs();   exit(0);
  //    fp = fopen(PAIRS_FILE,"r");
  //      spect_pairs = Mperm(Mr_file(fp));
  read_spect_pairs();


  //        compute_var_pairs();  exit(0);
      /*    exit(0);*/

  //    fph = fopen("data","w");
  /*  fp = fopen(PAIRS_FILE,"r");
  spect_pairs = Mperm(Mr_file(fp));
  fclose(fp);*/

	   //  fp = fopen(VAR_PAIRS_FILE,"r");
	   //  var_pairs = Mperm(Mr_file(fp));
      //    	   read_var_pairs();

	   /***********************************/

	   //	   for (i=rel_freqs; i < 2*rel_freqs; i++)
	   //	     for (j=0; j < 2*rel_freqs; j++) var_pairs.el[i][j] = 0;

	   /***********************************/

	   /*	   spect_pairs.rows = spect_pairs.cols = var_pairs.rows = var_pairs.cols = 5;
	   Mp(spect_pairs);
	   Mp(var_pairs);*/


  
  
  start = 0;
  end =   vring.audio_frames;


  sub_pred = Mperm_alloc(rel_freqs,rel_freqs);   /* the maximum number of predictors */
  sub_resp = Mperm_alloc(rel_freqs,rel_freqs);   /* and the max number of response variables */
  Sxx = Mperm_alloc(rel_freqs,rel_freqs);       /* should be the limits here ... also */
  Cyy = Mperm_alloc(rel_freqs,rel_freqs);       /* should be the limits here ... also */
  Sxy = Mperm_alloc(rel_freqs,rel_freqs);      /* the size of the INDEX_LIST shsoudl be */
  U = Mperm_alloc(rel_freqs,rel_freqs);       /* changed accordingly */
  D = Mperm_alloc(rel_freqs,rel_freqs);
  V = Mperm_alloc(rel_freqs,rel_freqs);
  theta = Mperm_alloc(rel_freqs,rel_freqs);
  observed = Mperm(Mzeros(1,rel_freqs));


  /*  printf("here\n");
  Mcopymat(spect_pairs,&concentration);
  Msvd_inv_in_place(concentration);
  exit(0);*/
  

  *rgb = (unsigned char *) malloc(3*(end-start)*VCODE_FREQS);
  s = matrix(start,end,0,rel_freqs);
  mark = imatrix(start,end,0,VCODE_FREQS-1);
  for (f=start; f < end; f++) {


    vcode_interp((float) f, vcode_data[f]);

    
    //    for (i=0; i < rel_freqs; i++) vcode_data[f][i].modulus = log(vcode_data[f][i].modulus);
    //    vcode_data[f][0].modulus = 1;


    secs = vcode_frames2secs((float) f);
    marked = mark_solo_cut(secs,mark[f]);
    //    marked = mark_solo_harms(secs,mark[f],vcode_data[f]);


    //    temp = vcode_data[f][3].modulus;

    /*    for (i=0; i < 5; i++) mark[f][i] = 1;
    set_sub_matrices(&sub_pred,&sub_resp,mark[f]);
    theta = Mm(Mi(sub_pred),sub_resp);
    Mp(spect_pairs);
    Mp(sub_pred);
    Mp(sub_resp);
    Mp(theta);
    exit(0);*/

    /*       find_harmonic_data(fph,secs, mark[f], vcode_data[f]);
	     continue;*/

    //    printf("f = %d marked = %d\n",f,marked);


    //	get_relevant_freqs(secs, &rsolo ,&rorch, mark[f]);

    get_relevant_vars(f, &rpast, &rsolo ,&rorch, mark);


        printf("f = %d\n",f);
    //    printf("orch has %d els solo has %d past has %d\n",rorch.num,rsolo.num,rpast.num);
    /*    print_index_list(&rpast);
    print_index_list(&rsolo)
    print_index_list(&rorch);*/
    if (marked) {
      //      if (new_situation(&rorch,&last_rorch,&rpast,&last_rpast,&rsolo,&last_rsolo)) {
      if (new_situation2(&rorch,&last_rorch,&rsolo,&last_rsolo)) {
	clear_matrix_buff();
	
	
	merge_index_lists(&rpast,&rorch,&rpred);

	printf("%d predictors\n",rpred.num);
	//	rpred.num = 30; for (i=0; i < 30; i++) rpred.index[i] = i;
      
	extract_cov(&Sxx, spect_pairs, &rorch, &rorch);
	extract_cov(&Sxy, spect_pairs, &rorch, &rsolo);
	//	extract_cov(&Cyy, concentration, &rsolo, &rsolo);

	//	print_index_list(&rpast);
	//	print_index_list(&rorch);
	//	print_index_list(&rsolo);
	//	Mp(Sxx);
	//	Mp(Sxy);
	copy_index_list(&rorch,&last_rorch);

	copy_index_list(&rsolo,&last_rsolo);	
	//	copy_index_list(&rpast,&last_rpast);
	Msvd_inv_in_place(Sxx);
	//	Msvd_inv_in_place(Cyy);
	//	Mi_inplace(Sxx);
	Mma(Sxx,Sxy,&theta);
	/*				Mp(Sxx);
	Mp(Sxy);
	Mp(theta);
	get_predictors(f, vcode_data, &rpred, &observed);
	hidden = Mm(observed,theta);
	Mp(observed);
	Mp(hidden);
	set_solo(vcode_data[f], &rsolo, hidden);
//	for (i=flo; i < fhi; i++) printf("%f\n",vcode_data[f][i].modulus);
exit(0);*/
      }
            get_orch(vcode_data[f], &rorch, &observed);
      //  get_predictors(f, mark, vcode_data, &rpred, &observed);
  //      get_observed(vcode_data[f],marked,mark[f],&observed);
      hidden = Mm(observed,theta);
      //      set_hidden(vcode_data[f], marked, mark[f],hidden);

      //      Mp(observed);
      //      Mp(hidden);
      for (i=0; i < VCODE_FREQS; i++) if (mark[f][i]) vcode_data[f][i].modulus = 0;
                set_solo(vcode_data[f], &rsolo, hidden);

      

    }





  }

        blur_transients(start,end,mark, vcode_data);
  //  filter_out_transients(vcode_data);

		write_vcode_data(start,end);
		exit(0);


    for (f = start; f < end; f++) for (j=0; j < rel_freqs; j++)  
      s[f][j] =  pow(log(1+square(vcode_data[f][j].modulus)),/*.4*/ .2);






  m=HUGE_VAL;
  for (i = 0; i < rel_freqs; i++) 
    for (j=start; j < end; j++) {
    if (s[j][i] > mmax) {
      mmax = s[j][i];  
      //            printf("max = %d %d %f\n",j,i,mmax);
    }
    if (s[j][i] < mmin) {
      mmin = s[j][i];  
      //            printf("min = %d %d %f\n",j,i,mmin);
    }
  }
  printf("min = %f max = %f\n",mmin,mmax);
  //  exit(0);
  spread = mmax - mmin;
  k= 0;
  for (i = 0; i < rel_freqs; i++) {
    for (j=start; j < end; j++) {
      v = (int) (256*(s[j][rel_freqs-i-1] - mmin)/spread);
      mask = 1-mark[j][rel_freqs-i-1];
      (*rgb)[k++] = v/**mask*/;      (*rgb)[k++] = v*mask;      (*rgb)[k++] = v/**mask*/;
    }
  }
  *cols = end-start;
  *rows = rel_freqs;
  first_frame = start;
  //  fclose(fph); exit(0);
}


void
set_vcode_rate(float rate) {
  vring.rate = rate;
}

void write_features(char *name) {
    
    FILE *fp;
    fp = fopen(name, "w");
    if (fp == NULL) { printf("can't open %s\n",name); return; }
    
    AUDIO_FEATURE af;
    int start, end, s, e, midi, frame_8k, offset, frames;
    float hz0, abs_time;
    unsigned char *ptr;
    frames = vring.audio_frames;
    fprintf(fp, "Total number of frames: %d\n", frames);
    for (int i = 0; i < frames; i++) {
        frame_8k = i * HOP_LEN / (float) (SKIPLEN * 6); //get corresponding frame index at 8k in order to map it to corresponding MIDI note
        midi = binary_search(firstnote, lastnote, frame_8k);
        if (midi == -1 || midi == -2) { //note out of bounds: -1 if before/after solo, -2 if frame is during a rest
            // should be done in some smarter ways.
            fprintf(fp,"%d\t%f\t%f\t%f\n", i, -1.0, -1.0, -1.0); //signals unusable frame
        }
        else {
            hz0 = (int) (pow(2,((midi - 69)/12.0)) * 440);
            offset = frame_8k * SKIPLEN * BYTES_PER_SAMPLE;
            af = cal_feature(audiodata+offset, hz0);
            fprintf(fp,"%d\t%f\t%f\t%f\n", i, af.hz, af.amp, hz0);
        }
        
    }
    
    fclose(fp);
}



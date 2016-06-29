#include <math.h>
#include <stdlib.h>
#include "share.c"
#include "global.h"
#include "linux.h"
#include "vocoder.h"
#include "wasapi.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>








void
float2sample(float x, unsigned char *samp) {
    unsigned char *ptr;
    int i,v;
    
    
    v =  (int) (x * (float) 0x10000);
    //  v =  (int) (x * (float) 0x8000);
    ptr = (unsigned char *) &v;
    for (i=0; i < BYTES_PER_SAMPLE; i++) samp[i] = ptr[i];
}


void
int2sample(int v, unsigned char *samp) {
    unsigned char *ptr;
    int i;
    
    ptr = (unsigned char *) &v;
    for (i=0; i < BYTES_PER_SAMPLE; i++) samp[i] = ptr[i];
}

int
sample2int(unsigned char *temp) {
    int i,tot,j,g;
    unsigned char b[BYTES_PER_SAMPLE],*ptr;
    
    
    
    tot= 0;
    ptr = (unsigned char *) &tot;
    for (j = 0; j < BYTES_PER_SAMPLE; j++)  ptr[j] = temp[j];
    if (tot & 0x8000) tot |= 0xffff0000;
    return(tot);
}




float
sample2float(unsigned char *temp) {
    int i,tot,j,g;
    unsigned char b[BYTES_PER_SAMPLE],*ptr;
    float x;
    
    
    
    tot= 0;
    ptr = (unsigned char *) &tot;
    for (j = 0; j < BYTES_PER_SAMPLE; j++)  ptr[j] = temp[j];
    if (tot & 0x8000) tot |= 0xffff0000;
    x = tot / (float) 0x10000;
    //  x = tot / (float) 0x8000;
    //  if (fabs(x) > 1) { printf(" zzzz %x %x %f\n",temp[0],temp[1],x); exit(0); }
    return(x);
}

void
samples2floats(unsigned char *audio, float *buff, int n) {
    int i;
    
#ifdef GAIN_CORRECTION
    for (i=0; i < n; i++)  buff[i] = .2*sample2float(audio + i*BYTES_PER_SAMPLE);
#else
    for (i=0; i < n; i++)  buff[i] = sample2float(audio + i*BYTES_PER_SAMPLE);
#endif
}

void
floats2samples(float *buff, unsigned char *audio, int n) {
    int i;
    
    for (i=0; i < n; i++)  float2sample(buff[i],audio+i*BYTES_PER_SAMPLE);
}

void
floats2samplesvar(float *buff, unsigned char *audio, int n) {
    int i;
    
    for (i=0; i < n; i++){
          float cur = sample2float(audio + i*BYTES_PER_SAMPLE);
          float2sample(buff[i] + cur,audio+i*BYTES_PER_SAMPLE);
      }
}


void
samples2data() {
    unsigned char *temp, b[BYTES_PER_SAMPLE];
    int i,tot,j,offset;
    // float xxx[10000];
    // int yyy[10000];
    
    
    
    
    offset =  max(( (token+1)*SKIPLEN - FRAMELEN)*BYTES_PER_SAMPLE,0);
    /* offset marks the start of the frame.  this convention is chosen so that
     
     secs2tokens(tokens2secs(n))
     
     gives a value lying in the center of the nth frame */
    
    temp =  audiodata + offset;
    //  temp =  audiodata + ((token+1)*SKIPLEN - FRAMELEN)*BYTES_PER_SAMPLE;
    
    
    /*  for (i=0; i < FRAMELEN; i++, temp+= BYTES_PER_SAMPLE) {
     //    yyy[i] = (int) temp;
     data[i] = (temp < audiodata) ? 0. :  sample2int(temp)/ (float) 0x10000;
     }*/
    
    //  if (token == 69) printf(" here is %x %x %x %f\n",audiodata[(70*SKIPLEN+256-FRAMELEN)*BYTES_PER_SAMPLE+1],temp[256*BYTES_PER_SAMPLE],temp[256*BYTES_PER_SAMPLE+1],sample2float(temp+256*BYTES_PER_SAMPLE));
    
    samples2floats(temp, data, FRAMELEN);
    temp =  orchdata + offset;
    //  temp =  orchdata + ((token+1)*SKIPLEN - FRAMELEN)*BYTES_PER_SAMPLE;
#ifdef ORCHESTRA_EXPERIMENT
    samples2floats(temp, orch_data_frame, FRAMELEN);
#endif
    //  for (i=0; i < FRAMELEN; i++) printf("%d %d %f\n",i,temp[i],orch_data_frame[i]);
    
    //  if (token == 69) for (i=0; i < FRAMELEN; i++)  printf("%d %f\n",i,data[i]);
    
}


int
read_audio(char *af) {
    FILE *fp;
    int b,i;
    
    token = -1;
    printf("reading %s\n",af);
    fp = fopen(af, "rb");
    if (fp == NULL) printf("couldn't open %s\n",af);
    b = fread(audiodata,1,MAXAUDIO,fp);
    if (b == MAXAUDIO) { printf("%s too long\n",af); exit(0); }
    fclose(fp);
    frames = b/(TOKENLEN*BYTES_PER_SAMPLE);
    printf("%d frames read\n",frames);
    return(frames);
}

void
write_audio(char *af) {
    int b,i;
    unsigned char temp[TOKENLEN];
    char  file[500],suf[2],*infop = NULL;
    unsigned ilen=0;
    FILE *fp;
    
    printf("writing %s\n",af);
    fp = fopen(af, "wb");
    fwrite(audiodata,frames*TOKENLEN,BYTES_PER_SAMPLE,fp);
    fclose(fp);
}

void create_test_audiodata() {
    float samp, f, sine, delta, v, g, t, end;
    int offset = 473 * SKIPLEN;
    end = 160000;
    for(int i = offset; i < offset + end; i++)
    {
        t = (float) (i-offset)/SR;
        samp = 0;
        f = 800;
        delta = 100;
        v = 10;
        g = 2*PI*f*(float) i/SR - (float) delta/v*cosf(2*PI*v*(float)i/SR);
        //g = 2*PI*(440* t + 440*t*t/ ((float) end/SR *2));
        for (int j = 1; j <= 3; j++) {
            samp += ((float) 0.001/j) * sinf(j*g + j*0.1);
        }
        float2sample(samp, audiodata + 2*i);
    }
    
}


int
read_target_audio() {
    int fd;
    int b,i;
    unsigned char temp[TOKENLEN];
    char  infop[50],file[500],suf[2];
    unsigned ilen;
    FILE *fp;
    
    fp = fopen("/Users/apple/Documents/Performance-View/user/audio/sanna/mozart_voi_che_sapete/mozart_voi_che_sapete.005.raw","rb");
    if (fp == NULL) printf("couldn't open %s\n",audio_file);
    
    b = fread(audiodata_target,1,MAXAUDIO,fp);
    
    if (b == MAXAUDIO) { printf("%s too long\n",audiodata_target); exit(0); }
    fclose(fp);
    
    /*  fd = open(audio_file,0);
     if (fd == -1) { printf("can't open %s\n",audio_file); return(0); }
     b = read(fd,audiodata,MAXAUDIO);
     if (b == MAXAUDIO) { printf("\n%s  > %d bytes\n",audio_file,MAXAUDIO);  exit(0); }
     close(fd);*/
    frames_target = b/(SKIPLEN*BYTES_PER_SAMPLE);
    printf("%d frames read\n",frames_target);
    return(frames_target);
}

int
readaudio() {
    int fd;
    int b,i;
    unsigned char temp[TOKENLEN];
    char  infop[50],file[500],suf[2];
    unsigned ilen;
    FILE *fp;
    
    
    token = -1;
    
#ifdef WINDOWS
    strcpy(audio_file,audio_data_dir);
    strcat(audio_file,current_examp);
    strcat(audio_file,".raw");
#else
    printf("enter the audio file (no suffix):");
    scanf("%s",audio_file);
    strcat(audio_file,".raw");
#endif
    
    printf("reading %s\n",audio_file);
    fp = fopen(audio_file,"rb");
    if (fp == NULL) printf("couldn't open %s\n",audio_file);
    
    b = fread(audiodata,1,MAXAUDIO,fp);
    
    if (b == MAXAUDIO) { printf("%s too long\n",audio_file); exit(0); }
    fclose(fp);
    if (test_audio_wave == 1) {
        create_test_audiodata();
    }
    
    /*  fd = open(audio_file,0);
     if (fd == -1) { printf("can't open %s\n",audio_file); return(0); }
     b = read(fd,audiodata,MAXAUDIO);
     if (b == MAXAUDIO) { printf("\n%s  > %d bytes\n",audio_file,MAXAUDIO);  exit(0); }
     close(fd);*/
    frames = b/(SKIPLEN*BYTES_PER_SAMPLE);
    printf("%d frames read\n",frames);
    
    //read_target_audio();
    return(frames);
}



int
read_current_audio() {
    int fd;
    int b,i;
    unsigned char temp[TOKENLEN];
    char  infop[50],file[200],suf[2];
    unsigned ilen;
    FILE *fp;
    
    
    
    
    token = -1;
    fp = fopen(audio_file,"rb");
    if (fp == NULL) printf("couldn't open %s\n",file);
    b = fread(audiodata,1,MAXAUDIO,fp);
    
    if (b == MAXAUDIO) { printf("%s too long\n",file); exit(0); }
    fclose(fp);
    frames = b/(SKIPLEN*BYTES_PER_SAMPLE);
    printf("%d frames read\n",frames);
    return(frames);
}





int
old_readaudio() {
    int fd;
    int b,i;
    unsigned char temp[TOKENLEN];
    char  infop[50],file[500],suf[2];
    unsigned ilen;
    
    token = -1;
    printf("enter the audio file (no suffix):");
    scanf("%s",audio_file);
    strcat(audio_file,".raw");
    fd = open(audio_file,0);
    if (fd == -1) { printf("can't open %s\n",audio_file); return(0); }
    b = read(fd,audiodata,MAXAUDIO);
    if (b == MAXAUDIO) { printf("\n%s  > %d bytes\n",audio_file,MAXAUDIO);  exit(0); }
    close(fd);
    frames = b/(SKIPLEN*BYTES_PER_SAMPLE);
    return(frames);
}



void
play_audio_buffer_one_channel(int first_frame)  {
    int b,i,j,k,arg,status,total,t,fd,l,h;
    unsigned char temp[IO_CHANNELS*BYTES_PER_FRAME*6],*ptr;
    
    prepare_playing(NOMINAL_OUT_SR);
    init_clock();
    total = frames*BYTES_PER_FRAME;
    t= first_frame*SKIPLEN*BYTES_PER_SAMPLE;
    //  t = 3500*BYTES_PER_FRAME;  /* to start in middle */
    while (t < total) {
        ptr = temp;
        for (i=0; i < TOKENLEN; i++)
            for (k=0; k < IO_CHANNELS*6; k++)
                for (j=0; j < BYTES_PER_SAMPLE; j++)
                    *ptr++ = audiodata[t + i*BYTES_PER_SAMPLE + j];
        //    b = write(Audio_fd, temp, BYTES_PER_FRAME);
        b = write_samples(temp, TOKENLEN*6);
        
        
        if (b > 0) t +=IO_CHANNELS*b/6;
        //   b = write(Audio_fd, audiodata+t, BYTES_PER_FRAME);
        //   if (b > 0) t += b;
    }
    while (system_secs() < token2secs(frames));
    /*  status =  ioctl(Audio_fd, SOUND_PCM_SYNC,0);  */
    end_playing();
}



void
combine_parts() {
    char name[200],auraw[200],orch[200],out[200];
    FILE *fp;
    int i;
    float x;
    
    printf("enter the full name of the file (no ext) having both .raw and .ork extenstions\n");
    scanf("%s",name);
    strcpy(auraw,name);
    strcat(auraw,".raw");
    strcpy(audio_file,auraw);
    read_audio(auraw);
    read_orchestra_audio();
    printf("enter the full name of the stereo output file\n");
    scanf("%s",out);
    fp = fopen(out,"w");
    for (i=0; i < frames*FRAMELEN; i++) {
        fwrite(audiodata + i*BYTES_PER_SAMPLE,BYTES_PER_SAMPLE,1,fp);
        x = sample2float(orchdata + i*BYTES_PER_SAMPLE);
        x *= .3;
        float2sample(x,orchdata + i*BYTES_PER_SAMPLE);
        fwrite(orchdata + i*BYTES_PER_SAMPLE,BYTES_PER_SAMPLE,1,fp);
    }
    fclose(fp);
}

void
write_wav(char *af, short *buff, int samples,int channels) {
    FILE *fp;
    int b,glob = 16,pcm_format = 1,sr = 48000;
    int bytes_per_sec, block_align, bits_per_sample,data,fs;
    
    bytes_per_sec = channels*sr*2;
    block_align=2*channels;
    bits_per_sample = 16;
    
    
    
    data = 2*channels*samples;
    fs = data+36;
    printf("writing %s\n",af);
    fp = fopen(af, "wb");
    if (fp == NULL) { printf("can't open %s\n",af); return; }
    fprintf(fp,"%s","RIFF");
    fwrite(&fs,4,1,fp);
    fprintf(fp,"%s","WAVE");
    fprintf(fp,"%s","fmt ");
    fwrite(&glob,4,1,fp);	/* length of what follows */
    fwrite(&pcm_format,2,1,fp);
    fwrite(&channels,2,1,fp);
    fwrite(&sr,4,1,fp);
    fwrite(&bytes_per_sec,4,1,fp);
    fwrite(&block_align,2,1,fp);
    fwrite(&bits_per_sample,2,1,fp);
    fprintf(fp,"%s","data");
    fwrite(&data,4,1,fp);
    b = fwrite(buff,1,data,fp);
    fclose(fp);
}



/*static void
 write_wav(char *af) {
 FILE *fp;
 int b,glob = 16,pcm_format = 1,channels = 1,sr = OUTPUT_SR;
 int bytes_per_sec, block_align, bits_per_sample,data,fs;
 
 
 bytes_per_sec = channels*OUTPUT_SR*2;
 block_align=2*channels;
 bits_per_sample = 16;
 
 
 
 data = frames*TOKENLEN;
 fs = data+36;
 printf("writing %s\n",af);
 fp = fopen(af, "w");
 if (fp == NULL) { printf("can't open %s\n",af); return(0); }
 fprintf(fp,"%s","RIFF");
 fwrite(&fs,4,1,fp);
 fprintf(fp,"%s","WAVE");
 fprintf(fp,"%s","fmt ");
 fwrite(&glob,4,1,fp);	// length of what follows
 fwrite(&pcm_format,2,1,fp);
 fwrite(&channels,2,1,fp);
 fwrite(&sr,4,1,fp);
 fwrite(&bytes_per_sec,4,1,fp);
 fwrite(&block_align,2,1,fp);
 fwrite(&bits_per_sample,2,1,fp);
 fprintf(fp,"%s","data");
 fwrite(&data,4,1,fp);
 b = fwrite(audiodata,1,frames*TOKENLEN,fp);
 fclose(fp);
 }   */

int
read_wav(char *af, unsigned char *adata) {
    FILE *fp;
    int b,glob = 16,pcm_format = 1,channels = 1,sr = OUTPUT_SR;
    int bytes_per_sec, block_align, bits_per_sample,fs,len,f;
    unsigned int data;
    char str[500];
    
    
    
    fp = fopen(af, "rb");
    if (fp == NULL) { printf("couldn't open %s\n",af); exit(0); }
    fscanf(fp,"RIFF");
    fread(&fs,4,1,fp);
    data = fs-36;
    fscanf(fp,"WAVE");
    fscanf(fp,"fmt ");
    fread(&glob,4,1,fp);	/* length of what follows */
    fread(&pcm_format,2,1,fp);
    fread(&channels,2,1,fp);
    fread(&sr,4,1,fp);
    fread(&bytes_per_sec,4,1,fp);
    fread(&block_align,2,1,fp);
    bits_per_sample = 0;
    fread(&bits_per_sample,2,1,fp);
    fread(str,4,1,fp);
    str[4] = 0;
    //  printf("str = %s\n",str);
    len = 0;
    fread(&len,2,1,fp);
    //  printf("len = %d\n",len);
    fread(adata,len+2,1,fp);  // i don't know why + 2
    fread(str,4,1,fp);
    str[4] = 0;
    //  printf("str = %s\n",str);
    //  fscanf(fp,"data");
    
    /*data = 0;          this didn't work on wave file from audacity
     fread(&data,4,1,fp);   */
    
    fseek(fp, 0L, SEEK_END);
    data = ftell (fp) - 48;
    fseek(fp, 0L, SEEK_SET);
    fread(adata,1,48,fp);  // get rid of header
    
    
    
    b = fread(adata,1,data,fp);
    
    f = data/(IO_CHANNELS*IO_FACTOR*BYTES_PER_FRAME);
    
    // these are the units written in play_background_frame
    
    
    //  printf("bps = %d bps = %d channels = %d sr = %d bytes = %d\n",bytes_per_sec,bits_per_sample,channels,sr,data);
    fclose(fp);
    return(f);
}

void
triple_mix(float c[], float *o1, float *o2) {
    int i;
    
    *o1 = *o2 = 0;
    for (i=0; i < SIMPLE_MIXES; i++) {
        *o2 += c[i]*simple_mix[i].volume*simple_mix[i].pan*(1-simple_mix[i].mute);
        *o1 += c[i]*simple_mix[i].volume*(1-simple_mix[i].pan)*(1-simple_mix[i].mute);
        
    }
}

void
n_mix(float c[], float *o1, float *o2, int n) {  // n <= SIMPLE_MIXES
    int i;
    
    *o1 = *o2 = 0;
    for (i=0; i < n; i++) {
        *o2 += c[i]*simple_mix[i].volume*simple_mix[i].pan*(1-simple_mix[i].mute);
        *o1 += c[i]*simple_mix[i].volume*(1-simple_mix[i].pan)*(1-simple_mix[i].mute);
        
    }
}

void
mix_channels(float c1, float c2, float *o1, float *o2) {
    float p1,p2,v1,v2,mx,px,b1,b2;
    
    
    
    p1 = mixlev+(pan_value-.5);  /* this needs to be rethought */
    p2 = mixlev-(pan_value-.5);
    mx = max(mixlev,1-mixlev);
    b1 = master_volume*(mixlev/mx)*c1;   // the balanced levels
    b2 = master_volume*((1-mixlev)/mx)*c2;
    
    *o1 = pan_value*b1 + (1-pan_value)*b2;
    *o2 = (1-pan_value)*b1 + pan_value*b2;
    
    return;
    
    px = max(pan_value,1-pan_value);
    *o1 = (pan_value/px)*(mixlev/mx)*c1;
    *o2 = ((1-pan_value)/px)*((1-mixlev)/mx)*c2;
    //  *o1 = p1*c1 + (1-p1)*c2;
    //  *o2 = p2*c1 + (1-p2)*c2;
}


void
float_mix(float *s1, float *s2, float *o1, float *o2, int num) {
    int i;
    
    for (i=0; i < num; i++)  mix_channels(s1[i], s2[i], o1+i, o2+i);
}

void
triple_float_mix(float *s1, float *s2, float *s3, float *o1, float *o2, int num) {
    int i;
    float m[3];
    
    for (i=0; i < num; i++)  {
        m[0] = s1[i]; m[1] = s2[i]; m[2] = s3[i];
        // triple_mix(m,o1+i,o2+i);
        n_mix(m,o1+i,o2+i,SIMPLE_MIXES); // mix all three channels
        
    }
}



static void
make_file_name(char *dir, char *file, char *suff, char *dest) {
    
    // strcpy(dest,user_dir);  // assuming everything is in the user dir
    strcpy(dest,dir);
    strcat(dest,file);
    strcat(dest,".");
    strcat(dest,suff);
}


static int
hi_res_ready() {
    char name[200],solo[500],orch[500],out[500];
    FILE *fp;
    
    make_file_name(audio_data_dir,current_examp,"48k",name);
    fp = fopen(name,"rb");
    if (fp == NULL) return(0);
    fclose(fp);
    
    make_file_name(audio_data_dir,current_examp,"48o",name);
    fp = fopen(name,"rb");
    if (fp == NULL) return(0);
    fclose(fp);
    
    return(1);
    
}

lo_res_ready() {
    char name[200],solo[500],orch[500],out[500];
    FILE *fp;
    
    make_file_name(audio_data_dir,current_examp,"raw",name);
    fp = fopen(name,"rb");
    if (fp == NULL) return(0);
    fclose(fp);
    
    make_file_name(audio_data_dir,current_examp,"ork",name);
    fp = fopen(name,"rb");
    if (fp == NULL) return(0);
    fclose(fp);
    
    return(1);
    
}



int
create_wav() {
    int bytes_per_sec, block_align, bits_per_sample,data,fs,sr,samps_per_frame,i;
    FILE *fpo,*fps,*fpm;
    char name[500],solo[500],orch[500],out[500],suff[20],temp[500];
    int b,glob = 16,pcm_format = 1,channels = 2;
    unsigned char ssamp[BYTES_PER_SAMPLE],osamp[BYTES_PER_SAMPLE];
    float f1,f2,o1,o2,c[2];
    
    
    //  printf("output file %s\n",background_info.name);
    if (hi_res_ready()) sr =  OUTPUT_SR;
    else if (lo_res_ready()) sr = NOMINAL_SR;
    else { printf("coulnd't create wav from %s %s\n", audio_data_dir,current_examp); return(0); }
    
    printf("sr = %d\n",sr);
    bytes_per_sec = channels*sr*2;
    block_align=2*channels;
    bits_per_sample = 16;
    samps_per_frame = SKIPLEN*sr/NOMINAL_SR;
    data = frames*samps_per_frame*channels*BYTES_PER_SAMPLE;
    fs = data+36;
    
    make_file_name(audio_data_dir,current_examp,(sr == OUTPUT_SR) ? "48o" : "ork",orch);
    fpo = fopen(orch,"rb");
    if (fpo == NULL) { printf("couldn't open %s\n",orch);  return(0); }
    
    make_file_name(audio_data_dir,current_examp,(sr == OUTPUT_SR) ? "48k" : "raw",solo);
    fps = fopen(solo,"rb");
    if (fps == NULL) { printf("couldn't open %s\n",solo);  return(0); }
    
    //  strcpy(temp,background_info.name);
    make_file_name(background_info.name,current_examp,"wav",out);
    // make_file_name(background_info.name ,"","wav",out);
    printf("the output wave file is %s\n",out);
    
    // fpm = fopen(out,"wb");
    fpm = fopen(background_info.name,"wb");
    if (fpm == NULL) { perror(background_info.name); printf("couldn't open %s\n",out);  return(0); }
    
    fprintf(fpm,"%s","RIFF");
    fwrite(&fs,4,1,fpm);
    fprintf(fpm,"%s","WAVE");
    fprintf(fpm,"%s","fmt ");
    fwrite(&glob,4,1,fpm);	/* length of what follows */
    fwrite(&pcm_format,2,1,fpm);
    fwrite(&channels,2,1,fpm);
    fwrite(&sr,4,1,fpm);
    fwrite(&bytes_per_sec,4,1,fpm);
    fwrite(&block_align,2,1,fpm);
    fwrite(&bits_per_sample,2,1,fpm);
    fprintf(fpm,"%s","data");
    fwrite(&data,4,1,fpm);
    
    for (i=0; i < frames*samps_per_frame; i++) {
        background_info.fraction_done = (i+1) / (float)(frames*samps_per_frame);
        fread(ssamp,BYTES_PER_SAMPLE,1,fps);
        fread(osamp,BYTES_PER_SAMPLE,1,fpo);
        c[0] = f1 = sample2float(ssamp);
        c[1] = f2 = sample2float(osamp);
        //mix_channels(f1,f2,&o1,&o2);
        n_mix(c,&o1,&o2,2); // mix two channels solo and acc
        float2sample(o1,ssamp);
        float2sample(o2,osamp);
        /*    float2sample(mixlev*f1+(1-mixlev)*f2,ssamp);
         float2sample(mixlev*f1+(1-mixlev)*f2,osamp); */
        fwrite(&ssamp,BYTES_PER_SAMPLE,1,fpm);
        fwrite(&osamp,BYTES_PER_SAMPLE,1,fpm);
    }
    fclose(fpo);
    fclose(fps);
    fclose(fpm);
    return(0);
    
}


void
combine_hires_parts() {
    FILE *fpo,*fps,*fpm;
    char name[200],solo[500],orch[500],out[500];
    unsigned char ssamp[BYTES_PER_SAMPLE],osamp[BYTES_PER_SAMPLE];
    int i;
    int b,glob = 16,pcm_format = 1,channels = 2,sr = OUTPUT_SR;
    int bytes_per_sec, block_align, bits_per_sample,data,fs;
    float f1,f2,o1,o2;
    
    
    bytes_per_sec = channels*OUTPUT_SR*2;
    block_align=2*channels;
    bits_per_sample = 16;
    data = frames*SKIPLEN*IO_FACTOR*channels*BYTES_PER_SAMPLE;
    fs = data+36;
    
    
    
    strcpy(solo,audio_data_dir);
    strcat(solo,current_examp);
    strcat(solo,".");
    strcat(solo,"48k");
    fps = fopen(solo,"rb");
    if (fps == NULL) { printf("couldn't open %s\n",solo); exit(0); }
    
    strcpy(orch,audio_data_dir);
    strcat(orch,current_examp);
    strcat(orch,".");
    strcat(orch,"48o");
    fpo = fopen(orch,"rb");
    if (fpo == NULL) { printf("couldn't open %s\n",orch);  exit(0); }
    
    strcpy(out,audio_data_dir);
    strcat(out,current_examp);
    strcat(out,".");
    strcat(out,"wav");
    fpm = fopen(out,"wb");
    if (fpm == NULL) { printf("couldn't open %s\n",out);  exit(0); }
    
    
    fprintf(fpm,"%s","RIFF");
    fwrite(&fs,4,1,fpm);
    fprintf(fpm,"%s","WAVE");
    fprintf(fpm,"%s","fmt ");
    fwrite(&glob,4,1,fpm);	/* length of what follows */
    fwrite(&pcm_format,2,1,fpm);
    fwrite(&channels,2,1,fpm);
    fwrite(&sr,4,1,fpm);
    fwrite(&bytes_per_sec,4,1,fpm);
    fwrite(&block_align,2,1,fpm);
    fwrite(&bits_per_sample,2,1,fpm);
    fprintf(fpm,"%s","data");
    fwrite(&data,4,1,fpm);
    
    
    
    
    
    for (i=0; i < frames*SKIPLEN*IO_FACTOR; i++) {
        fread(ssamp,BYTES_PER_SAMPLE,1,fps);
        fread(osamp,BYTES_PER_SAMPLE,1,fpo);
        f1 = sample2float(ssamp);
        f2 = sample2float(osamp);
        mix_channels(f1,f2,&o1,&o2);
        float2sample(o1,ssamp);
        float2sample(o2,osamp);
        /*    float2sample(mixlev*f1+(1-mixlev)*f2,ssamp);
         float2sample(mixlev*f1+(1-mixlev)*f2,osamp); */
        fwrite(&ssamp,BYTES_PER_SAMPLE,1,fpm);
        fwrite(&osamp,BYTES_PER_SAMPLE,1,fpm);
    }
    fclose(fpo);
    fclose(fps);
    fclose(fpm);
}




void
downsample_48k_solo() {
    char solo[500];
    FILE *fps;
    unsigned char buff[SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE],lbuff[SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE],*target;
    unsigned char temp[10000];
    int i,j,result;
    
    //  audiodata += 14;
    //  return;
    
    
    strcpy(solo,audio_data_dir);
    strcat(solo,current_examp);
    strcat(solo,".");
    strcat(solo,"48k");
    fps = fopen(solo,"rb");
    if (fps == NULL) { printf("couldn't open %s\n",solo); exit(0); }
    for (j=0; j < SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE; j++) lbuff[j] = 0;
    
    for (i=0; i < frames; i++) {
        fread(buff,1,SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE,fps);
        target = audiodata+i*SKIPLEN*BYTES_PER_SAMPLE;
        //    filtered_downsample(lbuff,buff,target);
        filtered_downsample(buff,target,SKIPLEN*IO_FACTOR,i,&result);
        if (result != SKIPLEN) { printf("error in using filtered_downsample\n"); exit(0); }
        //        for (j=0; j < 20; j++) printf("%d %f\n",j,sample2float(target+2*j));
        //    local_downsample_audio(buff,audiodata+i*SKIPLEN*BYTES_PER_SAMPLE,IO_FACTOR);
        //    float_local_downsample_audio(buff,temp,IO_FACTOR);
        //                for (j=0; j < 20; j++) printf("%d %f\n",j,sample2float(target+2*j));
        //    if (fabs(sample2float(temp)- sample2float(target+4)) > .0001) {printf("not equal at frame %d (%f %f)\n",i,sample2float(temp),sample2float(target+4)); exit(0); }
        for (j=0; j < SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE; j++) lbuff[j] = buff[j];
    }
}



int stop_recording_flag;


void
record_in_background_action(int num) {
    void exec_on_timer_expire();
    float t;
    
    
    
    //    printf("time = %f flag = %d\n",system_secs(),stop_recording_flag);
    if (stop_recording_flag) return;
    wait_for_samples();
    token++;
    //    if (token % 1 == 0) printf("token = %d now = %f late in tokens = %f\n",token,system_secs(),system_secs()*SR/(float)TOKENLEN - token);
    //    if (rand() % 100 == 0) printf("token = %d now = %f late in tokens = %f\n",token,system_secs(),system_secs()*SR/(float)TOKENLEN - token);
    t = token2secs(token+1) - system_secs();
    if (t < 0) t = .0001;
    //  queue_midi_event(t);
    set_timer_signal(t);
}


static void
start_record_in_background() {
    int prepare_sampling();
    float t;
    
    token = 0;
    stop_recording_flag = 0;
    /*  begin_sampling(); */
    prepare_sampling();
    init_clock();
    begin_sampling();
    //  init_timer_record_in_background();
    install_signal_action(record_in_background_action);
    t = token2secs(1) - system_secs();
    //  queue_midi_event(token2secs(1) - system_secs());
    set_timer_signal(token2secs(1) - system_secs());
    
}




void
record_in_background() {
    char b,c,name[300];
    int i;
    
    printf("start playing\n");
    printf("'x<RET>' to stop\n");
    start_record_in_background();
    do {
        c = getchar();
        //        printf("%f\n",system_secs());
        pause();
        //        printf("after pause %f\n",system_secs());
    } while (c != 'x') ;
    stop_recording_flag = 1;
    printf("recorded %d frames\n",token);
    scanf("%s",name);  /* stuff left in stdin ???*/
    printf("full name of audio file\n");
    scanf("%s",name);
    printf("name = %s\n",name);
    frames = token; /* ugh*/
    write_audio(name);
}





void
set_sine_wave(char *ptr, float f, int sr, float secs) {  // stereo !!
    int i;
    for (i=0; i < (int) (sr*secs); i++) {
        //    printf("%f\n",.1*sin(440*i/(float) SR));
        float2sample(.1 *sin(2*PI*f*i/(float) sr),ptr + 2*i*BYTES_PER_SAMPLE);
        float2sample(.1 *sin(2*PI*f*i/(float) sr),ptr + (2*i+1)*BYTES_PER_SAMPLE);
        //    printf("%f\n",.1 *sin(2*PI*f*i/(float) sr));
        //    float2sample(.5*sin(2*PI*440*i/(float) SR),ptr + i*BYTES_PER_SAMPLE);
        //   float2sample(.5*(i%10 == 0),audiodata + i*BYTES_PER_SAMPLE);
    }
    //  frames = 2000;
}

void
set_white_noise(char *ptr, int sr, float secs) {  // stereo !!
    int i;
    double drand48();
    
    for (i=0; i < (int) (sr*secs); i++) {
        //    printf("%f\n",.1*sin(440*i/(float) SR));
        float2sample(.1 *(rand()/(float)RAND_MAX-.5),ptr + 2*i*BYTES_PER_SAMPLE);
        float2sample(.1 *(rand() / (float)RAND_MAX - .5),ptr + (2*i+1)*BYTES_PER_SAMPLE);
        //    printf("%f\n",.1 *sin(2*PI*f*i/(float) sr));
        //    float2sample(.5*sin(2*PI*440*i/(float) SR),ptr + i*BYTES_PER_SAMPLE);
        //   float2sample(.5*(i%10 == 0),audiodata + i*BYTES_PER_SAMPLE);
    }
    //  frames = 2000;
}

void
set_silence(char *ptr, int sr, float secs) {  // stereo !!
    int i;
    // float drand48();
    
    for (i=0; i < (int) (sr*secs); i++) {
        //    printf("%f\n",.1*sin(440*i/(float) SR));
        float2sample(0.,ptr + 2*i*BYTES_PER_SAMPLE);
        float2sample(0.,ptr + (2*i+1)*BYTES_PER_SAMPLE);
    }
}


void
set_clicks(char *ptr, int sr, int secs) {
    int i;
    float x,s;
    
    for (i=0; i < (int) (sr*secs); i++) {
        s = .3*sin(2*PI*440*i/ (float) sr);
        x = ((i % sr) < 50) ?  1 : 0;
        float2sample(x*s,ptr + i*BYTES_PER_SAMPLE);
        //    printf("%f\n",.1 *sin(2*PI*f*i/(float) sr));
        //    float2sample(.5*sin(2*PI*440*i/(float) SR),ptr + i*BYTES_PER_SAMPLE);
        //   float2sample(.5*(i%10 == 0),audiodata + i*BYTES_PER_SAMPLE);
    }
    //  frames = 2000;
}



int
index2micvol(int vq) {
    
    if (vq < 0 || vq >= VOL_TEST_QUANTA) {
        printf("bad input to index2micvol %d\n",vq); exit(0);
    }
    return(vol_test_levels[vq]);
    //  return((vq+1)*100/VOL_QUANTA);
}


int
micvol2index(int vol) {
    int i;
    
    for (i=0; i < VOL_TEST_QUANTA; i++)
        if (vol_test_levels[i] >= vol) return(i);
    printf("couldn't convert vol %d\n",vol);
    exit(0);
    /*
     i = VOL_QUANTA*vol/100 - 1;
     if (i < 0 || i >= VOL_QUANTA) {
     printf("bad input to index2micvol\n"); exit(0);
     }
     return(i); */
}

void
write_mic_level(int v) {
    char file[500];
    FILE *fp;
    
    strcpy(file,user_dir);
    strcat(file,AUDIO_DIR);
    strcat(file,"/");
    strcat(file,player);
    strcat(file,"/");
    strcat(file,"mic_level.dat");
    fp = fopen(file,"w");
    fprintf(fp,"miclevel = %d\n",v);
    fclose(fp);
    printf("created %s\n",file);
}

int
read_mic_level() {
    char file[500];
    FILE *fp;
    int v;
    
    strcpy(file,user_dir);
    strcat(file,AUDIO_DIR);
    strcat(file,"/");
    strcat(file,player);
    strcat(file,"/");
    strcat(file,"mic_level.dat");
    fp = fopen(file,"r");
    if (fp == NULL) return(0);
    fscanf(fp,"miclevel = %d",&mic_vol);
    printf("mic_vol set to %d\n",mic_vol);
    fclose(fp);
    return(1);
    
}

#define MAX_RECORD_MEAS 1000

typedef struct {
    int num;
    float time[MAX_RECORD_MEAS];
    float count[MAX_RECORD_MEAS];
} SAMPLE_MEAS;

static SAMPLE_MEAS  rec_stat;
static SAMPLE_MEAS  pla_stat;

void
init_sample_meas() {
    rec_stat.num = 0;
    pla_stat.num = 0;
}

void
make_sample_meas() {
    if (rec_stat.num >=  MAX_RECORD_MEAS) return;
    rec_stat.time[rec_stat.num] = system_secs();
    rec_stat.count[rec_stat.num] = (float) samples_recorded();
    rec_stat.num++;
    
    if (pla_stat.num >=  MAX_RECORD_MEAS) return;
    pla_stat.time[pla_stat.num] = system_secs();
    pla_stat.count[pla_stat.num] = (float) samples_played();
    pla_stat.num++;
}


static float
regression_beta(SAMPLE_MEAS sm) {
    int i,f,samps_per_call;
    float xbar,ybar,Sxx,Sxy,slope_hat,sr_hat,sanity,diff;
    
    Sxx = Sxy = xbar = ybar = 0;
    for (i=0; i < sm.num; i++) {
        xbar += sm.time[i];
        ybar += sm.count[i];
    }
    xbar /= sm.num;
    ybar /= sm.num;
    for (i=0; i < sm.num; i++) {
        Sxx += (sm.time[i]-xbar)*(sm.time[i]-xbar);
        Sxy += (sm.time[i]-xbar)*(sm.count[i]-ybar);
    }
    slope_hat = Sxy/Sxx;
    /*
     samps_per_call = RING_EL_SIZE/(BYTES_PER_SAMPLE*IO_CHANNELS);
     diff = callib_times[completed_frames-1] - callib_times[0];
     sanity = diff/(completed_frames-1);
     sr_hat = samps_per_call/slope_hat ;
     sr_hat = samps_per_call/sanity;
     printf("slope = %f sanity = %f sr_hat = %f %f\n",slope_hat,sanity,samps_per_call/slope_hat,samps_per_call/sanity);
     */
    return(slope_hat);
    
}

void
analyze_sample_meas() {
    int i;
    //    for (i=0; i < rec_stat.num; i++)
    //    printf("%d %f %f\n",i,rec_stat.time[i],rec_stat.count[i]);
    printf("rec rate est = %f\n",regression_beta(rec_stat));
    printf("play rate est = %f\n",regression_beta(pla_stat));
    sound_info.input_sr = regression_beta(rec_stat);
    sound_info.output_sr = regression_beta(pla_stat);
}



void
downsample_audio(unsigned char *fr, unsigned char *to, int samps, int factor) {
    int i,j;
    long int ave, s;
    
    for (i=0; i < samps/factor; i++, fr += BYTES_PER_SAMPLE*factor) {
        for (j = 0, ave=0; j < factor; j++)
            ave += sample2int(fr + BYTES_PER_SAMPLE*j);
        s = ave / factor;
        int2sample(s,to + BYTES_PER_SAMPLE*i);
    }
}



void
downsample_audio_to_mono(unsigned char *fr, unsigned char *to, int samps, int factor) {
    int i,j;
    long int ave, s;
    
    for (i=0; i < samps/factor; i++, fr += 2*BYTES_PER_SAMPLE*factor) {
        for (j = 0, ave=0; j < factor; j++)
            ave += sample2int(fr + 2*BYTES_PER_SAMPLE*j);
        s = ave / factor;
        int2sample(s,to + BYTES_PER_SAMPLE*i);
    }
}






void
write_sound_info() {
    char file[500];
    FILE *fp;
    int i;
    
    // strcpy(file,user_dir);
    //  strcat(file,AUDIO_DIR);
    strcpy(file,MISC_DIR);
    strcat(file,"/");
    strcat(file,SOUND_INFO);
    fp = fopen(file,"wb");
    fwrite(&sound_info,sizeof(SOUND_INFO),1,fp);
    fclose(fp);
}

int
read_sound_info() {
    char file[500];
    FILE *fp;
    int i,l;
    
    sound_info.output_sr = sound_info.input_sr = 48000.;
    // strcpy(file,user_dir);
    //  strcat(file,AUDIO_DIR);
    strcpy(file,MISC_DIR);
    strcat(file,"/");
    strcat(file,SOUND_INFO);
    fp = fopen(file,"rb");
    if (fp == NULL) {
        printf("couldn't read %s --- using defaults\n",file);
        fclose(fp);
        return(0);
    }
    fread(&sound_info,sizeof(SOUND_INFO),1,fp);
    fclose(fp);
    printf("output sr = %f input sr = %f\n",sound_info.output_sr,sound_info.input_sr);
    //  printf("input sr = %f\n",sound_info.input_sr);
    
    return(1);
}



void
old_upsample_audio(unsigned char *fr, unsigned char *to, int samps, int factor, int sample_bytes) {
    int i,j;
    
    for (i=0; i < samps; i++, fr += sample_bytes)
        for (j = 0; j < factor; j++,to+=sample_bytes)
            memcpy(to,fr,sample_bytes);
}




void
upsample_audio(unsigned char *fr, unsigned char *to, int samps, int factor, int channels) {
    int i,j,k;
    float p,q,s1,s2,s;
    
    for (i=0; i < samps; i++, fr += channels*BYTES_PER_SAMPLE) {
        for (j = 0; j < factor; j++,to+=channels*BYTES_PER_SAMPLE) {
            p = j/(float) factor;
            q = 1-p;
            for (k=0; k < channels; k++) {
                s1 = sample2float(fr + k*BYTES_PER_SAMPLE);
                s2 = sample2float(fr + (channels+k)*BYTES_PER_SAMPLE);
                //	if (i == samps-1) s2 = s1;
                s = q*s1 + p*s2;
                //printf("%d %f %f %f\n",j,s1,s2,s);
                float2sample(s, to + k*BYTES_PER_SAMPLE);
            }
        }
    }
}




float
max_audio_val() {
    int i;
    float m=0;
    
    for (i=0; i < FRAMELEN; i++) if (fabs(data[i]) > m) m = fabs(data[i]);
    return(m);
}


float
audio_power() {
    int i;
    float m=0,sum=0;;
    
    for (i=0; i < FRAMELEN; i++) sum += data[i]*data[i];
    return(sum/FRAMELEN);
}




void
mix_mono_channels(unsigned char *left, unsigned char *right, unsigned char *mix, int n, float p1, float p2) {
    int i,j,one_channel;
    float c1,c2,o1,o2;
    unsigned char s1[BYTES_PER_SAMPLE],s2[BYTES_PER_SAMPLE];
    
    for (i=0; i < n; i++) {
        c1 = sample2float(left + i*BYTES_PER_SAMPLE);
        c2 = sample2float(right + i*BYTES_PER_SAMPLE);
        /*    o1 = p1*c1 + (1-p1)*c2;
         o2 = p2*c1 + (1-p2)*c2; */
        mix_channels(c1,c2,&o1,&o2);
        float2sample(o1,s1);
        float2sample(o2,s2);
        for (j=0; j < BYTES_PER_SAMPLE; j++) {
            mix[2*i*BYTES_PER_SAMPLE + j] = s1[j];
            mix[(2*i+1)*BYTES_PER_SAMPLE + j] = s2[j];
        }
    }
}


void
add_channels(float *in1, float *in2, float *out, int n) {
    int i;
    
    for (i=0; i < n; i++) out[i] = in1[i] + in2[i];
}


void
interleave_channels(unsigned char *left, unsigned char *rite, unsigned char *inter, int n) {
    int i,j,l,r,m;
    
    for (i=l=r=m=0; i < n; i++) {
        for (j=0; j < BYTES_PER_SAMPLE; j++) inter[m++] = left[l++];
        for (j=0; j < BYTES_PER_SAMPLE; j++) inter[m++] = rite[r++];
    }
}

void
mix_single_channel(unsigned char *left, unsigned char *mix, int n) {
    int i,j,one_channel;
    float c1,c2,o1,o2;
    unsigned char l[BYTES_PER_SAMPLE],r[BYTES_PER_SAMPLE];
    
    for (i=0; i < n; i++) {
        c2 = sample2float(left + i*BYTES_PER_SAMPLE);
        c1 = 0;
        /*    o1 = p1*c1 + (1-p1)*c2;
         o2 = p2*c1 + (1-p2)*c2; */
        mix_channels(c1,c2,&o1,&o2);
        float2sample(o1,l);
        float2sample(o2,r);
        for (j=0; j < BYTES_PER_SAMPLE; j++) {
            mix[2*i*BYTES_PER_SAMPLE + j] = l[j];
            mix[(2*i+1)*BYTES_PER_SAMPLE + j] = r[j];
        }
    }
}



#define DEFAULT_MASTER_VOLUME .5
#define DEFAULT_PAN_VALUE .5
#define DEFAULT_MIXLEV .5
#define DEFAULT_DELAY_SECONDS 0.
#define DEFAULT_IS_HIRES 1
#define DEFAULT_ORCH_PITCH 440.
#define DEFAULT_SPEAKERS 1
#define DEFAULT_PLUCK 1


void
reset_orch_pitch() {
    set_orch_pitch(DEFAULT_ORCH_PITCH);
}

#define MIX_FILE "parms.mix"

void
get_mix() {
    FILE *fp;
    char name[200];
    float op,v,b,p,lev;
    int i,aos,ih,d,pluck;
    
    strcpy(name,user_dir);
    strcat(name,MIX_FILE);
    //  get_player_file_name(name,"mix");
    for (i=0; i < NUM_MIXES; i++) {  // initializaitons ...
        audio_mix[i].volume = DEFAULT_MASTER_VOLUME;
        audio_mix[i].pan = DEFAULT_PAN_VALUE;
        audio_mix[i].balance = DEFAULT_MIXLEV;
    }
    set_orch_pitch(DEFAULT_ORCH_PITCH);
    accomp_on_speakers = DEFAULT_SPEAKERS;
    is_hires =  DEFAULT_IS_HIRES;
    delay_seconds = DEFAULT_DELAY_SECONDS;
    use_pluck = DEFAULT_PLUCK;
    mic_input_level = 1.;
    fp = fopen(name,"r");
    if (fp == NULL) return;  // this is not a problem
    for (i=0; i < NUM_MIXES; i++) {
        fscanf(fp,"%f %f %f",&v,&b,&p);
        if (v > 0 && v < 2) audio_mix[i].volume = v;
        if (b >= 0 && b <= 1) audio_mix[i].balance = b;
        if (p >= 0 && p <= 1) audio_mix[i].pan = p;
        //    fscanf(fp,"%f %f %f",&audio_mix[i].volume,&audio_mix[i].balance,&audio_mix[i].pan);
    }
    //  fscanf(fp,"%f %d %d %d %d", &op,&accomp_on_speakers,&is_hires,&delay_seconds,&use_pluck);
    fscanf(fp,"%f %d %d %d %d", &op,&aos,&ih,&d,&pluck); 
    if (op > 420 && op < 470) set_orch_pitch(op);
    if (aos == 0 || aos == 1) accomp_on_speakers = aos;
    if (ih == 0 || ih == 1) is_hires = ih;
    if (d >= 0 && d < 20) delay_seconds = d;
    if (pluck == 0 || pluck == 1) use_pluck = pluck;
    
    fscanf(fp,"%f", &lev);
    if (lev > 0 && lev <= 1) mic_input_level = lev;
    //  fscanf(fp,"%f", &mic_input_level);
    
    /*  fscanf(fp,"%f %f %f %f %d %d %d",
     &op,&master_volume,&pan_value,&mixlev,&accomp_on_speakers,
     &is_hires,&delay_seconds); */
    //  set_orch_pitch(op);
    //  printf("just read pitch = %f speakers = %d\n",get_orch_pitch(),accomp_on_speakers);
    fclose(fp);
}



void
save_mix() {
    FILE *fp;
    char name[200];
    int i;
    
    strcpy(name,user_dir);
    strcat(name,MIX_FILE);
    
    //  get_player_file_name(name,"mix");
    fp = fopen(name,"w");
    if (fp == NULL) return;
    for (i=0; i < NUM_MIXES; i++) fprintf(fp,"%f\n%f\n%f\n",audio_mix[i].volume,audio_mix[i].balance,audio_mix[i].pan);
    fprintf(fp,"%f\n%d\n%d\n%d\n%d\n",
            get_orch_pitch(),accomp_on_speakers,is_hires,delay_seconds,use_pluck);
    fprintf(fp,"%f\n",mic_input_level);
    /*  fprintf(fp,"%f\n%f\n%f\n%f\n%d\n%d\n%d",
     get_orch_pitch(),master_volume,pan_value,mixlev,accomp_on_speakers,is_hires,delay_seconds);*/
    fclose(fp);
}

void
get_preference_file_name(char *examps) {
    strcpy(examps,user_dir);
    strcat(examps,"preferences");
    
}


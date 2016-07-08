//metasynth, graphic to sound. rtcmix. data sonification
//

//  MyNSView.m
//  TestApp
//
//  Created by Christopher Raphael on 12/26/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import "MyNSView.h"
#import "AppDelegate.h"
//#include "AppKit/NSBitmapImageRep.h"
#include "share.h"
#include "global.h"
#include "linux.h"
#include "belief.h"
//#include "Piano.h"
//#include "PatControl.h"
#include <dirent.h>

#include "yin.h"
#include "dp.h"
#include "vocoder.h"
#include "audio.h"
#include "Resynthesis.h"


#define ON_SOLO 0
#define ON_ACCOMP 1


int cur_play_frame_num;
int start_play_frame_num;
static int audio_type = ON_SOLO;

static int last_frame;  // the last played frame
static FILE *hires_solo_fp = NULL;
static FILE *hires_orch_fp = NULL;
static int is_hires_audio=0;  // this boolean is local to guispect.
static int cur_nt;
int SPECT_HT = 512;
static int prediction_target[MAX_ACCOMP_NOTES];
NSBitmapImageRep *scroll_bitmap;

static int scroll_pos;  // the frame for the left edge of what is displayed in the window

#define MAX_SPECT_HEIGHT 512
#define NOTATION_BUFFER_SIZE 1500 // this is the vertical amount of pixels buffered for scrolling of notation
#define MAX_SPECT_WIDTH 2500 //2000  // this is the official version, no matter what the gui might say
#define CLICK_DIFF 20 //  80 //40 //10

#define RULER_HT 20

static int click_diff = CLICK_DIFF;


typedef struct {  // always using scroll_pos for the 1st frame
    unsigned char buff[MAX_SPECT_WIDTH*MAX_SPECT_HEIGHT];
    unsigned char *ptr[MAX_SPECT_WIDTH];
} SPECT_PAGE;


typedef struct {
    unsigned char *buff;
    unsigned char *scan_line[MAX_SPECT_HEIGHT];
} VISIBLE_PAGE;

#define READ_BUFFER_SIZE 100000  //100000

typedef struct {
    FILE *fp;
    unsigned char *buff;
    int cur;
    int seam;
    int is_empty;
} READ_BUFFER;

typedef struct {
    int index;  // index into either solo or accomp list depending on is_solo
    int is_solo;  // boolean,  is there a solo note here?
    RATIONAL wholerat;
    int frames;  // only used for a fixed (accomp or interpolated) measure mark
    int meas_index;  // index into measdex
    int mark;
} RULER_EL;


#define MAX_MARKERS 20000

typedef struct {
    int num;
    RULER_EL el[MAX_MARKERS];
} RULER_LIST;

static RULER_LIST ruler;


static SPECT_PAGE spect_page;
static VISIBLE_PAGE visible_page;
//static void *bitmap; // the objective c bitmap class instance
NSBitmapImageRep *bitmap;
//void *view_self; // the objective c class instance for this view method
MyNSView *view_self;
static READ_BUFFER solo_read_buff;
static READ_BUFFER orch_read_buff;
//static void *nsTimer; // the objective c class instance for the timer
static NSTimer *nsTimer; // the objective c class instance for the timer

static int image_set = 0;

#define SCROLL_MARGIN 50
#define SPECT_DISC_FILE   "spect.dat"





static unsigned char audio_frame_back[MAX_FRAMES];


static void
add_click(int m) {
    //  printf("adding click to %d\n",m);
    audiodata[2*m*TOKENLEN + 1] += click_diff;
}


static void
rem_click(int m) {
    //   printf("removing click to %d\n",m);
    //   audiodata[2*m*TOKENLEN + 1] -= CLICK_DIFF;
    audiodata[2*m*TOKENLEN + 1] = audio_frame_back[m];
}





#define COLOR_BLACK 0
#define COLOR_BLUE 1
#define COLOR_GREEN 2
#define COLOR_RED 4
#define COLOR_T 3
#define COLOR_Q 5

#define MOVING_LINE_COLOR COLOR_BLUE


void interpolate_freq() {
    int startframe = score.solo.note[1].frames, offset1;
    float pitch_diff;
    int samps_per_frame = SKIPLEN * IO_FACTOR;
    for (int i = first_analyzed_frame; i < last_analyzed_frame - 1; i++) {
        offset1 = first_analyzed_frame * SKIPLEN * IO_FACTOR +(i - first_analyzed_frame) * SKIPLEN * IO_FACTOR;  //8000 k to 48000k
  //8000 k to 48000k
        pitch_diff = inst_freq[i+1] - inst_freq[i];
        for (int j = offset1; j < offset1 + samps_per_frame; j++) {
            cumsum_freq[j] = inst_freq[i] + pitch_diff * (float) (j - offset1) / samps_per_frame;
            float temp = cumsum_freq[j];
        }
    }
}

void cumulative_sum(int sr) {
    double curr = (double) cumsum_freq[0] / sr ;
    cumsum_freq[0] = (float) curr;
    for (int i = 1; i < MAX_SAMPLE; i++) {
        curr += (double) 2*PI*cumsum_freq[i] / sr;
        if (curr >= 2*PI) curr -= 2*PI*floor(curr/(2*PI));
        cumsum_freq[i] = (float) curr;
        if (i%100 == 0) printf("\n%f", cumsum_freq[i]);

        //if (i >= score.solo.note[1].frames * SKIPLEN * IO_FACTOR)printf("\ncumsum freq = %f",  cumsum_freq[i]);
    }
    
}

void synth_audio(overtone_num) {
    float wav;
    float amp = 0.015;
    for (int i = 0; i < MAX_SAMPLE; i++) {
        wav = 0;
        for (int j = 1; j <= overtone_num; j++) {
            wav += (float) amp/j * sinf(cumsum_freq[i] + j * 0.1);
        }
        //if (i >= score.solo.note[1].frames * SKIPLEN * IO_FACTOR)printf("\nwav = %f",  wav);
        float2sample(wav, synth_pitch + i * 2);
    }
}

void resynth_solo(int sr) { //use cumsum instead of concatenating sine waves
    int overtone_num = 7;
    char stump[500];
    interpolate_freq();
    cumulative_sum(sr);
    synth_audio(overtone_num);
    FILE *fp;
    strcpy(stump,audio_data_dir);
    strcat(stump, "synth_pitch");
    fp = fopen(stump, "wb");
    fwrite(synth_pitch,MAX_SAMPLE,BYTES_PER_SAMPLE, fp);
    fclose(fp);
    play_synthesis = 0;
}

void save_audio_data(){
    char fileName[200];
    
    strcpy(fileName, user_dir);
    strcat(fileName, "output.wav");

    FILE *fp = fopen(fileName,"w");
      WavHeader header;

    header.chunk_id[0] = 'R';
    header.chunk_id[1] = 'I';
    header.chunk_id[2] = 'F';
    header.chunk_id[3] = 'F';
    header.chunk_size = 4 + (8 + 16) + (8 + (frames * BYTES_PER_FRAME / BYTES_PER_SAMPLE));
    header.format[0] = 'W';
    header.format[1] = 'A';
    header.format[2] = 'V';
    header.format[3] = 'E';
    header.fmtchunk_id[0] = 'f';
    header.fmtchunk_id[1] = 'm';
    header.fmtchunk_id[2] = 't';
    header.fmtchunk_id[3] = ' ';
    header.fmtchunk_size = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = 8000;
    header.byte_rate = (header.sample_rate * header.num_channels * 1);
    header.block_align = 2;
    header.bitspersample = 16;
    header.datachunk_id[0] = 'd';
    header.datachunk_id[1] = 'a';
    header.datachunk_id[2] = 't';
    header.datachunk_id[3] = 'a';
    header.datachunk_size = frames * BYTES_PER_FRAME / BYTES_PER_SAMPLE;


    fwrite(&header, 1, sizeof(WavHeader), fp);
    fwrite(audiodata,1,header.datachunk_size,fp);
    fclose(fp);
}

//measure similarity between two audio frame feature vectors
float compare_feature(AUDIO_FEATURE ff1, AUDIO_FEATURE ff2) {
    float euclid_dist = powf(ff1.hz - ff2.hz, 2);/* + powf(1000*(ff1.amp - ff2.amp), 2);*/
    return euclid_dist;
}

static float frame_feature_dist(AUDIO_FEATURE ff1, AUDIO_FEATURE ff2){
      float dist = fabsf(ff1.hz - ff2.hz) + 100*fabsf(ff1.amp - ff2.amp);
      return dist;
}

static int find_closest_frame_index(AUDIO_FEATURE f, AUDIO_FEATURE_LIST database){
      int opt = -1;
      float dist = HUGE_VAL;
      
      AUDIO_FEATURE f1 = f;
      f1.hz/=3.3;
      
      for(int i = 0; i < database.num; i++){
            AUDIO_FEATURE f2 = database.el[i];
            if(f2.hz < 0) continue;
            float new_dist = compare_feature(f1, f2);
            if(new_dist < dist){
                  opt = i;
                  dist = new_dist;
            }
      }
      
      float hz_opt = database.el[opt].hz;
      
      return(opt);
}

static void read_features(char *name, AUDIO_FEATURE_LIST *list) {
    
    FILE *fp;
    fp = fopen(name, "r");
    if (fp == NULL) { printf("can't open %s\n",name); return; }
    
    int frames = 0;
    fscanf(fp,"Total number of frames: %d\n",&frames);
    
    AUDIO_FEATURE af;
    list->num = 0;
    list->el = malloc(frames * sizeof(AUDIO_FEATURE));
    while (feof(fp) == 0) {
      fscanf(fp, "%d\t%f\t%f\t%f\n", &af.frame, &af.hz, &af.amp, &af.nominal);
      list->el[list->num++] = af;
    }
    
    fclose(fp);
}

static int** malloc_int_matrix(int rows, int cols){
      int **matrix = malloc(rows *sizeof(int*));
      
      for(int i = 0; i < rows; i++){
            matrix[i] = malloc(cols *sizeof(int));
      }
      return matrix;
}

static float** malloc_float_matrix(int rows, int cols){
      float ** matrix = malloc(rows *sizeof(float*));
      
      for(int i = 0; i < rows; i++){
            matrix[i] = malloc(cols *sizeof(float));
      }
      return matrix;
}

typedef struct{
      int index;
      float score;
} PAIR;

static int cmp_pair(void *ptr1, void *ptr2){
      PAIR *p1 = (PAIR *)ptr1;
      PAIR *p2 = (PAIR *)ptr2;
      if(p1->score < p2->score) return -1;
      else if(p1->score == p2->score) return 0;
      else return 1;
}

static void build_best_path(int **best, AUDIO_FEATURE_LIST list, int num){
      PAIR *p = malloc(list.num *sizeof(PAIR));
      
      for(int i = 0; i < list.num; i++){
            AUDIO_FEATURE f = list.el[i];
            
            for(int j = 0; j < list.num; j++){
                  AUDIO_FEATURE f2 = list.el[j];
                  PAIR temp;
                  temp.index = j;
                  temp.score = frame_feature_dist(f, f2);
                  p[j] = temp;
            }
            qsort(p, list.num, sizeof(PAIR), cmp_pair);
            
            for(int j = 0; j < num; j++){
                  best[i][j] = p[j].index;
            }
      }
    free(p);
}

void resynth_solo_phase_vocoder(AUDIO_FEATURE_LIST database_feature_list) {
      char target_name[200];
      strcpy(target_name,audio_data_dir);
      strcat(target_name,current_examp);
      strcat(target_name, ".feature");
      AUDIO_FEATURE_LIST saved_feature_list;
      read_features(target_name, &saved_feature_list);
      
      vcode_init();
      temp_rewrite_audio();
      
      int n_best = 50;
      float **score = malloc_float_matrix(saved_feature_list.num, database_feature_list.num);
      int **prev = malloc_int_matrix(saved_feature_list.num, database_feature_list.num);
      int **best = malloc_int_matrix(database_feature_list.num, n_best);
      
      for(int i = 0; i < saved_feature_list.num; i++){
            for(int j = 0; j < database_feature_list.num; j++){
                  score[i][j] = (i==0)? 0:HUGE_VAL;
                  prev[i][j] = -1;
            }
      }
      
      build_best_path(best, database_feature_list, n_best);
      
      float penalty = 100;
      int trans_interval = 5;
      for(int i = 1; i < saved_feature_list.num; i++){
            AUDIO_FEATURE f1 = saved_feature_list.el[i];
            f1.hz /= 3;//kludgy transposition
            for(int j = 0; j < database_feature_list.num; j++){
                  AUDIO_FEATURE f2 = database_feature_list.el[j];
                  float dis = frame_feature_dist(f1, f2);
                  
                  if(j > 0 && score[i][j] > score[i-1][j-1] + dis){
                        score[i][j] = score[i-1][j-1] + dis;
                        prev[i][j] = j-1;//already fixed? (needs to be fixed, j-1 is the index of feature list but not actually the index of frame)
                  }
                  
                  if(i > trans_interval && f1.nominal != saved_feature_list.el[i - trans_interval].nominal) continue; //no splice during note transition
                  
                  if(i < saved_feature_list.num - trans_interval && f1.nominal != saved_feature_list.el[i + trans_interval].nominal) continue;
                  
                  for(int jj = 0; jj < n_best; jj++){
                        int index = best[j][jj];
                        if(score[i][j] > score[i-1][index] + dis + penalty){
                              score[i][j] = score[i-1][index] + dis + penalty;
                              prev[i][j] = index;
                        }
                  }
            }
      }
      
      int i = saved_feature_list.num - 1;
      float opt_score = HUGE_VAL;
      int opt_j;
      for(int j = 0; j < database_feature_list.num; j++){
            if(score[i][j] < opt_score){
                  opt_score = score[i][j];
                  opt_j = j;
            }
      }
      
      int best_prev[saved_feature_list.num];
      for(int i = saved_feature_list.num - 1; i > 0; i--){
            best_prev[i] = database_feature_list.el[opt_j].frame;
            opt_j = prev[i][opt_j];
      }
      
      for(int i = 1; i < saved_feature_list.num; i++){ //i is the frame index of test data
            vcode_synth_frame_var(best_prev[i]);
      }
}



static int
line_color(n) { /* this is definitive color algorithm */
    int f,b;
    
    if (n == firstnote-1) return(COLOR_GREEN);
    f = score.solo.note[n].set_by_hand;
    b = (n == cur_nt)  ? COLOR_GREEN : COLOR_RED;
    return( b | (f*COLOR_BLUE));
}


void  // removed for compile
add_text_pos() {
    char pos[500],t[500],c[500],frame[500],text[500],meas[500];
    
    if (cur_nt < firstnote)  {
        strcpy(meas,"Pos:");
        strcpy(frame, "0"); // "Frame: 0";
        [[[NSApplication sharedApplication] delegate] setStatusPos:meas];
        [[[NSApplication sharedApplication] delegate] setStatFrame];
        return;
    }
    wholerat2string(score.solo.note[cur_nt].wholerat,pos);
    strcpy(t,pos);
    sndnums2string(score.solo.note[cur_nt].snd_notes,c);
    //  sprintf(t," note(s) %s at pos %s (frame %d)",c,pos,score.solo.note[cur_nt].frames);
    sprintf(frame,"%d",score.solo.note[cur_nt].frames);
    if (score.solo.note[cur_nt].frames == 0) strcpy(frame,"unknown");
    //  Spect->Label1->Caption = t;
    sprintf(text,"%s",pos);
    //  sprintf(text,"Pos: %s",pos);
    strcpy(meas,text);
    
    [[[NSApplication sharedApplication] delegate] setStatusPos:meas];
    
    //  Spect->Status->Panels->Items[4]->Text = text;
    //sprintf(text,"Pitches: %s",c);
    //sprintf(text,"%s",c);
    //  Spect->Status->Panels->Items[5]->Text = text;
    //  sprintf(text,"Frame: %s",frame);
    sprintf(text,"%s",frame);
    [[[NSApplication sharedApplication] delegate] setStatFrame];
    //  Spect->Status->Panels->Items[5]->Text = text;
}



static void
init_visible_page(unsigned char *buff, int width, int height) {
    int i;
    visible_page.buff = buff;
    for (i=0; i < height; i++) visible_page.scan_line[i] = buff + i*width*3;
}


static int
show_index(int j) {
    RATIONAL r,d;
    
    if (is_solo_rest(j) && show_rests == 0) return(0);
    if (spect_inc.num == 0) return(1);
    r = wholerat2measrat(score.solo.note[j].wholerat);
    d = div_rat(r,spect_inc);
    return (d.den == 1);
}




static void
rem_clicks() {
    char* col;
    int i,j,m;
    extern unsigned char *audiodata;
    
    
    
    for (j=firstnote; j <= lastnote; j++) {
        m = score.solo.note[j].frames;
        if (m >= frames) continue;  /* ??? */
        rem_click(m);
    }
}



static void
add_clicks() {
    char* col;
    int i,j,m;
    extern unsigned char *audiodata;
    
    return;
    
    for (j=firstnote; j <= lastnote; j++) {
        if (show_index(j) == 0) continue;
        m = score.solo.note[j].frames;
        if (m >= frames) continue;  /* ??? */
        //    if (score.solo.note[j].snd_notes.num == 0) continue;
        //    audiodata[2*m*TOKENLEN + 1] += v;
        add_click(m);
    }
}



static void
add_mark_to_visible(int column, int color, int top, int bot) {
    unsigned int r, g, b;
    int i,width;
    Byte *ptr;
    NSUInteger z[3];
    
    width = spect_wd;
    if (column < scroll_pos) return;
    if (column >= scroll_pos+width) return;
    z[2] = r = (color&1) ? 255 : 0;
    z[1] = g = (color&2) ? 255 : 0;
    z[0]=  b = (color&4) ? 255 : 0;
    //  for (i=0; i < SPECT_HT; i++) {
    for (i=top; i < bot; i++) {
        [bitmap setPixel:z atX:(column-scroll_pos) y:i];
    }
    
    
    NSRect rect  = {column-scroll_pos,0,1,SPECT_HT};
    
    [view_self setNeedsDisplayInRect:rect];
    
    
    
    
}



static void
restore_synth_marks(int column) {
    int i;
    
    for (i=firstnote; i < cur_note; i++) {  // fix solo note markers that were erased
        if (column != (int) secs2tokens(score.solo.note[i].on_line_secs)) continue;
        if (score.solo.note[i].treating_as_observed == 0) continue;
        add_mark_to_visible(column/*-scroll_pos*/, COLOR_GREEN, 0,25);
    }
    if (column != prediction_target[cur_accomp-1]) return;  // fix orchestra mark that was erased
    add_mark_to_visible(column/*-scroll_pos*/, COLOR_RED, 25,50);
}






static void
restore_line_to_visible(int column) {
    unsigned int r, g, b;
    int i,j,width,base,col;
    Byte *ptr,*iptr;
    char comment[500];
    NSUInteger z[3],*cptr,black[3]={0,0,0};
    //  TRect src,dst;
    
    base = (SPECT_HT >= freqs) ? 0 : freqs-SPECT_HT;
    width = spect_wd;
    if (column < scroll_pos) return;
    if (column >= scroll_pos+width) return;
    col = column - scroll_pos;
    for (i=RULER_HT; i < SPECT_HT; i++) {
        //  for (i=0; i < SPECT_HT; i++) {
        
        r = g = b = spect_page.ptr[col][i];
        for (j=0; j < 3; j++) z[j] = r;
        //  cptr = (i < RULER_HT) ? black : z;
        //        NSUInteger zColourAry[3] = {r,g,b};
        [bitmap setPixel:z atX:col y:i];
        //  for (i=0; i < SPECT_HT; i++) {
        /*  ptr = (Byte *) visible->ScanLine[i];
         iptr = (Byte *) spectrogram->ScanLine[base+i];
         for (j=0; j < 3; j++)  ptr[3*(column-scroll_pos)+j] = iptr[3*column+j];*/
        
    }
    // NSRect rect  = {col-5,100,10,100};
    if (mode == SYNTH_MODE) restore_synth_marks(column);
    
    //    NSRect rect  = {col,0,1,SPECT_HT};
    // NSRect rect  = {col,0,1,SPECT_HT-RULER_HT};
    NSRect rect  = {col-1,0,2,SPECT_HT-RULER_HT};
    /* AFAICT this is a bug in Apple's implementation.  This used to work fine with the rectangle
     as defined above. Now I need a 2-pixel-wide rectangle to undo the moving line */
    [view_self setNeedsDisplayInRect:rect];
    
}



static void
add_line_to_visible(int column, int color) {
    unsigned int r, g, b;
    int i,width;
    Byte *ptr;
    NSUInteger z[3],*cptr;
    NSUInteger black[3] = {0,0,0};
    
    
    width = spect_wd;
    if (column < scroll_pos) return;
    if (column >= scroll_pos+width) return;
    z[2] = r = (color&1) ? 255 : 0;
    z[1] = g = (color&2) ? 255 : 0;
    z[0]=  b = (color&4) ? 255 : 0;
    //  for (i=0; i < SPECT_HT; i++) {
    for (i=RULER_HT; i < SPECT_HT; i++) { //adds a little space on top for measure markings
        
        //  for (i=0; i < SPECT_HT; i++) {
        //        NSUInteger zColourAry[3] = {r,g,b};
        //[bitmap setPixel:zColourAry atX:(column-scroll_pos) y:i];
        // cptr = (i < RULER_HT) ? black : z;
        [bitmap setPixel:z atX:(column-scroll_pos) y:i]; //x //true version
        
        /*  ptr = (Byte *) visible->ScanLine[i] + 3*(column-scroll_pos);
         ptr[0] = r;
         ptr[1] = g;
         ptr[2] = b;*/
    }
    
    
    // NSRect rect  = {column-scroll_pos,0,1,SPECT_HT};
    NSRect rect  = {column-scroll_pos,0,1,SPECT_HT-RULER_HT};
    
    [view_self setNeedsDisplayInRect:rect];
    
    
    
    
}

static void
add_line_to_visible_pitch(int column, int endpos, int color, int freq, int *beg) {
    unsigned int r, g, b;
    int i,width;
    Byte *ptr;
    NSUInteger z[3],*cptr;
    NSUInteger black[3] = {0,0,0};
    
    
    width = spect_wd;
    if (column < scroll_pos) return;
    if (*beg == 1) {
        first_analyzed_frame = column; //get index of first visible note
        *beg = 0;
    }
    if (column >= scroll_pos+width) return;
    last_analyzed_frame = column; //get index of last visible note
    z[2] = r = (color&1) ? 255 : 0;
    z[1] = g = (color&2) ? 255 : 0;
    z[0]=  b = (color&4) ? 255 : 0;
    
    double j, k;
    for (i = column; i < endpos; i++) {
        [bitmap setPixel:z atX:(i-scroll_pos) y:(SPECT_HT - freq)];
        
    }
    
    NSRect rect  = {column-scroll_pos,0,(endpos - column),SPECT_HT-RULER_HT};
    
    [view_self setNeedsDisplayInRect:rect];
    
}

int calc_inst_freq_bin_yin(int pos, int bin) {
    
    int offset, inc, buffer_length;
    unsigned char *ptr1;
    float inst_bin, hz_hat, tp1[FRAMELEN];
    int16_t yin_audio[FRAMELEN];
    offset = pos*SKIPLEN*BYTES_PER_SAMPLE;
    ptr1 = audiodata + offset;
    hz_hat = omega2hz(bin);
    buffer_length = 5*((float) SAMPLE_SR/hz_hat);
    for (int i = 0; i < 1000; i = i+2) {
        int16_t temp = (int16_t) ptr1 + i;
    }
    Yin yin;
    float pitch;

    Yin_init(&yin, buffer_length, 0.6);
    pitch = Yin_getPitch(&yin, ptr1, hz_hat);
    buffer_length++;
    
    printf("Pitch is found to be %f with buffer length %i and probability %f\n",pitch, buffer_length, Yin_getProbability(&yin) );
    inst_freq[pos] = pitch; //actual instantaneous frequency

    inst_bin = hz2omega(inst_freq[pos]);
    return (int) inst_bin;
}

int calc_inst_freq_bin(int pos, int bin) {
    
    float binf = (float) bin/2; //bin value on spectrogram is 2*the actual value
    int offset, inc;
    unsigned char *ptr1;
    unsigned char *ptr2;
    float inst_bin, tp1[FRAMELEN_PITCH], tp2[FRAMELEN_PITCH], ph1, ph2, omega, ph_inc, i1, i2, r1, r2, c, ratio;
    ratio = (float) FRAMELEN_PITCH/FRAMELEN;
    inc = 20; //number of samples between first and second frames for phase-based pitch calculation
    offset = pos*SKIPLEN*BYTES_PER_SAMPLE;
    ptr1 = audiodata + offset;
    ptr2 = ptr1 + inc*2; //advance by 2 unsigned char per sample in audiodata
    samples2floats(ptr1, tp1, FRAMELEN_PITCH);
    samples2floats(ptr2, tp2, FRAMELEN_PITCH);
    i1 = i2 = r1 = r2 = 0; //calculate fft for this bin for the chunks starting at ptr1 and ptr2
    c = 2*PI*binf*ratio / FRAMELEN_PITCH;
    for (int j=0; j<FRAMELEN_PITCH; j++) {
        i1 -= tp1[j] * sinf(c * j) * coswindow2[j]; //there seem to be fewer gross errors when using coswindow (wrong window size)
        i2 -= tp2[j] * sinf(c * j) * coswindow2[j];
        r1 += tp1[j] * cosf(c * j) * coswindow2[j];
        r2 += tp2[j] * cosf(c * j) * coswindow2[j];
    }
    omega = 2 * PI * binf * (float) inc * ratio / FRAMELEN_PITCH; //expected number of cycles per increment
    
    ph1 = atan2f(i1, r1); //get phase increment
    ph2 = atan2f(i2, r2);
    float phtemp = princarg(ph2 - ph1 - omega);
    //printf("\n phase incr new = %f", phtemp);
    ph_inc = omega + princarg(ph2 - ph1 - omega); //add increment between -pi and +pi
    inst_freq[pos] = ph_inc * (float) SR / (2 * PI * inc); //actual instantaneous frequency
    
    float tempp = inst_freq[pos];
    //printf("\n inst pitch = %f", tempp);
    
    inst_bin = hz2omega(inst_freq[pos]);
    return (int) inst_bin;
}

void calc_max_bin(int startpos, int endpos, int fbin) {
    int max_bin, m, bin, k_min, k_max, width;
    float ph1, ph2, omega, ph_inc, f_inst, s, c, temp; //FREQDIM is 1024
    
    width = spect_wd; //check that note fits completely in window
    if (startpos < scroll_pos) return;
    if (endpos >= scroll_pos+width) return;
    k_min = max(fbin - 5, 2); //examine bins in range of nominal bin for this note +=5
    k_max = k_min + 10;
    if (test_audio_wave == 1) { //except when analyzing synthetic sound wave: then, just look at all the bins
        k_min = 2;
        k_max = 500;
    }
    for (int j = startpos; j < endpos; j++) {
        max_bin = m = -1;
        //for (int i = k; i < k+8; i++) {
        for (int i = k_min; i < k_max; i++) {
            
            //printf("\t%d", (int) spect_page.ptr[j-scroll_pos][SPECT_HT-i]);
            if (spect_page.ptr[j-scroll_pos][SPECT_HT-i] > m) { //???
                m = spect_page.ptr[j-scroll_pos][SPECT_HT-i];
                max_bin = i;
            }
            inst_fbin[j] = max_bin;
        }
    }
    for (int j = startpos; j < endpos; j++) {
        inst_fbin[j] = calc_inst_freq_bin_yin(j, inst_fbin[j]); //calculate instantaneous pitch using selected bin
        //inst_fbin[j] = calc_inst_freq_bin(j, inst_fbin[j]); //calculate instantaneous pitch using selected bin
    }
}


static void
add_line_to_inst_pitch(int column, int endpos, int color, int fbin) {
    unsigned int r, g, b;
    int i,width;
    float bin_inst;
    Byte *ptr;
    NSUInteger z[3],*cptr;
    NSUInteger black[3] = {0,0,0};
    
    width = spect_wd;
    if (column < scroll_pos) return;
    if (column >= scroll_pos+width) return;
    z[2] = r = (color&1) ? 255 : 0;
    z[1] = g = (color&2) ? 255 : 0;
    z[0]=  b = (color&4) ? 255 : 0;
    
    init_window(); //window for stft
    
    calc_max_bin(column, endpos, fbin); //choose maximum bin in neighborhood of nominal pitch, calculate true frequency
    
    double j, k;
    for (i = column; i < endpos; i++) {
        inst_fbin[i] = hz2omega(inst_freq[i]);
        [bitmap setPixel:z atX:(i-scroll_pos) y:(SPECT_HT - inst_fbin[i])];

    }
    
    //temp: add amplitude to plot
    color = color - 2;
    z[2] = r = (color&1) ? 255 : 0;
    z[1] = g = (color&2) ? 255 : 0;
    z[0]=  b = (color&4) ? 255 : 0;
    for (i = column; i < endpos; i++) {
        [bitmap setPixel:z atX:(i-scroll_pos) y:(SPECT_HT - 100*inst_amp[i])];
    }
    
    NSRect rect  = {column-scroll_pos,0,1,SPECT_HT-RULER_HT};
    
    [view_self setNeedsDisplayInRect:rect];
    
}




draw_ruler() {
    unsigned int r, g, b;
    int i,j,k,l,width,is_solo,frame,m;
    Byte *ptr;
    NSUInteger grey[3],red[3],black[3];
    RATIONAL q;
    
    width = spect_wd;
    grey[2] = r = 50;
    grey[1] = g = 50;
    grey[0] = b = 50;
    red[2] = r = 256;
    red[1] = g = 0;
    red[0] = b = 0;
    black[2] = r = 0;
    black[1] = g = 0;
    black[0] = b = 0;
    
    
    
    for (i=0; i < RULER_HT; i++) for (j=0; j < spect_wd; j++)
        [bitmap setPixel:grey atX:j y:i];
    for (k=0; k < ruler.num; k++) {
        q = wholerat2measrat(ruler.el[k].wholerat);
        m = wholerat2measnum(ruler.el[k].wholerat);
        if (q.num != 0) continue;
        l = ruler.el[k].index;
        is_solo = ruler.el[k].is_solo;
        //   frame = (is_solo) ? score.solo.note[l].frames : score.midi.burst[l].frames;
        frame = (is_solo) ? score.solo.note[l].frames : ruler.el[k].frames;
        //   if (m == 1) NSLog(@"draw_ruler meas 1 at %d",frame);
        j = frame-scroll_pos;
        for (i=0; i < RULER_HT; i++)  [bitmap setPixel:black atX:j y:i];
        
        
    }
    
    NSRect rect  = {0,SPECT_HT-RULER_HT,spect_wd,RULER_HT};  // vertical coords go from bottom
    
    
    [view_self setNeedsDisplayInRect:rect];
    
    
}



static void
draw_note_markers(int b) {
    int i,j,m,color,f,width;
    
    
    for (j=firstnote; j <= lastnote; j++) {
        //        if (score.solo.note[firstnote].frames) NSLog(@"firstnote set at %d",j);
        if ((b == 1) && (show_index(j) == 0)) continue;
        m = score.solo.note[j].frames;
        f = score.solo.note[j].set_by_hand;
        
        //  if (m >= spectrogram->Width) continue;
        
        color = COLOR_RED | (f*COLOR_BLUE);
        if (is_solo_rest(j)) color = COLOR_T | (f*COLOR_Q);
        if (b) add_line_to_visible(m,color); else restore_line_to_visible(m);
        
    }
    
}

void calculate_amplitude(startframe, endframe) {
    char fileName[500];
    unsigned char *temp;
    int offset;
    float amp;
    for (int j = startframe; j < endframe; j++) {
        offset = j*SKIPLEN*BYTES_PER_SAMPLE;
        temp =  audiodata + offset;
        samples2floats(temp, data, FRAMELEN);
        inst_amp[j] = 0;
        for (int i = 0; i < FRAMELEN; i++) { //sum of squares
            inst_amp[j] += data[i]*data[i];
        }
    }
    
    strcpy(fileName, user_dir);
    strcat(fileName, "/audio/inst_amp");
    FILE *fp;
    fp = fopen(fileName, "w");
    fwrite(inst_amp, sizeof(float), 4500, fp);
    fclose(fp);
}

static void
draw_pitch(int b) {
    int i,j,s,e,color,f,midi,width,beg = 1;
    float fmidi,fbin;
    
    
    for (j=firstnote; j < lastnote; j++) {
        if ((b == 1) && (show_index(j) == 0)) continue;
        
        s = score.solo.note[j].frames; //starting position
        e = score.solo.note[j+1].frames; //ending position
        midi = score.solo.note[j].snd_notes.snd_nums[0]; //midi pitch
        
        color = COLOR_Q | (f*COLOR_T);
        
        fmidi = (int) (pow(2,((midi - 69)/12.0)) * 440);
        fbin = (int) hz2omega(fmidi);
        
        calculate_amplitude(s, e);
        
        if (b) add_line_to_visible_pitch(s, e, color, fbin, &beg);
        if (b) add_line_to_inst_pitch(s, e, color+1, fbin);
        
        
        
    }
    
}



static void
get_spect_page(int edge, int width, SPECT_PAGE *page) {
    FILE *fp;
    int i,j;
    char file[500];
    
    strcpy(file,user_dir);
    strcat(file,SPECT_DISC_FILE);
    bzero(page->buff,MAX_SPECT_WIDTH*MAX_SPECT_HEIGHT);
    // fp = fopen(SPECT_DISC_FILE,"rb");
    fp = fopen(file,"rb");
    fseek(fp,edge*MAX_SPECT_HEIGHT,SEEK_SET);
    fread(page->buff,MAX_SPECT_HEIGHT*width,1,fp);
    
    fclose(fp);
    for (i=0; i < width; i++)   page->ptr[i] = page->buff + i*MAX_SPECT_HEIGHT;
    /*  for (i=0; i < width; i++) for (j=0; j < MAX_SPECT_HEIGHT; j++)
     if (page->ptr[i][j] > 0) printf("i = %d j = %d val = %d\n",i,j,page->ptr[i][j]);
     fflush(stdout);
     exit(0);*/
    
    
}









void
highlight_notes() {
    highlight_solo_notes();
}



static void
make_spect_visible() {
    NSUInteger r,g,b;
    int x,y,i,bpr;
    unsigned char v,*ptr,*row[MAX_SPECT_HEIGHT],*col;
    
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    //    bpr = 3*spect_wd;
    for (y = 0; y < SPECT_HT; y++) row[y] = ptr + y*bpr;
    for (x = 0; x < spect_wd; x++) { // loop over columns
        col = spect_page.ptr[x];
        for (y = 0; y < SPECT_HT; y++) {  // going through column
            v = *col++;
            *row[y]++ = v;	*row[y]++ = v;	*row[y]++ = v;
        }
    }
    if (clicks_in) highlight_notes();
    return;
    
    
    
    for (y = 0; y < SPECT_HT; y++) {
        for (x = 0; x < spect_wd; x++) {
            r = g = b = spect_page.ptr[x][y];
            NSUInteger zColourAry[3] = {r,g,b};
            [bitmap setPixel:zColourAry atX:x y:y];
        }
    }
    // draw_note_markers(1);
    if (clicks_in) highlight_notes();
}

void
unhighlight_notes() {
    rem_clicks();
    draw_note_markers(0);
    draw_pitch(0);
}




static void
scroll_if_needed(int new_pos, int dir) {  /* dir is + or - */
    int width,mv,rel,next,out_of_range,left,scroll_left,scroll_rite,far_rite,on_page;
    char comment[500];
    // TRect r1,r2;
    
    
    if (dir == 0) return;
    width = spect_wd;
    //  width = [[[NSApplication sharedApplication] delegate] spectWindowWidth];
    on_page = (new_pos < scroll_pos+width  && new_pos >=  scroll_pos);
    
    if (on_page) {
        if (dir > 0 && new_pos < scroll_pos+width-SCROLL_MARGIN) return;
        if (dir > 0 && frames == scroll_pos+width) return;  // no scrolling possible
        //  if (dir < 0 && new_pos >= scroll_pos+SCROLL_MARGIN) return;
        if (dir < 0) if (scroll_pos == 0 || new_pos >= scroll_pos+SCROLL_MARGIN) return;
    }
    
    // prepare  to scroll
    
    if (dir > 0) scroll_pos = new_pos - SCROLL_MARGIN;
    if (dir < 0) scroll_pos = new_pos - (width - SCROLL_MARGIN);
    if (scroll_pos < 0) scroll_pos = 0;
    far_rite = max((frames - width),0);  // the greatest possible scroll_pos (left edge)
    if (scroll_pos >= far_rite) scroll_pos = far_rite;
    get_spect_page(scroll_pos, spect_wd, &spect_page);
    make_spect_visible();
    draw_ruler();
    [view_self setNeedsDisplay:YES];
    
    /* init_visible();
     Spect->Image1->Canvas->Draw(0,0,visible);*/
}




static void
initialize_note_pos() {
    int i,l=-1,r=-1;
    
    for (i=0; i < score.midi.num; i++)
        if (score.solo.note[i].set_by_hand && i < cur_nt) l = i;
    score.solo.note[cur_nt].frames =
    (l == -1) ? 0 : score.solo.note[l].frames;
}

void
unmark_note(void) {
    int c,n;
    
    
    score.solo.note[cur_nt].set_by_hand = 0;
    c = line_color(cur_nt);
    n = score.solo.note[cur_nt].frames;
    add_line_to_visible(n,c);
}


void
move_cur_nt(int inc) {
    int p,n,c,i;
    
    current_parse_changed_by_hand =1;
    if (score.solo.note[cur_nt].frames <= 0) initialize_note_pos();  // an experiment 12-09
    if (cur_nt < firstnote) return;
    n = score.solo.note[cur_nt].frames + inc;
    if (n < 0 || n >= frames) return;
    for (i=firstnote; i <= lastnote; i++) {
        if (score.solo.note[i].set_by_hand == 0) continue;
        if (i < cur_nt && score.solo.note[i].frames >= n) break;
        if (i > cur_nt && score.solo.note[i].frames <= n) break;
    }
    if (i <= lastnote) return;  // can't cross over hand set note
    /* this isn't right.  it is okay to cross over handset if you are crossing in correct direction.
     it is not okay to move from the correct side to the incrorrect side */
    score.solo.note[cur_nt].set_by_hand = 1;
    /*if (cur_nt > firstnote && score.solo.note[cur_nt].frames < score.solo.note[cur_nt-1].frames)
     score.solo.note[cur_nt].frames = score.solo.note[cur_nt-1].frames + 2;
     this is an experiment.  If note is out of sequence at least put it in sequence. */
    p = score.solo.note[cur_nt].frames;
    
    score.solo.note[cur_nt].frames = n;
    restore_line_to_visible(p);
    rem_click(p);
    c = line_color(cur_nt);
    add_line_to_visible(n,c);
    add_click(n);
    
    //  add_text_pos();  // really just rewrite the frame number
    draw_ruler();  // really don't need to draw *all* of ruler ...
    scroll_if_needed(n,inc);
    
}



void
highlight_note_with_index(int index) {
    int linepos,c,inc;
    
    inc = index-cur_nt;
    restore_line_to_visible(score.solo.note[cur_nt].frames);
    cur_nt = index;
    linepos = (cur_nt == firstnote-1) ? 0 : score.solo.note[cur_nt].frames;
    c = line_color(cur_nt);
    scroll_if_needed(linepos , inc);
    add_line_to_visible(linepos,c);
}


static int
note_showable(index) {
    if (index == firstnote-1) return(1);
    if (index < firstnote-1 && index > lastnote) return(0);
    if (show_index(index) == 0) return(0);
    if (score.solo.note[index].frames <= 0) return(0);
    return(1);
}


void
highlight_neighbor_note(int inc) {
    static char *green, *red;
    char chord[100],comment[500];
    int i,n,width,linepos,rel,mv,pos,d,c,orig_note,m,nxt;
    //  TRect r;
    static scrollpos;
    //   extern int piano_key_down[];
    
    d = (inc > 0) ? 1 : -1;
    nxt = cur_nt+inc;
    if (nxt > lastnote || nxt < firstnote-1) return;
    orig_note = cur_nt;
    cur_nt += inc;
    
    /* while ((show_index(cur_nt) == 0 || score.solo.note[cur_nt].frames <= 0) &&
     // (cur_nt < lastnote) && (cur_nt > firstnote)) cur_nt += d;
     (cur_nt < lastnote) && (cur_nt >= firstnote)
     ) cur_nt += d;*/
    
    
    while ((note_showable(cur_nt) == 0) &&
           (cur_nt < lastnote) && (cur_nt >= firstnote)
           ) cur_nt += d;
    
    
    //  if (show_index(cur_nt) == 0) cur_nt = orig_note;  // there wasn't showable note in requested direction
    
    if (cur_nt != (firstnote-1) && note_showable(cur_nt) == 0)
        // ((score.solo.note[cur_nt].frames <= 0) || (show_index(cur_nt) == 0)))
        cur_nt = orig_note;
    // there wasn't showable note in requested direction
    
    if (clicks_in && (orig_note != firstnote-1)) {
        c = line_color(orig_note);
        add_line_to_visible(score.solo.note[orig_note].frames,c);
    }
    // xxxx crashes here though the argument to restore_line looks okay ???
    
    else restore_line_to_visible(score.solo.note[orig_note].frames);
    for (i=0;i < score.solo.note[orig_note].snd_notes.num; i++){
        //     draw_piano_key(score.solo.note[orig_note].snd_notes.snd_nums[i],0);
        m = score.solo.note[orig_note].snd_notes.snd_nums[i];
        //   piano_key_down[m]=0;
        
    }
    
    //   add_text_pos();
    
    //  linepos = score.solo.note[cur_nt].frames;
    linepos = (cur_nt == firstnote-1) ? 0 : score.solo.note[cur_nt].frames;
    c = line_color(cur_nt);
    scroll_if_needed(linepos , inc);
    add_line_to_visible(linepos,c);
    if (cur_nt >= firstnote && cur_nt <= lastnote) {
        for (i=0;i < score.solo.note[cur_nt].snd_notes.num; i++) {
            //     draw_piano_key(score.solo.note[cur_nt].snd_notes.snd_nums[i],1);
            m = score.solo.note[cur_nt].snd_notes.snd_nums[i];
            //     piano_key_down[m] = 1;
        }
    }
    //   [piano_self setNeedsDisplay:YES];  // removed for compile
}

//static void
int
highlight_solo_notes() {
    int j,m;
    
    add_clicks();
    draw_note_markers(1);
    draw_pitch(1);
    resynth_solo(48000);
    highlight_neighbor_note(0);  //(increment = 0) add text pos and green for current note
    // xxx this makes program crash after live performance sometimes
    draw_ruler();
}


void
save_audio_values() {
    int i,m;
    
    for (i=0; i < frames; i++) {
        audio_frame_back[i]  = audiodata[2*i*TOKENLEN + 1];
    }
}

static void
scroll_to_left() {
    int width,new_pos;
    
    scroll_pos = 0;  // 0th frame at left edge
    return;   // always at left edge
    new_pos = score.solo.note[cur_nt].frames;
    //  width = Spect->ScrollBox1->ClientRect.Right;  /* this incudes container? */ // need this in mac
    width = 0; // not this!!!
    if (new_pos >= scroll_pos && new_pos < scroll_pos+width) return;
    
    scroll_pos = new_pos - SCROLL_MARGIN;
    if (scroll_pos < 0) scroll_pos = 0;
    if (scroll_pos >= frames - width) scroll_pos = frames - width;
    
    
    //  if (score.solo.note[cur_nt].frames  < Spect->ScrollBox1->ClientRect.Right)
    //  else scroll_pos = score.solo.note[cur_nt].frames
}








void
get_exper_rgb_spect_nommap() {
    int ii,i,j,t,v,k,n,spec_ht,bufsz,chunk;
    float **ss,**s,spread,mmax= -10000,mmin=10000,m,**matrix(),*mem,tot,pred[FREQDIM],mx;
    FILE *fp;
    unsigned char *line;
    Byte *ptr;
    extern float last_spect[];
    extern float diff_spect[];
    
    line = (unsigned char *) malloc(3*frames);
    mem = (float *) malloc(FREQDIM*frames*sizeof(float));
    s = (float **) malloc(freqs*sizeof(float *));
    
    
    for (j=0; j < freqs; j++)  s[j] = mem + frames*j;
    
    for (token=0; token < frames; token++) {
        //Application->ProcessMessages();
        
        
        audio_listen();       /* really just want to compute spectrum here.  it would be faster
                               just do the fft calculation and skip orchestra spect etc. */
        
        tot=0;
        mx=0;
        for (j=0; j < freqs; j++) {
            //      s[j][token] =  pow(log(1+spect[j]),.4);
            s[j][token] =  spect[j];
            if (s[j][token] > mx) mx = s[j][token];
            /*#ifdef ATTACK_SPECT
             s[j][token] =  diff_spect[j];
             tot += s[j][token];
             #endif*/
            s[j][token] =  pow(log(1+s[j][token]),.4);
        }
        // for (j=0; j < freqs; j++) s[j][token] /= mx;
        
        /*#ifdef ATTACK_SPECT
         for (j=0; j < freqs; j++)  s[j][token] =  (tot <= 0) ? 0 : pow(log(1+s[j][token]/tot),.6);
         #endif */
        
    }
    m=HUGE_VAL;
    
    /*  for (j=0; j < frames; j++) { //
     mmax = 0;
     for (i = 0; i < freqs; i++)   if (s[i][j] > mmax) mmax = s[i][j];
     for (i = 0; i < freqs; i++) s[i][j] /= mmax;
     }*/
    
    
    for (i = 0; i < freqs; i++)   for (j=0; j < frames; j++) {
        if (s[i][j] > mmax) mmax = s[i][j];
        if (s[i][j] < mmin) mmin = s[i][j];
    }
    
    
    spread = mmax - mmin;
    for (i = 0; i < SPECT_HT; i++) {
        for (j=0, t=0; j < frames; j++) {
            v = (int) (256*(s[SPECT_HT-i-1][j] - mmin)/spread);
            line[t++] = v;      line[t++] = v;      line[t++] = v;
        }
        //    ptr = (Byte *) spectrogram->ScanLine[i];
        // memcpy(ptr,line,3*frames);
    }
    free(mem);
}


void
get_exper_rgb_spect_to_disc() {
    int ii,i,j,t,v,k,n,spec_ht,bufsz,chunk;
    float **ss,**s,spread,mmax= -10000,mmin=10000,m,**matrix(),*mem,tot,pred[FREQDIM],mx,z;
    FILE *fp;
    unsigned char *line,*col;
    Byte *ptr;
    extern float last_spect[];
    extern float diff_spect[];
    char file[500];
    
    line = (unsigned char *) malloc(3*frames);
    col = (unsigned char *) malloc(3*SPECT_HT);
    mem = (float *) malloc(FREQDIM*frames*sizeof(float));
    s = (float **) malloc(freqs*sizeof(float *));
    
    
    for (j=0; j < freqs; j++)  s[j] = mem + frames*j;
    for (token=0; token < frames; token++) {
        background_info.fraction_done =  token / (float) frames;
        audio_listen();       /* really just want to compute spectrum here.  it would be faster
                               just do the fft calculation and skip orchestra spect etc. */
        tot=0;
        mx=0;
        for (j=0; j < freqs; j++) {
            s[j][token] =  spect[j];
            if (s[j][token] > mx) mx = s[j][token];
            s[j][token] =  pow(log(1+s[j][token]),.4);
        }
    }
    m=HUGE_VAL;
    for (i = 0; i < freqs; i++)   for (j=0; j < frames; j++) {
        if (s[i][j] > mmax) mmax = s[i][j];
        if (s[i][j] < mmin) mmin = s[i][j];
    }
    spread = mmax - mmin;
    strcpy(file,user_dir);
    strcat(file,SPECT_DISC_FILE);
    // fp = fopen(SPECT_DISC_FILE,"wb");
    fp = fopen(file,"wb");
    for (j=0; j < frames; j++) {
        for (i = 0; i < SPECT_HT; i++) {
            z = s[SPECT_HT-i-1][j];
            
            v = (int) (256*( z- mmin)/spread);
            col[i] = v;
        }
        fwrite(col,sizeof(unsigned char)*SPECT_HT,1,fp);
    }
    fclose(fp);
    free(mem);
    
}



get_exper_rgb_spect() {
    //  get_exper_rgb_spect_mmap();  // may want to switch this back to mem mapping ...
    //  get_exper_rgb_spect_nommap();
    get_exper_rgb_spect_to_disc();  // may want to switch this back to mem mapping ...
}


static void
add_ruler_marker(int index, int is_solo, int meas_index, int frames) {
    RATIONAL r;
    
    if (ruler.num >= MAX_MARKERS) { printf("no room to add marker\n"); exit(0);}
    ruler.el[ruler.num].index = index;
    ruler.el[ruler.num].is_solo = is_solo;
    ruler.el[ruler.num].meas_index = meas_index;
    ruler.el[ruler.num].frames = frames;
    if (is_solo) r = score.solo.note[index].wholerat;
    else r = score.midi.burst[index].wholerat;
    ruler.el[ruler.num].wholerat = r;
    ruler.num++;
}


static int
ruler_comp(void *p1, void *p2) {
    RULER_EL *r1,*r2;
    int c;
    
    r1 = (RULER_EL *) p1;
    r2 = (RULER_EL *) p2;
    c = rat_cmp(r1->wholerat,r2->wholerat);
    if (c != 0) return(c);
    return ((r1->is_solo) ? -1 : 1);  // if both choose the solo
}

static int
ruler_mark_comp(void *p1, void *p2) {
    RULER_EL *r1,*r2;
    
    r1 = (RULER_EL *) p1;
    r2 = (RULER_EL *) p2;
    return(r1->mark - r2->mark);
}


static void
new_init_ruler() {
    int i,j,l,r,ls,rs,lm,rm,first_meas,last_meas,is_solo,lf,rf,f;
    RATIONAL lrat,rrat,wr;
    float xl,xr,xc,p;
    
    ruler.num = 0;
    set_accomp_range();
    for (first_meas=1; first_meas <= score.measdex.num; first_meas++)
        if (rat_cmp(score.measdex.measure[first_meas].wholerat,start_pos) >= 0) break;
    for (last_meas = score.measdex.num; last_meas >= 1; last_meas--)
        if (rat_cmp(score.measdex.measure[last_meas].wholerat,end_pos) <= 0) break;
    for (i=first_meas; i <= last_meas; i++) {
        wr = score.measdex.measure[i].wholerat;
        j = coincident_solo_index(wr);
        if (j != -1 && score.solo.note[j].frames > 0) {
            add_ruler_marker(j,is_solo=1, i,0);
            continue;
        }
        j = coincident_midi_index(wr);
        if (j != -1) { add_ruler_marker(j,is_solo=0, i,score.midi.burst[j].frames); continue; }
        ls = left_solo_index(wr);
        lm = left_midi_index(wr);
        if (rat_cmp(score.solo.note[ls].wholerat,score.midi.burst[lm].wholerat) < 0) {
            lrat = score.midi.burst[lm].wholerat;
            lf = score.midi.burst[lm].frames;
        }
        else {
            lrat = score.solo.note[ls].wholerat;
            lf = score.solo.note[ls].frames;
        }
        rs = rite_solo_index(score.measdex.measure[i].wholerat);
        rm = right_midi_index(score.measdex.measure[i].wholerat);
        if (rat_cmp(score.solo.note[rs].wholerat,score.midi.burst[rm].wholerat) < 0) {
            rrat = score.solo.note[rs].wholerat;
            rf = score.solo.note[rs].frames;
            
        }
        else {
            rrat = score.midi.burst[rm].wholerat;
            rf = score.midi.burst[rm].frames;
        }
        xl = lrat.num/(float)lrat.den;
        xr = rrat.num/(float)rrat.den;
        xc = wr.num/(float)wr.den;
        p = ((xc-xl)/(xr-xl));
        f = p*rf + (1-p)*lf;
        add_ruler_marker(0,is_solo=0, i,f);
    }
}

static void
init_ruler() {
    int i,j,is_solo,b,frame;
    RATIONAL r,q;
    char s[100];
    
    new_init_ruler(); return;
    
    ruler.num = 0;
    set_accomp_range();
    for (i=firstnote; i <= lastnote; i++)
        if (score.solo.note[i].frames) add_ruler_marker(i,is_solo=1,0,0);
    for (i=first_accomp; i <= last_accomp; i++) {
        /*r = score.midi.burst[i].wholerat;
         q = wholerat2measrat(r);
         if (q.num == 0)*/ add_ruler_marker(i,is_solo=0,0,0);
    }
    qsort(ruler.el,ruler.num,sizeof(RULER_EL),ruler_comp);
    
    
    
    
    for (i=0; i < ruler.num; i++) {
        b = (i > 0 && rat_cmp(ruler.el[i].wholerat,ruler.el[i-1].wholerat) == 0);
        ruler.el[i].mark = (b) ? 1 : 0;
    }
    qsort(ruler.el,ruler.num,sizeof(RULER_EL),ruler_mark_comp);
    
    for (i=0; i < ruler.num; i++) if (ruler.el[i].mark) break;
    ruler.num = i;
    qsort(ruler.el,ruler.num,sizeof(RULER_EL),ruler_comp);
    
    /*  for (i=0; i < ruler.num; i++) {
     wholerat2string(ruler.el[i].wholerat,s);
     j = ruler.el[i].index;
     is_solo = ruler.el[i].is_solo;
     frame = (is_solo) ? score.solo.note[j].frames : score.midi.burst[j].frames;
     printf("%d\t%-15s\tmark=%d\tis_solo=%d\tj = %d\tframe = %d\n",i,s,ruler.el[i].mark, ruler.el[i].is_solo,j,frame);
     }
     fflush(stdout);
     exit(0);*/
    
}


void
show_spect() {
    Byte *ptr;
    int f,rows,cols,i,width,edge,f0;
    unsigned char *rgb;
    char comment[1000];
    float m=0;
    //  TRect rect;
    
    
    init_ruler();
    //  draw_piano();
    //  enable_spect_buttons();
    ensemble_in = 0;
    current_parse_changed_by_hand=0;
    //  Spect->AccompRadioGroup->ItemIndex = 1;
    //  [[[NSApplication sharedApplication] delegate] chooseAccompRadio:@"Out"];
    
    //  Spect->AccompRadioGroup->Refresh();
    // clicks_in = 1;
    //  Spect->MarkersRadioGroup->ItemIndex = 0;
    show_rests = 1;
    // spect_inc.num = 0; spect_inc.den = 1;
    last_frame = 0;
    save_audio_values();
    currently_playing = 0;
    mode = 0;  /* kludge since listen balks for this is set to synth mode */
    cur_nt = firstnote;
    scroll_to_left();
    rows = SPECT_HT; cols = frames;
    //	if (cols < spectrogram->Width) {
    //	        rect = TRect(cols,0,spectrogram->Width,SPECT_HT);  /* *old* spectrogram width */
    //	        Spect->Image1->Canvas->FillRect(rect);  /* restore background */
    //    }
    // spectrogram->Height = freqs;  spectrogram->Width = frames;
    get_exper_rgb_spect();
    get_spect_page(0, spect_wd, &spect_page);
    //      init_visible();
    f0 = (cur_nt < firstnote) ? 0 : score.solo.note[cur_nt].frames;
    
    //draw_ruler();
    
    //  add_line_to_visible(f0,COLOR_GREEN);
    //	  Spect->Image1->Picture->Graphic = visible;  // for some reason this works when  Spect->Image1->Canvas->Draw(0,0,visible); doesn't
    
    printf("max audio level = %f\n",max_audio_level());
    printf("done with guts\n");
    background_info.fraction_done = 1;
    
}








#define MAX_PLUCK_DECAY .00020  // this is "click"
#define MIN_PLUCK_DECAY .00005  // this is "pluck"
#define PLUCK_DECAY .00015   // higher means faster decay
#define PLUCK_HARMONICS 7

#define PLUCK_TAB_LEN 10000
static float *plucktab = NULL;


#define MAX_PLUCK 10


typedef struct {
    float amp;
    float phase;
    float delta;
    float hz;
} PLUCK_STATE;

typedef struct {
    int num;
    PLUCK_STATE pluck[MAX_PLUCK];
} ALL_PLUCK_STATE;

static ALL_PLUCK_STATE all_pluck_state;



static void
del_pluck(int j) {
    all_pluck_state.pluck[j] = all_pluck_state.pluck[--all_pluck_state.num];
}

static void
add_pluck(int m, float amp) {
    PLUCK_STATE *p;
    int i,mini;
    float mina=HUGE_VAL;
    
    if (all_pluck_state.num == MAX_PLUCK) {
        for (i=0; i < all_pluck_state.num; i++) {
            p = all_pluck_state.pluck + i;
            if (p->amp < mina) { mina = p->amp; mini = i; }
        }
        del_pluck(mini);
    }
    
    p = all_pluck_state.pluck + all_pluck_state.num;
    all_pluck_state.num++;
    p->phase = 0;
    //  p->amp = .01;
    //  p->amp = .005*master_volume;
    p->amp = amp;
    p->hz = 440*pow(2.,(m-69)/12.);
    p->delta = 2*PI*(p->hz)/(float)NOMINAL_OUT_SR;
}





static void
init_plucktab() {
    int i,h;
    float x;
    
    if (plucktab != NULL) return;
    plucktab = (float *) malloc(PLUCK_TAB_LEN*sizeof(float));
    for (i=0; i < PLUCK_TAB_LEN; i++) plucktab[i] = 0;
    for (i=0; i < PLUCK_TAB_LEN; i++) {
        x = i / (float) PLUCK_TAB_LEN;
        for (h=1; h <= PLUCK_HARMONICS; h++) plucktab[i] += sin(2*PI*h*x);
    }
}




static int
range2index(int lo, int hi) {
    int i,hf;
    
    for (i = firstnote; i <= lastnote; i++) {
        hf = (score.solo.note[i].frames-start_play_frame_num)*SKIPLEN*IO_FACTOR;
        if (hf >= lo && hf < hi) return(i);
    }
    return(-1);
}



static int
gs_is_note_on(MIDI_EVENT *p) {
    return ( ((p->command&0xf0) == NOTE_ON) && (p->volume > 0));
}



static int
is_marked_range(int lo, int hi, int *which, int *note) {  // these are 48Khz frames
    int i,hf,ii;
    
    *which = *note = -1;
    if (clicks_in == 0) return(0);
    i = range2index(lo,hi);
    if (i == -1) return(0);
    if (show_index(i) == 0) return(0);
    hf = (score.solo.note[i].frames-start_play_frame_num)*SKIPLEN*IO_FACTOR;
    if (lo > hf || hf >= hi) { printf("this shouldn't happen here\n"); exit(0); }
    *which = hf;
    *note = i;
    return(1);
}



static void
generate_pluck_buff(float *buff, float *solo, int played, int n) {
    int i,j,k,h,which,note,event,m;
    PLUCK_STATE *p;
    float decay,t;
    static float loudness=.02;
    
    
    for (i=t =0; i < n; i++) t += (solo[i]*solo[i]);
    loudness = .99*loudness + .01*sqrt(t/n); // this is a good idea, sometimes miss plucks
    
    decay = PLUCK_DECAY;
    for (i=0; i < n; i++) buff[i] = 0;
    //  if (is_marked_range(played, played+n,&which,&note))  for (k=0; k < 10; k++) buff[k+which-played] = /*.005*/0*master_volume;  // .05
    event = is_marked_range(played, played+n,&which,&note);
    if (event && use_pluck == 0)
        for (k=0; k < 10; k++) buff[k+which-played] = 4*loudness;  // .05
    //  else which = -1;
    if (use_pluck == 0) return;
    for (i=0; i < n; i++) {
        if (i == which-played) {
            for (k=0; k < score.solo.note[note].action.num; k++)
                if (gs_is_note_on(score.solo.note[note].action.event+k))
                    add_pluck(score.solo.note[note].action.event[k].notenum,.5*loudness);
        }
        
        for (j=0; j < all_pluck_state.num; j++) {
            p = all_pluck_state.pluck+j;
            /*      for (h=1; h <= PLUCK_HARMONICS; h++) {
             if (h*p->hz > NOMINAL_OUT_SR/2) break;
             buff[i] += p->amp*sin(h*p->phase);
             }*/
            m = (int) PLUCK_TAB_LEN*p->phase/(2*PI);
            buff[i] += p->amp*plucktab[m];
            p->phase += p->delta;
            if (p->phase > 2*PI) p->phase -= 2*PI;
            //      p->amp *= PLUCK_DECAY;
            //      p->amp -= PLUCK_DECAY*p->amp;
            p->amp -= decay*p->amp;
            if (p->amp < .0001) del_pluck(j);
        }
    }
}



static void
init_pluck() {
    all_pluck_state.num = 0;
    init_plucktab();
}


static void
buffer_hires_audio() {
    char stump[500];
    int f,i,j,b,n;
    
    if (hires_solo_fp != NULL)
        fclose(hires_solo_fp);
    strcpy(stump,audio_data_dir);
    
    if (play_synthesis == 1) {
        strcat(stump, "synth_pitch");
    }
    else {
        strcat(stump,current_examp);
        strcat(stump,".48k");
    }
    
    hires_solo_fp = fopen(stump,"rb");
    //  if (hires_solo_fp == NULL) hires_solo_fp = fopen(HIRES_OUT_NAME,"rb");
    if (hires_solo_fp == NULL) { is_hires_audio = 0; return; }
    is_hires_audio = 1;
    fseek(hires_solo_fp,start_play_frame_num*SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE,SEEK_SET);
    if (hires_orch_fp != NULL)
        fclose(hires_orch_fp);
    strcpy(stump,audio_data_dir);
    strcat(stump,current_examp);
    strcat(stump,".48o");
    hires_orch_fp = NULL;
    hires_orch_fp = fopen(stump,"rb");
    if (hires_orch_fp == NULL) { printf("yaya couldn't open %s\n",stump); return; /*exit(0); */}
    fseek(hires_orch_fp,start_play_frame_num*SKIPLEN*IO_FACTOR*BYTES_PER_SAMPLE,SEEK_SET);
}


#define SIM_UP_BUFF (IO_FACTOR*256)

void
sim_upsample_read(int samps, int frame, unsigned char *solo, unsigned char *orch) {
    /* asio introduces an awkward problem since it will write audio in fixed buffer size chunks that will have nothing
     to do with our frame length.  This routine simulates what fread does, but it uses low res samples and produces
     hi res ones.
     */
    static int seam,cur;  // sample indexes to circular buffer
    static int dex; // index to audiodata and orchdata
    static unsigned char orchbuff[SIM_UP_BUFF*BYTES_PER_SAMPLE],solobuff[SIM_UP_BUFF*BYTES_PER_SAMPLE];  // hi-res circ buffers
    int avail,n,i;
    
    if (frame == 0) {
        dex =  SKIPLEN*start_play_frame_num; // next lo res sample for audiodata and orchdata
        seam = cur = 0;
    }
    avail = cur-seam;
    if (avail < 0) avail = SIM_UP_BUFF-avail;
    n = ((samps-avail)/IO_FACTOR) + 1;  // number of low res samples to bring in
    for (i = 0; i < n; i++, dex++, cur = (cur+IO_FACTOR)%SIM_UP_BUFF) {
        upsample_audio(audiodata + dex*BYTES_PER_SAMPLE, solobuff + BYTES_PER_SAMPLE*cur, 1, IO_FACTOR, 1);
        upsample_audio( orchdata + dex*BYTES_PER_SAMPLE, orchbuff + BYTES_PER_SAMPLE*cur, 1, IO_FACTOR, 1);
        if (i > 0 && cur == seam) { printf("this is not good\n"); exit(0); }
    }
    if (cur == seam) { printf("this is not good\n"); exit(0); }
    for (i=0; i < samps; i++,seam = (seam+1)%SIM_UP_BUFF) {
        memcpy(solo + i*BYTES_PER_SAMPLE, solobuff + seam*BYTES_PER_SAMPLE, BYTES_PER_SAMPLE);
        memcpy(orch + i*BYTES_PER_SAMPLE, orchbuff + seam*BYTES_PER_SAMPLE, BYTES_PER_SAMPLE);
    }
}



void
init_read_buffer(READ_BUFFER *rb, FILE *fp) {
    rb->fp = fp;
    if (rb->buff == NULL)  rb->buff = (unsigned char *) malloc(READ_BUFFER_SIZE);
    rb->is_empty = 1;
    rb->cur = rb->seam = 0;
    
}

void
fill_read_buffer(READ_BUFFER *rb) {
    int d,i=0;
    
    if (rb->seam == rb->cur && rb->is_empty == 0) return; // buffer full
    do {
        d = fread(rb->buff + rb->seam,1,1,rb->fp);
        if (d == 0) rb->buff[rb->seam] = 0;  // always fill with something even if file done
        (rb->seam)++;
        if (rb->seam == READ_BUFFER_SIZE) rb->seam = 0;
        i++;
    } while (rb->cur != rb->seam);
    rb->is_empty = 0;
}


void
drain_read_buffer(READ_BUFFER *rb, int n, unsigned char *buff) {
    int i;
    
    for (i=0; i < n; i++) {
        if (rb->is_empty) {
            buff[i] = 0; // no more data to read
            NSLog(@"buffer drained to 0!!!!");
        }
        else {
            buff[i] = rb->buff[rb->cur];
            (rb->cur)++;
            if (rb->cur == READ_BUFFER_SIZE) rb->cur = 0;
            if (rb->seam == rb->cur) rb->is_empty = 1;
        }
    }
}


/*
 void
 fill_read_buffer(READ_BUFFER *rb) {
 int n;
 
 if ((seam+1)%READ_BUFFER_SIZE == cur) return;  // all full
 if (seam+1 < cur) {
 n = fread(rb->buff + rb->seam + 1, cur-seam-1,1,fp);
 seam += n;
 return;
 }
 n = fread(rb->buff + rb->seam + 1, READ_BUFFER_SIZE-seam-1,1,fp);
 rb->seam += n;
 if (rb->cur == 0 || rb->seam < (READ_BUFFER_SIZE-1)) return;
 n = fread(rb->buff, cur-1,1,fp);
 rb->seam = n;
 }
 */

#define BAS_BUFF 4096

void
buffer_audio_samples(int samps, int played) {  // write the next samps audio samples read from from the files
    /* this should take the place of buffer_one_audio_frame() (to avoid parallel versions) */
    unsigned char buff[BAS_BUFF], solo_upbuff[BAS_BUFF], orch_upbuff[2*BAS_BUFF], upbuff[2*BAS_BUFF];
    int offset,j,b,ff,n,chan,which,note;
    float t;
    static int cur,hipos,lopos;
    float fsolo[BAS_BUFF],forch[BAS_BUFF],lmix[BAS_BUFF],rmix[BAS_BUFF],pluck[BAS_BUFF];
    
    //  offset = BYTES_PER_FRAME*(start_play_frame_num + play_frames_buffered);
    //  if (mode != TEST_BALANCE_MODE) samples2floats(audiodata+offset, data, FRAMELEN);  // this seems to be just for the volume meter
    if (samps > BAS_BUFF) { printf("can't deal with this many samps here\n"); exit(0); }
    ff = start_play_frame_num + play_frames_buffered;
    b = samps*BYTES_PER_SAMPLE;
    if (is_hires_audio) {
        t = now();
        
        /*                fread(solo_upbuff , b, 1, hires_solo_fp);
         if (hires_orch_fp) fread(orch_upbuff , b, 1, hires_orch_fp);  // do I need to check the fp's?**/
        
        drain_read_buffer(&solo_read_buff, b, solo_upbuff);
        drain_read_buffer(&orch_read_buff, b, orch_upbuff);
        
        
        
        if ((now()-t) > .01) printf("delay of %f reading audio from disk\n",now()-t);
    }
    else sim_upsample_read(samps, played,solo_upbuff,orch_upbuff);
    samples2floats(solo_upbuff,fsolo,samps);
    samples2floats(orch_upbuff,forch,samps);
    generate_pluck_buff(pluck, fsolo, played, samps);
    /* if (ensemble_in) {
     if (clicks_in) add_channels(pluck, fsolo, fsolo, samps);
     float_mix(fsolo,forch,lmix,rmix,samps);
     }
     else  float_mix(fsolo,pluck,lmix,rmix,samps); */
    triple_float_mix(fsolo,forch,pluck,lmix,rmix,samps);
    
    floats2samples(lmix,solo_upbuff,samps);  // these aren't really solo and orch, but rather left and right
    floats2samples(rmix,orch_upbuff,samps);
    interleave_channels(solo_upbuff, orch_upbuff, upbuff, samps);
    n = write_samples(upbuff, samps);
}






void
buffer_audio() {
    
    if (using_asio)  return;
    while (ready_for_play_frame(0)) {
        buffer_audio_samples(IO_FACTOR*TOKENLEN, play_frames_buffered*IO_FACTOR*TOKENLEN);
    }
}


static void
jump_to_end() {
    int i;
    
    for (i=firstnote; i <= lastnote; i++) if (score.solo.note[i].frames > last_frame) break;
    if (i > firstnote) i--;
    highlight_neighbor_note(i-cur_nt);
    
}


static int
is_frame_a_note(int f, int *n) {
    int lo,hi,md,i;
    
    
    for (i= firstnote; i <= lastnote; i++)
        if (f == score.solo.note[i].frames) { *n = i; return(1); }
    return(0);
}


static int
is_marked_frame(int f, int *n) {
    int i;
    
    
    //  if (f == 0 ) { *n = firstnote-1; return(1); }
    if (f == 0 ) { *n = firstnote-1; return(clicks_in); }
    for (i= firstnote; i <= lastnote; i++) {
        // if (f == score.solo.note[i].frames && (show_index(i)) && (clicks_in == 1))
        if (f == score.solo.note[i].frames && (show_index(i)) && (clicks_in || i == cur_nt))
        { *n = i; return(1); }
    }
    return(0);
    
    return(is_frame_a_note(f,n) && (show_index(*n)) && (clicks_in == 1));
}





void
repair_line_to_visible(int column) {
    int n,f,c;
    
    if (is_marked_frame(column,&n)) {
        c = line_color(n);
        add_line_to_visible(column, c);
    }
    else  restore_line_to_visible(column);
}



void
stop_playing() {
    int draw_frame,i;
    
    last_frame = start_play_frame_num + play_frames_played() + 1;
    if (mode != SYNTH_MODE && currently_playing == 0) return;
    draw_frame = cur_play_frame_num;
    
    
    for (i=0; i < 2; i++)   repair_line_to_visible(draw_frame-i);
    // NEED THIS BACK (took out for synthesize test)
    // not sure about loop being needed
    
    
    /*  for (i=0; i < SLICE_COLS; i++)   repair_line_on_bitmap(draw_frame-i,SLICE_COLS-1-i,vert_line_pair);
     Spect->Image1->Canvas->Draw(draw_frame-scroll_pos-(SLICE_COLS-1),0,vert_line_pair);*/
    currently_playing = 0;
    //  performance_interrupted = 1; // added this in MAC:  maybe this shoudl take the place of currently_playing
    //  blank_live_button();
    /*  [nsTimer invalidate];
     nsTimer   = nil;*/
    end_playing();
}




static void
stop_action() {
    if (mode == MIDI_MODE) { performance_interrupted = 1; return; }
    if (mode == SYNTH_MODE){  performance_interrupted = 1; /*end_playing();*/  }
    stop_playing();
}

void
quit_action() { // quit playing
    [nsTimer invalidate];
    nsTimer = nil;
    //   [[[NSApplication sharedApplication] delegate] blank_live_button];
    NSLog(@"stopped the timer");
    stop_action();
}

void
play_action() {
    int ret,f0;
    
    if (frames == 0) return;
    if (currently_playing || live_locked) return;
    
    //[NSThread detachNewThreadSelector:@selector(aMethod:) toTarget:[MyObject class] withObject:nil];
    mode = BASIC_PLAY_MODE;  // anyhting other than SYNTH_MODE
    init_pluck();
    if (audio_type == ON_SOLO) f0 = (cur_nt < firstnote) ? 0 : score.solo.note[cur_nt].frames;
    cur_play_frame_num = start_play_frame_num =
    (audio_type == ON_SOLO) ?  f0 :  score.midi.burst[cur_nt].frames;
    buffer_hires_audio();  // sets is_hires_audio flag as appropriate  (this doesn't seem to buffer, but rather just fseek to right place)
    /*  if (prepare_playing(NOMINAL_OUT_SR) == 0) {
     message->Label1->Caption = "Couldn't access audio device.  If using external device under ASIO, make sure it is connected to computer";
     message->Show();
     return;
     }*/
    currently_playing = 1;
    init_clock();
    // buffer_audio();  // buffer as many frames as buffer holds
    //  Spect->PlayTimer->Enabled = 1;
    
    /*    if (nsTimerRef) {
     [nsTimerRef invalidate];
     nsTimerRef   = nil;
     }*/
    /*nsTimerRef   = */
    
    // prepare_playing();
    
    init_read_buffer(&solo_read_buff, hires_solo_fp);  fill_read_buffer(&solo_read_buff);
    init_read_buffer(&orch_read_buff, hires_orch_fp);  fill_read_buffer(&orch_read_buff);
    start_coreaudio_play();
    //  [NSThread detachNewThreadSelector:@selector(playAnim:) toTarget:[MyNSView class] withObject:nil];
    [NSThread detachNewThreadSelector:@selector(playRead:) toTarget:[MyNSView class] withObject:nil];
    
    
    if (nsTimer) NSLog(@"timer already running ...");
    
    /* NB .03 gives smoother animation than .01, but don't really know
     why.  Need to draw frame about every .031 secs, but this seems
     to run the risk of getting behind */
    nsTimer =  [NSTimer scheduledTimerWithTimeInterval:0.03
                                                target:view_self
                                              selector:@selector(play_timer)
                                              userInfo:NULL
                                               repeats:YES];
    
    
    
    
}








static void
update_spect_image(int draw_frame) {
    int width,color,i,start;
    
    add_line_to_visible(draw_frame, MOVING_LINE_COLOR);
    
    repair_line_to_visible(draw_frame-1);
    
    
    //  for (i=0; i < SLICE_COLS-1; i++)  if (draw_frame - i - 1 >= 0) repair_line_on_bitmap(draw_frame-1-i,SLICE_COLS-2-i,vert_line_pair);
    scroll_if_needed(draw_frame+1,1);
    
    /*  width = Spect->ScrollBox1->ClientRect.Right;
     if (draw_frame < scroll_pos) {printf("return premature in update_spect_image\n"); return; }
     if (draw_frame >= scroll_pos+width) {printf("return premature in update_spect_image\n"); return; }
     Spect->Image1->Canvas->Draw(draw_frame-scroll_pos-(SLICE_COLS-1),0,vert_line_pair);*/
}





int  /* returns boolean if continuing */
gui_spect_play_function() {
    float next_frame,call_time,interval,t;
    unsigned char buff[(TOKENLEN+1)*BYTES_PER_SAMPLE*2];
    unsigned char upbuff[IO_FACTOR*TOKENLEN*BYTES_PER_SAMPLE*2];
    unsigned char orch_upbuff[IO_FACTOR*TOKENLEN*BYTES_PER_SAMPLE*2];
    unsigned char solo_upbuff[IO_FACTOR*TOKENLEN*BYTES_PER_SAMPLE*2];
    char comment[500];
    int frame_diff,offset,draw_frame,j,b,f,k,n,i,play_frame;
    
    
    play_frame = start_play_frame_num + play_frames_played();  // this change made 12-09
    if (play_frame >= frames-1) {
        //    stop_playing();
        //     [[[NSApplication sharedApplication] delegate] StopPlayingAction:nil];
        //quit_action(0);  // stop timer as well
        return(0);
    }
    if (currently_playing == 0) return(0);
    offset = BYTES_PER_FRAME*(start_play_frame_num + play_frames_played());
    if (mode != TEST_BALANCE_MODE) samples2floats(audiodata+offset, data, FRAMELEN);  // this seems to be just for the volume meter
    
    //if (using_asio == 0)   buffer_audio_samples(IO_FACTOR*TOKENLEN, play_frames_buffered*IO_FACTOR*TOKENLEN);
    //    if ((play_frames_played()%5) == 0) [[NSApp delegate] setVolumeMeter];
    
    //  NSSlider *xxx = [[NSApp delegate]
    //  [[NSApp delegate] setVolumeMeter:val 50];
    
    
    // if ((play_frames_played()%5) == 0) [[NSApp delegate] setVolumeMeter];
    
    
    //  [UIApplication sharedApplication];
    //[AppDelegate showVolMeter];  // change made 12-09 to accomodate asio
    draw_frame = cur_play_frame_num;
    update_spect_image(draw_frame);
    cur_play_frame_num++;
    return(1);
}





@implementation MyNSView

-(void) get_spect_page_no_args {
    get_spect_page(scroll_pos, spect_wd, &spect_page);
}


- (id)initWithFrame:(NSRect)pFrameRect {
    
    if (! (self = [super initWithFrame:pFrameRect])) {
        NSLog(@"MyView initWithFrame *Error*");
        return self;
    } // end if
    
    nsRectFrameRect   = pFrameRect;
    
    return self;
}

- (void)awakeFromNib {
    unsigned char *ptr;
    
    
    // [[self window] setAcceptsMouseMovedEvents:YES];  // think more carefully about this since don't want to flood computation with unneeded events
    [self setAcceptsTouchEvents:YES];  // maybe only want this in more limited context, like for notation selectng
    
#ifdef NOTATION
    bigBitmap = [[NSBitmapImageRep alloc]
                 initWithBitmapDataPlanes:NULL
                 pixelsWide: MAX_SPECT_WIDTH
                 pixelsHigh:  NOTATION_BUFFER_SIZE
                 bitsPerSample:8
                 samplesPerPixel:3
                 hasAlpha:NO
                 isPlanar:NO
                 colorSpaceName:@"NSCalibratedRGBColorSpace"
                 bytesPerRow:0
                 bitsPerPixel:0];
#else
    bigBitmap = [[NSBitmapImageRep alloc]
                 initWithBitmapDataPlanes:NULL
                 pixelsWide: nsRectFrameRect.size.width
                 pixelsHigh: nsRectFrameRect.size.height
                 bitsPerSample:8
                 samplesPerPixel:3
                 hasAlpha:NO
                 isPlanar:NO
                 colorSpaceName:@"NSCalibratedRGBColorSpace"
                 bytesPerRow:0
                 bitsPerPixel:0];
    
#endif
    
    
    
    cgFloatRed         = 1.0;
    cgFloatRedUpdate   = 0.02;
    spect_wd = nsRectFrameRect.size.width;
    SPECT_HT = nsRectFrameRect.size.height;
    bitmap = bigBitmap;
    view_self = self;
    
    
    
} // end awakeFromNib








- (void) displaySpect {
    
    //  [[[NSApplication sharedApplication] delegate] showMainWindow];
    
    [[[NSApplication sharedApplication] delegate] backgroundTask:show_spect Text:"Computing Spectrogram" Indeterminate:FALSE];
    
    [[[NSApplication sharedApplication] delegate] enableMarkerControls];
    
    //    show_spect();
    [self show_the_spect];
}




-(void) redraw_markers {
    if (clicks_in == 0) return;
    [self show_the_spect];
    
    //  init_visible();
    //  Spect->Image1->Canvas->Draw(0,0,visible);
    
}



- (IBAction) startAnimation {
    
    
    show_spect();
    [self show_the_spect];
    return;
    
    
    if (nsTimerRef) {
        [nsTimerRef invalidate];
        nsTimerRef   = nil;
    }
    nsTimerRef   = [NSTimer scheduledTimerWithTimeInterval:0.05
                                                    target:self
                                                  selector:@selector(paintGradientBitmap)
                                                  userInfo:NULL
                                                   repeats:YES];
    
} // end startAnimation


- (void) fillImageRect: (RECT) src val: (int) val {
    int i,j,k,bpr;
    Byte *ptr,*row;
    
    ptr = [bigBitmap bitmapData];
    bpr = [bigBitmap bytesPerRow];
    for (i=0; i < src.height; i++) {
        row = ptr + (i+src.loc.i)*bpr;
        for (j=0; j < src.width; j++)
            for (k=0; k < 3; k++) row[3*(src.loc.j+j)+k] = val;
    }
    [self setNeedsDisplay:YES];
}





static void
highlight_image_rect(RECT src) {
    int i,j,k,bpr;
    Byte *ptr,*row;
    
    
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    for (i=0; i < src.height; i++) {
        row = ptr + (i+src.loc.i)*bpr;
        for (j=0; j < src.width; j++)
            for (k=0; k < 1; k++) row[3*(src.loc.j+j)+k] = 255;  // only turn r (?) byte on
    }
}


static void
add_dotted_line(int h) {
    Byte *ptr,*row;
    int i,k,bpr;
    
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    row = ptr + h*bpr;
    for (i=0; i < spect_wd; i += 2) for (k=0; k < 3; k++) row[3*i +k] = 0;
    
}


#define MAX_DST_WD  2000
#define MAX_DST_HT  600

- (void) copyBitmap: (NSBitmapImageRep *) im source:(RECT) src  dest:(RECT) dst {
    unsigned char *ptr1,*row1,*ptr2,*row2;
    int i,j,k,ii,jj,bpr1,bpr2;
    float irat,jrat;
    short int count[MAX_DST_HT][MAX_DST_WD];
    int result[MAX_DST_HT][MAX_DST_WD];
    
    if (dst.width > MAX_DST_WD || dst.height > MAX_DST_HT) {
        NSLog(@"bad height or with in copyBitmap ht = %d wd = %d",dst.height,dst.width);
        exit(0);
    }
    if (src.height < dst.height || src.width < dst.width) {
        NSLog(@"dst rect must be smaller than src rect");
        exit(0);
    }
    for (i=0; i < dst.height; i++) for (j=0; j < dst.width; j++)
        count[i][j] = result[i][j] = 0;
    ptr1 = [im bitmapData];
    bpr1 = [im bytesPerRow];
    ptr2 = [bigBitmap bitmapData];
    bpr2 = [bigBitmap bytesPerRow];
    for (i=0; i < src.height; i++) {
        row1 = ptr1 + (i+src.loc.i)*bpr1;
        ii = i*dst.height/src.height;
        for (j=0; j < src.width; j++) {
            jj = j*dst.width/src.width;
            result[ii][jj] += row1[3*(src.loc.j+j)];
            count[ii][jj]++;
        }
    }
    for (ii=0; ii < dst.height; ii++) {
        row2 = ptr2 + (dst.loc.i+ii)*bpr2;
        for (jj=0; jj < dst.width; jj++) {
            for (k=0; k < 3; k++) row2[3*(dst.loc.j+jj) + k] =
                result[ii][jj] / count[ii][jj];
        }
    }
    [self setNeedsDisplay:YES];
    
}




- (IBAction) stopAnimation:(id)pId {
    [nsTimerRef invalidate];
    nsTimerRef   = nil;
    
} // end stopAnimation



-(void) jumpToMeas:(int)meas {
    int i,j;
    
    for (i=1; i < score.measdex.num; i++) if (score.measdex.measure[i].meas >= meas) break;
    //  if (i > score.measdex.num) i = score.measdex.num; //return;
    j = lefteq_solo_index(score.measdex.measure[i].wholerat);
    for (j=firstnote; j <= lastnote; j++) {
        if (rat_cmp(score.solo.note[j].wholerat,score.measdex.measure[i].wholerat) > 0) break;
        if (score.solo.note[j].frames <= 0) break;
    }
    j--;
    // highlight_neighbor_note(j-cur_nt);
    highlight_note_with_index(j);
}

-(int) current_note {
    return cur_nt;
}


- (void) play_timer {
    int done,draw_frame,f,t;
    float p;
    static int locked=0;
    static int last_cur_note;
    
    // NSLog(@"timer callback cur_play_frame_num = %d",cur_play_frame_num);
    //  NSLog(@"timer");
    
    //  if (mode == SYNTH_MODE) { synth_timer(); return; }
    //  if (mode == PV_MODE) { pv_timer(); return; }
    if (currently_playing == 0) return;
    if (locked) { NSLog(@"timer locked out"); return; }
    locked = 1;
    
    
    f = cur_play_frame_num - start_play_frame_num;
    if (ready_for_play_frame(f)) {  // only 1 iteration per timer call
        done =  gui_spect_play_function();
        // [view_self setNeedsDisplay:YES];
    }
    
    t = play_frames_played();
    if (abs(t -f) > 5) {
        // in case the playing gets out of sync as with resize of window
        repair_line_to_visible(cur_play_frame_num-1);
        cur_play_frame_num = start_play_frame_num + t;
        NSLog(@"out of sync");
    }
    
    /*  while (1) {
     f = cur_play_frame_num - start_play_frame_num;
     if (ready_for_play_frame(f) == 0) break;
     gui_spect_play_function();
     }*/
    
    
    
    if (mode != SYNTH_MODE) { locked = 0; return; }
    
    
    
    if (last_cur_note != cur_note && score.solo.note[cur_note-1].treating_as_observed) {  //detected new note in Listen
        draw_frame = (int) secs2tokens(score.solo.note[cur_note-1].on_line_secs);
        add_mark_to_visible(draw_frame, COLOR_GREEN,0,25);
        last_cur_note = cur_note;
    }
    
    p = secs2tokens(score.midi.burst[cur_accomp].ideal_secs);
    if (p == HUGE_VAL) { locked = 0; return; }
    t = (int) secs2tokens(score.midi.burst[cur_accomp].ideal_secs);
    if (t != prediction_target[cur_accomp]) {
        add_mark_to_visible(prediction_target[cur_accomp], COLOR_BLACK,25,50);
        add_mark_to_visible(t, COLOR_RED,25,50);
        prediction_target[cur_accomp] = t;
    }
    
    
    
    
    //   draw_frame = start_play_frame_num + play_frames_played();
    locked = 0;
}


- (void) paintGradientBitmap {
    // green and blue values are a function of
    // their position within the nsRectFrameRect.
    // red has the same value throughout the picture
    // but changes value with each iteration
    cgFloatRed   += cgFloatRedUpdate;
    if (cgFloatRed > 1.0) {
        cgFloatRed   = 1.0;
        cgFloatRedUpdate = cgFloatRedUpdate * (-1.0);
        cgFloatRed   += cgFloatRedUpdate;
    };
    
    if (cgFloatRed < 0.0) {
        cgFloatRed   = 0.0;
        cgFloatRedUpdate = cgFloatRedUpdate * (-1.0);
        cgFloatRed   += cgFloatRedUpdate;
    };
    
    CGFloat zFloatFrameHeight = (CGFloat)nsRectFrameRect.size.height;
    CGFloat zFloatFrameWidth  = (CGFloat)nsRectFrameRect.size.width;
    NSUInteger   zIntRed   = round(255.0 * cgFloatRed);
    NSInteger y;
    CGFloat   zFloatY;
    for (y = 0; y < nsRectFrameRect.size.height; y++) {
        zFloatY = (CGFloat)y;
        NSInteger x;
        CGFloat   zFloatX;
        NSUInteger zIntBlue = round((255.0 / zFloatFrameHeight) * zFloatY);
        
        for (x = 0; x < nsRectFrameRect.size.width; x++) {
            zFloatX = (CGFloat)x;
            NSUInteger zIntGreen = round((255.0/zFloatFrameWidth)*zFloatX);
            NSUInteger zColourAry[3] = {zIntRed,zIntGreen,zIntBlue};
            [bigBitmap setPixel:zColourAry atX:x y:y];
        } // end for x
        
    } // end for y
    
    
    [self setNeedsDisplay:YES];
    
} // end paintGradientBitmap


static void
abcde() {
    int i;
    NSUInteger zColourAry[3] = {255,0,0};
    for (i=0; i < SPECT_HT; i++)
        [bitmap setPixel:zColourAry atX:i y:100];
}

- (void) show_the_spect {
    NSUInteger r,g,b;
    int x,y,i;
    unsigned char *ptr;
    
    
    //  clear_piano_keys();
    
    make_spect_visible();
    [view_self markersChange];
    draw_ruler();
    draw_pitch(1);
    
    image_set = 1;
    // [self setNeedsDisplay:YES];
    [view_self setNeedsDisplayInRect:CGRectMake(0,0,spect_wd,SPECT_HT)];
    // [self setNeedsDisplay:YES];
    return;
    
    //    init_visible_page([bigBitmap bitmapData], spect_wd,SPECT_HT);
    //    ptr = [bigBitmap bitmapData];
    //    ptr = visible_page.buff;
    for (y = 0; y < nsRectFrameRect.size.height; y++) {
        for (x = 0; x < nsRectFrameRect.size.width; x++) {
            printf("y = %d x = %d val = %d\n",y,x,spect_page.ptr[x][y]);
            //      NSLog(spect_page.ptr[y][x]);
            r = g = b = spect_page.ptr[x][y];
            NSUInteger zColourAry[3] = {r,g,b};
            [bigBitmap setPixel:zColourAry atX:x y:y];
            //	    	    ptr = visible_page.scan_line[y] + 3*x;
            //	    ptr[0] = ptr[1] = ptr[2] = r;
        }
    }
    //  for (i=0; i < 100; i++) visible_page.buff[i*(spect_wd*3+8)] = 255;
    // abcde();
    highlight_notes();
    [self setNeedsDisplay:YES];
    
}

- (BOOL)acceptsFirstResponder
{
    //return NO; // this doesn't seem to do anything
    return YES;
}


-(void) viewDidEndLiveResize   {
    int h,w;
    PAGE_STAFF tp,bt;
    
#ifdef NOTATION
    tp = first_showing_line();
    fill_notation_scroll_buffer(tp.page,tp.staff);
    notation_scroll = 0;
    //    display_first_page();
    [view_self setNeedsDisplay: YES];
#endif
    [[[NSApplication sharedApplication] delegate] mainWindowResized];
    
    h =  [[[NSApplication sharedApplication] delegate] mainWindowHeight];
    w =  [[[NSApplication sharedApplication] delegate] mainWindowWidth];
    //  [Spectrogram frame];
    NSLog(@"end live resize height width %d %d",h,w);
}


void
pause_audio() {
    //stop_action();
    quit_action();
    jump_to_end();
}




static void
discard_tail() {
    int i,m;
    
    current_parse_changed_by_hand =1;
    for (i=cur_nt+1; i <= lastnote; i++) {
        m = score.solo.note[i].frames;
        restore_line_to_visible(m);
    }
    lastnote = cur_nt;
    end_pos = score.solo.note[cur_nt].wholerat;
    
}




- (void)keyDown:(NSEvent *)pEvent
{
    char c;
    //[theController reactToKeyDownEvent: pEvent];
    //[super keyDown: pEvent];
    
    if (live_locked) return; // would be better if we could just disable the whole window ...
    
    NSString *cmd = [pEvent characters];
    c = [cmd characterAtIndex:0];
    //    NSLog(@"got key %c %d",c,c);
    
    /* if ((c == 'l' || c == 'L' || c == 'r' || c == 'R') && (upgrade_purchased == 0)) {
     [[[NSApplication sharedApplication] delegate] upgradeAlert];
     [[NSAlert alertWithMessageText:@"Requires upgrade"
     defaultButton:@"Ok"
     alternateButton:nil
     otherButton:nil
     informativeTextWithFormat:@"You must purchase the upgrade to use this feature"] runModal];
     return;
     } */
    
    
    
    
    if (c == 'f') highlight_neighbor_note(1);
    else if (c == 'F') highlight_neighbor_note(10);
    else if (c == 'b') highlight_neighbor_note(-1);
    else if (c == 'B') highlight_neighbor_note(-10);
    else if (c == 'r') move_cur_nt(1);
    else if (c == 'l') move_cur_nt(-1);
    else if (c == 'R') move_cur_nt(50);
    else if (c == 'L') move_cur_nt(-50);
    else if (c == 'u') unmark_note();
    else if (c == 'q') [[[NSApplication sharedApplication] delegate] StopPlayingAction:nil];
    /*quit_action();*/
    
    if (currently_playing) return;
    
    
    
    if (c == 'p') [[[NSApplication sharedApplication] delegate] StartPlayingAction:nil];
    /* play_action();*/
    else if (c == 'e') jump_to_end();
    else if (c == '+') highlight_notes();
    else if (c == '-') unhighlight_notes();
    else if (c == 5) discard_tail();  // control e
    
    //  [self setNeedsDisplay:YES];
    // [self setNeedsDisplayInRect:CGRectMake(0,0,spect_wd,SPECT_HT)];
}


typedef struct {
    int page;
    int staff;
} PAGE_STAFF;

static int notation_scroll=0;
static int scroll_dir;
static int scan_top;  // the first scan line of the crrent segment
static int scan_bot;  // the first line of the next segment --- could have scan_bot < scan_top from wraparound
static int scan_line_num=0;
/* If the piece was concatenated onto single page, this is the total number of lines.  0 means unset */
PAGE_STAFF top_line,bot_line;  // what is showing on the


- (void) addRulerNumbers {
    NSString *s;
    RATIONAL q;
    int i,m,j,k,frame,is_solo,m_no_rpt;
    
    if (frames == 0) return;
    if (ruler.num == 0) return;
    
    NSMutableDictionary *textAttrib = [[NSMutableDictionary alloc] init];
    [textAttrib setObject:[NSFont fontWithName:@"Helvetica Light" size:10] forKey:NSFontAttributeName];
    [textAttrib setObject:[NSColor lightGrayColor] forKey:NSForegroundColorAttributeName];
    
    //  NSLog(@"addRulerNumbers");
    for (i=0; i < ruler.num; i++) {
        q = wholerat2measrat(ruler.el[i].wholerat);
        if (q.num != 0) continue;
        /* m = wholerat2measnum(ruler.el[i].wholerat);
         m_no_rpt = score.measdex.measure[m].meas;*/
        k = ruler.el[i].meas_index;
        m_no_rpt = score.measdex.measure[k].meas;
        j = ruler.el[i].index;
        is_solo = ruler.el[i].is_solo;
        //  if (is_solo) continue;
        //frame = (is_solo) ? score.solo.note[j].frames : score.midi.burst[j].frames;
        frame = (is_solo) ? score.solo.note[j].frames : ruler.el[i].frames;
        if (frame == 0) continue;  // this is an unrecognized note
        if (frame < scroll_pos) continue;
        if (frame > scroll_pos + spect_wd) continue; //Freturn;
        s = [[NSNumber numberWithInt:m_no_rpt] stringValue];
        [s drawAtPoint:NSMakePoint((frame-scroll_pos)+5,SPECT_HT-RULER_HT + 3) withAttributes:textAttrib];
        
    }
    
    
    
}


- (void)drawRect:(NSRect)dstRect { /* origin is the LOWER left */
    NSPoint p;
    NSRect srcRect;
    int overflow;
    
    
    /* pRect.origin.x = pRect.origin.y = 0; pRect.size.width = pRect.size.height = 200;
     destRect = pRect; destRect.size.width = 200; destRect.size.height = 500;*/
    
    srcRect = dstRect; srcRect.origin.y = NOTATION_BUFFER_SIZE-SPECT_HT - notation_scroll;
    //   NSLog(@"%d",SPECT_HT);
    [NSGraphicsContext saveGraphicsState];
    
    
    [bigBitmap drawInRect:dstRect  // destination rect
                 fromRect:dstRect  // source rect (rescales if not same dimensions
                operation:NSCompositeCopy
                 fraction:1.0
           respectFlipped:NO
                    hints:nil];
    
    if (dstRect.origin.y + dstRect.size.height > SPECT_HT - RULER_HT)
        [self addRulerNumbers];  // if not redrawn every time the spectrogram drawing will write over the numbers --- they are not part of the image.
    
    /*  NSString *s = @"hello";
     
     NSMutableDictionary *textAttrib = [[NSMutableDictionary alloc] init];
     [textAttrib setObject:[NSFont fontWithName:@"Helvetica Light" size:15] forKey:NSFontAttributeName];
     [textAttrib setObject:[NSColor yellowColor] forKey:NSForegroundColorAttributeName];
     
     
     [[NSColor redColor] set];
     [s drawAtPoint:NSMakePoint(100,100) withAttributes:textAttrib];*/
    
    
    
    // pRect = CGRectMake(0,0,400,500);
    
    
    // [bigBitmap drawInRect:dstRect fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];
    
    // [bigBitmap drawAtPoint:p fromRect:pRect operation:NSCompositeCopy fraction:1.0];
    
    /*overflow = (notation_scroll+SPECT_HT) - NOTATION_BUFFER_SIZE;
     if (overflow > 0) {
     srcRect.size.height = dstRect.size.height = SPECT_HT-overflow;
     srcRect.origin.y = 0;
     dstRect.origin.y = overflow;
     [bigBitmap drawInRect:dstRect  // draw the regular portion of the rectangle
     fromRect:srcRect
     operation:NSCompositeCopy
     fraction:1.0
     respectFlipped:NO
     hints:nil];
     
     srcRect.origin.y = NOTATION_BUFFER_SIZE-overflow;
     srcRect.size.height = dstRect.size.height = overflow;
     dstRect.origin.y = 0;
     [bigBitmap drawInRect:dstRect  // draw the wrap around portion
     fromRect:srcRect
     operation:NSCompositeCopy
     fraction:1.0
     respectFlipped:NO
     hints:nil];
     }
     else {
     
     [bigBitmap drawInRect:dstRect  // destination rect
     fromRect:srcRect  // source rect (rescales if not same dimensions
     operation:NSCompositeCopy
     fraction:1.0
     respectFlipped:NO
     hints:nil];
     } */
    
    
    /*   [bigBitmap drawInRect:dstRect
     fromRect:dstRect
     operation:NSCompositeCopy fraction:1.0]; */
    
    
    /*  [bigBitmap drawAtPoint:pRect.origin fromRect:pRect
     operation:NSCompositeCopy fraction:1.0
     respectFlipped:NO
     hints:nil];*/
    
    
    [NSGraphicsContext restoreGraphicsState];
    
    
} // end drawRect


-(void) synthAmim {
    int i;
    
    currently_playing = 1;
    for (i=0; i < score.midi.num; i++) prediction_target[i] = 0;
    nsTimer =  [NSTimer scheduledTimerWithTimeInterval:0.01
                                                target:view_self
                                              selector:@selector(play_timer)
                                              userInfo:NULL
                                               repeats:YES];
    
}

-(void) stopSynthAnim {
    [nsTimer invalidate];
    nsTimer = nil;
}

+(void)playAnim:(id)param{
    int f,done;
    
    //  if (mode == SYNTH_MODE) { synth_timer(); return; }
    //  if (mode == PV_MODE) { pv_timer(); return; }
    
    while (currently_playing) {
        f = cur_play_frame_num - start_play_frame_num;
        if (ready_for_play_frame(f)) gui_spect_play_function();   // only 1 iteration per sleep
        /*    while (ready_for_play_frame(cur_play_frame_num - start_play_frame_num))
         gui_spect_play_function(); */
        
        
        
        
        usleep(10000); // .01 secs
        //    usleep(25000); // .025 secs
        //        NSLog(@"anim Thread says hello\n");
        
    }
    NSLog(@"finished playAnim thread");
}

+(void)playRead:(id)param{
    int f,done;
    
    
    while (currently_playing) {
        fill_read_buffer(&solo_read_buff);
        fill_read_buffer(&orch_read_buff);
        usleep(100000); // .1 secs
        //    NSLog(@"read iter");
    }
    NSLog(@"finished playRead thread");
}

-(void) markersChange {
    int f0;
    
    //  [self show_the_spect];
    draw_note_markers(clicks_in);
    f0 = (cur_nt < firstnote) ? 0 : score.solo.note[cur_nt].frames;
    add_line_to_visible(f0,COLOR_GREEN);
}

-(void) clear_spect {
    NSUInteger r,g,b;
    int x,y,i;
    unsigned char *ptr;
    
    [[[[NSApplication sharedApplication] delegate] PieceTitleOutlet] setStringValue:@""];
    
#ifdef NOTATION
    return;
#endif
    
    r = g = b = 0;
    NSUInteger zColourAry[3] = {r,g,b};
    for (y = 0; y < SPECT_HT; y++) {
        for (x = 0; x < spect_wd; x++) {
            [bitmap setPixel:zColourAry atX:x y:y];
        }
    }
    [view_self setNeedsDisplay:YES];
}

/*************************************************************************************/

#define PAGE_DIR "page"
#define MAX_PAGES 50

#define MAX_STAVES 100

typedef struct {
    int lo;
    int hi;
} INT_RANGE;


typedef struct {
    int n;
    INT_RANGE range[MAX_STAVES];    // range on actual bitmap page
    INT_RANGE showing[MAX_STAVES];  // range in the SPECT window
} STAFF_RANGE;


#define MAX_PATCH_ROWS 21 // should be odd number
#define MAX_PATCH_COLS 21

typedef struct {
    LOC loc;  // the upper left corner
    int rows;
    int cols;
    unsigned char grey[MAX_PATCH_ROWS][MAX_PATCH_COLS];
} IMAGE_PATCH;

#define MAX_IMAGE_PATCHES  500 // the max number of noteheads that could be showing at one time

typedef struct {
    int top;
    IMAGE_PATCH *ptr[MAX_IMAGE_PATCHES];
    IMAGE_PATCH el[MAX_IMAGE_PATCHES];
} IMAGE_PATCH_STACK;


typedef struct {
    char tag[20];
    int staff;
    LOC loc;
    IMAGE_PATCH patch; // might want to use pointers to these an alloc them wiht a stack to save space ... simpler implementation for now. see IMAGE_PATCH_STACK for later ...
} NOTE_HEAD;


typedef struct {
    int num;
    NOTE_HEAD *head;
} NOTE_HEAD_LIST,STAFF_HEAD_LIST;

typedef struct {
    int num;
    STAFF_HEAD_LIST staff[MAX_STAVES];
} STAFF_HEAD_STRUCT;


typedef struct {
    char *name;
    NSBitmapImageRep *bitmap;
    STAFF_RANGE map;
    NOTE_HEAD_LIST dex;
    STAFF_HEAD_STRUCT staffdex;  // this structure will phase out one above
} PAGE;

typedef struct {
    int n;
    PAGE page[MAX_PAGES];
} PAGE_LIST;


typedef struct {
    //INT_RANGE src; // will phase out src and dst in favor of fr and to
    //INT_RANGE dst;
    RECT fr;
    RECT to;
    int page;
    int staff;
    int filler;  /* is this just white space filler?  only dst field meaningful if true.
                  this is also used for an interval that has been chopped in two by writing another interval over it.*/
} IMAGE_INTERVAL;  // a full width chunk of the notation image


#define MAX_IMAGE_INTERVALS 100

typedef struct {
    float scale;
    int num;
    IMAGE_INTERVAL list[MAX_IMAGE_INTERVALS];
} INTERVAL_MAP;

typedef struct {
    INTERVAL_MAP imap;
    int write_top;  // boolean, the next write will go at the top of the page preserving bot iff write_top
    int last_staff;  // the number of last staff on the page (this will be preserved when write to top
    int last_staff_top;
    int last_staff_bot;
    int ptop;  // the page for the top line in the display
    int ltop;  // the line index for the top line in the display
    INT_RANGE pages_showing;
    INT_RANGE lines_showing;  // first showing line is lines_showing.lo is the line on pages_showing.lo
    int hi_page;
} PAGE_STATUS;

typedef struct {
    int page;
    int staff;
    int index;
} NOTATION_SCROLL_POS;

static NOTATION_SCROLL_POS nps;
static PAGE_STATUS page_status;
static NSBitmapImageRep *pageBitmap;
static PAGE_LIST notation;
static STAFF_RANGE staff_range;
static IMAGE_PATCH_STACK ips;



static void
myexit2() {
    exit(0);
}


static void
init_image_patch_list() {
    int i;
    
    for (i=0; i < MAX_IMAGE_PATCHES; i++) ips.ptr[i] = ips.el + i;
    ips.top = 0;
}


static IMAGE_PATCH*
get_image_patch() {
    if (ips.top == MAX_IMAGE_PATCHES-1) { printf("no more patches available\n"); myexit2(); }
    return(ips.ptr[ips.top++]);
}

static void
free_image_patch(IMAGE_PATCH *ptr) {
    if (ips.top <= 0) { printf("something went very very wrong\n"); myexit2(); }
    ips.ptr[--ips.top] = ptr;
}


static void
init_interval_map(INTERVAL_MAP *imap) {
    IMAGE_INTERVAL ii;
    imap->num = 2;
    
    /* imap->list[0].dst.lo = 0;
     imap->list[0].dst.hi = SPECT_HT;*/
    imap->list[0].filler = 1;
    
    //  ii.dst.lo = 0; ii.dst.hi = SPECT_HT;
    ii.filler = 1;
    ii.fr.loc.i = ii.fr.loc.j = ii.fr.height = ii.fr.width = 0;
    ii.to.loc.i = 0; ii.to.loc.j = 0;
    ii.to.height = NOTATION_BUFFER_SIZE/2; //SPECT_HT;
    ii.to.width = spect_wd;
    imap->list[0] = ii;
    ii.to.loc.i = NOTATION_BUFFER_SIZE/2;
    imap->list[1] = ii;
    
    
    
    
}

static int
mod_out(int n) {
    return((n + NOTATION_BUFFER_SIZE) % NOTATION_BUFFER_SIZE);
}



static void
split_image_interval(IMAGE_INTERVAL *ival, int split,  IMAGE_INTERVAL *new_ival ) {
    int old_height;
    
    *new_ival = *ival;
    /*  ival->src.hi =  ival->src.lo + (ival->src.hi - ival->src.lo) * (split - ival->dst.lo)/ (ival->dst.hi - ival->dst.lo);
     ival->dst.hi = new_ival->dst.lo = split;
     
     new_ival->src.lo = ival->src.hi;*/
    
    old_height = ival->to.height;
    ival->to.height = mod_out(split - ival->to.loc.i);
    if (ival->filler == 0) ival->fr.height = ival->fr.height * ival->to.height/ old_height;
    new_ival->to.loc.i = mod_out(ival->to.loc.i + ival->to.height);
    new_ival->to.height -= ival->to.height;
    new_ival->fr.height -= ival->fr.height;
    new_ival->filler = ival->filler = 1;  // not a full interval anymore so really is trash
    
    //printf("new_ival->src.lo = %d ival->src.hi = %d\n",new_ival->src.lo,ival->src.hi);
}


/*static int
 consistency(IMAGE_INTERVAL ival) {
 if (ival.dst.lo != ival.to.loc.i) return(0);
 if (ival.dst.hi != (ival.to.loc.i + ival.to.height)) return(0);
 if (ival.filler) return(1);
 if (ival.src.lo != ival.fr.loc.i) return(0);
 if (ival.src.hi != (ival.fr.loc.i + ival.fr.height)) return(0);
 return(1);
 }*/



static int
crosses_boundary(int a, int b, int bound) {
    int n = NOTATION_BUFFER_SIZE,t=0;
    
    t += (bound - a + n) % n;
    t += (b - bound + n) % n;
    t += (a - b + n) % n;
    return (t == n);
}


static int
in_order(int a, int b, int c) { // is b encountered before c beginning at a and increasing modulo NOTATION_BUFFER_SIZe
    
    // return(crosses_boundary(b,c,a) == 0);
    
    if (a == b && b == c) return(1);
    if (a == c) return(0);
    int n = NOTATION_BUFFER_SIZE,t=0;
    
    t += (b - a + n) % n;
    t += (c - b + n) % n;
    t += (a - c + n) % n;
    return (t <= n);  // 0 or n since t should be multiple of n
}


static int
is_showing(int i) {
    return(in_order(notation_scroll, i,mod_out(notation_scroll+SPECT_HT)));
}



static void
split_interval_map_on_point(int split, INTERVAL_MAP *imap) {
    int k,kk,lo,hi;
    IMAGE_INTERVAL new_ival,*ival;
    
    for (k=0; k < imap->num; k++) {
        ival = imap->list+k;
        lo = imap->list[k].to.loc.i;
        hi = mod_out(lo + imap->list[k].to.height);
        //  if (split > imap->list[k].dst.lo && split < imap->list[k].dst.hi) {
        // if (split > lo && split < hi) {
        if (split != lo && split != hi && in_order(lo,split,hi)) {
            split_image_interval(imap->list + k,split,&new_ival);
            for (kk=imap->num; kk > k+1; kk--) imap->list[kk] = imap->list[kk-1];
            imap->list[k+1] = new_ival;
            (imap->num)++;
            break;
        }
    }
    
}

static void
split_interval_map_on_range(int lo, int hi, /*INT_RANGE ran,*/ INTERVAL_MAP *imap) {
    split_interval_map_on_point(/*ran.*/lo,imap);
    split_interval_map_on_point(/*ran.*/hi,imap);
}


static RECT
center_box(LOC loc, int half_ht, int half_wd) {
    RECT r;
    
    r.loc.i = loc.i - half_ht;
    r.loc.j = loc.j - half_wd;
    r.height = 2*half_ht+1;
    r.width = 2*half_wd +1;
    return(r);
}

static LOC
src_loc2dst_loc(LOC src, IMAGE_INTERVAL ival) {
    LOC dst;
    
    
    dst.i = mod_out(ival.to.loc.i + (src.i-ival.fr.loc.i)*ival.to.height/ival.fr.height);
    //  if (dst.i >= NOTATION_BUFFER_SIZE) dst.i -= NOTATION_BUFFER_SIZE;
    dst.j = (src.j*ival.to.width)/ival.fr.width;
    /* in theory shoudld get same answer if we use the ratio of heights rather than widthds, but this is better
     numerically speaking */
    return(dst);
}

static LOC
dst_loc2src_loc(LOC dst, IMAGE_INTERVAL ival) {
    LOC src;
    int d;
    
    d = mod_out(dst.i - ival.to.loc.i);
    src.i = ival.fr.loc.i + ival.fr.height*d / ival.to.height;
    //  src.i = ival.fr.loc.i + ival.fr.height*(dst.i - ival.to.loc.i) / ival.to.height;
    // src.j = dst.j * ival.fr.height/ival.to.height;
    src.j = dst.j * ival.fr.width/ival.to.width;
    return(src);
}




static RECT
src_rct2dst_rct(RECT src, IMAGE_INTERVAL ival) {
    float scale;
    RECT dst;
    
    //    scale = (ival.dst.hi-ival.dst.lo)/(float)(ival.src.hi-ival.src.lo);
    /* dst.loc.i = ival.dst.lo + (src.loc.i-ival.src.lo)*ival.to.height/ival.fr.height;
     dst.loc.j = src.loc.j*ival.to.width/ival.fr.width;*/
    
    dst.loc = src_loc2dst_loc(src.loc,ival);
    dst.height = (src.height*ival.to.height)/ival.fr.height;
    dst.width = (src.width*ival.to.width)/ival.fr.width;
    return(dst);
}

static int
range_overlap(INT_RANGE i1, INT_RANGE i2) {
    int hi,lo,d;
    
    hi = (i1.hi < i2.hi) ? i1.hi : i2.hi;
    lo = (i1.lo > i2.lo) ? i1.lo : i2.lo;
    d = hi-lo;
    return((d > 0) ? d : 0);
}


static int
max_overlap_line_index(IMAGE_INTERVAL ival,int p) {
    int k,maxk,max_overlap=0,d;
    INT_RANGE ir1,ir2;
    
    for (k=0; k < notation.page[p].map.n; k++) {
        ir1 = notation.page[p].map.range[k];
        ir2.lo = ival.fr.loc.i;
        ir2.hi = ir2.lo + ival.fr.height;
        d = range_overlap(ir1,ir2);
        if (d > max_overlap) { max_overlap = d; maxk = k; }
        
    }
    if (max_overlap == 0) { printf("couldn't find any overlap\n"); myexit2(); }
    return(maxk);
    
}


#define HEAD_BOX_HALF_WD 3//7
#define HEAD_BOX_HALF_HT 3//5


static void
identify_note_head_screen_positions(IMAGE_INTERVAL ival) {
    int k,p,j,i,h,bpr,kk ;
    INT_RANGE ir;
    LOC src,dst;
    STAFF_HEAD_STRUCT *shs;
    IMAGE_PATCH *patch;
    unsigned char *ptr,*row;
    STAFF_HEAD_LIST *shl;
    RECT src_rect,dst_rect;
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    p = ival.page;
    
    // k = max_overlap_line_index(ival,p); // index of relevant line  (rather than recomputing this, shouldn't it be passed or just part of the ival structure? )
    k = ival.staff;
    shs = &notation.page[p].staffdex;
    shl = shs->staff + k;
    for (h=0; h < shl->num; h++) {
        src = shs->staff[k].head[h].loc;
        src_rect =   center_box(src, /*HEAD_BOX_HALF_HT*/10, /*HEAD_BOX_HALF_WD*/ 13);
        
        dst  =  src_loc2dst_loc(src, ival);
        
        patch = &(shs->staff[k].head[h].patch);
        dst_rect = src_rct2dst_rct(src_rect,ival);
        patch->loc.i = dst_rect.loc.i;
        patch->loc.j = dst_rect.loc.j;
        patch->rows = dst_rect.height;
        patch->cols = dst_rect.width;
        if (patch->rows > MAX_PATCH_ROWS || patch->cols > MAX_PATCH_COLS)
            printf("adfadsf\n");
        
        
        for (i=0; i < patch->rows; i++) {
            row = ptr + mod_out(i+patch->loc.i)*bpr;
            for (j=0; j < patch->cols; j++) {
                
                patch->grey[i][j] = row[3*(j+patch->loc.j)];
                
            }
            
        }
    }
}

static void
old_identify_note_head_screen_positions(IMAGE_INTERVAL ival) {
    int k,p,j,i,h,bpr,kk ;
    INT_RANGE ir;
    LOC src,dst;
    STAFF_HEAD_STRUCT *shs;
    IMAGE_PATCH *patch;
    unsigned char *ptr,*row;
    STAFF_HEAD_LIST *shl;
    RECT src_rect,dst_rect;
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    p = ival.page;
    
    for (k=0; k < notation.page[p].map.n; k++) {
        ir = notation.page[p].map.range[k];
        NSLog(@"%d %d %d %d",ir.lo,ival.fr.loc.i, ir.hi, ival.fr.loc.i+ival.fr.height);
        // if (ir.lo < ival.src.lo || ir.hi > ival.src.hi) continue;
        if (ir.lo < ival.fr.loc.i || ir.hi > (ival.fr.loc.i+ival.fr.height)) continue;
        kk = max_overlap_line_index(ival,p);
        shs = &notation.page[p].staffdex;
        shl = shs->staff + k;
        for (h=0; h < shl->num; h++) {
            src = shs->staff[k].head[h].loc;
            src_rect =   center_box(src, /*HEAD_BOX_HALF_HT*/8, /*HEAD_BOX_HALF_WD*/ 13);
            
            dst  =  src_loc2dst_loc(src, ival);
            
            patch = &(shs->staff[k].head[h].patch);
            dst_rect = src_rct2dst_rct(src_rect,ival);
            patch->loc.i = dst_rect.loc.i;
            patch->loc.j = dst_rect.loc.j;
            patch->rows = dst_rect.height;
            patch->cols = dst_rect.width;
            if (patch->rows > MAX_PATCH_ROWS || patch->cols > MAX_PATCH_COLS)
                printf("adfadsf\n");
            
            
            for (i=0; i < patch->rows; i++) {
                row = ptr + (i+patch->loc.i)*bpr;
                for (j=0; j < patch->cols; j++) {
                    
                    patch->grey[i][j] = row[3*(j+patch->loc.j)];
                    
                }
                
            }
        }
        break;
    }
}

static int
height_contained_circ(RECT r1, RECT r2) {  // vertical range of r1 contained in vertical range of r2?
    /* the intervals can wrap around NOTATION_BUFFER_SIZE */
    int lo1,lo2,hi1,hi2,tot=0;
    
    lo1 = r1.loc.i;
    hi1 = mod_out(lo1+r1.height);
    lo2 = r2.loc.i;
    hi2 = mod_out(lo2 + r2.height);
    tot += mod_out(lo1 - lo2);
    tot += mod_out(hi1 - lo1);
    tot += mod_out(hi2 - hi1);
    tot += mod_out(lo2 - hi2);
    return(tot == NOTATION_BUFFER_SIZE);  // in other words the sequence wraps around exactly once.
}

static int
circ_rects_intersect(RECT r1, RECT r2) {  // vertical range of r1 intersect vertical range of r2?
    /* the intervals can wrap around NOTATION_BUFFER_SIZE */
    int lo1,lo2,hi1,hi2,tot=0;
    
    lo1 = r1.loc.i;
    hi1 = mod_out(lo1+r1.height);
    lo2 = r2.loc.i;
    hi2 = mod_out(lo2 + r2.height);
    return(in_order(lo1,lo2,hi1) || in_order(lo2,lo1,hi2));
    
    
}


static int
height_contained(RECT r1, RECT r2) {  // vertical range of r1 contained in vertical range of r2?
    int lo1,lo2,hi1,hi2;
    
    lo1 = r1.loc.i;
    hi1 = lo1+r1.height;
    lo2 = r2.loc.i;
    hi2 = lo2 + r2.height;
    return(lo1 >= lo2 && hi1 <= hi2);
}





static void
add_image_interval(IMAGE_INTERVAL ival, INTERVAL_MAP *imap) {
    int k,kk,m,lo,hi,b1,b2,a,b,c,d;
    IMAGE_INTERVAL ii;
    
    
    if (imap->num == 0) init_interval_map(imap);
    lo = ival.to.loc.i;
    hi = mod_out(lo+ival.to.height);
    split_interval_map_on_range(lo,hi,/*ival.dst,*/imap);
    //  for (k=0; k < imap->num; k++) if (ival.dst.lo == imap->list[k].dst.lo) break;
    for (k= imap->num-1; k >= 0; k--) if (height_contained_circ(imap->list[k].to, ival.to))  imap->list[k] = imap->list[--imap->num];
    /*  for (k= imap->num-1; k >= 0; k--)
     if (circ_rects_intersect(imap->list[k].to, ival.to))
     imap->list[k] = imap->list[--imap->num];*/
    imap->list[imap->num++] = ival;
    return;
    
    
    
    
    
    for (k=0; k < imap->num; k++) if (ival.to.loc.i == imap->list[k].to.loc.i) break;
    if (k == imap->num) { printf("shouldn't be no no\n"); myexit2(); }
    imap->list[k] = ival;
    for (kk = k+1; kk < imap->num; kk++) {
        if (height_contained_circ(imap->list[kk].to, ival.to)) {
            for (m=kk; m < imap->num; m++) imap->list[m] = imap->list[m+1];
            imap->num--;
        }
    }
}

/* idea ... first split the strcture as needed so the new intervale doesn't introduce any new knots.  then easy to add */

static void
print_imap(INTERVAL_MAP *imap) {
    int k;
    
    NSLog(@"printing interval map:\n");
    for (k=0; k < imap->num; k++) {
        NSLog(@"interval = %d\n",k);
        //  NSLog(@"dst = (%d %d)\n",imap->list[k].dst.lo,imap->list[k].dst.hi);
        //    NSLog(@"src = (%d %d)\n",imap->list[k].src.lo,imap->list[k].src.hi);
        NSLog(@"src = (%d %d)" , imap->list[k].fr.loc.i, mod_out(imap->list[k].fr.loc.i+imap->list[k].fr.height));
        NSLog(@"to = (%d %d)" , imap->list[k].to.loc.i, mod_out(imap->list[k].to.loc.i+imap->list[k].to.height));
        NSLog(@"page = %d line = %d\n",imap->list[k].page,imap->list[k].staff);
        NSLog(@"filler = %d\n",imap->list[k].filler);
        NSLog(@" ");
        
    }
    
}

static void
fill_image_wide_rect(RECT dst, int val) {
    int i,j,k,bpr;
    Byte *ptr,*row;
    IMAGE_INTERVAL ival;
    
    /*ival.dst.lo = dst.loc.i; ival.dst.hi = dst.loc.i+dst.height;
     ival.src.lo = ival.src.hi = 0;*/
    ival.filler = 1; ival.page = 0;
    ival.fr.loc.i = ival.fr.loc.j = ival.fr.height = ival.fr.width = ival.to.loc.j = 0; // these don't matter
    ival.to =dst;
    
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    for (i=0; i < dst.height; i++) {
        row = ptr + (i+dst.loc.i)*bpr;
        for (j=0; j < dst.width; j++)
            for (k=0; k < 3; k++) row[3*(dst.loc.j+j)+k] = val;
    }
}



static void
copy_image_box_rescale(NSBitmapImageRep *image, RECT src, RECT dst) {
    unsigned char *ptr1,*row1,*ptr2,*row2;
    int i,j,ii,q,jj,k,bpr1,bpr2;
    short int count[MAX_DST_HT][MAX_DST_WD];  // lots of stack space claimed here ... be careful
    int result[MAX_DST_HT][MAX_DST_WD];
    
    if (dst.width > MAX_DST_WD || dst.height > MAX_DST_HT) {
        NSLog(@"bad height or with in copyBitmap ht = %d wd = %d",dst.height,dst.width);
        myexit2();
    }
    if (src.height < dst.height || src.width < dst.width) {
        printf("dst rect must be smaller than src rect\n");
        myexit2();
    }
    for (i=0; i < dst.height; i++) for (j=0; j < dst.width; j++)
        count[i][j] = result[i][j] = 0;
    ptr1 = [image bitmapData];
    bpr1 = [image bytesPerRow];
    ptr2 = [bitmap bitmapData];
    bpr2 = [bitmap bytesPerRow];
    for (i=0; i < src.height; i++) {
        row1 = ptr1 + (i+src.loc.i)*bpr1;
        ii = i*dst.height/src.height;
        for (j=0; j < src.width; j++) {
            jj = j*dst.width/src.width;
            result[ii][jj] += row1[3*(src.loc.j+j)];
            count[ii][jj]++;
        }
    }
    for (i=0; i < dst.height; i++) {
        ii = dst.loc.i+i;
        if (ii >= NOTATION_BUFFER_SIZE) ii -= NOTATION_BUFFER_SIZE;
        // row2 = ptr2 + (dst.loc.i+i)*bpr2;
        row2 = ptr2 + ii*bpr2;
        
        for (j=0; j < dst.width; j++) {
            q = result[i][j] / count[i][j];
            for (k=0; k < 3; k++) row2[3*(dst.loc.j+j) + k] = q;
            //  result[i][jj] / count[i][jj];
        }
    }
    
    
}


void add_notation_rect(/*NSBitmapImageRep *target,*/ int page_num, int staff, RECT src, RECT dst) {
    unsigned char *ptr1,*row1,*ptr2,*row2;
    int i,j,k,ii,jj,bpr1,bpr2;
    float irat,jrat;
    NSBitmapImageRep *image;
    IMAGE_INTERVAL ival;
    
    image = notation.page[page_num].bitmap;
    /*ival.dst.lo = dst.loc.i; ival.dst.hi = dst.loc.i+dst.height;
     ival.src.lo = src.loc.i; ival.src.hi = src.loc.i+src.height;*/
    ival.filler = 0; ival.page = page_num; ival.staff = staff;
    ival.fr = src; ival.to = dst;
    
    add_image_interval(ival,&page_status.imap);
    copy_image_box_rescale(image, src, dst);
    identify_note_head_screen_positions(ival);
    
}


#define CLOSE_LIM 15

static int
locs_close(LOC l1, LOC l2) {
    LOC d;
    
    d.i = abs(l1.i-l2.i);
    if (d.i > CLOSE_LIM) return(0);
    d.j = abs(l1.j-l2.j);
    if (d.j > CLOSE_LIM) return(0);
    return(1);
    
}

static int
pos_on_note(LOC dst_loc, NOTATION_SCROLL_POS  *nsp, LOC *head_loc) {  // this is loc in the image window --- not page coords
    int k,m,d,s,top;
    IMAGE_INTERVAL ival;
    PAGE page;
    LOC src_loc,cand;
    
    
    for (k=0; k < page_status.imap.num; k++) {
        ival = page_status.imap.list[k];
        // if (!(dst_loc.i >= ival.dst.lo && dst_loc.i < ival.dst.hi)) continue;
        top = mod_out(ival.to.loc.i+ival.to.height);
        if (!in_order(ival.to.loc.i, dst_loc.i, top)) continue;
        //      if (!(dst_loc.i >= ival.to.loc.i && dst_loc.i < (ival.to.loc.i+ival.to.height))) continue;
        if (ival.filler) return(0);
        s = ival.staff;
        page = notation.page[ival.page];
        /*  src_loc.i = ival.src.lo +
         (ival.src.hi-ival.src.lo)*(dst_loc.i-ival.dst.lo)/(ival.dst.hi-ival.dst.lo);
         
         src_loc.j = dst_loc.j *(ival.src.hi-ival.src.lo) /(ival.dst.hi-ival.dst.lo);*/
        
        /* src_loc.i = ival.fr.loc.i + ival.fr.height*(dst_loc.i - ival.to.loc.i) / ival.to.height;
         src_loc.j = dst_loc.j * ival.fr.height/ival.to.height;*/
        
        src_loc = dst_loc2src_loc(dst_loc,ival);
        
        /*    for (s=0; s < page.map.n; s++) {
         if (src_loc.i > page.map.range[s].lo && src_loc.i < page.map.range[s].hi) break;
         }
         if (s == page.map.n) return(0); */
        for (m=0; m < page.staffdex.staff[s].num; m++) {
            cand = page.staffdex.staff[s].head[m].loc;
            if (locs_close(cand,src_loc) == 0) continue;
            nsp->page = ival.page; nsp->staff = s; nsp->index = m;
            return(1);
            
        }
        return(0);
        
        
        
        
        
        /* horizontal and vertical scaling must be identical to prevent image warping */
        for (m=0; m < page.dex.num; m++) {  // phasing this out ... in favor of line by line index
            d = src_loc.i - page.dex.head[m].loc.i;
            if (d < 0) d = -d;
            if (d > 15) continue;
            d = src_loc.j - page.dex.head[m].loc.j;
            if (d < 0) d = -d;
            if (d > 15) continue;
            src_loc = page.dex.head[m].loc;
            
            
            
            /*  head_loc->i = ival.dst.lo + (src_loc.i-ival.src.lo)*(ival.dst.hi-ival.dst.lo)/(ival.src.hi-ival.src.lo);
             head_loc->j = (src_loc.j*(ival.dst.hi-ival.dst.lo))/(ival.src.hi-ival.src.lo);*/
            
            
            /* head_loc->i = ival.to.loc.i + (src_loc.i-ival.fr.loc.i)*ival.to.height/ival.fr.height;
             head_loc->j = src_loc.j*ival.to.height/ival.fr.height;*/
            
            *head_loc = src_loc2dst_loc(src_loc,ival);
            return(1);
        }
        
        
    }
    return(0);
}







static NOTATION_SCROLL_POS
pos2nsp(LOC dst_loc) {  // this is loc in the the big bitmap in [0 ... n-1 ]
    int k,m,d,s,h,kk,bestk,i,besti;
    IMAGE_INTERVAL ival;
    PAGE page;
    LOC src_loc;
    NOTATION_SCROLL_POS nsp;
    STAFF_HEAD_LIST *shl;
    
    if (dst_loc.i > 1500)
        NSLog(@"I don't think so");
    
    // besti = notation_scroll; // this will always be beaten
    besti = scan_top; // this will always be beaten
    bestk = -1;
    for (k=0; k < page_status.imap.num; k++) {
        ival = page_status.imap.list[k]; // just for debugging
        if (page_status.imap.list[k].filler) continue;
        i = page_status.imap.list[k].to.loc.i;
        // if (is_showing(i) == 0) continue;
        /* if (in_order(notation_scroll,i,dst_loc.i) == 0) continue;  // only want staff lines that are before the mouse
         if (in_order(notation_scroll,besti,i)) { besti = i; bestk = k; }*/
        if (in_order(scan_top,i,dst_loc.i) == 0) continue;  // only want staff lines that are before the mouse
        if (in_order(scan_top,besti,i)) { besti = i; bestk = k; }
    }
    if ( bestk == -1) {
        
        
        //   besti = notation_scroll; // this will always be beaten
        besti = scan_top; // this will always be beaten
        
        bestk = -1;
        for (k=0; k < page_status.imap.num; k++) {
            ival = page_status.imap.list[k]; // just for debugging
            if (page_status.imap.list[k].filler) continue;
            i = page_status.imap.list[k].to.loc.i;
            //     if (is_showing(i) == 0) continue;
            /* if (in_order(notation_scroll,i,dst_loc.i) == 0) continue;  // only want staff lines that are before the mouse
             if (in_order(notation_scroll,besti,i)) { besti = i; bestk = k; } */
            if (in_order(scan_top,i,dst_loc.i) == 0) continue;  // only want staff lines that are before the mouse
            if (in_order(scan_top,besti,i)) { besti = i; bestk = k; }
        }
        
        printf("o my o my\n");
        
        
        myexit2();
    }
    ival = page_status.imap.list[bestk];
    nsp.page = ival.page;
    nsp.staff = ival.staff;
    page = notation.page[ival.page];
    src_loc = dst_loc2src_loc(dst_loc,ival);
    shl = page.staffdex.staff + nsp.staff;
    for (h= shl->num-1; h >= 0; h--)
        if (src_loc.j > shl->head[h].loc.j) break;
    nsp.index = h+1;
    return(nsp);
    
    
    // ------------------------------- the old way ----------------------
    
    for (k=page_status.imap.num-1; k >= 0; k--) {
        if (page_status.imap.list[k].filler) continue;
        kk = k;
        //  if (dst_loc.i > page_status.imap.list[k].dst.lo) break;
        if (dst_loc.i > page_status.imap.list[k].to.loc.i) break;
    }
    if (k < 0) k = kk;
    ival = page_status.imap.list[k];
    
    nps.page = ival.page;
    
    
    page = notation.page[ival.page];
    
    src_loc = dst_loc2src_loc(dst_loc,ival);
    
    for (s=page.map.n-1; s >= 0; s--) {
        INT_RANGE ir  = page.map.range[s];
        if (src_loc.i > page.map.range[s].lo) break;
    }
    if (s < 0) /*s++;*/ { nps.staff = nps.index = 0; return(nps); }
    nps.staff = s;
    
    for (h= page.staffdex.staff[s].num-1; h >= 0; h--)
        if (src_loc.j > page.staffdex.staff[s].head[h].loc.j) break;
    //  if (h < 0) h = 0;
    nps.index = h+1;
    return(nps);
}





static void
inc_pl(int *p, int *l) {
    PAGE *page;
    
    page = notation.page + *p;
    if (++(*l) == page->map.n) { *l = 0; (*p)++; }
}




static int
count_showing_lines() {
    int p,l,count=0;
    PAGE *page;
    
    p = page_status.pages_showing.lo;
    l = page_status.lines_showing.lo;
    page = notation.page + p;
    while (1) {
        count++;
        if (p == page_status.pages_showing.hi && l == page_status.lines_showing.hi) break;
        if (++l == page->map.n) {l = 0; p++; }
        
    }
    return(count);
}

static void
split_line(int *rp, int *rl) {
    int p,l,count=0,tot,last;
    PAGE *page;
    
    tot = count_showing_lines();
    last = (tot%2) ? tot/2+1 : tot/2;
    *rp = page_status.pages_showing.lo;
    *rl = page_status.lines_showing.lo;
    page = notation.page + *rp;
    while (count != last) {
        count++;
        inc_pl(rp,rl);
    }
    
}

static int  // what is the vertical point we will split the display on for page turning
split_point() {
    int p,l,count=0,tot,last;
    PAGE *page;
    
    tot = count_showing_lines();
    last = (tot%2) ? tot/2+1 : tot/2;
    p = page_status.pages_showing.lo;
    l = page_status.lines_showing.lo;
    page = notation.page + p;
    while (1) {
        count++;
        if (++l == page->map.n) {l = 0; p++; }
        if (count == last) break;
        
    }
    return(page->map.showing[l].lo);
}

/*typedef struct {
 int n;
 char *name[MAX_PAGES];
 } PAGE_NAMES;*/

//static PAGE_NAMES page;

static void
get_page_dir(char *name) {
    strcpy(name,user_dir);
    strcat(name,SCORE_DIR);
    strcat(name,"/");
    strcat(name,scoretag);
    strcat(name,"/");
    strcat(name,PAGE_DIR);
}


static void
read_staff_range(char *name, STAFF_RANGE *sr) {
    FILE *fp;
    int k;
    
    fp = fopen(name,"r");
    fscanf(fp,"staff num = %d\n",&(sr->n));
    for (k=0; k < sr->n; k++)
        fscanf(fp,"lo = %d hi = %d\n",&(sr->range[k].lo),&(sr->range[k].hi));
    fclose(fp);
    
}



#define MAX_NOTE_HEADS_ON_PAGE 1000

static int
read_note_heads(char *name, NOTE_HEAD_LIST *nhl, STAFF_HEAD_STRUCT *shs) {
    FILE *fp;
    char tag[200];
    int staff,lines,count[MAX_STAVES];
    LOC loc;
    int i = 0,n=0,j,k;
    NOTE_HEAD head[MAX_NOTE_HEADS_ON_PAGE],*h;
    
    for (i=0; i < MAX_STAVES; i++) count[i] = 0;
    fp = fopen(name,"r");
    if (fp == NULL) return(0);
    for (n=0; n < MAX_NOTE_HEADS_ON_PAGE; n++) {
        h = head + n;
        fscanf(fp, "%s %d %d %d",h->tag,&h->staff,&h->loc.i,&h->loc.j);
        if (feof(fp)) break;
        count[h->staff]++;
        
    }
    nhl->num = n;
    nhl->head = (NOTE_HEAD *) malloc(sizeof(NOTE_HEAD)*n);
    for (i=0; i < n; i++) nhl->head[i] = head[i];
    
    lines = head[n-1].staff+1;
    shs->num = lines;
    for (i=k=0; i < lines; i++) {
        shs->staff[i].num = count[i];
        shs->staff[i].head = (NOTE_HEAD *) malloc(sizeof(NOTE_HEAD)*count[i]);
        for (j=0; j < count[i]; j++) {
            if (head[k].staff != i) { printf("bad news in reading lines\n"); return(0); }
            shs->staff[i].head[j] = head[k++];
            
        }
    }
    return(1);
    
    for (i=0; i < nhl->num; i++) NSLog(@"%s %d %d %d",nhl->head[i].tag,nhl->head[i].staff,nhl->head[i].loc.i,nhl->head[i].loc.j);
    
}


static void
get_page_names() {
    struct dirent *de;
    char path[500],bitmap[500],range[500],head[500],*name;
    DIR *dir;
    PAGE *page;
    
    get_page_dir(path);
    dir = opendir(path);
    notation.n=0;
    while ((de = readdir(dir)) != NULL) {
        name = de->d_name;
        if (strstr(name,"bmp") == 0) continue;
        page = notation.page + notation.n;
        name[strlen(name)-4] = 0;
        page->name = (char *) malloc(strlen(name)+1);
        strcpy(page->name,name);
        
        strcpy(bitmap,path);
        strcat(bitmap,"/");
        strcat(bitmap,name);
        strcat(bitmap,".bmp");
        NSString *s = [[NSString alloc] initWithCString: bitmap];
        NSImage *image = [[NSImage alloc] initWithContentsOfFile:s];
        page->bitmap =[[NSBitmapImageRep alloc] initWithCGImage:[image CGImageForProposedRect:NULL context:NULL hints:NULL]];
        
        strcpy(range,path);
        strcat(range,"/");
        strcat(range,name);
        strcat(range,".srg");
        read_staff_range(range,&(page->map));
        
        strcpy(head,path);
        strcat(head,"/");
        strcat(head,name);
        strcat(head,".nhd");
        read_note_heads(head,&(page->dex),&(page->staffdex));
        notation.n++;
    }
    
    
}


static void
read_staff_ranges(char *name) {
    FILE *fp;
    int k;
    
    fp = fopen(name,"r");
    fscanf(fp,"staff num = %d\n",&staff_range.n);
    for (k=0; k < staff_range.n; k++)
        fscanf(fp,"lo = %d hi = %d\n",&staff_range.range[k].lo,&staff_range.range[k].hi);
    fclose(fp);
    
}




#define STAFF_PADDING 20

static int
src2dst(int h) {
    return(h*spect_wd/pageBitmap.pixelsWide);
}

static int
dst2src(int h) {
    return(h*pageBitmap.pixelsWide/spect_wd);
}

static int
src2dst2(int h, int p, int l) {
    /*  float t;
     t = h*spect_wd/(float)notation.page[p].bitmap.pixelsWide;
     return( (int) (t + .5));*/
    return(h*spect_wd/notation.page[p].bitmap.pixelsWide);
}

static int
dst2src2(int h ,int p, int l) {
    return(h*notation.page[p].bitmap.pixelsWide/spect_wd);
}



static int
staves_on_page() {
    int k;
    
    for (k=0; k < staff_range.n; k++)
        if (src2dst(staff_range.range[k].hi) > SPECT_HT) return(k);
    //  if (staff_range.range[k].hi*spect_wd/pageBitmap.pixelsWide > SPECT_HT) return(k);
    return(k);
}


static int
last_staff_on_top() {
    int k,k0,k1,lim,diff;
    
    k0 = page_status.last_staff;
    k1 = k0+1;
    for (k = k1; k < staff_range.n; k++) {
        diff = staff_range.range[k].hi-staff_range.range[k1].lo;
        if (src2dst(diff) > page_status.last_staff_top) return(k-1);
    }
    return(k-1);
}

static int
last_staff_on_bot() {
    int k,k0,k1,lim,diff;
    
    k0 = page_status.last_staff;
    k1 = k0+1;
    for (k = k1; k < staff_range.n; k++) {
        diff = staff_range.range[k].hi-staff_range.range[k1].lo;
        if (src2dst(diff) + STAFF_PADDING > (SPECT_HT-page_status.last_staff_bot)) return(k-1);
    }
    return(k-1);
}



static void
old_display_first_page() {
    int p,l,dsth,srch;
    RECT src,dst;
    PAGE *page;
    
    get_page_names();
    dst.loc.j = src.loc.j = 0;  dst.width = spect_wd;
    p=l=dsth=srch=0;
    while(1) {
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = page->map.range[l].lo;
        src.height = page->map.range[l].hi - src.loc.i;
        dst.loc.i = dsth;
        dst.height = src2dst2(src.height,p,l);
        if (dst.loc.i + dst.height >= SPECT_HT) break;
        // [view_self copyBitmap: page->bitmap source:src dest:dst];
        add_notation_rect(/*bitmap,*/p,l,src,dst);
        dsth += dst.height;
        if (++l == page->map.n) { l = 0; p++; }
        if (p == notation.n) break;
        
    }
    dst.loc.i = dsth;
    dst.height = SPECT_HT-dst.loc.i;
    // [view_self fillImageRect: dst val:255];
    fill_image_wide_rect(dst,255);
}


static RECT
center_rect(LOC c, int h, int w) {
    RECT r;
    
    r.loc.i = c.i - h/2;
    r.loc.j = c.j - w/2;
    r.height = h;
    r.width = w;
    return(r);
}

static RECT
src_rect2dst_rect(RECT r, int page) {
    r.loc.i = mod_out(src2dst2(r.loc.i, page,0));
    r.loc.j = src2dst2(r.loc.j, page,0);
    r.height = src2dst2(r.height, page,0);
    r.width = src2dst2(r.width,page,0);
    return(r);
}


#define HL_RAD 5

static void
highlight_note_heads() {
    int l,k,page;
    NOTE_HEAD_LIST *nhl;
    LOC loc;
    RECT r,q;
    
    nhl = &notation.page[0].dex;
    for (k=0; k < nhl->num; k++) {
        if (nhl->head[k].staff > page_status.lines_showing.hi) continue;
        loc = nhl->head[k].loc;
        loc.i *= 2; loc.j *= 2;
        loc.i -= notation.page[0].map.range[0].lo;
        q = center_rect(loc,20,30);
        q = src_rect2dst_rect(q,page=0);
        loc.i = src2dst2(loc.i, 0,0);
        loc.j = src2dst2(loc.j, 0,0);
        r.loc.i = loc.i-HL_RAD;
        r.loc.j = loc.j-HL_RAD;
        r.height = HL_RAD*2;
        r.width = HL_RAD*2;
        highlight_image_rect(q);
    }
}




static void
unhighlight_note_head(NOTATION_SCROLL_POS nsp) {
    IMAGE_PATCH *patch;
    int i,j,k,bpr;
    unsigned char *ptr,*row;
    
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    patch = &notation.page[nsp.page].staffdex.staff[nsp.staff].head[nsp.index].patch;
    for (i=0; i < patch->rows; i++) {
        row = ptr + (i+patch->loc.i)*bpr;
        for (j=0; j < patch->cols; j++) {
            for (k=0; k < 3; k++) row[3*(j+patch->loc.j)+k] =  patch->grey[i][j];
        }
        
    }
}

static void
highlight_note_head(NOTATION_SCROLL_POS nsp) {
    IMAGE_PATCH *patch;
    int i,j,k,bpr;
    unsigned char *ptr,*row;
    
    ptr = [bitmap bitmapData];
    bpr = [bitmap bytesPerRow];
    patch = &notation.page[nsp.page].staffdex.staff[nsp.staff].head[nsp.index].patch;
    for (i=0; i < patch->rows; i++) {
        row = ptr + mod_out(i+patch->loc.i)*bpr;
        for (j=0; j < patch->cols; j++) {
            for (k=0; k < 1; k++) row[3*(j+patch->loc.j)+k] =  255;
        }
        
    }
}

/*static void
 highlight_note_head(NOTATION_SCROLL_POS nsp) {
 LOC src_loc,dst_loc;
 int k;
 INT_RANGE ir;
 RECT q;
 IMAGE_INTERVAL ival;
 
 src_loc = notation.page[nsp.page].staffdex.staff[nsp.staff].head[nsp.index].loc;
 //   src_loc.i *= 2; src_loc.j *= 2; // MORE AWFUL KLUDGE
 for (k=0; k < page_status.imap.num; k++) {
 if (page_status.imap.list[k].filler) continue;
 ir = page_status.imap.list[k].src;
 if (src_loc.i >= ir.lo && src_loc.i < ir.hi) break;
 }
 if (k == page_status.imap.num) return; // didn't find it
 ival = page_status.imap.list[k];
 
 dst_loc.i = ival.dst.lo + (src_loc.i-ival.src.lo)*(ival.dst.hi-ival.dst.lo)/(ival.src.hi-ival.src.lo);
 dst_loc.j = (src_loc.j*(ival.dst.hi-ival.dst.lo))/(ival.src.hi-ival.src.lo);
 q = center_rect(dst_loc,20,30);
 highlight_image_rect(q);
 } */

static void
init_notation() {
    init_interval_map(&page_status.imap);
    init_image_patch_list();
}

static int
inc_page_line(int *p, int *l) { // return value is boolean for if the page has changed
    (*l)++;
    if (*l == notation.page[*p].staffdex.num) { *l = 0; (*p)++; return(1); }
    return(0);
}


static int
dec_page_line(int *p, int *l) { // return value is boolean for if the page has changed
    (*l)--;
    if (*l == -1) { (*p)--; *l = ((*p) < 0) ? 0 : notation.page[*p].staffdex.num-1; return(1); }
    return(0);
}


static int
compare_page_staff(PAGE_STAFF ps1, PAGE_STAFF ps2) {
    if (ps1.page != ps2.page) return(ps1.page-ps2.page);
    return(ps1.staff - ps2.staff);
}



static PAGE_STAFF
first_showing_line() {
    int i;
    PAGE_STAFF top,bot,cur;
    IMAGE_INTERVAL ival;
    
    //is_showing(int i) {
    /* top.page = page_status.imap.list[0].page;
     top.staff = page_status.imap.list[0].staff;
     bot = top;*/
    top.page = 1000;
    for (i=0; i < page_status.imap.num; i++) {
        ival = page_status.imap.list[i];
        if (ival.filler) continue;
        cur.page = ival.page;
        cur.staff = ival.staff;
        if (is_showing(ival.to.loc.i) == 0) continue;
        
        if (compare_page_staff(cur,top) < 0) top = cur;
    }
    return(top);
}

static PAGE_STAFF
last_showing_line() {
    int i;
    PAGE_STAFF bot,cur;
    IMAGE_INTERVAL ival;
    
    bot.page = bot.staff = 0;
    for (i=0; i < page_status.imap.num; i++) {
        ival = page_status.imap.list[i];
        if (ival.filler) continue;
        cur.page = ival.page;
        cur.staff = ival.staff;
        if (is_showing(ival.to.loc.i) == 0) continue;
        
        if (compare_page_staff(cur,bot) > 0) bot = cur;
    }
    return(bot);
}



static void
find_line_range(PAGE_STAFF *first, PAGE_STAFF *last) {
    int i;
    PAGE_STAFF top,bot,cur;
    IMAGE_INTERVAL ival;
    
    /* top.page = page_status.imap.list[0].page;
     top.staff = page_status.imap.list[0].staff;
     bot = top;*/
    top.page = 1000;  bot.page = -1;
    for (i=0; i < page_status.imap.num; i++) {
        ival = page_status.imap.list[i];
        if (ival.filler) continue;
        //   if (ival.)
        cur.page = ival.page;
        cur.staff = ival.staff;
        if (compare_page_staff(cur,top) < 0) top = cur;
        if (compare_page_staff(cur,bot) > 0) bot = cur;
    }
    *first = top; *last = bot;
    //    NSLog(@"top line is %d %d  bot line is %d %d",top.page,top.staff,bot.page,bot.staff);
}

static IMAGE_INTERVAL*
get_ival_ptr(PAGE_STAFF ps) {
    int i;
    IMAGE_INTERVAL *ival;
    
    for (i=0; i < page_status.imap.num; i++) {
        ival = page_status.imap.list + i;
        if (ival->filler) continue;
        if (ival->page == ps.page && ival->staff == ps.staff) return(ival);
    }
    return(NULL);
    
}


static void
fill_notation_scroll_buffer(int p, int l) {
    int dsth,srch;
    RECT src,dst;
    PAGE *page;
    
    
    page_status.lines_showing.lo = l;
    page_status.pages_showing.lo = p;
    dst.width = spect_wd;
    scan_top = dst.loc.j = src.loc.j = dsth = 0;
    top_line.staff = top_line.page = 0;
    
    page = notation.page + p;
    srch = page->map.range[l].lo;
    while(1) {
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = srch;  //page->map.range[l].lo;
        src.height = page->map.range[l].hi - src.loc.i;
        dst.loc.i = dsth;
        dst.height = src2dst2(src.height,p,l);
        if (dst.loc.i + dst.height >= NOTATION_BUFFER_SIZE) break;
        scan_bot = dst.height+dst.loc.i;
        // [view_self copyBitmap: page->bitmap source:src dest:dst];
        add_notation_rect(/*bitmap,*/p,l,src,dst);
        page_status.pages_showing.hi = p;
        page_status.lines_showing.hi = l;
        page->map.showing[l].lo = dst.loc.i + src2dst2(page->map.range[l].lo-srch,p,l);
        page->map.showing[l].hi = dst.height+dst.loc.i;
        dsth += dst.height;
        srch += src.height;
        if (++l == page->map.n) {
            l = 0; p++;
            page = notation.page + p;
            srch = page->map.range[l].lo;
        }
        if (p == notation.n) break;
    }
    
    bot_line.page = p; bot_line.staff = l;
    /* dst.loc.i = dsth;
     dst.height = SPECT_HT-dst.loc.i;
     fill_image_wide_rect(dst,255);  */
    dst.loc.i=0; dst.loc.j = spect_wd; dst.height = NOTATION_BUFFER_SIZE; dst.width = MAX_SPECT_WIDTH-spect_wd;
    [view_self fillImageRect: dst val:255];  // if page expanded want to see white space at right edge
    // NSLog(@"split point = %d",split_point());  // split_point() hangs.  don't know what it is for
    page_status.write_top = 1;
    find_line_range(&top_line,&bot_line);
    [view_self setNeedsDisplay:YES];
    
    
}



static void
fill_circ_scroll_buffer_fwd() {  // write to botom of region as far as poss.
    int dsth,srch,l,p,end;
    RECT src,dst;
    PAGE *page;
    PAGE_STAFF tp,bt;
    IMAGE_INTERVAL *ival;
    
    
    find_line_range(&tp,&bt);
    ival = get_ival_ptr(bt);
    p = bot_line.page; l = bot_line.staff;  // begin writing with bot_line
    inc_page_line(&p,&l);
    if (p == notation.n) return;
    
    dst.width = spect_wd;
    dst.loc.j = src.loc.j = 0;
    // top_line.staff = top_line.page = 0;
    //  top_line = bot_line;  // have no way of computing this without refering to the imap.
    
    //   dsth = scan_bot;  // start at the end of what was previously written
    dsth = mod_out(ival->to.loc.i + ival->to.height);
    
    /* page = notation.page + p;
     srch = page->map.range[l].lo;*/
    srch = notation.page[p].map.range[l].lo;
    //  while(1) {
    for (; p < notation.n; inc_page_line(&p,&l)) {
        if (l == 0) srch = notation.page[p].map.range[0].lo;
        //     if (p == notation.n)  { scan_line_num = dsth; break;}
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = srch;  //page->map.range[l].lo;
        src.height = page->map.range[l].hi - src.loc.i;
        dst.loc.i = dsth;
        dst.height = src2dst2(src.height,p,l);
        end = mod_out(dst.loc.i + dst.height);
        if (crosses_boundary(dst.loc.i,end, notation_scroll)) break;
        add_notation_rect(p,l,src,dst);
        dsth = end;
        srch += src.height;
        //   inc_page_line(&p,&l);
        
    }
    scan_bot = dsth; //dst.height+dst.loc.i;
    /*if (scan_top < scan_bot)*/ scan_top = scan_bot;  // not sure what this is about
    dst.loc.i=0; dst.loc.j = spect_wd; dst.height = NOTATION_BUFFER_SIZE; dst.width = MAX_SPECT_WIDTH-spect_wd;
    [view_self fillImageRect: dst val:255];  // if page expanded want to see white space at right edge
    page_status.write_top = 1;
    [view_self setNeedsDisplay:YES];
    find_line_range(&top_line,&bot_line);
    NSLog(@"find_line_rage bot_line = %d %d",bot_line.page,bot_line.staff);
}



static void
fill_circ_scroll_buffer_bwd() {  // write to botom of region as far as poss.
    int dsth,srch,l,p,end,scroll_bot;
    RECT src,dst;
    PAGE *page;
    PAGE_STAFF tp,bt;
    IMAGE_INTERVAL *ival;
    
    
    scroll_bot = mod_out(notation_scroll + SPECT_HT);
    find_line_range(&tp,&bt);
    p = top_line.page; l = top_line.staff;  // begin writing with bot_line
    if (p == 0 && l == 0) return;
    ival = get_ival_ptr(tp);
    if (ival == NULL) { printf("this should never happen\n"); exit(0); }
    dsth = ival->to.loc.i;
    dec_page_line(&p,&l);
    dst.width = spect_wd;
    dst.loc.j = src.loc.j = 0;
    //  dsth = scan_top;  // start at the end of what was previously written
    srch = notation.page[p].map.range[l].hi;
    for (; p >= 0; dec_page_line(&p,&l)) {
        if (l == notation.page[p].map.n-1) srch = notation.page[p].map.range[l].hi;
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = page->map.range[l].lo;
        src.height = srch - src.loc.i;
        dst.height = src2dst2(src.height,p,l);
        dst.loc.i = mod_out(dsth - dst.height);
        if (crosses_boundary(dst.loc.i, dsth, /*notation_scroll*/ scroll_bot)) break;
        add_notation_rect(p,l,src,dst);
        //     NSLog(@"add rect at src = %d dst = %d page = %d line = %dnotation_scroll = %d",src.loc.i,dst.loc.i,p,l,notation_scroll);
        dsth = dst.loc.i;
        srch = src.loc.i;
    }
    scan_top = scan_bot = dsth; //dst.height+dst.loc.i;
    dst.loc.i=0; dst.loc.j = spect_wd; dst.height = NOTATION_BUFFER_SIZE; dst.width = MAX_SPECT_WIDTH-spect_wd;
    [view_self fillImageRect: dst val:255];  // if page expanded want to see white space at right edge
    page_status.write_top = 1;
    [view_self setNeedsDisplay:YES];
    find_line_range(&top_line,&bot_line);
    NSLog(@"find_line_rage top_line = %d %d",top_line.page,top_line.staff);
}






static void
display_next_page(int p, int l) {
    int dsth,srch;
    RECT src,dst;
    PAGE *page;
    
    /*  p = page_status.pages_showing.hi;
     l = page_status.lines_showing.hi + 1;
     if (l == notation.page[p].staffdex.num) { l == 0; p++;}
     if (p == notation.n) return;*/
    
    page_status.lines_showing.lo = l;
    page_status.pages_showing.lo = p;
    dst.width = spect_wd;
    dst.loc.j = src.loc.j = dsth = 0;
    
    page = notation.page + p;
    srch = page->map.range[l].lo;
    while(1) {
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = srch;  //page->map.range[l].lo;
        src.height = page->map.range[l].hi - src.loc.i;
        dst.loc.i = dsth;
        dst.height = src2dst2(src.height,p,l);
        if (dst.loc.i + dst.height >= SPECT_HT) break;
        // [view_self copyBitmap: page->bitmap source:src dest:dst];
        add_notation_rect(/*bitmap,*/p,l,src,dst);
        page_status.pages_showing.hi = p;
        page_status.lines_showing.hi = l;
        page->map.showing[l].lo = dst.loc.i + src2dst2(page->map.range[l].lo-srch,p,l);
        page->map.showing[l].hi = dst.height+dst.loc.i;
        dsth += dst.height;
        srch += src.height;
        if (++l == page->map.n) {
            l = 0; p++;
            page = notation.page + p;
            srch = page->map.range[l].lo;
        }
        if (p == notation.n) break;
    }
    dst.loc.i = dsth;
    dst.height = SPECT_HT-dst.loc.i;
    fill_image_wide_rect(dst,255);  // might want this back later
    dst.loc.i=0; dst.loc.j = spect_wd; dst.height = SPECT_HT; dst.width = MAX_SPECT_WIDTH-spect_wd;
    [view_self fillImageRect: dst val:255];  // if page expanded want to see white space at right edge
    NSLog(@"split point = %d",split_point());
    page_status.write_top = 1;
    [view_self setNeedsDisplay:YES];
    
    
}




static void
display_first_page() {
    int p,l,dsth,srch;
    RECT src,dst;
    PAGE *page;
    
    
    init_notation();  // this might not be the right place for this ...
    get_page_names();
    dst.loc.j = src.loc.j = 0;  dst.width = spect_wd;
    p=l=dsth=0;
    page_status.pages_showing.lo = page_status.lines_showing.lo = 0;
    page_status.pages_showing.hi = page_status.lines_showing.hi = 0;
    
    fill_notation_scroll_buffer(0,0);
    print_imap(&page_status.imap);
    
    //  fill_circ_scroll_buffer_fwd();
    //display_next_page(0,0);
    // display_next_page();
    return;
    
    page = notation.page + p;
    srch = page->map.range[l].lo;
    while(1) {
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = srch;  //page->map.range[l].lo;
        src.height = page->map.range[l].hi - src.loc.i;
        dst.loc.i = dsth;
        dst.height = src2dst2(src.height,p,l);
        if (dst.loc.i + dst.height >= SPECT_HT) break;
        // [view_self copyBitmap: page->bitmap source:src dest:dst];
        add_notation_rect(/*bitmap,*/p,l,src,dst);
        page_status.pages_showing.hi = p;
        page_status.lines_showing.hi = l;
        page->map.showing[l].lo = dst.loc.i + src2dst2(page->map.range[l].lo-srch,p,l);
        page->map.showing[l].hi = dst.height+dst.loc.i;
        dsth += dst.height;
        srch += src.height;
        if (++l == page->map.n) {
            l = 0; p++;
            page = notation.page + p;
            srch = page->map.range[l].lo;
        }
        if (p == notation.n) break;
        
    }
    dst.loc.i = dsth;
    dst.height = SPECT_HT-dst.loc.i;
    
    // [view_self fillImageRect: dst val:255];
    fill_image_wide_rect(dst,255);
    dst.loc.i=0; dst.loc.j = spect_wd; dst.height = SPECT_HT; dst.width = MAX_SPECT_WIDTH-spect_wd;
    [view_self fillImageRect: dst val:255];  // if page expanded want to see white space at right edge
    NSLog(@"split point = %d",split_point());
    page_status.write_top = 1;
    // highlight_note_heads();
    [view_self setNeedsDisplay:YES];
}


static void
display_notation_in_box(INT_RANGE range, int start_page, int start_line) {
    int p,l,dsth,srch;
    RECT src,dst;
    PAGE *page;
    
    dst.loc.j = src.loc.j = 0;  dst.width = spect_wd;
    p = start_page;
    l = start_line;
    dsth = range.lo;
    page = notation.page + p;
    srch = page->map.range[l].lo;
    while(1) {
        page = notation.page + p;
        src.width = page->bitmap.pixelsWide;
        src.loc.i = srch;  //page->map.range[l].lo;
        src.height = page->map.range[l].hi - src.loc.i;
        dst.loc.i = dsth;
        dst.height = src2dst2(src.height,p,l);
        if (dst.loc.i + dst.height >= range.hi) break;
        // [view_self copyBitmap: page->bitmap source:src dest:dst];
        add_notation_rect(/*bitmap,*/p,l,src,dst);
        page_status.pages_showing.hi = p;
        page_status.lines_showing.hi = l;
        page->map.showing[l].lo = dst.loc.i + src2dst2(page->map.range[l].lo-srch,p,l);
        page->map.showing[l].hi = dst.loc.i + dst.height;
        
        dsth += dst.height;
        srch += src.height;
        if (++l == page->map.n) {
            l = 0; p++;
            page = notation.page + p;
            srch = page->map.range[l].lo;
        }
        if (p == notation.n) break;
        
    }
    dst.loc.i = dsth;
    dst.height = range.hi-dst.loc.i;
    // [view_self fillImageRect: dst val:255];
    fill_image_wide_rect(dst,255);
    [view_self setNeedsDisplay:YES];
}




static void
display_visible_page() {
    int p,l,disth;
    PAGE *page;
    INT_RANGE r;
    
    p=l=disth=0;
    init_notation();  // this might not be the right place for this ...
    get_page_names();
    page_status.pages_showing.lo = page_status.lines_showing.lo = 0;
    page_status.pages_showing.hi = page_status.lines_showing.hi = 0;
    page = notation.page + p;
    r.lo = 0; r.hi = SPECT_HT;
    page_status.pages_showing.lo = p;
    page_status.lines_showing.lo = l;
    display_notation_in_box(r, p,l);
    page_status.write_top = 1;
    NSLog(@"lo = %d hi = %d",page_status.lines_showing.lo,page_status.lines_showing.hi);
}



- (void) showFirstPage {
    char name[500],staff_name[500];
    RECT r1,r2;
    int last_staff,k;
    extern int SPECT_HT;
    NOTATION_SCROLL_POS nsp;
    
    //   display_first_page();  // this is for the scrolling and selecting
    display_visible_page();  // this is to do begin the half-page writing for live accompaiment with write_page_top and write_page_bot */
    cur_note = 0;
    highlight_note_head(index2nsp(0));
    
    
    [view_self setNeedsDisplay: YES];
}


static void
write_page_top() {
    INT_RANGE r;
    int p,l,sp,nxt;
    PAGE *page;
    
    split_line(&p,&l);
    page = notation.page + p;
    r.lo = 0; r.hi = sp = page->map.showing[l].lo;
    page_status.pages_showing.lo = p;
    page_status.lines_showing.lo = l;
    p = page_status.pages_showing.hi;
    l = page_status.lines_showing.hi;
    inc_pl(&p,&l);
    page_status.ptop = p; page_status.ltop = l;
    display_notation_in_box(r, p,l);
    p = page_status.pages_showing.hi;
    l = page_status.lines_showing.hi;
    page = notation.page + p;
    nxt = page->map.showing[l].hi;
    add_dotted_line((sp+nxt)/2);
    page_status.write_top = 0;
    
    NSLog(@"lo = %d hi = %d",page_status.lines_showing.lo,page_status.lines_showing.hi);
}


static void
write_page_bot() {
    INT_RANGE r;
    int p,l,sp,nxt;
    PAGE *page;
    
    p = page_status.pages_showing.hi;
    l = page_status.lines_showing.hi;  // most recently written
    page = notation.page + p;
    r.hi = SPECT_HT; r.lo = sp = page->map.showing[l].hi;
    inc_pl(&p,&l);
    display_notation_in_box(r, p,l);
    page_status.write_top = 1;
    page_status.pages_showing.lo = page_status.ptop;
    page_status.lines_showing.lo = page_status.ltop;
    NSLog(@"lo = %d hi = %d",page_status.lines_showing.lo,page_status.lines_showing.hi);
    
}



static int
compare_nsps(NOTATION_SCROLL_POS nsp1, NOTATION_SCROLL_POS nsp2) {
    /* negative: nsp1 < nsp2; 0: nsp1=nsp2;  positive: nsp1 > nsp2 */
    if (nsp1.page != nsp2.page) return(nsp1.page-nsp2.page);
    if (nsp1.staff != nsp2.staff) return(nsp1.staff-nsp2.staff);
    if (nsp1.index != nsp2.index) return(nsp1.index-nsp2.index);
    return(0);
}

static void
inc_nsp(NOTATION_SCROLL_POS *nsp) {
    
    nsp->index++;
    if (nsp->index >= notation.page[nsp->page].staffdex.staff[nsp->staff].num) {
        nsp->index = 0;
        nsp->staff++;
        if (nsp->staff ==  notation.page[nsp->page].staffdex.num) {
            nsp->staff = 0;
            nsp->page++;
        }
    }
    
}

static int
head_showing(NOTATION_SCROLL_POS nsp) {
    NOTATION_SCROLL_POS lo,hi;
    PAGE_STAFF ps;
    
    ps.page = nsp.page; ps.staff = nsp.staff;
    //   NSLog(@"bot_line = %d %d ps = %d %d",bot_line.page,bot_line.staff,ps.page,ps.staff);
    return(compare_page_staff(top_line,ps) <= 0 && compare_page_staff(bot_line,ps) >= 0);
    
    lo.page = top_line.page; lo.staff = top_line.staff; lo.index = 0;
    hi.page = bot_line.page; hi.staff = bot_line.staff; hi.index = 0;
    return(compare_nsps(nsp,lo) >= 0    && compare_nsps(nsp,hi) < 0);
    
}

static NOTATION_SCROLL_POS
index2nsp(int i)  {
    char tag[500],*t;
    int k;
    NOTATION_SCROLL_POS nsp;
    
    t = score.solo.note[i].observable_tag;
    t += 5;  // get rid of "solo_"
    nsp.page = nsp.staff = nsp.index = 0;
    while(nsp.page < notation.n) {
        if (strcmp(notation.page[nsp.page].staffdex.staff[nsp.staff].head[nsp.index].tag,t) == 0) return(nsp);
        inc_nsp(&nsp);
    }
    NSLog(@"not found ind index2nsp"); exit(0);
    
    
    
}


static void
modify_selected_heads(NOTATION_SCROLL_POS lo, NOTATION_SCROLL_POS hi, int add) {
    // NSLog(@"top line = %d %d",top_line.page,top_line.staff);
    NSLog(@"hi = %d %d %d",hi.page,hi.staff,hi.index);
    while (compare_nsps(lo,hi) < 0) {
        if (head_showing(lo)) {
            if (add) highlight_note_head(lo);
            else unhighlight_note_head(lo);
        }
        inc_nsp(&lo);
    }
}

static void
update_selection(NOTATION_SCROLL_POS last, NOTATION_SCROLL_POS this) {
    int c,add;
    
    c = compare_nsps(this,last);
    if (c == 0) return;
    else if (c > 0) modify_selected_heads(last,this,add=1);
    else  modify_selected_heads(this,last,add=0);
    NSLog(@"this = %d %d %d last = %d %d %d",this.page,this.staff,this.index,last.page,last.staff,last.index);
    [view_self setNeedsDisplay:YES];
    
}

static void
clear_note_selection(NOTATION_SCROLL_POS last, NOTATION_SCROLL_POS this) {
    int c,add;
    
    c = compare_nsps(this,last);
    if (c == 0) return;
    modify_selected_heads(last,this,add=0);
    [view_self setNeedsDisplay:YES];
    
}




static NOTATION_SCROLL_POS last_nsp = {0,0,0};
static NOTATION_SCROLL_POS start_nsp = {-1,0,0};  // -1 means not yet set

-(void) mouseMoved: (NSEvent *) e {
    NSPoint q;
    LOC loc,head;
    RECT r;
    NOTATION_SCROLL_POS nsp;
    
    
    NSPoint pt = [e locationInWindow];
    
    q = [self convertPoint:pt fromView:nil];
    NSLog(@"mouse: %f %f",q.x,q.y);  // leave this in to remind about the mouse moves and the computational burden of this
    loc.i = (int) (SPECT_HT-q.y+.5);
    loc.j = (int) (q.x+.5);
    // NSLog(@"on note? : %d",pos_on_note(loc,&head));
    
    nsp = pos2nsp(loc);
    update_selection(last_nsp,nsp);
    last_nsp = nsp;
    //  NSLog(@"nsp = %d %d %d",nsp.page,nsp.staff,nsp.index);
    
    
    
    if(pos_on_note(loc,&nsp, &head) ) {
        r.loc.i  = head.i-HL_RAD;
        r.loc.j = head.j-HL_RAD;
        r.height = HL_RAD*2;
        r.width = HL_RAD*2;
        highlight_image_rect(r);
        [view_self setNeedsDisplay:YES];
    }
    //    if (/*q.x < 0 || q.y < 0 || */q.x >= [pageBitmap pixelsHigh] ||  q.y >=[pageBitmap  pixelsWide]) return;
    //  [[NSApp delegate] showMousePos:[self mouse2ArrayLoc:pt]];
}

static void
move_note_head_forward() {
    PAGE_STAFF ps;
    NOTATION_SCROLL_POS nsp;
    int last_line;
    
    unhighlight_note_head(index2nsp(cur_note++));
    nsp = index2nsp(cur_note);
    highlight_note_head(nsp);
    ps = last_showing_line();
    last_line = ((ps.page+1) == notation.n && (ps.staff+1) == notation.page[ps.page].staffdex.num);
    if (!last_line && ps.page == nsp.page && ps.staff == nsp.staff) {
        if (page_status.write_top) write_page_top();
        else write_page_bot();
        
    }
    [view_self setNeedsDisplay:YES];
    
}

-(void) mouseDown: (NSEvent *) e {
    NSPoint q;
    LOC loc,head,score_loc;
    RECT r;
    NOTATION_SCROLL_POS nsp;
    int p,l;
    
#ifndef NOTATION
    return;
#endif
    move_note_head_forward(); return;
    if (page_status.write_top) write_page_top();
    else write_page_bot();
    return;
    
    
    /*  p = page_status.pages_showing.hi;
     l = page_status.lines_showing.hi;
     inc_pl(&p,&l);
     display_next_page(p,l); return;*/
    
    NSPoint pt = [e locationInWindow];
    
    q = [self convertPoint:pt fromView:nil];
    loc.i = (int) (SPECT_HT-q.y+.5);
    loc.j = (int) (q.x+.5);
    loc.i = mod_out(loc.i+notation_scroll);
    // NSLog(@"on note? : %d",pos_on_note(loc,&head));
    
    /* nsp = pos2nsp(loc);
     update_selection(last_nsp,nsp);
     last_nsp = nsp;
     NSLog(@"nsp = %d %d %d",nsp.page,nsp.staff,nsp.index);*/
    
    
    
    if (pos_on_note(loc,&nsp, &head) == 0) return;
    if (start_nsp.page != -1) clear_note_selection(start_nsp,last_nsp);
    last_nsp = start_nsp = nsp;
    inc_nsp(&last_nsp);
    highlight_note_head(start_nsp);
    [view_self setNeedsDisplay:YES];
    NSLog(@"on note %d %d %d",start_nsp.page,start_nsp.staff,start_nsp.index);
    //  NSPoint p = [NSEvent mouseLocation];
    // p.x -= 100;
    //  p.y -= 100;
    
    //  CGDisplayMoveCur
    //  CGGetDisplaysWithPoint
    /*
     CGEventRef ourEvent = CGEventCreate(NULL);
     CGPoint mouseLocation = CGEventGetLocation(ourEvent);
     mouseLocation.x += 100;
     CGWarpMouseCursorPosition(mouseLocation);*/
    
}


static int
scroll_cushion() { // how many scan lines are left in buffer before we need to recompute more image data?
    int n = NOTATION_BUFFER_SIZE;
    if (scroll_dir == 1) return(scan_bot-(notation_scroll+SPECT_HT)); // scrolling down
    else return(notation_scroll-scan_top);
}


#define REFRESH_MARGIN 100

static int
needs_notation_buffer_refresh() {
    int n = NOTATION_BUFFER_SIZE,lp,d;
    
    
    if (scroll_dir == 1){
        if (bot_line.page == (notation.n-1) && bot_line.staff == (notation.page[notation.n-1].staffdex.num-1)) return(0);
        d = scan_bot-(notation_scroll+SPECT_HT);
        if (d < 0) d += NOTATION_BUFFER_SIZE;
        if (d < REFRESH_MARGIN) NSLog(@"needs refresh scrolling dn");
        return(d < REFRESH_MARGIN); // scrolling down
    }
    else {
        if (top_line.page == 0 && top_line.staff == 0) return(0);
        d = mod_out(notation_scroll-/*scan_top*/scan_bot);
        //     NSLog(@" d = %d",d);
        if (d < REFRESH_MARGIN) NSLog(@"needs refresh scrolling up");
        return(d < REFRESH_MARGIN);
        
    }
}

static int
needs_notation_buffer_refresh_arg(inc) {
    int n = NOTATION_BUFFER_SIZE,lp,d;
    
    
    if (inc > 0){
        if (bot_line.page == (notation.n-1) && bot_line.staff == (notation.page[notation.n-1].staffdex.num-1)) return(0);
        d = scan_bot-(notation_scroll+SPECT_HT);
        if (d < 0) d += NOTATION_BUFFER_SIZE;
        if (d < REFRESH_MARGIN) NSLog(@"needs refresh scrolling dn");
        return(d < REFRESH_MARGIN); // scrolling down
    }
    else {
        if (top_line.page == 0 && top_line.staff == 0) return(0);
        d = mod_out(notation_scroll-/*scan_top*/scan_bot);
        //     NSLog(@" d = %d",d);
        if (d < REFRESH_MARGIN) NSLog(@"needs refresh scrolling up");
        return(d < REFRESH_MARGIN);
        
    }
}



#define SCROLL_INC 5


static int
is_at_bot_inc(int inc) {
    int i,last_scan_line,scr1,scr2,last_page,last_line;;
    IMAGE_INTERVAL *ival;
    
    
    if (inc < 0) return(0);
    for (i=0; i < page_status.imap.num; i++) {
        ival = page_status.imap.list + i;
        last_page = notation.n - 1;
        last_line = notation.page[last_page].staffdex.num-1;
        if (ival->page != last_page || ival->staff != last_line) continue;
        last_scan_line = mod_out(ival->to.loc.i + ival->to.height);
        scr1 = mod_out(notation_scroll + SPECT_HT);
        scr2 = mod_out(scr1 + inc);
        return(crosses_boundary(scr1,scr2,last_scan_line));
    }
    return(0);
}


static int
is_at_top_inc(int inc) {
    int i,first_scan_line,scr1,scr2,last_page,last_line;;
    IMAGE_INTERVAL *ival;
    
    if (inc > 0) return(0);  // only can be at top if moving upwards
    for (i=0; i < page_status.imap.num; i++) {
        ival = page_status.imap.list + i;
        if (ival->page != 0 || ival->staff != 0) continue;
        first_scan_line = ival->to.loc.i;
        scr1 = notation_scroll;  // we are here ...
        scr2 = mod_out(scr1 + inc);  // but going to be here ...
        return(in_order(scr2,first_scan_line,scr1));
    }
    return(0);
}




static void
increment_scroll_pos(int inc) {
    if (is_at_top_inc(inc)) return;
    if (is_at_bot_inc(inc)) return;
    
    notation_scroll = mod_out(notation_scroll + inc);
    [view_self setNeedsDisplay:YES];
    if ( needs_notation_buffer_refresh_arg(inc)) {
        if (inc > 0) fill_circ_scroll_buffer_fwd();
        else fill_circ_scroll_buffer_bwd();
    }
}




- (void) scroll_timer {
    NSPoint p,q;
    NSRect r;
    int at_top,at_bot,d;
    
    
    /*  CGEventRef ourEvent = CGEventCreate(NULL);
     CGPoint mouseLocation = CGEventGetLocation(ourEvent);
     q = [self convertPoint:p toView: nil];*/
    
    q =  [self convertPoint:[[self window] mouseLocationOutsideOfEventStream] fromView:nil];
    d = SCROLL_INC*scroll_dir;
    if (is_at_top_inc(d) || is_at_bot_inc(d) || (q.y > 10 &&  q.y < SPECT_HT-10)) {
        [nsTimer invalidate]; nsTimer = nil;
        //NSLog(@"attop = %d",at_top);
        return;
    }
    
    increment_scroll_pos(d);
    return;
    
    /*
     
     // NSLog(@"mouse at %f %f scroll at %d",q.x,q.y,notation_scroll);
     notation_scroll = mod_out(notation_scroll + SCROLL_INC*scroll_dir);
     [view_self setNeedsDisplay:YES];
     if (needs_notation_buffer_refresh()) {
     
     if (scroll_dir == 1) fill_circ_scroll_buffer_fwd();
     else fill_circ_scroll_buffer_bwd();
     NSLog(@"refreshing  scroll_dir = %d now showing %d %d to %d %d",scroll_dir, top_line.page,top_line.staff,bot_line.page,bot_line.staff);
     }*/
}


static LOC
window_pt2loc(NSPoint q) {  // convert window coords (origin at lower left) to LOC.  the LOC will be on the image
    LOC loc;
    
    loc.i = (int) (SPECT_HT-q.y+.5);
    if (loc.i < 0) loc.i = 0;
    if (loc.i >= SPECT_HT) loc.i = SPECT_HT-1;
    loc.j = (int) (q.x+.5);
    return(loc);
}


/*NSPoint initial_touch_pos;
 int initial_scroll_pos;
 
 - (void)touchesBeganWithEvent:(NSEvent *)ev {
 
 NSSet *touches = [ev touchesMatchingPhase:NSTouchPhaseBegan inView:self];
 NSLog(@"got touch with %d fingers",touches.count);
 for (NSTouch *touch in touches) {
 
 initial_touch_pos = touch.normalizedPosition;
 initial_scroll_pos = notation_scroll;
 break; // just use first finger found
 //  NSPoint fraction = touch.normalizedPosition;
 }
 }*/

- (void) scrollWithEvent: (NSEvent *) ev {
    NSLog(@"scroll gesture");
}

- (void) swipeWithEvent: (NSEvent *) ev {
    NSLog(@"swipe gesture");
}

- (void) scrollWheel: (NSEvent *) ev {
    int d;
    d =  [ev scrollingDeltaY];
    if (d == 0) return;
    increment_scroll_pos(d);
    /* notation_scroll += d;
     [view_self setNeedsDisplay:YES];*/
    // if (x != 0)  NSLog(@"scroll event (two finger scroll?) %f ",x);
    
}


/*- (void)touchesMovedWithEvent:(NSEvent *)ev {
 NSPoint pt;
 NSLog(@"touch moved");
 
 NSSet *touches = [ev touchesMatchingPhase:NSTouchPhaseBegan inView:self];
 
 for (NSTouch *touch in touches) {
 
 pt = touch.normalizedPosition;
 break;
 notation_scroll = initial_scroll_pos + (pt.y-initial_touch_pos.y);
 
 }
 notation_scroll = initial_scroll_pos + (pt.y-initial_touch_pos.y);
 [view_self setNeedsDisplay:YES];
 }*/

- (void)mouseDragged:(NSEvent *)e {
    NSPoint q;
    LOC screen_loc,score_loc,head;
    RECT r;
    NOTATION_SCROLL_POS nsp;
    int scroll_up,scroll_dn;
    
    
    NSPoint pt = [e locationInWindow];
    
    q = [self convertPoint:pt fromView:nil];
    screen_loc = window_pt2loc(q);
    /* screen_loc.i = (int) (SPECT_HT-q.y+.5);
     if (screen_loc.i < 0) screen_loc.i = 0;
     if (screen_loc.i >= SPECT_HT) screen_loc.i = SPECT_HT-1;
     screen_loc.j = (int) (q.x+.5);*/
    scroll_dn = (screen_loc.i > (SPECT_HT-10));
    scroll_up = (screen_loc.i < 10);
    if ((scroll_up || scroll_dn) && nsTimer == nil) {
        NSLog(@"time to scroll");
        nsTimer =  [NSTimer scheduledTimerWithTimeInterval:0.01
                                                    target:view_self
                                                  selector:@selector(scroll_timer)
                                                  userInfo:NULL
                                                   repeats:YES];
        if (scroll_up) scroll_dir = -1;
        if (scroll_dn) scroll_dir = 1;
    }
    if (start_nsp.page == -1) return; // start not set yet
    score_loc = screen_loc;
    // score_loc.i += notation_scroll;  // should be mod NOTATION_BUFFER_SIZE?
    score_loc.i = mod_out(score_loc.i+notation_scroll);
    //  score_loc.i = (score_loc.i+notation_scroll)%NOTATION_BUFFER_SIZE;
    nsp = pos2nsp(score_loc);
    if (compare_nsps(nsp,start_nsp) < 0) return;  // only meaningful if after start_nsp
    //  NSLog(@"nsp = %d %d %d",nsp.page,nsp.staff,nsp.index);
    update_selection(last_nsp,nsp);
    last_nsp = nsp;
}

-(void) mouseUp: (NSEvent *) e {
    NSLog(@"range = %d %d %d to %d %d %d",start_nsp.page,start_nsp.staff,start_nsp.index,last_nsp.page,last_nsp.staff,last_nsp.index);
    
}


@end


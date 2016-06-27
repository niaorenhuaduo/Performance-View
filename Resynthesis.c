//
//  Resynthesis.c
//  Performance View
//
//  Created by Sanna Wager on 6/27/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#include "Resynthesis.h"
#include "global.h"

//
//  Resynthesis.c
//  Performance View
//
//  Created by Sanna Wager on 6/27/16.
//  Copyright © 2016 craphael. All rights reserved.
//


typedef struct {
    float hz;
    float amp;
    //add other features?
} AUDIO_FEATURE;


float cal_pitch_yin(unsigned char *ptr, float hz0) {
    int buffer_length;
    buffer_length = 5*((float) SAMPLE_SR/hz0);
    Yin yin;
    Yin_init(&yin, buffer_length, 0.6);
    return(Yin_getPitch(&yin, ptr, hz0));
}


float cal_amp(unsigned char *ptr) {
    float amp;
    samples2floats(ptr, data, FRAMELEN);
    amp = 0;
    for (int i = 0; i < FRAMELEN; i++) { //sum of squares
        amp += data[i]*data[i];
    }
    return(sqrtf(amp/FRAMELEN));
}


//calculate feature vector for a given frame of audio
float cal_feature(unsigned char *ptr, float hz0) {
    AUDIO_FEATURE af;
    af.hz = cal_pitch_yin(ptr, hz0);
    af.amp = cal_amp(ptr);
    //add other features?
}

//measure similarity between two audio frame feature vectors
float compare_feature(int ff1, int ff2) {
    
}
/*
 int binary_search(int firstframe, int lastframe, int frame)
 {
 int c, first, last, middle, n, search;
 n = firstframe - lastframe;
 
 first = 0;
 last = n - 1;
 middle = (first+last)/2;
 
 while (first <= last) {
 if (score.solo.note[middle].frames < search)
 first = middle;
 else if (score.solo.note[middle+1].frames >= search)
 int midi = 0; //???
 else
 last = middle + 1;
 middle = (first + last)/2;
 }
 
 while (first <= last) {
 if (array[middle] < search)
 first = middle + 1;
 else if (array[middle] == search) {
 printf("%d found at location %d.\n", search, middle+1);
 break;
 }
 else
 last = middle - 1;
 
 middle = (first + last)/2;
 }
 if (first > last)
 printf("Not found! %d is not present in the list.\n", search);
 
 return 0;
 }*/


//audiodata_target
void prep_cal_feature(int frame, unsigned char* audioname) {
    int offset, first, last, index;
    unsigned char* ptr;
    float hz0;
    if (audioname != audiodata && audioname != audiodata_target) { printf("prep_cal_feature argument is audiodata or audiodata_target"); exit(0); }
    offset = frame*SKIPLEN*BYTES_PER_SAMPLE;
    
    if (audioname == audiodata) {
        first = firstnote;
        last = lastnote;
    } else {
        //first = firstnote_target;
        //last = lastnote_target;
    }
    
    ptr = audioname + offset;
    
    if (frame < first || frame >= last) { printf("prep_cal_feature frame index out of score bounds"); exit(0); }
    
    //s = score.solo.note[j].frames; //starting position
    //e = score.solo.note[j+1].frames; //ending position
    //midi = score.solo.note[j].snd_notes.snd_nums[0]; //midi pitch
    
    //color = COLOR_Q | (f*COLOR_T);
    
    //hz0 = (int) (pow(2,((midi - 69)/12.0)) * 440);
}



/*
 
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
 }*/

/*
 
 void calculate_amplitude(startframe, endframe) {
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
 inst_amp[j]/=FRAMELEN;
 inst_amp[j] = sqrtf(inst_amp[j]);
 }
 FILE *fp;
 fp = fopen("/Users/apple/Documents/Performance-View/user/audio/inst_amp", "w");
 fwrite(inst_amp, sizeof(float), 4500, fp);
 fclose(fp);
 } */
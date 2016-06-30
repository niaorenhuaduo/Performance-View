//
//  Resynthesis.c
//  Performance View
//
//  Created by Sanna Wager on 6/27/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#include "Resynthesis.h"
#include "global.h"


float **similarity;
AUDIO_FEATURE *audio_feature;
int *best_match;


float cal_pitch_yin(unsigned char *ptr, float hz0) {
    int buffer_length;
    buffer_length = 5*((float) OUTPUT_SR/hz0);
    buffer_length = 900;
    Yin yin;
    Yin_init(&yin, buffer_length, 1);
    return(Yin_getPitch(&yin, ptr, hz0));
}


float cal_amp(unsigned char *ptr) {
    float amp;
    samples2floats(ptr, data48k, FRAMELEN_48k);
    amp = 0;
    for (int i = 0; i < FRAMELEN_48k; i++) { //sum of squares
        amp += data48k[i]*data48k[i];
    }
    return(sqrtf(amp/FRAMELEN_48k));
}




//measure similarity between two audio frame feature vectors
float compare_feature(AUDIO_FEATURE ff1, AUDIO_FEATURE ff2) {
    float euclid_dist = powf(ff1.hz - ff2.hz, 2) + powf(1000*(ff1.amp - ff2.amp), 2);
    return euclid_dist;
}

//read in 48k data
//read in score, find offset times for each pitch change
//for each frame, consecutively, calculate features and write to file


int binary_search(int firstnote, int lastnote, int search)
//finds which midi pitch the search frame belongs to
{
    int c, first, last, middle;
    
    first = firstnote;
    last = lastnote;
    middle = (first+last)/2;
    
    while (first < last) {
        if (score.solo.note[middle].frames > search) //frame occured before the beginning of the middle note
            last = middle;
        else if (score.solo.note[middle+1].frames > search) { //found note
            return score.solo.note[middle].snd_notes.snd_nums[0]; //return the midi pitch
        }
        else //frame occurred later than the current note
            first = middle + 1;
        middle = (first + last)/2;
    }
    
    return(-1);
}

int frame2offset_48k(int frame) {
    int offset = frame * SKIPLEN * 6 *BYTES_PER_SAMPLE;
    return offset;
}



void write_features(char *name) {
    //FILE *test;
    //test = fopen("/Users/apple/Documents/Performance-View/user/audio/sanna/mozart_voi_che_sapete/test.txt", "wb");
    //if (test == NULL) { printf("can't open %s\n","/Users/apple/Documents/Performance-View/user/audio/sanna/mozart_voi_che_sapete/test.txt"); return; }
    
    FILE *fp;
    fp = fopen(name, "w+");
    if (fp == NULL) { printf("can't open %s\n",name); return; }
    
    
    for (int i = firstnote; i <= lastnote; i++) {
        float time = frame2offset_48k(score.solo.note[i].frames) / (float) 48000 - 53.312;
        printf("\note absolute time %f", time);
    }
    
    AUDIO_FEATURE af;
    int start, end, s, e, midi, frame_8k, offset, frames;
    float hz0, abs_time;
    unsigned char *ptr;
    frames = get_frame_count();
    fprintf(fp, "\n%d", frames);
    for (int i = 0; i < frames; i++) {
        //if (i > 1000) continue; //???
        frame_8k = i * HOP_LEN / (float) (SKIPLEN * 6);
        midi = binary_search(firstnote, lastnote, frame_8k);
        if (midi == -1) continue; //note out of bounds
        hz0 = (int) (pow(2,((midi - 69)/12.0)) * 440);
        offset = frame2offset_48k(i);
        abs_time = (float) offset/48000 - 53.312;
        af = cal_feature(offset, hz0);
        fprintf(fp,"\n%d\t%f\t%f\t%f\t%f", i, af.hz, af.amp, hz0, abs_time);
        //fprintf(test, "\n%f", af.hz);

    }
    
    fclose(fp);
    //fclose(test);
}


/*
void test_swap_similar_frames() {
    AUDIO_FEATURE ff1, ff2;
    init_similarity();
    float threshold = 0.1;
    float mean = 0, val;
    int dim = score.solo.note[lastnote].frames - score.solo.note[firstnote].frames;
    int offset = score.solo.note[firstnote].frames;
    for (int i = 0; i < dim; i++)
        audio_feature[i] = prep_cal_feature(i+offset, audiodata);
    for (int i = 0; i < dim; i++) {
        best_match[i] = -1;
        val = HUGE_VAL;
        for (int j = 0; j < dim; j++) {
            similarity[i][j] = compare_feature(audio_feature[i], audio_feature[j]);
            if (similarity[i][j] < val && i != j) {
                val = similarity[i][j];
                best_match[i] = j;
            }
        }
        printf("\nbest match for %d is %d", i, best_match[i]);
    }
}
*/

/*
 //audiodata_target
 AUDIO_FEATURE prep_cal_feature(int frame, unsigned char* audioname) {
 int offset, first, last, midi;
 unsigned char* ptr;
 float hz0;
 if (!(audioname != audiodata || audioname != audiodata_target)) { printf("prep_cal_feature argument is audiodata or audiodata_target"); exit(0); }
 offset = frame*SKIPLEN*BYTES_PER_SAMPLE;
 
 if (audioname == audiodata) {
 first = firstnote;
 last = lastnote;
 } else {
 //first = firstnote_target; //need to create these
 //last = lastnote_target;
 }
 
 ptr = audioname + offset;
 int temp = score.solo.note[first].frames;
 if (frame < score.solo.note[first].frames || frame > score.solo.note[last].frames) { printf("prep_cal_feature frame index out of score bounds"); exit(0); }
 
 midi = binary_search(first, last, frame);
 hz0 = (int) (pow(2,((midi - 69)/12.0)) * 440);
 
 return cal_feature(ptr, hz0);
 }
 
 */





/*
void init_similarity() {
    int dim = score.solo.note[lastnote].frames - score.solo.note[firstnote].frames + 1;
    int test1 = score.solo.note[lastnote].frames;
    int test2 = score.solo.note[firstnote].frames;
    similarity = (float **) malloc(dim*sizeof(float *));
    for (int i = 0; i < dim; i++) {
        similarity[i] = (float *) malloc(dim*sizeof(float));
    }
    audio_feature = (AUDIO_FEATURE *) malloc(dim*sizeof(AUDIO_FEATURE));
    best_match = (int *) malloc(dim*sizeof(int));
}
*/
 

//add a file for each recording, with the features
//resynthesize from collection to test the correctness of the feature vector
//euclidean distance
//set absolute threshold on pitch error? or, change audioframes by resampling? -later
//pv is at 48000 khz
//find conversion function for frame position frame2?
//match every frame to the best possible match?
//check that amplitude works with resynthesis
//add timbre?
//for now, glissando between pitches is ok - change later?




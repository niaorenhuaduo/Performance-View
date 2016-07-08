//
//  Resynthesis.c
//  Performance View
//
//  Created by Sanna Wager on 6/27/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#include "Resynthesis.h"
#include "global.h"


float cal_pitch_yin(unsigned char *ptr, float hz0) {
    int buffer_length;
    buffer_length = 5*((float) SAMPLE_SR/hz0);
    Yin yin;
    Yin_init(&yin, buffer_length, 0.6);
    return(Yin_getPitch(&yin, ptr, hz0));
}


float cal_amp(unsigned char *ptr) {
    float amp;
    samples2floats(ptr, data_48k, FREQDIM);
    amp = 0;
    for (int i = 0; i < FREQDIM; i++) { //sum of squares
        amp += data_48k[i]*data_48k[i]*coswindow_1024[i];
    }
    return(sqrtf(amp/FREQDIM));
}


//calculate feature vector for a given frame of audio
AUDIO_FEATURE cal_feature(unsigned char *ptr, float hz0) {
    AUDIO_FEATURE af;
    af.hz = cal_pitch_yin(ptr, hz0);
    af.amp = cal_amp(ptr);
    return af;
}

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
            if (is_solo_rest(middle) == 1) {
                return(-2); //note is a rest
            }
            return score.solo.note[middle].snd_notes.snd_nums[0]; //return the midi pitch
        }
        else //frame occurred later than the current note
            first = middle + 1;
        middle = (first + last)/2;
    }
    
    return(-1);
}


//audiodata_target
void prep_cal_feature(int frame, unsigned char* audioname) {
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
    
    cal_feature(ptr, hz0);
}

//dp: do not allow jump when source is changing note. must use entire change of note



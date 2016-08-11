//
//  Resynthesis.h
//  Performance View
//
//  Created by Sanna Wager on 6/27/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#ifndef Resynthesis_h
#define Resynthesis_h

#include "yin.h"
#include "share.h"
#include "audio.h"
#include "global.h"


#include <stdio.h>

typedef struct {
    float hz;
    float amp;
    int frame;
    int nominal;
    int onset;
    //add other features?
} AUDIO_FEATURE;

typedef struct AMPLITUDE {
    double mu[128];
    //double musq[128];
    double sd[128];
    double var[128];
    //int num[128];
} AMPLITUDE;

typedef struct {
    AUDIO_FEATURE* el;
    int num;
    double mu;
    double sd;
    double var;
    AMPLITUDE amplitude;
} AUDIO_FEATURE_LIST;

void init_feature_list(AUDIO_FEATURE_LIST *a);

void prep_cal_feature(int frame, unsigned char* audioname);
void write_features(char *name, int onset_frames, int offset_frames);
void add_amplitude_elem(AUDIO_FEATURE_LIST *list, int nominal, float amp);
void cal_amplitude_dist(AUDIO_FEATURE_LIST *list, int frames);
#endif /* Resynthesis_h */


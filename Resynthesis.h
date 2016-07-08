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
    float nominal;
    //add other features?
} AUDIO_FEATURE;

typedef struct {
    AUDIO_FEATURE* el;
    int num;
} AUDIO_FEATURE_LIST;

void prep_cal_feature(int frame, unsigned char* audioname);
AUDIO_FEATURE cal_feature(unsigned char *ptr, float hz0);
void write_features(char *name);

#endif /* Resynthesis_h */


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
#include "vocoder.h"


#include <stdio.h>

AUDIO_FEATURE prep_cal_feature(int frame, unsigned char* audioname);
void test_swap_similar_frames();
void write_features(char *name);

#endif /* Resynthesis_h */


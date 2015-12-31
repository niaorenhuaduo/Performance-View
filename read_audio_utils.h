//
//  read_audio_utils.h
//  TestApp
//
//  Created by Christopher Raphael on 12/24/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#ifndef TestApp_read_audio_utils_h
#define TestApp_read_audio_utils_h
#define MAX_POS_STRING_LEN 20

typedef struct {
    char num[4]; // a 3-digit number
    char date[9];   // 12/12/12
    char start[MAX_POS_STRING_LEN];
    char end[MAX_POS_STRING_LEN];
    char correct[4];
    char train[4];
    char range[50];
} AUDIO_FILE_STRUCT;


#define MAX_AUDIO_FILE_LIST 1000

typedef struct {
    int num;
    AUDIO_FILE_STRUCT list[MAX_AUDIO_FILE_LIST];
} AUDIO_FILE_LIST;

void get_audio_file_data();
int num_audio_files();
char *last_audio_file_num();

extern AUDIO_FILE_LIST audio_files;

#endif

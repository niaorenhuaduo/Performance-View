//
//  score_utils.h
//  TestApp
//
//  Created by Christopher Raphael on 12/22/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#ifndef TestApp_score_utils_h
#define TestApp_score_utils_h

#define MAX_SCORES 250

typedef struct {
    char *name;
    char *title;
} NAME_TITLE;



typedef struct {
    int num;
    NAME_TITLE score[MAX_SCORES];
} SCORE_NAMES;



int fast_get_pieces();
int num_available_scores();
char* get_score_title(int i);
int check_for_mmo_data(char *scoretag, char *audio);
int check_for_voc_data();
int set_title(char *title);
int read_score_files();
void save_player_piece();
void set_audio_data_dir();
void scoretag2title(char *tag, char *title);
void scoretitle2tag2(char *title, char *tag);
void gui_after_read_score();



extern SCORE_NAMES pieces;


#endif

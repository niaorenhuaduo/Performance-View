//
//  score_utils.c
//  TestApp
//
//  Created by Christopher Raphael on 12/22/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#include <stdio.h>
#include <dirent.h>
#include <memory.h>
#include <stdlib.h>
#include "share.h"
#include "global.h"
#include "score_utils.h"

SCORE_NAMES pieces;


int
num_available_scores() {
    return(pieces.num);
}

char*
get_score_title(int i) {
    return(pieces.score[i].title);
}


static int
name_title_comp(const void *p1, const void *p2) {
    int c;
    NAME_TITLE *i,*j;
    
    i = (NAME_TITLE *) p1;
    j = (NAME_TITLE *) p2;
    return(strcmp(i->title,j->title));
}


int
check_for_mmo_data(char *scoretag, char *audio) {
  FILE *fp;
  int found;

  //  strcpy(audio,"scores/");
   strcpy(audio,user_dir);

  strcat(audio,SCORE_DIR);
  strcat(audio,"/");
  strcat(audio,scoretag);
  strcat(audio,"/");
  strcat(audio,scoretag);
  strcat(audio,".raw");  /* raw 48KHz sound */
  fp = fopen(audio,"rb");
  fclose(fp);
  found = (fp != 0);
  return(found);
}

int
check_for_voc_data() {
  FILE *fp;
  int found;
  char audio[500];

  //  strcpy(audio,"scores/");
  strcpy(audio,user_dir);
 strcat(audio,SCORE_DIR);
  strcat(audio,"/");
  strcat(audio,scoretag);
  strcat(audio,"/");
  strcat(audio,scoretag);
  strcat(audio,".voc");  /* spectrogram data */
  fp = fopen(audio,"rb");
  fclose(fp);
  found = (fp != 0);
  return(found);
}

int
set_title(char *title) {
  int j;

  for (j=0; j < pieces.num; j++) 
    if (strcmp(scoretag,pieces.score[j].name) == 0) break;
  if (j == pieces.num) { 
    printf("couldn't  match scoretag %s in set_title\n",scoretag); 
    return(0); 
  }
  strcpy(title,pieces.score[j].title);
  return(1);
}



void
gui_after_read_score() {
  char title[500],lab[500];
 // int j;

  if (set_title(title) == 0) exit(0);
  strcpy(lab,title);

  /*  Live->Live->Enabled = 0;
  Spect->Settings1->Enabled = 1;
  Live->LiveInfo->SimpleText = "";*/
 // init_range();
  /*  disable_spect_buttons();
      guisetup->Close(); */
  save_player_piece();

  //  Spect->Custom->Enabled = 1;
  //  Spect->SpectMenu->Items[1]->Enabled = 1;  // live perform
  //  Spect->SpectMenu->Items[2]->Enabled = 1;  // live perform


}




int
fast_get_pieces() {
    DIR *dir;
    char path[500],file[500],name[500],title[500],tag[500];
    struct dirent *drnt;
    FILE *fp;
    int i;
    
    pieces.num = 0;
    strcpy(path,user_dir);
    strcat(path,SCORE_DIR);
    strcat(path,"/");
    dir = opendir(path);
    while ((drnt=readdir(dir)) != NULL) {
        strcpy(name,path);
        strcpy(tag,drnt->d_name);
        strcat(name,tag);
        strcat(name,"/");
        strcat(name,tag);
        strcat(name,".nam");
        fp = fopen(name,"r");
        if (fp == NULL) continue;
        printf("found %s\n",name);
        for (i=0; i < 500; i++) if ((title[i] = fgetc(fp)) == '\n') break;
        title[i] = 0;
        pieces.score[pieces.num].title = (char *) malloc(strlen(title)+1);
        strcpy(pieces.score[pieces.num].title,title);
        pieces.score[pieces.num].name = (char *) malloc(strlen(tag)+1);
        strcpy(pieces.score[pieces.num].name,tag);
        fclose(fp);
        pieces.num++;
    }
    qsort(pieces.score,pieces.num,sizeof(NAME_TITLE),name_title_comp);
    if (pieces.num == 0) printf("no pieces found in score directory\n");
}


void
scoretitle2tag2(char *title, char *tag) {
  int i;

  tag[0] = 0;
  for (i=0; i < pieces.num; i++) if (strcmp(title,pieces.score[i].title) == 0) break;
  if (i == pieces.num) return;
  strcpy(tag,pieces.score[i].name);
}



void
scoretag2title(char *tag, char *title) {
  int i;

  title[0] = 0;
  for (i=0; i < pieces.num; i++) if (strcmp(tag,pieces.score[i].name) == 0) break;
  if (i == pieces.num) return;
  strcpy(title,pieces.score[i].title);
}





int
read_score_files() {
  char title[500],lab[500],score[500],audio[500],times[500],dir[500],tap[500];
  int vcode_exists, audio_exists,j,result;
  unsigned int key;


  init_live_state();
  fast_get_pieces();  /* this is called at initialization in the gui version so in
		    that case this call is redundant.  but need this for 
		    timewarp interface */
  if (set_title(score_title) == 0) return(0);

  /*  for (j=0; j < pieces.num; j++) 
    if (strcmp(scoretag,pieces.score[j].name) == 0) break;
  if (j == pieces.num) { printf("could find scoretag\n"); return(0); }
  strcpy(title,pieces.score[j].title); */

  set_audio_data_dir();
  if (midi_accomp == 0) {
    audio_exists =  check_for_mmo_data(scoretag,audio);
    vcode_exists =  check_for_voc_data();
    if (!audio_exists && !vcode_exists) { 
      printf("neither audio format present\n"); 
      return(0);
    }
    if (vcode_exists)  set_vcode_data_type(1);
    if (audio_exists)  set_vcode_data_type(0);  /* really must be one or othes */
  }


  //  strcpy(score,"scores/");
  strcpy(score,user_dir);
 strcat(score,SCORE_DIR);
  strcat(score,"/");
  strcat(score,scoretag);
  strcat(score,"/");
  strcat(score,scoretag);
  //  strcpy(lab,"Score: ");

  //  Patience *newthread = new Patience(false);
  //  Sleep(10000);

  printf("reading midi score %s\n",score);
  if (read_midi_score_input(score) == 0)  return(0);


  //  if (read_player_tempos() == 0) return(0);  /* override score tempos if these exist */  // these in .hnd now
 if (midi_accomp == 0) {
  strcpy(times,score);
  strcat(times,".times");

  printf("reading times %s\n",times);
  
  result =  read_orchestra_times_name(times);
  if (result == 0) return(0);
 }

#ifdef DISTRIBUTION
  //  if (verify_key(key) == 0) return(0);
#endif

  if (midi_accomp == 0) {
    if (vcode_exists && audio_exists == 0) {
      printf("reading vcode data\n");
      read_vcode_data();
    }
    else {
      strcpy(audio,score);
      strcat(audio,".raw");
      printf("reading 48Khz data %s\n",audio);
      if (read_48khz_raw_audio_name(audio) == 0) return(0);
    }
  }
  strcpy(tap,score);
  strcat(tap,".tap");
  read_mmo_taps_name(tap);



  //  Application->ProcessMessages();  // need this to make sure the window closes after this routine
  if (make_complete_bbn_graph_windows() == 0) return(0);
  // Application->ProcessMessages(); 

  return(1);


  
}


#define PLAYER_STATE "player_state.dat"

void
save_player_piece() {
  FILE *fp;
  char name[500];

  strcpy(name,  user_dir);
 //  strcat(name,AUDIO_DIR);
 // strcat(name, "/");
  strcat(name, PLAYER_STATE);
  fp = fopen(name,"w");
  printf("%s\n",name);
  fprintf(fp,"%s\n",player);
  fprintf(fp,"%s\n",scoretag);
  fprintf(fp,"%d/%d %d/%d\n",start_pos.num,start_pos.den,end_pos.num,end_pos.den);
  /*  fprintf(fp,"%f\n",get_orch_pitch());
  fprintf(fp,"%f\n",mixlev);
  fprintf(fp,"%f\n",pan_value);
  fprintf(fp,"%f\n",master_volume);*/
  fclose(fp);
}



void
set_audio_data_dir() {
  struct dirent *drnt;
  DIR *dir;

  strcpy(audio_data_dir,user_dir);
  strcat(audio_data_dir,AUDIO_DIR);
  strcat(audio_data_dir,"/");
  strcat(audio_data_dir,player);
  strcat(audio_data_dir,"/");
  strcat(audio_data_dir,scoretag);
  dir = opendir(audio_data_dir);
  //  if (DirectoryExists(audio_data_dir) == 0) CreateDir(audio_data_dir);
 // if (dir == NULL) mkdir(audio_data_dir,0777); // commented this out for apple compile
  strcat(audio_data_dir,"/");
}


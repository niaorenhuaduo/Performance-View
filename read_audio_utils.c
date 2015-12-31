//
//  read_audio_utils.c
//  TestApp
//
//  Created by Christopher Raphael on 12/24/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#include <stdio.h>
#include <dirent.h>
#include "share.h"
#include "global.h"
#include <sys/stat.h>
#include <time.h>
#include "read_audio_utils.h"
//#include "AppDelegate.h"



AUDIO_FILE_LIST audio_files;

void
make_range_string(char *lab) {
    char num[100];
    
    wholerat2string(start_pos,lab);
    strcat(lab," -- ");
    wholerat2string(end_pos,num);
    strcat(lab,num);
}


int
num_audio_files() {
    return(audio_files.num);
}

char *
last_audio_file_num() {
    return(audio_files.list[audio_files.num-1].num);
}

void
get_start_end(char *name, char *start, char *end, char *range) {
  char times[500],f1[500],f2[500],p1[500],p2[500];
  FILE *fp;
    int s,e,ss,ee;

  start[0] = end[0] = 0;
  strcpy(times,audio_data_dir);
  strcat(times,name);
  strcat(times,".times");
  fp = fopen(times,"r");
  if (fp == NULL) return;
  fscanf(fp,"%s %s %s",f1,f2,start);
  fscanf(fp,"%s %s %s",f1,f2,end);
  s = wholerat2measnum(string2wholerat(start));
    ss = score.measdex.measure[s].meas;  // taking into account repeats, get score meas num
  e = wholerat2measnum(string2wholerat(end));
    ee = score.measdex.measure[e].meas;
  sprintf(range,"m. %d - %d",ss,ee);
  fclose(fp);
}

int
count_instances(char *name, char *suff) {
 char examps[500],ex[500];
  FILE *fp;
  int ret=0;

  get_player_file_name(examps,suff);
  fp = fopen(examps,"r");
  if (fp == NULL) return(0);
  while(1) {
	//    fscanf(fp,"%[^\n]",ex);
	fscanf(fp,"%s",ex);
	if (feof(fp)) break;
	if (strcmp(ex,name) == 0) ret++;  //{ret = 1; break; }
  }
  fclose(fp);
  return(ret);
}

int
already_have_instance(char *name, char *suff) {
 char examps[500],ex[500];
  FILE *fp;
  int ret=0;

  get_player_file_name(examps,suff);
  fp = fopen(examps,"r");
  if (fp == NULL) return(0);
  while(1) {
	//    fscanf(fp,"%[^\n]",ex);
	fscanf(fp,"%s",ex);
	if (feof(fp)) break;
	if (strcmp(ex,name) == 0) {ret = 1; break; }
  }
  fclose(fp);
  return(ret);
}


void
get_audio_file_data() {
  char path[500],*name,full[500],time[500],*tm,*ptr,num[10],start[100],end[100],range[500];
  DIR *dir;
  struct dirent *de;
  struct stat filestat;
  struct tm *timeinfo;
  int n=0,count,i;
  AUDIO_FILE_STRUCT *afs;

  strcpy(path,user_dir);
  strcat(path,"/");
  strcat(path,AUDIO_DIR);
  strcat(path,"/");
  strcat(path,player);
  strcat(path,"/");
  strcat(path,scoretag);
  dir = opendir(path);
  if (dir == NULL) return;
  while ((de = readdir(dir)) != NULL) {
    name = de->d_name;
    if (strcmp(name+strlen(name)-3,"raw") != 0) continue;
    afs = audio_files.list + n;
    ptr = name + strlen(name) - 7;
    for (i=0; i < 3; i++) afs->num[i] = ptr[i];
    afs->num[4] = 0;
    //    printf("name = %s\n",name);
    strcpy(full,path); strcat(full,"/"); strcat(full,name);
    stat(full,&filestat);
    timeinfo = localtime(&(filestat.st_ctime));
    //    tm = ctime(&(filestat.st_ctime));
    strftime(time, 500, "%x", timeinfo);
    strcpy(afs->date,time);
    name[strlen(name)-4] = 0; // remove suffix
    get_start_end(name,start,end,range);
    strncpy(afs->start,start,MAX_AUDIO_FILE_LIST);
    strncpy(afs->end,end,MAX_AUDIO_FILE_LIST);
      strncpy(afs->range,range,MAX_AUDIO_FILE_LIST);
    afs->train[0] = afs->correct[0] = 0;
    count = count_instances(name,BBN_EX_SUFF);
    if (count) strcpy(afs->train, "yes");
  /*  if (count) strcpy(afs->train, "x");
    if (count > 1) { sprintf(num,"%d",count); strcat(afs->train,num); } */
    if (already_have_instance(name,DP_EX_SUFF)) strcpy(afs->correct,"yes");  
    n++;
  }
  audio_files.num = n;
}

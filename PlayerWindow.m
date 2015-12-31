//
//  PlayerWindow.m
//  TestApp
//
//  Created by Christopher Raphael on 12/21/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import "PlayerWindow.h"
#include "share.h"
#include "global.h"

#define PLAYER_FILE "player"
#define INSTRUMENT_FILE "instruments"


/*#define SHORT_STRING_MAX 25

typedef struct {
    char inst[SHORT_STRING_MAX];
    char person[SHORT_STRING_MAX];
} PLAYER_INFO;


#define MAX_PLAYERS 100

typedef struct {
    int num;
    PLAYER_INFO player[MAX_PLAYERS];
} PLAYER_LIST;

static PLAYER_LIST player_list;*/

int
read_player_list() {
  char file[500],line[500];
  FILE *fp;
  PLAYER_INFO *pi;
  int i,j,k,l;

  player_list.num = 0;
  //  strcpy(file,MISC_DIR);
  strcpy(file,user_dir);
 // strcat(file,AUDIO_DIR);
//  strcat(file,"/");
  strcat(file,PLAYER_FILE);
  fp = fopen(file,"r");
  //  printf("file = %s fp = %d\n",file,fp); exit(0);
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    return(0);
  }
  for (i=0; i < MAX_PLAYERS; i++) {
      pi = &player_list.player[i];
    //    fscanf(fp, "%[^\n]s",line);
    // for (j=0; j < 100; j++) if ((line[j] = fgetc(fp)) == '\n') break;
    fgets(line,100,fp);
    if (feof(fp)) break;
    l = strlen(line);
    for (j=0; j < l; j++) if (line[j] == ',') break;
    line[j] = 0;
    strcpy(pi->person,line);
    for (; j < l; j++) 
      if (('a' <= line[j]  && line[j] <= 'z')  ||
	 ('A' <= line[j]  && line[j] <= 'Z')) break;
    for (k=j; k < l; k++) if (line[k] == ',') break;
    line[k] = 0;
    strcpy(pi->inst,line+j);
    //    sscanf(line,"%[^,]s %[^,]s",pi->person,pi->inst);
    //    fscanf(fp, "%[^,]s %[^,\n]s",&(pi->person),&(pi->inst));
    //    printf("my strings[%d] are %s %s\n",i,pi->person,pi->inst);
     //    fscanf(fp, "%s %s",&(pi->person),&(pi->inst));
  }
  player_list.num = i;
  fclose(fp);
  return(1);
}

static int
write_player_list() {
  char file[100];
  FILE *fp;
  PLAYER_INFO *pi;
  int i;

  //  strcpy(file,MISC_DIR);
  strcpy(file,user_dir);
 // strcat(file,AUDIO_DIR);
//  strcat(file,"/");
  strcat(file,PLAYER_FILE);
  fp = fopen(file,"w");
  if (fp == 0) return(0);
  for (i=0; i < player_list.num; i++) {
    pi = &player_list.player[i];
    fprintf(fp, "%s, %s,\n",pi->person,pi->inst);
  }
  fclose(fp);
  return(1);
}

static int
player_el_comp(const void *n1, const void *n2) {
  PLAYER_INFO *p1,*p2;
  int c1,c2,r;

  p1 = (PLAYER_INFO *) n1;
  p2 = (PLAYER_INFO *) n2;
  return(strcmp(p1->person,p2->person));
}



static void
add_player_list(char *play, char *inst) {
  int i;

  if (player_list.num == MAX_PLAYERS) {
    printf("out of room in player list\n");
    return;
  }
  i = player_list.num++;
  strcpy(player_list.player[i].person,play);
  strcpy(player_list.player[i].inst,inst);
  qsort(player_list.player,player_list.num,sizeof(PLAYER_INFO),
	player_el_comp);  
}



static int
read_instrument_list() {
  FILE *fp;
  char file[500], inst[100];
  int i;
    
  strcpy(file,user_dir);
  strcat(file,INSTRUMENT_FILE);
  fp = fopen(file,"r");
 
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    return(0);
  }
  for (i=0; i < MAX_INSTRUMENTS; i++) {
    fscanf(fp,"%s\n",inst);
    
      instrument_list.instrument[i] = malloc(strlen(inst)+1);
      strcpy(instrument_list.instrument[i],inst);
      printf("%s\n",inst);
      if (feof(fp)) break;
  }
    instrument_list.num = i+1;
    
    fclose(fp);   
    
}

#define INST_CONV_NUM 14

static char* inst_conv[INST_CONV_NUM][2] = {
    {"strings", "strings"},
    {"winds","winds"},
    {"piano","piano"},
    {"voice", "voice"},
    {"guitar", "guitar"},
    {"Oboe", "winds"},
    {"Flute", "winds"},
    {"Clarinet", "winds"},
    {"Bassoon", "winds"},
    {"Violin", "strings"},
    {"Viola", "strings"},
    {"Cello", "strings"},
    {"Trumpet", "winds"},
    {"Horn", "winds"} };


static void
inst_cat_conversion(char *inst, char *cat) {
    int i;
    
    for (i=0; i < INST_CONV_NUM; i++) if (strcmp(inst_conv[i][0],inst) == 0)  {
        strcpy(cat,inst_conv[i][1]);
        return;
    }
    strcpy(cat,"strings");
}

int
read_training_data() {
    char dir[500],file[500],cat[500];
    int suc;
    
    /*#ifdef JAN_BERAN                                                               
     strcpy(instrument,"jan_beran");                                                
     #endif */
    strcpy(dir,user_dir);
    strcat(dir,WIN_TRAIN_DIR);
    strcat(dir,"/");
    //  strcat(dir,"oboe");  /* temporary */
    inst_cat_conversion(instrument,cat);
   // strcat(dir,instrument);
    strcat(dir,cat);
    
    strcat(dir,"/");
    
    strcpy(file,dir);
    strcat(file,QUANTFILE);
    suc = read_cdfs_file(file);
    if (suc) printf("read %s\n",file);
    else {
        printf("coulnd't read %s\n",file);
        return(0);
    }
    
    strcpy(file,dir);
    strcat(file,DISTRIBUTION_FILE);
    suc = read_distributions_file(file);
    if (suc) printf("read %s\n",file);
    else {
        printf("coulnd't read %s\n",file);
        return(0);
    }
    return(1);
}



#include <dirent.h>

#include <sys/stat.h>

int
set_up_after_player() {
    char dir[500],name[500],path[500];
    int i;
    struct dirent *de;
    DIR *dr;
    
    dr = opendir(user_dir);
    if (dr == NULL) mkdir(user_dir,0777);
 //   if (DirectoryExists(user_dir) == 0)  CreateDir(user_dir);
    strcpy(dir,user_dir);
    
    strcat(dir,AUDIO_DIR);
    dr = opendir(dir);
    if (dr == NULL) mkdir(dir,0777);
    strcat(dir,"/");
    strcat(dir,player);
    // printf("dir = %s\n",dir);  
    dr = opendir(dir);
    if (dr == NULL) mkdir(dir,0777);
   // if (DirectoryExists(dir) == 0)  CreateDir(dir);
    
    if (player_list.num > 0) {  // if no players then this must be the single user vserion.  instrument already set!
    
    for (i=0; i < player_list.num; i++) {
        //    printf("%d %s %s\n",i,player,player_list.player[i].person);            
        if (strcmp(player,player_list.player[i].person) == 0) break;
    }
    if (i == player_list.num) {
        NSLog(@"coulnd't match player %s\n",player);
        return(0);
    }
    strcpy(instrument,player_list.player[i].inst);
    }
    
    if (read_training_data() == 0) {
        NSLog(@"couldn't read_training_data");
      return(0);       
    }
   
    strcpy(path,MISC_DIR);
    strcat(path,"/background.dat");
    strcat(name,path);
    
    
 /*   path = MISC_DIR;    // since player no longer involved in path this could be read in at startup                                                                
    path = path + "/"  + "background.dat";
    strcpy(name,path.c_str()); */
    
    read_background_model(name);
    return(1);
}



@implementation PlayerWindow

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
        NSLog(@"init window");
    }
    
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    // Drawing code here.
}


- (void) awakeFromNib {
    read_player_list();
    read_instrument_list();
   // NSLog(@"Hello!");  // Not seeing my Hello.  :(
}

- (void) dealloc {
    [super dealloc];
}

@end

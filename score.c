#include "share.c"
#include "global.h"
#include "dp.h"
#include "joint_norm.h"
#include "new_score.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "gui.h"

extern PITCH *sol;   /* solfege array */
extern char scorename[];
extern int *notetime;

int pedal;   /* boolean for pedal on */
int line; /* current line number*/

EVENT_LIST pl;


name2num(n,a,o)
char n,a;
int o; {
  int i,hs;
  
  if (isrest(n)) return(RESTNUM);
       if ( n == 'c') hs = 0;
  else if ( n == 'd') hs = 2;
  else if ( n == 'e') hs = 4;
  else if ( n == 'f') hs = 5;
  else if ( n == 'g') hs = 7;
  else if ( n == 'a') hs = 9;
  else if ( n == 'b') hs = 11;
  if ((a == 's') || (a == '#')) hs += 1;
  else if ((a == 'f') || (a == 'b')) hs -= 1;
  for (i=0; i < o; i++) hs += 12;
  return(hs);
}

void
num2name(n,s)   /* note num to note name */
int n;
char *s; {
  int i=0,o,hs;
  
  if (n == RESTNUM) { s[i++] = '~'; s[i] = 0; return; }
  if (n == RHYTHMIC_RESTNUM) { s[i++] = '~'; s[i++] = '!'; s[i] = 0; return; }
  o = n/12; 
  hs = n - 12*o;
  if      (hs == 0 ) s[i++] = 'c';
  else if (hs == 1 ) { s[i++] = 'c'; s[i++] = 's'; }
  else if (hs == 2 ) s[i++] = 'd';
  else if (hs == 3 ) { s[i++] = 'e'; s[i++] = 'f'; }
  else if (hs == 4 ) s[i++] = 'e';
  else if (hs == 5 ) s[i++] = 'f';
  else if (hs == 6 ) { s[i++] = 'f'; s[i++] = 's'; }
  else if (hs == 7 ) s[i++] = 'g';
  else if (hs == 8 ) { s[i++] = 'a'; s[i++] = 'f'; }
  else if (hs == 9 ) s[i++] = 'a';
  else if (hs == 10) { s[i++] = 'b'; s[i++] = 'f'; }
  else if (hs == 11 ) s[i++] = 'b';
  s[i++] = '0' + (o-1);   // this is just the convetion where middle c is octave 4.
  s[i] = 0;
}


isnote(c)
char c; {
  return( ((c >= 'a') && (c <= 'g')) );  
}

isnum(c)
char c; {
  return( ((c >= '0') && (c <= '9')) );  
}

isacc(c)  /* is c an accidental */
char c; {
  return( (c == 's') || (c == 'f') || (c == '#') || (c == 'b') );  
}

isrest(c)  /* is c a rest */
char c; {
  return((c == '~'));  
}
islb(c)  /* is c a left brackey */
char c; {
  return((c == '['));  
}

isrb(c)  /* is c a right brackett */
char c; {
  return((c == ']'));  
}

isslash(c)  /* is c a right brackett */
char c; {
  return((c == '/'));  
}

isbar(c)  /* is c a right brackett */
char c; {
  return((c == '-'));  
}

isdb(c)  /* is c a double bar */
char c; {
  return((c == '|'));  
}

istie(c)  /* is c a tie */
char c; {
  return((c == 't'));  
}

iscue(s,i)  /* is c a cue */
char *s;
int *i; {
  if (strncmp("cue",s+(*i),3) == 0) { (*i) += 3; return(1); }
  return(0);
}

void
pedal_on(s,i)  /* is pedal on? */
char *s;
int *i; {
  if (strncmp("ped+",s+(*i),4) == 0) { (*i) += 4; pedal = 1; }
}

void
pedal_off(s,i)  /* is pedal on? */
char *s;
int *i; {
  if (strncmp("ped-",s+(*i),4) == 0) { (*i) += 4; pedal = 0; }
}

is_space(c)  /* is c a space */
char c; {
  return((c == ' '));  
}


float frac2meas(s,i)  /* insists s points to first number char in frac */
char *s;
int *i;  {
  int num=0,denom=0;
  float frac;

  while (isnum(s[*i])) { num*= 10; num += (s[(*i)++] - '0');  }
  if (isslash(s[*i])) {
    (*i)++;
    while (isnum(s[*i])) { denom*= 10; denom += (s[(*i)++] - '0');  }
    frac = (float)num/(float)denom;
    return(frac/score.measure);
  }
  return(-1);
} 

solf2num(s,i)  /* insists s points to first char in note name */
char *s;
int *i;  {
  char n,a=0;
  static int o=0;

  if (isrest(n=s[*i])) { (*i)++;  return(RESTNUM); }
  if (istie(n)) { (*i)++;  return(TIENUM); }
  if (isnote(n)) {
    (*i)++;
    if (isacc(s[*i])) {
      a = s[*i];
      (*i)++;
    }
    if (isnum(s[*i])) {
      o = s[*i] - '0'; 
      (*i)++;
    }
    if (is_space(s[*i])) {
      return(name2num(n,a,o));
    }
    else return(-1);
  }           
  return(-1);
}
 
void
skipspace(s,i) 
char *s;
int *i; {
  while (s[*i] == ' ') (*i)++;
}

void
mygetline(char *s,int *i,FILE *fp)  {
  *i = 0;
  while( (s[*i] = fgetc(fp)) != '\n')   (*i)++;
  s[*i] = 0; 
  *i = 0;
  line++;
}

int
readnote(s,i,n,b,c)
char *s;
int *i; 
int *n;
float *b;
int *c; {

  if (islb(s[*i])) (*i)++;
  else return(-1);
  *n = solf2num(s,i);
  if (*n == -1) return(-1);
  skipspace(s,i);
  *b = frac2meas(s,i);
  if (*b == -1) return(-1);
  skipspace(s,i);
  *c = iscue(s,i);
  skipspace(s,i);
  pedal_on(s,i);
  pedal_off(s,i);
  skipspace(s,i);
    if (isrb(s[*i])) { (*i)++; return(0); }
  else return(-1);
}


static int  timecomp(const void *p1, const void *p2) {
  MIDI_BURST *n1,*n2;

  n1 = (MIDI_BURST *) p1;
  n2 = (MIDI_BURST *) p2;
  if ( n1->time > n2->time) return(1);
  else return(-1);
}


void
printpart(p)
SOLO_PART *p; {
  int i;
  for (i=0; i < p->num; i++)  {
    printf("%d  notenum = %d start = %f length = %f\n",
      i,p->note[i].num,p->note[i].time,p->note[i].length);
    if (p->note[i].cue) printf("cue\n");
   }
  printf("\n");
}

void
printaccomp(p)
MIDI_PART *p; {
  int i;
  for (i=0; i < p->num; i++)  {
    printf("%d  notenum = %d start = %f length = %f\n",
      i,p->burst[i].num,p->burst[i].time,p->burst[i].length);
   }
  printf("\n");
}

void
printscore() {
  int i,j;

  printf("measure = %3.0f\n",score.measure);
  printf("solo part:\n");
  printpart(&(score.solo));
  printf("no printing accomp notes\n");
  /*  printf("accompaniment:\n");
  printaccomp(&(score.accomp));  */
} 

#define NOTE_ON   0x90
#define NOTE_OFF  0x80


mid_comp(const void *e1, const void *e2) {
  MIDI_EVENT *p1,*p2;

  p1 = (MIDI_EVENT *) e1;
  p2 = (MIDI_EVENT *) e2;
  if (p1->meas > p2->meas) return(1);
  if (p1->meas < p2->meas) return(-1);
  if (p1->tag != p2->tag) return(p1->tag - p2->tag);
  return(p1->command - p2->command);
}



char *mmalloc(n)
int n; {
  printf(" allocated %d\n",n);
  return((char *) malloc(n));
}



void
add_event(int tag, int c, int n, float t, int v, RATIONAL rat) {
  float r;
  r = (float) rat.num / (float) rat.den;
  if (fabs(r-t) > .001)
    exit(0);

  event.list[event.num].tag = tag;
  event.list[event.num].command = c;
  event.list[event.num].notenum = n;
  event.list[event.num].meas= t;
  event.list[event.num].volume= v;
  event.list[event.num].wholerat = rat;
  /*  printf("tag is %s\n",event.list[event.num].observable_tag);*/
  event.num++;
  /*  if (fabs(t-6.333) < .01 && n == 50) {
      printf("adding event %d %d %f\n",n,v,t); 
      }*/
      
  if (event.num == MAX_EVENTS) {
    printf("out of room in event list\n"); exit(0);   
  }
}


void
make_cue_list() {
    int i;
    char nn[20];
    JN init_start_state();

/*    cue.list = (CUE_LIST_EL *) mmalloc(event.num*sizeof(CUE_LIST_EL)); */
    cue.list = (PHRASE_LIST_EL *) malloc(MAX_PHRASES*sizeof(PHRASE_LIST_EL));
     cue.num = 0;
    for (i=0; i < score.solo.num; i++) {
	if (score.solo.note[i].cue) {
	    cue.list[cue.num].note_num = i;
	    cue.list[cue.num].trained = 0;
	    cue.list[cue.num].state_init = init_start_state(score.meastime);
	    cue.num++;
	    num2name(score.solo.note[i].num,nn);
/*	    printf("cueing note %d %s\n",i,nn); */
	}
    }
    /*    for (i=0; i < cue.num; i++) printf("cue %d at %d\n",i,cue.list[i].note_num); */
}


void
make_midi() {
  char  file[200],nn[10];
  int i,length,j,maxnotes; 
  float s;
  RATIONAL rat;
  
  strcpy(file,scorename);
  strcat(file,".midi");
  maxnotes = score.solo.num+score.midi.num;

/*  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].cue) 
      add_event(FERMATA,0,0,score.solo.note[i].time - .01,0,rat);
    else
      add_event(UPDATE,0,i,score.solo.note[i].time,0,rat);
  } */
  qsort(event.list,event.num,sizeof(MIDI_EVENT),mid_comp);
/* for (i=0; i < event.num; i++) printf("event = %d command = %d\nnum = %d\nvol = %d\n meas = %f\n\n",
i,      event.list[i].command,event.list[i].notenum,event.list[i].volume,event.list[i].meas);     */
  /*  make_cue_list(); */
}

void
make_midi_all() {    /* make an event list with both parts in it */
  char  file[200];
  int i,length,j; 

  
  strcpy(file,scorename);
  strcat(file,".midi");
  event.num = (score.midi.num+score.solo.num)*2;
  if (event.num >= MAX_EVENTS) {
    printf("not enough room in event.list\n");
    exit(0);
  }
/*  event.list = (MIDI_EVENT *) mmalloc(event.num*sizeof(MIDI_EVENT)); */
  j = 0;
  for (i=0; i < score.midi.num; i++) if (is_a_rest(score.midi.burst[i].num) == 0) {
    event.list[2*j].command = NOTE_ON;
    event.list[2*j].notenum = score.midi.burst[i].num; 
    event.list[2*j].meas = score.midi.burst[i].time; 
    event.list[2*j].volume = 50;
    event.list[2*j+1].command = NOTE_OFF;
    event.list[2*j+1].notenum = score.midi.burst[i].num; 
    event.list[2*j+1].meas = score.midi.burst[i].time + score.midi.burst[i].length; 
    event.list[2*j+1].volume = 0;
    j++;
  }
  for (i=0; i < score.solo.num; i++) if (is_a_rest(score.solo.note[i].num) == 0) {
    event.list[2*j].command = NOTE_ON;
    event.list[2*j].notenum = score.solo.note[i].num; 
    event.list[2*j].meas = score.solo.note[i].time; 
    event.list[2*j].volume = 50;
    event.list[2*j+1].command = NOTE_OFF;
    event.list[2*j+1].notenum = score.solo.note[i].num; 
    event.list[2*j+1].meas = score.solo.note[i].time + score.solo.note[i].length; 
    event.list[2*j+1].volume = 0;
    j++;
  }
  event.num = 2*j;
  qsort(event.list,event.num,sizeof(MIDI_EVENT),mid_comp);
/* for (i=0; i < 20; i++) printf("command = %d\nnum = %d\nvol = %d\n time = %f\n\n",
      event.list[i].command,event.list[i].notenum,event.list[i].volume,event.list[i].time);  */
}




old_readscore() {
  FILE  *fp;
  float tempo,curtime=0,bar,len,curbar;
  char  file[200],s[1000],acc,name,slash;
  int i,cur=0,oct=0,octave,numer,denom,nn,test,issolo,cue,p;
  SOLO_NOTE *sp;
  MIDI_BURST *ap;
  JN init_update();
  RATIONAL rat;

/*  event.list = (MIDI_EVENT *) mmalloc(MAX_EVENTS*sizeof(MIDI_EVENT)); */
  event.num = line =   pedal =   pl.num = 0;
/*  pl.list = (MIDI_EVENT *) mmalloc(MAX_EVENTS*sizeof(MIDI_EVENT)); */
/*  score.solo.note = (SOLO_NOTE *) 
    malloc(MAX_SOLO_NOTES*sizeof(SOLO_NOTE));
  score.midi.burst = (MIDI_BURST *)
    malloc(MAX_MIDI_BURSTS*sizeof(MIDI_BURST)); */
  strcpy(file,scorename);
  strcat(file,".score");
  fp = fopen(file,"r");
  if ( fp == NULL)
  {
     printf("couldn't find %s\n",file);
     return(0);
  }
  mygetline(s,&i,fp);
  fscanf(fp,"meastime = %f\n",&score.meastime);
  fscanf(fp,"measure = %s\n",s);
  line += 2;
  score.measure = 1;  /* a little kludge for next statement */
  score.measure = frac2meas(s,&i);
/*  printf("string = %s\n",s);
  printf("meastime = %f meas = %f\n",score.meastime,score.measure);  */
  bar = -1 /*-score.measure */ ;
  score.solo.num = score.midi.num = 0;
  while (!(feof(fp))) {  /* for each line (one measure long) */
    curbar = 0;
    mygetline(s,&i,fp);
    if (isdb(s[0])) break;  /* double bar */
    if ( isbar(s[0]) ) { bar += 1; issolo = 1; }
    else {
      do {
        skipspace(s,&i);
        test = readnote(s,&i,&nn,&len,&cue);
        if (test == -1) { 
          printf("line %d: %s\n",line,s+i);
          return(0);
        }
        else if (nn == TIENUM){
           score.solo.note[score.solo.num-1].length += len;
           curbar += len;
	 }
        else {
          if (issolo) { 
            sp = &(score.solo.note[score.solo.num++]);
	    if (is_a_rest(nn) == 0)
	    if (nn < LOWEST_NOTE || nn > HIGHEST_NOTE) {
		printf("illegal pitch %d \n",nn);
		return(0);
	    }
            sp->num = nn;
            sp->time = bar + curbar;
            curbar += len;
            sp->length = len;
            sp->cue = cue;
          }
          else {
            ap = &(score.midi.burst[score.midi.num++]);
            ap->num = nn;
            ap->time = bar + curbar;
            curbar += len;
            ap->length = len;
            add_event(MIDI_COM,NOTE_ON,nn,ap->time,50,rat);
            if (pedal == 0) {
              add_event(MIDI_COM,NOTE_OFF,nn,ap->time+len,0,rat);
              for (p=0; p < pl.num; p++) 
                add_event(MIDI_COM,NOTE_OFF,pl.list[p].notenum,ap->time+len,0,rat);
              pl.num = 0;
            }
            else {
              pl.list[pl.num++].notenum = nn;
            }
          }
        }
        skipspace(s,&i);
      } while (islb(s[i]));
      if (issolo && fabs(1-curbar) > .01) {
	printf("problem in %s at line %d\nmeasure doesn't add up: %s ( %f)\n",file,line,s,curbar);
	exit(0);
      }
    issolo = 0;
    }
  }
  fclose(fp);
  qsort(&(score.midi.burst[0]),score.midi.num,sizeof(MIDI_BURST),timecomp);
  initstats(); 
  for (i=0; i < score.solo.num; i++) {  /* maybe none of this loop used? */
    score.solo.note[i].count  = 0;
    score.solo.note[i].size = score.meastime;
    /*    score.solo.note[i].meas_size = score.meastime; */
    if (i > 0)
      score.solo.note[i].update = init_update(score.solo.note[i-1].length);
    else 
      score.solo.note[i].update = init_update(0.);
  }
}


readscore() {

  /*  old_readscore(); */
  new_readscore();             //CF:  ***
  /*      printscore(); */
  initscore(); 
  initstats(); /* resets the means + stds set in initscore */
  make_midi();
/*  set_cues(); */
/*  printscore();  */
  firstnote = 0; lastnote = score.solo.num-1;   /* default */
  init_updates();
  read_examples();
  estimate_length_stats();  
  read_updates();
  /*   bel_read_updates(); */
  return(1);
}

void
select_nick_score() {
  char name[200],i;

  printf("enter the nick score file (no suffix): ");
  scanf("%s",name);
  strcpy(full_score_name,name);
  for (i = strlen(name); i >= 0; i--) if (name[i] == '/') break;
  if (i > 0) i++; 
    strcpy(scorename,name+i);
  read_nick_score(name);
  initscore(); 
  initstats(); /* resets the means + stds set in initscore */
  make_midi();
  firstnote = 0; lastnote = score.solo.num-1;   /* default */
  init_updates();
  read_examples();
  estimate_length_stats();  
  read_updates();
}

void
select_nick_score_var(char *name) {
  char i;

  for (i = strlen(name); i >= 0; i--) if (name[i] == '/') break;
  if (i > 0) i++; 
    strcpy(scorename,name+i);
  read_nick_score(name);
  initscore(); 
  initstats(); /* resets the means + stds set in initscore */
  make_midi();
  firstnote = 0; lastnote = score.solo.num-1;   /* default */
  init_updates();
  read_examples();
  estimate_length_stats();  
  read_updates();
}




set_cues() {
  int i,a=0;
  float s;

  for (i=1; i < score.solo.num; i++) {
    s = score.solo.note[i-1].length*score.meastime;
    score.solo.note[i].cue = (s > 1);
    if (s > 1) {
      while (event.list[a+1].meas < score.solo.note[i].time) a++;
      event.list[a].command = FERMATA;
    }
  }
}


readbinscore() {
  char binname[200];
  FILE *fp;

  strcpy(binname,scorename);
  strcat(binname,".bin");
  fp = fopen(binname,"rb");
  if (fp == NULL)  return(0);
  fread(&score,sizeof(SCORE),1,fp);
  fclose(fp);
  return(1);
}


writebinscore() {
  char binname[200],xxx[200];
  FILE *fp;

  strcpy(binname,scorename);
  strcat(binname,".bin");
  fp = fopen(binname,"wb");
  fwrite(&score,sizeof(SCORE),1,fp);
  fclose(fp);
}

#define IRAT   .5   /*.25   */
#define MAXSTD 30. /*10.  /* this is in tokens */
#define MINSTD 2.

float mean2std(m)
float m; {
  float s;

  s = IRAT*m;
  if (s < MINSTD) return(MINSTD);
  if (s > MAXSTD) return(MAXSTD);
  return(s);
}


updatescore() {  /* updates score parms based on .nt file */
  int i,j,t;
  SOLO_NOTE *n;
  
  printf("updating score\n");
  readbinscore();
/*   readbeat(); */
  for (i=firstnote; i < lastnote-1; i++)  {
    n = score.solo.note+i;
/*    n->mean = (n->count*n->mean) + (notetime[i+1] - notetime[i]);
    (n->count)++;
    n->mean /= n->count; */
/*      n->mean = (2./3.)*(n->mean) + (1./3.)*(notetime[i+1] - notetime[i]); */
      n->mean = (1./3.)*(n->mean) + (2./3.)*(notetime[i+1] - notetime[i]);
      n->std =  mean2std(n->mean);   /* a kludge guess */
    n->count++;
    if (n->click == 1) {
      j = i+1;
      while ( score.solo.note[j].click == 0  && j < lastnote) j++;
      if (j==lastnote) break;
      t = notetime[j] - notetime[i];
/*      n->clickmean = (1./2.)*(n->clickmean) + (1./2.)*t; */
      n->clickmean = (1./3.)*(n->clickmean) + (2./3.)*t;
      n->size = (TOKENLEN*n->clickmean/SR)/n->clicklen;
    }
/*    printf("i = %d mean = %f count = %d\n",i,n->mean,n->count);  */
  }     
  writebinscore();
}




/*main() {
 readscore();
 printscore(); 
/*} */




write_trained_score() {
  char binname[200],xxx[200];
  FILE *fp;
  int i;

  strcpy(binname,scorename);
  strcat(binname,".bin");
  fp = fopen(binname,"wb");
  fwrite(&score,sizeof(SCORE),1,fp);
  fwrite(score.solo.note,sizeof(SOLO_NOTE),score.solo.num,fp);
  fwrite(score.midi.burst,sizeof(MIDI_BURST),score.midi.num,fp);
 /* for (i=0; i < 10; i++) printf("examp = %d\n",score.solo.note[i].examp.list[4]);
  for (i=0; i < 10; i++) mp(score.solo.note[i].update.mean); */
  fclose(fp);
}

read_trained_score() {
  char binname[200];
  FILE *fp;
  int i;

  strcpy(binname,scorename);
  strcat(binname,".bin");
  fp = fopen(binname,"rb");
  if (fp == NULL)  return(0);
  fread(&score,sizeof(SCORE),1,fp);
printf("numnotes = %d %d\n",score.solo.num,score.midi.num);
/*  score.solo.note = (SOLO_NOTE *) 
    malloc(score.solo.num*sizeof(SOLO_NOTE));  */
  fread(score.solo.note,sizeof(SOLO_NOTE),score.solo.num,fp);
/*  score.midi.burst = (MIDI_BURST *) 
    malloc(score.midi.num*sizeof(MIDI_BURST)); */
  fread(score.midi.burst,sizeof(MIDI_BURST),score.midi.num,fp);
  fclose(fp);
  for (i=0; i < 10; i++) printf("examp = %d\n",score.solo.note[i].examp.list[4]);
  for (i=0; i < score.solo.num; i++) mp(score.solo.note[i].update.mean);
  return(1);
}


#ifdef GAMMA_EXPERIMENT
  #define STD_HAT_RAT .3
#else
  #define STD_HAT_RAT .25 //.3 /*  .75  /* .3 /*.1 /* init est of std = this*mean */ 
#endif



/* results with no training very sensitive to above constant.  can't find
   a good choice and suspect the symmetric normal assumption for note
   length is bad ... */


#define INIT_COUNT  .3  /*1  /* as if this many samples were observed 
			 appropriate mean + var samples already 
			 observed */
#define MAX_COUNT_LIM  10  /* only allow this many (adjusted) training
			      examples to avoid model getting too definite */


estimate_length_stats() {
  int n,i,start,end;
  char name[500];
  float m,ss,x,l,mu,meas2tokens(),sz,count,st,a,b;


  for (i=0; i < score.solo.num; i++) {
    count = INIT_COUNT;
    l  =  score.solo.note[i].length;    //CF:  in whole notes
    //    sz  =  score.solo.note[i].meas_size;  //CF:  tempo in secs/whole note
    sz  =  get_whole_secs(score.solo.note[i].wholerat);
    //        printf("%s\tl = %f\tsz = %f\n",score.solo.note[i].observable_tag,l,sz);
    /*  mu =   meas2tokens(l); */
    mu = secs2tokens(l*sz);    //CF:  expected length in secs
    
	//		printf("mu = %f l = %f sz = %f\n",mu,l,sz);
    //CF:  accumulators -- init with pseudocount priors
    m =   INIT_COUNT*mu;
    ss =  (STD_HAT_RAT*STD_HAT_RAT+1)*mu*mu*INIT_COUNT;   //CF:  squares of observables -- assume Stdev lin realted to mean
                                                          //CF:  assume:  sigma=STD_HAT_RAT*mu
                                                          //CF:  (This then leads the equation here for ss=\sum x^2 )
                                                          //CF:  E[x^2] = (STD_HAT_RAT^2 + 1)*mu^2

    //        printf("i = %d ss = %f mu = %f const = %f %f num = %d\n",i,ss,mu,STD_HAT_RAT,INIT_COUNT,score.example.num);
    //    for (n=0; n < score.example.num; n++) { 
    for (n =  score.example.num -1; n >= 0; n--) { /* recent examples first*/
      start = score.example.list[n].start;  
      end   = score.example.list[n].end;
      if ((count < MAX_COUNT_LIM) && (i >= start) && (i < end)) {
	a = score.example.list[n].time[i+1-start];
	b = score.example.list[n].time[i-start];
	x = a-b;
	if (a == -1 || b == -1 || x < 0) continue;
	/*	x = score.example.list[n].time[i+1-start];
		x -= score.example.list[n].time[i-start];*/
	//	if (i == 200) printf("added count in example %d x = %f\n",n,x);
	count = count+1;
	
 //	if (x <= 0) printf("bad length at %s in estimate_length_stats()\n",score.solo.note[i].observable_tag);
	//		 printf("note = %d len = %f\n",i,x);
	m += x;
	ss += x*x;
      }
    }
     m /= count;
    if (m < 0) { printf("note = %s m = %f (ouch)\n",score.solo.note[i].observable_tag,m); exit(0); }
    //    if (i == 200) printf("in estimate_length_stats for %s : m = %f ss = %f count = %f\n",score.solo.note[i].observable_tag,m,ss,count);

    //       printf("---ss = %f count = %f m = %f\n",ss,count,m);
    //    printf("%f\n",ss);


    // ss = (ss/count) - m*m;
    /* these statement should be identical to above but they
       keep program from getting -NaN on ss in Borland */
    ss /= count;  
    ss -= m*m;

 
  if (ss < 0) {   printf("i = %d ss = %f count = %f m = %f\n",i,ss,count,m); exit(0);}

  score.solo.note[i].mean = m;
    score.solo.note[i].std = sqrt(ss); 
    num2name(score.solo.note[i].num,name);
    //      printf("%d\t%s\tm = %f\tstd = %f\tcount = %f %f %f \n",i,name,m,sqrt(ss),count,score.solo.note[i].mean,score.solo.note[i].std);       
  }
}








int
read_parse_example(EXAMPLE *ex, char *aname) {
  char file[300],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,j,k,notenum,type;
  float time;
  char start_string[100];
  char end_string[100];
  RATIONAL r;

#ifdef WINDOWS
  strcpy(file,audio_data_dir);  
  //  strcat(file,current_examp);   
  strcat(file,aname);   
  strcat(file,".times");
#else
  get_parse_name(file);
#endif


  type = get_parse_type(file);


  fp = fopen(file,"r");
  printf("opening %s\n",file); 
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    return(0);   
  }
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    return(0);
  }
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    return(0);
  }
  if (feof(fp)) {
    fclose(fp);
    return(0);  /* parse file doesn't have times */
  }
  fscanf(fp,"firstnote = %d\n",&(ex->start)); // meaningless if score has changed
  fscanf(fp,"lastnote = %d\n",&(ex->end));


  notenum = ex->end + 1 - ex->start;


  ex->time = (float *) malloc(sizeof(float)*notenum);  // substituted this 11-06
  /*  if (notenum > MAX_EXAMP_LENGTH) {
    printf("example has %d notes --- too big with MAX_EXAMP_LENGTH = %d\n",notenum,MAX_EXAMP_LENGTH);
    printf("might want to allocate examples of necessary size\n");
    exit(0);
    }*/


  /*  this is start of more forgiving way to read in times if score changed  
      k=0; ex->start = -1;
  while(1){
    if (type == 2) fscanf(fp,"%s %f %d",tag,&time,&j); //use this to read new style
    else fscanf(fp,"%s %f",tag,&time);
    if (feof(fp)) break;
    r = string2wholerat(tag+5);  // skipping "solo_"
    for (; k++; k < score.solo.num) { // as long as "new" score note k
      if (rat_cmp(score.solo.note[k].wholerat,r) >= 0) break;
      if (ex->start != -1) ex->time[k-ex->start] = 0;
    }
    if (k == 0 && ex->start == -1) { printf("couldn't find first note %s \n",tag); exit(0); }
    if (k == score.solo.num) break;
    if (ex->start == -1) ex->start = k;  // initialize the start note
    ex->time[k-ex->start] = time;
    
    
  }
  */


  for (i=ex->start; i <= ex->end; i++) {
    if (type == 2) fscanf(fp,"%s %f %d",tag,&time,&j); //use this to read new style
    else fscanf(fp,"%s %f",tag,&time);
    //    fscanf(fp,"%s %f",tag,&time);
    if (strcmp(tag,score.solo.note[i].observable_tag) != 0) {
      printf("%s != %s\n",tag,score.solo.note[i].observable_tag);
      printf("score inconsistent with parse file (1) %s --- parse again\n",file);
      fclose(fp);
      return(0);
    }
    ex->time[i-ex->start] = time;

    /*    score.solo.note[i].realize = time;  5-05 change
    score.solo.note[i].frames = (int) (time + .5);
    score.solo.note[i].off_line_secs = toks2secs(time); */
  }
  /*  printf("parse file okay\n");*/
  fclose(fp); 
  return(1);
}



static int
is_solo_string(char *s) {
  char check[1000];

  strcpy(check,s);
  check[5] = 0;
  return((strcmp(check,"solo_") == 0));
}

static int
find_solo_index_range(char *file,int *first_solo,int *last_solo) {
  /* returns the solo index range for the .tiems file.  This work even when the score has changed. 
     In this routine we are ignoring the index range that is in the file itself */
  FILE *fp;
  int i,marked;
  float time;
  char line[1000],c,tag[1000],check[1000],last[1000];
  RATIONAL r;
  
  fp = fopen(file,"r");
  if (fp == NULL) return(0);
  for (i=0; i < 4; i++) { fscanf(fp,"%[^\n]",line); fscanf(fp,"%c",&c); }  // skip first 4 lines
  fscanf(fp,"%s %f %d",tag,&time,&marked); 
  r = string2wholerat(tag+5);  // skipping "solo_"
  if (r.den == 0) return(0); // invalid file or no solo notes
  *first_solo = 0;
  while (rat_cmp(score.solo.note[*first_solo].wholerat,r) < 0) (*first_solo)++;
  while (1) {
    fscanf(fp,"%s %f %d\n",tag,&time,&marked);
    if (feof(fp)) break;
    strcpy(check,tag);
    check[5] = 0;
    if (strcmp(check,"solo_") != 0) break;
    strcpy(last,tag);
    //    fscanf(fp,"%[^\n]",line); 
  }
  r = string2wholerat(last+5);  // skipping "solo_"
  if (r.den == 0) return(0);
  *last_solo = score.solo.num-1;
  while (rat_cmp(score.solo.note[*last_solo].wholerat,r) > 0) (*last_solo)--;
  fclose(fp);
  return(1);
}



int
new_read_parse_example(EXAMPLE *ex, char *aname) {
  char file[300],name[500],alt[500],tag[500],*nm;
  FILE *fp;
  int i,n,j,k,kk,notenum,type,first_solo,last_solo,ret,blah1,blah2;
  float time;
  char start_string[100];
  char end_string[100];
  RATIONAL r;

#ifdef WINDOWS
  strcpy(file,audio_data_dir);  
  //  strcat(file,current_examp);   
  strcat(file,aname);   
  strcat(file,".times");
#else
  get_parse_name(file);
#endif

  if (find_solo_index_range(file,&first_solo,&last_solo) == 0) return(0);


  //if (ret) printf("ret = %d first_solo = %s last_solo = %s\n",ret,score.solo.note[first_solo].observable_tag,score.solo.note[last_solo].observable_tag);



  fp = fopen(file,"r");
  printf("opening %s\n",file); 
  if (fp == NULL) {
    printf("couldn't find %s\n",file);
    return(0);   
  }
  fscanf(fp,"%s = %s\n",name,start_string);
  if (strcmp(name,"start_pos") != 0) {
    printf("unknown format in %s (name = %s)\n",file,name);
    return(0);
  }
  fscanf(fp,"%s = %s\n",name,end_string);
  if (strcmp(name,"end_pos") != 0) {
    printf("unknown format in %s\n",file);
    return(0);
  }
  if (feof(fp)) {
    fclose(fp);
    return(0);  /* parse file doesn't have times */
  }
  
  fscanf(fp,"firstnote = %d\n",&blah1); // meaningless if score has changed
  fscanf(fp,"lastnote = %d\n",&blah2);
  ex->start = first_solo;  // these are now right if score has changed
  ex->end = last_solo;
  notenum = ex->end + 1 - ex->start;
  ex->time = (float *) malloc(sizeof(float)*notenum);  // substituted this 11-06

  /*  this is a more forgiving way to read in times if score changed.  set to -1 times in 
      current score that weren't found in example   */
  for (k=ex->start; k <= ex->end; k++) ex->time[k-ex->start] = -1; // unset
  i = ex->start;
  while (1) {
    fscanf(fp,"%s %f %d",tag,&time,&j); 
    if (is_solo_string(tag) == 0 || feof(fp)) break;
    r = string2wholerat(tag+5);  // skipping "solo_"
    for (; i <= ex->end; i++) if (rat_cmp(score.solo.note[i].wholerat,r) >= 0) break;
    if (i > ex->end) break;
    if (rat_cmp(score.solo.note[i].wholerat,r) == 0) ex->time[i-ex->start] = time;
    


    /*    for (k = ex->start; k <= ex->end;  k++) { // if this is too slow can do binary search
      if (rat_cmp(score.solo.note[k].wholerat,r) == 0) {
	ex->time[k-ex->start] = time;
	break;
      }
      }*/
  }
  //  for (k = ex->start; k <= ex->end;  k++) printf("left with %s %f\n",score.solo.note[k].observable_tag,ex->time[k-ex->start]);
  fclose(fp); 
  return(1);
}







static void
new_read_examp() {
  int n,i,start,end,count,notenum,fn,ln;
  FILE *fp;
  char name[200];
  float m,ss,x,len;
  

  n = score.example.num = 0;

#ifdef WINDOWS
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
#else
  strcpy(name,score_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
#endif
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't find %s\n",name);
    //    exit(0);
    return;
  }
  else printf("reading %s\n",name);
  while(1) {
    fscanf(fp,"%s",audio_file);
    if (feof(fp)) break;
    //       printf("audio file is %s\n",audio_file);
    //    strcpy(current_examp,audio_file);   /* audio_file not used in windows */
    printf("tag = %s\n",audio_file);
    //    simple_read_parse(&fn,&ln);
    //    read_parse_example(score.example.list + n, current_examp);

    //    if (new_read_parse_example(score.example.list + n, audio_file) == 0) continue; // just experimenting

    if (read_parse_example(score.example.list + n, audio_file) == 0) continue;

    //        printf("firstnote = %d lastnote = %d\n",firstnote,lastnote);
    /*    notenum = ln + 1 - fn;
    if (notenum > MAX_EXAMP_LENGTH) {
      printf("example has %d notes --- too big with MAX_EXAMP_LENGTH = %d\n",notenum,MAX_EXAMP_LENGTH);
      printf("might want to allocate examples of necessary size\n");
      exit(0);
      }*/
    fn =     score.example.list[n].start;
    ln = score.example.list[n].end;
    for (i=fn; i <= ln; i++) {
      //      score.example.list[n].time[i-fn] =
      //	secs2tokens(score.solo.note[i].off_line_secs);
      //      if (fabs(score.example.list[n].time[i-fn] - secs2tokens(score.solo.note[i].off_line_secs)) > .1) {
      //	printf("baddd mojo here\n"); exit(0); }
      //      if (i == firstnote) continue; 
      if (i == fn) continue; 
      //      len = score.solo.note[i].off_line_secs - score.solo.note[i-1].off_line_secs;
      len = score.example.list[n].time[i-fn] - score.example.list[n].time[i-1-fn];
      if (len <= 0) { 
	printf("length = %f in %s at %s --- skipping training file\n",len,audio_file,score.solo.note[i].observable_tag); 
	break;
	//exit(0); 
      }
    }
    if (i > ln) n++;
    //    n++;
  }
  score.example.num = n;
  //  printf("read %d exs\n",score.example.num); exit(0);  
    fclose(fp);
}


static void
new_new_read_examp() {
  int n,i,start,end,count,notenum,fn,ln;
  FILE *fp;
  char name[400];
  float m,ss,x,len;
  

  n = score.example.num = 0;

#ifdef WINDOWS
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
#else
  strcpy(name,score_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
#endif
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't find %s\n",name);
    //    exit(0);
    return;
  }
  else printf("reading %s\n",name);
  while(1) {
    fscanf(fp,"%s",audio_file);
    if (feof(fp)) break;
    printf("tag = %s\n",audio_file);
    if (new_read_parse_example(score.example.list + n, audio_file) == 0) continue; // just experimenting
    n++;
  }
  score.example.num = n;
  fclose(fp);
}


static void
old_new_read_examp() {
  int n,i,start,end,count,notenum,fn,ln;
  FILE *fp;
  char name[200];
  float m,ss,x,len;
  

  n = score.example.num = 0;
  /*  strcpy(name,scorename);
      strcat(name,".ex");*/

#ifdef WINDOWS
  strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
#else
  strcpy(name,score_dir);
  strcat(name,scoretag);
  strcat(name,".ex");
#endif
  /*    strcpy(name,audio_data_dir);
  strcat(name,scoretag);
  strcat(name,".ex");


  printf("name = %s\n",name); exit(0); */


  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't find %s\n",name);
    //    exit(0);
    return;
  }
  else printf("reading %s\n",name);
  while(1) {
    fscanf(fp,"%s",audio_file);
    if (feof(fp)) break;
    //       printf("audio file is %s\n",audio_file);
    //    strcpy(current_examp,audio_file);   /* audio_file not used in windows */
    printf("tag = %s\n",audio_file);
    //    read_parse();
    simple_read_parse(&fn,&ln);
    //    read_parse_example(score.example.list + n, current_examp);
    if (read_parse_example(score.example.list + n, audio_file) == 0) continue;

    //        printf("firstnote = %d lastnote = %d\n",firstnote,lastnote);
    //    notenum = lastnote + 1 - firstnote;
    notenum = ln + 1 - fn;
    if (notenum > MAX_EXAMP_LENGTH) {
      printf("example has %d notes --- too big with MAX_EXAMP_LENGTH = %d\n",notenum,MAX_EXAMP_LENGTH);
      printf("might want to allocate examples of necessary size\n");
      exit(0);
    }
    //    score.example.list[n].start = firstnote;
    //    score.example.list[n].end = lastnote;
    //    for (i=firstnote; i <= lastnote; i++) {
    //      score.example.list[n].time[i-firstnote] =
    score.example.list[n].start = fn;
    score.example.list[n].end = ln;
    for (i=fn; i <= ln; i++) {
      score.example.list[n].time[i-fn] =
	secs2tokens(score.solo.note[i].off_line_secs);
      if (fabs(score.example.list[n].time[i-fn] - secs2tokens(score.solo.note[i].off_line_secs)) > .1) {
	printf("baddd mojo here\n"); exit(0); }
      //      if (i == firstnote) continue; 
      if (i == fn) continue; 
      len = score.solo.note[i].off_line_secs - score.solo.note[i-1].off_line_secs;
      if (len < 0) { printf("length = %f in %s at %s\n",len,audio_file,score.solo.note[i].observable_tag); exit(0); }
    }
    n++;
  }
  score.example.num = n;
  //  printf("read %d exs\n",score.example.num); exit(0);  
    fclose(fp);
}

int
read_examples() {
    int n,i,start,end,count;
    FILE *fp;
    char name[500];
    float m,ss,x;


    //       new_read_examp();
    new_new_read_examp();  
    return(0);
    /* this change allows (I think) one to alter the score and still have past training 
       examples function okay.*/






    strcpy(name,scorename);
    strcat(name,".examp");
    fp = fopen(name,"r");
    if (fp == NULL) {

      printf("couldn't find examples\n");
      score.example.num = 0;
      return(0);
    }
    fscanf(fp,"example num = %d\n",&score.example.num);
    for (n=0; n < score.example.num; n++) {
	fscanf(fp,"from = %d to = %d\n",&start,&end);
/*	printf("start = %d end = %d\n",start,end); */
	score.example.list[n].start = start;
	score.example.list[n].end = end;
	for (i=0; i < 1+end-start; i++) {
	    fscanf(fp,"%f\n",score.example.list[n].time+i);
/*	    printf("%f ",score.example.list[n].time[i]); */
	}
/*	printf("\n");*/
    }
    fclose(fp);
/*score.example.num = 0;
printf("zeroing examples\n"); */
  }


write_examples() {
    int n,i,start,end;
    FILE *fp;
    char name[500];


    strcpy(name,scorename);
    strcat(name,".examp");
    fp = fopen(name,"w");
    fprintf(fp,"example num = %d\n",score.example.num);
    for (n=0; n < score.example.num; n++) {
	start = score.example.list[n].start;
	end = score.example.list[n].end;
	fprintf(fp,"from = %d to = %d\n",start,end);
	for (i=0; i < 1+end-start; i++) 
	    fprintf(fp,"%f\n",score.example.list[n].time[i]);
	fprintf(fp,"\n");
    }
    fclose(fp);
}


write_updates() {
    int n,q;
    FILE *fp;
    char name[500];

    printf("writing updates\n");
    strcpy(name,scorename);
    strcat(name,".update");
    fp = fopen(name,"w");
    /*    printf("var 46 = \n");
	  mp(score.solo.note[46].update.var); */
    for (n=0; n < cue.num; n++) {
	q = cue.list[n].note_num;
	score.solo.note[q].update = cue.list[n].state_init;
	fprintf(fp,"cue = %d\n",n);
	fprintf(fp,"mean = \n");
	mp_file(cue.list[n].state_init.mean,fp);
	fprintf(fp,"var = \n");
	mp_file(cue.list[n].state_init.var,fp);
    }
    for (n=0; n < score.solo.num; n++) {
      /*	fprintf(fp,"note = %d\n",n); */
	fprintf(fp,"note = %s\n",score.solo.note[n].observable_tag);
	fprintf(fp,"mean = \n");
	mp_file(score.solo.note[n].update.mean,fp);
	fprintf(fp,"var = \n");
	mp_file(score.solo.note[n].update.var,fp);
    }
    fclose(fp);
}


write_solo_training() {
    int n,q;
    FILE *fp;
    char name[500];

    printf("writing solo training\n");
    strcpy(name,scorename);
    strcat(name,".solo");
    fp = fopen(name,"w");
    for (n=0; n < score.solo.num; n++) {
	fprintf(fp,"note = %d\n",n);
	QFprint_file(score.solo.note[n].qf_update,fp);
    }
    fclose(fp);
}


bel_write_updates() {
    int n,q;
    FILE *fp;
    char name[500];

    printf("writing updates\n");
    strcpy(name,scorename);
    strcat(name,".update");
    fp = fopen(name,"w");
    for (n=0; n < score.solo.num; n++) {
	fprintf(fp,"note = %d\n",n);
	fprintf(fp,"mean = \n");
	Mmp_file(score.solo.note[n].qf_update.m,fp);
	fprintf(fp,"var = \n");
	Mmp_file(score.solo.note[n].qf_update.cov,fp);
    }
    fclose(fp);
}






read_updates() {
    int n,nn,q,i,j;
    FILE *fp;
    char name[500],tag[500];
    MX mr_file(),temp;
    float scale;
    QUAD_FORM qf;


    scale = TOKENLEN/(float)SR;
    printf("reading updates\n");
    strcpy(name,scorename);
    strcat(name,".update");
    fp = fopen(name,"r");
    if (fp == NULL) { 
      printf("%s not found\nrun training to create it\n",name); 
      return(0);
    }
    for (n=0; n < cue.num; n++) {
	fscanf(fp,"cue = %d\n",&nn);
	if (nn != n) { printf("problem in read_updates()\n"); exit(0); }
	fscanf(fp,"mean = \n");
	cue.list[n].state_init.mean = mr_file(fp);
	fscanf(fp,"var = \n");
	cue.list[n].state_init.var = mr_file(fp);
    }
    for (n=0; n < score.solo.num; n++) {
      /*     	fscanf(fp,"note = %d\n",&nn); */
     	fscanf(fp,"note = %s\n",tag);
	fscanf(fp,"cue = %d\n",&nn);
	fscanf(fp,"mean = \n");
	temp = mr_file(fp);
	for (i=0; i < temp.rows; i++)
	  for (j=0; j < temp.cols; j++)
	    	    temp.el[i][j] *= scale; 
	score.solo.note[n].update.mean = temp;
	fscanf(fp,"var = \n");
	temp = mr_file(fp);
	for (i=0; i < temp.rows; i++)
	  for (j=0; j < temp.cols; j++)
	    	    temp.el[i][j] *= scale*scale;  
	score.solo.note[n].update.var = temp;
	/*	if (n == score.solo.num-1) /* kludge for variance probelm */
	/*	  score.solo.note[n].update.var.el[1][1] += .01; */

	/*	score.solo.note[n].qf_update = QFperm(QFconvert(score.solo.note[n].update)); */

	qf = QFconvert(score.solo.note[n].update); 
	if (score.solo.note[n].cue) qf = QFmake_pos_unif(qf);
	score.solo.note[n].qf_update = QFperm(qf);
	
	/*	if (n > 0) score.solo.note[n].qf_update.m = Mzeros(2,1);  */
    }
    fclose(fp);
}


read_solo_training() {
    int n,nn,q,i,j;
    FILE *fp;
    char name[500];
    MX mr_file(),temp;
    float scale;


    printf("reading updates\n");
    strcpy(name,scorename);
    strcat(name,".solo");
    fp = fopen(name,"r");
    if (fp == NULL) { 
      printf("%s not found\n run solo training");
      return(0);
    }
    for (n=0; n < score.solo.num; n++) {
	fscanf(fp,"note = %d\n",&nn);
	score.solo.note[n].qf_update = QFperm(QFread_file(fp));
    }
    fclose(fp);
}



void
bel_read_updates() {
    int n,nn,q;
    FILE *fp;
    char name[500];
    MATRIX m,cov;
    

  printf("reading updates\n");
    strcpy(name,scorename);
    strcat(name,".update");
    fp = fopen(name,"r");
    if (fp == NULL) { 
      printf("%s not found\n",name); 
      return;
    }
    for (n=0; n < score.solo.num; n++) {
	fscanf(fp,"note = %d\n",&nn);
	fscanf(fp,"mean = \n");
	m = Mmr_file(fp);
	fscanf(fp,"var = \n");
	cov =  Mmr_file(fp);
	score.solo.note[n].qf_update = QFperm(QFmv(m,cov));
    }
    fclose(fp);
}






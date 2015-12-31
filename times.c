#include <stdio.h>
#include <math.h>
#include "share.h"
//#include "share.c"
#include "global.h"
#include "dp.h"


extern char  scorename[];
extern int firstnote,lastnote;

int   *notetime;       /* notetime[i] is time in tokens the ith note starts */
int   *toklab;


#define INITRAT  .5 

initstats() {
  int i;
  SOLO_NOTE *n;
  float meas2tokens();

  for (i=0; i < score.solo.num; i++) {
    n = score.solo.note + i;
    n->count = 2;
    n->mean = meas2tokens(n->length);
    n->std = INITRAT*n->mean;
  }
}


updatestats() {
  int i,l;
  SOLO_NOTE *n;
  float x2bar,x;

  for (i=0; i < 10; i++) {
    n = score.solo.note + i;
    printf("note = %d mean = %f std = %f count = d\n",i,n->mean,n->std,n->count);
  }
  for (i=firstnote; i < lastnote-1; i++) {
    n = score.solo.note + i;
    x = notetime[i+1] - notetime[i];
    x2bar = (n->std)*(n->std) + (n->mean)*(n->mean);
    n->mean = (n->count * n->mean + x)/(n->count + 1);
    x2bar = (n->count * x2bar + x*x)/(n->count + 1);
    n->std  = sqrt(x2bar - n->mean*n->mean);
    n->count++;
  }
  for (i=0; i < 10; i++) {
    n = score.solo.note + i;
    printf("note = %d mean = %f std = %f count = d\n",i,n->mean,n->std,n->count);
  }
}


void
writebeat() {
  FILE *fp;
  int i,diff,num;
  char s[10],name[200];
  float t,l;


  strcpy(name,scorename);
  strcat(name,".nt");
  fp = fopen(name,"w");
  if (fp == NULL) { printf("couldn't open %s\n",name); return; }
  fprintf(fp,"notes = %d\n",score.solo.num);
  for (i=0; i < score.solo.num; i++) {
    num2name(score.solo.note[i].num,s);
    fprintf(fp,"%s %d\n",s,notetime[i]);
/*    diff = notetime[i+1]-notetime[i]; */
    l = score.solo.note[i].length;
    t = score.solo.note[i].time;
/*    printf("%s %f %f \n",s,t,tokens2secs((float) diff)/l); */
  }
  fclose(fp);
}   

readbeat() {
  FILE *fp;
  char s[10],name[200];
  int i,num;

  strcpy(name,scorename);
  strcat(name,".nt");
  fp = fopen(name,"r");
  if (fp == NULL) { printf("%s not found\n",name); return(0); }
  fscanf(fp,"notes = %d\n",&num);
  notetime  = (int*) malloc(num*sizeof(int));
  for (i=0; i < num; i++)
    fscanf(fp,"%s %d\n",s,notetime+i);
  fclose(fp);
 /* for (i=0; i < score.solo.num; i++) {
    num2name(score.solo.note[i].num,s);
    printf("%s %d\n",s,notetime[i]); 
  } */
  return(1);
}   

float 
toks2secs(float t) {
  return(t*(float)SKIPLEN/(float)SR);
}







set_beat(p)   
NODEPTR p; {
  NODEPTR cur = p;
  int i,count=0,j;
  int temp[1000];
 
  notetime  = (int*) malloc(score.solo.num*sizeof(int));
  for (i=0; i < score.solo.num; i++) notetime[i] = 0;
  while (p->place != start_node) {
    i= p->place->note;
    if (i < 0) i = 0;
    if (i >= firstnote && i <= lastnote)  notetime[i]++;
    p = p->mom;
  }
/*  for (i=firstnote; i <= lastnote; i++) 
    printf("%d %d %d\n",i,notetime[i],notetime[i]-notetime[i-1]); */
  for (i=firstnote+1; i <= lastnote; i++) notetime[i] += notetime[i-1];
  for (i=lastnote; i > firstnote; i--) notetime[i] = notetime[i-1];
  notetime[firstnote] = 0;

/*  for (i=firstnote; i <= lastnote; i++) 
    printf("%d %d %d\n",i,notetime[i],notetime[i]-notetime[i-1]); */
}


typedef struct {
  int last;
  float score;
} ACCNODE;

#define SIZEINC .1     /* increments for measure size */
#define SIZERAN 3.    
#define SIZESTD .3     /* seconds per measure */
#define MAXSTDS 3.
#define NOISESTD .03    /* SIZESTD/NOISESTD is relative weight of the data */

solotrain() {
  int nn,minsize,maxsize,meansize,sizes,s,n,minn,maxx,ss,notes,curcount;
  ACCNODE **grid; 
  float temp,diff,actual,expec,x,widthcon,curlength; 

  printf("training solo part\n");
  readbinscore();
  widthcon =  MAXSTDS*SIZESTD/SIZEINC;  /* width of search in discrete size units */
  minsize = (score.meastime/SIZERAN)/SIZEINC;
  maxsize = (score.meastime*SIZERAN)/SIZEINC;
  meansize = score.meastime/SIZEINC;
  sizes = maxsize+1-minsize;
  notes = score.solo.num ;
  grid = (ACCNODE **) malloc(sizeof(ACCNODE*)*(notes+1));
  grid++;
  for (n=-1; n < notes; n++) {
    grid[n] = (ACCNODE *) malloc(sizeof(ACCNODE)*sizes);
    grid[n] -= minsize;
    for (s=minsize; s <= maxsize; s++) {
      grid[n][s].score = HUGE_VAL; 
      grid[n][s].last = 0;
    }
  }
  for (s=minsize; s <= maxsize; s++)  grid[-1][s].score = 0;
  for (n=0; n < notes; n++) {
    curlength = score.solo.note[n].length;
    curcount = score.solo.note[n].count;
    actual = score.solo.note[n].mean*TOKENLEN;
    actual /= SR;  /* seconds */
    for (s=minsize; s <= maxsize; s++) {
      minn = s - widthcon;
      if (minn < minsize) minn = minsize;
      maxx = s + widthcon;
      if (maxx > maxsize) maxx = maxsize;
minn = minsize; maxx = maxsize;
      for (ss = minn; ss <= maxx; ss++) {
        temp = grid[n-1][s].score;
        x =  (ss-s)*SIZEINC/(SIZESTD*curlength);
        temp += x*x; 
        expec = ss*SIZEINC*curlength;
        x = curcount*(expec-actual)/NOISESTD;
        temp += x*x;
        if (temp < grid[n][ss].score) { 
          grid[n][ss].score = temp; 
          grid[n][ss].last = s;
        }
      }
    }
  }
  temp = HUGE_VAL;
  for (s=minsize; s <= maxsize; s++) if (grid[notes-1][s].score < temp) { 
    ss = s; 
    temp = grid[notes-1][s].score; 
  } 
  for (n=notes-1; n >= 0; n--) {
    score.solo.note[n].size = ss*SIZEINC;
    ss = grid[n][ss].last;
printf("note = %d size = %f\n",n,score.solo.note[n].size);
  }
  writebinscore();
} 
  
accomptrain() {   /* assumes both parts begin at 0 */
  int n,i,lastsdex = 0,lastadex=0,sdex=0,adex=0,a,diff;
  float goal,orig,div;
  SOLO_NOTE *solo;
  MIDI_BURST *accomp;

  printf("training accompaniment\n");
  readbinscore();
  while (sdex < score.solo.num-2) {
    sdex++; 
    while (score.solo.note[sdex].click == 0  && sdex < score.solo.num-1) sdex++;
    adex++;
    while (fabs(score.solo.note[sdex].time - score.midi.burst[adex].time) > .01) 
      if (score.solo.note[sdex].time < score.midi.burst[adex].time) 
        /*sdex++; */
        while (score.solo.note[sdex].click == 0  && sdex < score.solo.num-2) sdex++;
      else adex++;
    diff = adex - lastadex;
    orig = score.solo.note[lastsdex].size;
    goal = score.solo.note[sdex].size;
    for (a=lastadex; a < adex; a++)   {
      div = (score.midi.burst[a].time-score.solo.note[lastsdex].time) /
	    (score.solo.note[sdex].time - score.solo.note[lastsdex].time);
      score.midi.burst[a].size  = orig + (goal-orig)*div;
  /*    printf("a = %d sl = %f al = %f st = %f at = %f size1 = %f size2 = %f asize = %f\n",a,score.solo.note[lastsdex].time,score.midi.burst[lastadex].time,score.solo.note[sdex].time,score.midi.burst[adex].time,orig,goal,score.midi.burst[a].size); */
    } 
    lastadex = adex; lastsdex = sdex;
  }
  writebinscore();
}





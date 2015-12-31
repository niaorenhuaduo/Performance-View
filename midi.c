
#include "share.c"
#include "new_score.h"
#include "global.h"
#include "midi.h"
#include "conductor.h"
#include "gui.h"
#include <math.h>
#include "wasapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


#define NOTE_ON 0x90


typedef struct {
  int num;
  MIDI_EVENT *event[MAX_ACCOMP_NOTES];
} ANOTE_LIST;

typedef struct {
  int hi;
  int lo;
} ACC_EVENT;

typedef struct {
  int num;
  ACC_EVENT list[MAX_ACCOMP_NOTES];
} ACC_EVENT_LIST;


typedef struct {
  float time;
  int pitch;
  int vel;
} NT_ON;
 
typedef struct {
  int num;
  NT_ON list[MAX_ACCOMP_NOTES];
} NT_ON_DATA;

typedef struct twnode {
  int index;
  struct twnode *prev;
  int score;
  int i; /* score pos */
  int j; /* data pos */
  int k; /* data pos */
} TW_NODE; /* time warping node */

typedef struct {
  int lo;  /* range of possible k's */
  int hi;  /* for square[i][j], square[i][j].node[k] state means have currently at
	      pos i+k in score and pos j in data */
  TW_NODE *node;
} SQUARE;

#define MAX_MATCH_SUCC 15
#define MAX_DRIFT 70 /*20 */

typedef struct {
  struct match_node *node;
  int index;  /* index of consumed symbol */
} MATCH_ARC;

typedef struct {
  int num;
  MATCH_ARC arc[MAX_MATCH_SUCC];
} MATCH_NODE_LIST;

typedef struct warp_node {
  int score;
  int dat_index;  /* index into data */
  int dat_ofst;   
  int scr_index;  /* index consumed by score going into node */
  struct match_node *mom;
  struct warp_node *best_prev;
} WARP_NODE;

typedef struct match_node {
  MATCH_NODE_LIST next;
  MATCH_NODE_LIST prev;
  int lo_di;  /* data index range */
  int hi_di;
  int lo_scr;  /* score index range */
  int hi_scr;
  int rand;
  int wnum;
  WARP_NODE *warp;
  WARP_NODE *base;
} MATCH_NODE;

typedef struct {
  char name[200];
  float start;  /* phasing these out*/
  float end;
  RATIONAL start_rat;
  RATIONAL end_rat;
} SEQ_DESC;

#define MAX_SEQ_FILES 200

typedef struct {
  int num;
  SEQ_DESC sd[MAX_SEQ_FILES];
} SEQ_FILE_LIST;

  
static ANOTE_LIST no_list;
static NT_ON_DATA nod;
static SQUARE **square;  /* score pos x data pos array */
static ACC_EVENT_LIST aevent;
static MATCH_NODE *gtop,*gbot;
static SEQ_FILE_LIST sfl;
static float start_time_excerpt;
static float end_time_excerpt;
static RATIONAL start_time_excerpt_rat;
static RATIONAL end_time_excerpt_rat;
static MATCH_NODE *excerpt_start,*excerpt_end;
static int first_score_index;
static int first_chord_index;
static int last_chord_index;
static SEQ_LIST seq;
WARP_NODE *ttt;


int
MIDI_read(int fd, unsigned char *mbuff, unsigned int len)
{
  return(read(fd, mbuff, len));
}


int
read_a_byte(int fd)  /* stolen from midiator driver */
{
	unsigned char ch;

	do {
		MIDI_read(fd, &ch, 1);
	} while ( (ch & 0xff) >= 0xf8);
	/*	printf("ch = %d (%x)\n",ch,ch);*/
	return(ch);
}



void
collect_midi_data() {
  extern int midi_fd;
  int x,status;
  extern float now();
  char name[500];
  int n, i;
 
  FILE *fp;
  
  printf("made it here\n");
  /*  strcpy(name,s);
  strcat(name,".seq"); */
  printf("enter .seq file name:\n" );
  scanf("%s", name);
  printf("writing %s\n",name);
  fp = fopen(name,"w");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    exit(0);
    }  
  
  printf("made it here and here\n");
  init_midi();
  printf("ho here\n");
  init_clock();
  status = NOTE_ON;
  for (seq.num = 0; ; seq.num++) {
    x = read_a_byte(midi_fd);
    if (x == NOTE_ON || x == NOTE_OFF || x ==  PEDAL_COM) {
      status = x;
      seq.event[seq.num].time = now();
      seq.event[seq.num].command = x;
      seq.event[seq.num].d1 = read_a_byte(midi_fd);
      seq.event[seq.num].d2 = read_a_byte(midi_fd);
      printf("%f %d %d %d\n",now(),x,seq.event[seq.num].d1,seq.event[seq.num].d2);
    }
    else {
      seq.event[seq.num].time = now();
      seq.event[seq.num].command = status;
      seq.event[seq.num].d1 = x;
      seq.event[seq.num].d2 = read_a_byte(midi_fd);
    }
    n = seq.num;
    if (n >= 3 && seq.event[n-1].d1 == 21 && seq.event[n-3].d1 == 21) break;
  }
  for(i=0; i <= n-4; i++) {
    fprintf(fp, "%f\t%x\t%d\t%d\n",seq.event[i].time,seq.event[i].command,seq.event[i].d1,seq.event[i].d2); }

  fclose(fp);
}


void
collect_midi_roland_digital() {
  extern int midi_fd;
  unsigned char x,y;
  extern float now();
  char name[500];
  int n, i;
 
  FILE *fp;
  
  /*  strcpy(name,s);
  strcat(name,".seq"); */
  printf("enter .seq file name:\n" );
  scanf("%s", name);
  printf("writing %s\n",name);
  fp = fopen(name,"w");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    exit(0);
    }  
  
  init_midi();
  init_clock();
  seq.num = 0;
  while(1) {
    MIDI_read(midi_fd,&x,1);
    if (x == 0xff)  continue;
    MIDI_read(midi_fd,&y,1);
    printf("read %d %d\n",x,y);
    continue;
    /*    if (x == NOTE_ON || x == NOTE_OFF || x ==  PEDAL_COM) {
	n = seq.num;
     	seq.event[seq.num].time = now();
	seq.event[seq.num].command = x;
	MIDI_read(midi_fd,&seq.event[seq.num].d1,1);
	MIDI_read(midi_fd,&seq.event[seq.num].d2,1);
   printf("%f %x %x %x\n",seq.event[seq.num].time,seq.event[seq.num].command,seq.event[seq.num].d1,seq.event[seq.num].d2);

   if (n >= 3 && seq.event[n-1].d1 == 45 && seq.event[n-3].d1 == 45) break;*/

       /* fprintf(fp, "%f %x %x %x\n",seq.event[n].time,seq.event[n].command,seq.event[n].d1,seq.event[n].d2); */
       seq.num++;
      
    
  }

  for(i=0; i <= n-4; i++) {
    fprintf(fp, "%f %x %x %x\n",seq.event[i].time,seq.event[i].command,seq.event[i].d1,seq.event[i].d2); }

  fclose(fp);
}




void
old_collect_midi_data() {
  extern int midi_fd;
  unsigned char x;
  extern float now();

  init_midi();
  init_clock();
  seq.num = 0;
  while(1) {
    if (MIDI_read(midi_fd,&x,1)) if (x != 0xfe)  {
      if (x == NOTE_ON || x == NOTE_OFF) {
	seq.event[seq.num].time = now();
	seq.event[seq.num].command = x;
	MIDI_read(midi_fd,&seq.event[seq.num].d1,1);
	MIDI_read(midi_fd,&seq.event[seq.num].d2,1);
	printf("%f %x %x %x\n",seq.event[seq.num].time,seq.event[seq.num].command,seq.event[seq.num].d1,seq.event[seq.num].d2);
	seq.num++;
      }
    }
  }
}




static int
times_near(float t1, float t2) {
  return( fabs(t1-t2) < .001);
}





static void
make_acc_events() {
  int i,start,j;

  aevent.num =0;
  start = 0;
  for (i=0; i < MAX_ACCOMP_NOTES; i++) {
    j= start;
    while ( j-1 >= 0 && 
	    rat_cmp(score.accompaniment.note[j-1].wholerat,score.accompaniment.note[start].wholerat) == 0) j--;
    //	    times_near(score.accompaniment.note[j-1].meas,score.accompaniment.note[start].meas)) j--;
    aevent.list[aevent.num].lo = j;
    j= start;
    while ( j+1 < score.accompaniment.num && 
	    rat_cmp(score.accompaniment.note[j+1].wholerat,score.accompaniment.note[start].wholerat) == 0) j++;
    aevent.list[aevent.num].hi = j;
    aevent.num++;
    if (j == score.accompaniment.num-1) break;
    start = j+1;
  }
  if (i == MAX_ACCOMP_NOTES) { printf("out of room in make_acc_events\n"); exit(0); }
  /*   for (i=0; i < aevent.num; i++) printf("%d %d %d\n",i,aevent.list[i].lo,aevent.list[i].hi);
       exit(0);*/
}

static int
diff_by_one(unsigned i, unsigned j, int *which)  {
  /* i subset of j and differ by one element */
  unsigned temp,mask = 1;
  int k,count;

  
  temp = i & ~j;
  if (temp) return(0);  /* i not contianed in j */
  temp = j & ~i;
  count = 0;
  for (k=0; k < 32; k++) {
    if (mask&temp) { count++; *which = k; }
    mask <<= 1;
    if (count > 1) return(0);
  }
  return(count == 1);
}
     


add_match_arc(MATCH_NODE *n1, MATCH_NODE *n2, int which) {
  if (n1->next.num == MAX_MATCH_SUCC) {
    printf("can't add next arc \n");
    exit(0);
  }
  n1->next.arc[n1->next.num].node = n2;
  n1->next.arc[n1->next.num].index  = which;
  n1->next.num++;
  if (n2->prev.num == MAX_MATCH_SUCC) {
    printf("can't add prev arc \n");
    exit(0);
  }
  n2->prev.arc[n2->prev.num].node = n1;
  n2->prev.arc[n2->prev.num].index  = which;
  n2->prev.num++;
}




static MATCH_NODE* 
match_node_alloc(int lo_scr, int hi_scr,int lo_data, int hi_data) {
  /* lo_data, hi_data unused in future */
  MATCH_NODE *temp;
  int i,num;

  num = hi_scr+1-lo_scr;
  temp = (MATCH_NODE *) malloc(sizeof(MATCH_NODE));
  temp->next.num = temp->prev.num = 0;
  temp->lo_di = lo_scr - MAX_DRIFT; /*lo_data;*/
  temp->hi_di = hi_scr + MAX_DRIFT; /*hi_data;*/
  temp->wnum = temp->hi_di + 1 - temp->lo_di;
  temp->lo_scr = lo_scr;
  temp->hi_scr = hi_scr;
  temp->base = temp->warp = 
    (WARP_NODE *) malloc(sizeof(WARP_NODE)*temp->wnum);
  temp->warp -= temp->lo_di;
  for (i=temp->lo_di; i <= temp->hi_di; i++) {
    temp->warp[i].score = -1;
    temp->warp[i].dat_index = i;
    temp->warp[i].scr_index = 1010;
    temp->warp[i].mom = temp;
    temp->warp[i].best_prev = NULL;
  }
  return(temp);
}


#define MAX_MATCH_BUFF 50000


static void
perm_graph(int lo, int hi, MATCH_NODE *start, MATCH_NODE *end) {
  int length,iter,i,which,j,mask,ld,hd;
  MATCH_NODE *buff[MAX_MATCH_BUFF];


  length = hi + 1 - lo;
  iter = 1;
  iter <<= length;
  if (iter > MAX_MATCH_BUFF) {
    printf("not enough room in perm_graph()\n");
    exit(0);
  }
  buff[0] = start;
  buff[iter-1] = end;
  ld = (lo-MAX_DRIFT < -1) ? -1 : lo-MAX_DRIFT;
  /*  hd = (hi+MAX_DRIFT > nod.num-1) ? nod.num-1 : hi+MAX_DRIFT; */
  hd = hi+MAX_DRIFT;
  for (i=1; i < iter-1; i++) buff[i] = match_node_alloc(lo,hi,ld,hd);
  for (i=0; i < iter; i++) for (j=0; j < iter; j++) {
    if (diff_by_one(i,j,&which)) add_match_arc(buff[i],buff[j],which+lo);
  }
}
 

void
read_seq_file(char *s, SEQ_LIST *sq) {  /* no suffix */
  FILE *fp;
  int i,j,command,pitch,vel;
  float time;
  char name[500];

  if (sq->event == NULL) sq->event = (SEQ_EVENT *) malloc(sizeof(SEQ_EVENT)*MAX_SEQ);
  strcpy(name,s);
  strcat(name,".seq");
  printf("reading %s\n",name);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    exit(0);
  }
  sq->num = 0;
  for (i=0; i < MAX_SEQ; i++) {
    if (feof(fp)) break;
    fscanf(fp,"%f %x %d %d\n",&time,&command,&pitch,&vel);
    sq->event[sq->num].time = time;
    sq->event[sq->num].command = command;
    sq->event[sq->num].d1 = pitch;
    sq->event[sq->num].d2 = vel;
    sq->num++;
  }
  if (i== MAX_SEQ) {
    printf("not enough room for sequencer file\n");
    exit(0);
  }
}


static void
read_no_data(char *s) {
  FILE *fp;
  int i,j,command,pitch,vel;
  float time;
  char name[500];

  if (seq.event == NULL) seq.event = (SEQ_EVENT *) malloc(sizeof(SEQ_EVENT)*MAX_SEQ);
  strcpy(name,s);
  strcat(name,".seq");
  printf("reading %s\n",name);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    exit(0);
  }
  j = 0;
  seq.num = 0;
  for (i=0; i < MAX_SEQ; i++) {
    if (feof(fp)) break;
    fscanf(fp,"%f %x %d %d\n",&time,&command,&pitch,&vel);
    seq.event[seq.num].time = time;
    seq.event[seq.num].command = command;
    seq.event[seq.num].d1 = pitch;
    seq.event[seq.num].d2 = vel;
    seq.num++;
    if (command == NOTE_ON && vel != 0) {
      nod.list[j].time = time;
      nod.list[j].pitch = pitch;
      nod.list[j].vel = vel;
      j++;
      if (j==MAX_ACCOMP_NOTES) { printf("out of room in note on data list\n"); exit(0); }
      /*                              printf("j = %d time = %f command = %d pich = %d vel = %d\n",j,time,command,pitch,vel);     */
    }
  }
  if (i== MAX_SEQ) {
    printf("not enough room for sequencer file\n");
    exit(0);
  }
  nod.num = j;
  /*    printf("read %d note_on's\n",j); */
     /*       exit(0);  */
}


void
write_seq(char *s) {
  FILE *fp;
  int i,j,command,pitch,vel;
  float time;
  char name[500];

  strcpy(name,s);
  strcat(name,".seq");
  printf("writing %s\n",name);
  fp = fopen(name,"w");
  if (fp == NULL) {
    printf("couldn't open %s\n",name);
    exit(0);
  }
  for (i =first_accomp+1; i < last_accomp; i++) // kludge to avoid negative dt's when writing midi
	if (score.midi.burst[i].ideal_secs < score.midi.burst[i-1].ideal_secs) {
      printf("nudging %s pos by %f\n",score.midi.burst[i].observable_tag,score.midi.burst[i].ideal_secs-score.midi.burst[i-1].ideal_secs);
	  score.midi.burst[i].ideal_secs = score.midi.burst[i-1].ideal_secs;
	}


  //  for (i=0; i < score.midi.num; i++) {
  for (i=first_accomp; i <= last_accomp; i++) {
    time = score.midi.burst[i].ideal_secs;
    for (j=0; j < score.midi.burst[i].action.num; j++) {
      pitch = score.midi.burst[i].action.event[j].notenum;
      command = score.midi.burst[i].action.event[j].command;
      vel = score.midi.burst[i].action.event[j].volume;
	  fprintf(fp,"%f %x %d %d\n",time,command,pitch,vel);
	}
  }
}


#define TEMP_LIM 197

static void
make_match_graph(MATCH_NODE **top, MATCH_NODE **bot) {
  int i,j,s,ld,hd;
  MATCH_NODE *start,*end,*lend,*cur;
  MATCH_ARC arc;


  start = *top = match_node_alloc(-1,-1,-1,MAX_DRIFT);
  for (i=0; i < /*TEMP_LIM*/aevent.num; i++) {
    ld = aevent.list[i].lo-MAX_DRIFT;
    if (ld < -1) ld = -1;
    hd = aevent.list[i].hi+MAX_DRIFT;
    /*  if (hd  > nod.num-1) hd = nod.num-1; */
    end = match_node_alloc(aevent.list[i].hi,aevent.list[i].hi,ld,hd);
    /*    printf("%d %d\n",aevent.list[i].lo,aevent.list[i].hi); */
    perm_graph(aevent.list[i].lo,aevent.list[i].hi,start,end);
    /*    printf("to perm graph: lo = %d hi = %d\n", aevent.list[i].lo,aevent.list[i].hi); */
    start = end;
  }
  *bot = end;
  /*  for (i=0; i < 10; i++) {
    cur = top;
    while (cur != bot) {
      s = rand()%(cur->next.num);
      printf("%d ",cur->next.arc[s].index);
      cur = cur->next.arc[s].node;
    }
    printf("\n");
  }
  exit(0); */
}







static int
examine_arc(MATCH_NODE *m1, int d1, MATCH_NODE *m2, int d2, int which) {
  int scr,cost;
  WARP_NODE *w1,*w2;
  void score_warp_node(WARP_NODE *wn);


   if (d2 > m2->hi_di || d2 < m2->lo_di) {
    /*  printf("weirdness d2 = %d hi = %d lo = %d\n",d2,m2->hi_di,m2->lo_di);
    exit(0);  */
    return(10000);
  }
  if (d1 > m1->hi_di || d1 < m1->lo_di) {
    return(10000);
    /*    printf("weirdness d1 = %d hi = %d lo = %d\n",d1,m1->hi_di,m1->lo_di);
    exit(0);  */
  }
  /*  if (printf("%f\n",score.accompaniment.note[which].meas);*/

  w1 = &m1->warp[d1];
  w2 = &m2->warp[d2];
  if (w1->score < 0) score_warp_node(w1);
  if (d1 == d2 || m1 == m2) cost = 1;
  else cost = (score.accompaniment.note[which].pitch == nod.list[d2].pitch) ? 0 : 1;
  scr = w1->score + cost;
if (w2 == ttt) printf("w1->score = %d cost = %d score = %d\n",w1->score,cost,scr);
  if (scr < w2->score) { 
if (w2 == ttt) printf("which =  %d\n",which);
    w2->score = scr; 
    w2->best_prev = w1; 
    w2->scr_index = which;
  }
}




#define NOTE_OMITTED -1
#define NOTE_ADDED -2





void
score_warp_node(WARP_NODE *wn) {
  int i,cost,which,d2;
  MATCH_NODE *mom,*node;
  
  
  if (wn->score >= 0) return;
  wn->score = 10000;
  mom = wn->mom;
  /*printf("%d\n",wn->scr_index); */
  d2 = wn->dat_index;
  /*    if (d2 == -1 && mom->prev.num == 0) {  */
  /*  if (score.accompaniment.note[wn->mom->lo_scr].meas <= start_time_excerpt) {*/
    if (wn->mom->lo_scr < first_score_index) {
  /*  if (wn->mom == excerpt_start && d2 == -1) { */
    wn->score = 0;
    return;
  }
  examine_arc(mom,d2-1,mom,d2,NOTE_ADDED);
  for (i=0; i < mom->prev.num; i++) {
    node = mom->prev.arc[i].node;
    which = mom->prev.arc[i].index;
    examine_arc(node,d2,mom,d2,which);  /* note omitted */
  }
  for (i=0; i < mom->prev.num; i++) {
    node = mom->prev.arc[i].node;
    which = mom->prev.arc[i].index;
    examine_arc(node,d2-1,mom,d2,which);
  }
}



void
rearrange_gnode(MATCH_NODE *node) {
  int i,lo,hi;

  lo = node->lo_scr - MAX_DRIFT - first_score_index;
  if (lo < -1) lo = -1;
  hi = node->hi_scr + MAX_DRIFT - first_score_index;
  if (hi > nod.num-1) hi = nod.num-1;
  node->lo_di = lo;
  node->hi_di = hi;
  node->warp = node->base - node->lo_di;
  for (i=node->lo_di; i <= node->hi_di; i++) {
    node->warp[i].dat_index = i;
    node->warp[i].score = -1;
    node->warp[i].scr_index = 1010;  /* this is unset */
    node->warp[i].best_prev = NULL;
  }
}

static void
adj_grph(MATCH_NODE *node,int rand) {
  int i,lo,hi;

  if (node->rand == rand) return;
  rearrange_gnode(node);
  for (i=0; i < node->next.num; i++) {
    adj_grph(node->next.arc[i].node,rand);
    node->rand = rand;
  }
}


static void
adjust_graph(MATCH_NODE *start) {
  int r;

  r = rand();
  adj_grph(start,r);
}

static float
find_end(float time, int pitch) {
  int pedal,j,past;

  pedal = 0;
  past = 0;
  for (j=0; j < seq.num; j++) {
    if (seq.event[j].command == 0xb0)  pedal = (seq.event[j].d2 == 127);
    if (seq.event[j].time > time) {
      if (seq.event[j].command == NOTE_ON &&
	  seq.event[j].d1 == pitch && seq.event[j].d2 > 0)
	return(seq.event[j].time);
      if (seq.event[j].command == NOTE_ON &&
	  seq.event[j].d1 == pitch && seq.event[j].d2 == 0) past = 1;

      if (past && (pedal==0)) return(seq.event[j].time);
    }
    /*    if (seq.event[j].time > time) {
      if (pedal == 0 && seq.event[j].d1 == pitch  && seq.event[j].d2 == 0) 
	return(seq.event[j].time);
      if (pedal == 1 && seq.event[j].command == 64 && seq.event[j].d2 == 0) 
	return(seq.event[j].time);
    }*/

  }
}

static void
traceback(WARP_NODE *w, int quiet) {
  int s,d,dd,ss,i,pedal;
  char dname[500],sname[500],lname[500];
  float meas,time,start,last_meas_match,last_match_time;

  printf("quiet = %d\n",quiet);
  for (i=0; i < score.accompaniment.num; i++) {
    score.accompaniment.note[i].found = 0;
    score.accompaniment.note[i].start_time = 0;
    score.accompaniment.note[i].end_time = 0;
    score.accompaniment.note[i].vel = 0;
  }
  while (w->mom != NULL) {
    d =   w->dat_index;
    dd = nod.list[d].pitch;
    time =   nod.list[d].time;
    s = w->scr_index;
    ss = score.accompaniment.note[s].pitch;
    num2name(ss,sname);
    num2name(dd,dname);
    meas = score.accompaniment.note[s].meas;
    /*    printf("s = %d (%s %f) d = %d (%s %f)\n",s,sname,meas,d,dname,time); */
    /*                     printf("score = %d d = %d (%d) s = %d (%d)\n",w->score,d,dd,s,ss);      */
	/*    printf("lo = %d hi = %d\n",w->mom->lo_di,w->mom->hi_di); */
    if (w->best_prev == NULL) break;
    if (d == w->best_prev->dat_index) {
      if (quiet == 0) 
	printf("%d-th score note (%s) omitted at meas %f (last match was %s measure = %f time = %f)\n",s,sname,meas,lname,last_meas_match,last_match_time);
    }
    else if (s == NOTE_ADDED) { 
      if (quiet == 0) 
	printf("data note %d (%s) added at time %f (last match was %s measure = %f time = %f)\n",d,dname,time,lname,last_meas_match,last_match_time);
    }
    else if (dd != ss) {
      if (quiet == 0) 
	printf("note %d (%s) substitued for %d (%s) at meas %f (time %f)\n",
	       dd,dname,ss,sname,meas,time);     
    }
    else {
      score.accompaniment.note[s].found = 1;
      start = score.accompaniment.note[s].start_time = nod.list[d].time;
      score.accompaniment.note[s].vel = nod.list[d].vel;
      score.accompaniment.note[s].end_time = find_end(start,ss);
      last_meas_match = score.accompaniment.note[s].meas;
      last_match_time = nod.list[d].time;
      num2name(score.accompaniment.note[s].pitch,lname);
    }
    w = w->best_prev;
  }
}

static void
extract_pos_in_meas(char *s1, char *s2) {
  char *s;


  while (*s1 != '+') s1++;
  s1++;
  strcpy(s2,s1);
  s = s2;
  while (*s != '_') s++;
  *s = 0;

}

static int
simultaneous_notes(int i) {
  int j,jj;

  jj = j = i;
  while ((j-1 >= 0) && rat_cmp(score.accompaniment.note[j-1].wholerat,score.accompaniment.note[j].wholerat) == 0) j--;
  while ((jj+1 < score.accompaniment.num) && rat_cmp(score.accompaniment.note[jj+1].wholerat,score.accompaniment.note[jj].wholerat) == 0) jj++;
  return(1+jj-j);
}

#define NOT_FND_STR "---"

static void
write_mch(char *s) {
  int i;
  FILE *fp;
  char name[500],mp[500];;

  strcpy(name,s);
  strcat(name,".mch");
  printf("creating %s\n",name);
  fp = fopen(name,"w");
  /*  fprintf(fp,"tag\t\tmeaspos\twhole\tpitch\tstart\tend\tvel\tchord\n"); */
  for (i=0; i < score.accompaniment.num; i++) {
    /*    printf("found = %d %d\n",i,score.accompaniment.note[i].found); */
    if (score.accompaniment.note[i].found) {
      extract_pos_in_meas(score.accompaniment.note[i].observable_tag,mp);
      fprintf(fp,"%s\t%s\t%5.3f\t%d\t%5.3f\t%5.3f\t%d\t%d\n", 
	      score.accompaniment.note[i].observable_tag,
	      mp,
	      score.accompaniment.note[i].meas,
	      score.accompaniment.note[i].pitch,
	      score.accompaniment.note[i].start_time,
	      score.accompaniment.note[i].end_time,
	      score.accompaniment.note[i].vel,
	      simultaneous_notes(i));
    }
    else
      fprintf(fp,"%s\t%s\t%5.3f\t%d\t%s\t%s\t%s\t%s\n",
	      score.accompaniment.note[i].observable_tag,
	      NOT_FND_STR,
	      score.accompaniment.note[i].meas,
	      score.accompaniment.note[i].pitch,
	      NOT_FND_STR,
	      NOT_FND_STR,
	      NOT_FND_STR,
	      NOT_FND_STR);
    if (score.accompaniment.note[i].found == 0) 
      if (score.accompaniment.note[i].meas >= start_time_excerpt && score.accompaniment.note[i].meas <= end_time_excerpt)
	  printf("%s not found\n",score.accompaniment.note[i].observable_tag);
  }
  fclose(fp);
}


void
accomp_time_tag(RATIONAL rat, float time, char *tag) {
  char stm[500],s[500];
  RATIONAL r;

  wholerat2string(rat, s);
  /*  printf("%d/%d = %s\n",rat.num,rat.den,s);
  r = string2wholerat(s);
  printf("%d/%d %d/%d\n",r.num,r.den,rat.num,rat.den);
  if (rat_cmp(r,rat) != 0) {
    printf("problem with string2wholerat\n");
    exit(0);
    }*/
  sprintf(stm,"%5.3f",time);
  strcpy(tag,"atm_");
  strcat(tag,s);
}


static void
write_accomp_times_midi(char *s) {
  int i,j,found;
  FILE *fp;
  char name[500],tag[500];
  float mint,t;
  ACCOMPANIMENT_NOTE *acc;

  strcpy(name,s);
  strcat(name,".atm");
  printf("creating %s\n",name);
  fp = fopen(name,"w");
  acc = score.accompaniment.note;
  mint = (acc[0].found) ? acc[0].start_time : HUGE_VAL;
  
  for (i=1; i < score.accompaniment.num; i++) {
      if (rat_cmp(acc[i].wholerat, acc[i-1].wholerat) == 0) {
      if (acc[i].found == 0) continue;
      t = acc[i].start_time;
      if (t < mint) mint = t;
    }
    else {
      accomp_time_tag(acc[i-1].wholerat,acc[i-1].meas,tag);
      /*      if (mint < HUGE_VAL) printf("%s\t%f\n",tag,mint);*/
      if (mint < HUGE_VAL) fprintf(fp,"%s\t%f\n",tag,mint);
      mint = (acc[i].found) ? acc[i].start_time : HUGE_VAL;
    }
  }
  accomp_time_tag(acc[i-1].wholerat,acc[i-1].meas,tag);
  if (mint < HUGE_VAL) fprintf(fp,"%s\t%f\n",tag,mint);
  fclose(fp);
}

static void
print_record(char *s) {
  write_mch(s);
  write_accomp_times_midi(s);
}

void
read_match(char *s) {
  int i,vel,j,x;
  FILE *fp;
  char name[500],tag[500],g1[500],g2[500],g3[500],st[500],en[500],vl[500],xx[500];
  float start,end;


  strcpy(name,s);
  strcat(name,".mch");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("couldn't open %s\n",name);
    exit(0);
  }
  //  while (fgetc(fp) != '\n');  /* throw out first line (not tested yet) */
  for (i=0; i < score.accompaniment.num; i++)  score.accompaniment.note[i].found = 0;
  while (!feof(fp)) {
    fscanf(fp,"%s %s %s %s %s %s %s %s\n",tag,g1,g2,g3,st,en,vl,xx);
    //    fscanf(fp,"%s %s %s %s %f %f %d %d\n",tag,g1,g2,g3,&start,&end,&vel,&x);
    for (j=0; j < score.accompaniment.num; j++) {
      if (score.accompaniment.note[j].observable_tag == 0) continue;
      if (strcmp(tag,score.accompaniment.note[j].observable_tag) == 0) break;
    }
    if (j == score.accompaniment.num) continue;
    if (strcmp(st,NOT_FND_STR) == 0) score.accompaniment.note[j].found = 0;
    else {
      score.accompaniment.note[j].found = 1;
      /*      score.accompaniment.note[j].start_time = start;
      score.accompaniment.note[j].end_time = end;
      score.accompaniment.note[j].vel = vel;*/
      sscanf(st,"%f",&score.accompaniment.note[j].start_time);
      sscanf(en,"%f",&score.accompaniment.note[j].end_time);
      sscanf(vl,"%d",&score.accompaniment.note[j].vel);
    }
  }
  fclose(fp);
}


#define PEDAL_WINDOW_SECS .2  

static void
min_max_pedal(float s, int *mn, int *mx) {  /* compute the minimum and maximum pedal value in the neighborhood */
  int i;

  *mn = 127;
  *mx = 0;
  for (i=0; i < seq.num; i++) {
    if (seq.event[i].time < s - PEDAL_WINDOW_SECS) continue;
    if (seq.event[i].time > s + PEDAL_WINDOW_SECS) continue;
    if (seq.event[i].command != PEDAL_COM) continue;
    if (seq.event[i].d1 != 64)   continue;
    if (seq.event[i].d2 > *mx) *mx = seq.event[i].d2;
    if (seq.event[i].d2 < *mn) *mn = seq.event[i].d2;
  }
  if (*mx == 0) *mn = 0;  // no pedaling at all
}



void
compute_pedaling_from_bosendorfer(char *file) {
  int i,j,n,mn,mx;
  ACCOMPANIMENT_NOTE *a;
  MIDI_EVENT *m;
  float tot,ave;
  FILE *fp;
  char name[500],pos[500];
  RATIONAL mid;


  read_seq_file(file,&seq);
  read_match(file);
  strcpy(name,file);
  strcat(name,".ped");
  fp = fopen(name,"w");
  for (i=0; i < score.midi.num; i++) {
    for (j=tot=n=0; j < score.midi.burst[i].action.num; j++) {
      m = score.midi.burst[i].action.event + j;
      a = score.midi.burst[i].action.event[j].accomp_note;
      if (a == NULL) continue;
      n++;
      tot += a->start_time;
      //      printf("%s %x %d %d\n",score.midi.burst[i].observable_tag,m->command,m->notenum,m->volume);
      //      if (a == NULL) { printf("problem here\n"); continue; }
      //      printf("time = %f\n",a->start_time);
    }
    if (n == 0) continue;
    ave = tot / n;
    min_max_pedal(ave,&mn,&mx);
    if (mx-mn < 40)  {
      fprintf(fp,"%s\t%d\n",score.midi.burst[i].observable_tag+4,(mn+mx)/2);
      continue;
    }
    if (i == score.midi.num-1) continue;
    fprintf(fp,"%s\t%d\n",score.midi.burst[i].observable_tag+4,mn);
    mid = add_rat(score.midi.burst[i].wholerat,score.midi.burst[i+1].wholerat);
    mid.den *= 2; // div by 2
    wholerat2string(mid,pos);
    fprintf(fp,"%s\t%d\n",pos,mx);
    //    printf("%s min pedal = %d max pedal = %d\n",score.midi.burst[i].observable_tag,mn,mx);
  }
  fclose(fp);
}


static void
read_seq_desc_file(char *name) {
  FILE *fp;
  char line[1000],c,st[500],en[500];
  float start,end;

  //  printf("enter the name of the .seq description file:\n");
  //  scanf("%s",name);
  fp = fopen(name,"r");
  if (fp == NULL) {
    printf("couldn't read %s\n",name);
    exit(0);
  }
  while (c = fgetc(fp) != '\n');
  while (!feof(fp)) {
	fscanf(fp,"%s %s -- %s",name,st,en);
	if (feof(fp)) break;
	start =  meas2score_pos(st);
    end  =  meas2score_pos(en);
       printf("start = %s %f\n",st,start);
    printf("end = %s %f\n",en,end);
    printf("name = %s\n",name);

    /*    printf("last = %f\n",score.measdex.measure[score.measdex.num].pos);
    exit(0);*/
    /*    fscanf(fp,"%s\t%f-%f",name,&start,&end); */
    sfl.sd[sfl.num].start = start;
    sfl.sd[sfl.num].end = end;
    sfl.sd[sfl.num].start_rat =   string2wholerat(st);
    sfl.sd[sfl.num].end_rat =   string2wholerat(en);
    strcpy(sfl.sd[sfl.num].name,name);
    //    printf("start = %d/%d end = %d/%d\n",sfl.sd[sfl.num].start_rat.num,sfl.sd[sfl.num].start_rat.den,sfl.sd[sfl.num].end_rat.num,sfl.sd[sfl.num].end_rat.den);
    //    exit(0);
    /*    printf("%s %f-%f\n",sfl.sd[sfl.num].name,sfl.sd[sfl.num].start,sfl.sd[sfl.num].end);  */
    sfl.num++;
    while (c = fgetc(fp) != '\n') if (feof(fp)) break;
  }
  fclose(fp);
}

static void
find_start_end(char *name, float *start, float *end, int *first) {
  int i,j;
  char *temp;

  temp = name;
  i =0;
  while (name[i]) {
    if (name[i] == '/') temp = name+i+1;
    i++;
  }
  
  for (i=0; i < sfl.num; i++) {
    if (strcmp(temp,sfl.sd[i].name) != 0) continue;
    *start = sfl.sd[i].start;
    *end = sfl.sd[i].end;
    for (j= score.accompaniment.num-1; j >= 0; j--) {
      if (score.accompaniment.note[j].meas <= *start) {
	if (times_near(score.accompaniment.note[j].meas,*start) == 0) break;
	else *first = j;
      }
    }
    return;
  }
  printf("%s not found in sequence description list\n",temp);
  exit(0);
}

static void
find_start_end_rat(char *name) {
  int i,j;
  char *temp,str[500];
  RATIONAL r;

  temp = name;

  /*  i =0;
  while (name[i]) {
    if (name[i] == '/') temp = name+i+1;
    i++;
    }*/
  
  first_chord_index = last_chord_index = -1;
  for (i=0; i < sfl.num; i++) {
    if (strcmp(temp,sfl.sd[i].name) != 0) continue;
    start_time_excerpt_rat = sfl.sd[i].start_rat;
    end_time_excerpt_rat  = sfl.sd[i].end_rat;
    for (j=0; j < aevent.num; j++) {
      i = aevent.list[j].lo;
      if (rat_cmp(score.accompaniment.note[i].wholerat,start_time_excerpt_rat) == 0) 
	first_chord_index = j;
      if (rat_cmp(score.accompaniment.note[i].wholerat,end_time_excerpt_rat) == 0) 
	last_chord_index = j;
      r = score.accompaniment.note[i].wholerat;
      wholerat2string(r,str);
      //      printf("%d/%d  %d/%d %s\n",r.num,r.den,end_time_excerpt_rat.num,end_time_excerpt_rat.den,str);
    }
    if (last_chord_index == -1) {
      printf("couldn't find match for end: %d/%d\n",
	     end_time_excerpt_rat.num,end_time_excerpt_rat.den);
      exit(0);
    }
    if (first_chord_index == -1) {
      printf("couldn't find match for start: %d/%d\n",
	     start_time_excerpt_rat.num,start_time_excerpt_rat.den);
      exit(0);
    }
    return;
  }
  printf("%s not found in sequence description list\n",temp);
  exit(0);
}

static int
is_knot(MATCH_NODE *node) {
  return(node->lo_scr == node->hi_scr);
}


static void
set_excerpt_ends(MATCH_NODE *start, MATCH_NODE *end) {
  int i;


  excerpt_end = start;
  while (excerpt_end->next.num) {
    if (is_knot(excerpt_end) && 
	score.accompaniment.note[excerpt_end->lo_scr].meas >= end_time_excerpt)
      break;
    else excerpt_end = excerpt_end->next.arc[0].node;
  }
  excerpt_start = end;
  while (excerpt_start->prev.num) {
    if (is_knot(excerpt_start) && 
	score.accompaniment.note[excerpt_start->lo_scr].meas <= start_time_excerpt)
      break;
    else excerpt_start = excerpt_start->prev.arc[0].node;
  }
  /*    printf("start = %f end = %f\n",score.accompaniment.note[excerpt_start->lo_scr].meas,
	score.accompaniment.note[excerpt_end->lo_scr].meas);  */
}


static int
find_path() {
  int scr,quiet,last;
  float er;

  last = nod.num-1;
  if (last < excerpt_end->lo_di || last > excerpt_end->hi_di) {
    printf("score and data lengths incompatable\n");
    printf("lo_di = %d last_data  = %d hi_di = %d\n",
	   excerpt_end->lo_di,last, excerpt_end->hi_di);
    return(0);
  }
  score_warp_node(&excerpt_end->warp[last]); 
  scr = excerpt_end->warp[last].score;
  er = scr/(float)nod.num;
  printf("error rate  = %f score = %d nod.num = %d\n",er,scr,nod.num);
  quiet = (er < .15); 
  traceback(&excerpt_end->warp[last],quiet); 
  return(quiet);
}


match_midi_stable() {
  int i,s,success;
  MATCH_NODE_LIST list;
  MATCH_NODE *end,*start,*cur;
  char name[500],file[500];
  FILE *fp;
  float st,en;

  //  read_seq_desc_file();
  printf("enter the file containing the seq files\n");
  printf("(.seq files should not have .seq suffix)\n");
  scanf("%s",file);
  /*  printf("filer = %s\n",file); */
  fp = fopen(file,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",file);
    exit(0);
  }
  make_acc_events();
  make_match_graph(&start,&end);
  while (feof(fp) == 0) {
    fscanf(fp,"%s",name);
    if (feof(fp)) break;
    find_start_end(name,&start_time_excerpt,&end_time_excerpt,&first_score_index);
    printf("processing %s %f %f %d\n",name,start_time_excerpt,end_time_excerpt,first_score_index);
    set_excerpt_ends(start,end);
    /*for (i=first_score_index-5; i < first_score_index+5; i++)
printf("%d %f\n",i,score.accompaniment.note[i].meas); */
    read_no_data(name);
    adjust_graph(start);
    /*    for (i=0; i < excerpt_start->next.num; i++)
printf("index = %d num = %d\n",excerpt_start->next.arc[i].index,excerpt_start->next.arc[i].node->next.num);
       printf("%d %d\n",excerpt_start->lo_di,excerpt_start->hi_di);
    printf("%d %d\n",excerpt_start->lo_scr,excerpt_start->hi_scr);
    printf("%d %d\n",start->lo_scr,start->hi_scr);
exit(0); */
    success = find_path();
    /*    score_warp_node(&excerpt_end->warp[nod.num-1]);  */
    /*printf("%d %d %d\n",excerpt_end->lo_di, nod.num-1,excerpt_end->hi_di);
printf("%d %d %d\n",excerpt_start->lo_di, 0,excerpt_start->hi_di); 
exit(0); */
    /*    traceback(&excerpt_end->warp[nod.num-1],0);  */
    if (success) print_record(name);
  }
}


typedef struct msx {
  int chrd;
  unsigned int set;  /* taken as a boolean from 0 .. 2^aevent.list[chrd].hi-aevent.list[chrd].lo+1 
			gives a subset of
			acconted for notes  in chord */
  float score;
  struct msx *pred;
  int last_chrd;
  float last_time;
  int dat;
  int used;
} MATCH_STATE;


#define MATCH_LIST_LEN 2000

typedef struct {
  int num;
  MATCH_STATE *list[MATCH_LIST_LEN];
} MATCH_LIST;

static MATCH_LIST match_list;


#define MATCH_STATE_BUF_SIZE 50000

typedef struct {
  int num;
  MATCH_STATE **list;
} MATCH_BUF;

static MATCH_BUF match_buf;


static void
alloc_match_buf() {
  int i;

  match_buf.list = (MATCH_STATE **) malloc(MATCH_STATE_BUF_SIZE*sizeof(MATCH_STATE));
  for (i=0; i < MATCH_STATE_BUF_SIZE; i++) 
    match_buf.list[i] = (MATCH_STATE *) malloc(sizeof(MATCH_STATE));
  match_buf.num = 0;
}

static MATCH_STATE *
alloc_match_node() {

  if (match_buf.num >= MATCH_STATE_BUF_SIZE-1) {
    printf("out of room in alloc_match_node()\n");
    exit(0);
  }
  return(match_buf.list[match_buf.num++]);
}


static void
add_match_state(MATCH_STATE *m) {
  if (match_list.num == MATCH_LIST_LEN) {
    printf("out ofroom in add_match_state()\n");
    exit(0);
  }
  if (m->dat >= nod.num) return;
  if (m->chrd > last_chord_index) return;
  match_list.list[match_list.num++] = m;
}

static void
delete_match_state(int j) {
  if (j >= match_list.num) {
    printf("trying to delete nonexistant el\n");
    exit(0);
  }
  match_list.list[j] = match_list.list[--match_list.num];
}



static MATCH_STATE *
init_match_state(int chord, unsigned int set, MATCH_STATE *pred, int dat, float score, 
		 int last_chord,  float last_time) {
  MATCH_STATE *new;

  new = alloc_match_node();
  new->chrd = chord;
  new->set = set;
  new->pred = pred;
  new->dat = dat;
  new->score = score;
  new->last_chrd = last_chord;
  new->last_time = last_time;
  return(new);
}

static void
find_used(MATCH_STATE *m) {
  if (m->used == 1) return;
  m->used = 1;
  if (m->pred != NULL) find_used(m->pred);
}

static void
unused_prune() {
  int i;
  MATCH_STATE *temp;

  for (i=0; i < match_buf.num; i++) match_buf.list[i]->used = 0;
  for (i=0; i < match_list.num; i++) find_used(match_list.list[i]);
  for (i= match_buf.num-1; i >= 0; i--)  if (match_buf.list[i]->used == 0) {
    match_buf.num--;
    temp =  match_buf.list[i];
    match_buf.list[i] = match_buf.list[match_buf.num];
    match_buf.list[match_buf.num] = temp;
  }
}


static void
print_match_list() {
  int i;

  for (i=0; i < match_list.num; i++)
    printf("chrd = %d\tset = %x\tdat=%d\tscore =%f\tpred = %d\n",match_list.list[i]->chrd,match_list.list[i]->set,match_list.list[i]->dat,match_list.list[i]->score,match_list.list[i]->pred);
  printf("\n");
}

static int
chord_full(int chrd, unsigned int set) {
  int num,full;

  if (chrd == -1) return(1);
  full = 1;
  num =   1+ aevent.list[chrd].hi-aevent.list[chrd].lo;
  full <<= num;
  full--;
  return (set == full);
}



static void
fast_expand(MATCH_STATE *cur) {
  unsigned int full,mask,set;
  int i,chrd,num,dat,nno,ai,chord_is_full,last_chord,used_all_data;
  MATCH_STATE *new;
  float next_t,scr;


  dat = cur->dat;
  nno = nod.list[dat+1].pitch; // next data note pitch
  next_t = nod.list[dat+1].time; // next data note time
  scr = cur->score;
  new = init_match_state(cur->chrd, cur->set, cur, dat+1, scr+1., 
			 cur->last_chrd, cur->last_time);
  add_match_state(new); /* skip the data note */
  chord_is_full = chord_full(cur->chrd,cur->set);
  last_chord = (cur->chrd == last_chord_index);
  used_all_data = (dat == nod.num-1);
  if (used_all_data && chord_is_full && last_chord) {
    add_match_state(cur);
    return;
  }
  if (chord_is_full) {
    chrd = cur->chrd+1;
    set = 0;
  }
  else  {
    chrd = cur->chrd;
    set = cur->set;
  }
  num =   1+ aevent.list[chrd].hi-aevent.list[chrd].lo;
  ai = aevent.list[chrd].lo;
  mask = 1;
  for (i=0; i < num; i++)  {
    if ((mask & set) == 0) {  
      new = init_match_state(chrd, set | mask, cur, dat, scr+1., 
			 cur->last_chrd, cur->last_time);
      add_match_state(new); /* skip this chord member */
      //      printf("num = %d i = %d set = %d mask = %d or = %d\n",num,i,set,mask,new->set);
    }
    if (((mask & set) == 0) && (nno == score.accompaniment.note[ai+i].pitch)) {  
      new = init_match_state(chrd, set | mask, cur, dat+1, scr, 
			 cur->chrd, next_t);
      add_match_state(new); /* a match */
      //      printf("new->chrd = %d new->last_chrd = %d\n",new->chrd,new->last_chrd);
      if (new->chrd == new->last_chrd) 
	if (next_t - cur->last_time > .1) new->score = 10000;
      add_match_state(new);
    }
    mask <<= 1;
  }
}

static int
match_comp(const void *p1, const void *p2) {
  MATCH_STATE **m1, **m2;

  m1 = (MATCH_STATE **) p1;
  m2 = (MATCH_STATE **) p2;
  if ((*m1)->chrd != (*m2)->chrd) return((*m1)->chrd-(*m2)->chrd);
  if ((*m1)->set != (*m2)->set) return((*m1)->set-(*m2)->set);
  if ((*m1)->dat != (*m2)->dat) return((*m1)->dat-(*m2)->dat);
  if ((*m1)->score > (*m2)->score) return(1);
  if ((*m1)->score < (*m2)->score) return(-1);
  return(0);
}


static int
match_score_comp(const void *p1, const void *p2) {
  MATCH_STATE **m1,**m2;

  m1 = (MATCH_STATE **) p1;
  m2 = (MATCH_STATE **) p2;
  if ((*m1)->score > (*m2)->score) return(1);
  if ((*m1)->score < (*m2)->score) return(-1);
  return(0);
}


#define MAX_MATCH_STATES 50


static void
find_path_prune() {
  int i;

  /*  for (i=match_list.num-1; i >= 0; i--) 
    if (match_list.list[i]->score) delete_match_state(i);
    return;*/


  qsort(match_list.list,match_list.num,sizeof(MATCH_STATE *),match_comp);
  for (i=match_list.num-1; i >= 1; i--) {
    if (match_list.list[i]->chrd != match_list.list[i-1]->chrd) continue;
    if (match_list.list[i]->set != match_list.list[i-1]->set) continue;
    if (match_list.list[i]->dat != match_list.list[i-1]->dat) continue;
    delete_match_state(i);
  }
  qsort(match_list.list,match_list.num,sizeof(MATCH_STATE *),match_score_comp);
  match_list.num = (match_list.num < MAX_MATCH_STATES) ? 
    match_list.num : MAX_MATCH_STATES;
}

static int
find_path_iter() {
  int i,d,j;

  /*  i = match_list.num-1;
  d = match_list.list[i]->dat;
  while(i-1 >= 0 && match_list.list[i-1]->dat == d) i--;*/
  for (j=match_list.num-1; j >= 0; j--) {
    fast_expand(match_list.list[j]);
    delete_match_state(j);
  }
  find_path_prune();
}

static int
match_done() {
  int i;

  for (i=0; i < match_list.num; i++) {
    if (match_list.list[i]->dat <  nod.num-1) return(0);
    if (chord_full(match_list.list[i]->chrd , match_list.list[i]->set) == 0) return(0);
    if (match_list.list[i]->chrd < last_chord_index) return(0);
  }
  return(1);
}


#define MAX_MATCH_ITER 10000
static int
find_path_fast() {
  MATCH_STATE *m;
  int i;

  match_list.num = match_buf.num = 0;
  m  = alloc_match_node();
  m->dat = -1;
  m->chrd = first_chord_index;
  m->set = 0;

  m->chrd = first_chord_index-1;
  m->set = 0xffff;

  m->pred = NULL;
  m->score = 0.;
  m->last_chrd = -1;
  m->last_time  = -1.;
  add_match_state(m);
  for (i=0; i < MAX_MATCH_ITER, match_done()==0; i++) {
    //    printf("iter = %d\n",i);
    find_path_iter();
    unused_prune();
    //        print_match_list();
  }
  if (i == MAX_MATCH_ITER) {
    printf("never found path to end\n");
    exit(0);
  }
}


static int
set_diff(unsigned int s1, unsigned int s2) {
  int i;
  unsigned mask;

  mask = 1;
  for (i=0; i < 16; i++, mask<<=1) 
    if ((s1&mask) != (s2&mask)) return(i);
  printf("%d %d no difference\n",s1,s2);
  exit(0);
}

static void
fast_traceback() {
  MATCH_STATE *cur;
  int member,j,p,s,i,n;
  char *tag,name[500];
    
  
  for (i=0; i < score.accompaniment.num; i++) {
    score.accompaniment.note[i].found = 0;
    score.accompaniment.note[i].start_time = 0;
    score.accompaniment.note[i].end_time = 0;
    score.accompaniment.note[i].vel = 0;
  }
  cur = match_list.list[0];
  n = aevent.list[last_chord_index].lo - aevent.list[first_chord_index].lo;
  printf("score = %f notes = %d\n",cur->score,n);
  while (cur->pred != NULL) {
    //      printf("chord = %d set = %o dat = %d score = %f\n",cur->chrd,cur->set,cur->dat,cur->score);
    //        printf("chord = %d set = %o dat = %d score = %f\n",cur->pred->chrd,cur->pred->set,cur->pred->dat,cur->pred->score);
    if (cur->dat == cur->pred->dat) {
      /*      if (cur->set == 1 && cur->pred->set == 1) {
	printf("ook\n");
	exit(0);
	}*/
      member = (cur->chrd == cur->pred->chrd) ? 
	set_diff(cur->set,cur->pred->set) : set_diff(cur->set,0);

      //      member = set_diff(cur->set,cur->pred->set);

      j = aevent.list[cur->chrd].lo + member;
      tag = score.accompaniment.note[j].observable_tag;
      printf("couldn't find data matching chord %s\n",tag);
    }
    else if (cur->set == cur->pred->set && cur->chrd == cur->pred->chrd) {
      j = aevent.list[cur->chrd].lo;
      tag = score.accompaniment.note[j].observable_tag;
      p = nod.list[cur->dat].pitch;
      num2name(p,name);
      printf("added a %s around chord %s\n",name,tag);
    }
    else {
      member = (cur->chrd == cur->pred->chrd) ? 
	set_diff(cur->set,cur->pred->set) : set_diff(cur->set,0);
      s = aevent.list[cur->chrd].lo + member;
      score.accompaniment.note[s].found = 1;
      score.accompaniment.note[s].start_time = nod.list[cur->dat].time;
      score.accompaniment.note[s].vel = nod.list[cur->dat].vel;
    }

    cur = cur->pred;
  }
}


typedef struct {
  int command;
  float secs;
  RATIONAL pos;
  int ped_on;
  int ped_off;
  int note_on;
} CORRESP;


#define MAX_CORRESP 10000

typedef struct {
  int num;
  CORRESP list[MAX_CORRESP];
} CORRESP_LIST;

static CORRESP_LIST corr;

static void
add_corresp(int command, float secs, RATIONAL pos, int ped_on, 
	    int ped_off, int note_on) {
  int i,j;

  if (corr.num >= MAX_CORRESP-1) {
    printf("out of room in add_corresp\n");
    exit(0);
  }
  for (i=0; i < corr.num; i++) if (corr.list[i].secs >= secs) break;
  for (j=corr.num; j > i; j--) corr.list[j] = corr.list[j-1];
  corr.list[i].command = command;
  corr.list[i].secs = secs;
  corr.list[i].pos = pos;
  corr.list[i].ped_on = ped_on;
  corr.list[i].note_on = note_on;
  corr.list[i].ped_off = ped_off;
  corr.num++;
}

static void
init_corresp() {
  corr.num = 0;
}

#define ROUND_LEVEL 64

static RATIONAL 
interp_corresp(float t) {
  int i,j;
  RATIONAL r1,r2,a1,a2,r,m2,m1;
  float t1,t2,s;

  for (i=0; i < corr.num; i++) {
    if (corr.list[i].command != NOTE_ON) continue;
    if (corr.list[i].secs >= t) break;
  }
  if (corr.list[i].secs <= t) return(corr.list[i].pos);  /* this is wrong */
  for (j = corr.num-1; j > 0; j--) {
    if (corr.list[j].command != NOTE_ON) continue;
    if (corr.list[j].secs <= t) break;
  }
  //  for (j=corr.num-20; j < corr.num; j++) printf("%f\n",corr.list[j].secs);
  if (corr.list[j].secs >= t) return(corr.list[j].pos);
  //  if (j != (i-1)) { printf("problem i = %d j = %d herexx\n",i,j); 
  //   printf("t = %ftime[i] = %f time[j] = %f\n",t,corr.list[i].secs,corr.list[j].secs); exit(0); }
  r1 = corr.list[j].pos;
  r2 = corr.list[i].pos;
  t1 = corr.list[j].secs;
  t2 = corr.list[i].secs;
  s = (t-t1) / (t2-t1);
  a1.den = a2.den = ROUND_LEVEL;
  a1.num = (int) (s*ROUND_LEVEL + .5);
  a2.num = ROUND_LEVEL-a1.num;
  m1 = mult_rat(a2,r1);
  m2 = mult_rat(a1,r2);
  r = add_rat(m1,m2);
  if (r.num < 0) {
    printf("r = %d/%d\n",r.num,r.den);
     printf("t1 = %f t = %f t2 = %f a1 = %d/%d a2 = %d/%d\n",t1,t,t2,a1.num,a1.den,a2.num,a2.den);
     printf("r1 = %d/%d  r2 = %d/%d\n",r1.num,r1.den,r2.num,r2.den);
     printf("m1  %d/%d m2 = %d/%d\n",m1.num,m1.den,m2.num,m2.den);
     printf("unfactored is %d/%d\n",m1.num*m2.den+m2.num*m1.den,m1.den*m2.den); 
    exit(0);
  }
  return(r);
}

static RATIONAL 
interp_accomp(float t) {
  int i,j,i1,i2;
  RATIONAL r1,r2,a1,a2,r,m2,m1;
  float t1,t2,s;

  for (i2=0; i2 < score.accompaniment.num; i2++) {
    if (score.accompaniment.note[i2].found == 0) continue;
    if (score.accompaniment.note[i2].start_time >= t) break;
  }
  if (i2 == score.accompaniment.num) 
    return(score.accompaniment.note[i2-1].wholerat);
  for (i1=score.accompaniment.num-1; i1 >= 0; i1--) {
    if (score.accompaniment.note[i1].found == 0) continue;
    if (score.accompaniment.note[i1].start_time < t) break;
    /* this has to be < or else could have i1 == i2 */
  }
  if (i1 == 0) return(score.accompaniment.note[0].wholerat);
  r1 = score.accompaniment.note[i1].wholerat;
  r2 = score.accompaniment.note[i2].wholerat;
  //  if (r1.num == r2.num && r1.den == r2.den) printf("zoikes\n");
  t1 = score.accompaniment.note[i1].start_time;
  t2 = score.accompaniment.note[i2].start_time;
  s = (t-t1) / (t2-t1);
  a1.den = a2.den = ROUND_LEVEL;
  a1.num = (int) (s*ROUND_LEVEL + .5);
  if (a1.num == 0) a1.num++;
  if (a1.num == ROUND_LEVEL) a1.num--;
  a2.num = ROUND_LEVEL-a1.num;
  m1 = mult_rat(a2,r1);
  m2 = mult_rat(a1,r2);
  r = add_rat(m1,m2);
  if (r.num  < 0) {
    printf("r = %d/%d\n",r.num,r.den);
     printf("t1 = %f t = %f t2 = %f a1 = %d/%d a2 = %d/%d\n",t1,t,t2,a1.num,a1.den,a2.num,a2.den);
     printf("r1 = %d/%d  r2 = %d/%d\n",r1.num,r1.den,r2.num,r2.den);
     printf("m1  %d/%d m2 = %d/%d\n",m1.num,m1.den,m2.num,m2.den);
     printf("unfactored is %d/%d\n",m1.num*m2.den+m2.num*m1.den,m1.den*m2.den); 
    exit(0);
  }
  return(r);
}


static void
find_ped_on_pos(float t) {
  int j,k;

  for (j = corr.num-1; j >= 0; j--) if (corr.list[j].secs <= t) break;
  /* pedal ons can move earlier with no essential change */
  //  printf("t = %f\n",t);
  for (k = j; k >=0; k--) if (corr.list[k].ped_on || corr.list[k].ped_off) break;
  //  printf("cor time = %f\n",corr.list[k].secs);
  if (!corr.list[k].ped_on) {
    //    printf("did it\n");
    corr.list[j].ped_on = 1;
  }
  //  return(corr.list[j].pos);
}


static int
corresp_cmp(CORRESP *c1, CORRESP *c2) {
  if (c1->secs > c2->secs) return(1);
  if (c1->secs < c2->secs) return(-1);
  return(0);
}


static void
write_pedals(SEQ_LIST *ped) {
  CORRESP *c;
  int i;
  char pedal_file[500],name[500],s[500];
  FILE *fp;
  RATIONAL r;
  float t;

  strcpy(pedal_file,scorename);
  strcat(pedal_file,".ped");
  fp = fopen(pedal_file,"w");
  for (i=0; i < ped->num; i++) {
    r = interp_accomp(ped->event[i].time);
    wholerat2string(r,name);
    t = (float)r.num/(float)r.den;
    wholerat2string(r,s);
    //    printf("pedal %d at %d/%d %s\n",ped->event[i].d2,r.num,r.den,name);
    if (ped->event[i].d2 == 0) fprintf(fp,"pedal_off\t%5d / %5d\t%s\n",r.num,r.den,s);  
    if (ped->event[i].d2 != 0) fprintf(fp,"pedal_on \t%5d / %5d\t%s\n",r.num,r.den,s);  
  }
  fclose(fp);
}



static void
compute_pedals(SEQ_LIST *seq) {
  int i;
  SEQ_LIST s;
  ACCOMPANIMENT_NOTE *a;
  RATIONAL r;

  init_corresp();
  for (i=0; i < score.accompaniment.num; i++) {
    a = &score.accompaniment.note[i];
    if (a->found == 0) continue;
    add_corresp(NOTE_ON,a->start_time,a->wholerat,0,0,1);
  }
  //  for (i=0; i < corr.num; i++) printf("%f\n",corr.list[i].secs);
  for (i=0; i < seq->num; i++) {
    if (seq->event[i].command != PEDAL_COM) continue;
    if (seq->event[i].d1 != 64)   continue;
    if (seq->event[i].d2 == 0) { /* pedal off:  the pedal off times can't be moved */ 
      r = interp_corresp(seq->event[i].time);
      add_corresp(PEDAL_COM,seq->event[i].time,r,0,1,0);
      //      fprintf(fp,"pedal_off\t%d / %d\n",r.num,r.den);
    }
  }
  // for (i=0; i < corr.num; i++) printf("%f\n",corr.list[i].secs);
  //  qsort(corr.list,corr.num,sizeof(CORRESP),corresp_cmp);
  for (i=0; i < seq->num; i++) {
    if (seq->event[i].command != PEDAL_COM) continue;
    if (seq->event[i].d1 != 64)   continue;
    if (seq->event[i].d2 != 0) { /* pedal on:  move earlier */ 
      find_ped_on_pos(seq->event[i].time);
      //      fprintf(fp,"i = %d pedal_on\t%d / %d\n",i,r.num,r.den);
      //      printf("%x %x %x\n",seq->event[i].command,seq->event[i].d1,seq->event[i].d2);
    }
  }
  //  write_pedals();
}
    

static void
select_pedals(SEQ_LIST *seq, SEQ_LIST *ped) {
  int i,state,j;  
  RATIONAL r;
  float t;

  ped->num = 0;
  state = 0; /* pedal off */
  for (i=0; i < seq->num; i++) {
    if (seq->event[i].command != PEDAL_COM) continue;
    if (seq->event[i].d1 != 64) continue;  /* 64 is right ped */
    if (seq->event[i].d2 > 0 && state == 1) seq->event[i].command =0;
    else if (seq->event[i].d2 == 0 && state == 0) seq->event[i].command =0;
    else {
      state = 1-state;
      ped->event[ped->num++] = seq->event[i];
      //      printf("%d %d %d \n",ped->num,seq->event[i].d2,state);
    }
  }
  for (i=1; i < ped->num; i++) {
    //    printf("i = %d\n",i);
    if (ped->event[i].d2 &&  ped->event[i-1].d2) { printf("this must not be\n"); exit(0); }
    if (ped->event[i].d2) continue;
    if (ped->event[i].time-ped->event[i-1].time >  .05) continue;
    /* a pedal on followed right away by a pedal off */
    //    printf("gap = %f\n",ped->event[i].time-ped->event[i-1].time);
    for (j=i+1; j < ped->num; j++) ped->event[j-2] = ped->event[j];
    ped->num -= 2;
    i--;
  }
  //    for (j=0; j < ped->num; j++)  printf("pedal %3d at %f (%f)\n",ped->event[j].d2,ped->event[j].time,ped->event[j].time-ped->event[j-1].time);
  //    exit(0);
}

    
    
    
    
  
  
  

static void
get_pedals(SEQ_LIST *seq) {
  int i,j,k,ped = 0;
  char pedal_file[500];
  FILE *fp;
  MIDI_BURST *b;
  SEQ_LIST pedal;

  pedal.event = (SEQ_EVENT *) malloc(sizeof(SEQ_EVENT)*MAX_SEQ);  // seq is local here
  select_pedals(seq,&pedal);
  write_pedals(&pedal);
  return;
  //  for (i=0; i < score.midi.num; i++) 
  //    score.midi.burst[i].pedal_on = score.midi.burst[i].pedal_off = 0;
  for (i=0; i < seq->num; i++) {
    if (seq->event[i].command != PEDAL_COM) continue;
    if (seq->event[i].d1 != 64) {
      //      printf("unknown pedal command here of %d %d at time %f\n",
      //	     seq->event[i].d1,seq->event[i].d2,seq->event[i].time);
      continue;
    }
    for (j=score.accompaniment.num-1; j > 0; j--) {
      if (score.accompaniment.note[j].found == 0) continue;
      if (score.accompaniment.note[j].start_time < seq->event[i].time) break;
    } /* j is index into accompaniment for pedal note */
    for (k=0; k < score.midi.num; k++) 
      if (rat_cmp(score.midi.burst[k].wholerat, score.accompaniment.note[j].wholerat) == 0)
	break;
    if (k == score.midi.num) {
      printf(" coldn't find match\n");
      exit(0);
    }
    if (seq->event[i].d2 != 0 && ped == 0) {
      ped = 1;
      //      printf("pedal on at %f %f %s j = %d\n",seq->event[i].time,
      //	     score.accompaniment.note[j].start_time,score.accompaniment.note[j].observable_tag,j);
      score.midi.burst[k].pedal_on = 1;
    }
    if (seq->event[i].d2 == 0) {
      //      printf("pedal off at %f %f %s j = %d\n",seq->event[i].time,
      //	     score.accompaniment.note[j].start_time,score.accompaniment.note[j].observable_tag,j);
      ped = 0;
      score.midi.burst[k].pedal_off = 1;
    }
  }
  strcpy(pedal_file,scorename);
  strcat(pedal_file,".ped");
  fp = fopen(pedal_file,"w");
  for (k=0; k < score.midi.num; k++) {
    b = &score.midi.burst[k];
    if (score.midi.burst[k].pedal_off)
      fprintf(fp,"pedal_off\t%s\t%d / %d\n",b->observable_tag,b->wholerat.num,b->wholerat.den);
    if (score.midi.burst[k].pedal_on) 
      fprintf(fp,"pedal_on \t%s\t%d / %d\n",b->observable_tag,b->wholerat.num,b->wholerat.den);
  }
  fclose(fp);
}

void
match_midi() {
  int i,s,success;
  MATCH_NODE_LIST list;
  MATCH_NODE *end,*start,*cur;
  char name[500],file[500];
  FILE *fp;
  float st,en;
  SEQ_LIST seq;

  seq.event = (SEQ_EVENT *) malloc(sizeof(SEQ_EVENT)*MAX_SEQ);  // seq is local here
  alloc_match_buf();
  read_seq_desc_file("bosendorfer/seq_description");

  strcpy(file,"bosendorfer/seq_files");
 /* printf("enter the file containing the seq files\n");

  printf("(.seq files should not have .seq suffix)\n");
  scanf("%s",file);  */
  /*  printf("filer = %s\n",file); */
  fp = fopen(file,"r");
  if (fp == NULL) {
    printf("couldn't open %s\n",file);
    exit(0);
  }
  make_acc_events();
  while (feof(fp) == 0) {
    fscanf(fp,"%s",name);
    if (feof(fp)) break;
    find_start_end_rat(name);
        printf("processing %s %d/%d %d/%d %d\n",name,start_time_excerpt_rat.num,start_time_excerpt_rat.den,end_time_excerpt_rat.num,end_time_excerpt_rat.den,first_chord_index);
	//		exit(0);
	//    set_excerpt_ends(start,end);
    /*for (i=first_score_index-5; i < first_score_index+5; i++)
printf("%d %f\n",i,score.accompaniment.note[i].meas); */
    read_no_data(name);
    //    adjust_graph(start);
    /*    for (i=0; i < excerpt_start->next.num; i++)
printf("index = %d num = %d\n",excerpt_start->next.arc[i].index,excerpt_start->next.arc[i].node->next.num);
       printf("%d %d\n",excerpt_start->lo_di,excerpt_start->hi_di);
    printf("%d %d\n",excerpt_start->lo_scr,excerpt_start->hi_scr);
    printf("%d %d\n",start->lo_scr,start->hi_scr);
exit(0); */
    success = find_path_fast();
    fast_traceback();
    /*    score_warp_node(&excerpt_end->warp[nod.num-1]);  */
    /*printf("%d %d %d\n",excerpt_end->lo_di, nod.num-1,excerpt_end->hi_di);
printf("%d %d %d\n",excerpt_start->lo_di, 0,excerpt_start->hi_di); 
exit(0); */
    /*    traceback(&excerpt_end->warp[nod.num-1],0);  */
    /*if (success) */
    print_record(name);
    read_seq_file(name,&seq);
        get_pedals(&seq);
	//    compute_pedals(&seq);
  }
  write_velocities_from_example("bosendorfer/seq_files");
}



static float
velocity_dist(int i, int j) {
  float pd,td;
  RATIONAL d;

  pd = score.accompaniment.note[i].pitch - score.accompaniment.note[j].pitch;
  d = sub_rat(score.accompaniment.note[i].wholerat,score.accompaniment.note[j].wholerat);
  td = d.num/(float)d.den;
  if (td > .5) return(HUGE_VAL);
  return(td + pd/10.);
}


#define PRIOR_VELOCITY_SMOOTHING 2.

void
write_velocities_from_example(char *infile) {
  int i,j,v,vel[50000],count[50000];
  float smooth[50000],sum_dist[50000];
  FILE *fp,*fpi;
  char vel_file[500], match[500],tag[500];

  for (i=0; i < score.accompaniment.num; i++) vel[i] = count[i] = 0;
  for (i=0; i < score.accompaniment.num; i++) smooth[i] = sum_dist[i] = 0;
  //  printf("enter the file containing the match files (don't use .mch in file)\n");
  //  scanf("%s",infile);
  fpi = fopen(infile,"r");
  if (fpi == NULL) {
    printf("couldn't open %s\n",infile);
    exit(0);
  }
  while (!feof(fpi)) {
    fscanf(fpi,"%s\n",match);
    printf("file is %s\n",match);
    read_match(match);
    for (i=0; i < score.accompaniment.num; i++) {
      for (j=i+1; j < score.accompaniment.num; j++) {
	if (velocity_dist(i,j) == HUGE_VAL) break;
	sum_dist[i] += velocity_dist(i,j);
	smooth[i] += score.accompaniment.note[j].vel;
      }
      for (j=i-1; j >= 0; j--) {
	if (velocity_dist(i,j) == HUGE_VAL) break;
	sum_dist[i] += velocity_dist(i,j);
	smooth[i] += score.accompaniment.note[j].vel;
      }
      /* need to come back to this and implement smoothing */

      if (score.accompaniment.note[i].found) {
	vel[i] += score.accompaniment.note[i].vel;
	count[i]++;
      }
    }
  }
  strcpy(vel_file,scorename);
  strcat(vel_file,".vel");
  printf("creating %s\n",vel_file);
  fp = fopen(vel_file,"w");
  for (i=0; i < score.accompaniment.num; i++) {
    v = (count[i]) ? vel[i]/count[i] : 20;
    if (count[i] == 0) printf("%s never observed\n",score.accompaniment.note[i].observable_tag);
    fprintf(fp, "%s\t%d\n",score.accompaniment.note[i].observable_tag,v);
  }
  fclose(fp);
}


void 
midi_change_program(int channel, int program) {
  unsigned char buff[500];

  buff[0] = 0xc0 + channel;
  buff[1] = program;
  play_midi_chunk(buff,2);
}



void
windows_play_seq(SEQ_LIST *s) {
  int i,ms;
  unsigned char buff[3];

  
  init_midi();
  for (i=0; i < s->num; i++) {
    buff[0] = s->event[i].command;
    buff[1] = s->event[i].d1;
    buff[2] = s->event[i].d2;
    play_midi_chunk(buff,3);
    if (i == s->num-1) break;
    ms = (int) (1000*(s->event[i+1].time-s->event[i].time));
    wait_ms(ms);
  }
  end_midi();
}











#define CLOCKS_PER_BEAT 256
#define MS_PER_BEAT 500000  /* this is midi default tempo in microsecs  per quarter */
 

write_midi_number(FILE *fp, unsigned int n, int length) { 
//write out the length byte number n */
  unsigned int mask,t;
  unsigned char c[100];
  int i;


  printf("n = %d\n",n);
  mask = 0xff;
  for (i=0; i < length; i++) {
    c[i] = n&mask;
    n >>= 8;
  }
  for (i=0; i < length; i++) {
    fputc(c[length-1-i],fp);
      printf("%d\n",c[length-1-i]);
  }
}

write_midi_string(FILE *fp, char *s) { 
  int i;

  for (i=0; i < strlen(s); i++) fputc(s[i],fp);
}

#define MAX_VAR_LEN 4

static int
get_var_length_rep(unsigned int n, unsigned char *c, int *num) {
  unsigned int sum = 0,mask;
  unsigned char b[100];
  int count,i;

 
  mask = 0x7f;
  for (i = 0; i < MAX_VAR_LEN; i++) {
    b[i] = mask&n;
    n >>= 7;
    if (n == 0) break;
  }
  if (i == MAX_VAR_LEN) {
    printf("badd stuff here\n");
    exit(0);
  }
  *num =i+1;
  for (i=0; i < *num; i++) { 
    c[i] = b[*num-1-i];
    if (i < *num-1) c[i] |= 0x80;
  }
}

static void
write_var_length(FILE *fp, int n) {
  unsigned int num;
  unsigned char c[100];
  int i;

  get_var_length_rep(n,c,&num);
  printf("n = %d num in var length = %d\n",n,num);
  for (i=0; i < num; i++) fputc(c[i],fp);
}



static int
get_var_length(int n) {
  unsigned int num;
  unsigned char c[100];

  
  get_var_length_rep(n,c,&num);
  return(num);
}



void
create_midi_from_seq(char *name) {
  FILE *fp;
  int i;
  float dsecs;
  unsigned int dt,total;
  

  /*  printf("enter the name of the .seq file (no .seq extension)\n");
      scanf("%s",name);*/
  read_no_data(name);  /* reads to global "seq" */
  strcat(name,".mid");
  fp = fopen(name,"w");
  write_midi_string(fp,"MThd");
  write_midi_number(fp,6,4);  /* 6 bytes of data in midi header */
  write_midi_number(fp,0,2);  /* midi format 0 (one tracks) */
  write_midi_number(fp,1,2);  /* there will be 1 tracks */
  write_midi_number(fp,CLOCKS_PER_BEAT,2);  
  write_midi_string(fp,"MTrk");  
  

  total = 4;  /* three bytes at end of track */
  for (i=0; i < seq.num; i++) {
    dsecs = (i == 0) ? 0 : seq.event[i].time - seq.event[i-1].time;
	dt = (int) ((dsecs * 1000000 * CLOCKS_PER_BEAT) / MS_PER_BEAT);
	 if (dt < 0)
	printf("bad thing here\n");
	total += get_var_length(dt);
    total += 3;
  }
  write_midi_number(fp,total,4);  /* bytes in track */
  printf("%d bytes in track\n",total);
  for (i=0; i < seq.num; i++) {
    dsecs = (i == 0) ? 0 : seq.event[i].time - seq.event[i-1].time;
    dt = (int) ((dsecs * 1000000 * CLOCKS_PER_BEAT) / MS_PER_BEAT);
    write_var_length(fp,dt);
    write_midi_number(fp,seq.event[i].command,1); 
    write_midi_number(fp,seq.event[i].d1,1); 
    write_midi_number(fp,seq.event[i].d2,1); 
    //    printf("%f %x %d %d\n",seq.event[i].time,seq.event[i].command,seq.event[i].d1,seq.event[i].d2);
  }
  write_midi_number(fp,0,1); /* delta time */
  write_midi_number(fp,0xff,1); /* end of track */
  write_midi_number(fp,0x2f,1); 
  write_midi_number(fp,0x00,1); 


  //	dt = read_var_length(mfp);
  //	dsecs = (float) (tempo*dt) / (float) (1000000*/*div*/clocks_per_beat);     

  
  /*  fputc(0,fp);
      fputc(0,fp);  */
  fclose(fp);
}


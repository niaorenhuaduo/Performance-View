#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "share.c"
#include "global.h"
#include "belief.h"
#include "matrix_util.h"
#include "joint_norm.h"
#include "midi.h"
#include "linux.h"
#include "gui.h"
#include "class.h"
#include "new_score.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NOTE_ON   0x90
#define NOTE_OFF  0x80
#define MAX_MIDI_VOL 128
#define MIDI_PITCH_NUM 128 // total # of midi pitches
#define MAX_SUCC 8

#define MAX_KNOTS 20


typedef struct {
  int num;
  int vol[MAX_KNOTS];
} VOLUME_KNOTS;

typedef struct {
  int num;
  int list[MAX_SUCC];
} NEXT_LIST;


#define CONTROL_LENGTH 5

typedef struct {
  char control[CONTROL_LENGTH];
  int (*func)();
} BOOL_FUNC;

#define MAX_FUNCS 4

typedef struct {
  int num;
  void (*func[MAX_FUNCS])();
} ACTION;
  

#define DESC_LEN 15

typedef struct {
  NEXT_LIST next;
  ACTION action;
  BOOL_FUNC legal;  /* if legal.func field is null, node consumes no data */
  char  desc[DESC_LEN];
} SCORE_NODE;


typedef struct {
  int head;
  int tail;
} MODEL_STAT;

#define MAX_END 5

typedef struct {
  int num;
  int list[MAX_END];
} END_LIST;


typedef struct {
  int prev;
  int state;
  char ch;
} HYPO;

#define MAX_HYPS 400000

typedef struct {
  int num;
  HYPO *hyp;
} ACTLIST;

#define MAX_CHARS 100000

typedef struct {
  int num;
  char buff[MAX_CHARS];
} CHAR_BUFF;

#define SOLO_VOICE 0
#define MAX_SCORE_NODES 5000
#define MAX_VOICE 10
#define MAX_CHORD 100


typedef struct {
  int num;  /* total number of notes */
  float start;
  float end;
  RATIONAL whole_st;
  RATIONAL whole_end;
  unsigned char vol;  /*  this should be different for each chord mem */
  int pitch[MAX_CHORD];
} CHORD;

typedef struct {
  float start_time; /* starting time in measures of cresc, decresc*/
  float start_dyn;  /* starting dynamic in [0,1] */
  float end_time;   /* ending time*/
  float end_dyn;    /* ending dynamic */
} DYNAMIC_RAMP;


typedef struct {
  CHORD chord;
  DYNAMIC_RAMP dyn_ramp;
  int cur_vol;
  int index;
}  VOICE;

#define MAX_OCT 12
#define HI_PITCH 12*MAX_OCT

typedef struct {  /* boolean values for held notes */
  char pitch[HI_PITCH];
} BOOL_CHORD;

typedef struct {
  int num;
  /*  int index[MAX_VOICE]; */
  CHORD chord[MAX_VOICE];  /* the pending chord */
  VOICE voice[MAX_VOICE];
  BOOL_CHORD bool_chord;  /* all currently held notes */
} STAFF;  /* maybe this should be INSTR */


typedef struct {
  int num;
  float val[MAX_KNOTS];
} KNOT_LIST;

typedef struct {
  float midi_vol;  /* must be 0 <= x <= 128 */
  float pitch; /* midi pitch number */
  float zero_one_vol;  /* volume in 0,1 range */
} KNOT;

typedef struct {
  KNOT_LIST zero_one;
  KNOT_LIST midi_pitch;
  float tab[MAX_KNOTS][MAX_KNOTS]; /* midi_pitch X zero_one_vol */
} VOL_EQ;  /* volume equalizing struct */


#define MAX_STRING 100
#define MAX_COMMAND_ARGS 5

typedef struct {
  float time;
  RATIONAL rat;
  char command[MAX_STRING];
  float arg[MAX_COMMAND_ARGS];
} ACCOM_COMMAND;


#define MAX_COMMANDS 500

typedef struct {
  int num;
  ACCOM_COMMAND list[MAX_COMMANDS];
} COMMAND_LIST;

#define MAX_FLOAT_VAL 200
#define WHAT_LEN 20

typedef struct {
  int num;
  float val[MAX_FLOAT_VAL];
  RATIONAL rat[MAX_FLOAT_VAL];
  char  what[MAX_FLOAT_VAL][WHAT_LEN];
} FLOAT_LIST;







static SCORE_NODE node_buff[MAX_SCORE_NODES];
static int num_states;
static CHAR_BUFF cb;
static int start_state;
static int end_state;
static int cur_char;
static int numerator;
static int digits;
static int denominator;
static int tying; /* boolean */
static int vol_override;
static int number;
static float float_number;
static float quotient;
static float note_length;
static float meas_pos;
static int midi_pitch;
static STAFF staff,old_staff;
static int cur_line;
static int pedal_on;
static int cur_voice;   
static int include_solo_part; /* boolean: include solo part in accomp notes */
static float note_value2meas_value;
static int counts_per_meas;
static int note_gets_one_count;
static int solo_note_is_cue;
static int solo_note_is_trigger;
static float recent_dyn;
static VOLUME_KNOTS vol_knots;
static VOL_EQ vol_eq;
static COMMAND_LIST comm;  /* the accompaniment command list */
static int barlines;
static int trill_lo;  /* lower pitch of trill 0 if not trill */
static int trill_hi;  /* same */
static float cur_meas_size;
static FLOAT_LIST accom_cue_list;
static FLOAT_LIST wait_list;
static RATIONAL rat_meas_pos;
static RATIONAL rat_meas_start;
static RATIONAL rat_recent;
static RATIONAL time_signature;
static int rhythmic_rest;
static int which_parts;

static int
xgcdx(int n1, int n2) {
  int temp;

  if (n2 == 0) return(n1);
  return(xgcdx(n2,n1%n2));
}

int
gcd(int n1, int n2) {
  int temp;

  if (n2 > n1) { temp = n1; n1 = n2; n2 = temp; } /* n1 > n2 */
  return(xgcdx(n1,n2));
}


float 
rat2f(RATIONAL r) {
  if (r.den == 0) 
    printf("can't divide by 0\n");
  return( (float) r.num / (float) r.den);
}


RATIONAL
add_rat(RATIONAL r1,RATIONAL r2) {
  RATIONAL temp;
  int com;

  temp.num = r1.num*r2.den + r2.num*r1.den;
  temp.den = r1.den*r2.den;
  com = gcd(temp.num,temp.den);
  temp.num /= com;
  temp.den /= com;
  return(temp);
}

int
rat_cmp(RATIONAL r1,RATIONAL r2) {  /* 1 if r1 > r2;  0 if equal;  -1 ow*/
  RATIONAL temp;
  int com,g;

  g = gcd(r1.den,r2.den);
  return(r1.num*(r2.den/g) - (r1.den/g)*r2.num);
  //  return(r1.num*r2.den - r1.den*r2.num);
}

RATIONAL
sub_rat(RATIONAL r1,RATIONAL r2) {
  RATIONAL temp;
  int com;

  temp.num = r1.num*r2.den - r2.num*r1.den;
  temp.den = r1.den*r2.den;
  com = gcd(temp.num,temp.den);
  temp.num /= com;
  temp.den /= com;
  if (temp.den < 0) { temp.den *= -1; temp.num *= -1; }
  return(temp);
}

RATIONAL
mult_rat(RATIONAL r1,RATIONAL r2) {
  RATIONAL temp;
  int com;

  temp.num = r1.num*r2.num;
  temp.den = r1.den*r2.den;
  com = gcd(temp.num,temp.den);
  temp.num /= com;
  temp.den /= com;
  return(temp);
}

RATIONAL
div_rat(RATIONAL r1,RATIONAL r2) {
  RATIONAL temp;
  int com;

  temp.num = r1.num*r2.den;
  temp.den = r1.den*r2.num;
  com = gcd(temp.num,temp.den);
  temp.num /= com;
  temp.den /= com;
  return(temp);
}


static void
add_float_list(RATIONAL rat, float x, char *comment, FLOAT_LIST *f) {

  if (f->num == MAX_FLOAT_VAL) {
    printf("out of room in add_float_list\n");
    exit(0);
  }
  strcpy(f->what[f->num],comment);
  f->val[(f->num)] = x;
  f->rat[(f->num)] = rat;
  f->num++;
}


static void set_volume_knots(void) {
  vol_knots.vol[0] = 1;
  vol_knots.vol[1] = 12;
  vol_knots.vol[2] = 24;
  vol_knots.vol[3] = 46;
  vol_knots.vol[4] = 60;
  vol_knots.vol[5] = 80;
  vol_knots.vol[6] = 96;
  vol_knots.vol[7] = 120;
  vol_knots.vol[8] = 127;
  vol_knots.num = 9;
}


#define VOL_EQ_DATA "vol_eq.dat"


static void 
read_vol_eq() {
  int i,j;
  FILE *fp; 
  char string[1000],c;

  fp = fopen(VOL_EQ_DATA, "r");
  if (fp == NULL) {
    printf("couldn't find %s\n",VOL_EQ_DATA);
    exit(0);
  }
  do {
    fscanf(fp,"%c",&c);
  } while (c != '\n');
  fscanf(fp,"%d x %d",&vol_eq.midi_pitch.num,&vol_eq.zero_one.num);
  /*  printf("table is %d x %d\n",vol_eq.midi_pitch.num,vol_eq.zero_one.num);*/
  for (i=0; i < vol_eq.zero_one.num; i++) {
    fscanf(fp,"%f",vol_eq.zero_one.val+i);
    /*    printf("%f\n",vol_eq.zero_one.val[i]);*/
  }
  for (j=0; j < vol_eq.midi_pitch.num; j++) {
    fscanf(fp,"%f",vol_eq.midi_pitch.val+j);
    for (i=0; i < vol_eq.zero_one.num; i++) 
      fscanf(fp,"%f",&vol_eq.tab[j][i]);
  }
  /*   for (j=0; j < vol_eq.midi_pitch.num; j++) {
    for (i=0; i < vol_eq.zero_one.num; i++) {
      printf("%f\t",vol_eq.tab[j][i]);
    }
    printf("\n");
  } 
  exit(0); */
}


static int 
zo2vel(float zo, float pitch) { 
  int i,j;
  float ti,tj,ret;

  /*printf("zo = %f pitch = %f\n",zo,pitch);*/
  for (i=0; i < vol_eq.midi_pitch.num-1; i++)
    if (pitch <= vol_eq.midi_pitch.val[i+1]) break;
  /*     printf("i = %d\n",i);  */
  ti = (pitch - vol_eq.midi_pitch.val[i])
    / (vol_eq.midi_pitch.val[i+1] - vol_eq.midi_pitch.val[i]);
  for (j=0; j < vol_eq.zero_one.num-1; j++)
    if (zo <= vol_eq.zero_one.val[j+1]) break;
  /*  printf("j = %d\n",j); */
  tj = (zo - vol_eq.zero_one.val[j])
    / (vol_eq.zero_one.val[j+1] - vol_eq.zero_one.val[j]);

  /*printf("ti = %f tj = %f\n",ti,tj); */
  /*printf("%f %f %f %f\n", vol_eq.tab[i][j], vol_eq.tab[i+1][j],vol_eq.tab[i][j+1],vol_eq.tab[i+1][j+1]);  */
 ret = vol_eq.tab[i][j] * (1-ti)*(1-tj)
   + vol_eq.tab[i+1][j] * (ti)*(1-tj)
   + vol_eq.tab[i][j+1] * (1-ti)*tj
   + vol_eq.tab[i+1][j+1] * ti*tj;
   /*       printf("returning (int) %f\n",ret);   */
   return((int)ret);
}


static int
f2midi_vol(float x) {
  int i,ret;
  float p,q,ii,val;


  if (x > 1.) x = 1;
  if (x < 0.) x = 0;
  if (x > 1. || x < 0) {
    printf("out of range in f2midi_vol (x = %f)\n",x);
    exit(0);
  }
  if (x == 1.) return(127/*vol_knots.vol[vol_knots.num-1]*/);
  ii = (vol_knots.num-1)*x;
  i = ii;
  p = (ii-i);
  q = 1-p;
  val = vol_knots.vol[i]*q + vol_knots.vol[i+1]*p;
  ret = (int) val;
  /*      printf("x = %f volum = %d\n",x,ret);  */
  return(ret);
}



init_end_list(e)
END_LIST *e;
{
  e->num = 0;
}

add_end_list(e,i)
END_LIST *e;
int i; {
  e->list[e->num] = i;
  (e->num)++;
  if (e->num > MAX_END) {
    printf("end list full\n");
    exit(0);
  }
}



static int claim_state(act,leg,string,desc) 
void (*act)();
int (*leg)();
char *string; 
char *desc; {
    int i;
    
    i = num_states;
    num_states++;
    if (num_states == MAX_SCORE_NODES) {
      printf("out of states to allocate\n");
      exit(0);
    }
    node_buff[i].next.num = 0;
    node_buff[i].action.func[0] = act;
    node_buff[i].action.num = 1;
    node_buff[i].legal.func = leg;
    strcpy(node_buff[i].desc,desc);
    strcpy(node_buff[i].legal.control,string);
    return(i);
}

static void add_act(i,f)
int i;  /* number of node */
void (*f)(); {
  int n;
  ACTION *a;
  
  a = &node_buff[i].action;
  n = a->num;
  if (n == MAX_FUNCS) {
    printf("out of room in add_act()\n");
    exit(0);
  }
  (a->func)[n] = f;
  (a->num)++;
}

static void add_hyp(a,h)
ACTLIST *a;
HYPO h; {
  int n;

  n = a->num;
  if (n == MAX_HYPS) {
    printf("out of room in add_hyp()\n");
    exit(0);
  }
  (a->hyp)[n] = h;
  (a->num)++;
}

static void add_succ(s,i)
int s;
int i; {
    int n;

    n = node_buff[s].next.num;
    node_buff[s].next.num++;
    if (n >= MAX_SUCC) printf("out of room in add_succ()\n");
    node_buff[s].next.list[n] = i;
}

void
connect_end(e,s)
END_LIST e;
int s; {
  int i;

  for (i=0; i < e.num; i++) add_succ(e.list[i],s);
}

static int is_char(c,s)
char c,*s; {
  return(c == *s);
}


static int is_solf_or_rest(c,s)
char c,*s; {
  return((c >= 'a' && c <= 'g') || c == '~');
}

static int is_white(c,s)
char c,*s; {
  return(c == ' ');
}

static int is_digit(c,s)
char c,*s; {
  return(c >= '0' && c <= '9');
}  

static int not_cr(c,s)
char c,*s; {
  return(c != '\n');
}

static int any_char(c,s)
char c,*s; {
  return(1);
}


static int is_acc(c)
char c; {
  return(c == 'b' || c == '#' || c == 'n' || c == 's' || c == 'f' || c == '!');
  /* the ! is for a rhythmic rest */
}

static int is_alpha(c)
char c; {
  return((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}


static void num1_act(c)
char c; {
  digits = 1;
  number = c-'0';
}

static void 
num2_act(c)
char c; {
  digits++;
  number *= 10;
  number += c-'0';
}

static void 
no_act(c)
char c; {
}

static void


make_string_model(top,bot,str)
int *top,*bot; 
char *str; {
  int i,cur,last;
  char bf[2];

  bf[1] = 0;
  *top = claim_state(no_act,0,"",str);
  last = *top;
  for (i=0; i < strlen(str); i++) {
    bf[0] = str[i];
    cur = claim_state(no_act,is_char,bf,bf);
    add_succ(last,cur);
    last = cur;
  }
  *bot = cur;
}


static void
make_white_model(top,bot)
  int *top,*bot; {
  int w;

  init_end_list(bot);
  w = claim_state(no_act,is_white,"","non opt white");
  add_succ(w,w);
  *bot = *top = w;
}

static void
make_opt_white_model(top,bot)  /* optional white space */
  int *top,*bot; {
  int w,s,e;

  w = claim_state(no_act,is_white,"","opt white");
  s = claim_state(no_act,0,"","opt white start");
  e = claim_state(no_act,0,"","opt white end");
  add_succ(w,w);
  add_succ(s,w);
  add_succ(w,e);
  add_succ(s,e);
  *bot = e;
  *top = s;
}

#define MAX_STR_LEN 100

static int any_string_len;
static char sbuff[MAX_STR_LEN];

start_string_act(char c) {
  int i;

  any_string_len = 1;
  for (i=0; i < MAX_STR_LEN; i++) sbuff[i] = 0;
  sbuff[0] = c;
}

add_new_char(char c) {
  sbuff[any_string_len++] = c;
  if (any_string_len == MAX_STR_LEN) {
    printf("can't accomodate the long of a string\n");
    exit(0);
  }
}

static void
make_any_string_model(int *top, int *bot) {  /* must be at least 2 chars */
  int s,e;

  s = claim_state(start_string_act,is_alpha,"","string* start");
  e = claim_state(add_new_char,is_alpha,"","string* end");
  add_succ(s,e);
  add_succ(e,e);
  *top = s;
  *bot = e;
}     



void
new_add_event(int tag, int c, int n, float t, int v, RATIONAL rat) {
 float r;

     r = (float) rat.num / (float) rat.den; 
      if (fabs(r-t) > .001) {
    printf("rat and float don't agree in new_add_event\n");
    exit(0);
    } 
 
 /* t = (float) rat.num / (float) rat.den;   /* phasing htis out */
 


  event.list[event.num].tag = tag;
  event.list[event.num].command = c;
  event.list[event.num].notenum = n;
  event.list[event.num].meas= t;
    /*  event.list[event.num].meas= r;*/
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

/*#ifdef MIREX
void
#else
static void
#endif*/
void
add_solo_event(int tag, int c, int n, float t, int v, RATIONAL rat, int trill) { 
  float r;

     r = (float) rat.num / (float) rat.den; 
      if (fabs(r-t) > .001) {
    printf("rat and float don't agree in new_add_sevent\n");
    exit(0);
    } 
 
 /* t = (float) rat.num / (float) rat.den;   /* phasing htis out */
 


  sevent.list[sevent.num].tag = tag;
  sevent.list[sevent.num].command = c;
  sevent.list[sevent.num].notenum = n;
  sevent.list[sevent.num].meas= t;
    /*  sevent.list[sevent.num].meas= r;*/
  sevent.list[sevent.num].volume= v;
  sevent.list[sevent.num].wholerat = rat;
  sevent.list[sevent.num].trill = trill;


  /*  printf("tag is %s\n",sevent.list[sevent.num].observable_tag);*/
  sevent.num++;
  /*  if (fabs(t-6.333) < .01 && n == 50) {
      printf("adding sevent %d %d %f\n",n,v,t); 
      }*/
      
  if (sevent.num == MAX_EVENTS) {
    printf("out of room in sevent list\n"); exit(0);   
  }
}






static void
flush_pedal() {
  int i,v,j;

  for (i = 0; i < HI_PITCH; i++) {
    if (staff.bool_chord.pitch[i]) {
      new_add_event(MIDI_COM,NOTE_OFF,i,meas_pos,0,rat_meas_pos);
      /*      printf("note = %d meas_pos = %f\n",i,meas_pos);*/
      staff.bool_chord.pitch[i] = 0;
    }
  }
}


void
test_wholerat2string(RATIONAL whole, char *s) {
  int i;
  RATIONAL frac;
  char n[500],d[500],w[500];

  for (i=1; i < score.measdex.num; i++) {
    printf("%d %d/%d\n",i,score.measdex.measure[i].wholerat.num,score.measdex.measure[i].wholerat.den);
    if ( (rat_cmp(whole,score.measdex.measure[i].wholerat) >= 0)  && (rat_cmp(whole,score.measdex.measure[i+1].wholerat) < 0)) break;
  }
  frac = sub_rat(whole,score.measdex.measure[i].wholerat);
  sprintf(n,"%d",frac.num);
  sprintf(d,"%d",frac.den);
  sprintf(w,"%d",i);
  strcpy(s,w);
  /*  if (frac.num == 0) return;*/
  strcat(s,"+");
  strcat(s,n);
  strcat(s,"/");
  strcat(s,d);
  printf("%d/%d\n",whole.num,whole.den);
  printf("%s\n",s);
  exit(0);
}


void
wholerat2string(RATIONAL whole, char *s) {
  int i;
  RATIONAL frac;
  char n[500],d[500],w[500];

  if  (rat_cmp(whole,score.measdex.measure[1].wholerat) < 0){
    frac = sub_rat(score.measdex.measure[1].wholerat,whole);
    sprintf(n,"%d",frac.num);
    sprintf(d,"%d",frac.den);
    strcpy(s,n);
    strcat(s,"/");
    strcat(s,d);
    strcat(s,"-pickup");
    return;
  }

  for (i=1; i < score.measdex.num; i++) 
    if ( (rat_cmp(whole,score.measdex.measure[i].wholerat) >= 0)  && (rat_cmp(whole,score.measdex.measure[i+1].wholerat) < 0)) break;
  frac = sub_rat(whole,score.measdex.measure[i].wholerat);
  sprintf(n,"%d",frac.num);
  sprintf(d,"%d",frac.den);
  //  sprintf(w,"%d",i);
  sprintf(w,"%d",score.measdex.measure[i].meas);
  strcpy(s,w);
  /*  if (frac.num == 0) return;*/
  strcat(s,"+");
  strcat(s,n);
  strcat(s,"/");
  strcat(s,d);
  //  printf("visit = %d\n",score.measdex.measure[i].visit);
  if (score.measdex.measure[i].visit == 1) return;
  sprintf(w,"(%d)",score.measdex.measure[i].visit);
  strcat(s,w);
}


void
wholerat2string_old(RATIONAL whole, char *s) {
  int i;
  RATIONAL frac;
  char n[500],d[500],w[500];


  if  (rat_cmp(whole,score.measdex.measure[1].wholerat) < 0){
    frac = sub_rat(score.measdex.measure[1].wholerat,whole);
    sprintf(n,"%d",frac.num);
    sprintf(d,"%d",frac.den);
    strcpy(s,n);
    strcat(s,"/");
    strcat(s,d);
    strcat(s,"-pickup");
    return;
  }

  for (i=1; i < score.measdex.num; i++) 
    if ( (rat_cmp(whole,score.measdex.measure[i].wholerat) >= 0)  && (rat_cmp(whole,score.measdex.measure[i+1].wholerat) < 0)) break;
  frac = sub_rat(whole,score.measdex.measure[i].wholerat);
  sprintf(n,"%d",frac.num);
  sprintf(d,"%d",frac.den);
  sprintf(w,"%d",i);
  strcpy(s,w);
  /*  if (frac.num == 0) return;*/
  strcat(s,"+");
  strcat(s,n);
  strcat(s,"/");
  strcat(s,d);
}

RATIONAL
wholerat2measrat(RATIONAL whole) {
  int i;
  RATIONAL frac;
  char n[500],d[500],w[500];

  if (rat_cmp(whole,score.measdex.pickup) < 0) {
    frac = score.measdex.pickup;
    frac.num = -frac.num;
    return(frac);
  }
  for (i=1; i < score.measdex.num; i++) 
    if ( (rat_cmp(whole,score.measdex.measure[i].wholerat) >= 0)  && (rat_cmp(whole,score.measdex.measure[i+1].wholerat) < 0)) break;
  frac = sub_rat(whole,score.measdex.measure[i].wholerat);
  return(frac);
}

int
wholerat2measnum(RATIONAL whole) {
  int i;
  RATIONAL frac;
  char n[500],d[500],w[500];

  if (rat_cmp(whole,score.measdex.pickup) < 0) return(1);
  for (i=1; i < score.measdex.num; i++) 
    if ( (rat_cmp(whole,score.measdex.measure[i].wholerat) >= 0)  && (rat_cmp(whole,score.measdex.measure[i+1].wholerat) < 0)) break;
  return(i);
}





static void
observable_tag(int pitch, RATIONAL rat, char *tag) {
  char name[500],s[500];

  num2name(pitch,name); 
  wholerat2string(rat, s);
  /*  sprintf(tag,"%5.3f",time);*/
  strcpy(tag,s);
  strcat(tag,"_");
  strcat(tag,name);
}  



int
is_a_rest(int d) {
  return(d == RESTNUM || d == RHYTHMIC_RESTNUM);
}


int
is_solo_rest(int i) {
  int d;
#ifdef POLYPHONIC_INPUT_EXPERIMENT
  return(score.solo.note[i].snd_notes.num == 0 && score.solo.note[i].rhythmic_rest == 0 );
#else
  d = score.solo.note[i].num;
  return(d == RESTNUM || d == RHYTHMIC_RESTNUM);
#endif
}
     
static void
add_solo_note() {
  SOLO_NOTE *sp;
  CHORD *ch;
  int p;


  if (cur_voice != SOLO_VOICE) return;
  ch = &(staff.voice[SOLO_VOICE].chord);
  /*printf("adding note %d\n",ch->pitch[0]); */
  if (ch->num == 0) {
    return; /* nothing in chord */
  }


  sp = &(score.solo.note[score.solo.num++]);
  if (score.solo.num == MAX_SOLO_NOTES) {
    printf("out of room in solo notes\n");
    exit(0);
  }
  p = ch->pitch[0];   /* only one possible pitch */
  /*  printf("note = %d cue = %d pitch = %d\n",score.solo.num,solo_note_is_cue,p);  */
  //  if (p != RESTNUM)
  if (is_a_rest(p) == 0)
    if (p < LOWEST_NOTE || p > HIGHEST_NOTE) {
      printf("illegal pitch %d voice = %d at score pos %d\n",p,cur_voice,cur_char);
      exit(0);
    }
  //  if (trill_lo == 0)  sp->num = is_a_rest(p) ? RESTNUM : p;
  if (trill_lo == 0)  sp->num = p;  /* could be RHTYMIC_RESTNUM too */
  else {sp->num = trill_lo; sp->trill = trill_hi; }
  sp->time = ch->start;
  sp->wholerat = ch->whole_st; /*rat_meas_pos; */
  sp->length = ch->end - ch->start;
  sp->cue = solo_note_is_cue;
  sp->trigger = solo_note_is_trigger;
  sp->meas_size = cur_meas_size;

  /* chage before nips */
  /* sp->obs_var = (p == RESTNUM) ? 1000. :  1;   /* in tokens */
  sp->obs_var = (p == RESTNUM) ? HUGE_VAL :  (p == RHYTHMIC_RESTNUM) ? 10 : 1;   /* in tokens */
  /* this is not where this variable currently controlled */


  observable_tag(sp->num, sp->wholerat, sp->observable_tag);
  trill_hi = trill_lo = 0;
}




static void
note_on_off() {
  int p,i;
  CHORD *ch;
  float s;


  if (cur_voice == SOLO_VOICE && include_solo_part == 0) return;
  ch = &(staff.voice[cur_voice].chord);
  if (cur_voice == SOLO_VOICE) ch->vol = 40;
  for (i=0; i < ch->num; i++) {
    p = ch->pitch[i];
        new_add_event(MIDI_COM,NOTE_ON,p,ch->start,ch->vol,ch->whole_st); 
	/*	printf("adding %d at %f\n",p,ch->start); */

	/*    for (s = ch->start; s < ch->end; s += 1/32.)  multiplies each note to 32nds 
	      new_add_event(MIDI_COM,NOTE_ON,p,s,ch->vol,cur_voice); */
    if (pedal_on) {
      staff.bool_chord.pitch[p] = 1;
    }
    else {
      new_add_event(MIDI_COM,NOTE_OFF,p,ch->end,0,ch->whole_end);
    }
  }
}

static void
add_accompaniment_notes() {
  int p,i,n,j;
  CHORD *ch;
  float s;


  if (cur_voice == SOLO_VOICE && include_solo_part == 0) return;
  ch = &(staff.voice[cur_voice].chord);
  if (cur_voice == SOLO_VOICE) ch->vol = 40;
  for (i=0; i < ch->num; i++) {
    n = score.accompaniment.num++;
    //    if (n == MAX_ACCOMPANIMENT_NOTES) {
    if (n == MAX_ACCOMP_NOTES) {
      printf("out of room in accomapnment notes\n");
      exit(0);
    }
    score.accompaniment.note[n].pitch = ch->pitch[i];
    score.accompaniment.note[n].meas = ch->start;
    score.accompaniment.note[n].wholerat = ch->whole_st;
    observable_tag(ch->pitch[i], ch->whole_st, score.accompaniment.note[n].observable_tag);
    //    if (score.midi.num == MAX_ACCOMPANIMENT_NOTES) {
    if (score.midi.num == MAX_ACCOMP_NOTES) {
      printf("out of room in accompaniment list\n");
      exit(0);
    }
  }
}


add_accomp_note() {
  char *tag;

  add_accompaniment_notes();
  note_on_off(tag);
}



static void
make_notes() {
  add_solo_note();
  add_accomp_note();
}


static void
flush_voices() {
  int i,j,k,v1,v2,temp;

  /*   printf("flush voices  \n");*/
  for (i = 0; i < old_staff.num; i++) {
    v1 = old_staff.voice[i].index;
    for (j = 0; j < staff.num; j++) {
      v2 = staff.voice[j].index;
      if (v2 == v1) break;
    }
    if (j == staff.num) { /* not found in list */
      temp = cur_voice;
      cur_voice = v1;
      /* add_accomp_note(); */
      make_notes();
      staff.voice[v1].chord.num = 0;  /* no notes will be pending when the voice becomes active again */
      cur_voice = temp;
    }
  }
  /*  printf("ending flush voices\n"); */
}



flush_all_accomp_notes() {
  int i,v,j,p;
  CHORD *ch;

  /*printf("flush all accomp\n"); */


  for (j = 0; j < staff.num; j++) {
     cur_voice = staff.voice[j].index;
     if (cur_voice != SOLO_VOICE  || include_solo_part)  add_accomp_note();
  }
  flush_pedal();
}
     


flush_all_notes() {
  int i,v,j,p;
  CHORD *ch;

  /*   printf("flush all notes\n"); */


  for (j = 0; j < staff.num; j++) {
     cur_voice = staff.voice[j].index;
     if (cur_voice != SOLO_VOICE  || include_solo_part)  add_accomp_note();
     else add_solo_note();
  }
  flush_pedal();
}
     



static void
barline_act(char c) {
  int n;

  barlines++;
  n = ++score.measdex.num;
  score.measdex.measure[n].pos = meas_pos;
  score.measdex.measure[n].wholerat = rat_meas_pos;
}

static void
make_double_bar_model(top,bot)  /* anything okay after double bar */
  int *top,*bot; {
  int w,junk,db2,db1;

  /*  db2 = claim_state(flush_all_accomp_notes,is_char,"|","dbl bar2");*/
  db2 = claim_state(flush_all_notes,is_char,"|","dbl bar2");
  *top = db1 = claim_state(no_act,is_char,"|","dbl bar1");
  junk = claim_state(no_act,any_char,"","end junk");
  add_act(db2,barline_act);
  add_succ(db1,db2);
  add_succ(db2,junk);
  add_succ(junk,junk);
  *bot =  end_state = junk;
}





static void
make_barline_model(top,bot)  /* at least one "-" followed by anything */
  int *top,*bot; {
  int b,g,e;

  init_end_list(bot);
  b = claim_state(barline_act,is_char,"-","barline");
  g = claim_state(no_act,not_cr,"","comments");
  e = claim_state(no_act,is_char,"\n","end barline");
  add_succ(b,g);
  add_succ(g,g);
  add_succ(g,e);
  add_succ(b,e);
  *top = b;
  *bot = e;
}



static void
make_eq_model(top,bot)
int *top,*bot; {
  int ws1,we1,ws2,we2,e;

  make_opt_white_model(&ws1,&we1);
  make_opt_white_model(&ws2,&we2);
  e = claim_state(no_act,is_char,"=","eq");
  add_succ(we1,e);
  add_succ(e,ws2);
  *top = ws1;
  *bot = we2;
}





static void
make_comment_model(top,bot)
int *top,*bot; {
  int c,cr;

  c = claim_state(no_act,not_cr,"","comment");
  cr = claim_state(no_act,is_char,"\n","com end");
  add_succ(c,c);
  add_succ(c,cr);
  *top = c;
  *bot = cr;
}

static void
make_number_model(top,bot)
int *top,*bot; {
  int num1,num2,last;

  num1 = claim_state(num1_act,is_digit,"","num1");
  num2 = claim_state(num2_act,is_digit,"","num2");
  last = claim_state(no_act,0,"","num_end");
  add_succ(num1,num2);
  add_succ(num2,num2);
  add_succ(num2,last);
  add_succ(num1,last);
  *top = num1;
  *bot = last;
}


static void
add_voice() {
  int i;

  if (staff.num == MAX_VOICE) {
    printf("out of room in voice array\n");
    printf("char = %d\n",cur_char);
    exit(0);
  }
  staff.voice[staff.num].index = number;
  staff.voice[staff.num].dyn_ramp.start_time = 0.;
  staff.voice[staff.num].dyn_ramp.end_time = 0.;
  staff.voice[staff.num].dyn_ramp.start_dyn = .75;
  staff.voice[staff.num].dyn_ramp.end_dyn = .75;
  staff.num++;
  /*  for (i = 0; i < old_staff.num; i++) 
    if (old_staff.voice[i].index == number)   
      staff.voice[number].chord = old_staff.voice[i].chord; */
}

static void
init_voice() {
  old_staff = staff;
  staff.num = 0;
}

static void
make_voice_model(top,bot)
int *top,*bot; {
  int sv,ev,sn,en,sf,ef,comma,sw,ew;

  make_string_model(&sv,&ev,"voice(");
  add_act(sv,init_voice);
  make_number_model(&sn,&en);
  add_act(sn,add_voice);
  make_string_model(&sf,&ef,")\n");
  add_act(ef,flush_voices);
  make_opt_white_model(&sw,&ew);
  comma = claim_state(no_act,is_char,",","voice sep");
  add_succ(ev,sn);
  add_succ(en,sw);
  add_succ(ew,comma);
  add_succ(ew,sf);
  add_succ(comma,sn);
  *top = sv;
  *bot = ef;
}  


static void
set_time_signature() {
  counts_per_meas = numerator;
  note_gets_one_count = denominator;
  time_signature = rat_recent;
  note_value2meas_value =  (note_gets_one_count/(float)counts_per_meas);
  printf("ts = %d/%d\n",counts_per_meas,note_gets_one_count);
}

static void
set_meastime() {
  printf("%d/%d = %d\n",rat_recent.num,rat_recent.den,number);
  cur_meas_size = (float) (60*rat_recent.den) / (float) (rat_recent.num*number); 
  printf("cur_meas_size = %f\n",cur_meas_size);
  /*  cur_meas_size = float_number;  /* for all solo notes after current pos */
}

static void 
number2whole(c)
char c; {
  float_number = (float) number;
}

static void 
add_fractional(c)
char c; {
  int den,temp;
  int i;

  temp = number;
  den = 1;
  for (i=0; i < digits; i++) den *= 10;
  /*  while (temp >= 10) {
    temp /= 10;
    den *= 10;
  } */
  /*   printf("number = %d den = %d\n",number,den);  */
  float_number +=  (float) number / (float) den;
  /*     printf("float_number is %f\n",float_number); */
}

static void 
number2numerator(c)
char c; {
  rat_recent.num = numerator = number;
}

static void
number2denominator(c) 
char c; {
  rat_recent.den = denominator = number;
}


static void 
set_quotient() {
  quotient = (float) numerator / (float) denominator;
}

static void 
compute_notelength(c) 
char c; {
  note_length = (float) numerator / (float) denominator;
  note_length *=   note_value2meas_value;
}


static void
make_frac_model(top,bot)
int *top,*bot; {
  int num_top,num_bot,den_top,den_bot,slash;

  slash = claim_state(no_act,is_char,"/","slash");
  make_number_model(&num_top,&num_bot);
  make_number_model(&den_top,&den_bot);
  add_act(num_bot,number2numerator);
  add_act(den_bot,number2denominator);
  add_act(den_bot,set_quotient);
  add_act(den_bot,compute_notelength);
  add_succ(num_bot,slash);
  add_succ(slash,den_top);
  *top = num_top;
  *bot = den_bot;
}


static void
init_float() {
  float_number = 0;
}

static void
make_float_model(top,bot)
int *top,*bot; {
  int num_top,num_bot,frac_top,frac_bot,point,trans,end,start;

  point = claim_state(no_act,is_char,".","dec pt");
  start = claim_state(init_float,0,"","start float");
  make_number_model(&num_top,&num_bot);
  make_number_model(&frac_top,&frac_bot);
  end = claim_state(no_act,0,"","no frac");
  *top = start;
  *bot = end;
 

  add_act(num_bot,number2whole);
  add_act(frac_bot,add_fractional);

  add_succ(start,num_top);
  add_succ(num_bot,point);
  add_succ(point,frac_top);
  add_succ(frac_bot,end);
  add_succ(start,point);
  add_succ(point,end);

}

#define MAX_FLOAT_AR 10

static float float_ar[MAX_FLOAT_AR];
static int float_count;

static void
zero_float_count(char c) {
  float_count = 0;
}

static void
add_float_to_list(char c) {
  float_ar[float_count++] = float_number;
}


make_float_list_model(int *top, int *bot) {
  int f1s,f1e,f2s,f2e,wss,wse,comma,end;

  make_float_model(&f1s,&f1e);
  make_float_model(&f2s,&f2e);
  make_white_model(&wss,&wse);
  comma = claim_state(no_act,is_char,",","float list sep");
  end = claim_state(no_act,0,"","float list end");
  add_act(f1s,zero_float_count);
  add_act(f1e,add_float_to_list);
  add_act(f2e,add_float_to_list);
  add_succ(f1e,comma);
  add_succ(comma,f2s);
  add_succ(comma,wss);
  add_succ(wse,f2s);
  add_succ(f2e,comma);
  add_succ(f2e,end);
  add_succ(f1e,end);
  *top = f1s;
  *bot = end;
}
  


/*  float time;
  char command[MAX_STRING];
  float arg[MAX_COMMAND_ARGS];*/


add_accomp_dir(char c) {
  int i,n;

  n = comm.num;
  /*  strcpy(&(comm.list[n].command),sbuff); */
  strcpy(comm.list[n].command,sbuff); /* linux */
  comm.list[n].time = meas_pos;
  comm.list[n].rat = rat_meas_pos;
  printf("added accomp dir\n");

  if (float_count > MAX_COMMAND_ARGS) {
    printf("too many floats for array\n");
    exit(0);
  }
  for (i=0; i < float_count; i++)
    comm.list[n].arg[i] = float_ar[i];
  comm.num++;
}

make_accomp_dir_model(int *top, int *bot) {
  int s,e,lp,rp,ss,se,fs,fe;

  make_any_string_model(&ss,&se);
  lp = claim_state(no_act,is_char,"(","acmp dir lp");
  rp = claim_state(add_accomp_dir,is_char,")","acmp dir rp");
  make_float_list_model(&fs,&fe);
  add_succ(se,lp);
  add_succ(lp,fs);
  add_succ(fe,rp);
  *top = ss;
  *bot = rp;
}



static void
make_time_sig_model(top,bot)
int *top,*bot; {
  int ts,te,es,ee,fs,fe,cr;

  make_string_model(&ts,&te,"measure");
  make_eq_model(&es,&ee);
  make_frac_model(&fs,&fe);
  cr = claim_state(set_time_signature,is_char,"\n","ts_end");
  add_succ(te,es);
  add_succ(ee,fs);
  add_succ(fe,cr);
  *top = ts;
  *bot = cr;
}
  
static void
make_meastime_model(top,bot)
int *top,*bot; {
  int ts,te,es,ee,fs,fe,cr,frs,fre;

  make_frac_model(&frs,&fre);
  make_eq_model(&es,&ee);
  make_number_model(&fs,&fe);
  cr = claim_state(set_meastime,is_char,"\n","mt_end");
  add_succ(fre,es);
  add_succ(ee,fs);
  add_succ(fe,cr);
  *top = frs;
  *bot = cr;
}
  
static void
make_inline_meastime_model(top,bot)
int *top,*bot; {
  int ts,te,es,ee,fs,fe,cr,frs,fre,temps,tempe;

  make_string_model(&temps,&tempe,"tempo ");
  make_frac_model(&frs,&fre);
  make_eq_model(&es,&ee);
  make_number_model(&fs,&fe);
  add_act(fe,set_meastime);
  add_succ(fre,es);
  add_succ(ee,fs);
  /*  add_succ(fe,cr);*/
  add_succ(tempe,frs);
  *top = temps;
  /*  *bot = cr;*/
  *bot = fe;
}
  
static void
old_make_meastime_model(top,bot)
int *top,*bot; {
  int ts,te,es,ee,fs,fe,cr;

  make_string_model(&ts,&te,"meastime");
  make_eq_model(&es,&ee);
  make_float_model(&fs,&fe);
  cr = claim_state(set_meastime,is_char,"\n","mt_end");
  add_succ(te,es);
  add_succ(ee,fs);
  add_succ(fe,cr);
  *top = ts;
  *bot = cr;
}
  
    
static char solf_name;
static int octave;

static void 
set_octave(c)
char c; {
  octave = number;
}


static void
set_solfname(c) {
  solf_name = c;
  rhythmic_rest = 0;
}

static int accident[7];  /* a through g */

static void
set_dyn_level() {
    staff.voice[cur_voice].dyn_ramp.start_dyn = recent_dyn;
    staff.voice[cur_voice].dyn_ramp.end_dyn = recent_dyn;
    staff.voice[cur_voice].dyn_ramp.end_time = -1;
    /* time way always be > end_time */
}


static int
compute_volume() {
  float v1,v2,t1,t2,t,v,p,q;
  int m;
  

  t = meas_pos;
  t2 = staff.voice[cur_voice].dyn_ramp.end_time;
  v2 = staff.voice[cur_voice].dyn_ramp.end_dyn;
  if (t > t2) {
    recent_dyn = v2;
    set_dyn_level();
  }
  v1 = staff.voice[cur_voice].dyn_ramp.start_dyn;
  v2 = staff.voice[cur_voice].dyn_ramp.end_dyn;
  t1 = staff.voice[cur_voice].dyn_ramp.start_time;
  t2 = staff.voice[cur_voice].dyn_ramp.end_time;
  if (t > t2) p = 1;
  else p = (t-t1)/(t2-t1);
  if (p < -0.1 || p > 1.1) {
    printf("problem in compute_vol (p = %f)\n",p);
    printf("v1 = %f v2 = %f t1 = %f t2 = %f t = %f\n",v1,v2,t1,t2,t);
    printf("cur_voice = %d index = %d\n",cur_voice,staff.voice[cur_line].index);
    printf("cur char is %d\n",cur_char);
    exit(0);
  }
  q = 1-p;
  v = q*v1+p*v2;
  m = f2midi_vol(v);
  return(m);
}



static void
increment_position() {
  int v,vol,is_rest;
  RATIONAL x;
  float temp,r;

  v = staff.voice[cur_line].index;
  if (tying == 0) {
    staff.voice[v].chord.start = meas_pos;
    staff.voice[v].chord.whole_st = rat_meas_pos;
  }
  /*    x = div_rat(rat_recent,time_signature); /* rat_recent is note value eg 1/4 */
  x = rat_recent;
  rat_meas_pos = add_rat(rat_meas_pos,x);
  /*  printf("rat_meas_pos = %d/%d\n",rat_meas_pos.num,rat_meas_pos.den); */
  /*  meas_pos += note_length;  */
  meas_pos = rat_meas_pos.num / (float) rat_meas_pos.den;
  staff.voice[v].chord.end = meas_pos;
  staff.voice[v].chord.whole_end = rat_meas_pos;
  /*  vol = staff.voice[v].cur_vol; */
  //  is_rest = (midi_pitch == RESTNUM); 
  is_rest = is_a_rest(midi_pitch);
  if (is_rest || cur_voice == SOLO_VOICE) vol = 0;
  else if (vol_override == 0) {
    vol = compute_volume();
    if (vol <=0 && cur_voice != SOLO_VOICE && !is_rest) {
      printf("impossible volume %d at char %d for voice %d\n",vol,cur_char,v);
      printf("cur_voice is %d\n",cur_voice);
      exit(0);
    }
    staff.voice[v].chord.vol = vol;
  }
 }



static void
compute_midi_pitch(c)
char c; {
  int v;

  c = solf_name;

  if ( c == '~')    {
    midi_pitch = (rhythmic_rest) ? RHYTHMIC_RESTNUM : RESTNUM;
    //    printf("midi_pitch = %d\n",midi_pitch);
  }
  else {
    if ( c == 'c') midi_pitch = 0;
    else if ( c == 'd') midi_pitch = 2;
    else if ( c == 'e') midi_pitch = 4;
    else if ( c == 'f') midi_pitch = 5;
    else if ( c == 'g') midi_pitch = 7;
    else if ( c == 'a') midi_pitch = 9;
    else if ( c == 'b') midi_pitch = 11;
    midi_pitch += 12*octave;
    midi_pitch += accident[c - 'a'];
  }
  v = staff.voice[cur_line].index;
  //  if (midi_pitch != RESTNUM || cur_voice == 0) {
  if (is_a_rest(midi_pitch) == 0 || cur_voice == 0) {
    if (staff.voice[v].chord.num < MAX_CHORD) 
      staff.voice[v].chord.pitch[staff.voice[v].chord.num++] = midi_pitch;
    else printf("exceeding maximum chord depth\n");
  }
} 




static void
set_accidental(c)
char c; {
  if ( solf_name == '~') {
    rhythmic_rest = (c == '!');
    //    printf("rhythmic_rest = %d\n",rhythmic_rest);
    return;
  }
  if (c == '#' || c == 's')   accident[solf_name - 'a'] = 1;
  if (c == 'b' || c == 'f')   accident[solf_name - 'a'] = -1;
  if (c == 'n' )              accident[solf_name - 'a'] = 0;
} 

static void
tie_act() {
  tying = 1;
}


static void
make_pitch_model(top,bot)
int *top,*bot; {
  int solfege,oct_top,oct_bot,accidental,pitch_end,start;

  start = claim_state(no_act,0,"","start pitch");
  solfege = claim_state(set_solfname,is_solf_or_rest,"","let");
  accidental = claim_state(set_accidental,is_acc,"","accid");
  pitch_end = claim_state(compute_midi_pitch,0,"","pitch end");
  /*  add_act(pitch_end,issue_note_on); */
  make_number_model(&oct_top,&oct_bot);
  add_act(oct_bot,set_octave);

  add_succ(start,solfege);  
  add_succ(solfege,oct_top);
  add_succ(solfege,accidental);
  add_succ(solfege,pitch_end);
  add_succ(accidental,oct_top); 
  add_succ(accidental,pitch_end);
  add_succ(oct_bot,pitch_end);
  *top = start;
  *bot = pitch_end;
}

static void
lo_trill_act() {
  trill_lo = midi_pitch;
}

static void
hi_trill_act() {
  trill_hi = midi_pitch;
}


static void
make_trill_model(int *top, int *bot) {
  int tr_start,tr_end,lp,rp,p1s,p1e,p2s,p2e,comma;

  make_string_model(&tr_start,&tr_end,"tr");
  lp = claim_state(no_act,is_char,"(","start trill");
  rp = claim_state(no_act,is_char,")","end trill");
  comma = claim_state(no_act,is_char,",","trill sep");
  make_pitch_model(&p1s,&p1e);
  make_pitch_model(&p2s,&p2e);
  add_succ(tr_end,lp);
  add_succ(lp,p1s);
  add_succ(p1e,comma);
  add_succ(comma,p2s);
  add_succ(p2e,rp);
  add_act(comma,lo_trill_act);
  add_act(rp,hi_trill_act);
  *top = tr_start;
  *bot = rp;
}

static void
init_volumes() {
  int i;

  for (i=0; i < MAX_VOICE; i++) staff.voice[i].cur_vol = 100;
}


static void
init_score() {  /* called once at start of score reading */
  int i,v;

  score.measdex.num = score.example.num = score.measdex.num =  score.accompaniment.num = 0;
  score.solo.num=  score.midi.num = 0;
  num_states = 0;
  comm.num = 0;
  staff.num = 0;
  event.num = 0;
  pedal_on = 0;
  barlines = 0;
  rat_meas_start.num = 0;
  rat_meas_start.den = 1;
  rat_meas_pos = rat_meas_start;



  cur_char = numerator = digits =  denominator = tying = vol_override = 0;
  number =  float_number = quotient = note_length =  meas_pos = midi_pitch = cur_line = pedal_on = 0;
  cur_voice =  note_value2meas_value = counts_per_meas = note_gets_one_count = solo_note_is_cue = 0;
  barlines = trill_lo = trill_hi = cur_meas_size = solo_note_is_trigger =  recent_dyn = 0;


  for (i=0; i < HI_PITCH; i++) staff.bool_chord.pitch[i]=0;
  for (i=0; i < MAX_VOICE; i++) staff.voice[i].cur_vol = 0;
  for (i=0; i < MAX_VOICE; i++) staff.voice[i].chord.start = 0;
  for (i=0; i < MAX_VOICE; i++) staff.voice[i].chord.num = 0;
  for (i=0; i < MAX_VOICE; i++) staff.voice[i].chord.whole_st = rat_meas_pos;
  set_volume_knots();
  init_volumes();
  accom_cue_list.num = 0;
  wait_list.num = 0;
}


static void
init_chord() {
  int i,v;

  staff.voice[cur_voice].chord.num = 0;
  if (cur_voice == SOLO_VOICE) solo_note_is_cue = solo_note_is_trigger = 0;
  /*  printf("disarming cue\n");*/
}



static void
make_multi_pitch_model(top,bot)  /* tie or string of pitches or trill*/
int *top,*bot; {
  int comma,tie,ps,pe,start,end,not_tie,end_notes,trill_start,trill_end;

  
  start  = claim_state(no_act,0,"","start multi");
  end  = claim_state(no_act,0,"","end multi");
  end_notes  = claim_state(no_act,0,"","end notes");
  not_tie  = claim_state(make_notes,0,"","not tie");
  tie = claim_state(tie_act,is_char,"t","tie");
  add_act(not_tie,init_chord);
  make_pitch_model(&ps,&pe);
  make_trill_model(&trill_start,&trill_end);
  comma  = claim_state(no_act,is_char,",","note sep");

  add_succ(start,tie);
  add_succ(tie,end);
  
  add_succ(start,not_tie);
  add_succ(not_tie,ps);
  add_succ(pe,comma);
  add_succ(comma,ps);
  add_succ(pe,end_notes);
  add_succ(end_notes,end);

  add_succ(not_tie,trill_start);
  add_succ(trill_end,end);

  *top = start;
  *bot = end;
}


static void
cue_act() {
  /*  printf("setting cue\n");*/
  if (cur_voice == SOLO_VOICE) 
    solo_note_is_cue = SOLO_CUE_NUM;
  else
    add_float_list(rat_meas_pos,meas_pos,"cue",&accom_cue_list);
}

static void
acue_act() {
  if (cur_voice == SOLO_VOICE) 
    solo_note_is_cue = ACCOM_CUE_NUM;
  else
    add_float_list(rat_meas_pos,meas_pos,"cue",&accom_cue_list);
}

static void
xchg_act() {
  if (cur_voice == SOLO_VOICE) add_float_list(rat_meas_pos,meas_pos,"sxchg",&accom_cue_list);
  else add_float_list(rat_meas_pos,meas_pos,"axchg",&accom_cue_list);
}

static void
wait_act() {
    add_float_list(rat_meas_pos,meas_pos,"wait",&wait_list);
}

static void
trig_act() {
  if (cur_voice != SOLO_VOICE) {
    printf("trigger must be in  solo voice\n");
    exit(0);
  }
  solo_note_is_cue =  solo_note_is_trigger = 1;
}

static void
ped_on_act() {
  CHORD *ch;

  if (cur_voice == SOLO_VOICE) {
    printf("can't add pedal to solo\n"); 
    exit(0);
  }
  ch = &(staff.voice[cur_voice].chord);
  /*  printf("adding pedal on at start = %d/%d end = %d/%d cur_voice = %d\n",ch->whole_st.num,ch->whole_st.den,ch->whole_end.num,ch->whole_end.den,cur_voice); */
  new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,ch->end,PEDAL_ON,ch->whole_end); 
  return;
  pedal_on = 1;
}

static void
ped_off_act() {  /* all held notes are flushed */
  int i,v,j;
  CHORD *ch;

  if (cur_voice == SOLO_VOICE) {
    printf("can't add pedal to solo\n"); 
    exit(0);
  }
  ch = &(staff.voice[cur_voice].chord);
  new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,ch->end,PEDAL_OFF,ch->whole_end); 
  return;
  pedal_on = 0;
  flush_pedal();
}

static void
ped_both_act() {  
  int i,v,j;
  CHORD *ch;
  RATIONAL next,ts,ps,pe;
  float r;
  char n1[500],n2[500];

  ts.num = 1; ts.den = 16;
  if (cur_voice == SOLO_VOICE) {
    printf("can't add pedal to solo\n"); 
    exit(0);
  }
  ch = &(staff.voice[cur_voice].chord);

  ps = ch->whole_end;  /* this is a kludge.  current note's beginning and end not yet
			  set.  compute them here */
  pe = add_rat(rat_meas_pos,rat_recent);


  next = add_rat(ch->whole_end, ts);
  r = next.num/ (float) next.den;
    new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,ch->start,PEDAL_OFF,ch->whole_st); 
      new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,ch->end,PEDAL_ON,ch->whole_end); 
      /*   new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,ch->start,PEDAL_OFF,ps);
	   new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,ch->end,PEDAL_ON,pe); */
  wholerat2string(ch->whole_st, n1);
  wholerat2string(ch->whole_end, n2);
  printf("off at %s    on at %s\n",n1,n2);
  return;
  pedal_on = 0;
  flush_pedal();
}

static void
make_pedal_model(top,bot)
int *top,*bot; {
  int s,e;
  int ped_on_s,ped_on_e,ped_off_s,ped_off_e,ws2s,ws2e,ped_both_s,ped_both_e;

  s = claim_state(no_act,0,"","pedal start");
  e = claim_state(no_act,0,"","pedal end");
  make_string_model(&ped_on_s,&ped_on_e,"ped+");
  make_string_model(&ped_off_s,&ped_off_e,"ped-");
  make_string_model(&ped_both_s,&ped_both_e,"ped^");
  add_succ(s,ped_on_s);
  add_succ(s,ped_off_s);
  add_succ(s,ped_both_s);
  add_succ(ped_on_e,e);
  add_succ(ped_off_e,e);
  add_succ(ped_both_e,e);
  add_act(ped_on_e,ped_on_act);
  add_act(ped_off_e,ped_off_act);
  add_act(ped_both_e,ped_both_act);
  *top = s;
  *bot = e;
}

static void
set_dyn(float x) {
  recent_dyn = x;
}
 
static void
set_dyn_ff() {
    set_dyn(.9);
}

static void
set_dyn_f() {
    set_dyn(3.2/6.);
}

static void
set_dyn_mf() {
    set_dyn(3.0/6.);
}

static void
set_dyn_mp() {
    set_dyn(2.8/6.);
}

static void
set_dyn_p() {
    set_dyn(2.5/6.);
    set_dyn(2.2/6.);
}


static void
set_dyn_pp() {
    set_dyn(2.3/6.);
    set_dyn(1.9/6.);
}

static void
set_dyn_ppp() {
    set_dyn(1.9/6.);
}

/*static void
set_dyn_mp() {
    set_dyn(3.5/6.);
}

static void
set_dyn_p() {
    set_dyn(3./6.);
}

static void
set_dyn_pp() {
    set_dyn(2.5/6.);
}

static void
set_dyn_ppp() {
    set_dyn(0.);
}
*/
static void
make_dyn_model(top,bot)
int *top,*bot; {
  int mpms,mpme,s,e,pps,ppe,ps,pe,mps,mpe,mfs,mfe,fs,fe,ffs,ffe,ppps,pppe;

  s = claim_state(no_act,0,"","dyn start");
  e = claim_state(no_act,0,"","dyn end");
  make_string_model(&ppps,&pppe,"ppp");
  add_act(pppe,set_dyn_ppp);
  make_string_model(&pps,&ppe,"pp");
  add_act(ppe,set_dyn_pp);
  make_string_model(&ps,&pe,"p");
  add_act(pe,set_dyn_p);
  make_string_model(&mps,&mpe,"mp");
  add_act(mpe,set_dyn_mp);
  make_string_model(&mfs,&mfe,"mf");
  add_act(mfe,set_dyn_mf);
  make_string_model(&fs,&fe,"f");
  add_act(fe,set_dyn_f);
  make_string_model(&ffs,&ffe,"ff");
  add_act(ffe,set_dyn_ff);
  add_succ(s,ppps);
  add_succ(s,pps);
  add_succ(s,ps);
  add_succ(s,mps);
  add_succ(s,mfs);
  add_succ(s,fs);
  add_succ(s,ffs);

  add_succ(pppe,e);
  add_succ(ppe,e);
  add_succ(pe,e);
  add_succ(mpe,e);
  add_succ(mfe,e);
  add_succ(fe,e);
  add_succ(ffe,e);
  *top = s;
  *bot = e;
}


static void
make_plain_dyn_model(top,bot)
int *top,*bot; {
  int s,e;

  s = claim_state(no_act,0,"","pln dyn s");
  e = claim_state(no_act,0,"","pln dyn e");
  make_dyn_model(&s,&e);
  add_act(e,set_dyn_level);
  *top = s;
  *bot = e;
}
  


static void
chord_init() {
}


static void
chord_end() {
  increment_position();
  tying = 0;
  vol_override = 0;
}


static void
set_volume() {
  if (number < MAX_MIDI_VOL)
    staff.voice[cur_voice].chord.vol = number;
  else { 
    printf("%d is not a possible volume\n",number);
    exit(0);
  }
}

static void
set_volume_float() {
  int pitch;

  if (float_number <= 1. && float_number >= 0.) {
    pitch =   staff.voice[cur_voice].chord.pitch[0];
    staff.voice[cur_voice].chord.vol = zo2vel(float_number,(float)pitch);
  }
  else { 
    printf("%f is not a possible volume\n",float_number);
    exit(0);
  }
}

static void
set_vol_override() {
  vol_override = 1;
}
     

static void
make_volume_model(top,bot) 
int *top,*bot; {
  int opt_ws,opt_we,sv,ev,sn,en,sf,ef,end;

  make_string_model(&sv,&ev,"v=");
  make_number_model(&sn,&en);
  end = claim_state(no_act,0,"","end vol");
  make_float_model(&sf,&ef);
  add_act(en,set_volume);
  add_act(ef,set_volume_float);
  add_act(sv,set_vol_override);

  add_succ(ev,sn);
  add_succ(en,end);
  add_succ(ev,sf);
  add_succ(ef,end);
  *top = sv;
  *bot = end;
}

static void
make_chord_model(top,bot)
int *top,*bot; {
  int lb,rb,ps,ws,fs,pe,we,fe,cue_start,cue_end,acue_start,acue_end,opt_ws,opt_we,peds,pede,trig_start,trig_end;
  int ws2s,ws2e,vs,ve,xchg_start,xchg_end,wait_start,wait_end;
  int ws3s,ws3e,as,ae,ws4s,ws4e,ws5s,ws5e;

  lb = claim_state(chord_init,is_char,"[","lb");
  make_multi_pitch_model(&ps,&pe); 
  make_opt_white_model(&ws3s,&ws3e);
  make_volume_model(&vs,&ve);
  make_white_model(&ws,&we); 
  make_frac_model(&fs,&fe); 
  rb = claim_state(chord_end,is_char,"]","rb");
  make_string_model(&cue_start,&cue_end,"cue");
  make_string_model(&acue_start,&acue_end,"acue");
  make_string_model(&xchg_start,&xchg_end,"xchg");
  make_string_model(&trig_start,&trig_end,"trigger");
  make_string_model(&wait_start,&wait_end,"wait");
  make_opt_white_model(&opt_ws,&opt_we);
  make_opt_white_model(&ws2s,&ws2e);
  make_opt_white_model(&ws4s,&ws4e);
  make_opt_white_model(&ws5s,&ws5e);
  make_pedal_model(&peds,&pede);
  make_accomp_dir_model(&as,&ae);
  add_act(cue_end,cue_act);
  add_act(acue_end,acue_act);
  add_act(trig_end,trig_act);
  add_act(xchg_end,xchg_act);
  add_act(wait_end,wait_act);

  add_succ(lb,ps);
  add_succ(pe,ws3s);
  add_succ(ws3e,vs);
  add_succ(ve,ws);

  add_succ(pe,ws);
  add_succ(we,fs);
  add_succ(fe,rb);
  add_succ(fe,opt_ws);

  add_succ(opt_we,cue_start);
  add_succ(ws4e,cue_start);
  add_succ(cue_end,rb);

  add_succ(opt_we,acue_start);
  add_succ(ws4e,acue_start);
  add_succ(acue_end,rb);

  add_succ(opt_we,xchg_start);
  add_succ(xchg_end,rb);
  add_succ(xchg_end,ws2s); 
  add_succ(xchg_end,ws4s); 

  add_succ(opt_we,trig_start);
  add_succ(trig_end,rb);

  add_succ(opt_we,peds);

  add_succ(cue_end,ws2s);
  add_succ(ws2e,peds);
  add_succ(pede,rb);

  add_succ(ae,rb);
  add_succ(pede,as);
  add_succ(opt_we,as);
  add_succ(ws2e,as);  

  add_succ(opt_we,wait_start);
  add_succ(wait_end,rb);
  add_succ(wait_end,ws5s);
  add_succ(ws5e,xchg_start);
  add_succ(ws5e,cue_start);
  add_succ(ws5e,peds);
  add_succ(ws5e,as);


  *top = lb;
  *bot = rb;
}

  

static void
line_init() {
  int i;

  for (i=0; i < 7; i++) accident[i] = 0;
  rat_meas_pos = rat_meas_start;
  meas_pos = rat_meas_pos.num / (float) rat_meas_pos.den;

  if (cur_line >= staff.num) {
    printf("too many voices or voices unset at char %d\n",cur_char);
    printf("voices = %d max = %d\n",cur_line,staff.num);
    exit(0);
  }
  cur_voice = staff.voice[cur_line].index;
}
  

static void
line_end_act() {
  float error;
  int meas;
  RATIONAL rem;

  /*  printf("meas pos = %f barline = %d\n",meas_pos,barlines); */
  /*  meas = (int) meas_pos;
  error = fabs(barlines - meas_pos);
    if (error > .001) { */


  rem = sub_rat(add_rat(rat_meas_start,time_signature),rat_meas_pos);
  if (rem.num != 0) {
  /*  if (rat_meas_pos.num != barlines || rat_meas_pos.den != 1) {*/
    printf("note values don't add up in measure %d rat_meas_pos = %d/%d time_sig = %d/%d rat_meas_start = %d/%d rem = %d/%d",barlines-1,rat_meas_pos.num,rat_meas_pos.den,time_signature.num,time_signature.den,rat_meas_start.num,rat_meas_start.den,rem.num,rem.den);
    exit(0); 
  }

  cur_line++;
}

static void
set_cresc_parms() {
  RATIONAL x;
  float x1,x2;

    staff.voice[cur_voice].dyn_ramp.start_dyn = 
      staff.voice[cur_voice].dyn_ramp.end_dyn;
    staff.voice[cur_voice].dyn_ramp.end_dyn = recent_dyn;
    staff.voice[cur_voice].dyn_ramp.start_time = meas_pos;
    x = add_rat(rat_meas_pos,rat_recent);
    /*    staff.voice[cur_voice].dyn_ramp.end_time = meas_pos + quotient; */
    staff.voice[cur_voice].dyn_ramp.end_time = x.num /(float) x.den;
}


static void
make_cresc_model(top,bot)
int *top,*bot; {
  int crescs,cresce,dims,dime,de,ds,fs,fe,ws1s,ws1e,ws2s,ws2e,lp,rp;

  lp = claim_state(no_act,is_char,"(","start cresc");
  rp = claim_state(no_act,is_char,")","end cresc");
  make_string_model(&crescs,&cresce,"cresc");
  make_string_model(&dims,&dime,"dim");
  make_dyn_model(&ds,&de);
  make_frac_model(&fs,&fe);
  make_white_model(&ws1s,&ws1e);
  make_white_model(&ws2s,&ws2e);
  add_succ(lp,crescs);
  add_succ(lp,dims);
  add_succ(cresce,ws1s);
  add_succ(dime,ws1s);
  add_succ(ws1e,ds);
  add_succ(de,ws2s);
  add_succ(ws2e,fs);
  add_succ(fe,rp);
  add_act(rp,set_cresc_parms);
  *top = lp;
  *bot = rp;
}


  


static void
make_line_model(top,bot)
int *top,*bot; {
  int ns,ws,cr,ne,we,line_start,ds,de,wws,wwe,ce,cs,ws3s,ws3e,mts,mte;

  cr = claim_state(line_end_act,is_char,"\n","end line");
  line_start = claim_state(line_init,0,"","line start");
  make_white_model(&wws,&wwe);
  make_white_model(&ws3s,&ws3e);
  make_opt_white_model(&ws,&we);
  make_chord_model(&ns,&ne); 
  make_plain_dyn_model(&ds,&de);
  make_cresc_model(&cs,&ce);
  make_inline_meastime_model(&mts,&mte);
  add_succ(line_start,ws);
  add_succ(we,ds);
  add_succ(de,wws);
  add_succ(wwe,cs);
  add_succ(ce,ws3s);
  add_succ(ws3e,ns);
  add_succ(ne,ws);

  add_succ(we,ns);
  add_succ(we,cs);
  add_succ(we,cr);
  add_succ(wwe,ns);

     add_succ(we, mts);
       add_succ(mte, wws);
  

  *top = line_start;
  *bot = cr;
}
  
  
static void 
measure_act() {
  if (cur_line != staff.num) {
    printf("voices not accounted for in measure %d\n",barlines-1);
    printf("at character %d\n",cur_char);
    exit(0);
  }
  /*  if (rat_meas_start.den != 1) {  changed
    printf("weird mojo here\n");
    exit(0);
    }*/
  /*  rat_meas_start.num++; */
  rat_meas_start = add_rat(rat_meas_start,time_signature); /*changed */
}
  


static void
measure_init() {
  int i;

  cur_line = 0;
}



static void
set_score_meastime() {
  score.meastime = /*float_number*/ cur_meas_size;
  printf("score.meastime = %f\n",score.meastime);
}

static void
make_measure_model(top,bot)
int *top,*bot; {
  int bs,be,ls,le,me,vs,ve,mts,mte,tss,tse; 
  
  make_barline_model(&bs,&be);
  make_voice_model(&vs,&ve);
  make_meastime_model(&mts,&mte);
  make_time_sig_model(&tss,&tse);
  add_act(mts,set_meastime);
  add_act(bs,measure_init);
  make_line_model(&ls,&le);
  me = claim_state(measure_act,0,"","meas end");
  add_succ(be,tss);
  add_succ(tse,ls);
  add_succ(tse,mts);
  add_succ(tse,vs);
  add_succ(be,ls);
  add_succ(be,mts);
  add_succ(mte,ls);
  add_succ(be,vs);
  add_succ(ve,ls);
  add_succ(ve,mts);
  add_succ(le,ls);
  add_succ(le,me);
  *top = bs;
  *bot = me;
}
    



static void
make_score_graph() {
  END_LIST end;
  int ls,ss,es,sdb,ms,me,cs,ce,tss,tse,mts,mte;
  
  make_comment_model(&cs,&ce);
  make_time_sig_model(&tss,&tse);
  make_meastime_model(&mts,&mte);
  add_act(mte,set_score_meastime);
  ss = claim_state(no_act,is_char," ","start state"); 
  make_measure_model(&ms,&me);  
  make_double_bar_model(&sdb,&end_state);
  add_succ(me,ms);
  add_succ(me,sdb);
  add_succ(ss,cs);
  add_succ(ce,tss);
  add_succ(tse,ms);

  add_succ(ce,mts);
  add_succ(mte,ms);
  add_succ(mte,tss);
  start_state = ss;
}

static void 
add_null_tras(hl,s,prev,c)
ACTLIST *hl;
int s,prev;  
char c; {
  int j,t;
  HYPO h;
  int (*is_legal)();
  char *control;

  h.state = s;
  h.prev = prev;
  h.ch = c;
  if (node_buff[s].legal.func == 0) {
    prev      = hl->num;  /* most recently added  hypo */
    add_hyp(hl,h);
    for (j = 0; j < node_buff[s].next.num; j++) {
      t = node_buff[s].next.list[j];
      add_null_tras(hl,t,prev,c);
    }
  }
  else {
    is_legal = node_buff[s].legal.func;
    control = node_buff[s].legal.control;
    if (is_legal(c,control))  add_hyp(hl,h);
  }
}


static void
show_recent_past(ACTLIST *hl) {
  int i,s;

  for (i = hl->num-1; (i >= 0 && i > hl->num-20); i--) {
    s = hl->hyp[i].state;
    printf("%d %c %s\n",i,hl->hyp[i].ch,node_buff[s].desc);
  }
}

static int
order(h1,h2)
HYPO *h1,*h2; {
  return(h1->state-h2->state);
}


  
check_dups(hl,lo,hi,num)
ACTLIST *hl; 
int lo,hi,num; {
  int i,j,p1,p2;

  for (i=lo; i < hi; i++)  for (j=i+1; j < hi; j++) {
    if (hl->hyp[i].state == hl->hyp[j].state) {
      p1 = hl->hyp[hl->hyp[i].prev].state;
      p2 = hl->hyp[hl->hyp[j].prev].state;
      printf("duplicate paths at char %d.  both %s (%d), %s (%d) are predecessors of %s\n",num,
	     node_buff[p1].desc,p1,
	     node_buff[p2].desc,p2,
	     node_buff[hl->hyp[i].state].desc);
      printf("duplicate paths\n");
      exit(0);
    }
  }
}


#define TOTAL_HYPS 1000
#define MAX_ACT 10

static void 
make_tree(hl,low,high)
ACTLIST *hl; 
int *low,*high; {
  HYPO h;
  int i,j,s,lo,hi,ss,tt,text_ptr;
  char c,*control;
  int (*is_legal)(),null_trans;
  ACTION *action;

  h.state = start_state;
  add_hyp(hl,h);
  hi = 0;
  for (i=0/*1*/; i < cb.num; i++)  {
    lo = hi;
    hi = hl->num;
    c = cb.buff[i];
    check_dups(hl,lo,hi,i);
    if (lo == hi) {
      printf("I cannot parse at char %d\n",i);
      show_recent_past(hl);
      exit(0); 
    }
    for (s = lo; s < hi; s++) {
      ss = hl->hyp[s].state;
      null_trans = (node_buff[ss].legal.func == 0);
      if (!null_trans) for (j = 0; j < node_buff[ss].next.num; j++) {
	tt = node_buff[ss].next.list[j];
	add_null_tras(hl,tt,s,c);
      }
    }
                       for (s = hi; s < hl->num; s++) {
      ss = hl->hyp[s].state;
      /*      printf("char# = %d desc = %s s = %d st = %d char = %c\n",i,node_buff[ss].desc,s,ss,c);    */
    }
		       /*		           printf("\n");    */
  } 
  *low = hi;
  *high = hl->num;
}

static void trace_back(hl,state,lo,hi,count) 
     /* state array stored in reverse order */
ACTLIST hl;
int *state,lo,hi,*count; {
  int mark,s,i;

  mark = -1;
  for (s = lo; s < hi; s++)
    if (hl.hyp[s].state == end_state) mark = s;
  if (mark == -1) {
    printf("no path to end_state\n");
    exit(0);
  }
  state[0] = end_state;
  for (i = 1; state[i-1] != start_state; i++) {
    if (i == MAX_HYPS) {
      printf("screw up here\n");
      exit(0); 
    }
    /*    printf("mark = %d \n",mark);*/
    mark = hl.hyp[mark].prev;
    state[i] = hl.hyp[mark].state;
  }  
  *count = i;
}


static void  take_action(state,count)
int *state,count; {
  int i,j,k,s;
  char c;
  ACTION *action;

  k = 0;
  for (i=count-1; i >= 0; i--)  { 
    cur_char = k-1;  /* a  kludgy way to get char to lower level */
    c = cb.buff[k-1];
    s = state[i];
    action = &(node_buff[s].action);
    /*    printf("i = %d\tchar = %c\tstate = %d\tdesc = %s\tnum = %d\n",i,c,s,node_buff[s].desc,action->num);      */
    for (j=0; j < action->num; j++) {
      /*                         printf("c = %c state = %s // hi_trill = %d\n",c,node_buff[s].desc, trill_hi);    */
      (action->func)[j](c); 
    }
    if (node_buff[s].legal.func != 0) k++;
  }
}  

static void 
parse_score() {
  ACTLIST hl;
  int lo,hi,count,i,s;
#ifndef WINDOWS
  HYPO list[MAX_HYPS];
  int state[MAX_HYPS];
#else
  HYPO *list;
  int *state;

  list = (HYPO *) malloc(MAX_HYPS*sizeof(HYPO));
  state =  (int *) malloc(MAX_HYPS*sizeof(int));
#endif


  init_score();
  hl.num = 0;
  hl.hyp = list;
  make_tree(&hl,&lo,&hi);
  /*    for (i=0; i < hl.num; i++) printf("i = %d state = %d prev = %d\n",i,hl.hyp[i].state,hl.hyp[i].prev); */
  trace_back(hl,state,lo,hi,&count);
  /*   for (i=0; i < count; i++) printf("i = %d state = %d desc = %s\n",i,state[i],node_buff[state[i]].desc);   */
  take_action(state,count);
}


read_score_file(){
  char file[500];
  int fd,i;

  /*  strcpy(file,scorename);
      strcat(file,".score");*/

  strcpy(file,score_dir);
  strcat(file,scoretag);
  strcat(file,".score");


  fd = open(file,0);
  if ( fd == -1)
  {
     printf("couldn't find %s\n",file);
     printf("score_dir = %s scoretag = %s\n",score_dir,scoretag);
     exit(0);
  }
  i = read(fd,cb.buff,MAX_CHARS);
  if (i == MAX_CHARS) {
    printf("file %s too long to read\n",file);
    exit(0);
  }
  cb.num = i;
  /*  for (i =0; i < 50; i++) printf("%c %d\n",cb.buff[i],cb.buff[i]); */
}


int
is_note_on(MIDI_EVENT *p) {
  return ( ((p->command&0xf0) == NOTE_ON) && (p->volume > 0));
}

static int
is_note_off(MIDI_EVENT *p) {
  if ( ((p->command&0xf0) == NOTE_ON) && (p->volume == 0)) return(1);
  if ( (p->command&0xf0) == NOTE_OFF) return(1);
  return(0);
}


static int
mid_comp(const void *n1, const void *n2) {
  MIDI_EVENT *p1,*p2;
  int c1,c2,r;

  p1 = (MIDI_EVENT *) n1;
  p2 = (MIDI_EVENT *) n2;




  /*  if (p1->meas > p2->meas) return(1);
      if (p1->meas < p2->meas) return(-1); */
  r = rat_cmp(p1->wholerat,p2->wholerat);
  if (r != 0) return(r);
  if (p1->tag != p2->tag) return(p1->tag - p2->tag);
  c1 = ((p1->command&0xf0) == NOTE_ON) ? 3 : (p1->command == NOTE_OFF || p1->volume == 0) ? 2 : 1;  /* pedal last command */
  c2 = ((p2->command&0xf0) == NOTE_ON) ? 3 : (p2->command == NOTE_OFF || p2->volume == 0) ? 2 : 1;  /* pedal last command */

  c1 = (is_note_on(p1)) ? 3 : (is_note_off(p1)) ? 2 : 1;  /* pedal last command */
  c2 = (is_note_on(p2)) ? 3 : (is_note_off(p2)) ? 2 : 1;  /* pedal last command */

  if (p1->command == 0xb0 && p2->command == 0xb0) {
    printf("hello\n");
  }


  if ((c1-c2) != 0) return((c1-c2));
  /*  return(p1->command - p2->command); */
  if (c1 == 1 && c2 == 1) 
    return(p1->volume -p2->volume);  /* pedal offs first */
  return(p1->notenum - p2->notenum);
}


/*make_cue_list() {
    int i;
    char nn[20];
    JOINT_NORM init_start_state();

     cue.num = 0;
    for (i=0; i < score.solo.num; i++) {
	if (score.solo.note[i].cue) {
	    cue.list[cue.num].note_num = i;
	    cue.list[cue.num].trained = 1;
	    cue.list[cue.num].state_init = init_start_state(score.meastime);
	    cue.num++;
	    num2name(score.solo.note[i].num,nn);
	}
    }
}
*/



set_accom_tags() {
  int i,type;

  for (i=0; i < score.midi.num; i++) {
    score.midi.burst[i].length = (i == score.midi.num-1) 
      ? 0 : score.midi.burst[i+1].time -score.midi.burst[i].time;
    accomp_time_tag(score.midi.burst[i].wholerat, 0., score.midi.burst[i].observable_tag);
    type =  (i > 0 && score.midi.burst[i].acue == 0)  ?  ACCOM_UPDATE_NODE : ACCOM_PHRASE_START;
    strcpy(score.midi.burst[i].trainable_tag,trainable_tag(type,i));
  }

}


static int
contains_true_note_on(MIDI_EVENT e) {
  if (((e.command & 0xf0)  == NOTE_ON) && (e.volume > 0)) return(1);
  return(0);
}



void
solo_time_tag(RATIONAL rat, float time, char *tag) {
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
  strcpy(tag,"solo_");
  strcat(tag,s);
}




set_solo_tags_etc() {
  int i,type,j;

  for (i=0; i < score.solo.num; i++) {
    score.solo.note[i].length = (i == score.solo.num-1) 
      ? 0 : score.solo.note[i+1].time -score.solo.note[i].time;
    //            accomp_time_tag(score.solo.note[i].wholerat, 0., score.solo.note[i].observable_tag);
	    solo_time_tag(score.solo.note[i].wholerat, 0., score.solo.note[i].observable_tag);
    type =  (i > 0 && score.solo.note[i].cue == 0)  ?  SOLO_UPDATE_NODE : SOLO_PHRASE_START;
    //    strcpy(score.solo.note[i].trainable_tag,trainable_tag(type,i));


    for (j=0; j < score.solo.note[i].action.num; j++) {
      if (contains_true_note_on(score.solo.note[i].action.event[j])) break;
    }
    //    score.solo.note[i].obs_var =  (j == score.solo.note[i].action.num) ? 10 : 1;
    score.solo.note[i].obs_var =  
#ifdef ROMEO_JULIET
      (j == score.solo.note[i].action.num) ? HUGE_VAL : 10;  // this val definitely has effect
    //      (score.solo.note[i].cue) ? 10 : HUGE_VAL;
#else
      (j == score.solo.note[i].action.num) ? HUGE_VAL : 1;
#endif
    /* this is really where obs var controlled.  this variable definitely does something*/
  }
}





thin_comp(const void *n1, const void *n2) {
  MIDI_BURST *p1,*p2;

  p1 = (MIDI_BURST *) n1;
  p2 = (MIDI_BURST *) n2;
  return(p1->mark-p2->mark);
}

static int
wr_comp(const void *n1, const void *n2) {
  MIDI_BURST *p1,*p2;

  p1 = (MIDI_BURST *) n1;
  p2 = (MIDI_BURST *) n2;
  return(rat_cmp(p1->wholerat,p2->wholerat));
}


static int
set_thin_marks() {
  int i,j,m[MAX_EVENTS],b[MAX_EVENTS],bb[MAX_EVENTS],mm[MAX_EVENTS];
  int cl,cr,cr1,cr2;
  RATIONAL wr,beat,rem,q;
  char temp[500];

  for (i=0; i < score.midi.num; i++) {
    wr = score.midi.burst[i].wholerat;
    m[i] = wholerat2measnum(wr);
    rem = wholerat2measrat(wr);
    mm[i] = (rem.num == 0 && m[i] > 1) ? m[i] : m[i]+1;  //meas if giving barline notes to prev measure
    beat = score.measdex.measure[m[i]].beat;
    if (beat.den == 0)
	 { printf("maybe not enough measures meas = %d i = %d\n");
	 return(0);  }
    q = div_rat(rem,beat);
    b[i] = q.num/q.den;
    rem = sub_rat(score.measdex.measure[mm[i]].wholerat,wr);
	q = div_rat(rem,beat);
    bb[i] = q.num/q.den;
  }
  for (i=0; i < score.midi.num; i++) {
    //    printf("%s\n",score.midi.burst[i].observable_tag);
    cl = (i > 0 && m[i-1] == m[i] && b[i-1] == b[i]); // is in interior of on left
    cr = (i < (score.midi.num-1) && mm[i+1] == mm[i] && bb[i+1] == bb[i]); // is in interior of on right
    if (score.midi.burst[i].xchg) score.midi.burst[i].mark = 0; 
    else score.midi.burst[i].mark = (cl && cr);

    //    printf("%s\tm = %d\tb = %d\tmm = %d\tbb = %d\tmark = %d\n",score.midi.burst[i].observable_tag,m[i],b[i],mm[i],bb[i],score.midi.burst[i].mark);
  }
  // maybe want this but not tested 
/*  for (j=0; j < tempo_list.num; j++) {  // tempo changes shouldn't be eliminated
	i = coincident_midi_index(tempo_list.el[j].wholerat);
	wholerat2string(tempo_list.el[j].wholerat,temp);
	if (i != -1) score.midi.burst[i].mark = 0;
  }   */
  return(1);
}


static int
thin_acc_list() {
  int i;
  if (set_thin_marks() == 0) return(0);
  qsort(score.midi.burst, score.midi.num, sizeof(MIDI_BURST),thin_comp); 
  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].mark) break;
  score.midi.num = i;  
  qsort(score.midi.burst, score.midi.num, sizeof(MIDI_BURST),wr_comp); 
  return(1);
  //  for (i=0; i < score.midi.num; i++) printf("%s %d\n",score.midi.burst[i].observable_tag,score.midi.burst[i].mark);exit(0);
}



static int
old_thin_acc_list() {
  int i,j,m[MAX_EVENTS],b[MAX_EVENTS],bb[MAX_EVENTS],mm[MAX_EVENTS];
  int cl,cr,cr1,cr2;
  RATIONAL wr,beat,rem,q;

  for (i=0; i < score.midi.num; i++) {
    wr = score.midi.burst[i].wholerat;
    m[i] = wholerat2measnum(wr);
    rem = wholerat2measrat(wr);
    mm[i] = (rem.num == 0 && m[i] > 1) ? m[i] : m[i]+1;  //meas if giving barline notes to prev measure
    beat = score.measdex.measure[m[i]].beat;
    if (beat.den == 0)
	 { printf("maybe not enough measures meas = %d i = %d\n");
	 return(0);  }
    q = div_rat(rem,beat);
    b[i] = q.num/q.den;
    rem = sub_rat(score.measdex.measure[mm[i]].wholerat,wr);
    q = div_rat(rem,beat);
    bb[i] = q.num/q.den;
  }
  for (i=0; i < score.midi.num; i++) {
    //    printf("%s\n",score.midi.burst[i].observable_tag);
    cl = (i > 0 && m[i-1] == m[i] && b[i-1] == b[i]); // is in interior of on left
    cr = (i < (score.midi.num-1) && mm[i+1] == mm[i] && bb[i+1] == bb[i]); // is in interior of on right
    score.midi.burst[i].mark = (cl && cr);
    //    printf("%s\tm = %d\tb = %d\tmm = %d\tbb = %d\tmark = %d\n",score.midi.burst[i].observable_tag,m[i],b[i],mm[i],bb[i],score.midi.burst[i].mark);
  }  
  qsort(score.midi.burst, score.midi.num, sizeof(MIDI_BURST),thin_comp); 
  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].mark) break;
  score.midi.num = i;  
  qsort(score.midi.burst, score.midi.num, sizeof(MIDI_BURST),wr_comp); 
  return(1);
  //  for (i=0; i < score.midi.num; i++) printf("%s %d\n",score.midi.burst[i].observable_tag,score.midi.burst[i].mark);exit(0);
}




static int
collect_accom_events() {
  int i,j=0,start,split,n,a,b,type;
  char name[500];
  float time;
  RATIONAL wholerat;


  qsort(event.list,event.num,sizeof(MIDI_EVENT),mid_comp);
  /*    for (i=0; i < event.num; i++) printf("%x %d %d %f\n",event.list[i].command,event.list[i].notenum,event.list[i].volume,event.list[i].meas);   
	exit(0);*/
  start = 0;
  
  score.midi.num = 0;
  for (i=0; i < event.num; i++) {
    split = (i == event.num-1);

    if (split == 0) split = (rat_cmp(event.list[i].wholerat,event.list[i+1].wholerat) != 0);
    if (split) {
      n = score.midi.num;
      score.midi.burst[n].time = event.list[i].meas;
      wholerat = score.midi.burst[n].wholerat = event.list[i].wholerat;
      score.midi.burst[n].xchg = score.midi.burst[n].acue = 0;
      for (j=0; j < accom_cue_list.num; j++) {
	if (strcmp(accom_cue_list.what[j],"cue") == 0) 

          if (rat_cmp(wholerat, accom_cue_list.rat[j]) == 0) {
	    score.midi.burst[n].acue = 1;
	    //	    printf("cue at %f\n",score.midi.burst[n].time);  exit(0);
	  }
      }
      score.midi.burst[n].action.num =  i+1-start;
      score.midi.burst[n].action.event = event.list + start;
      start = i+1;
      score.midi.num++;
      if (score.midi.num == MAX_ACCOMP_NOTES) {
	printf("out of room for MIDI notes (%d) at %d/%d\n",score.midi.num,wholerat.num,wholerat.den);
	return(0);
      }
    }
  }
  return(1);
}


static int
set_accom_events() {
  /*  int a,b,c;
  a = sizeof(MIDI_BURST);
  b = MAX_ACCOMP_NOTES;
  c = sizeof(SOUND_NUMBERS); 
  printf("%d %d %d\n",a,b,a*b); exit(0);    */

  if (collect_accom_events() == 0) return(0);
  set_accom_tags();
#ifdef ROMEO_JULIET
    
#else
  //  return(1);  // don't thin
    if (midi_accomp == 0) if (thin_acc_list() == 0) return(0);
#endif
  return(1);
}
      
  
static void
set_solo_events() {
  int i,j=0,start,split,n,a,b,type,num;
  char name[500];
  float time;
  RATIONAL wholerat;


  qsort(sevent.list,sevent.num,sizeof(MIDI_EVENT),mid_comp);
//  for (i=0; i < 10; i++) printf("set_solo_events: %x\t%d\t%d\t%d/%d\t%f\n",sevent.list[i].command,sevent.list[i].notenum,sevent.list[i].volume,sevent.list[i].wholerat.num,sevent.list[i].wholerat.den,sevent.list[i].meas);   


  
  start = 0;
  
  score.solo.num = 0;
  for (i=0; i < sevent.num; i++) {
    split = (i == sevent.num-1);

    if (split == 0) split = (rat_cmp(sevent.list[i].wholerat,sevent.list[i+1].wholerat) != 0);
    if (split) {
      n = score.solo.num;
      score.solo.note[n].time = sevent.list[i].meas;
      wholerat = score.solo.note[n].wholerat = sevent.list[i].wholerat;
      score.solo.note[n].xchg = score.solo.note[n].cue = score.solo.note[n].rhythmic_rest = 0;
      //      for (j=0; j < accom_cue_list.num; j++) {
      //	if (strcmp(accom_cue_list.what[j],"cue") == 0) 
      //          if (rat_cmp(wholerat, accom_cue_list.rat[j]) == 0) {
      //	    score.solo.note[n].cue = 1;
      //	    //	    printf("cue at %f\n",score.solo.note[n].time);  exit(0);
      //	  }
      //      }
      score.solo.note[n].action.num =  i+1-start;
      score.solo.note[n].action.event = sevent.list + start;
      start = i+1;
      score.solo.num++;
      if (score.solo.num > MAX_SOLO_NOTES) 
	{ printf("out of room in set_solo_events()\n"); exit(0); }
    }
  }
  for (i=0; i < score.solo.note[0].action.num; i++)
    printf("note[%d] = %d\n",i,score.solo.note[0].action.event[i].notenum);
}
      
  
/*static void
default_updates() {
  int i,firsts,lasts,firsta,lasta;
  float length,next_length;
  MATRIX A;
 

  if (strcmp("weinen",scorename) == 0) printf("weinen\n");
e,scorename);
  strcat(file,".score");
  fd = open(file,0);
  if ( fd == -1)
  {
     printf("couldn't find %s\n",file);
     return(0);
  }
  i = read(fd,cb.buff,MAX_CHARS);
  if (i == MAX_CHARS) {
    printf("file %s too long to read\n",file);
    exit(0);
  }
  cb.num = i;
}*/


/*make_cue_list() {
    int i;
    char nn[20];
    JOINT_NORM init_start_state();

     cue.num = 0;
    for (i=0; i < score.solo.num; i++) {
	if (score.solo.note[i].cue) {
	    cue.list[cue.num].note_num = i;
	    cue.list[cue.num].trained = 1;
	    cue.list[cue.num].state_init = init_start_state(score.meastime);
	    cue.num++;
	    num2name(score.solo.note[i].num,nn);
	}
    }
}
*/

  
static void
default_weinen_updates() {
  int i,firsts,lasts,firsta,lasta;
  float length,next_length;
  MATRIX A;
 

  for (i=0; i < score.midi.num; i++) {
    if (i == score.midi.num-1) length = 0;
    else length =   score.midi.burst[i+1].time - score.midi.burst[i].time;
    score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,length));

    if (i > 0) {
      length = score.midi.burst[i].time - score.midi.burst[i-1].time;
      next_length = score.midi.burst[i+1].time - score.midi.burst[i].time;
      A =  score.midi.burst[i-1].kalman_gain;
	score.midi.burst[i].qf_update = 
      QFperm(QFtempo_change_pos_slop(next_length));   
	/*      if (score.midi.burst[i].coincides) 
	score.midi.burst[i].qf_update = 
	   else 
	   QFperm(QFtempo_change(next_length));   */
      /*      score.midi.burst[i].qf_update = QFperm(QFtempo_change(length));  */
		/*     score.midi.burst[i].qf_update = QFperm(QFimmediate_tempo_change(A,length));   */
	      /*	      	          score.midi.burst[i].qf_update = QFperm(QFimmediate_tempo_change_slop(A,length));   */
      if (score.midi.burst[i].time > 13.5)
	score.midi.burst[i].qf_update = QFperm(QFfollow_at_all_costs(A,length));  
      /*      score.midi.burst[i].qf_update = QFperm(QFpos_slop(length));*/
      /*                  score.midi.burst[i].qf_update = QFperm(QFmeas_size_slop(10.*sqrt(length)));  */
      /*      score.midi.burst[i].qf_update = QFperm(QFunif(ACCOM_DIM)); */
    }
  }
  for (i=0; i < cue.num; i++) {
    phrase_span(i,&firsts,&lasts,&firsta,&lasta);
    printf("accompaniment note = %d time = %f\n",firsta,score.midi.burst[firsta].time);
    /*    score.midi.burst[firsta].qf_update = QFperm(QFunif_accel_zero(ACCOM_DIM));*/
    score.midi.burst[firsta].qf_update = QFperm(QFmeas_size(score.meastime,1.));  
    /*	    QFprint(score.midi.burst[firsta].qf_update); 
	    exit(0); */
  } 
  /*  score.midi.burst[4].qf_update.m.el[1][0] = 2.; 
    score.midi.burst[16].qf_update.m.el[1][0] =2.;  */

   
  /*  score.midi.burst[84].qf_update = QFperm(QFmeas_size(1.5,.3)); /* fixed value */
  /*  score.midi.burst[100].qf_update.m.el[1][0] = -.3; 
  score.midi.burst[101].qf_update.m.el[1][0] = -.3; 
  score.midi.burst[102].qf_update.m.el[1][0] = -.3; 
  score.midi.burst[103].qf_update.m.el[1][0] = -.3; 
  score.midi.burst[104].qf_update.m.el[1][0] = -.3;   */
  /*  score.midi.burst[97].qf_update.m.el[1][0] = .8;   
  score.midi.burst[113].qf_update.m.el[1][0] = .4;
  score.midi.burst[112].qf_update.m.el[1][0] = .4; */
}    

static void
default_habanera_updates() {
  int i,firsts,lasts,firsta,lasta;
  float length,next_length;
  MATRIX A;
 

  printf("must account for this later\n");
  for (i=0; i < score.midi.num; i++) {
    if (i == score.midi.num-1) length = 0;
    else length =   score.midi.burst[i+1].time - score.midi.burst[i].time;
    score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,length));
    if ((i == 0) || score.midi.burst[i].acue)  {
      score.midi.burst[i].qf_update = QFperm(QFmeas_size(score.meastime,1.));  
      /*      if (fabs(score.midi.burst[i].time - 49.) < .001) 
	      score.midi.burst[i].qf_update = QFperm(QFmeas_size(3.,1.));   */
    } 
    else {
      length = score.midi.burst[i].time - score.midi.burst[i-1].time;
      score.midi.burst[i].qf_update = 
	QFperm(QFtempo_change_pos_slop_parms(length*.5, length*1.));   
    }
  }
}

static void
default_reverie_updates() {
  int i,firsts,lasts,firsta,lasta;
  float length,next_length;
  MATRIX A;
 

  printf("must account for this later\n");
  for (i=0; i < score.midi.num; i++) {
    if (i == score.midi.num-1) length = 0;
    else length =   score.midi.burst[i+1].time - score.midi.burst[i].time;
    score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,length));
    if ((i == 0) || score.midi.burst[i].acue)  {
      score.midi.burst[i].qf_update = QFperm(QFmeas_size(score.meastime,1.));  
    } 
    else {
      length = score.midi.burst[i].time - score.midi.burst[i-1].time;
      score.midi.burst[i].qf_update = 
	QFperm(QFtempo_change_pos_slop_parms(length*.5, length*1.));   
    }
  }
}

static void
old_default_habanera_updates() {
  int i,firsts,lasts,firsta,lasta;
  float length,next_length;
  MATRIX A;
 

  for (i=0; i < score.midi.num; i++) 
    score.midi.burst[i].prior  = QFperm(QFunif(ACCOM_DIM));
  for (i=0; i < score.midi.num; i++) {
    if (i == score.midi.num-1) length = 0;
    else length =   score.midi.burst[i+1].time - score.midi.burst[i].time;
    score.midi.burst[i].kalman_gain = Mperm(evol_matrix(ACCOM_DIM,length));
    score.midi.burst[i].connect = 1;

    if (i > 0) {
      length = score.midi.burst[i].time - score.midi.burst[i-1].time;
      next_length = score.midi.burst[i+1].time - score.midi.burst[i].time;
      A =  score.midi.burst[i-1].kalman_gain;
	score.midi.burst[i].qf_update = 
	  QFperm(QFtempo_change_pos_slop_parms(length*.5, length*1.));   
	/*	if (fabs(score.midi.burst[i].time - 45.5) < .001) {
	  score.midi.burst[i].qf_update = 
	    QFperm(QFtempo_change_pos_slop_parms(10000.,10000.));
	    } */
	/*	QFcheck(score.midi.burst[i].qf_update);    */
    }
    if (fabs(score.midi.burst[i].time - 0.) < .001) {
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
    }
    if (fabs(score.midi.burst[i].time - 12.) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
      }
    /*    if (fabs(score.midi.burst[i].time - 19.375) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
      }   */
    /*  if (fabs(score.midi.burst[i].time - 17.) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));   
      }*/
    if (fabs(score.midi.burst[i].time - 24.) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
      }
    if (fabs(score.midi.burst[i].time - 28.9375 /*29.*/) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
      }
    if (fabs(score.midi.burst[i].time - 37.5) < .001) {
      score.midi.burst[i].connect = 0;
      /*      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  */
      }
    if (fabs(score.midi.burst[i].time - 40.) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
      }
      if (fabs(score.midi.burst[i].time - 52.) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.0,0.));  
      }
      if (fabs(score.midi.burst[i].time - (59.-1./10.)) < .001) {
      score.midi.burst[i].connect = 0;
      score.midi.burst[i].prior = QFperm(QFmeas_size(2.2,0.));  
      }
    if (fabs(score.midi.burst[i].time - 29.) < .001) {
      score.midi.burst[i].connect = 0;
      }
    /*    if (fabs(score.midi.burst[i].time - 33.25) < .001) {
      score.midi.burst[i].connect = 0;
      } */









       /*        if (fabs(score.midi.burst[i].time - 40.5) < .001) 
	  score.midi.burst[i].prior = QFperm(QFmeas_size(1.2,0.));   
       if (fabs(score.midi.burst[i].time - 37.5) < .001) 
	 score.midi.burst[i].prior = QFperm(QFmeas_size(2.2,0.));   
    */
  }
  /*    for (i=0; i < cue.num; i++) {
    phrase_span(i,&firsts,&lasts,&firsta,&lasta);
    printf("accompaniment note = %d time = %f\n",firsta,score.midi.burst[firsta].time);
    score.midi.burst[firsta].prior = QFperm(QFmeas_size(score.meastime,1.));  
    }    */
}

static void
default_updates() {
  int i,firsts,lasts,firsta,lasta;
  float length,next_length;
  MATRIX A;
 

  if (strcmp("weinen",scorename) == 0) default_weinen_updates();
  else if (strcmp("habanera",scorename) == 0) default_habanera_updates();
  else if (strcmp("reverie",scorename) == 0) default_reverie_updates();
  else     printf("no default updates\n");

}    






static void
set_sounding_notes() {
  MIDI_EVENT_LIST nt_evt;
  int i,j,k,aux[MIDI_PITCH_NUM],attack[MIDI_PITCH_NUM],n,p,v,c;

  for (i=0; i<MIDI_PITCH_NUM; i++) aux[i]=0;
  
  // finding the current sounding notes

  for (j = 0; j < score.midi.num; j++) {
    //    printf("%s\n",score.midi.burst[j].observable_tag);
    for (p=0; p<MIDI_PITCH_NUM; p++) attack[p] = 0;
    nt_evt = score.midi.burst[j].action;
    for (i = 0; i < nt_evt.num; i++) {
      n = nt_evt.event[i].notenum;
      v = nt_evt.event[i].volume;
      c = nt_evt.event[i].command & 0xf0;


      if ((c == NOTE_ON) && (v > 0)) { aux[n]++; attack[n] = 1; }
      else if ( (c == NOTE_OFF) || (c == NOTE_ON && v == 0)) aux[n]--;
      else if (c == PEDAL_COM) printf("need to fix this to account for sustain pedal\n");
      else printf("unknown midi command %s\n");


      //      printf("j = %d n = %d cmd = %x vel = %d aux = %d\n",j,n,nt_evt.event[i].command ,v,aux[n]);

      /*      if ((nt_evt.event[i].command & 0xf0) == NOTE_ON) aux[n] = 1;
      else {
	if ((nt_evt.event[i].command & 0xf0) == NOTE_OFF || nt_evt.event[i].volume == 0)
	  aux[n] = 0;
	else if (nt_evt.event[i].command == PEDAL_COM) {
	  printf("need to fix this to account for sustain pedal\n");
	}
	else printf("unknown midi command %s\n");
	}*/


    } // aux gives the indexes to all currently sounding notes
    k=0;
    for (p=0; p<MIDI_PITCH_NUM; p++) {
      if (aux[p] >= 1) {
	if (k >= MAX_SOUNDING_NOTES) { printf("out of room with MAX_SOUNDING_NOTES\n"); exit(0); }
	// this won't work if we are subsampling the orchestra positions.
	score.midi.burst[j].snd_notes.snd_nums[k] = p;
	score.midi.burst[j].snd_notes.attack[k] = attack[p];
	//		printf("chord = %d midi = %d pos = %d/%d attack = %d\n",j,p,score.midi.burst[j].wholerat.num,score.midi.burst[j].wholerat.den,attack[p]);
	++k;
      }
    }
    score.midi.burst[j].snd_notes.num = k;
  }
}


static MIDI_EVENT*
associated_note_event(int pitch, MIDI_EVENT *ptr) {
  while (ptr->notenum != pitch) ptr--;
  return(ptr);
}


static void
set_sounding_solo_notes() {
  MIDI_EVENT_LIST nt_evt;
  int i,j,k,aux[MIDI_PITCH_NUM],attack[MIDI_PITCH_NUM],n,p,v,c,trill[MIDI_PITCH_NUM],t;
  MIDI_EVENT *e;
  RATIONAL r;

  for (i=0; i<MIDI_PITCH_NUM; i++) trill[i] = aux[i]=0;
  
  for (j = 0; j < score.solo.num; j++) {
    //   printf("%s\n",score.midi.burst[j].observable_tag);
    for (p=0; p<MIDI_PITCH_NUM; p++) attack[p] = 0;
    nt_evt = score.solo.note[j].action;
    for (i = 0; i < nt_evt.num; i++) {
      //            printf("notenum = %d vol = %d com = %d\n",n,v,c);
      n = nt_evt.event[i].notenum;
      v = nt_evt.event[i].volume;
      c = nt_evt.event[i].command & 0xf0;
      t = nt_evt.event[i].trill;
      //      if (t) exit(0);
      
      //      printf("j = %d n = %d c = %x v = %d\n",j,n,c,v);
      if ((c == NOTE_ON) && (v > 0)) { 
	attack[n] = 1; 
	aux[n]++;
	//	if (aux[n] > 1) aux[n] = 1;
	trill[n] = t;
	//	if (j == 1396) printf("aux[%d] = %d\n",n,aux[n]);
      }
      else if ( (c == NOTE_OFF) || (c == NOTE_ON && v == 0)) { 	
	aux[n]--; 
	//	if (aux[n] < 0) aux[n] = 0;
	trill[n] = 0; 
      }
      else if (c == PEDAL_COM) printf("need to fix this to account for sustain pedal\n");
      else printf("unknown midi command %s\n");
    } // aux gives the indexes to all currently sounding notes
    k=0;
    for (p=0; p<MIDI_PITCH_NUM; p++) {
      //      if (aux[p] < 0) { printf("aux[%d] = %d\n",p,aux[p]); exit(0); }
      if (aux[p] >= 1) {
	if (k >= MAX_SOUNDING_NOTES) { printf("out of room with MAX_SOUNDING_NOTES\n"); exit(0); }
	e = associated_note_event(p,&nt_evt.event[i-1]);
	r = sub_rat(score.solo.note[j].wholerat,e->wholerat);
    //	score.solo.note[j].snd_notes.age[k] = (r.num * score.meastime)/r.den;
	score.solo.note[j].snd_notes.vel[k] = e->volume;
	score.solo.note[j].snd_notes.snd_nums[k] = p;
	score.solo.note[j].snd_notes.trill[k] = trill[p];
	if (trill[p]) score.solo.note[j].trill = 1;
	score.solo.note[j].snd_notes.attack[k] = attack[p];	
	//	printf("chord = %d midi = %d attack = %d\n",j,p,attack[p]);
	++k;
      }
    }
    score.solo.note[j].snd_notes.num = k;
    //    printf("k= %d\n",k);
  }
}




static int
accom_com_comp(const void *e1, const void *e2) {
  ACCOM_COMMAND *p1, *p2;

  p1 = (ACCOM_COMMAND *) e1;
  p2 = (ACCOM_COMMAND *) e2;
  if (p2->time == p1->time) return(0);
  if (p2->time > p1->time) return(-1);
  if (p2->time < p1->time) return(1);
}




static void
process_accom_commands() {
  int i,j=0,k;
  float a1,a2;
  MATRIX v;
  
  qsort(comm.list,comm.num,sizeof(ACCOM_COMMAND),accom_com_comp);
  for (i=0; i < comm.num; i++) {
    while (fabs(score.midi.burst[j].time -comm.list[i].time) > .0001) j++;
    if (strcmp(comm.list[i].command,"set_meas_time") == 0) {
      a1 = comm.list[i].arg[0];
      a2 = comm.list[i].arg[1];
      score.midi.burst[j].qf_update = 	QFperm(QFmeas_size(a1,a2));
      printf("set meas time of note %d (%f) to m=%f std=%f\n",
	     j,comm.list[i].time,a1,a2);
    }
    if (strcmp(comm.list[i].command,"a_tempo") == 0) {
      v = Mzeros(ACCOM_DIM,1);
      v.el[0][1] =  -comm.list[i].arg[0];
      score.midi.burst[j].qf_update = 	QFperm(QFpoint(v));
      printf("set meas time of note %d (%f) to m=%f\n", 
	     j,comm.list[i].time,a1);
    }
    if (strcmp(comm.list[i].command,"rit") == 0) {
      v = Mzeros(ACCOM_DIM,1);
      v.el[0][1] =  comm.list[i].arg[0];
      score.midi.burst[j].qf_update = 	QFperm(QFpoint(v));
      printf("set meas time of note %d (%f) to m=%f\n", 
	     j,comm.list[i].time,a1);
    }
  }
}

set_coincidences() {
  int i,j;

  for (i=0; i < score.midi.num; i++) score.midi.burst[i].coincides = 0;
  i = j = 0;
  while (i < score.solo.num && j < score.midi.num) {
      
    if   (rat_cmp(score.solo.note[i].wholerat, score.midi.burst[j].wholerat) == 0) 
    score.midi.burst[j].coincides = 1;
    if (rat_cmp(score.solo.note[i].wholerat,score.midi.burst[j].wholerat) < 0) i++;
    else j++;
    /*    printf("%d %d %f %f\n",i,j,score.solo.note[i].time, score.midi.burst[j].time); */
  }
  /*  for (i=0; i < score.midi.num; i++) printf("i = %d coin = %d\n",i,score.midi.burst[i].coincides);
      exit(0); */
}




static int
accompaniment_comp(const void *e1, const void *e2) {
  ACCOMPANIMENT_NOTE *p1,*p2;

  p1 = (ACCOMPANIMENT_NOTE *) e1;
  p2 = (ACCOMPANIMENT_NOTE *) e2;
  if (p2->meas > p1->meas) return(-1);
  if (p2->meas < p1->meas) return(1);
  return(p1->pitch - p2->pitch);
}





static void
connect_midi_and_accompaniment() {
  int i,j,k;
  char tag[50];
  MIDI_EVENT *m;

  for (i=0; i < score.midi.num; i++)   for (j=0; j < score.midi.burst[i].action.num; j++) {
    m = &score.midi.burst[i].action.event[j];
    if ((m->command & 0xf0) != NOTE_ON) continue;
    observable_tag(m->notenum,m->wholerat,tag);
    for (k=0; k < score.accompaniment.num; k++) if (strcmp(tag,score.accompaniment.note[k].observable_tag) == 0) break;
    if (k == score.accompaniment.num) {
      printf("couldn't match %s\n",tag);
      exit(0);
    }
    m->accomp_note = &score.accompaniment.note[k];
    /*    printf("%d hooked \n",k); */
    score.accompaniment.note[k].note_on = m;
    if (score.accompaniment.note[k].note_on->command != NOTE_ON) { printf("problem in connect_midi_...\n"); exit(0); }
  }
}

static int
rat2meas_num(RATIONAL pos_rat) {
  int i;

  for (i=1; i < score.measdex.num; i++) {
    if ( (rat_cmp(pos_rat,score.measdex.measure[i].wholerat) >= 0)  && (rat_cmp(pos_rat,score.measdex.measure[i+1].wholerat) < 0)) break;
  }
    /*  printf("score_pos = %f  index = %f\n",score_pos,score.measdex.measure[i]);*/
  return(i);
}

int
solo_measure_empty(int m) {
  int i;
  RATIONAL start,end,w;

  start = score.measdex.measure[m].wholerat;
  end = add_rat(start,score.measdex.measure[m].time_sig);
  for (i=0; i < score.solo.num; i++) {
    w = score.solo.note[i].wholerat;
    if (rat_cmp(w, start) < 0) continue;
    if (rat_cmp(w, end) <= 0) return(0);
    else return(1);
  }
  return(1);
}

//CF:  compute spectrum model for each score state.
//CF:  They are stored in the SCORE's representation of the notes.
//CF:  (Later, GNODEs will refer to these to get the models)
static void
set_poly_solo_chord_spectra() {
  int i,j,k,pk;
  float *s1,*s2,*outs,p,t;
  SOUND_NUMBERS trill_chord;
  SOLO_NOTE *sn;

  for (i=0; i < score.solo.num; i++) {  //CF:  for each solo note
    sn = score.solo.note + i;
    score.solo.note[i].spect = (float *) malloc(sizeof(float) * FREQDIM/2);
    score.solo.note[i].attack_spect = (float *) malloc(sizeof(float) * FREQDIM/2);
    //    superpos_model(score.solo.note[i].snd_notes, score.solo.note[i].spect);

    //    solo_gauss_mixture_model(score.solo.note[i].snd_notes, score.solo.note[i].spect);  //CF:  create speactrum for score state****
    solo_gauss_mixture_model(sn->snd_notes, sn->spect, &(sn->peaks));  //CF:  create speactrum for score state****

    /*    for (j=0; j < TUNE_LEN; j++) {
      score.solo.note[i].tune_spect[j] = (float *) malloc(sizeof(float) * FREQDIM/2);
      t = -.5 + .5*2*j/(float) (TUNE_LEN -1);
      //      t = .2;
      t = 0;
      solo_gauss_mixture_model_tune(score.solo.note[i].snd_notes, t,score.solo.note[i].tune_spect[j]);  
      }*/

    
    gauss_mixture_model_attack(score.solo.note[i].snd_notes, score.solo.note[i].attack_spect);

    //CF:  this chunk is old code for trill spectra
    if (score.solo.note[i].trill == 0) continue;
    score.solo.note[i].trill_spect = (float *) malloc(sizeof(float) * FREQDIM/2);
    trill_chord = score.solo.note[i].snd_notes;
    for (j=0; j < trill_chord.num; j++) if (trill_chord.trill[j]) {
      //      printf("changing %d to %d\n",trill_chord.snd_nums[j], trill_chord.trill[j] );
      trill_chord.snd_nums[j] = trill_chord.trill[j]; 
    }
    solo_gauss_mixture_model(trill_chord, score.solo.note[i].trill_spect,&pk);
  }

  //CF:  blend spectra at note boundaries (if MELD_LEN=0, then no blending)
  for (i=0; i < score.solo.num; i++)  {
    s1 = (i == 0) ? restspect : score.solo.note[i-1].spect;
    s2 = score.solo.note[i].spect;
    for (j=0; j < MELD_LEN; j++) {   
      p = (MELD_LEN - j) / (float) (MELD_LEN + 1);
      outs = score.solo.note[i].meld_spect[j] = (float *) malloc(sizeof(float) * FREQDIM/2);
      simple_meld_spects(s1,s2,outs,p);
      
    }
  }
}

static void
set_solo_chord_spectra() {
  int i,j;
  float *s1,*s2,*outs,p;
  SOUND_NUMBERS trill_chord;

  for (i=0; i < score.solo.num; i++) {
    score.solo.note[i].spect = (float *) malloc(sizeof(float) * FREQDIM/2);
    //    superpos_model(score.solo.note[i].snd_notes, score.solo.note[i].spect);
    if (score.solo.note[i].num == RESTNUM) score.solo.note[i].snd_notes.num = 0;
    else {
      score.solo.note[i].snd_notes.num = 1;
      score.solo.note[i].snd_notes.snd_nums[0] = score.solo.note[i].num;
    }
    gauss_mixture_model(score.solo.note[i].snd_notes, score.solo.note[i].spect);
    // gauss_mixture_model_attack(score.solo.note[i].snd_notes, score.solo.note[i].spect,0.);
    if (score.solo.note[i].trill == 0) continue;
    score.solo.note[i].trill_spect = (float *) malloc(sizeof(float) * FREQDIM/2);
    trill_chord = score.solo.note[i].snd_notes;
    for (j=0; j < trill_chord.num; j++) if (trill_chord.trill[j]) {
      //      printf("changing %d to %d\n",trill_chord.snd_nums[j], trill_chord.trill[j] );
      trill_chord.snd_nums[j] = trill_chord.trill[j]; 
    }
    gauss_mixture_model(trill_chord, score.solo.note[i].trill_spect);
  }
  for (i=0; i < score.solo.num; i++)  {
    s1 = (i == 0) ? restspect : score.solo.note[i-1].spect;
    s2 = score.solo.note[i].spect;
    for (j=0; j < MELD_LEN; j++) {
      p = (MELD_LEN - j) / (float) (MELD_LEN + 1);
      outs = score.solo.note[i].meld_spect[j] = (float *) malloc(sizeof(float) * FREQDIM/2);
      meld_spects(s1,s2,outs,p);
      
    }
  }
}



static void
set_note_times() {
  int i;
  float ws;
  RATIONAL d;
  
  score.midi.burst[0].secs = 0;
  for (i=1; i < score.midi.num; i++) {
    ws = get_whole_secs(score.midi.burst[i].wholerat);
    d = sub_rat(score.midi.burst[i].wholerat,score.midi.burst[i-1].wholerat);
    score.midi.burst[i].secs = score.midi.burst[i-1].secs + rat2f(d)*ws;
    //    printf("%d %f\n",i,score.midi.burst[i].secs);
  }
}


static int
terminal_index(int pitch, int start_index) {  // index on which the note ends considering pedaling
  int i,j,note_off=0,pedal=0;
  MIDI_EVENT m;

  for (i = start_index+1; i < score.midi.num; i++) {
    for (j=0; j < score.midi.burst[i].action.num; j++) {
      m = score.midi.burst[i].action.event[j];
      if (m.command == PEDAL_COM) pedal = (m.volume > 30);  // pedal = 1/0 
      if (m.notenum == pitch) {
	if (is_note_off(&m)) note_off = 1;
	if (is_note_on(&m)) note_off = 0;
      } 
    }
    if (note_off == 1 && pedal == 0 && pitch < 84) return(i);  // high notes never damped
  }
  return(i);
}



static void
remove_excess_notes(int *m, float *age) {
  int i,count=0,index[128],maxi;
  float maxa;

  for (i=0; i < 128; i++) index[i] = i;
  for (i=0; i < 128; i++) if (m[i]) count++;
  while (count > MAX_SOUNDING_NOTES) {
    for (maxa = i=0; i < 128; i++) if (m[i] && age[i] > maxa) { maxa = age[i]; maxi = i; }
    m[maxi] = 0;
    count--;
  }
}


static void
recent_piano_notes(int index, SOUND_NUMBERS *s, int include_last) {  // age ,m have 128 values for all pitches
  int i,j,term,mp, m[128];
  float age[128];
  
  //  printf("index = %d\n",index);
  for (i=0; i < 128; i++) m[i] = age[i] = 0;
  for (i=0; i <= index; i++) {
    if (score.midi.burst[i].time < score.midi.burst[index].time-2.) continue;
    for (j=0; j < score.midi.burst[i].action.num; j++) {
      if (is_note_on(score.midi.burst[i].action.event + j) == 0) continue;
      mp = score.midi.burst[i].action.event[j].notenum;
      term = terminal_index(mp, i);
      if (include_last == 0 && term <= index) continue;
      if (include_last == 1 && term < index) continue;
      m[mp] = 1;
      age[mp] = score.midi.burst[index].time - score.midi.burst[i].time;
    }
  }
  remove_excess_notes(m,age);
  s->num =0;
  for (i=0; i < 128; i++) {
    if (m[i] == 0) continue;
    s->snd_nums[s->num] = i;
    s->age[s->num] = age[i];
    s->num++;
  }
}


static void
set_orchestra_chord_spectra() {
  int i,j,include_last;
  float *s1,*s2,*outs,p;
  char notes[500];
  SOUND_NUMBERS sn;

#ifdef PIANO_RECOG
  set_note_times();
#endif
  for (i=0; i < score.midi.num; i++) {
    score.midi.burst[i].spect = (float *)  malloc(sizeof(float) * FREQDIM/2);
    //    sndnums2string(score.midi.burst[i].snd_notes,notes);
    //    printf("i = %d num = %d notes = %s\n",i,score.midi.burst[i].snd_notes.num,notes);
#ifdef PIANO_RECOG
    score.midi.burst[i].atck_spect = (float *)  malloc(sizeof(float) * FREQDIM/2);
    recent_piano_notes(i,&sn,include_last = 0);
        printf("i = %d notes = %d at %s\n",i,sn.num,score.midi.burst[i].observable_tag);
    for (j=0; j < sn.num; j++)
    printf("%d %f\n",sn.snd_nums[j],sn.age[j]);

    piano_mixture_model(sn, score.midi.burst[i].spect);
    recent_piano_notes(i,&sn,include_last = 1);
    piano_mixture_model(sn, score.midi.burst[i].atck_spect);



    //    gauss_mixture_model(score.midi.burst[i].snd_notes, score.midi.burst[i].spect);
    /*
    score.midi.burst[i].atck_spect = (float *)  malloc(sizeof(float) * FREQDIM/2);
    piano_mixture_model(score.midi.burst[i].snd_notes, score.midi.burst[i].spect);
    atck_piano_mixture_model(score.midi.burst[i].snd_notes, score.midi.burst[i].atck_spect);*/
#else
        gauss_mixture_model(score.midi.burst[i].snd_notes, score.midi.burst[i].spect);
#endif
  }
  //        exit(0);
}




process_accompaniment() {
  int i,error = 0;
  char name[500];
  ACCOMPANIMENT_NOTE *n1,*n2;

  qsort(score.accompaniment.note,score.accompaniment.num,sizeof(ACCOMPANIMENT_NOTE),accompaniment_comp);
  for (i=0; i < score.accompaniment.num-1; i++) {
    n1 = &score.accompaniment.note[i];
    n2 = &score.accompaniment.note[i+1];
    if (accompaniment_comp(n1,n2)) continue;
    num2name(n1->pitch,name);
    printf("multiple %s's in measure %d\n",name,rat2meas_num(n1->wholerat));
    error = 1;
  }
  //    if (error) exit(0); 
}

float
meas2score_pos(char *s) {  /* the string is of the form %d+%d/%d */
  char *s1,*s2,*s3; 
  int i,n1,n2,n3;
  float x;

  i=0;
  s1 = s;
  while (s[i] != '+' && s[i]) i++;
  sscanf(s1,"%d",&n1);
  if (n1 > score.measdex.num) {
    return(score.measdex.measure[score.measdex.num].pos);
  }
  if (s[i] == 0) {
    /*    printf("returning %f\n",score.measdex.measure[n1].pos);*/
    return(score.measdex.measure[n1].pos);
  }
  i++;
  s2 = s+i;
  while (s[i] != '/' && s[i]) i++;
  if (s[i] == 0) {
    printf("string %s unparsable in meas2score_pos()\n",s);
    exit(0);
  }
  sscanf(s2,"%d",&n2);
  i++;
  s3 = s+i;
  sscanf(s3,"%d",&n3);
  x = score.measdex.measure[n1].pos + (float) n2 / (float) n3;
  /*  printf("returning %f\n",x);*/
  return(x);
}




RATIONAL
string2wholerat(char *s) {  /* the string is of the form %d+%d/%d with optional (%d) at end */
  char *s1,*s2,*s3,pick[500],trash[500]; 
  int i,n1,n2,n3,v;
  float x;
  RATIONAL tot,frac; 
  int plus = 0,paren=0;

  pick[0] = 0;
  sscanf(s,"%d/%d-%s",&frac.num,&frac.den,pick);
  if (strcmp(pick,"pickup") == 0) {
	if (frac.den == 0) { frac.num = -1; return(frac); }  // bad input
	tot = sub_rat(score.measdex.measure[1].wholerat,frac);
  //  printf("xyz frac = %d/%d meas = %d/%d\n",frac.num,frac.den,score.measdex.measure[1].wholerat.num,score.measdex.measure[1].wholerat.den);
    return(tot);
  }

  for (i=0; i < strlen(s); i++) {
    if (s[i] == '+') plus = 1;
    if (s[i] == '(') paren = 1;
  }
  v = 1;
  frac.num = 0;
  frac.den = 1;

  if (plus && paren) sscanf(s,"%d+%d/%d(%d)",&n1,&frac.num,&frac.den,&v);
  if (plus == 0 && paren) sscanf(s,"%d(%d)",&n1,&v);
  if (plus && paren == 0) sscanf(s,"%d+%d/%d",&n1,&frac.num,&frac.den);
  if (plus == 0 && paren == 0) { 
    sscanf(s,"%d",&n1);
    //    if (strcmp(trash,"")) { printf("can't parse %s strlen = %d n1 = %d\n",s,strlen(s ),n1); exit(0); }
  }

  for (i=1; i <= score.measdex.num; i++) 
    if (score.measdex.measure[i].meas == n1 && score.measdex.measure[i].visit == v) break;
  if (i > score.measdex.num) {
    printf("%s not found in measure list\n",s); tot.num = tot.den = 0; return(tot); }
  tot = add_rat(score.measdex.measure[i].wholerat,frac);
  return(tot);
}





RATIONAL
string2wholerat_old(char *s) {  /* the string is of the form %d+%d/%d */
  char *s1,*s2,*s3,pick[500]; 
  int i,n1,n2,n3,is_plus,num,den;
  float x;
  RATIONAL tot,frac;

  pick[0] = 0;
  sscanf(s,"%d/%d-%s",&frac.num,&frac.den,pick);
  if (strcmp(pick,"pickup") == 0) {
    tot = sub_rat(score.measdex.measure[1].wholerat,frac);
    return(tot);
    //    printf("got %d/%d\n",tot.num,tot.den);
    //    exit(0);
  }

  i=0;
  s1 = s;
  while (s[i] && (s[i] != '+') && (s[i] != '-')) { 
    if (s[i] > '9' || s[i] < '0') {
      printf("bad data in string2wholerat: %s\n",s);
      exit(0);
    }
    i++;
  }
  sscanf(s1,"%d",&n1);
  if (n1 > score.measdex.num) {
    return(score.measdex.measure[score.measdex.num].wholerat);
  }
  if (s[i] == 0) {
    /*    printf("returning %f\n",score.measdex.measure[n1].pos);*/
    /*    printf("n1 = %d xxx = %d/%d\n",n1,score.measdex.measure[n1].rat.num,score.measdex.measure[n1].rat.den);*/
    return(score.measdex.measure[n1].wholerat);
  }
  if (s[i] != '+' && s[i] != '-') { printf("must be +/- here\n"); exit(0); }
  is_plus = (s[i] == '+');
  i++;
  s2 = s+i;
  while (s[i] != '/' && s[i]) {
    if (s[i] > '9' || s[i] < '0') {
      printf("bad data in string2wholerat: %s\n",s);
      exit(0);
    }
    i++;
  }
  if (s[i] == 0) {
    printf("string %s unparsable in meas2score_pos()\n",s);
    exit(0);
  }
  sscanf(s2,"%d",&frac.num);
  i++;
  s3 = s+i;
  sscanf(s3,"%d",&frac.den);
  if (is_plus == 0) frac.num = -frac.num;
  tot = add_rat(score.measdex.measure[n1].wholerat,frac);
  //  printf("isplus = %d n1 = %d frac = %d/%d\n",is_plus,n1,frac.num,frac.den);
  return(tot);
}



static void
check_consecutive_solo_rests() {
  int i,flag=0;
  char temp[500];

  for (i=0; i < score.solo.num-1; i++) {
    /*    printf("%d %d/%d %s\n",i,score.solo.note[i].wholerat.num,score.solo.note[i].wholerat.den,score.solo.note[i].observable_tag); */
    //    if (score.solo.note[i].num == RESTNUM && score.solo.note[i+1].num == RESTNUM) {
    if (is_a_rest(score.solo.note[i].num) && is_a_rest(score.solo.note[i+1].num)) {
      printf("consectutive rests in solo part at position %s i = %d\n",score.solo.note[i].observable_tag,i);
      flag = 1;
    }
  }
  if (flag) exit(0);
}


/*static int
hard_solo_note_to_recognize(int i) {
  int j,k,p1,p2;
  
  p1 = score.solo.note[i].num;
//  if (p1 == RESTNUM) return(1);
  if (is_a_rest(p1)) return(1);
  if (i) p2 = score.solo.note[i-1].num;
  if ((i > 0) && ((p1%12) == (p2%12)))  return(2);  // reartic or octave jump 
  j = coincident_midi_index(score.solo.note[i].wholerat);
  if (j == -1) return(0);
  for (k=0; k < score.midi.burst[j].action.num; k++) 
    if (score.midi.burst[j].action.event[k].command == NOTE_ON &&
	score.midi.burst[j].action.event[k].notenum == p1) return(3);  // accomp has same note at same time 
  return(0);
}
*/


static void
set_waits() {
  int i,j,k;
  RATIONAL rat;

  for (i=0; i < wait_list.num; i++) {
    rat = wait_list.rat[i];
    j = coincident_midi_index(rat);
    if (j == -1) { printf("wait must match midi\n"); exit(0); }
    k = coincident_solo_index(rat);
    if (k == -1) { printf("wait must match solo\n"); exit(0); }
    score.midi.burst[j].gate = k;
    printf("%d matches %d (%s)\n",j,k,score.solo.note[k].observable_tag);
  }
  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].cue == 0) continue;
    rat = score.solo.note[i].wholerat;
    for (j=0; j < score.midi.num; j++) if (rat_cmp(rat,score.midi.burst[j].wholerat) <= 0) break;
    score.midi.burst[j].gate = i;
  }
}



#define MIN_GATE_DIFF /*.25   */ /*.5*/ -.01

static void
set_gates() {
  int i,j;
  RATIONAL dr,rl,rh,r;
  float time_diff;


  //  for (i=0; i < score.solo.num; i++) score.solo.note[i].gate = -1; 
  /* above now done in get_hand_set_*/
  for (i=0; i < score.midi.num; i++) score.midi.burst[i].gate = -1;  // default is unset
  set_waits();
  return;

  for (i=0; i < score.midi.num; i++) {
    if (strcmp(score.midi.burst[i].observable_tag,"atm_103+0/1") != 0) continue;
    if (strcmp(scorename,"oboesonata2") == 0 && 
	rat_cmp(r,score.midi.burst[i].wholerat) == 0) continue;
    for (j=score.solo.num-1; j >= 0; j--) {
      /*            printf("%s\n",score.solo.note[j].observable_tag);
		    printf("midi = %d/%d solo = %d/%d\n",score.midi.burst[i].wholerat.num,score.midi.burst[i].wholerat.den,score.solo.note[j].wholerat.num,score.solo.note[j].wholerat.den); */
      r = string2wholerat("21+15/16");  /* a kludge */
      if (strcmp(scorename,"oboesonata2") == 0 && 
	  rat_cmp(r,score.solo.note[j].wholerat) == 0) continue;

      dr = sub_rat(score.midi.burst[i].wholerat,score.solo.note[j].wholerat);
      /*           printf("%d/%d\n",dr.num,dr.den);*/
      time_diff = ((float)dr.num/(float)dr.den) * score.solo.note[j].meas_size;
      /*      printf("time_diff = %f\n",time_diff); */
      //      if (time_diff > MIN_GATE_DIFF && score.solo.note[j].num != RESTNUM) break;
      if (time_diff > MIN_GATE_DIFF && is_a_rest(score.solo.note[j].num) == 0) break;
    }
    /*            printf("%s %s\n",score.midi.burst[i].observable_tag,score.solo.note[j].observable_tag);  */
    score.midi.burst[i].gate = j;  /* could be -1 if unset */
    /*        printf("gate is %s\n",score.solo.note[j].observable_tag);*/
	 /*    printf("\n");*/
  }
}




static void
add_xchg_points() {
  int i,j;
  RATIONAL r;

  for (i=0; i < accom_cue_list.num; i++) {
    if (strcmp(accom_cue_list.what[i],"sxchg")) continue;
    r  = accom_cue_list.rat[i];
    j = coincident_solo_index(r);
    if (j == -1) { printf("couldn't find xchg point\n"); exit(0); }
    score.solo.note[j].xchg = 1;
    /*    printf("solo xchg at %d/%d\n",r.num,r.den);*/
  }
  for (i=0; i < accom_cue_list.num; i++) {
    if (strcmp(accom_cue_list.what[i],"axchg")) continue;
    r  = accom_cue_list.rat[i];
    j = coincident_midi_index(r);
    if (j == -1) { printf("couldn't find xchg point\n"); exit(0); }
    score.midi.burst[j].xchg = 1;
    /*    printf("accomp xchg at %d/%d\n",r.num,r.den); */
  }
}

#define PEDAL_CHANGE_LEN 20

typedef struct {
  char   ms[PEDAL_CHANGE_LEN][100];
  int num;
} PEDAL_CHANGE_STRUCT;

static PEDAL_CHANGE_STRUCT ped_ch;


void
get_pedal_changes() {
  char file[500],str[500];
  FILE *fp;
  int i;

  ped_ch.num = 0;
  /*  strcpy(file,scorename);
      strcat(file,".pedchg");*/

  strcpy(file,BASE_DIR);
  strcat(file,"scores/");
  strcat(file,scoretag);
  strcat(file,".pedchg");



  fp = fopen(file,"r");
  if (fp == NULL) return;
  
  for (i=0; i < PEDAL_CHANGE_LEN; i++)  {
    fscanf(fp,"%s",str);
    if (feof(fp)) break;
    strcpy(ped_ch.ms[ped_ch.num++],str);
  }
}
	     
static int
in_pedal_change_list(char *s) {
  int i;

  for (i=0; i < ped_ch.num; i++) if (strcmp(s,ped_ch.ms[i]) == 0) return(1);
  return(0);
}


static void
add_pedaling() {
  char pedal_file[500],command[500],tag[500],slash[500],string[500];
  FILE *fp;
  RATIONAL w,x;
  float t,tt;
  int i;

  get_pedal_changes();
  x.num = 1; x.den = 16;
  tt = (float) x.num / (float) x.den;

  strcpy(pedal_file,BASE_DIR);
  strcat(pedal_file,"scores/");
  strcat(pedal_file,scoretag);
  strcat(pedal_file,".ped");


  /*  strcpy(pedal_file,scorename);
      strcat(pedal_file,".ped");*/
  fp = fopen(pedal_file,"r");
  if (fp == NULL) { printf("couldn't find %s\n",pedal_file); return; }
  while(1) {
    fscanf(fp,"%s %d %s %d %s\n",command,&w.num,slash,&w.den,string);
    if (feof(fp)) break;
    if (w.num < 0) continue;
    if (in_pedal_change_list(string)) continue; //printf("in list %s\n",string);
    t = (float) w.num / (float) w.den;
    //  printf("%s\tat\t%d/%d\t%f\n",command,w.num,w.den,t);
    if (strcmp(command,"pedal_off") == 0)
      new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,t,PEDAL_OFF,w);
    if (strcmp(command,"pedal_on") == 0)
      /*      if (w.num == 9 && w.den == 2)   
       new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,t + tt,PEDAL_ON,
		     add_rat(w,x));
		     else */new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,t,PEDAL_ON,w);
  }
  fclose(fp);
}


int
put_in_cue(RATIONAL r) {
  int i,j;

 
  for (i=0; i < score.solo.num; i++) {
    //    printf("%s\n",score.solo.note[i].observable_tag);
    if (rat_cmp(score.solo.note[i].wholerat,r) != 0) continue;
 //  if (is_solo_rest(i)) return(0);
    score.solo.note[i].cue = 1;
    score.solo.note[i].xchg = 0;
    for (j=0; j < score.midi.num; j++) 
      if (rat_cmp(score.midi.burst[j].wholerat,r) == 0) score.midi.burst[j].xchg = 0;
    for (j=0; j < score.midi.num; j++) if (rat_cmp(r,score.midi.burst[j].wholerat) <= 0) break;
    score.midi.burst[j].gate = i;
    return(1);
  }
  return(0);
}


int
put_in_cue_okay(RATIONAL r) {  // okay to put cue here ?
  int i,j;

 
  for (i=0; i < score.solo.num; i++) {
    //    printf("%s\n",score.solo.note[i].observable_tag);
    if (rat_cmp(score.solo.note[i].wholerat,r) != 0) continue;
 //   if (is_solo_rest(i)) return(0);
    return(1);
  }
  return(0);
}


static int
add_cue(char *place) {
  RATIONAL r,rat;  
  int i,j;

  r = string2wholerat(place);
  if (r.num == 0 && r.den == 0) return(0);
  //  printf("r = %d/%d\n",r.num,r.den);

  if (put_in_cue(r) == 0) {
    printf("couldn't find cue pos %s\n",place);
    return(0);
  }
  return(1);


  for (i=0; i < score.solo.num; i++) {
    //    printf("%s\n",score.solo.note[i].observable_tag);
    if (rat_cmp(score.solo.note[i].wholerat,r) == 0) {
      score.solo.note[i].cue = 1;

      rat = score.solo.note[i].wholerat;
      for (j=0; j < score.midi.num; j++) if (rat_cmp(rat,score.midi.burst[j].wholerat) <= 0) break;
      score.midi.burst[j].gate = i;



      //      printf("cue at note %s midi = %d solo = %d\n",place,j,i); exit(0);
      break;
    }
  }
  if (i == score.solo.num) {
    printf("couldn't find cue pos %s\n",place);
    exit(0);
  }
}

int
put_in_accomp_xchg(RATIONAL w) {
  int j,i;

  //  for (j=0; j < score.midi.num; j++) if (rat_cmp(score.midi.burst[j].wholerat,w) == 0)
  //    break;
  /*  j  = right_midi_index(w);
      if (j >= score.midi.num)  return(0);*/


  j  = coincident_midi_index(w);
  if (j == -1)  return(0);




  score.midi.burst[j].xchg = 1;
  //  printf("put in accom_xchg at %s\n",score.midi.burst[j].observable_tag);
  for (j=0; j < score.solo.num; j++) if (rat_cmp(score.solo.note[j].wholerat,w) == 0)
    score.solo.note[j].xchg = 0;


  /*  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].xchg) printf("xchg at %s\n",score.midi.burst[i].observable_tag);
      printf("\n");*/


  return(1);

}

int
put_in_accomp_xchg_okay(RATIONAL w) {  // okay to put in accomp xchg here?
  int j,i;

  /*  j  = right_midi_index(w);
      if (j >= score.midi.num)  return(0);*/
  j  = coincident_midi_index(w);
  if (j == -1)  return(0);


  return(1);
}



static int
set_accomp_xchg(char *pos) {
  RATIONAL w;  
  int j;
  
  w = string2wholerat(pos);
  if (w.num == 0 && w.den == 0) return(0);
  if ( put_in_accomp_xchg(w) == 0) {
    printf("couldn't find match for %s in set_accomp_xchg\n",pos); 
    j  = right_midi_index(w);
    printf("hello\n");
    printf("near ones are %s and %s\n",score.midi.burst[j].observable_tag,score.midi.burst[j-1].observable_tag);
    return(0);
    //    exit(0);
  }
  return(1);


  j  = right_midi_index(w);
  if (rat_cmp(score.midi.burst[j].wholerat,w) != 0) 
    printf("substituting %s for %s in set_accomp_xchg\n",score.midi.burst[j].observable_tag,pos);
  if (j >= score.midi.num) { 
    printf("couldn't find match for %s in set_accomp_xchg\n",pos); exit(0); 
    exit(0);
  }
  /*  for (j=0; j < score.midi.num; j++) if (rat_cmp(score.midi.burst[j].wholerat,w) == 0)
    break;
  if (j == score.midi.num) { 
    printf("couldn't find match for %s in set_accomp_xchg\n",pos); exit(0); 
    exit(0);
    }*/
  printf("accomp xchg at %s pos = %s\n",score.midi.burst[j].observable_tag,pos);
  score.midi.burst[j].xchg = 1;
}

static int
set_accomp_gate(char *pos) {
  RATIONAL w;  
  int i,j;
  
  w = string2wholerat(pos);
  if (w.num == 0 && w.den == 0) return(0);
  i = coincident_midi_index(w);
  if (i == -1) { printf("no matching midi time\n"); return(0); }
  j = left_solo_index(w);  /* first solo note to the left */
  if (j == -1) { printf("couldn't find preceding solo note\n"); return(0); }
  score.midi.burst[i].gate = j;
  printf("accomp %s is gated by solo %s\n",score.midi.burst[i].observable_tag,score.solo.note[j].observable_tag);
  return(1);
}


int
put_in_solo_xchg(RATIONAL w) {
  int j;

  for (j=0; j < score.solo.num; j++) if (rat_cmp(score.solo.note[j].wholerat,w) == 0)
    break;
  if (j == score.solo.num) return(0);
  score.solo.note[j].xchg = 1;
  score.solo.note[j].cue = 0;
  for (j=0; j < score.midi.num; j++) if (rat_cmp(score.midi.burst[j].wholerat,w) == 0) score.midi.burst[j].xchg = 0;
  return(1);
}

int
put_in_solo_xchg_okay(RATIONAL w) {  // okay to put in solo xchg here ?
  int j;

  for (j=0; j < score.solo.num; j++) if (rat_cmp(score.solo.note[j].wholerat,w) == 0)
    return(1);
  return(0);
}

static int
set_solo_xchg(char *pos) {
  RATIONAL w;  
  int j;
  
  w = string2wholerat(pos);
  if (w.num == 0 && w.den == 0) return(0);
  if (w.den == 0) { printf("set_solo_xchg() failed\n"); return(0); }
  if (put_in_solo_xchg(w) == 0) {
    printf("warning: couldn't find match for %s in set_solo_xchg\n",pos);
    j  = left_solo_index(w);
    printf("near ones are %s and %s\n",score.solo.note[j].observable_tag,score.solo.note[j+1].observable_tag);
    return(0);
    //    exit(0); 
  }
  return(1);
  /*  for (j=0; j < score.solo.num; j++) if (rat_cmp(score.solo.note[j].wholerat,w) == 0)
    break;
  if (j == score.solo.num) { 
    printf("couldn't find match for %s in set_solo_xchg\n",pos);
    exit(0); 
  }
  score.solo.note[j].xchg = 1;
  score.solo.note[j].cue = 0;*/
}



static int
add_rhythmic_rest(char *string) {
  char label[500], pos[500];
  RATIONAL unit,place;
  float bpm,whole_secs;
  int i;

  sscanf(string,"%s %s %d/%d = %f",label,pos); 
  printf("rhythmic rest at %s\n",pos);
  place = string2wholerat(pos);
  if (place.num == 0 && place.den == 0) return(0);
  i = coincident_solo_index(place);
  if (i == -1) { printf("can't add rhythmic rest at %s\n",pos); return(0); }
  score.solo.note[i].rhythmic_rest = 1;
  return(1);
}


static int
tempo_setting(char *string) {
  char label[500], pos[500];
  RATIONAL unit,place;
  float bpm,whole_secs;
  int i;

  printf("tempo setting %d\n",tempo_list.num);
  sscanf(string,"%s %s %d/%d = %f",label,pos,&unit.num,&unit.den,&bpm); 
  place = string2wholerat(pos);
  if (place.num == 0 && place.den == 0) return(0);
  //    printf("place = %d/%d\n",place.num,place.den); exit(0);
  //      printf("%s %s %d %d %f\n",label,pos,unit.num,unit.den,bpm); exit(0);
  whole_secs =  (float) (60*unit.den) / (float) (unit.num*bpm); 

  add_tempo_list(place,whole_secs,unit,(int) (bpm+.5),/*1*/0);  // change 2-10  want trained model to override, if there is one
  return(1);

  /*
  if (tempo_list.num >= MAX_TEMPO_SETTINGS) {
    printf("out of room in tempo list\n"); exit(0);
  }
  tempo_list.el[tempo_list.num].wholerat = place;
  tempo_list.el[tempo_list.num].whole_secs = whole_secs;
  tempo_list.el[tempo_list.num].unit = unit;
  tempo_list.el[tempo_list.num].bpm = (int) (bpm+.5);
  tempo_list.num++; */

//  printf("%s\n",string);
  //  printf("%s %s %d/%d %f\n",label,pos,r.num,r.den,bpm);
  
  //  exit(0);
}


static int
set_accomp_cue(char *string) {
  char label[500], pos[500];
  int i,j;
  RATIONAL place;
  

  sscanf(string,"%s %s",label,pos);
  place = string2wholerat(pos);
  if (place.num == 0 && place.den == 0) return(0);
  for (j=0; j < score.solo.num; j++) if (rat_cmp(score.solo.note[j].wholerat,place) == 0)
    break;
  if (j == score.solo.num) { 
    printf("couldn't find match for %s in set_accomp_cue\n",pos); return(0); 
  }
  score.solo.note[j].cue = ACCOM_CUE_NUM;
  printf("accomp cue at %s (%d/%d)\n",pos,place.num,place.den);
  return(1);
}


int
left_midi_index(RATIONAL r) {  /* first before or equal */
  int i;

  for (i=score.midi.num-1; i >= 0; i--) {
    if (rat_cmp(score.midi.burst[i].wholerat,r) <= 0)  return(i);
  }
  return(-1);  /* error */
}

int
right_midi_index(RATIONAL r) {  /* first after or equal */
  int i;

  for (i=0; i < score.midi.num; i++) {
    if (rat_cmp(score.midi.burst[i].wholerat,r) >= 0)  return(i);
  }
  return(-1);  /* error */
}

int
strict_left_midi_index(RATIONAL r) {  /* first before */
  int i;

  for (i=score.midi.num-1; i >= 0; i--) {
    if (rat_cmp(score.midi.burst[i].wholerat,r) < 0)  return(i);
  }
  return(-1);  /* error */
}


int 
left_solo_index(RATIONAL r) {
  int i;
  
  for (i=score.solo.num-1; i >= 0; i--) {
    if (rat_cmp(score.solo.note[i].wholerat,r) < 0)  return(i);
  }
  return(-1);
}

int
rite_solo_index(RATIONAL r) {
    int i;
    
    for (i=0; i < score.solo.num; i++) {
        if (rat_cmp(score.solo.note[i].wholerat,r) > 0)  return(i);
    }
    return(-1);
}




int 
lefteq_solo_index(RATIONAL r) {
  int i;
  
  for (i=score.solo.num-1; i >= 0; i--) {
    if (rat_cmp(score.solo.note[i].wholerat,r) <= 0)  return(i);
  }
  return(-1);
}

int 
coincident_solo_index(RATIONAL r) {
  int i;
  
  for (i=0; i < score.solo.num; i++) {
    if (rat_cmp(score.solo.note[i].wholerat,r) == 0)  return(i);
  }
  return(-1);
}

int 
closest_solo_index(RATIONAL r) {
  int i,mi;
  float m,x;
  RATIONAL d;
  
  m = HUGE_VAL;
  for (i=0; i < score.solo.num; i++) {
    d = sub_rat(r,score.solo.note[i].wholerat);
    x = fabs(d.num / (float) d.den);
    if (x < m) { m = x;  mi = i; }
  }
  //  printf("diff = %f\n",m);
  return(mi);
}


int
coincident_midi_index(RATIONAL r) {
  int i;

  for (i=0; i < score.midi.num; i++) {

    if (rat_cmp(r,score.midi.burst[i].wholerat) == 0)  return(i);
    /*    printf("%d/%d %d/%d\n",r.num,r.den,score.midi.burst[i].wholerat.num,score.midi.burst[i].wholerat.den);*/
  }
  return(-1);
}


static int
set_solo_gate(char *label, char *pos) {
  RATIONAL w;  /* not used now */
  int i,j;

  w = string2wholerat(pos);
  if (w.num == 0 && w.den == 0) return(0);
  i = closest_solo_index(w);
  //  i = coincident_solo_index(w);
  if (i == -1) { printf("heosd\n"); printf("couldn't find solo_gate %s\n",pos); return(0); }
  j = left_midi_index(w);
  if (j == -1) { printf("cheosd\n"); return(0); }
  score.solo.note[i].gate = j;
  return(1);
}



void
read_midi_velocities() {
  char vel_file[500], tag[500];
  int i,vel;
  FILE *fp;

  /* something bad happens when this is called twice.  pedal commands get one of their 
     data bytes changed to the previous velocity */


  get_score_file_name(vel_file,".vel");

  /*  strcpy(vel_file,BASE_DIR);
  strcat(vel_file,"scores/");
  strcat(vel_file,scoretag);
  strcat(vel_file,".vel");*/

  /*  strcpy(vel_file,scorename);
      strcat(vel_file,".vel");*/
  fp = fopen(vel_file,"r");
  if (fp == NULL)  {
    printf("couldn't find %s using default velocites \n",vel_file);
    return;
  }
  while (!feof(fp)) {
    fscanf(fp,"%s %d\n",tag,&vel);
    for (i=0; i < score.accompaniment.num; i++)
      if (strcmp(tag,score.accompaniment.note[i].observable_tag) == 0) break;
    if (i == score.accompaniment.num) {
      printf("no match for %s in read_midi_velocities()\n",tag);
    }
    else {
      if (is_note_on(score.accompaniment.note[i].note_on) == 0) { printf("yoikes\n"); exit(0); }
      //      if (score.accompaniment.note[i].note_on->command != NOTE_ON) 
      score.accompaniment.note[i].note_on->volume = vel;
    }
  }
  fclose(fp);
}






static int
get_hand_set_parms(char *sname) {
  char pos[500], action[500],name[500],string[1000],label[500];
  FILE *fp;
  RATIONAL timesig,r,w;  /* not used now */
  float bpm;
  int j,i;

  for (i=0; i < score.solo.num; i++) score.solo.note[i].gate = -1;
  for (i=0; i < score.midi.num; i++) score.midi.burst[i].gate = -1;
  //  tempo_list.num = 0;

  strcpy(name,audio_data_dir);  // first choice is in the player directory  -- player has customized these
  strcat(name,scoretag);
  strcat(name,".hnd");
  fp = fopen(name,"r");
  if (fp == 0) {   // second choice is in the scores directory
    strcpy(name,sname);
    strcat(name,".hnd");
    printf("name = %s\n",name);
    fp = fopen(name,"r");
    if (fp == 0) { return(1); } /* this is okay */
  } 

  while (1) {
    string[0] = 0;
    for (i=0; i < 1000; i++) {
      string[i] = fgetc(fp);
      if (string[i] == '\n') { string[i+1] = 0; break; }
    }
    if (feof(fp) || i == 1000) break;
    printf("string = %s\n",string);
    sscanf(string,"%s",label);  /* the first string */
    if (strcmp(label,"solo_gate") == 0) {
      sscanf(string,"%s %s",label,pos); 
      if (set_solo_gate(label,pos) == 0) return(0);

      /*      w = string2wholerat(pos);
      i = coincident_solo_index(w);
      if (i == -1) { printf("heosd\n"); printf("couldn't find solo_gate %s\n",pos); exit(0); }
      j = left_midi_index(w);
      if (j == -1) { printf("cheosd\n"); exit(0); }
      score.solo.note[i].gate = j;*/


      //      printf("solo gate at %s\n",score.midi.burst[j].observable_tag);
      /*      for (j=score.midi.num-1; j < score.midi.num; j++) if (rat_cmp(score.midi.burst[j].wholerat,w) == 0)
	break;
      if (j == score.midi.num) { printf("cheosd\n"); exit(0); }
      score.solo.note[i].gate = j;*/
    }
    else if (strcmp(label,"accomp_gate") == 0) {
      sscanf(string,"%s %s",label,pos); 
      if (set_accomp_gate(pos) == 0) return(0);
    }
    else if (strcmp(label,"xchg_to_accomp") == 0) {
      sscanf(string,"%s %s",label,pos); 
      if (set_accomp_xchg(pos) == 0) {printf("failed in set_accomp_xchg\n"); return(0); }
    }
    else if (strcmp(label,"xchg_to_solo") == 0) {
      sscanf(string,"%s %s",label,pos); 
      if (set_solo_xchg(pos) == 0) { printf("failed with %s\n",string); return(0);}
    }
    else if (strcmp(label,"cue") == 0) {
      sscanf(string,"%s %s",label,pos); 
      if (add_cue(pos) == 0) return(0);
    }
    else if (strcmp(label,"set_tempo") == 0) {
      //      printf("string = %s\n",string); exit(0);
      if (tempo_setting(string) == 0) return(0);
    }
    else if (strcmp(label,"rhythmic_rest") == 0) {
      //      printf("string = %s\n",string); exit(0);
      if (add_rhythmic_rest(string) == 0) return(0);
    }
    else if (strcmp(label,"accomp_cue") == 0) {
      if (set_accomp_cue(string) == 0) return(0);
    }
    else { printf("no matching action for %s\n",label); return(0); }
    //    printf("string is %s label = %s\n",string,label);
    /*
    fscanf(fp,"%s %s",action,pos);
    r = string2wholerat(pos);
    if (strcmp(action,"to_solo") == 0) {
      j = coincident_solo_index(r);
      if (j == -1) { printf("couldn't find solo xchg point\n"); exit(0); }
      score.solo.note[j].xchg = 1;
    }
    else if (strcmp(action,"to_accomp") == 0) {
      j = coincident_midi_index(r);
      if (j == -1) { printf("couldn't find midi xchg point\n"); exit(0); }
      score.midi.burst[j].xchg = 1;
    }
    else {
      printf("unknown action %s in get_nick_xchg\n",action);
      exit(0);
      }*/
  }
  fclose(fp);
  return(1);
}





new_readscore() {
  int i,j;
  MIDI_EVENT e;
  char name[500];
  
  //  read_vol_eq();
  /*  this messes up graph ... and other things*/
  //           include_solo_part = 1;     
/* put this in to add solo part  to accomp */

  read_score_file();
  make_score_graph();
  parse_score();
  add_pedaling();
  set_accom_events();
  make_cue_list();
  process_accom_commands();
  set_coincidences();
  default_updates();
  set_sounding_notes();
  process_accompaniment();
  connect_midi_and_accompaniment();
  check_consecutive_solo_rests();
  set_gates();
  add_xchg_points();
  read_midi_velocities();
  set_solo_chord_spectra();
  strcpy(name,score_dir);
  strcat(name,scoretag);
  get_hand_set_parms(name);
  set_orchestra_chord_spectra();


  /*        for (i=0; i < score.solo.num; i++) {
    num2name(score.solo.note[i].num,name);
    printf("%d\t%s %f\n",i,name,score.solo.note[i].time);
  }    
  exit(0);  */
	
    
  /*              for (i=0; i < score.midi.num; i++) {
    printf("note %s %d\n",score.midi.burst[i].observable_tag,(int)(score.midi.burst[i].time*32));
	      }
	      exit(0); */
    
	      /*      for (j=0; j < score.midi.burst[i].action.num; j++) {
      e = score.midi.burst[i].action.event[j];
      num2name(e.notenum,name);
      printf("command = %x\n note =  %s\nvol = %d\n time = %f\n\n",
	     e.command,name,e.volume,e.meas);    
    }
  }
  exit(0);       */


  /*    for (i=0; i < score.accompaniment.num; i++) {
    num2name(score.accompaniment.note[i].pitch,name);
    printf("%d\t%s\t%f\t%s\n",i,name,score.accompaniment.note[i].meas,score.accompaniment.note[i].observable_tag);
  }   
  exit(0);*/
  /*  for (i=1; i <= score.measdex.num; i++) printf("%d %f\n",i,score.measdex.measure[i]);
      exit(0);*/
}


void
read_nick_solo(char *sname) {
  char name[500],tag[500];
  FILE *fp; 
  int pitch,vel,i;
  RATIONAL r,rr;
  float len;


  strcpy(name,sname);
  strcat(name,".solo");
  fp = fopen(name,"r");
  if (fp == 0) { printf("could open %s\n",name); exit(0); }
  for (i=0; i < MAX_SOLO_NOTES; i++) {
    fscanf(fp,"%s %d/%d %d %d",tag,&r.num,&r.den,&pitch,&vel);
    if (feof(fp)) break;
    score.solo.note[i].wholerat = r;
    score.solo.note[i].time = r.num / (float) r.den;  /* this is time in whole notes */
    score.solo.note[i].num = pitch;
    score.solo.note[i].trigger = 0;
    score.solo.note[i].cue = 0;  /* need to fix this */
    score.solo.note[i].meas_size =   cur_meas_size;     /* not really meas_size but 
							   rather wholenote size */
    //    score.solo.note[i].obs_var =  (pitch == RESTNUM) ? HUGE_VAL :  1;   
    score.solo.note[i].obs_var =  (pitch == RESTNUM) ? HUGE_VAL :
      (pitch == RHYTHMIC_RESTNUM) ? 10 :  1;   
    observable_tag(pitch,  r, score.solo.note[i].observable_tag);
    if (i) {
      rr = score.solo.note[i-1].wholerat;
      len = (r.num/(float)r.den) - (rr.num/(float)rr.den); /* in whole notes */
      score.solo.note[i-1].length  = len;  
    }
    score.solo.note[i].length = 1.;  /* just sets last length arbitrarily*/
  }
  score.solo.num = i;
  fclose(fp);
}

void
read_nick_changes(char *sname) {
  char name[500],tag[500],key[500];
  FILE *fp; 
  int pitch,vel,i,m1,m2,j;
  RATIONAL r,rr;
  float len;

  strcpy(name,sname);
  strcat(name,".chg");
  fp = fopen(name,"r");
  if (fp == 0) return; 
  while (1) {
    fscanf(fp,"%s %d --> %d",tag,&m1,&m2);
    if (feof(fp)) break;
    for (i=0; i < score.solo.num; i++) {
      strcpy(key, score.solo.note[i].observable_tag);
      for (j=0; j < strlen(key); j++) if (key[j] == '_') key[j] = 0;
      if (strcmp(key,tag) == 0) break;
    }
    if (i == score.solo.num) { printf("couldn't find tag\n"); exit(0); }
    if (score.solo.note[i].num != m1) { printf("couldn't match pitch %d\n",m1); exit(0); }
    score.solo.note[i].num = m2;
    printf("changed midi pitch %d at %s to %d\n",m1,tag,m2);
    r = string2wholerat(tag);
    observable_tag(m2,  r, score.solo.note[i].observable_tag);
  }
  fclose(fp);
}

void
read_bosendorfer_pedaling(char *sname) {
  char name[500],tag[500];
  FILE *fp; 
  int pitch,vel,i,command,ped;
  RATIONAL r,rr,mp;
  float len,meas;


  strcpy(name,sname);
  strcat(name,".ped");
  fp = fopen(name,"r");
  if (fp == 0) { printf("could open %s\n",name); return; }
  for (i=0; i < MAX_EVENTS; i++) {
    fscanf(fp,"%s %d",tag,&ped);
    if (feof(fp)) break;
    r = string2wholerat(tag);
    if (r.den == 0) {
      printf("hello\n");
      r = string2wholerat(tag);
    }
    meas = r.num/(float)r.den;
    new_add_event(MIDI_COM,PEDAL_COM,SUSTAIN_PEDAL,meas, ped, r);
  }
  fclose(fp);
}


void
read_nick_acc_file(char *sname) {
  char name[500],tag[500];
  FILE *fp; 
  int pitch,vel,i,command;
  RATIONAL r,rr,mp;
  float len,meas;


  strcpy(name,sname);
  strcat(name,".acc");
  fp = fopen(name,"r");
  if (fp == 0) { printf("could open %s\n",name); exit(0); }
  for (i=0; i < MAX_EVENTS; i++) {
    fscanf(fp,"%s %d/%d %x %d %d",tag,&r.num,&r.den,&command,&pitch,&vel);
    if (feof(fp)) break;
    meas = r.num/ (float) r.den;
    mp = wholerat2measrat(r);
    //    if (mp.den > 4) continue;
    new_add_event(MIDI_COM, /*NOTE_ON*/command, pitch, meas, vel, r);
  }
  fclose(fp);
}


static int
read_nick_acc(char *sname) {

  read_nick_acc_file(sname);
  //  thin_acc_events();
}

static int
read_midi_solo_transpose(char *sname, int transp) {
    char name[500],tag[500], new_name[500];
    FILE *fp, *fp2;
    int pitch,vel,i,command,trill=0,ticks;
    RATIONAL r,rr;
    float len,meas;
    
    
    
    strcpy(name,sname);
    strcat(name,".solo");
    fp = fopen(name,"r");
    if (fp == 0) { printf("could open %s\n",name); return(0); }
    
    strcpy(new_name,sname);
    strcat(new_name,".solo2");
    fp2 = fopen(new_name,"w+");
    if (fp2 == 0) { printf("could open %s\n",new_name); return(0); }
    
    r.num = 0; r.den = 1;
    //  add_solo_event(MIDI_COM, NOTE_ON, 0,0.,100, r,0);
    add_solo_event(MIDI_COM, NOTE_OFF, 0,0.,0, r,0);  /* kludge ...
                                                       solo must have some chord taking place
                                                       throughout entire composition.  this
                                                       should be a "rest" when no notes
                                                       sounding*/
    for (i=0; i < MAX_EVENTS; i++) {
        if (strcmp(scorename,"brahms_vc_mvmt1") == 0) {
            fscanf(fp,"%s %d/%d %x %d %d %d %d",tag,&r.num,&r.den,&command,&pitch,&vel,&trill,&ticks);
        }
        else {
            fscanf(fp,"%s %d/%d %x %d %d",tag,&r.num,&r.den,&command,&pitch,&vel);
        }
        
        //        if (pitch == 78) printf("vel = %d r = %d/%d\n",vel,r.num,r.den);
        if (feof(fp)) break;
        
        pitch += transp;
        fprintf(fp2,"%s\t%d/%d\t%x\t%d\t%d\n",tag,r.num,r.den,command,pitch,vel);
        
        meas = r.num/ (float) r.den;
        //    if (command != 0x92) { printf("command = %x\n",command); exit(0); }
        add_solo_event(MIDI_COM, /*NOTE_ON*/command, pitch, meas, vel, r,trill);
        //        printf("com = %d pitch = %d meas = %f vel = %d r = %d/%d\n",command,pitch,meas,vel,r.num,r.den);
        
    }
    fclose(fp);
    fclose(fp2);
    return(1);
    //      exit(0);
}




static int
read_midi_solo(char *sname) {
  char name[500],tag[500], tps_name[200];
  FILE *fp; 
  int pitch,vel,i,command,trill=0,ticks;
  RATIONAL r,rr;
  float len,meas;
    
    
    strcpy(tps_name,audio_data_dir);
    strcat(tps_name,scoretag);
    strcat(tps_name, ".tps");

  tps = get_score_transposition(tps_name);

  strcpy(name,sname);
  strcat(name,".solo");
  fp = fopen(name,"r");
  if (fp == 0) { printf("could open %s\n",name); return(0); }
  r.num = 0; r.den = 1;
  //  add_solo_event(MIDI_COM, NOTE_ON, 0,0.,100, r,0);  
    add_solo_event(MIDI_COM, NOTE_OFF, 0,0.,0, r,0);  /* kludge ...
						       solo must have some chord taking place
						     throughout entire composition.  this
						     should be a "rest" when no notes 
						     sounding*/
  for (i=0; i < MAX_EVENTS; i++) {
    if (strcmp(scorename,"brahms_vc_mvmt1") == 0) {
      fscanf(fp,"%s %d/%d %x %d %d %d %d",tag,&r.num,&r.den,&command,&pitch,&vel,&trill,&ticks);
    }
    else fscanf(fp,"%s %d/%d %x %d %d",tag,&r.num,&r.den,&command,&pitch,&vel);
    //        if (pitch == 78) printf("vel = %d r = %d/%d\n",vel,r.num,r.den);
    if (feof(fp)) break;
    meas = r.num/ (float) r.den;
    //    if (command != 0x92) { printf("command = %x\n",command); exit(0); }
    add_solo_event(MIDI_COM, /*NOTE_ON*/command, pitch+tps, meas, vel, r,trill);
    //        printf("com = %d pitch = %d meas = %f vel = %d r = %d/%d\n",command,pitch,meas,vel,r.num,r.den);

  }
  fclose(fp);
  return(1);
  //      exit(0);
}








static void
get_repeat_info(char *sname) {
  char name[500];
  FILE *fp;
  int i,j,l,r;

  score.repeat.num = 0;
  strcpy(name,sname);
  strcat(name,".rpt");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("couldn't find file %s\n",name);
    return;
  }
  for (i=0; i < MAX_REPEATS; i++) {
    fscanf(fp,"%d %d",&l,&r);
    if (feof(fp)) break;
    score.repeat.left[score.repeat.num] = l;
    score.repeat.rite[score.repeat.num] = r;
    score.repeat.num++;
  }
  fclose(fp);
}


static void
get_info(char *sname) {
  char name[500],tag[200],eq[500],last[500];
  FILE *fp;
  int i,j,l,r;

  score.measdex.pickup.num = 0;
  score.measdex.pickup.den = 1;
  strcpy(name,sname);
  strcat(name,".info");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("couldn't find file %s\n",name);
    return;
  }
  while (feof(fp) == 0) {
    fscanf(fp,"%s %s %s",tag,eq,last);
    if (strcmp(tag,"pickup") != 0) continue;
    sscanf(last,"%d/%d",&score.measdex.pickup.num,&score.measdex.pickup.den);
  }
  fclose(fp);
}


int
measrpt2measindex(int meas, int rpt) {
  int i;
  MEASURE_POS mp;

  for (i=1; i <= score.measdex.num; i++) {
    mp = score.measdex.measure[i];
    if (mp.meas == meas && mp.visit == rpt) return(i);
  }
  return(-1);
}

int
max_repeat_val(int meas) {
  int m;

  for (m=1; m < 10; m++) if (measrpt2measindex(meas, m+1) == -1) break;
  return(m);
}


static int
get_bar_data(char *sname) {
  char name[500],string[500], eq[500];
  FILE *fp;
  RATIONAL timesig,r,tot;  /* not used now */
  float bpm;
  int i,j,c,bar=1,visit[MAX_MEASURES];

  /*  score.measdex.pickup.num = 0;  now initialized in get_info
      score.measdex.pickup.den = 1;*/  

  strcpy(name,sname);
  strcat(name,".bar");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("couldn't find file %s\n",name);
    return(0);
  }

  fscanf(fp,"%s %s %d/%d",string,eq,&r.num,&r.den);
  if (strcmp(string,"pickup") == 0) {
    score.measdex.pickup = r;
    printf("pickup = %d/%d\n",r.num,r.den);
  }
  else  fseek(fp,0,SEEK_SET);  /* read with/without pickup */

  
  tot = score.measdex.pickup;


  for (bar=1; bar < MAX_MEASURES; bar++) visit[bar] = 0;
  for (i=1,bar=1,c=0; i < MAX_MEASURES; i++,bar++) {
    if (c < score.repeat.num && bar == score.repeat.rite[c]) bar =  score.repeat.left[c++];
    score.measdex.measure[i].meas = bar;
    score.measdex.measure[i].visit = ++visit[bar];
    //    printf("visit = %d\n",    score.measdex.measure[i].visit);
    score.measdex.measure[i].wholerat = tot;
    score.measdex.measure[i].pos = tot.num / (float) tot.den;
    fscanf(fp,"%d %d/%d",&j,&r.num,&r.den);
    if (feof(fp)) break;
    if (i != j) { 
       printf("strange trouble in get_bar_data (bar %d != %d)\n",i,j); 
       exit(0);
    }
    score.measdex.measure[i].time_sig = r;  // NOT REDUCED eg 6/8 okay
    tot = add_rat(tot,r);
  }
  if (i == MAX_MEASURES) { printf("too many bars in %s\n",name); return(0); }


  /*  for (i=1; i < MAX_MEASURES; i++) {
    score.measdex.measure[i].wholerat = tot;
    score.measdex.measure[i].pos = tot.num / (float) tot.den;
    fscanf(fp,"%d %d/%d",&j,&r.num,&r.den);
    if (feof(fp)) break;
    if (i != j) { printf("strange trouble in get_bar_data (bar %d != %d)\n",i,j); exit(0);}
    tot = add_rat(tot,r);
    } */
  //  score.measdex.num = i;  // this is one more than needed
    score.measdex.num = i-1;  // 10-09
  /*  for (i=1; i <= score.measdex.num; i++) 
      printf("meas = %d pos = %d/%d = %f\n",i,score.measdex.measure[i].wholerat.num,score.measdex.measure[i].wholerat.den,score.measdex.measure[i].pos); */
  fclose(fp);
  return(1);
}

static void
get_nick_info(char *sname) {
  char name[500],eq[500];
  FILE *fp;
  RATIONAL timesig,r;  /* not used now */
  float bpm;

  return;
  strcpy(name,sname);
  strcat(name,".tpo");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("couldn't find file %s\n",name);
    exit(0);
  }
  fscanf(fp,"%d/%d %s %f",&r.num,&r.den,eq,&bpm);
  cur_meas_size = (float) (60*r.den) / (float) (r.num*bpm); 
  //  printf("cur_meas_size = %f r.num = %d bpm = %f\n",cur_meas_size,r.num,bpm);
  score.meastime = cur_meas_size;
  fclose(fp);
}


static int
tempo_list_comp(const void *n1, const void *n2) {
  TEMPO_SETTING *p1,*p2;

  p1 = (TEMPO_SETTING *) n1;
  p2 = (TEMPO_SETTING *) n2;
  return(rat_cmp(p1->wholerat,p2->wholerat));
}

static int
tempo_is_redundant(RATIONAL wr, RATIONAL u, int bpm) {
  int i;


  for (i=tempo_list.num-1; i >= 0; i--) 
    if (rat_cmp(tempo_list.el[i].wholerat,wr) <= 0) break;
  if (i < 0) return(0);
  if ((rat_cmp(u,tempo_list.el[i].unit) == 0) && bpm == tempo_list.el[i].bpm) return(1); 
  return(0);
}

void
add_tempo_list(RATIONAL wr, float ws, RATIONAL u, int bpm, int hand) {
  int i;
  char tag[500];

  //  if (tempo_is_redundant(wr,u,bpm)) return;
  //    printf("tempo_list.num = %d event.num = %d\n",tempo_list.num,event.num); 
  for (i=0; i < tempo_list.num; i++) 
    if (rat_cmp(wr,tempo_list.el[i].wholerat) == 0) break;
  if (i < tempo_list.num) {  // if already have tempo at the position, change its parameters
    //    if (hand == 0) return; 
    tempo_list.el[i].whole_secs = ws;   // overwrite if dup time
    tempo_list.el[i].unit = u;
    tempo_list.el[i].bpm = bpm;
    tempo_list.el[i].hand = hand;
    wholerat2string(tempo_list.el[i].wholerat,tag);
    //    printf("was a duplicate tempo at %s\n",tag);
    return;
  }
  if (tempo_list.num >= MAX_TEMPO_SETTINGS) {
    printf("out of room in tempo list\n"); exit(0);
  }

  tempo_list.el[tempo_list.num].wholerat = wr;
  tempo_list.el[tempo_list.num].whole_secs = ws;
  tempo_list.el[tempo_list.num].unit = u;
  tempo_list.el[tempo_list.num].bpm = bpm;
  tempo_list.el[tempo_list.num].hand = hand;
  tempo_list.num++;
  qsort(tempo_list.el,tempo_list.num,sizeof(TEMPO_SETTING),tempo_list_comp); 

}

void
del_tempo_list(RATIONAL wr) {
  int i;

  for (i=0; i < tempo_list.num; i++) 
    if (rat_cmp(wr,tempo_list.el[i].wholerat) == 0) break;
  if (i == tempo_list.num) return;
  if (i == 0) return;  /* don't delete first tempo ever */
  tempo_list.el[i] = tempo_list.el[tempo_list.num-1];
  tempo_list.num--;
  qsort(tempo_list.el,tempo_list.num,sizeof(TEMPO_SETTING),tempo_list_comp); 
}


static void
set_solo_note_tempos() {
  int i,j;
  RATIONAL s;

  qsort(tempo_list.el,tempo_list.num,sizeof(TEMPO_SETTING),tempo_list_comp); 
  //  for (i=0; i < tempo_list.num; i++) printf("%d/%d\n",tempo_list.el[i].wholerat.num,tempo_list.el[i].wholerat.den); exit(0);

  for (i=0,j=0; i < score.solo.num; i++) {
    s = score.solo.note[i].wholerat;
    /*    while ((j < (tempo_list.num-1)) && 
	   (rat_cmp(s,tempo_list.el[j+1].wholerat) > 0))  j++; 
	   //	   (rat_cmp(s,tempo_list.el[j+1].wholerat) >= 0))  j++; 
	   score.solo.note[i].meas_size = tempo_list.el[j].whole_secs;  */ 
    score.solo.note[i].meas_size = get_whole_secs(s);  // substituted this for above 6-11
    //    printf("%s at %f j = %d\n",score.solo.note[i].observable_tag,score.solo.note[i].meas_size,j);
  }
  for (i=0; i < score.solo.num; i++)   score.solo.note[i].tempo_change =0;  
  for (i=0; i < score.solo.num-1; i++) 
    if (score.solo.note[i].meas_size != score.solo.note[i+1].meas_size)
      score.solo.note[i].tempo_change =  1;
}

static int
read_tempo_changes(char *sname, int type) {
  char name[500],eq[100],str[300],string[300];
  FILE *fp;
  RATIONAL timesig,r,u;  /* not used now */
  float bpm,whole_secs;
  int hand,ret=1;

  fp = fopen(sname,"r");
  if (fp == 0) {  printf("couldn't find file %s\n",sname); return(0); }
  tempo_list.num = 0;
  while (1) {
    if (type == 1) {
      fscanf(fp,"%d/%d %d/%d %s %f",&r.num,&r.den,&u.num,&u.den,eq,&bpm);
      hand = 0;  //default setting
    }
    else if (type == 2) { 
      fscanf(fp,"%s %d/%d %d/%d %s %f",str, &r.num,&r.den,&u.num,&u.den,eq,&bpm);
      wholerat2string(r,string);
      hand = 0;  //default setting
      if (strcmp(string,str) != 0) {
        printf("problem in read_tempo_changes()\n");
        printf("%s != %s in read_tempo_changes\n",string,str);
	r = string2wholerat(str);
	printf("%s = %d/%d\n",str,r.num,r.den);
        return(0);
      }
    }
    else if (type == 3) { 
      fscanf(fp,"%s %d/%d %d/%d %s %f %d",str, &r.num,&r.den,&u.num,&u.den,eq,&bpm,&hand);
      wholerat2string(r,string);
      if (strcmp(string,str) != 0) { 
	printf("%s != %s in read_tempo_changes\n",string,str); 
	r = string2wholerat(str);
	printf("%s = %d/%d\n",str,r.num,r.den);
	return(0);
	//	exit(0);
      }
    }
    if (feof(fp)) break;
    if (r.num < 0 || r.den < 0) { printf("bad position setting\n"); ret = 0; }
    //    cur_meas_size = (float) (60*r.den) / (float) (r.num*bpm); 
    whole_secs =  (float) (60*u.den) / (float) (u.num*bpm); 
    add_tempo_list(r,whole_secs,u,(int) (bpm+.5), hand);
    //     printf("%d/%d %d/%d %s %f (%f) \n",r.num,r.den,u.num,u.den,eq,bpm,whole_secs);
  }
  fclose(fp);
  return(ret);
}

int
file_type(char *name) {  // counts number of tabs per line
  FILE *fp;
  char c;
  int t=0;

  fp = fopen(name,"r");
  if (fp == NULL) return(0);
  while((feof(fp) == 0) && ((c = fgetc(fp))  != '\n')) if (c == '\t')  t++;
  fclose(fp);
  return(t);
}



static int
get_tempo_changes(char *sname) {
  char name[500],eq[100];
  FILE *fp;
  RATIONAL timesig,r,u;  /* not used now */
  float bpm,whole_secs;
  int type;

  strcpy(name,sname);
  strcat(name,".tch");
  type = file_type(name);
  if (type > 0) { if (read_tempo_changes(name,type) == 0) return(0); }
  else {   printf("get tempo changes type = %d\n",type); return(0); }
  //  read_tempo_changes(name);
  if (tempo_list.num == 0) {
    printf("%s has no tempo information\n",name); return(0);
  }
  return(1);
  /*  fp = fopen(name,"r");
  if (fp == 0)  printf("couldn't find file %s\n",name);
  while (1) {
    fscanf(fp,"%d/%d %d/%d %s %f",&r.num,&r.den,&u.num,&u.den,eq,&bpm);
    if (feof(fp)) break;
    //    cur_meas_size = (float) (60*r.den) / (float) (r.num*bpm); 
    whole_secs =  (float) (60*u.den) / (float) (u.num*bpm); 
    add_tempo_list(r,whole_secs,u,(int) (bpm+.5),0);
    //     printf("%d/%d %d/%d %s %f (%f) \n",r.num,r.den,u.num,u.den,eq,bpm,whole_secs);
  }
  fclose(fp); */

}

static void
get_nick_xchg(char *sname) {
  char pos[100], action[100],name[100];
  FILE *fp;
  RATIONAL timesig,r;  /* not used now */
  float bpm;
  int j;

  strcpy(name,sname);
  strcat(name,".xchg");
  fp = fopen(name,"r");
  if (fp == 0) return;  /* this is okay */
  while (1) {
    fscanf(fp,"%s %s",action,pos);
    if (feof(fp)) break;
    r = string2wholerat(pos);
    if (strcmp(action,"to_solo") == 0) {
      j = coincident_solo_index(r);
      if (j == -1) { printf("couldn't find solo xchg point\n"); exit(0); }
      score.solo.note[j].xchg = 1;
    }
    else if (strcmp(action,"to_accomp") == 0) {
      j = coincident_midi_index(r);
      if (j == -1) { printf("couldn't find midi xchg point\n"); exit(0); }
      score.midi.burst[j].xchg = 1;
    }
    else {
      printf("unknown action %s in get_nick_xchg\n",action);
      exit(0);
    }
  }
  fclose(fp);
}


static void
get_nick_cue(char *sname) {
  char place[100], name[500];
  FILE *fp;
  RATIONAL timesig,r;  /* not used now */
  float bpm,f1,f2;
  int i;


  strcpy(name,sname);
  strcat(name,".cue");
  fp = fopen(name,"r");
  if (fp == 0) {
    printf("couldn't find file %s\n",name);
    exit(0);
  }
  while (1) {
    fscanf(fp,"%s",place);
    if (feof(fp)) break;
    r = string2wholerat(place);
    for (i=0; i < score.solo.num; i++) {
      //      f1 = score.solo.note[i].wholerat.num/ (float)score.solo.note[i].wholerat.den;
      //      f2 = r.num / (float)r.den;
      /*      if (strcmp(place,"33+11/16") == 0) printf("%d/%d (%f) %d/%d (%f) %d\n",score.solo.note[i].wholerat.num,score.solo.note[i].wholerat.den,f1,r.num,r.den,f2,score.solo.note[i].num);*/
      if (rat_cmp(score.solo.note[i].wholerat,r) == 0) {
	score.solo.note[i].cue = 1;
	/*	printf("cue at note %d\n",i);*/
	break;
      }
    }
    if (i == score.solo.num) {
      printf("couldn't find cue pos %s\n",place);
      exit(0);
    }
  }
  fclose(fp);
}




void
read_nick_score(char *name) {
  int i,j;
  MIDI_EVENT e;
  char nm[500];

  read_vol_eq();
  get_bar_data(name);
  get_nick_info(name);
  read_nick_solo(name);
  read_nick_changes(name);
  get_nick_cue(name);
  read_nick_acc(name);
  set_accom_events();
  make_cue_list();
  process_accom_commands();
  set_coincidences();
  default_updates();
  set_sounding_notes();
  process_accompaniment();
  /*  connect_midi_and_accompaniment();*/  /* I don't see why I'll need this */
  set_gates();
  get_nick_xchg(name);


}




static void
set_solo_tempos() {
  int i;

  for (i=0; i < score.solo.num; i++) {
    score.solo.note[i].meas_size =   score.meastime;
  }
}


static void
set_trills_to_double_stops() {
  int i,k,n,t;

  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].trill == 0) continue;
    n = score.solo.note[i].snd_notes.num;
    for (k=0; k < score.solo.note[i].snd_notes.num; k++) {
      t = score.solo.note[i].snd_notes.trill[k];
      if (t == 0) continue;
      score.solo.note[i].snd_notes.snd_nums[n++] = t;
      printf("added %d to %d\n",t,i);
    }
    score.solo.note[i].trill = 0;
    score.solo.note[i].snd_notes.num = n;
    if (n >= MAX_SOUNDING_NOTES) { printf("problem in set_trills_to_dobule_stops\n"); exit(0); }  }
}



#define CLICK_PITCH 120
#define CLICK_VEL 120

static void
add_click_track() {
  int i,j;
  RATIONAL barline,q;
  float meas;

  q.num = 1; q.den = 4;
  for (i=1; i <= score.measdex.num; i++) {
    barline = score.measdex.measure[i].wholerat;
    printf("barline = %d/%d\n",barline.num,barline.den);
    meas = barline.num/ (float) barline.den;
    for (j=0; j < 4; j++) {
      meas = barline.num/ (float) barline.den;
      new_add_event(MIDI_COM, NOTE_OFF, CLICK_PITCH, meas, CLICK_VEL, barline);
      new_add_event(MIDI_COM, NOTE_ON, CLICK_PITCH, meas, CLICK_VEL, barline);
      //      printf("hello\n");
      ///      printf("barline = %d/%d q = %d/%d\n",barline.num,barline.den,q.num,q.den);
      barline = add_rat(barline,q);
    }
  }
}

#define GATE_SECS .25

static void
set_solo_gates() {
  int i,j;
  RATIONAL diff,rs,ra,gap;
  float ws,time;

  gap.num = (int)(GATE_SECS * 1000); gap.den = 1000;  /* time in measures */
  for (i=0; i < score.solo.num; i++) {
    rs = score.solo.note[i].wholerat;
    diff = sub_rat(rs,gap);
    j = left_midi_index(sub_rat(rs,gap));
    if (j == -1) continue;
    ra = score.midi.burst[j].wholerat;
        score.solo.note[i].gate = j;
	//	        printf("note = %s accomp gate = %s\n",
	//    	   score.solo.note[i].observable_tag, score.midi.burst[j].observable_tag);
    
  }
}

void
write_player_cues() {
  char file[300],tag[100];
  FILE *fp;
  int i;

  strcpy(file,audio_data_dir);
  strcat(file,scoretag);
  strcat(file,".cue");
  fp = fopen(file,"w");
  if (fp == NULL) { printf("couldn't open %s\n"); exit(0); }
  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].cue == 0) continue;
    wholerat2string(score.solo.note[i].wholerat,tag);
    fprintf(fp,"cue = %s\n",tag);
  }
  fclose(fp);
}


static void
add_hnd_mark_list(HAND_MARK_LIST *hnd, char *label, RATIONAL pos, RATIONAL unit, int bpm) {

  if (hnd->num == HAND_MARK_MAX) { printf("out of room in hand mark list\n"); exit(0); }
  strcpy(hnd->list[hnd->num].type,label);
  hnd->list[hnd->num].pos = pos;
  hnd->list[hnd->num].unit = unit;
  hnd->list[hnd->num].bpm = bpm;
  (hnd->num)++;
}

static int
cmp_hand_mark(const void *p1, const void *p2) {
  HAND_MARK *h1,*h2;
  int c;
 
  h1 = (HAND_MARK *) p1;
  h2 = (HAND_MARK *) p2;
  c = rat_cmp(h1->pos,h2->pos);
  if (c != 0) return(c);
  return(strcmp(h1->type,h2->type));
}


void
make_hand_mark_list(HAND_MARK_LIST *hnd) {
  int i,bpm;
  char tag[100];
  RATIONAL unit;

  hnd->num = 0;
  unit.num = unit.den = bpm = 0;  // unset
  for (i=0; i < score.solo.num; i++)  if (score.solo.note[i].cue) {
    wholerat2string(score.solo.note[i].wholerat,tag);
    add_hnd_mark_list(hnd, "cue", score.solo.note[i].wholerat, unit, bpm);
  }
  for (i=0; i < score.solo.num; i++)  if (score.solo.note[i].xchg) {
    wholerat2string(score.solo.note[i].wholerat,tag);
    add_hnd_mark_list(hnd, "xchg_to_solo", score.solo.note[i].wholerat, unit, bpm);
  }
  for (i=0; i < score.midi.num; i++)   if (score.midi.burst[i].xchg) {
    wholerat2string(score.midi.burst[i].wholerat,tag);
    add_hnd_mark_list(hnd, "xchg_to_accomp", score.midi.burst[i].wholerat, unit, bpm);
  }
  for (i=0; i < tempo_list.num; i++) {
    add_hnd_mark_list(hnd, "set_tempo",  tempo_list.el[i].wholerat, tempo_list.el[i].unit, tempo_list.el[i].bpm);
  }
  qsort(hnd->list, hnd->num, sizeof(HAND_MARK),cmp_hand_mark);
}


void
write_hnd_to_file(HAND_MARK_LIST *hnd, FILE *fp) {
  int i,bpm;
  RATIONAL unit;
  char tag[100];

  for (i=0; i < hnd->num; i++) {
    wholerat2string(hnd->list[i].pos,tag);
    unit = hnd->list[i].unit;
    bpm = hnd->list[i].bpm;
    if (strcmp(hnd->list[i].type,"set_tempo") != 0)  fprintf(fp,"%-10s\t%s\n",hnd->list[i].type,tag);
    else fprintf(fp,"%s\t%s\t%d/%d = %d\n",hnd->list[i].type,tag,unit.num,unit.den,bpm);
  }
}


void
read_hand_mark_lists(HAND_MARK_LIST_LIST *h) {
  int i,j,n,bpm;
  FILE *fp;
  char name[500],tag[100],eq[100];
  RATIONAL pos,unit;



  /*  get_score_file_name(char *name, "hnd");
  fp = fopen(name,"r");
  if (fp == 0) { printf("couldn't read history file\n"); exit(0); }*/
  

  
  get_player_file_name(name,"hst");
  fp = fopen(name,"r");
  if (fp == 0) { printf("couldn't read history list\n"); exit(0); }
  h->num = 0;
  for (i=0; i < HAND_MARK_LIST_MAX; i++) {
    fscanf(fp,"%s",h->examp[i]);
    fscanf(fp,"%d",&n);
    h->snap[i].num = n;
    for (j=0; j < n; j++) {
      h->snap[i].list[j].unit.num = h->snap[i].list[j].unit.den = h->snap[i].list[j].bpm = 0;
      fscanf(fp,"%s %s",h->snap[i].list[j].type,tag);
      h->snap[i].list[j].pos = string2wholerat(tag);
      if (strcmp(h->snap[i].list[j].type,"set_tempo") == 0) {
	fscanf(fp,"%d/%d %s %d",&unit.num,&unit.den,eq,&bpm);
	h->snap[i].list[j].unit = unit;
	h->snap[i].list[j].bpm = bpm;
      }
    }
    if (feof(fp)) break;
  }
  fclose(fp);





  h->num = i;
  return;
  for (i=0; i < h->num; i++) {
    printf("%s\n",h->examp[i]);
    for (j=0; j < h->snap[i].num; j++) {
      wholerat2string(h->snap[i].list[j].pos,tag);
      unit = h->snap[i].list[j].unit;
      printf("%s\t%s\t%d/%d = %d\n",h->snap[i].list[j].type,tag,unit.num,unit.den,h->snap[i].list[j].bpm);
    }
  }
}


int
read_hand_mark(HAND_MARK_LIST *h, char *name) {
  int i,j,n,bpm;
  FILE *fp;
  char tag[100],eq[100];
  RATIONAL pos,unit;


  h->num = 0;
  fp = fopen(name,"r");
  if (fp == 0) return(0);
  for (j=0; j < HAND_MARK_MAX; j++) {
    h->list[j].unit.num = h->list[j].unit.den = h->list[j].bpm = 0;
    fscanf(fp,"%s %s",h->list[j].type,tag);
    if (feof(fp)) break;
    h->list[j].pos = string2wholerat(tag);
    if (strcmp(h->list[j].type,"set_tempo") == 0) {
      fscanf(fp,"%d/%d %s %d",&unit.num,&unit.den,eq,&bpm);
      h->list[j].unit = unit;
      h->list[j].bpm = bpm;
    }
  }
  h->num = j;
  fclose(fp);
  return(1);
}








void
write_player_hnd() {
  char file[300],tag[100];
  FILE *fp;
  int i,j,iloop,bpm;
  HAND_MARK_LIST hnd;
  RATIONAL unit;

  /*  strcpy(file,audio_data_dir);
  strcat(file,scoretag);
  strcat(file,".hnd"); */
  get_player_file_name(file,"hnd");
  fp = fopen(file,"w");
  if (fp == NULL) { printf("couldn't open %s\n"); exit(0); }


  make_hand_mark_list(&hnd);
  
  /*  hnd.num = 0;
  for (i=0; i < score.solo.num; i++)  if (score.solo.note[i].cue) {
    wholerat2string(score.solo.note[i].wholerat,tag);
    add_hnd_mark_list(&hnd, "cue", score.solo.note[i].wholerat, unit, bpm);
  }
  for (i=0; i < score.solo.num; i++)  if (score.solo.note[i].xchg) {
    wholerat2string(score.solo.note[i].wholerat,tag);
    add_hnd_mark_list(&hnd, "xchg_to_solo", score.solo.note[i].wholerat, unit, bpm);
  }
  for (i=0; i < score.midi.num; i++)   if (score.midi.burst[i].xchg) {
    wholerat2string(score.midi.burst[i].wholerat,tag);
    add_hnd_mark_list(&hnd, "xchg_to_accomp", score.midi.burst[i].wholerat, unit, bpm);
  }
  for (i=0; i < tempo_list.num; i++) {
    add_hnd_mark_list(&hnd, "set_tempo",  tempo_list.el[i].wholerat, tempo_list.el[i].unit, tempo_list.el[i].bpm);
  }
  qsort(hnd.list, hnd.num, sizeof(HAND_MARK),cmp_hand_mark); */



  write_hnd_to_file(&hnd,fp);
  fclose(fp);
  /*  for (i=0; i < hnd.num; i++) {
    wholerat2string(hnd.list[i].pos,tag);
    unit = hnd.list[i].unit;
    bpm = hnd.list[i].bpm;
    if (strcmp(hnd.list[i].type,"set_tempo") != 0)  fprintf(fp,"%-10s\t%s\n",hnd.list[i].type,tag);
    else fprintf(fp,"%s\t%s\t%d/%d = %d\n",hnd.list[i].type,tag,unit.num,unit.den,bpm);
  }
  fclose(fp);*/
  return;



  i = j = 0;
  //  printf("solo note 40 xchg = %d\n",score.solo.note[40].xchg);
  while (i <  score.solo.num && j < score.midi.num) {
    
    if (i < score.solo.num && j < score.midi.num) 
      iloop = (rat_cmp(score.solo.note[i].wholerat,score.midi.burst[j].wholerat) < 0);
    else iloop = (i < score.solo.num);
    if (iloop) {
     if (score.solo.note[i].cue) {
       wholerat2string(score.solo.note[i].wholerat,tag);
       fprintf(fp,"cue\t\t%s\n",tag);
     }
     if (score.solo.note[i].xchg) {
       wholerat2string(score.solo.note[i].wholerat,tag);
       fprintf(fp,"xchg_to_solo\t%s\n",tag);
     } 
     i++;
    }
    else {
      if (score.midi.burst[j].xchg) {
	wholerat2string(score.midi.burst[j].wholerat,tag);
	fprintf(fp,"xchg_to_accomp\t%s\n",tag);
      }
      j++;
    }
  }
  /*  for (i=0; i < score.solo.num; i++) {
    if (score.solo.note[i].cue) {
      wholerat2string(score.solo.note[i].wholerat,tag);
      fprintf(fp,"cue\t%s\n",tag);
    }
    if (score.solo.note[i].xchg) {
      wholerat2string(score.solo.note[i].wholerat,tag);
      fprintf(fp,"xchg_to_solo\t%s\n",tag);
    }
  }
  for (i=0; i < score.midi.num; i++) {
    if (score.midi.burst[i].xchg) {
      wholerat2string(score.midi.burst[i].wholerat,tag);
      fprintf(fp,"xchg_to_accomp\t%s\n",tag);
    }
    }*/

  fclose(fp);
}

void
write_player_tempos() {
  char file[300],tag[100];
  FILE *fp;
  int i,bpm,h;
  RATIONAL r,u;

  strcpy(file,audio_data_dir);
  strcat(file,scoretag);
  strcat(file,".tch");
  fp = fopen(file,"w");
  if (fp == NULL) { printf("couldn't open\n"); exit(0); }
  for (i=0; i < tempo_list.num; i++) {
    r = tempo_list.el[i].wholerat;
    u = tempo_list.el[i].unit;
    h = tempo_list.el[i].hand;
    bpm = tempo_list.el[i].bpm;
    wholerat2string(r,tag);
    fprintf(fp,"%s\t%d/%d\t%d/%d = %5.3f\t%d\n",tag,r.num,r.den,u.num,u.den,(float)bpm,h);
  }
  fclose(fp);
}




void
read_player_cues() {
  char file[300],tag[100],com[100],eq[100];
  FILE *fp;
  int i;
  RATIONAL r;

  strcpy(file,audio_data_dir);
  strcat(file,scoretag);
  strcat(file,".cue");
  fp = fopen(file,"r");
  if (fp == NULL) return;
  for (i=0; i < score.solo.num; i++) score.solo.note[i].cue = 0; /* override previous cue settings */
  while(1) {
    fscanf(fp,"%s %s %s",com,eq,tag);
    if (feof(fp)) break;
    if (strcmp(com,"cue")) continue;
    r = string2wholerat(tag); 
    for (i=0; i < score.solo.num; i++) if (rat_cmp(r,score.solo.note[i].wholerat) == 0) break;
    if (i == score.solo.num) { printf("bad cue\n"); exit(0); }
    score.solo.note[i].cue = 1;
  }
  fclose(fp);
}

int
read_player_tempos() {  // overrides original tempos if there are player-specified tempos. 
  char file[300],tag[100],com[100],eq[100];
  FILE *fp;
  int i;
  RATIONAL r;

  strcpy(file,audio_data_dir);
  strcat(file,scoretag);
  strcat(file,".tch");
  return(read_tempo_changes(file,3));  // 3 = format with string position and hand
}


static void 
midi_score_init() {
//  start_pos.num = end_pos.num = 0;
//  start_pos.den = end_pos.den = 1;      // these taken ot 6-11
  score.measdex.num = score.example.num = score.measdex.num =  score.accompaniment.num = 0;
  score.solo.num=  score.midi.num = 0;
  num_states = 0;
  comm.num = 0;
  staff.num = 0;
  event.num = 0;
  sevent.num = 0;
  pedal_on = 0;
  barlines = 0;
  rat_meas_start.num = 0;
  rat_meas_start.den = 1;
  rat_meas_pos = rat_meas_start;
  accom_cue_list.num = 0;
  wait_list.num = 0;
}





static void
old_set_solo_long_note_gates() {
  int i,j;
  RATIONAL diff,rs,ra,gap;
  float ws,time;

  for (i=0; i < score.solo.num-1; i++) {   //CF:  each note
    diff = sub_rat(score.solo.note[i+1].wholerat,score.solo.note[i].wholerat);
    ws = get_whole_secs(score.solo.note[i].wholerat);
    time = ws*diff.num / (float) diff.den;
    //    printf("%s time = %f\n", score.solo.note[i+1].observable_tag,time);
    if (time > /*1.5*/ 2.5  /* && score.solo.note[i+1].snd_notes.num*/)  {
      // maybe should also involve # of intervening solo notes?
    //     if (time > 1. && score.solo.note[i+1].snd_notes.num)  { //CF:  if longer than threhsold, and next note is not a rest
      j = strict_left_midi_index(score.solo.note[i+1].wholerat);
      if (j != -1) {
	score.solo.note[i+1].gate = j;                         //CF:  set a gate
		printf("automatic solo gate at %s (%s) %f wholerat = %d/%d\n",
	       score.solo.note[i+1].observable_tag,
	       score.midi.burst[j].observable_tag,time,diff.num,diff.den);
      }
      
    }
  }
}


static void
set_solo_long_note_gates() {
  int i,j;
  RATIONAL diff,rs,ra,gap;
  float ws,time;

  for (i=1; i < score.solo.num; i++) {   //CF:  each note
    diff = sub_rat(score.solo.note[i].wholerat,score.solo.note[i-1].wholerat);
    ws = get_whole_secs(score.solo.note[i-1].wholerat);
    time = ws*diff.num / (float) diff.den;
    //    printf("%s time = %f\n", score.solo.note[i].observable_tag,time);
    if (time <= /*1.5*/ 2.5  /* && score.solo.note[i].snd_notes.num*/)   continue;




      // maybe should also involve # of intervening solo notes?
    //     if (time > 1. && score.solo.note[i].snd_notes.num)  { //CF:  if longer than threhsold, and next note is not a rest
    j = strict_left_midi_index(score.solo.note[i].wholerat);
    if (j == -1) continue;
    /*    for (; j >= 0; j--) {  // this loop added 11-10 to make the gating orchestra note longer before solo note
      diff = sub_rat(score.solo.note[i].wholerat,score.midi.burst[j].wholerat);
      time = ws*diff.num / (float) diff.den;
      if (time > .5) break;  // gating orchestra note should be at least this much before solo note
      }*/
    if (j < 0) continue;
    score.solo.note[i].gate = j;                         //CF:  set a gate
    printf("automatic solo gate at %s (%s) %f wholerat = %d/%d\n",
	   score.solo.note[i].observable_tag,
	   score.midi.burst[j].observable_tag,time,diff.num,diff.den);
  }
}



static void
set_automatic_xchgs() {
  int i,j,r,s;
  RATIONAL diff,rs,ra,gap;
  float ws,time;

  for (i=0; i < score.solo.num-1; i++) {   //CF:  each note
    for (j=i+1; j < score.solo.num-1; j++) if (is_solo_rest(j) == 0) break;
    diff = sub_rat(score.solo.note[j].wholerat,score.solo.note[i].wholerat);
    ws = get_whole_secs(score.solo.note[i].wholerat);
    time = ws*diff.num / (float) diff.den;
    if (time < 5.) continue;
    r = right_midi_index(score.solo.note[i].wholerat);
    if (r == -1) continue;
    if (rat_cmp(score.midi.burst[r].wholerat,score.solo.note[i].wholerat) ==0)
      if (score.solo.note[i].cue || i ==0)
	if (r < score.midi.num-1) r++;
	else continue;
    score.midi.burst[r].xchg = 1;
    score.solo.note[i+1].xchg = 1;
    printf("accomp xchg at %s ws == %f\n", score.midi.burst[r].observable_tag,ws);
    printf("back to solo at at %s\n", score.solo.note[j].observable_tag);
    i = j;
    
  }
}


static RATIONAL
simplify_rat(RATIONAL r) {
  int com;

  com = gcd(r.num,r.den);
  r.num /= com;
  r.den /= com;
  return(r); 
}

  

/*#ifdef ROMEO_JULIET
  #define MIN_BEAT_SECS .15
  #else */
#define MIN_BEAT_SECS .3 //.2   // .4  /* this should change in orchestra:score.c too */
#define TUTTI_MIN_BEAT_SECS .4       /* same here */
/*#endif */



static int
solo_plays_in_meas(int m) {
  int i;
  
  for (i=0; i < score.solo.num; i++)
    if (rat_cmp(score.solo.note[i].wholerat,score.measdex.measure[m].wholerat) >= 0) break;
  if (i == score.solo.num) return(0);
  return((wholerat2measnum(score.solo.note[i].wholerat) == m));
}

static RATIONAL
get_beat(int meas, RATIONAL ts, float ws) {
  int div,i,dex;
  float bt,min_beat;
  RATIONAL d,rnew,cl,diff,end,start,left;
  
  d.num = d.den = 1;
  if (solo_plays_in_meas(meas)) min_beat = MIN_BEAT_SECS;
  else min_beat = TUTTI_MIN_BEAT_SECS;
  while (1) {
    if ((ts.num % 2) == 0) d.den = 2;
    else if ((ts.num%3) ==0) d.den = 3;
    else if (ts.num == 1) d.den = 2;
    else d.den = ts.num;
    rnew = mult_rat(ts,d);
    bt = ws*rnew.num / (float) rnew.den;
    //    if (bt >  MIN_BEAT_SECS) ts =rnew;
    if (bt >  min_beat) ts =rnew;
    else return(ts);
  } 
}

static int
rat_eq(RATIONAL r1, RATIONAL r2) {
  return(r1.num == r2.num && r1.den == r2.den);
}


calc_accom_beat() {
  int i,ms; 
  RATIONAL wr,ts,r1,r2,beat,last;
  float ws, ws1,ws2;


  for (i=1; i <=score.measdex.num; i++) {
    //  for (i=1; i <= score.measdex.num; i++) {
    r1 = score.measdex.measure[i].wholerat;
    ts =  score.measdex.measure[i].time_sig;
    ws = ws1 = get_whole_secs(r1);
    if (i < score.measdex.num/*-1*/) {  /* if next neas slower take slower tempo */
      //    if (i < score.measdex.num) {  /* if next neas slower take slower tempo */
      r2 = score.measdex.measure[i+1].wholerat;
      ws2 = get_whole_secs(r2);
      if (ws2 > ws1) ws = ws2;
    }
    beat = get_beat(i,ts,ws);
    if (i == 1 || rat_eq(last,beat)==0) printf("beat at measure  %d in (%d/%d) is %d/%d\n",i,ts.num,ts.den,beat.num,beat.den);
    score.measdex.measure[i].beat =beat;
    //        printf("%d %d/%d %f beat = %d/%d\n",i,ts.num,ts.den,ws,beat.num,beat.den);
    last = beat;
  }
  //  exit(0);
}


static int
is_reartic(int i) {
  int j;

  if (i == 0) return(0);
  if (score.solo.note[i].snd_notes.num != score.solo.note[i-1].snd_notes.num) return(0);
  //  printf("note = %d sounding = %d\n",i,score.solo.note[i].snd_notes.num);

  for (j=0; j < score.solo.note[i].snd_notes.num; j++) {
    //    printf("%d %d\n",score.solo.note[i].snd_notes.snd_nums[j], score.solo.note[i-1].snd_notes.snd_nums[j]);
    if (score.solo.note[i].snd_notes.snd_nums[j] != score.solo.note[i-1].snd_notes.snd_nums[j]) return(0);
  }
  //  printf("got reartic \n"); 
  return(1);
}

#define GATE_TARGET 5.  //.3  // a very long time
#define MIN_GATE_NOTES  5 /* change 11-10*/ //1  // must be at least this many solo notes between gate and accomp note
#define MIN_GATE_SECS  3.  // must be at least this many seconds betwen gate and accomp note

static void
set_automatic_accomp_gates() {
  int i,j,j0,bestj;
  RATIONAL diff;
  float ws,time,best,dist;


#ifdef JAN_BERAN
  return;
#endif


  /*  for (i=0; i < score.midi.num; i++) score.midi.burst[i].gate = -1;  
  // take out all accompaniment gates 5-11.  these add potential for system to stall. maybe better to allow player to proceed with music minus on 
  return;*/


  for (i=0; i < score.midi.num; i++) {
    //    printf("examine %s\n",score.midi.burst[i].observable_tag);
    if (score.midi.burst[i].gate != -1) continue;
    j0 = left_solo_index(score.midi.burst[i].wholerat);
    best = HUGE_VAL;
    bestj = -1;
    for (j=j0; j >= 0; j--) {  /* j=-1 (unset) if this falls through loop */
      if (is_solo_rest(j) || is_reartic(j)) continue;  // don't gate on hard-to-detect note
      ws = get_whole_secs(score.solo.note[j].wholerat);

      /*      diff = sub_rat(score.solo.note[j].wholerat,score.solo.note[j-1].wholerat);
      time = ws*diff.num / (float) diff.den;
      if (time > .6) break;  */ /* don't want gate on long solo note  (j-1)*/

      diff = sub_rat(score.midi.burst[i].wholerat,score.solo.note[j].wholerat);
      time = ws*diff.num / (float) diff.den;
      if ((j0 - j) < MIN_GATE_NOTES || time < MIN_GATE_SECS) continue;  // added 12-06
      bestj = j; break;   // added 12-06

      dist = fabs(time-GATE_TARGET);
      if (dist < best) { best = dist; bestj = j; }
      else break; 
    }
    score.midi.burst[i].gate = bestj;

    //    if (bestj != -1) printf("%s gated at %s\n",score.midi.burst[i].observable_tag,score.solo.note[bestj].observable_tag);
	    //  else printf("-1 at %s\n",score.midi.burst[i].observable_tag);
    //   if (rat_cmp(score.midi.burst[i].wholerat,score.solo.note[bestj].wholerat) == 0) printf("gating %s at coincident solo time %d\n",score.solo.note[bestj].observable_tag,bestj);
  }
  //  exit(0);
}


static int
set_accompaniment_from_midi() {
  int i,j,n=0,p;
  RATIONAL r;

  for (i=0; i < score.midi.num; i++)  {
    for (j=0; j < score.midi.burst[i].action.num; j++) {
      if (is_note_on(score.midi.burst[i].action.event + j) == 0) continue; 
      r = score.accompaniment.note[n].wholerat = score.midi.burst[i].wholerat;
      p = score.midi.burst[i].action.event[j].notenum;
      score.midi.burst[i].action.event[j].accomp_note = score.accompaniment.note + n;
      score.accompaniment.note[n].pitch = p;
      score.accompaniment.note[n].meas = r.num/(float)r.den;
      score.accompaniment.note[n].note_on = score.midi.burst[i].action.event + j;
      observable_tag(p, r, score.accompaniment.note[n].observable_tag);
      n++;
      if (n >= MAX_ACCOMP_NOTES) { printf("farewell friend\n"); return(0); }
      //      if (n >= MAX_ACCOMPANIMENT_NOTES) { printf("farewell friend\n"); exit(0); }
    }
  }
  score.accompaniment.num = n;
  return(1);
}



int
read_midi_score_input(char *name) {
  //  char name[500];
  int flag,i,j;
  char chrd[200];
  RATIONAL r;
  MIDI_EVENT *m; 


  midi_score_init();
  //  printf("tempo_list.num = %d\n",tempo_list.num);
  //  printf("enter the nick score file (no suffix): ");
  //  scanf("%s",name);
  strcpy(full_score_name,name);    //CF:  full_score_name is global
  for (i = strlen(name); i >= 0; i--) if (name[i] == '/') break;
  if (i > 0) i++; 
  strcpy(scorename,name+i);   //CF:  filename (strip off directory structure)
  strcpy(scoretag,name+i);
    /********************************/
  get_info(name);
  get_repeat_info(name);
  if (get_bar_data(name) == 0) return(0);
  if (get_tempo_changes(name) == 0) return(0);  // moved from location a few lines later 2-06

  //  get_nick_info(name);
  if (read_midi_solo(name) == 0) return(0);
    //read_midi_solo_transpose(name, -12); //comment this in to transpose midi pitches in file
  set_solo_events();   // this was after set_accom_events()  change 8-08
  calc_accom_beat();   // this was before read_midi_solo ... changed 8-08


  read_nick_changes(name);
  read_nick_acc(name);
  if (midi_accomp) read_bosendorfer_pedaling(name);

  //    add_click_track();
  if (set_accom_events() == 0) return(0);




  //  set_solo_events();
  //  get_nick_cue(name);
  set_solo_tempos();
  set_solo_tags_etc();
  make_cue_list();
  process_accom_commands();



  set_coincidences();
  default_updates();

  if (midi_accomp) set_sounding_notes();  
  /* If we are using vocoded orchestra then we thin the accompaniment events so it isn't possible to compute
    the sounding accompaniment notes.  But we don't thin when using midi accompaniment so this is still
    valid */
                           
  set_sounding_solo_notes();
  set_trills_to_double_stops();
  process_accompaniment();
  /*  connect_midi_and_accompaniment();*/  /* I don't see why I'll need this */
  get_nick_xchg(name);




  //  get_tempo_changes(name);  
  if (get_hand_set_parms(name) == 0) return(0); //CF:  load (from .hnd file) 'hand-set' data such as cue, exchange points and gates
  read_player_cues();  /* override cues if there is a cue file */
  //  get_tempo_changes(name);  
  set_solo_note_tempos();
  //  set_gates();  /* this erases previously set accompaniment gates */

  //  set_chord_spectra();
  set_poly_solo_chord_spectra();

  if (midi_accomp)  set_orchestra_chord_spectra();   




  set_automatic_accomp_gates();  // 9-06 trying this with new parms to stop
  //accompaniment when listening is lost or audio bad.
  
  set_solo_long_note_gates();               //CF: set a gate for each solo note after a very long solo note (see docs)
  //  set_solo_gates();    /* considering this experiment */
  //  set_automatic_xchgs();  // nex expt
    /********************************/
  initscore(); 
  initstats(); /* resets the means + stds set in initscore */



  //  make_midi();  // taking this out 8-09,  messes up ordering of midi commands in burst




  firstnote = 0; lastnote = score.solo.num-1;   /* default */
  //  init_updates();
  read_examples();
  estimate_length_stats();  



  read_updates();

  if (set_accompaniment_from_midi() == 0) return(0);
      read_midi_velocities(); 


  //  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].xchg) printf("xchg at %s\n",score.midi.burst[i].observable_tag);





      /*               for (i=0; i < score.solo.num; i++) {
	       r = score.solo.note[i].wholerat;
           printf("tag = %s (%d/%d)\n",score.solo.note[i].observable_tag,r.num,r.den);
	sndnums2string(score.solo.note[i].snd_notes, chrd);
		printf("i=%d\tchord = %s\n",i,chrd);
	   continue;
	flag = 0;
	if (score.solo.note[i].action.num ==0) exit(0);
	for (j=0; j < score.solo.note[i].action.num; j++) 
	  if (score.solo.note[i].action.event[j].volume > 0) flag = 1;

		printf("i=%d\tchord = %s\n",i,chrd);
	if (flag == 0) printf("not real new chord\n");
      }
      exit(0);   */






      /*            for (i=0; i < score.midi.num; i++) {
	sndnums2string(score.midi.burst[i].snd_notes, chrd);
	printf("tag = %s chrd = %s\n",score.midi.burst[i].observable_tag,chrd);
	for (j=0; j < score.midi.burst[i].snd_notes.num; j++) printf("atck = %d\n",score.midi.burst[i].snd_notes.attack[j]);
		flag = 0;
	if (score.midi.burst[i].action.num ==0) exit(0);
	for (j=0; j < score.midi.burst[i].action.num; j++) 
	  if (score.midi.burst[i].action.event[j].volume > 0) flag = 1;

	printf("i=%d\tchord = %s\n",i,chrd);
	if (flag == 0) printf("not real new chord\n");
      }
  
      exit(0);    */


  /*            for (i=0; i < score.midi.num; i++) {
    printf("tag = %s\n",score.midi.burst[i].observable_tag);
	for (j=0; j < score.midi.burst[i].action.num; j++) 
	  printf("%x %d %d\n",score.midi.burst[i].action.event[j].command,
		 score.midi.burst[i].action.event[j].notenum,
		 score.midi.burst[i].action.event[j].volume);
	    }
	    exit(0);*/





  return(1);


}



void
set_mirex_score() {
  set_solo_events();
  set_sounding_solo_notes();
  set_poly_solo_chord_spectra();
  initscore(); 
  initstats(); /* resets the means + stds set in initscore */
  make_midi();
  firstnote = 0; lastnote = score.solo.num-1;   /* default */
  //  init_updates();
  read_examples();
  estimate_length_stats();  
}





void
read_midi_score() {
  char name[300],notes[100];
  char chrd[200];
  int i,flag,j;

  printf("enter the midi score file (no suffix): ");
  scanf("%s",name);
  read_midi_score_input(name);

  /*  for (i=0; i < 10; i++) {
    sndnums2string(score.midi.burst[i].snd_notes,notes);
    printf("chord %d is %s (%d)\n",i,notes,score.midi.burst[i].snd_notes.num);
  }
  exit(0);*/

            for (i=0; i < score.midi.num; i++) {
    printf("tag = %s\n",score.midi.burst[i].observable_tag);
    //	sndnums2string(score.midi.burst[i].snd_notes, chrd);
    //	flag = 0;
    //	if (score.midi.burst[i].action.num ==0) exit(0);
	for (j=0; j < score.midi.burst[i].action.num; j++) 
	  printf("%x\n",score.midi.burst[i].action.event[j].command,
		 score.midi.burst[i].action.event[j].notenum,
		 score.midi.burst[i].action.event[j].volume);
	  //  if (score.midi.burst[i].action.event[j].volume > 0) flag = 1;

	//	printf("i=%d\tchord = %s\n",i,chrd);
	//	if (flag == 0) printf("not real new chord\n");
      }
  
      exit(0);   

  /*      for (i=0; i < score.solo.num; i++) {
           printf("tag = %s\n",score.solo.note[i].observable_tag);
	sndnums2string(score.solo.note[i].snd_notes, chrd);
	flag = 0;
	if (score.solo.note[i].action.num ==0) exit(0);
	for (j=0; j < score.solo.note[i].action.num; j++) 
	  if (score.solo.note[i].action.event[j].volume > 0) flag = 1;

		printf("i=%d\tchord = %s\n",i,chrd);
	if (flag == 0) printf("not real new chord\n");
      }
      exit(0); */

}

void
select_score(char *name) {
  strcpy(scoretag,name);
  readscore();
}





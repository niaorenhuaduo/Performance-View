
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "share.c"
#include "global.h"
#include "belief.h"
#include "conductor.h"
#include "dp.h"
//#include "platform.h"
#include "new_score.h"
#include "timer.h"
#include "vocoder.h"
//#include <windows.h>


#define EN 30


/*typedef struct {  /* these fields should maybe jsut be contained in the event list */
/*  float secs;
  float temp;
  float meas;
} COND_EVENT;

typedef struct {
  COND_EVENT last;  
  COND_EVENT coming;
} COND_STATE; */



typedef struct {  /* i want to reach meas_time at sec_time */
  float meas_time;
  float sec_time;
} GOAL;



#define MAX_RITARD 50.  /* meas size can change (at most) by this factor each second */

/*COND_STATE cond_state; */

float meas_size;  /* secs/measure */
float cur_meas;   /* current place in the score in measures */
float cur_secs;   /* current time into score in seconds */
int   ritard;      /* 1 0 -1  resp ritard, a tempo, accel */
GOAL  goal;       /* the meas_time and sec_time we're shooting for */
int   cur_event;  /* the next accompaniment event */


//CF:  make a Timer event to play a MIDI note
void
queue_event() {  /* queue up the pending midi event to happen
		  at specified time in event stucture */
  /* after entry into this procedure cur_event is locked an cannot
     change its value until new_conduct() is called, either on timer
     expiration or because event is in past.  this event is the
     only pending event and the previous pending event (if there is
     one) is deleted */
  float target,interval,nw,now(),ideal_ret;
  extern int cur_event;
  void queue_midi_event(float);  /* linux */
  TIMER_EVENT te;


  target = score.midi.burst[cur_accomp].ideal_secs;
  nw = now();
  interval = (target-nw);

  ideal_ret = (nw > target) ? nw : target;   //CF:  if ideal time is in past, play it now; else schedule it for ideal time
  score.midi.burst[cur_accomp].ideal_timer_ret =  ideal_ret;
//       printf("in queue_event: target = %f now = %f ideal_ret = %f\n",target,nw,ideal_ret);  

  //CF:  set Timer event
  te.priority = te.type = PLAYING_EVENT;
  te.time =  ideal_ret;
  te.id = cur_accomp;
  set_timer_event(te);
}

void
async_queue_event() { 
  float target,nw;

  target = score.midi.burst[cur_accomp].ideal_secs;
  nw = now();
  if (target < nw) {
    new_conduct();
    /*  if (interval < -.05)
	printf("event is in past: event = %f now = %f token = %d cur_accomp = %d\n",target,nw,token,cur_accomp); */
    return;
  }
  add_async(target,cur_accomp); 
}

  

#define TOO_EARLY_TO_LET_SLIDE .01

exec_on_timer_expire() {
  float target,interval,nw,now();
  extern int cur_event;


  thread_nesting++;
  if (thread_nesting > 1) {
    printf("two timer interrupts have collided.  this could be bad\n");
    exit(0);
  }
  /*  target = event.list[cur_event].secs; */
  target = score.midi.burst[cur_accomp].ideal_secs;
  nw = now();
  interval = target-nw;
  if (-interval > .03) printf("lag by %f on return from timer interrupt\n",-interval);
  if (interval > TOO_EARLY_TO_LET_SLIDE) { /*  timer returned too soon.  prob small error */
    printf("early by %f secs\n",interval); 
    //    queue_midi_event(interval);      set_timer_signal(interval);  
  }
  else   new_conduct();
  thread_nesting--;
}




set_goal(meas,secs)
float meas,secs; {
  float default_arrival;    /* time (secs) of goal with 0 accel */
 extern int cur_note;
   
/* printf("goal: meas = %f secs = %f cur_meas = %f now = %f\n",meas, secs, cur_meas,cur_secs);  */
  goal.meas_time = meas;  
  goal.sec_time = secs;
/* if(goal.sec_time < cur_secs) printf("goal = %f now = %f\n",goal.sec_time,cur_secs); */
  default_arrival = (goal.meas_time - cur_meas)*meas_size + cur_secs;
  ritard =  (default_arrival < goal.sec_time)  ? 1 : -1;
/*if (cur_note >= lastnote) printf("rit = %d def = %f goal = %f cur = %f ferm = %d\n",ritard,default_arrival,goal.sec_time,cur_secs,in_fermata());  */


/*printf("goal.meas = %f last accomp = %f next accomp = %f\n",goal.meas_time,event.list[cur_event-1].meas,event.list[cur_event].meas); */

printf("set goal\n");
  if (!in_fermata())  {
    update_conductor(); 
    queue_event();
  }
  /* in_fermata == 0 ==>  event.list[event_num-1] not set */
 }


set_meas_size(s)
float s; {
  meas_size = s;
}

set_pos_tempo(cur,size)
float cur,size; {
  int i;
  float meas;

  meas_size = size;
  cur_meas = cur;
/*  cond_state.coming.secs = cur_secs;  /* phasing this out */
/*  cond_state.coming.meas = cur_meas;
  cond_state.coming.temp = 1./meas_size; */
/*  event.list[cur_event].secs = cur_secs; */
/*  event.list[cur_event].meas = cur_meas;*/
/*  event.list[cur_event].tempo = 1./meas_size; */

  meas =   event.list[cur_event].meas;
  for (i=cur_event; i < event.num; i++) {
    if (event.list[i].meas != meas) break;
    event.list[i].tempo = 1./meas_size /* HUGE_VAL*/;
    event.list[i].secs =  cur_secs;
  }
}


#define MAX_ACCEL 3. /*1. for folk */  /* .69/MAX_ACCEL is time in secs to double or half tempo */

old_update_conductor() { /* assumes event.list[cur_event-1] and goal are set*/
  float s1,m1,t1; /* secs meas tempo for last note played by accomp */
  float s2,m2,t2; /* secs meas tempo for next note played by solo */
  float sa,ma,ta; /* secs meas tempo for next note played by accom */
  float si,mi,ti; /* secs meas tempo when accel or rit stops */
  float d,c,k,e;
  int i;
/* set event.list[cur_event].  we assume that the score position is what we
   get by integrating the tempo.  the tempo is what we get by integrating
   the accelerando.  accelerando is constrained to be either +- c or 0.
   we decide if we must ritard or accel to reach goal.sec_time and 
   the sign of c is pos for accel and neg for ritard.  We change tempo
   at the rate of c until si seconds at which point we have reached mi measures
   at tempo ti.  We assume that the tempo is then constant until next note.
   in general we should reach the next note at the goal, but sometimes the
   constraint on the rate at which we change tempo will not allow this. */

  t2=0;
  s2 = goal.sec_time - event.list[cur_event-1].secs;
  if (s2 < 0) {    /* unusual but not wrong! */
    /*  printf(" goal.sec_time = %f last event = %f now = %f cur_event = %d\n",
	 goal.sec_time,event.list[cur_event-1].secs,cur_secs,cur_event); */
    s2 = 0;
  }
  m1 = event.list[cur_event-1].meas;
  t1 =  event.list[cur_event-1].tempo;
  s1 = event.list[cur_event-1].secs;
  ma = event.list[cur_event].meas;  /* the next accompaniment time */
 

  m2 = goal.meas_time; 

  if (m1 > m2) { printf("problem 2 in update_conductor() %f %f\n",m1,m2); exit(0); }
  c = MAX_ACCEL / score.meastime;
  d = 2*(t1*s2 - (m2-m1));
  if (d > 0) c = -c;
  e = s2*s2 + d/c;     /* d/c always < 0 */
  if (e < 0) si = s2;  /* cannot catch up to soloist without being to jerky */
  else si = s2-sqrt(e); /* can catch up to soloist */
  mi = m1 + t1*si + c*si*si/2;
  ti = t1 + si*c;
printf("s2 = %f t1 = %f cur_event = %d\n",s2,t1,cur_event);
 printf("si = %f s2 = %f c = %f d = %f e = %f  %f = %f\n",si,s2,c,d,e,mi+ti*(s2-si),m2);   
/*  event.list[cur_event].meas = m2; */
  sa = s1 + si + (m2-mi)/ti;  /*WHERE IS MA */
  ta = ti;
printf("event = %d secs = %f tempo = %f\n",cur_event,sa,ta);
  for (i=cur_event; i < event.num; i++) {
    if (event.list[i].meas != ma) break;
    event.list[i].tempo = ta;
    event.list[i].secs = sa;
  }


/*if (fabs(goal.sec_time-event.list[cur_event].secs) > .01)
 printf("cond_up: goal = %f play = %f cur_secs = %f\n",goal.sec_time,event.list[cur_event].secs,cur_secs);  */

/*
if (cur_note >=0) { printf("cond_up: goal = %f play = %f cur_secs = %f\n",goal.sec_time,event.list[cur_event].secs,cur_secs); 
printf("s1 = %7f s2 = %7f si = %7f sa = %7f\n",s1,s2,si,sa);
printf("m1 = %7f m2 = %7f mi = %7f ma = %7f\n",m1,m2,mi,ma);
printf("t1 = %7f t2 = %7f ti = %7f ta = %7f\n",t1,t2,ti,ta);
printf("cur_event = %d\n",cur_event);
printf("c = %f d = %f e = %f  %f = %f\n",c,d,e,mi+ti*(s2-si),m2);   
printf("goal.sec-time = %f last event = %f\n\n",goal.sec_time,event.list[cur_event-1].secs);}  */
}
  

float static
quad_root(a,b,c,sign)
float a,b,c; 
int sign;  /* 1 for plus root, -1 for minus root */ {
  float disc,ret;

  disc = b*b-4*a*c;
  if (disc < 0) return(HUGE_VAL); /* will never reached desired measure if we continue ritarding */
  ret = (-b+ sign*sqrt(disc))/(2*a);  
  if (ret > 0) return(ret);
  if (ret > -.001) return(0.);
  printf("problem in quad_root_plus() ret = %f\n",ret);
}


static float
knot_func(m1,m2,s2,t1,sk,c) /* equals 0 for correct knot location */
float m1,m2,s2,t1,sk,c; {
  float temp;

  temp = (m1-m2) + t1*(exp(c*sk)*(1/c+s2-sk) - 1/c);
  return(c*temp);  /* funtion increaseing in sk */
}


#define BISECT_THRESH .01

static float
find_knot(m1,m2,s2,t1,hi,lo,c)
float m1,m2,s2,t1,hi,lo,c; {
  float mid;

  if (hi == HUGE_VAL)  {
    hi = 1;
    while (knot_func(m1,m2,s2,t1,hi,c) < 0) hi = 2*hi;
  }
if (knot_func(m1,m2,s2,t1,lo,c) > 0) {
  printf("sdfsdfsdf\n");
  exit(0);
}
if (knot_func(m1,m2,s2,t1,hi,c) < 0) {
  printf("sdfsfffffdfsdf\n");
  exit(0);
}

  while ( hi-lo > BISECT_THRESH) {
    mid = (hi+lo)/2;
    if (knot_func(m1,m2,s2,t1,mid,c) > 0) hi = mid;
    else lo = mid;
  }
/*printf("m1-m2 = %f s2 = %f t1 = %f hi = %f lo = %f c = %f\n",m1-m2,s2,t1,hi,lo,c);
printf("knot fucn = %f\n",knot_func(m1,m2,s2,t1,mid,c)); */
  return(mid);
}

  



update_conductor() { /* assumes event.list[cur_event-1] and goal are set*/
  float s1,m1,t1; /* secs meas tempo for last note played by accomp */
  float s2,m2,t2; /* secs (until) meas tempo for next note played by accomp */
  float sa,ma,ta; /* secs meas tempo for next note played by accom */
  float si,mi,ti; /* secs (until) meas tempo when accel or rit stops */
  float d,c,k,e;
  float earliest,latest;  /* the earliest and latest times UNTIL  we could arrive at m2 */
  float arg;

  float def;    /* the time we arrive at m2 if no tempo change */
  int i,sign;
 /* set event.list[cur_event].  we assume that the score position is what we
   get by integrating the tempo.  the tempo is what we get by integrating
   the accelerando.  accelerando is constrained to be either +- c*tempo or 0.
   we decide if we must ritard or accel to reach goal.sec_time and 
   the sign of c is pos for accel and neg for ritard.  We change tempo
   at the rate of c until si seconds at which point we have reached mi measures
   at tempo ti.  We assume that the tempo is then constant until next note.
   in general we should reach the next note at the goal, but sometimes the
   constraint on the rate at which we change tempo will not allow this. */

  t2=0;
  s2 = goal.sec_time - event.list[cur_event-1].secs;
/*  printf("goal = %f last evne t = %f\n",goal.sec_time, event.list[cur_event-1].secs);  */
  m1 = event.list[cur_event-1].meas;
  t1 =  event.list[cur_event-1].tempo;
  s1 = event.list[cur_event-1].secs;
  ma = event.list[cur_event].meas;  /* the next accompaniment time */
  m2 = goal.meas_time;

if (m2 != ma) printf("why is this?\n");

  if (m1 > m2) { printf("problem 2 in update_conductor() %f %f\n",m1,m2); exit(0); }
  c = MAX_ACCEL / score.meastime;
  earliest = log(1 + (m2-m1)*MAX_ACCEL/t1)/MAX_ACCEL;
/* quad_root(c/2,t1,m1-m2,1);   /* m2 = c/2*s^2 + t1*s + m1 */
/* printf("earliest = %f\n",earliest); */
  arg = 1 - (m2-m1)*MAX_ACCEL/t1;
  latest = (arg < 0) ? HUGE_VAL : -log(arg)/MAX_ACCEL;
/*quad_root(-c/2,t1,m1-m2,1);*/
  def = (m2-m1)/t1;
/* printf("cur_event = %d m1 = %f m2 = %f s2 = %f t1 = %f goal = %f last_time = %f\n",cur_event,m1,m2,s2,t1,goal.sec_time,s1); */
/*  if (t1 == HUGE_VAL)  { 
    ta = (m2-m1)/s2;
    sa = s1 + s2;

    if (s2 < 0)       printf("looks weird in update_conductor()\n");
      
  } */
/* printf("def = %f earliest = %f lastest = %f\n",def,earliest,latest); */
  if (s2 <= earliest) { 
    sa = s1 + earliest; 
/*    ta = t1 + earliest*c; */
    ta = t1*exp(earliest*MAX_ACCEL);
/*      printf("speeding up as much as possible\n"); */
  }
  else if (s2 >= latest)   { 
    sa = s1 + latest; 
/*    ta = t1 - latest*c;  */
    ta = t1/exp(latest*MAX_ACCEL);
/*     printf("slowing down as much as possible\n");*/
  }
  else if (s2 >= def) {
    si = find_knot(m1,m2,s2,t1,s2,0.,-MAX_ACCEL);
    sa = goal.sec_time;
    ta = t1/exp(si*MAX_ACCEL);
  }
  else if (s2 <= def) {
    si = find_knot(m1,m2,s2,t1,s2,0.,MAX_ACCEL);
    sa = goal.sec_time;
    ta = t1*exp(si*MAX_ACCEL);
  }
  

#ifdef YES
printf("sholnd't be here\n");
  else {

      
      
    sign = (s2 > def) ? -1 : 1;

    c *= sign;     /* m2 = m1 + t1*si + c/2*si^2 + (s2-si)*(t1+si*c) */
    si = quad_root(-c/2, s2*c, (m1-m2) + s2*t1,sign);  /* sign root guarantees 0 <= si <= s2 */
    ti = t1 + si*c;
    mi = m1 + t1*si + c*si*si/2;
    sa = s1 + si + (m2-mi)/ti;   /* is this just s2 */
    ta = ti;
    if (fabs((mi+ti*(s2-si))-m2) > .01)
      printf("problem in update_conductor() :  %f should = %f\n",mi+ti*(s2-si),m2);   
    if (si < 0 || si > s2) printf("should have %f <= %f <= %f\n",0,si,s2);
  }
  if (ta < 0) { printf("prob in update_conductor\nt1 = %f si = %f c = %f s1 = %f s2 = %f m1 = %f m2 = %f\n",t1,si,c,s1,s2,m1,m2 );
    si = quad_root(-c/2, s2*c, (m1-m2) + s2*t1,-1);  
 printf("other si = %f sing = %d\n",si,sign);
}		

#endif	       
  for (i=cur_event; i < event.num; i++) {
    if (event.list[i].meas != ma) break;
    event.list[i].tempo = ta;
    event.list[i].secs = sa;
  }
}
  
  
  
  



unset_ritard() {
  float default_arrival,s;    /* time (secs) of goal with 0 accel */

  s = (goal.sec_time - cur_secs)/(goal.meas_time - cur_meas);
  if (s<0) s=0;
  default_arrival = (goal.meas_time - cur_meas)*meas_size + cur_secs;
  if      (ritard ==  1 && default_arrival > goal.sec_time) {
      ritard = 0;
      set_meas_size(s);
  }
  else if (ritard == -1 && default_arrival < goal.sec_time) {
      ritard = 0;
      set_meas_size(s);
  }
/* if (ritard == 0) printf("goal attained %f\n",goal.meas_time); */
}


float token_time[(int) (MAXAUDIO/TOKENLEN)];
float listen_complete_time[(int) (MAXAUDIO/TOKENLEN)];


#define PARTITIONS 20
#define MAX_INTERVAL .2

show_token_hist() {
  int i,h;
  int *hgram,*ivector();
  FILE *fp;
  float total=0,mx=0,t;
    char name[500];

    strcpy(name,user_dir);
    strcat(name,LOG_FILE);
 
 //    fp = fopen(LOG_FILE,"a");
     fp = fopen(name,"a");
  hgram = ivector(-1,PARTITIONS+1);
  for (i=-1; i <=PARTITIONS; i++) hgram[i] = 0;
  for (i=0; i < frames; i++) {
    h = (token_time[i]-tokens2secs(i+1))*PARTITIONS/MAX_INTERVAL;
    if (h < -1) h = -1;
    if (h > PARTITIONS) h = PARTITIONS;
   hgram[h]++;
  }
  fprintf(fp,"late token histogram\n");
  for (i=-1; i < PARTITIONS+1; i++) 
    fprintf(fp,"%f %d\n",i*MAX_INTERVAL/PARTITIONS,hgram[i]);
   fprintf(fp,"\n");
  free_ivector(hgram,-1,PARTITIONS+1);

  for (i=0; i < frames-1; i++) {
    t = listen_complete_time[i] - token_time[i];
    if (t < 0)  fprintf(fp,"what is this at frame %d??? %f %f\n",i,listen_complete_time[i], token_time[i]);
    if (t > mx) mx = t;
    total += t;
  }
  fprintf(fp,"average (max) time per listen iteration = %f (%f)\n",total/frames,mx);
  fclose(fp);
  

}

int   midi_fd;
  
play_midi_event(m)
MIDI_EVENT *m; {
  char buff[3];
  int i;
  float now();
 
  buff[0] = m->command; 
  buff[1] = m->notenum;  /* this is identifier of which pedal in case of pedal command */
  buff[2] = m->volume;  /* this is PEDAL_ON or PEDAL_OFF if pedal command */
  if (is_a_rest(buff[1]) == 0) /* ??? */ 
    //    midi_command(buff);
    play_midi_chunk(buff, 3);
  m->realize = now();
  /*  printf("midi_event %d %d %d at %f\n",buff[0],buff[1],buff[2],now());       */
}


/*
#define MAX_MIDI_BYTES 3*300

static void
play_midi_events() {
     float play_time,now(),nw;
     char buff[MAX_MIDI_BYTES];
     int i = 0;
     MIDI_EVENT *m;
     RATIONAL pt;

     play_time = event.list[cur_event].meas;  
     pt = event.list[cur_event].wholerat;
     nw = now();
       while (rat_cmp(event.list[cur_event].wholerat,pt) == 0)  {
       m = event.list + cur_event;
       m->realize = nw;
       buff[i++] = m->command;
       buff[i++] = m->notenum;
       buff[i++] = m->volume;
       if (i > MAX_MIDI_BYTES) printf("writing out of midi buffer\n");
       cur_event++;
       if (cur_event == event.num) break;
     }
     multi_midi_command(buff,i);
     if (cur_event == event.num) end_conduct();
}

*/

int histogram[PARTITIONS+1];


start_cond(ms) 
 float ms;  {  /* size in seconds of meas */
   int i;
   extern void init_midi();

   for (i=0; i < PARTITIONS+1; i++) histogram[i]=0;
   init_midi();
   init_clock();
   init_timer();
   meas_size = ms;
   cur_meas = 0;
   cur_secs = 0;
 /*  cur_event = 0;
   cue.cur = 0; */
   ritard = 0;
 }


static void
set_accomp_range_from_solo() {
  int i;

   for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].time > 
					    score.solo.note[lastnote].time) break;
   last_accomp = i-1;
   first_accomp = 0;
   while (score.solo.note[firstnote].time > score.midi.burst[first_accomp].time) 
     first_accomp++;
 }


static void
set_ranges(float start, float end) {
  int i;

  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].time > end) break;
  last_accomp = i-1;
  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].time >= start) break;
  first_accomp = i;
}

void
set_accomp_range() {
  int i;


  /*  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].time > end_meas) break;
  // end_meas not reliable here 
    last_accomp = i-1;
  last_accomp = i;
  //  printf("(1st way) last_accomp = %d num = %d\n",last_accomp,score.midi.num);
  */



  //  printf("(2nd way) last_accomp = %d num = %d\n",last_accomp,score.midi.num);

  /*  for (i=0; i < score.midi.num; i++) if (score.midi.burst[i].time >= start_meas) break;
      first_accomp = i;*/





  /*  for (i=0; i < score.midi.num; i++) if (rat_cmp(score.midi.burst[i].wholerat,end_pos) >= 0) break;
      last_accomp =  (i == score.midi.num)  ? i-1 :  i;*/

  for (i=0; i < score.midi.num; i++) 
    if (rat_cmp(score.midi.burst[i].wholerat, end_pos) > 0) break;
  last_accomp = (i == 0) ? 0 : i-1;


  //  printf("last_accomp = %d num = %d\n",last_accomp,score.midi.num);
  for (i= 0; i < score.midi.num-1; i++) 
    if (rat_cmp(score.midi.burst[i].wholerat,start_pos) >= 0) break;
  first_accomp =  (i == score.midi.num)  ? i-1 :  i;
  cur_accomp = first_accomp;
}


void
queue_if_accomp_plays_first() {
  int is,ia,diff,acc_first=0,flush;
  float lag,t1;


  for (is=0; is < score.solo.num; is++) if (rat_cmp(score.solo.note[is].wholerat,start_pos) >= 0) break;
  for (ia=0; ia < score.midi.num; ia++) if (rat_cmp(score.midi.burst[ia].wholerat,start_pos) >= 0) break;
  diff = rat_cmp(score.midi.burst[ia].wholerat,score.solo.note[is].wholerat);
  /*  printf("solo = %s acc = %s\n",score.solo.note[is].observable_tag,score.midi.burst[ia].observable_tag);
      exit(0); */
  if (diff < 0) acc_first = 1;
  //  if ((diff == 0) && (is_a_rest(score.solo.note[is].num))) acc_first = 1;
  if ((diff == 0) && (is_solo_rest(is))) acc_first = 1;
  if (acc_first == 0) return;
  cur_accomp = ia;

  score.midi.burst[ia].ideal_secs = 1.;  /* does nothing i think */

  t1 = now();
  lag = .1;
 // if (mode == SYNTH_MODE) lag = .6;
#ifdef BOSENDORFER
  lag += BOSENDORFER_LAG;
#endif
  update_belief_accom(cur_accomp, lag, flush=1);

  /* printf("time[%d] = %f\n",cur_accomp,score.midi.burst[cur_accomp].ideal_secs); */
  printf("resetting cur_accomp in queue_if_acomp_plays_first()\n");
  queue_event();
}



static void
send_midi_alert() {
  unsigned char buff[3];

  buff[0] = NOTE_ON;
  buff[1] = 60;
  buff[2] = 60;
  play_midi_chunk(buff,3);
}



void
new_start_cond(float start,float end,float ms) {
     /* start is start time in measures */
     /* i think end and ms are not used */
   int i;
   extern void init_midi();

   //       printf("start conducting at %f\n",start);  

   if (mode != NO_SOLO_MODE)  {
     /*     set_accomp_range_from_solo(); */

     set_accomp_range(); 
     //   printf("last_accomp = %s\n",score.midi.burst[last_accomp].observable_tag);
     //   exit(0);
   }
   /*   set_ranges(start,end);*/
   for (i=0; i < PARTITIONS+1; i++) histogram[i]=0;
   //    init_clock();  /* if I delete this (redundant) statement no midi commands get through to midi device */
   //   if (midi_accomp) init_midi();
  // init_clock(); // this maybe be redundant now since it is done in asio_begin_live (but not for non-asio version)
   init_timer();
   //   if (midi_accomp) send_midi_alert();
   meas_size = ms;
   cur_meas = start;
   cur_secs = 0;
   cur_event = 0;
   while (cur_meas > event.list[cur_event].meas) cur_event++;
   //   cur_accomp = 0;
   //   while (cur_meas > score.midi.burst[cur_accomp].time) cur_accomp++;

   cur_accomp = 0;   /* this is redundant if begin_live is called */
   while (rat_cmp(start_pos, score.midi.burst[cur_accomp].wholerat) > 0) cur_accomp++;
   if (cur_accomp == score.midi.num) { printf("couldn't find starting pos\n"); exit(0); }
   cue.cur = 0;
   ritard = 0; 
   //          printf("end new_start_cond at %f\n",start);  
 }



void
newer_start_cond(RATIONAL s, RATIONAL e) {
   int i;
   extern void init_midi();

   for (i=0; i < PARTITIONS+1; i++) histogram[i]=0;
   cur_accomp = 0;
   while (rat_cmp(s,score.midi.burst[cur_accomp].wholerat) > 0) cur_accomp++;
   last_accomp = 0;
   while (rat_cmp(e,score.midi.burst[last_accomp].wholerat) > 0) last_accomp++;



   init_clock();  /* if I delete this (redundant) statement no midi commands get through to midi device */
   init_midi();
   init_clock();
   init_timer();
}




show_cond_hist() {
  int i;

  printf("conductor histogram\n");
  for (i=0; i < PARTITIONS+1; i++) 
    printf("%f %d\n",i*MAX_INTERVAL/PARTITIONS,histogram[i]);
   printf("\n");
}



extern int cond_flag;
extern int token;



in_fermata() {  /* boolean.  are we waiting to be cued (so no notes should be played) */
  int cn;
  float ct;

  if (mode != NO_SOLO_MODE) {/* otherwise just testing score */
    if (cue.cur < cue.num) { /* otherwise all cues have been cued */
      if (cur_event < event.num) {
	cn = cue.list[cue.cur].note_num;
	ct =  score.solo.note[cn].time;
	if (ct == event.list[cur_event].meas) return(1);
      }
    }
  }
/*printf("cue.cur = %d cue.num = %d cur_event = %d event.num = %d, ct = %f time = %f\n",cue.cur,cue.num,cur_event,event.num,ct,event.list[cur_event].meas); */
  return(0);	
}

end_conduct() {
  printf("ending conductor\n");
  end_midi();
  end_timer();
}


conduct() {}


void
set_midi_buffer(char *buff, int *len) {
  int i,j=0,status=0,x;
  MIDI_EVENT *m; 


// if (strcmp("atm_51+0/1",score.midi.burst[cur_accomp].observable_tag) == 0)
 //  printf("observable tag = %s\n",score.midi.burst[cur_accomp].observable_tag);
  for (i=0; i < score.midi.burst[cur_accomp].action.num; i++) {
    m = score.midi.burst[cur_accomp].action.event + i;
    if (m->command != status) buff[j++] = status = m->command; 
	buff[j++] = x = m->notenum;
    buff[j++] = x = m->volume;
  }
  *len = j;
}

#define TOO_LATE_TO_PLAY  .1

void
new_conduct() {
  float elapsed,wait,m,std,ct,cm,play_time,t1,t2,play_secs,ideal_secs,set_time;
  int w,uw,old_mask,i,cn,tag,n,ce,midi_bytes,flush;
  float now();
  char buff[1000];
/*  extern int lasttoken; */


// if (cur_accomp == 82)printf("really don't get this ...\n");
  if (mode != BASIC_PLAY_MODE && performance_interrupted) return;
  /*  printf("new_conduct at %f %d meas = %f\n",now(),cur_accomp,score.midi.burst[cur_accomp].time); */

  /*  if (cur_event >= event.num) return;*/
  if (cur_accomp > last_accomp/*score.midi.num*/) return;

  cur_secs = now();


  /*  if (in_fermata()) return;  /* waiting for cue before accomp can go on */ 
/*if (lastnote < cur_note) printf("cond: elapsed = %f play = %f cur_secs = %f\n",elapsed,event.list[cur_event].secs,cur_secs); */

  /*  if (cur_secs >= event.list[cur_event].secs) */ {  /* notes to be played */
/*printf("cur eve = %d meas = %f time = %f tempo = %f\n",cur_event,event.list[cur_event].meas,event.list[cur_event].secs,event.list[cur_event].tempo*60/score.measure);         */
    

/*    play_midi_events(); */


    ideal_secs  = score.midi.burst[cur_accomp].ideal_secs;
    t1 = now();

    /*    if (cur_accomp == 62) printf("late by %f\n",t1-ideal_secs);*/

    if (t1 - ideal_secs > TOO_LATE_TO_PLAY) {
      //      printf("modify the code so that no note is played here\n");
    }

    set_midi_buffer(buff,&midi_bytes);

    set_time = (fabs(ideal_secs-t1) > .03) ? t1 : ideal_secs; 
    set_time = ideal_secs; 
#ifdef BOSENDORFER
    set_time += BOSENDORFER_LAG;
#endif
    /* to avoid accumulation of errors we should set the accompaniment times
       to the ideal predicuted by the model.  except when the actual differ
       from the predictions significantly, as in when an accomp note is
       late as a result of detecting a solo note and realizing the acomp
       note should be played immediately */

    play_midi_chunk(buff,midi_bytes);
    t2 = now();
#ifdef BOSENDORFER
    t2 += BOSENDORFER_LAG;
#endif
    score.midi.burst[cur_accomp].actual_secs = t2;
    //    printf("playxed %s (%d)  at %f with %d bytes\n",score.midi.burst[cur_accomp].observable_tag,cur_accomp,t2,midi_bytes);
    //    fflush(stdout);
    /*    if (t2 - t1 > .01) printf("lag of %f in writing midi data\n",t2-t1);*/

    /*    for (i=0; i < score.midi.burst[cur_accomp].action.num; i++) {
	  t1 = now();
      play_midi_event(score.midi.burst[cur_accomp].action.event + i);
      score.midi.burst[cur_accomp].actual_secs = t2 = now();
      if (t2 - t1 > .01) printf("lag of %f in new_conduct\n",t2-t1);
      } */
    cur_accomp++;

    if (cur_accomp == score.midi.num || cur_accomp > last_accomp) {
      printf("ending conduct: %d %d %d %d\n",cur_accomp,score.midi.num , cur_accomp, last_accomp);
      end_conduct();
      return;
    }
      



 /*     printf("from conduct\n");   */
	if (mode == NO_SOLO_MODE || mode == BELIEF_TEST_MODE || mode == BASIC_PLAY_MODE)  { queue_event(); }
	if (mode == SYNTH_MODE || mode == LIVE_MODE) {
	  update_belief_accom(cur_accomp-1, set_time/*ideal_secs*/, flush=1);
	  printf("initial scheduling of %s for %f (now = %f)\n",score.midi.burst[cur_accomp].observable_tag,score.midi.burst[cur_accomp].ideal_secs,now());
	}
  }
}


 /* predict_future -> set_goal */


/* play_note(d) 
 int d; {
   int i;
   char buff[3];

   buff[0] = 0x90;
   buff[1] = d;
   buff[2] = 100;
   i = write(midi_fd,buff,3);  
   if (i < 3) printf("wrote %d ---  problem writing midi data\n",i); 
 }
*/



 /*main() {
   int i,j;
   float buff[100];

   start_cond(.05);
   while (1) conduct();
   while (cond_running()) {
     for (i=0; i < 1000000; i++);
     printf("%d\n",cond_running());
   }
   for (i=0; i < event_num; i++) printf("%d %f %f\n",i,event[i],actual[i]);
   return;
   for (i=0; i < 100; i++) {
     buff[i] = cur_secs;
     for (j=0; j < 100; j++) { conduct(); set_meas_size(1.); }
   }

   for (i=0; i < 100; i++) printf("%d %f\n",i,buff[i]);

 } */



play_events() {
   start_cond(1.);
   while (cur_event < event.num) conduct();
 }




 match_realizations() { /* compare the event time realizations that correspond in
			 beats to the solo note realiztions */
   int i,j;
   float on_line_solo,accom,on_diff,off_diff,meas,off_line_solo,on_off_diff;
   char c[20];
   extern int *notetime;
   int lag;

   for (i=firstnote; i <= lastnote; i++) {
     num2name(score.solo.note[i].num,c);
     for (j=0; j < event.num; j++) {
 /*      printf("%f %f\n",score.solo.note[i].time,event.list[j].time); */
       if (fabs(score.solo.note[i].time - event.list[j].meas) < .001) {
	 meas = score.solo.note[i].time;
	 lag = score.solo.note[i].lag;
	 on_line_solo = TOKENLEN*score.solo.note[i].realize/(float)SR;
	off_line_solo = TOKENLEN*notetime[i]/(float)SR;
	accom = event.list[j].realize;
	on_off_diff = on_line_solo-off_line_solo;
	off_diff = off_line_solo-accom;
	printf("note = %d (%s) error =  %5.3f online late by = %5.3f lag = %d\n",i,c,off_diff,on_off_diff,lag);
	/*printf("accom = %f on_line_solo = %f off_line_solo = %f\n",accom,on_line_solo,off_line_solo); */
	break;
      }
    }
  }
}

float
overall_synch_err() {
  int j,i;
  float solo_time,accomp_time,err,total=0;
  MIDI_BURST *an;
  FILE *fp;

  for (i=first_accomp; i <= last_accomp; i++) {
    j = coincident_solo_index(score.midi.burst[i].wholerat);
    if (j == -1) continue;
    solo_time = score.solo.note[j].off_line_secs;
    accomp_time = score.midi.burst[i].actual_secs;
    err = solo_time - accomp_time;
    total += err*err;
  }
  return(total);
}

float
overall_smoothness() {
  int j,i;
  float solo_time,accomp_time,err,total=0,note_secs,meas_len,last_len,diff,whole;
  MIDI_BURST *an;
  FILE *fp;
  RATIONAL r;

  for (i=first_accomp; i < last_accomp; i++) {
    note_secs = score.midi.burst[i+1].actual_secs - score.midi.burst[i].actual_secs;
    r = sub_rat(score.midi.burst[i+1].wholerat, score.midi.burst[i].wholerat);
    whole = r.num / (float) r.den;
    meas_len = note_secs / whole;
    diff = meas_len - last_len;
    if (i > first_accomp) total += diff*diff;
    last_len = meas_len;
    printf("len = %f %d/%d\n",last_len,r.num,r.den);
  }
  return(total);
}

#define EVAL_BINS 10
#define BIN_MS 1

void
eval_accomp_note_times() {
  int j,i,b,hist[EVAL_BINS+1],range[EVAL_BINS+1],ms_err,ideal,actual,set,solo_time,solo_detect,sync_err,pred_error;
  extern int first_event,last_event;
  float err,late,time;
  MIDI_BURST *an;
  char name[500],tag[500],*solo_str,log_file[500];
  FILE *fp;

    strcpy(log_file,user_dir);
    strcat(log_file,LOG_FILE);
  //  printf("sync err = %f smooth = %f\n",overall_synch_err(),overall_smoothness()); exit(0);
 // fp = fopen(LOG_FILE,"a");
     fp = fopen(log_file,"a");
  b = 1;
  range[0] = 0;
  for (i=1; i <= EVAL_BINS; i++) { range[i] = b; b *= 2; }
  fprintf(fp,"histogram of accom note error (+-) in ms\n");
  for (b=0; b < EVAL_BINS; b++) hist[b] = 0;
  for (i=first_accomp; i <= last_accomp; i++) {
    err = fabs(score.midi.burst[i].actual_secs-score.midi.burst[i].ideal_timer_ret/*idea_secs*/);
    b = err/(BIN_MS*.001);
    for (j=0; j < EVAL_BINS; j++) if (b <= range[j+1] ) break;
    /*    if (b > EVAL_BINS-1) b = EVAL_BINS-1; */
    hist[j]++;
  }
  /*  for (b=0; b < EVAL_BINS; b++) printf("%d-%d\t:\t%d\n",range[b],range[b+1],hist[b]);*/
  for (b=0; b < EVAL_BINS; b++) fprintf(fp,"%d-%d\t:\t%d\n",range[b],range[b+1],hist[b]);
  fclose(fp);
  printf("note\t\ttime\t\tms late\tset at\tideal\tacutal\tsolo\tnt_evnt\tvcod_er\tpred_er\tsync_er\n");
  for (i=first_accomp; i <= last_accomp; i++) {
    an = score.midi.burst + i;
    for (j=0; j < score.solo.num; j++)
      if (fabs(score.solo.note[j].time - an->time) < .001) break; // crashed here 5-11.  i think this shouldn't be called of performace_interupted
    solo_time = (j < score.solo.num) ? 1000*score.solo.note[j].off_line_secs : 0;
    solo_detect = (j < score.solo.num) ? 1000*score.solo.note[j].detect_time : 0;
    ms_err = 1000*(an->actual_secs- an->ideal_timer_ret /*ideal_secs*/);
    set = an->ideal_set*1000;
    actual = an->actual_secs*1000;
    sync_err = (actual && solo_time) ? actual - solo_time : 0;
    solo_str = (solo_time) ? score.solo.note[j].observable_tag : NULL;
    ideal = an->ideal_secs*1000;
    /*    num2name(score.midi.burst[i].action.event[0].notenum,name); */
    time = score.midi.burst[i].time;
    pred_error = (j < score.solo.num) ? ideal-solo_time : 0;
    wholerat2string(score.midi.burst[i].wholerat, tag);

    /*    if (abs(ms_err) > 20) printf("accomp note = %d  was %f ms late: set at %f  ideal = %f acutal = %f\n",i,ms_err,an->ideal_set,an->ideal_secs,an->actual_secs); */
    if (abs(ms_err) > -1/*20*/) printf("%10s\t%10s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",tag,solo_str,ms_err,set,ideal,actual,solo_time,solo_detect,actual-ideal,pred_error,sync_err);
  }
}




#define SOLO_TIMES "solo_times.dat"
#define ACCOMP_TIMES "accomp_times.dat"

void
make_performance_data() {
  FILE *fp;
  int i;

  printf("entering make_performance_data()\n");
  fp = fopen(ACCOMP_TIMES,"w");
  for (i=first_accomp; i <= last_accomp; i++)
    fprintf(fp,"%f %f\n",score.midi.burst[i].time,score.midi.burst[i].actual_secs);
  fclose(fp);
  fp = fopen(SOLO_TIMES,"w");
  for (i=firstnote; i <= lastnote; i++)
    fprintf(fp,"%f %f\n",score.solo.note[i].time,score.solo.note[i].on_line_secs);
  fclose(fp);
  printf("leaving make_performance_data()\n");
}


void
synchronize()  {
  if (mode == LIVE_MODE) { 
    prepare_sampling();
    start_cond(1.);  /* 0.0 is now */
    begin_sampling();
    return;
  }
  if (mode == SYNTH_MODE)  {
    prepare_playing(NOMINAL_SR);
    start_cond(1.);  /* 0.0 is now */
       begin_playing(); 
    return;
  }
  printf("don't know how to handle mode %d in synchronize()\n",mode);
}


void
new_synchronize(float start_meas)  {
  int w;
  unsigned char buff[1024];
  extern int Audio_fd;



  init_timer_list();
  if (mode == LIVE_MODE) { 
#ifdef ORCHESTRA_EXPERIMENT
    /*        prepare_playing_and_sampling();*/
        prepare_playing(NOMINAL_OUT_SR);      //CF:  init the audio system (eg ALSA)
	prepare_sampling();
    vcode_init();                            //CF:  init the vocoder   ***
    /*            w = write(Audio_fd, buff,10);
		  printf("w = %d\n",w); */
#else
    prepare_sampling();
#endif
    new_start_cond(start_meas,0.,1.);  /* 0.0 is now */
    queue_if_accomp_plays_first(); /* should appear before new_start_cond()*/
    begin_sampling();
#ifdef ORCHESTRA_EXPERIMENT
    start_playing_orchestra();   //CF:  start up the vocoder **
#endif
    return;
  }
  if (mode == BELIEF_TEST_MODE)  {
    prepare_playing(NOMINAL_SR);
    /*    start_cond(1.);  /* 0.0 is now */
    new_start_cond(start_meas,0.,1.);  /* 0.0 is now */
    begin_playing();
   return;
  }
  if (mode == SYNTH_MODE)  {

#ifdef ORCHESTRA_EXPERIMENT
    prepare_playing(NOMINAL_OUT_SR);
    vcode_init();
#else
    prepare_playing(NOMINAL_SR);
    //    begin_playing(); 
#endif


    new_start_cond(start_meas,0.,1.);  /* 0.0 is now */
    //                queue_if_accomp_plays_first(); /* should appear before new_start_cond()*/
#ifdef ORCHESTRA_EXPERIMENT
    start_playing_orchestra();
#else
    queue_if_accomp_plays_first(); /* should appear before new_start_cond()*/
    begin_playing(); 
#endif
    return;
  }
  printf("don't know how to handle mode %d in new_synchronize()\n",mode);
}


static void
init_cur_accomp() {
  cur_accomp = 0;
  while (rat_cmp(start_pos, score.midi.burst[cur_accomp].wholerat) > 0) cur_accomp++;
   if (cur_accomp == score.midi.num) { printf("couldn't find starting pos\n"); exit(0); }
}




void
begin_pv(float start_meas)  {
  int i;
  //  printf("begin_live: lastnote = %s\n",score.solo.note[lastnote].observable_tag);
  //  exit(0);

  mode = PV_MODE;
  performance_interrupted= 0;
  //  init_cur_accomp();
  prepare_playing(NOMINAL_OUT_SR);
  //  if (mode == LIVE_MODE)  prepare_sampling();
  vcode_init();
  new_start_cond(start_meas,0.,1.);  /* 0.0 is now */
  last_accomp = score.midi.num-1;
  //queue_if_accomp_plays_first();  /* should appear before new_start_cond()*/
  //  if (mode == LIVE_MODE)   begin_sampling();   
  //    printf("begin_sampling returned at time = %f\n",now());
  if (using_asio) init_orchestra();
  else start_playing_orchestra();
  //  printf("time = %f\n",now());
// strip_start_playing_orchestra();
}





typedef struct {
  float time;
  int id;  /* event number or DP_ASYNC_ID for a dp (listen) event */
} ASYNC_EVENT;

#define MAX_ASYNC_EVENT 5

typedef struct {
  ASYNC_EVENT event[MAX_ASYNC_EVENT];
  int num;
} ASYNC_LIST;

static ASYNC_LIST async;
static int listen_in_progress;
static int midi_in_progress;
static int listen_pending; 



static void
print_async() {
  int i;

  printf("printing asynchronous events\n");
  for (i=0; i < async.num; i++)  
    printf("time = %f id = %d\n",async.event[i].time,async.event[i].id);
  printf("\n");
}



void
add_async(float time,  int id) {
  int i,j;
  ASYNC_EVENT e;

  e.time = time; e.id = id;
  if (async.num > MAX_ASYNC_EVENT) {
    printf("out of room in async event list\n");
    exit(0);
  }
  for (i=0; i < async.num; i++) if (e.time < async.event[i].time) break;
  for (j=async.num-1; j >= i; j--) async.event[j+1] = async.event[j];
  async.event[i] = e;
  /*  printf("time = %f\n",e.time); */
  if (i == 0) set_async_event(e.time);
  async.num++;
}

static void
remove_async(int id) {
  int i;

  for (i=0; i < async.num; i++) if (async.event[i].id == id) break;
  if (i == async.num) {
    printf("event %d not found in list\n",id);
    exit(0);
  }
  for (; i < async.num-1; i++)  async.event[i] = async.event[i+1];
  async.num--;
}


static void 
reschedule_async(float time, int id) {
  int i;
  ASYNC_EVENT e;

  remove_async(id);
  add_async(time,  id);
}


void
init_async() {
  async.num = 0;
  listen_pending = listen_in_progress = listen_in_progress = 0;
}


static void
execute_listen() {
  listen_in_progress = 1;
  listen_pending = 0;
  async_dp();
  listen_in_progress = 0;
}

static void
execute_midi() {
  midi_in_progress = 1;
  new_conduct();
  midi_in_progress = 0;
}

static void
dequeue_async() {
  int id;

  id = async.event[0].id;
}


void
execute_async() {  /* execute the event a start of the list */
  float now(),error,time; 
  int id;

  if (async.num == 0) { printf("calling execute_async() with no events\n"); exit(0); }
  error = now()-async.event[0].time;
  if (fabs(error) > .01)
    printf("error of %f in execute_async() (%f - %f)\n",error,now(),async.event[0].time);
  id = async.event[0].id;
  dequeue_async();
  if (id == DP_ASYNC_ID && !listen_in_progress) { 
    if (midi_in_progress) listen_pending = 1;
    else execute_listen();
  }
  else {
    execute_midi();
    if (listen_pending) execute_listen();
  }
  if (async.num == 0) { printf("about to screw up\n"); exit(0); }
  remove_async(id);  
  if (async.num) {
    if (async.event[0].time < now())  execute_async();
    else set_async_event(async.event[0].time);
  }
}


void
premature_end_action() {
  printf("performance ended prematurely\n");
  dequeue_event();  /* probably not necessary */
  end_midi();
  end_timer();
 // if (midi_accomp == 0 || mode == SYNTH_MODE) end_playing();  /* either of these might cause error */
  end_sampling();
}


void
end_timer() { /* sun specific procedure */
  printf("end_timer()\n");
  dequeue_event();
}










//#include <sys/time.h>
#include <math.h>
#include "linux.h"
#include "share.c"
#include "global.h"
#include "timer.h"
#include "dp.h"
#include "vocoder.h"
//#include "wasapi.h"
//#include <windows.h>




#define MAX_TIMER_EVENTS 10

typedef struct {
  TIMER_EVENT list[MAX_TIMER_EVENTS];
  int num;
} TIMER_EVENT_LIST;


static TIMER_EVENT_LIST pending;
static TIMER_EVENT running,next_midi;
static int signal_handler_busy = 0;

void
init_timer_list() {
  pending.num = 0;
  running.type = NO_EVENT_RUNNING;
  running.priority = NO_EVENT_RUNNING;
  dequeue_event();
}



TIMER_EVENT 
make_timer_event(int pri, int type, int id, float time) {
  TIMER_EVENT e;

  e.priority = pri;
  e.type = type;
  e.id = id;
  e.time = time;
  return(e);
}



void
print_timer_list() {
  int i;

  if (pending.num == 0) printf("timer list empty\n");
  for (i=0; i < pending.num; i++)
    printf("time = %f\t priority = %d\t type = %d\t id = %d\n",
	   pending.list[i].time, 
	   pending.list[i].priority,
	   pending.list[i].type,
	   pending.list[i].id);
}



//CF:  Remove e from the pending queue, and move it to become the running event
//CF:  e is an index into the event queue (usually 0, the head of queue)
static void
promote_event(int e) {
  int i;

  if (e >= pending.num) {
    printf("weird problem in promte_event e = %d pending.num = %d\n",e,pending.num);
    exit(0);
  }
  running = pending.list[e];
  pending.num--;
  for (i=e; i < pending.num; i++) pending.list[i] = pending.list[i+1];
}


//CF:  look at the first event in the queue
//CF:  and set the OS signal to refer this event
static void
arm_timer() {
  float interval;
  
  if (signal_handler_busy) printf("collsion inarm_timer()\n");
  interval = pending.list[0].time -  now();
  if (interval <= 0) {
    printf("shouldn't be setting a timing event in past: event = %f now = %f\n",
	   pending.list[0].time,now());
    exit(0);
  }
  set_timer_signal(interval);
}



static int
add_timer_event(TIMER_EVENT event) {  /* sorted in increasing order */
  int i,j;

  /*  printf("adding event at %f now = %f\n",event.time,now());*/
  if (pending.num == MAX_TIMER_EVENTS) {
    printf("no room in timer event list\n");
    exit(0); 
  }
  for (i=0; i < pending.num; i++) 
    if (event.time < pending.list[i].time) break;
  for (j = pending.num; j > i; j--) pending.list[j] = pending.list[j-1];
  pending.list[i] = event;
  pending.num++;
  return(i);
}


#define CLOSE_ENOUGH_TO_WAIT .005

void  /* if timer event is new, add it; otherwise change time on existing event */
working_set_timer_event(TIMER_EVENT event) {
  int i,j;
  float cutoff;


#ifdef WINDOWS  
  if (midi_accomp == 0)  return; 
#endif

  /*  if (performance_interrupted) {
    printf("can't do  set_timer_event\n");
    return;
    }*/


  if (signal_handler_busy) printf("collision here\n");
  cutoff = now()+CLOSE_ENOUGH_TO_WAIT;
  for (i=0; i < pending.num; i++) 
    if (event.type == pending.list[i].type && event.id == pending.list[i].id ) 
      break;
  if (i <  pending.num)  pending.list[i].time = event.time;
  /* change time if already in list */
  else i = add_timer_event(event);
  if (i != 0) return;   /* i is position of event just added or time-changed */
  if (pending.list[0].time < cutoff)  
    execute_on_timer_expire();
  else arm_timer();
}



#define TOO_EARLY_TO_LET_SLIDE .01

static void
midi_action() {
    float target,interval,nw,now();
    TIMER_EVENT event;
    void new_conduct();
    
    
    target = score.midi.burst[cur_accomp].ideal_secs;
    //  target -= (audio_skew/ (float)OUTPUT_SR);
    
    /*  printf("playing note %d now at %f (target = %f) note = %s\n",cur_accomp,now(),target,score.midi.burst[cur_accomp].observable_tag);*/
    nw = now();
    //  nw = (mode == SYNTH_MODE) ? audio_out_now() : now();
    interval = target-nw;
#ifdef BOSENDORFER
    interval -= BOSENDORFER_LAG;
#endif
    /*  if (-interval > .03) printf("lag by %f in midi_action\n",-interval);*/
    if (interval > TOO_EARLY_TO_LET_SLIDE) { /*  timer returned too soon.  prob small error */
        printf("earlyyy by %f secs target = %f\n",interval, target);
        event.priority = event.type = PLAYING_EVENT;
        event.time = target;
        event.id = cur_accomp;
        set_timer_event(event);
    }
    else   {
        new_conduct();
    }
}


void  
set_timer_event(TIMER_EVENT event) {
  int i,j;
  float cutoff,interval,target,nw;
  void midi_action();


  if (midi_accomp == 0)  return; 
  if (event.id != cur_accomp || event.type != PLAYING_EVENT) {
    printf("this is bad\n"); 
    exit(0); 
  }
  target = score.midi.burst[cur_accomp].ideal_secs;
  //  target -= (audio_skew/ (float)OUTPUT_SR);

  nw = now();
  //  nw = (mode == SYNTH_MODE) ? audio_out_now() : now();

  interval = target -  nw;
#ifdef BOSENDORFER
  interval -= BOSENDORFER_LAG;
#endif
 if (interval < 0.) interval = .001;
 /* this routine is called indirectly by flush_belief_queue which uses a locking mechanism to prevent
	 overlapping (reentrant) calls to the belief network message passing code.  If we call midi_action
	 directly here, this produces another call to flush_belief_queue.  The collision is handled by
	 simply ignoring any colliding calls.  For this reason we delay the call slighly so the call to
	 the belief code is not a child of itself */
  if ( interval < 0.) {  // time has passed.  Play right now.  
    printf("target time in past ... act now at %f\n",now());
    set_timer_signal(0.);  // unset the pending event.
    midi_action();
    return;
  }  
 // printf("event in %f secs\n",interval); 
  set_timer_signal(interval);
}




#define NO_WAITING_EVENT -1

static int
next_waiting_event() { /* index of highest priority past event.  -1 if none */
  float t;
  int i,curp,curi;

  curi = NO_WAITING_EVENT;
  curp = 0;
  t = now()+CLOSE_ENOUGH_TO_WAIT;
  for (i=0; i < pending.num; i++) {
    if ((pending.list[i].time < t) && (pending.list[i].priority > curp)) {
      curi = i;
      curp = pending.list[i].priority;
    }
  }
  return(curi);
}


int
num_pending_timer_events() {
  return(pending.num);
}



TIMER_EVENT
get_pending_event() {
  return(pending.list[0]);
}


static void
nothing() {
  printf("in nothing: time = %f\n",now()*SR/(float)TOKENLEN);
}


static void
do_action() {
  if (performance_interrupted) {
    printf("not doing action\n");
    return;
  }
  //  printf("type = %d (%d %d %d %d)\n",running.type,PLAYING_AUDIO,PLAYING_EVENT,PROCESSING_TOKEN,STALLING_ORCHESTRA);
  if (running.type == PLAYING_AUDIO)   vcode_action();
  if (running.type == PLAYING_EVENT)      midi_action(); 
   if (running.type == PROCESSING_TOKEN)  dpiter(); 
   if (running.type == STALLING_ORCHESTRA)  stall_orchestra();
}

int am_done = 0;


void
working_execute_on_timer_expire() {  /* time for 0th event in list to be launched */
  float t,w;
  int i,e;
  TIMER_EVENT old_running,x;




if (performance_interrupted) { premature_end_action(); return; }

  //  printf("recieved callaback at %f\n",now());
  /*      t = now();
       if (fabs(t-pending.list[0].time) > .03) 
       printf("late by %f in return from timer\n",t-pending.list[0].time);*/

  if (signal_handler_busy) { printf("signals collidd\n"); exit(0); }
  /*  signal_handler_busy = 1;*/
  //CF:  if priority of the signalled event less than interrupted event
  //CF:  then ignore it (but leave it in the queue)
  if (pending.list[0].priority <= running.priority)  return;
  /* next event's priority not high, so wait until the currently running event is finished. At this
   point will run the pending event.*/

    //CF:  TODO: how does this ever get run, now there's no signal for it?
  /*  while (now() < pending.list[0].time); */
  while ((e = next_waiting_event()) != NO_WAITING_EVENT) {
    w =  pending.list[e].time - now();
   // if (w > 0) Sleep((int) (1000*w));
      if (w > 0) usleep((int) (1000000*w));
    //    while (now() < pending.list[e].time); //CF:  spin until time for pending event
    signal_handler_busy = 1; //CF:  lock 'semaphore'
    old_running = running; //CF:  save interrupted (currently running) event (for prioity planning)
    promote_event(e); //CF: move e from the pending queue and make it the running event


    signal_handler_busy = 0;
    do_action(); //CF:  MAIN ACTION -- most work done here
    running = old_running; //CF:  restore interrupted event
    /*    printf("now = %f next = %f\n",now(),pending.list[0].time);*/

  } 
if (pending.num) arm_timer();
}


void
execute_on_timer_expire() {  
  midi_action();
}


void test_my_timer() {
  TIMER_EVENT t;
  int status;

  t.priority = 1;
  t.time = 1.;
  t.type = 1;

  add_timer_event(t);
  while(am_done == 0) pause();
}

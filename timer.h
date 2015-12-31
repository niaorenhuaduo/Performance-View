
#ifndef TIMER_DEFS
#define TIMER_DEFS


#define NO_EVENT_RUNNING 0   /* for now these defines serve as priorities as well */
#define STALLING_ORCHESTRA 1
#define PROCESSING_TOKEN 2
#define PLAYING_EVENT 3
#define PLAYING_AUDIO 4



/*#define PROCESSING_TOKEN 1
#define PLAYING_EVENT 2
#define PLAYING_AUDIO 3*/

typedef struct {
  float time;
  int   priority;  /* higher number is higher priority */
  int   type;   
  int   id; /* two TIMER_EVENTS with same type *and* id together 
	       correspond to same event */
} TIMER_EVENT;


void test_my_timer();
void execute_on_timer_expire();
void set_timer_event(TIMER_EVENT event);
TIMER_EVENT get_pending_event();
int num_pending_timer_events();
void print_timer_list();
void init_timer_list();
TIMER_EVENT make_timer_event(int pri, int type, int id, float time);

#endif



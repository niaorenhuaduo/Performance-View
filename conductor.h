#ifndef CONDUCTOR
#define CONDUCTOR

#define DP_ASYNC_ID -1



void queue_if_accomp_plays_first();
void new_start_cond(float start,float end,float ms);
void set_midi_buffer(char *buff, int *len);
void begin_pv(float start_meas);
void set_accomp_range();
int begin_live(float start_meas);
void new_synchronize(float start_meas);
void eval_accomp_note_times();
void make_performance_data();
void init_async();
void add_async(float time,  int id);
void execute_async();
void async_queue_event();
void newer_start_cond(RATIONAL s, RATIONAL e);
void new_conduct();
void premature_end_action();
void end_timer();
void queue_event();


#endif



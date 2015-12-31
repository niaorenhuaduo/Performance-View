
#ifndef MIDI
#define MIDI

#define PEDAL_COM 0xb0
#define SUSTAIN_PEDAL 64
#define RIGHT_PEDAL 67
#define PEDAL_ON 127
#define PEDAL_OFF 0


typedef struct {
  float time;
  int   command;
  int   d1;
  int   d2;
} SEQ_EVENT;

#define MAX_SEQ 100000

typedef struct {
  int num;
  //  SEQ_EVENT event[MAX_SEQ];
  SEQ_EVENT *event;
} SEQ_LIST;


void compute_pedaling_from_bosendorfer(char *file);
void windows_play_ideal();
void windows_play_seq(SEQ_LIST *s);
void collect_midi_data();
void accomp_time_tag(RATIONAL rat, float time, char *tag);
void write_seq(char *s);
void read_midi_velocities();
void collect_midi_roland_digital();
void write_velocities_from_example();
void read_seq_file(char *s, SEQ_LIST *sq);
void midi_change_program(int channel, int program);
void match_midi();
void create_midi_from_seq(char *name);

#endif


#ifndef NEW_SCORE
#define NEW_SCORE

#include <stdio.h>


typedef struct {
  int num;
  int den;
} RATIONAL;


typedef struct {
  RATIONAL wholerat;
  float whole_secs; /* secs per whole note */
  RATIONAL unit;
  int bpm;
  int hand;    // is a hand set tempo (as opposed to one that comes from midi)
  // hand settings will override training
} TEMPO_SETTING;

typedef struct {
  RATIONAL start;
  RATIONAL end;
  char tag[100];
  int score_supplied; // true if this range came from the score, otherwise user supplied
} PERFORM_RANGE;




#ifdef JAN_BERAN
#define MAX_PERFORM_RANGE 300
#else
#define MAX_PERFORM_RANGE 30
#endif

typedef struct {
  int num;
  PERFORM_RANGE list[MAX_PERFORM_RANGE];
} RANGE_LIST;


#define MAX_TEMPO_SETTINGS 200

typedef struct {
  int num;
  TEMPO_SETTING el[MAX_TEMPO_SETTINGS];
} TEMPO_LIST;



typedef struct {
  char type[20];
  RATIONAL pos;
  RATIONAL unit;
  int bpm;
} HAND_MARK;


#define HAND_MARK_MAX 200

typedef struct {
  int num;
  HAND_MARK list[HAND_MARK_MAX];
} HAND_MARK_LIST;

#define HAND_MARK_LIST_MAX 30

typedef struct {
  int num;
  HAND_MARK_LIST snap[HAND_MARK_LIST_MAX];
  char examp[HAND_MARK_LIST_MAX][100];
} HAND_MARK_LIST_LIST;



#define SOLO_CUE_NUM 1
#define ACCOM_CUE_NUM 2

int solo_measure_empty(int m);
void add_solo_event(int tag, int c, int n, float t, int v, RATIONAL rat, int trill);
void set_mirex_score();

int is_solo_rest(int i);
void add_tempo_list(RATIONAL wr, float ws, RATIONAL u, int bpm, int hand);
void del_tempo_list(RATIONAL wr);
int read_player_tempos();
void select_score(char *name);
float meas2score_pos(char *s);
RATIONAL string2wholerat(char *s);
int rat_cmp(RATIONAL r1,RATIONAL r2);
void wholerat2string(RATIONAL whole, char *s);
int coincident_solo_index(RATIONAL r);
int coincident_midi_index(RATIONAL r);
RATIONAL wholerat2measrat(RATIONAL whole);
int wholerat2measnum(RATIONAL whole);
RATIONAL add_rat(RATIONAL r1,RATIONAL r2);
RATIONAL mult_rat(RATIONAL r1,RATIONAL r2);
RATIONAL sub_rat(RATIONAL r1,RATIONAL r2);
RATIONAL div_rat(RATIONAL r1,RATIONAL r2);
float rat2f(RATIONAL r);
int is_a_rest(int d);
void read_midi_score();
int gcd(int n1, int n2);
int read_midi_score_input(char *name);
int measrpt2measindex(int meas, int rpt);
int max_repeat_val(int meas);
void write_player_cues();
void write_player_tempos();
int file_type(char *name);
int put_in_cue(RATIONAL r);
int put_in_cue_okay(RATIONAL r);
int put_in_solo_xchg(RATIONAL r);
int put_in_solo_xchg_okay(RATIONAL w);
int put_in_accomp_xchg(RATIONAL w);
int put_in_accomp_xchg_okay(RATIONAL w);
void write_player_hnd();
void write_hnd_to_file(HAND_MARK_LIST *hnd, FILE *fp);
void make_hand_mark_list(HAND_MARK_LIST *hnd);
void read_hand_mark_lists(HAND_MARK_LIST_LIST *h);
int read_hand_mark(HAND_MARK_LIST *h, char *name);
int lefteq_solo_index(RATIONAL r);
 
#endif

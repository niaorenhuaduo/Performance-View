#ifndef KALMAN
#define  KALMAN


#define MAXDIM 4  /* maybe 2 is enough */

typedef struct {
  int rows;
  int cols;
  float el[MAXDIM][MAXDIM];
} MX;

typedef struct {
  MX mean;
  MX var;
} JN;  



void phrase_span(int phrase, int *start_solo, int *last_solo, int *start_accom, int *last_accom);
void play_accomp_phrase();
void play_accomp_with_recording();
void play_seq();

#endif

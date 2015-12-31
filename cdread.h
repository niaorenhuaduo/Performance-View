
#ifndef CDREAD
#define CDREAD


#define ID_COUNT 100
#define ID_GAP 10
#define FIRST_CHUNK 1024

typedef struct {
  int first_track;
  int last_track;
  char text[500];
  //  unsigned char id[ID_COUNT];
  unsigned int id;
} CD_INFO;



int read_mmo_data(char *audio, CD_INFO *info);
int read_mmo_data_background(char *audio, CD_INFO *info);
int percent_cd_progress();
int check_cd(char *audio, CD_INFO *info);
int read_cd_iter();


#endif


#ifndef CLASS
#define CLASS

// int state2index(int st,int p, FULL_STATE fs);

#define MAX_SIG  20 //15    /* maximum number of singnature vals for feature */

typedef struct {
  int lo;
  int hi;
  float div[MAX_SIG];
  float group;  /* half steps grouped together in cluster */
} PITCH_INTERVAL;


typedef struct {
  int pos;
  int index;
  float *acc_spect;
  int shade;
  int midi_pitch;
  int last_pitch;
  SOUND_NUMBERS *acc_notes;
  float last_accomp_time; // #secs until last accomp note 
} SA_STATE;


void train_class_probs();
void piano_table_spect(SA_STATE sas, float *meld);
int spect_train_pos(int pos, int n);
float midi2omega(float m);
int read_cdfs_file(char *quant_file);
int read_distributions_file(char *dist_file);
float time_to_last_accomp(int frame);
SOUND_NUMBERS *sounding_accomp(int frame);
void init_sa_pitch_signature_table(SOUND_NUMBERS *acc_notes);
void sndnums2string(SOUND_NUMBERS s, char *target);
void compute_diff_spect();

SA_STATE set_sa_state(float *acc_spect, int index, int shade, int midi_pitch, int last_pitch, float last_acc, SOUND_NUMBERS  *sn, int pos);

float* get_overlap_accomp_spect(int frame);
void frame2accomp_index(int frame, int *index, float *secs);
void arb_meld(int n, float *p, float **q, float *meld);
void  last_attack_desc(int frame, float *secs, float **spect);
void superpos_model(SOUND_NUMBERS notes, float *spect);
void gauss_mixture_model(SOUND_NUMBERS notes, float *spect);
void solo_gauss_mixture_model(SOUND_NUMBERS notes, float *spect, int *peaks);
float* frame2spect(int frame);
void set_atckspect(int frame);
float* frame2atckspect(int frame);
void clear_spect_like_table();
void gauss_mixture_model_attack(SOUND_NUMBERS notes, float *spect);
float sa_class_like(SA_STATE sas);
void save_spect();
void get_spect_hyp(float *acc_spect, SA_STATE sas, float *meld);
void get_acc_spect_hyp(float *acc_spect, SA_STATE sas, float *meld);
float spect_like(float *model);
void gauss_accomp_mixture_model(SOUND_NUMBERS notes, float *spect);
void audio_level();
float max_audio_level();
void read_background_model(char *s);
void simple_meld_spects(float *s1, float *s2, float *sout, float p);
void meld_spects(float *s1, float *s2, float *sout, float p);
void analyze_background(int lo, int hi);
void read_room_response();
void  read_ambient();
void poisson_like_test();
void solo_gauss_mixture_model_tune(SOUND_NUMBERS notes, float tune, float *spect);
void piano_mixture_model(SOUND_NUMBERS notes, float *spect);
void atck_piano_mixture_model(SOUND_NUMBERS notes, float *spect);
float* get_background_spect();


#endif


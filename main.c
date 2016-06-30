


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "share.h"
#include "global.h"
#include "dp.h"
#include "linux.h"
#include "gui.h"
#include "vocoder.h"
#include "audio.h"
#include "graph.h"
#include "class.h"
#include "matrix_util.h"
#include "wasapi.h"


extern PITCH *sol;
extern int freqs;
extern float *data;
extern float *spect;


char scorename[200];  /* phasing this out */
float startmeas,endmeas;




static void
makepitches() {
  float w,a,c,hs,/*hz2omega(),*/midc;
  int i,j;
  FILE *fp;
 
  sol = (PITCH *) malloc((OCTAVES*12+1)*sizeof(PITCH));
  //  hs = pow(2.,1./12.);
  //a = hz2omega(440.);  /* ththis is tuning A */

  for (i=0; i < OCTAVES*12; i++) 
    sol[i].omega = hz2omega(440.*pow(2.,(i-69)/12.)); 
 return;
  
 

  c = a*hs*hs*hs;      /* this is C above middle C */
  w = midc = c/2.;     /* this is middle C */
  for (i=MIDI_MIDDLE_C; i >= 0; i--) {
    sol[i].omega = w; 
    w /= hs;
  }
  w = midc;
  for (i=MIDI_MIDDLE_C; i < OCTAVES*12; i++) {
    sol[i].omega = w; 
    w *= hs;
  }
}


alloc_audio_buff() {
    extern unsigned char *audiodata;
    
    audiodata = (unsigned char*) malloc(MAXAUDIO);
    if (audiodata == NULL) printf("couldn't allocate audio data buffer\n");
    audiodata_target = (unsigned char*) malloc(MAXAUDIO);
    if (audiodata_target == NULL) printf("couldn't allocate audio data buffer\n");
    synth_pitch = (unsigned char*) malloc(MAX_SAMPLE*2);
    if (synth_pitch == NULL) printf("couldn't allocate synth pitch buffer\n");
    cumsum_freq = (float*) malloc(MAX_SAMPLE);
    if (cumsum_freq == NULL) printf("couldn't allocate synth pitch buffer\n");
    //  printf("max frames = %d\n",MAX_FRAMES); exit(0);
#ifdef ORCHESTRA_EXPERIMENT
    orchdata = (unsigned char*) malloc(MAXAUDIO);
    if (orchdata == NULL) printf("couldn't allocate orchdata buffer\n");
#endif
    

}

//#include <shlobj.h>


int
maininit() {
  SOUND_NUMBERS restnotes;
  int result;
  char pth[1000];

/*#ifdef DISTRIBUTION
  TCHAR szPath[MAX_PATH];
  SHGetFolderPath( NULL, CSIDL_COMMON_APPDATA , NULL, 0, szPath);
  strcpy(user_dir,szPath);
  strcat(user_dir,"/Music Plus One/");
#else
  strcpy(user_dir,"user/");
#endif*/
    
/*    chdir("/Users/craphael");
    
    getcwd(pth,1000);
    strcat(pth,"/");
    
  strcpy(user_dir,"user/"); */
  strcpy(pth,user_dir);
  strcat(pth,"output.txt");  // write output into the user directory
    

//  freopen("output.txt","w",stdout);

  if (freopen(pth,"w",stdout) == 0) return(0);
 // if (read_sound_info() == 0) return(0); // Is this needed anymore?
  get_mix();
  SetMasterVolumeMax();
  set_os_version();
  //  result = is_vista_running();


 // strcpy(user_dir,"audio");
  freqs = FREQDIM/2;
  alloc_gnode_buff();

  clear_matrix_buff();
  init_like(); 
  init_matrices();
/*  readtable(); */
  scorename[0] = 0;   /* score unset */

  data = (float *) malloc(FRAMELEN*sizeof(float));
  data48k = (float *) malloc(FRAMELEN_48k*sizeof(float));
  orch_data_frame = (float *) malloc(FRAMELEN*sizeof(float));

  spect = (float *) malloc(freqs*sizeof(float));
  strcpy(score_dir,BASE_DIR); 
  strcat(score_dir,"scores/");
  makepitches();
  alloc_audio_buff();
  alloc_matrix_buff();
  alloc_perm_matrix_buff();
  alloc_snapshot_buffer();
  alloc_node_buff();
  init_template();
  init_class();
/*train_cdfs(); exit(0);  */
#ifndef WINDOWS
  read_cdfs(); 
  read_distributions();
#endif
  //   init_midi();
  restnotes.num = 0;
  start_pos.num = -1;
  //  superpos_model(restnotes, restspect);
  gauss_mixture_model(restnotes, restspect);
  vcode_initialize();
 // read_room_response();
 // read_ambient();
  make_downsampling_filter();
  return(1);
}




accompguide()  {
  int n;
  FILE *fp;
  
  for (n=0; n < score.midi.num-1 ; n++) 
    if (score.midi.burst[n].time != score.midi.burst[n+1].time)
      if (score.midi.burst[n].size > .001) 
        fprintf(fp,"%f %f\n",score.midi.burst[n].time,score.midi.burst[n].size);
}



extern int *notetime;

void
selectscore() {
  char file[500];
  FILE *fp;
  int i;

  printf("enter the score file (no suffix): ");
  scanf("%s",scorename);
  strcpy(scoretag,scorename);
/*  if (readbinscore() == 0)  */
    if (readscore() == 0)  return;
  

/*    else writebinscore();*/
}

parsedata() {
  char file[500];




  /*  set_range(); */
  /*  if (readaudio() == 0) {  */
  read_audio_indep();
  parse();

}




test_score() {
  make_midi_all();
  
}





mainmenu() {
  printf("0 : parse\n"); //CF:  score matching of wav solo to MIDI represenation (assumes HMM has already been fwd-bkwrd trained)
  printf("1 : select score\n"); //CF:  not used
  printf("2 : test score\n");
  printf("3 : rehearse\n");
  printf("4 : train probabilities\n");
  printf("5 : synthetic play\n");
  printf("6 : test probabilities\n");
  printf("7 : make the graph\n");
  printf("8 : play events\n");
  printf("9 : update training\n");
  printf("10: read trained score\n");
  printf("11: write trained score\n");
  printf("12: au to wav\n");
  printf("13: play accomp\n");
  printf("14: exit\n");
  printf("15: test new class\n");
  printf("16: live rehearsal\n");
  printf("17: test_belief\n");
  printf("18: play accompaniment phrase\n");
  printf("19: paste on accompaniment\n");
  printf("20: make graphs of belief networks for paper\n");
  printf("21: play sequencer file\n");
  printf("22: synthetic updates\n");
  printf("23: train accompaniment rhythm model\n");
  printf("24: posterior test\n");
  printf("25: potential test\n");
  printf("26: belief graph test\n");
  printf("27: paste on accomp\n");
  printf("28: match sequencer files to score\n");
  printf("29: synthesize mean accomp\n");
  printf("30: play trained accompaniment performance\n");
  printf("31: record a rolad digital piano midi  performace\n");
  printf("32: write velocities from example\n");
  printf("33: simulated play (no regard for clock)\n");
  printf("34  record audio in background\n");
  printf("35  train composite bbn model\n");
  printf("36  select nick score\n");
  printf("37  annotate match\n");
  printf("38  play solo part\n");
  printf("39  graphical user interface\n");
  printf("40  gui spectrogram\n");
  printf("41  vocoder test\n");
  printf("42  play back parse\n");
  printf("43  read midi score\n");   //CF:  load score, that was previously created from a MIDI file by midi_score
  printf("44  vocoder test\n");
  printf("45  combine raw and orchestra audio channles\n");
  printf("46  gauss dp parse\n");
}

extern int *notetime;

/*main() {
  char c[10];
  int  x;
  

  maininit();
  //  gui_choose_score();
  //  gui_choose_player();
  //  make_settable_names(); exit(0);
  //         gui_top_level();
  //          printf("top level ended\n");
  //          exit(0);
      //   gui_play_music();
  //       gui_review_performance();
  //         gui_learn_from_rehearsal();
  //  gui_test_xpm();
  //  exit(0);

  //    scramble_phase(); exit(0);
  while (x != 14) {
    mainmenu();
    scanf("%d",&x);
    if (x == 0) 
		parsedata();
    if (x == 1) selectscore(); //CF:  not used -- old style score
    if (x == 2) test_score();
    if (x == 3) rehearse(); 
    if (x == 4) train_class_probs();
    if (x == 5)  synthetic_play();
    if (x == 6) test_probs();
    if (x == 7) { mode = FORWARD_MODE; firstnote = 0; lastnote = score.solo.num-1; make_dp_graph(); }
    if (x == 8) play_events();
    if (x == 9) update_training();
    if (x == 10) read_trained_score();
    if (x == 11) write_trained_score();
  
    if (x == 13)   play_accomp();
    if (x == 14) { printf("exiting\n"); exit(0);} 
    if (x == 15) test_main(); 
    if (x == 16) live_rehearse();
    if (x == 17) belief_test();
    if (x == 18) play_accomp_phrase();
    if (x == 19) paste_on_accomp();
    if (x == 20) make_belief_network_graphs();
    if (x == 21) play_seq();
    if (x == 22) synthetic_updates();
    if (x == 23) bbn_training_test();
    if (x == 24) posterior_test();
    if (x == 25) potential_test();
    if (x == 26) belief_graph_test();
    if (x == 27) paste_on_accomp();
    if (x == 28) match_midi();
    if (x == 29) synthesize_mean_accomp();
    if (x == 30) play_accomp_performance();
    if (x == 31) collect_midi_data();
    if (x == 32) write_velocities_from_example();
    if (x == 33) simulate_play();
    if (x == 34) record_in_background();
    if (x == 35) bbn_train_composite_model();
    if (x == 36) select_nick_score();
    if (x == 37) annotate_match();
    if (x == 38) play_solo_part();
    //    if (x == 39) gui_top_level();
    if (x == 40) gui_interactive_spect();  //gui_test_xpm();
    if (x == 41) vocoder_test();
    if (x == 42) playback_parse();
    if (x == 43) read_midi_score();
    if (x == 44) gui_vocoder_test();
    if (x == 45) combine_parts();
    if (x == 46)  gauss_dp_parse(); 
    if (x == 47)  gui_vocoder();
  }    
}



*/


















//#define TRAIN_DIR "/usr/raphael/train/romance1/lexicon/" 

/*#define TRAIN_DIR "/usr/raphael/train/nick/"*/

/*#define TRAIN_DIR "/usr/raphael/train/nick_1st/"  // okay */

// #define TRAIN_DIR "/usr/raphael/train/schumannaa/"



//#define TRAIN_DIR "/home/raphael/train/rachmaninov_pc2/"
//#define TRAIN_DIR "/home/raphael/train/mimi2/"


/*#ifdef VIOLIN_EXPERIMENT
#define TRAIN_DIR "/home/raphael/train/violin/"
#else
#define TRAIN_DIR "/home/raphael/train/mimi_longer_frame/"
#endif*/


#ifdef VIOLIN_EXPERIMENT
  #define TRAIN_DIR "/home/raphael/train/violin/"
#else 
#ifdef PIANO_EXPERIMENT
  #define TRAIN_DIR "/home/raphael/train/rachmaninov_pc2/"
#else
#ifdef JAN_EXPERIMENT
  #define TRAIN_DIR "/home/raphael/train/jan/"
#else 
  #define TRAIN_DIR "/home/raphael/train/mimi_longer_frame/"
#endif
#endif
#endif






//#define TRAIN_DIR "/home/raphael/train/sonatine/"  /* this is what I'm using for mimi */




//#define TRAIN_DIR "/home/raphael/train/mozart_4tet_2/"



//#define TRAIN_DIR "/home/raphael/train/mozart_4tet/"

//#define TRAIN_DIR "/home/raphael/train/mimi/"


// #define TRAIN_DIR "/home/raphael/train/horn2/"
//#define TRAIN_DIR "/home/raphael/train/test/"
// #define TRAIN_DIR "/home/raphael/train/marcello/"


#define SAMPLE_DELIVERY 112  /* when reading in non-blockng mode the computer
				makes sound data avaiable to my program
				for reading at various times.  This
				is the data chunk size (in bytes) that
				is used most often.  This number was
				found by making repeated non-blocking calls
				to read from the audio stream and seeing
				what chunk sizes are received */

#define NOMINAL_SR       8000     /* in requesting NOMINAL_SR actually get SR */
//#define SR       8010     /* the true sampling rate. */
//#define SR       8000.5    /* the true sampling rate. */

/* should rethink the above for inspiron */


#define MIDI_INTERFACE DIRECT_SERIAL
//#define MIDI_INTERFACE MS124T

#define OVERSAMPLE_FACTOR  1   /* for earlier delivery of data */

#define IO_CHANNELS 2

/*#define TRAIN_DIR "/usr/raphael/train/romance1/new_energy/"*/

/*#define TRAIN_DIR "/usr/raphael/train/romance1/yale/"     */
/*#define TRAIN_DIR "/usr/raphael/train/romance1/icmc/"     */
/*#define TRAIN_DIR "/usr/raphael/train/romance1/accumulate_fix/"  */
/*#define TRAIN_DIR "/usr/raphael/train/romance1/test/" */

/*#define MS124W   // put this in to use midiator MS124W (fancy) , else use MS124T */
/*#define MIDI_LATENCY //0.*/ /*.5 */ // (for midi_send_time) disklavier has 500 ms latency */











#include "share.h"
#include "global.h"
#include "audio.h"
#include "vocoder.h"

/*#include <unistd.h>
 #include <CoreAudio/CoreAudio.h>
 #import <CoreAudio/CoreAudioTypes.h>
 
 #include <AudioUnit/AUComponent.h>
 #include <AudioToolbox/AudioServices.h>*/



//#include <AudioToolbox/AudioConverter.h>
//#include <CoreAudio/CoreAudio.h>
//#include <CoreFoundation/CoreFoundation.h>


#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>

#include <AudioToolbox/AudioQueue.h>



#define CA_SAMPS_PER_BUFFER 1024
#define AUDIO_SAMPS_PER_BUFFER 1024  // this are redundant defines ...


static unsigned char *both_channels;
static unsigned char *global_audio_buff;
static long played_samples = 0;
//static int total_samps_played = 0;
static long lowres_samps_produced = 0;
int input_callbacks_done,output_callbacks_done;  /* number of frames we have access to */
static UInt64 cur_output_host_time,output_zero,input_zero;  // in nanoseconds
static int zero_input_frame,zero_output_frame;  /* the first several frames will be dropped without action so that the
                                                 zeroth input and output audio frames corrspond to approx the same real time */


void prepare_hires_solo_output(int chunk) {
    char file[500];
    
    strcpy(file,audio_data_dir);
    strcat(file,current_examp);
    strcat(file,".48k");
    init_io_circ_buff(&solo_out,file,chunk);
    //  hires_out_fp = solo_out.fp;  // carrying around the fp in addition to the strucutre is redundant and should be fixed
}



int
frames_behind() {
    return((lowres_samps_produced - (token+2)*SKIPLEN)/SKIPLEN);
}

int record_frame_ready(int f) {
    //  extern long played_samples,lowres_samps_produced;
    float s;
    
    if (mode == SYNTH_MODE || mode == LIVE_MODE || mode == SOUND_CHECK_MODE || mode == TEST_BALANCE_MODE ) {
        if (token == -1) return(lowres_samps_produced >= FRAMELEN);
        return(lowres_samps_produced >= (token+2)*SKIPLEN);
        /* should be (token+1)*SKIPLEN (see samples2data) but token is incremented *after* this test passed so
         need token+2 */
    }
    //printf("now = %f audio out secs = %f skew = %d\n", now(),wasapi_audio_out_now(),audio_skew);
    return ( played_samples/(float)OUTPUT_SR >  tokens2secs((float)token+1));  // should this be token+2 as above??
    
}




//static unsigned char *left_channel;
//static unsigned char *rite_channel;

int
write_samples(unsigned char *interleaved, int num) {
    int i;
    
    memcpy(both_channels, interleaved, num*2*BYTES_PER_SAMPLE);
    play_frames_buffered++;
}



OSStatus	MyRenderer(void 				*inRefCon,
                       AudioUnitRenderActionFlags 	*ioActionFlags,
                       const AudioTimeStamp 		*inTimeStamp,
                       UInt32 						inBusNumber,
                       UInt32 						inNumberFrames,
                       AudioBufferList 			*ioData)

{
    extern void buffer_audio_samples(int samps, int played);
    
    
    // printf("callback # %d\n",play_frames_buffered);
    both_channels = ioData->mBuffers[0].mData;
    //     rite_channel = ioData->mBuffers[1].mData;
    buffer_audio_samples(inNumberFrames,played_samples);
    played_samples += inNumberFrames;
    return noErr;
    
    //	RenderSin (sSinWaveFrameCount, inNumberFrames,  ioData->mBuffers[0].mData, sSampleRate, sAmplitude, sToneFrequency, sWhichFormat);
    
    //we're just going to copy the data into each channel
    for (UInt32 channel = 1; channel < ioData->mNumberBuffers; channel++)
        memcpy (ioData->mBuffers[channel].mData, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);
    
    //	sSinWaveFrameCount += inNumberFrames;
    
}





static const int kNumberBuffers = 3;
AudioUnit	gOutputUnit;


typedef struct  {
    AudioStreamBasicDescription   mDataFormat;
    AudioQueueRef                 mQueue;
    AudioQueueBufferRef           mBuffers[kNumberBuffers];
    //   AudioFileID                   mAudioFile;
    UInt32                        bufferByteSize;
    SInt64                        mCurrentPacket;
    UInt32                        mNumPacketsToRead;
    AudioStreamPacketDescription  *mPacketDescs;
    bool                          mIsRunning;
} AQPlayerState;

AQPlayerState aqData;


int
ready_for_play_frame(int frame) {
    return(played_samples  > frame*SKIPLEN*IO_FACTOR);
}


int
play_frames_played() {
    return(played_samples / (SKIPLEN*IO_FACTOR));
}




void
set_timer_signal(float interval) {
    printf("set_timer_signal not yet implemented\n");
}

int
get_delay() {
    printf("get_delay not yet implemented\n");
}

void
play_midi_chunk(unsigned char *buff, int len) {
    printf("play_midi_chunk not yet implemented\n");
}

void
play_audio_buffer() {
    printf("play_audio_buffer not yet implemented\n");
}

void
set_os_version() {
    printf("set_os_version not yet implemented\n");
}

int
init_midi() {
    printf("init_midi not yet implemented\n");
}

void SetMasterVolumeMax() {
    printf("SetMasterVolumeMax not yet implemented\n");
}



void
dequeue_event() {
    printf("dequeue_event not yet implemnted\n");
}

void
begin_playing() {
    printf("begin_playing not yet implemneted\n");
}


void
init_timer() {
    printf("init_timer not yet implemented\n");
}


void
set_async_event(float time) {
    printf("set_async_event not yet implmented\n");
}

void
move_playback_cursor(int pos) {
    printf("move_playback_cursor not yet implemented\n");
}


int
write_audio_channels(float *left, float *rite, int samps) {
    int i;
    float total;
    static float mx;
    
    
    for (i=1; i < samps; i++) total += fabs(left[i]-left[i-1]) + fabs(rite[i]-rite[i-1]);
    total /= (2*samps);  // average absolute diff between neighboring samples.
    /* if (total > mx) {
     mx = total;
     printf("white noise number is %f\n",total);
     
     } */
    if (total > .15) {  // never seem to get values as large as  .02, even with clipping.
        performance_interrupted = LIVE_ERROR;
        strcpy(live_failure_string,"audio processing error");
        return(0);  // don't write any new audio to the output.
    }
    
    
    //    printf("channels = %f %f\n",left[0],rite[0]);
    for (i=0; i < samps; i++) {
        float2sample(left[i],global_audio_buff + 2*BYTES_PER_SAMPLE*i);
        float2sample(rite[i],global_audio_buff + 2*BYTES_PER_SAMPLE*i + BYTES_PER_SAMPLE);
    }
    return(0);
}



int
get_playback_cursor() {
    printf("get_playback_cursor not yet implemented\n");
}

void
samples_recorded() {
    printf("samples_recoorded not yet implemented\n");
}

void
samples_played() {
    printf("samples_played not yet implemented\n");
}

int
prepare_sampling() {
    printf("prepare_smapling not yet implemented\n");
}

int
begin_sampling() {
    printf("begin_sampling not yet implemented\n");
}


void
install_signal_action(void (*func)(int)) {
    printf("insall_signal_action not yet implemented\n");
}



static struct timeval time_zero;

float
now() {
    struct timeval t;
    float s1,s2,s;
    
    gettimeofday(&t,NULL);
    
    
    s = (t.tv_sec-time_zero.tv_sec)+  (t.tv_usec-time_zero.tv_usec) / 1.E6;
    return(s);
    
    //  printf("now now implemented yet\n");
    //  return(0);
}

float
system_secs() {
    printf("system_secs not yet implemented\n");
}

int
init_clock() {
    gettimeofday(&time_zero,NULL);
    //  printf("init_clock not yet implmeneted\n");
}



static unsigned char *ca_buffer;




static void
HandleOutputBuffer (void *aqData, AudioQueueRef inAQ,AudioQueueBufferRef inBuffer) {
    AQPlayerState *pAqData = (AQPlayerState *) aqData;
    extern void buffer_audio_samples(int samps, int played);
    
    //    if (pAqData->mIsRunning == 0) return;
    ca_buffer = (unsigned char *) inBuffer;  // will write here
    
    buffer_audio_samples(pAqData->mCurrentPacket,CA_SAMPS_PER_BUFFER);
    
    // this is a little kludgy, but uses buffer_audio_samples as is, only using different changing write_samples
    //  UInt32 numBytesReadFromFile;
    UInt32 numPackets = CA_SAMPS_PER_BUFFER;
    if (numPackets > 0) {
        AudioQueueEnqueueBuffer (pAqData->mQueue, inBuffer,(pAqData->mPacketDescs ? numPackets : 0), pAqData->mPacketDescs);
        pAqData->mCurrentPacket += numPackets;
    }
    else {
        AudioQueueStop (pAqData->mQueue,false);
        pAqData->mIsRunning = false;
    }
}


int
CreateDefaultAU()
{
    OSStatus err = noErr;
    
    // Open the default output unit
    ComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) { printf ("failed in FindNextComponent\n"); return(0); }
    
    err = OpenAComponent(comp, &gOutputUnit);
    if (comp == NULL) { printf ("FAILED IN OpenAComponent=%ld\n", err); return(0); }
    
    // Set up a callback function to generate output to the output unit
    AURenderCallbackStruct input;
    input.inputProc = MyRenderer;
    input.inputProcRefCon = NULL;
    
    err = AudioUnitSetProperty (gOutputUnit,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                0,
                                &input,
                                sizeof(input));
    if (err) { printf ("failed in AudioUnitSetProperty-CB=%ld\n", err); return(0); }
    
    return(1);
    
}


int
prepare_playing() {
    OSStatus err = noErr;
    
    played_samples = 0;
    CreateDefaultAU(); // don't know how often this needs to be done ...
    
    
    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = OUTPUT_SR;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger
    | kLinearPCMFormatFlagIsPacked;
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBitsPerChannel = 16;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame  =   streamFormat.mChannelsPerFrame * sizeof (SInt16);
    streamFormat.mFramesPerPacket = 1;
    
    
    err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                0,&streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) { printf ("AudioUnitSetProperty-SF=%4.4s, %ld\n", (char*)&err, err); return(0); }
    
    UInt32 frames = 2000; //1050;
    err = AudioUnitSetProperty(gOutputUnit,
                               kAudioDevicePropertyBufferFrameSize,
                               kAudioUnitScope_Global, 0,
                               &frames, sizeof(frames));
    
    
    err = AudioUnitInitialize(gOutputUnit);
    if (err) { printf ("AudioUnitInitialize=%ld\n", err); return(0); }
    
    
    /*  Float64 outSampleRate;
     UInt32 size = sizeof(Float64);
     err = AudioUnitGetProperty (gOutputUnit,kAudioUnitProperty_SampleRate, kAudioUnitScope_Output,0,&outSampleRate, &size);
     if (err) { printf ("AudioUnitSetProperty-GF=%4.4s, %ld\n", (char*)&err, err); return; }*/
    
    
    CFRunLoopSourceContext context = { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, MyRenderer };
    CFRunLoopSourceRef source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       source,
                       kCFRunLoopDefaultMode);
    
    
    
    
    err = AudioOutputUnitStart (gOutputUnit);
    if (err) { printf ("AudioOutputUnitStart=%ld\n", err); return(0); }
    
    //  while (performance_interrupted == 0)
    while (currently_playing == 1)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, .1, false);
    
    // REALLY after you're finished playing STOP THE AUDIO OUTPUT UNIT!!!!!!
    // but we never get here because we're running until the process is nuked...
    verify_noerr (AudioOutputUnitStop (gOutputUnit));
    
    err = AudioUnitUninitialize (gOutputUnit);
    if (err) { printf ("AudioUnitUninitialize=%ld\n", err); return(0); }
    
    
}






static OSStatus recordingCallback(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData) {
    
    // TODO: Use inRefCon to access our interface object to do stuff
    // Then, use inNumberFrames to figure out how much data is available, and make
    // that much space available in buffers in an AudioBufferList.
    
    AudioBufferList *bufferList; // <- Fill this up with buffers (you will want to malloc it, as it's a dynamic-length list)
    
    // Then:
    // Obtain recorded samples
    OSStatus status=0;
    
    /*   status = AudioUnitRender([audioInterface audioUnit],
     ioActionFlags,
     inTimeStamp,
     inBusNumber,
     inNumberFrames,
     bufferList);
     checkStatus(status);
     
     // Now, we have the samples we just read sitting in buffers in bufferList
     DoStuffWithTheRecordedAudio(bufferList); */
    return noErr;
}


static OSStatus playbackCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData) {
    // Notes: ioData contains buffers (may be more than one!)
    // Fill them up as much as you can. Remember to set the size value in each buffer to match how
    // much data is in the buffer.
    
    extern void buffer_audio_samples(int samps, int played);
    
    both_channels = ioData->mBuffers[0].mData;
    buffer_audio_samples(inNumberFrames,played_samples);
    played_samples += inNumberFrames;
    return noErr;
}



static OSStatus introductionCallback(void *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp *inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList *ioData) {
    
    static float gain = 1;
    float x;
    int i,finished_playing;
    
    if (performance_interrupted)  gain *= .95;
    finished_playing = (played_samples/inNumberFrames) == (frames*(IO_FACTOR*TOKENLEN)/inNumberFrames);
    if (gain < .001 || finished_playing) {
        end_playing();
        return(noErr);  // done
    }
    
    //    if (inNumberFrames != CA_SAMPS_PER_BUFFER) { printf("wrong number of samples in callback\n"); exit(0); }
    memcpy(ioData->mBuffers[0].mData, orchdata + played_samples*2*BYTES_PER_SAMPLE, inNumberFrames*2*BYTES_PER_SAMPLE);
    for (i=0; i < 2*inNumberFrames; i++) {
        x = gain*sample2float(ioData->mBuffers[0].mData + BYTES_PER_SAMPLE*i);
        float2sample(x,ioData->mBuffers[0].mData + BYTES_PER_SAMPLE*i);
    }
    played_samples += inNumberFrames;
    return(noErr);
}







#define kOutputBus 0
#define kInputBus 1


void failed_iphone_duplex_test() {
    OSStatus status;
    AudioComponentInstance audioUnit;
    
    // Describe audio component
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput; // changed this from below
    // desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    // Get component
    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    // Get audio units
    status = AudioComponentInstanceNew(inputComponent, &audioUnit);
    //   checkStatus(status);
    
    // Enable IO for recording
    UInt32 flag = 1;
    /*   status = AudioUnitSetProperty(audioUnit,
     kAudioOutputUnitProperty_EnableIO,
     kAudioUnitScope_Input,
     kInputBus,
     &flag,
     sizeof(flag));
     //   checkStatus(status);
     
     // Enable IO for playback
     status = AudioUnitSetProperty(audioUnit,
     kAudioOutputUnitProperty_EnableIO,
     kAudioUnitScope_Output,
     kOutputBus,
     &flag,
     sizeof(flag));*/
    //   checkStatus(status);
    
    // Describe format
    AudioStreamBasicDescription audioFormat;
    audioFormat.mSampleRate			= 48000;
    audioFormat.mFormatID			= kAudioFormatLinearPCM;
    audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    audioFormat.mFramesPerPacket	= 1;
    audioFormat.mChannelsPerFrame	= 1;
    audioFormat.mBitsPerChannel		= 16;
    audioFormat.mBytesPerPacket		= 2;
    audioFormat.mBytesPerFrame		= 2;
    
    // Apply format
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  kInputBus,
                                  &audioFormat,
                                  sizeof(audioFormat));
    // checkStatus(status);
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  kOutputBus,
                                  &audioFormat,
                                  sizeof(audioFormat));
    // checkStatus(status);
    
    
    // Set input callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = recordingCallback;
    callbackStruct.inputProcRefCon = NULL;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  kInputBus,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    //   checkStatus(status);
    
    // Set output callback
    callbackStruct.inputProc = playbackCallback;
    callbackStruct.inputProcRefCon = NULL;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  kOutputBus,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    //   checkStatus(status);
    
    // Disable buffer allocation for the recorder (optional - do this if we want to pass in our own)
    flag = 0;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_ShouldAllocateBuffer,
                                  kAudioUnitScope_Output,
                                  kInputBus,
                                  &flag,
                                  sizeof(flag));
    
    // TODO: Allocate our own buffers if we want
    
    // Initialise
    status = AudioUnitInitialize(audioUnit);
    //   checkStatus(status);
}

int
useles_prepare_playing() {
    int err;
    
    aqData.mDataFormat.mFormatID = kAudioFormatLinearPCM;
    aqData.mDataFormat.mSampleRate = OUTPUT_SR;
    aqData.mDataFormat.mChannelsPerFrame = 2;
    aqData.mDataFormat.mBitsPerChannel   = 16;
    aqData.mDataFormat.mBytesPerPacket   =
    aqData.mDataFormat.mBytesPerFrame =
    aqData.mDataFormat.mChannelsPerFrame * sizeof (SInt16);
    aqData.mDataFormat.mFramesPerPacket  = 1;
    aqData.mDataFormat.mFormatFlags =
    kLinearPCMFormatFlagIsBigEndian
    | kLinearPCMFormatFlagIsSignedInteger
    | kLinearPCMFormatFlagIsPacked;
    
    
    aqData.bufferByteSize = CA_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE*IO_CHANNELS;
    aqData.mPacketDescs = NULL;
    
    //  err = AudioQueueNewOutput (&aqData.mDataFormat, HandleOutputBuffer, &aqData, CFRunLoopGetCurrent (),
    //                     kCFRunLoopCommonModes, 0, &aqData.mQueue);
    
    err = AudioQueueNewOutput (&aqData.mDataFormat, HandleOutputBuffer, &aqData, nil,
                               nil, 0, &aqData.mQueue);
    
    //   err = AudioQueueNewOutput (&streamFormat, AudioEngineOutputBufferCallback, self, nil, nil, 0, &outputQueue);
    
    
    
    aqData.mCurrentPacket = 0;
    
    for (int i = 0; i < kNumberBuffers; ++i) {
        err = AudioQueueAllocateBuffer(aqData.mQueue, aqData.bufferByteSize,&aqData.mBuffers[i]);
        //     HandleOutputBuffer(&aqData, aqData.mQueue, aqData.mBuffers[i]);
    }
    
    
    
    Float32 gain = 1.0;
    err = AudioQueueSetParameter(aqData.mQueue,kAudioQueueParam_Volume,gain);
    aqData.mIsRunning = true;
    
    err = AudioQueueStart(aqData.mQueue, NULL);
    do  { CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.25,false); }
    while (aqData.mIsRunning);
    
    AudioQueueDispose (aqData.mQueue, true);
    
}








void end_midi() {
    printf("end_midi not yet implemented\n");
}


int
end_sampling() {
    printf("not yet implemented\n");
}

int
testprobs() {
    printf("testprobs not yet implemented\n");
}

int
wait_for_samples() {
    //printf("wait_for_samples not yet implemented\n");
}

void wait_ms(int ms) {
    printf("wait_ms not yet implemented\n");
}

void checkStatus(int status){
    if (status) {
        printf("Status not 0! %d\n", status);
        //		exit(1);
    }
}



AudioComponentInstance audioUnit;
AudioBuffer tempBuffer;


static OSStatus recordCallback(void *inRefCon,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList *ioData) {
    
    // Because of the way our audio format (setup below) is chosen:
    // we only need 1 buffer, since it is mono
    // Samples are 16 bits = 2 bytes.
    // 1 frame includes only 1 sample
    
    AudioBuffer buffer;
    
    buffer.mNumberChannels = 1;
    buffer.mDataByteSize = inNumberFrames * 2;
    buffer.mData = malloc( inNumberFrames * 2 );
    
    // Put buffer in a AudioBufferList
    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0] = buffer;
    
    // Then:
    // Obtain recorded samples
    
    OSStatus status;
    
    status = AudioUnitRender(audioUnit,
                             ioActionFlags,
                             inTimeStamp,
                             inBusNumber,
                             inNumberFrames,
                             &bufferList);
    checkStatus(status);
    
    // Now, we have the samples we just read sitting in buffers in bufferList
    // Process the new data
    /*	[iosAudio processAudio:&bufferList]; took this out without replace functionality */
    
    // release the malloc'ed data in the buffer we created earlier
    free(bufferList.mBuffers[0].mData);
    
    return noErr;
}

/**
 This callback is called when the audioUnit needs new data to play through the
 speakers. If you don't have any, just don't write anything in the buffers
 */
static OSStatus playCallback(void *inRefCon,
                             AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *inTimeStamp,
                             UInt32 inBusNumber,
                             UInt32 inNumberFrames,
                             AudioBufferList *ioData) {
    // Notes: ioData contains buffers (may be more than one!)
    // Fill them up as much as you can. Remember to set the size value in each buffer to match how
    // much data is in the buffer.
    
    for (int i=0; i < ioData->mNumberBuffers; i++) { // in practice we will only ever have 1 buffer, since audio format is mono
        AudioBuffer buffer = ioData->mBuffers[i];
        
        //		NSLog(@"  Buffer %d has %d channels and wants %d bytes of data.", i, buffer.mNumberChannels, buffer.mDataByteSize);
        
        // copy temporary buffer data to output buffer
        UInt32 size = min(buffer.mDataByteSize, tempBuffer.mDataByteSize); // dont copy more data then we have, or then fits
        memcpy(buffer.mData, tempBuffer.mData, size);
        buffer.mDataByteSize = size; // indicate how much data we wrote in the buffer
        
        // uncomment to hear random noise
        /*
         UInt16 *frameBuffer = buffer.mData;
         for (int j = 0; j < inNumberFrames; j++) {
         frameBuffer[j] = rand();
         }
         */
        
    }
    
    return noErr;
}



//-----------------------------------------------------------------------------------------



#define kOutputBus 0
#define kInputBus 1


int
yushen_audio_test() {
    // ...
    
    
    OSStatus status;
    AudioComponentInstance audioUnit;
    AudioStreamBasicDescription audioFormat;
    AURenderCallbackStruct callbackStruct;
    AudioComponentDescription desc;
    
    // Describe audio component
    desc.componentType = kAudioUnitType_Output;
    // desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    // Get component
    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    // Get audio units
    status = AudioComponentInstanceNew(inputComponent, &audioUnit);
    checkStatus(status);
    
    // Enable IO for recording
    UInt32 flag = 1;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  kInputBus,
                                  &flag,
                                  sizeof(flag));
    checkStatus(status);
    
    // Enable IO for playback
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  kOutputBus,
                                  &flag,
                                  sizeof(flag));
    checkStatus(status);
    
    // Describe format
    audioFormat.mSampleRate			= 48000.00;
    audioFormat.mFormatID			= kAudioFormatLinearPCM;
    audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    audioFormat.mFramesPerPacket	= 1;
    audioFormat.mChannelsPerFrame	= 1;
    audioFormat.mBitsPerChannel		= 16;
    audioFormat.mBytesPerPacket		= 2;
    audioFormat.mBytesPerFrame		= 2;
    
    // Apply format
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  kInputBus,
                                  &audioFormat,
                                  sizeof(audioFormat));
    checkStatus(status);
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  kOutputBus,
                                  &audioFormat,
                                  sizeof(audioFormat));
    checkStatus(status);
    
    
    // Set input callback
    callbackStruct.inputProc = recordingCallback;
    callbackStruct.inputProcRefCon = NULL; //self;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  kInputBus,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    checkStatus(status);
    
    // Set output callback
    callbackStruct.inputProc = playbackCallback;
    callbackStruct.inputProcRefCon = NULL;  //self;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  kOutputBus,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    checkStatus(status);
    
    // Disable buffer allocation for the recorder (optional - do this if we want to pass in our own)
    flag = 0;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_ShouldAllocateBuffer,
                                  kAudioUnitScope_Output,
                                  kInputBus,
                                  &flag,
                                  sizeof(flag));
    
    // TODO: Allocate our own buffers if we want
    
    // Initialise
    status = AudioUnitInitialize(audioUnit);
    checkStatus(status);
    
    status = AudioOutputUnitStart(audioUnit);
    checkStatus(status);
}

//-----------------------------------------------------------------------------------




static AudioDeviceID inputDevice,outputDevice;
AudioUnit mInputUnit,mOutputUnit;
UInt32 mSafetyOffset,mBufferSizeFrames;
AudioStreamBasicDescription	mFormat;
//AudioBufferList *mInputBuffer;
AUGraph mGraph;
AUNode mOutputNode;

static int
get_audio_devices() {
    UInt32 propsize=0;
    AudioObjectPropertyAddress aopa;
    OSStatus err;
    
    propsize = sizeof(AudioDeviceID);
    
    aopa.mSelector = kAudioHardwarePropertyDefaultInputDevice;
    aopa.mScope = kAudioObjectPropertyScopeGlobal;
    aopa.mElement = kAudioObjectPropertyElementMaster;
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &propsize, &inputDevice);
    if (err) return(0);
    
    aopa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    aopa.mScope = kAudioObjectPropertyScopeGlobal;
    aopa.mElement = kAudioObjectPropertyElementMaster;
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &propsize, &outputDevice);
    if (err) return(0);
    return(1);
    
    
    
}

static int
set_sample_rate(Float64 sr) { // for input and output
    UInt32 propsize=0;
    AudioObjectPropertyAddress aopa;
    OSStatus err;
    Float64 actual,target;
    
    propsize = sizeof(Float64);
    aopa.mSelector = kAudioDevicePropertyNominalSampleRate;
    aopa.mScope = kAudioDevicePropertyScopeInput;
    aopa.mElement = kAudioObjectPropertyElementMaster;
    
    target = sr;
    err = AudioObjectSetPropertyData(inputDevice, &aopa, 0, NULL, propsize, &target);
    if (err) return(0);
    AudioObjectGetPropertyData(inputDevice, &aopa, 0, NULL, &propsize, &actual);
    if (fabs(1-target/actual) > .01) return(0);
    
    
    target = sr;
    aopa.mScope = kAudioDevicePropertyScopeOutput;
    err = AudioObjectSetPropertyData(outputDevice, &aopa, 0, NULL, propsize, &target);
    if (err) return(0);
    AudioObjectGetPropertyData(outputDevice, &aopa, 0, NULL, &propsize, &actual);
    if (fabs(1-target/actual) > .01) return(0);
    
    
    return(1);
    
    
}




static OSStatus
EnableIO() {
    OSStatus err = noErr;
    UInt32 enableIO;
    
    ///////////////
    //ENABLE IO (INPUT)
    //You must enable the Audio Unit (AUHAL) for input and disable output
    //BEFORE setting the AUHAL's current device.
    
    //Enable input on the AUHAL
    enableIO = 1;
    err =  AudioUnitSetProperty(mInputUnit,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                1, // input element
                                &enableIO,
                                sizeof(enableIO));
    if (err) { printf("failed enabling Input IO\n"); return(0); }
    
    
    //disable Output on the AUHAL
    enableIO = 0;
    err = AudioUnitSetProperty(mInputUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output,
                               0,   //output element
                               &enableIO,
                               sizeof(enableIO));
    
    if (err) { printf("failed disabling Output IO\n"); return(0); }
    
    return(1);
}


static int
AudioDeviceInit(AudioDeviceID devid, bool isInput)
{
    
    OSStatus err;
    
    //	mID = devid;
    //	mIsInput = isInput;
    if (devid == kAudioDeviceUnknown) return(0);
    
    UInt32 propsize;
    AudioObjectPropertyAddress aopa;
    
    
    // I think the getpropertydata calls may be wrong syntax and may generate errors
    propsize = sizeof(UInt32);
    aopa.mSelector = kAudioDevicePropertySafetyOffset;
    aopa.mScope = kAudioObjectPropertyScopeGlobal;
    aopa.mElement = kAudioObjectPropertyElementMaster;
    verify_noerr(AudioObjectGetPropertyData(devid, &aopa, 0, NULL, &propsize, &mSafetyOffset ));
    
    propsize = sizeof(UInt32);
    aopa.mSelector = kAudioDevicePropertyBufferFrameSize;
    verify_noerr(AudioObjectGetPropertyData( devid, &aopa, 0, NULL, &propsize, &mBufferSizeFrames ));
    
    propsize = sizeof(AudioStreamBasicDescription);
    aopa.mElement = kAudioObjectPropertyElementMaster;
    aopa.mSelector = kAudioDevicePropertyStreamFormat;
    aopa.mScope = isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    verify_noerr(AudioObjectGetPropertyData( devid, &aopa, 0, NULL, &propsize, &mFormat ));
    
    
    return(1);
}



static int
SetInputDeviceAsCurrent(AudioDeviceID in)
{
    UInt32 size = sizeof(AudioDeviceID);
    OSStatus err = noErr;
    
    if(in == kAudioDeviceUnknown) //get the default input device if device is unknown
    {
        AudioObjectPropertyAddress aopa;
        aopa.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        aopa.mScope = kAudioObjectPropertyScopeGlobal;
        aopa.mElement = kAudioObjectPropertyElementMaster;
        
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &size, &in);
        if (err) {printf("failed getting property data in SetInputDeviceAsCurrent\n"); return(0); }
        
    }
    
    AudioDeviceInit(in,true);
    
    //Set the Current Device to the AUHAL.
    //this should be done only after IO has been enabled on the AUHAL.
    err = AudioUnitSetProperty(mInputUnit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global,
                               0,
                               &in,
                               sizeof(in));
    if (err) { printf("failed in setting property as currentdevice\n"); return(0);}
    return(1);
}


#define NUM_RING_BUFF 10



typedef struct {
    int hi;  // next input goes here
    int lo;  // next output comes from here
    Byte data[NUM_RING_BUFF][8192];
} RING_BUFFER;


RING_BUFFER ring_buff;

init_ring_buff() {
    ring_buff.hi = ring_buff.lo = 0; // empty
}



OSStatus InputProc(void *inRefCon,
                   AudioUnitRenderActionFlags *ioActionFlags,
                   const AudioTimeStamp *inTimeStamp,
                   UInt32 inBusNumber,
                   UInt32 inNumberFrames,
                   AudioBufferList * ioData)  {
    OSStatus err = noErr;
    AudioBufferList abl;
    AudioBuffer ab[2],buff;
    
    
    
    abl.mNumberBuffers = 1;
    abl.mBuffers[0] = ab[0];
    abl.mBuffers[0].mNumberChannels = 1;
    abl.mBuffers[0].mDataByteSize = 2048;
    abl.mBuffers[0].mData = ring_buff.data[ring_buff.hi];
    ring_buff.hi = (ring_buff.hi+1)%NUM_RING_BUFF;
    
    
    //Get the new audio data
    err = AudioUnitRender(mInputUnit,
                          ioActionFlags,
                          inTimeStamp,
                          inBusNumber,
                          inNumberFrames, //# of frames requested
                          // mInputBuffer
                          &abl);// Audio Buffer List to hold data
    if (err) { printf("error in InputProc\n"); exit(0); }
    return err;
}

OSStatus liveInputProc(void *inRefCon,
                       AudioUnitRenderActionFlags *ioActionFlags,
                       const AudioTimeStamp *inTimeStamp,
                       UInt32 inBusNumber,
                       UInt32 inNumberFrames,
                       AudioBufferList * ioData)  {
    OSStatus err = noErr;
    AudioBufferList abl;
    AudioBuffer ab,buff;
    static unsigned char *base,*upbase,temp[AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE],down[AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE];
    int result;
    UInt64 nanoDiff;
    int skew_set;
    float t1,t2;
    
    t1 = now();
    
    //    if (output_callbacks_done == 0) return; // must wait until output callbacks start to synch input and output audio streams
    //    printf("input callback time = %f\n",inTimeStamp->mSampleTime);
    abl.mNumberBuffers = 1;
    abl.mBuffers[0] = ab;
    abl.mBuffers[0].mNumberChannels = 1;
    abl.mBuffers[0].mDataByteSize = AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE;
    abl.mBuffers[0].mData = temp;
    
    
    
    //Get the new audio data
    err = AudioUnitRender(mInputUnit,
                          ioActionFlags,
                          inTimeStamp,
                          inBusNumber,
                          inNumberFrames, //# of frames requested
                          // mInputBuffer
                          &abl);// Audio Buffer List to hold data
    if (err) { printf("error in InputProc\n"); return(err); }
    
    if (input_callbacks_done == 0) {
        input_zero = inTimeStamp->mHostTime;
        if (output_callbacks_done > 0) set_audio_skew();
    }
    skew_set = (output_callbacks_done > 0 && input_callbacks_done > 0);  // zero_input_frame is set
    if (mode != LIVE_MODE || (skew_set && input_callbacks_done >= zero_input_frame)) {
        base = (mode == LIVE_MODE) ? audiodata + lowres_samps_produced*BYTES_PER_SAMPLE : down;
        upbase = temp;
        filtered_downsample(upbase, base,AUDIO_SAMPS_PER_BUFFER,input_callbacks_done,&result); // this is to decrease aliasing
        lowres_samps_produced += result;
        if ((mode == LIVE_MODE) && is_hires)   queue_io_buff(&solo_out, upbase); // delay disk writing to outside of callback
    }
    input_callbacks_done++;
    //      nanoDiff = AudioConvertHostTimeToNanos(cur_output_host_time - inTimeStamp->mHostTime);
    //      printf("input frames = %d output frames = %d diff in frames = %f\n",input_callbacks_done, output_callbacks_done,(OUTPUT_SR/1024.)*(nanoDiff/1.0E9));
    //          }
    t2 = now();
    if (t2-t2 > .0001) printf("too long in input %f\n",t2-t1);
    return(0);
}


OSStatus liveInputProc_working(void *inRefCon,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList * ioData)  {
    OSStatus err = noErr;
    AudioBufferList abl;
    AudioBuffer ab,buff;
    static unsigned char *base,*upbase,temp[1024*BYTES_PER_SAMPLE],down[1024*BYTES_PER_SAMPLE];
    int result;
    UInt64 nanoDiff;
    
    //    if (output_callbacks_done == 0) return; // must wait until output callbacks start to synch input and output audio streams
    //    printf("input callback time = %f\n",inTimeStamp->mSampleTime);
    abl.mNumberBuffers = 1;
    abl.mBuffers[0] = ab;
    abl.mBuffers[0].mNumberChannels = 1;
    abl.mBuffers[0].mDataByteSize = 2048;  // buff size of 1024 2-byte samples of mono
    abl.mBuffers[0].mData = temp;
    
    
    
    //Get the new audio data
    err = AudioUnitRender(mInputUnit,
                          ioActionFlags,
                          inTimeStamp,
                          inBusNumber,
                          inNumberFrames, //# of frames requested
                          // mInputBuffer
                          &abl);// Audio Buffer List to hold data
    if (err) { printf("error in InputProc\n"); return(err); }
    
    if (input_callbacks_done == 0) {
        input_zero = inTimeStamp->mHostTime;
        if (output_callbacks_done > 0) set_audio_skew();
    }
    //    if (mode == LIVE_MODE) {
    //          if (input_callbacks_done >= 2) {
    base = (mode == LIVE_MODE) ? audiodata + lowres_samps_produced*BYTES_PER_SAMPLE : down;
    upbase = temp;
    filtered_downsample(upbase, base,AUDIO_SAMPS_PER_BUFFER,input_callbacks_done,&result); // this is to decrease aliasing
    lowres_samps_produced += result;
    if ((mode == LIVE_MODE) && is_hires)   queue_io_buff(&solo_out, upbase); // delay disk writing to outside of callback
    //	  }
    input_callbacks_done++;
    //   nanoDiff = AudioConvertHostTimeToNanos(cur_output_host_time - inTimeStamp->mHostTime);
    /* commented out above for apple complie */
    //      printf("input frames = %d output frames = %d diff in frames = %f\n",input_callbacks_done, output_callbacks_done,(OUTPUT_SR/1024.)*(nanoDiff/1.0E9));
    //          }
    return err;
}


static int
CallbackSetup(OSStatus (*input_callback)()) {
    OSStatus err = noErr;
    AURenderCallbackStruct input;
    
    input.inputProc = input_callback; //InputProc;
    input.inputProcRefCon = NULL;
    
    //Setup the input callback.
    err = AudioUnitSetProperty(mInputUnit,
                               kAudioOutputUnitProperty_SetInputCallback,
                               kAudioUnitScope_Global,
                               0,
                               &input,
                               sizeof(input));
    if (err) { printf("failed in CallbackSetup\n"); return(0); }
    return(1);
}



OSStatus SetupAUHAL(AudioDeviceID in, OSStatus (*input_callback)())
{
    OSStatus err = noErr;
    
    Component comp;
    ComponentDescription desc;
    
    //There are several different types of Audio Units.
    //Some audio units serve as Outputs, Mixers, or DSP
    //units. See AUComponent.h for listing
    desc.componentType = kAudioUnitType_Output;
    
    //Every Component has a subType, which will give a clearer picture
    //of what this components function will be.
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    
    //all Audio Units in AUComponent.h must use
    //"kAudioUnitManufacturer_Apple" as the Manufacturer
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    //Finds a component that meets the desc spec's
    comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) {printf("failed in FindNextComponent\n"); return(0);}
    
    //gains access to the services provided by the component
    OpenAComponent(comp, &mInputUnit);
    
    //AUHAL needs to be initialized before anything is done to it
    err = AudioUnitInitialize(mInputUnit);
    if (err) { printf("failed in AudioUnitInitialize\n"); return(0); }
    
    if (EnableIO() == 0)  { printf("failed in EnableIO\n"); return(0); }
    
    
    if (SetInputDeviceAsCurrent(inputDevice) == 0) { printf("failed in SetInputDeviceAsCurrent\n"); return(0); }
    
    
    if (CallbackSetup(input_callback) == 0) { printf("failed in CallbackSetup\n"); return(0); }
    
    
    
    
    
    //Don't setup buffers until you know what the
    //input and output device audio streams look like.
    
    err = AudioUnitInitialize(mInputUnit);
    if (err) { printf("failed in AudioUnitInitialize\n"); return(0); }
    
    UInt32 startAtZero = 0;
    err = AudioUnitSetProperty(mInputUnit,
                               kAudioOutputUnitProperty_StartTimestampsAtZero,
                               kAudioUnitScope_Global,
                               0,
                               &startAtZero,
                               sizeof(startAtZero));
    if (err) { printf("couldn't start timestamps at 0\n"); return(0); }
    
    
    
    
    // above is here twice --- is this really needed?
    
    return(1);
}


static int
MakeGraph() {
    OSStatus err = noErr;
    AudioComponentDescription varispeedDesc,outDesc;
    
    outDesc.componentType = kAudioUnitType_Output;
    outDesc.componentSubType = kAudioUnitSubType_DefaultOutput;
    outDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
    outDesc.componentFlags = 0;
    outDesc.componentFlagsMask = 0;
    
    //////////////////////////
    ///MAKE NODES
    //This creates a node in the graph that is an AudioUnit, using
    //the supplied ComponentDescription to find and open that unit
    
    err = AUGraphAddNode(mGraph, &outDesc, &mOutputNode);
    if (err) { printf("couldn't add node to audio graph\n"); return(0); }
    
    //Get Audio Units from AUGraph node
    err = AUGraphNodeInfo(mGraph, mOutputNode, NULL, &mOutputUnit);
    if (err) { printf("couldn't get audio graph info\n"); return(0); }
    
    // don't connect nodes until the varispeed unit has input and output formats set
    
    return(1);
}

OSStatus OutputProc(void *inRefCon,
                    AudioUnitRenderActionFlags *ioActionFlags,
                    const AudioTimeStamp *TimeStamp,
                    UInt32 inBusNumber,
                    UInt32 inNumberFrames,
                    AudioBufferList * ioData)
{
    OSStatus err = noErr;
    Float64 rate = 0.0;
    AudioTimeStamp inTS, outTS;
    Byte buff[4096],*fptr;
    
    
    if (ring_buff.lo == ring_buff.hi) return(0);
    //memcpy(ioData->mBuffers[0].mData,ring_buff.data[ring_buff.lo],4096);
    fptr = ring_buff.data[ring_buff.lo];
    interleave_channels(fptr, fptr, ioData->mBuffers[0].mData, 1024) ;
    ioData->mBuffers[0].mDataByteSize = 4096;
    ioData->mBuffers[0].mNumberChannels = 2;
    ring_buff.lo = (ring_buff.lo+1)%NUM_RING_BUFF;
    
}


float
input_lag_secs() {  // how much does the 0th actual time of the input sample lag behind the time of the 0th output sample ---- could be negative, of course.
    int diff;
    float secs;
    
    diff = input_zero-output_zero;
    secs = diff/1.0E9;
    return(secs);
    
}

int
set_audio_skew() {
    int diff,frame_diff;
    float secs;
    static int visit;
    
    
    diff = output_zero-input_zero;
    secs = diff/1.0E9;
    frame_diff = (int) ((secs*OUTPUT_SR/AUDIO_SAMPS_PER_BUFFER) + .5);
    frame_diff = (int) round(secs*OUTPUT_SR/AUDIO_SAMPS_PER_BUFFER);
    if (frame_diff > 0) {  // input starts before output
        zero_output_frame = output_callbacks_done+1; // the next one
        zero_input_frame = zero_output_frame+frame_diff;
    }
    else {
        zero_input_frame = input_callbacks_done+1; // the next frame
        zero_output_frame = zero_input_frame-frame_diff;
    }
    //  printf("secs = %f input frames = %d output = frames %d diff = %f frame_diff = %d zero_input = %d zero output = %d\n",secs,input_callbacks_done,output_callbacks_done,frame_diff,zero_input_frame,zero_output_frame);
    //	 printf("secs1 = %f secs2 = %f input = %d output = %d diff = %f frame_diff = %d\n",secs,diff/1.0E9,input_callbacks_done,output_callbacks_done,secs*OUTPUT_SR/AUDIO_SAMPS_PER_BUFFER,frame_diff);  secs = (2*1024)/(float)OUTPUT_SR;
    
    //  audio_skew = (int) (secs*OUTPUT_SR); // audio_skew in hi res sample --- don't do this since now adjusting by discarding start of audio input and/or output to syncrhonize them.
    visit++;
    //  printf("audio skew = %d\n",audio_skew);  // don't want to print inside callback
}


OSStatus liveOutputProc(void *inRefCon,
                        AudioUnitRenderActionFlags *ioActionFlags,
                        const AudioTimeStamp *TimeStamp,
                        UInt32 BusNumber,
                        UInt32 NumberFrames,
                        AudioBufferList * ioData)
{
    OSStatus err = noErr;
    Float64 rate = 0.0;
    AudioTimeStamp inTS, outTS;
    Byte buff[4096],*fptr;
    int skew_set;
    float t1,t2;
    
    t1 = now();
    //    if (input_callbacks_done == 0) return;  // wait until input starts to sync audio streams
    if (output_callbacks_done == 0) {
        output_zero = TimeStamp->mHostTime;
        if (input_callbacks_done > 0) set_audio_skew();
    }
    skew_set = (output_callbacks_done > 0 && input_callbacks_done > 0);
    if (mode != LIVE_MODE || (skew_set && output_callbacks_done >= zero_output_frame)) {
        global_audio_buff = ioData->mBuffers[0].mData;
        vcode_action();
        // printf("after vcode_action %f\n",sample2float(outputBuffer));
        played_samples += NumberFrames;
    }
    else bzero(ioData->mBuffers[0].mData,2*AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE);
    //    printf("output callback time = %f\n",TimeStamp->mSampleTime);
    output_callbacks_done++;
    t2 = now();
    if (t2 - t1 > .0015) printf("too long in output %f\n",t2-t1);
    return(0);
}



OSStatus liveDuplexProc(void *inRefCon,
                        AudioUnitRenderActionFlags *ioActionFlags,
                        const AudioTimeStamp *TimeStamp,
                        UInt32 BusNumber,
                        UInt32 NumberFrames,
                        AudioBufferList * ioData)
{
    OSStatus err = noErr;
    AudioBufferList abl,*ablptr;
    AudioBuffer ab;
    AudioTimeStamp inTS, outTS;
    //    Byte buff[4096],*fptr;
    int skew_set,i;
    static unsigned char *base,*upbase,temp[AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE],down[AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE];
    unsigned char x[2][2048];
    
    
    
    //    if (output_callbacks_done == 0) return; // must wait until output callbacks start to synch input and output audio streams
    //    printf("input callback time = %f\n",inTimeStamp->mSampleTime);
    abl.mNumberBuffers = 1;
    //    abl.mBuffers[0] = ab;
    abl.mBuffers[0].mNumberChannels = 1;
    abl.mBuffers[0].mDataByteSize = AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE;
    abl.mBuffers[0].mData = temp;
    
    
    //Get the new audio data
    err = AudioUnitRender(gOutputUnit, ioActionFlags, TimeStamp, kInputBus,  NumberFrames,  &abl);// Audio Buffer List to hold data
    if (err) { printf("error in InputProc\n"); return(err); }
    
    
    
    
    printf("callback at %f\n",now());
    return(0);
    
    //    if (input_callbacks_done == 0) return;  // wait until input starts to sync audio streams
    if (output_callbacks_done == 0) {
        output_zero = TimeStamp->mHostTime;
        if (input_callbacks_done > 0) set_audio_skew();
    }
    skew_set = (output_callbacks_done > 0 && input_callbacks_done > 0);
    if (mode != LIVE_MODE || (skew_set && output_callbacks_done >= zero_output_frame)) {
        global_audio_buff = ioData->mBuffers[0].mData;
        vcode_action();
        // printf("after vcode_action %f\n",sample2float(outputBuffer));
        played_samples += NumberFrames;
    }
    else bzero(ioData->mBuffers[0].mData,2*AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE);
    //    printf("output callback time = %f\n",TimeStamp->mSampleTime);
    output_callbacks_done++;
    return(0);
}



OSStatus liveOutputProc_working(void *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *TimeStamp,
                                UInt32 BusNumber,
                                UInt32 NumberFrames,
                                AudioBufferList * ioData)
{
    OSStatus err = noErr;
    Float64 rate = 0.0;
    AudioTimeStamp inTS, outTS;
    Byte buff[4096],*fptr;
    int skew_set;
    
    
    
    //    if (input_callbacks_done == 0) return;  // wait until input starts to sync audio streams
    if (output_callbacks_done == 0) {
        output_zero = TimeStamp->mHostTime;
        if (input_callbacks_done > 0) set_audio_skew();
    }
    
    //    printf("output callback time = %f\n",TimeStamp->mSampleTime);
    global_audio_buff = ioData->mBuffers[0].mData;
    vcode_action();
    // printf("after vcode_action %f\n",sample2float(outputBuffer));
    played_samples += NumberFrames;
    output_callbacks_done++;
    cur_output_host_time = TimeStamp->mHostTime;
    return(0);
}



static int
SetOutputDeviceAsCurrent(AudioDeviceID out) {
    UInt32 size = sizeof(AudioDeviceID);
    OSStatus err = noErr;
    
    if(out == kAudioDeviceUnknown) //Retrieve the default output device
    {
        AudioObjectPropertyAddress aopa;
        aopa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        aopa.mScope = kAudioObjectPropertyScopeGlobal;
        aopa.mElement = kAudioObjectPropertyElementMaster;
        
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &size, &out);
    }
    if (AudioDeviceInit(out, false) == 0) { printf("failed in AudioDeviceInit for output\n"); return(0); }
    
    //Set the Current Device to the Default Output Unit.
    err = AudioUnitSetProperty(mOutputUnit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global,
                               0,
                               &out,
                               sizeof(out));
    if (err) { printf("failed setting output device as default\n"); return(0); }
    
    return(1);
}



static int
SetupGraph(AudioDeviceID out, OSStatus (*output_callback)()) {
    OSStatus err = noErr;
    AURenderCallbackStruct output;
    
    //Make a New Graph
    err = NewAUGraph(&mGraph);
    if (err) { printf("couldn't create new audio graph\n"); return(0); }
    
    //Open the Graph, AudioUnits are opened but not initialized
    err = AUGraphOpen(mGraph);
    if (err) { printf("couldn't open audio graph\n"); return(0); }
    
    if (MakeGraph() == 0) { printf("couldn't MakeGraph for audio graph\n"); return(0); }
    
    if (SetOutputDeviceAsCurrent(out) == 0) { printf("failed in SetOutputDeviceAsCurrent\n"); return(0); }
    
    //Tell the output unit not to reset timestamps
    //Otherwise sample rate changes will cause sync los
    UInt32 startAtZero = 0;
    err = AudioUnitSetProperty(mOutputUnit,
                               kAudioOutputUnitProperty_StartTimestampsAtZero,
                               kAudioUnitScope_Global,
                               0,
                               &startAtZero,
                               sizeof(startAtZero));
    if (err) { printf("couldn't start timestamps at 0\n"); return(0); }
    
    output.inputProc = output_callback; //OutputProc;
    output.inputProcRefCon = NULL;
    
    err = AudioUnitSetProperty(mOutputUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0,
                               &output,
                               sizeof(output));
    
    
    if (err) { printf("coulnd't set output callback\n"); return(0);	}
    
    return(1);
}


//#define CAPT_DEBUG(msg, args...) printf( msg, ##args )
#define CAPT_DEBUG(msg, args...)



static int
setup_audio_unit_buffer_size_in_samples() {
    UInt32 param=0;
    OSStatus err = noErr;
    UInt32 fAudioSamples = 1024,check;
    
    
    
    // Set the input buffer size 1024 -  IO buffer size is also a direct factor in the amount of latency in the system !!
    
    
    err = AudioUnitSetProperty( mInputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &fAudioSamples,  sizeof(UInt32) );
    if (err) { printf("failed setting input buffer size\n"); return(0);}
    
    // check the setting worked
    param = sizeof(UInt32);
    err = AudioUnitGetProperty( mInputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &check, &param);
    if (err || (check != fAudioSamples)) { printf("failed getting input buffer size or unable to set buffer size\n"); return(0);}
    
    err = AudioUnitSetProperty( mOutputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &fAudioSamples,  sizeof(UInt32));
    if (err) { printf("failed setting output buffer size\n"); return(0);}
    
    
    // Get the number of frames in the IO buffer(s)
    param = sizeof(UInt32);
    err = AudioUnitGetProperty( mOutputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &check, &param);
    if (err || (check != fAudioSamples)) { printf("failed getting output buffer size or unable to set buffer size\n"); return(0);}
    
    return(1);
    
}


static int
set_safety_margin() {
    UInt32 propsize,safety,check;
    AudioObjectPropertyAddress aopa;
    OSStatus err;
    
    safety = 512;
    propsize = sizeof(UInt32);
    aopa.mSelector = kAudioDevicePropertySafetyOffset;
    aopa.mScope = kAudioObjectPropertyScopeGlobal;
    aopa.mElement = kAudioObjectPropertyElementMaster;
    
    
    /*  err = AudioDeviceSetProperty(outputDevice, NULL, 0, 0, kAudioDevicePropertySafetyOffset, propsize, &safety);
     
     err = AudioDeviceGetProperty(inputDevice, 0, true, kAudioDevicePropertySafetyOffset, &propsize, &check);
     
     */
    
    
    
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &propsize, &safety );
    
    err = AudioObjectSetPropertyData(inputDevice, &aopa, 0, NULL, propsize, &safety );
    if (err) { printf("failed to set input safety margin\n"); return(0); }
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &propsize, &check );
    if (err) { printf("failed to get input safety margin\n"); return(0); }
    if (check != safety) { printf("safety not set to desired value\n"); return(0); }
    return(1);
}

static int
set_stream_parms() {
    AudioStreamBasicDescription streamFormat;
    OSStatus err;
    
    streamFormat.mSampleRate = OUTPUT_SR;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags =  kLinearPCMFormatFlagIsSignedInteger |kLinearPCMFormatFlagIsPacked;
    streamFormat.mChannelsPerFrame = 1;
    streamFormat.mBitsPerChannel = 16;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame  =   streamFormat.mChannelsPerFrame * sizeof (SInt16);
    streamFormat.mFramesPerPacket = 1;
    
    
    err = AudioUnitSetProperty (mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &streamFormat, sizeof(AudioStreamBasicDescription));
    
    if (err) { printf ("couldn't set input stream parms\n", err); return(0); }
    
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame  =   streamFormat.mChannelsPerFrame * sizeof (SInt16);
    
    
    err = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) { printf ("couldn't set output stream parms\n", err); return(0); }
    return(1);
}


static int
set_audio_parms() {
    if (setup_audio_unit_buffer_size_in_samples() == 0) return(0);
    if (set_stream_parms() == 0) return(0);
    return(1);
}


//Allocate Audio Buffer List(s) to hold the data from input.
static int SetupBuffers() {
    OSStatus err = noErr;
    UInt32 bufferSizeFrames,bufferSizeBytes,propsize,param;
    AudioObjectPropertyAddress aopa;
    //CAStreamBasicDescription asbd,asbd_dev1_in,asbd_dev2_out;
    AudioStreamBasicDescription asbd,asbd_dev1_in,asbd_dev2_out;
    Float64 rate=0;
    
    if (setup_audio_unit_buffer_size_in_samples() == 0) return(0);
    if (set_stream_parms() == 0) return(0);
    
    return(1);
    
    
    /*	//Get the size of the IO buffer(s)
     UInt32 propertySize = sizeof(bufferSizeFrames);
     err = AudioUnitGetProperty(mInputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &bufferSizeFrames, &propertySize);
     bufferSizeBytes = bufferSizeFrames * sizeof(Float32);
     CAPT_DEBUG( "Input device buffer size is %ld frames.\n", bufferSizeFrames );
     
     UInt32 outBufferSizeFrames;
     propertySize = sizeof(outBufferSizeFrames);
     err = AudioUnitGetProperty(mOutputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &outBufferSizeFrames, &propertySize);
     CAPT_DEBUG( "Output device buffer size is %ld frames.\n", outBufferSizeFrames );
     
     
     propertySize = sizeof(asbd);
     err = AudioUnitGetProperty(mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &asbd, &propertySize);
     if (err) return(0);
     
     //calculate number of buffers from channels
     propsize = offsetof(AudioBufferList, mBuffers[0]) + (sizeof(AudioBuffer) *asbd.mChannelsPerFrame);
     
     //malloc buffer lists
     mInputBuffer = (AudioBufferList *)malloc(propsize);
     mInputBuffer->mNumberBuffers = asbd.mChannelsPerFrame;
     
     //pre-malloc buffers for AudioBufferLists
     for(UInt32 i =0; i< mInputBuffer->mNumberBuffers ; i++) {
     mInputBuffer->mBuffers[i].mNumberChannels = 1;
     mInputBuffer->mBuffers[i].mDataByteSize = bufferSizeBytes;
     mInputBuffer->mBuffers[i].mData = malloc(bufferSizeBytes);
     }*/
    
    // Some test code to run the ring through its paces...
    //
    //    CARingBuffer::SampleTime readTime = 0;
    //    CARingBuffer::SampleTime writeTime = 0;
    //    for ( int i = 0; i < 32768; i++ ) {
    //        printf( "run buf %d\n", i );
    //        UInt32 writeLength = 512;
    //        UInt32 readLength = 256;//(i%2==0) ? 440 : 441;
    //        OSStatus err = mBuffer->Store( mInputBuffer, writeLength, writeTime );
    //        if( err != kCARingBufferError_OK )
    //        {
    //            printf( "HIT ERR IN Store()\n" );
    //        }
    //        writeTime += writeLength;
    //
    //        err = mBuffer->Fetch( mInputBuffer, readLength, readTime );
    //        if(err != kCARingBufferError_OK)
    //        {
    //            printf( "HIT ERR IN Fetch()\n" );
    //            MakeBufferSilent (mInputBuffer);
    //            SInt64 bufferStartTime, bufferEndTime;
    //            mBuffer->GetTimeBounds(bufferStartTime, bufferEndTime);
    //        }
    //        readTime += readLength;
    //    }
    
    return(1);
}

OSStatus	DeviceListenerProc (	AudioDeviceID           inDevice,
                                UInt32                  inChannel,
                                Boolean                 isInput,
                                AudioDevicePropertyID   inPropertyID,
                                void*                   inClientData)
{
    if (inPropertyID == kAudioDeviceProcessorOverload) {
        performance_interrupted = LIVE_ERROR;
        strcpy(live_failure_string,"audio underflow error");
        printf("audio underflow\n");
    }
    
}

int
start_coreaudio_duplex_parms(OSStatus (*input_callback)(),(*output_callback)()) {
    int err;
    
    if (get_audio_devices() == 0) return(0);
    if (set_sample_rate(OUTPUT_SR) == 0) return(0);
    if (SetupAUHAL(inputDevice,input_callback) == 0) return(0);
    if (SetupGraph(outputDevice,output_callback) == 0) return(0);
    if (set_audio_parms() == 0) return(0);
    
    
    
    err = AudioDeviceAddPropertyListener(outputDevice, 0, true, kAudioDeviceProcessorOverload, DeviceListenerProc, nil);
    err = AUGraphInitialize(mGraph);
    if (err) return(0);
    err = AudioOutputUnitStart(mInputUnit);
    if (err) return(0);
    
    err = AUGraphStart(mGraph);
    if (err) return(0);
    return(1);
}

int
duplex_audio_test() {
    start_coreaudio_duplex_parms(InputProc,OutputProc);
}

int
start_coreaudio_duplex() {
    done_recording = played_samples = input_callbacks_done = output_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) {
        if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    }
    
    return(start_coreaudio_duplex_parms(liveInputProc,liveOutputProc));
}

int
end_duplex_audio() {
    int err;
    
    err = AudioOutputUnitStop(mInputUnit);
    if (err) { printf("couldn't do AudioOutputUnitStop()\n"); return(0); }
    err = AUGraphStop(mGraph);
    if (err) { printf("couldn't do AUGraphStop()\n"); return(0); }
    return(1);
}


int
end_playing() {
    int err;
    
    err = AudioOutputUnitStop(gOutputUnit);
    if (err) { printf("couldn't do AudioOutputUnitStop()\n"); return(0); }
}

static int
prepare_audio_output_unit() {
    Component comp;
    ComponentDescription desc;
    OSStatus err;
    //  ComponentInstance audioOutputUnit;
    
    //There are several different types of Audio Units.
    //Some audio units serve as Outputs, Mixers, or DSP
    //units. See AUComponent.h for listing
    desc.componentType = kAudioUnitType_Output;
    
    //Every Component has a subType, which will give a clearer picture
    //of what this components function will be.
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    //  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    
    //all Audio Units in AUComponent.h must use
    //"kAudioUnitManufacturer_Apple" as the Manufacturer
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    //Finds a component that meets the desc spec's
    comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) { printf("failed in FindNextComponent\n"); return(0); }
    
    //gains access to the services provided by the component
    OpenAComponent(comp, &gOutputUnit);
    
    
    UInt32 enableIO;
    UInt32 size=0;
    
    //When using AudioUnitSetProperty the 4th parameter in the method
    //refer to an AudioUnitElement. When using an AudioOutputUnit
    //the input element will be '1' and the output element will be '0'.
    
    
    enableIO = 0;  // disable input
    err = AudioUnitSetProperty(gOutputUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Input,
                               1, // input element
                               &enableIO,
                               sizeof(enableIO));
    
    if (err) { printf("couldn't disable audio inut\n"); return(0); }
    
    enableIO = 1; // enable output
    err = AudioUnitSetProperty(gOutputUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output,
                               0,   //output element
                               &enableIO,
                               sizeof(enableIO));
    
    if (err) { printf("couldn't enable audio ouptut\n"); return(0); }
    
    
    
    return(1);
}


static int
prepare_audio_unit_for_duplex() {
    Component comp;
    ComponentDescription desc;
    OSStatus err;
    //  ComponentInstance audioOutputUnit;
    
    //There are several different types of Audio Units.
    //Some audio units serve as Outputs, Mixers, or DSP
    //units. See AUComponent.h for listing
    desc.componentType = kAudioUnitType_Output;
    
    //Every Component has a subType, which will give a clearer picture
    //of what this components function will be.
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    //  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    
    //all Audio Units in AUComponent.h must use
    //"kAudioUnitManufacturer_Apple" as the Manufacturer
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    //Finds a component that meets the desc spec's
    comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) { printf("failed in FindNextComponent\n"); return(0); }
    
    //gains access to the services provided by the component
    OpenAComponent(comp, &gOutputUnit);
    
    err = AudioUnitInitialize(gOutputUnit);
    
    UInt32 enableIO;
    UInt32 size=0;
    
    //When using AudioUnitSetProperty the 4th parameter in the method
    //refer to an AudioUnitElement. When using an AudioOutputUnit
    //the input element will be '1' and the output element will be '0'.
    
    enableIO = 1; // enable output
    err = AudioUnitSetProperty(gOutputUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Output,
                               0,   //output element
                               &enableIO,
                               sizeof(enableIO));
    
    if (err) { printf("couldn't enable audio ouptut\n"); return(0); }
    
    
    enableIO = 1;  // enable input
    err = AudioUnitSetProperty(gOutputUnit,
                               kAudioOutputUnitProperty_EnableIO,
                               kAudioUnitScope_Input,
                               1, // input element
                               &enableIO,
                               sizeof(enableIO));
    
    if (err) { printf("couldn't enable audio inut\n"); return(0); }
    
    
    
    return(1);
}


static int
SetDefaultOutputDeviceAsCurrent() {
    UInt32 size;
    OSStatus err =noErr;
    size = sizeof(AudioDeviceID);
    
    //  AudioDeviceID inputDevice;
    /*  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
     &size,
     &outputDevice);
     
     if (err) { printf("couldn't set the default output device\n"); return(0); }*/
    
    err =AudioUnitSetProperty(gOutputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &outputDevice, sizeof(outputDevice));
    
    
    if (err) { printf("couldn't associate output unit with current device\n"); return(0); }
    return(1);
}



static int
SetDefaultOutputDevicesAsCurrent() {
    UInt32 size;
    OSStatus err =noErr;
    size = sizeof(AudioDeviceID);
    
    //  AudioDeviceID inputDevice;
    /*  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
     &size,
     &outputDevice);
     
     if (err) { printf("couldn't set the default output device\n"); return(0); }*/
    
    
    //   AudioDeviceInit(inputDevice,true);
    
    
    err =AudioUnitSetProperty(gOutputUnit,
                              kAudioOutputUnitProperty_CurrentDevice,
                              kAudioUnitScope_Global,
                              0,
                              &inputDevice,
                              sizeof(inputDevice));
    
    if (err) { printf("couldn't associate input device with audio unit\n"); return(0); }
    
    err =AudioUnitSetProperty(gOutputUnit,
                              kAudioOutputUnitProperty_CurrentDevice,
                              kAudioUnitScope_Global,
                              0,
                              &outputDevice,
                              sizeof(outputDevice));
    
    if (err) { printf("couldn't associate output device with audio unit\n"); return(0); }
    
    return(1);
}



static int
set_output_stream_parms(float sr) {
    AudioStreamBasicDescription streamFormat;
    OSStatus err;
    
    streamFormat.mSampleRate = sr;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags =  kLinearPCMFormatFlagIsSignedInteger |kLinearPCMFormatFlagIsPacked;
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBitsPerChannel = 16;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame  =   streamFormat.mChannelsPerFrame * sizeof (SInt16);
    streamFormat.mFramesPerPacket = 1;
    
    
    err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) { printf ("couldn't set output stream parms\n", err); return(0); }
    return(1);
}

static int
set_duplex_stream_parms(float sr) {
    AudioStreamBasicDescription streamFormat;
    OSStatus err;
    
    streamFormat.mSampleRate = sr;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags =  kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBitsPerChannel = 16;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame  =   streamFormat.mChannelsPerFrame * sizeof (SInt16);
    streamFormat.mFramesPerPacket = 1;
    
    
    err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, kOutputBus, &streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) { printf ("couldn't set output stream parms\n", err); return(0); }
    
    streamFormat.mChannelsPerFrame = 1;
    
    err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,  kInputBus, &streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) { printf ("couldn't set input stream parms\n", err); return(0); }
    
    
    
    return(1);
}

static int
set_various_parms() {
    AudioStreamBasicDescription streamFormat;
    OSStatus err;
    UInt32 bufferSize,outSize;
    Boolean isWritable;
    int in_nChannels,out_nChannels;
    
    outSize = sizeof(UInt32);
    
    err = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, kInputBus, (UInt32*)&bufferSize, sizeof(UInt32));
    if (err) { printf("failed setting MaximumFramesPerSlice\n"); return(0); }
    
    err = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, kOutputBus, (UInt32*)&bufferSize, sizeof(UInt32));
    if (err) { printf("failed setting MaximumFramesPerSlice\n"); return(0); }
    
    err = AudioUnitGetPropertyInfo(gOutputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, kInputBus, &outSize, &isWritable);
    if (err) { printf("failed getting ChannelMap\n"); return(0); }
    in_nChannels = (err == noErr) ? outSize / sizeof(SInt32) : 0;
    printf("in_nChannels = %ld\n", in_nChannels);
    
    err = AudioUnitGetPropertyInfo(gOutputUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output, kOutputBus, &outSize, &isWritable);
    if (err) { printf("failed getting ChannelMap\n"); return(0); }
    out_nChannels = (err == noErr) ? outSize / sizeof(SInt32) : 0;
    printf("out_nChannels = %ld\n", out_nChannels);
    
    
    return(1);
}

static int
setup_device_audio_buffer_size_in_samples() {
    UInt32 param=0,outSize,propSize;
    OSStatus err = noErr;
    UInt32 fAudioSamples = 1024,check;
    
    
    
    // Set the input buffer size 1024 -  IO buffer size is also a direct factor in the amount of latency in the system !!
    
    
    outSize = sizeof(UInt32);
    err = AudioDeviceSetProperty(outputDevice, NULL, 0, false, kAudioDevicePropertyBufferFrameSize, outSize, &fAudioSamples);
    if (err) { printf("failed setting output buffer size\n"); return(0);}
    err = AudioDeviceGetProperty(outputDevice, 0, true, kAudioDevicePropertyBufferFrameSize, &propSize, &check);
    if (err || (check != fAudioSamples)) { printf("failed getting output buffer size or unable to set buffer size\n"); return(0);}
    
    return(1);
    
}


static int
setup_output_audio_buffer_size_in_samples() {
    UInt32 param=0,outSize,propSize;
    OSStatus err = noErr;
    UInt32 fAudioSamples = 1024,check;
    
    
    
    // Set the input buffer size 1024 -  IO buffer size is also a direct factor in the amount of latency in the system !!
    
    
    
    err = AudioUnitSetProperty( gOutputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &fAudioSamples,  sizeof(UInt32));
    if (err) { printf("failed setting output buffer size\n"); return(0);}
    
    // Get the number of frames in the IO buffer(s)
    param = sizeof(UInt32);
    err = AudioUnitGetProperty( gOutputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &check, &param);
    if (err || (check != fAudioSamples)) { printf("failed getting output buffer size or unable to set buffer size\n"); return(0);}
    
    return(1);
    
}


static int
setup_playback_callback(OSStatus (*callback)()) {
    AURenderCallbackStruct output;
    output.inputProc = callback; //playbackCallback;
    output.inputProcRefCon = 0;
    OSStatus err;
    
    err = AudioUnitSetProperty(gOutputUnit,
                               // kAudioOutputUnitProperty_SetInputCallback,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Global,
                               kOutputBus,
                               &output,
                               sizeof(output));
    
    if (err) { printf("failed to set output callback\n"); return(0); }
    return(1);
}

static int
setup_duplex_callback() {
    AURenderCallbackStruct output,input;
    OSStatus err;
    output.inputProc = liveDuplexProc;
    output.inputProcRefCon = 0;
    
    //  err = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, kOutputBus, &output, sizeof(output));
    
    err = AudioUnitSetProperty(gOutputUnit,      kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &output, sizeof(output));
    
    if (err) { printf("failed to set output callback\n"); return(0); }
    
    
    if (err) { printf("failed to set input callback\n"); return(0); }
    
    
    
    
    return(1);
}

static int
start_audio_output_callbacks() {
    OSStatus err;
    
    err = AudioUnitInitialize(gOutputUnit);
    if (err) {printf("couldn't initialize audio output unit\n"); return(0); }
    err = AudioOutputUnitStart(gOutputUnit);
    if (err) { printf("couldn't start audio output unit\n"); return(0); }
    return(1);
}

int
start_coreaudio_play() {
    AudioStreamBasicDescription streamFormat;
    OSStatus err;
    Float64 sr;
    OSStatus (*callback)();
    
    done_recording = played_samples = input_callbacks_done = output_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) {
        if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    }
    
    sr = (mode == INTRO_CLIP_MODE) ? 44100 : OUTPUT_SR;
    callback = (mode == INTRO_CLIP_MODE) ? introductionCallback : playbackCallback;
    
    
    if (get_audio_devices() == 0) return(0);  // set inputDevice and outputDevice
    if (set_sample_rate(sr) == 0) return(0);
    if (prepare_audio_output_unit() == 0) return(0);
    if (SetDefaultOutputDeviceAsCurrent() == 0) return(0);
    if (set_output_stream_parms(sr) == 0) return(0);
    if (setup_output_audio_buffer_size_in_samples() == 0) return(0);
    if (setup_playback_callback(callback) == 0) return(0);
    if (start_audio_output_callbacks() == 0) return(0);
    
    return(1);
    
}



int
start_coreaudio_simple_duplex() {
    AudioStreamBasicDescription streamFormat;
    OSStatus err;
    //    OSStatus (*callback)();
    
    done_recording = played_samples = input_callbacks_done = output_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) {
        if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    }
    
    if (get_audio_devices() == 0) return(0);  // set inputDevice and outputDevice
    if (set_sample_rate(OUTPUT_SR) == 0) return(0);  // both input and ouptut
    if (prepare_audio_unit_for_duplex() == 0) return(0); // enable both input and output
    
    
    //       if (SetDefaultOutputDevicesAsCurrent() == 0) return(0);
    if (SetDefaultOutputDeviceAsCurrent() == 0) return(0);  // should this be output or input or both?
    
    if (set_duplex_stream_parms(OUTPUT_SR) == 0) return(0);
    //    if (set_various_parms() == 0) return(0);
    if (setup_device_audio_buffer_size_in_samples() == 0) return(0);  // sets input and output buffer size, I think ...
    
    if (setup_duplex_callback() == 0) return(0);
    if (start_audio_output_callbacks() == 0) return(0);
    
    return(1);
    
}



unsigned int
str2code(char *s) {
    unsigned int i,t=0,d;
    
    for (i=0; i < strlen(s); i++) {
        d = s[i];
        d *= 137603921;
        t = t^d;
    }
    return(t^unique_id); // unique_id is different for each machine so the verify list keeps piece ids that are only meaningful for the machine.
}

#define VERIFY_NAME  "veritas.dat"

void
add_piece_to_verified(char *tag) {
    char verify[1000];
    FILE *fp;
    unsigned code;
    
    strcpy(verify,user_dir);
    strcat(verify,VERIFY_NAME);
    fp = fopen(verify,"a");
    code = str2code(tag);
    fprintf(fp,"%08X\n",code);
    fclose(fp);
}

int
already_verified(char *tag) {
    char verify[1000];
    FILE *fp;
    unsigned code,c;
    int ret=0;
    
    strcpy(verify,user_dir);
    strcat(verify,VERIFY_NAME);
    fp = fopen(verify,"r");
    if (fp == NULL) return(0);
    code = str2code(tag);
    while (!feof(fp)) {
        fscanf(fp,"%X\n",&c);
        if (c == code) {ret = 1; break; }
    }
    fclose(fp);
    return(ret);
}

/************************************************************************************************************/

static AudioObjectID fPluginID;    // Used for aggregate device
static bool fState;
static AudioDeviceID fDeviceID;
static AudioBufferList* fInputData;
static int fDevNumInChans;
static int fDevNumOutChans;

/*static float **fInChannel;
 static float **fOutChannel;*/

static unsigned char **fInChannel;
static unsigned char **fOutChannel;

static AudioUnit fAUHAL = 0;

#define OPEN_ERR -1
#define NO_ERR 0

typedef	UInt8	CAAudioHardwareDeviceSectionID;
#define	kAudioDeviceSectionInput	((CAAudioHardwareDeviceSectionID)0x01)
#define	kAudioDeviceSectionOutput	((CAAudioHardwareDeviceSectionID)0x00)
#define	kAudioDeviceSectionGlobal	((CAAudioHardwareDeviceSectionID)0x00)
#define	kAudioDeviceSectionWildcard	((CAAudioHardwareDeviceSectionID)0xFF)


#define MAX_AUDIO_DEVICE 10

typedef struct {
    int num;
    AudioDeviceID el[MAX_AUDIO_DEVICE];
} AudioDeviceIDArray;


#define MAX_STRING_REF 10

typedef struct {
    int num;
    CFStringRef el[MAX_STRING_REF];
} CFStringRefArray;

#define WAIT_COUNTER 60


static void
printError(OSStatus err) {
    switch (err) {
        case kAudioHardwareNoError:
            printf("error code : kAudioHardwareNoError\n");
            break;
        case kAudioConverterErr_FormatNotSupported:
            printf("error code : kAudioConverterErr_FormatNotSupported\n");
            break;
        case kAudioConverterErr_OperationNotSupported:
            printf("error code : kAudioConverterErr_OperationNotSupported\n");
            break;
        case kAudioConverterErr_PropertyNotSupported:
            printf("error code : kAudioConverterErr_PropertyNotSupported\n");
            break;
        case kAudioConverterErr_InvalidInputSize:
            printf("error code : kAudioConverterErr_InvalidInputSize\n");
            break;
        case kAudioConverterErr_InvalidOutputSize:
            printf("error code : kAudioConverterErr_InvalidOutputSize\n");
            break;
        case kAudioConverterErr_UnspecifiedError:
            printf("error code : kAudioConverterErr_UnspecifiedError\n");
            break;
        case kAudioConverterErr_BadPropertySizeError:
            printf("error code : kAudioConverterErr_BadPropertySizeError\n");
            break;
        case kAudioConverterErr_RequiresPacketDescriptionsError:
            printf("error code : kAudioConverterErr_RequiresPacketDescriptionsError\n");
            break;
        case kAudioConverterErr_InputSampleRateOutOfRange:
            printf("error code : kAudioConverterErr_InputSampleRateOutOfRange\n");
            break;
        case kAudioConverterErr_OutputSampleRateOutOfRange:
            printf("error code : kAudioConverterErr_OutputSampleRateOutOfRange\n");
            break;
        case kAudioHardwareNotRunningError:
            printf("error code : kAudioHardwareNotRunningError\n");
            break;
        case kAudioHardwareUnknownPropertyError:
            printf("error code : kAudioHardwareUnknownPropertyError\n");
            break;
        case kAudioHardwareIllegalOperationError:
            printf("error code : kAudioHardwareIllegalOperationError\n");
            break;
        case kAudioHardwareBadDeviceError:
            printf("error code : kAudioHardwareBadDeviceError\n");
            break;
        case kAudioHardwareBadStreamError:
            printf("error code : kAudioHardwareBadStreamError\n");
            break;
        case kAudioDeviceUnsupportedFormatError:
            printf("error code : kAudioDeviceUnsupportedFormatError\n");
            break;
        case kAudioDevicePermissionsError:
            printf("error code : kAudioDevicePermissionsError\n");
            break;
        default:
            printf("error code : unknown\n");
            break;
    }
}



int SetupSampleRateAux(AudioDeviceID inDevice, int samplerate)
{
    OSStatus err = noErr;
    UInt32 outSize;
    Float64 sampleRate;
    
    // Get sample rate
    outSize =  sizeof(Float64);
    err = AudioDeviceGetProperty(inDevice, 0, kAudioDeviceSectionGlobal, kAudioDevicePropertyNominalSampleRate, &outSize, &sampleRate);
    if (err != noErr) {
        printf("Cannot get current sample rate\n");
        printError(err);
        return -1;
    } else {
        printf("Current sample rate = %f\n", sampleRate);
    }
    
    // If needed, set new sample rate
    if (samplerate != (int)sampleRate) {
        sampleRate = (Float64)samplerate;
        
        // To get SR change notification
        /*   err = AudioDeviceAddPropertyListener(inDevice, 0, true, kAudioDevicePropertyNominalSampleRate, SRNotificationCallback, this);
         if (err != noErr) {
         printf("Error calling AudioDeviceAddPropertyListener with kAudioDevicePropertyNominalSampleRate\n");
         printError(err);
         return -1;
         }*/
        err = AudioDeviceSetProperty(inDevice, NULL, 0, kAudioDeviceSectionGlobal, kAudioDevicePropertyNominalSampleRate, outSize, &sampleRate);
        if (err != noErr) {
            printf("Cannot set sample rate = %d\n", samplerate);
            printError(err);
            return -1;
        }
        
        // Waiting for SR change notification
        int count = 0;
        while (!fState && count++ < WAIT_COUNTER) {
            usleep(100000);
            printf("Wait count = %d\n", count);
        }
        
        // Check new sample rate
        outSize =  sizeof(Float64);
        err = AudioDeviceGetProperty(inDevice, 0, kAudioDeviceSectionGlobal, kAudioDevicePropertyNominalSampleRate, &outSize, &sampleRate);
        if (err != noErr) {
            printf("Cannot get current sample rate\n");
            printError(err);
        } else {
            printf("Checked sample rate = %f\n", sampleRate);
        }
        
        // Remove SR change notification
        /*        AudioDeviceRemovePropertyListener(inDevice, 0, true, kAudioDevicePropertyNominalSampleRate, SRNotificationCallback);*/
    }
    
    return 0;
}

static CFStringRef
GetDeviceName(AudioDeviceID id)
{
    UInt32 size = sizeof(CFStringRef);
    CFStringRef UIname;
    OSStatus err = AudioDeviceGetProperty(id, 0, false, kAudioDevicePropertyDeviceUID, &size, &UIname);
    return (err == noErr) ? UIname : NULL;
}


OSStatus
GetDeviceNameFromID(AudioDeviceID id, char* name)
{
    UInt32 size = 256;
    return AudioDeviceGetProperty(id, 0, false, kAudioDevicePropertyDeviceName, &size, name);
}



OSStatus DestroyAggregateDevice() {
    OSStatus osErr = noErr;
    AudioObjectPropertyAddress pluginAOPA;
    pluginAOPA.mSelector = kAudioPlugInDestroyAggregateDevice;
    pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
    pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
    UInt32 outDataSize;
    
    if (fPluginID > 0)   {
        
        osErr = AudioObjectGetPropertyDataSize(fPluginID, &pluginAOPA, 0, NULL, &outDataSize);
        if (osErr != noErr) {
            printf("TCoreAudioRenderer::DestroyAggregateDevice : AudioObjectGetPropertyDataSize error\n");
            printError(osErr);
            return osErr;
        }
        
        osErr = AudioObjectGetPropertyData(fPluginID, &pluginAOPA, 0, NULL, &outDataSize, &fDeviceID);
        if (osErr != noErr) {
            printf("TCoreAudioRenderer::DestroyAggregateDevice : AudioObjectGetPropertyData error\n");
            printError(osErr);
            return osErr;
        }
        
    }
    
    return noErr;
}



OSStatus CreateAggregateDeviceAux(AudioDeviceIDArray captureDeviceID, AudioDeviceIDArray playbackDeviceID, int samplerate, AudioDeviceID* outAggregateDevice)
{    OSStatus osErr = noErr;
    UInt32 outSize;
    Boolean outWritable;
    
    bool fClockDriftCompensate = true;
    
    // Prepare sub-devices for clock drift compensation
    // Workaround for bug in the HAL : until 10.6.2
    AudioObjectPropertyAddress theAddressOwned = { kAudioObjectPropertyOwnedObjects, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    AudioObjectPropertyAddress theAddressDrift = { kAudioSubDevicePropertyDriftCompensation, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    UInt32 theQualifierDataSize = sizeof(AudioObjectID);
    AudioClassID inClass = kAudioSubDeviceClassID;
    void* theQualifierData = &inClass;
    UInt32 subDevicesNum = 0;
    CFStringRefArray captureDeviceUID, playbackDeviceUID;
    
    captureDeviceUID.num = playbackDeviceUID.num = 0;
    
    //---------------------------------------------------------------------------
    // Setup SR of both devices otherwise creating AD may fail...
    //---------------------------------------------------------------------------
    UInt32 keptclockdomain = 0;
    UInt32 clockdomain = 0;
    outSize = sizeof(UInt32);
    bool need_clock_drift_compensation = false;
    
    for (UInt32 i = 0; i < captureDeviceID.num; i++) {
        if (SetupSampleRateAux(captureDeviceID.el[i], samplerate) < 0) {
            printf("TCoreAudioRenderer::CreateAggregateDevice : cannot set SR of input device\n");
        } else  {
            // Check clock domain
            osErr = AudioDeviceGetProperty(captureDeviceID.el[i], 0, kAudioDeviceSectionGlobal, kAudioDevicePropertyClockDomain, &outSize, &clockdomain);
            if (osErr != 0) {
                printf("TCoreAudioRenderer::CreateAggregateDevice : kAudioDevicePropertyClockDomain error\n");
                printError(osErr);
            } else {
                keptclockdomain = (keptclockdomain == 0) ? clockdomain : keptclockdomain;
                printf("TCoreAudioRenderer::CreateAggregateDevice : input clockdomain = %d\n", clockdomain);
                if (clockdomain != 0 && clockdomain != keptclockdomain) {
                    printf("TCoreAudioRenderer::CreateAggregateDevice : devices do not share the same clock!! clock drift compensation would be needed...\n");
                    need_clock_drift_compensation = true;
                }
            }
        }
    }
    
    for (UInt32 i = 0; i < playbackDeviceID.num; i++) {
        if (SetupSampleRateAux(playbackDeviceID.el[i], samplerate) < 0) {
            printf("TCoreAudioRenderer::CreateAggregateDevice : cannot set SR of output device\n");
        } else {
            // Check clock domain
            osErr = AudioDeviceGetProperty(playbackDeviceID.el[i], 0, kAudioDeviceSectionGlobal, kAudioDevicePropertyClockDomain, &outSize, &clockdomain);
            if (osErr != 0) {
                printf("TCoreAudioRenderer::CreateAggregateDevice : kAudioDevicePropertyClockDomain error\n");
                printError(osErr);
            } else {
                keptclockdomain = (keptclockdomain == 0) ? clockdomain : keptclockdomain;
                printf("TCoreAudioRenderer::CreateAggregateDevice : output clockdomain = %d", clockdomain);
                if (clockdomain != 0 && clockdomain != keptclockdomain) {
                    printf("TCoreAudioRenderer::CreateAggregateDevice : devices do not share the same clock!! clock drift compensation would be needed...\n");
                    need_clock_drift_compensation = true;
                }
            }
        }
    }
    
    // If no valid clock domain was found, then assume we have to compensate...
    if (keptclockdomain == 0) {
        need_clock_drift_compensation = true;
    }
    
    //---------------------------------------------------------------------------
    // Start to create a new aggregate by getting the base audio hardware plugin
    //---------------------------------------------------------------------------
    
    char device_name[256];
    for (UInt32 i = 0; i < captureDeviceID.num; i++) {
        GetDeviceNameFromID(captureDeviceID.el[i], device_name);
        printf("Separated input = '%s' \n", device_name);
    }
    
    for (UInt32 i = 0; i < playbackDeviceID.num; i++) {
        GetDeviceNameFromID(playbackDeviceID.el[i], device_name);
        printf("Separated output = '%s' \n", device_name);
    }
    
    osErr = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyPlugInForBundleID, &outSize, &outWritable);
    if (osErr != noErr) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : AudioHardwareGetPropertyInfo kAudioHardwarePropertyPlugInForBundleID error\n");
        printError(osErr);
        return osErr;
    }
    
    AudioValueTranslation pluginAVT;
    
    CFStringRef inBundleRef = CFSTR("com.apple.audio.CoreAudio");
    
    pluginAVT.mInputData = &inBundleRef;
    pluginAVT.mInputDataSize = sizeof(inBundleRef);
    pluginAVT.mOutputData = &fPluginID;
    pluginAVT.mOutputDataSize = sizeof(fPluginID);
    
    osErr = AudioHardwareGetProperty(kAudioHardwarePropertyPlugInForBundleID, &outSize, &pluginAVT);
    if (osErr != noErr) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : AudioHardwareGetProperty kAudioHardwarePropertyPlugInForBundleID error\n");
        printError(osErr);
        return osErr;
    }
    
    //-------------------------------------------------
    // Create a CFDictionary for our aggregate device
    //-------------------------------------------------
    
    CFMutableDictionaryRef aggDeviceDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFStringRef AggregateDeviceNameRef = CFSTR("JackDuplex");
    CFStringRef AggregateDeviceUIDRef = CFSTR("com.grame.JackDuplex");
    
    // add the name of the device to the dictionary
    CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceNameKey), AggregateDeviceNameRef);
    
    // add our choice of UID for the aggregate device to the dictionary
    CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceUIDKey), AggregateDeviceUIDRef);
    
    // add a "private aggregate key" to the dictionary
    int value = 1;
    CFNumberRef AggregateDeviceNumberRef = CFNumberCreate(NULL, kCFNumberIntType, &value);
    
    SInt32 system;
    Gestalt(gestaltSystemVersion, &system);
    
    printf("TCoreAudioRenderer::CreateAggregateDevice : system version = %x limit = %x\n", system, 0x00001054);
    
    // Starting with 10.5.4 systems, the AD can be internal... (better)
    if (system < 0x00001054) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : public aggregate device....\n");
    } else {
        printf("TCoreAudioRenderer::CreateAggregateDevice : private aggregate device....\n");
        CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceIsPrivateKey), AggregateDeviceNumberRef);
    }
    
    // Prepare sub-devices for clock drift compensation
    CFMutableArrayRef subDevicesArrayClock = NULL;
    
    /*
     if (fClockDriftCompensate) {
     if (need_clock_drift_compensation) {
     jack_info("Clock drift compensation activated...");
     subDevicesArrayClock = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
     
     for (UInt32 i = 0; i < captureDeviceID.size(); i++) {
     CFStringRef UID = GetDeviceName(captureDeviceID[i]);
     if (UID) {
     CFMutableDictionaryRef subdeviceAggDeviceDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
     CFDictionaryAddValue(subdeviceAggDeviceDict, CFSTR(kAudioSubDeviceUIDKey), UID);
     CFDictionaryAddValue(subdeviceAggDeviceDict, CFSTR(kAudioSubDeviceDriftCompensationKey), AggregateDeviceNumberRef);
     //CFRelease(UID);
     CFArrayAppendValue(subDevicesArrayClock, subdeviceAggDeviceDict);
     }
     }
     
     for (UInt32 i = 0; i < playbackDeviceID.size(); i++) {
     CFStringRef UID = GetDeviceName(playbackDeviceID[i]);
     if (UID) {
     CFMutableDictionaryRef subdeviceAggDeviceDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
     CFDictionaryAddValue(subdeviceAggDeviceDict, CFSTR(kAudioSubDeviceUIDKey), UID);
     CFDictionaryAddValue(subdeviceAggDeviceDict, CFSTR(kAudioSubDeviceDriftCompensationKey), AggregateDeviceNumberRef);
     //CFRelease(UID);
     CFArrayAppendValue(subDevicesArrayClock, subdeviceAggDeviceDict);
     }
     }
     
     // add sub-device clock array for the aggregate device to the dictionary
     CFDictionaryAddValue(aggDeviceDict, CFSTR(kAudioAggregateDeviceSubDeviceListKey), subDevicesArrayClock);
     } else {
     jack_info("Clock drift compensation was asked but is not needed (devices use the same clock domain)");
     }
     }
     */
    
    //-------------------------------------------------
    // Create a CFMutableArray for our sub-device list
    //-------------------------------------------------
    
    // we need to append the UID for each device to a CFMutableArray, so create one here
    CFMutableArrayRef subDevicesArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    
    //    vector<CFStringRef> captureDeviceUID;
    
    for (UInt32 i = 0; i < captureDeviceID.num; i++) {
        CFStringRef ref = GetDeviceName(captureDeviceID.el[i]);
        if (ref == NULL)
            return -1;
        //captureDeviceUID.push_back(ref);
        captureDeviceUID.el[captureDeviceUID.num++] = ref;
        // input sub-devices in this example, so append the sub-device's UID to the CFArray
        CFArrayAppendValue(subDevicesArray, ref);
    }
    
    //    vector<CFStringRef> playbackDeviceUID;
    for (UInt32 i = 0; i < playbackDeviceID.num; i++) {
        CFStringRef ref = GetDeviceName(playbackDeviceID.el[i]);
        if (ref == NULL)
            return -1;
        //playbackDeviceUID.push_back(ref);
        playbackDeviceUID.el[playbackDeviceUID.num++] = ref;
        // output sub-devices in this example, so append the sub-device's UID to the CFArray
        CFArrayAppendValue(subDevicesArray, ref);
    }
    
    //-----------------------------------------------------------------------
    // Feed the dictionary to the plugin, to create a blank aggregate device
    //-----------------------------------------------------------------------
    
    AudioObjectPropertyAddress pluginAOPA;
    pluginAOPA.mSelector = kAudioPlugInCreateAggregateDevice;
    pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
    pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
    UInt32 outDataSize;
    
    osErr = AudioObjectGetPropertyDataSize(fPluginID, &pluginAOPA, 0, NULL, &outDataSize);
    if (osErr != noErr) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : AudioObjectGetPropertyDataSize error\n");
        printError(osErr);
        goto error;
    }
    
    osErr = AudioObjectGetPropertyData(fPluginID, &pluginAOPA, sizeof(aggDeviceDict), &aggDeviceDict, &outDataSize, outAggregateDevice);
    if (osErr != noErr) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : AudioObjectGetPropertyData error\n");
        printError(osErr);
        goto error;
    }
    
    // pause for a bit to make sure that everything completed correctly
    // this is to work around a bug in the HAL where a new aggregate device seems to disappear briefly after it is created
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    
    //-------------------------
    // Set the sub-device list
    //-------------------------
    
    pluginAOPA.mSelector = kAudioAggregateDevicePropertyFullSubDeviceList;
    pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
    pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
    outDataSize = sizeof(CFMutableArrayRef);
    osErr = AudioObjectSetPropertyData(*outAggregateDevice, &pluginAOPA, 0, NULL, outDataSize, &subDevicesArray);
    if (osErr != noErr) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : AudioObjectSetPropertyData for sub-device list error\n");
        printError(osErr);
        goto error;
    }
    
    // pause again to give the changes time to take effect
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    
    //-----------------------
    // Set the master device
    //-----------------------
    
    // set the master device manually (this is the device which will act as the master clock for the aggregate device)
    // pass in the UID of the device you want to use
    pluginAOPA.mSelector = kAudioAggregateDevicePropertyMasterSubDevice;
    pluginAOPA.mScope = kAudioObjectPropertyScopeGlobal;
    pluginAOPA.mElement = kAudioObjectPropertyElementMaster;
    outDataSize = sizeof(CFStringRef);
    osErr = AudioObjectSetPropertyData(*outAggregateDevice, &pluginAOPA, 0, NULL, outDataSize, &captureDeviceUID.el[0]);  // First apture is master...
    if (osErr != noErr) {
        printf("TCoreAudioRenderer::CreateAggregateDevice : AudioObjectSetPropertyData for master device error\n");
        printError(osErr);
        goto error;
    }
    
    // pause again to give the changes time to take effect
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    
    // Prepare sub-devices for clock drift compensation
    // Workaround for bug in the HAL : until 10.6.2
    
    if (fClockDriftCompensate) {
        if (need_clock_drift_compensation) {
            printf("Clock drift compensation activated...\n");
            
            // Get the property data size
            osErr = AudioObjectGetPropertyDataSize(*outAggregateDevice, &theAddressOwned, theQualifierDataSize, theQualifierData, &outSize);
            if (osErr != noErr) {
                printf("TCoreAudioRenderer::CreateAggregateDevice kAudioObjectPropertyOwnedObjects error\n");
                printError(osErr);
            }
            
            //	Calculate the number of object IDs
            subDevicesNum = outSize / sizeof(AudioObjectID);
            printf("TCoreAudioRenderer::CreateAggregateDevice clock drift compensation, number of sub-devices = %d\n", subDevicesNum);
            AudioObjectID subDevices[subDevicesNum];
            outSize = sizeof(subDevices);
            
            osErr = AudioObjectGetPropertyData(*outAggregateDevice, &theAddressOwned, theQualifierDataSize, theQualifierData, &outSize, subDevices);
            if (osErr != noErr) {
                printf("TCoreAudioRenderer::CreateAggregateDevice kAudioObjectPropertyOwnedObjects error\n");
                printError(osErr);
            }
            
            // Set kAudioSubDevicePropertyDriftCompensation property...
            for (UInt32 index = 0; index < subDevicesNum; ++index) {
                UInt32 theDriftCompensationValue = 1;
                osErr = AudioObjectSetPropertyData(subDevices[index], &theAddressDrift, 0, NULL, sizeof(UInt32), &theDriftCompensationValue);
                if (osErr != noErr) {
                    printf("TCoreAudioRenderer::CreateAggregateDevice kAudioSubDevicePropertyDriftCompensation error\n");
                    printError(osErr);
                }
            }
        } else {
            printf("Clock drift compensation was asked but is not needed (devices use the same clock domain)\n");
        }
    }
    
    // pause again to give the changes time to take effect
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    
    //----------
    // Clean up
    //----------
    
    // release the private AD key
    CFRelease(AggregateDeviceNumberRef);
    
    // release the CF objects we have created - we don't need them any more
    CFRelease(aggDeviceDict);
    CFRelease(subDevicesArray);
    
    if (subDevicesArrayClock)
        CFRelease(subDevicesArrayClock);
    
    // release the device UID
    for (UInt32 i = 0; i < captureDeviceUID.num; i++) {
        CFRelease(captureDeviceUID.el[i]);
    }
    
    for (UInt32 i = 0; i < playbackDeviceUID.num; i++) {
        CFRelease(playbackDeviceUID.el[i]);
    }
    
    printf("New aggregate device %d\n", *outAggregateDevice);
    return noErr;
    
error:
    DestroyAggregateDevice();
    return -1;
}








OSStatus CreateAggregateDevice(AudioDeviceID captureDeviceID, AudioDeviceID playbackDeviceID, int samplerate, AudioDeviceID* outAggregateDevice)
{
    OSStatus err = noErr;
    AudioObjectID sub_device[32];
    UInt32 outSize = sizeof(sub_device);
    AudioDeviceIDArray captureDeviceIDArray, playbackDeviceIDArray;
    
    err = AudioDeviceGetProperty(captureDeviceID, 0, kAudioDeviceSectionGlobal, kAudioAggregateDevicePropertyActiveSubDeviceList, &outSize, sub_device);
    //    vector<AudioDeviceID> captureDeviceIDArray;
    captureDeviceIDArray.num = playbackDeviceIDArray.num = 0;
    
    if (err != noErr) {
        printf("Input device does not have subdevices\n");
        //captureDeviceIDArray.push_back(captureDeviceID);
        captureDeviceIDArray.el[captureDeviceIDArray.num++] = captureDeviceID;
    } else {
        int num_devices = outSize / sizeof(AudioObjectID);
        printf("Input device has %d subdevices\n", num_devices);
        for (int i = 0; i < num_devices; i++) {
            captureDeviceIDArray.el[captureDeviceIDArray.num++] = sub_device[i];
        }
    }
    
    err = AudioDeviceGetProperty(playbackDeviceID, 0, kAudioDeviceSectionGlobal, kAudioAggregateDevicePropertyActiveSubDeviceList, &outSize, sub_device);
    //    vector<AudioDeviceID> playbackDeviceIDArray;
    
    if (err != noErr) {
        printf("Output device does not have subdevices\n");
        //        playbackDeviceIDArray.push_back(playbackDeviceID);
        playbackDeviceIDArray.el[playbackDeviceIDArray.num++] = playbackDeviceID;
        
    } else {
        int num_devices = outSize / sizeof(AudioObjectID);
        printf("Output device has %d subdevices\n", num_devices);
        for (int i = 0; i < num_devices; i++) {
            //            playbackDeviceIDArray.push_back(sub_device[i]);
            playbackDeviceIDArray.el[playbackDeviceIDArray.num++] = sub_device[i];
            
        }
    }
    
    return CreateAggregateDeviceAux(captureDeviceIDArray, playbackDeviceIDArray, samplerate, outAggregateDevice);
}


OSStatus GetDefaultDevice(int inChan, int outChan, int samplerate, AudioDeviceID* id)
{
    UInt32 theSize = sizeof(UInt32);
    AudioDeviceID inDefault;
    AudioDeviceID outDefault;
    OSStatus res;
    
    if ((res = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                        &theSize, &inDefault)) != noErr)
        return res;
    
    if ((res = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                        &theSize, &outDefault)) != noErr)
        return res;
    
    // Duplex mode
    if (inChan > 0 && outChan > 0) {
        // Get the device only if default input and output are the same
        if (inDefault == outDefault) {
            *id = inDefault;
            return noErr;
        } else {
            printf("GetDefaultDevice : input = %uld and output = %uld are not the same, create aggregate device...\n", inDefault, outDefault);
            if (CreateAggregateDevice(inDefault, outDefault, samplerate, id) != noErr)
                return kAudioHardwareBadDeviceError;
       	}
    } else if (inChan > 0) {
        *id = inDefault;
        return noErr;
    } else if (outChan > 0) {
        *id = outDefault;
        return noErr;
    } else {
        return kAudioHardwareBadDeviceError;
    }
    
    return noErr;
}

static void PrintStreamDesc(AudioStreamBasicDescription *inDesc) {
    printf("- - - - - - - - - - - - - - - - - - - -\n");
    printf("  Sample Rate: %f\n", inDesc->mSampleRate);
    printf("  Format ID: %d\n", inDesc->mFormatID);
    printf("  Format Flags %x", inDesc->mFormatFlags);
    printf("  Bytes per Packet: %d\n", inDesc->mBytesPerPacket);
    printf("  Frames per Packet: %d\n", inDesc->mFramesPerPacket);
    printf("  Bytes per Frame: %d\n", inDesc->mBytesPerFrame);
    printf("  Channels per Frame: %d\n", inDesc->mChannelsPerFrame);
    printf("  Bits per Channel: %d\n", inDesc->mBitsPerChannel);
    printf("- - - - - - - - - - - - - - - - - - - -\n");
}

OSStatus liveRender(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,UInt32 unnamed,UInt32 NumberFrames,AudioBufferList *ioData)    {
    static unsigned char *base,*upbase,down[AUDIO_SAMPS_PER_BUFFER*BYTES_PER_SAMPLE];
    int result,i;
    
    
    
    AudioUnitRender(fAUHAL, ioActionFlags, inTimeStamp, 1, NumberFrames, fInputData);
    
    /* the input data */
    if (input_callbacks_done >= (2+buffer_shift)) {  // no change as  buffer_shift decreases below -2
        base = (mode == LIVE_MODE) ? audiodata + lowres_samps_produced*BYTES_PER_SAMPLE : down;
        upbase = fInputData->mBuffers[0].mData;
        filtered_downsample(upbase, base,AUDIO_SAMPS_PER_BUFFER,input_callbacks_done,&result); // this is to decrease aliasing
        lowres_samps_produced += result;
        if ((mode == LIVE_MODE) && is_hires)   queue_io_buff(&solo_out, upbase); // delay disk writing to outside of callback
    }
    input_callbacks_done++;
    
    /* the output data */
    global_audio_buff = ioData->mBuffers[0].mData;
    vcode_action();
    /*  if (upbase) for (i=0; i < NumberFrames; i++) {  // kludge for software monitoring of solo audio
     global_audio_buff[i*BYTES_PER_SAMPLE*2] = upbase[2*i];
     global_audio_buff[i*BYTES_PER_SAMPLE*2+1] = upbase[2*i+1];
     
     }*/
    played_samples += NumberFrames;
    output_callbacks_done++;
    return(0);
    
    /*	//Get the new audio data
     err = AudioUnitRender(mInputUnit,
     ioActionFlags,
     inTimeStamp,
     inBusNumber,
     inNumberFrames, //# of frames requested
     // mInputBuffer
     &abl);// Audio Buffer List to hold data
     if (err) { printf("error in InputProc\n"); return(err); }
     
     if (input_callbacks_done == 0) {
     input_zero = inTimeStamp->mHostTime;
     if (output_callbacks_done > 0) set_audio_skew();
     }
     base = (mode == LIVE_MODE) ? audiodata + lowres_samps_produced*BYTES_PER_SAMPLE : down;
     upbase = temp;
     filtered_downsample(upbase, base,AUDIO_SAMPS_PER_BUFFER,input_callbacks_done,&result); // this is to decrease aliasing
     lowres_samps_produced += result;
     if ((mode == LIVE_MODE) && is_hires)   queue_io_buff(&solo_out, upbase); // delay disk writing to outside of callback
     input_callbacks_done++;
     nanoDiff = AudioConvertHostTimeToNanos(cur_output_host_time - inTimeStamp->mHostTime);
     return err;*/
}


OSStatus playThroughRender(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,UInt32 unnamed,UInt32 inNumberFrames,AudioBufferList *ioData)    {
    unsigned char *in_ptr;
    
    //  printf("now = %f\n",now());
    
    AudioUnitRender(fAUHAL, ioActionFlags, inTimeStamp, 1, inNumberFrames, fInputData);
    in_ptr = fInputData->mBuffers[0].mData;
    interleave_channels(in_ptr, in_ptr , ioData->mBuffers[0].mData, inNumberFrames) ;
    return 0;
}


#define DELAY_FRAMES 50

OSStatus delayRender(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,UInt32 unnamed,UInt32 inNumberFrames,AudioBufferList *ioData)    {
    //  unsigned char *in_ptr;
    int i,k;
    static unsigned char buff[DELAY_FRAMES][BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER];
    
    
    if (input_callbacks_done == 0) memset(buff,0,sizeof(buff));
    //  printf("now = %f\n",now());
    
    AudioUnitRender(fAUHAL, ioActionFlags, inTimeStamp, 1, inNumberFrames, fInputData);
    //  in_ptr = fInputData->mBuffers[0].mData;
    memcpy(buff[input_callbacks_done%DELAY_FRAMES], fInputData->mBuffers[0].mData, BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    i = (input_callbacks_done + 2)%DELAY_FRAMES;
    /* with 2 as constant, this verifies the usual double buffering model of timing:
     The incoming samples were recorded 1 buffer ago; the output samples will cross the speaker 1 buffer in the future.
     So there is a difference of two buffers between the true times of the incoming and outgoing samples.
     To get the correct synchronization, the first two incoming buffers should dropped.  Having done this the ith incoming and outgoing
     buffers coincide in real time. */
    //  if (input_callbacks_done%DELAY_FRAMES == 0) for (k=0; k < 10; k++) buff[i][k] += 100; // put click in audio
    interleave_channels(buff[i], buff[i] , ioData->mBuffers[0].mData, inNumberFrames) ;
    input_callbacks_done++;
    return 0;
}

static int audio_warmed_up = 0;

int
audio_warm_up_done() {
    return(audio_warmed_up);
}

void warm_up_audio() {
    int in_chan,out_chan,sr;
    extern long start_apple_duplex(),stop_apple_duplex();
    
    //GetDefaultDevice(in_chan=1, out_chan=2, sr=48000,&fDeviceID);
    start_apple_duplex();
    stop_apple_duplex();
    audio_warmed_up = 1;
    
}


long OpenDefault(/*dsp* dsp,*/ long inChan, long outChan, long bufferSize, long samplerate) {
    OSStatus err = noErr;
    ComponentResult err1;
    UInt32 outSize;
    UInt32 enableIO;
    Boolean isWritable;
    AudioStreamBasicDescription srcFormat, dstFormat, sampleRate;
    long in_nChannels, out_nChannels;
    
    /*    fDSP = dsp; */  // don't see where this is used yet ...
    
    fDevNumInChans = inChan;
    fDevNumOutChans = outChan;
    
    /*        fInChannel = (float **) malloc(fDevNumInChans*sizeof(float *));
     fOutChannel = (float **) malloc(fDevNumOutChans*sizeof(float *));*/
    fInChannel = (unsigned char  **) malloc(fDevNumInChans*BYTES_PER_SAMPLE);
    fOutChannel = (unsigned char **) malloc(fDevNumOutChans*BYTES_PER_SAMPLE);
    
    printf("OpenDefault inChan = %ld outChan = %ld bufferSize = %ld samplerate = %ld\n", inChan, outChan, bufferSize, samplerate);
    
    SInt32 major;
    SInt32 minor;
    Gestalt(gestaltSystemVersionMajor, &major);
    Gestalt(gestaltSystemVersionMinor, &minor);
    
    // Starting with 10.6 systems, the HAL notification thread is created internally
    if (major == 10 && minor >= 6) {
        CFRunLoopRef theRunLoop = NULL;
        AudioObjectPropertyAddress theAddress = { kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
        OSStatus osErr = AudioObjectSetPropertyData (kAudioObjectSystemObject, &theAddress, 0, NULL, sizeof(CFRunLoopRef), &theRunLoop);
        if (osErr != noErr) {
            printf("TCoreAudioRenderer::Open kAudioHardwarePropertyRunLoop error\n");
            printError(osErr);
        }
    }
    
    if (GetDefaultDevice(inChan, outChan, samplerate,&fDeviceID) != noErr) {
        printf("Cannot open default device\n");
        return OPEN_ERR;
    }
    
    // Setting buffer size
    outSize = sizeof(UInt32);
    err = AudioDeviceSetProperty(fDeviceID, NULL, 0, false, kAudioDevicePropertyBufferFrameSize, outSize, &bufferSize);
    if (err != noErr) {
        printf("Cannot set buffer size %ld\n", bufferSize);
        printError(err);
        return OPEN_ERR;
    }
    
    // Setting sample rate
    outSize = sizeof(AudioStreamBasicDescription);
    err = AudioDeviceGetProperty(fDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &outSize, &sampleRate);
    if (err != noErr) {
        printf("Cannot get current sample rate\n");
        printError(err);
        return OPEN_ERR;
    }
    
    //    if (samplerate != long(sampleRate.mSampleRate)) {
    if (samplerate != (long) sampleRate.mSampleRate) {
        sampleRate.mSampleRate = (Float64)(samplerate);
        err = AudioDeviceSetProperty(fDeviceID, NULL, 0, false, kAudioDevicePropertyStreamFormat, outSize, &sampleRate);
        if (err != noErr) {
            printf("Cannot set sample rate = %ld\n", samplerate);
            printError(err);
            return OPEN_ERR;
        }
    }
    
    // AUHAL
    ComponentDescription cd = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0};
    Component HALOutput = FindNextComponent(NULL, &cd);
    
    err1 = OpenAComponent(HALOutput, &fAUHAL);
    if (err1 != noErr) {
        printf("Error calling OpenAComponent\n");
        printError(err1);
        goto error;
    }
    
    err1 = AudioUnitInitialize(fAUHAL);
    if (err1 != noErr) {
        printf("Cannot initialize AUHAL unit\n");
        printError(err1);
        goto error;
    }
    
    enableIO = 1;
    err1 = AudioUnitSetProperty(fAUHAL, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enableIO, sizeof(enableIO));
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output\n");
        printError(err1);
        goto error;
    }
    
    enableIO = 1;
    err1 = AudioUnitSetProperty(fAUHAL, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enableIO, sizeof(enableIO));
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input\n");
        printError(err1);
        goto error;
    }
    
    err1 = AudioUnitSetProperty(fAUHAL, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &fDeviceID, sizeof(AudioDeviceID));
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_CurrentDevice\n");
        printError(err1);
        goto error;
    }
    
    err1 = AudioUnitSetProperty(fAUHAL, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 1, (UInt32*)&bufferSize, sizeof(UInt32));
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioUnitProperty_MaximumFramesPerSlice\n");
        printError(err1);
        goto error;
    }
    
    err1 = AudioUnitSetProperty(fAUHAL, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, (UInt32*)&bufferSize, sizeof(UInt32));
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioUnitProperty_MaximumFramesPerSlice\n");
        printError(err1);
        goto error;
    }
    
    err1 = AudioUnitGetPropertyInfo(fAUHAL, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Input, 1, &outSize, &isWritable);
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_ChannelMap-INFO 1\n");
        printError(err1);
    }
    
    in_nChannels = (err1 == noErr) ? outSize / sizeof(SInt32) : 0;
    printf("in_nChannels = %ld\n", in_nChannels);
    
    err1 = AudioUnitGetPropertyInfo(fAUHAL, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output, 0, &outSize, &isWritable);
    if (err1 != noErr) {
        printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_ChannelMap-INFO 0\n");
        printError(err1);
    }
    
    out_nChannels = (err1 == noErr) ? outSize / sizeof(SInt32) : 0;
    printf("out_nChannels = %ld\n", out_nChannels);
    
    /*
     Just ignore this case : seems to work without any further change...
     
     if (outChan > out_nChannels) {
     printf("This device hasn't required output channels\n");
     goto error;
     }
     if (inChan > in_nChannels) {
     printf("This device hasn't required input channels\n");
     goto error;
     }
     */
    
    if (outChan < out_nChannels) {
        SInt32 chanArr[out_nChannels];
        for (int i = 0;	i < out_nChannels; i++) {
            chanArr[i] = -1;
        }
        for (int i = 0; i < outChan; i++) {
            chanArr[i] = i;
        }
        err1 = AudioUnitSetProperty(fAUHAL, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output, 0, chanArr, sizeof(SInt32) * out_nChannels);
        if (err1 != noErr) {
            printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_ChannelMap 0\n");
            printError(err1);
        }
    }
    
    if (inChan < in_nChannels) {
        SInt32 chanArr[in_nChannels];
        for (int i = 0; i < in_nChannels; i++) {
            chanArr[i] = -1;
        }
        for (int i = 0; i < inChan; i++) {
            chanArr[i] = i;
        }
        AudioUnitSetProperty(fAUHAL, kAudioOutputUnitProperty_ChannelMap , kAudioUnitScope_Input, 1, chanArr, sizeof(SInt32) * in_nChannels);
        if (err1 != noErr) {
            printf("Error calling AudioUnitSetProperty - kAudioOutputUnitProperty_ChannelMap 1\n");
            printError(err1);
        }
    }
    
    if (inChan > 0) {
        outSize = sizeof(AudioStreamBasicDescription);
        err1 = AudioUnitGetProperty(fAUHAL, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &srcFormat, &outSize);
        if (err1 != noErr) {
            printf("Error calling AudioUnitGetProperty - kAudioUnitProperty_StreamFormat kAudioUnitScope_Output\n");
            printError(err1);
        }
        PrintStreamDesc(&srcFormat);
        
        srcFormat.mSampleRate = samplerate;
        srcFormat.mFormatID = kAudioFormatLinearPCM;
        srcFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
        srcFormat.mBytesPerPacket = BYTES_PER_SAMPLE;
        srcFormat.mFramesPerPacket = 1;
        srcFormat.mBytesPerFrame = BYTES_PER_SAMPLE;
        srcFormat.mChannelsPerFrame = inChan;
        srcFormat.mBitsPerChannel = 8*BYTES_PER_SAMPLE;
        
        PrintStreamDesc(&srcFormat);
        
        err1 = AudioUnitSetProperty(fAUHAL, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &srcFormat, sizeof(AudioStreamBasicDescription));
        if (err1 != noErr) {
            printf("Error calling AudioUnitSetProperty - kAudioUnitProperty_StreamFormat kAudioUnitScope_Output\n");
            printError(err1);
        }
    }
    
    if (outChan > 0) {
        outSize = sizeof(AudioStreamBasicDescription);
        err1 = AudioUnitGetProperty(fAUHAL, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &dstFormat, &outSize);
        if (err1 != noErr) {
            printf("Error calling AudioUnitGetProperty - kAudioUnitProperty_StreamFormat kAudioUnitScope_Output\n");
            printError(err1);
        }
        PrintStreamDesc(&dstFormat);
        
        dstFormat.mSampleRate = samplerate;
        dstFormat.mFormatID = kAudioFormatLinearPCM;
        //dstFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
        
        dstFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
        
        dstFormat.mChannelsPerFrame = outChan;
        dstFormat.mBytesPerPacket = outChan*BYTES_PER_SAMPLE;
        dstFormat.mFramesPerPacket = 1;
        dstFormat.mBytesPerFrame = outChan*BYTES_PER_SAMPLE;
        dstFormat.mBitsPerChannel = 8*BYTES_PER_SAMPLE;
        
        PrintStreamDesc(&dstFormat);
        
        err1 = AudioUnitSetProperty(fAUHAL, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &dstFormat, sizeof(AudioStreamBasicDescription));
        if (err1 != noErr) {
            printf("Error calling AudioUnitSetProperty - kAudioUnitProperty_StreamFormat kAudioUnitScope_Output\n");
            printError(err1);
        }
    }
    
    if (inChan > 0 && outChan == 0) {
        AURenderCallbackStruct output;
        output.inputProc =  (mode == SOUND_CHECK_MODE) ? delayRender : liveRender;
        output.inputProcRefCon = NULL; //this;
        err1 = AudioUnitSetProperty(fAUHAL, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &output, sizeof(output));
        if (err1 != noErr) {
            printf("Error calling AudioUnitSetProperty - kAudioUnitProperty_SetRenderCallback 1\n");
            printError(err1);
            goto error;
        }
    } else {
        AURenderCallbackStruct output;
        output.inputProc =  (mode == SOUND_CHECK_MODE) ? delayRender : liveRender;
        output.inputProcRefCon = NULL; //this;
        err1 = AudioUnitSetProperty(fAUHAL, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &output, sizeof(output));
        if (err1 != noErr) {
            printf("Error calling AudioUnitSetProperty - kAudioUnitProperty_SetRenderCallback 0\n");
            printError(err1);
            goto error;
        }
    }
    
    fInputData = (AudioBufferList*)malloc(sizeof(UInt32) + inChan * sizeof(AudioBuffer));
    if (fInputData == 0) {
        printf("Cannot allocate memory for input buffers\n");
        goto error;
    }
    fInputData->mNumberBuffers = inChan;
    
    // Prepare buffers
    for (int i = 0; i < inChan; i++) {
        fInputData->mBuffers[i].mNumberChannels = 1;
        fInputData->mBuffers[i].mData = malloc(bufferSize * sizeof(float));
        fInputData->mBuffers[i].mDataByteSize = bufferSize * sizeof(float);
    }
    
    return NO_ERR;
    
error:
    AudioUnitUninitialize(fAUHAL);
    CloseComponent(fAUHAL);
    return OPEN_ERR;
}

float
callback_time() {
    /* approximation of current time using audio.  As this is a function of output_callbakcs_done it can't be very accurate */
    return(((output_callbacks_done-1)*HOP_LEN)/(float) OUTPUT_SR);
}

long start_apple_duplex()
{
    int in_chan, out_chan, buff_size, sr;
    OSStatus err;
    
    done_recording = played_samples = output_callbacks_done = input_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    
    err = OpenDefault(/*dsp* dsp,*/ in_chan = 1, out_chan = 2, buff_size = 1024, sr = 48000);
    if (err == OPEN_ERR) return(0);
    
    err = AudioOutputUnitStart(fAUHAL);
    
    if (err != noErr) {
        printf("Error while opening device : device open error \n");
        return(0);
    } 
    else return(1);
}

long stop_apple_duplex()
{
    OSStatus err;
    err = AudioOutputUnitStop(fAUHAL);
    
    if (err != noErr) {
        printf("Error while closing device : device close error \n");
        return(0);
    }
    
    
    err = DestroyAggregateDevice();  // If I don't do this the 2nd call to start_apple_duplex fails
    
    if (err != noErr) {
        printf("Error in DestroyAggregateDevice \n");
        return(0);
    }
    
    return(1);
}


int faust_release() { // doesn't seem to fix the problem with the opening clip not playing after start_apple_duplex called 
    OSStatus err = noErr;
    
    if (fAUHAL == 0) return(1);
    err = AudioUnitUninitialize(fAUHAL);
    if (err) { printf ("AudioUnitUninitialize=%ld\n", err); return(0); }
    err = CloseComponent(fAUHAL);
    if (err) { printf ("CloseComponent=%ld\n", err); return(0); }
    DestroyAggregateDevice();
    return(1);
}



void
faust_test() {
    AudioDeviceID adid;
    int in_chan, out_chan, buff_size, sr;
    
    // OpenDefault(/*dsp* dsp,*/ in_chan = 1, out_chan = 2, buff_size = 1024, sr = 48000);
    start_apple_duplex();
    
    //  GetDefaultDevice(1,2,48000,&adid);
}

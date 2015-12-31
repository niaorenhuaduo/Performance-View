

int
pa_end_playing() {
    int err;
    
    
    if (Pa_StopStream( audio_stream ) != paNoError) { printf("couldn't stop stream\n"); return(0); } 
    
    //  return;  // for now
    err = Pa_Terminate();
    if( err != paNoError ) { printf("error in Pa_Terminate()\n");  return(0); }
    return(1);
}



int  start_audio_callbacks() {
    static paTestData data;
    PaStreamInfo *stream_info;
    PaStreamParameters outparam, inparam;

   
    PaError err;
    int input_channels;

    done_recording = played_samples = output_callbacks_done = input_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) {
      if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    }
    
    if( Pa_Initialize() != paNoError ) { printf("error in Pa_Initialize()\n"); return(0); }
    
    input_channels = (mode == LIVE_MODE) ? 1 : 0;
    
    
    outparam.device = Pa_GetDefaultOutputDevice();
    if (outparam.device==paNoDevice) { NSLog("couldn't open output audio device"); return(0); }
    outparam.channelCount = 2;
    outparam.sampleFormat = paInt16;
    outparam.suggestedLatency = Pa_GetDeviceInfo(outparam.device)->defaultLowOutputLatency;
    outparam.suggestedLatency = .01;
    printf("suggested output latency = %f\n",outparam.suggestedLatency);
    outparam.hostApiSpecificStreamInfo = NULL;
    
    
    inparam.device = Pa_GetDefaultInputDevice();
    if (inparam.device==paNoDevice) { NSLog("couldn't open input audio device"); return(0); }
    inparam.channelCount = input_channels;
    inparam.sampleFormat = paInt16;
    inparam.suggestedLatency = Pa_GetDeviceInfo(inparam.device)->defaultLowInputLatency;
    inparam.suggestedLatency = .01;
    
    printf("suggested input latency = %f\n",inparam.suggestedLatency);
    inparam.hostApiSpecificStreamInfo = NULL;
    
    
    err =  Pa_OpenStream(&audio_stream,
                         &inparam,
                         &outparam, 
                         48000,
                         AUDIO_SAMPS_PER_BUFFER, 
                         paNoFlag, 
                         vocodeCallback, 
                         &data);
    if (err != paNoError) { printf("failed opening stream\n"); return(0); }
    
    stream_info = Pa_GetStreamInfo(audio_stream);
    printf("input latency = %f\noutput latency = %f\n",stream_info->inputLatency,stream_info->outputLatency);  

    
    
    
    
    
    
    
 /*   err = Pa_OpenDefaultStream( &audio_stream, input_channels,  
                               2,        //   stereo output 
                               paInt16,  //hpe these are signed 
                               48000,
                               AUDIO_SAMPS_PER_BUFFER,        
				//                               patestCallback, // this is your callback function 
                               vocodeCallback, // this is your callback function 
                               &data ); //This is a pointer that will be passed to your callback
   
    if( err != paNoError ) { printf("PaOpenDefaultStream error\n"); return(0); }*/
   
        err = Pa_StartStream( audio_stream );
    if( err != paNoError ) { printf("couldn't start stream\n"); return(0); }
    
    return(1);
}


int  test_audio_callbacks() {
    static paTestData data;
    PaStreamParameters outparam, inparam;
    PaError err;
    PaStreamInfo *stream_info;
    int input_channels,devices,i;
    PaDeviceInfo *info;
    PaMacCoreStreamInfo *macinfo;
    
    macinfo = (PaMacCoreStreamInfo *) malloc(sizeof( PaMacCoreStreamInfo ));
    bzero(macinfo, sizeof( PaMacCoreStreamInfo ) );
    macinfo->size = sizeof( PaMacCoreStreamInfo );
    macinfo->hostApiType = paCoreAudio;
    macinfo->version = 0x01;
    macinfo->flags = paMacCorePro;
    macinfo->channelMap = NULL;
    macinfo->channelMapSize = 0;

    done_recording = played_samples = output_callbacks_done = input_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) {
      if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    }
    
    if( Pa_Initialize() != paNoError ) { printf("error in Pa_Initialize()\n"); return(0); }
    
    devices = Pa_GetDeviceCount();
    for (i=0; i < devices; i++) {
        info = Pa_GetDeviceInfo(i);
        printf("device %d: name = %s input=%d output=%d sr = %f input latency = %f output latency = %f\n",i,info->name,info->maxInputChannels,info->maxOutputChannels,info->defaultSampleRate,info->defaultLowInputLatency,info->defaultLowOutputLatency);
    }
    

    outparam.device = Pa_GetDefaultOutputDevice();
    if (outparam.device==paNoDevice) { NSLog("couldn't open output audio device"); return(0); }
    outparam.channelCount = 2;
    outparam.sampleFormat = paInt16;
    outparam.suggestedLatency = Pa_GetDeviceInfo(outparam.device)->defaultLowOutputLatency;
 //   outparam.suggestedLatency = .005;
    printf("suggested output latency = %f\n",outparam.suggestedLatency);
    outparam.hostApiSpecificStreamInfo = macinfo;


    inparam.device = Pa_GetDefaultInputDevice();
    if (inparam.device==paNoDevice) { NSLog("couldn't open input audio device"); return(0); }
    inparam.channelCount = 1;
    inparam.sampleFormat = paInt16;
    inparam.suggestedLatency = Pa_GetDeviceInfo(inparam.device)->defaultLowInputLatency;
  //  inparam.suggestedLatency = .005;

    printf("suggested input latency = %f\n",inparam.suggestedLatency);
    inparam.hostApiSpecificStreamInfo = macinfo;


    err =  Pa_OpenStream(&audio_stream,
		  &inparam,
		  &outparam, 
		  48000.,
		  1024, 
		  paNoFlag,
		  delayTestCallback, 
		  &data);
   if (err != paNoError) { printf("failed opening stream\n"); return(0); }

    stream_info = Pa_GetStreamInfo(audio_stream);
    printf("input latency = %f\noutput latency = %f\n",stream_info->inputLatency,stream_info->outputLatency);  

    /* Open an audio I/O stream. */

    err = Pa_StartStream( audio_stream );
    if( err != paNoError ) { printf("couldn't start stream\n"); return(0); }
    return(1);
}

int  start_play_callbacks() {
    static paTestData data;
   
    PaError err;
    int input_channels;
    PaStreamParameters outparam;
    

    done_recording = played_samples = output_callbacks_done = input_callbacks_done = lowres_samps_produced = 0;
    if (mode == LIVE_MODE) {
      if (is_hires) prepare_hires_solo_output(BYTES_PER_SAMPLE*AUDIO_SAMPS_PER_BUFFER);
    }
    
    if( Pa_Initialize() != paNoError ) { printf("error in Pa_Initialize()\n"); return(0); }


    

    input_channels = 0;
    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &audio_stream, input_channels,  
                               2,          /* stereo output */
                               paInt16,  /* hpe these are signed */
                               (mode == INTRO_CLIP_MODE) ? 44100 : 48000,
                               AUDIO_SAMPS_PER_BUFFER,        /* frames per buffer REQUEST */
				(mode == INTRO_CLIP_MODE) ? IntroCallback : PlayCallback,
                               &data ); /*This is a pointer that will be passed to
                                         your callback*/
    if( err != paNoError ) { printf("PaOpenDefaultStream error\n"); return(0); }
    
    err = Pa_StartStream( audio_stream );
    if( err != paNoError ) { printf("couldn't start stream\n"); return(0); }
    
    return(1);
}


void duplex_test() {
    static paTestData data;
    PaStream *stream;
    PaError err;
    
    err = Pa_Initialize();
    if( err != paNoError ) { printf("error in Pa_Initialize()\n"); exit(0); }
    
            
    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                               1,          /* no input channels */
                               2,          /* stereo output */
                               paFloat32,  /* 32 bit floating point output */
                               48000,
                               1024,        /* frames per buffer, i.e. the number
                                            of sample frames that PortAudio will
                                            request from the callback. Many apps
                                            may want to use
                                            paFramesPerBufferUnspecified, which
                                            tells PortAudio to pick the best,
                                            possibly changing, buffer size.*/
                               patestCallback, /* this is your callback function */
                               &data ); /*This is a pointer that will be passed to
                                         your callback*/
    if( err != paNoError ) { printf("PaOpenDefaultStream error\n"); exit(0); }
    
    err = Pa_StartStream( stream );
    if( err != paNoError ) { printf("couldn't start stream\n"); }
    
    Pa_Sleep(3000);
    
    if (Pa_StopStream( stream ) != paNoError) { printf("couldn't stop stream\n"); exit(0); } 
    
   // return;  // for now
    err = Pa_Terminate();
    if( err != paNoError ) { printf("error in Pa_Terminate()\n"); exit(0); }
    

}

/* Extremely simple (and small) example of using AKRip32.dll in a command-
 * line application.  This sample:
 *   1. Finds and gets a handle to the first CD unit
 *   2. Reads the TOC
 *   3. Extracts the first track into a WAV file, "track1.wav"
 */

#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "akrip32.h"
#include "aspilib.h"
#include "audio.h"
#include "cdread.h"
#include "linux.h"




/*
 * WAV file header format
 */
typedef struct
{
  BYTE  riff[4];            /* must be "RIFF"                */
  DWORD len;                /* #bytes + 44 - 8               */
  BYTE  cWavFmt[8];         /* must be "WAVEfmt"             */
  DWORD dwHdrLen;
  WORD  wFormat;
  WORD  wNumChannels;
  DWORD dwSampleRate;
  DWORD dwBytesPerSec;
  WORD  wBlockAlign;
  WORD  wBitsPerSample;
  BYTE  cData[4];            /* must be "data"               */
  DWORD dwDataLen;           /* #bytes                       */
} PACKED WAVHDR, *PWAVHDR, *LPWAVHDR;


HCDROM GetFirstCDHandle( void );
void MSB2DWORD( DWORD *d, BYTE *b );
void RipAudio( HCDROM hCD, DWORD dwStart, DWORD dwLen );
void get_mmo_audio( char *ofile, HCDROM hCD, DWORD dwStart, DWORD dwLen );
LPTRACKBUF NewTrackBuf( DWORD numFrames );
void writeWavHeader( FILE *fp, DWORD len );
void writemono48WavHeader( FILE *fp, DWORD len );
void write_resample_header( FILE *fp, DWORD len );
void write_wav_header(FILE *fp, DWORD len, int sr, int channels);
void get_cd_bytes(HCDROM hCD, DWORD dwStart, DWORD bytes, unsigned char *buff);
void get_mmo_audio_background( char *ofile, HCDROM hCD, DWORD dwStart, DWORD dwLen );








/*int
sample2int(unsigned char *temp) {
  int i,tot,j,g;
  unsigned char b[2],*ptr;


  
  tot= 0;
  ptr = (unsigned char *) &tot;
  for (j = 0; j < 2; j++)  ptr[j] = temp[j];
  if (tot & 0x8000) tot |= 0xffff0000;
  return(tot);
}

void
int2sample(int v, unsigned char *samp) {
  unsigned char *ptr;
  int i;

  ptr = (unsigned char *) &v;
  for (i=0; i < 2; i++) samp[i] = ptr[i];
}

void
float2sample(float x, unsigned char *samp) {
  unsigned char *ptr;
  int i,v;

  v =  (int) (x * (float) 0x10000);
  ptr = (unsigned char *) &v;
  for (i=0; i < 2; i++) samp[i] = ptr[i];
}

float
sample2float(unsigned char *temp) {
  int i,tot,j,g;
  unsigned char b[2],*ptr;
  float x;


  
  tot= 0;
  ptr = (unsigned char *) &tot;
  for (j = 0; j < 2; j++)   ptr[j] = temp[j];
  if (tot & 0x8000) tot |= 0xffff0000;
  x = tot / (float) 0x10000;
  return(x);
}
*/


#define CIRC_SAMPLES 10

static void
convert_sr() {
  FILE *fp,*ofp;
  float buff[CIRC_SAMPLES];
  double t=0,inc,x,y;
  int p=0,a,b,count,lasta=0;
  WAVHDR wav;
  unsigned char sample[2];

  fp = fopen( "track1.wav", "rb" );
  ofp = fopen( "track1_var.wav", "wb" );
  write_wav_header(ofp, 0, 48000, 1);
  inc = 44100 / (float) 48000;
  fread(&wav,sizeof(wav),1,fp);
  for (p=0; p < 2; p++) { 
    fread(sample,2,1,fp); buff[p] =  sample2float(sample); 
  }
  for (count=0; ; count++) {
    if (feof(fp)) break;
    a = ((int) t);
    if (a != lasta) {
      fread(sample,2,1,fp); 
      buff[p%CIRC_SAMPLES] =  sample2float(sample); 
      p++;
      p = p % CIRC_SAMPLES;
    }
    lasta = a;
    b = (a+1) % CIRC_SAMPLES;
    x = t - (int) t;
    y = 1-x;
    float2sample(y*buff[a] + x*buff[b], &sample);
    fwrite(sample,2,1,ofp);
    t += inc;
    if (t > CIRC_SAMPLES) t -= CIRC_SAMPLES;
  }
  write_wav_header(ofp, count*2, 48000, 1);
  fclose(ofp);
  fclose(fp);
}


static float
get_sample(FILE *fp ) {
  unsigned char left[2],rite[2];

  fread(left,2,1,fp); 
  fread(rite,2,1,fp); 
  return((sample2float(left) + sample2float(rite))/2); 
}

static void
cd248mono(char *wavin, char *wavout) {
  FILE *fp,*ofp;
  float buff[CIRC_SAMPLES];
  double t=0,x,y;
  int p=0,a,b;
  WAVHDR wav;
  long numer,count;
  unsigned char sample[2];

  fp = fopen( wavin, "rb" );
  ofp = fopen( wavout, "wb" );
  write_wav_header(ofp, 0, 48000, 1);
  fread(&wav,sizeof(wav),1,fp);
  for (p=0; p < 2; p++) buff[p] = get_sample(fp); 
  for (count=0; ; count++) {
    if (feof(fp)) break;
    a = ((count * 441) % (480 * CIRC_SAMPLES))  / 480;
    t = ((count * 441) % (480 * CIRC_SAMPLES))  / 480.;
    b = (a+1) % CIRC_SAMPLES;
    if (count*441/480 != (count-1)*441/480) {
      buff[p%CIRC_SAMPLES] = get_sample(fp);
      p++;
      p = p % CIRC_SAMPLES;
    }
    x = t - a;
    y = 1-x;
    float2sample(y*buff[a] + x*buff[b], &sample);
    fwrite(sample,2,1,ofp);
  }
  write_wav_header(ofp, count*2, 48000, 1);
  fclose(ofp);
  fclose(fp);
}










void RipAudio( HCDROM hCD, DWORD dwStart, DWORD dwLen )
{
  DWORD dwStatus;
  FILE *fp;
  DWORD num2rip = 26;
  int retries;
  LPTRACKBUF t;
  int c = 0;
  DWORD numWritten = 0;

  t = NewTrackBuf( 26 );
  if ( !t )
    return;

  fp = fopen( "track1.wav", "w+b" );

  writeWavHeader( fp, 0 );

  printf( "Beginning to rip %d sectors starting at sector %d\n", dwLen, dwStart );

  while( dwLen )
    {
      if ( !( c++ % 5) )
	printf( "%d: %d\n", dwStart, dwLen );

      if ( dwLen < num2rip )
	num2rip = dwLen;

      retries = 3;
      dwStatus = SS_ERR;
      while ( retries-- && (dwStatus != SS_COMP) )
	{
	  t->numFrames = num2rip;
	  t->startOffset = 0;
	  t->len = 0;
	  t->startFrame = dwStart;
	  dwStatus = ReadCDAudioLBA( hCD, t );
	}
      if ( dwStatus == SS_COMP )
	{
	  fwrite( t->buf + t->startOffset, 1, t->len, fp );
	  numWritten += t->len;
	}
      else
	{
	  printf( "Error at %d (%d:%d)\n", dwStart, GetAspiLibError(), GetAspiLibAspiError() );
	}
      dwStart += num2rip;
      dwLen -= num2rip;
    }

  writeWavHeader( fp, numWritten );
  
  fclose( fp );
  GlobalFree( (HGLOBAL)t );
}




void
stereo2mono(unsigned char *buff, int bytes) {
  int i,s1,s2,s;

  if (i%4) { printf("bad input in stereo2mono\n"); exit(0); }
  for (i=0; i < bytes; i+= 4) {
    s1 = sample2int(buff + i);
    s2 = sample2int(buff + i + 2);
    s = (s1 + s2) / 2;
    int2sample(s,buff + i/2);
  }
}







void get_mmo_audio( char *ofile, HCDROM hCD, DWORD dwStart, DWORD dwLen )
{
  DWORD dwStatus;
  FILE *fp;
  DWORD num2rip = 26;
  int retries;
  LPTRACKBUF t;
  int c = 0,b,frames,total;
  DWORD numWritten = 0;

  total = dwLen;
  t = NewTrackBuf( 26 );
  if ( !t )
    return;

  printf("opening %s\n",ofile);
  fp = fopen( ofile, "w+b" );

  //  writeWavHeader( fp, 0);

  printf( "Beginning to rip %d sectors starting at sector %d\n", dwLen, dwStart );

  while( dwLen )    {
    //    printf("progree = %d\n",percentage_cd_progress);
    //    if (c == 10) return;  /* temporary */
    if ( !( c++ % 5) )   printf( "%d: %d\n", dwStart, dwLen );
    if ( dwLen < num2rip )
      num2rip = dwLen;

      retries = 3;
      dwStatus = SS_ERR;
      while ( retries-- && (dwStatus != SS_COMP) )
	{
	  t->numFrames = num2rip;
	  t->startOffset = 0;
	  t->len = 0;
	  t->startFrame = dwStart;
	  dwStatus = ReadCDAudioLBA( hCD, t );
	}
      if ( dwStatus == SS_COMP ) {
	  frames  = t->len/4;
	  numWritten += add_circ_buff(fp,t->buf + t->startOffset, frames);
      }
      else
	{
	  printf( "Error at %d (%d:%d)\n", dwStart, GetAspiLibError(), GetAspiLibAspiError() );
	}
      dwStart += num2rip;
      dwLen -= num2rip;
    }

  //  writeWavHeader( fp, numWritten );
  fclose( fp );
  GlobalFree( (HGLOBAL)t );
}


static void
read_cd_progress(int num) {
  DWORD num2rip = 26;
  int retries;
  DWORD dwStatus;
  int c = 0,b,frames,total;
  DWORD numWritten = 0;

  static int times;

  times++;
  //  if (times == 2500) return;

  printf("back_len = %d\n",back_len);

  //  if (back_len == 0)    {
  if (back_len <= 0)    {
    fclose( back_fp );
    printf("closed back_fp\n");
    GlobalFree( (HGLOBAL)back_t );
    CloseCDHandle( back_hCD );
    myDllMain(DLL_PROCESS_DETACH);
    return;
  }
  if ( back_len < num2rip )  num2rip = back_len;
  retries = 3;
  dwStatus = SS_ERR;
  while ( retries-- && (dwStatus != SS_COMP) )	{
    back_t->numFrames = num2rip;
    back_t->startOffset = 0;
    back_t->len = 0;
    back_t->startFrame = back_start;
    dwStatus = ReadCDAudioLBA( back_hCD, back_t );
  }
  if ( dwStatus == SS_COMP ) {
    frames  = back_t->len/4;
    printf("calling calling\n");
    numWritten += add_circ_buff(back_fp,back_t->buf + back_t->startOffset, frames);
  }
  else  printf( "Error at %d (%d:%d)\n", back_start, GetAspiLibError(), GetAspiLibAspiError() );
  printf("num2rip = %d backt->len = %d\n",num2rip,back_t->len);
  back_start += num2rip;
  back_len -= num2rip;
  set_timer_signal(.01);
}



int
read_cd_iter() {
  DWORD num2rip = 26;
  int retries;
  DWORD dwStatus;
  int c = 0,b,frames,total;
  DWORD numWritten = 0;

  static int times;

  times++;
  //  if (times == 2500) return;

  printf("back_len = %d\n",back_len);

  //  if (back_len == 0)    {
  if (back_len <= 0)    {
    fclose( back_fp );
    printf("closed back_fp\n");
    GlobalFree( (HGLOBAL)back_t );
    CloseCDHandle( back_hCD );
    myDllMain(DLL_PROCESS_DETACH);
    return(0);
  }
  if ( back_len < num2rip )  num2rip = back_len;
  retries = 3;
  dwStatus = SS_ERR;
  while ( retries-- && (dwStatus != SS_COMP) )	{
    back_t->numFrames = num2rip;
    back_t->startOffset = 0;
    back_t->len = 0;
    back_t->startFrame = back_start;
    dwStatus = ReadCDAudioLBA( back_hCD, back_t );
  }
  if ( dwStatus == SS_COMP ) {
    frames  = back_t->len/4;
    printf("calling calling\n");
    numWritten += add_circ_buff(back_fp,back_t->buf + back_t->startOffset, frames);
  }
  else  printf( "Error at %d (%d:%d)\n", back_start, GetAspiLibError(), GetAspiLibAspiError() );
  printf("num2rip = %d backt->len = %d numWritten = %d\n",num2rip,back_t->len,numWritten);
  back_start += num2rip;
  back_len -= num2rip;
  return(1);
}



void get_mmo_audio_background( char *ofile, HCDROM hCD, DWORD dwStart, DWORD dwLen )
{
  FILE *fp;

  back_t = NewTrackBuf( 26 );
  if ( !back_t )
    return;
  back_len = back_tot = dwLen;
  back_start = dwStart;


  printf("opening %s\n",ofile);
  back_fp = fopen( ofile, "w+b" );
  printf( "Beginning to rip %d sectors starting at sector %d\n", dwLen, dwStart );
  init_clock();
  install_signal_action(read_cd_progress);
  read_cd_progress(0);
}





void get_cd_bytes(HCDROM hCD, DWORD dwStart, DWORD bytes, unsigned char *biff)
{
  DWORD dwStatus;
  FILE *fp;
  DWORD num2rip = 26;
  int retries;
  LPTRACKBUF t;
  int c = 0,b,frames,i;
  DWORD numWritten = 0;

  t = NewTrackBuf( 26 );
  if ( !t )
    return;

  retries = 3;
  dwStatus = SS_ERR;
  while ( retries-- && (dwStatus != SS_COMP) ) {
    t->numFrames = ID_COUNT;  /* is this frames or bytes?*/
    t->startOffset = 0;
    t->len = 0;
    t->startFrame = dwStart;
    dwStatus = ReadCDAudioLBA( hCD, t );
  }
  if ( dwStatus != SS_COMP ) 
    printf( "Error at %d (%d:%d)\n", 
	    dwStart, GetAspiLibError(), GetAspiLibAspiError() );
  for (i=0; i < ID_COUNT; i++) biff[i] = t->buf[i];
}

void old_get_mmo_audio( char *ofile, HCDROM hCD, DWORD dwStart, DWORD dwLen )
{
  DWORD dwStatus;
  FILE *fp;
  DWORD num2rip = 26;
  int retries;
  LPTRACKBUF t;
  int c = 0,b;
  DWORD numWritten = 0;

  t = NewTrackBuf( 26 );
  if ( !t )
    return;

  fp = fopen( ofile, "w+b" );

  writeWavHeader( fp, 0);

  printf( "Beginning to rip %d sectors starting at sector %d\n", dwLen, dwStart );

  while( dwLen )
    {
      if ( !( c++ % 5) )
	printf( "%d: %d\n", dwStart, dwLen );

      if ( dwLen < num2rip )
	num2rip = dwLen;

      retries = 3;
      dwStatus = SS_ERR;
      while ( retries-- && (dwStatus != SS_COMP) )
	{
	  t->numFrames = num2rip;
	  t->startOffset = 0;
	  t->len = 0;
	  t->startFrame = dwStart;
	  dwStatus = ReadCDAudioLBA( hCD, t );
	}
      if ( dwStatus == SS_COMP )
	{
	  //	  stereo2mono(t->buf + t->startOffset, t->len);
	  //	  b = t->len/2;
	  //	  add_circ_buff(fp,t->buf + t->startOffset, b/2);
	  fwrite( t->buf + t->startOffset, 1, t->len, fp );
	  //	  printf("len = %d\n",b);
	  numWritten += t->len;
	}
      else
	{
	  printf( "Error at %d (%d:%d)\n", dwStart, GetAspiLibError(), GetAspiLibAspiError() );
	}
      dwStart += num2rip;
      dwLen -= num2rip;
    }

  writeWavHeader( fp, numWritten );
  fclose( fp );
  GlobalFree( (HGLOBAL)t );
}




void writeWavHeader( FILE *fp, DWORD len )
{
  WAVHDR wav;

  if ( !fp )
    return;

  memcpy( wav.riff, "RIFF", 4 );
  wav.len = len + 44 - 8;
  memcpy( wav.cWavFmt, "WAVEfmt ", 8 );
  wav.dwHdrLen = 16;
  wav.wFormat = 1;
  wav.wNumChannels = 2;
  wav.dwSampleRate = 44100;
  wav.dwBytesPerSec = 44100*2*2;
  wav.wBlockAlign = 4;
  wav.wBitsPerSample = 16;
  memcpy( wav.cData, "data", 4 );
  wav.dwDataLen = len;
  fseek( fp, 0, SEEK_SET );

  fwrite( &wav, 1, sizeof(wav), fp );
}

void writemono48WavHeader( FILE *fp, DWORD len )
{
  WAVHDR wav;

  if ( !fp )
    return;

  memcpy( wav.riff, "RIFF", 4 );
  wav.len = len + 44 - 8;
  memcpy( wav.cWavFmt, "WAVEfmt ", 8 );
  wav.dwHdrLen = 16;
  wav.wFormat = 1;
  wav.wNumChannels = 1;
  wav.dwSampleRate = 48000;
  wav.dwBytesPerSec = 48000*2;
  wav.wBlockAlign = 4;
  wav.wBitsPerSample = 16;
  memcpy( wav.cData, "data", 4 );
  wav.dwDataLen = len;
  fseek( fp, 0, SEEK_SET );

  fwrite( &wav, 1, sizeof(wav), fp );
}


void write_resample_header( FILE *fp, DWORD len )
{
  WAVHDR wav;

  if ( !fp )
    return;

  memcpy( wav.riff, "RIFF", 4 );
  wav.len = len + 44 - 8;
  memcpy( wav.cWavFmt, "WAVEfmt ", 8 );
  wav.dwHdrLen = 16;
  wav.wFormat = 1;
  wav.wNumChannels = 1;
  wav.dwSampleRate = 44100;
  wav.dwBytesPerSec = 44100*2;
  wav.wBlockAlign = 2;  /* maybe should be 2? */
  wav.wBitsPerSample = 16;
  memcpy( wav.cData, "data", 4 );
  wav.dwDataLen = len;
  fseek( fp, 0, SEEK_SET );

  fwrite( &wav, 1, sizeof(wav), fp );
}


void write_wav_header( FILE *fp, DWORD len, int sr, int channels)
{
  WAVHDR wav;

  if ( !fp )
    return;

  memcpy( wav.riff, "RIFF", 4 );
  wav.len = len + 44 - 8;
  memcpy( wav.cWavFmt, "WAVEfmt ", 8 );
  wav.dwHdrLen = 16;
  wav.wFormat = 1;
  wav.wNumChannels = channels;
  wav.dwSampleRate = sr;
  wav.dwBytesPerSec = sr*2*channels;
  wav.wBlockAlign = 2*channels;
  wav.wBitsPerSample = 16;
  memcpy( wav.cData, "data", 4 );
  wav.dwDataLen = len;
  fseek( fp, 0, SEEK_SET );
  fwrite( &wav, 1, sizeof(wav), fp );
}



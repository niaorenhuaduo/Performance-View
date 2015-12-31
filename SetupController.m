//
//  SetupController.m
//  TestApp
//
//  Created by Christopher Raphael on 12/23/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import "SetupController.h"
#import "ChooseScoreController.h"
//#import "MyNSView.h"
#include "share.h"
#include "global.h"
#include "Player.h"
#include "MainController.h"
//#include "QuickStart.h"

@implementation SetupController
@synthesize SetupLabel;
@synthesize SetupWindow;







#define OPENING_CLIP_NAME "opening_clip"

void
decode_opening_clip_if_needed() {
  // AnsiString d,s;
  char source[500],dest[500];
  int len,channels;
  short *decoded;
  FILE *fp;

  strcpy(dest,user_dir);
    strcat(dest,MISC_DIR);
    strcat(dest,"/");
  strcat(dest,OPENING_CLIP_NAME);
  strcat(dest,".wav");
  fp = fopen(dest,"r");
  if (fp) { fclose(fp); return; }
  strcpy(source,user_dir);
  strcat(source,MISC_DIR);
  strcat(source,"/");
  strcat(source,OPENING_CLIP_NAME);
  strcat(source,".ogg");
  fp = fopen(source,"r");
  fclose(fp);
  if (fp == NULL) return;
  len = stb_vorbis_decode_filename(source, &channels, &decoded);
  write_wav(dest,decoded,len,channels);
	 
  

  /*  d = user_dir;
  d = d +  OPENING_CLIP_NAME + ".wav";
  if (FileExists(d)) return;
  s = MISC_DIR;
  s = s  + "/" + OPENING_CLIP_NAME + ".ogg";
  len = stb_vorbis_decode_filename(s.c_str(), &channels, &decoded);
  write_wav(d.c_str(),decoded,len,channels);*/
}



void
play_opening_clip(){
  char name[500],pref[500],source[500];
  FILE *fp;
  int f,play;
  //  AnsiString s;

  get_preference_file_name(pref);
  fp = fopen(pref,"r");
  if (fp) {
	fscanf(fp,"audio_intro = %d\n",&play);
	fclose(fp);
    if (play == 0) return;
  }

  decode_opening_clip_if_needed();
  strcpy(source,user_dir);
    strcat(source,MISC_DIR);
    strcat(source,"/");
  strcat(source,OPENING_CLIP_NAME);
  strcat(source,".wav");
    fp = fopen(source,"r");
    fclose(fp);
    if (fp == NULL) return;
  /*  s = user_dir;
  s = s + OPENING_CLIP_NAME + ".wav";
  f = read_wav(s.c_str(),orchdata); // number of frames */
  frames = read_wav(source,orchdata); // number of frames */
  mode = INTRO_CLIP_MODE;
    start_coreaudio_play();
  //  wave_prepare_playing(44100);
  playing_opening_clip = 1;
  //  OpeningPlay *OpeningPlay_thread = new OpeningPlay(false,f);
}



int
score_has_audio() {      // check for empty or missing .raw file.
    FILE *fp;
    int length;
    char path[500],name[500];
    
    strcpy(path,full_score_name);
    strcat(path,".raw");
    fp = fopen(path,"r");
    if (fp == NULL) return(0);
    fseek(fp,0,SEEK_END);
    length = ftell(fp);
    fclose(fp);
    return(length > 0);
}



#define PLAYER_STATE "player_state.dat"

int
get_player_piece(char *last_player, char *last_scoretag) {
    FILE *fp;
    char name[500];
    float t;
    
   
    last_player[0] = last_scoretag[0] = 0;
    strcpy(name,  user_dir);
    strcat(name, PLAYER_STATE); 
    fp = fopen(name,"r");
    if (fp == NULL) return(0);
    fscanf(fp,"%[^\n]",last_player);
    fscanf(fp,"%s",last_scoretag);
    fclose(fp);
    return(1);
}


static void
InitSetupLabel(char *ret) {
char label[500],last_player[500],last_scoretag[500],piece[500],file[500];
FILE *fp;
int i,l,live;

    strcpy(ret,"");
    //RestoreScore->Enabled = 0;
  if (get_player_piece(last_player,last_scoretag)) {
    strcpy(scoretag,last_scoretag);
    get_score_file_name(file,".nam");
      scoretag[0] = 0; // awkward kludge to undo the temp setting of this
    fp = fopen(file,"r");
    if (fp == NULL) return;
    fgets(piece,500,fp);
    fclose(fp);
    l = strlen(piece)-1;
    if (piece[l] == '\n')  piece[l] = 0;
	strcpy(label,last_player);
	strcat(label,", ");
    strcat(label,piece);
    strcpy(ret,label);
	//guisetup->Status->SimpleText = label;
    //	RestoreScore->Enabled = 1;
  }
}




- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    setup_self = (__bridge void *)(self);
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}

- (void) awakeFromNib {
    char text[500];
    
    InitSetupLabel(text);
    NSString *str = [[NSString alloc] initWithCString:text];
    [SetupLabel setStringValue:str];
    [SetupWindow makeKeyAndOrderFront:nil];
    NSLog(@"awak");
  }



/*- (IBAction)SetupPlayer:(id)sender {
    printf("got it\n");
} */


- (IBAction)SetupPlayer:(id)sender {
    static Player *controller = nil;
    
    
    playing_opening_clip = 0;
    performance_interrupted = 1;
    
    
    
    if ([[controller window] isVisible]) {
        [[controller window] makeKeyAndOrderFront:nil];
        return;  
    }
    controller =   [[Player alloc] initWithWindowNibName:@"Player"];
    [controller showWindow:nil];
    
  
}
 


- (IBAction)SetupScore:(id)sender {
   
  //  playing_opening_clip = 0;
    performance_interrupted = 1;
    if (strcmp(player,"") == 0) {
        [[NSAlert alertWithMessageText:@"Please choose player before selecting score"
                         defaultButton:@"OK"
                       alternateButton:nil 
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];
      return;
    }

    
  //  NSWindowController* controller =   [[NSWindowController alloc] initWithWindowNibName:@"Score"]; 
    ChooseScoreController* controller =   [[ChooseScoreController alloc] initWithWindowNibName:@"ChooseScore"];
    [controller showWindow:nil];
    
  /*  Player *cont =   [[Player alloc] initWithWindowNibName:@"Player"];
    [cont showWindow:nil]; */

 
}


-(void) finishSetup {
    [SetupWindow close];
    start_pos.num = -1; // meaning unset
    
    

  
    /*  commented out for compile
    [[[NSApplication sharedApplication] delegate] setStatusName];
    
    [[[NSApplication sharedApplication] delegate] setPieceName];
  //  [[[NSApplication sharedApplication] delegate] MenuDisplayPdf:nil];
    [[[NSApplication sharedApplication] delegate] haveScoreMenuConfig]; 
    if (score_has_audio() == 0) return;  // don't enable live panel if there is an empty or missing .raw file
    [[[NSApplication sharedApplication] delegate] set_focus_on_live_panel]; 
 //   [[[NSApplication sharedApplication] delegate] haveScoreMenuConfig]; 
    erase_spect();
   // warm_up_audio();
   // [NSThread detachNewThreadSelector:@selector(warmUpAudio:) toTarget:[SetupController class] withObject:nil];  // audio starts slowly the first time it is called, so get this out of the way.
     */
}



- (void) loadImage {
    NSSize size;
  //  RECT r1,r2;
    int last;
  //  int h,w;
    char name[500],staff_name[500];

    
  //  h =  [[[NSApplication sharedApplication] delegate] mainWindowHeight];
  //  w =  [[[NSApplication sharedApplication] delegate] mainWindowWidth];

  //  NSLog(@"%d",[view_self pixelsHigh]);
    
    
 //  [view_self showFirstPage];  commented out for compile 
    return;
 /*
    score_file("001.srg",staff_name);
    read_staff_ranges(staff_name);
    score_file("001.bmp", name);
   
    NSString *s = [[NSString alloc] initWithCString: name];
    NSImage *image = [[NSImage alloc] initWithContentsOfFile:s];
    size = [image size]; size.width *= .5; size.height *= .5;
        [image setSize: size];
    NSBitmapImageRep *pageBitmap =[[NSBitmapImageRep alloc] initWithCGImage:[image CGImageForProposedRect:NULL context:NULL hints:NULL]];
    
    r1.loci = r1.locj = r2.loci = r2.locj = 0;
    r2.width = spect_wd; r2.height = 512;
    r1.width = pageBitmap.pixelsWide; r1.height = r2.height*r1.width/r2.width;
    
    r1.width = pageBitmap.pixelsWide; r1.height = staff_range.range[0].hi;
    r2.width = spect_wd; r2.height = r1.height*r2.width/r1.width;
    last = staff_range.range[0].lo*r2.width/r1.width;
    [view_self copyBitmap: pageBitmap source:r1 dest:r2];
    r2.loci += r2.height;
    r2.height = 512-r2.loci;
    [view_self fillImageRect: r2 val:255];
    
    r1.loci = staff_range.range[1].lo;
    r1.height = staff_range.range[3].hi - staff_range.range[1].lo;
    r2.loci = r2.locj = 0;
    r2.width = spect_wd; r2.height = r1.height*r2.width/r1.width;
    [view_self copyBitmap: pageBitmap source:r1 dest:r2];
    
    r2.loci = r2.height;
    r2.height = last-r2.loci;
     [view_self fillImageRect: r2 val:0];
   */ 
    
}




- (IBAction)SetupRecall:(id)sender {
    playing_opening_clip = 0;
    performance_interrupted = 1;
    
    get_player_piece(player,scoretag);
    if (strlen(player) == 0) {
        modal_message(@"No previous score");
        return;
    }
    [SetupLabel setStringValue:@"reading score ..."];
    read_player_list();
    set_up_after_player();
    if (read_score_files() == 0) {
      [[NSAlert alertWithMessageText:@"Couldn't read score"
                       defaultButton:@"OK"
                       alternateButton:nil 
                        otherButton:nil
                        informativeTextWithFormat:@""] runModal];
      return;
    }
    gui_after_read_score();
    
#ifdef NOTATION
    [self loadImage];
#endif

    
    [self finishSetup];
}

-(BOOL) windowShowing {
    BOOL b = [SetupWindow isVisible];
    return [SetupWindow isVisible];
}


@end

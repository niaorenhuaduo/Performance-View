//
//  ChooseScoreController.m
//  Music+One
//
//  Created by Christopher Raphael on 2/22/12.
//  Copyright (c) 2012 Christopher Raphael. All rights reserved.
//

#import "ChooseScoreController.h"
#import "SetupController.h"
//#import "KeyVerifyController.h"
//#import "SetTempoController.h"
//#import "MyNSView.h"
#include "share.h"
#include "global.h"
#include "score_utils.h"
#include "dp.h"
#include <dirent.h>

void
open_and_show_pdf() {

  int ret;
  char var[500],name[500],mess[500];
  FILE *fp;

}


void
init_range() {
  /*  range->Edit1->Clear();
      range->Edit2->Clear();*/
}

void
get_title(char *name, char *title) {
  int i;

  title[0] = 0;
  for (i=0; i < pieces.num; i++) 
    if (strcmp(name,pieces.score[i].name) == 0) strcpy(title,pieces.score[i].title);
}


 int
set_scoretag(char *score) {
    int i,j;
    char title[400];
    
    
    /*  for (i=0; i < Scorex->ListBox1->Items->Count; i++)
     if (Scorex->ListBox1->Selected[i]) break;
     if (i == Scorex->ListBox1->Items->Count) return(0);
     strcpy(title,Scorex->ListBox1->Items->Strings[i].c_str());*/ 
 //   [ScoreNames selectedRowInColumn:0];
    
 /*   for (j=0; j < pieces.num; j++) {
        if (strcmp(get_score_title(j),title) == 0) break;
    }
    if (j == pieces.num) return(0);
    strcpy(scoretag,pieces.score[j].name);
    //  strcpy(score_title, pieces.score[j].title);
    return(1);*/
    
    if (scoretag == score) return(1);
     // this conditions occurs for unknown reasons and causes SIGABRT.  Luckily the scoretag is already set
     strcpy(scoretag,score);
    return(1);
}





@implementation ChooseScoreController
@synthesize DecompressLabel;
@synthesize ChooseScrWindow;
@synthesize ChooseScrBrowser;


static void *choose_score_self;

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    choose_score_self = (__bridge void *)(self);
    
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    [ChooseScrBrowser setDoubleAction:@selector(ChooseScoreButton:)];
    [ChooseScrBrowser setTarget:self];
    [self noDecodingMessage];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}



 void
add_status_text() {
  char text[500];

  sprintf(text,"%s on %s",player,score_title);

  /*  sprintf(text,"%s",player);
  Spect->Status->Panels->Items[0]->Text = text; //player;
  sprintf(text,"%s",score_title);
  Spect->Status->Panels->Items[1]->Text = text;
   Spect->Status->Panels->Items[2]->Text = "Meas:";
 Spect->Status->Panels->Items[3]->Text = "Take:";
 Spect->Status->Panels->Items[4]->Text = "Pos";
 Spect->Status->Panels->Items[5]->Text = "Frame:";*/

}


void
get_score_etc() {
 if (read_score_files() == 0) {
     [[NSAlert alertWithMessageText:@"Couldn't read score"
                      defaultButton:@"OK"
                    alternateButton:nil 
                        otherButton:nil
          informativeTextWithFormat:@""] runModal];
	  return;
	}
    

	gui_after_read_score();
	add_status_text();
 //   [set_tempo_self close];  // out for assembling Sanna's project
}

void erase_spect() {
  int width;
    
   // clear_spect();
   // [view_self clear_spect];  // out for assembling Sanna's project
  
  //TRect rect;

  //  width = Spect->ScrollBox1->ClientRect.Right;  /* this incudes container? */
  //  rect = TRect(0,0,width,SPECT_HT);
    //  Spect->Image1->Canvas->FillRect(rect);  /* restore background */
  frames=0;
}



void
set_focus_on_live_panel() {
  int live;


  /*  restore_live_state();
  focus_on_live = 1;
  Spect->MenuMicCheck->Enabled = true;
  enable_panel(Spect->NavigatePanel,false);
  enable_panel(Spect->LivePanel,true);
  ready_mix_panel();
  ready_live_panel();
  initial_alerts();
  set_toggle_arrow_toward_navigate_panel();*/

}


/*unsigned int
str2code(char *s) {
  unsigned int i,t=0,d;

  for (i=0; i < strlen(s); i++) {
    d = s[i];
    d *= 137603921;
    t = t^d;
  }
  return(t);
  }*/



static int 
is_free_piece(char *tag) {
  char name[500];
  unsigned code,key;
  FILE *fp;

  get_score_file_name(name, ".key");  // the arguments were backwards in the windows version,  this might be a bug there ...
  fp = fopen(name,"r");
  if (fp == NULL) return(0);
  fscanf(fp,"%X",&key);
  fclose(fp);
  code = str2code(tag);
  /*  fp = fopen(name,"w");
  fprintf(fp,"%X\n",code);
  fclose(fp);*/
  sprintf(name,"code = %X\n",code);
  return(key == code);
}





static void
add_pieces_to_verified(char *tag, int is_full_piece) {
  FILE *fp;
  char verify[1000],piece[500],test[500],path[500];
  unsigned code;
  struct dirent *drnt;
  DIR *dir;
    
  //  AnsiString path,name;
  //  TSearchRec F;
  int i;

  if (strcmp(tag,PROGRAM_KEY) == 0) {add_piece_to_verified(tag); return; }  // just add key for the program
  if (is_full_piece == 0) {add_piece_to_verified(tag); return; }  // just add single movment to verified list


    strcpy(path,user_dir);
    strcat(path, SCORE_DIR);
    strcat(path, "/");
    strcpy(piece,tag); // non destructive for input
    strip_mvmt(piece);
    dir = opendir(path);
    while ((drnt=readdir(dir)) != NULL) {
      strcpy(test,drnt->d_name);
        for (i=0; i < strlen(piece); i++) if (piece[i] != test[i]) break;
        if (i == strlen(piece)) {  // add all pieces with prefix of "piece"
            add_piece_to_verified(test);
            printf("adding %s to verified\n",test);
        }
    }
}



void
score_file(char *suff, char *dest) {

  strcpy(dest,user_dir);
  strcat(dest,SCORE_DIR);
  strcat(dest,"/");
  strcat(dest,scoretag);
  strcat(dest,"/");
  strcat(dest,scoretag);
  strcat(dest,".");
  strcat(dest,suff);
}

-(void) decodingMessage {
    [DecompressLabel setStringValue:@"Decompressing audio ..."];
    [DecompressLabel displayIfNeeded];
}

-(void) noDecodingMessage {
    [DecompressLabel setStringValue:@""];
    [DecompressLabel displayIfNeeded];
}

static int
convert_oggvorbis() {
  char ogg[500],raw[500];
  FILE *fp;
  short *decoded;
  int channels, len;

  
  /*  message->Label1->Caption = "Decompressing audio ...";
  message->Show();
  message->Refresh(); */
    [choose_score_self decodingMessage];
    score_file("ogg",ogg);
  fp = fopen(ogg,"rb");
  if (fp == NULL) return(0);
  len = stb_vorbis_decode_filename(ogg, &channels, &decoded);
  fclose(fp);
  if (len == 0) return(0);
  score_file("raw",raw);
  fp = fopen(raw,"wb");
  fwrite(decoded, 2, len*channels, fp);
  fclose(fp);
  /*  message->Close(); */
  /*  Spect->Refresh();  // hoping to avoid hole in the main window */
  return(1);
}



int
my_choose_score(char *score_name) {
    int i,j,vcode_exists=0,audio_exists=0,res;
    char title[500],lab[500],score[500],audio[500],times[500],dir[500],tap[500];
    float lt;
    
    if (set_scoretag(score_name) == 0) return(0);
    

    
#ifdef DISTRIBUTION
#ifdef COPY_PROTECTION
    if (key_verification() == 0) return(0);
#endif
#endif
    
    audio_exists =  check_for_mmo_data(scoretag,audio);
    vcode_exists =  check_for_voc_data();
/*#ifdef DISTRIBUTION*/
    if (!audio_exists) {
        if (convert_oggvorbis() == 0) {
            //	  message->Label1->Caption = "Couldn't find Ogg Vorbis data";
            //	  message->Show();
            return(0);
        }
    }
/*#else
    if (midi_accomp == 0) {
        audio_exists =  check_for_mmo_data(scoretag,audio);
        vcode_exists =  check_for_voc_data();
       if (!audio_exists && !vcode_exists)  return(0);
    }
#endif*/
    //	Scorex->StatusBar1->SimplePanel = true;
    //	Scorex->StatusBar1->SimpleText = "reading score ...";
    
	get_score_etc();
    //	Scorex->Close();
	frames = 0;   // navigate actions should be disabled
//	erase_spect();
	set_focus_on_live_panel();
	if (midi_accomp == 0) open_and_show_pdf();   // midi_accomp means Jan Beran for now
    return(1);
}


void
modal_message(NSString *s) {
[[NSAlert alertWithMessageText:s
                 defaultButton:@"OK"
               alternateButton:nil 
                   otherButton:nil
     informativeTextWithFormat:@""] runModal];
}



/*- (IBAction)ChooseScrBut:(id)sender {
    int k = [ChooseScrBrowser selectedRowInColumn:0];
    if (k == -1) {
        modal_message(@"Please select piece");
        return;
    }
    my_choose_score(pieces.score[k].name);
    [ChooseScrWindow close];
    [setup_self finishSetup];
}*/








- (IBAction)ChooseScoreButton:(id)sender {
    int k = [ChooseScrBrowser selectedRowInColumn:0];
    char *tag;

    if (k == -1) {
        modal_message(@"Please select piece");
        return;
    }
    tag = pieces.score[k].name;
   /* if (strcmp(tag,TUTORIAL_PIECE) != 0 && already_verified(tag) == 0) {  // can't find this piece in list of legitimately verified pieces
      modal_message(@"Can't load piece");
      return;
    } */

    my_choose_score(tag);
 
    [ChooseScrWindow close];
    [setup_self finishSetup];
}
@end

//
//  ReadAudioController.m
//  TestApp
//
//  Created by Christopher Raphael on 12/24/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import "ReadAudioController.h"
#include "read_audio_utils.h"
#include "MyNSView.h"
#include "share.h"
#include "global.h"
#include <sys/stat.h>


@implementation ReadAudioController
@synthesize ReadAudioTableView;
@synthesize ReadAudioWindow;


- (id)initWithWindow:(NSWindow *)window
{
   
    read_audio_self = self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (void)windowDidLoad
{
    char name[500];
    
    [super windowDidLoad];
    scoretag2title(scoretag,name);
    [_PieceLabelOutlet setStringValue: [[NSString alloc] initWithCString: name]];
     [_NewReadAudioWindow makeKeyAndOrderFront:nil];
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}


- (void) TableDoubleClick {
    NSLog(@"double click");
}

- (void) awakeFromNib {
    int i;
    get_audio_file_data();
   
  
    NSArray *columnArray;
    columnArray = [ReadAudioTableView tableColumns];
//    NSTableColumn *column;
  //  column = [columnArray objectAtIndex:0];
//    [column setIdentifier:@"num"];
    [[columnArray objectAtIndex:0] setIdentifier:@"num"];
    [[columnArray objectAtIndex:1] setIdentifier:@"date"];
    [[columnArray objectAtIndex:2] setIdentifier:@"start"];
    [[columnArray objectAtIndex:3] setIdentifier:@"end"];
    
    [[columnArray objectAtIndex:4] setIdentifier:@"correct"];
    [[columnArray objectAtIndex:5] setIdentifier:@"train"];
  
    for (i=0; i < 6; i++)
        [[columnArray objectAtIndex:i] setEditable:NO];

    
    columnArray = [_ReadAudioNewTable tableColumns];
    [[columnArray objectAtIndex:0] setIdentifier:@"date"];
    [[columnArray objectAtIndex:1] setIdentifier:@"num"];
    
    [[columnArray objectAtIndex:2] setIdentifier:@"range"];
    
    [[columnArray objectAtIndex:3] setIdentifier:@"correct"];
    [[columnArray objectAtIndex:4] setIdentifier:@"train"];
    
    
    

    
    for (i=0; i < 5; i++)
        [[columnArray objectAtIndex:i] setEditable:NO];
    
    
    
    
    
    [ReadAudioWindow  makeKeyAndOrderFront:nil];  // set focus on new window
   
    
    [ReadAudioTableView setDoubleAction:@selector(ViewDoubleClick:)];
    [ReadAudioTableView setTarget:self];
    
    [_NewReadAudioTableViewOutlet setDoubleAction:@selector(ViewDoubleClick:)];
    [_NewReadAudioTableViewOutlet setTarget:self];
   
    NSLog(@"awoke Read audio");
}
 

static int 
file_exists(char *file) {
   struct stat statinfo; 
    
  return(stat(file,&statinfo) >= 0);
}





static void
decode_if_needed() {
    char ifile[500],ofile[500];
    FILE *fp;
    short *decoded;
    int channels, len;
   
    
    strcpy(ofile,audio_data_dir);
    
    strcat(ofile,current_examp);
    strcat(ofile,".48k");
    strcpy(ifile,ofile);
    strcat(ifile,".ogg");
    if (file_exists(ifile) == 0) return;
    if (file_exists(ofile)) return;
    
 //   message->Label1->Caption = "Decompressing audio ...";
  //  message->Show();
  //  message->Refresh();
    len = stb_vorbis_decode_filename(ifile, &channels, &decoded);
    fp = fopen(ofile,"wb");
    fwrite(decoded, 2, len*channels, fp);
    fclose(fp);
    
    strcpy(ofile,audio_data_dir);
    strcat(ofile,current_examp);
    strcat(ofile,".48o");
    strcpy(ifile,ofile);
    strcat(ifile,".ogg");
    if (file_exists(ifile) == 0) return;
    if (file_exists(ofile)) return;
    len = stb_vorbis_decode_filename(ifile, &channels, &decoded);
    fp = fopen(ofile,"wb");
    fwrite(decoded, 2, len*channels, fp);
    fclose(fp);
 //   message->Close();
    
    
}





void
display_take_string() {
    char *ptr;
    
   // ptr =	AnsiStrPos(current_examp,".") + 1;
    ptr = current_examp + strlen(current_examp) - 3;
    [[[NSApplication sharedApplication] delegate] setStatTake:ptr];

    //Spect->Status->Panels->Items[3]->Text = ptr;      // the Take
}







void
display_range_string() {
    char lab[500];
    
    if (start_pos.num == -1)  strcpy(lab,"Meas:");
    else make_range_string(lab);
    [[[NSApplication sharedApplication] delegate] setStatRange:lab];
    //   Spect->Status->Panels->Items[2]->Text = lab;     // range
}





- (IBAction)ViewDoubleClick:(id)sender {
    //[self ReadAudioOk:sender];
    [self RefineAndTrainAction:sender];
    NSLog(@"double test");
}

- (IBAction)ReadAudioOk:(id)sender {
    int i;
    char *num;
    
    current_examp[0] = 0;
    i = [ReadAudioTableView selectedRow];
    if (i < 0 || i >= audio_files.num) return;
    num = audio_files.list[i].num;
    strcpy(current_examp,scoretag);  
    strcat(current_examp,".");  
    strcat(current_examp,num);
    printf("current_ex = %s\n",current_examp);
    decode_if_needed();
    if (read_audio_indep() == 0) {
      [[NSAlert alertWithMessageText:@"bad format in parse file"
         defaultButton:@"OK"
         alternateButton:nil 
         otherButton:nil
         informativeTextWithFormat:@""] runModal];
      return;
    }
    [ReadAudioWindow close];
     [_NewReadAudioWindow close];
    [view_self displaySpect];
    

  //  ChooseAudio->ModalResult = 1;
   display_range_string();
  display_take_string();
     [[[NSApplication sharedApplication] delegate] displayTitleString];
   //set_focus_on_navigate_panel();
    [[[NSApplication sharedApplication] delegate] set_focus_on_navigate_panel];
        
}

- (IBAction)ReadAudioCancel:(id)sender {
    [ReadAudioWindow close];
}

- (IBAction)ReadAudioQuit:(id)sender {
    [ReadAudioWindow close];
}


- (IBAction)RefineAndTrainAction:(id)sender {
    int i;
    char *num;
    
    current_examp[0] = 0;
    i = [_ReadAudioNewTable selectedRow];
    if (i < 0 || i >= audio_files.num) return;
    num = audio_files.list[i].num;
    strcpy(current_examp,scoretag);
    strcat(current_examp,".");
    strcat(current_examp,num);
    printf("current_ex = %s\n",current_examp);
    decode_if_needed();
    if (read_audio_indep() == 0) {
        [[NSAlert alertWithMessageText:@"bad format in parse file"
                         defaultButton:@"OK"
                       alternateButton:nil
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];
        return;
    }
    [_NewReadAudioWindow close];
    [view_self displaySpect];
    
    resynth_solo(48000);
    display_range_string();
    display_take_string();
    
    [[[NSApplication sharedApplication] delegate] showMainWindow];
   
     [[[NSApplication sharedApplication] delegate] displayTitleString];
    [[[NSApplication sharedApplication] delegate] set_focus_on_navigate_panel];
    

}
@end

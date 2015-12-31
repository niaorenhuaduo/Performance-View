//
//  Player.m
//  TestApp
//
//  Created by Christopher Raphael on 12/17/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import "Player.h"
#import "NameBrowser.h"
#include "share.h"
#include "global.h"

@implementation Player
@synthesize Instrument;
@synthesize PlayerNames;
@synthesize PlayerWind;
@synthesize choosePlayerWindow;
@synthesize NewPlayer;


#define PLAYER_FILE "player"  // this duplicates some identical define ... fix this

int
write_player_list() {
    char file[500];
    FILE *fp;
    PLAYER_INFO *pi;
    int i;
    
    //  strcpy(file,MISC_DIR);                                                     
    strcpy(file,user_dir);
    // strcat(file,AUDIO_DIR);                                                      
    //  strcat(file,"/");                                                            
    strcat(file,PLAYER_FILE);
    fp = fopen(file,"w");
    if (fp == 0) return(0);
    for (i=0; i < player_list.num; i++) {
        pi = &player_list.player[i];
        fprintf(fp, "%s, %s,\n",pi->person,pi->inst);
    }
    fclose(fp);
    return(1);
}


static int
player_el_comp(const void *n1, const void *n2) {
    PLAYER_INFO *p1,*p2;
    int c1,c2,r;
    
    p1 = (PLAYER_INFO *) n1;
    p2 = (PLAYER_INFO *) n2;
    return(strcmp(p1->person,p2->person));
}



static void
add_player_list(char *play, char *inst) {
    int i;
    
    if (player_list.num == MAX_PLAYERS) {
        printf("out of room in player list\n");
        return;
    }
    i = player_list.num++;
    strcpy(player_list.player[i].person,play);
    strcpy(player_list.player[i].inst,inst);
    qsort(player_list.player,player_list.num,sizeof(PLAYER_INFO),
          player_el_comp);
}



- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    [PlayerNames setDoubleAction:@selector(PlayerDoubleClick:)];
    [PlayerNames setTarget:self];

    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}

-(void) PlayerDoubleClick:(id) sender {
    [self WindowBut:sender];
}

- (IBAction)WindowBut:(id)sender {
  //  [Browser reloadColumn:0];
//   int k =  [Brow numberofRowsinColumn:0];
 //  [Brow SetTitle:@"aaa" ofColumn:0];
    NSLog(@"hello"); 
 //   [TextBox setStringValue:@"hello wold"];
   int k =  [PlayerNames selectedRowInColumn:0];
    if (k == -1) {
        [[NSAlert alertWithMessageText:@"Please select player"
                         defaultButton:@"Ok"
                    alternateButton:nil 
                          otherButton:nil
             informativeTextWithFormat:@""] runModal];
        return;
    }
    printf("k = %d\n",k);
    strcpy(player,player_list.player[k].person);
    [PlayerWind close];
    
    
    
    if (set_up_after_player() == 0) {
        [[NSAlert alertWithMessageText:@"Couldn't initialize player"
                         defaultButton:@"Ok"
                       alternateButton:nil 
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];
        player[0] = 0;
        return;
    }

  // [super close];
    
}

- (IBAction)AddPlayer:(id)sender {
    char *name,*inst;
    int i;
    
    NSString *str = [NewPlayer stringValue];
    if (str.length == 0) {
        [[NSAlert alertWithMessageText:@"Please enter player name"
                         defaultButton:@"Ok"
                       alternateButton:nil 
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];  
        return;
    }
    name = [str cStringUsingEncoding:NSMacOSRomanStringEncoding];
    for (i=0; i < player_list.num; i++)
        if (strcmp(name,player_list.player[i].person) == 0) break;
    if (i < player_list.num) {
        [[NSAlert alertWithMessageText:@"Cannot have duplicate player names"
                         defaultButton:@"Ok"
                       alternateButton:nil 
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];
        return;
    }
    i = [Instrument selectedRowInColumn:0];
    if (i == -1) {
        [[NSAlert alertWithMessageText:@"Please select instrument"
                         defaultButton:@"Ok"
                       alternateButton:nil 
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];
        return;
    }
    //inst = player_list.player[i].inst;
    inst = instrument_list.instrument[i];
    add_player_list(name,inst);
    write_player_list();
    [PlayerNames reloadColumn:0];
    for (i=0; i < player_list.num; i++)
        if (strcmp(player_list.player[i].person,name) == 0) break;
    [PlayerNames selectRow:i inColumn:0];
}


@end

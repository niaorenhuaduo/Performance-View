//
//  AppDelegate.m
//  Performance View
//
//  Created by craphael on 11/28/15.
//  Copyright (c) 2015 craphael. All rights reserved.
//

#import "AppDelegate.h"
#import "MainController.h"
#import "SetupController.h"
#import "ReadAudioController.h"
#import "PatControl.h"
#import "MyNSView.h"
#include "share.h"
#include "global.h"
#include "Resynthesis.h"
#include "vocoder.h"

INC_VALS inc_vals;

@implementation AppDelegate

//@synthesize SpectWind = _SpectWind;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
      //the only two global variable taking path of interest
    strcpy(user_dir,"/Users/apple/Documents/Performance-View/user/"); //???moved this higher, it came after setIncButtons
    
    maininit();
    [self setIncButtons];

    
    [self windowDidResize:nil];
    // MainController* controller =   [[MainController alloc] initWithWindowNibName:@"MainMenu"];
   // [controller showWindow:nil];
    SetupController* controller =   [[SetupController alloc] initWithWindowNibName:@"Setup"];
    [controller showWindow:nil]; 
    
  /*  SetupController* controller =   [[SetupController alloc] initWithWindowNibName:@"Setup"];
    [controller showWindow:nil]; */
    // Insert code here to initialize your application
}





- (IBAction)closeWindow:(id)sender {
    
    
    NSWindow *w = [NSApp mainWindow];
    [w close];
  //  [[rehearsal_self ReviewButtonOutlet] setEnabled:(take_range.frames>0)];  // comment for compile
    
}


- (IBAction)MenuRehearsalAction:(id)sender {
  //  [quick_start_self StartAction:nil]; // comment for compile
}


- (IBAction)SpectPlayToggle:(id)sender {
    if ([sender state] == NSOffState) [self StopPlayingAction:nil];
    //[[[NSApplication sharedApplication] delegate] LiveStop:nil];
    else [self StartPlayingAction:nil];
    //[[[NSApplication sharedApplication] delegate] LiveStart:nil];
}

- (IBAction)MenuLiveOptions:(id)sender {
   /* static LiveOptionsController *controller = nil;
    
    if ([[controller window] isVisible]) {
        [[controller window] makeKeyAndOrderFront:nil];
        return;
    }
    controller =   [[LiveOptionsController alloc] initWithWindowNibName:@"LiveOptions"];
    [controller showWindow:nil];
    */
 
}

- (IBAction)MenuResetTraining:(id)sender {
/*    static ResetTrainingController *controller = nil;
    
    if ([[controller window] isVisible]) {
        [[controller window] makeKeyAndOrderFront:nil];
        return;
    }
    controller =   [[ResetTrainingController alloc] initWithWindowNibName:@"ResetTraining"];
    [controller showWindow:nil]; */
    
}


- (IBAction)MenuLoadTake:(id)sender {
   // static ReadAudioController *controller = nil;
    /* to make this work click on files owner cube and look for
     the window outlet (little circle) of the connects in the
     right panel.  drag that circle to the window */
    
  /*  if (current_parse_changed_by_hand && [_MainWindowOutlet isVisible]) [self save_before_closing];
    
    if ([[controller ReadAudioWindowOutlet] isVisible]) {
        [[controller ReadAudioWindowOutlet] makeKeyAndOrderFront:nil];
        return;
    }
    controller =   [[ReadAudioController alloc] initWithWindowNibName:@"NewReadAudio"];
    // controller =   [[ReadAudioController alloc] initWithWindowNibName:@"ReadAudio"];
    [controller showWindow:nil];
    [[controller window] makeKeyAndOrderFront:nil]; */
    
    
    
}


- (IBAction)DeleteRefinementAction:(id)sender {
    
    [self MenuDeleteTake:nil];
}

- (IBAction)ExportAudioAction:(id)sender {
    [self MenuOutputWave:nil];
}

- (IBAction)SaveRefinementAction:(id)sender {
    //[self MenuSaveTake:nil];
    //if (check_parse() == 0) return;
    write_current_parse();
}

- (IBAction)MinimizeAction:(id)sender {
    [[NSApp mainWindow] orderOut:nil];
}


- (IBAction)ZoomCurrentWindow:(id)sender {
    NSWindow *w = [NSApp mainWindow];
    
    if ([[w title] isEqualToString:@"Refine & Train"] == false) return;
    
    NSRect r = [[NSScreen mainScreen] visibleFrame];
    NSRect cur  = [w frame];
    r.size.height = cur.size.height;
    [w setFrame:r display:YES];
    
}

- (IBAction)BringAllToFront:(id)sender {
    NSWindow *w;
    NSArray *ar = [NSApp windows];
    for (int i=0; i < [ar count]; i++) {
        w = [ar objectAtIndex:i];
        if ([w isVisible] == NO) continue;
        [w orderFront:nil];
        
    }
}

- (IBAction)HideOthersAction:(id)sender {
    [[NSWorkspace sharedWorkspace] performSelectorOnMainThread:@selector(hideOtherApplications) withObject:NULL waitUntilDone:NO];
}

- (IBAction)MenuExit:(id)sender {
    char message[500];
    //  shut_down();  // this was commented out in windows version
    
    
    performance_interrupted = 1;
    [self quitProgram];
    //   [self save_before_closing];
    /* [_window close];
     exit(0);*/
}

- (IBAction)HideApplication:(id)sender {
    [[NSRunningApplication currentApplication] hide];
}

- (IBAction)StartupAction:(id)sender {
    [self StartupWindow:nil];
    //[self RehearsalPanel];
}

- (IBAction)MarkerCheckAction:(id)sender {
    
    if (frames == 0) return;
    clicks_in  = [sender state];
    [self enableMarkerControls];
    show_rests = 1;
    // [self ready_mix_panel];
    
    
    //   init_visible();  need this!  ???
    
    [view_self markersChange];  
    
}

- (IBAction)SimpleMarkVolAction:(id)sender {
    if( [[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask )
        [sender setFloatValue:  ([sender minValue] + [sender maxValue])/2]; //NSLog(@"option click");
    
    simple_mix[2].volume = [sender floatValue]/100.;
}

- (IBAction)SimpleMarkBalAction:(id)sender {
    if( [[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask )
        [sender setFloatValue:  ([sender minValue] + [sender maxValue])/2]; //NSLog(@"option click");
    
    simple_mix[2].pan = [sender floatValue]/100.;
}

- (IBAction)SimpleMarkMuteAction:(id)sender {
    simple_mix[2].mute = [sender state];
}

- (IBAction)SimpleSoloVolAction:(id)sender {
    //NSLog(@"simple solo vol action %f",[sender floatValue]);
    if( [[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask )
        [sender setFloatValue:  ([sender minValue] + [sender maxValue])/2]; //NSLog(@"option click");
    
    simple_mix[0].volume = [sender floatValue]/100.;
}

- (IBAction)SimpleSoloBalAction:(id)sender {
    if( [[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask )
        [sender setFloatValue:  ([sender minValue] + [sender maxValue])/2]; //NSLog(@"option click");
    
    simple_mix[0].pan = [sender floatValue]/100.;
}

- (IBAction)SimpleSoloMuteAction:(id)sender {
    simple_mix[0].mute = [sender state];
}

- (IBAction)SimpleAccMuteAction:(id)sender {
    simple_mix[1].mute = [sender state];
}

- (IBAction)SimpleAccBalAction:(id)sender {
    if( [[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask )
        [sender setFloatValue:  ([sender minValue] + [sender maxValue])/2]; //NSLog(@"option click");
    
    simple_mix[1].pan = [sender floatValue]/100.;
}

- (IBAction)SimpleAccVolAction:(id)sender {
    if( [[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask )
        [sender setFloatValue:  ([sender minValue] + [sender maxValue])/2]; //NSLog(@"option click");
    
    simple_mix[1].volume = [sender floatValue]/100.;
}

- (IBAction)RefinePrevButtonAction:(id)sender {
    [self NavBackwardButton:NULL];
}

- (IBAction)RefineNextButtonAction:(id)sender {
    [self NavForwardButton:NULL];
}

- (IBAction)RefineUndoAction:(id)sender {
    [self NavUndoButton:NULL];
    
}

- (IBAction)RefineAlignAction:(id)sender {
    [self MenuRealign:NULL];
    
}

- (IBAction)RefineNudgeRightAction:(id)sender {
    [self NavRiteButton:NULL];
}

- (IBAction)RefineNudgeLeftAction:(id)sender {
    
    [self NavLeftButton:NULL];
}

- (IBAction)NoteMarksSoundMenuChangeAction:(id)sender {
    int i = [sender indexOfSelectedItem];
    use_pluck = i;
    
}


-(void) show_increment {
    char s[500];
    
    /* if (spect_inc.num == 0) strcpy(s,"showing all notes");
     else if (spect_inc.den == 1) strcpy(s,"showing barlines");
     else sprintf(s,"showing multiples of %d/%d",spect_inc.num,spect_inc.den);
     [IncLabel setStringValue: [[NSString alloc] initWithCString:s]];*/
    //  SetInc->IncLabel->Caption = s;
    [view_self redraw_markers];
    // redraw_markers();
}


-(void) set_inc_display:(int) i {
  //  extern  INC_VALS inc_vals;
    
    spect_inc =  inc_vals.inc[i];
    [self show_increment];
}


- (IBAction)NoteMarksIncMenuChangeAction:(id)sender {
    int i = [sender indexOfSelectedItem];
    if (frames == 0) return;
    [self set_inc_display:i];
}


- (IBAction)ReadAudio:(id)sender {
    ReadAudioController* controller =   [[ReadAudioController alloc] initWithWindowNibName:@"NewReadAudio"];
    [controller showWindow:nil];
   
}

-(int) backgroundTask: (void *) func Text: (char *) str Indeterminate: (int) indet {
    background_info.function = func;
    background_info.label = str;
    background_info.indeterminate = indet;
    PatControl* controller = [[PatControl alloc] initWithWindowNibName:@"Pat"];
    [controller showWindow:nil];
    [pat_self prepare];
    
    
    [controller beginWaiting];
    return (background_info.ret_val);
}


- (void) enableMarkerControls {
    [_AlignMarkersOutlet setEnabled: clicks_in];
    [_RefineNudgeLeftOutlet setEnabled: clicks_in];
    [_RefineNudgeRightOutlet setEnabled: clicks_in];
    [_RefineUndoOutlet setEnabled: clicks_in];
    
}

-(void)setStatRange:(char *) meas {
    [_StatusMeasRange setStringValue: [[NSString alloc] initWithCString:meas]];
}

-(void)setStatTake:(char *) take {
    [_StatusTake setStringValue: [[NSString alloc] initWithCString:take]];
}


#define SIDE_MARGIN 0 //10 //60
#define TOP_MARGIN 22 //50
#define BOT_MARGIN  275 //230 // room for all the controls (must have this)

-(void) windowDidResize:(id) sender {
    NSRect spect,wind;
    
    
   
    wind = [_window frame];
     spect = view_self.frame;  // made this substitution 11-15
    
    
    
    
   // spect = [_SpectWind frame];
    spect.size.height = wind.size.height - BOT_MARGIN - TOP_MARGIN;
   // spect.size.height = 512;
    if (spect.size.height > 512) spect.size.height = 512;
    spect.origin.x = SIDE_MARGIN; spect.origin.y = BOT_MARGIN;
    spect.origin.y  = wind.size.height - spect.size.height - TOP_MARGIN;
    
    spect.size.width = wind.size.width-2*SIDE_MARGIN;
   // [_SpectWind setFrame:spect];
    [view_self setFrameSize:spect.size];
    [view_self setFrameOrigin:spect.origin];
    
  //  spect = [_SpectWind frame];
     spect = view_self.frame;
    spect_wd = spect.size.width;
    /* in case the width is not really set due to some constraints I don't understand,
     read the gui width again and set the spect_wd to that  */
}


#define MAIN_WIND_HEIGHT 805
#define MAIN_WIND_WIDTH 1232

-(void) showMainWindow {
    static MainController *controller = nil;
    
    
    
    if (![_MainWindowOutlet isVisible]) {
        
        // controller =   [[MainController alloc] initWithWindowNibName:@"MainMenu"];
        
        NSRect rect = [_MainWindowOutlet frame];
        rect.size.width = MAIN_WIND_WIDTH;
        rect.size.height = MAIN_WIND_HEIGHT;
        [_MainWindowOutlet setFrame:rect display:YES];
        
        [self windowDidResize:nil];
        /*controller = [[NSApplication sharedApplication] delegate];
         [controller showWindow:nil];*/
    }
    [_MainWindowOutlet makeKeyAndOrderFront:nil];
   // [[rehearsal_self ReviewButtonOutlet] setEnabled:NO];
    
    
    return;
    
    
    
    
    [_MainWindowOutlet makeKeyAndOrderFront:nil];
}



- (void) displayTitleString {
    char title[500], *ptr;
    
    ptr = current_examp + strlen(current_examp) - 3;
    scoretag2title(scoretag,title);
    int n = strlen(title);
    if (title[n-1] == '\r') title[n-1] = 0; // don't know why the Saint Saens tutorial has stray \r in title
    strcat(title,", take ");
    strcat(title,ptr);
    [_PieceTitleOutlet setStringValue: [[NSString alloc] initWithCString:title]];
    
}

-(void) set_focus_on_live_panel {
    int live;
    
    restore_live_state();
    focus_on_live = 1;
    /* Spect->MenuMicCheck->Enabled = true;  will need this */
    [self enablePanel:"navigate" val:NO];
    [self enablePanel:"live" val:YES];
    [self ready_mix_panel];
    [self ready_live_panel];
    [view_self clear_spect];
    [self audioUnloadedMenuConfig];
    
    
    
    
    // set_toggle_arrow_toward_navigate_panel();  // this too
    
}


- (IBAction)AccompRadio:(id)sender {
    NSString *label = [[sender selectedCell] title];
    ensemble_in = [label isEqualToString:@"In"];
    // ensemble_in = 1-AccompRadioGroup->ItemIndex;
    [self ready_mix_panel];
}


audio_mix_num() {
    if (focus_on_live) return(0);
    else if (ensemble_in) return(1);
    else if (ensemble_in == 0 && clicks_in) return(2);
    else if (ensemble_in == 0 && clicks_in == 0) return(3);
    else { printf("don't understand which mix\n"); exit(0); }
}

#define MAXIMUM_VOLUME 4. //2. //3. //1.3  // this can lead to clipping if > 1

-(void) refresh_mix {  // need this eventually
    float mxvol = [_VolumeSliderOutlet maxValue];
    [_VolumeSliderOutlet setFloatValue:master_volume*mxvol/MAXIMUM_VOLUME];
    float mxpan = [_PanSliderOutlet maxValue];
    [_PanSliderOutlet  setFloatValue:pan_value*mxpan];
    float mxbal = [_BalanceSliderOutlet maxValue];
    [_BalanceSliderOutlet setFloatValue:mixlev*mxbal];
    
    /* Spect->VolMix->Position = (int) (master_volume*Spect->VolMix->Max/MAXIMUM_VOLUME);
     Spect->MixCenter->Position = (int) (pan_value*Spect->MixCenter->Max);
     Spect->MixBal->Position  = (int) (mixlev*Spect->MixCenter->Max); */
}

-(void) ready_mix_panel {
    int m;
    
    [self enablePanel:"mix" val:YES];
    if (focus_on_live || (clicks_in == 0 && ensemble_in == 0)) {  // this all will belong eventually
        
        float mxbal = [_BalanceSliderOutlet maxValue];
        [_BalanceSliderOutlet setFloatValue:mxbal/2];
        [_BalanceSliderOutlet setEnabled:NO];
        float mxpan = [_PanSliderOutlet maxValue];
        [_PanSliderOutlet setFloatValue:mxpan/2];
        [_PanSliderOutlet setEnabled:NO];
        
        /*Spect->MixBal->Position = Spect->MixBal->Max/2;
         Spect->MixBal->Enabled = false;
         Spect->Balance->Enabled = false;
         Spect->MixCenter->Position = Spect->MixCenter->Max/2;
         Spect->MixCenter->Enabled = false;
         Spect->Center->Enabled = false;*/
    }
    BOOL b = (focus_on_live) ? NO : YES;
    // if (focus_on_live) {
    [_MarkersRadioOutlet setEnabled:b];
    [_AccompRadioOutlet setEnabled:b];
	/*Spect->MarkersRadioGroup->Enabled = false;
     Spect->AccompRadioGroup->Enabled = false;*/
    // }
    if (focus_on_live) [_MixerBox  setTitle:@"Output Mix of accomp/accomp"];
    else if (ensemble_in ==0 && clicks_in==0)  [_MixerBox setTitle:@"Output Mix of solo/solo"];
    else if (ensemble_in ==0 && clicks_in)  [_MixerBox setTitle:@"Output Mix of solo/note markers"];
    else if (ensemble_in) [_MixerBox setTitle:@"Output Mix of solo/accomp"];
    /*  if (focus_on_live)   Spect->MixDesc->Caption = "Output Mix of accomp/accomp";
     else if (ensemble_in ==0 && clicks_in==0)  Spect->MixDesc->Caption = "Output Mix of solo/solo";
     else if (ensemble_in ==0 && clicks_in)  Spect->MixDesc->Caption = "Output Mix of solo/note markers";
     else if (ensemble_in) Spect->MixDesc->Caption = "Output Mix of solo/accomp"; */
    m = audio_mix_num();
    master_volume = audio_mix[m].volume;
    pan_value = audio_mix[m].pan;
    mixlev = audio_mix[m].balance;
    
    //  [_AccompRadioOutlet setIntValue:2/*ensemble_in*/];
 //   [_AccompRadio selectCellAtRow:((ensemble_in) ? 0 : 1) column:0];
    /* not sure what this is trying to do so skipping */
    [_MarkersRadioOutlet selectCellAtRow: (clicks_in) ? (show_rests) ? 2 : 0 : 1 column:0];
    // refresh_mix();
    [self refresh_mix];
}


-(void)setStatusPos:(char *) meas {
    [_StatusPosition setStringValue: [[NSString alloc] initWithCString:meas]];
}


-(void) enableSpectControls:(bool) b {
    int i=0;
    for (NSView *v in [[_MainWindowOutlet contentView] subviews]) {
        if( [v respondsToSelector:@selector(setEnabled:)] )
        {
            [(NSControl*)v setEnabled:b];
        }
        /* for (NSView *vv in [v subviews]) {
         if( [vv respondsToSelector:@selector(setEnabled:)] )
         {
         [(NSControl*)vv setEnabled:b];
         }
         
         }*/
        
    }
    int n = [_TabChooserOutlet numberOfTabViewItems];
    for (int i=0; i < n; i++) {
        
        NSView *v = [[_TabChooserOutlet tabViewItemAtIndex:i] view];
        for (NSView *vv in [v subviews]) {
            if( [vv respondsToSelector:@selector(setEnabled:)] )
            {
                [(NSControl*)vv setEnabled:b];
            }
            
        }
    }
    
    
    
    /* [_ExportAudioOutlet setEnabled:b];
     [_SaveRefinementOutlet setEnabled:b];
     [_DeleteRefinementOutlet setEnabled:b];*/
}


-(void) audioLoadedMenuConfig { // all menu options available
  //  [self haveScoreMenuConfig];  // shouldn't be necessary
    [self enableSpectControls:YES];
    return;
    
}



-(void) set_focus_on_navigate_panel {
    int live;
    char lab[500];
    
    focus_on_live = 0;
    [self enablePanel:"navigate" val:YES];
    [self enablePanel:"live" val:NO];
    //  Spect->MenuMicCheck->Enabled = false;  // may want this in mac ...?
    [self ready_mix_panel];
    restore_take_state();
    display_range_string();
    display_take_string();
    add_text_pos();
    [self audioLoadedMenuConfig];
    
    
    
    //    NSString *label = [[sender selectedCell] title];
    //    ensemble_in = [label isEqualToString:@"In"];
    
    //  set_toggle_arrow_toward_live_panel();  // will want this ...
}


- (void) enableSubControls: (id) control val:(BOOL) b {
    for (NSView *subview in [control  subviews]) {
        for (NSView *sub in [subview subviews]) {
            if ([sub respondsToSelector:@selector(setEnabled:)])
                [sub setEnabled:b];
        }
    }
}


-(void) enablePanel: (char *) s val:(BOOL) b {
    if (strcmp(s,"live") == 0)  [self enableSubControls: _LivePanel val:b];
    else if (strcmp(s,"navigate")== 0)  [self enableSubControls: _NavigatePanel val:b];
    else if (strcmp(s,"mix")== 0)  [self enableSubControls: _MixPanel val:b];
}


-(void)setStatFrame {
    int i,f;
    char fr[50];
    
    i = [view_self current_note];
    f = (i < firstnote) ? 0 :  score.solo.note[i].frames;
    sprintf(fr,"%d",f);
    [_StatusFrame setStringValue: [[NSString alloc] initWithCString:fr]];
}

- (IBAction)StartPlayingAction:(id)sender {
    [_SpectPlayToggleOutlet setState:NSOnState];
  //  [rehearsal_self enableStartStopButton:FALSE];
    play_action();
}


- (IBAction)PausePlayingAction:(id)sender {
    pause_audio();
    
}

- (IBAction)StopPlayingAction:(id)sender {
    [_SpectPlayToggleOutlet setState:NSOffState];
  //  [rehearsal_self enableStartStopButton:TRUE];
    quit_action();
}

static int nav_calls = 0;
static int nav_increment;

- (IBAction)NavForwardButton:(id)sender {
    if ([sender state] == NSOnState) {
        NSLog (@"on: %d",nav_calls);
        nav_calls++;
        
    }
    if ([sender state] == NSOffState) {
        NSLog (@"off: %d",nav_calls);
        nav_calls = 0;
        [_NavForwardOutlet setState:NSOnState];
    }
    nav_increment = (nav_calls > 10) ? 10 : 1;
    highlight_neighbor_note(nav_increment);
}

- (IBAction)NavBackwardButton:(id)sender {
    if ([sender state] == NSOnState) {
        NSLog (@"on: %d",nav_calls);
        nav_calls++;
        
    }
    if ([sender state] == NSOffState) {
        NSLog (@"off: %d",nav_calls);
        nav_calls = 0;
        [_NavBackwardOutlet setState:NSOnState];
    }
    nav_increment = (nav_calls > 10) ? -10 : -1;
    highlight_neighbor_note(nav_increment);
    
    
}

- (IBAction)CalculateFeatures:(id)sender {
    char name[200];
    char database[200];
    
    strcpy(name, user_dir);
    strcat(name, "database_full/");
    strcat(name,current_examp);
    strcat(name, "_48k.raw");
    
    strcpy(database, user_dir);
    strcat(database, "database_full/");
    strcat(database,current_examp);
    strcat(database, ".feature");
    
    write_features(database, 3);
    new_create_raw_from_48k(name);
}

- (IBAction)ReSynthesize:(id)sender {
    if (current_examp[0] == '\0') { printf("need to read audio\n"); exit(0); }
    
    char name[200];
    strcpy(name,audio_data_dir);
    strcat(name,current_examp);
    strcat(name,".feature");
    write_features(name, 3);
    
    database_pitch = (int*) calloc (128, sizeof(int)); //store existing intervals here
    AUDIO_FEATURE_LIST database_feature_list;
    database_feature_list.num = 0;
    database_feature_list.el = malloc(80000*sizeof(AUDIO_FEATURE));
    char directory[200];
    strcpy(directory, user_dir);
    strcat(directory, "database/");
    if(read_48khz_raw_audio_data_base(directory , &database_feature_list) == 0){
      NSLog(@"Problems in reading database");
      return;
    }
    resynth_solo_phase_vocoder(database_feature_list);
}



- (IBAction)CountIntervals:(id)sender {
    if (current_examp[0] == '\0') { printf("need to read audio\n"); exit(0); }
    //if (user[0] == '\0') { printf("need to read audio\n"); exit(0); }

    char name[200];
    int transp = 0; //for now
    strcpy(name, user_dir);
    strcat(name, "python/");
    strcat(name,player);
    strcat(name, "_");
    strcat(name,current_examp);
    strcat(name, ".intervals");

    count_intervals(name, transp);
    /* to count intervals in database
    strcpy(name, user_dir);
    strcat(name, "python/intervals");
    strcpy(directory, user_dir);
    strcat(directory, "database/");
    if(count_database_intervals(directory, name) == 0){
        NSLog(@"Problems in counting intervals");
        return;
    }
     */
}

- (IBAction)TransposeFeatures:(NSButton *)sender {
    int semitones = -7;
    char name[200], new_file[200], suffix[3];
    strcpy(name, user_dir);
    strcat(name, "database/");
    strcat(name,current_examp);
    strcat(name, ".feature");
    
    
    sprintf(suffix, "%d", abs(semitones));
    
    strcpy(new_file, user_dir);
    strcat(new_file, "database/");
    strcat(new_file,current_examp);
    strcat(new_file,"_t");
    if (semitones >= 0) strcat(new_file, "p");
    else strcat(new_file, "m");
    strcat(new_file, suffix);
    strcat(new_file, ".feature");
    transpose_features(name, new_file, semitones);
}


void
init_incs() {
    int i;
    char text[500];
    
    inc_vals.num = INC_NUMS;
    inc_vals.cur = 0;  // bar line
    inc_vals.inc[0].num = 0; inc_vals.inc[0].den = 1;
    inc_vals.inc[1].num = 1; inc_vals.inc[1].den = 1;
    inc_vals.inc[2].num = 1; inc_vals.inc[2].den = 2;
    inc_vals.inc[3].num = 1; inc_vals.inc[3].den = 4;
    inc_vals.inc[4].num = 1; inc_vals.inc[4].den = 8;
    inc_vals.inc[5].num = 1; inc_vals.inc[5].den = 16;
    inc_vals.inc[6].num = 3; inc_vals.inc[6].den = 8;
    inc_vals.inc[7].num = 3; inc_vals.inc[7].den = 16;
    //  SetInc->IncList->Clear();
    for (i=0; i < INC_NUMS; i++) {
        if (inc_vals.inc[i].num == 0) strcpy(text,"all");
        else if (inc_vals.inc[i].den == 1) strcpy(text,"barlines");
        else sprintf(text,"%d/%d",inc_vals.inc[i].num,inc_vals.inc[i].den);
        //      SetInc->IncList->Items->Add(text);
    }
    //  SetInc->IncList->Show();
}





- (void) setIncButtons {
    NSString *s;
    extern INC_VALS inc_vals;
    
    [_MarkerCheckOutlet setState: clicks_in];
    init_incs();
    int n = [_NoteMarkIncOutlet numberOfItems];
    for (int i=0; i < n; i++) {
        NSLog([_NoteMarkIncOutlet itemTitleAtIndex:i]);
        // [_NoteMarkIncOutlet s
        s = [_NoteMarkIncOutlet itemTitleAtIndex:i];
        if ([s isEqualToString:@"all"]) { inc_vals.inc[i].num = 0; inc_vals.inc[i].den = 1; continue;}
        if ([s isEqualToString:@"bar line"]) {
            inc_vals.inc[i].num = 0; inc_vals.inc[i].den = 1;  spect_inc = inc_vals.inc[i]; inc_vals.cur = i; continue;} // initialize to this.
        if ([s isEqualToString:@"half-note"]) { inc_vals.inc[i].num = 1; inc_vals.inc[i].den = 2; continue;}
        if ([s isEqualToString:@"quarter-note"]) { inc_vals.inc[i].num = 1; inc_vals.inc[i].den = 4; continue;}
        if ([s isEqualToString:@"eighth-note"]) { inc_vals.inc[i].num = 1; inc_vals.inc[i].den = 8; continue;}
        if ([s isEqualToString:@"sixteenth-note"]) { inc_vals.inc[i].num = 1; inc_vals.inc[i].den = 16; continue;}
        if ([s isEqualToString:@"dotted quarter-note"]) { inc_vals.inc[i].num = 3; inc_vals.inc[i].den = 8; continue; }
        if ([s isEqualToString:@"dotted eighth-note"]) { inc_vals.inc[i].num = 3; inc_vals.inc[i].den = 16; continue;}
        if ([s isEqualToString:@"no"]) { inc_vals.inc[i].num = 1; inc_vals.inc[i].den = 1; continue;}
       
        
    }
    [_NoteMarkIncOutlet selectItemWithTitle: @"bar line"];
    NSLog(@"%d %d",n,inc_vals.num);
    
}


- (IBAction)NavLeftButton:(id)sender {
   /* if (upgrade_purchased == 0) {
        [sender setIntValue:0];
        [[[NSApplication sharedApplication] delegate] upgradeAlert];
         [[NSAlert alertWithMessageText:@"Requires upgrade"
         defaultButton:@"OK"
         alternateButton:nil
         otherButton:nil
         informativeTextWithFormat:@"You must purchase the upgrade to use this feature"] runModal];
        return;
    } */
    
    move_cur_nt(-1);
}

- (IBAction)NavUndoButton:(id)sender {
    unmark_note();
}

- (IBAction)NavRiteButton:(id)sender {
 /*   if (upgrade_purchased == 0) {
        [sender setIntValue:0];
        [[[NSApplication sharedApplication] delegate] upgradeAlert];
        [[NSAlert alertWithMessageText:@"Requires upgrade"
         defaultButton:@"OK"
         alternateButton:nil
         otherButton:nil
         informativeTextWithFormat:@"You must purchase the upgrade to use this feature"] runModal];
        return;
    }
    */
    move_cur_nt(1);
}



- (IBAction)MenuRealign:(id)sender {
    int ios;
    
  /*  if (upgrade_purchased == 0) {
        [sender setIntValue:0];
        [[[NSApplication sharedApplication] delegate] upgradeAlert];
        return;
    } */
    if (currently_playing || live_locked || frames == 0) return;
    ios = accomp_on_speakers;  // save the user-set state
    accomp_on_speakers = is_on_speakers();  // the state assoc with the saved audio file
    printf("temp set accomp_on_speakers to %d\n",accomp_on_speakers);
    unhighlight_notes();
    
    make_current_dp_graph();
    
    background_info.function = parse;
    background_info.label = "Realigning audio ... please wait";
    
    PatControl* controller = [[PatControl alloc] initWithWindowNibName:@"Pat"];
    [controller showWindow:nil];
    [controller beginWaiting];
    
    if (background_info.ret_val == 0) {
        printf("couldn't parse successfully\n");
        [[NSAlert alertWithMessageText:@"Parse Failed"
                         defaultButton:@"OK"
                       alternateButton:nil
                           otherButton:nil
             informativeTextWithFormat:@""] runModal];
    }
    else printf("parsed succesfully\n");
    
    
    highlight_notes();
    accomp_on_speakers = ios;  // restore state
    printf("reset accomp_on_speakers to %d\n",accomp_on_speakers);
}






-(void) mainWindowResized {
    NSRect spect,wind,xxx;
    
    /* wind = [_window frame];
     spect = [_SpectWind frame];
     spect.origin.x = SIDE_MARGIN; spect.origin.y = BOT_MARGIN;
     spect.size.height = wind.size.height - BOT_MARGIN - TOP_MARGIN;
     //  spect.size.width = 100;
     // spect.size.height = 100;
     [_SpectWind setFrame:spect];
     xxx = [_SpectWind frame];*/
    if (frames > 0 && current_examp[0] != 0 && scoretag[0] != 0) {
        //      if (frames > 0)  {
        [view_self get_spect_page_no_args];
        [view_self show_the_spect];
    }
    
    NSLog(@"height  %f",wind.size.height);
}


-(int) mainWindowHeight {
 NSRect wind = [_window frame];
 return(wind.size.height);
 }
 
 -(int) mainWindowWidth {
 NSRect wind = [_window frame];
 return(wind.size.width);
 }







@end

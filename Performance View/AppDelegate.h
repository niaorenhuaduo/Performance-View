//
//  AppDelegate.h
//  Performance View
//
//  Created by craphael on 11/28/15.
//  Copyright (c) 2015 craphael. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "MyNSView.h"

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
- (IBAction)closeWindow:(id)sender;
- (IBAction)MenuRehearsalAction:(id)sender;
- (IBAction)SpectPlayToggle:(id)sender;
- (IBAction)MenuLiveOptions:(id)sender;
- (IBAction)MenuResetTraining:(id)sender;
- (IBAction)MenuLoadTake:(id)sender;
- (IBAction)DeleteRefinementAction:(id)sender;
- (IBAction)ExportAudioAction:(id)sender;
- (IBAction)SaveRefinementAction:(id)sender;
- (IBAction)MinimizeAction:(id)sender;
- (IBAction)ZoomCurrentWindow:(id)sender;
- (IBAction)BringAllToFront:(id)sender;
- (IBAction)HideOthersAction:(id)sender;
- (IBAction)MenuExit:(id)sender;
- (IBAction)HideApplication:(id)sender;
- (IBAction)StartupAction:(id)sender;
- (IBAction)MarkerCheckAction:(id)sender;
- (IBAction)SimpleMarkVolAction:(id)sender;
- (IBAction)SimpleMarkBalAction:(id)sender;
- (IBAction)SimpleMarkMuteAction:(id)sender;
- (IBAction)SimpleSoloVolAction:(id)sender;
- (IBAction)SimpleSoloBalAction:(id)sender;
- (IBAction)SimpleSoloMuteAction:(id)sender;
- (IBAction)SimpleAccMuteAction:(id)sender;
- (IBAction)SimpleAccBalAction:(id)sender;
- (IBAction)SimpleAccVolAction:(id)sender;
- (IBAction)RefinePrevButtonAction:(id)sender;
- (IBAction)RefineNextButtonAction:(id)sender;
- (IBAction)RefineUndoAction:(id)sender;
- (IBAction)RefineAlignAction:(id)sender;
- (IBAction)RefineNudgeRightAction:(id)sender;
- (IBAction)RefineNudgeLeftAction:(id)sender;
- (IBAction)NoteMarksSoundMenuChangeAction:(id)sender;
- (IBAction)NoteMarksIncMenuChangeAction:(id)sender;
- (IBAction)ReadAudio:(id)sender;
- (IBAction)AccompRadio:(id)sender;
- (IBAction)StopPlayingAction:(id)sender;
- (IBAction)StartPlayingAction:(id)sender;
- (IBAction)PausePlayingAction:(id)sender;
- (IBAction)NavForwardButton:(id)sender;
- (IBAction)NavBackwardButton:(id)sender;


-(void)setStatRange:(char *) meas;
-(void)setStatTake:(char *) take;
-(void) enablePanel: (char *) s val:(BOOL) b;
-(void)setStatusPos:(char *) meas;
-(void)setStatFrame;
-(int) mainWindowHeight;
-(int) mainWindowWidth;


@property (assign) IBOutlet NSButton *AlignMarkersOutlet;
@property (assign) IBOutlet NSButton *RefineNudgeLeftOutlet;
@property (assign) IBOutlet NSButton *RefineNudgeRightOutlet;
@property (assign) IBOutlet NSButton *RefineUndoOutlet;
@property (assign) IBOutlet NSTextField *StatusMeasRange;
@property (assign) IBOutlet NSTextField *StatusTake;
@property (assign) IBOutlet NSWindow *MainWindowOutlet;
@property (assign) IBOutlet MyNSView *SpectWind;
//@property (assign) IBOutlet NSWindow *SpectWind;
@property (assign) IBOutlet NSTextField *PieceTitleOutlet;
@property (assign) IBOutlet NSBox *LivePanel;
@property (assign) IBOutlet NSBox *NavigatePanel;
@property (assign) IBOutlet NSBox *MixPanel;
@property (assign) IBOutlet NSSlider *BalanceSliderOutlet;
@property (assign) IBOutlet NSSlider *PanSliderOutlet;
@property (assign) IBOutlet NSMatrix *AccompRadioOutlet;
@property (assign) IBOutlet NSMatrix *MarkersRadioOutlet;
@property (assign) IBOutlet NSBox *MixerBox;
@property (assign) IBOutlet NSSlider *VolumeSliderOutlet;
@property (assign) IBOutlet NSTextField *StatusPosition;
@property (assign) IBOutlet NSTextField *StatusFrame;
@property (assign) IBOutlet NSTabView *TabChooserOutlet;
@property (assign) IBOutlet NSButtonCell *SpectPlayToggleOutlet;
@property (assign) IBOutlet NSButton *NavForwardOutlet;
@property (assign) IBOutlet NSButton *NavBackwardOutlet;
//@property (assign) IBOutlet NSButton *MarkerCheckOutlet;
//@property (assign) IBOutlet NSPopUpButton *NoteMarkIncOutlet;
@property (assign) IBOutlet NSButton *MarkerCheckOutlet;
@property (assign) IBOutlet NSPopUpButton *NoteMarkIncOutlet;








@end

//
//  ChooseScoreController.h
//  Music+One
//
//  Created by Christopher Raphael on 2/22/12.
//  Copyright (c) 2012 Christopher Raphael. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "ChooseScoreBrowser.h"

@interface ChooseScoreController : NSWindowController
@property (assign) IBOutlet NSWindow *ChooseScrWindow;
@property (assign) IBOutlet ChooseScoreBrowser *ChooseScrBrowser;
- (IBAction)ChooseScrBut:(id)sender;
- (IBAction)ChooseBut:(id)sender;
- (IBAction)ChooseScoreButton:(id)sender;
@property (assign) IBOutlet NSTextField *DecompressLabel;

@end

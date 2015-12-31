//
//  SetupController.h
//  TestApp
//
//  Created by Christopher Raphael on 12/23/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import <Cocoa/Cocoa.h>


void *setup_self;

@interface SetupController : NSWindowController
- (IBAction)SetupPlayer:(id)sender;
- (IBAction)SetupScore:(id)sender;
- (IBAction)SetupRecall:(id)sender;
@property (assign) IBOutlet NSTextField *SetupLabel;

@property (assign) IBOutlet NSWindow *SetupWindow;

@end

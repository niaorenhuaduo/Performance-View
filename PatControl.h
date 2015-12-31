//
//  PatControl.h
//  TestApp
//
//  Created by Christopher Raphael on 1/12/12.
//  Copyright (c) 2012 Indiana University/Informatics. All rights reserved.
//

#import <Cocoa/Cocoa.h>

void *pat_self;

@interface PatControl : NSWindowController
@property (assign) IBOutlet NSView *PatWindow;
@property (assign) IBOutlet NSProgressIndicator *PatienceBar;
@property (assign) IBOutlet NSWindow *PatWind;
@property (assign) IBOutlet NSTextField *PatienceLabel;


@end

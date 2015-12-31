//
//  Player.h
//  TestApp
//
//  Created by Christopher Raphael on 12/17/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface Player : NSWindowController

@property (assign) IBOutlet NSBrowser *PlayerNames;
@property (assign) IBOutlet NSWindow *PlayerWind;

@property (assign) IBOutlet NSView *choosePlayerWindow;
@property (assign) IBOutlet NSTextField *NewPlayer;

- (IBAction)WindowBut:(id)sender;
- (IBAction)AddPlayer:(id)sender;
@property (assign) IBOutlet NSBrowser *Instrument;


@end

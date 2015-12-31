//
//  ReadAudioController.h
//  TestApp
//
//  Created by Christopher Raphael on 12/24/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
void *read_audio_self;

@interface ReadAudioController : NSWindowController
@property (assign) IBOutlet NSTableView *ReadAudioTableView;
@property (assign) IBOutlet NSWindow *ReadAudioWindow;

- (IBAction)ReadAudioOk:(id)sender;
- (IBAction)ReadAudioCancel:(id)sender;
- (IBAction)ReadAudioQuit:(id)sender;
- (IBAction)ReadAudioDismiss:(id)sender;
@property (assign) IBOutlet NSTableView *ReadAudioNewTable;
- (IBAction)RefineAndTrainAction:(id)sender;
@property (assign) IBOutlet NSWindow *NewReadAudioWindow;
@property (assign) IBOutlet NSTextField *PieceLabelOutlet;
@property (assign) IBOutlet NSTableView *NewReadAudioTableViewOutlet;
@property (assign) IBOutlet NSWindow *ReadAudioWindowOutlet;

@end

//
//  PatControl.m
//  TestApp
//
//  Created by Christopher Raphael on 1/12/12.
//  Copyright (c) 2012 Indiana University/Informatics. All rights reserved.
//

#import "PatControl.h"
#include "share.h"
#include "global.h"
static void *patienceTimer;

@implementation PatControl
@synthesize PatWindow;
@synthesize PatienceBar;
@synthesize PatWind;
@synthesize PatienceLabel;

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    pat_self = self;
  /*   [PatienceBar setIndeterminate:FALSE];
    [PatienceBar setDoubleValue:0];
    [PatienceBar displayIfNeeded];
    [PatienceLabel setStringValue:@"xxx"];*/
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
   
   
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}


-(void) patienceEvent {
  //  NSLog(@"Patience timer");
    [PatienceBar setDoubleValue: background_info.fraction_done];
    if (background_info.fraction_done <  1) return;
    [patienceTimer invalidate];
    patienceTimer   = nil;
    [self close];
    
}

+(void)backgroundThread:(id)param{
    background_info.ret_val = background_info.function();
    NSLog(@"done with background");

}


-(void) prepare {
  //  [PatienceBar setIndeterminate:FALSE];
    [PatienceBar setIndeterminate:background_info.indeterminate];
  [PatienceBar setDoubleValue:0];
    [PatienceBar setNeedsDisplay:YES];
  [PatienceBar displayIfNeeded];
}

static void *waitTimer;

-(void) wait_timer {
    if (background_info.fraction_done < 1) return;
    [waitTimer invalidate];
    [self close];
}


-(void) beginWaiting {
    NSThread *thread;
    
    [PatWind makeKeyAndOrderFront:nil];
    //    [PatienceBar setIndeterminate:FALSE];
    [PatienceBar setIndeterminate:background_info.indeterminate];

    [PatienceLabel setStringValue: [[NSString alloc] initWithCString:background_info.label]];
    if (background_info.indeterminate) [PatienceBar setUsesThreadedAnimation:YES];
    [PatienceLabel displayIfNeeded];
    [PatienceBar setMinValue:0];
    [PatienceBar setMaxValue:1];
    [PatienceBar setDoubleValue:0];
    [PatienceBar setNeedsDisplay:YES];
    [PatienceBar displayIfNeeded];
    background_info.fraction_done = 0;
    [PatienceBar startAnimation:self];
    thread = [[NSThread alloc] initWithTarget: [PatControl class] selector:@selector(backgroundThread:) object:nil];
    [thread start];
    
 //   [NSThread detachNewThreadSelector:@selector(backgroundThread:) toTarget:[PatControl class] withObject:nil];
    
    
 /*   waitTimer =  [NSTimer scheduledTimerWithTimeInterval:0.01 
                                                target:self
                                              selector:@selector(wait_timer) 
                                              userInfo:NULL 
                                               repeats:YES];
    return;*/
    
  //  while (background_info.fraction_done <  1) {
    
    while ([thread isExecuting]) {
  //  NSLog(@"val = %f",background_info.fraction_done);
      [PatienceBar setDoubleValue: background_info.fraction_done];
        [PatienceBar displayIfNeeded];
        //usleep(100000);
        usleep(10000);
        
    }
    [self close];

   
}

@end

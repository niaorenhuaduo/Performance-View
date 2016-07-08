//
//  MyNSView.h
//  TestApp
//
//  Created by Christopher Raphael on 12/26/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import <Cocoa/Cocoa.h>

void buffer_audio_samples(int samps, int played);
//void *view_self;



typedef struct {
    int i;
    int j;
} LOC;


typedef struct {
  LOC loc;
  int height;
  int width;
} RECT;

@interface MyNSView : NSView {
    
  NSBitmapImageRep         *   bigBitmap;  
  /* this is a bit map sized for the maximal bitmap size due to resizing.  usually only a portion will be used */
    NSRect    nsRectFrameRect;
    CGFloat   cgFloatRed;
    CGFloat   cgFloatRedUpdate;
    NSTimer                  *   nsTimerRef;
}

@property (assign) IBOutlet MyNSView *Spectrogram;

- (IBAction) startAnimation;
- (IBAction) stopAnimation:(id)pId;

-(void)paintGradientBitmap;
-(void)show_the_spect;
-(void) markersChange;
-(void) synthAmim;
-(void) stopSynthAnim;
-(void) displaySpect;
-(void) redraw_markers;



@end




MyNSView *view_self;


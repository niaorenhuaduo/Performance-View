//
//  ChooseScoreBrowser.m
//  Music+One
//
//  Created by Christopher Raphael on 2/22/12.
//  Copyright (c) 2012 Christopher Raphael. All rights reserved.
//

#import "ChooseScoreBrowser.h"
#include "score_utils.h"

@implementation ChooseScoreBrowser


- (void) awakeFromNib {
    fast_get_pieces();
}


- (int)browser:(NSBrowser *)sender numberOfRowsInColumn:(int)column {
	return (column == 0) ? num_available_scores() : 0;
}

- (void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(int)row column:(int)column {
    // Find our parent FSNodeInfo and access the child at this particular row
    NSString *name;
    //[name initwithCString:;
    name = [[NSString alloc] initWithCString:get_score_title(row) encoding:NSMacOSRomanStringEncoding];
	
	[cell setStringValue:name];
    [cell setLeaf:1];
    
}    

@end

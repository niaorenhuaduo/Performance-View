//
//  NameBrowser.m
//  TestApp
//
//  Created by Christopher Raphael on 12/17/11.
//  Copyright (c) 2011 Indiana University/Informatics. All rights reserved.
//

#import "NameBrowser.h"
#include "share.h"
#include "global.h"

@implementation NameBrowser

- (int)browser:(NSBrowser *)sender numberOfRowsInColumn:(int)column {
	return (column == 0) ? player_list.num: 0;
}

- (void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(int)row column:(int)column {
    // Find our parent FSNodeInfo and access the child at this particular row
    NSString *name;
    //[name initwithCString:;
    name = [[NSString alloc] initWithCString:player_list.player[row].person encoding:NSMacOSRomanStringEncoding];
	
	[cell setStringValue:name];
    [cell setLeaf:1];
    

	
//	currentNode = [currentNode childAtIndex:row];
	
}



@end

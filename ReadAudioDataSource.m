//
//  ReadAudioDataSource.m
//  TestApp
//


#import "ReadAudioDataSource.h"
#include "read_audio_utils.h"


@implementation ReadAudioDataSource

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return audio_files.num;
}

- (id)tableView:(NSTableView *)tableView
objectValueForTableColumn:(NSTableColumn *)tableColumn
            row:(int)row
{
    
    
    NSString *ident = [tableColumn identifier];
    if ([ident isEqualToString:@"num"]) return [[NSString alloc] initWithCString:audio_files.list[row].num];
    if ([ident isEqualToString:@"date"]) return [[NSString alloc] initWithCString:audio_files.list[row].date];
     if ([ident isEqualToString:@"range"]) return [[NSString alloc] initWithCString:audio_files.list[row].range];
 //    if ([ident isEqualToString:@"end"]) return [[NSString alloc] initWithCString:audio_files.list[row].end];
     if ([ident isEqualToString:@"train"]) return [[NSString alloc] initWithCString:audio_files.list[row].train];
    if ([ident isEqualToString:@"correct"]) return [[NSString alloc] initWithCString:audio_files.list[row].correct];
}

/*-(void) mouseDown:(NSEvent *)theEvent  {
    NSLog(@"mouseDown");
}*/

@end

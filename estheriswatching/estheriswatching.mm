#include <stdlib.h>
#include <stdio.h>
#import <Foundation/Foundation.h>
#include <asl.h>
#include <objc/runtime.h>

#include "NetworkStatistics.h"

// Keys for NStatSourceCopyProperty



void printSortedCounts(NSMutableDictionary * sources ) {
    NSArray * keys = [sources allKeys];
    
    NSArray * sortedKeys = [keys sortedArrayUsingComparator:^NSComparisonResult(id key1, id key2) {
        NSString * rx1 = sources[key1][@"RxBytes"];
        NSString * rx2 = sources[key2][@"RxBytes"];
        return [rx1 compare:rx2];
    }];
    
    for( id k in sortedKeys ) {
        NSLog(@"%@: up: %@, down: %@>\n", sources[k][@"pname"], sources[k][@"TxBytes"], sources[k][@"RxBytes"]);
    }
}

void printPname( NSMutableDictionary * sources, NSString * pname ) {
    for( id k in sources ) {
        if( [(NSString *)sources[k][@"pname"] rangeOfString:pname].length > 0 )
            NSLog( @"%@\n", sources[k] );
    }
}

void printSummary( NSMutableDictionary * sources ) {
    NSMutableDictionary * sorted = [[NSMutableDictionary alloc] init];
    for( id k in sources ) {
        NSString * pname = [NSString stringWithFormat:@"%@ %@", sources[k][@"pid"], sources[k][@"pname"]];
        if( pname != nil) {
            if( [sorted objectForKey:pname] == nil )
                sorted[pname] = @(0);
            
            if( [pname rangeOfString:@"966"].length > 0 )
                NSLog(@"%@: %@\n", pname, sources[k][@"RxBytes"] );
            sorted[pname] = @([sorted[pname] intValue] + [sources[k][@"RxBytes"] intValue]);
        }
    }
    
    NSArray * keys = [sorted allKeys];
    keys = [keys sortedArrayUsingComparator:^NSComparisonResult(NSString * obj1, NSString * obj2) {
        int pid1 = [[obj1 componentsSeparatedByString:@" "][0] integerValue];
        int pid2 = [[obj2 componentsSeparatedByString:@" "][0] integerValue];
        if( pid1 < pid2 )
            return NSOrderedAscending;
        if( pid1 > pid2 )
            return NSOrderedDescending;
        return NSOrderedSame;
    }];
    
    for( id k in keys ) {
        NSLog(@"%@ %@\n", k, sorted[k]);
    }
}

int main(int argc, const char * argv[])
{
    dispatch_queue_t queue = dispatch_queue_create("haxx0r queue", 0x0);
    
    // there must be more to this than.... just this. :P
    void (^query_callback)() = ^(){
        NSLog(@"Query Callback\n");
    };
    
    void (^desc_callback)() = ^(){
        NSLog(@"Desc Callback\n");
    };
    
    void (^Removed_callback)() = ^() {
        NSLog(@"Removed Callback\n");
    };
    
    // Supposedly this will get called for each new source?
    NSMutableDictionary * sources = [[NSMutableDictionary alloc] init];
    
    void (^create_callback)(__NStatSource * ) = ^(__NStatSource * src){
        //NSLog(@"Create Callback: 0x%llx\n", (unsigned long long)src);
        
        // This is the key we index into sources with
        NSValue * key = [NSValue valueWithPointer:src];
        
        if( sources[key] )
            NSLog(@"NOOOO");
        
        // Each source will record le informationz
        sources[key] = [[NSMutableDictionary alloc] init];
        
        // Setup description callback
        NStatSourceSetRemovedBlock(src, ^() {
            //NSLog(@"[0x%llx]: REMOVED\n", (unsigned long long)src);
            
            // Cleanup!
            //[sources removeObjectForKey:key];
        });
        NStatSourceSetCountsBlock(src, ^(CFDictionaryRef counts) {
            NSLog(@"0x%x %@ COUNTS\n", src, counts);
            
            [sources[key] addEntriesFromDictionary:(__bridge NSDictionary*)counts];
        });
        NStatSourceSetDescriptionBlock(src, ^(CFDictionaryRef desc) {
            if( [[(__bridge NSDictionary*)desc objectForKey:@"pid"] integerValue] == 966 ) {
                
            }
            NSLog(@"0x%x %@\n", src, desc);
            [sources[key] addEntriesFromDictionary:(__bridge NSDictionary*)desc];
        });
    };
    
    __NStatManager * man = NStatManagerCreate( kCFAllocatorDefault, queue, create_callback );
    NSLog(@"Instantiated NStatManager at 0x%llx\n", (unsigned long long)man);
    
    NStatManagerAddAllTCP(man);
    NStatManagerAddAllUDP(man);
    NStatManagerQueryAllSourcesDescriptions(man, desc_callback);
    
    sleep(1);
    NStatManagerQueryAllSources(man, query_callback);
    sleep(1);
    
    //printSortedCounts(sources);
    NStatManagerDestroy(man);
    printSummary(sources);
    sleep(1);
    return 0;
}


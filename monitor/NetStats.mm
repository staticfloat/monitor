#include "NetStats.h"
#include "NetStats_private.h"
#import <Foundation/Foundation.h>
#include <sys/types.h>
#include "ProcStats.h"


NetworkMonitor::NetworkMonitor(double refreshRate) : refreshRate(refreshRate) {
    queue = dispatch_queue_create("com.staticfloat.NetworkMonitor", DISPATCH_QUEUE_SERIAL);
    
    // Helper function that updates a pid_netstats_internal structure with a source_netstats structure
    void (^update_pidstats)(pid_netstats_internal *, source_netstats *) = ^(pid_netstats_internal * pidstats, source_netstats * srcstats){
        pidstats->downPerInterval += (srcstats->currDown - srcstats->lastDown);
        pidstats->upPerInterval += (srcstats->currUp - srcstats->lastUp);
        pidstats->totalDown += srcstats->currDown;
        pidstats->totalUp += srcstats->currUp;
    };
    
    // downdate.  lol.
    void (^downdate_pidstats)(pid_netstats_internal *, source_netstats *) = ^(pid_netstats_internal * pidstats, source_netstats * srcstats){
        pidstats->downPerInterval -= (srcstats->currDown - srcstats->lastDown);
        pidstats->upPerInterval -= (srcstats->currUp - srcstats->lastUp);
        pidstats->totalDown -= srcstats->currDown;
        pidstats->totalUp -= srcstats->currUp;
    };
    
    void (^update_sourcestats)(source_netstats *, long, long) = ^(source_netstats * srcstats, long currUp, long currDown) {
        srcstats->lastUp = srcstats->currUp;
        srcstats->lastDown = srcstats->currDown;
        srcstats->currUp = currUp;
        srcstats->currDown = currDown;
    };

    // Let's set up our create callback
    void (^create_callback)(__NStatSource * ) = ^(__NStatSource * src){
        NStatSourceSetRemovedBlock(src, ^() {
            // When a source is removed, just go ahead and update pidstats immediately, then junk the source_stats from it
            CFNumberRef cf_pid = NStatSourceCopyProperty(src, kNStatSrcKeyPID);
            if( cf_pid != NULL ) {
                pid_t pid = 0;
                CFNumberGetValue(cf_pid, kCFNumberLongType, (void *) &pid);
                
                // If we've recorded the history of this source/pid, let's update it just as it dies!
                if( stats.find(pid) != stats.end() ) {
                    pid_netstats_internal * pidstats = stats[pid];
                    if( pidstats->sources.find(src) != pidstats->sources.end() ) {
                        source_netstats * srcstats = pidstats->sources[src];
                        
                        
                        downdate_pidstats(pidstats, srcstats);
                        
                        // Update srcstats
                        CFDictionaryRef counts = NStatSourceCopyCounts(src);
                        long currUp = [((__bridge NSDictionary*)counts)[kNStatSrcKeyTxBytes] integerValue];
                        long currDown = [((__bridge NSDictionary*)counts)[kNStatSrcKeyRxBytes] integerValue];
                        srcstats->lastUp = srcstats->currUp = currUp;
                        srcstats->lastDown = srcstats->currDown = currDown;
                        
                        // Update pidstats
                        update_pidstats(pidstats, srcstats);
                        
                        // Remove srcstats from pidstats, and delete it
                        pidstats->sources.erase(src);
                        delete srcstats;
                        
                        // Do our releasing dance
                        CFRelease(counts);
                    }
                }
                
                if( cf_pid != NULL )
                    CFRelease(cf_pid);
            }
        });
        NStatSourceSetDescriptionBlock(src, ^(CFDictionaryRef desc) {
            CFDictionaryRef counts = NStatSourceCopyCounts(src);
            pid_t pid = (pid_t)[((__bridge NSDictionary*)desc)[kNStatSrcKeyPID] integerValue];
            
            // Get the pid_netstats that belongs to this pid
            if( stats.find(pid) == stats.end() )
                stats[pid] = new pid_netstats_internal();
            pid_netstats_internal * pidstats = stats[pid];
            
            if( pidstats->sources.find(src) == pidstats->sources.end() ) {
                pidstats->sources[src] = new source_netstats();
                
                // Store into currUp/currDown NOW, so that when we do this again in a minute, we get zero velocity
                pidstats->sources[src]->currUp = [((__bridge NSDictionary*)counts)[kNStatSrcKeyTxBytes] integerValue];
                pidstats->sources[src]->currDown = [((__bridge NSDictionary*)counts)[kNStatSrcKeyRxBytes] integerValue];
            } else {
                /*
                source_netstats * srcstats = pidstats->sources[src];
                bool activity = (srcstats->currUp - srcstats->lastUp) != 0 || (srcstats->currDown - srcstats->lastDown) != 0;
                if( pid == 43 && activity ) {
                    printf("%lu %lu\n", pidstats->downPerInterval, pidstats->upPerInterval);
                    printf("");
                }
                 */
                
                // Remove from pidstats the influence of a previous version of this srcstats
                downdate_pidstats(pidstats, pidstats->sources[src]);
                /*
                if( pid == 43 && activity ) {
                    printf("%lu %lu\n", pidstats->downPerInterval, pidstats->upPerInterval);
                    printf("");
                }
                */
            }
            source_netstats * srcstats = pidstats->sources[src];
            
            // Update srcstats with the new data we've gotten from `counts`
            long currUp = [((__bridge NSDictionary*)counts)[kNStatSrcKeyTxBytes] integerValue];
            long currDown = [((__bridge NSDictionary*)counts)[kNStatSrcKeyRxBytes] integerValue];
            update_sourcestats(srcstats, currUp, currDown );
            
            // Now update the pid struct with this source's new info
            update_pidstats(pidstats, srcstats);
            
            // Don't forget to release this memory, apparently. ;)
            CFRelease(counts);
        });
    };

    manager = NStatManagerCreate( kCFAllocatorDefault, queue, create_callback );
    NStatManagerAddAllTCP(manager);
    NStatManagerAddAllUDP(manager);
    
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, refreshRate * NSEC_PER_SEC, 0);
    dispatch_source_set_event_handler(timer, ^{
        NStatManagerQueryAllSources(manager, NULL);
        NStatManagerQueryAllSourcesDescriptions( manager, NULL );
    });
    dispatch_resume(timer);
}

/*
std::map<pid_t, pid_netstats> * NetworkMonitor::copyStats() {
    __block std::map<pid_t, pid_netstats> * ret = new std::map<pid_t, pid_netstats>();
    
    // Run this on the queue so that we're synchronized
    dispatch_sync(queue, ^{
        // Create a map of pid_netstats from our pid_netstats_internal thingies
        for( auto itty : stats ) {
            new(&(*ret)[itty.first]) pid_netstats(*itty.second);
        }
    });
    
    return ret;
}
*/

void NetworkMonitor::killPID(pid_t pid) {
    dispatch_async(queue, ^{
        auto itty = stats.find(pid);
        if( itty != stats.end() ) {
            auto sources = stats[pid]->sources;
            while( !sources.empty() ) {
                delete sources.begin()->second;
                sources.erase(sources.begin());
            }
            auto pid_stats = stats[pid];
            stats.erase(pid);
            delete pid_stats;
        }
    });
}

bool NetworkMonitor::copyStats(pid_t pid, pid_netstats_internal * outstats) {
    if( stats.find(pid) == stats.end() )
        return false;
    
    // Run this on the queue so that we're synchronized
    __block pid_netstats_internal temp;
    dispatch_sync(queue, ^{
        temp = *stats[pid];
    });
    *outstats = temp;
    return true;
}

double NetworkMonitor::getRefreshRate() {
    return refreshRate;
}


NetworkMonitor::~NetworkMonitor() {
    dispatch_suspend(timer);
    dispatch_suspend(queue);
    // Look at what good citizens we are!
    NStatManagerDestroy(manager);
}
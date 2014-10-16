#ifndef monitor_NetStats_private_h
#define monitor_NetStats_private_h

#include "NetStats.h"
#import <Foundation/Foundation.h>

// Function prototypes for NetworkStatistics framework
extern NSConstantString * kNStatSrcKeyPID;
extern NSConstantString * kNStatSrcKeyTxBytes;
extern NSConstantString * kNStatSrcKeyRxBytes;

extern "C" {
    __NStatManager * NStatManagerCreate( CFAllocatorRef allocator, dispatch_queue_t queue, void (^)(__NStatSource *) );
    void NStatManagerDestroy( __NStatManager * man );
    
    void NStatManagerAddAllTCP( __NStatManager * man );
    void NStatManagerAddAllUDP( __NStatManager * man );
    
    void NStatSourceSetRemovedBlock( __NStatSource * src, void (^)());
    void NStatSourceSetCountsBlock( __NStatSource * src, void (^)(CFDictionaryRef));
    void NStatSourceSetDescriptionBlock( __NStatSource * src, void (^)(CFDictionaryRef));
    
    
    // I guess I don't actually need these guys
    /**/
     CFDictionaryRef NStatSourceCopyCounts( __NStatSource * src );
     CFNumberRef NStatSourceCopyProperty( __NStatSource * src, NSConstantString * str );
     void NStatSourceQueryDescription( __NStatSource * src );
    /**/
    
    void NStatManagerQueryAllSources( __NStatManager * man, void (^)() );
    void NStatManagerQueryAllSourcesDescriptions( __NStatManager * man, void (^)() );
}

#endif

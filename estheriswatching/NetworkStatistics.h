#ifndef monitor_NetworkStatistics_h
#define monitor_NetworkStatistics_h

struct __NStatSource {
};

struct __NStatManager {
};


// Function prototypes for NetworkStatistics framework
extern "C" {
    __NStatManager * NStatManagerCreate( CFAllocatorRef allocator, dispatch_queue_t queue, void (^)(__NStatSource *) );
    void NStatManagerDestroy( __NStatManager * man );
    
    void NStatManagerAddAllTCP( __NStatManager * man );
    void NStatManagerAddAllUDP( __NStatManager * man );
    
    void NStatSourceSetRemovedBlock( __NStatSource * src, void (^)());
    void NStatSourceSetCountsBlock( __NStatSource * src, void (^)(CFDictionaryRef));
    void NStatSourceSetDescriptionBlock( __NStatSource * src, void (^)(CFDictionaryRef));
    
    
    /*
     CFDictionaryRef NStatSourceCopyCounts( __NStatSource * src );
     CFNumberRef NStatSourceCopyProperty( __NStatSource * src, NSConstantString * str );
     void NStatSourceQueryDescription( __NStatSource * src );
     */
    
    void NStatManagerQueryAllSources( __NStatManager * man, void (^)() );
    void NStatManagerQueryAllSourcesDescriptions( __NStatManager * man, void (^)() );
}

#endif

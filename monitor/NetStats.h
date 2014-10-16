#ifndef monitor_NetStats_h
#define monitor_NetStats_h

#include <map>
#include <sys/types.h>
#include <dispatch/dispatch.h>

struct __NStatSource {
};

struct __NStatManager {
};

// The network statistics for a certain source
struct source_netstats {
    long lastUp, lastDown;
    long currUp, currDown;
    
    // Trivial zero-initialization constructor
    source_netstats() : lastUp(0), lastDown(0), currUp(0), currDown(0) {}
};

// the network statistics for a given PID
struct pid_netstats_internal {
    long totalUp, totalDown;
    long upPerInterval, downPerInterval;
    std::map<__NStatSource *, source_netstats *> sources;
    
    // Trivial zero-initialization constructor
    pid_netstats_internal() : totalUp(0), totalDown(0), upPerInterval(0), downPerInterval(0) {}
};

// This is the structure we pass to the outside world so we don't have to worry about `sources` in the outside world
struct pid_netstats {
    long totalUp, totalDown;
    long upPerSec, downPerSec;
    
    // Trivial zero-initialization constructor
    pid_netstats() : totalUp(0), totalDown(0), upPerSec(0), downPerSec(0) {}
    
    // Copy constructor from a pid_netstats_internal
    pid_netstats( const pid_netstats_internal & papa, const double refreshRate ) : totalUp(papa.totalUp), totalDown(papa.totalDown), upPerSec(papa.upPerInterval/refreshRate), downPerSec(papa.downPerInterval/refreshRate) {}
};

class Monitor;
class NetworkMonitor {
public:
    NetworkMonitor( double refreshRate = 1.0 );
    ~NetworkMonitor();
    
    // Allows the outside world to take a look at our statistics!
    std::map<pid_t, pid_netstats> * copyStats();
    
    // Peek at just a single PID
    bool copyStats(pid_t pid, pid_netstats_internal * outstats);
    
    // Update a Monitor object with our data
    //void updateMonitor( Monitor * mon );
    void killPID( pid_t pid );
    
    double getRefreshRate();
private:
    // Time interval between queries
    double refreshRate;
    
    __NStatManager * manager;
    dispatch_queue_t queue;
    dispatch_source_t timer;
    std::map<pid_t, pid_netstats_internal *> stats;
};

#endif // monitor_NetStats_h
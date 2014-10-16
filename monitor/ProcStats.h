#ifndef __monitor__ProcStats__
#define __monitor__ProcStats__

#include "NetStats.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unordered_map>

struct pid_cpustats_internal {
    // Total userspace and kernel time
    uint64_t user_time, sys_time;
    uint64_t last_user_time, last_sys_time;
    
    // Percentage of userspace and kernel time used in the last time interval
    float user_pct, sys_pct;
    
    pid_cpustats_internal() : user_time(0), sys_time(0), last_user_time(0), last_sys_time(0), user_pct(0.0f), sys_pct(0.0f) {};
};

struct pid_cpustats {
    uint64_t user_time, sys_time;
    float user_pct, sys_pct;
    
    pid_cpustats() : user_time(0), sys_time(0), user_pct(0.0f), sys_pct(0.0f) {};
    pid_cpustats(pid_cpustats_internal & papa) : user_time(papa.user_time), sys_time(papa.sys_time), user_pct(papa.user_pct), sys_pct(papa.sys_pct) {};
};

struct pid_stats_internal {
    pid_netstats_internal net;
    pid_cpustats_internal cpu;
};

struct pid_stats {
    pid_netstats net;
    pid_cpustats cpu;
};


class Monitor {
public:
    Monitor();
    ~Monitor();
    
    // Get the stats for a single PID
    pid_stats * getStats(pid_t pid);
    
    const char * getPIDName(pid_t pid);
private:
    dispatch_queue_t queue;
    dispatch_source_t timer;
    dispatch_source_t cleaning_timer;
    NetworkMonitor * netmon;
    double refresh_rate;
    
    std::unordered_map<pid_t, pid_stats_internal> stats;
};


#endif

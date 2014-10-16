#include "ProcStats.h"
#include <libproc.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <signal.h>
#include <errno.h>


// Returns a - b in usec
unsigned long timeDiff(timeval & a, timeval & b) {
    return (a.tv_sec - b.tv_sec)*1000000 + a.tv_usec - b.tv_usec;
}

const char * Monitor::getPIDName( pid_t pid ) {
    static char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
    bzero(pathBuffer, PROC_PIDPATHINFO_MAXSIZE);
    proc_name(pid, pathBuffer, PROC_PIDPATHINFO_MAXSIZE);
    
    if( pathBuffer[0] == 0 )
        proc_pidpath(pid, pathBuffer, PROC_PIDPATHINFO_MAXSIZE);
    return pathBuffer;
}


Monitor::Monitor() : refresh_rate(1.0) {
    queue = dispatch_queue_create("com.staticfloat.Monitor", DISPATCH_QUEUE_SERIAL);
    this->netmon = new NetworkMonitor(refresh_rate*.75);
    
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, refresh_rate * NSEC_PER_SEC, 0);
    dispatch_source_set_event_handler(timer, ^{
        // First, query for all PIDs
        unsigned int numProcs = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
        pid_t * pids = new pid_t[numProcs];
        bzero(pids, numProcs*sizeof(pid_t));
        proc_listpids(PROC_ALL_PIDS, 0, pids, numProcs*sizeof(pid_t));
        
        // Now, get information about each PID
        for( unsigned int i=0; i<numProcs; ++i ) {
            // Get CPU usage statistics for this PID
            proc_taskinfo ti;
            proc_pidinfo(pids[i], PROC_PIDTASKINFO, 0, &ti, PROC_PIDTASKINFO_SIZE);
            
            // If we're seeing a new PID for the first time, make sure we don't give it any CPU usage this time
            if( stats.find(pids[i]) == stats.end() ) {
                stats[pids[i]].cpu.sys_time = ti.pti_total_system;
                stats[pids[i]].cpu.user_time = ti.pti_total_user;
            }
            pid_cpustats_internal * cpu = &stats[pids[i]].cpu;
            cpu->last_sys_time = cpu->sys_time;
            cpu->last_user_time = cpu->user_time;
            cpu->sys_time = ti.pti_total_system;
            cpu->user_time = ti.pti_total_user;
            
            cpu->sys_pct = (float)((cpu->sys_time - cpu->last_sys_time)/(refresh_rate*10000000.0));
            cpu->user_pct = (float)((cpu->user_time - cpu->last_user_time)/(refresh_rate*10000000.0));
            
            // Update the network information as well
            pid_netstats_internal * net = &stats[pids[i]].net;
            netmon->copyStats(pids[i], net);
        }
        
        delete[] pids;
    });
    dispatch_resume(timer);
    
    cleaning_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(cleaning_timer, DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC, 0);
    dispatch_source_set_event_handler(cleaning_timer, ^{
        for( auto itty = stats.begin(); itty != stats.end(); ) {
            // Try check to make sure this PID still lives!
            kill(itty->first, NULL);
            if( errno == ESRCH ) {
                netmon->killPID(itty->first);
                itty = stats.erase(itty);
            } else
                itty++;
        }
    });
    dispatch_resume(cleaning_timer);
}

Monitor::~Monitor() {
    delete this->netmon;
    dispatch_suspend(timer);
    dispatch_suspend(queue);
}

pid_stats * Monitor::getStats(pid_t pid) {
    __block pid_stats * ret = NULL;
    dispatch_sync(queue, ^{
        if( stats.find(pid) != stats.end() ) {
            ret = new pid_stats();
            new(&ret->net) pid_netstats(stats[pid].net, netmon->getRefreshRate());
            ret->cpu = stats[pid].cpu;
        }
    });
    return ret;
}
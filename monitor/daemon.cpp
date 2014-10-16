#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ProcStats.h"
#include "zmq.hpp"


int main( void ) {
    Monitor mon;
    zmq::context_t ctx;
    
    // Setup ZMQ communication socket
    zmq::socket_t comm(ctx, ZMQ_REP);
    zmq::message_t msg;
    comm.bind("ipc:///tmp/mundo.sock");
    
    while( true ) {
        // Receive a message, which is just a PID to get info about
        try {
            comm.recv(&msg);
        } catch (zmq::error_t e) {
        }
        
        if( msg.size() == sizeof(pid_t) ) {
            pid_t pid;
            memcpy( &pid, msg.data(), sizeof(pid_t) );
            
            //printf("Asking about pid %d...\n", pid);
            pid_stats * pstat = mon.getStats(pid);
            if( pstat ) {
                //printf("%u %lu %lu\n", pid, pstat->net.downPerSec, pstat->net.upPerSec);
                comm.send(pstat, sizeof(pid_stats));
                delete pstat;
            }
            else
                comm.send(NULL, 0);
        } else {
            printf("Malformed request!  Size == %lu, not %lu!\n", msg.size(), sizeof(pid_t));
        }
        
        /*
        pid_t pid = 7186;
        pid_stats * pstat = mon.getStats(pid);
        if( pstat )
            printf("[%5d] %s <%ld, %ld> [%ld %ld] %.2f%%\n", pid, mon.getPIDName(pid), pstat->net.downPerInterval, pstat->net.upPerInterval, pstat->net.totalDown, pstat->net.totalUp, pstat->cpu.sys_pct + pstat->cpu.user_pct );
        delete pstat;
        */
    }
    
    // Cleanup ZMQ
    comm.close();
    ctx.close();
    return 0;
}
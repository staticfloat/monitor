#include <stdio.h>
#include <dtrace.h>
#include <libproc.h>

int dtrace_dcmdbuffered(const dtrace_bufdata_t *bufdata, void *arg) {
    printf("%s", bufdata->dtbda_buffered);
    return(DTRACE_HANDLE_OK);
}

int chew(const dtrace_probedata_t *data, void *arg) {
    return (DTRACE_CONSUME_THIS);
}

int chewrec(const dtrace_probedata_t *data, const dtrace_recdesc_t *rec, void *arg) {
    if (rec == NULL)
        return (DTRACE_CONSUME_NEXT);
    return (DTRACE_CONSUME_THIS);
}


int main( void ) {
    int err;
    dtrace_hdl_t * dh = dtrace_open(DTRACE_VERSION, 0, &err);
    
    if (dh == NULL) {
        printf("Cannot open dtrace library: %s\n", dtrace_errmsg(dh, err));
        return -1;
    }
    
    // Set a few necessary options
    dtrace_setopt(dh, "strsize", "4096");
    dtrace_setopt(dh, "bufsize", "4m");
    
    // Compile my program
    const char * script = "syscall::*exit*:entry { printf(\"%s %d\\n\", execname, curpsinfo->pr_pid); }";
    dtrace_prog_t *prog = dtrace_program_strcompile(dh, script, DTRACE_PROBESPEC_NAME, 0, 0, NULL);
    if( prog == NULL ) {
        printf("Cannot compile program: %s\n", dtrace_errmsg(dh, err));
        return -1;
    }
    
    // Add buffered output handler, as this is the only way I can get data out of Dtrace?
    if (dtrace_handle_buffered(dh, dtrace_dcmdbuffered, NULL) == -1) {
        printf("Couldn't add buffered handler");
        return -1;
    }

    dtrace_proginfo_t info;
    if( dtrace_program_exec(dh, prog, &info) == -1 ) {
        printf("Cannot exec program: %s\n", dtrace_errmsg(dh, err));
        return -1;
    }
    
    if( dtrace_go(dh) == -1 ) {
        printf("Cannot go: %s\n", dtrace_errmsg(NULL, err));
        return -1;
    }
    
    while(1) {
        dtrace_sleep(dh);
        switch (dtrace_work(dh, NULL, chew, chewrec, NULL)) {
            case DTRACE_WORKSTATUS_DONE:
                if (dtrace_stop(dh) == -1)
                    printf("couldn't stop tracing");
                break;
                break;
            case DTRACE_WORKSTATUS_OKAY:
                break;
            case DTRACE_WORKSTATUS_ERROR:
                printf("WARNING: DTRACE_WORKSTATUS_ERROR!\n");
                break;
            default:
                printf("Unknown return value from dtrace_work()\n");
        }
    }
    
    dtrace_close(dh);
    return 0;
}
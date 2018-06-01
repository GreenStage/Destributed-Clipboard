#include <stdlib.h>
#include <time.h>
#include "../thread/rwlock.h"

unsigned time_started;
rw_lock time_lock;

unsigned time_m_local(){
    unsigned t = (unsigned) time(NULL);
    return t;
}

void time_m_sync(unsigned now){
    unsigned real_now = time(NULL);

    rw_lock_wlock(time_lock);
    time_started = real_now - now;
    rw_lock_wunlock(time_lock);
}

unsigned time_m_now(){
    rw_lock_rlock(time_lock);
    unsigned t = (unsigned) time(NULL) - time_started;
    rw_lock_runlock(time_lock);
    return t;
}

int time_init(){
    time_started = 0;
    return rw_lock_init(&time_lock);
}

int time_finish(){
    time_started = 0;
    return rw_lock_destroy(time_lock);
}

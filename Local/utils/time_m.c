#include <time.h>

unsigned started_at = 0;

unsigned time_m_local(){
    unsigned t = (unsigned) time(NULL);
    return t;
}

void time_m_sync(unsigned now){
    unsigned real_now;
    real_now = time(NULL);
    started_at = real_now - now;
}

int time_m_now(){
    unsigned t = (unsigned) time(NULL) - started_at;
    return t;
}
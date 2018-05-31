#ifndef TIME_CLIP_HEADER
#define TIME_CLIP_HEADER

unsigned time_m_local();
void time_m_sync(unsigned now);
unsigned time_m_now();
int time_init();
int time_finish();

#endif
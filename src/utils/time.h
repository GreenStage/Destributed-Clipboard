#ifndef TIME_CLIP_HEADER
#define TIME_CLIP_HEADER

/* Returns local machine current time */
unsigned time_m_local();

/* Synchronizes the clock with the value referenced by 'now' */
void time_m_sync(unsigned now);

/* Returns network syncronized time */
unsigned time_m_now();

/* Initializes network time */
int time_init();

/* Destroys network time*/
int time_finish();

#endif

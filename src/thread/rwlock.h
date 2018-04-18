#ifndef RWLOCK_HEADER
#define RWLOCK_HEADER
typedef struct rw_lock_ * rw_lock;

#define ERR_LOCK_NULL -2

int rw_lock_init(rw_lock lock);
int rw_lock_destroy(rw_lock lock);
int rw_lock_rlock(rw_lock lock);
int rw_lock_wlock(rw_lock lock);
int rw_lock_runlock(rw_lock lock);
int rw_lock_wunlock(rw_lock lock);
int rw_lock_wait_update(rw_lock lock);
#endif
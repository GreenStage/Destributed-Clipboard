#ifndef RWLOCK_HEADER
#define RWLOCK_HEADER
typedef struct rw_lock_ * rw_lock;

/*Initializes a read/write lock*/
int rw_lock_init(rw_lock *lock);

/*Destroys a read/write lock*/
int rw_lock_destroy(rw_lock lock);

/*Locks for reading*/
int rw_lock_rlock(rw_lock lock);

/*Locks for writing*/
int rw_lock_wlock(rw_lock lock);

/*Unlocks reading lock*/
int rw_lock_runlock(rw_lock lock);

/*Unlocks writing lock*/
int rw_lock_wunlock(rw_lock lock);

/*Blocks until a writing lock as been released*/
int rw_lock_wait_update(rw_lock lock);
#endif

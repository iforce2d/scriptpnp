#include "scopelock.h"

ScopeLock::ScopeLock(pthread_mutex_t *m)
{
    mutex = m;
    pthread_mutex_lock(m);
}

ScopeLock::~ScopeLock()
{
    pthread_mutex_unlock(mutex);
}

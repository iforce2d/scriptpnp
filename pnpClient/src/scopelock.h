#ifndef SCOPELOCK_H
#define SCOPELOCK_H

#include <pthread.h>

class ScopeLock
{
    pthread_mutex_t* mutex;

public:
    ScopeLock(pthread_mutex_t* m);
    ~ScopeLock();
};

#endif // SCOPELOCK_H

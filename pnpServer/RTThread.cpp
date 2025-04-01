
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <stdexcept>
#include <cstring>

#include "rpispi.h"
#include "RTThread.h"

bool sigInt = false;
bool sigIntRT = false;

void rtLoop();

void* RTThread::RunThread(void* data) {
    RTThread* thread = static_cast<RTThread*>(data);
    thread->Run();
    return NULL;
}

RTThread::RTThread(int priority, int policy, int64_t period_ns = 1000000)
    : priority_(priority), policy_(policy), period_ns_(period_ns)
{
}

//RTThread::~Thread() = default;

void RTThread::Start() {
    pthread_attr_t attr;

    // Initialize the pthread attribute
    int ret = pthread_attr_init(&attr);
    if (ret) {
        throw std::runtime_error(std::string("error in pthread_attr_init: ") + std::strerror(ret));
    }

    // Set the scheduler policy
    ret = pthread_attr_setschedpolicy(&attr, policy_);
    if (ret) {
        throw std::runtime_error(std::string("error in pthread_attr_setschedpolicy: ") + std::strerror(ret));
    }

    // Set the scheduler priority
    struct sched_param param;
    param.sched_priority = priority_;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret) {
        throw std::runtime_error(std::string("error in pthread_attr_setschedparam: ") + std::strerror(ret));
    }

    // Make sure threads created using the thread_attr_ takes the value from the attribute instead of inherit from the parent thread.
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret) {
        throw std::runtime_error(std::string("error in pthread_attr_setinheritsched: ") + std::strerror(ret));
    }

    ret = pthread_create(&thread_, &attr, &RTThread::RunThread, this);
    if (ret) {
        throw std::runtime_error(std::string("error in pthread_create: ") + std::strerror(ret));
    }
}

void RTThread::Run() noexcept {
    // Need to initialize the next_wakeup_time_ so it can be used to calculate
    // the first wakeup time after the program starts.
    clock_gettime(CLOCK_MONOTONIC, &next_wakeup_time_);

    //pid_t threadId = syscall(__NR_gettid);
    //printf("RTThread id = %d\n", threadId);

    // Map the RPi BCM2835 peripherals - uses "rtapi_open_as_root" in place of "open"
    /*if (!rt_bcm2835_init())
    {
        printf("rt_bcm2835_init failed. Are you running as root?\n");
        return;
    }

    if ( ! bcm2835_setupSPI() ) {
        printf("bcm2835_setupSPI failed.\n");
        return;
    }*/

    if ( ! rpispi_init() ) {
        printf("RTThread returning!\n");
        return;
    }

    while ( ! sigIntRT ) {
        Loop();
        next_wakeup_time_ = AddTimespecByNs(next_wakeup_time_, period_ns_);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wakeup_time_, NULL);
    }

    printf("RTThread exiting\n");
}

void RTThread::Loop() noexcept {
    // RT loop iteration code here.
    //printf("Running...\n");
    rtLoop();
}

int RTThread::Join() {
    return pthread_join(thread_, NULL);
}

struct timespec RTThread::AddTimespecByNs(struct timespec ts, int64_t ns) {
    ts.tv_nsec += ns;

    while (ts.tv_nsec >= 1000000000) {
        ++ts.tv_sec;
        ts.tv_nsec -= 1000000000;
    }

    while (ts.tv_nsec < 0) {
        --ts.tv_sec;
        ts.tv_nsec += 1000000000;
    }

    return ts;
}





#ifndef RTTHREAD_H
#define RTTHREAD_H

extern bool sigInt;
extern bool sigIntRT;

class RTThread {

  int priority_;
  int policy_;

  int64_t         period_ns_;
  struct timespec next_wakeup_time_;

  pthread_t thread_;

  static void* RunThread(void* data);
  struct timespec AddTimespecByNs(struct timespec ts, int64_t ns);

 public:
  RTThread(int priority, int policy, int64_t period_ns);
  virtual ~RTThread() = default;

  void Start();
  virtual void Run() noexcept;
  virtual void Loop() noexcept;
  int Join();

};

#endif

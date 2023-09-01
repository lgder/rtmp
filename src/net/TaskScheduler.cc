/// @file TaskScheduler.h
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/28

#include "TaskScheduler.h"

#include <signal.h>

TaskScheduler::TaskScheduler(int id)
    : id_(id),
      is_shutdown_(false),
      wakeup_pipe_(new Pipe()),
      trigger_events_(new RingBuffer<TriggerEvent>(kMaxTriggetEvents)) {
  static std::once_flag flag;
  std::call_once(flag, [] {

  });

  if (wakeup_pipe_->Create()) {
    wakeup_channel_.reset(new Channel(wakeup_pipe_->Read()));
    wakeup_channel_->EnableReading();
    wakeup_channel_->SetReadCallback([this]() { this->Wake(); });
  }
}

TaskScheduler::~TaskScheduler() {}

void TaskScheduler::Start() {
  // 忽略系统信号, 防止线程崩溃导致进程退出, 但也可能导致后续运行异常且影响调试
  ::signal(SIGPIPE, SIG_IGN);
  ::signal(SIGQUIT, SIG_IGN);
  ::signal(SIGUSR1, SIG_IGN);
  ::signal(SIGTERM, SIG_IGN);
  ::signal(SIGKILL, SIG_IGN);

  is_shutdown_ = false;
  // 事件循环, 这个才是真正的 muduo 中的 EventLoop
  while (!is_shutdown_) {
    this->HandleTriggerEvent();
    this->timer_queue_.HandleTimerEvent();
    // 获取定时列中下一个事件的剩余时间作为阻塞等待的时间, 队列为空时候 epoll 一直阻塞等待事件到达
    // 阻塞的这段时间内可被 wakeup_pipe 上的可读事件唤醒
    int64_t timeout = this->timer_queue_.GetTimeRemaining();
    this->HandleEvent((int)timeout);
  }
}

void TaskScheduler::Stop() {
  is_shutdown_ = true;
  char event = kTriggetEvent;
  wakeup_pipe_->Write(&event, 1);
}

TimerId TaskScheduler::AddTimer(TimerEvent timerEvent, uint32_t msec) {
  int64_t timeout = timer_queue_.GetTimeRemaining();
  TimerId id = timer_queue_.AddTimer(timerEvent, msec);
  // 唤醒条件: 队列为空或者快于队列中所有元素触发
  // 唤醒之后都没有事件的话会空转一轮调整 I/O 多路复用的阻塞等待时间
  if (timeout == -1 || timeout > msec) {
    char event = kTimerEvent;
    wakeup_pipe_->Write(&event, 1);
  }
  return id;
}

void TaskScheduler::RemoveTimer(TimerId timerId) { timer_queue_.RemoveTimer(timerId); }

bool TaskScheduler::AddTriggerEvent(TriggerEvent callback) {
  if (trigger_events_->Size() < kMaxTriggetEvents) {
    std::lock_guard<std::mutex> lock(mutex_);
    char event = kTriggetEvent;
    trigger_events_->Push(std::move(callback));
    wakeup_pipe_->Write(&event, 1);
    return true;
  }

  return false;
}

void TaskScheduler::Wake() {
  // 唤醒之后需要取走 pipe 上的数据, 否则 epoll LT 模式下可读事件将一直在
  char event[10] = {0};
  while (wakeup_pipe_->Read(event, 10) > 0)
    ;
}

void TaskScheduler::HandleTriggerEvent() {
  do {
    TriggerEvent callback;
    if (trigger_events_->Pop(callback)) {
      callback();
    }
  } while (trigger_events_->Size() > 0);
}

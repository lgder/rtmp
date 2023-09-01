/// @file Timer.cc
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/29

#include "Timer.h"

#include <iostream>

TimerId TimerQueue::AddTimer(const TimerEvent& event, uint32_t ms) {
  // std::lock_guard<std::mutex> lock(mutex_);

  int64_t timeout = GetTimeNow();
  TimerId timer_id = ++last_timer_id_;

  auto timer = std::make_shared<Timer>(event, ms);
  timer->SetNextTimeout(timeout);
  timers_.emplace(timer_id, timer);
  events_.emplace(std::pair<int64_t, TimerId>(timeout + ms, timer_id), std::move(timer));
  return timer_id;
}

void TimerQueue::RemoveTimer(TimerId timerId) {
  // std::lock_guard<std::mutex> lock(mutex_);

  auto iter = timers_.find(timerId);
  if (iter != timers_.end()) {
    int64_t timeout = iter->second->getNextTimeout();
    events_.erase(std::pair<int64_t, TimerId>(timeout, timerId));
    timers_.erase(timerId);
  }
}

int64_t TimerQueue::GetTimeNow() {
  auto time_point = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
}

int64_t TimerQueue::GetTimeRemaining() {
  // std::lock_guard<std::mutex> lock(mutex_);

  if (timers_.empty()) {
    return -1;
  }

  int64_t msec = events_.begin()->first.first - GetTimeNow();
  if (msec < 0) {
    msec = 0;
  }

  return msec;
}

void TimerQueue::HandleTimerEvent() {
  if (!timers_.empty()) {
    // std::lock_guard<std::mutex> lock(mutex_);
    int64_t timePoint = GetTimeNow();
    while (!timers_.empty() && events_.begin()->first.first <= timePoint) {
      TimerId timerId = events_.begin()->first.second;
      bool flag = events_.begin()->second->event_callback_();
      if (flag) {
        events_.begin()->second->SetNextTimeout(timePoint);
        auto timerPtr = std::move(events_.begin()->second);
        events_.erase(events_.begin());
        events_.emplace(std::pair<int64_t, TimerId>(timerPtr->getNextTimeout(), timerId), timerPtr);
      } else {
        events_.erase(events_.begin());
        timers_.erase(timerId);
      }
    }
  }
}

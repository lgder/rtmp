/// @file Timer.h
/// @brief 定时器任务封装
/// @version 0.1
/// @author lq
/// @date 2023/04/29

#ifndef RTMP_SERVER_TCP_TIMER_H
#define RTMP_SERVER_TCP_TIMER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

typedef std::function<bool(void)> TimerEvent;
typedef uint32_t TimerId;

class Timer {
 public:
  Timer(TimerEvent event, uint32_t msec) : event_callback_(event), interval_(msec) {
    if (interval_ == 0) {
      interval_ = 1;
    }
  }

  void SetEventCallback(const TimerEvent& event) { event_callback_ = event; }

  void Start(int64_t microseconds, bool repeat = false) {
    is_repeat_ = repeat;
    auto time_begin = std::chrono::high_resolution_clock::now();
    int64_t elapsed = 0;

    do {
      std::this_thread::sleep_for(std::chrono::microseconds(microseconds - elapsed));
      time_begin = std::chrono::high_resolution_clock::now();
      if (event_callback_) {
        event_callback_();
      }
      elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - time_begin)
                    .count();
      if (elapsed < 0) {
        elapsed = 0;
      }

    } while (is_repeat_);
  }

  void Stop() { is_repeat_ = false; }

  static void SleepMilliseconds(uint32_t msec) {
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
  }

 private:
  friend class TimerQueue;

  void SetNextTimeout(int64_t time_point) { next_timeout_ = time_point + interval_; }

  int64_t getNextTimeout() const { return next_timeout_; }

  bool is_repeat_ = false;
  TimerEvent event_callback_ = [] { return false; };
  uint32_t interval_ = 0;  // 定时器间隔, 单位 ms
  int64_t next_timeout_ = 0;  // 定时器触发时间, 下一次超时时间, 即当前时间 + 定时器间隔
};

// 命名虽然是队列但是内部用的有序的 map, 按触发时间升序排序.
class TimerQueue {
 public:
  TimerId AddTimer(const TimerEvent& event, uint32_t msec);
  void RemoveTimer(TimerId timerId);

  /// @brief 获取队头元素剩余触发时间
  /// @return int64_t 队为空返回 -1, 作为 epoll_wait 的参数传入
  int64_t GetTimeRemaining();
  void HandleTimerEvent();

 private:
  int64_t GetTimeNow();

  // 考虑到 Timer 只用在 TaskScheduler 中, 这里先取消加锁提升性能.
  // 如有其他多线程下使用的需求, 再改回或者使用外部调用函数的时候加锁的方式
  // std::mutex mutex_;

  std::unordered_map<TimerId, std::shared_ptr<Timer>> timers_;

  // 按触发时间升序排序, 时间相同的时候按照 TimerId 升序排序, 
  // TimerId 是递增的, 所以相当于 FIFO, 有队列性质
  std::map<std::pair<int64_t, TimerId>, std::shared_ptr<Timer>> events_;
  uint32_t last_timer_id_ = 0;
};

#endif  // RTMP_SERVER_TCP_TIMER_H
/// @file TaskScheduler.h
/// @brief 任务调度器抽象类, 作为 epoll, poll, selcet 等多路复用封装的派生类的基类,
///        多线程情况下是 Muduo 库 EventLoop 对应事件循环, 
///        事件循环中实现 触发事件, 定时事件和 I/O 事件的轮流处理 
/// @version 0.1
/// @author lq
/// @date 2023/04/28

#ifndef RTMP_SERVER_TASK_SCHEDULER_H
#define RTMP_SERVER_TASK_SCHEDULER_H

#include "Channel.h"
#include "Pipe.h"
#include "RingBuffer.h"
#include "Timer.h"

typedef std::function<void(void)> TriggerEvent;

class TaskScheduler {
 public:
  TaskScheduler(int id = 1);
  virtual ~TaskScheduler();

  void Start();
  void Stop();
  TimerId AddTimer(TimerEvent timerEvent, uint32_t msec);
  void RemoveTimer(TimerId timerId);

  // 当存储 TriggerEvent 的 Ring Buffer 满了之后返回 false 表示失败
  bool AddTriggerEvent(TriggerEvent callback);

  // I/O 读写事件注册, I/O 多路复用子类实现
  virtual void UpdateChannel(ChannelPtr channel) = 0;
  
  // I/O 读写事件移除, I/O 多路复用子类实现
  virtual void RemoveChannel(ChannelPtr& channel) = 0;

  // timeout: poll 和 epoll_wait 阻塞等待的时间, 单位 ms 
  virtual bool HandleEvent(int timeout) = 0;

  int GetId() const { return id_; }

 protected:
  // 在等待 I/O 事件时, 添加 Trigger 事件或 Timer 事件(部分情况下)需要从阻塞中唤醒
  void Wake();
  void HandleTriggerEvent();

  int id_ = 0;
  std::atomic_bool is_shutdown_;

  // 其他类可通过 wakeup 管道可读事件唤醒 TaskScheduler 执行任务
  std::unique_ptr<Pipe> wakeup_pipe_;
  std::shared_ptr<Channel> wakeup_channel_;
  std::unique_ptr<RingBuffer<TriggerEvent>> trigger_events_;

  std::mutex mutex_;
  TimerQueue timer_queue_;  // 内部取消了锁

  static const char kTriggetEvent = 1;  // 写管道时标识来源为触发事件
  static const char kTimerEvent = 2;  // 写管道时标识来源为定时事件
  static const int kMaxTriggetEvents = 50000;
};

#endif  // RTMP_SERVER_TASK_SCHEDULER_H

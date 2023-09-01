/// @file EventLoop.h
/// @brief 单线程或多线程的 Reactor 模型核心类
/// @version 0.1
/// @author lq
/// @date 2023/04/28
/// @note Muduo 中每个线程只能存在一个 EventLoop, 然后多线程使用 EventLoopPool, 相当于线程池,
///       而这里在单线程时,  EventLoop 在只有一个 TaskScheduler 的情况下和 Muduo 中相同, 是线程和 EventLoop 一对一的关系.
///       多线程的时候, 这里不使用 EventLoopPool 而是直接在当前的 EventLoop 类中创建多个线程, 每个线程更小 TaskScheduler 作为原来的 EventLoop, 
///       内部使用一个数组来存 n 个 TaskScheduler, 每个 TaskScheduler 对应一个线程也用数组存.
///       当新连接到达负责监听新连接的主 Reactor 时, 数组模拟环形队列按序取一个线程的 TaskScheduler 创建连接, 
///       TaskScheduler 相当于原来 Muduo 中的 EventLoop, 用于处理连接的事件,
///       然后保留 0 号 TaskScheduler 及所在线程用于处理 Timer, TriggerEvent, Channel 的处理, 
///       因为这些事件不需要在多个线程中负载均衡, 也不需要 epoll, 只需要一个线程处理即可.


#ifndef RTMP_SERVER_EVENT_LOOP_H
#define RTMP_SERVER_EVENT_LOOP_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "EpollTaskScheduler.h"
#include "PollTaskScheduler.h"
#include "Pipe.h"
#include "RingBuffer.h"
#include "Timer.h"

class EventLoop {
 public:
  enum IO_MULTIPLEXING_MODE {
    IO_MULTIPLEXING_EPOLL,
    IO_MULTIPLEXING_POLL,
  };
  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;
  // 多线程则传入 std::thread::hardware_concurrency()
  EventLoop(uint32_t num_threads = 1, 
            IO_MULTIPLEXING_MODE mode = IO_MULTIPLEXING_EPOLL);  
  virtual ~EventLoop();

  std::shared_ptr<TaskScheduler> GetTaskScheduler();

  bool AddTriggerEvent(TriggerEvent callback);
  TimerId AddTimer(TimerEvent timerEvent, uint32_t msec);
  void RemoveTimer(TimerId timerId);
  void UpdateChannel(ChannelPtr channel);
  void RemoveChannel(ChannelPtr& channel);

  // 主循环, 创建 num_threads_ 个线程并在每个线程中创建一个 TaskScheduler
  void Loop();

  // 回收所有线程和 TaskScheduler
  void Quit();

 private:
  std::mutex mutex_;
  uint32_t num_threads_ = 1;
  const IO_MULTIPLEXING_MODE kIoMultiplexingMode_;
  // Round Robin算法做负载均衡, 表示当前待接收任务的 task_scheduler 下标, 
  // 初始化为 1, 因为多线程情况下 0 线程是 Server 线程, 其他为工作线程
  uint32_t index_ = 1;
  std::vector<std::shared_ptr<TaskScheduler>> task_schedulers_;  // 注意加锁
  std::vector<std::shared_ptr<std::thread>> threads_;  // 注意加锁
};

#endif  // RTMP_SERVER_EVENT_LOOP_H

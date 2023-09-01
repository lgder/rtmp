/// @file EpollTaskScheduler.h
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/28

#ifndef RTMP_SERVER_EPOLL_TASK_SCHEDULER_H
#define RTMP_SERVER_EPOLL_TASK_SCHEDULER_H

#include <mutex>
#include <unordered_map>

#include "TaskScheduler.h"

class EpollTaskScheduler : public TaskScheduler {
 public:
  EpollTaskScheduler(int id = 0);
  ~EpollTaskScheduler() override;

  // 注册读写事件, channel 没有事件时 从 channels 中删除 channel, 否则更新/添加 channel
  void UpdateChannel(ChannelPtr channel) override;
  void RemoveChannel(ChannelPtr& channel) override;

  // timeout: ms
  bool HandleEvent(int timeout) override;

 private:
  void Update(int operation, ChannelPtr& channel);

  int epollfd_ = -1;
  std::mutex mutex_;
  std::unordered_map<int, ChannelPtr> channels_;  // <fd, channel>
};

#endif  // RTMP_SERVER_EPOLL_TASK_SCHEDULER_H

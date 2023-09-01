/// @file PollTaskScheduler.h
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/04/28

#ifndef RTMP_SERVER_POLL_TASK_SCHEDULER_H
#define RTMP_SERVER_POLL_TASK_SCHEDULER_H

#include <sys/poll.h>
#include <mutex>

#include "Socket.h"
#include "TaskScheduler.h"

class PollTaskScheduler : public TaskScheduler {
 public:
  PollTaskScheduler(int id = 0);
  ~PollTaskScheduler() override;

  void UpdateChannel(ChannelPtr channel) override;
  void RemoveChannel(ChannelPtr& channel) override;

  // timeout: ms
  bool HandleEvent(int timeout) override;

 private:
  void RemovePollfd(SOCKET fd);

  std::mutex mutex_;
  std::vector<struct pollfd> pollfds_;
  std::unordered_map<int, ChannelPtr> channels_;  // <fd, channel>

  // SOCKET maxfd_ = 0;  // 直接用 pollfds_.size() 代替
};

#endif  // RTMP_SERVER_POLL_TASK_SCHEDULER_H
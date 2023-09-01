/// @file EpollTaskScheduler.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/04/28

#include "EpollTaskScheduler.h"

#include <errno.h>
#include <sys/epoll.h>

#include <cstdio>

#include "Logger.h"

EpollTaskScheduler::EpollTaskScheduler(int id) : TaskScheduler(id) {
  epollfd_ = epoll_create(1024);  // 1024 is just a hint for the kernel

  this->UpdateChannel(wakeup_channel_);
}

EpollTaskScheduler::~EpollTaskScheduler() {}

void EpollTaskScheduler::UpdateChannel(ChannelPtr channel) {
  std::lock_guard<std::mutex> lock(mutex_);

  int fd = channel->GetSocket();
  if (channels_.find(fd) != channels_.end()) {
    if (channel->IsNoneEvent()) {
      Update(EPOLL_CTL_DEL, channel);
      channels_.erase(fd);
    } else {
      Update(EPOLL_CTL_MOD, channel);
    }
  } else {
    if (!channel->IsNoneEvent()) {
      channels_.emplace(fd, channel);
      Update(EPOLL_CTL_ADD, channel);
    }
  }
}

void EpollTaskScheduler::Update(int operation, ChannelPtr& channel) {
  struct epoll_event event = {0};

  if (operation != EPOLL_CTL_DEL) {
    event.data.ptr = channel.get();
    event.events = channel->GetEvents();
  }

  if (::epoll_ctl(epollfd_, operation, channel->GetSocket(), &event) < 0) {
    LOG_ERROR("epoll_ctl error: %s", strerror(errno));
  }
}

void EpollTaskScheduler::RemoveChannel(ChannelPtr& channel) {
  std::lock_guard<std::mutex> lock(mutex_);

  int fd = channel->GetSocket();

  if (channels_.find(fd) != channels_.end()) {
    Update(EPOLL_CTL_DEL, channel);
    channels_.erase(fd);
  }
}

bool EpollTaskScheduler::HandleEvent(int timeout) {
  struct epoll_event events[512] = {{0}};
  int num_events = -1;

  num_events = epoll_wait(epollfd_, events, 512, timeout);
  if (num_events < 0) {
    if (errno != EINTR) {
      return false;
    }
  }

  // 通过 Channel 调用相应事件的回调函数
  for (int i = 0; i < num_events; i++) {
    if (events[i].data.ptr) {
      ((Channel*)events[i].data.ptr)->HandleEvent(events[i].events);
    }
  }
  return true;
}

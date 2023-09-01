/// @file PollTaskScheduler.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/04/28

#include "PollTaskScheduler.h"

#include <algorithm>
#include <forward_list>
#include <mutex>

#include "Socket.h"

PollTaskScheduler::PollTaskScheduler(int id) : TaskScheduler(id) {
  pollfds_.push_back({ wakeup_channel_->GetSocket(), static_cast<short>(wakeup_channel_->GetEvents()), 0});
  channels_.emplace(wakeup_channel_->GetSocket(), wakeup_channel_);
}

PollTaskScheduler::~PollTaskScheduler() {}

void PollTaskScheduler::UpdateChannel(ChannelPtr channel) {
  std::lock_guard<std::mutex> lock(mutex_);

  SOCKET fd = channel->GetSocket();

  if (channels_.find(fd) != channels_.end()) {
    if (channel->IsNoneEvent()) {
      channels_.erase(fd);
      RemovePollfd(fd);
    } else {
      channels_[fd] = channel;
      for (auto& pollfd : pollfds_) {
        if (pollfd.fd == fd) {
          pollfd.fd = channel->GetSocket();
          pollfd.events = channel->GetEvents();
          break;
        }
      }
    }
  } else {
    if (!channel->IsNoneEvent()) {
      channels_.emplace(fd, channel);
      pollfds_.push_back({fd, static_cast<short>(channel->GetEvents()), 0});
    }
  }
}

void PollTaskScheduler::RemoveChannel(ChannelPtr& channel) {
  std::lock_guard<std::mutex> lock(mutex_);

  SOCKET fd = channel->GetSocket();

  if (channels_.find(fd) != channels_.end()) {
    channels_.erase(fd);
    RemovePollfd(fd);
  }
}

void PollTaskScheduler::RemovePollfd(SOCKET fd) {
  auto it =
      std::find_if(pollfds_.begin(), pollfds_.end(), [fd](pollfd _fd) { return _fd.fd == fd; });
  *it = pollfds_.back();
  pollfds_.pop_back();
}

bool PollTaskScheduler::HandleEvent(int timeout) {
  int num_events = ::poll(&*pollfds_.begin(), pollfds_.size(), timeout);
  if (num_events < 0 && errno != EINTR) {
    return false;
  }
  if (num_events > 0) {
    // 不能直接在这里处理事件，因为在处理事件的过程中可能会修改 pollfds_, 因此添加和执行分开
    std::forward_list<std::pair<ChannelPtr, short>> active_events;

    for (auto& pollfd : pollfds_) {
      if (pollfd.revents > 0) {
        active_events.emplace_front(channels_[pollfd.fd], pollfd.revents);
      }
    }

    for (auto& it : active_events) {
      it.first->HandleEvent(it.second);
    }
  }
  return true;
}

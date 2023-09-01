/// @file Channel.h
/// @brief selectable IO channel, 负责注册与响应 IO 事件.
/// @version 0.1
/// @author lq
/// @date 2023/04/28
/// @note 注意它不拥有 fd, 它是 Acceptor, Connector, EventLoop, TcpConnection 的成员,
///       生命期由后者控制.

#ifndef RTMP_SERVER_CHANNEL_H
#define RTMP_SERVER_CHANNEL_H

#include <functional>
#include <memory>

#include "Socket.h"

// 和 poll 接口对应, 每个二进制位代表一个独立的事件类型, 多个出现的时候就是 `& 相应位` 来判断
enum EventType {
  EVENT_NONE = 0,
  EVENT_IN = 1,       // 数据可读
  EVENT_PRI = 2,      // 优先级数据可读
  EVENT_OUT = 4,      // 数据可写
  EVENT_ERR = 8,      // 错误 0x008
  EVENT_HUP = 16,     // 挂起 0x010
  EVENT_RDHUP = 8192  // 对端关闭连接或者 shutdown 写端
};

class Channel {
 public:
  typedef std::function<void()> EventCallback;

  Channel() = delete;

  Channel(SOCKET sockfd) : sockfd_(sockfd) {}

  virtual ~Channel(){};

  void SetReadCallback(const EventCallback& cb) { read_callback_ = cb; }

  void SetWriteCallback(const EventCallback& cb) { write_callback_ = cb; }

  void SetCloseCallback(const EventCallback& cb) { close_callback_ = cb; }

  void SetErrorCallback(const EventCallback& cb) { error_callback_ = cb; }

  SOCKET GetSocket() const { return sockfd_; }

  int GetEvents() const { return events_; }
  void SetEvents(int events) { events_ = events; }

  void EnableReading() { events_ |= EVENT_IN; }

  void EnableWriting() { events_ |= EVENT_OUT; }

  void DisableReading() { events_ &= ~EVENT_IN; }

  void DisableWriting() { events_ &= ~EVENT_OUT; }

  bool IsNoneEvent() const { return events_ == EVENT_NONE; }
  bool IsWriting() const { return (events_ & EVENT_OUT) != 0; }
  bool IsReading() const { return (events_ & EVENT_IN) != 0; }

  // fd 上事件分发
  void HandleEvent(int events) {
    if (events & (EVENT_PRI | EVENT_IN)) {
      read_callback_();
    }

    if (events & EVENT_OUT) {
      write_callback_();
    }

    if (events & EVENT_HUP) {
      close_callback_();
      return;
    }

    if (events & (EVENT_ERR)) {
      error_callback_();
    }
  }

 private:
  EventCallback read_callback_ = [] {};
  EventCallback write_callback_ = [] {};
  EventCallback close_callback_ = [] {};
  EventCallback error_callback_ = [] {};

  SOCKET sockfd_ = 0;
  int events_ = 0;
};

typedef std::shared_ptr<Channel> ChannelPtr;

#endif  // RTMP_SERVER_CHANNEL_H

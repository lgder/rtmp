/// @file Acceptor.h
/// @brief TCP 连接接收器，用于封装 创建 fd, listen(), bind(), accept() 这些操作
/// @version 0.1
/// @author lq
/// @date 2023/04/30
/// @note 只负责连接的创建, 销毁是 TcpConnection 回调函数来做的

#ifndef RTMP_SERVER_ACCEPTOR_H
#define RTMP_SERVER_ACCEPTOR_H

#include <functional>
#include <memory>
#include <mutex>

#include "Channel.h"
#include "TcpSocket.h"

using NewConnectionCallback = std::function<void(SOCKET)>;

class EventLoop;

class Acceptor {
 public:
  Acceptor(EventLoop* eventLoop);
  virtual ~Acceptor();

  void SetNewConnectionCallback(const NewConnectionCallback& cb) { new_connection_callback_ = cb; }

  int Listen(std::string ip, uint16_t port);
  void Close();

 private:
  void OnAccept();

  EventLoop* event_loop_ = nullptr;
  std::mutex mutex_;
  std::unique_ptr<TcpSocket> tcp_socket_;
  ChannelPtr channel_ptr_;
  NewConnectionCallback new_connection_callback_;
};

#endif  // RTMP_SERVER_ACCEPTOR_H

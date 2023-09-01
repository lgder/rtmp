/// @file TcpServer.h
/// @brief 业务基于 TCP 的应用层协议的服务端的基类
/// @version 0.1
/// @author lq
/// @date 2023/04/30

#ifndef RTMP_SERVER_TCP_SERVER_H
#define RTMP_SERVER_TCP_SERVER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Socket.h"
#include "TcpConnection.h"

class Acceptor;
class EventLoop;

class TcpServer {
 public:
  TcpServer(EventLoop* event_loop);
  virtual ~TcpServer();

  virtual bool Start(std::string ip, uint16_t port);
  virtual void Stop();

  std::string GetIPAddress() const { return ip_; }

  uint16_t GetPort() const { return port_; }

 protected:
  // 连接建立之后的调用的函数, 默认实现 Round Robin 分发到下一个 TaskScheduler
  virtual TcpConnection::Ptr OnConnect(SOCKET sockfd);
  // 新连接存储, 默认是存到当前类中的连接哈希表中
  virtual void AddConnection(SOCKET sockfd, TcpConnection::Ptr tcp_conn);
  virtual void RemoveConnection(SOCKET sockfd);

  EventLoop* event_loop_;
  uint16_t port_;
  std::string ip_;
  std::unique_ptr<Acceptor> acceptor_;
  bool is_started_;
  std::mutex mutex_;
  std::unordered_map<SOCKET, TcpConnection::Ptr> connections_;
};

#endif  // RTMP_SERVER_TCP_SERVER_H

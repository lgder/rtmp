/// @file TcpSocket.h
/// @brief tcp socket 操作封装
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#ifndef RTMP_SERVER_TCP_SOCKET_H
#define RTMP_SERVER_TCP_SOCKET_H

#include <cstdint>
#include <string>

#include "Socket.h"

class TcpSocket {
 public:
  TcpSocket(SOCKET sockfd = -1);
  virtual ~TcpSocket();

  SOCKET Create();
  bool Bind(std::string ip, uint16_t port);
  bool Listen(int backlog);
  SOCKET Accept();
  bool Connect(std::string ip, uint16_t port, int timeout = 0);
  void Close();
  void ShutdownWrite();
  SOCKET GetSocket() const { return sockfd_; }

 private:
  SOCKET sockfd_ = -1;
};

#endif  // RTMP_SERVER_TCP_SOCKET_H

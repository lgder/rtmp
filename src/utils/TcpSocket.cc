/// @file TcpSocket.cc
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#include "TcpSocket.h"

#include "Logger.h"
#include "Socket.h"
#include "SocketUtil.h"

TcpSocket::TcpSocket(SOCKET sockfd) : sockfd_(sockfd) {}

TcpSocket::~TcpSocket() {}

SOCKET TcpSocket::Create() {
  sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  return sockfd_;
}

bool TcpSocket::Bind(std::string ip, uint16_t port) {
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip.c_str());
  addr.sin_port = htons(port);

  if (::bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
    LOG_DEBUG(" <socket=%d> bind <%s:%u> failed.\n", sockfd_, ip.c_str(), port);
    return false;
  }

  return true;
}

bool TcpSocket::Listen(int backlog) {
  if (::listen(sockfd_, backlog) == SOCKET_ERROR) {
    LOG_DEBUG("<socket=%d> listen failed.\n", sockfd_);
    return false;
  }

  return true;
}

SOCKET TcpSocket::Accept() {
  struct sockaddr_in addr = {0};
  socklen_t addrlen = sizeof addr;

  SOCKET socket_fd = ::accept(sockfd_, (struct sockaddr*)&addr, &addrlen);
  return socket_fd;
}

bool TcpSocket::Connect(std::string ip, uint16_t port, int timeout) {
  if (!SocketUtil::Connect(sockfd_, ip, port, timeout)) {
    LOG_DEBUG("<socket=%d> connect failed.\n", sockfd_);
    return false;
  }

  return true;
}

void TcpSocket::Close() {
  ::close(sockfd_);

  sockfd_ = 0;
}

void TcpSocket::ShutdownWrite() {
  shutdown(sockfd_, SHUT_WR);
  sockfd_ = 0;
}

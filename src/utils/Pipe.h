/// @file Pipe.h
/// @brief pipe 的 RAII 封装
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#ifndef RTMP_SERVER_PIPE_H
#define RTMP_SERVER_PIPE_H

#include "TcpSocket.h"

class Pipe {
 public:
  Pipe();
  bool Create();
  int Write(void *buf, int len);
  int Read(void *buf, int len);
  void Close();

  SOCKET Read() const { return pipe_fd_[0]; }
  SOCKET Write() const { return pipe_fd_[1]; }

 private:
  SOCKET pipe_fd_[2];
};

#endif  // RTMP_SERVER_PIPE_H

/// @file Pipe.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#include "Pipe.h"

#include <array>
#include <random>
#include <string>

#include "SocketUtil.h"

Pipe::Pipe() {}

bool Pipe::Create() {
  if (pipe2(pipe_fd_, O_NONBLOCK | O_CLOEXEC) < 0) {
    return false;
  }
  return true;
}

int Pipe::Write(void *buf, int len) { return ::write(pipe_fd_[1], buf, len); }

int Pipe::Read(void *buf, int len) { return ::read(pipe_fd_[0], buf, len); }

void Pipe::Close() {
  ::close(pipe_fd_[0]);
  ::close(pipe_fd_[1]);
}

/// @file Timestamp.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/04/29

#include "Timestamp.h"

#include <iostream>
#include <sstream>

std::string Timestamp::Localtime() {
  std::ostringstream stream;
  auto now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);

  char buffer[200] = {0};
  std::string timeString;
  // 年-月-日 时:分:秒
  std::strftime(buffer, 200, "%F %T", std::localtime(&tt));
  stream << buffer;

  return stream.str();
}
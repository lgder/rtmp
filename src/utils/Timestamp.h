/// @file Timestamp.h
/// @brief 精确时间戳封装
/// @version 0.1
/// @author lq
/// @date 2023/04/29

#ifndef RTMP_SERVER_TIMESTAMP_H
#define RTMP_SERVER_TIMESTAMP_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

class Timestamp {
 public:
  Timestamp() : begin_time_point_(std::chrono::high_resolution_clock::now()) {}

  void Reset() { begin_time_point_ = std::chrono::high_resolution_clock::now(); }

  // 返回当前对象创建时间到现在时间经过的精确的毫秒数
  int64_t Elapsed() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now() - begin_time_point_)
        .count();
  }

  // 返回当前系统时间的字符串表示, 格式为 年-月-日 时:分:秒
  static std::string Localtime();

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_point_;
};

#endif  // RTMP_SERVER_TIMESTAMP_H

/// @file Logger.cc
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/27

#include "Logger.h"

#include <cstdarg>
#include <iostream>

#include "Timestamp.h"

const char* Priority_To_String[]{"DEBUG", "STATE", "INFO", "WARNING", "ERROR"};

Logger::Logger() {}

Logger& Logger::Instance() {
  static Logger s_logger;  // C++11之后, 局部静态变量线程安全
  return s_logger;
}

Logger::~Logger() {}

void Logger::Init(char* pathname) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (pathname != nullptr) {
    ofs_.open(pathname, std::ios::out | std::ios::binary);
    if (ofs_.fail()) {
      std::cerr << "Failed to open logfile." << std::endl;
    }
  }
}

void Logger::Exit() {
  std::unique_lock<std::mutex> lock(mutex_);

  if (ofs_.is_open()) {
    ofs_.close();
  }
}

void Logger::Log(Priority priority, const char* __file, const char* __func, int __line,
                 const char* fmt, ...) {
  char buf[2048] = {0};
  sprintf(buf, "[%s][%s:%s:%d] ", Priority_To_String[priority], __file, __func, __line);
  va_list args;
  va_start(args, fmt);
  vsprintf(buf + strlen(buf), fmt, args);
  va_end(args);
  this->Write(std::string(buf));
}

void Logger::Log2(Priority priority, const char* fmt, ...) {
  char buf[4096] = {0};
  sprintf(buf, "[%s] ", Priority_To_String[priority]);
  va_list args;
  va_start(args, fmt);
  vsprintf(buf + strlen(buf), fmt, args);
  va_end(args);
  this->Write(std::string(buf));
}

void Logger::Write(std::string&& info) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (ofs_.is_open()) {
    ofs_ << "[" << Timestamp::Localtime() << "]" << info << std::endl;
  }

  std::cout << "[" << Timestamp::Localtime() << "]" << info << std::endl;
}

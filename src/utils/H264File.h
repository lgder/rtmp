/// @file H264File.h
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/06/30

#ifndef RTMP_SERVER_H264FILE_H
#define RTMP_SERVER_H264FILE_H
#include <cstdint>
#include <iostream>
#include <utility>

typedef std::pair<uint8_t *, uint8_t *> Nal;  // 两个元素是 NAL 单元起始指针和末尾指针

class H264File {
 public:
  explicit H264File(int bufSize = 5000000);
  ~H264File();

  bool open(const char *path);
  void Close();

  bool isOpened() const { return (file_ != nullptr); }

  /// @brief 从文件读取帧存到数组返回. 重复调用时读到末尾之后会从头继续读
  /// 
  /// @param in_buf 存读出的帧 
  /// @param in_buf_size 上一个参数数组大小
  /// @param is_end_frame 传入参数, 是否是该文件最后一个帧
  /// @return int 读出的字节数
  int readFrame(char *in_buf, int in_buf_size, bool *is_end_frame);

  /// @brief 从数据读 NAL 单元
  /// 
  /// @param data 
  /// @param size 
  /// @return Nal 
  static Nal findNal(const uint8_t *data, uint32_t size);

 private:
  FILE *file_ = nullptr;
  int buf_size_ = 0;
  char *buf_ = nullptr;
  int bytes_used_ = 0;
  int count_ = 0;
};
#endif  // RTMP_SERVER_H264FILE_H

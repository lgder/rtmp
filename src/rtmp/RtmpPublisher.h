/// @file RtmpPublisher.h
/// @brief 推流客户端封装
/// @version 0.1
/// @author lq
/// @date 2023/06/23

#ifndef RTMP_SERVER_RTMP_PUBLISHER_H
#define RTMP_SERVER_RTMP_PUBLISHER_H

#include <mutex>
#include <string>

#include "EventLoop.h"
#include "RtmpConnection.h"
#include "Timestamp.h"

class RtmpPublisher : public Rtmp, public std::enable_shared_from_this<RtmpPublisher> {
 public:
  static std::shared_ptr<RtmpPublisher> Create(EventLoop *loop);
  ~RtmpPublisher();

  // 设置媒体信息，并根据媒体信息初始化 AAC 和 H264 的序列头
  int SetMediaInfo(MediaInfo media_info);

  /// @brief 打开链接推流
  ///
  /// @param url rtmp url
  /// @param msec 超时时间, ms
  /// @param status 传出参数, 打开后的 rtmp 连接状态
  /// @return int 异常情况返回 -1
  int OpenUrl(std::string url, int msec, std::string &status);
  void Close();

  bool IsConnected();

  int PushVideoFrame(uint8_t *data, uint32_t size);
  int PushAudioFrame(uint8_t *data, uint32_t size);

 private:
  friend class RtmpConnection;

  RtmpPublisher(EventLoop *event_loop);
  bool IsKeyFrame(uint8_t *data, uint32_t size);

  EventLoop *event_loop_ = nullptr;
  TaskScheduler *task_scheduler_ = nullptr;
  std::mutex mutex_;
  std::shared_ptr<RtmpConnection> rtmp_conn_;

  MediaInfo media_info_;
  std::shared_ptr<char> avc_sequence_header_;
  std::shared_ptr<char> aac_sequence_header_;
  uint32_t avc_sequence_header_size_ = 0;
  uint32_t aac_sequence_header_size_ = 0;
  uint8_t audio_tag_ = 0;  // 0: aac, 1: mp3
  bool has_key_frame_ = false;  // 有了关键帧才能从 I 帧开始推流
  Timestamp timestamp_;
  uint64_t video_timestamp_ = 0;
  uint64_t audio_timestamp_ = 0;

  const uint32_t kSamplingFrequency[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                           16000, 12000, 11025, 8000,  7350,  0,     0,     0};
};

#endif  // RTMP_SERVER_RTMP_PUBLISHER_H

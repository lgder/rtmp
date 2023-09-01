/// @file rtmp.h
/// @brief rtmp 数据结构和协议规定的一些常量等
/// @version 0.1
/// @author lq
/// @date 2023/05/21

#ifndef RTMP_SERVER_RTMP_H
#define RTMP_SERVER_RTMP_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

static const int RTMP_VERSION = 0x3;
static const int RTMP_SET_CHUNK_SIZE = 0x1;    // 设置块大小
static const int RTMP_AOBRT_MESSAGE = 0X2;     // 终止消息
static const int RTMP_ACK = 0x3;               // 确认
static const int RTMP_USER_CONTROL = 0x4;      // 用户控制消息
static const int RTMP_ACK_SIZE = 0x5;          // 发送确认的窗口大小限制
static const int RTMP_BANDWIDTH_SIZE = 0x6;    // 设置对端带宽
static const int RTMP_AUDIO = 0x08;            // 音频消息
static const int RTMP_VIDEO = 0x09;            // 视频消息
static const int RTMP_FLEX_MESSAGE = 0x11;     // amf3, 暂不支持
static const int RTMP_DATA_MESSAGE = 0x12;     // 通知类型消息
static const int RTMP_COMMAND_MESSAGE = 0x14;  // amf0, 支持
static const int RTMP_FLASH_VIDEO = 0x16;      // flv 视频, 暂不支持

static const int RTMP_CHUNK_TYPE_0 = 0;  // 块消息头 11 Byte, 块流的开头
static const int RTMP_CHUNK_TYPE_1 = 1;  // 块消息头 7 Byte, 沿用上一个消息的消息流ID
static const int RTMP_CHUNK_TYPE_2 = 2;  // 块消息头 3 Byte, 沿用上一个块的消息流ID和消息长度
static const int RTMP_CHUNK_TYPE_3 = 3;  // 无块消息头, 使用上一个块相同的块流ID, 分割块除第一个都用这个

static const int RTMP_CHUNK_CONTROL_ID = 2;
static const int RTMP_CHUNK_COMMAND_ID = 3;
static const int RTMP_CHUNK_AUDIO_ID = 4;
static const int RTMP_CHUNK_VIDEO_ID = 5;
static const int RTMP_CHUNK_DATA_ID = 6;

static const int RTMP_CODEC_ID_H264 = 7;
static const int RTMP_CODEC_ID_AAC = 10;

static const int RTMP_AVC_SEQUENCE_HEADER = 0x18;
static const int RTMP_AAC_SEQUENCE_HEADER = 0x19;

struct MediaInfo {
  uint8_t video_codec_id = RTMP_CODEC_ID_H264;
  uint8_t video_framerate = 0;
  uint32_t video_width = 0;
  uint32_t video_height = 0;
  std::shared_ptr<uint8_t> sps;  // 序列参数集
  std::shared_ptr<uint8_t> pps;  // 图像参数集
  std::shared_ptr<uint8_t> sei;  // 补充增强信息
  uint32_t sps_size = 0;
  uint32_t pps_size = 0;
  uint32_t sei_size = 0;

  uint8_t audio_codec_id = RTMP_CODEC_ID_AAC;
  uint32_t audio_channel = 0;     // 声道数
  uint32_t audio_samplerate = 0;  // 采样率
  uint32_t audio_frame_len = 0;   // 不包括头信息和额外元数据的音频帧长度, 单位 Byte
  std::shared_ptr<uint8_t> audio_specific_config;  // 音频配置信息, 需存入 aac packet 中
  uint32_t audio_specific_config_size = 0;
};

class Rtmp {
 public:
  virtual ~Rtmp(){};

  void SetChunkSize(uint32_t size) {
    if (size > 0 && size <= 60000) {
      max_chunk_size_ = size;
    }
  }

  void SetGopCache(uint32_t len = 5000) { max_gop_cache_len_ = len; }

  void SetPeerBandwidth(uint32_t size) { peer_bandwidth_ = size; }

  uint32_t GetChunkSize() const { return max_chunk_size_; }

  uint32_t GetGopCacheLen() const { return max_gop_cache_len_; }

  uint32_t GetAcknowledgementSize() const { return acknowledgement_size_; }

  uint32_t GetPeerBandwidth() const { return peer_bandwidth_; }

  int ParseRtmpUrl(std::string url) {
    char ip[100] = {0};
    char streamPath[500] = {0};
    char app[100] = {0};
    char streamName[400] = {0};
    uint16_t port = 0;

    if (url.find("rtmp://") == std::string::npos) {
      return -1;
    }

    if (sscanf(url.c_str() + 7, "%[^:]:%hu/%s", ip, &port, streamPath) == 3) {
      port_ = port;
    } else if (sscanf(url.c_str() + 7, "%[^/]/%s", ip, streamPath) == 2) {
      port_ = 1935;
    } else {
      return -1;
    }

    ip_ = ip;
    stream_path_ += "/";
    stream_path_ += streamPath;
    url_ = url;

    if (sscanf(stream_path_.c_str(), "/%[^/]/%s", app, streamName) != 2) {
      return -1;
    }

    app_ = app;
    stream_name_ = streamName;
    swf_url_ = tc_url_ = std::string("rtmp://") + ip_ + ":" + std::to_string(port_) + "/" + app_;
    return 0;
  }

  std::string GetUrl() const { return url_; }

  std::string GetStreamPath() const { return stream_path_; }

  std::string GetApp() const { return app_; }

  std::string GetStreamName() const { return stream_name_; }

  std::string GetSwfUrl() const { return swf_url_; }

  std::string GetTcUrl() const { return tc_url_; }

  uint16_t port_ = 1935;
  std::string url_;
  std::string tc_url_;   // URL of the Server, protocol://servername:port/appName/appInstance
  std::string swf_url_;  // URL of the source SWF file making the connection
  std::string ip_;
  std::string app_;
  std::string stream_name_;
  std::string stream_path_;

  uint32_t peer_bandwidth_ = 5000000;
  uint32_t acknowledgement_size_ = 5000000;
  uint32_t max_chunk_size_ = 128;
  uint32_t max_gop_cache_len_ = 0;  // 为 0 时表示不启用 GOP cache
};

#endif  // RTMP_SERVER_RTMP_H

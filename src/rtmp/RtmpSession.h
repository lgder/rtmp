/// @file RtmpSession.h
/// @brief 会话封装, 一个会话对应一个 url, 包含一个 publisher 和多个 client
///        封装的操作主要是将 pushlisher 的数据转发给所有 client
/// @version 0.1
/// @author lq
/// @date 2023/06/23

#ifndef RTMP_SERVER_RTMP_SESSION_H
#define RTMP_SERVER_RTMP_SESSION_H

#include <list>
#include <memory>
#include <mutex>

#include "Socket.h"
#include "amf.h"

class RtmpConnection;
class HttpFlvConnection;

class RtmpSession {
 public:
  using Ptr = std::shared_ptr<RtmpSession>;

  RtmpSession() = default;
  virtual ~RtmpSession() = default;

  void SetMetaData(AmfObjects metaData) {
    std::lock_guard<std::mutex> lock(mutex_);
    meta_data_ = metaData;
  }

  AmfObjects GetMetaData() {
    std::lock_guard<std::mutex> lock(mutex_);
    return meta_data_;
  }

  // 向所有拉流端发送视频信息的元数据
  void SendMetaData(AmfObjects& metaData);

  // 向所有拉流端发送音视频数据 type: RTMP_AUDIO 或 RTMP_VIDEO
  void SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size);

  void SetAvcSequenceHeader(std::shared_ptr<char> avcSequenceHeader,
                            uint32_t avcSequenceHeaderSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    avc_sequence_header_ = avcSequenceHeader;
    avc_sequence_header_size_ = avcSequenceHeaderSize;
  }

  void SetAacSequenceHeader(std::shared_ptr<char> aacSequenceHeader,
                            uint32_t aacSequenceHeaderSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    aac_sequence_header_ = aacSequenceHeader;
    aac_sequence_header_size_ = aacSequenceHeaderSize;
  }

  void AddConn(std::shared_ptr<RtmpConnection> conn);
  void RemoveConn(std::shared_ptr<RtmpConnection> conn);

  std::shared_ptr<RtmpConnection> GetPublisher();
  int GetClientsNum();

  void SetGopCacheLen(uint32_t cacheLen) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_gop_cache_len_ = cacheLen;
  }

  void SaveGop(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size);
  void SendGop(std::shared_ptr<RtmpConnection> conn);

 private:
  struct AVFrame {
    uint8_t type = 0;                      // RTMP_AUDIO 或 RTMP_VIDEO
    uint64_t timestamp = 0;                // 对应绝对时间戳
    std::shared_ptr<char> data = nullptr;  // 存数据的数组指针
    uint32_t size = 0;                     // 占用字节数, data 数组大小
  };

  std::mutex mutex_;  // 多个 connection 分布在多个 thread 访问 session
  AmfObjects meta_data_;
  bool has_publisher_ = false;
  std::weak_ptr<RtmpConnection> publisher_;
  std::unordered_map<SOCKET, std::weak_ptr<RtmpConnection>> rtmp_conns_;

  std::shared_ptr<char> avc_sequence_header_;
  std::shared_ptr<char> aac_sequence_header_;
  uint32_t avc_sequence_header_size_ = 0;
  uint32_t aac_sequence_header_size_ = 0;
  uint64_t gop_index_ = 0;
  uint32_t max_gop_cache_len_ = 0;

  typedef std::shared_ptr<AVFrame> AVFramePtr;
  std::map<uint64_t, std::shared_ptr<std::list<AVFramePtr>>> gop_cache_;  // <I 帧 timestamp, GOP 帧序列列表指针>
};

#endif  // RTMP_SERVER_RTMP_SESSION_H

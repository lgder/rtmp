/// @file RtmpChunk.h
/// @brief RTMP 块操作封装
/// @version 0.1
/// @author lq
/// @date 2023/06/22
/// @note header: 基本头 - 消息头 - 扩展时间戳

#ifndef RTMP_SERVER_RTMP_CHUNK_H
#define RTMP_SERVER_RTMP_CHUNK_H

#include <map>

#include "BufferReader.h"
#include "BufferWriter.h"
#include "RtmpMessage.h"
#include "amf.h"

class RtmpChunk {
 public:
  enum State {
    PARSE_HEADER,
    PARSE_BODY,
  };

  RtmpChunk();
  ~RtmpChunk();

  // 解析 in_buffer 的块数据为 RTMP 消息, 返回 0 成功, -1 失败
  int Parse(BufferReader& in_buffer, RtmpMessage& out_rtmp_msg);

  // 将信息封装成块, 由 buf 数组传出, 返回 buf 中存的块的大小, -1 失败
  int CreateChunk(uint32_t csid, RtmpMessage& rtmp_msg, char* buf, uint32_t buf_size);

  void SetInChunkSize(uint32_t in_chunk_size) { in_chunk_size_ = in_chunk_size; }

  void SetOutChunkSize(uint32_t out_chunk_size) { out_chunk_size_ = out_chunk_size; }

  void Clear() { rtmp_messages_.clear(); }

  int GetStreamId() const { return stream_id_; }

 private:
  int ParseChunkHeader(BufferReader& buffer);
  int ParseChunkBody(BufferReader& buffer);
  int CreateBasicHeader(uint8_t fmt, uint32_t csid, char* buf);
  int CreateMessageHeader(uint8_t fmt, RtmpMessage& rtmp_msg, char* buf);

  State state_;
  int chunk_stream_id_ = 0;  // 对应的 rtmp_messages_ 的 key
  int stream_id_ = 0;  // 从块中解析的 stream id
  uint32_t in_chunk_size_ = 128;  // 接收分块大小
  uint32_t out_chunk_size_ = 128;  // 发送分块大小
  std::map<int, RtmpMessage> rtmp_messages_;  // <流id, 消息>

  const int kDefaultStreamId = 1;
  const int kChunkMessageHeaderLen[4] = {11, 7, 3, 0};  // 对应格式 0, 1, 2, 3 消息头部长度
};

#endif  // RTMP_SERVER_RTMP_CHUNK_H
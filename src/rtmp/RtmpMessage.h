/// @file RtmpMessage.h
/// @brief Rtmp消息数据结构
/// @version 0.1
/// @author lq
/// @date 2023/06/24

#ifndef RTMP_SERVER_RTMP_MESSAGE_H
#define RTMP_SERVER_RTMP_MESSAGE_H

#include <cstdint>
#include <memory>

/// chunk header: basic header + rtmp message header + extend message timestamp
/// 依据 chunk type (basic header 中 fmt字段) 不同, 分成 4 种类型
///   type-0(11 Byte):
///
///        0                   1                   2                   3
///        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///        |                 timestamp                     |message length |
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///        |      message length (cont)    |message type id| msg stream id |
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///        |            message stream id (cont)           |
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///
///
///   type-1(7 Byte): 
///
///        0                   1                   2                   3
///        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///        |                timestamp delta                |message length |
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///        |     message length (cont)     |message type id|
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///
///
///   type-2(3 Byte): 
///
///        0                   1                   2
///        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///        |               timestamp delta                 |
///        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///
///
///   type-3(0 Byte, 无消息头) 
///
/// 以下 RtmpMessageHeader 存储的是 COMMON HEADER FIELDS, 通用标头字段, 根据实际的块类型来选择其中的字段使用

struct RtmpMessageHeader {
  // type-0 使用绝对时间戳, type-1 和 type-2 使用相对时间戳
  //  如果时间戳大于或等于 16777215（十六进制 0xFFFFFF）,
  //  则该字段必须为 16777215，表明存在扩展时间戳字段来编码完整的 32 位时间戳,
  //  否则该字段应该是整个时间戳
  uint8_t timestamp[3];
  
  // 消息长度（3 Byte）：对于 type-0 或 type-1 块，此处发送消息的长度。
  // 注意, 这通常与块 payload 的长度不同。
  // 块 payload 长度是除最后一个块之外的所有块的最大块大小，以及最后一个块的剩余部分
  uint8_t length[3];

  // 消息类型 ID（1 字节）：对于 type-0 或 type-1 块，此处发送消息的类型
  uint8_t type_id;

  // 对于 type-0 的块，存储消息流ID, 小端序
  uint8_t stream_id[4];
};

struct RtmpMessage {
  uint32_t timestamp_delta = 0;     // 相对时间戳, 块中 3 Byte
  uint32_t extend_timestamp = 0;    // 扩展时间戳, 块中 4 Byte
  uint64_t absolute_timestamp = 0;  // 绝对时间戳, 计算用

  uint8_t type_id = 0;     // 类型 id, 1 Byte
  uint32_t length = 0;     // 消息有效负载 payload 的长度, 3 Byte, 大端
  uint32_t stream_id = 0;  // 消息流 id, 3 Byte, 大端

  uint8_t csid = 0;  // Chunk Stream ID
  std::shared_ptr<char> payload = nullptr;  // 消息有效负载, 存实际数据
  uint32_t index = 0;  // payload 的当前正处理的下标

  void Clear() {
    index = 0;
    timestamp_delta = 0;
    extend_timestamp = 0;
    if (length > 0) {
      payload.reset(new char[length], std::default_delete<char[]>());
    }
  }

  bool IsCompleted() const {
    if (index == length && length > 0 && payload != nullptr) {
      return true;
    }
    return false;
  }
};

#endif  // RTMP_SERVER_RTMP_MESSAGE_H
/// @file RtmpHandshake.h
/// @brief RTMP 握手封装, 传输的包是固定大小的块
/// @version 0.1
/// @author lq
/// @date 2023/05/13
/// @note RTMP 握手过程
///       en: https://rtmp.veriskope.com/docs/spec/#52handshake
///       zh: https://chenlichao.gitbooks.io/rtmp-zh_cn/content/5.2-handshake.html
///        c0/s0: 1 Byte, 版本号, 0x03
///        c1/s1: 1536 Byte, 4 Byte 时间戳, 4 Byte 0, 1528 Byte 随机数
///        c2/s2: 1536 Byte, 考虑到时间戳估算带宽延迟可能不准确, 这里简化一下直接设置与 c1/s1 相同
///       流程:
///        1. 客户端发送 C0 + C1 包到达服务器端
///        2. 服务器收到 C0 + C1, 返回 S0 + S1 + S2 包
///        3. 客户端收到 S0 + S1 + S2 包, 返回 C2 包, 客户端握手完成, 接下来的发数据包
///        4. 服务器收到 C2 包, 服务器握手完成, 接下来的发数据包
///       状态转移: 服务器端 HANDSHAKE_C0C1 -> HANDSHAKE_C2 -> HANDSHAKE_COMPLETE
///       状态转移: 客户端 HANDSHAKE_S0S1S2 -> HANDSHAKE_COMPLETE

#ifndef RTMP_SERVER_RTMP_HANDSHAKE_H
#define RTMP_SERVER_RTMP_HANDSHAKE_H

#include "BufferReader.h"

class RtmpHandshake {
 public:
  enum State {
    HANDSHAKE_C0C1,     // 服务器端 C0 + C1 包到达, 后续状态为 HANDSHAKE_C2
    HANDSHAKE_S0S1S2,   // 客户端 S0 + S1 + S2 包到达, 后续状态为 HANDSHAKE_COMPLETE
    HANDSHAKE_C2,       // 服务器端 C2 包到达, 后续状态为 HANDSHAKE_COMPLETE
    HANDSHAKE_COMPLETE  // 握手完成
  };

  RtmpHandshake(State state);
  virtual ~RtmpHandshake();

  /**
   * @brief 解析握手包并返回响应包
   *
   * @param in_buffer 到来的握手包
   * @param res_buf 返回参数, 已经初始化的存放响应包 char 数组
   * @param res_buf_size res_buf 的数组大小
   * @return int 解析的字节数, 返回 0 表示数据还未完全到达继续等待, 返回 -1 表示解析失败
   */
  int Parse(BufferReader& in_buffer, char* res_buf, uint32_t res_buf_size);

  // 客户端在连接时发送 C0 + C1 包才能进入 HANDSHAKE_S0S1S2 状态
  int BuildC0C1(char* buf, uint32_t buf_size);

  bool IsCompleted() const { return handshake_state_ == HANDSHAKE_COMPLETE; }

 private:
  State handshake_state_;
};

#endif  // RTMP_SERVER_RTMP_HANDSHAKE_H
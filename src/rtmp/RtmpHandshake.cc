/// @file RtmpHandshake.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/05/13

#include "RtmpHandshake.h"

#include <cstdint>
#include <random>

#include "Logger.h"
#include "rtmp.h"

RtmpHandshake::RtmpHandshake(State state) { handshake_state_ = state; }

RtmpHandshake::~RtmpHandshake() {}

int RtmpHandshake::Parse(BufferReader& buffer, char* res_buf, uint32_t res_buf_size) {
  uint8_t* buf = (uint8_t*)buffer.Peek();
  uint32_t buf_size = buffer.ReadableBytes();
  uint32_t pos = 0;
  uint32_t res_size = 0;
  std::random_device rd;

  switch (handshake_state_) {
    case HANDSHAKE_C0C1:
      // 仅接收到 C0 则直接返回等待 C1 到达
      if (buf_size < 1537) {
        return res_size;
      } else {
        // 按标准应该返回 3 提示降级为 3 , 考虑到 0-2 版本 RTMP 已经弃用, 这里偷懒直接返回错误
        if (buf[0] != RTMP_VERSION) {
          return -1;
        }

        pos += 1537;
        res_size = 1 + 1536 + 1536;
        memset(res_buf, 0, 1537);  // S0 S1 S2
        res_buf[0] = RTMP_VERSION;

        char* p = res_buf;
        p += 9;
        for (int i = 0; i < 1528; i++) {
          *p++ = rd();
        }
        memcpy(p, buf + 1, 1536);
        handshake_state_ = HANDSHAKE_C2;
      }
      break;

    case HANDSHAKE_S0S1S2:
      if (buf_size < (1 + 1536 + 1536)) {  // S0S1S2
        return res_size;
      }

      if (buf[0] != RTMP_VERSION) {
        LOG_ERROR("unsupported rtmp version %x\n", buf[0]);
        return -1;
      }

      pos += 1 + 1536 + 1536;
      res_size = 1536;
      memcpy(res_buf, buf + 1, 1536);  // C2
      handshake_state_ = HANDSHAKE_COMPLETE;
      break;

    case HANDSHAKE_C2:
      if (buf_size < 1536) {  // C2
        return res_size;
      } else {
        pos = 1536;
        handshake_state_ = HANDSHAKE_COMPLETE;
      }
      break;

    default:
      return -1;
  }

  buffer.Retrieve(pos);
  return res_size;
}

int RtmpHandshake::BuildC0C1(char* buf, uint32_t buf_size) {
  uint32_t size = 1 + 1536;  // COC1
  memset(buf, 0, 1537);
  buf[0] = RTMP_VERSION;

  std::random_device rd;
  uint8_t* p = (uint8_t*)buf;
  p += 9;
  for (int i = 0; i < 1528; i++) {
    *p++ = rd();
  }

  return size;
}
/// @file test_all.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/06/14

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>

#include "EventLoop.h"
#include "H264File.h"
#include "RtmpClient.h"
#include "RtmpPublisher.h"

#define RTMP_URL "rtmp://127.0.0.1:1935/live/stream0"
#define PUSH_FILE "./test.h264"

int TestRtmpPublisher(EventLoop *event_loop, uint32_t push_times);

// 测试一次推流, 推完结束
TEST(TestPushOnce, BasicAssertions) {
  // Expect two strings not to be equal.
  // EXPECT_STRNE("hello", "world");
  // Expect equality.
  // EXPECT_EQ(7 * 6, 42);

  EventLoop event_loop(1);
  std::thread t([&event_loop]() { TestRtmpPublisher(&event_loop, 1); });
  t.join();
}

// 测试不间断推流, 到达文件末尾从头开始再推
TEST(TestPushMutiTimes, BasicAssertions) {
  EventLoop event_loop(1);
  std::thread t([&event_loop]() { TestRtmpPublisher(&event_loop, 5); });
  t.join();
}

// 测试拉流, 仅输出流信息不播放
TEST(TestPull, BasicAssertions) {
  EventLoop event_loop1(1);
  auto rtmp_client = RtmpClient::Create(&event_loop1);
  rtmp_client->SetRecvFrameCB(
      [](uint8_t *payload, uint32_t length, uint8_t codecId, uint32_t timestamp) {
        printf("recv frame, type:%u, size:%u,\n", codecId, length);
      });

  std::string status;
  EXPECT_EQ(rtmp_client->OpenUrl(RTMP_URL, 3000, status), 0);
  
  EventLoop event_loop0(1);
  std::thread t([&event_loop0]() { TestRtmpPublisher(&event_loop0, 2); });
  t.join();
}


/// @brief 本地 H264 文件推流测试
/// 
/// @param event_loop 
/// @param push_times 该文件到末尾时从头开始读
/// @return int 正常结束: 0
int TestRtmpPublisher(EventLoop *event_loop, uint32_t push_times) {
  H264File h264_file;
  EXPECT_TRUE(h264_file.open(PUSH_FILE));

  MediaInfo media_info;
  auto publisher = RtmpPublisher::Create(event_loop);
  publisher->SetChunkSize(60000);
  std::string status;
  EXPECT_NE(publisher->OpenUrl(RTMP_URL, 3000, status), -1);

  int buf_size = 500000;
  bool end_of_frame = false;
  bool has_sps_pps = false;
  uint8_t *frame_buf = new uint8_t[buf_size];
  while (publisher->IsConnected()) {
    int frameSize = h264_file.readFrame((char *)frame_buf, buf_size, &end_of_frame);
    if (end_of_frame && --push_times == 0) break;
    if (frameSize > 0) {
      if (!has_sps_pps) {
        // 查当前帧是否包含SPS数据，并将SPS数据存储在media_info.sps中
        if (frame_buf[3] == 0x67 || frame_buf[4] == 0x67) {
          Nal sps = H264File::findNal(frame_buf, frameSize);
          if (sps.first != nullptr && sps.second != nullptr && *sps.first == 0x67) {
            media_info.sps_size = (uint32_t)(sps.second - sps.first + 1);
            media_info.sps.reset(new uint8_t[media_info.sps_size],
                                 std::default_delete<uint8_t[]>());
            memcpy(media_info.sps.get(), sps.first, media_info.sps_size);

            Nal pps = H264File::findNal(sps.second, frameSize - (int)(sps.second - frame_buf));
            // 检查是否包含PPS数据，并将PPS数据存储在media_info.pps
            if (pps.first != nullptr && pps.second != nullptr && *pps.first == 0x68) {
              media_info.pps_size = (uint32_t)(pps.second - pps.first + 1);
              media_info.pps.reset(new uint8_t[media_info.pps_size],
                                   std::default_delete<uint8_t[]>());
              memcpy(media_info.pps.get(), pps.first, media_info.pps_size);

              has_sps_pps = true;
              publisher->SetMediaInfo(media_info);  // 将 SPS/PPS 信息存到 publisher 中
              printf("Start rtmp pusher, rtmp url: %s \n\n", RTMP_URL);
            }
          }
        }
      } else if (has_sps_pps) {
        publisher->PushVideoFrame(frame_buf, frameSize); /* send h.264 frame */
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }

  delete[] frame_buf;
  return 0;
}
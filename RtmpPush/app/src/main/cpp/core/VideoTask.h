/// @file VideoTask.h
/// @brief 视频编码任务类
/// @author xhunmon
/// @last_editor lq

#ifndef RTMPPUSH_VIDEOTASK_H
#define RTMPPUSH_VIDEOTASK_H

#include <pthread.h>
#include <x264.h>

#include <cstring>
#include <functional>

#include "../librtmp/rtmp.h"
#include "stdint.h"
#include "LogUtil.h"

class VideoTask {
  using VideoCallback = std::function<void(RTMPPacket* packet)>;
 public:
  VideoTask(int fps, int bit_rate);

  void SetDataCallback(VideoCallback callback);

  void DataChange(int width, int height);

  void EncodeData(int8_t* data);

  // 视频开始处, 发送 SPS 和 PPS 单元来告知解码器解码规则
  void SendSpsPps(uint8_t* sps, uint8_t* pps, int sps_len, int pps_len);

  void SendFrame(int type, uint8_t* payload, int i_payload);

 private:
  int fps_;
  int bit_rate_;
  int y_size_;
  int uv_size_;
  VideoCallback video_callback_;
  x264_t* video_codec_ = 0;
  x264_picture_t* pic_in_ = 0;
  pthread_mutex_t mutex_;
};

#endif  // RTMPPUSH_VIDEOTASK_H

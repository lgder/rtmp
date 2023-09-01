/// @file AudioTask.h
/// @brief 音频编码任务类
/// @author xhunmon
/// @last_editor lq

#ifndef RTMPPUSH_AUDIOTASK_H
#define RTMPPUSH_AUDIOTASK_H

#include <faac.h>
#include <sys/types.h>

#include "../librtmp/rtmp.h"
#include "sync_queue.h"

class AudioTask {
  using AudioCallback = std::function<void(RTMPPacket* packet)>;

 public:
  void Init(int sampleRate, int channels);

  void EncodeData(int8_t* data);

  void SetDataCallback(AudioCallback callback) { this->callback_ = callback; }

  u_long GetSamples() const { return input_samples_; }

 private:
  AudioCallback callback_;
  u_long input_samples_;
  faacEncHandle audio_codec_ = 0;
  u_long max_output_bytes_;
  int channels_;  // 声道数
  u_char* buffer_ = 0;
};

#endif  // RTMPPUSH_AUDIOTASK_H

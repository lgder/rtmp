/// @file Pusher.h
/// @brief 推流类
/// @author xhunmon
/// @last_editor lq

#ifndef RTMPPUSH_PUSHER_H
#define RTMPPUSH_PUSHER_H

#include "AudioTask.h"
#include "VideoTask.h"

static uint32_t start_time = 0;
static SyncQueue<RTMPPacket*> packets;

static void Callback(RTMPPacket* packet);

static void Release(RTMPPacket*&packet);

static void* PushTask(void* args);

class Pusher {
 public:
  Pusher(int fps, int bitRate, int sampleRate, int channels);

  void EncodeAudioData(signed char* data);

  void Prepare(char* url);

  void Stop();

  void VideoDataChange(int width, int height);

  void EncodeVideoData(int8_t* data);

  u_long AudioGetSamples();

 public:
  int is_ready_pushing_ = 0;
  char* url_ = 0;

 private:
  pthread_t pid_;
  AudioTask* audio_task_;
  VideoTask* video_task_;
};

#endif  // RTMPPUSH_PUSHER_H

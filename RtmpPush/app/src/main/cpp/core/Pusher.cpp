/// @file Pusher.cpp
/// @brief 推流类
/// @author xhunmon
/// @last_editor lq

#include "Pusher.h"

Pusher::Pusher(int fps, int bitRate, int sampleRate, int channels)
    : audio_task_(new AudioTask()),
      video_task_(new VideoTask(fps, bitRate)) {
  audio_task_->SetDataCallback(Callback);
  audio_task_->Init(sampleRate, channels);
  video_task_->SetDataCallback(Callback);
  packets.setReleaseCallback(Release);
}

void Pusher::EncodeAudioData(int8_t* data) { audio_task_->EncodeData(data); }

void Pusher::Prepare(char* url) {
  this->url_ = url;
  is_ready_pushing_ = 1;
  packets.setWork(1);
  pthread_create(&pid_, 0, PushTask, this);
}

void Pusher::Stop() {
  if (!is_ready_pushing_) {
    return;
  }
  is_ready_pushing_ = 0;
  packets.setWork(0);
  // pthread_join(pid_, 0);
  pthread_detach(pid_);
}

void Pusher::VideoDataChange(int width, int height) {
  video_task_->DataChange(width, height);
}

void Pusher::EncodeVideoData(int8_t* data) { video_task_->EncodeData(data); }

u_long Pusher::AudioGetSamples() { return audio_task_->GetSamples(); }

void Callback(RTMPPacket* packet) {
  if (packet) {
    // 设置时间戳
    packet->m_nTimeStamp = RTMP_GetTime() - start_time;
    packets.push(packet);
  }
}

void Release(RTMPPacket*&packet) {
  if (packet) {
    RTMPPacket_Free(packet);
    delete packet;
    packet = 0;
    LOGD("释放资源。。。。。");
  }
}


void* PushTask(void* args) {
  auto* pusher = static_cast<Pusher*>(args);
  char* url = pusher->url_;

  RTMP* rtmp = RTMP_Alloc();
  do {
    if (!rtmp) {
      LOGD("alloc rtmp失败~~~~");
      break;
    }
    RTMP_Init(rtmp);
    int ret = RTMP_SetupURL(rtmp, url);
    if (!ret) {
      LOGD("设置地址失败:%s", url);
      break;
    }
    // 15s超时时间
    rtmp->Link.timeout = 15;
    RTMP_EnableWrite(rtmp);
    ret = RTMP_Connect(rtmp, 0);
    if (!ret) {
      LOGD("连接服务器:%s", url);
      break;
    }
    ret = RTMP_ConnectStream(rtmp, 0);
    if (!ret) {
      LOGD("连接流:%s", url);
      break;
    }
    //记录一个开始时间
    start_time = RTMP_GetTime();
    //表示可以开始推流了
    //保证第一个数据是 aac解码数据包
    //        Callback(audioChannel->getAudioTag());
    RTMPPacket* packet = 0;
    LOGD("准备推流~~~~~~~~");
    while (pusher->is_ready_pushing_) {
      packets.pop(packet);
      if (!packet) {
        continue;
      }
      packet->m_nInfoField2 = rtmp->m_stream_id;
      // 发送rtmp包 1：队列
      // 意外断网？发送失败，rtmpdump 内部会调用RTMP_Close
      // RTMP_Close 又会调用 RTMP_SendPacket
      // RTMP_SendPacket  又会调用 RTMP_Close
      // 将rtmp.c 里面WriteN方法的 Rtmp_Close注释掉
      ret = RTMP_SendPacket(rtmp, packet, 1);
      if (!ret) {
        LOGD("发送失败~~~~~~~~");
        break;
      } else {
        LOGD("发送成功：%d", packet->m_nTimeStamp);
      }
    }
  } while (false);
  if (rtmp) {
    RTMP_Close(rtmp);
    RTMP_Free(rtmp);
  }
  packets.clear();
  delete (url);
  LOGD("结束推流~~~~~~~~");
  return 0;
}
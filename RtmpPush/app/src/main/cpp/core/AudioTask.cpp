/// @file AudioTask.cpp
/// @brief 音频编码任务类
/// @author xhunmon
/// @last_editor lq

#include "AudioTask.h"

#include <cstring>

#include "../librtmp/rtmp.h"

void AudioTask::Init(int sample_rate, int channels) {
  this->channels_ = channels;
  // 1. 打开 AAC 编码器，使用传出参数获取 input_samples_ 和 max_output_bytes_ 的值，用于后面编码
  // 参数 1：采样率；2：声道数；3：单次输入的样本数；4：输出数据最大字节数
  // 返回值：编码器句柄
  audio_codec_ = faacEncOpen(sample_rate, channels, &input_samples_, &max_output_bytes_);

  // 2.设置编码器参数
  faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(audio_codec_);

  // 除了 MPEG4 也可使用老的 MPEG2 AAC
  config->mpegVersion = MPEG4;
  // lc 标准, Low Complexity, 低复杂度, 内存小兼容性高, 最常用
  config->aacObjectType = LOW;
  // 16位
  config->inputFormat = FAAC_INPUT_16BIT;
  // 编码出原始数据；0 = Raw; 1 = ADTS
  config->outputFormat = 1;
  faacEncSetConfiguration(audio_codec_, config);

  // 输出缓冲区, 保存编码后的数据
  buffer_ = new u_char[max_output_bytes_];
}

void AudioTask::EncodeData(int8_t* data) {
  // 3.编码成 AAC
  // 参数 1：编码器句柄；2：采集的 PCM 原始数据；3：从 faacEncOpen 获取的 input_samples_；4：至少有从 faacEncOpen 获取的 max_output_bytes_ 大小的缓冲区；5：前一个参数数组大小
  // 返回值: 编码后数据字节的长度
  int encoded_len = faacEncEncode(audio_codec_, reinterpret_cast<int32_t*>(data), input_samples_,
                                  buffer_,
                                  max_output_bytes_);
  if (encoded_len > 0) {
    int body_size = 2 + encoded_len;
    auto* packet = new RTMPPacket;
    RTMPPacket_Alloc(packet, body_size);
    if (channels_ == 1) {
      packet->m_body[0] = 0xAE;  //单声道
    } else {
      packet->m_body[0] = 0xAF;  //双声道
    }
    // 编码出的声音 都是 0x01
    packet->m_body[1] = 0x01;
    // 音频数据
    memcpy(&packet->m_body[2], buffer_, encoded_len);

    packet->m_hasAbsTimestamp = 0;
    packet->m_nBodySize = body_size;
    packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet->m_nChannel = 0x11;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    // 准备好 packet 之后开始调用推流的回调函数
    callback_(packet);
  }
}


/// @file RtmpConnection.h
/// @brief RTMP 连接的各种操作封装, 包括了推流拉流客户端的连接
/// @version 0.1
/// @author lq
/// @date 2023/06/04
/// @note
/// @todo RTMP Server, Client, Publisher Connection 解耦, 封装成子类

#ifndef RTMP_SERVER_RTMP_CONNECTION_H
#define RTMP_SERVER_RTMP_CONNECTION_H

#include <cstdint>
#include <vector>

#include "EventLoop.h"
#include "RtmpChunk.h"
#include "RtmpHandshake.h"
#include "TcpConnection.h"
#include "amf.h"
#include "rtmp.h"

class RtmpServer;
class RtmpPublisher;
class RtmpClient;
class RtmpSession;

class RtmpConnection : public TcpConnection {
 public:
  using PlayCallback =
      std::function<void(uint8_t* payload, uint32_t length, uint8_t codecId, uint32_t timestamp)>;

  enum ConnectionState {
    HANDSHAKE,
    START_CONNECT,
    START_CREATE_STREAM,
    START_DELETE_STREAM,
    START_PLAY,
    START_PUBLISH,
  };

  enum ConnectionMode { RTMP_SERVER, RTMP_PUBLISHER, RTMP_CLIENT };

  RtmpConnection(std::shared_ptr<RtmpServer> server, TaskScheduler* scheduler, SOCKET sockfd);
  RtmpConnection(std::shared_ptr<RtmpPublisher> publisher, TaskScheduler* scheduler, SOCKET sockfd);
  RtmpConnection(std::shared_ptr<RtmpClient> client, TaskScheduler* scheduler, SOCKET sockfd);
  ~RtmpConnection() override {};

  std::string GetStreamPath() const { return stream_path_; }

  std::string GetStreamName() const { return stream_name_; }

  std::string GetApp() const { return app_; }

  AmfObjects GetMetaData() const { return meta_data_; }

  bool IsPlayer() { return connection_state_ == START_PLAY; }

  bool IsPublisher() { return connection_state_ == START_PUBLISH; }

  bool IsPlaying() { return is_playing_; }

  bool IsPublishing() { return is_publishing_; }

  uint32_t GetId() { return (uint32_t)this->GetSocket(); }

  std::string GetStatus() {
    if (status_ == "") {
      return "unknown error";
    }
    return status_;
  }

 private:
  friend class RtmpSession;
  friend class RtmpServer;
  friend class RtmpPublisher;
  friend class RtmpClient;

  RtmpConnection(TaskScheduler* scheduler, SOCKET sockfd, Rtmp* rtmp);

  void SetPlayCB(const PlayCallback& cb) { play_cb_ = cb; }

  // TCP 层接收的新数据到来的入口函数
  bool OnRead(BufferReader& buffer);
  void OnClose();

  // 发送 Command Message 类型消息, AMF 格式, 远程调用的语义
  bool SendCommandMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payload_size);

  // 发送 Data Message 类型消息, AMF 格式, 一般是元数据
  bool SendDataMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payload_size);

  // 发送块, 会根据消息大小和 max_chunk_size_ 内部分块发送
  void SendRtmpChunks(uint32_t csid, RtmpMessage& rtmp_msg);

  /* 以下一些函数用来处理客户端 RTMP 协议的几个请求 */

  bool HandleConnect();
  bool HandleCreateStream();
  bool HandlePublish();
  bool HandlePlay();
  bool HandlePlay2();
  bool HandleDeleteStream();

  bool HandleChunk(BufferReader& buffer);
  bool HandleMessage(RtmpMessage& rtmp_msg);
  bool HandleCommand(RtmpMessage& rtmp_msg);
  bool HandleData(RtmpMessage& rtmp_msg);
  bool HandleVideo(RtmpMessage& rtmp_msg);
  bool HandleAudio(RtmpMessage& rtmp_msg);

  // 服务器接收并设置对等带宽消息, 目的是限制服务器输出带宽
  void SetPeerBandwidth();

  // 确认窗口大小消息, 在处理连接消息时候返回
  void SendAcknowledgement();

  // 设置块大小, 并发送一个块告知给对端
  void SetChunkSize();

  // 从 payload 中解析视频帧是否是关键帧
  bool IsKeyFrame(std::shared_ptr<char> payload, uint32_t payload_size);

  /* 以下一些函数推/拉流客户端使用 */

  bool Connect();
  bool CreateStream();
  bool Publish();
  bool Play();

  // 客户端发送 C0C1 包主动发起握手
  bool Handshake();

  // 推拉流客户端用, 解析 _result 命令消息, 判断状态是成功之后开始推流/拉流
  bool HandleResult(RtmpMessage& rtmp_msg);

  // 推拉流客户端用, 解析推流或播放时候服务器返回的状态信息
  bool HandleOnStatus(RtmpMessage& rtmp_msg);

  // 推流客户端用, 主动发起断开流消息
  bool DeleteStream();

  /* 以下函数向拉流端发送各种数据, 由 session 调用 */

  bool SendMetaData(AmfObjects metaData);
  bool SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload,
                     uint32_t payload_size);
  bool SendVideoData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);
  bool SendAudioData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);

  std::weak_ptr<RtmpServer> rtmp_server_;
  std::weak_ptr<RtmpPublisher> rtmp_publisher_;
  std::weak_ptr<RtmpClient> rtmp_client_;
  std::weak_ptr<RtmpSession> rtmp_session_;

  std::shared_ptr<RtmpHandshake> handshake_;
  std::shared_ptr<RtmpChunk> rtmp_chunk_;
  ConnectionMode connection_mode_;
  ConnectionState connection_state_;

  TaskScheduler* task_scheduler_;
  std::shared_ptr<Channel> channel_;

  uint32_t peer_bandwidth_ = 5000000;  // 对等带宽, 限制另一端输出带宽, 默认设置大一些相当于无限制
  uint32_t acknowledgement_size_out_ = 5000000;  // 用来通知对端如果收到该大小字节的数据，需要回复 Acknowledgement 消息
  uint32_t acknowledgement_size_in_ = 5000000;   // 对端对本端的回复 Acknowledgement 消息限制
  uint32_t max_chunk_size_ = 4096;   // 分块大小, 默认 128, 这里设置大一些 (4096) 减少块头的开销并降低 CPU 占用
  uint32_t max_gop_cache_len_ = 0;  // 缓存 GOP 的长度, 0 表示不缓存
  uint32_t stream_id_ = 0;
  uint32_t number_ = 0;  // 控制命令的事务id, 不需要可以设为 0, 这里方便 debug 每次控制消息都 +1
  std::string app_;          // 应用名称, rtmp://ip:port/app/stream_name
  std::string stream_name_;  // 流名称, rtmp://ip:port/app/stream_name
  std::string stream_path_;
  std::string status_;  // 控制命令或通知命令使用, 当前状态信息, "NetStream.xxx.xxx"

  AmfObjects meta_data_;
  AmfDecoder amf_decoder_;
  AmfEncoder amf_encoder_;

  bool is_playing_ = false;
  bool is_publishing_ = false;
  bool has_key_frame_ = false;
  std::shared_ptr<char> avc_sequence_header_;
  std::shared_ptr<char> aac_sequence_header_;
  uint32_t avc_sequence_header_size_ = 0;
  uint32_t aac_sequence_header_size_ = 0;
  PlayCallback play_cb_;
};

#endif  // RTMP_SERVER_RTMP_CONNECTION_H

/// @file RtmpConnection.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/06/04

#include "RtmpConnection.h"

#include <random>

#include "Logger.h"
#include "RtmpClient.h"
#include "RtmpPublisher.h"
#include "RtmpServer.h"

RtmpConnection::RtmpConnection(TaskScheduler *task_scheduler, SOCKET sockfd, Rtmp *rtmp)
    : TcpConnection(task_scheduler, sockfd),
      task_scheduler_(task_scheduler),
      channel_(new Channel(sockfd)),
      rtmp_chunk_(new RtmpChunk),
      connection_state_(HANDSHAKE) {
  peer_bandwidth_ = rtmp->GetPeerBandwidth();
  acknowledgement_size_out_ = rtmp->GetAcknowledgementSize();
  max_gop_cache_len_ = rtmp->GetGopCacheLen();
  max_chunk_size_ = rtmp->GetChunkSize();
  stream_path_ = rtmp->GetStreamPath();
  stream_name_ = rtmp->GetStreamName();
  app_ = rtmp->GetApp();

  this->SetReadCallback([this](std::shared_ptr<TcpConnection> conn, BufferReader &buffer) {
    return this->OnRead(buffer);
  });

  this->SetCloseCallback([this](std::shared_ptr<TcpConnection> conn) { this->OnClose(); });

}

RtmpConnection::RtmpConnection(std::shared_ptr<RtmpServer> rtmp_server,
                               TaskScheduler *task_scheduler, SOCKET sockfd)
    : RtmpConnection(task_scheduler, sockfd, rtmp_server.get()) {
  handshake_.reset(new RtmpHandshake(RtmpHandshake::HANDSHAKE_C0C1));
  rtmp_server_ = rtmp_server;
  connection_mode_ = RTMP_SERVER;
}

RtmpConnection::RtmpConnection(std::shared_ptr<RtmpPublisher> rtmp_publisher,
                               TaskScheduler *task_scheduler, SOCKET sockfd)
    : RtmpConnection(task_scheduler, sockfd, rtmp_publisher.get()) {
  handshake_.reset(new RtmpHandshake(RtmpHandshake::HANDSHAKE_S0S1S2));
  rtmp_publisher_ = rtmp_publisher;
  connection_mode_ = RTMP_PUBLISHER;
}

RtmpConnection::RtmpConnection(std::shared_ptr<RtmpClient> rtmp_client,
                               TaskScheduler *task_scheduler, SOCKET sockfd)
    : RtmpConnection(task_scheduler, sockfd, rtmp_client.get()) {
  handshake_.reset(new RtmpHandshake(RtmpHandshake::HANDSHAKE_S0S1S2));
  rtmp_client_ = rtmp_client;
  connection_mode_ = RTMP_CLIENT;
}

bool RtmpConnection::OnRead(BufferReader &buffer) {
  bool ret = true;

  if (handshake_->IsCompleted()) {
    ret = HandleChunk(buffer);
  } else {
    // 还未完成握手则解析握手数据并返回握手响应数据
    std::shared_ptr<char> res(new char[4096], std::default_delete<char[]>());
    int res_size = handshake_->Parse(buffer, res.get(), 4096);
    if (res_size < 0) {
      ret = false;
    }

    if (res_size > 0) {
      this->Send(res.get(), res_size);
    }

    if (handshake_->IsCompleted()) {
      // 完成握手还有额外数据则继续解析
      if (buffer.ReadableBytes() > 0) {
        ret = HandleChunk(buffer);
      }

      // 如果是推拉流客户端, 主动发出建立网络连接消息
      if (connection_mode_ == RTMP_PUBLISHER || connection_mode_ == RTMP_CLIENT) {
        this->SetChunkSize();
        this->Connect();
      }
    }
  }

  return ret;
}

void RtmpConnection::OnClose() {
  if (connection_mode_ == RTMP_SERVER) {
    this->HandleDeleteStream();
  } else if (connection_mode_ == RTMP_PUBLISHER) {
    this->DeleteStream();
  }
}

bool RtmpConnection::HandleChunk(BufferReader &buffer) {
  int ret = -1;

  do {
    RtmpMessage rtmp_msg;
    ret = rtmp_chunk_->Parse(buffer, rtmp_msg);
    if (ret >= 0) {
      if (rtmp_msg.IsCompleted()) {
        if (!HandleMessage(rtmp_msg)) {
          return false;
        }
      }

      if (ret == 0) {
        break;
      }
    } else if (ret < 0) {
      return false;
    }
  } while (buffer.ReadableBytes() > 0);

  return true;
}

bool RtmpConnection::HandleMessage(RtmpMessage &rtmp_msg) {
  bool ret = true;
  switch (rtmp_msg.type_id) {
    case RTMP_VIDEO:
      ret = HandleVideo(rtmp_msg);
      break;
    case RTMP_AUDIO:
      ret = HandleAudio(rtmp_msg);
      break;
    case RTMP_COMMAND_MESSAGE:
      ret = HandleCommand(rtmp_msg);
      break;
    case RTMP_DATA_MESSAGE:
      ret = HandleData(rtmp_msg);
      break;
    case RTMP_FLEX_MESSAGE:
      LOG_INFO("unsupported rtmp flex message.\n");
      ret = false;
      break;
    case RTMP_SET_CHUNK_SIZE:
      rtmp_chunk_->SetInChunkSize(ReadUint32BE(rtmp_msg.payload.get()));
      break;
    case RTMP_BANDWIDTH_SIZE:
      break;
    case RTMP_FLASH_VIDEO:
      LOG_INFO("unsupported rtmp flash video.\n");
      ret = false;
      break;
    case RTMP_ACK:
      // @todo 由 Window Acknowledgement Size 控制发多少字节后回复一个 ACK
      break;
    case RTMP_ACK_SIZE:
      acknowledgement_size_in_ = ReadUint32BE(rtmp_msg.payload.get());
      break;
    case RTMP_USER_CONTROL:
      break;
    default:
      LOG_INFO("unkonw message type : %d\n", rtmp_msg.type_id);
      break;
  }

  return ret;
}

bool RtmpConnection::HandleCommand(RtmpMessage &rtmp_msg) {
  bool ret = true;
  amf_decoder_.Reset();

  int bytes_used = amf_decoder_.Decode((const char *)rtmp_msg.payload.get(), rtmp_msg.length, 1);
  if (bytes_used < 0) {
    return false;
  }

  std::string method = amf_decoder_.GetString();
  // LOG_INFO("[Method] %s\n", method.c_str());

  if (connection_mode_ == RTMP_PUBLISHER || connection_mode_ == RTMP_CLIENT) {
    bytes_used +=
        amf_decoder_.Decode(rtmp_msg.payload.get() + bytes_used, rtmp_msg.length - bytes_used);
    if (method == "_result") {  // 建立连接阶段和建立流阶段, 服务器发送的最后一个命令 "_result" 
      ret = HandleResult(rtmp_msg);
    } else if (method == "onStatus") {  // 推流端 push 阶段 或 拉流端 play 阶段服务器返回的状态命令
      ret = HandleOnStatus(rtmp_msg);
    }
  } else if (connection_mode_ == RTMP_SERVER) {
    if (rtmp_msg.stream_id == 0) {
      bytes_used +=
          amf_decoder_.Decode(rtmp_msg.payload.get() + bytes_used, rtmp_msg.length - bytes_used);
      if (method == "connect") {
        ret = HandleConnect();
      } else if (method == "createStream") {
        ret = HandleCreateStream();
      }
    } else if (rtmp_msg.stream_id == stream_id_) {  // 处理已经建立完流的一些状态: publish/play/play2 等
      bytes_used += amf_decoder_.Decode((const char *)rtmp_msg.payload.get() + bytes_used,
                                        rtmp_msg.length - bytes_used, 3);
      stream_name_ = amf_decoder_.GetString();
      stream_path_ = "/" + app_ + "/" + stream_name_;

      if ((int)rtmp_msg.length > bytes_used) {
        bytes_used += amf_decoder_.Decode((const char *)rtmp_msg.payload.get() + bytes_used,
                                          rtmp_msg.length - bytes_used);
      }

      if (method == "publish") {
        ret = HandlePublish();
      } else if (method == "play") {
        ret = HandlePlay();
      } else if (method == "play2") {
        ret = HandlePlay2();
      } else if (method == "DeleteStream") {
        ret = HandleDeleteStream();
      } else if (method == "releaseStream") {
      }
    }
  }

  return ret;
}

bool RtmpConnection::HandleData(RtmpMessage &rtmp_msg) {
  amf_decoder_.Reset();
  int bytes_used = amf_decoder_.Decode((const char *)rtmp_msg.payload.get(), rtmp_msg.length, 1);
  if (bytes_used < 0) {
    return false;
  }

  // 收到元信息之后立即设置到 session 并发送给拉流客户端
  if (amf_decoder_.GetString() == "@setDataFrame") {
    amf_decoder_.Reset();
    bytes_used = amf_decoder_.Decode((const char *)rtmp_msg.payload.get() + bytes_used,
                                     rtmp_msg.length - bytes_used, 1);
    if (bytes_used < 0) {
      return false;
    }

    if (amf_decoder_.GetString() == "onMetaData") {
      amf_decoder_.Decode((const char *)rtmp_msg.payload.get() + bytes_used,
                          rtmp_msg.length - bytes_used);
      meta_data_ = amf_decoder_.GetObjects();

      auto server = rtmp_server_.lock();
      if (!server) {
        return false;
      }

      auto session = rtmp_session_.lock();
      if (session) {
        session->SetMetaData(meta_data_);
        session->SendMetaData(meta_data_);
      }
    }
  }

  return true;
}

bool RtmpConnection::HandleVideo(RtmpMessage &rtmp_msg) {
  uint8_t type = RTMP_VIDEO;
  uint8_t *payload = (uint8_t *)rtmp_msg.payload.get();
  uint32_t length = rtmp_msg.length;
  uint8_t frame_type = (payload[0] >> 4) & 0x0f;  // 是否关键帧
  uint8_t codec_id = payload[0] & 0x0f;  // 编码格式, 仅支持 H264

  if (connection_mode_ == RTMP_CLIENT) {
    if (is_playing_ && connection_state_ == START_PLAY) {
      if (play_cb_) {
        play_cb_(payload, length, codec_id, (uint32_t)rtmp_msg.absolute_timestamp);
      }
    }
  } else if (connection_mode_ == RTMP_SERVER) {
    auto server = rtmp_server_.lock();
    if (!server) {
      return false;
    }

    auto session = rtmp_session_.lock();
    if (session == nullptr) {
      return false;
    }

    if (frame_type == 1 && codec_id == RTMP_CODEC_ID_H264) {
      if (payload[1] == 0) {
        avc_sequence_header_size_ = length;
        avc_sequence_header_.reset(new char[length], std::default_delete<char[]>());
        memcpy(avc_sequence_header_.get(), rtmp_msg.payload.get(), length);
        session->SetAvcSequenceHeader(avc_sequence_header_, avc_sequence_header_size_);
        type = RTMP_AVC_SEQUENCE_HEADER;
      }
    }

    session->SendMediaData(type, rtmp_msg.absolute_timestamp, rtmp_msg.payload, rtmp_msg.length);
  }

  return true;
}

bool RtmpConnection::HandleAudio(RtmpMessage &rtmp_msg) {
  uint8_t type = RTMP_AUDIO;
  uint8_t *payload = (uint8_t *)rtmp_msg.payload.get();
  uint32_t length = rtmp_msg.length;
  uint8_t sound_format = (payload[0] >> 4) & 0x0f;  // 音频格式, 仅支持 AAC
  // 下面两个字段无需解析, 拷贝即可
  // uint8_t sound_size = (payload[0] >> 1) & 0x01;
  // uint8_t sound_rate = (payload[0] >> 2) & 0x03;
  uint8_t codec_id = payload[0] & 0x0f;  // 编码格式, 仅支持 AAC

  if (connection_mode_ == RTMP_CLIENT) {
    if (connection_state_ == START_PLAY && is_playing_) {
      if (play_cb_) {
        play_cb_(payload, length, codec_id, (uint32_t)rtmp_msg.absolute_timestamp);
      }
    }
  } else {
    auto server = rtmp_server_.lock();
    if (!server) {
      return false;
    }

    auto session = rtmp_session_.lock();
    if (session == nullptr) {
      return false;
    }

    if (sound_format == RTMP_CODEC_ID_AAC && payload[1] == 0) {
      aac_sequence_header_size_ = rtmp_msg.length;
      aac_sequence_header_.reset(new char[rtmp_msg.length], std::default_delete<char[]>());
      memcpy(aac_sequence_header_.get(), rtmp_msg.payload.get(), rtmp_msg.length);
      session->SetAacSequenceHeader(aac_sequence_header_, aac_sequence_header_size_);
      type = RTMP_AAC_SEQUENCE_HEADER;
    }

    session->SendMediaData(type, rtmp_msg.absolute_timestamp, rtmp_msg.payload, rtmp_msg.length);
  }

  return true;
}

bool RtmpConnection::Handshake() {
  uint32_t req_size = 1 + 1536;  // COC1
  std::shared_ptr<char> req(new char[req_size], std::default_delete<char[]>());
  handshake_->BuildC0C1(req.get(), req_size);
  this->Send(req.get(), req_size);
  return true;
}

bool RtmpConnection::Connect() {
  AmfObjects objects;
  amf_encoder_.Reset();

  amf_encoder_.EncodeString("connect", 7);
  amf_encoder_.EncodeNumber((double)(++number_));
  objects["app"] = AmfObject(app_);
  objects["type"] = AmfObject(std::string("nonprivate"));

  if (connection_mode_ == RTMP_PUBLISHER) {
    auto publisher = rtmp_publisher_.lock();
    if (!publisher) {
      return false;
    }
    objects["swfUrl"] = AmfObject(publisher->GetSwfUrl());
    objects["tcUrl"] = AmfObject(publisher->GetTcUrl());
  } else if (connection_mode_ == RTMP_CLIENT) {
    auto client = rtmp_client_.lock();
    if (!client) {
      return false;
    }
    objects["swfUrl"] = AmfObject(client->GetSwfUrl());
    objects["tcUrl"] = AmfObject(client->GetTcUrl());
  }

  amf_encoder_.EncodeObjects(objects);
  connection_state_ = START_CONNECT;
  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  return true;
}

bool RtmpConnection::CreateStream() {
  AmfObjects objects;
  amf_encoder_.Reset();

  amf_encoder_.EncodeString("createStream", 12);
  amf_encoder_.EncodeNumber((double)(++number_));
  amf_encoder_.EncodeObjects(objects);

  connection_state_ = START_CREATE_STREAM;
  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  return true;
}

bool RtmpConnection::Publish() {
  AmfObjects objects;
  amf_encoder_.Reset();

  amf_encoder_.EncodeString("publish", 7);
  amf_encoder_.EncodeNumber((double)(++number_));
  amf_encoder_.EncodeObjects(objects);
  amf_encoder_.EncodeString(stream_name_.c_str(), (int)stream_name_.size());

  connection_state_ = START_PUBLISH;
  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  return true;
}

bool RtmpConnection::Play() {
  AmfObjects objects;
  amf_encoder_.Reset();

  amf_encoder_.EncodeString("play", 4);
  amf_encoder_.EncodeNumber((double)(++number_));
  amf_encoder_.EncodeObjects(objects);
  amf_encoder_.EncodeString(stream_name_.c_str(), (int)stream_name_.size());

  connection_state_ = START_PLAY;
  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  return true;
}

bool RtmpConnection::DeleteStream() {
  AmfObjects objects;
  amf_encoder_.Reset();

  amf_encoder_.EncodeString("DeleteStream", 12);
  amf_encoder_.EncodeNumber((double)(++number_));
  amf_encoder_.EncodeObjects(objects);
  amf_encoder_.EncodeNumber(stream_id_);

  connection_state_ = START_DELETE_STREAM;
  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  return true;
}

bool RtmpConnection::HandleConnect() {
  if (!amf_decoder_.HasObject("app")) {
    return false;
  }

  AmfObject amfObj = amf_decoder_.GetObject("app");
  app_ = amfObj.amf_string;
  if (app_ == "") {
    return false;
  }

  SendAcknowledgement();
  SetPeerBandwidth();
  SetChunkSize();

  // 返回成功状态的 _result 消息
  AmfObjects objects;
  amf_encoder_.Reset();
  amf_encoder_.EncodeString("_result", 7);
  amf_encoder_.EncodeNumber(amf_decoder_.GetNumber());

  objects["fmsVer"] = AmfObject(std::string("FMS/4,5,0,297"));  // fms服务器版本
  objects["capabilities"] = AmfObject(255.0);  // 服务器支持的功能, 取全部
  amf_encoder_.EncodeObjects(objects);
  objects.clear();
  objects["level"] = AmfObject(std::string("status"));
  objects["code"] = AmfObject(std::string("NetConnection.Connect.Success"));
  objects["description"] = AmfObject(std::string("Connection succeeded."));
  objects["objectEncoding"] = AmfObject(0.0);
  amf_encoder_.EncodeObjects(objects);

  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  return true;
}

bool RtmpConnection::HandleCreateStream() {
  int stream_id = rtmp_chunk_->GetStreamId();

  AmfObjects objects;
  amf_encoder_.Reset();
  amf_encoder_.EncodeString("_result", 7);
  amf_encoder_.EncodeNumber(amf_decoder_.GetNumber());
  amf_encoder_.EncodeObjects(objects);
  amf_encoder_.EncodeNumber(stream_id);

  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());
  stream_id_ = stream_id;
  return true;
}

bool RtmpConnection::HandlePublish() {
  // LOG_INFO("[Publish] app: %s, stream name: %s, stream path: %s\n", app_.c_str(),
  //           stream_name_.c_str(), stream_path_.c_str());

  // User Control(StreamBegin)

  auto server = rtmp_server_.lock();
  if (!server) {
    return false;
  }

  AmfObjects objects;
  amf_encoder_.Reset();
  amf_encoder_.EncodeString("onStatus", 8);
  amf_encoder_.EncodeNumber(0);
  amf_encoder_.EncodeObjects(objects);

  bool is_error = false;

  if (server->HasPublisher(stream_path_)) {  // 已经有人在推这个 url 对应的流了
    is_error = true;
    objects["level"] = AmfObject(std::string("error"));
    objects["code"] = AmfObject(std::string("NetStream.Publish.BadName"));
    objects["description"] = AmfObject(std::string("Stream already publishing."));
  } else if (connection_state_ == START_PUBLISH) {  // 该连接已经存在推流, 不能一个连接多个推流
    is_error = true;
    objects["level"] = AmfObject(std::string("error"));
    objects["code"] = AmfObject(std::string("NetStream.Publish.BadConnection"));
    objects["description"] = AmfObject(std::string("Connection already publishing."));
  } else {
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetStream.Publish.Start"));
    objects["description"] = AmfObject(std::string("Start publishing."));
    server->AddSession(stream_path_);
    rtmp_session_ = server->GetSession(stream_path_);

    if (server) {
      server->NotifyEvent("publish.start", stream_path_);
    }
  }

  amf_encoder_.EncodeObjects(objects);
  SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size());

  // 设置状态并加入 session 中来转发数据
  if (is_error) {
    return false;
  } else {
    connection_state_ = START_PUBLISH;
    is_publishing_ = true;
  }

  auto session = rtmp_session_.lock();
  if (session) {
    session->SetGopCacheLen(max_gop_cache_len_);
    session->AddConn(std::dynamic_pointer_cast<RtmpConnection>(shared_from_this()));
  }

  return true;
}

bool RtmpConnection::HandlePlay() {
  LOG_INFO("[Play] app: %s, stream name: %s, stream path: %s\n", app_.c_str(), 
            stream_name_.c_str(), stream_path_.c_str());

  auto server = rtmp_server_.lock();
  if (!server) {
    return false;
  }

  // User Control (StreamIsRecorded) 不需要, 没有实现录制功能
  // User Control (StreamBegin) 可以有, 但是不需要, 有数据就开始播放

  // 1. 返回 NetStream.Play.Reset 控制消息
  AmfObjects objects;
  amf_encoder_.Reset();
  amf_encoder_.EncodeString("onStatus", 8);
  amf_encoder_.EncodeNumber(0);
  amf_encoder_.EncodeObjects(objects);
  objects["level"] = AmfObject(std::string("status"));
  objects["code"] = AmfObject(std::string("NetStream.Play.Reset"));
  objects["description"] = AmfObject(std::string("Resetting and playing stream."));
  amf_encoder_.EncodeObjects(objects);
  if (!SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size())) {
    return false;
  }

  // 2. 返回 NetStream.Play.Start 控制消息
  objects.clear();
  amf_encoder_.Reset();
  amf_encoder_.EncodeString("onStatus", 8);
  amf_encoder_.EncodeNumber(0);
  amf_encoder_.EncodeObjects(objects);
  objects["level"] = AmfObject(std::string("status"));
  objects["code"] = AmfObject(std::string("NetStream.Play.Start"));
  objects["description"] = AmfObject(std::string("Started playing."));
  amf_encoder_.EncodeObjects(objects);
  if (!SendCommandMessage(RTMP_CHUNK_COMMAND_ID, amf_encoder_.Data(), amf_encoder_.Size())) {
    return false;
  }

  // 3. 返回 |RtmpSampleAccess Data 消息
  amf_encoder_.Reset();
  amf_encoder_.EncodeString("|RtmpSampleAccess", 17);
  amf_encoder_.EncodeBoolean(true);
  amf_encoder_.EncodeBoolean(true);
  if (!this->SendDataMessage(RTMP_CHUNK_DATA_ID, amf_encoder_.Data(), amf_encoder_.Size())) {
    return false;
  }

  // 设置 START_PLAY 状态, 将连接加入到 session 中来转发数据
  connection_state_ = START_PLAY;

  rtmp_session_ = server->GetSession(stream_path_);
  auto session = rtmp_session_.lock();
  if (session) {
    session->AddConn(std::dynamic_pointer_cast<RtmpConnection>(shared_from_this()));
  }

  if (server) {
    server->NotifyEvent("play.start", stream_path_);
  }

  return true;
}

bool RtmpConnection::HandlePlay2() {
  /// @todo 根据客户端比特率要求来发送数据
  HandlePlay();
  LOG_INFO("[Play2] stream path: %s\n", stream_path_.c_str());
  return false;
}

bool RtmpConnection::HandleDeleteStream() {
  auto server = rtmp_server_.lock();
  if (!server) {
    return false;
  }

  if (stream_path_ != "") {
    auto session = rtmp_session_.lock();
    if (session) {
      auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
      task_scheduler_->AddTimer(
          [session, conn] {
            session->RemoveConn(conn);
            return false;
          },
          1);

      if (is_publishing_) {
        server->NotifyEvent("publish.stop", stream_path_);
      } else if (is_playing_) {
        server->NotifyEvent("play.stop", stream_path_);
      }
    }

    is_playing_ = false;
    is_publishing_ = false;
    has_key_frame_ = false;
    rtmp_chunk_->Clear();
  }

  return true;
}

bool RtmpConnection::HandleResult(RtmpMessage &rtmp_msg) {
  bool ret = false;

  if (connection_state_ == START_CONNECT) {
    // 连接建立成功之后立即建立流
    if (amf_decoder_.HasObject("code")) {
      AmfObject amfObj = amf_decoder_.GetObject("code");
      if (amfObj.amf_string == "NetConnection.Connect.Success") {
        CreateStream();
        ret = true;
      }
    }
  } else if (connection_state_ == START_CREATE_STREAM) {
    // 流建立成功之后就可以开始推/拉流了
    if (amf_decoder_.GetNumber() > 0) {
      stream_id_ = (uint32_t)amf_decoder_.GetNumber();
      if (connection_mode_ == RTMP_PUBLISHER) {
        this->Publish();
      } else if (connection_mode_ == RTMP_CLIENT) {
        this->Play();
      }

      ret = true;
    }
  }

  return ret;
}

bool RtmpConnection::HandleOnStatus(RtmpMessage &rtmp_msg) {
  bool ret = true;

  if (connection_state_ == START_PUBLISH || connection_state_ == START_PLAY) {
    if (amf_decoder_.HasObject("code")) {
      AmfObject amfObj = amf_decoder_.GetObject("code");
      status_ = amfObj.amf_string;
      if (connection_mode_ == RTMP_PUBLISHER) {
        if (status_ == "NetStream.Publish.Start") {
          is_publishing_ = true;
        } else if (status_ == "NetStream.Publish.BadConnection" ||
                   status_ == "NetStream.Publish.BadName") {
          ret = false;
        }
      } else if (connection_mode_ == RTMP_CLIENT) {
        if (status_ == "NetStream.Play.Start") {
          is_playing_ = true;
        } else if (status_ == "NetStream.Play.UnpublishNotify" ||
                   status_ == "NetStream.Play.BadConnection") {
          ret = false;
        }
      }
    }
  }

  if (connection_state_ == START_DELETE_STREAM) {
    if (amf_decoder_.HasObject("code")) {
      AmfObject amfObj = amf_decoder_.GetObject("code");
      if (amfObj.amf_string != "NetStream.Unpublish.Success") {
        ret = false;
      }
    }
  }

  return ret;
}

bool RtmpConnection::SendMetaData(AmfObjects meta_data) {
  if (this->IsClosed()) {
    return false;
  }

  if (meta_data.size() == 0) {
    return false;
  }

  amf_encoder_.Reset();
  amf_encoder_.EncodeString("onMetaData", 10);
  amf_encoder_.EncodeECMA(meta_data);
  if (!this->SendDataMessage(RTMP_CHUNK_DATA_ID, amf_encoder_.Data(), amf_encoder_.Size())) {
    return false;
  }

  return true;
}

void RtmpConnection::SetPeerBandwidth() {
  //  4 Byte 的大端序的限制带宽大小的数 + 1 Byte 的限制类型
  std::shared_ptr<char> data(new char[5], std::default_delete<char[]>());
  WriteUint32BE(data.get(), peer_bandwidth_);
  /// 限制类型取值如下： 
  /// 0 Hard：收到消息的一端需要按照消息中设置的 Window size 进行限制
  /// 1 Soft：收到消息的一端按照消息中设置的 Window size 或者已经生效的限制进行限制，以两者中较小的为准
  /// 2 Dynamic：如果之前的类型为 Hard，则此消息也为 Hard 类型，否则忽略该类型
  data.get()[4] = 2;
  RtmpMessage rtmp_msg;
  rtmp_msg.type_id = RTMP_BANDWIDTH_SIZE;
  rtmp_msg.payload = data;
  rtmp_msg.length = 5;
  SendRtmpChunks(RTMP_CHUNK_CONTROL_ID, rtmp_msg);
}

void RtmpConnection::SendAcknowledgement() {
  std::shared_ptr<char> data(new char[4], std::default_delete<char[]>());
  WriteUint32BE(data.get(), acknowledgement_size_out_);

  RtmpMessage rtmp_msg;
  rtmp_msg.type_id = RTMP_ACK_SIZE;
  rtmp_msg.payload = data;
  rtmp_msg.length = 4;
  SendRtmpChunks(RTMP_CHUNK_CONTROL_ID, rtmp_msg);
}

void RtmpConnection::SetChunkSize() {
  rtmp_chunk_->SetOutChunkSize(max_chunk_size_);
  std::shared_ptr<char> data(new char[4], std::default_delete<char[]>());
  WriteUint32BE((char *)data.get(), max_chunk_size_);

  RtmpMessage rtmp_msg;
  rtmp_msg.type_id = RTMP_SET_CHUNK_SIZE;
  rtmp_msg.payload = data;
  rtmp_msg.length = 4;
  SendRtmpChunks(RTMP_CHUNK_CONTROL_ID, rtmp_msg);
}

bool RtmpConnection::SendCommandMessage(uint32_t csid, std::shared_ptr<char> payload,
                                       uint32_t payload_size) {
  if (this->IsClosed()) {
    return false;
  }

  RtmpMessage rtmp_msg;
  rtmp_msg.type_id = RTMP_COMMAND_MESSAGE;
  rtmp_msg.timestamp_delta = 0;
  rtmp_msg.stream_id = stream_id_;
  rtmp_msg.payload = payload;
  rtmp_msg.length = payload_size;
  SendRtmpChunks(csid, rtmp_msg);
  return true;
}

bool RtmpConnection::SendDataMessage(uint32_t csid, std::shared_ptr<char> payload,
                                       uint32_t payload_size) {
  if (this->IsClosed()) {
    return false;
  }

  RtmpMessage rtmp_msg;
  rtmp_msg.type_id = RTMP_DATA_MESSAGE;
  rtmp_msg.timestamp_delta = 0;
  rtmp_msg.stream_id = stream_id_;
  rtmp_msg.payload = payload;
  rtmp_msg.length = payload_size;
  SendRtmpChunks(csid, rtmp_msg);
  return true;
}

bool RtmpConnection::IsKeyFrame(std::shared_ptr<char> payload, uint32_t payload_size) {
  uint8_t frame_type = (payload.get()[0] >> 4) & 0x0f;  // 最高的一位表示是否 H264 关键帧, 第二位表示是否 H264 非关键帧
  uint8_t codec_id = payload.get()[0] & 0x0f;  // 低四位表示编码类型
  return (frame_type == 1 && codec_id == RTMP_CODEC_ID_H264);
}

bool RtmpConnection::SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload,
                                   uint32_t payload_size) {
  if (this->IsClosed()) {
    return false;
  }

  if (payload_size == 0) {
    return false;
  }

  is_playing_ = true;

  if (type == RTMP_AVC_SEQUENCE_HEADER) {
    avc_sequence_header_ = payload;
    avc_sequence_header_size_ = payload_size;
  } else if (type == RTMP_AAC_SEQUENCE_HEADER) {
    aac_sequence_header_ = payload;
    aac_sequence_header_size_ = payload_size;
  }

  auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
  task_scheduler_->AddTriggerEvent([conn, type, timestamp, payload, payload_size] {
    // 如果此前没有 I 帧先检查一下当前帧是否为 I 帧
    if (!conn->has_key_frame_ && conn->avc_sequence_header_size_ > 0 &&
        (type != RTMP_AVC_SEQUENCE_HEADER) && (type != RTMP_AAC_SEQUENCE_HEADER)) {
      if (conn->IsKeyFrame(payload, payload_size)) {
        conn->has_key_frame_ = true;
      } else {  // 没有 I 帧就先不发送数据, 继续等待 I 帧到达
        return;
      }
    }

    RtmpMessage rtmp_msg;
    rtmp_msg.absolute_timestamp = timestamp;
    rtmp_msg.stream_id = conn->stream_id_;
    rtmp_msg.payload = payload;
    rtmp_msg.length = payload_size;

    if (type == RTMP_VIDEO || type == RTMP_AVC_SEQUENCE_HEADER) {
      rtmp_msg.type_id = RTMP_VIDEO;
      conn->SendRtmpChunks(RTMP_CHUNK_VIDEO_ID, rtmp_msg);
    } else if (type == RTMP_AUDIO || type == RTMP_AAC_SEQUENCE_HEADER) {
      rtmp_msg.type_id = RTMP_AUDIO;
      conn->SendRtmpChunks(RTMP_CHUNK_AUDIO_ID, rtmp_msg);
    }
  });

  return true;
}

bool RtmpConnection::SendVideoData(uint64_t timestamp, std::shared_ptr<char> payload,
                                   uint32_t payload_size) {
  if (payload_size == 0) {
    return false;
  }

  auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
  task_scheduler_->AddTriggerEvent([conn, timestamp, payload, payload_size] {
    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_VIDEO;
    rtmp_msg.absolute_timestamp = timestamp;
    rtmp_msg.stream_id = conn->stream_id_;
    rtmp_msg.payload = payload;
    rtmp_msg.length = payload_size;
    conn->SendRtmpChunks(RTMP_CHUNK_VIDEO_ID, rtmp_msg);
  });

  return true;
}

bool RtmpConnection::SendAudioData(uint64_t timestamp, std::shared_ptr<char> payload,
                                   uint32_t payload_size) {
  if (payload_size == 0) {
    return false;
  }

  auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
  task_scheduler_->AddTriggerEvent([conn, timestamp, payload, payload_size] {
    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_AUDIO;
    rtmp_msg.absolute_timestamp = timestamp;
    rtmp_msg.stream_id = conn->stream_id_;
    rtmp_msg.payload = payload;
    rtmp_msg.length = payload_size;
    conn->SendRtmpChunks(RTMP_CHUNK_VIDEO_ID, rtmp_msg);
  });
  return true;
}

void RtmpConnection::SendRtmpChunks(uint32_t csid, RtmpMessage &rtmp_msg) {
  uint32_t capacity = rtmp_msg.length + rtmp_msg.length / max_chunk_size_ * 5 + 1024;  // 除了额外头空间外, 预留 1K 的空间
  std::shared_ptr<char> buffer(new char[capacity], std::default_delete<char[]>());

  int size = rtmp_chunk_->CreateChunk(csid, rtmp_msg, buffer.get(), capacity);
  if (size > 0) {
    this->Send(buffer.get(), size);
  }
}

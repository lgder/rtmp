/// @file RtmpServer.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/05/13

#include "RtmpServer.h"

#include "Logger.h"
#include "RtmpConnection.h"
#include "SocketUtil.h"

RtmpServer::RtmpServer(EventLoop* event_loop)
    : TcpServer(event_loop), event_loop_(event_loop), event_callbacks_(10) {
  // 定时关闭无客户端的 session 节省服务器资源  
  event_loop_->AddTimer(
      [this] {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto iter = rtmp_sessions_.begin(); iter != rtmp_sessions_.end();) {
          if (iter->second->GetClientsNum() == 0) {
            rtmp_sessions_.erase(iter++);
          } else {
            iter++;
          }
        }
        return true;
      },
      30000);
}

std::shared_ptr<RtmpServer> RtmpServer::Create(EventLoop* event_loop) {
  std::shared_ptr<RtmpServer> server(new RtmpServer(event_loop));
  return server;
}

void RtmpServer::SetEventCallback(EventCallback event_cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  event_callbacks_.push_back(event_cb);
}

void RtmpServer:: NotifyEvent(std::string event_type, std::string stream_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto event_cb : event_callbacks_) {
    if (event_cb) {
      event_cb(event_type, stream_path);
    }
  }
}

TcpConnection::Ptr RtmpServer::OnConnect(SOCKET sockfd) {
  return std::make_shared<RtmpConnection>(shared_from_this(), event_loop_->GetTaskScheduler().get(),
                                          sockfd);
}

void RtmpServer::AddSession(std::string stream_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (rtmp_sessions_.find(stream_path) == rtmp_sessions_.end()) {
    rtmp_sessions_[stream_path] = std::make_shared<RtmpSession>();
  }
}

void RtmpServer::RemoveSession(std::string stream_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  rtmp_sessions_.erase(stream_path);
}

bool RtmpServer::HasSession(std::string stream_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  return (rtmp_sessions_.find(stream_path) != rtmp_sessions_.end());
}

RtmpSession::Ptr RtmpServer::GetSession(std::string stream_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (rtmp_sessions_.find(stream_path) == rtmp_sessions_.end()) {
    rtmp_sessions_[stream_path] = std::make_shared<RtmpSession>();
  }

  return rtmp_sessions_[stream_path];
}

bool RtmpServer::HasPublisher(std::string stream_path) {
  auto session = GetSession(stream_path);
  if (session == nullptr) {
    return false;
  }

  return (session->GetPublisher() != nullptr);
}

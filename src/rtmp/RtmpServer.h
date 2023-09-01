/// @file RtmpServer.h
/// @brief 应用层实现 rtmp 服务器
///        1. 设置 acceptor 回调
///        2. 管理 url 和 session
///        3. 出现新连接到达或者连接断开事件时调回调函数输出一些日志等
/// @version 0.1
/// @author lq
/// @date 2023/05/13

#ifndef RTMP_SERVER_RTMP_SERVER_H
#define RTMP_SERVER_RTMP_SERVER_H

#include <mutex>
#include <string>

#include "RtmpSession.h"
#include "TcpServer.h"
#include "rtmp.h"

class RtmpServer : public TcpServer, public Rtmp, public std::enable_shared_from_this<RtmpServer> {
 public:
  using EventCallback = std::function<void(std::string event_type, std::string stream_path)>;

  static std::shared_ptr<RtmpServer> Create(EventLoop *event_loop);
  ~RtmpServer() = default;

  void SetEventCallback(EventCallback event_cb);

 private:
  friend class RtmpConnection;

  RtmpServer(EventLoop *event_loop);
  void AddSession(std::string stream_path);
  void RemoveSession(std::string stream_path);
  RtmpSession::Ptr GetSession(std::string stream_path);

  bool HasSession(std::string stream_path);
  bool HasPublisher(std::string stream_path);

  void NotifyEvent(std::string event_type, std::string stream_path);

  TcpConnection::Ptr OnConnect(SOCKET sockfd) override;

  EventLoop *event_loop_;
  std::mutex mutex_;
  std::unordered_map<std::string, RtmpSession::Ptr> rtmp_sessions_;  // <流url, 流会话>
  std::vector<EventCallback> event_callbacks_;
};

#endif  // RTMP_SERVER_RTMP_SERVER_H
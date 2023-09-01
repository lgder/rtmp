/// @file RtmpClient.h
/// @brief 拉流的客户端, Subscriber
/// @version 0.1
/// @author lq
/// @date 2023/06/23

#ifndef RTMP_SERVER_RTMP_CLIENT_H
#define RTMP_SERVER_RTMP_CLIENT_H

#include <mutex>
#include <string>

#include "EventLoop.h"
#include "RtmpConnection.h"
#include "Timestamp.h"

class RtmpClient : public Rtmp, public std::enable_shared_from_this<RtmpClient> {
 public:
  using FrameCallback =
      std::function<void(uint8_t* payload, uint32_t length, uint8_t codecId, uint32_t timestamp)>;

  static std::shared_ptr<RtmpClient> Create(EventLoop* loop);
  ~RtmpClient();

  void SetRecvFrameCB(const FrameCallback& cb);
  int OpenUrl(std::string url, int msec, std::string& status);
  void Close();
  bool IsConnected();

 private:
  friend class RtmpConnection;

  RtmpClient(EventLoop* event_loop);

  std::mutex mutex_;  // 此处的锁不是必要的, SetFrameCB 和 conn 都是只有一个线程会调用
  EventLoop* event_loop_;
  TaskScheduler* task_scheduler_;
  std::shared_ptr<RtmpConnection> rtmp_conn_;
  FrameCallback recv_frame_cb_;
};

#endif  // RTMP_SERVER_RTMP_CLIENT_H
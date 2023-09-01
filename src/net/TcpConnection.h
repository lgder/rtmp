/// @file TcpConnection.h
/// @brief TCP 连接类, 用于封装 TCP 连接读写和销毁操作, 创建在 Acceptor 中
/// @version 0.1
/// @author lq
/// @date 2023/04/30

#ifndef RTMP_SERVER_TCP_CONNECTION_H
#define RTMP_SERVER_TCP_CONNECTION_H

/// @note 每个 Connection 都是在一个独立的 TaskScheduler 的线程中的,
///       这里仅访问内部成员变量的情况下, 理论上不会有资源竞争问题, 这里默认去掉了加锁操作
// #define THEAD_SAFE_TCP_CONNECTION

#include <atomic>
#include <mutex>

#include "BufferReader.h"
#include "BufferWriter.h"
#include "Channel.h"
#include "SocketUtil.h"
#include "TaskScheduler.h"

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
 public:
  using Ptr = std::shared_ptr<TcpConnection>;
  using DisconnectCallback = std::function<void(std::shared_ptr<TcpConnection> conn)>;
  using CloseCallback = std::function<void(std::shared_ptr<TcpConnection> conn)>;
  using ReadCallback =
      std::function<bool(std::shared_ptr<TcpConnection> conn, BufferReader& buffer)>;

  TcpConnection(TaskScheduler* task_scheduler, SOCKET sockfd);
  virtual ~TcpConnection();

  TaskScheduler* GetTaskScheduler() const { return task_scheduler_; }

  void SetReadCallback(const ReadCallback& cb) { read_cb_ = cb; }

  void SetCloseCallback(const CloseCallback& cb) { close_cb_ = cb; }

  void Send(std::shared_ptr<char> data, uint32_t size);
  void Send(const char* data, uint32_t size);

  void Disconnect();

  bool IsClosed() const { return is_closed_; }

  SOCKET GetSocket() const { return channel_->GetSocket(); }

  uint16_t GetPort() const { return SocketUtil::GetPeerPort(channel_->GetSocket()); }

  std::string GetIp() const { return SocketUtil::GetPeerIp(channel_->GetSocket()); }

 protected:
  friend class TcpServer;

  virtual void HandleRead();
  virtual void HandleWrite();
  virtual void HandleClose();
  virtual void HandleError();

  void SetDisconnectCallback(const DisconnectCallback& cb) { disconnect_cb_ = cb; }

  TaskScheduler* task_scheduler_;  // 每个 connection 对应一个 TaskScheduler, 用于处理连接的事件
  std::unique_ptr<BufferReader> read_buffer_;
  std::unique_ptr<BufferWriter> write_buffer_;
  std::atomic_bool is_closed_;  // 可能被 server 线程 session 线程访问, 需要原子操作

 private:
  void Close();

  std::shared_ptr<Channel> channel_;

#ifdef THEAD_SAFE_TCP_CONNECTION
  std::mutex mutex_;
#endif
  DisconnectCallback disconnect_cb_;
  CloseCallback close_cb_;
  ReadCallback read_cb_;
};

#endif  // RTMP_SERVER_TCP_CONNECTION_H

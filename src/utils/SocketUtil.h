/// @file SocketUtil.h
/// @brief socket fd 操作封装
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#ifndef RTMP_SERVER_SOCKET_UTIL_H
#define RTMP_SERVER_SOCKET_UTIL_H

#include <string>

#include "Socket.h"

class SocketUtil {
 public:
  // 设置非阻塞读写
  static void SetNonBlock(SOCKET fd);
  
  // 阻塞读写并设置超时时间
  static void SetBlock(SOCKET fd, int write_timeout = 0);
  
  // 地址复用, 断开连接 TIME_WAIT 状态下的端口重新利用, 解决服务器重启时 address already in use 错误
  static void SetReuseAddr(SOCKET fd);

  // 端口复用, 多个套接字绑定到同一个端口号，并进行负载均衡. 注意和地址复用区分.
  static void SetReusePort(SOCKET sockfd);

  // 禁用Nagle算法，使得小包可以更快地发送; Nagle 和 Delayed Ack 需要关闭一者
  static void SetNoDelay(SOCKET sockfd);

  // TCP 保活选项, 长时间没有数据发送接收时发送探测报文
  static void SetKeepAlive(SOCKET sockfd);

  // 防止在写入已关闭的套接字时收到 SIGPIPE 管道破裂的信号
  static void SetNoSigpipe(SOCKET sockfd);

  // 获取对端的 IP 地址, 返回点分十进制字符串
  static std::string GetPeerIp(SOCKET sockfd);
  
  // 获取套接字绑定的 IP 地址, 返回点分十进制字符串
  static std::string GetSocketIp(SOCKET sockfd);
  
  // 获取套接字 IP 地址和端口号存到传出参数 sockaddr_in 指向的结构体, 成功返回 0, 失败返回 -1
  static int GetSocketAddr(SOCKET sockfd, struct sockaddr_in* addr);

  // 获取对端的端口
  static uint16_t GetPeerPort(SOCKET sockfd);

  // 获取对端 IP 地址和端口号存到传出参数 sockaddr_in 指向的结构体, 成功返回 0, 失败返回 -1
  static int GetPeerAddr(SOCKET sockfd, struct sockaddr_in* addr);

  static void SetSendBufSize(SOCKET sockfd, int size);
  static void SetRecvBufSize(SOCKET sockfd, int size);
  static void Close(SOCKET sockfd);
  static bool Connect(SOCKET sockfd, std::string ip, uint16_t port, int timeout = 0);
  static bool Bind(SOCKET sockfd, std::string ip, uint16_t port);
};

#endif  // RTMP_SERVER_SOCKET_UTIL_H

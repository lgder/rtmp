/// @file Socket.h
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#ifndef RTMP_SERVER_SOCKET_H
#define RTMP_SERVER_SOCKET_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

#include <cstdint>
#include <cstring>

#endif  // RTMP_SERVER_SOCKET_H

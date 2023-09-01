/// @file main.cc
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/04/13

#include <chrono>
#include <cstdint>
#include <string>
#include "EventLoop.h"
#include "RtmpClient.h"
#include "RtmpPublisher.h"
#include "RtmpServer.h"


int TestRtmpPublisher(EventLoop *event_loop);

int main(int argc, const char **argv) {
  uint16_t port = 1935;
  uint32_t thread_num = std::thread::hardware_concurrency();
  EventLoop event_loop(thread_num);

  auto rtmp_server = RtmpServer::Create(&event_loop);
  rtmp_server->SetChunkSize(60000);
  
  rtmp_server->SetGopCache();
  
  rtmp_server->SetEventCallback([](std::string type, std::string stream_path) {
    printf("[Event] %s, stream path: %s\n\n", type.c_str(), stream_path.c_str());
  });

  if (!rtmp_server->Start("0.0.0.0", port)) {
    printf("RTMP Server listen on %d failed.\n", port);
  }

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  rtmp_server->Stop();
  return 0;
}

#pragma once

#if defined(EBUS_INTERNAL)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <ebus/controller.hpp>
#include <memory>
#include <vector>

// ClientManager handles all connected clients and routes data between them and
// the eBus It supports ReadOnly, Regular, and Enhanced clients.

class ClientManager {
 public:
  ClientManager();

  void start(ebus::Controller* controller);
  void stop();

 private:
  struct ServerSocket {
    uint16_t port;
    int listenFd = -1;
  };

  ServerSocket readonlyServer{3334};
  ServerSocket regularServer{3333};
  ServerSocket enhancedServer{3335};

  volatile bool stopRunner = false;
  ebus::Controller* controller = nullptr;

  struct ConnectedClient {
    int fd;
  };
  std::vector<ConnectedClient> clients;

  TaskHandle_t clientManagerTaskHandle;

  static void taskFunc(void* arg);

  static bool createListenSocket(ServerSocket& server);
  static int acceptClient(ServerSocket& server);
  void acceptClients();
};

extern ClientManager clientManager;
#endif

#if defined(EBUS_INTERNAL)
#include "ClientManager.hpp"

#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>

#include <algorithm>
#include <cerrno>

#include "Logger.hpp"

ClientManager clientManager;

ClientManager::ClientManager() = default;

void ClientManager::start(ebus::Controller* controller) {
  createListenSocket(readonlyServer);
  createListenSocket(regularServer);
  createListenSocket(enhancedServer);

  this->controller = controller;

  // Start the clientManagerRunner task
  xTaskCreate(&ClientManager::taskFunc, "clientManagerRunner", 4096, this, 3,
              &clientManagerTaskHandle);
}

void ClientManager::stop() { stopRunner = true; }

bool ClientManager::createListenSocket(ServerSocket& server) {
  if (server.listenFd >= 0) return true;

  server.listenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (server.listenFd < 0) return false;

  int enable = 1;
  setsockopt(server.listenFd, SOL_SOCKET, SO_REUSEADDR, &enable,
             sizeof(enable));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server.port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(server.listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    close(server.listenFd);
    server.listenFd = -1;
    return false;
  }

  if (listen(server.listenFd, 4) != 0) {
    close(server.listenFd);
    server.listenFd = -1;
    return false;
  }

  int flags = fcntl(server.listenFd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(server.listenFd, F_SETFL, flags | O_NONBLOCK);
  }

  return true;
}

int ClientManager::acceptClient(ServerSocket& server) {
  if (server.listenFd < 0 && !createListenSocket(server)) return -1;

  sockaddr_in addr{};
  socklen_t addrLen = sizeof(addr);
  const int clientFd =
      accept(server.listenFd, reinterpret_cast<sockaddr*>(&addr), &addrLen);
  if (clientFd < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) return -1;
    return -1;
  }

  int flag = 1;

  if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
    logger.warn("Failed to set TCP_NODELAY on client socket (fd=" +
                std::to_string(clientFd) + "): " + std::to_string(errno));
  }

  return clientFd;
}

void ClientManager::taskFunc(void* arg) {
  ClientManager* self = static_cast<ClientManager*>(arg);

  for (;;) {
    if (self->stopRunner) {
      self->clientManagerTaskHandle = nullptr;
      vTaskDelete(NULL);
    }

    // Check for new clients
    self->acceptClients();

    // Periodically clean up closed FDs from our tracking list if necessary.
    // Note: The library typically handles the I/O errors and stops using the
    // FD, but we can remove them from our internal list by checking socket
    // status.
    auto it = self->clients.begin();
    while (it != self->clients.end()) {
      char buffer = 0;
      int res = recv(it->fd, &buffer, 1, MSG_PEEK | MSG_DONTWAIT);
      if (res == 0 || (res < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
        logger.info("TCP Client FD " + std::to_string(it->fd) + " closed.");
        self->controller->removeClient(it->fd);
        close(it->fd);
        it = self->clients.erase(it);
      } else {
        ++it;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void ClientManager::acceptClients() {
  auto registerNew = [this](ServerSocket& server, ebus::ClientType type,
                            const char* label) {
    for (;;) {
      const int clientFd = acceptClient(server);
      if (clientFd < 0) break;

      logger.info(std::string(label) +
                  " client connected (FD=" + std::to_string(clientFd) + ")");
      controller->addClient(clientFd, type);
      clients.push_back({clientFd});
    }
  };

  registerNew(readonlyServer, ebus::ClientType::read_only, "ReadOnly");
  registerNew(regularServer, ebus::ClientType::regular, "Regular");
  registerNew(enhancedServer, ebus::ClientType::enhanced, "Enhanced");
}

#endif

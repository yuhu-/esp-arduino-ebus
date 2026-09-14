#pragma once

#if defined(EBUS_INTERNAL)

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

#include "app/command.hpp"
#include "ebus/callbacks.hpp"

// System health monitor: drains log/protocol queues fed by ebus library
// threads and reports heap, socket and task telemetry.
// Instance owned by App; all methods are thread-safe (called from the
// reactor, HTTP and main tasks).
class SystemMonitor {
 public:
  struct Status {
    uint32_t uptime_seconds;
    size_t free_heap;
    size_t min_free_heap;
    size_t largest_free_block;
    int sockets_detected;
    int sockets_connected;
  };

  TaskHandle_t task_handle();

  bool begin();
  void stop();

  void enqueueLogRequest(std::string_view key);
  void enqueueProtocolInfo(const ebus::ProtocolInfo& info);

  size_t getLogQueueSize();
  size_t getLogQueueCapacity();
  size_t getLogQueueHighWatermark();

  size_t getProtocolQueueSize();
  size_t getProtocolQueueCapacity();
  size_t getProtocolQueueHighWatermark();

  void getSocketStatus(int& detected, int& connected);

 private:
  static void taskEntry(void* arg);
  void taskLoop();

  void processLogRequests();
  void processProtocolInfo();

  Status getStatus();
  void collectStatus();
  void logSummary();

  TaskHandle_t task_handle_ = nullptr;
  QueueHandle_t log_queue_ = nullptr;
  QueueHandle_t protocol_queue_ = nullptr;

  Status status_{};
  portMUX_TYPE status_mux_ = portMUX_INITIALIZER_UNLOCKED;
  std::atomic<int> sockets_detected_{0};
  std::atomic<int> sockets_connected_{0};
};

#endif

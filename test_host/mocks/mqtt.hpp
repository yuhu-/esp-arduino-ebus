#pragma once

// Mock Mqtt for host testing - no ESP-IDF dependency.
// Static API matches historical usage; the recording instance API lets tests
// assert what MqttHA would publish without a broker.

#include <cstdint>
#include <ebus/types.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct PublishedMessage {
  std::string topic;
  std::string payload;
};

class Mqtt {
 public:
  static void publishComponentDiscovery() {}
  static void publishValue(std::string_view) {}
  static void publishError(int) {}

  void publish(const char* topic, uint8_t qos, bool retain,
               const char* payload = nullptr, bool prefix = true);
  void publishStream(
      const char* topic, uint8_t qos, bool retain,
      const std::function<void(const ebus::JsonChunkVisitor&)>& builder,
      bool prefix = true);

  static std::vector<PublishedMessage> published;
  static void clearPublished();
};

extern Mqtt mqtt;

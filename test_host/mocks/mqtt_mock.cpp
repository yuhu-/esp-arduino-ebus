// Recording backend for the host Mqtt mock (see mqtt.hpp).

#include <string>

#include "mqtt.hpp"

Mqtt mqtt;

std::vector<PublishedMessage> Mqtt::published;

void Mqtt::clearPublished() { published.clear(); }

void Mqtt::publish(const char* topic, uint8_t qos, bool retain,
                   const char* payload, bool prefix) {
  (void)qos;
  (void)retain;
  (void)prefix;
  published.push_back(
      {topic != nullptr ? topic : "", payload != nullptr ? payload : ""});
}

void Mqtt::publishStream(
    const char* topic, uint8_t qos, bool retain,
    const std::function<void(const ebus::JsonChunkVisitor&)>& builder,
    bool prefix) {
  (void)qos;
  (void)retain;
  (void)prefix;
  std::string payload;
  if (builder) {
    builder([&payload](std::string_view chunk) {
      payload.append(chunk.data(), chunk.size());
    });
  }
  published.push_back({topic != nullptr ? topic : "", payload});
}

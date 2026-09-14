#pragma once

// Mock Mqtt for host testing - no ESP-IDF dependency.
// Only the static façade used by non-MQTT translation units. MqttHA tests
// inject lambdas directly (see mqtt_ha_tests.cpp); no recording needed here.

#include <string>
#include <string_view>

class Mqtt {
 public:
  static void publishComponentDiscovery() {}
  static void publishValue(std::string_view) {}
  static void publishError(int) {}
};

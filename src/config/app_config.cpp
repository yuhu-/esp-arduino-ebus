#include "config/app_config.hpp"

#include <cstdlib>
#include <ebus/detail/json_reader.hpp>
#include <ebus/types.hpp>
#include <string>
#include <string_view>

#include "app/app_limits.hpp"

void AppConfig::reset() {
  *this = AppConfig{};
  network.wifi_ssid = "ebus-test";
  network.wifi_password = "lectronz";
  network.ap_password = "ebusebus";
  sntp.server = "pool.ntp.org";
  sntp.timezone = "UTC0";
  bus.address = "ff";
  mqtt_ha.thing_name = "esp-eBus";
  pwm.value = 130;
  bus.window_us = 4400;
  bus.offset_us = 50;
  bus.system_inquiry = false;
  bus.system_response = true;
  bus.scan_on_startup = false;
}

bool AppConfig::isValid() const {
  // 1. Required network credentials
  if (network.wifi_ssid.empty()) return false;

  // 2. PWM
  if (pwm.value < app::limits::Pwm::min || pwm.value > app::limits::Pwm::max)
    return false;

  // 3. eBUS address must be non-empty and fit the FixedString capacity.
  if (bus.address.empty()) return false;

  // 4. Bus timing
  if (bus.window_us < app::limits::Bus::window_min_us ||
      bus.window_us > app::limits::Bus::window_max_us)
    return false;
  if (bus.offset_us < app::limits::Bus::offset_min_us ||
      bus.offset_us > app::limits::Bus::offset_max_us)
    return false;

  return true;
}

namespace {

bool parseFlatBool(std::string_view value) {
  return value == "selected" || value == "true" || value == "1" ||
         value == "on";
}

// Parses a decimal int from a non-terminated view via a bounded copy.
bool parseFlatInt(std::string_view value, int32_t& out) {
  if (value.empty() || value.size() > 10) return false;
  char buf[12]{};
  const size_t len =
      value.size() < sizeof(buf) - 1 ? value.size() : sizeof(buf) - 1;
  for (size_t i = 0; i < len; ++i) buf[i] = value[i];
  char* end = nullptr;
  const long parsed = std::strtol(buf, &end, 10);
  if (end == buf || *end != '\0') return false;
  out = static_cast<int32_t>(parsed);
  return true;
}

void assignFlatU8(uint8_t& dst, std::string_view value) {
  int32_t parsed = 0;
  if (parseFlatInt(value, parsed) && parsed >= 0 && parsed <= 255) {
    dst = static_cast<uint8_t>(parsed);
  }
}

void assignFlatU16(uint16_t& dst, std::string_view value) {
  int32_t parsed = 0;
  if (parseFlatInt(value, parsed) && parsed >= 0 && parsed <= 65535) {
    dst = static_cast<uint16_t>(parsed);
  }
}

}  // namespace

bool AppConfig::isKnownFlatKey(std::string_view key) {
  return key == "wifiSsid" || key == "wifiPassword" || key == "wifiBssid" ||
         key == "apModePassword" || key == "staticIPEnabled" ||
         key == "ipAddress" || key == "gateway" || key == "netmask" ||
         key == "dns1" || key == "dns2" || key == "sntpEnabled" ||
         key == "sntpServer" || key == "sntpTimezone" || key == "pwmValue" ||
         key == "ebusAddress" || key == "busWindow" || key == "busOffset" ||
         key == "systemInquiry" || key == "systemResponse" ||
         key == "scanOnStartup" || key == "mqttEnabled" ||
         key == "mqttServer" || key == "mqttUser" || key == "mqttPass" ||
         key == "rootTopic" || key == "haEnabled" || key == "thingName" ||
         key == "httpHeaders";
}

bool AppConfig::mergeFlatJson(
    std::string_view json,
    std::vector<std::pair<std::string, std::string>>* unknowns) {
  ebus::detail::JsonReader reader(json);
  if (reader.next() != ebus::detail::JsonReader::Token::object_start) {
    return false;
  }

  while (true) {
    auto token = reader.next();
    if (token == ebus::detail::JsonReader::Token::object_end ||
        token == ebus::detail::JsonReader::Token::end) {
      break;
    }
    if (token != ebus::detail::JsonReader::Token::key) continue;
    std::string key(reader.value());
    if (reader.next() != ebus::detail::JsonReader::Token::string) {
      return false;
    }
    std::string_view value = reader.value();

    if (key == "wifiSsid") {
      network.wifi_ssid.assign(value);
    } else if (key == "wifiPassword") {
      network.wifi_password.assign(value);
    } else if (key == "wifiBssid") {
      network.wifi_bssid.assign(value);
    } else if (key == "apModePassword") {
      network.ap_password.assign(value);
    } else if (key == "staticIPEnabled") {
      network.static_ip_enabled = parseFlatBool(value);
    } else if (key == "ipAddress") {
      network.ip_address.assign(value);
    } else if (key == "gateway") {
      network.gateway.assign(value);
    } else if (key == "netmask") {
      network.netmask.assign(value);
    } else if (key == "dns1") {
      network.dns1.assign(value);
    } else if (key == "dns2") {
      network.dns2.assign(value);
    } else if (key == "sntpEnabled") {
      sntp.enabled = parseFlatBool(value);
    } else if (key == "sntpServer") {
      sntp.server.assign(value);
    } else if (key == "sntpTimezone") {
      sntp.timezone.assign(value);
    } else if (key == "pwmValue") {
      assignFlatU8(pwm.value, value);
    } else if (key == "ebusAddress") {
      bus.address.assign(value);
    } else if (key == "busWindow") {
      assignFlatU16(bus.window_us, value);
    } else if (key == "busOffset") {
      assignFlatU16(bus.offset_us, value);
    } else if (key == "systemInquiry") {
      bus.system_inquiry = parseFlatBool(value);
    } else if (key == "systemResponse") {
      bus.system_response = parseFlatBool(value);
    } else if (key == "scanOnStartup") {
      bus.scan_on_startup = parseFlatBool(value);
    } else if (key == "mqttEnabled") {
      mqtt.enabled = parseFlatBool(value);
    } else if (key == "mqttServer") {
      mqtt.server.assign(value);
    } else if (key == "mqttUser") {
      mqtt.user.assign(value);
    } else if (key == "mqttPass") {
      mqtt.pass.assign(value);
    } else if (key == "rootTopic") {
      mqtt.root_topic.assign(value);
    } else if (key == "haEnabled") {
      mqtt_ha.enabled = parseFlatBool(value);
    } else if (key == "thingName") {
      mqtt_ha.thing_name.assign(value);
    } else if (key == "httpHeaders") {
      http.headers.assign(value);
    } else if (unknowns != nullptr) {
      unknowns->emplace_back(key, std::string(value));
    }
  }

  return true;
}

#include "config/app_config.hpp"

#include <cstdlib>
#include <ebus/detail/json_reader.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/types.hpp>
#include <string>
#include <string_view>

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

void AppConfig::toJson(ebus::detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();

  {
    auto netScope = writer.objectScope("network");
    writer.writeField("wifi_ssid", network.wifi_ssid.c_str());
    writer.writeField("wifi_password", network.wifi_password.c_str());
    writer.writeField("wifi_bssid", network.wifi_bssid.c_str());
    writer.writeField("ap_password", network.ap_password.c_str());

    writer.writeField("static_ip_enabled", network.static_ip_enabled);
    writer.writeField("ip_address", network.ip_address.c_str());
    writer.writeField("gateway", network.gateway.c_str());
    writer.writeField("netmask", network.netmask.c_str());
    writer.writeField("dns1", network.dns1.c_str());
    writer.writeField("dns2", network.dns2.c_str());
  }

  {
    auto sntpScope = writer.objectScope("sntp");
    writer.writeField("enabled", sntp.enabled);
    writer.writeField("server", sntp.server.c_str());
    writer.writeField("timezone", sntp.timezone.c_str());
  }

  writer.writeField("pwm", pwm.value);

  {
    auto busScope = writer.objectScope("bus");
    writer.writeField("window_us", bus.window_us);
    writer.writeField("offset_us", bus.offset_us);
    writer.writeField("address", bus.address.c_str());
    writer.writeField("system_inquiry", bus.system_inquiry);
    writer.writeField("system_response", bus.system_response);
    writer.writeField("scan_on_startup", bus.scan_on_startup);
  }

  {
    auto mqttScope = writer.objectScope("mqtt");
    writer.writeField("enabled", mqtt.enabled);
    writer.writeField("server", mqtt.server.c_str());
    writer.writeField("user", mqtt.user.c_str());
    writer.writeField("pass", mqtt.pass.c_str());
    writer.writeField("root_topic", mqtt.root_topic.c_str());
  }

  {
    auto mqttHaScope = writer.objectScope("mqtt_ha");
    writer.writeField("enabled", mqtt_ha.enabled);
    writer.writeField("thing_name", mqtt_ha.thing_name.c_str());
  }

  writer.writeField("http_headers", http.headers.c_str());
}

AppConfig AppConfig::fromJson(std::string_view json) {
  AppConfig cfg;
  cfg.reset();
  cfg.mergeFromJson(json);
  return cfg;
}

bool AppConfig::mergeFromJson(std::string_view json) {
  ebus::detail::JsonReader reader(json);
  if (reader.next() != ebus::detail::JsonReader::Token::object_start)
    return false;

  reader.forEachField([&](std::string_view key, ebus::detail::JsonReader& r) {
    if (key == "network") {
      if (r.next() == ebus::detail::JsonReader::Token::object_start) {
        r.forEachField(
            [&](std::string_view k, ebus::detail::JsonReader& inner) {
              if (k == "wifi_ssid") {
                inner.next();
                network.wifi_ssid.assign(inner.value());
                return true;
              }
              if (k == "wifi_password") {
                inner.next();
                network.wifi_password.assign(inner.value());
                return true;
              }
              if (k == "wifi_bssid") {
                inner.next();
                network.wifi_bssid.assign(inner.value());
                return true;
              }
              if (k == "ap_password") {
                inner.next();
                network.ap_password.assign(inner.value());
                return true;
              }

              if (k == "static_ip_enabled") {
                inner.next();
                network.static_ip_enabled = inner.asBool();
                return true;
              }
              if (k == "ip_address") {
                inner.next();
                network.ip_address.assign(inner.value());
                return true;
              }
              if (k == "gateway") {
                inner.next();
                network.gateway.assign(inner.value());
                return true;
              }
              if (k == "netmask") {
                inner.next();
                network.netmask.assign(inner.value());
                return true;
              }
              if (k == "dns1") {
                inner.next();
                network.dns1.assign(inner.value());
                return true;
              }
              if (k == "dns2") {
                inner.next();
                network.dns2.assign(inner.value());
                return true;
              }
              return false;
            });
      }
      return true;
    }

    if (key == "sntp") {
      if (r.next() == ebus::detail::JsonReader::Token::object_start) {
        r.forEachField(
            [&](std::string_view k, ebus::detail::JsonReader& inner) {
              if (k == "enabled") {
                inner.next();
                sntp.enabled = inner.asBool();
                return true;
              }
              if (k == "server") {
                inner.next();
                sntp.server.assign(inner.value());
                return true;
              }
              if (k == "timezone") {
                inner.next();
                sntp.timezone.assign(inner.value());
                return true;
              }
              return false;
            });
      }
      return true;
    }

    if (key == "pwm") {
      r.next();
      auto val = r.asNumStrict<uint8_t>();
      if (val) pwm.value = *val;
      return val.has_value();
    }

    if (key == "bus") {
      if (r.next() == ebus::detail::JsonReader::Token::object_start) {
        r.forEachField(
            [&](std::string_view k, ebus::detail::JsonReader& inner) {
              if (k == "window_us") {
                inner.next();
                auto val = inner.asNumStrict<uint16_t>();
                if (val) bus.window_us = *val;
                return val.has_value();
              }
              if (k == "offset_us") {
                inner.next();
                auto val = inner.asNumStrict<uint16_t>();
                if (val) bus.offset_us = *val;
                return val.has_value();
              }
              if (k == "address") {
                inner.next();
                bus.address.assign(inner.value());
                return true;
              }
              if (k == "system_inquiry") {
                inner.next();
                bus.system_inquiry = inner.asBool();
                return true;
              }
              if (k == "system_response") {
                inner.next();
                bus.system_response = inner.asBool();
                return true;
              }
              if (k == "scan_on_startup") {
                inner.next();
                bus.scan_on_startup = inner.asBool();
                return true;
              }
              return false;
            });
      }
      return true;
    }

    if (key == "mqtt") {
      if (r.next() == ebus::detail::JsonReader::Token::object_start) {
        r.forEachField(
            [&](std::string_view k, ebus::detail::JsonReader& inner) {
              if (k == "enabled") {
                inner.next();
                mqtt.enabled = inner.asBool();
                return true;
              }
              if (k == "server") {
                inner.next();
                mqtt.server.assign(inner.value());
                return true;
              }
              if (k == "user") {
                inner.next();
                mqtt.user.assign(inner.value());
                return true;
              }
              if (k == "pass") {
                inner.next();
                mqtt.pass.assign(inner.value());
                return true;
              }
              if (k == "root_topic") {
                inner.next();
                mqtt.root_topic.assign(inner.value());
                return true;
              }
              return false;
            });
      }
      return true;
    }

    if (key == "mqtt_ha") {
      if (r.next() == ebus::detail::JsonReader::Token::object_start) {
        r.forEachField(
            [&](std::string_view k, ebus::detail::JsonReader& inner) {
              if (k == "enabled") {
                inner.next();
                mqtt_ha.enabled = inner.asBool();
                return true;
              }
              if (k == "thing_name") {
                inner.next();
                mqtt_ha.thing_name.assign(inner.value());
                return true;
              }
              return false;
            });
      }
      return true;
    }

    if (key == "http_headers") {
      r.next();
      http.headers.assign(r.value());
      return true;
    }
    return false;
  });

  return true;
}

bool AppConfig::isValidJson(std::string_view json) {
  return ebus::detail::JsonReader::validate(json);
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
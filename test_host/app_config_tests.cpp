#include <catch2/catch_test_macros.hpp>
#include <ebus/detail/json_writer.hpp>
#include <string>

#include "config/app_config.hpp"
#include "config/app_config_validator.hpp"

namespace {

std::string toJsonString(const AppConfig& cfg) {
  std::string out;
  ebus::detail::JsonWriter writer([&out](std::string_view chunk) {
    out.append(chunk.data(), chunk.size());
  });
  cfg.toJson(writer);
  return out;
}

}  // namespace

TEST_CASE("AppConfig reset provides migration-safe defaults", "[app_config]") {
  AppConfig cfg;
  cfg.reset();

  // Values relied upon by the main.cpp migration (must match old fallbacks)
  REQUIRE(cfg.pwm.value == 130);
  REQUIRE(cfg.bus.window_us == 4400);
  REQUIRE(cfg.bus.offset_us == 50);
  REQUIRE(std::string(cfg.bus.address.c_str()) == "ff");
  REQUIRE(cfg.bus.system_inquiry == false);
  REQUIRE(cfg.bus.system_response == true);
  REQUIRE(cfg.bus.scan_on_startup == false);
  REQUIRE(std::string(cfg.sntp.server.c_str()) == "pool.ntp.org");
  REQUIRE(std::string(cfg.sntp.timezone.c_str()) == "UTC0");
  REQUIRE(std::string(cfg.mqtt_ha.thing_name.c_str()) == "esp-eBus");

  // reset() ships test WiFi credentials, so the default snapshot validates
  REQUIRE(config::AppConfigValidator::validate(cfg) == true);
}

TEST_CASE("AppConfig mergeFromJson updates only present keys", "[app_config]") {
  AppConfig cfg;
  cfg.reset();

  REQUIRE(cfg.mergeFromJson(R"({"pwm":200})") == true);
  REQUIRE(cfg.pwm.value == 200);
  // Untouched keys keep their defaults
  REQUIRE(cfg.bus.window_us == 4400);
  REQUIRE(std::string(cfg.bus.address.c_str()) == "ff");

  REQUIRE(cfg.mergeFromJson(R"({"bus":{"window_us":4300,"address":"01"}})") ==
          true);
  REQUIRE(cfg.bus.window_us == 4300);
  REQUIRE(std::string(cfg.bus.address.c_str()) == "01");
  REQUIRE(cfg.bus.offset_us == 50);

  // Unknown keys are ignored, known keys still applied
  REQUIRE(cfg.mergeFromJson(R"({"nope":1,"pwm":100})") == true);
  REQUIRE(cfg.pwm.value == 100);

  // Non-object JSON is rejected (malformed object bodies are leniently
  // ignored field-by-field, matching the implementation)
  REQUIRE(cfg.mergeFromJson(R"(not json)") == false);
}

TEST_CASE("AppConfig fromJson falls back to defaults for missing keys",
          "[app_config]") {
  AppConfig cfg = AppConfig::fromJson(R"({"pwm":200})");

  REQUIRE(cfg.pwm.value == 200);
  // Documented behavior: defaults are used for missing keys
  REQUIRE(cfg.bus.window_us == 4400);
  REQUIRE(cfg.bus.offset_us == 50);
  REQUIRE(std::string(cfg.bus.address.c_str()) == "ff");
  REQUIRE(std::string(cfg.sntp.server.c_str()) == "pool.ntp.org");
}

TEST_CASE("AppConfig toJson/fromJson roundtrip preserves fields",
          "[app_config]") {
  AppConfig cfg;
  cfg.reset();
  cfg.pwm.value = 200;
  cfg.bus.address.assign("01");
  cfg.network.wifi_ssid.assign("my-ssid");
  cfg.mqtt.server.assign("mqtt.local");

  AppConfig restored = AppConfig::fromJson(toJsonString(cfg));

  REQUIRE(restored.pwm.value == 200);
  REQUIRE(std::string(restored.bus.address.c_str()) == "01");
  REQUIRE(std::string(restored.network.wifi_ssid.c_str()) == "my-ssid");
  REQUIRE(std::string(restored.mqtt.server.c_str()) == "mqtt.local");
  REQUIRE(restored.bus.window_us == 4400);
  REQUIRE(config::AppConfigValidator::validate(restored) == true);
}

TEST_CASE("AppConfigValidator accepts a fully populated config",
          "[app_config]") {
  AppConfig cfg;
  cfg.reset();
  cfg.network.wifi_ssid.assign("my-ssid");

  REQUIRE(config::AppConfigValidator::validate(cfg) == true);
}

TEST_CASE("AppConfigValidator rejects out-of-range values", "[app_config]") {
  AppConfig cfg;
  cfg.reset();
  cfg.network.wifi_ssid.assign("my-ssid");

  AppConfig bad = cfg;
  bad.pwm.value = 0;
  REQUIRE(config::AppConfigValidator::validate(bad) == false);

  bad = cfg;
  bad.bus.window_us = 4000;
  REQUIRE(config::AppConfigValidator::validate(bad) == false);

  bad = cfg;
  bad.bus.offset_us = 500;
  REQUIRE(config::AppConfigValidator::validate(bad) == false);

  bad = cfg;
  bad.bus.address.assign("");
  REQUIRE(config::AppConfigValidator::validate(bad) == false);

  bad = cfg;
  bad.network.wifi_ssid.assign("");
  REQUIRE(config::AppConfigValidator::validate(bad) == false);
}

TEST_CASE("AppConfigValidator validateJson guards raw payloads",
          "[app_config]") {
  REQUIRE(config::AppConfigValidator::validateJson(R"({"pwm":200})") == true);
  REQUIRE(config::AppConfigValidator::validateJson(R"({"pwm":0})") == false);
  REQUIRE(config::AppConfigValidator::validateJson(
              R"({"bus":{"window_us":4000}})") == false);
  REQUIRE(config::AppConfigValidator::validateJson(R"({not json)") == false);
  // Unknown keys are ignored
  REQUIRE(config::AppConfigValidator::validateJson(R"({"nope":1})") == true);
}

TEST_CASE("AppConfig isValidJson detects structural errors", "[app_config]") {
  REQUIRE(AppConfig::isValidJson(R"({"pwm":200})") == true);
  REQUIRE(AppConfig::isValidJson(R"({not json)") == false);
}

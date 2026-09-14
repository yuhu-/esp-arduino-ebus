#include <catch2/catch_test_macros.hpp>
#include <string>

#include "command.hpp"
#include "command_manager.hpp"
#include "mqtt.hpp"
#include "mqtt_ha.hpp"

namespace {

MqttHA makeHa() {
  MqttHA ha;
  ha.setUniqueId("820b80");
  ha.setRootTopic("ebus/820b80/");
  ha.setWillTopic("ebus/820b80/state/available");
  ha.setThingName("esp-eBus");
  ha.setThingModel("esp-eBus Adapter");
  ha.setThingModelId("esp-ebus-adapter");
  ha.setThingHwVersion("7.0");
  ha.setEnabled(true);
  return ha;
}

void registerTempCommand() {
  commandManager.wipeCommands();
  std::string json =
      R"({"key":"01","name":"Outside_Temperature","read_cmd":"fe070009",)"
      R"("write_cmd":"","interval":0,"master":true,)"
      R"("fields":[{"name":"value","profile":"data2b_celsius","position":1,)"
      R"("ha_profile":"sensor_temperature"}]})";
  ebus::detail::JsonReader reader(json);
  commandManager.insertCommand(Command::fromJson(reader));
}

bool payloadContains(const PublishedMessage& msg, std::string_view needle) {
  return msg.payload.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("MqttHA publishes sensor discovery config", "[mqttha]") {
  Mqtt::clearPublished();
  registerTempCommand();
  MqttHA ha = makeHa();

  const Command* cmd = commandManager.findCommand("01");
  REQUIRE(cmd != nullptr);
  ha.publishComponent(cmd, 0, false);

  REQUIRE(Mqtt::published.size() == 1);
  const PublishedMessage& msg = Mqtt::published[0];
  REQUIRE(msg.topic ==
          "homeassistant/sensor/ebus820b80/01_outside_temperature_value/"
          "config");
  REQUIRE(payloadContains(msg, "ebus820b80"));
  REQUIRE(payloadContains(msg, "unique_id"));
  REQUIRE(payloadContains(msg, "state_topic"));
  REQUIRE(payloadContains(msg, "device"));
}

TEST_CASE("MqttHA remove publishes empty retained payload", "[mqttha]") {
  Mqtt::clearPublished();
  registerTempCommand();
  MqttHA ha = makeHa();

  const Command* cmd = commandManager.findCommand("01");
  REQUIRE(cmd != nullptr);
  ha.publishComponent(cmd, 0, true);

  REQUIRE(Mqtt::published.size() == 1);
  REQUIRE(Mqtt::published[0].topic ==
          "homeassistant/sensor/ebus820b80/01_outside_temperature_value/"
          "config");
  REQUIRE(Mqtt::published[0].payload.empty());
}

TEST_CASE("MqttHA publishes nothing when disabled", "[mqttha]") {
  Mqtt::clearPublished();
  registerTempCommand();
  MqttHA ha = makeHa();
  ha.setEnabled(false);

  const Command* cmd = commandManager.findCommand("01");
  REQUIRE(cmd != nullptr);
  ha.publishComponentIfEnabled(cmd, 0);

  REQUIRE(Mqtt::published.empty());
}

TEST_CASE("MqttHA skips fields without HA profile", "[mqttha]") {
  Mqtt::clearPublished();
  commandManager.wipeCommands();
  std::string json =
      R"({"key":"02","name":"Plain","read_cmd":"fe070009","write_cmd":"",)"
      R"("interval":0,"master":true,)"
      R"("fields":[{"name":"value","profile":"uint8","position":1,)"
      R"("ha_profile":""}]})";
  ebus::detail::JsonReader reader(json);
  commandManager.insertCommand(Command::fromJson(reader));

  MqttHA ha = makeHa();
  const Command* cmd = commandManager.findCommand("02");
  REQUIRE(cmd != nullptr);
  ha.publishComponent(cmd, 0, false);

  REQUIRE(Mqtt::published.empty());
}

TEST_CASE("MqttHA device info carries thing identity", "[mqttha]") {
  Mqtt::clearPublished();
  MqttHA ha = makeHa();
  ha.publishDeviceInfo();

  REQUIRE(!Mqtt::published.empty());
  bool found_thing = false;
  for (const PublishedMessage& msg : Mqtt::published) {
    if (payloadContains(msg, "esp-eBus") && payloadContains(msg, "danman.eu")) {
      found_thing = true;
    }
  }
  REQUIRE(found_thing == true);
}

#include "config/app_config_validator.hpp"

#include "app/app_limits.hpp"

namespace config {

bool AppConfigValidator::validate(const AppConfig& config) {
  // 1. Required network credentials
  if (config.network.wifi_ssid.empty()) return false;

  // 2. PWM
  if (config.pwm.value < app::limits::Pwm::min ||
      config.pwm.value > app::limits::Pwm::max)
    return false;

  // 3. eBUS address must be non-empty and fit the FixedString capacity.
  if (config.bus.address.empty()) return false;

  // 4. Bus timing
  if (config.bus.window_us < app::limits::Bus::window_min_us ||
      config.bus.window_us > app::limits::Bus::window_max_us)
    return false;
  if (config.bus.offset_us < app::limits::Bus::offset_min_us ||
      config.bus.offset_us > app::limits::Bus::offset_max_us)
    return false;

  return true;
}

}  // namespace config
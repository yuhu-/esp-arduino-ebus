#pragma once

#include "config/app_config.hpp"

namespace config {

/**
 * Validates AppConfig values against protocol limits and logical constraints.
 * Mirrors ebus::detail::ConfigValidator for the application configuration
 * layer.
 */
class AppConfigValidator {
 public:
  static bool validate(const AppConfig& config);
};

}  // namespace config
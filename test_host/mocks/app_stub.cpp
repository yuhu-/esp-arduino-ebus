// Host stub for App: there is no live application object in host tests,
// so ConfigManager HTTP handlers fall back to the legacy direct-NVS path
// (see ConfigManager::handleSet/handleReset).

#include "app/app.hpp"

App* App::instance_ = nullptr;

App* App::instance() { return nullptr; }

bool App::loadConfig() { return false; }

bool App::applyFlatConfigJson(std::string_view body, std::string& error) {
  (void)body;
  error = "no App instance on host";
  return false;
}

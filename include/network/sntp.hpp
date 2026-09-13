#pragma once

#if defined(EBUS_INTERNAL)

#include "config/app_config.hpp"

void initSNTP(const AppConfig::Sntp& sntp);
void setTimezone(const AppConfig::Sntp& sntp);

#endif

#ifndef DOMAIN_CONFIGURATION_CODEC_H
#define DOMAIN_CONFIGURATION_CODEC_H

#include <ArduinoJson.h>

#include <utility>

#include "configuration.h"

namespace configuration
{
    String serializeConfig(const Configuration *config, bool revealSecrets = false);
    String prettyPrintConfig(const char *configStr);
    JsonDocument serializeSunrise(const SunriseSettings *config);
    JsonDocument serializeDaySettings(const AlarmWeekday *config);

    std::pair<bool, Configuration> deserializeConfig(
        const char *configStr, const Configuration &existingConfig);
    SunriseSettings deserializeSunrise(JsonVariantConst doc);
    AlarmWeekday deserializeDaySetting(JsonVariantConst doc);
}

#endif

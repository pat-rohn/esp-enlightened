

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "domain/configuration.h"
#include "domain/configuration_codec.h"

namespace configman
{
    // static const char kPathToConfig = "config.json";
    const char kPathToConfig[] = "/config.json";
    using configuration::AlarmWeekday;
    using configuration::Configuration;
    using configuration::Light;
    using configuration::SunriseSettings;
    using configuration::Time;
    using configuration::weekday_t;
    using configuration::Monday;
    using configuration::Tuesday;
    using configuration::Wednesday;
    using configuration::Thursday;
    using configuration::Friday;
    using configuration::Saturday;
    using configuration::Sunday;

    void begin();

    const Configuration& getConfig();
    void setConfig(Configuration config);

    Configuration readConfig();
    String readConfigAsString();
    bool saveConfig(const Configuration *config);
    // Validate and stage a config update from any task (e.g. async web
    // handlers). Optionally returns the serialized form of the staged config.
    bool stageConfig(const char *configStr, String *serialized = nullptr);
    // Apply + persist a staged config. Must only be called from loop().
    // Returns true if a staged config was applied.
    bool applyStagedConfig();

    std::pair<bool, Configuration> deserializeConfig(const char *configStr);
    SunriseSettings deserializeSunrise(const JsonDocument &doc);
    using configuration::deserializeDaySetting;
    using configuration::serializeConfig;
    using configuration::serializeDaySettings;
    using configuration::serializeSunrise;

}

#endif
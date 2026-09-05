#include "config.h"
#include "config_store.h"

#include <atomic>

namespace configman
{
    Configuration config = Configuration();

    // Handoff slot for config updates coming from the async web server task.
    // Web handlers stage a validated copy here; only loop() (applyStagedConfig)
    // may assign the global config or write flash, so readers in loop() never
    // see the config mutate under them.
    std::atomic<Configuration *> stagedConfig{nullptr};

    void begin()
    {
        configstore::begin();
    }

    const Configuration& getConfig()
    {
        return config;
    }

    void setConfig(Configuration newConfig)
    {
        config = newConfig;
    }

    Configuration readConfig()
    {
        String configStr = configstore::read(kPathToConfig);
        if (configStr.length() <= 0)
        {
            Serial.println("No config was stored");
            auto defaultConf = Configuration();
            if (!saveConfig(&defaultConf))
            {
                Serial.println("Failed to read or create config...");
                return Configuration();
            }
            delay(500);
            Serial.println("Retry reading config...");
            return readConfig();
        }

        auto res = deserializeConfig(configStr.c_str());
        if (!res.first)
        {
            Serial.println("Well, store default then");
            auto defaultConf = Configuration();
            if (!saveConfig(&defaultConf))
            {
                Serial.println("Failed to save config, return default for now...");
                return Configuration();
            }
        }
        config = res.second;
        return getConfig();
    }

    String readConfigAsString()
    {
        Serial.println("readConfigAsString");
        auto configStr = configstore::read(kPathToConfig);
        auto res = deserializeConfig(configStr.c_str());
        if (configStr.isEmpty() || !res.first)
        {
            Configuration defaultConf = Configuration();
            Serial.print("Invalid config. write default.");
            if (!saveConfig(&defaultConf))
            {
                Serial.print("Failed to write default.");
                return "{}";
            }
            delay(1000);
            return readConfigAsString();
        }
        return configuration::prettyPrintConfig(configStr.c_str());
    }

    bool saveConfig(const Configuration *newConfig)
    {
        Serial.println("Save config.");
        config = Configuration(newConfig);
        String configStr = configuration::serializeConfig(
            newConfig, /*revealSecrets=*/true);
        return configstore::write(kPathToConfig, configStr.c_str());
    }

    bool stageConfig(const char *configStr, String *serialized)
    {
        Serial.println("Stage config.");
        auto res = deserializeConfig(configStr);
        if (!res.first)
        {
            Serial.print("Invalid config.");
            return false;
        }
        if (serialized != nullptr)
        {
            *serialized = configuration::serializeConfig(&res.second);
        }
        Configuration *fresh = new Configuration(&res.second);
        delete stagedConfig.exchange(fresh);
        return true;
    }

    bool applyStagedConfig()
    {
        Configuration *pending = stagedConfig.exchange(nullptr);
        if (pending == nullptr)
        {
            return false;
        }
        config = *pending;
        String configStr = configuration::serializeConfig(
            pending, /*revealSecrets=*/true);
        if (!configstore::write(kPathToConfig, configStr.c_str()))
        {
            Serial.println("Failed to write config.");
        }
        delete pending;
        return true;
    }

    std::pair<bool, Configuration> deserializeConfig(const char *configStr)
    {
        return configuration::deserializeConfig(configStr, config);
    }

    SunriseSettings deserializeSunrise(const JsonDocument &doc)
    {
        return configuration::deserializeSunrise(doc.as<JsonVariantConst>());
    }
}

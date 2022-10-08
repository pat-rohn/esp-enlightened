

#include "config.h"
#include <ArduinoJson.h>
#include <sstream>

namespace configman
{

    void begin()
    {
        if (!LittleFS.begin())
        {
            Serial.println("An Error has occurred while mounting LittleFS");
            return;
        }
        Serial.println("LittleFS succesfully mounted");
    }

    Configuration readConfig()
    {
        String configStr = readFileLFS(kPathToConfig);
        if (configStr.length() <= 0)
        {
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
        return res.second;
    }

    void writeConfig(const char *configStr)
    {
        auto res = deserializeConfig(configStr);
        if (!res.first)
        {
            Serial.print("Invalid config.");
            return;
        }
        if (!writeFileLFS(kPathToConfig, configStr))
        {
            Serial.print("Failed to write config.");
        }
    }

    bool saveConfig(const Configuration *config)
    {
        String confStr = serializeConfig(config);
        Serial.print(confStr);
        return writeFileLFS(kPathToConfig, confStr.c_str());
    }

    String readFile(fs::FS &fs, const char *path)
    {
        Serial.printf("Reading file: %s\r\n", path);
        File file = fs.open(path, "r");
        if (!file || file.isDirectory())
        {
            Serial.println("- empty file or failed to open file");
            return String();
        }
        String fileContent;
        while (file.available())
        {
            fileContent += String((char)file.read());
        }
        // Serial.println(fileContent);
        return fileContent;
    }

    String readFileLFS(const char *path)
    {
        return readFile(LittleFS, path);
    }

    bool writeFile(fs::FS &fs, const char *path, const char *message)
    {
        Serial.printf("Writing file: %s\r\n", path);
        File file = fs.open(path, "w");
        if (!file)
        {
            Serial.println("- failed to open file for writing");
            return false;
        }
        if (file.print(message))
        {
            Serial.println("- file written");
        }
        else
        {
            Serial.println("- write failed");
            return false;
        }
        return true;
    }

    bool writeFileLFS(const char *path, const char *message)
    {
        return writeFile(LittleFS, path, message);
    }

    String serializeConfig(const Configuration *config)
    {
        DynamicJsonDocument doc(4096);
        doc["ServerAddress"] = config->ServerAddress;
        doc["SensorID"] = config->SensorID;
        doc["WiFiName"] = config->WiFiName;
        doc["WiFiPassword"] = config->WiFiPassword;
        doc["DhtPin"] = config->DhtPin;
        doc["WindSensorPin"] = config->WindSensorPin;
        doc["RainfallSensorPin"] = config->RainfallSensorPin;
        doc["LEDPin"] = config->LEDPin;
        doc["NumberOfLEDs"] = config->NumberOfLEDs;
        doc["FindSensors"] = config->FindSensors;
        doc["IsOfflineMode"] = config->IsOfflineMode;
        doc["ShowWebpage"] = config->ShowWebpage;
        doc["IsConfigured"] = config->IsConfigured;

        char buffer[2000];

        serializeJsonPretty(doc, buffer);

        return String(buffer);
    }

    std::pair<bool, Configuration> deserializeConfig(const char *configStr)
    {
        std::pair<bool, Configuration> res = std::pair(false, Configuration());
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, configStr);
        if (err.code() != DeserializationError::Code::Ok)
        {
            Serial.printf("Deserializing failed %d\n", err.code());
            Serial.print(configStr);
            return res;
        }
        res.second.IsConfigured = doc["IsConfigured"];
        res.second.ServerAddress = doc["ServerAddress"].as<String>();
        res.second.SensorID = doc["SensorID"].as<String>();
        res.second.WiFiName = doc["WiFiName"].as<String>();
        res.second.WiFiPassword = doc["WiFiPassword"].as<String>();
        res.second.DhtPin = doc["DhtPin"];
        res.second.WindSensorPin = doc["WindSensorPin"];
        res.second.RainfallSensorPin = doc["RainfallSensorPin"];
        res.second.LEDPin = doc["LEDPin"];
        res.second.NumberOfLEDs = doc["NumberOfLEDs"];
        res.second.FindSensors = doc["FindSensors"];
        res.second.IsOfflineMode = doc["IsOfflineMode"];
        res.second.ShowWebpage = doc["ShowWebpage"];
        res.first = true;
        return res;
    }

    String readConfigAsString()
    {
        auto configStr = readFileLFS(kPathToConfig);
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, configStr.c_str());
        if (err.code() != DeserializationError::Code::Ok)
        {
            return String(err.code());
        }

        char buffer[2000];

        serializeJsonPretty(doc, buffer);

        String pretty = buffer;
        Serial.println(pretty);
        return buffer;
    }
}
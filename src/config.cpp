

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
        std::stringstream str;
        str << R"({"ServerAddress":")" << config->ServerAddress.c_str()
            << R"(" ,"SensorID": ")" << config->SensorID.c_str()
            << R"(" ,"WiFiName": ")" << config->WiFiName.c_str()
            << R"(" ,"WiFiPassword": ")" << config->WiFiPassword.c_str()
            << R"(" ,"DhtPin": )" << config->DhtPin
            << R"( ,"WindSensorPin": )" << config->WindSensorPin
            << R"( ,"RainfallSensorPin": )" << config->RainfallSensorPin
            << R"( ,"LEDPin": )" << config->LEDPin
            << R"( ,"NumberOfLEDs": )" << config->NumberOfLEDs
            << R"( ,"FindSensors": )" << (config->FindSensors ? "true" : "false")
            << R"( ,"IsOfflineMode": )" << (config->IsOfflineMode ? "true" : "false")
            << R"( ,"ShowWebpage": )" << (config->ShowWebpage ? "true" : "false")
            << R"( ,"IsConfigured": )" << (config->IsConfigured ? "true" : "false")
            << R"( })" << std::endl;

        return String(str.str().c_str());
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

    String readPrettyJson()
    {
        auto configStr = readFileLFS(kPathToConfig);
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, configStr.c_str());
        if (err.code() != DeserializationError::Code::Ok)
        {
            return "";
        }

        char buffer[2000];

        serializeJsonPretty(doc, buffer);

        String pretty = buffer;
        Serial.println(pretty);
        return buffer;
    }

    void print(const Configuration *config)
    {
        Serial.println("Configuration:");
        Serial.println("------------------------");
        Serial.printf("Server-address: %s\n", config->ServerAddress.c_str());
        Serial.printf("Sensor ID: %s\n", config->SensorID.c_str());
        Serial.printf("WiFi-Name: %s\n", config->WiFiName.c_str());
        Serial.printf("WiFi-Password: %s\n", config->WiFiPassword.c_str());
        Serial.printf("DHT Pin:%d\n", config->DhtPin);
        Serial.printf("WindSensorPin Pin:%d\n", config->WindSensorPin);
        Serial.printf("RainfallSensorPin Pin:%d\n", config->RainfallSensorPin);
        Serial.printf("LED Pin:%d\n", config->LEDPin);
        Serial.printf("Number of LEDs:%d\n", config->NumberOfLEDs);
        Serial.printf("Show Webpage:%s\n", config->ShowWebpage ? "true" : "false");
        Serial.printf("Find sensors:%s\n", config->FindSensors ? "true" : "false");
        Serial.printf("Is Offline Mode:%s\n", config->IsOfflineMode ? "true" : "false");
        Serial.printf("IsConfigured:%s\n", config->IsConfigured ? "true" : "false");
        Serial.println("-------------");
    }

}
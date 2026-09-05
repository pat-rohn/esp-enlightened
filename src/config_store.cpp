#include "config_store.h"

#include <LittleFS.h>

namespace configstore
{
    void begin()
    {
        Serial.println("LittleFS.begin()");
#ifdef ESP8266
        if (!LittleFS.begin())
        {
            Serial.println("Failed to mount LittleFS");
        }
        else
        {
            Serial.println("LittleFS successfully mounted");
        }
#endif
#ifdef ESP32
        if (!LittleFS.begin(false))
        {
            Serial.println("Failed to mount LittleFS");
            if (!LittleFS.begin(true))
            {
                Serial.println("Failed to format LittleFS");
            }
            else
            {
                Serial.println("LittleFS formatted successfully");
            }
        }
        else
        {
            Serial.println("LittleFS successfully mounted");
        }
#endif
    }

    String read(const char *path)
    {
        Serial.printf("Reading file: %s\r\n", path);
        File file = LittleFS.open(path, "r");
        if (!file || file.isDirectory())
        {
            Serial.println("- empty file or failed to open file");
            return String();
        }

        String fileContent;
        while (file.available())
        {
            fileContent += String(static_cast<char>(file.read()));
        }
        return fileContent;
    }

    bool write(const char *path, const char *message)
    {
        Serial.printf("Writing file: %s\r\n", path);
        File file = LittleFS.open(path, "w");
        if (!file)
        {
            Serial.println("- failed to open file for writing");
            return false;
        }
        if (!file.print(message))
        {
            Serial.println("- write failed");
            return false;
        }
        Serial.println("- file written");
        return true;
    }
}

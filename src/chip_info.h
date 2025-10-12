

#ifndef CHIPINFO_H
#define CHIPINFO_H
#include <Arduino.h>

const String getChipInfo()
{
#ifdef ESP32
    String chipInfo = "Chip: ";

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    chipInfo += ESP.getChipModel();
    chipInfo += " Rev" + String(chip_info.revision);
    chipInfo += ", " + String(chip_info.cores) + " cores";
    chipInfo += ", WiFi" + String((chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "");
    chipInfo += ", " + String(ESP.getFlashChipSize() / (1024 * 1024)) + "MB Flash";

    Serial.println(chipInfo);

    return chipInfo;
#endif
    return "ESP8266";
}

#endif
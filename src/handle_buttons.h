#ifndef HANDLE_BUTTONS_H
#define HANDLE_BUTTONS_H

#include <Arduino.h>
#include "led/leds_service.h"
#include "led/sunrise_alarm.h"

enum class light_level
{
    off = 0,
    low = 1,
    medium = 2,
    high = 3
};

extern light_level level;
extern unsigned long buttonResetTime;

void handleButton1(sunrise::CSunriseAlarm *sunrise, LedStrip *leds);
void handleButton2();
void handleButtons(sunrise::CSunriseAlarm *sunrise, LedStrip *leds);

#endif // HANDLE_BUTTONS_H

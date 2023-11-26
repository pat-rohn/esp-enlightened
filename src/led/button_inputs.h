
#include <Arduino.h>
#include "ledstrip.h"
#include "sunrise_alarm.h"

#ifndef LED_INPUTS_H
#define LED_INPUTS_H

namespace button_inputs
{
    void start(LedStrip *ledstr, sunrise::CSunriseAlarm *sunriseAlarm, int pin1, int pin2);
    IRAM_ATTR void detectsChangeButton1();
    IRAM_ATTR void detectsChangeButton2();
}

#endif
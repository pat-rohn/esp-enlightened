
#include <Arduino.h>
#include "ledstrip.h"

#ifndef LED_INPUTS_H
#define LED_INPUTS_H

namespace led_inputs
{

    void start(LedStrip *ledstr, int pin1, int pin2);
    IRAM_ATTR void detectsChangeButton1();
    IRAM_ATTR void detectsChangeButton2();
}

#endif
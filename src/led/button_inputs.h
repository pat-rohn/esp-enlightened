#ifndef BUTTON_INPUTS_H
#define BUTTON_INPUTS_H
#include <Arduino.h>

namespace button_inputs
{
    const unsigned long debounceTime = 200;
    struct Button
    {
        int pin;
        volatile bool pressed;
    };

    extern volatile unsigned long buttonTime1;
    extern volatile unsigned long buttonTime2;
    extern Button button1;
    extern Button button2;

    IRAM_ATTR void button1Pressed();
    IRAM_ATTR void button2Pressed();
    void start();
}

#endif

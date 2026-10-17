#ifndef BUTTON_INPUTS_H
#define BUTTON_INPUTS_H
#include <Arduino.h>

namespace button_inputs
{
    const unsigned long debounceTime = 200;
    // Pin must stay LOW this long (every sample) to count as a press; rejects EMI spikes
    const unsigned long stableTime = 50;
    struct Button
    {
        int pin;
        volatile bool pressed;
        unsigned long lowSince;
        bool lowActive;
        bool armed;
    };

    extern Button button1;
    extern Button button2;

    void start();
    void update();
}

#endif

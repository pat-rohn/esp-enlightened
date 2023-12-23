#ifndef BUTTON_INPUTS_H
#define BUTTON_INPUTS_H
#include <Arduino.h>

namespace button_inputs
{
    struct Button
    {
        int pin;
        bool pressed;
    };

    unsigned long buttonTime1 = millis();
    unsigned long buttonTime2 = millis();
    Button button1 = {-1, false};
    Button button2 = {-1, false};

    IRAM_ATTR void button1Pressed()
    {
        if (button1.pressed || millis() < buttonTime1 + 200)
        {
            return;
        }
        buttonTime1 = millis();
        button1.pressed = true;
    }

    IRAM_ATTR void button2Pressed()
    {
        if (button2.pressed || millis() < buttonTime2 + 200)
        {
            return;
        }
        buttonTime2 = millis();
        button2.pressed = true;
    }

    void start()
    {
        Serial.printf("Button 1: %d  -  Button 2: %d \n", button1.pin, button2.pin);
        if (button1.pin > 0)
        {
            pinMode(button1.pin, INPUT_PULLDOWN);
            attachInterrupt(digitalPinToInterrupt(button1.pin), button1Pressed, FALLING);
        }
        if (button2.pin > 0)
        {
            pinMode(button2.pin, INPUT_PULLDOWN);
            attachInterrupt(digitalPinToInterrupt(button2.pin), button2Pressed, FALLING);
        }
    }
}

#endif
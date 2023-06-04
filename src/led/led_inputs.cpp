#include "led_inputs.h"

namespace led_inputs
{
    LedStrip *leds;
    void start(LedStrip *ledstr, int pin1, int pin2)
    {
        leds = ledstr;

        Serial.printf("Button 1: %d  -  Button 2: %d \n", pin1, pin2);
        if (pin1 > 0)
        {
            attachInterrupt(digitalPinToInterrupt(pin1), detectsChangeButton1, RISING);
        }
        if (pin2 > 0)
        {
            attachInterrupt(digitalPinToInterrupt(pin2), detectsChangeButton1, RISING);
        }
    }

    IRAM_ATTR void detectsChangeButton1()
    {
        Serial.println("Button 1 pressed");
        LedStrip::LEDModes mode = leds->m_LEDMode;
        if (mode == LedStrip::LEDModes::off)
        {
            Serial.println("Turn on (default)");
            leds->m_LEDMode = LedStrip::LEDModes::on;
            leds->m_Factor=0.4;
            leds->setColor(100, 75, 35);

            leds->applyModeAndColor();
        }
        else
        {
            if (leds->m_Factor >= 1.0)
            {
                Serial.println("Turn off");
                leds->m_LEDMode = LedStrip::LEDModes::off;
                leds->applyModeAndColor();
            }
            else
            {
                leds->m_Factor += 0.3;
                Serial.printf("Brighter :%f\n", leds->m_Factor);
                leds->applyColorImmediate();
            }
        }
    }

    IRAM_ATTR void detectsChangeButton2()
    {
        Serial.println("Button 2 pressed");
    }

}
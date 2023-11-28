#include "button_inputs.h"

namespace button_inputs
{
    LedStrip *leds;
    sunrise::CSunriseAlarm *sunriseAlarm;
    unsigned long buttonTime1 = millis();

    void start(LedStrip *ledstr, sunrise::CSunriseAlarm *sunrise, int pin1, int pin2)
    {
        leds = ledstr;
        sunriseAlarm = sunrise;

        Serial.printf("Button 1: %d  -  Button 2: %d \n", pin1, pin2);
        if (pin1 > 0)
        {
            attachInterrupt(digitalPinToInterrupt(pin1), detectsChangeButton1, FALLING);
        }
        if (pin2 > 0)
        {
            attachInterrupt(digitalPinToInterrupt(pin2), detectsChangeButton2, FALLING);
        }
    }

    IRAM_ATTR void detectsChangeButton1()
    {
        Serial.println("Button 1 pressed");
        if (millis() < buttonTime1 + 200)
        {
            Serial.println("Prevent double press.");
            return;
        }
        buttonTime1 = millis();

        if (sunriseAlarm != nullptr && sunriseAlarm->run())
        {
            Serial.println("interrupt sunrise");
            sunriseAlarm->interruptAlarm();
            return;
        }
        LedStrip::LEDModes mode = leds->m_LEDMode;
        if (mode == LedStrip::LEDModes::off)
        {
            Serial.println("Turn on (default)");
            leds->m_LEDMode = LedStrip::LEDModes::on;
            leds->m_Factor = 0.35;
            leds->setColor(100, 75, 35);

            leds->applyModeAndColor();
        }
        else
        {
            if (leds->m_Factor >= 1.95)
            {
                Serial.println("Turn off");
                leds->m_LEDMode = LedStrip::LEDModes::off;
                leds->applyModeAndColor();
            }
            else
            {
                leds->m_Factor += 0.8;
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
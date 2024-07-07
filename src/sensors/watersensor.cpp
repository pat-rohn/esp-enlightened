#include "watersensor.h"
#include <Arduino.h>
/*



*/

namespace watersensor
{
    unsigned long clickCounter = 0;
    unsigned long lastTime = millis();
    unsigned long lastClickCount = 0;
    unsigned long lastClickTime = millis();

    double getClicks()
    {
        double clicks = clickCounter - lastClickCount;
        lastClickCount = clickCounter;
        return clicks;
    }

    IRAM_ATTR void detectRainClick()
    {
        unsigned long now = millis();
        if (now - lastClickTime < 250)
        {
            // Serial.print(" . "); // skip
            return;
        }
        lastClickTime = now;
        Serial.print("Counter;Vol:  ");
        Serial.print(clickCounter);
        Serial.print("; ");

        clickCounter++;
    }

    void start(uint8_t pin)
    {
        Serial.print("Water Sensor on pin: ");
        Serial.println(pin);
        pinMode(pin, INPUT);
        attachInterrupt(digitalPinToInterrupt(pin), detectRainClick, RISING);
    }

    double getFactor()
    {
        double areaX = 0.110;
        double areaY = 0.055;
        double testVolume = 0.2;
        double clicks = 200; // test1 188
        return 1.0 / (clicks / testVolume / (areaX * areaY));
    }

};

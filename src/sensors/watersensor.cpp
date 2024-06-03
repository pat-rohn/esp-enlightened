#include "watersensor.h"
#include <Arduino.h>
/*



*/

namespace watersensor
{
    int clickCounter = 0;
    int lastTime = millis();
    int lastClickCount = 0;
    unsigned long lastClickTime = millis();

    double getValue()
    {
        return getFactor() * clickCounter;
    }

    double getClicks()
    {
        return clickCounter;
    }

    double getFlow()
    {
        double diffFlow = (clickCounter - lastClickCount) * getFactor();
        lastClickCount = getClicks();
        return diffFlow / (millis() - lastTime) * 1000.0;
    }

    IRAM_ATTR void detectRainClick()
    {
        unsigned long now = millis();
        if (now - lastClickTime < 250)
        {
            //Serial.print(" . "); // skip
            return;
        }
        lastClickTime = now;
        Serial.print("Counter;Vol:  ");
        Serial.print(clickCounter);
        Serial.print("; ");
        Serial.println(getValue(), 8);

        clickCounter++;
    }

    void start(uint8_t pin)
    {
        Serial.print("Water Sensor on pin: ");
        Serial.println(pin);
        clickCounter++;
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

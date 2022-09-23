#include "watersensor.h"
#include <Arduino.h>
/*



*/

namespace watersensor
{
    int clickCounter = 0;
    int lastTime = millis();
    int lastClickCount = 0;

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

    IRAM_ATTR void detectsChange()
    {
        Serial.print("Counter;Vol:  ");
        Serial.print(clickCounter);
        Serial.print("; ");
        Serial.println(getValue(), 8);

        clickCounter++;
    }

    void start(uint8_t pin)
    {
        clickCounter++;
        attachInterrupt(digitalPinToInterrupt(pin), detectsChange, RISING);
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

#include "windsensor.h"
#include <Arduino.h>
/*



*/

namespace windsensor
{
    unsigned long clickCounter = 0;
    unsigned long lastTime = millis();
    unsigned long lastClickCount = 0;

    double getValue()
    {
        return getFactor() * clickCounter;
    }

    double getClicks()
    {
        return clickCounter;
    }

    double getSpeed()
    {
        double diffFlow = (clickCounter - lastClickCount) * getFactor();
        double diffTime = (millis() - lastTime) / 1000.0;
        Serial.print("DiffTime:  ");
        Serial.println(diffTime);
        Serial.print("DiffFlow:  ");
        Serial.println(diffFlow);
        lastClickCount = getClicks();
        lastTime = millis();
        return diffFlow / diffTime;
    }

    IRAM_ATTR void detectsChange()
    {
        Serial.print("Counter:  ");
        Serial.print(clickCounter);
        Serial.print("; ");

        clickCounter++;
    }

    void start(uint8_t pin)
    {
        Serial.print("Start windsensor with pin:  ");
        Serial.print(pin);
        attachInterrupt(digitalPinToInterrupt(pin), detectsChange, RISING);
    }

    double getFactor()
    {
        return 1 / 50.0;
    }

};

#include "windsensor.h"
#include <Arduino.h>
/*



*/

namespace windsensor
{
    unsigned long clickCounter = 0;
    unsigned long lastTime = 0;
    unsigned long lastClickCount = 0;

    unsigned long lastPeakTime = 0;
    unsigned long lastPeakClickCount = 0;
    unsigned long lastClickTime = 0;
    double peakSpeed = 0;

    double getValue()
    {
        return getFactor() * clickCounter;
    }

    double getClicks()
    {
        return clickCounter;
    }

    std::pair<double, double> getSpeed()
    {
        double diff = (clickCounter - lastClickCount) * getFactor();
        double diffTime = (millis() - lastTime) / 1000.0;
        lastClickCount = getClicks();
        lastTime = millis();
        std::pair<double, double> res = std::make_pair(diff / diffTime, getPeak());
        peakSpeed = 0.0;
        return res;
    }

    double getPeak()
    {
        return peakSpeed;
    }

    IRAM_ATTR void detectsChange()
    {
        unsigned long now = millis();
        if (now - lastClickTime < 50)
        {
            //Serial.print(" . "); // skip
            return;
        }

        lastClickTime = now;
        clickCounter++;
        if (clickCounter % 3 == 0)
        {
            double diff = (clickCounter - lastPeakClickCount) * getFactor();
            double diffTime = (now - lastPeakTime) / 1000.0;
            double currentSpeed = diff / diffTime;
            if (peakSpeed < currentSpeed)
            {
                Serial.print("New Peak: ");
                Serial.println(currentSpeed);
                peakSpeed = currentSpeed;
            }
            lastPeakClickCount = getClicks();
            lastPeakTime = now;
        }
    }

    void start(uint8_t pin)
    {
        Serial.print("Start windsensor with pin:  ");
        Serial.println(pin);
        attachInterrupt(digitalPinToInterrupt(pin), detectsChange, RISING);
        lastPeakTime = millis();
        lastTime = millis();
    }

    double getFactor()
    {
        return 1 / 50.0;
    }

};

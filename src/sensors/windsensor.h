
#include <Arduino.h>

#ifndef WIND_SENSOR_H
#define WIND_SENSOR_H

namespace windsensor
{
    double getValue();
    double getClicks();
    double getSpeed();
    void start(uint8_t pin);
    double getFactor();

}

#endif
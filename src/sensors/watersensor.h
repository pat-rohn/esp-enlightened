
#include <Arduino.h>

#ifndef WATER_SENSOR_H
#define WATER_SENSOR_H

namespace watersensor
{
    double getValue();
    double getClicks();
    void start(uint8_t pin);
    double getFactor();

}

#endif
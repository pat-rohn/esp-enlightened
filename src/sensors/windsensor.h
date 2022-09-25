
#include <Arduino.h>

#ifndef WIND_SENSOR_H
#define WIND_SENSOR_H

/*
"Anemometer"
*/
namespace windsensor
{
    double getValue();
    double getClicks();
    std::pair<double, double> getSpeed();
    double getPeak();
    void start(uint8_t pin);
    double getFactor();

}

#endif
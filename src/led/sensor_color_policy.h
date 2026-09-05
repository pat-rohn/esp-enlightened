#ifndef SENSOR_COLOR_POLICY_H
#define SENSOR_COLOR_POLICY_H

#include <map>
#include "ledstrip.h"
#include "sensors/sensors.h"

namespace led
{
    void applySensorColor(LedStrip &ledStrip, const std::map<String, sensor::SensorData> &values);
}

#endif

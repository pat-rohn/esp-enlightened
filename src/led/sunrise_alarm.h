
#include <Arduino.h>
#include <array>
#include <vector>
#include "ledstrip.h"

#ifndef SUNRISE_ALARM_H
#define SUNRISE_ALARM_H

namespace sunrise
{
    class CSunriseAlarm
    {
        CSunriseAlarm(LedStrip *ledStrip);
        virtual ~CSunriseAlarm();
        
    };

}

#endif
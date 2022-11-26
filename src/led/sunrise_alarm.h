
#ifndef SUNRISE_ALARM_H
#define SUNRISE_ALARM_H

#include <Arduino.h>
#include "ledstrip.h"
#include "timehelper.h"
#include "config.h"

namespace sunrise
{
    class CSunriseAlarm
    {
    public:
        CSunriseAlarm(LedStrip *ledStrip, CTimeHelper *timelper);
        virtual ~CSunriseAlarm(){};

        void applySettings(const configman::SunriseSettings &settings);
        bool run();

    private:
        LedStrip *m_LedStrip;
        CTimeHelper *m_TimeHelper;
        configman::SunriseSettings m_Settings;
        bool m_IsAlarmActive;
        unsigned long m_AlarmStartTime;
        unsigned long m_AlarmEndTime;
    };

}

#endif
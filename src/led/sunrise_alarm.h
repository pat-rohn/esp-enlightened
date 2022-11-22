
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

        void setAlarmTime(const configman::Time &alarmTime, double maxAlarmTime);
        bool run();

    private:
        LedStrip *m_LedStrip;
        CTimeHelper *m_TimeHelper;
        configman::Time m_AlarmTime;
        bool m_IsAlarmActive;
        int m_MaxAlarmTime;
        unsigned long m_AlarmStartTime;
        unsigned long m_AlarmEndTime;
    };

}

#endif
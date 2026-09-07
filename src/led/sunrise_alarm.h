
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
        void interruptAlarm();

        // Runs the sunrise now, over `durationSeconds`, without touching the
        // stored schedule [F11]. Verifying wiring, brightness and colour
        // otherwise means setting an alarm a few minutes ahead and waiting.
        void startTest(double durationSeconds);

        // True while a sunrise is playing, scheduled or test. The main loop
        // needs this to know whether the strip is currently the alarm's.
        bool isRunning() const { return m_IsAlarmActive; }

    private:
        void beginSunrise(double durationSeconds);
        void stopSunrise();

    private:
        LedStrip *m_LedStrip;
        CTimeHelper *m_TimeHelper;
        configman::SunriseSettings m_Settings;
        bool m_IsAlarmActive;
        unsigned long m_AlarmEndTime;
    };

}

#endif
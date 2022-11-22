#include "sunrise_alarm.h"

using namespace sunrise;

CSunriseAlarm::CSunriseAlarm(LedStrip *ledStrip, CTimeHelper *timelper) : m_LedStrip(ledStrip),
                                                                          m_TimeHelper(timelper)
{
    m_IsAlarmActive = false;
    m_AlarmStartTime = millis();
    m_AlarmEndTime = millis();
    m_MaxAlarmTime = 20 * 60 * 1000;
}

void CSunriseAlarm::setAlarmTime(const configman::Time &alarmTime, double maxAlarmTime)
{
    m_AlarmTime = alarmTime;
    m_MaxAlarmTime = maxAlarmTime * 60 * 1000;
}

bool CSunriseAlarm::run()
{
    auto currentTime = m_TimeHelper->getHoursAndMinutes();
    if ((m_AlarmTime.Hours == currentTime.first &&
         m_AlarmTime.Minutes == currentTime.second)) // ||  true  test alarm
    {
        if (!m_IsAlarmActive)
        {
            Serial.printf("Activate Sunrise: %ld:%ld (%d)\n",
                          currentTime.first, currentTime.second, m_MaxAlarmTime);
            m_AlarmEndTime = millis() + m_MaxAlarmTime;
            m_IsAlarmActive = true;
            m_AlarmStartTime = millis();
            m_LedStrip->m_LEDMode = LedStrip::LEDModes::sunrise;
            m_LedStrip->m_SunriseStartTime = m_AlarmStartTime;
            m_LedStrip->m_SunriseMaxTime = m_MaxAlarmTime/2;
            m_LedStrip->runModeAction();
            return true;
        }
    }
    if (m_IsAlarmActive)
    {
        if (millis() > m_AlarmEndTime)
        {
            Serial.printf("Turn off Sunrise: %ld:%ld ", currentTime.first, currentTime.second);
            m_IsAlarmActive = false;
            m_LedStrip->m_LEDMode = LedStrip::LEDModes::off;

            m_LedStrip->apply();
            return false;
        }

        m_LedStrip->runModeAction();
        return true;
    }
    return false;
}
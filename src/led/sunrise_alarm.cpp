#include "sunrise_alarm.h"

using namespace sunrise;

CSunriseAlarm::CSunriseAlarm(LedStrip *ledStrip, CTimeHelper *timelper) : m_LedStrip(ledStrip),
                                                                          m_TimeHelper(timelper)
{
    m_Settings = configman::SunriseSettings();
    m_AlarmStartTime = millis();
    m_AlarmEndTime = millis();
    m_IsAlarmActive = false;
}

void CSunriseAlarm::applySettings(const configman::SunriseSettings &settings)
{
    m_Settings = settings;
    auto doc = configman::serializeSunrise(&settings);

    char buffer[2000];
    serializeJsonPretty(doc, buffer);
    Serial.printf("Applied settings: %s", buffer);
}

bool CSunriseAlarm::run()
{
    configman::weekday_t weekday = static_cast<configman::weekday_t>(m_TimeHelper->getWeekDay());
    if (!m_Settings.DaySettings.at(weekday).IsActive)
    {
        return false;
    }
    auto currentTime = m_TimeHelper->getHoursAndMinutes();
    auto alarmTime = m_Settings.DaySettings.at(weekday).AlarmTime;
    if ((alarmTime.Hours == currentTime.first &&
         alarmTime.Minutes == currentTime.second)) // ||  true  test alarm
    {
        if (!m_IsAlarmActive)
        {
            Serial.printf("Activate Sunrise: %ld:%ld (%f)\n",
                          currentTime.first, currentTime.second, m_Settings.SunriseLightTime);
            m_IsAlarmActive = true;
            m_AlarmStartTime = millis();
            m_AlarmEndTime = millis() + m_Settings.SunriseLightTime * 60 * 1000;
            m_LedStrip->m_LEDMode = LedStrip::LEDModes::sunrise;
            m_LedStrip->m_SunriseStartTime = m_AlarmStartTime;
            m_LedStrip->m_SunriseMaxTime = m_Settings.SunriseLightTime * 60 * 1000 / 2;
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
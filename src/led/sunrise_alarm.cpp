#include "sunrise_alarm.h"

using namespace sunrise;

CSunriseAlarm::CSunriseAlarm(LedStrip *ledStrip, CTimeHelper *timelper) : m_LedStrip(ledStrip),
                                                                          m_TimeHelper(timelper)
{
    m_Settings = configman::SunriseSettings();
    m_AlarmEndTime = millis();
    m_IsAlarmActive = false;
}

void CSunriseAlarm::applySettings(const configman::SunriseSettings &settings)
{
    m_Settings = settings;
    auto doc = configman::serializeSunrise(&settings);

    char buffer[2000];
    serializeJsonPretty(doc, buffer);
    if (settings.IsActivated)
    {
        Serial.printf("Applied settings: %s", buffer);
    }
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
    if ((!m_IsAlarmActive && (alarmTime.Hours == currentTime.first &&
                              alarmTime.Minutes == currentTime.second))) // ||  !m_IsAlarmActive  test alarm
    {

        Serial.printf("Activate Sunrise: %ld:%ld (%f)\n",
                      currentTime.first, currentTime.second, m_Settings.SunriseLightTime);
        startSunrise();
        return true;
    }
    if (m_IsAlarmActive && millis() > m_AlarmEndTime)
    {
        Serial.printf("Stop Sunrise: %ld:%ld ", currentTime.first, currentTime.second);
        stopSunrise();
        return false;
    }
    if (m_IsAlarmActive)
    {
        Serial.printf("Alarm active for %f s\n", double(millis() - m_LedStrip->m_SunriseStartTime) / 1000.0);
        m_LedStrip->runModeAction();
        return true;
    }

    return false;
}

void CSunriseAlarm::interruptAlarm()
{
    m_AlarmEndTime = millis();
}

void CSunriseAlarm::startSunrise()
{
    m_IsAlarmActive = true;
    m_AlarmEndTime = millis() + m_Settings.SunriseLightTime * 60 * 1000;
    m_LedStrip->m_Factor = 0.0;
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::sunrise;
    m_LedStrip->m_SunriseStartTime = millis();
    Serial.printf("Alarm start/end: %ld/%ld", m_LedStrip->m_SunriseStartTime, m_AlarmEndTime);
    m_LedStrip->m_SunriseDuration = m_Settings.SunriseLightTime * 60 / 2;
    m_LedStrip->applyModeAndColor();
    m_LedStrip->runModeAction();
}

void CSunriseAlarm::stopSunrise()
{
    m_IsAlarmActive = false;
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::off;

    m_LedStrip->applyModeAndColor();
}
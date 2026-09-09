#include "sunrise_alarm.h"
#include "domain/deadline.h"

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
    if (!m_Settings.IsActivated && m_IsAlarmActive)
    {
        Serial.println("Alarm deactivated - stop running sunrise");
        stopSunrise();
    }
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
    // A sunrise that is already playing has to be advanced and stopped no
    // matter what the schedule says -- a test alarm [F11] runs while the
    // schedule is disarmed, and previously the activation guards below would
    // have returned early and left it playing forever.
    if (m_IsAlarmActive)
    {
        if (timing::deadlineReached(millis(), m_AlarmEndTime))
        {
            Serial.println("Stop Sunrise");
            stopSunrise();
            return false;
        }
        m_LedStrip->runModeAction();
        return true;
    }

    if (!m_Settings.IsActivated)
    {
        return false;
    }
    configman::weekday_t weekday = static_cast<configman::weekday_t>(m_TimeHelper->getWeekDay());
    if (!m_Settings.DaySettings.at(weekday).IsActive)
    {
        return false;
    }
    auto currentTime = m_TimeHelper->getHoursAndMinutes();

    auto alarmTime = m_Settings.DaySettings.at(weekday).AlarmTime;
    if (alarmTime.Hours == currentTime.first && alarmTime.Minutes == currentTime.second)
    {
        Serial.printf("Activate Sunrise: %ld:%ld (%f)\n",
                      currentTime.first, currentTime.second, m_Settings.SunriseLightTime);
        beginSunrise(m_Settings.SunriseLightTime * 60.0);
        return true;
    }

    return false;
}

void CSunriseAlarm::startTest(double durationSeconds)
{
    Serial.printf("Test sunrise over %f s\n", durationSeconds);
    beginSunrise(durationSeconds);
}

void CSunriseAlarm::interruptAlarm()
{
    m_AlarmEndTime = millis();
}

// The light starts dark and reaches full brightness `durationSeconds` later,
// so the sunrise *begins* at the alarm time rather than finishing there.
void CSunriseAlarm::beginSunrise(double durationSeconds)
{
    m_IsAlarmActive = true;
    m_AlarmEndTime = millis() + static_cast<unsigned long>(durationSeconds * 1000.0);
    m_LedStrip->m_Factor = 0.0;
    m_LedStrip->m_Owner = LedStrip::LEDOwner::sunrise;
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::sunrise;
    m_LedStrip->m_SunriseStartTime = millis();
    Serial.printf("Alarm start/end: %ld/%ld", m_LedStrip->m_SunriseStartTime, m_AlarmEndTime);
    m_LedStrip->m_SunriseDuration = durationSeconds;
    m_LedStrip->applyModeAndColor();
    m_LedStrip->runModeAction();
}

void CSunriseAlarm::stopSunrise()
{
    m_IsAlarmActive = false;
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::off;

    m_LedStrip->applyModeAndColor();
}
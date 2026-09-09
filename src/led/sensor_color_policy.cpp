#include "sensor_color_policy.h"

namespace led
{
    void applySensorColor(LedStrip &ledStrip, const std::map<String, sensor::SensorData> &values)
    {
        // Take the strip over only when there is actually a reading to show.
        // This used to switch to pulse mode first and check afterwards, so a
        // device with no sensors -- every pin at -1, which is what a
        // light-only build looks like -- had its mode forced to pulse every
        // measurement interval, overwriting whatever the user had just set.
        const bool hasCO2 = values.count("CO2") && values.at("CO2").isValid;
        const bool hasTemperature =
            values.count("Temperature") && values.at("Temperature").isValid;
        if (!hasCO2 && !hasTemperature)
        {
            return;
        }

        if (ledStrip.m_LEDMode != LedStrip::LEDModes::pulse)
        {
            Serial.printf("Changed to pulse mode. Mode was %d\n", int(ledStrip.m_LEDMode));
            ledStrip.m_LEDMode = LedStrip::LEDModes::pulse;
        }
        ledStrip.m_Owner = LedStrip::LEDOwner::sensor;

        if (hasCO2)
        {
            ledStrip.setCO2Color(values.at("CO2").value);
        }
        else
        {
            ledStrip.setTemperatureColor(values.at("Temperature").value);
        }
    }
}

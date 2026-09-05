#include "sensor_color_policy.h"

namespace led
{
    void applySensorColor(LedStrip &ledStrip, const std::map<String, sensor::SensorData> &values)
    {
        if (ledStrip.m_LEDMode != LedStrip::LEDModes::pulse)
        {
            Serial.printf("Changed to pulse mode. Mode was %d\n", int(ledStrip.m_LEDMode));
            ledStrip.m_LEDMode = LedStrip::LEDModes::pulse;
        }

        if (values.empty())
        {
            Serial.println("No Sensors");
            return;
        }
        if (values.count("CO2") && values.at("CO2").isValid)
        {
            ledStrip.setCO2Color(values.at("CO2").value);
        }
        else if (values.count("Temperature"))
        {
            ledStrip.setTemperatureColor(values.at("Temperature").value);
        }
    }
}

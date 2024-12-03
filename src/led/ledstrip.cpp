#include "ledstrip.h"

LedStrip::LedStrip(uint8_t pin, int nrOfPixels) : m_Pixels(nrOfPixels, pin, NEO_GRB + NEO_KHZ800),
                                                  m_PulseMode(),
                                                  m_FlameMode(),
                                                  m_ColorfulMode(),
                                                  m_PixelColors(nrOfPixels)
{
    m_NextLEDActionTime = millis();
    m_CurrentColor = std::array<uint8_t, 3>{100, 70, 35};
    m_OldCurrentColor = std::array<uint8_t, 3>{100, 70, 35};
    m_SunriseDuration = 2 * 60;
    m_Factor = 0.35;
    m_LEDMode = LEDModes::on;
    m_LedColor = LEDColor::white;
    m_NrOfPixels = nrOfPixels;
    m_UseAllLEDs = true;
    Serial.printf("LedStrip with pin %d (%d)\n", pin, nrOfPixels);
}

void LedStrip::beginPixels(bool doFancyStartup)
{
    Serial.printf("beginPixels (%d)\n", m_NrOfPixels);
    m_Pixels.begin();
    m_Pixels.updateLength(m_NrOfPixels);
    if (doFancyStartup)
    {
        fancy();
    }
}

void LedStrip::applyModeAndColor()
{
    Serial.printf("Apply mode: %d (%f)\n", int(m_LEDMode), m_Factor);
    switch (m_LEDMode)
    {
    case LEDModes::on:
    {
        applyColorImmediate();
        break;
    }
    case LEDModes::off:
    {
        Serial.println("switch LEDs Off.");
        m_Pixels.clear();
        m_Pixels.show();
        m_Pixels.updateLength(m_NrOfPixels);
        break;
    }
    case LEDModes::sunrise:
        Serial.println("Started sunrise.");
        m_Factor = 0;
        break;
    case LEDModes::pulse:
    {
        m_PulseMode.LowerLimit = 0.05;
        m_PulseMode.UpperLimit = 0.5;
        m_PulseMode.StepSize = 0.002;
        m_PulseMode.UpdateInterval = 40;
        break;
    }
    case LEDModes::campfire:
    {
        break;
    }
    case LEDModes::colorful:
    {
        break;
    }
    default:
        Serial.printf("unkown mode: %d\n", int(m_LEDMode));
        break;
    }
}

void LedStrip::updateLEDs(bool allAtOnce, bool ignoreOnNothingChanged /*false*/)
{
    for (int i = 0; i < m_NrOfPixels; i++)
    {
        if (!m_UseAllLEDs && i % 2 == 0)
        {
            m_Pixels.setPixelColor(i, m_Pixels.Color(0, 0, 0));
        }
        else
        {
            m_Pixels.setPixelColor(i,
                                   m_Pixels.Color(
                                       m_CurrentColor.at(0) * m_Factor,
                                       m_CurrentColor.at(1) * m_Factor,
                                       m_CurrentColor.at(2) * m_Factor));
        }
        if (!allAtOnce)
        {
            m_Pixels.show();
            delay(25);
        }
    }
    if (allAtOnce)
    {
        if (m_OldCurrentColor.at(0) == int(double(m_CurrentColor.at(0)) * m_Factor) &&
            m_OldCurrentColor.at(1) == int(double(m_CurrentColor.at(1)) * m_Factor) &&
            m_OldCurrentColor.at(2) == int(double(m_CurrentColor.at(2)) * m_Factor))
        {
            if (!ignoreOnNothingChanged)
            {
                m_Pixels.show();
            }
            // skip sending when nothing changed
            // Serial.printf("skip show (%d,%d,%d)", m_OldCurrentColor.at(0), m_OldCurrentColor.at(1), m_OldCurrentColor.at(2));
            // Serial.printf("-> (%d,%d,%d)", m_CurrentColor[0]*m_Factor, m_CurrentColor[1]*m_Factor, m_CurrentColor[2]*m_Factor);
        }
        else
        {
            // Serial.printf("show: %d", int(m_CurrentColor.size()));
            m_OldCurrentColor.at(0) = m_CurrentColor.at(0);
            m_OldCurrentColor.at(1) = m_CurrentColor.at(1);
            m_OldCurrentColor.at(2) = m_CurrentColor.at(2);

            m_Pixels.show();
        }
    }
}

void LedStrip::applyColorSmoothly()
{
    double currentFactor = m_Factor;
    m_Factor = 0.1;

    for (double f = 0; f < currentFactor; f = f + 0.02)
    {
        m_Factor = f;
        updateLEDs(true, true);
        delay(50);
    }

    m_Factor = currentFactor;

    updateLEDs(true);
}

void LedStrip::fancy()
{
    Serial.println("fancy");
    double currentFactor = m_Factor;
    for (double f = currentFactor; f > 0.1; f = f - 0.01)
    {
        m_Factor = f;
        updateLEDs(true, true);
        delay(20);
    }
    for (double f = 0; f < currentFactor; f = f + 0.01)
    {
        m_Factor = f;
        updateLEDs(true, true);
        delay(20);
    }

    m_Factor = currentFactor;
}

void LedStrip::pulseMode()
{
    unsigned long currentTime = millis();
    if (currentTime > m_PulseMode.NextUpdateTime)
    {
        m_PulseMode.NextUpdateTime = currentTime + m_PulseMode.UpdateInterval;
        if (m_PulseMode.IsIncreasing)
        {
            m_Factor += m_PulseMode.StepSize;
            if (m_Factor > m_PulseMode.UpperLimit)
            {
                m_PulseMode.IsIncreasing = false;
                Serial.println("Decrease");
            }
        }
        else
        {
            m_Factor -= m_PulseMode.StepSize;
            if (m_Factor < m_PulseMode.LowerLimit)
            {
                m_PulseMode.IsIncreasing = true;
                Serial.println("Increase");
            }
        }

        updateLEDs(true, true);
    }
}

void LedStrip::sunriseMode()
{
    unsigned long currentTime = millis();
    double timeDiff = float(currentTime - m_SunriseStartTime) / 1000.0;
    double timeFactor = (timeDiff / m_SunriseDuration) + 0.05;
    if (timeFactor > 1.2)
    {
        timeFactor = 1.2;
    }
    int blue = timeFactor * 13 - 5;
    if (blue < 0)
    {
        blue = 0;
    }
    m_CurrentColor[0] = 100;
    m_CurrentColor[1] = 10 + timeFactor * 35;
    m_CurrentColor[2] = blue;
    m_Factor = timeFactor;
    // Serial.printf("Sunrise: %f (%f/%f) (%d %d %d)\n", m_Factor, timeDiff,
    //               m_SunriseDuration, m_CurrentColor[0], m_CurrentColor[1], m_CurrentColor[2]);

    updateLEDs(true, true);
}

void LedStrip::showError()
{
    m_LedColor = LEDColor::red;
    updateLEDs(false);
    delay(5000);
    m_LedColor = LEDColor::white;
    updateLEDs(false);
}

int LedStrip::runModeAction()
{
    switch (m_LEDMode)
    {
    case LEDModes::colorful:
        colorfulMode();
        return 50;
        break;
    case LEDModes::campfire:
        campfireMode();
        return 10;
        break;
    case LEDModes::pulse:
        pulseMode();
        return 10;
        break;
    case LEDModes::sunrise:
        sunriseMode();
        return 100;
        break;
    case LEDModes::on:
    case LEDModes::off:
    default:
        return 500;
        break;
    }
}

void LedStrip::colorfulMode()
{
    const double maxBrightness = 4.0 * m_Factor;

    for (int i = 0; i < m_NrOfPixels; i++)
    {
        if (i % 30 < 10)
        {
            m_PixelColors.pRed[(m_ColorfulMode.LedActioncounter + i) % m_NrOfPixels] = static_cast<double>(i % 10) * maxBrightness;
            m_PixelColors.pGreen[((m_ColorfulMode.LedActioncounter + i) + 10) % m_NrOfPixels] = static_cast<double>(i % 10) * maxBrightness;
            m_PixelColors.pBlue[(m_ColorfulMode.LedActioncounter + i + 20) % m_NrOfPixels] = static_cast<double>(i % 10) * maxBrightness;
        }
    }

    showPixels();
    int waitTime = 0;
    if (m_ColorfulMode.LedActioncounter % 100 > 50)
    {
        waitTime = 20 + (m_ColorfulMode.LedActioncounter % 50);
    }
    else
    {
        waitTime = 70 - (m_ColorfulMode.LedActioncounter % 50);
    }
    delay(waitTime);

    m_ColorfulMode.LedActioncounter++;
    m_NextLEDActionTime = millis() + waitTime;
}

void LedStrip::campfireMode()
{
    if (millis() < m_NextLEDActionTime)
    {
        return;
    }

    std::vector<uint8_t> colorTemplate{46, 47, 49, 51, 53, 55, 55, 56, 57, 58,
                                       57, 56, 55, 54, 52, 51, 50, 49, 49, 50,
                                       51, 53, 54, 55, 55, 54, 52, 50, 48, 47};

    for (int i = 0; i <= m_NrOfPixels; i++)
    {
        if (colorTemplate.size() <= uint8_t(i))
        {
            colorTemplate.emplace_back(colorTemplate[i % 30]);
        }
    }
    double random = (rand() % 100) / 100.0;
    if (random > 0.1)
    {
        m_FlameMode.Brightness += 0.005;
    }
    else if (random < 0.9)
    {
        m_FlameMode.Brightness -= 0.005;
    }
    if (m_FlameMode.Brightness <= 0.65)
    {
        m_FlameMode.Brightness += 0.005;
    }
    else if (m_FlameMode.Brightness >= 1.0)
    {
        m_FlameMode.Brightness -= 0.005;
    }

    random = rand() % 100;
    if (random < 10)
    {
        m_FlameMode.LastIndexGreen -= 1;
    }
    else if (random > 90)
    {
        m_FlameMode.LastIndexGreen += 1;
    }

    if (m_FlameMode.LastIndexGreen >= m_NrOfPixels - 1)
    {
        m_FlameMode.LastIndexGreen = m_NrOfPixels - 1;
    }
    else if (m_FlameMode.LastIndexGreen <= 0)
    {
        m_FlameMode.LastIndexGreen = 0;
    }
    random = rand() % 100;
    if (random < 10)
    {
        m_FlameMode.LastIndexRed -= 1;
    }
    else if (random > 90)
    {
        m_FlameMode.LastIndexRed += 1;
    }

    if (m_FlameMode.LastIndexRed >= m_NrOfPixels - 1)
    {
        m_FlameMode.LastIndexRed = m_NrOfPixels - 1;
    }
    else if (m_FlameMode.LastIndexRed <= 0)
    {
        m_FlameMode.LastIndexRed = 0;
    }

    double brightness = m_FlameMode.Brightness * m_Factor;
    for (int i = 0; i < m_NrOfPixels; i++)
    {
        m_PixelColors.pRed[i] = static_cast<double>(colorTemplate[m_FlameMode.LastIndexRed]) * brightness;
        m_PixelColors.pGreen[i] = static_cast<double>(colorTemplate[m_FlameMode.LastIndexGreen]) * brightness / 4;
        m_PixelColors.pBlue[i] = 0.1 * brightness;
    }

    m_FlameMode.LedActioncounter++;
    m_NextLEDActionTime = millis() + 10;

    showPixels();
}

void LedStrip::applyColorImmediate()
{
    updateLEDs(true);
}

void LedStrip::setColor(uint8_t red, uint8_t green, uint8_t blue)
{
    m_CurrentColor.at(0) = red;
    m_CurrentColor.at(1) = green;
    m_CurrentColor.at(2) = blue;
    Serial.printf("\nColors Values (%d) - red: %d, green: %d, blue: %d\n",
                  m_NrOfPixels, m_CurrentColor.at(0), m_CurrentColor.at(1), m_CurrentColor.at(2));
}

std::array<uint8_t, 3> LedStrip::getColor()
{
    return m_CurrentColor;
}

void LedStrip::showPixels()
{
    for (int i = 0; i < m_NrOfPixels; i++)
    {
        m_Pixels.setPixelColor(i, m_Pixels.Color(m_PixelColors.pRed[i], m_PixelColors.pGreen[i], m_PixelColors.pBlue[i]));
    }
    m_Pixels.show();
}

void LedStrip::setCO2Color(double co2Val)
{
    // good: 0-800 (white to yellow)
    // medium: 800-1000 (yellow to red)
    // bad:1000:1800 (red to dark)
    double blue = (1.0 - (co2Val - 400) / 400) * 100.0;
    if (blue > 100.0)
    {
        blue = 100;
    }
    if (blue < 0)
    {
        blue = 0;
    }
    double green = (1.0 - (co2Val - 800) / 600) * 100.0;
    if (green > 100.0)
    {
        green = 100;
    }
    if (green < 0)
    {
        green = 0;
    }
    double red = (1.0 - (co2Val - 1400) / 1000) * 100.0;
    if (red > 100.0)
    {
        red = 100;
    }
    if (red < 0)
    {
        red = 0;
    }
    setColor(red, green, blue);
}

void LedStrip::setTemperatureColor(double temperature)
{
    double blue = (1.0 - (temperature - 15) / 5) * 100.0;
    if (blue > 100.0)
    {
        blue = 100;
    }
    if (blue < 0)
    {
        blue = 0;
    }
    double green = (1.0 - (temperature - 20) / 5) * 100.0;
    if (green > 100.0)
    {
        green = 100;
    }
    if (green < 0)
    {
        green = 0;
    }
    double red = (1.0 - (temperature - 25) / 5) * 100.0;
    if (red > 100.0)
    {
        red = 100;
    }
    if (red < 0)
    {
        red = 0;
    }
    setColor(red, green, blue);
}

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
    m_Factor = 0.35;
    m_LEDMode = LEDModes::on;
    m_LedColor = LEDColor::white;
    m_NrOfPixels = nrOfPixels;
    m_UseAllLEDs = true;
    Serial.printf("LedStrip with pin %d (%d)\n", pin, nrOfPixels);
}

void LedStrip::beginPixels()
{
    Serial.printf("beginPixels (%d)\n", m_NrOfPixels);
    m_Pixels.begin();

    fancy();
}

void LedStrip::apply()
{
    if (m_LEDMode == LEDModes::autochange)
    {
        changeColor(true);
    }
    else
    {
        m_LedColor = LEDColor::white;

        if (m_LEDMode == LEDModes::off)
        {
            Serial.println("switch LEDs Off.");
            m_Pixels.clear();
            m_Pixels.show();
            return;
        }
    }

    updateLEDs(true);
    if (m_LEDMode == LEDModes::on) // todo: hack
    {
        m_Pixels.show();
    }
}

void LedStrip::updateLEDs(bool doImmediate)
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
                                       m_CurrentColor[0] * m_Factor,
                                       m_CurrentColor[1] * m_Factor,
                                       m_CurrentColor[2] * m_Factor));
        }
        if (!doImmediate)
        {
            m_Pixels.show();
            delay(25);
        }
    }
    if (doImmediate)
    {
        if (m_OldCurrentColor[0] == int(double(m_CurrentColor[0]) * m_Factor) &&
            m_OldCurrentColor[1] == int(double(m_CurrentColor[1]) * m_Factor) &&
            m_OldCurrentColor[2] == int(double(m_CurrentColor[2]) * m_Factor))
        {
            // skip sending when nothing changed
            // Serial.printf("skip show (%d,%d,%d)", m_OldCurrentColor[0], m_OldCurrentColor[1], m_OldCurrentColor[2]);
            // Serial.printf("-> (%d,%d,%d)", m_CurrentColor[0]*m_Factor, m_CurrentColor[1]*m_Factor, m_CurrentColor[2]*m_Factor);
        }
        else
        {
            for (uint8_t i = 0; i < m_CurrentColor.size(); i++)
            {
                m_OldCurrentColor.at(i) = m_CurrentColor.at(i) * m_Factor;
            }
            m_Pixels.show();
        }
    }
}

void LedStrip::changeColor(bool autoChange)
{
    double currentFactor = m_Factor;
    int delayTime = 20;
    for (double f = currentFactor; f > 0.1; f = f - 0.02)
    {
        m_Factor = f;
        updateLEDs(true);
        delay(delayTime);
    }
    Serial.printf("old color: %d\n", (int)m_LedColor);
    int nextColor = (int)m_LedColor + 1;
    if (nextColor > 3)
    {
        nextColor = 0;
    }
    m_LedColor = LEDColor(nextColor);
    Serial.printf("new color: %d\n", (int)m_LedColor);
    switch (m_LedColor)
    {
    case LEDColor::red:
        m_CurrentColor[0] = 100;
        m_CurrentColor[1] = 0;
        m_CurrentColor[2] = 0;
        break;
    case LEDColor::green:
        m_CurrentColor[0] = 0;
        m_CurrentColor[1] = 100;
        m_CurrentColor[2] = 0;
        break;
    case LEDColor::blue:

        m_CurrentColor[0] = 0;
        m_CurrentColor[1] = 0;
        m_CurrentColor[2] = 100;
        break;
    case LEDColor::white:
        m_CurrentColor[0] = 100;
        m_CurrentColor[1] = 100;
        m_CurrentColor[2] = 100;
        break;
    default:
        break;
    }
    for (double f = 0; f < currentFactor; f = f + 0.02)
    {
        m_Factor = f;
        updateLEDs(true);
        delay(delayTime);
    }

    m_Factor = currentFactor;
}

void LedStrip::fancy()
{
    Serial.println("fancy");
    double currentFactor = m_Factor;
    for (double f = currentFactor; f > 0.1; f = f - 0.01)
    {
        m_Factor = f;
        updateLEDs(true);
        delay(20);
    }
    for (double f = 0; f < currentFactor; f = f + 0.01)
    {
        m_Factor = f;
        updateLEDs(true);
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
                // Serial.println("Go down");
            }
        }
        else
        {
            m_Factor -= m_PulseMode.StepSize;
            if (m_Factor < m_PulseMode.LowerLimit)
            {
                m_PulseMode.IsIncreasing = true;
                // Serial.println("Go up");
            }
        }

        updateLEDs(true);
    }
}

void LedStrip::sunriseMode()
{
    unsigned long currentTime = millis();
    long timeDiff = currentTime - m_SunriseStartTime;
    double timeFactor = (float(timeDiff) / float(m_SunriseMaxTime)) + 0.1;
    if (timeFactor > 1.2)
    {
        timeFactor = 1.2;
    }
    m_CurrentColor[0] = 100;
    m_CurrentColor[1] = 10 + timeFactor * 35;
    m_CurrentColor[2] = 0 + timeFactor / 1.5 * 15;
    m_Factor = timeFactor;

    // Serial.printf("Sunrise: %f (%ld/%ld)\n", m_Factor, timeDiff, m_SunriseMaxTime);

    updateLEDs(true);
}

void LedStrip::showError()
{
    m_LedColor = LEDColor::red;
    updateLEDs();
    delay(5000);
    m_LedColor = LEDColor::white;
    updateLEDs();
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
        return 50;
        break;
    case LEDModes::pulse:
        pulseMode();
        return 10;
        break;
    case LEDModes::sunrise:
        sunriseMode();
        return 50;
        break;
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
        m_PixelColors.pGreen[i] = static_cast<double>(colorTemplate[m_FlameMode.LastIndexGreen]) * brightness / 2.3;
        m_PixelColors.pBlue[i] = 0.1 * brightness;
    }

    m_FlameMode.LedActioncounter++;
    m_NextLEDActionTime = millis() + 10;

    showPixels();
}

void LedStrip::setColor(uint8_t red, uint8_t green, uint8_t blue)
{
    m_CurrentColor.at(0) = red;
    m_CurrentColor.at(1) = green;
    m_CurrentColor.at(2) = blue;
    for (int i = 0; i < m_NrOfPixels; i++)
    {
        m_Pixels.setPixelColor(i, m_Pixels.Color(m_CurrentColor.at(0), m_CurrentColor.at(1), m_CurrentColor.at(2)));
    }
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
    apply();
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
    apply();
}

#include "ledstrip.h"

#ifdef __AVR__
#include <avr/power.h>
#endif

LedStrip::LedStrip(uint8_t pin) : m_Pixels(NUMPIXELS, pin, NEO_GRB + NEO_KHZ800),
                                  m_FlameMode(),
                                  m_ColorfulMode()
{
    m_NextLEDActionTime = millis();
    m_CurrentColor = std::array<uint8_t, 3>{100, 100, 100};
    m_Factor = 0.3;
    m_CurrentColorStr = "White";
    m_LEDMode = LEDModes::on;
}

void LedStrip::beginPixels()
{
    Serial.println("beginPixels.");
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
#endif
    m_Pixels.begin();
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

        if (m_LEDMode != LEDModes::on)
        {
            Serial.println("Turn Leds Off.");
            m_Pixels.clear();
            m_Pixels.show();
            return;
        }
    }

    updateLEDs(true);
}

void LedStrip::updateLEDs(bool doImmediate)
{
    
    m_Pixels.clear();

    for (int i = 0; i < NUMPIXELS; i++)
    {
        m_Pixels.setPixelColor(i, m_Pixels.Color(m_CurrentColor[0] * m_Factor, m_CurrentColor[1] * m_Factor, m_CurrentColor[2] * m_Factor));
        if (!doImmediate)
        {
            m_Pixels.show();
            delay(25);
            Serial.print(" .");
        }
    }
    if (doImmediate)
    {
        m_Pixels.show();
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
    Serial.println("color");
    switch (m_LedColor)
    {
    case LEDColor::red:
        if (autoChange)
        {
            m_LedColor = LEDColor::green;
        }
        m_CurrentColorStr = "Red";
        m_CurrentColor[0] = 100;
        m_CurrentColor[1] = 0;
        m_CurrentColor[2] = 0;
        break;
    case LEDColor::green:
        if (autoChange)
        {
            m_LedColor = LEDColor::blue;
        }
        m_CurrentColorStr = "Green";
        m_CurrentColor[0] = 0;
        m_CurrentColor[1] = 100;
        m_CurrentColor[2] = 0;
        break;
    case LEDColor::blue:
        if (autoChange)
        {
            m_LedColor = LEDColor::white;
        }
        m_CurrentColorStr = "Blue";
        m_CurrentColor[0] = 0;
        m_CurrentColor[1] = 0;
        m_CurrentColor[2] = 100;
        break;
    case LEDColor::white:
        if (autoChange)
        {
            m_LedColor = LEDColor::red;
        }
        m_CurrentColorStr = "White";
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
    Serial.println("color");
    double currentFactor = m_Factor;
    int delayTime = 20;
    for (double f = currentFactor; f > 0.1; f = f - 0.02)
    {
        m_Factor = f;
        updateLEDs(true);
        delay(delayTime);
    }
    for (double f = 0; f < currentFactor; f = f + 0.02)
    {
        m_Factor = f;
        updateLEDs(true);
        delay(delayTime);
    }

    m_Factor = currentFactor;
}

void LedStrip::showError()
{
    m_LedColor = LEDColor::red;
    updateLEDs();
    delay(5000);
    m_LedColor = LEDColor::white;
    updateLEDs();
}

void LedStrip::runModeAction()
{
    switch (m_LEDMode)
    {
    case LEDModes::colorful:
        colorfulMode();
        break;
    case LEDModes::campfire:
        campfireMode();
        break;
    default:
        break;
    }
}
void LedStrip::colorfulMode()
{
    const double maxBrightness = 4.0;
    std::array<uint8_t, NUMPIXELS> pRed{};
    std::array<uint8_t, NUMPIXELS> pGreen{};
    std::array<uint8_t, NUMPIXELS> pBlue{};
    for (int i = 0; i < 10; i++)
    {
        pRed[(m_ColorfulMode.LedActioncounter + i) % 30] = static_cast<double>(i) * maxBrightness;
        pGreen[(m_ColorfulMode.LedActioncounter + i + 10) % 30] = static_cast<double>(i) * maxBrightness;
        pBlue[(m_ColorfulMode.LedActioncounter + i + 20) % 30] = static_cast<double>(i) * maxBrightness;
    }

    showPixels(pRed, pGreen, pBlue);
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
    std::array<uint8_t, NUMPIXELS> pRed{};
    std::array<uint8_t, NUMPIXELS> pGreen{};
    std::array<uint8_t, NUMPIXELS> pBlue{};
    std::array<uint8_t, NUMPIXELS> colorTemplate{46, 47, 49, 51, 53, 55, 55, 56, 57, 58, 57, 56, 55, 54, 52, 51, 50, 49, 49, 50, 51, 53, 54, 55, 55, 54, 52, 50, 48, 47};

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

    if (m_FlameMode.LastIndexGreen >= 29)
    {
        m_FlameMode.LastIndexGreen = 29;
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

    if (m_FlameMode.LastIndexRed >= 29)
    {
        m_FlameMode.LastIndexRed = 29;
    }
    else if (m_FlameMode.LastIndexRed <= 0)
    {
        m_FlameMode.LastIndexRed = 0;
    }

    double brightness = m_FlameMode.Brightness * m_Factor;
    for (int i = 0; i < 30; i++)
    {
        pRed[i] = static_cast<double>(colorTemplate[m_FlameMode.LastIndexRed]) * brightness;
        pGreen[i] = static_cast<double>(colorTemplate[m_FlameMode.LastIndexGreen]) * brightness / 2.3;
        pBlue[i] = 0.1 * brightness;
    }

    m_FlameMode.LedActioncounter++;
    m_NextLEDActionTime = millis() + 10;
    showPixels(pRed, pGreen, pBlue);
}

void LedStrip::setColor(double red, double green, double blue)
{
    Serial.print("red: ");
    Serial.println(red);
    Serial.print("green: ");
    Serial.println(green);
    Serial.print("blue: ");
    Serial.println(blue);
    m_CurrentColor[0] = red;
    m_CurrentColor[1] = green;
    m_CurrentColor[2] = blue;
    for (int i = 0; i < NUMPIXELS; i++)
    {
        m_Pixels.setPixelColor(i, m_Pixels.Color(m_CurrentColor[0], m_CurrentColor[1], m_CurrentColor[2]));
    }
    m_Pixels.show();
}

std::array<uint8_t, 3> LedStrip::getColor()
{
    return m_CurrentColor;
}

void LedStrip::showPixels(std::array<uint8_t, NUMPIXELS> pRed, std::array<uint8_t, NUMPIXELS> pGreen, std::array<uint8_t, NUMPIXELS> pBlue)
{
    for (int i = 0; i < NUMPIXELS; i++)
    {
        m_Pixels.setPixelColor(i, m_Pixels.Color(pRed[i], pGreen[i], pBlue[i]));
    }
    m_Pixels.show();
}



#include <Arduino.h>
#include <array>
#include <vector>
#include <Adafruit_NeoPixel.h>

#ifndef LED_STRIP_H
#define LED_STRIP_H

class LedStrip
{
public:
    struct PixelColors
    {
        std::vector<uint8_t> pRed;
        std::vector<uint8_t> pGreen;
        std::vector<uint8_t> pBlue;
        PixelColors(int nrOfPixels)
        {
            for (int i = 0; i < nrOfPixels; i++)
            {
                pRed.emplace_back(0);
                pGreen.emplace_back(0);
                pBlue.emplace_back(0);
            }
        }
    };

    enum class LEDColor
    {
        white = 0,
        red = 1,
        green = 2,
        blue = 3,
    };

    enum class LEDModes
    {
        on = 0,
        off = 1,
        campfire = 2,
        colorful = 3,
        sunrise = 4,
        pulse = 5,
    };

private:
    struct FlameMode
    {
        unsigned long LedActioncounter;
        double Brightness;
        int LastIndexRed;
        int LastIndexGreen;
        FlameMode() : LedActioncounter(0),
                      Brightness(0.8),
                      LastIndexRed(15),
                      LastIndexGreen(15)
        {
        }
    };

    struct ColorfulMode
    {
        unsigned long LedActioncounter;
        int LastIndexRed;
        int LastIndexGreen;
        ColorfulMode() : LedActioncounter(0),
                         LastIndexRed(15),
                         LastIndexGreen(15)
        {
        }
    };

    struct PulseMode
    {
        unsigned long NextUpdateTime;
        unsigned long UpdateInterval;
        double LowerLimit;
        double UpperLimit;
        double StepSize;
        bool IsIncreasing;
        PulseMode() : NextUpdateTime(0),
                      UpdateInterval(10),
                      LowerLimit(0.4),
                      UpperLimit(0.9),
                      StepSize(0.0015),
                      IsIncreasing(false)
        {
        }
    };

public:
    LedStrip(uint8_t pin, int nrOfPixels);
    void beginPixels(bool doFancyStartup);
    void applyModeAndColor();
    void applyColorSmoothly();
    void fancy();
    void showError();
    int runModeAction();

    void applyColorImmediate();
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    std::array<uint8_t, 3> getColor();


    // Sensor Coloring
    void setCO2Color(double co2Val);
    void setTemperatureColor(double temperature);

private:
    void updateLEDs(bool doImmediate, bool ignoreOnNothingChanged = false);
    void colorfulMode();
    void campfireMode();
    void pulseMode();
    void sunriseMode();
    void showPixels();

public:
    Adafruit_NeoPixel m_Pixels;
    LEDColor m_LedColor;
    LEDModes m_LEDMode;
    double m_Factor;
    unsigned long m_SunriseStartTime;
    double m_SunriseDuration;
    PulseMode m_PulseMode;

private:
    unsigned long m_NextLEDActionTime;
    FlameMode m_FlameMode;
    ColorfulMode m_ColorfulMode;
    std::array<uint8_t, 3> m_CurrentColor;
    std::array<uint8_t, 3> m_OldCurrentColor;
    int m_NrOfPixels;
    bool m_UseAllLEDs;
    PixelColors m_PixelColors;


};

#endif
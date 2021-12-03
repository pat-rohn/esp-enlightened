#include <Arduino.h>
#include <array>
#include <Adafruit_NeoPixel.h>

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
        autochange = 4,
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
    void beginPixels();
    void apply();
    void changeColor(bool autoChange = true);
    void fancy();
    void showError();
    void runModeAction();

    void setColor(double red, double green, double blue);
    std::array<uint8_t, 3> getColor();

private:
    void updateLEDs(bool doImmediate = false);
    void colorfulMode();
    void campfireMode();
    void pulseMode();
    void showPixels();

public:
    Adafruit_NeoPixel m_Pixels;
    LEDColor m_LedColor = LEDColor::white;
    LEDModes m_LEDMode = LEDModes::on;
    double m_Factor;

private:
    unsigned long m_NextLEDActionTime;
    FlameMode m_FlameMode;
    ColorfulMode m_ColorfulMode;
    PulseMode m_PulseMode;
    std::array<uint8_t, 3> m_CurrentColor;
    int m_NrOfPixels;
    bool m_UseAllLEDs;
    PixelColors m_PixelColors;
};

#include <Arduino.h>
#include <array>
#include <Adafruit_NeoPixel.h>

#define NUMPIXELS 30

class LedStrip
{
public:
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

public:
    LedStrip(uint8_t pin);
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
    void showPixels(std::array<uint8_t, NUMPIXELS> pRed, std::array<uint8_t, NUMPIXELS> pGreen, std::array<uint8_t, NUMPIXELS> pBlue);

public:
    Adafruit_NeoPixel m_Pixels;
    LEDColor m_LedColor = LEDColor::white;
    LEDModes m_LEDMode = LEDModes::on;
    double m_Factor;
    String m_CurrentColorStr;

private:
    unsigned long m_NextLEDActionTime;
    FlameMode m_FlameMode;
    ColorfulMode m_ColorfulMode;
    std::array<uint8_t, 3> m_CurrentColor;
};

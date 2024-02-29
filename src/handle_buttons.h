#ifndef HANDLE_BUTTONS_H
#define HANDLE_BUTTONS_H

#include <Arduino.h>
#include "events.h"
#include "led/leds_service.h"
#include "led/button_inputs.h"
#include "led/sunrise_alarm.h"
#include "mqtt_events.h"
#include "config.h"


enum class light_level
{
    off = 0,
    low = 1,
    medium = 2,
    high = 3
};

light_level level = light_level::off;

void handleButton1(sunrise::CSunriseAlarm *sunrise, LedStrip *leds)
{
    Serial.println("Button 1 pressed");
    if (sunrise != nullptr && sunrise->run())
    {
        Serial.println("interrupt sunrise");
        sunrise->interruptAlarm();
        return;
    }

    LedStrip::LEDModes mode = leds->m_LEDMode;
    if (mode == LedStrip::LEDModes::off || level == light_level::off)
    {
        Serial.println("Turn to low");
        leds->m_LEDMode = LedStrip::LEDModes::on;
        level = light_level::low;
        leds->m_Factor = 1.0;
            leds->setColor(configman::getConfig().LightLow.Red, configman::getConfig().LightLow.Green, configman::getConfig().LightLow.Blue); 
        leds->applyModeAndColor();
    }
    else
    {
        if (level == light_level::low)
        {
            Serial.println("turn to medium");
            leds->m_LEDMode = LedStrip::LEDModes::on;
            level = light_level::medium;
            leds->m_Factor = 1.0;
            leds->setColor(configman::getConfig().LightMedium.Red, configman::getConfig().LightMedium.Green, configman::getConfig().LightMedium.Blue); 
            leds->applyModeAndColor();
        }
        else if (level == light_level::medium)
        {
            Serial.println("turn to high");
            leds->m_LEDMode = LedStrip::LEDModes::on;
            level = light_level::high;
            leds->m_Factor = 1.0;
            leds->setColor(configman::getConfig().LightHigh.Red, configman::getConfig().LightHigh.Green, configman::getConfig().LightHigh.Blue);
            leds->applyModeAndColor();
        }
        else
        {
           Serial.println("turn off");
            leds->m_LEDMode = LedStrip::LEDModes::off;
            level = light_level::off;
            leds->m_Factor = 1.0;
            leds->setColor(configman::getConfig().LightHigh.Red, configman::getConfig().LightHigh.Green, configman::getConfig().LightHigh.Blue); 
            leds->applyModeAndColor();
        }
    }
    mqtt_events::sendStateTopic(leds->getColor(), leds->m_LEDMode == LedStrip::LEDModes::on, leds->m_Factor);
}

void handleButton2()
{
  Serial.println("Button 2 pressed");
  CallEvent(configman::getConfig().Button2GetURL);
}

void handleButtons(sunrise::CSunriseAlarm *sunrise, LedStrip *leds)
{
  if (button_inputs::button1.pressed)
  {
    handleButton1(sunrise, leds);
    button_inputs::button1.pressed = false;
  }
  if (button_inputs::button2.pressed)
  {
    handleButton2();
    button_inputs::button2.pressed = false;
  }
}
#endif // HANDLE_BUTTONS_H

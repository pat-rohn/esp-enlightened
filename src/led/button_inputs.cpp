#include "button_inputs.h"
#include "domain/deadline.h"

namespace button_inputs
{
    Button button1 = {-1, false, 0, false, true};
    Button button2 = {-1, false, 0, false, true};

    // Polled instead of edge-interrupt driven: a press is only accepted when the
    // pin reads LOW on every sample for stableTime. Any HIGH sample restarts the
    // timer, so short EMI spikes are ignored.
    static void poll(Button &b, unsigned long now)
    {
        if (b.pin <= 0)
        {
            return;
        }
        if (digitalRead(b.pin) != LOW)
        {
            b.lowActive = false;
            b.armed = true;
            return;
        }
        if (!b.lowActive)
        {
            b.lowActive = true;
            b.lowSince = now;
            return;
        }
        if (b.armed && !b.pressed && now - b.lowSince >= stableTime)
        {
            b.pressed = true;
            b.armed = false; // must be released before the next press
        }
    }

    void update()
    {
        unsigned long now = millis();
        poll(button1, now);
        poll(button2, now);
    }

    void start()
    {
        Serial.printf("Button 1: %d  -  Button 2: %d \n", button1.pin, button2.pin);
        if (button1.pin > 0)
        {
            pinMode(button1.pin, INPUT);
        }
        if (button2.pin > 0)
        {
            pinMode(button2.pin, INPUT);
        }
    }
}

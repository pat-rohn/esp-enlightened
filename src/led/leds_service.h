#ifndef LED_SERVICE_H
#define LED_SERVICE_H


#include <Arduino.h>

#include "ledstrip.h"

class CLEDService
{
public:
    CLEDService(LedStrip *ledStrip);
    ~CLEDService(){};

public:
    bool apply(const String &ledString, String &response);
    String get(String msg = "Success");
    LedStrip *m_LedStrip;
    
};

#endif
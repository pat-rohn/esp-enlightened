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
    String apply(String ledString);
    String get(String msg = "Success");
    LedStrip *m_LedStrip;
    
};

#endif
#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

#include "ledstrip.h"

class CLEDService
{
public:
    CLEDService(LedStrip* ledStrip);
    ~CLEDService(){};

public:
    WiFiServer m_Server;
    String header;

    LedStrip* m_LedStrip;
    void beginServer();
    void listen();
    String getHomepage();
    String getHTTPOK();
    String getHTTPNotOK();
    void showError();

private:
    unsigned long m_CurrentTime;
    unsigned long m_PreviousTime;
    unsigned long m_TimeoutTime;
};

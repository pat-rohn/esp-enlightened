

#ifndef EVENTS_H
#define EVENTS_H

#ifdef ESP8266
#include <ESP8266HTTPClient.h>

#endif /* ESP8266 */

#ifdef ESP32
#include <HTTPClient.h>

#endif /* ESP32 */

void CallEvent()
{
    WiFiClient client = WiFiClient();
    HTTPClient http;
    //TODO: Add to config
    String serverPath = "http://192.168.1.125/relay/0?turn=toggle";
    Serial.println(serverPath);

    http.begin(client, serverPath.c_str());
    Serial.println("Post data");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        http.end();
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
    }
}

#endif


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
    // TODO: Add to config
    String msg = R"rawliteral(
        {"id":"switch.set_16639312304270.5810851193709093",
        "src":"localweb326",
        "method":"switch.set",
        "params":{"id":0,"on":false}}
        )rawliteral";

    WiFiClient client = WiFiClient();
    HTTPClient http;
    String serverPath = "http://192.168.1.120/rpc/Switch.Toggle?id=0";
    Serial.println(serverPath);

    http.begin(client, serverPath.c_str());
    Serial.println("Post data");
    int httpResponseCode = http.POST(msg);

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
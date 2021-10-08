#include "leds_service.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>
#include "ledstrip.h"

WiFiServer server(80);
String header;

LedStrip ledStrip = LedStrip(D4);

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

void beginPixels()
{
  ledStrip.beginPixels();
  ledStrip.updateLEDs();
}

void beginServer()
{
  server.begin();
}

void listen()
{
  ledStrip.runModeAction();

  WiFiClient client = server.available(); 
  if (client)
  { // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    String body = "";
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime)
    { // loop while the client's connected
      currentTime = millis();
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            if (header.indexOf("POST /api/play") >= 0)
            {
              ledStrip.m_PlaygroundMode = LedStrip::PlayGroundMode::none;
              Serial.println("Play");
              while (client.available())
              {
                char c = client.read();
                body += c;
              }
              Serial.println(body);
              const char *json_string = body.c_str();
              DynamicJsonDocument doc(2000);
              DeserializationError err = deserializeJson(doc, json_string);
              if (err.code() != DeserializationError::Code::Ok)
              {
                Serial.println("Failed to parse.");
              }
              else
              {
                int mode = doc["play"];
                if (mode == 1)
                {
                  Serial.println("mode 1");
                  ledStrip.setMode(1);
                }
                else if (mode == 2)
                {
                  Serial.println("mode 2");
                  ledStrip.setMode(2);
                }
              }
              doc.clear();
              client.println(getHTTPOK().c_str());
            }
            else if (header.indexOf("POST /api/changecolor") >= 0)
            {
              Serial.println("POST changecolor");
              ledStrip.m_PlaygroundMode = LedStrip::PlayGroundMode::none;
              while (client.available())
              {
                char c = client.read();
                body += c;
              }
              Serial.println(body);

              const char *json_string = body.c_str();
              DynamicJsonDocument doc(2000);
              DeserializationError err = deserializeJson(doc, json_string);
              if (err.code() != DeserializationError::Code::Ok)
              {
                Serial.println("Failed to parse.");
              }
              else
              {
                Serial.println("Valid json");
                const char* colorC = doc["color"];
                std::string color = std::string(colorC).c_str();
                if (color.find("White") != std::string::npos)
                {
                  ledStrip.m_LedColor = LedStrip::LEDColor::white;
                }
                else if (color.find("Red") != std::string::npos)
                {
                  ledStrip.m_LedColor = LedStrip::LEDColor::red;
                }
                else if (color.find("Green") != std::string::npos)
                {
                  ledStrip.m_LedColor = LedStrip::LEDColor::green;
                }
                else if (color.find("Blue") != std::string::npos)
                {
                  ledStrip.m_LedColor = LedStrip::LEDColor::blue;
                }
                else
                {
                  Serial.println("Color not found.");
                  ledStrip.m_LedColor = LedStrip::LEDColor::white;
                }
                ledStrip.changeColor(false);
              }

              doc.clear();
              client.println(getHTTPOK().c_str());
              body = "";
              break;
            }
            else if (header.indexOf("POST /api/factors") >= 0)
            {
              Serial.println("POST changecolor");
              ledStrip.m_PlaygroundMode = LedStrip::PlayGroundMode::none;
              while (client.available())
              {
                char c = client.read();
                body += c;
              }
              Serial.println(body);

              const char *json_string = body.c_str();
              DynamicJsonDocument doc(2000);
              DeserializationError err = deserializeJson(doc, json_string);
              if (err.code() != DeserializationError::Code::Ok)
              {
                Serial.println("Failed to parse.");
              }
              else
              {
                double factorRed = 0.0;
                double factorGreen = 0.0;
                double factorBlue = 0.0;
                factorRed = doc["Red"];
                factorGreen = doc["Green"];
                factorBlue = doc["Blue"];
                ledStrip.setColor(factorRed, factorGreen, factorBlue);
                client.println(getHTTPOK().c_str());
                body = "";
                break;
              }
              body = "";

              doc.clear();
              client.println(getHTTPOK().c_str());
            }
            else if (header.indexOf("POST /api/factor") >= 0)
            {
              ledStrip.m_PlaygroundMode = LedStrip::PlayGroundMode::none;
              Serial.println("POST factor");
              while (client.available())
              {
                char c = client.read();
                body += c;
              }
              Serial.println(body);
              const char *json_string = body.c_str();
              DynamicJsonDocument doc(2000);
              DeserializationError err = deserializeJson(doc, json_string);
              if (err.code() != DeserializationError::Code::Ok)
              {
                Serial.println("Failed to parse.");
              }
              else
              {
                ledStrip.m_Factor = doc["factor"];
                ledStrip.updateLEDs(true);
              }
              client.println(getHTTPOK().c_str());
              body = "";
              doc.clear();
              break;
            }
            else if (header.indexOf("GET /api/get") >= 0)
            {
              client.println(getHTTPOK().c_str());
              std::stringstream str;
              std::array<uint8_t, 3> color = ledStrip.getColor();
              str << R"({"Red":)" << int(color[0])
                  << R"( ,"Green": )" << int(color[1])
                  << R"( ,"Blue": )" << int(color[2])
                  << R"( ,"Factor": )" << int(ledStrip.m_Factor * 100)
                  << R"( ,"Color": ")" << ledStrip.m_CurrentColorStr.c_str()
                  << R"(" })" << std::endl;
              client.println(str.str().c_str());
              Serial.println(str.str().c_str());
              break;
            }
            else if (header.indexOf("GET /api/off") >= 0)
            {
              ledStrip.m_PlaygroundMode = LedStrip::PlayGroundMode::none;

              client.println(getHTTPOK().c_str());
              ledStrip.m_LedColor = LedStrip::LEDColor::off;
              ledStrip.m_CurrentColorStr = "Off";
              ledStrip.updateLEDs(true);

              break;
            }
            else
            {
              String homepage = getHomepage();
              client.println(homepage);
              break;
            }
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

String getHTTPOK()
{
  std::stringstream str;
  str << "HTTP/1.1 200 OK" << std::endl;
  str << "Content-type:text/json" << std::endl;
  str << "Connection: close" << std::endl;
  str << std::endl;
  return String(str.str().c_str());
}

String getHomepage()
{

  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  std::stringstream str;
  str << "HTTP/1.1 200 OK" << std::endl;
  str << "Content-type:text/html" << std::endl;
  str << "Connection: close" << std::endl;
  str << std::endl;

  // Display the HTML web page
  str << "<!DOCTYPE html><html>" << std::endl;
  str << "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" << std::endl;
  str << "<link rel=\"icon\" href=\"data:,\">" << std::endl;
  // CSS to style the on/off buttons
  // Feel free to change the background-color and font-size attributes to fit your preferences
  str << "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}" << std::endl;
  str << ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;" << std::endl;
  str << "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}" << std::endl;
  str << ".button2 {background-color: #555555;}</style></head>" << std::endl;

  // Web Page Heading
  str << "<body><h1>Caromio</h1>" << std::endl;
  if (header.indexOf("GET /colorchange") >= 0)
  {
    ledStrip.changeColor();
  }
  else if (header.indexOf("GET /low") >= 0)
  {
    Serial.println("low");
    ledStrip.m_Factor -= 0.05;
    ledStrip.updateLEDs();
  }

  else if (header.indexOf("GET /bright") >= 0)
  {
    Serial.println("bright");
    ledStrip.m_Factor += 0.05;
    ledStrip.updateLEDs();
  }
  else if (header.indexOf("GET /off") >= 0)
  {
    Serial.println("off");
    ledStrip.m_LedColor = LedStrip::LEDColor::off;
    ledStrip.updateLEDs();
  }

  str << "<p><a href=\"/low\"><button class=\"button\">Low</button></a></p>" << std::endl;
  str << "<p><a href=\"/bright\"><button class=\"button\">Bright</button></a></p>" << std::endl;
  str << "<p><a href=\"/off\"><button class=\"button\">OFF</button></a></p>" << std::endl;

  str << "<p><a href=\"/colorchange\"><button class=\"button\">Change Color</button></a></p>" << std::endl;

  str << "</body></html>" << std::endl;

  // The HTTP response ends with another blank line
  str << std::endl;
  return String(str.str().c_str());
}

void fancy()
{
  ledStrip.fancy();
}

void showError()
{
  ledStrip.showError();
}

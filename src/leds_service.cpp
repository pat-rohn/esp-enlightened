#include "leds_service.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>

CLEDService::CLEDService(LedStrip *ledStrip) : m_Server(80),
                                               m_LedStrip(ledStrip)
{
  m_CurrentTime = millis();
  m_PreviousTime = 0;
  m_TimeoutTime = 500;
}

void CLEDService::beginServer()
{
  Serial.println("LED Service: Begin Server.");
  m_Server.begin();
}

void CLEDService::listen()
{
  m_LedStrip->runModeAction();

  WiFiClient client = m_Server.available();
  if (client)
  {
    m_CurrentTime = millis();
    m_PreviousTime = m_CurrentTime;
    String body = "";
    Serial.println("New Client.");
    String currentLine = ""; // make a String to hold incoming data from the client
    while (client.connected() && m_CurrentTime - m_PreviousTime <= m_TimeoutTime)
    { // loop while the client's connected
      m_CurrentTime = millis();
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {

            if (header.indexOf("POST /api/apply") >= 0)
            {
              Serial.println("POST Apply");
              unsigned long endTime = millis() + 100;
              while (millis() < endTime)
              {
                if (client.available())
                {
                  char c = client.read();
                  body += c;
                }
              }
              DynamicJsonDocument doc(4096);
              DeserializationError err = deserializeJson(doc, body);
              if (err.code() != DeserializationError::Code::Ok)
              {
                Serial.print("Failed to parse: ");
                Serial.println(err.code());
                Serial.print("Input was: ");
                Serial.println(body);
                doc.clear();
                client.println(getHTTPOK().c_str());
                std::stringstream str;
                std::array<uint8_t, 3> color = m_LedStrip->getColor();
                str << R"({"Red":)" << int(color[0])
                    << R"( ,"Green": )" << int(color[1])
                    << R"( ,"Blue": )" << int(color[2])
                    << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100.0)
                    << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
                    << R"( ,"Message": "Failed to parse: )" << err.code() << R"( ")"
                    << R"( })" << std::endl;
                client.println(str.str().c_str());
                body = "";
              }
              else
              {
                Serial.print("Input ");
                Serial.println(body);
                int mode = doc["Mode"];
                m_LedStrip->m_LEDMode = LedStrip::LEDModes(mode);
                double factor = doc["Brightness"];
                m_LedStrip->m_Factor = factor / 100.0;
                double factorRed = doc["Red"];
                double factorGreen = doc["Green"];
                double factorBlue = doc["Blue"];
                String message = doc["Message"];
                m_LedStrip->setColor(factorRed, factorGreen, factorBlue);
                m_LedStrip->apply();
                client.println(getHTTPOK().c_str());
                std::stringstream str;
                std::array<uint8_t, 3> color = m_LedStrip->getColor();
                str << R"({"Red":)" << int(color[0])
                    << R"( ,"Green": )" << int(color[1])
                    << R"( ,"Blue": )" << int(color[2])
                    << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100.0)
                    << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
                    << R"( ,"Message": "Success - )" << message.c_str() << R"( ")"
                    << R"( })" << std::endl;
                client.println(str.str().c_str());
                Serial.println(str.str().c_str());
                body = "";
                doc.clear();
              }
              break;
            }
            else if (header.indexOf("GET /api/get") >= 0)
            {
              Serial.println("GET api");
              client.println(getHTTPOK().c_str());
              std::stringstream str;
              std::array<uint8_t, 3> color = m_LedStrip->getColor();
              str << R"({"Red":)" << int(color[0])
                  << R"( ,"Green": )" << int(color[1])
                  << R"( ,"Blue": )" << int(color[2])
                  << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100)
                  << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
                  << R"( ,"Message": "Success")"
                  << R"( })" << std::endl;
              client.println(str.str().c_str());
              Serial.println(str.str().c_str());
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

String CLEDService::getHTTPOK()
{
  std::stringstream str;
  str << "HTTP/1.1 200 OK" << std::endl;
  str << "Content-type:text/json" << std::endl;
  str << "Connection: close" << std::endl;
  str << std::endl;
  return String(str.str().c_str());
}

String CLEDService::getHTTPNotOK()
{
  std::stringstream str;
  str << "HTTP/1.1 400 Bad Request" << std::endl;
  str << "Content-type:text/json" << std::endl;
  str << "Connection: close" << std::endl;
  str << std::endl;
  return String(str.str().c_str());
}

String CLEDService::getHomepage()
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
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::autochange;
    m_LedStrip->apply();
  }
  else if (header.indexOf("GET /low") >= 0)
  {
    Serial.println("low");
    m_LedStrip->m_Factor -= 0.05;
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::on;
    m_LedStrip->apply();
  }

  else if (header.indexOf("GET /bright") >= 0)
  {
    Serial.println("bright");
    m_LedStrip->m_Factor += 0.05;
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::on;
    m_LedStrip->apply();
  }
  else if (header.indexOf("GET /off") >= 0)
  {
    Serial.println("off");
    m_LedStrip->m_LEDMode = LedStrip::LEDModes::off;
    m_LedStrip->apply();
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

void CLEDService::showError()
{
  m_LedStrip->showError();
}

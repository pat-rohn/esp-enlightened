#include "webpage.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>
#include <array>

#include "config.h"

namespace webpage
{

  const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <title>IoT Multi Device Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script>
        function submitMessage() {
            alert("Saved value to Device");
            setTimeout(function () { document.location.reload(false); }, 500);
        }
    </script>
</head>

<body>
    <h2>IoT Multi Device Configuration</h2>
    <form action="/get" target="hidden-form">
        <br>
        <textarea disabled cols="80" rows="20">%config.json%</textarea>
        <br>
        <textarea id="configuration" name="configuration" cols="80" rows="20"></textarea>
        <input type="submit" value="Submit" onclick="submitMessage()">
    </form>
    <br>

    <iframe style="display:none" name="hidden-form"></iframe>
</body>

</html>
)rawliteral";

  CWebPage::CWebPage() : m_Server(80)
  {
  }

  void CWebPage::beginServer()
  {
    Serial.println("Webpage: Begin Server.");
    // Send web page with input fields to client
    m_Server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send_P(200, "text/html", index_html, processor); });

    // Send a GET request to <ESP_IP>/get?configuration=<inputMessage>
    m_Server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    String inputMessage;
      // GET inputString value on <ESP_IP>/get?configuration=<inputMessage>
    if (request->hasParam("configuration")) {
      inputMessage = request->getParam("configuration")->value();
      configman::writeConfig(inputMessage.c_str());
    } else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" + inputMessage + ") with value: " + inputMessage + "<br><a href=\"/\">Return to Home Page</a>"); });
    m_Server.onNotFound(notFound);
    m_Server.begin();
  }

  String CWebPage::getHTTPOK()
  {
    std::stringstream str;
    str << "HTTP/1.1 200 OK" << std::endl;
    str << "Content-type:text/json" << std::endl;
    str << "Access-Control-Allow-Origin: * " << std::endl;
    str << "Access-Control-Allow-Headers: Content-Type, Authorization, Accept, Accept-Language, X-Authorization " << std::endl;
    str << "Connection: close" << std::endl;
    str << std::endl;
    return String(str.str().c_str());
  }

  String CWebPage::getHTTPNotOK()
  {
    std::stringstream str;
    str << "HTTP/1.1 400 Bad Request" << std::endl;
    str << "Content-type:text/json" << std::endl;
    str << "Connection: close" << std::endl;
    str << std::endl;
    return String(str.str().c_str());
  }

  String CWebPage::getFrontPage()
  {
    std::stringstream str;
    str << "HTTP/1.1 200 OK" << std::endl;
    str << "Content-type:text/html" << std::endl;
    str << "Access-Control-Allow-Origin: * " << std::endl;
    str << "Connection: close" << std::endl;
    str << std::endl;

    if (m_Header.indexOf("GET /") >= 0)
    {
      str << index_html << std::endl;
    }
    else if (m_Header.indexOf("GET /off") >= 0)
    {
      Serial.println("off");
    }

    // The HTTP response ends with another blank line
    str << std::endl;
    return String(str.str().c_str());
  }


}
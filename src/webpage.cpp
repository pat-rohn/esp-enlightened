#include "webpage.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>
#include <array>
#include "config.h"
#include "timehelper.h"
#include <atomic>
#include <memory>

namespace webpage
{

  CLEDService *m_LedService;
  CTimeHelper *m_TimeHelper;
  std::atomic<bool> *m_RestartTriggered;
  std::atomic<bool> *m_ButtonPressed1;
  std::atomic<bool> *m_ButtonPressed2;

  const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <title>IoT Multi Device Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script>
        function submitConfig() {
          console.log("submit config")
          alert("Save config to Device");
          setTimeout(function () { document.location.reload(false); }, 500);
        }
        
    </script>
</head>

<body>
    <h2>IoT Multi Device Configuration</h2>

  </body></html>
    <form action="/get" target="hidden-form">
        <br>
        <textarea disabled cols="80" rows="22">%devconfig%</textarea>
        <br>
        <textarea id="configuration" name="configuration" cols="80" rows="22"></textarea> 
        <br>
        <input type="submit" value="Submit" onclick="submitConfig()">
    </form>
     <a href="/restart" class="button">Restart </a>

    <br>

    <iframe style="display:none" name="hidden-form"></iframe>
</body>

</html>
)rawliteral";

  CWebPage::CWebPage() : m_Server(80)
  {
  }

  void CWebPage::setLEDService(CLEDService *ledService)
  {
    m_LedService = ledService;
  }

  void CWebPage::setTimeHelper(CTimeHelper *timeHelper)
  {
    m_TimeHelper = timeHelper;
    Serial.println("Set Time Helper");
  }

  void CWebPage::setTriggerFlag(std::atomic<bool> *restartTriggered)
  {
    m_RestartTriggered = restartTriggered;
  }

  void CWebPage::setButtonsPressed(std::atomic<bool> *buttonPressed1, std::atomic<bool> *buttonPressed2)
  {
    m_ButtonPressed1 = buttonPressed1;
    m_ButtonPressed2 = buttonPressed2;
  }

  void CWebPage::beginServer()
  {
    Serial.println("Webpage: Begin Server.");
    m_Server.on("/api/led", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "");
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); });

    m_Server.on("/api/led", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = m_LedService->get();
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", answer);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); });
    m_Server.on("/api/button1", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "");
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); });
    m_Server.on("/api/button1", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = "{\"msg\": \"button 1 pressed\"}";
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", answer);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); 
                  Serial.println(answer);
                  m_ButtonPressed1->store(true);
                });
    m_Server.on("/api/button2", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "");
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); });
    m_Server.on("/api/button2", HTTP_GET, [](AsyncWebServerRequest *request)
                               {
                  String answer = "{\"msg\": \"button 2 pressed\"}";
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", answer);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); 
                  Serial.println(answer);
                  m_ButtonPressed2->store(true);
                });
    
    m_Server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  int params = request->params();
                  Serial.printf("%d params sent in\n", params);
                  String input = "";
                  for (int i = 0; i < params; i++)
                  {
                    AsyncWebParameter *p = request->getParam(i);
                    if (p->isPost())
                    {
                      Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
                      input = p->value();
                    }
                  }
                  Serial.printf("Input is: %s\n", input.c_str());
                  String answer = m_LedService->apply(input);
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", answer);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");

                  request->send(response); });
    
    m_Server.on("/api/config", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", "");
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); 
                  });

    // Config Post
    m_Server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
                { 
                  int params = request->params();
                  Serial.printf("%d params sent in\n", params);
                  String input = "";
                  for (int i = 0; i < params; i++)
                  {
                    AsyncWebParameter *p = request->getParam(i);
                      if (p->isPost())
                    {
                        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
                        input = p->value();
                    }
                  }
                  Serial.printf("Input is: %s\n", input.c_str());
                  configman::writeConfig(input.c_str());
                  
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", input);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response);
                  Serial.println("restart triggered");
                  m_RestartTriggered->store(true);

                  });
    /// Config PUT (no restart)
    m_Server.on("/api/config", HTTP_PUT, [](AsyncWebServerRequest *request)
                { 
                  int params = request->params();
                  Serial.printf("%d params sent in\n", params);
                  String input = "";
                  for (int i = 0; i < params; i++)
                  {
                    AsyncWebParameter *p = request->getParam(i);
                      if (p->isPost())
                    {
                        Serial.printf("_PUT[%s]: %s\n", p->name().c_str(), p->value().c_str());
                        input = p->value();
                    }
                  }
                  Serial.printf("Input is: %s\n", input.c_str());
                  configman::writeConfig(input.c_str());
                  
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", input);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response);
                  });
    // Config Get
    m_Server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request)
                { 
                  Serial.println("get /api/config");
                  auto config =  configman::getConfig();
                  String answer = configman::serializeConfig(&config);
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", answer);
                  response->addHeader("Content-type", "text/json");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); });

    // Time Get
    m_Server.on("/api/time", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
                  response->addHeader("Content-type", "text/plain");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); });
    m_Server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request)
                { 
                auto hoursAndMinutes = m_TimeHelper->getHoursAndMinutes();
                int weekday = m_TimeHelper->getWeekDay();
                String answer = String(hoursAndMinutes.first) + ":" + String(hoursAndMinutes.second) 
                  + " (weekday " + String(weekday) + ")";
                Serial.println("get time " + answer);
                AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", answer);
                response->addHeader("Content-type", "text/plain");
                response->addHeader("Access-Control-Allow-Origin", "*");
                response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
                response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                request->send(response); });

    m_Server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { 
                  Serial.println("get web page");
                  request->send_P(200, "text/html", index_html, processor);
                });

    // Restart
    m_Server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\" /></head><body><h1>Redirecting in 10 seconds...</h1></body></html>";
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", answer);
                  response->addHeader("Content-type", "text/html");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response); request->send(200, "text/html", answer);
                  Serial.println("restart triggered");                 
                  m_RestartTriggered->store(true);
                });
    // Send a GET request to <ESP_IP>/get?configuration=<inputMessage>
    m_Server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
                {

        Serial.println("get config");
        String inputMessage;
          // GET inputString value on <ESP_IP>/get?configuration=<inputMessage>
        if (request->hasParam("configuration")) {
          inputMessage = request->getParam("configuration")->value();
          configman::writeConfig(inputMessage.c_str());
          Serial.println("Config written");
        } else{
          inputMessage = "Unknown param";
        }
        Serial.println(inputMessage);
        request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" + inputMessage + ") with value: " + inputMessage + "<br><a href=\"/\">Return to Home Page</a>"); 
        });
    m_Server.onNotFound([](AsyncWebServerRequest * req)
    {
      req->send(404);
    });
    m_Server.begin();
  }

  String CWebPage::processor(const String &var)
  {
    Serial.println(var);
    if (var == "devconfig")
    {
      Serial.println("get configuration");
      auto config = configman::getConfig();
      return configman::serializeConfig(&config);
    }
    else
    {
      Serial.println("there is nothing yet with the call " + var);
      Serial.println(configman::kPathToConfig);
      return "there is nothing yet with the call " + var;
    }
    return String();
  }
}

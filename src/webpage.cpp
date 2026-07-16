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

  namespace
  {
    void addCorsHeaders(AsyncWebServerResponse *response)
    {
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
    }

    void sendJson(AsyncWebServerRequest *request, int code, const String &body)
    {
      AsyncWebServerResponse *response = request->beginResponse(code, "application/json", body);
      addCorsHeaders(response);
      request->send(response);
    }

    // Body handler: accumulates a raw request body chunk by chunk into
    // request->_tempObject, which the library frees when the request is destroyed.
    void collectBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
    {
      if (index == 0)
      {
        request->_tempObject = calloc(total + 1, 1);
        if (request->_tempObject == nullptr)
        {
          Serial.printf("Failed to allocate %u bytes for request body\n", (unsigned)(total + 1));
        }
      }
      if (request->_tempObject != nullptr)
      {
        memcpy(static_cast<uint8_t *>(request->_tempObject) + index, data, len);
      }
    }

    // Raw request body if one was sent, otherwise the value of the last posted
    // form field (the historical input format).
    String getInput(AsyncWebServerRequest *request)
    {
      if (request->_tempObject != nullptr)
      {
        return String(static_cast<const char *>(request->_tempObject));
      }
      String input = "";
      int params = request->params();
      Serial.printf("%d params sent in\n", params);
      for (int i = 0; i < params; i++)
      {
        const AsyncWebParameter *p = request->getParam(i);
        if (p->isPost())
        {
          Serial.printf("_%s[%s]: %s\n", request->methodToString(), p->name().c_str(), p->value().c_str());
          input = p->value();
        }
      }
      return input;
    }
  }

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
                { sendJson(request, 200, ""); });
    m_Server.on("/api/button1", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, ""); });
    m_Server.on("/api/button2", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, ""); });
    m_Server.on("/api/config", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, ""); });
    m_Server.on("/api/time", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
                  addCorsHeaders(response);
                  request->send(response); });
    m_Server.on("/api/led", HTTP_GET, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, m_LedService->get()); });
    m_Server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  String input = getInput(request);
                  Serial.printf("Input is: %s\n", input.c_str());
                  sendJson(request, 200, m_LedService->apply(input)); },
                nullptr, collectBody);
    m_Server.on("/api/led", HTTP_PUT, [](AsyncWebServerRequest *request)
                {
                  Serial.printf("PUT set led\n");
                  String input = getInput(request);
                  if (input.isEmpty())
                  {
                    Serial.printf("No input sent\n");
                    sendJson(request, 400, m_LedService->get("Error: No input received"));
                    return;
                  }
                  Serial.printf("Input is: %s\n", input.c_str());
                  sendJson(request, 200, m_LedService->apply(input)); },
                nullptr, collectBody);

    m_Server.on("/api/button1", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = "{\"msg\": \"button 1 pressed\"}";
                  sendJson(request, 200, answer);
                  Serial.println(answer);
                  m_ButtonPressed1->store(true); });

    m_Server.on("/api/button2", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = "{\"msg\": \"button 2 pressed\"}";
                  sendJson(request, 200, answer);
                  Serial.println(answer);
                  m_ButtonPressed2->store(true); });

    // Config Post (restarts on success)
    m_Server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  String input = getInput(request);
                  Serial.printf("Input is: %s\n", input.c_str());
                  if (!configman::writeConfig(input.c_str()))
                  {
                    sendJson(request, 400, "{\"Message\": \"Error: invalid configuration\"}");
                    return;
                  }
                  const auto &config = configman::getConfig();
                  sendJson(request, 200, configman::serializeConfig(&config));
                  Serial.println("restart triggered");
                  m_RestartTriggered->store(true); },
                nullptr, collectBody);
    /// Config PUT (no restart)
    m_Server.on("/api/config", HTTP_PUT, [](AsyncWebServerRequest *request)
                {
                  String input = getInput(request);
                  Serial.printf("Input is: %s\n", input.c_str());
                  if (!configman::writeConfig(input.c_str()))
                  {
                    sendJson(request, 400, "{\"Message\": \"Error: invalid configuration\"}");
                    return;
                  }
                  const auto &config = configman::getConfig();
                  sendJson(request, 200, configman::serializeConfig(&config)); },
                nullptr, collectBody);
    // Config Get
    m_Server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  Serial.println("get /api/config");
                  const auto& config = configman::getConfig();
                  sendJson(request, 200, configman::serializeConfig(&config)); });

    // Time Get
    m_Server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request)
                { 
                auto hoursAndMinutes = m_TimeHelper->getHoursAndMinutes();
                int weekday = m_TimeHelper->getWeekDay();
                String answer = String(hoursAndMinutes.first) + ":" + String(hoursAndMinutes.second) 
                  + " (weekday " + String(weekday) + ")";
                Serial.println("get time " + answer);
                AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", answer);
                addCorsHeaders(response);
                request->send(response); });

    m_Server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                {
      Serial.println("get web page");
#ifdef ESP8266
                  request->send_P(200, "text/html", index_html, processor); });
#else
                  request->send(200, "text/html", index_html, processor); });
#endif

    // Restart
    m_Server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\" /></head><body><h1>Redirecting in 10 seconds...</h1></body></html>";
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", answer);
                  response->addHeader("Content-type", "text/html");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response);
                  Serial.println("restart triggered");                 
                  m_RestartTriggered->store(true); });
    // Send a GET request to <ESP_IP>/get?configuration=<inputMessage>
    m_Server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
                {

        Serial.println("get config");
        String inputMessage;
          // GET inputString value on <ESP_IP>/get?configuration=<inputMessage>
        if (request->hasParam("configuration")) {
          inputMessage = request->getParam("configuration")->value();
          if (configman::writeConfig(inputMessage.c_str())) {
            Serial.println("Config written");
          } else {
            Serial.println("Config invalid - not written");
          }
        } else{
          inputMessage = "Unknown param";
        }
        Serial.println(inputMessage);
        request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" + inputMessage + ") with value: " + inputMessage + "<br><a href=\"/\">Return to Home Page</a>"); });

    m_Server.onNotFound([](AsyncWebServerRequest *req)
                        { req->send(404); });
    m_Server.begin();
  }

  String CWebPage::processor(const String &var)
  {
    Serial.println(var);
    if (var == "devconfig")
    {
      Serial.println("get configuration");
      const auto& config = configman::getConfig();
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

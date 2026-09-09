#include "webpage.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>
#include <array>
#include "config.h"
#include "timehelper.h"
#include "version.h"
#include "web_assets.generated.h"
#include "domain/request_body.h"
#include <atomic>
#include <memory>

namespace webpage
{

  CLEDService *m_LedService;
  CTimeHelper *m_TimeHelper;
  sunrise::CSunriseAlarm *m_SunriseAlarm;
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

    // Sends {"Message": "..."} with the text properly escaped. Refusal reasons
    // quote the client's own input back, so they must not be concatenated into
    // a hand-written JSON literal.
    void sendJsonMessage(AsyncWebServerRequest *request, int code, const String &message)
    {
      JsonDocument doc;
      doc["Message"] = message;
      String body;
      serializeJson(doc, body);
      sendJson(request, code, body);
    }

    // [D2] Pre-shared token gate for configuration and lifecycle endpoints
    // (/api/config writes, /restart). Light control -- /api/led and the
    // /api/buttonN triggers -- is deliberately NOT gated: controlling the
    // lights must keep working regardless of whether a token is configured.
    // An empty ApiToken
    // (the default) means auth is not configured yet, so existing clients
    // that send no header (e.g. the companion app) keep working; setting a
    // token is how an operator opts into requiring it.
    bool isAuthorized(AsyncWebServerRequest *request)
    {
      const String &token = configman::getConfig().ApiToken;
      if (token.length() == 0)
      {
        return true;
      }
      const AsyncWebHeader *header = request->getHeader("X-Authorization");
      return header != nullptr && header->value() == token;
    }

    void sendUnauthorized(AsyncWebServerRequest *request)
    {
      sendJson(request, 401, "{\"Message\": \"Error: unauthorized\"}");
    }

    // Body handler: accumulates a raw request body chunk by chunk into
    // request->_tempObject, which the library frees when the request is destroyed.
    // Chunk bounds are peer controlled, so every write goes through
    // http_body::writableChunk (see the reasoning there).
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
      if (request->_tempObject == nullptr)
      {
        return;
      }
      const size_t writable = http_body::writableChunk(index, len, total);
      if (writable < len)
      {
        Serial.printf("Request body exceeds Content-Length (%u); dropped %u bytes\n",
                      (unsigned)total, (unsigned)(len - writable));
      }
      if (writable == 0)
      {
        return;
      }
      // calloc zeroed the trailing byte and writableChunk never reaches it, so
      // the accumulated body stays NUL-terminated for getInput()'s String.
      memcpy(static_cast<uint8_t *>(request->_tempObject) + index, data, writable);
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

  CWebPage::CWebPage() : m_Server(80)
  {
  }

  void CWebPage::setLEDService(CLEDService *ledService)
  {
    m_LedService = ledService;
  }

  void CWebPage::setSunriseAlarm(sunrise::CSunriseAlarm *sunriseAlarm)
  {
    m_SunriseAlarm = sunriseAlarm;
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
    registerOptionsRoutes();
    registerConfigRoutes();
    registerLedRoutes();
    registerCommandRoutes();
    registerStatusRoutes();
    m_Server.onNotFound([](AsyncWebServerRequest *req)
                        { req->send(404); });
    m_Server.begin();
  }

  void CWebPage::registerConfigRoutes()
  {
    // Config Post (restarts on success). The config is only staged here;
    // loop() applies and persists it before acting on the restart flag.
    m_Server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  if (!isAuthorized(request))
                  {
                    sendUnauthorized(request);
                    return;
                  }
                  String input = getInput(request);
                  Serial.printf("Input is: %s\n", input.c_str());
                  String staged;
                  String error;
                  if (!configman::stageConfig(input.c_str(), &staged, &error))
                  {
                    sendJsonMessage(request, 400,
                                    error.isEmpty()
                                        ? String("Error: invalid configuration")
                                        : String("Error: ") + error);
                    return;
                  }
                  sendJson(request, 200, staged);
                  Serial.println("restart triggered");
                  m_RestartTriggered->store(true); },
                nullptr, collectBody);
    /// Config PUT (no restart)
    m_Server.on("/api/config", HTTP_PUT, [](AsyncWebServerRequest *request)
                {
                  if (!isAuthorized(request))
                  {
                    sendUnauthorized(request);
                    return;
                  }
                  String input = getInput(request);
                  Serial.printf("Input is: %s\n", input.c_str());
                  String staged;
                  String error;
                  if (!configman::stageConfig(input.c_str(), &staged, &error))
                  {
                    sendJsonMessage(request, 400,
                                    error.isEmpty()
                                        ? String("Error: invalid configuration")
                                        : String("Error: ") + error);
                    return;
                  }
                  sendJson(request, 200, staged); },
                nullptr, collectBody);
    // Config Get
    m_Server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  Serial.println("get /api/config");
                  const auto& config = configman::getConfig();
                  sendJson(request, 200, configman::serializeConfig(&config)); });
  }

  void CWebPage::registerLedRoutes()
  {
    m_Server.on("/api/led", HTTP_GET, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, m_LedService->get()); });
    m_Server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  String input = getInput(request);
                  if (input.isEmpty())
                  {
                    Serial.printf("No input sent\n");
                    sendJson(request, 400, m_LedService->get("Error: No input received"));
                    return;
                  }
                  Serial.printf("Input is: %s\n", input.c_str());
                  String answer;
                  if (!m_LedService->apply(input, answer))
                  {
                    sendJson(request, 400, answer);
                    return;
                  }
                  sendJson(request, 200, answer); },
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
                  String answer;
                  if (!m_LedService->apply(input, answer))
                  {
                    sendJson(request, 400, answer);
                    return;
                  }
                  sendJson(request, 200, answer); },
                nullptr, collectBody);
  }

  void CWebPage::registerCommandRoutes()
  {
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

    // Restart
    // Test the sunrise now, without touching the stored schedule [F11].
    //
    // Ungated, like /api/led and /api/buttonN: this is light control, and the
    // token gate deliberately never blocks that. It changes no configuration.
    m_Server.on("/api/alarm/test", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  if (m_SunriseAlarm == nullptr)
                  {
                    sendJsonMessage(request, 503, "Alarm is not available on this device");
                    return;
                  }
                  if (configman::getConfig().NumberOfLEDs <= 0)
                  {
                    sendJsonMessage(request, 409,
                                    "No LEDs are configured, so there is nothing to show");
                    return;
                  }

                  // A short run by default: long enough to see the ramp, short
                  // enough not to strand anyone waiting for it to finish.
                  double seconds = 30.0;
                  if (request->hasParam("seconds"))
                  {
                    const double requested = request->getParam("seconds")->value().toDouble();
                    if (requested < 1.0 || requested > 600.0)
                    {
                      sendJsonMessage(request, 400, "seconds must be between 1 and 600");
                      return;
                    }
                    seconds = requested;
                  }

                  m_SunriseAlarm->startTest(seconds);
                  JsonDocument doc;
                  doc["Message"] = "Sunrise test started";
                  doc["DurationSeconds"] = seconds;
                  String body;
                  serializeJson(doc, body);
                  sendJson(request, 200, body); });

    m_Server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  if (!isAuthorized(request))
                  {
                    sendUnauthorized(request);
                    return;
                  }
                  String answer = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\" /></head><body><h1>Redirecting in 10 seconds...</h1></body></html>";
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", answer);
                  response->addHeader("Content-type", "text/html");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  response->addHeader("Access-Control-Allow-Methods", "GET");
                  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, Accept-Language, X-Authorization");
                  request->send(response);
                  Serial.println("restart triggered");
                  m_RestartTriggered->store(true); });
  }

  void CWebPage::registerStatusRoutes()
  {
    // Version Get
    m_Server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  String answer = getFirmwareVersion();
                  Serial.println("get version " + answer);
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", answer);
                  addCorsHeaders(response);
                  request->send(response); });

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

    // Status Get [F3]
    //
    // One cheap call behind a client's connection indicator and its clock
    // warning. Read-only, so ungated like GET /api/config. Deliberately does
    // not compute the next alarm: the client already has the schedule, and
    // ESP8266 flash headroom is worth more than duplicated date arithmetic.
    m_Server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  JsonDocument doc;
                  doc["Version"] = getFirmwareVersion();
                  doc["UpTimeSeconds"] = millis() / 1000;
                  doc["FreeHeap"] = ESP.getFreeHeap();

                  const auto &config = configman::getConfig();
                  doc["SensorID"] = config.SensorID;

                  JsonObject wifi = doc["WiFi"].to<JsonObject>();
                  wifi["Connected"] = WiFi.status() == WL_CONNECTED;
                  wifi["SSID"] = WiFi.SSID();
                  wifi["RSSI"] = WiFi.RSSI();
                  wifi["IP"] = WiFi.localIP().toString();

                  // The device owns the alarm, so a client cannot tell whether
                  // a schedule will actually fire without knowing that the
                  // device's clock is both set and correct [F9]. IsSynced
                  // reports whether NTP ever answered; Local is what the alarm
                  // is compared against.
                  JsonObject time = doc["Time"].to<JsonObject>();
                  if (m_TimeHelper != nullptr)
                  {
                    const auto hoursAndMinutes = m_TimeHelper->getHoursAndMinutes();
                    time["IsSynced"] = m_TimeHelper->isTimeSet();
                    time["Local"] = configuration::formatTimeOfDay(
                        configman::Time(hoursAndMinutes.first, hoursAndMinutes.second));
                    time["Weekday"] = m_TimeHelper->getWeekDay();
                    time["Utc"] = m_TimeHelper->getTimestamp();
                  }
                  else
                  {
                    time["IsSynced"] = false;
                  }

                  JsonObject alarm = doc["Alarm"].to<JsonObject>();
                  alarm["IsActivated"] = config.AlarmSettings.IsActivated;
                  alarm["IsRunning"] =
                      m_SunriseAlarm != nullptr && m_SunriseAlarm->isRunning();

                  String body;
                  serializeJson(doc, body);
                  sendJson(request, 200, body); });

    // The page is the one route that is never token-gated: a browser cannot
    // attach a header to a navigation, and this page is what lets an operator
    // type the token in. It is a static asset and discloses nothing.
    //
    // Served pre-compressed, and deliberately with no template processor. The
    // processor replaced every `%...%` pair in the body, which a control UI
    // full of percentages (brightness, `width:100%`) would trip over
    // constantly; its two placeholders were unused because the page fetches
    // /api/config and /api/status instead. Content-Encoding also rules out
    // template substitution, so the two decisions are the same decision.
    m_Server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  Serial.println("get web page");
                  AsyncWebServerResponse *response = request->beginResponse(
                      200, "text/html", web_index_html_gz, web_index_html_gz_len);
                  response->addHeader("Content-Encoding", "gzip");
                  request->send(response); });
  }

  void CWebPage::registerOptionsRoutes()
  {
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
    m_Server.on("/api/status", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, ""); });
    m_Server.on("/api/alarm/test", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                { sendJson(request, 200, ""); });
    m_Server.on("/api/version", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                {
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
                  addCorsHeaders(response);
                  request->send(response); });
  }
}

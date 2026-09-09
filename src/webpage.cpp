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
#include "sensors/sensors.h"
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

    // One rule for the whole API: when ApiToken is set, every /api/* request
    // and /restart must carry it. Reads included.
    //
    // The old split -- writes gated, light control and every GET ungated --
    // meant a device could be read out and its lights driven by anyone on the
    // network while its configuration was protected, and it forced clients
    // into a per-control unlock flow. An empty token (the default) means the
    // device is unprotected, which is what makes first-time setup over the
    // access point work.
    //
    // GET / is the one exception, and has to be: a browser cannot attach a
    // header to a navigation, and that page is what lets an operator type the
    // token in. It is a static asset and discloses nothing.
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

    // Wraps a handler in the token check, so the gate is structural rather
    // than a line every route has to remember.
    ArRequestHandlerFunction guarded(ArRequestHandlerFunction handler)
    {
      return [handler](AsyncWebServerRequest *request)
      {
        if (!isAuthorized(request))
        {
          sendUnauthorized(request);
          return;
        }
        handler(request);
      };
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

    // The raw request body. The form-urlencoded fallback that used to live
    // here existed for a client that could not send a JSON body; that client
    // is gone, and keeping two encodings meant a write could silently do
    // nothing when the wrong one was used.
    String getInput(AsyncWebServerRequest *request)
    {
      if (request->_tempObject == nullptr)
      {
        return String();
      }
      return String(static_cast<const char *>(request->_tempObject));
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
    m_Server.on("/api/config", HTTP_GET, guarded([](AsyncWebServerRequest *request)
                                                 {
                  const auto& config = configman::getConfig();
                  sendJson(request, 200, configman::serializeConfig(&config)); }));

    // Partial write: absent keys keep their stored value, so a client sends
    // only what it is changing. The response is the stored document, which is
    // what lets a client re-render from what the device kept instead of
    // saving and then polling to find out whether it landed.
    //
    // The config is only staged here; loop() applies and persists it.
    m_Server.on("/api/config", HTTP_PUT, guarded([](AsyncWebServerRequest *request)
                                                 {
                  String input = getInput(request);
                  String staged;
                  String error;
                  bool restartRequired = false;
                  if (!configman::stageConfig(input.c_str(), &staged, &error, &restartRequired))
                  {
                    sendJsonMessage(request, 400,
                                    error.isEmpty()
                                        ? String("Error: invalid configuration")
                                        : String("Error: ") + error);
                    return;
                  }
                  JsonDocument doc;
                  deserializeJson(doc, staged);
                  doc["RestartRequired"] = restartRequired;
                  String body;
                  serializeJson(doc, body);
                  sendJson(request, 200, body); }),
                nullptr, collectBody);
  }

  void CWebPage::registerLedRoutes()
  {
    // Partial, like the config write: {"Brightness":40} leaves colour and mode
    // alone. GET /api/led is gone -- /api/status carries the same values and
    // one read is enough to open a screen.
    m_Server.on("/api/led", HTTP_POST, guarded([](AsyncWebServerRequest *request)
                                               {
                  String input = getInput(request);
                  if (input.isEmpty())
                  {
                    sendJsonMessage(request, 400, "Error: No input received");
                    return;
                  }
                  String answer;
                  if (!m_LedService->apply(input, answer))
                  {
                    sendJson(request, 400, answer);
                    return;
                  }
                  sendJson(request, 200, answer); }),
                nullptr, collectBody);
  }

  void CWebPage::registerCommandRoutes()
  {
    // POST, not GET: these mutate. A GET that presses a button is something a
    // link preview or a crawler can trigger.
    m_Server.on("/api/button/1", HTTP_POST, guarded([](AsyncWebServerRequest *request)
                                                    {
                  sendJsonMessage(request, 200, "Button 1 pressed");
                  m_ButtonPressed1->store(true); }));

    m_Server.on("/api/button/2", HTTP_POST, guarded([](AsyncWebServerRequest *request)
                                                    {
                  sendJsonMessage(request, 200, "Button 2 pressed");
                  m_ButtonPressed2->store(true); }));

    // Run the sunrise now, without touching the stored schedule [F11].
    m_Server.on("/api/alarm/test", HTTP_POST, guarded([](AsyncWebServerRequest *request)
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
                  sendJson(request, 200, body); }));

    // POST and JSON, so a client gets an answer it can parse rather than an
    // HTML meta-refresh page written for a browser that no longer loads it.
    m_Server.on("/restart", HTTP_POST, guarded([](AsyncWebServerRequest *request)
                                               {
                  sendJsonMessage(request, 200, "Restarting");
                  Serial.println("restart triggered");
                  m_RestartTriggered->store(true); }));
  }

  void CWebPage::registerStatusRoutes()
  {
    // One read for everything live. /api/version and /api/time are gone; both
    // are fields here, and a client that needed all three used to make three
    // requests to open one screen.
    m_Server.on("/api/status", HTTP_GET, guarded([](AsyncWebServerRequest *request)
                                                 {
                  JsonDocument doc;
                  // Bumped on every breaking change, so a client on the wrong
                  // release can say "this device needs a firmware update"
                  // instead of surfacing a bare 404.
                  doc["ApiVersion"] = kApiVersion;
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

                  // Owner is why a client can state what is driving the strip
                  // rather than inferring it; see LedStrip::LEDOwner.
                  JsonObject light = doc["Light"].to<JsonObject>();
                  light["HasStrip"] = config.NumberOfLEDs > 0;
                  if (m_LedService != nullptr && m_LedService->m_LedStrip != nullptr)
                  {
                    LedStrip *strip = m_LedService->m_LedStrip;
                    const std::array<uint8_t, 3> color = strip->getColor();
                    light["Red"] = color[0];
                    light["Green"] = color[1];
                    light["Blue"] = color[2];
                    light["Brightness"] = int(strip->m_Factor * 100);
                    light["Mode"] = int(strip->m_LEDMode);
                    light["Owner"] = LedStrip::ownerName(strip->m_Owner);
                  }

                  JsonObject alarm = doc["Alarm"].to<JsonObject>();
                  alarm["IsActivated"] = config.AlarmSettings.IsActivated;
                  alarm["IsRunning"] =
                      m_SunriseAlarm != nullptr && m_SunriseAlarm->isRunning();

                  // The cache the loop fills, never a live read: sensor I/O on
                  // the HTTP task would race the loop for the same buses.
                  JsonObject sensors = doc["Sensors"].to<JsonObject>();
                  sensors["AgeSeconds"] = sensor::getCachedValuesAgeSeconds();
                  JsonArray values = sensors["Values"].to<JsonArray>();
                  for (const auto &entry : sensor::getCachedValues())
                  {
                    if (!entry.second.isValid)
                    {
                      continue;
                    }
                    JsonObject value = values.add<JsonObject>();
                    value["Name"] = entry.second.name;
                    value["Value"] = entry.second.value;
                    value["Unit"] = entry.second.unit;
                  }

                  String body;
                  serializeJson(doc, body);
                  sendJson(request, 200, body); }));

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
    for (const char *path : {"/api/config", "/api/led", "/api/status",
                             "/api/alarm/test", "/api/button/1", "/api/button/2",
                             "/restart"})
    {
      m_Server.on(path, HTTP_OPTIONS, [](AsyncWebServerRequest *request)
                  { sendJson(request, 200, ""); });
    }
  }
}

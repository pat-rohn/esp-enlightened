

#include "ts_mqtt.h"
#include <ArduinoJson.h>
#include "timeseries.h"
#include <Ticker.h>

using namespace timeseries;
namespace ts_mqtt
{
    namespace
    {
        bool publishJson(MqttClient *client, const String &topic, const JsonDocument &document)
        {
            // Stream with a known size; the buffered mode truncates payloads above the 128/256 byte TX buffer.
            client->beginMessage(topic, static_cast<unsigned long>(measureJson(document)), true);
            serializeJson(document, *client);
            return client->endMessage() == 1;
        }
    }

    CTimeseriesMQTT::CTimeseriesMQTT(
        const String &topic,
        const String &server,
        CTimeHelper *timehelper,
        MqttClient *client,
        const String &deviceID) : CTimeseries(server, timehelper),
                                  m_DeviceID(deviceID),
                                  m_Topic(topic),
                                  m_Client(client)
    {
    }

    void CTimeseriesMQTT::newValue(const String &name, const double &value)
    {
        Serial.printf("MQTT add value: %s\n", name.c_str());
        String topic = m_Topic + name + "/data";
        if (!m_Client->connected())
        {
            Serial.println("MQTT: Not connected");
            m_DiscoveredMetrics.clear();
            return;
        }
        String val = convertValue(value);
        m_Client->beginMessage(topic);
        m_Client->print(val);
        m_Client->endMessage();
        Serial.printf("%s %s\n", topic.c_str(), val.c_str());

        // Retained copy for Home Assistant; the legacy topic above stays unchanged.
        if (m_DeviceID.length() > 0 && name.startsWith(m_DeviceID))
        {
            const String metric = name.substring(m_DeviceID.length());
            if (metric.length() > 0)
            {
                if (m_DiscoveredMetrics.count(metric) == 0)
                {
                    publishMeasurementDiscovery(metric);
                    m_DiscoveredMetrics.insert(metric);
                }
                m_Client->beginMessage("iotedge/" + m_DeviceID + "/state/" + metric, true);
                m_Client->print(val);
                m_Client->endMessage();
            }
        }
    }

    bool CTimeseriesMQTT::reportHealth(const DeviceHealth &health)
    {
        bool mqttReported = false;
        if (!m_Client->connected())
        {
            Serial.println("MQTT: Not connected for health report");
            m_DiagnosticsDiscovered = false;
        }
        else
        {
            JsonDocument doc;
            doc["device"] = health.Device;
            doc["boot_id"] = health.BootID;
            doc["firmware_version"] = health.FirmwareVersion;
            doc["uptime_seconds"] = health.UptimeSeconds;
            doc["free_heap_bytes"] = health.FreeHeapBytes;
            doc["wifi_rssi"] = health.WiFiRSSI;
            doc["time_synced"] = health.TimeSynced;
            doc["sensor_age_seconds"] = health.SensorAgeSeconds;

            const String topic = "iotedge/" + health.Device + "/health";
            mqttReported = publishJson(m_Client, topic, doc);
            if (mqttReported)
            {
                m_Client->beginMessage("iotedge/" + health.Device + "/availability", true);
                m_Client->print("online");
                mqttReported = m_Client->endMessage() == 1;
                if (mqttReported && !m_DiagnosticsDiscovered)
                {
                    m_DiagnosticsDiscovered = publishDiscovery(health);
                    mqttReported = m_DiagnosticsDiscovered;
                }
            }
        }
        const bool httpReported = CTimeseries::reportHealth(health);
        return mqttReported || httpReported;
    }

    void CTimeseriesMQTT::publishMeasurementDiscovery(const String &metric)
    {
        const char *unit = nullptr;
        const char *deviceClass = nullptr;
        if (metric == "CO2")
        {
            unit = "ppm";
            deviceClass = "carbon_dioxide";
        }
        else if (metric.startsWith("Temperature"))
        {
            unit = "\u00b0C";
            deviceClass = "temperature";
        }
        else if (metric == "Humidity")
        {
            unit = "%";
            deviceClass = "humidity";
        }
        else if (metric == "Pressure")
        {
            unit = "hPa";
            deviceClass = "pressure";
        }

        JsonDocument doc;
        doc["name"] = metric;
        doc["unique_id"] = "enlightened_" + m_DeviceID + "_" + metric;
        doc["state_topic"] = "iotedge/" + m_DeviceID + "/state/" + metric;
        doc["state_class"] = "measurement";
        if (unit != nullptr)
        {
            doc["unit_of_measurement"] = unit;
        }
        if (deviceClass != nullptr)
        {
            doc["device_class"] = deviceClass;
        }
        doc["availability_topic"] = "iotedge/" + m_DeviceID + "/availability";
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"].to<JsonArray>().add("enlightened:" + m_DeviceID);
        device["name"] = m_DeviceID;
        device["manufacturer"] = "esp-enlightened";
        publishJson(m_Client, "homeassistant/sensor/" + m_DeviceID + "_" + metric + "/config", doc);
    }

    bool CTimeseriesMQTT::publishDiscovery(const DeviceHealth &health)
    {
        struct Entity
        {
            const char *component;
            const char *metric;
            const char *name;
            const char *valueTemplate;
            const char *unit;
            const char *deviceClass;
        };
        static const Entity entities[] = {
            {"sensor", "firmware_version", "Firmware version", "{{ value_json.firmware_version }}", nullptr, nullptr},
            {"sensor", "uptime", "Uptime", "{{ value_json.uptime_seconds }}", "s", "duration"},
            {"sensor", "wifi_rssi", "Wi-Fi RSSI", "{{ value_json.wifi_rssi }}", "dBm", "signal_strength"},
            {"sensor", "free_heap", "Free heap", "{{ value_json.free_heap_bytes }}", "B", "data_size"},
            {"sensor", "sensor_age", "Sensor freshness", "{{ value_json.sensor_age_seconds }}", "s", "duration"},
            {"binary_sensor", "time_synced", "Clock synchronized", "{{ 'ON' if value_json.time_synced else 'OFF' }}", nullptr, nullptr},
        };

        const String deviceID = "enlightened:" + health.Device;
        const String availabilityTopic = "iotedge/" + health.Device + "/availability";
        const String healthTopic = "iotedge/" + health.Device + "/health";

        auto fillDevice = [&](JsonDocument &doc)
        {
            doc["availability_topic"] = availabilityTopic;
            doc["payload_available"] = "online";
            doc["payload_not_available"] = "offline";
            JsonObject device = doc["device"].to<JsonObject>();
            device["identifiers"].to<JsonArray>().add(deviceID);
            device["name"] = health.Device;
            device["manufacturer"] = "esp-enlightened";
            device["model"] = "esp-enlightened";
            device["sw_version"] = health.FirmwareVersion;
        };

        // Connectivity follows the retained availability topic (LWT/birth) only.
        JsonDocument connectivity;
        connectivity["name"] = "Connectivity";
        connectivity["unique_id"] = "enlightened_" + health.Device + "_connectivity";
        connectivity["state_topic"] = availabilityTopic;
        connectivity["payload_on"] = "online";
        connectivity["payload_off"] = "offline";
        connectivity["device_class"] = "connectivity";
        connectivity["entity_category"] = "diagnostic";
        JsonObject connectivityDevice = connectivity["device"].to<JsonObject>();
        connectivityDevice["identifiers"].to<JsonArray>().add(deviceID);
        connectivityDevice["name"] = health.Device;
        connectivityDevice["manufacturer"] = "esp-enlightened";
        connectivityDevice["model"] = "esp-enlightened";
        connectivityDevice["sw_version"] = health.FirmwareVersion;
        if (!publishJson(m_Client, "homeassistant/binary_sensor/" + health.Device + "_connectivity/config", connectivity))
        {
            return false;
        }

        for (const Entity &entity : entities)
        {
            JsonDocument doc;
            doc["name"] = entity.name;
            doc["unique_id"] = "enlightened_" + health.Device + "_" + entity.metric;
            doc["state_topic"] = healthTopic;
            doc["value_template"] = entity.valueTemplate;
            doc["entity_category"] = "diagnostic";
            if (entity.unit != nullptr)
            {
                doc["unit_of_measurement"] = entity.unit;
            }
            if (entity.deviceClass != nullptr)
            {
                doc["device_class"] = entity.deviceClass;
            }
            fillDevice(doc);
            if (!publishJson(m_Client, String("homeassistant/") + entity.component + "/" + health.Device + "_" + entity.metric + "/config", doc))
            {
                return false;
            }
        }
        return true;
    }

}

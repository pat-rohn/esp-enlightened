#ifndef DOMAIN_CONFIGURATION_CODEC_H
#define DOMAIN_CONFIGURATION_CODEC_H

#include <ArduinoJson.h>

#include <utility>

#include "configuration.h"

namespace configuration
{
    // How strictly a document is parsed.
    //
    // The distinction exists because the two callers have opposite failure
    // costs. An HTTP write is untrusted input that must be refused when it is
    // wrong, so the client learns about it; the device's own stored
    // configuration is already-accepted data, and refusing it is destructive --
    // config.cpp overwrites the file with defaults when deserialization
    // fails, which would throw away the Wi-Fi credentials along with the bad
    // field.
    enum class ParseMode
    {
        // Repair what can be repaired, keeping the existing value for any field
        // that cannot be parsed. For reading the stored configuration.
        Lenient,
        // Refuse the whole document and report why. For HTTP writes.
        Strict,
    };

    // Always "HH:MM", zero-padded.
    String formatTimeOfDay(const Time &time);

    // Accepts "H:M" and "HH:MM", rejecting anything else -- including
    // out-of-range values, non-digits, a missing or repeated colon, and
    // trailing junk. Unpadded input is accepted because configurations written
    // by older builds carry it. Leaves `out` untouched when it returns false.
    bool parseTimeOfDay(const char *text, Time &out);

    // Whether moving from `current` to `next` needs a restart to take effect.
    //
    // Only the alarm schedule and the light presets are picked up live -- the
    // loop re-applies AlarmSettings on every config change, and the button
    // handlers read the presets at press time. Everything else was consumed at
    // boot: pins were configured, Wi-Fi was joined, MQTT connected, the OTA
    // password was set. Reporting this beats every client guessing at it from
    // which field it happened to change.
    bool requiresRestart(const Configuration &current, const Configuration &next);

    String serializeConfig(const Configuration *config, bool revealSecrets = false);
    String prettyPrintConfig(const char *configStr);
    JsonDocument serializeSunrise(const SunriseSettings *config);
    JsonDocument serializeDaySettings(const AlarmWeekday *config);

    // Merges `configStr` onto `existingConfig`: a key that is absent keeps the
    // stored value, a key that is present replaces it. So a client may write
    // any subset -- {"SunriseSettings":{"IsActivated":true}} is a complete,
    // valid request -- and cannot clobber fields it does not model.
    //
    // Secrets follow the same rule, which makes clearing one expressible:
    // absent keeps, "" clears. serializeConfig() omits them when redacting so
    // that a GET-then-PUT round trip cannot clear them by accident.
    //
    // `error`, when given, receives the reason the document was refused.
    std::pair<bool, Configuration> deserializeConfig(
        const char *configStr, const Configuration &existingConfig,
        ParseMode mode = ParseMode::Lenient, String *error = nullptr);
    SunriseSettings deserializeSunrise(
        JsonVariantConst doc, const SunriseSettings &existing = SunriseSettings(),
        ParseMode mode = ParseMode::Lenient, String *error = nullptr);
    AlarmWeekday deserializeDaySetting(
        JsonVariantConst doc, const AlarmWeekday &existing = AlarmWeekday(),
        ParseMode mode = ParseMode::Lenient, String *error = nullptr);
}

#endif

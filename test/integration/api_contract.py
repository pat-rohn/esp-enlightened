#!/usr/bin/env python3
"""HTTP contract check for a running esp-enlightened device (API version 2)."""

import argparse
import json
import re
import socket
import sys
import time
from dataclasses import dataclass
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


API_VERSION = 2

# Secrets are deliberately absent from this set: redacted output omits them
# entirely rather than blanking them, so that feeding a GET straight back into
# a PUT cannot clear what it could not disclose.
REQUIRED_CONFIG_FIELDS = {
    "IsConfigured", "ServerAddress", "SensorID", "WiFiName",
    "HasWiFiPassword", "HasApiToken",
    "DhtPin", "SerialRX", "SerialTX", "AnalogSensorPin0", "AnalogSensorPin1",
    "WindSensorPin", "RainfallSensorPin", "LEDPin", "OneWirePin",
    "Button1", "Button2", "Button2GetURL", "NumberOfLEDs",
    "FindSensors", "IsOfflineMode", "ShowWebpage",
    "UseMQTT", "MQTTTopic", "MQTTPort",
    "DeepSleepTime", "BufferedValues", "MeasureInterval",
    "SunriseSettings", "LightLow", "LightMedium", "LightHigh",
}

# Every route the v2 contract removed. Each must be gone, not merely unused: a
# route left behind is a second way to do something, and the reason the old API
# had two write encodings and three ways to read the time.
REMOVED_ROUTES = [
    ("GET", "/api/version"),
    ("GET", "/api/time"),
    ("GET", "/api/led"),
    ("PUT", "/api/led"),
    ("POST", "/api/config"),
    ("GET", "/api/button1"),
    ("GET", "/api/button2"),
    ("GET", "/restart"),
    ("GET", "/get"),
]


@dataclass
class Response:
    status: int
    body: bytes
    content_type: str


class ContractFailure(Exception):
    pass


def request(base_url: str, path: str, method: str = "GET",
            payload: Any | None = None, token: str | None = None) -> Response:
    headers = {"Accept": "application/json"}
    data = None
    if payload is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(payload).encode("utf-8")
    if token:
        headers["X-Authorization"] = token

    req = Request(f"{base_url}{path}", data=data, method=method, headers=headers)
    try:
        with urlopen(req, timeout=5) as response:
            return Response(response.status, response.read(),
                            response.headers.get_content_type())
    except HTTPError as error:
        return Response(error.code, error.read(), error.headers.get_content_type())
    except URLError as error:
        raise ContractFailure(f"{method} {path}: connection failed: {error.reason}") from error


def require_status(response: Response, expected: int, description: str) -> None:
    if response.status != expected:
        raise ContractFailure(
            f"{description}: expected HTTP {expected}, got {response.status}: "
            f"{response.body.decode('utf-8', errors='replace')}")


def response_json(response: Response, description: str) -> dict[str, Any]:
    try:
        value = json.loads(response.body)
    except json.JSONDecodeError as error:
        raise ContractFailure(f"{description}: response is not valid JSON: {error}") from error
    if not isinstance(value, dict):
        raise ContractFailure(f"{description}: expected a JSON object")
    return value


def check_config(config: dict[str, Any], description: str) -> None:
    missing = REQUIRED_CONFIG_FIELDS - config.keys()
    if missing:
        raise ContractFailure(f"{description}: missing fields: {', '.join(sorted(missing))}")
    for secret in ("WiFiPassword", "ApiToken"):
        if secret in config:
            raise ContractFailure(
                f"{description}: {secret} must be omitted, not returned")
    for flag in ("HasWiFiPassword", "HasApiToken"):
        if not isinstance(config[flag], bool):
            raise ContractFailure(f"{description}: {flag} must be a boolean")


def check_status(status: dict[str, Any]) -> None:
    if status.get("ApiVersion") != API_VERSION:
        raise ContractFailure(
            f"GET /api/status: ApiVersion is {status.get('ApiVersion')!r}, "
            f"expected {API_VERSION}")
    for field in ("Version", "UpTimeSeconds", "FreeHeap", "SensorID"):
        if field not in status:
            raise ContractFailure(f"GET /api/status: missing {field}")

    for group, fields in (
        ("WiFi", ("Connected", "SSID", "RSSI", "IP")),
        ("Time", ("IsSynced",)),
        ("Light", ("HasStrip",)),
        ("Alarm", ("IsActivated", "IsRunning")),
        ("Sensors", ("AgeSeconds", "Values")),
    ):
        if not isinstance(status.get(group), dict):
            raise ContractFailure(f"GET /api/status: missing {group} object")
        for field in fields:
            if field not in status[group]:
                raise ContractFailure(f"GET /api/status: missing {group}.{field}")

    if status["Light"]["HasStrip"]:
        for field in ("Red", "Green", "Blue", "Brightness", "Mode", "Owner"):
            if field not in status["Light"]:
                raise ContractFailure(f"GET /api/status: missing Light.{field}")
        if status["Light"]["Owner"] not in (
                "manual", "sunrise", "mqtt", "button", "sensor"):
            raise ContractFailure(
                f"GET /api/status: unknown Light.Owner {status['Light']['Owner']!r}")

    values = status["Sensors"]["Values"]
    if not isinstance(values, list):
        raise ContractFailure("GET /api/status: Sensors.Values must be a list")
    for value in values:
        for field in ("Name", "Value", "Unit"):
            if field not in value:
                raise ContractFailure(f"GET /api/status: missing Sensors.Values[].{field}")


def check_removed_routes(base_url: str, token: str | None) -> None:
    for method, path in REMOVED_ROUTES:
        require_status(request(base_url, path, method, token=token), 404,
                       f"removed route {method} {path}")


def check_uniform_auth(base_url: str) -> None:
    """With a token set, everything under /api and /restart needs it.

    The page itself must stay reachable: a browser cannot attach a header to a
    navigation, and that page is what lets an operator type the token in.
    """
    for method, path in (
        ("GET", "/api/status"),
        ("GET", "/api/config"),
        ("PUT", "/api/config"),
        ("POST", "/api/led"),
        ("POST", "/api/alarm/test"),
        ("POST", "/api/button/1"),
        ("POST", "/api/button/2"),
        ("POST", "/restart"),
    ):
        payload = {} if method in ("PUT", "POST") else None
        require_status(request(base_url, path, method, payload), 401,
                       f"unauthenticated {method} {path}")
    require_status(request(base_url, "/"), 200, "unauthenticated GET /")


def check_partial_writes(base_url: str, token: str | None,
                         config: dict[str, Any]) -> None:
    """A write carries only what it changes, and cannot clobber the rest.

    This is the rule the whole client contract rests on. Writing one field used
    to mean resending the entire document, which made every client responsible
    for fields it did not model -- and the app grew five otherwise-unused
    settings purely to avoid resetting them.
    """
    sensor_id = config["SensorID"]
    response = request(base_url, "/api/config", "PUT", {"SensorID": sensor_id}, token)
    require_status(response, 200, "partial PUT /api/config")
    stored = response_json(response, "partial PUT /api/config")
    check_config(stored, "partial PUT /api/config")

    if "RestartRequired" not in stored:
        raise ContractFailure("PUT /api/config: missing RestartRequired")
    if stored["RestartRequired"]:
        raise ContractFailure(
            "PUT /api/config: a no-op write reported RestartRequired")

    for field in ("NumberOfLEDs", "LEDPin", "MQTTTopic", "MeasureInterval"):
        if stored[field] != config[field]:
            raise ContractFailure(
                f"partial PUT /api/config: {field} changed from "
                f"{config[field]!r} to {stored[field]!r}")

    # Arming the alarm must not disturb the per-day schedule.
    armed = config["SunriseSettings"]["IsActivated"]
    response = request(base_url, "/api/config", "PUT",
                       {"SunriseSettings": {"IsActivated": armed}}, token)
    require_status(response, 200, "partial alarm PUT /api/config")
    stored = response_json(response, "partial alarm PUT /api/config")
    if stored["SunriseSettings"] != config["SunriseSettings"]:
        raise ContractFailure(
            "partial alarm PUT /api/config: the day schedule was not preserved")


def check_partial_light(base_url: str, token: str | None,
                        light: dict[str, Any]) -> None:
    """{"Brightness": n} leaves colour and mode alone."""
    response = request(base_url, "/api/led", "POST",
                       {"Brightness": light["Brightness"]}, token)
    require_status(response, 200, "partial POST /api/led")
    stored = response_json(response, "partial POST /api/led")
    for channel in ("Red", "Green", "Blue", "Mode"):
        if stored[channel] != light[channel]:
            raise ContractFailure(
                f"partial POST /api/led: {channel} changed from "
                f"{light[channel]!r} to {stored[channel]!r}")


def oversized_body_survives(base_url: str) -> None:
    """Declare a short Content-Length, then send far more than that.

    ESPAsyncWebServer >= 3.11 clamps body chunks to Content-Length itself, but
    3.6.x (the floor of our `^3.6.0` range) hands the handler whatever arrived.
    webpage.cpp's collectBody sized its buffer from Content-Length, so on 3.6.0
    this wrote ~1300 bytes into an 11 byte allocation and panicked the device
    with LoadProhibited on corrupted heap metadata -- before any handler, and
    therefore before isAuthorized, ever ran. The device must stay up.
    """
    host = re.sub(r"^https?://", "", base_url).split("/")[0]
    port = 80
    if ":" in host:
        host, _, port_text = host.partition(":")
        port = int(port_text)

    head = (
        f"PUT /api/config HTTP/1.1\r\nHost: {host}\r\n"
        "Content-Type: application/json\r\nContent-Length: 10\r\n"
        "Connection: close\r\n\r\n"
    ).encode()
    try:
        connection = socket.create_connection((host, port), timeout=10)
    except OSError as error:
        raise ContractFailure(f"oversized body probe: cannot connect: {error}") from error
    try:
        connection.sendall(head + b"A" * 4000)
        connection.settimeout(10)
        # A device that clamps correctly either answers (the library already
        # trimmed the body) or never completes the request (the body overshot
        # Content-Length, so the parser never reaches its end). Both are fine;
        # a reboot is not.
        connection.recv(4096)
    except (socket.timeout, TimeoutError, OSError):
        pass
    finally:
        connection.close()

    for _ in range(10):
        try:
            probe = request(base_url, "/api/status")
            if probe.status in (200, 401):
                return
        except ContractFailure:
            pass
        time.sleep(1)
    raise ContractFailure(
        "oversized body probe: device stopped answering -- it likely crashed on "
        "an over-declared Content-Length")


def check_page_is_gzipped(base_url: str) -> None:
    req = Request(f"{base_url}/", headers={"Accept-Encoding": "gzip"})
    try:
        with urlopen(req, timeout=5) as response:
            encoding = response.headers.get("Content-Encoding")
            body = response.read()
    except (HTTPError, URLError) as error:
        raise ContractFailure(f"GET /: {error}") from error
    if encoding != "gzip":
        raise ContractFailure(f"GET /: expected Content-Encoding gzip, got {encoding!r}")
    if not body.startswith(b"\x1f\x8b"):
        raise ContractFailure("GET /: body is not a gzip stream")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", required=True,
                        help="device origin, for example http://192.168.4.1")
    parser.add_argument("--token", help="configured pre-shared API token")
    parser.add_argument("--expect-auth", action="store_true",
                        help="assert that every /api request without the token is 401")
    parser.add_argument("--mutations", action="store_true",
                        help="run validation refusals and no-op partial writes")
    parser.add_argument("--commands", action="store_true",
                        help="press buttons and restart; requires --mutations")
    args = parser.parse_args()

    if args.commands and not args.mutations:
        parser.error("--commands requires --mutations")

    base_url = args.base_url.rstrip("/")
    try:
        check_page_is_gzipped(base_url)

        if args.expect_auth:
            check_uniform_auth(base_url)

        status_response = request(base_url, "/api/status", token=args.token)
        require_status(status_response, 200, "GET /api/status")
        status = response_json(status_response, "GET /api/status")
        check_status(status)

        config_response = request(base_url, "/api/config", token=args.token)
        require_status(config_response, 200, "GET /api/config")
        config = response_json(config_response, "GET /api/config")
        check_config(config, "GET /api/config")

        check_removed_routes(base_url, args.token)
        oversized_body_survives(base_url)

        if args.mutations:
            require_status(
                request(base_url, "/api/config", "PUT", {"NumberOfLEDs": "many"},
                        args.token),
                400, "wrong-typed PUT /api/config")
            require_status(
                request(base_url, "/api/config", "PUT",
                        {"SunriseSettings": {"Monday": {"AlarmTime": "25:00"}}},
                        args.token),
                400, "invalid alarm time PUT /api/config")
            require_status(
                request(base_url, "/api/led", "POST", {"Mode": 999}, args.token),
                400, "out-of-range POST /api/led")

            check_partial_writes(base_url, args.token, config)
            if status["Light"]["HasStrip"]:
                check_partial_light(base_url, args.token, status["Light"])

        if args.commands:
            require_status(request(base_url, "/api/button/1", "POST", token=args.token),
                           200, "POST /api/button/1")
            require_status(request(base_url, "/api/button/2", "POST", token=args.token),
                           200, "POST /api/button/2")
            require_status(request(base_url, "/restart", "POST", token=args.token),
                           200, "POST /restart")
    except ContractFailure as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print("PASS: HTTP contract checks completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Manual HTTP contract check for a running esp-enlightened device."""

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


REQUIRED_CONFIG_FIELDS = {
    "IsConfigured",
    "ServerAddress",
    "SensorID",
    "WiFiName",
    "WiFiPassword",
    "HasWiFiPassword",
    "ApiToken",
    "HasApiToken",
    "DhtPin",
    "SerialRX",
    "SerialTX",
    "AnalogSensorPin0",
    "AnalogSensorPin1",
    "WindSensorPin",
    "RainfallSensorPin",
    "LEDPin",
    "OneWirePin",
    "Button1",
    "Button2",
    "Button2GetURL",
    "NumberOfLEDs",
    "FindSensors",
    "IsOfflineMode",
    "ShowWebpage",
    "UseMQTT",
    "MQTTTopic",
    "MQTTPort",
    "DeepSleepTime",
    "BufferedValues",
    "MeasureInterval",
    "SunriseSettings",
    "LightLow",
    "LightMedium",
    "LightHigh",
}


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
            return Response(
                response.status,
                response.read(),
                response.headers.get_content_type(),
            )
    except HTTPError as error:
        return Response(
            error.code,
            error.read(),
            error.headers.get_content_type(),
        )
    except URLError as error:
        raise ContractFailure(f"{method} {path}: connection failed: {error.reason}") from error


def require_status(response: Response, expected: int, description: str) -> None:
    if response.status != expected:
        raise ContractFailure(
            f"{description}: expected HTTP {expected}, got {response.status}: "
            f"{response.body.decode('utf-8', errors='replace')}"
        )


def response_json(response: Response, description: str) -> dict[str, Any]:
    try:
        value = json.loads(response.body)
    except json.JSONDecodeError as error:
        raise ContractFailure(f"{description}: response is not valid JSON: {error}") from error
    if not isinstance(value, dict):
        raise ContractFailure(f"{description}: expected a JSON object")
    return value


def check_config(config: dict[str, Any]) -> None:
    missing = REQUIRED_CONFIG_FIELDS - config.keys()
    if missing:
        raise ContractFailure(f"GET /api/config: missing fields: {', '.join(sorted(missing))}")
    if config["WiFiPassword"] != "" or config["ApiToken"] != "":
        raise ContractFailure("GET /api/config: secret fields must be redacted")
    if not isinstance(config["HasWiFiPassword"], bool) or not isinstance(config["HasApiToken"], bool):
        raise ContractFailure("GET /api/config: HasWiFiPassword/HasApiToken must be booleans")


def check_led(led: dict[str, Any]) -> None:
    for field in ("Red", "Green", "Blue", "Brightness", "Mode", "Message"):
        if field not in led:
            raise ContractFailure(f"GET /api/led: missing {field}")
    if not isinstance(led["Message"], str):
        raise ContractFailure("GET /api/led: Message must be a string")


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
        # A device that clamps correctly either answers 401 (the library already
        # trimmed the body) or never completes the request (the body overshot
        # Content-Length, so the parser never reaches its end). Both are fine;
        # a reboot is not.
        connection.recv(4096)
    except (socket.timeout, TimeoutError, OSError):
        pass
    finally:
        connection.close()

    # The device needs a moment if it is busy, but it must not have restarted.
    for attempt in range(10):
        try:
            probe = request(base_url, "/api/version")
            if probe.status == 200 and probe.body.strip():
                return
        except ContractFailure:
            pass
        time.sleep(1)
    raise ContractFailure(
        "oversized body probe: device stopped answering -- it likely crashed on "
        "an over-declared Content-Length"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", required=True,
                        help="device origin, for example http://192.168.4.1")
    parser.add_argument("--token", help="configured pre-shared API token")
    parser.add_argument("--expect-auth", action="store_true",
                        help="assert a no-token command request returns 401")
    parser.add_argument("--mutations", action="store_true",
                        help="run safe validation and no-op PUT checks")
    parser.add_argument("--commands", action="store_true",
                        help="press buttons and restart; requires --mutations")
    args = parser.parse_args()

    if args.commands and not args.mutations:
        parser.error("--commands requires --mutations")

    base_url = args.base_url.rstrip("/")
    try:
        if args.expect_auth:
            require_status(
                request(base_url, "/api/button1"),
                401,
                "unauthenticated GET /api/button1",
            )

        config_response = request(base_url, "/api/config", token=args.token)
        require_status(config_response, 200, "GET /api/config")
        config = response_json(config_response, "GET /api/config")
        check_config(config)

        led_response = request(base_url, "/api/led", token=args.token)
        require_status(led_response, 200, "GET /api/led")
        check_led(response_json(led_response, "GET /api/led"))

        version_response = request(base_url, "/api/version", token=args.token)
        require_status(version_response, 200, "GET /api/version")
        if not version_response.body.strip():
            raise ContractFailure("GET /api/version: response must not be empty")

        time_response = request(base_url, "/api/time", token=args.token)
        require_status(time_response, 200, "GET /api/time")
        if not re.fullmatch(r"\d{1,2}:\d{1,2} \(weekday \d+\)",
                            time_response.body.decode("utf-8").strip()):
            raise ContractFailure("GET /api/time: unexpected response format")

        require_status(request(base_url, "/get", token=args.token), 404, "GET /get")

        oversized_body_survives(base_url)

        if args.mutations:
            require_status(
                request(base_url, "/api/config", "POST", {"IsConfigured": "invalid"},
                        args.token),
                400,
                "invalid POST /api/config",
            )
            require_status(
                request(base_url, "/api/led", "POST",
                        {"Mode": 999, "Brightness": 0, "Red": 0, "Green": 0, "Blue": 0},
                        args.token),
                400,
                "invalid POST /api/led",
            )
            put_response = request(base_url, "/api/config", "PUT", config, args.token)
            require_status(put_response, 200, "no-op PUT /api/config")
            check_config(response_json(put_response, "no-op PUT /api/config"))

        if args.commands:
            require_status(request(base_url, "/api/button1", token=args.token), 200,
                           "GET /api/button1")
            require_status(request(base_url, "/api/button2", token=args.token), 200,
                           "GET /api/button2")
            require_status(request(base_url, "/restart", token=args.token), 200,
                           "GET /restart")
    except ContractFailure as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print("PASS: HTTP contract checks completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

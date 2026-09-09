#!/usr/bin/env python3
"""Static checks on the embedded web page.

Run with no arguments from the repository root:

    python3 test/web/check_page.py

These are the checks that do not need a device. The device-side contract lives
in test/integration/api_contract.py.
"""

import gzip
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "web" / "index.html"
WEBPAGE_CPP = ROOT / "src" / "webpage.cpp"

# Matches the generator's budget in extra_scripts/web_assets.py.
MAX_COMPRESSED = 64 * 1024


class CheckFailure(Exception):
    pass


def page_text() -> str:
    return PAGE.read_text(encoding="utf-8")


def check_no_external_resources(text: str) -> None:
    """Setup runs from the device's own access point, with no internet.

    A CDN font or script would simply fail to load there, and the failure is
    silent -- the page renders, slightly wrong, with no error anyone will see.
    """
    for match in re.finditer(r'(?:src|href)\s*=\s*"([^"]+)"', text):
        url = match.group(1)
        if url.startswith(("http://", "https://", "//")):
            raise CheckFailure(f"external resource: {url}")
    for pattern in (r"@import\s+url\(", r"fonts\.googleapis\.com", r"cdn\."):
        if re.search(pattern, text):
            raise CheckFailure(f"external resource reference matching {pattern!r}")


def check_size(text: str) -> None:
    compressed = len(gzip.compress(text.encode("utf-8"), compresslevel=9, mtime=0))
    if compressed > MAX_COMPRESSED:
        raise CheckFailure(
            f"page compresses to {compressed} bytes, over the {MAX_COMPRESSED} budget")
    print(f"  size: {len(text.encode('utf-8'))} raw -> {compressed} gzipped")


def page_routes(text: str) -> set[tuple[str, str]]:
    """(method, path) pairs the page actually calls."""
    routes: set[tuple[str, str]] = set()
    # api("/api/x") and api("/api/x", {method: "POST"}), plus the fetch of "/".
    for match in re.finditer(
            r'api\(\s*(?:"([^"]+)"|`([^`$]+)`)\s*(?:,\s*\{([^}]*)\})?', text):
        path = match.group(1) or match.group(2)
        options = match.group(3) or ""
        method = re.search(r'method:\s*"(\w+)"', options)
        routes.add(((method.group(1) if method else "GET"), path.split("?")[0]))
    # Template-literal paths are parameterised; normalise the ones we use.
    return {(method, re.sub(r"\$\{[^}]+\}", "1", path)) for method, path in routes}


def firmware_routes() -> set[tuple[str, str]]:
    source = WEBPAGE_CPP.read_text(encoding="utf-8")
    routes = {
        (match.group(2), match.group(1))
        for match in re.finditer(r'm_Server\.on\(\s*"([^"]+)"\s*,\s*HTTP_(\w+)', source)
    }
    # Registered in a loop over a path list rather than one call each.
    for match in re.finditer(r'for \(const char \*path : \{([^}]+)\}\)', source):
        for path in re.findall(r'"([^"]+)"', match.group(1)):
            routes.add(("OPTIONS", path))
    return routes


def check_routes_exist(text: str) -> None:
    """Every route the page calls must be one the firmware registers.

    The page and the firmware are edited in the same repository but never
    compiled together, so a renamed or dropped route is only discovered by
    loading the page on a device and noticing something quietly does nothing.
    """
    firmware = firmware_routes()
    missing = sorted(route for route in page_routes(text) if route not in firmware)
    if missing:
        raise CheckFailure(
            "the page calls routes the firmware does not register: "
            + ", ".join(f"{method} {path}" for method, path in missing))
    print(f"  routes: {len(page_routes(text))} used, all registered")


def check_api_version_matches(text: str) -> None:
    """The page's expected API version must match the firmware's."""
    page_match = re.search(r"const API_VERSION = (\d+)", text)
    if page_match is None:
        raise CheckFailure("the page does not declare API_VERSION")
    header = (ROOT / "src" / "webpage.h").read_text(encoding="utf-8")
    firmware_match = re.search(r"constexpr int kApiVersion = (\d+)", header)
    if firmware_match is None:
        raise CheckFailure("webpage.h does not declare kApiVersion")
    if page_match.group(1) != firmware_match.group(1):
        raise CheckFailure(
            f"page expects API version {page_match.group(1)} but the firmware "
            f"reports {firmware_match.group(1)}")
    print(f"  API version: {page_match.group(1)} on both sides")


def check_no_stray_template_markers(text: str) -> None:
    """A leftover %placeholder% would have been substituted by the old
    processor. The processor is gone, but a stray marker still signals that
    something was written expecting server-side substitution."""
    for match in re.finditer(r"%(devconfig|fwversion)%", text):
        raise CheckFailure(f"stray template placeholder: {match.group(0)}")


def main() -> int:
    checks = [
        ("no external resources", check_no_external_resources),
        ("size within budget", check_size),
        ("routes exist in the firmware", check_routes_exist),
        ("API version agrees with the firmware", check_api_version_matches),
        ("no stray template placeholders", check_no_stray_template_markers),
    ]
    text = page_text()
    failed = False
    for name, check in checks:
        try:
            print(f"- {name}")
            check(text)
        except CheckFailure as error:
            print(f"  FAIL: {error}", file=sys.stderr)
            failed = True
    if failed:
        return 1
    print("PASS: web page checks completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

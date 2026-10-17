"""Read-only feasibility probe; no accounts, media playback or app changes.

Jamendo's public sample client ID is documented for read-API tests only.
It must never become a production application credential.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


SAMPLE_CLIENT_ID = "709fa152"
MAX_RESPONSE = 1024 * 1024
USER_AGENT = "TonalCatalogResearch/0.1 (local noncommercial feasibility study)"


def probe(provider, base, params):
    started = time.monotonic()
    result = {"provider": provider, "endpoint": base, "media_requested": False}
    try:
        request = Request(base + "?" + urlencode(params), headers={
            "User-Agent": USER_AGENT, "Accept": "application/json"})
        with urlopen(request, timeout=15) as response:
            result["http_status"] = response.status
            payload = response.read(MAX_RESPONSE + 1)
        if len(payload) > MAX_RESPONSE:
            raise ValueError("Response exceeds research size limit")
        data = json.loads(payload)
        if provider == "musicbrainz":
            entries = data.get("artists", [])
        elif provider == "jamendo":
            result["sample_client_id_only"] = True
            if data.get("headers", {}).get("status") != "success":
                result["provider_error_code"] = data.get("headers", {}).get("code")
                result["provider_error_message"] = str(data.get("headers", {}).get("error_message", ""))[:240]
                raise ValueError("Jamendo reported an API error")
            entries = data.get("results", [])
        else:
            if "error" in data:
                raise ValueError("Provider reported an API error")
            entries = data.get("data", [])
        if not isinstance(entries, list):
            raise ValueError("Unexpected collection type")
        result.update({"ok": True, "returned_items": len(entries)})
        # Small public metadata sample only; do not store expiring media URLs.
        result["samples"] = [
            {"id": str(item.get("id", "")),
             "title_or_name": item.get("title", item.get("name", "")),
             "artist": item.get("artist_name") or
                 (item.get("artist") or {}).get("name") or
                 (item.get("user") or {}).get("name") or "",
             "audio_url_present": bool(item.get("audio")),
             "preview_url_present": bool(item.get("preview")),
             "gated": item.get("is_stream_gated"),
             "streamable_metadata": item.get("is_streamable")}
            for item in entries[:3]
        ]
    except HTTPError as error:
        result.update({"ok": False, "http_status": error.code,
                       "error": "HTTP error", "retry_after": error.headers.get("Retry-After")})
    except (URLError, TimeoutError, OSError, ValueError) as error:
        result.update({"ok": False, "error": type(error).__name__})
    result["elapsed_seconds"] = round(time.monotonic() - started, 3)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    targets = [
        ("audius", "https://api.audius.co/v1/tracks/search",
         {"query": "Nirvana", "limit": 3}),
        ("musicbrainz", "https://musicbrainz.org/ws/2/artist",
         {"query": 'artist:"Nirvana"', "fmt": "json", "limit": 3}),
        ("jamendo", "https://api.jamendo.com/v3.0/tracks/",
         {"client_id": SAMPLE_CLIENT_ID, "format": "json", "limit": 3,
          "tags": "rock", "audioformat": "mp32", "type": "single albumtrack"}),
        ("deezer", "https://api.deezer.com/search",
         {"q": "Nirvana", "limit": 3}),
    ]
    # Exactly one metadata request per service; MusicBrainz stays below 1 req/s.
    with ThreadPoolExecutor(max_workers=4) as executor:
        results = list(executor.map(lambda item: probe(*item), targets))
    report = {
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "purpose": "public metadata feasibility, not playback or catalog equivalence",
        "credentials_from_user_accessed": False,
        "app_integrations_implemented": False,
        "results": results,
        "limits": ["No playback, audible quality or mobile performance test",
                   "Search names are not proof of artist identity",
                   "A URL or streamability field is not proof of playback authorization",
                   "No exclusivity or cross-provider recording match inferred",
                   "Public HTTP availability does not grant production API access"],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

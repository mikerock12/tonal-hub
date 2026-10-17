"""Read public metadata only to investigate artist ranking and live coverage."""
import json
import time
import urllib.parse
import urllib.request
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cases = [
    ("lma-veruca", "https://archive.org/advancedsearch.php", {"q": 'collection:etree AND (creator:"Veruca Salt" OR title:"Veruca Salt")', "output": "json", "rows": 5, "fl[]": "identifier,title,creator,date"}),
    ("lma-goose", "https://archive.org/advancedsearch.php", {"q": 'collection:etree AND (creator:"Goose" OR title:"Goose")', "output": "json", "rows": 5, "fl[]": "identifier,title,creator,date"}),
    ("mb-veruca", "https://musicbrainz.org/ws/2/artist", {"query": 'artist:"Veruca Salt"', "fmt": "json", "limit": 20}),
]
results = []
for name, base, args in cases:
    url = base + "?" + urllib.parse.urlencode(args)
    entry = {"case": name, "url": url}
    for attempt in range(3):
        try:
            result = subprocess.run(["C:/Windows/System32/curl.exe", "--silent", "--show-error", "--max-time", "25", "--user-agent", "IndieTonal/0.1.1 (https://github.com/natheus/tonal-player)", "--header", "Accept: application/json", "--write-out", "\n%{http_code}", url], capture_output=True, text=True, timeout=30, check=True)
            body, status = result.stdout.rsplit("\n", 1)
            if int(status) != 200:
                raise RuntimeError("HTTP " + status)
            entry.update(status=int(status), data=json.loads(body))
            entry.pop("error", None)
            break
        except Exception as error:
            entry["error"] = str(error)
            time.sleep(3 + attempt * 2)
    results.append(entry)
    print(name, entry.get("status"), entry.get("data", {}).get("response", {}).get("numFound"), entry.get("error", ""))
    time.sleep(2)
path = root / "out/research/indie-search-probe.json"
path.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")

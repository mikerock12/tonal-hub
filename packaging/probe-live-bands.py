"""Probe public LMA coverage; distinguish a named artist from title mentions."""
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time
import unicodedata
from urllib.parse import urlencode, quote

ROOT = Path(__file__).resolve().parents[1]
BANDS = ["Nirvana", "Alice In Chains", "Metallica", "Soundgarden", "Green Day", "Ramones", "The Distillers", "Veruca Salt", "Dinosaur Jr.", "King Gizzard & The Lizard Wizard", "Goose"]


def get_json(url):
    for attempt in range(3):
        run = subprocess.run(["C:/Windows/System32/curl.exe", "--silent", "--show-error", "--max-time", "25", "--user-agent", "IndieTonal/0.1.1 (https://github.com/natheus/tonal-player)", "--header", "Accept: application/json", "--write-out", "\n%{http_code}", url], capture_output=True, text=True, timeout=30)
        if run.returncode == 0:
            body, status = run.stdout.rsplit("\n", 1)
            if status == "200":
                return json.loads(body)
        if attempt == 2:
            raise RuntimeError(run.stderr.strip() or ("HTTP " + status))
        time.sleep(3 + attempt * 2)


def normalized(value):
    return " ".join("".join(ch.lower() if ch.isalnum() else " " for ch in unicodedata.normalize("NFKD", str(value)) if not unicodedata.combining(ch)).split())


def main():
    report = {"recorded_at_utc": datetime.now(timezone.utc).isoformat(), "source": "Internet Archive collection:etree (Live Music Archive)", "spotify_credentials_read": False, "bands": []}
    path = ROOT / "out/research/live-bands-probe.json"
    for band in BANDS:
        url = "https://archive.org/advancedsearch.php?" + urlencode({"q": f'collection:etree AND mediatype:etree AND (creator:"{band}" OR title:"{band}")', "output": "json", "rows": 20, "sort[]": "downloads desc", "fl[]": "identifier,title,creator,date,mediatype"})
        result = {"band": band, "query_url": url}
        try:
            response = get_json(url)["response"]
            docs = response.get("docs", [])
            result.update(total_query_hits=response["numFound"], sampled_results=docs)
            exact = []
            for doc in docs:
                creators = doc.get("creator", [])
                if not isinstance(creators, list): creators = [creators]
                if any(normalized(value) == normalized(band) for value in creators):
                    exact.append(doc)
            result["exact_creator_hits_in_sample"] = len(exact)
            checks = []
            # Metadata inspection only. No assumption that every catalog hit plays.
            for doc in exact[:2]:
                time.sleep(1.1)
                metadata = get_json("https://archive.org/metadata/" + quote(doc["identifier"], safe=""))
                restricted = bool(metadata.get("is_dark")) or str(metadata.get("metadata", {}).get("access-restricted-item", "")).lower() == "true"
                files = [f for f in metadata.get("files", []) if f.get("format") in ("MP3", "VBR MP3") and not f.get("private")]
                checks.append({"id": doc["identifier"], "restricted": restricted, "public_mp3_files": 0 if restricted else len(files)})
            result["metadata_checks"] = checks
        except Exception as error:
            result["error"] = str(error)
        report["bands"].append(result)
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(band, "hits=", result.get("total_query_hits"), "exact creators in sample=", result.get("exact_creator_hits_in_sample"), "checks=", result.get("metadata_checks"), result.get("error", ""), flush=True)
        time.sleep(1.1)


if __name__ == "__main__":
    main()

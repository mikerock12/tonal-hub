"""Repair CMake's cached UTF-8 diagnostics prefix on Portuguese Windows hosts.

Returns 10 when repaired: callers must rebuild objects whose old header
dependencies Ninja could not read. Only generated files in out/ are touched.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve()
if not build.is_relative_to(root / "out"):
    raise SystemExit("Compiler cache repair is limited to this workspace's out/ directory")
changed = False
for file in (build / "CMakeFiles").glob("*/CMake*Compiler.cmake"):
    text = file.read_text(encoding="utf-8")
    def fix(match):
        global changed
        prefix = match.group(2)
        if "├" not in prefix:
            return match.group(0)
        try:
            corrected = prefix.encode("cp850").decode("utf-8")
        except UnicodeError:
            return match.group(0)
        changed = True
        return match.group(1) + corrected + match.group(3)
    result = re.sub(r'(set\(CMAKE_(?:C|CXX)_CL_SHOWINCLUDES_PREFIX ")([^"\n]*)("\))', fix, text)
    if result != text:
        file.write_text(result, encoding="utf-8")
if changed:
    print("Repaired compiler diagnostics prefix; a clean rebuild is required.")
raise SystemExit(10 if changed else 0)

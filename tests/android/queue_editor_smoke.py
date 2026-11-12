"""Exercise the queue editor in the debuggable fixture on an emulator."""
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


ADB = os.environ.get("ADB", "adb")
PACKAGE = "app.tonal.android.benchmark"
ACTIVITY = PACKAGE + "/app.tonal.android.MainActivity"
OUTPUT = Path(sys.argv[1])
OUTPUT.mkdir(parents=True, exist_ok=True)
DUMP = f"/sdcard/tonal-queue-editor-{os.getpid()}.xml"


def adb(*args):
    return subprocess.check_output([ADB, *args], timeout=30)


def hierarchy():
    report = adb("shell", "uiautomator", "dump", DUMP)
    if b"dumped to" not in report:
        return []
    return list(ET.fromstring(adb("exec-out", "cat", DUMP)).iter("node"))


def matches(node, label):
    return any(node.get(key, "").casefold() == label.casefold() for key in ("text", "content-desc"))


def wait_for(*labels):
    deadline = time.monotonic() + 22
    while time.monotonic() < deadline:
        try:
            nodes = hierarchy()
        except (ET.ParseError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            nodes = []
        if all(any(matches(node, label) for node in nodes) for label in labels):
            return nodes
        time.sleep(0.3)
    screenshot("android-queue-diagnostic.png")
    (OUTPUT / "android-queue-logcat.txt").write_bytes(adb("logcat", "-d", "-s", "AndroidRuntime:E", "TonalTest:I"))
    raise AssertionError(f"Missing controls {labels}")


def center(nodes, label):
    node = next(n for n in nodes if matches(n, label))
    x1, y1, x2, y2 = map(int, re.findall(r"\d+", node.get("bounds")))
    return str((x1 + x2) // 2), str((y1 + y2) // 2)


def tap(nodes, label):
    adb("shell", "input", "tap", *center(nodes, label))


def screenshot(filename):
    (OUTPUT / filename).write_bytes(adb("exec-out", "screencap", "-p"))


def assert_order(nodes, *titles):
    heights = [int(center(nodes, title)[1]) for title in titles]
    assert heights == sorted(heights), (titles, heights)


assert adb("shell", "getprop", "ro.kernel.qemu").strip() == b"1"
adb("shell", "am", "force-stop", PACKAGE)
adb("shell", "am", "start", "-W", "-n", ACTIVITY, "--ez", "queueEditorFixture", "true",
    "--ez", "mixWithDiscovery", "true")
nodes = wait_for("Editar", "Agora · Mix Fixture 1", "Arrastar Mix Fixture 2")
assert any(node.get("text", "").startswith("7 mús. · 3 álb. · 3 art.") for node in nodes)
screenshot("android-fila-01514.png")

start = center(nodes, "Arrastar Mix Fixture 2")
end = center(nodes, "Arrastar Mix Fixture 4")
adb("shell", "input", "swipe", *start, *end, "550")
nodes = wait_for("Mix Fixture 2", "Mix Fixture 3", "Mix Fixture 4")
assert_order(nodes, "Mix Fixture 3", "Mix Fixture 4", "Mix Fixture 2")

tap(nodes, "Editar")
nodes = wait_for("Marcar Mix Fixture 4", "Marcar Mix Fixture 2", "↑ Subir")
tap(nodes, "Marcar Mix Fixture 4")
tap(nodes, "Marcar Mix Fixture 2")
nodes = wait_for("↑ Subir", "↓ Descer", "Excluir")
screenshot("android-editar-fila-01514.png")
tap(nodes, "↑ Subir")
nodes = wait_for("Mix Fixture 2", "Mix Fixture 3", "Mix Fixture 4")
assert_order(nodes, "Mix Fixture 4", "Mix Fixture 2", "Mix Fixture 3")
tap(nodes, "↓ Descer")
nodes = wait_for("Mix Fixture 2", "Mix Fixture 3", "Mix Fixture 4")
assert_order(nodes, "Mix Fixture 3", "Mix Fixture 4", "Mix Fixture 2")
tap(nodes, "Excluir")
nodes = wait_for("Mix Fixture 3", "Concluir")
assert not any(node.get("text") in ("Mix Fixture 4", "Mix Fixture 2") for node in nodes)
tap(nodes, "Concluir")
nodes = wait_for("Limpar")
tap(nodes, "Limpar")
tap(wait_for("Limpar fila?", "Cancelar"), "Limpar")
wait_for("Agora · Mix Fixture 1", "0 mús. · 0 álb. · 0 art. · 0min 0s")
print("PASS queue summary, drag, batch up/down, removal and clear with current track retained")

adb("shell", "am", "force-stop", PACKAGE)
adb("shell", "am", "start", "-W", "-n", ACTIVITY, "--ez", "queueEditorFixture", "true",
    "--ez", "queueLongFixture", "true")
wait_for("Agora · Faixa de teste 0", "120 mús. · 0 álb. · 0 art. · 6h 0min", "Editar")
print("PASS 20 observed tracks extended to 120 upcoming tracks")

#!/usr/bin/env bash
set -euo pipefail
# The action can otherwise wait indefinitely for a QEMU child after `emu kill`.
# Capture owned children while the emulator is alive, before teardown reparents
# auxiliary processes. Signal completion even on assertion failure.
port="${EMULATOR_PORT:-5554}"
marker="/tmp/tonal-emulator-${port}-finished"
test ! -e "$marker"
nohup python3 .github/scripts/stop-ci-emulator.py "$port" "$marker" \
  >/tmp/tonal-emulator-cleanup.log 2>&1 </dev/null &
trap 'touch "$marker"' EXIT
adb install android/app/build/outputs/apk/benchmark/app-benchmark.apk
adb shell pm grant app.tonal.android.benchmark android.permission.POST_NOTIFICATIONS
timeout 120 adb shell am instrument -w -e action halloweenTests \
  app.tonal.android.benchmark/app.tonal.android.DeviceInstrumentation | tee /tmp/result.txt
adb logcat -d -s AndroidRuntime:E | tail -100
adb pull /sdcard/Android/data/app.tonal.android.benchmark/files/halloween-0154 evidence || true
grep -q '"passed":true' /tmp/result.txt
python3 tests/android/queue_editor_smoke.py evidence

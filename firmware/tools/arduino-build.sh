#!/usr/bin/env bash
# Sestavení firmwaru bez PlatformIO (arduino-cli + arduino-esp32 2.0.17).
# Použití: tools/arduino-build.sh [pcf|gpio]
# Předpoklad: core v ~/Arduino/hardware/espressif/esp32, knihovny v ~/Arduino/libraries.
set -euo pipefail
VARIANT=${1:-pcf}
MODE=1; [ "$VARIANT" = gpio ] && MODE=0
FW="$(cd "$(dirname "$0")/.." && pwd)"
CLI=${ARDUINO_CLI:-arduino-cli}
WORK=${WORK:-/tmp/gogen-build}
SK="$WORK/sketch-$VARIANT/gogen_os"
rm -rf "$SK"; mkdir -p "$SK"
cp "$FW"/src/*.cpp "$FW"/src/*.h "$FW"/include/*.h "$SK"/
# prázdné .ino (arduino-cli ho vyžaduje), main.cpp zůstává C++
: > "$SK/gogen_os.ino"
"$CLI" compile --fqbn espressif:esp32:esp32:PartitionScheme=min_spiffs,PSRAM=disabled,DebugLevel=error \
  --build-property "compiler.cpp.extra_flags=-DGOGEN_INPUT_MODE=$MODE -DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=2 -Wall" \
  --build-property "tools.ctags.path=${CTAGS_DIR:-/opt/fakectags}" \
  --build-path "$WORK/build-$VARIANT" --output-dir "$WORK/out-$VARIANT" --warnings default "$SK"

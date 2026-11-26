#!/bin/sh
# Isle of Bears for the M-VAVE FM-1, on fm1-base (base/, a git submodule: clone with --recursive).
#   ./build.sh                 app/build/IsleOfBears.fwsc   (JieLi toolchain in Docker; nothing is sent to a device)
#   ./build.sh --host          the host simulator: app/build/host/isleofbears_host
#   ./build.sh --web           the browser emulator: app/build/emu/
#   ./build.sh --test          the computer's self-play, the simulator's scripts, and (with the web version beside
#                              this repository, IOB_WEB) the rules compared with it step by step
# The toolchain as fm1-base's README says: JIELI_TOOLCHAIN and AC79_SDK, or base/tools/setup_toolchain.sh.
set -e
cd "$(dirname "$0")"
ROOT=$PWD
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
if ! command -v docker >/dev/null 2>&1 && [ -x /Applications/Docker.app/Contents/Resources/bin/docker ]; then
    PATH="$PATH:/Applications/Docker.app/Contents/Resources/bin"
fi
case " $* " in
*" --test "*)
    mkdir -p tests/build
    cc -O2 -std=c99 -Wall -Wextra -Wno-unused-function -o tests/build/ai_test tests/ai_test.c
    tests/build/ai_test 100
    (cd base && python3 tools/build.py "$ROOT/app" --host) >/dev/null
    mkdir -p app/build/shots
    for t in app/tests/*.txt; do app/build/host/isleofbears_host "$t" app/build/shots | tail -1; done
    if [ -f "${IOB_WEB:-../isle-of-bears}/tools/oracle/js-engine.mjs" ]; then node tests/oracle.mjs 2000
    else echo "oracle: skipped (no web version at ${IOB_WEB:-../isle-of-bears}; set IOB_WEB)"; fi
    exit 0 ;;
esac
(cd base && python3 tools/build.py "$ROOT/app" "$@")
case " $* " in
*" --web "*) python3 tools/web_keys.py app/build/emu/index.html ;;
esac

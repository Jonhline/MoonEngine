#!/bin/sh
# Build Moon Engine on Linux, end to end.
#
#   tools/build_linux.sh [build-dir]            build + run the tests
#   tools/build_linux.sh build-native --no-tests build only
#   tools/build_linux.sh build-native --reconfigure
#
# What it does, in order — the same steps as the Linux section of
# docs/BUILDING_AND_TESTING.md, kept in one place so the list cannot drift:
#
#   1. tools/build_libav.sh linux  — the pinned in-process libav (movies and
#      custom music). Idempotent: a matching config.stamp is left alone.
#      Without these archives CMake silently falls back to the movie stub.
#   2. pick a Python 3 that can `import pycparser` — cmake/ReplayMath.cmake
#      refuses to configure without it. $MOON_PYTHON wins if set.
#   3. cmake configure (Release) and build.
#   4. ctest, exactly as on Windows.
#
# The engine is ./<build-dir>/MoonEngine; pass a disc image as the argument.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=build-native
RECONFIGURE=0
TESTS=1

for arg in "$@"; do
    case "$arg" in
        --no-tests)    TESTS=0 ;;
        --reconfigure) RECONFIGURE=1 ;;
        -h|--help)     sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)
            echo "build_linux: unknown option $arg" >&2
            exit 2 ;;
        *) BUILD=$arg ;;
    esac
done

cd "$ROOT"

# 1. libav archives. CMake looks for build/libav/linux/lib/libav*.a and turns
#    MOON_LINKED_MOVIES on only when every archive is there.
echo "=== libav (linux) ==="
./tools/build_libav.sh linux

# 2. a Python 3 with pycparser (the replay-math generator needs it).
PYTHON=
for candidate in ${MOON_PYTHON:-} python3 /usr/bin/python3 /usr/local/bin/python3; do
    [ -n "$candidate" ] || continue
    command -v "$candidate" >/dev/null 2>&1 || continue
    if "$candidate" -c 'import pycparser' >/dev/null 2>&1; then
        PYTHON=$candidate
        break
    fi
done
if [ -z "$PYTHON" ]; then
    echo "build_linux: no python3 with pycparser found." >&2
    echo "             Install it (python3 -m pip install pycparser==3.0)," >&2
    echo "             or set MOON_PYTHON=/path/to/python3." >&2
    exit 1
fi
echo "=== python: $PYTHON (pycparser $("$PYTHON" -c 'import pycparser; print(pycparser.__version__)')) ==="

# 3. configure + build.
if [ "$RECONFIGURE" = 1 ] || [ ! -f "$BUILD/CMakeCache.txt" ]; then
    echo "=== configure: $BUILD ==="
    # Ninja when present: CMake 4 refuses Makefiles for the Vulkan headers' C++ module target.
    GENERATOR=
    if command -v ninja >/dev/null 2>&1; then GENERATOR="-G Ninja"; fi
    [ "$RECONFIGURE" = 1 ] && rm -rf "$BUILD"
    # shellcheck disable=SC2086
    cmake -S . -B "$BUILD" $GENERATOR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DPython3_EXECUTABLE="$PYTHON"
else
    echo "=== configure: reusing $BUILD (--reconfigure to start over) ==="
fi
echo "=== build: $BUILD ==="
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"

# 4. tests. The GL/Vulkan cases need a display: on a headless host they fail with
#    "Vulkan couldn't find a predefined mode" / "Could not create GL context".
#    Xvfb + software Mesa is enough for them.
if [ "$TESTS" = 1 ]; then
    echo "=== tests ==="
    JOBS=$(nproc 2>/dev/null || echo 4)
    if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
        echo "    (headless: running under xvfb)"
        xvfb-run -a --server-args='-screen 0 1280x720x24' \
            ctest --test-dir "$BUILD" -j"$JOBS" --output-on-failure
    else
        ctest --test-dir "$BUILD" -j"$JOBS" --output-on-failure
    fi
fi

echo "=== done: $ROOT/$BUILD/MoonEngine ==="

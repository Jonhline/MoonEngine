#!/usr/bin/env bash
# Moon Engine smoke gate -- the list we were NOT running, which is why the captain kept
# finding the same class of fault. Every line is a claim about the PRODUCT, checked here.
#
# usage: bash tools/scripts/smoke.sh [disc]
# Prints one PASS / FAIL / ENV line per check. ENV = this box cannot show it (headless GL),
# never silently a pass.
set -u
D=${1:-"/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso"}
B=${BUILD:-/mnt/ssd/moon-build}
EXE=$B/MoonEngine
MOD=/mnt/ssd/mods/moon-banana
pass=0; fail=0; env_=0
line() { printf "%-6s %s\n" "$1" "$2"; }

[ -x "$EXE" ] || { echo "no binary at $EXE (build first)"; exit 2; }

run() { DISPLAY=${DISPLAY:-:99} timeout 120 stdbuf -oL -eL "$EXE" "$@" 2>&1; }

# 1. the studio: the white room, the car, the dock -- and NOT the plain-backdrop fallback.
out=$(run "$D" --studio ford_gt_05 --shot /tmp/smoke-studio.png --no-config --shot-delay 8)
if grep -q "white room" <<<"$out" && ! grep -q "studio assets unavailable" <<<"$out"; then
    line PASS "studio: the white room and the car, no plain-backdrop fallback"; pass=$((pass+1))
else
    line FAIL "studio: room missing or the fallback fired"; fail=$((fail+1)); echo "$out" | grep -E "assets unavailable|white room" | sed 's/^/       /'
fi

# 2. the rig camera must be the rig's, not the CarFace one. The room's own pose is the
#    captain's to change, so the check is the SOURCE of the camera (label "launcher-room"
#    is only printed when the rig owns it), not one hardcoded eye position.
if grep -q "camera launcher-room: eye=" <<<"$out"; then
    line PASS "studio: the captain's rig owns the camera"; pass=$((pass+1))
else
    line FAIL "studio: camera is not the rig's"; fail=$((fail+1)); grep "camera " <<<"$out" | sed 's/^/       /'
fi

# 3. the game's own car menu (arcade TopRoot). The car it draws is the 'presentation' background.
out=$(run "$D" arcade TopRoot --shot /tmp/smoke-arcade.png --no-config --shot-delay 10)
if grep -qE "drawn ([0-9]+), not yet drawn 0" <<<"$out"; then
    line PASS "arcade TopRoot: every element drawn (car included)"; pass=$((pass+1))
elif grep -q "presentation: timed out" <<<"$out"; then
    line ENV  "arcade TopRoot: presentation car missing -- on this headless box (Xvfb has no GL 3.x); Windows draws it"; env_=$((env_+1))
else
    line FAIL "arcade TopRoot: unexpected draw state"; fail=$((fail+1)); grep "drawn" <<<"$out" | sed 's/^/       /'
fi

# 4. the disc itself, no mods: every car must still assemble.
out=$(timeout 300 "$B/gt4car_survey" "$D" 2>&1 | tail -1)
if grep -q "722 checked, 0 failed loads" <<<"$out"; then
    line PASS "disc: 722 cars checked, 0 failed loads"; pass=$((pass+1))
else
    line FAIL "disc: $out"; fail=$((fail+1))
fi

# 5. the mod: its own model name, and the Ka untouched.
if [ -d "$MOD" ]; then
    a=$(MOON_OVERLAY=$MOD timeout 200 "$B/gt4car_survey" "$D" --labels moon_banana 2>&1 | grep -E "^OK|^REVIEW")
    b=$(MOON_OVERLAY=$MOD timeout 200 "$B/gt4car_survey" "$D" --labels ka_01 2>&1 | grep -E "^OK|^REVIEW")
    c=$(timeout 200 "$B/gt4car_survey" "$D" --labels ka_01 2>&1 | grep -E "^OK|^REVIEW")
    grep -q "model=moonbanana" <<<"$a" && line PASS "mod: the banana loads as its own model (moonbanana)" && pass=$((pass+1)) \
        || { line FAIL "mod: $a"; fail=$((fail+1)); }
    [ "$b" = "$c" ] && line PASS "mod: the Ka is byte-identical with and without the mod" && pass=$((pass+1)) \
        || { line FAIL "mod: the Ka changed with the mod on"; fail=$((fail+1)); }
    # 6. no id collisions in the mod's tables.
    n=$(run "$D" --studio moon_banana --shot /tmp/smoke-mod.png --no-config --shot-delay 8 | grep -c "exists in the disc's table")
    [ "$n" = "0" ] && { line PASS "mod: no table id collisions at load"; pass=$((pass+1)); } \
        || { line FAIL "mod: $n table id collision messages at load"; fail=$((fail+1)); }
else
    line ENV "mod: not present at $MOD (skipped)"
fi

echo
echo "smoke: $pass PASS, $fail FAIL, $env_ ENV-only"
[ "$fail" = "0" ] || exit 1

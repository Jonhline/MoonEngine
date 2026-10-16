#!/usr/bin/env bash
# Gate for "ADD a car": the engine's own survey walks the catalog. Baseline 722,
# with the mod folder on 723 and the added label loading a real model.
set -u
B=/mnt/ssd/vulcan4-build
D="/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso"
MOD=/mnt/ssd/mods/moon-first-car
rm -rf "$MOD"

echo "=== baseline: no mod source ==="
env -u OPENGT4_OVERLAY "$B/gt4car_survey" "$D" > /tmp/survey_base.txt 2>&1
echo "BASE_EXIT=$?"
tail -1 /tmp/survey_base.txt

echo "=== authoring the mod folder from the disc ==="
"$B/moonmod_addcar" "$D" ford_gt_no_stripe_05 moon_first_added_car "$MOD"
echo "AUTHOR_EXIT=$?"
find "$MOD" -type f -printf '%p %s bytes\n'

echo "=== the engine, with the mod folder on ==="
OPENGT4_OVERLAY="$MOD" "$B/gt4car_survey" "$D" > /tmp/survey_mod.txt 2>&1
echo "MOD_EXIT=$?"
tail -1 /tmp/survey_mod.txt

echo "=== the added car, as the engine loaded it ==="
grep -n "moon_first_added_car" /tmp/survey_mod.txt
echo "=== counts ==="
printf 'baseline lines: %s\n' "$(grep -c 'model=' /tmp/survey_base.txt)"
printf 'modded   lines: %s\n' "$(grep -c 'model=' /tmp/survey_mod.txt)"
printf 'baseline FAIL: %s\n' "$(grep -c '^FAIL' /tmp/survey_base.txt)"
printf 'modded   FAIL: %s\n' "$(grep -c '^FAIL' /tmp/survey_mod.txt)"

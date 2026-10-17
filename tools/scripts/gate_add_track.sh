#!/usr/bin/env bash
# Gate for "ADD a course": the engine reads a course whose label is not on the
# disc, and resolves that label through the merged COURSE.idi/COURSE.dbt.
set -u
B=/mnt/ssd/vulcan4-build
D="/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso"
SRC=20r60r
NEW=moon_first_added_track
MOD=/mnt/ssd/mods/moon-first-track
rm -rf "$MOD"

echo "=== baseline: the disc's own course ==="
"$B/gt4course" "$D" "$SRC" > /tmp/course_base.txt 2>&1
echo "BASE_EXIT=$?"
head -3 /tmp/course_base.txt

echo "=== authoring the mod folder from the disc ==="
"$B/moonmod_addtrack" "$D" "$SRC" "$NEW" "$MOD"
echo "AUTHOR_EXIT=$?"
find "$MOD" -type f -printf '%p %s bytes\n'

echo "=== the engine, reading the added label with the mod folder on ==="
MOON_OVERLAY="$MOD" "$B/gt4course" "$D" "$NEW" > /tmp/course_mod.txt 2>&1
echo "MOD_EXIT=$?"
head -3 /tmp/course_mod.txt

echo "=== the two courses, by the engine's own reader ==="
sed -E "s#^/crs/[^:]+:#/crs/<label>:#" /tmp/course_base.txt > /tmp/course_base.norm
sed -E "s#^/crs/[^:]+:#/crs/<label>:#" /tmp/course_mod.txt  > /tmp/course_mod.norm
if diff -q /tmp/course_base.norm /tmp/course_mod.norm >/dev/null; then
  echo "IDENTICAL: the added label loads the same course, byte for byte, from the mod folder"
else
  echo "DIFFERENT:"
  diff /tmp/course_base.norm /tmp/course_mod.norm | head -10
fi

echo "=== the merged COURSE.idi answers for a label the disc does not have ==="
MOON_OVERLAY="$MOD" "$B/moonmod_addtrack" "$D" "$NEW" moon_second_added_track /tmp/moon-second-track 2>&1 | head -4

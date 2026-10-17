#!/usr/bin/env bash
# sync-check.sh -- the gate that stops us building a tree that is missing a teammate's work.
#
# WHY THIS EXISTS (the captain, 2026-10-17): we shipped a Windows build to Chiyo while our tree was
# 2 commits BEHIND Jonhline -- those 2 commits were his entire Vulkan renderer. We had fetched them at
# 12:20 and never carried them into the working tree, so the build was "the newest of our copy" and
# missing the newest of the project. Boss: "we work together as a team. so we have to work with
# eachothers newest code."
#
# It answers three questions and fails if any of them is a problem:
#   1. does the working tree contain everything upstream has?   (behind == 0)
#   2. is anything of ours unpublished?                         (warns; the captain decides when to push)
#   3. what exactly is missing / unpublished?                   (subjects, so the number is never bare)
#
# Exit codes: 0 = safe to build, 1 = behind upstream (DO NOT BUILD), 2 = fetch failed (unknown state).
set -uo pipefail
REMOTE="${REMOTE:-origin}"
BRANCH="${BRANCH:-main}"
cd "$(git rev-parse --show-toplevel)" || exit 2

if ! git fetch -q "$REMOTE" "$BRANCH" 2>/dev/null; then
    echo "sync-check: FETCH FAILED -- cannot tell if we are up to date. Do not build on a guess."
    exit 2
fi

read -r ahead behind < <(git rev-list --left-right --count "HEAD...$REMOTE/$BRANCH")
echo "sync-check: local $(git rev-parse --short HEAD)  upstream $REMOTE/$BRANCH $(git rev-parse --short "$REMOTE/$BRANCH")"
echo "sync-check: $ahead ours not upstream, $behind upstream not ours"

if [ "$behind" -gt 0 ]; then
    echo
    echo "sync-check: BLOCKED -- $behind commit(s) upstream are NOT in our working tree:"
    git log --oneline --format='    %h %ci %an  %s' "HEAD..$REMOTE/$BRANCH"
    echo
    echo "  Carry them first (merge $REMOTE/$BRANCH), verify, THEN build."
    exit 1
fi

if [ "$ahead" -gt 0 ]; then
    echo
    echo "sync-check: NOTE -- $ahead commit(s) of ours are NOT upstream. He is working on old code:"
    git log --oneline --format='    %h %ci %s' "$REMOTE/$BRANCH..HEAD" | head -20
    echo "  Publishing ours is what keeps HIS copy new. The captain decides when."
fi

echo "sync-check: OK -- the working tree contains everything upstream has."
exit 0

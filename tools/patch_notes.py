#!/usr/bin/env python3
"""Turn a range of commits into Discord-ready patch notes.

    python3 tools/patch_notes.py                      # since the last tag, up to HEAD
    python3 tools/patch_notes.py --from v0.2.0-alpha  # explicit range
    python3 tools/patch_notes.py --post               # also post to DISCORD_WEBHOOK_URL
    python3 tools/patch_notes.py --dry                # print only (default)

No AI attribution: the notes are the commits' own words, grouped by area.
"""

import argparse
import datetime as dt
import json
import os
import re
import subprocess
import sys
import urllib.request

AREAS = [
    ("android", r"^(android|platform/android)\b[:\s]\s*"),
    ("linux", r"^(linux|platform/linux)\b[:\s]\s*"),
    ("windows", r"^(windows|win32)\b[:\s]\s*"),
    ("launcher", r"^(launcher|ui)\b[:\s]\s*"),
    ("physics", r"^(physics|sim)\b[:\s]\s*"),
    ("rendering", r"^(render|renderer|vulkan|opengl|gfx)\b[:\s]\s*"),
    ("audio", r"^(audio|music|sound)\b[:\s]\s*"),
    ("docs", r"^docs?\b[:\s]\s*"),
    ("build", r"^(build|cmake|ci|tools)\b[:\s]\s*"),
]
TITLES = {
    "android": "Android", "linux": "Linux", "windows": "Windows", "launcher": "Launcher",
    "physics": "Physics", "rendering": "Rendering", "audio": "Audio", "docs": "Docs",
    "build": "Build & tooling", "general": "General",
}
DISCORD_LIMIT = 1900
ANCHOR = os.path.join(".git", "patch-notes-last-sha")

# commits that say nothing a player would read
NOISE = re.compile(
    r"^(update to the current development state"
    r"|merge\b|wip\b|bump\b|chore\b|sync\b|checkpoint\b|misc\b|cleanup\b"
    r"|fix typo\b|typo\b|formatting\b|revert\b)",
    re.I)



def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout


def last_tag():
    try:
        return git("describe", "--tags", "--abbrev=0").strip()
    except subprocess.CalledProcessError:
        return ""


def commits(since, until):
    rng = f"{since}..{until}" if since else until
    raw = git("log", "--no-merges", "--reverse", "--pretty=%h%x1f%s%x1f%b%x1e", rng)
    out, seen = [], set()
    for chunk in raw.split("\x1e"):
        chunk = chunk.strip("\n")
        if not chunk:
            continue
        parts = chunk.split("\x1f")
        if len(parts) < 2:
            continue
        sha, subject = parts[0].strip(), parts[1].strip()
        body = parts[2].strip() if len(parts) > 2 else ""
        if NOISE.match(subject) or subject.lower() in seen:
            continue
        seen.add(subject.lower())
        out.append({"sha": sha, "subject": subject, "body": body})
    return out


def anchor():
    """The last commit already turned into notes, if we have posted before."""
    try:
        with open(ANCHOR) as f:
            return f.read().strip()
    except FileNotFoundError:
        return ""


def set_anchor(sha):
    with open(ANCHOR, "w") as f:
        f.write(sha + "\n")


def classify(subject):
    low = subject.lower()
    for area, pat in AREAS:
        m = re.match(pat, low)
        if m:
            return area, subject[m.end():].strip() or subject
    return "general", subject


def clean(text):
    text = re.sub(r"\s+", " ", text).strip()
    return text[:1].upper() + text[1:] if text else text


def build(commits_, title):
    groups = {}
    for c in commits_:
        area, subj = classify(c["subject"])
        groups.setdefault(area, []).append((subj, c["sha"]))

    order = [a for a, _ in AREAS] + ["general"]
    lines = [f"**{title}**", ""]
    for area in order:
        if area not in groups:
            continue
        lines.append(f"**{TITLES[area]}**")
        for subj, sha in groups[area]:
            lines.append(f"• {clean(subj)} (`{sha}`)")
        lines.append("")
    lines.append(f"_{len(commits_)} change{'s' if len(commits_) != 1 else ''} · "
                 f"{dt.date.today().isoformat()}_")
    return "\n".join(lines).strip()


def chunks(text, limit=DISCORD_LIMIT):
    if len(text) <= limit:
        return [text]
    out, cur = [], ""
    for line in text.splitlines():
        if len(cur) + len(line) + 1 > limit:
            out.append(cur.rstrip())
            cur = ""
        cur += line + "\n"
    if cur.strip():
        out.append(cur.rstrip())
    return out


def post(webhook, text):
    for i, part in enumerate(chunks(text), 1):
        payload = json.dumps({"content": part, "allowed_mentions": {"parse": []}}).encode()
        req = urllib.request.Request(webhook, data=payload, method="POST",
                                     headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=30) as r:
            print(f"posted part {i}: HTTP {r.status}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="since", default=None,
                    help="start ref; default = last posted anchor, else origin/main, else the last tag")
    ap.add_argument("--to", dest="until", default="HEAD")
    ap.add_argument("--title", default=None)
    ap.add_argument("--post", action="store_true", help="post to DISCORD_WEBHOOK_URL")
    ap.add_argument("--mark", action="store_true", help="remember HEAD as posted, even without --post")
    a = ap.parse_args()

    since = a.since
    if since is None:
        since = anchor()
        if not since:
            try:
                since = git("rev-parse", "--verify", "--quiet", "origin/main").strip()
            except subprocess.CalledProcessError:
                since = last_tag()
    if since:
        print(f"# range: {since[:12]}..{a.until}", file=sys.stderr)

    cs = commits(since, a.until)
    if not cs:
        print("no commits in range — nothing to say", file=sys.stderr)
        return 1

    version = ""
    try:
        version = git("describe", "--tags", "--always").strip()
    except subprocess.CalledProcessError:
        pass
    title = a.title or (f"Moon Engine — patch notes · {version}" if version
                        else "Moon Engine — patch notes")
    text = build(cs, title)
    print(text)

    if a.post:
        hook = os.environ.get("DISCORD_WEBHOOK_URL", "").strip()
        if not hook:
            print("\nDISCORD_WEBHOOK_URL is not set — printed instead of posted", file=sys.stderr)
            return 2
        post(hook, text)
        set_anchor(git("rev-parse", a.until).strip())
    elif a.mark:
        set_anchor(git("rev-parse", a.until).strip())
        print("# anchor moved", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

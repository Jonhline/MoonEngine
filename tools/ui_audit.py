#!/usr/bin/env python3
"""UI 1:1 audit: what the disc's menu projects use that the engine never names.

Usage: python tools/ui_audit.py <dir with projects/<name>/*.mproject,*.adc> <out.md>
(the folder is extracted from the user's own disc with gt4vol; nothing of it is
written to the report beyond names and counts).

Three lists, each a static lower bound, not proof of support:
  1. widget classes in the .mproject trees,
  2. widget properties (the per-class keys),
  3. engine names the Adhoc scripts reach (attribute/variable/static names that no
     script defines and no widget is called),
and for each the projects that use it and whether the name appears as a quoted
string anywhere in src/. A name the source never mentions is not handled by name;
it may still be covered by a generic path, so every hit needs a look.
"""
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def source_strings():
    names = set()
    for d, _, files in os.walk(os.path.join(ROOT, 'src')):
        for f in files:
            if f.endswith(('.c', '.h', '.inc', '.cpp')):
                text = open(os.path.join(d, f), encoding='utf-8', errors='replace').read()
                names.update(re.findall(r'"((?:[^"\\]|\\.)*)"', text))
    split = set()
    for s in names:  # "a.b" / "name:" / "x y" tables also count for each part
        split.update(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', s))
    return names, split


def parse_mproject(path, classes, props, where):
    proj = os.path.basename(os.path.dirname(path))
    stack = []
    root = '?'
    lines = open(path, encoding='latin-1').read().splitlines()
    for i, line in enumerate(lines):
        if line.strip() == 'RootWindow {' and i + 1 < len(lines):
            m = re.search(r'name string \{"([^"]*)"\}', lines[i + 1])
            root = m.group(1) if m else '?'

        t = line.strip()
        if t == '}':
            if stack:
                stack.pop()
            continue
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\s*(\[\d+\])?\s*\{\s*$', t)
        if m:  # a class block (Widget {) or a list (children [3] {)
            name = m.group(1)
            if name[0].isupper() and not m.group(2):
                classes[name].add(proj)
                where[name].append(f'{proj}::{root}')
                stack.append(name)
            else:
                if stack and stack[-1][0].isupper():
                    props[(stack[-1], name)].add(proj)
                    where[(stack[-1], name)].append(f'{proj}::{root}')
                stack.append(name)
            continue
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\s+([a-z]+)\s*(\[\d+\])?\s*\{', t)
        if m:
            if stack and stack[-1][0].isupper():
                props[(stack[-1], m.group(1))].add(proj)
                where[(stack[-1], m.group(1))].append(f'{proj}::{root}')
            if not t.endswith('}'):
                stack.append(m.group(1))


def widget_names(path, out):
    for m in re.finditer(r'name string \{"([^"]*)"\}', open(path, encoding='latin-1').read()):
        out.add(m.group(1))


def adc_names(path, used, defined):
    proj = os.path.basename(os.path.dirname(path))
    if os.path.samefile(os.path.dirname(path), sys.argv[1]):  # a script beside the projects
        proj = os.path.splitext(os.path.basename(path))[0]
    dis = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'adhoc_dis.py'), path],
                         capture_output=True, text=True, encoding='utf-8', errors='replace').stdout
    for line in dis.splitlines():
        m = re.search(r"\b(FUNCTION_DEFINE|METHOD_DEFINE|CLASS_DEFINE|MODULE_DEFINE|STATIC_DEFINE|"
                      r"ATTRIBUTE_DEFINE|VARIABLE_PUSH|ATTRIBUTE_PUSH|IMPORT|STATIC_PUSH|LIST_ASSIGN)\s+(.*)$", line)
        if not m:
            continue
        op, rest = m.group(1), m.group(2)
        toks = re.findall(r"'([^']*)'|([A-Za-z_][A-Za-z0-9_]*)", rest)
        names = [a or b for a, b in toks]
        if not names:
            continue
        if op.endswith('_DEFINE'):
            defined.add(names[0])  # CLASS_DEFINE 'X' Base: only X is defined
            if op == 'CLASS_DEFINE':
                used[names[-1]].add(proj) if len(names) > 1 else None
        elif op == 'VARIABLE_PUSH':
            # "VARIABLE_PUSH a::b::a::b slot": a path (a global or an import) is a::b, doubled;
            # a plain name is a local or a parameter
            path = rest.split()[0]
            if '::' in path:
                parts = path.split('::')
                for n in parts[:max(1, len(parts) // 2)]:
                    used[n].add(proj)
            else:
                defined.add(path)
        elif op == 'IMPORT':
            for n in names:
                used[n].add(proj)
        else:
            used[names[0]].add(proj)


def original_readers():
    """name -> the original functions that load the name's string (lui/addiu), when the
    user's local extracted/core.bin and splat/asm/core/text.s exist; addresses only."""
    core_p = os.path.join(ROOT, 'extracted', 'core.bin')
    text_p = os.path.join(ROOT, 'splat', 'asm', 'core', 'text.s')
    if not (os.path.exists(core_p) and os.path.exists(text_p)):
        return lambda name: ''
    import bisect
    from pathlib import Path
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    from find_named_strings import BASE, references, function_starts
    core = open(core_p, 'rb').read()
    refs, starts = references(core), function_starts(Path(text_p))
    cache = {}

    def find(name):
        if name in cache:
            return cache[name]
        b, out, i = name.encode() + bytes(1), set(), -1
        while True:
            i = core.find(b, i + 1)
            if i < 0:
                break
            if i == 0 or core[i - 1] == 0:
                for r in refs.get(BASE + i, ()):
                    out.add(starts[bisect.bisect_right(starts, r) - 1])
        cache[name] = ', '.join(f'{a:08X}' for a in sorted(out)[:3])
        return cache[name]
    return find


def screens(lst, n=8):
    u = sorted(set(lst))
    return ', '.join(u[:n]) + (f' (+{len(u) - n})' if len(u) > n else '')


def main():
    src, out = sys.argv[1], sys.argv[2]
    quoted, words = source_strings()
    reader = original_readers()
    classes, props, where = defaultdict(set), defaultdict(set), defaultdict(list)
    used, defined, wnames = defaultdict(set), set(), set()
    for d, _, files in os.walk(src):
        for f in sorted(files):
            p = os.path.join(d, f)
            if f.endswith('.mproject'):
                parse_mproject(p, classes, props, where)
                widget_names(p, wnames)
            elif f.endswith('.adc'):
                adc_names(p, used, defined)

    known = lambda n: n in quoted or n in words
    lines = ['# UI audit: names the disc uses that the engine never names', '',
             'Generated by `tools/ui_audit.py` from the disc\'s menu projects. A name missing from the source is not',
             'handled by name; a generic path may still cover it. Counts are projects using it.', '']

    miss_c = sorted((c for c in classes if not known(c)), key=lambda c: (-len(classes[c]), c))
    lines += ['## 1. Widget classes', '',
              f'{len(classes)} classes in the trees; {len(miss_c)} never named in src/.', '']
    lines += ['| class | uses | original reader | screens |', '|---|---|---|---|']
    lines += [f'| `{c}` | {len(where[c])} | {reader(c)} | {screens(where[c])} |' for c in miss_c] or ['| (none) | | | |']
    lines += ['', '<details><summary>all classes</summary>', '', ', '.join(f'`{c}`' for c in sorted(classes)), '', '</details>', '']

    miss_p = sorted((k for k in props if not known(k[1])), key=lambda k: (k[0], k[1]))
    lines += ['## 2. Widget properties', '',
              f'{len(props)} class/property pairs; {len(miss_p)} property names never named in src/.', '',
              '| class | property | uses | original reader | screens |', '|---|---|---|---|---|']
    lines += [f'| `{c}` | `{p}` | {len(where[(c, p)])} | {reader(p)} | {screens(where[(c, p)])} |' for c, p in miss_p
              if p != 'has_script'] or ['| (none) | | | | |']
    lines += ['']

    eng = {n: ps for n, ps in used.items() if n not in defined and n not in wnames}
    miss_e = [n for n in eng if not known(n)]
    # projects whose whole mode is out of scope (FIDELITY 4.4: LAN, online, EyeToy, printing;
    # event is the LAN battle setup, message the online messenger)
    out_scope = {'network', 'online', 'message', 'eyetoy', 'print', 'event'}
    # natives whose original module is an out-of-scope device or service (the module's own
    # registration function): network/LAN 001E9A40, HTTP 001D8398, XML 0026C830/00209F90,
    # printer 001991D8, EyeToy 001B6B10/001BA6A0/001B8D18
    off_modules = {'001E9A40', '001D8398', '0026C830', '00209F90', '001991D8', '001B6B10', '001BA6A0', '001B8D18'}
    native = [n for n in miss_e if reader(n)]
    off_name = lambda n: n.startswith('LAN_') or set(reader(n).split(', ')) <= off_modules
    in_scope = sorted((n for n in native if eng[n] - out_scope and not off_name(n)),
                      key=lambda n: (-len(eng[n] - out_scope), n))
    off = sorted(n for n in native if n not in in_scope)
    script_side = sorted(n for n in miss_e if not reader(n))
    lines += ['## 3. Engine names reached by the scripts', '',
              f'{len(eng)} names no script defines and no widget is called; {len(miss_e)} never named in src/:',
              f'{len(in_scope)} the original executable registers and an in-scope screen uses, {len(off)} only out-of-scope',
              f'modes use, {len(script_side)} with no native reader (attributes the scripts create at run time).', '',
              '### 3.1 In scope: the original has a native, the port never names it', '',
              '| name | original reader | projects |', '|---|---|---|']
    lines += [f'| `{n}` | {reader(n)} | {", ".join(sorted(eng[n]))} |' for n in in_scope] or ['| (none) | | |']
    lines += ['', f'<details><summary>3.2 Only out-of-scope modes ({len(off)})</summary>', '',
              ', '.join(f'`{n}`' for n in off), '', '</details>', '',
              f'<details><summary>3.3 No native reader ({len(script_side)})</summary>', '',
              ', '.join(f'`{n}`' for n in script_side), '', '</details>']
    open(out, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print(f'classes {len(classes)} (missing {len(miss_c)}), properties {len(props)} (missing {len(miss_p)}), '
          f'engine names {len(eng)} (missing {len(miss_e)}, in scope {len(in_scope)}) -> {out}')


if __name__ == '__main__':
    main()

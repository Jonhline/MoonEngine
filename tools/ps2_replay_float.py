#!/usr/bin/env python3
"""Build PS2-recording arithmetic from the project's native C physics.

Only our source is processed. This does not read a disc, executable or disassembly.
Integer expressions and memory accesses remain native C. Float operators call the
existing hardware-derived integer model in every mode. Native GT4R v1 recordings
retain their old arithmetic solely for playback compatibility. Requires pycparser 3.x.
"""
import argparse
from pathlib import Path
import subprocess
from pycparser import c_ast as A, c_parser, c_generator
FAKE = r"""
#ifndef MOON_AST_LIBC
#define MOON_AST_LIBC
#define NULL ((void *)0)
#define UINT64_C(x) x##ULL
#define UINT32_C(x) x##U
#define INT64_C(x) x##LL
#define INT32_C(x) x
#define true 1
#define false 0
#define bool _Bool
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef uint64_t uintptr_t;
typedef int64_t intptr_t;
typedef uint64_t size_t;
typedef int64_t ptrdiff_t;
typedef struct MoonAstFile FILE;
typedef struct MoonAstFenv fenv_t;
float sqrtf(float);
float fabsf(float);
float fminf(float,float);
float fmaxf(float,float);
double sqrt(double);
double fabs(double);
void *memcpy(void *,const void *,size_t);
void *memset(void *,int,size_t);
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void free(void *);
#endif
"""

class Gen(c_generator.CGenerator):

    def __init__(self, a):
        super().__init__()
        self.types = {}
        self.tags = {}
        self.depth = 0
        self.count = 0
        self.emitted_tags = set()
        self.scalar_math = True
        for n in a.ext:
            if isinstance(n, A.Typedef):
                self.types[n.name] = self.typ(n.type)
            elif isinstance(n, A.Decl):
                self.types[n.name] = self.typ(n.type)
            elif isinstance(n, A.FuncDef):
                self.types[n.decl.name] = self.typ(n.decl.type)

    def typ(self, n):
        if isinstance(n, (A.TypeDecl, A.Typename, A.Decl)):
            return self.typ(n.type)
        if isinstance(n, A.IdentifierType):
            t = ' '.join(n.names)
            return self.types.get(t, t)
        if isinstance(n, (A.PtrDecl, A.ArrayDecl)):
            return ('ptr', self.typ(n.type))
        if isinstance(n, A.FuncDecl):
            return ('fn', self.typ(n.type))
        if isinstance(n, (A.Struct, A.Union)):
            key = ('struct', n.name or str(id(n)))
            if n.decls:
                self.tags[key] = {d.name: self.typ(d.type) for d in n.decls}
            return key
        if isinstance(n, A.Enum):
            return 'int'
        return '?'

    def expr(self, n):
        if n is None:
            return '?'
        if isinstance(n, A.ID):
            return self.types.get(n.name, '?')
        if isinstance(n, A.Constant):
            return n.type
        if isinstance(n, A.Cast):
            return self.typ(n.to_type)
        if isinstance(n, A.UnaryOp):
            t = self.expr(n.expr)
            if n.op == '*':
                return t[1] if isinstance(t, tuple) and t[0] == 'ptr' else '?'
            if n.op == '&':
                return ('ptr', t)
            return 'int' if n.op in ['sizeof', '!'] else t
        if isinstance(n, A.ArrayRef):
            t = self.expr(n.name)
            return t[1] if isinstance(t, tuple) and t[0] == 'ptr' else '?'
        if isinstance(n, A.StructRef):
            t = self.expr(n.name)
            if n.type == '->' and isinstance(t, tuple) and (t[0] == 'ptr'):
                t = t[1]
            return self.tags.get(t, {}).get(n.field.name, '?')
        if isinstance(n, A.FuncCall):
            t = self.expr(n.name)
            if isinstance(t, tuple) and t[0] == 'ptr':
                t = t[1]
            return t[1] if isinstance(t, tuple) and t[0] == 'fn' else '?'
        if isinstance(n, A.Assignment):
            return self.expr(n.lvalue)
        if isinstance(n, A.TernaryOp):
            return self.common(self.expr(n.iftrue), self.expr(n.iffalse))
        if isinstance(n, A.ExprList):
            return self.expr(n.exprs[-1])
        if isinstance(n, A.BinaryOp):
            if n.op in ['<', '>', '<=', '>=', '==', '!=', '&&', '||']:
                return 'int'
            return self.common(self.expr(n.left), self.expr(n.right))
        if isinstance(n, A.CompoundLiteral):
            return self.typ(n.type)
        return '?'

    def common(self, l, r):
        if 'double' in [l, r] or 'long double' in [l, r]:
            return 'double'
        if isinstance(l, tuple) and l[0] == 'ptr':
            return l
        if isinstance(r, tuple) and r[0] == 'ptr':
            return r
        return 'float' if 'float' in [l, r] else 'int'

    def visit_Struct(self, n):
        if n.name and n.decls:
            if id(n) in self.emitted_tags:
                return 'struct ' + n.name
            self.emitted_tags.add(id(n))
        return super().visit_Struct(n)

    def visit_Compound(self, n):
        keep = self.types.copy()
        s = super().visit_Compound(n)
        self.types = keep
        return s

    def visit_For(self, n):
        keep = self.types.copy()
        s = super().visit_For(n)
        self.types = keep
        return s

    def visit_FuncDef(self, n):
        keep = self.types.copy()
        self.depth += 1
        if n.decl.type.args:
            for param in n.decl.type.args.params:
                if isinstance(param, A.Decl):
                    self.types[param.name] = self.typ(param.type)
        s = super().visit_FuncDef(n)
        self.depth -= 1
        self.types = keep
        return s

    def visit_Decl(self, n, no_type=False):
        if n.name:
            self.types[n.name] = self.typ(n.type)
        return super().visit_Decl(n, no_type)

    def visit_BinaryOp(self, n):
        if self.scalar_math and self.depth and n.op in ['+', '-', '*', '/'] and (self.expr(n) == 'float'):
            self.count += 1
            op = {'+': 'add', '-': 'sub', '*': 'mul', '/': 'div'}[n.op]
            return 'gt4_replay_f_' + op + '(' + self.visit(n.left) + ', ' + self.visit(n.right) + ')'
        return super().visit_BinaryOp(n)

    def visit_Assignment(self, n):
        if self.scalar_math and self.depth and n.op in ['+=', '-=', '*=', '/='] and (self.expr(n.lvalue) == 'float') and (self.expr(n.rvalue) != 'double'):
            self.count += 1
            op = {'+=': 'add', '-=': 'sub', '*=': 'mul', '/=': 'div'}[n.op]
            return 'gt4_replay_f_' + op + '_assign(&(' + self.visit(n.lvalue) + '), ' + self.visit(n.rvalue) + ')'
        return super().visit_Assignment(n)

    def visit_FuncCall(self, n):
        if self.scalar_math and isinstance(n.name, A.ID) and n.name.name == 'sqrtf':
            self.count += 1
            return 'gt4_replay_f_sqrt(' + self.visit(n.args) + ')'
        return super().visit_FuncCall(n)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--msvc', action='store_true')
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--generated-dir', type=Path)
    parser.add_argument('--legacy', action='store_true')
    parser.add_argument('sources', nargs='+')
    args = parser.parse_args()
    root, out = (args.root.resolve(), args.out.resolve())
    out.mkdir(parents=True, exist_ok=True)
    fake = out / 'ast_include'
    fake.mkdir(exist_ok=True)
    for name in ('stdint.h', 'stddef.h', 'stdlib.h', 'stdio.h', 'string.h', 'math.h', 'fenv.h', 'stdbool.h', 'limits.h', 'float.h', 'xmmintrin.h'):
        (fake / name).write_text(FAKE, encoding='utf-8', newline='')
    generated = args.generated_dir or root / 'build/generated'
    includes = [fake, root / 'include', root / 'src/core', root / 'src/core/physics', root / 'src/app', generated]
    if args.msvc:
        flags = ['/nologo', '/E', '/X']
        if args.legacy:
            flags.append('/DMOON_LEGACY_RECORDING=1')
        flags += ['/I' + str(path) for path in includes]
    else:
        flags = ['-E', '-std=c11', '-nostdinc']
        if args.legacy:
            flags.append('-DMOON_LEGACY_RECORDING=1')
        flags += ['-I' + str(path) for path in includes]
    trees, names = ({}, [])
    for name in args.sources:
        path = (root / name).resolve()
        result = subprocess.run([args.compiler, *flags, str(path)], capture_output=True, text=True, check=True)
        ast = c_parser.CParser().parse(result.stdout)
        own = [node for node in ast.ext if node.coord and Path(node.coord.file).resolve() == path]
        own = [node for node in own if not (isinstance(node, A.FuncDef) and node.decl.name == 'gt4_phys_fpu_enter')]
        trees[path] = (ast, own)
        for node in own:
            if isinstance(node, A.FuncDef) and 'static' not in node.decl.storage:
                names.append(node.decl.name)
    prefix = '/* Generated from native project symbols; no disc code. */\n'
    if args.legacy:
        prefix += ''.join((f'#define {name} legacy_{name}\n' for name in names))
    (out / 'prefix.h').write_text(prefix, encoding='utf-8', newline='')
    for path, (ast, own) in trees.items():
        generator, chunks = (Gen(ast), [])
        generator.scalar_math = not args.legacy
        for node in own:
            if args.legacy and isinstance(node, A.Decl) and 'static' not in node.storage and (not isinstance(node.type, A.FuncDecl)) and node.name:
                node.storage = ['extern']
                node.init = None
            code = generator.visit(node)
            chunks.append(code if isinstance(node, A.FuncDef) else code + ';')
        includes = []
        for line in path.read_text(encoding='utf-8').splitlines():
            if not line.startswith('#include') or 'xmmintrin' in line:
                continue
            if '"' in line:
                name = line.split('"')[1]
                candidate = path.parent / name
                if candidate.exists():
                    line = '#include "' + candidate.resolve().as_posix() + '"'
            includes.append(line)
        code = '/* Generated PS2 arithmetic of our native C; edit its source. */\n#include "prefix.h"\n' + '\n'.join(includes) + '\n#include "replay_float.h"\n' + '\n'.join(chunks)
        (out / path.name).write_text(code, encoding='utf-8', newline='')
    print(f'{"Legacy recording" if args.legacy else "PS2 physics"} arithmetic: {len(trees)} native sources, {len(names)} functions')
if __name__ == '__main__':
    main()

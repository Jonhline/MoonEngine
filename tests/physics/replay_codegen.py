"""Check typed operator selection, order and lvalue evaluation in replay C."""
import importlib.util
from pathlib import Path
import sys
from pycparser import c_parser

source = Path(__file__).resolve().parents[2] / "tools/ps2_replay_float.py"
spec = importlib.util.spec_from_file_location("replay_codegen", source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

ast = c_parser.CParser().parse("""
typedef float scalar;
typedef struct Point { scalar x; int count; } Point;
float sqrtf(float);
int next(void);
float f(Point *p, float *values, int i, float a, float b) {
    float x = (a - b) * (a + b);
    values[next()] += p->x / sqrtf(x);
    for (int a = 0; a < 2; a++) { i = a + i; }
    { int a = 1; i = a + i; }
    p->count = i + 2;
    double wide = (double)a + b;
    return a * b + x;
}
""")
generator = module.Gen(ast)
text = generator.visit(ast)
if "--negative" in sys.argv:
    text = text.replace("gt4_replay_f_mul(a, b)", "gt4_replay_f_mul(b, a)")
expected = [
    "gt4_replay_f_mul(gt4_replay_f_sub(a, b), gt4_replay_f_add(a, b))",
    "gt4_replay_f_add_assign(&(values[next()]), gt4_replay_f_div(p->x, gt4_replay_f_sqrt(x)))",
    "p->count = i + 2",
    "i = a + i",
    "((double) a) + b",
    "gt4_replay_f_add(gt4_replay_f_mul(a, b), x)",
]
for expression in expected:
    if expression not in text:
        raise SystemExit("missing preserved expression: " + expression + "\n" + text)
assert text.count("next()") == 1, text
print("PASS replay C: typed scalar operators, operand order, scopes and single lvalue evaluation")

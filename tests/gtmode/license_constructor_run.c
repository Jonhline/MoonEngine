/* Observe the real native builder immediately before 0034D490, then run
   original and native initialization independently from that pre-state. */
#include "car.h"
static void compare_initialization(uint32_t, uint32_t, const gt4_specdb_src *, uint32_t);
#define gt4_race_init compare_initialization
#include "../../src/core/physics/race_run.c"
#undef gt4_race_init
#include "ee.h"
#include "career_license_disc.h"

ee_fn recomp_lookup(uint32_t);
typedef struct {
    const gt4_specdb_src *db;
    int bad;
} constructor_context;
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "constructor line %d: %s\n", __LINE__, #x); return 1; } } while (0)
enum { ORACLE_STACK_BOTTOM = 0x01e18000, ORACLE_STACK_TOP = 0x01ef0000 };

/* Original: func_00443F00 +0x00. SpecDB data is a shared I/O boundary. */
static void original_row(ee_ctx *e)
{
    constructor_context *context = gt4_phys_pad.ctx;
    int n = context->db->row(context->db->ctx, e->r[6].uw[0], e->r[5].uw[0], 0,
                             e->ram + e->r[7].uw[0], 256);
    e->r[2].uw[0] = n > 0;
}

/* Original: func_004458D0 +0x00. Colour row selection is shared here;
   vehicle record and parameter construction execute original instructions. */
static void original_variation(ee_ctx *e)
{
    constructor_context *context = gt4_phys_pad.ctx;
    const gt4_specdb_src *db = context->db;
    uint32_t setup = e->r[4].uw[0], out = e->r[6].uw[0];
    int colour = (int32_t)e->r[5].uw[0];
    /* Original: func_004458D0 +0x00; the serialized variation key. */
    uint32_t key = ee_r32(e, setup + 0x10), table = ee_r32(e, setup + 0x14);
    int count = 0;
    while (db->row(db->ctx, table, key, count, NULL, 0) > 0)
        count++;
    e->r[2].uw[0] = 0;
    if (colour >= count)
        return;
    for (int i = 0; i < count; i++) {
        db->row(db->ctx, table, key, i, e->ram + out, 256);
        if (ee_r32(e, out + 4) == (uint32_t)colour + 1) {
            e->r[2].uw[0] = 1;
            return;
        }
    }
}

static void presentation_boundary(ee_ctx *e)
{
    (void)e;
}

static ee_fn initialization_hook(uint32_t address)
{
    switch (address) {
    case 0x00443f00: return original_row;
    case 0x004458d0: return original_variation;
    case 0x003f71a0: return presentation_boundary;
    default: return NULL;
    }
}

static int call_initialization(ee_ctx *e, uint32_t cond, uint32_t mode)
{
    e->r[4].uw[0] = cond;
    e->r[5].uw[0] = mode;
    e->r[29].uw[0] = ORACLE_STACK_TOP;
    jmp_buf jump;
    e->exit_jmp = &jump;
    if (setjmp(jump)) {
        fprintf(stderr, "original initialization stopped %08x: %s\n", e->end_pc, e->end_detail);
        return 1;
    }
    /* Original: func_0034D490 +0x00 */
    recomp_lookup(0x0034d490)(e);
    return 0;
}

static int compare_ram(const ee_ctx *e, const uint8_t *native)
{
    if (!memcmp(e->ram, native, ORACLE_STACK_BOTTOM) &&
        !memcmp(e->ram + ORACLE_STACK_TOP, native + ORACLE_STACK_TOP, RAM_SIZE - ORACLE_STACK_TOP))
        return 0;
    for (uint32_t i = 0; i < RAM_SIZE; i++) {
        if (i == ORACLE_STACK_BOTTOM)
            i = ORACLE_STACK_TOP;
        if (native[i] != e->ram[i]) {
            fprintf(stderr, "initialization address=%08x native=%02x original=%02x\n", i,
                    native[i], e->ram[i]);
            return 1;
        }
    }
    return 1;
}

static void compare_initialization(uint32_t cond, uint32_t mode,
                                    const gt4_specdb_src *db, uint32_t scratch)
{
    constructor_context *context = gt4_phys_pad.ctx;
    context->db = db;
    uint8_t *native = gt4_phys_mem;
    ee_ctx original;
    if (ee_init(&original)) {
        context->bad = 1;
        return;
    }
    original.lookup = recomp_lookup;
    original.hook = initialization_hook;
    memcpy(original.ram, native, RAM_SIZE);
    gt4_phys_mem = original.ram;
    gt4_phys_fpu_enter();
    context->bad = call_initialization(&original, cond, mode);
    gt4_phys_mem = native;
    gt4_phys_fpu_enter();
    gt4_race_init(cond, mode, db, scratch);
    if (getenv("GT4_LICENSE_CONSTRUCTOR_NEGATIVE"))
        native[cond + CONDUCTOR_FIELD_CARS_U + CAR_PLAYER] ^= 1;
    if (!context->bad)
        context->bad = compare_ram(&original, native);
    ee_free(&original);
}

int main(int argc, char **argv)
{
    REQUIRE(argc == 3);
    gt4_career_race_lookup_data tables;
    gt4_career_license_exam exam;
    char error[256];
    REQUIRE(gt4_career_race_lookup_data_load(argv[1], &tables));
    REQUIRE(gt4_career_license_prepare(&exam, argv[1], &tables, argv[2], error, sizeof error));
    constructor_context context = {0};
    gt4_phys_pad.ctx = &context;
    gt4_race_run *run = gt4_race_run_open(argv[1], &exam.desc, error, sizeof error);
    REQUIRE(run && !context.bad);
    gt4_race_run_close(run);
    gt4_phys_pad.ctx = NULL;
    puts("PASS original/native real pre-init checkpoint; SpecDB/colour/headless boundaries");
    return 0;
}

/* Unoptimized arithmetic reference; full recurrences remain unchanged. */
#define PS2F_DISABLE_FAST_PATHS 1
#define ps2f_add reference_ps2f_add
#define ps2f_sub reference_ps2f_sub
#define ps2f_mul reference_ps2f_mul
#define ps2f_div reference_ps2f_div
#define ps2f_sqrt reference_ps2f_sqrt
#define ps2f_rsqrt reference_ps2f_rsqrt
#define ps2f_madd reference_ps2f_madd
#define ps2f_msub reference_ps2f_msub
#define ps2f_madda reference_ps2f_madda
#define ps2f_msuba reference_ps2f_msuba
#define ps2f_ercpr reference_ps2f_ercpr
#define ps2f_esqrt reference_ps2f_esqrt
#define ps2f_esqur reference_ps2f_esqur
#define ps2f_ersqrt reference_ps2f_ersqrt
#define ps2f_clip reference_ps2f_clip
#define ps2f_compare reference_ps2f_compare
#define ps2f_compare_operands reference_ps2f_compare_operands
#include "../../third_party/ps2float/ps2float.c"

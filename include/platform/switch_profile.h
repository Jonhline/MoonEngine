/* P: passive Switch CPU timing. Host builds do not evaluate diagnostic arguments. */
#ifndef OPENGT4_SWITCH_PROFILE_H
#define OPENGT4_SWITCH_PROFILE_H
#include <stdint.h>

enum swp_stage {
    SWP_PHYSICS,
    SWP_SCENE,
    SWP_GL,
    SWP_SWAP,
    SWP_AUDIO_MIX,
    SWP_AUDIO_SYNTH,
    SWP_AUDIO_CONTROL,
    SWP_DISC,
    SWP_INFLATE,
    SWP_LOADING,
    SWP_LOGIC,
    SWP_IDLE,
    SWP_STAGE_COUNT
};

#if defined(__SWITCH__) || defined(OPENGT4_SWITCH_PROFILE_TEST)
void swp_push(enum swp_stage stage);
void swp_pop(void);
void swp_frame(int width, int height);
void swp_context(const char *project, const char *page, const char *course, const char *car,
                 int driving);
void swp_load_begin(const char *name);
void swp_load_end(void);
void swp_ready(int ready);
void swp_asset_status(int ready);
#define SWP_PUSH(stage) swp_push(stage)
#define SWP_POP() swp_pop()
#define SWP_VOID(stage, expression)                                                                \
    do {                                                                                           \
        swp_push(stage);                                                                           \
        expression;                                                                                \
        swp_pop();                                                                                 \
    } while (0)
#ifdef __SWITCH__
/* GCC-only expression wrapper; preserves short-circuiting at the call site. */
#define SWP_VALUE(stage, expression)                                                               \
    ({                                                                                             \
        swp_push(stage);                                                                           \
        __auto_type swp_result = (expression);                                                     \
        swp_pop();                                                                                 \
        swp_result;                                                                                \
    })
#define SWP_ASSET_DRAW(expression)                                                                 \
    ({                                                                                             \
        __auto_type swp_result = (expression);                                                     \
        swp_asset_status(swp_result);                                                              \
        swp_result;                                                                                \
    })
#endif
#define SWP_CONTEXT(project, page, course, car, driving)                                           \
    swp_context(project, page, course, car, driving)
#define SWP_FRAME(w, h) swp_frame(w, h)
#define SWP_LOAD_BEGIN(name) swp_load_begin(name)
#define SWP_LOAD_END() swp_load_end()
#define SWP_READY(ready) swp_ready(ready)
#else
#define SWP_PUSH(stage) ((void)0)
#define SWP_POP() ((void)0)
#define SWP_VOID(stage, expression)                                                                \
    do {                                                                                           \
        expression;                                                                                \
    } while (0)
#define SWP_VALUE(stage, expression) (expression)
#define SWP_CONTEXT(project, page, course, car, driving) ((void)0)
#define SWP_FRAME(w, h) ((void)0)
#define SWP_LOAD_BEGIN(name) ((void)0)
#define SWP_LOAD_END() ((void)0)
#define SWP_READY(ready) ((void)0)
#define SWP_ASSET_DRAW(expression) (expression)
#endif
#endif

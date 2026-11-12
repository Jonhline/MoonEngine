/* The assist line: the course's own ordered path, widened to the drivable
   corridor, solved into a minimum-curvature racing line, smoothed and
   coloured for braking/acceleration, ready to draw as a ribbon. */
#ifndef MOON_ASSIST_LINE_H
#define MOON_ASSIST_LINE_H
#include "course.h"
#include "model.h"

/* One point of the game's own AI course line (ADLN), already in the render
   frame: x, y up, z. Read from the guest's lines object, never guessed. */
typedef struct {
    float x, y, z;
    float distance;   /* along the course, metres */
    float arc_radius; /* metres, 0 for a straight */
    int speed_trim;   /* the point's s8 speed trim, percent */
    int kind;          /* 0 straight, else arc */
    int skip;          /* the s8 -128 marker: drop this point */
} gt4_ai_line_point;

typedef struct {
    gt4_model *model;       /* ribbon mesh material, owned here */
    int count;              /* points in the line */
    int triangles;          /* model->count, kept for reports */
    float (*points)[3];     /* smoothed world positions, road + 0.03 m */
    float *target;          /* the line's own target speed at each sample, m/s */
    float *vertex_speed;    /* target speed copied per ribbon vertex, m/s */
    unsigned char *red;     /* 1 where the line brakes into a corner */
    unsigned char *blue;    /* 1 where it accelerates out of one */
    double build_ms;        /* measured build cost, one course */
} gt4_assist_line;

/* Reads the AI course line `index` of the guest's lines object (car+0xEC)
   into points, dropping the pit line and every -128 skip point. Returns the
   number written; *active is the line actually read. */
int gt4_ai_line_export(uint32_t lines, int index, gt4_ai_line_point *out, int cap, int *active);
/* How many lines the table holds (non-null entries), for the STEP 0 dump. */
int gt4_ai_line_table_count(uint32_t lines);
/* Builds the ribbon from the game's own AI line. The old RNW4-section
   generator is gone. 0 when there is no usable line. */
int gt4_assist_line_build(const gt4_course *course, const gt4_ai_line_point *points, int count,
                          gt4_assist_line *out);
/* Repaint the ribbon from the player's speed: blue where the line's own
   target speed is at least the current speed, GT red where it is far below. */
void gt4_assist_line_paint(gt4_assist_line *line, float speed_mps);
void gt4_assist_line_free(gt4_assist_line *line);

#endif

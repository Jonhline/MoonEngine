/* The assist line: the course's own ordered path, widened to the drivable
   corridor, solved into a minimum-curvature racing line, smoothed and
   coloured for braking/acceleration, ready to draw as a ribbon. */
#ifndef MOON_ASSIST_LINE_H
#define MOON_ASSIST_LINE_H
#include "course.h"
#include "model.h"

typedef struct {
    gt4_model *model;       /* ribbon mesh material, owned here */
    int count;              /* points in the line */
    int triangles;          /* model->count, kept for reports */
    float (*points)[3];     /* smoothed world positions, road + 0.30 m */
    unsigned char *red;     /* 1 where the line brakes into a corner */
    unsigned char *blue;    /* 1 where it accelerates out of one */
    double build_ms;        /* measured build cost, one course */
} gt4_assist_line;

/* Builds the ribbon from the runway's ordered path table (+0x4e count,
   +0x84 offset, 16-byte x,y,z,tag entries), then moves it onto the racing
   line inside the road. 0 when the course has none. */
int gt4_assist_line_build(const gt4_course *course, gt4_assist_line *out);
void gt4_assist_line_free(gt4_assist_line *line);

#endif

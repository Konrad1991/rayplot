#ifndef VIEW_RAYPLOT_H
#define VIEW_RAYPLOT_H

#include "types_2D.h"

void data_to_screen(RayView *v, double dx, double dy,
                    float *sx, float *sy);
void screen_to_data(RayView *v, double sx, double sy,
                    double *dx, double *dy);
void fit_view(RayView *v);
double nice_step(double range, int max_ticks);
void draw_axes(RayView *v, const RayFont *font, const double *tick_pos,
                      char *const *tick_labels, int n_ticks);

#endif

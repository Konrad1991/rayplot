#ifndef LAYERS_D_RAYPLOT_H
#define LAYERS_D_RAYPLOT_H

#include "types_2D.h"
#include "view_2D.h"

bool layer_bounds(const RayLayer *layer,
                  double *xmin, double *xmax,
                  double *ymin, double *ymax);
void draw_frame(RayPlot *p);
void handle_input(RayPlot *p);

#endif // !LAYERS_D_RAYPLOT_H

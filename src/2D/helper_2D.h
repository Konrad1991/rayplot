#ifndef HELPER_2D_RAYPLOT_H
#define HELPER_2D_RAYPLOT_H

#include <R.h>
#include <Rinternals.h>
#include "types_2D.h"
#include "view_2D.h"

SEXP list_elt(SEXP list, const char *name);
void build_scatter_layer(RayLayer *layer, SEXP spec);
void build_box_layer(RayLayer *layer, SEXP spec);
void adopt_ticks(RayPlot *p, SEXP spec);

#endif // !HELPER_2D_RAYPLOT_H

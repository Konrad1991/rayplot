#ifndef LIFECYCLE_RAYPLOT_H
#define LIFECYCLE_RAYPLOT_H

#include <R.h>
#include <Rinternals.h>
#include "types_2D.h"
#include "view_2D.h"

void free_layer(RayLayer *layer);
void rayplot_finalize(SEXP ext);

#endif // !LIFECYCLE_RAYPLOT_H

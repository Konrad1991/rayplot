#ifndef TYPES_3D_RAYPLOT_H
#define TYPES_3D_RAYPLOT_H

/* Pure render-core types: no R headers here, mirrors types_2D.h so this
 * stays reusable outside the R glue if the 3D renderer ever needs it
 * (e.g. a future WebAssembly build, as already done for 2D). */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "raylib.h"

typedef enum {
  LAYER3D_SCATTER,
  LAYER3D_LINE,
  LAYER3D_AREA,
  LAYER3D_SURFACE
} LayerType3D;

typedef struct {
  int n;
  double* x;
  double* y;
  double* z;
  double point_radius;
  Color* colours; /* optional, length n; NULL => use the layer's flat colour.
                   * One entry per point -- the R side resolves either a
                   * categorical group->palette lookup or a continuous
                   * value->gradient mapping down to plain hex before it gets
                   * here, so this struct only ever deals with final colours. */
} ScatterPlot3D;

typedef struct {
  int n;
  double* x;
  double* y;
  double* z;
  Color* colours; /* optional, length n; NULL => flat. When set, segments are
                   * drawn with per-vertex colour (GL_LINES, hardware
                   * interpolates between endpoints) instead of DrawLine3D. */
  /* DrawLine3D-only for now (fixed ~1px, no lighting); a width field would
   * be a no-op until lines get upgraded to a tube mesh. */
} LinePlot3D;

typedef struct {
  int n;
  double* x;
  double* y;
  double* z;
  double base_y; /* ribbon drops from (x[i], y[i], z[i]) to (x[i], base_y, z[i]) */
  Color* colours; /* optional, length n; NULL => flat. Applies to both the top
                   * point and its corresponding base-plane point at index i. */
} AreaPlot3D;

typedef struct {
  int n_rows;      /* e.g. one row per MS scan / retention_time sample */
  int n_cols;      /* a grid shared by every row, e.g. a common m/z axis */
  double* row_z;   /* length n_rows: z coordinate of each row */
  double* col_x;   /* length n_cols: x coordinate of each column, shared by all rows */
  double* height;  /* length n_rows*n_cols, COLUMN-MAJOR (R's native matrix
                    * order: element (row, col) at height[col*n_rows + row]),
                    * so an R matrix can be handed over with plain as.double() */
  Color* colours;  /* optional, length n_rows*n_cols, same column-major layout
                    * as height; NULL => flat. Typically colour-by-height. */
} SurfacePlot3D;

typedef struct {
  LayerType3D type;
  union {
    ScatterPlot3D scatter;
    LinePlot3D line;
    AreaPlot3D area;
    SurfacePlot3D surface;
  };
  Color colour; /* includes alpha; areas/surfaces typically use a translucent one */
} RayLayer3D;

typedef struct {
  bool is_open;
  int width;
  int height;

  RayLayer3D* layers;
  int n_layers;

  /* axis titles for the visual X (right)/Y (up)/Z (depth) axes, drawn near
   * the reference axis lines in the render loop. */
  char xlab[64];
  char ylab[64];
  char zlab[64];

  /* Real-world bounding box across all layers, used to scale data into the
   * clean visual 0..10 cube. */
  double xmin, xmax;
  double ymin, ymax;
  double zmin, zmax;

  Camera3D camera;

  /* Orbit-camera state: the camera position is derived each frame from
   * these spherical coordinates around camera.target, so rotate/zoom/pan
   * always act on the data instead of spinning the camera in place. */
  float orbit_yaw;
  float orbit_pitch;
  float orbit_distance;
} RayPlot3D;

#endif // !TYPES_3D_RAYPLOT_H

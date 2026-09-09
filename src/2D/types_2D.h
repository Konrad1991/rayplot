#ifndef TYPES_RAYPLOT_H
#define TYPES_RAYPLOT_H

/* Pure render-core types: no R headers here, so view_2D / layers_2D / text_2D
 * also compile under Emscripten for the WebAssembly build. The R glue
 * (helper_2D, lifecycle_2D, rayplot_2D) includes <Rinternals.h> itself. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "raylib.h"
#include "text_2D.h"

#define MARGIN_LEFT   55.0
#define MARGIN_BOTTOM 30.0
#define MAX_TICKS     8

typedef struct {
  int width;
  int height;
  double xmin;
  double xmax;
  double ymin;
  double ymax;
  double ox;
  double oy;
  double scale_x;
  double scale_y;
  bool view_init;
  bool x_categorical;
} RayView;

typedef enum {
  LAYER_SCATTER,
  LAYER_BOX
} LayerType;

typedef struct {
  int n;
  double* x;
  double* y;
  double point_radius;
} ScatterPlot;

typedef struct {
  int n_boxes;
  double* center;
  double* quantile1;
  double* median;
  double* quantile3;
  double* whisker_low;
  double* whisker_high;
  double* outliers;
  int* outlier_start; // length n_boxes + 1; last entry = total outlier count
  double box_width;
} BoxPlot;

typedef struct {
  LayerType type;
  union {
    ScatterPlot scatter_plot;
    BoxPlot box_plot;
  };
  int* colour_groups;
  Color* colours;
  int n_colours;
} RayLayer;

typedef struct {
  bool is_open;
  RayView view;
  RayFont font;
  RayLayer* layers;
  int n_layers;
  double* tick_pos;
  char** tick_labels;
  int n_ticks;
} RayPlot;

#endif // !TYPES_RAYPLOT_H

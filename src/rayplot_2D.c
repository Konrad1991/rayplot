#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"

#define MARGIN_LEFT   55.0
#define MARGIN_BOTTOM 30.0
#define MAX_TICKS     8

typedef struct {
  bool is_open;
  int width;
  int height;
  int n;
  double* x;
  double* y;
  int* colour_groups;
  Color* colours;
  int n_colours;
  double xmin;
  double xmax;
  double ymin;
  double ymax;
  double ox;
  double oy;
  double scale_x;
  double scale_y;
  double point_radius;
  bool view_init;
} RayPlot;

static void data_to_screen(RayPlot *p, double dx, double dy, float *sx, float *sy) {
  *sx = (float)(MARGIN_LEFT + (dx - p->ox) * p->scale_x);
  *sy = (float)((p->height - MARGIN_BOTTOM) - (dy - p->oy) * p->scale_y);
}

static void screen_to_data(RayPlot *p, double sx, double sy, double *dx, double *dy) {
  *dx = (sx - MARGIN_LEFT) / p->scale_x + p->ox;
  *dy = ((p->height - MARGIN_BOTTOM) - sy) / p->scale_y + p->oy;
}

static void fit_view(RayPlot *p) {
  double xr = p->xmax - p->xmin;
  double yr = p->ymax - p->ymin;
  if (xr <= 0) xr = 1.0;
  if (yr <= 0) yr = 1.0;
  double pad = 0.08;                 /* 8 % Rand */
  double avail_w = p->width  - MARGIN_LEFT;
  double avail_h = p->height - MARGIN_BOTTOM;
  p->scale_x = avail_w * (1 - 2*pad) / xr;
  p->scale_y = avail_h * (1 - 2*pad) / yr;
  p->ox = p->xmin - (avail_w / p->scale_x - xr) / 2.0;
  p->oy = p->ymin - (avail_h / p->scale_y - yr) / 2.0;
  p->view_init = true;
}

static double nice_step(double range, int max_ticks) {
  if (range <= 0) return 1.0;
  double raw = range / max_ticks;
  double mag = pow(10.0, floor(log10(raw)));
  double norm = raw / mag;
  double step;
  if (norm < 1.5)      step = 1.0;
  else if (norm < 3.0) step = 2.0;
  else if (norm < 7.0) step = 5.0;
  else                 step = 10.0;
  return step * mag;
}

static void draw_axes(RayPlot *p) {
  int plot_left   = (int)MARGIN_LEFT;
  int plot_right  = p->width;
  int plot_top    = 0;
  int plot_bottom = (int)(p->height - MARGIN_BOTTOM);

  Color grid  = (Color){ 222, 222, 222, 255 };
  Color axis  = (Color){ 90, 90, 90, 255 };
  Color label = (Color){ 90, 90, 90, 255 };

  double dx_left, dy_top, dx_right, dy_bottom;
  screen_to_data(p, plot_left,  plot_top,    &dx_left,  &dy_top);
  screen_to_data(p, plot_right, plot_bottom, &dx_right, &dy_bottom);

  /* vertical grid + x ticks*/
  double xstep = nice_step(dx_right - dx_left, MAX_TICKS);
  for (double xt = ceil(dx_left / xstep) * xstep; xt <= dx_right + 1e-9; xt += xstep) {
    float sx, sy;
    data_to_screen(p, xt, 0.0, &sx, &sy);
    if (sx < plot_left || sx > plot_right) continue;
    DrawLine((int)sx, plot_top, (int)sx, plot_bottom, grid);
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", xt);
    int tw = MeasureText(buf, 12);
    DrawText(buf, (int)sx - tw / 2, plot_bottom + 6, 12, label);
  }

  /* horizontale grid + y ticks*/
  double ystep = nice_step(dy_top - dy_bottom, MAX_TICKS);
  for (double yt = ceil(dy_bottom / ystep) * ystep; yt <= dy_top + 1e-9; yt += ystep) {
    float sx, sy;
    data_to_screen(p, 0.0, yt, &sx, &sy);
    if (sy < plot_top || sy > plot_bottom) continue;
    DrawLine(plot_left, (int)sy, plot_right, (int)sy, grid);
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", yt);
    int tw = MeasureText(buf, 12);
    DrawText(buf, plot_left - tw - 6, (int)sy - 6, 12, label);
  }

  DrawLine(plot_left, plot_top, plot_left, plot_bottom, axis);
  DrawLine(plot_left, plot_bottom, plot_right, plot_bottom, axis);
}

static void draw_frame(RayPlot *p) {
  BeginDrawing();
  ClearBackground((Color){ 245, 245, 245, 255 });
  draw_axes(p);
  for (int i = 0; i < p->n; i++) {
    float sx, sy;
    data_to_screen(p, p->x[i], p->y[i], &sx, &sy);
    if (sx < MARGIN_LEFT - 10 || sx > p->width + 10) continue;   /* clip */
    if (sy < -10 || sy > p->height - MARGIN_BOTTOM + 10) continue;
    int g = p->colour_groups[i];
    if (g < 0 || g >= p->n_colours) g = 0;   /* defensive: R validates, direct .Call may not */
    Color pt = p->colours[g];
    DrawCircleV((Vector2){ sx, sy }, p->point_radius, pt);
  }
  DrawText("drag: pan   wheel: zoom   esc/X: close",
           10, 10, 16, (Color){ 90, 90, 90, 255 });
  EndDrawing();
}

static void handle_input(RayPlot *p) {
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 d = GetMouseDelta();
    p->ox -= d.x / p->scale_x;
    p->oy += d.y / p->scale_y;
  }

  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    Vector2 m = GetMousePosition();
    double dx_before, dy_before;
    screen_to_data(p, m.x, m.y, &dx_before, &dy_before);
    double factor = (wheel > 0) ? 1.1 : (1.0 / 1.1);
    p->scale_x *= factor;
    p->scale_y *= factor;
    double dx_after, dy_after;
    screen_to_data(p, m.x, m.y, &dx_after, &dy_after);
    p->ox += dx_before - dx_after;
    p->oy += dy_before - dy_after;
  }

  Vector2 m = GetMousePosition();
  bool mouse_on_point = false;
  float target_x = 0.0;
  float target_y = 0.0;
  for (int i = 0; i < p->n; i++) {
    float sx;
    float sy;
    data_to_screen(p, p->x[i], p->y[i], &sx, &sy);
    mouse_on_point = CheckCollisionCircles(
      m, p->point_radius, (Vector2){sx, sy}, p->point_radius
    );
    if (mouse_on_point) {
      target_x = p->x[i];
      target_y = p->y[i];
      break;
    }
  }
  if (mouse_on_point) {
    const int font_size = 16;
    const int x_pos = (int)m.x + 10;
    const int y_pos = (int)m.y;
    char tooltip[100];
    snprintf(tooltip, sizeof(tooltip), "X: %.2f,\nY: %.2f", target_x, target_y);
    DrawText(tooltip, x_pos, y_pos, font_size, BLACK);
  }
}

static void rayplot_finalize(SEXP ext) {
  RayPlot *p = (RayPlot *) R_ExternalPtrAddr(ext);
  if (p == NULL) return;
  if (p->is_open) {
    CloseWindow();
    p->is_open = false;
  }
  free(p->x);
  free(p->y);
  free(p->colour_groups);
  free(p->colours);
  free(p);
  R_ClearExternalPtr(ext);
}

SEXP rayplot_open_(
  SEXP x_, SEXP y_,
  SEXP w_, SEXP h_,
  SEXP colour_groups_, SEXP colours_,
  SEXP title_, SEXP point_radius
) {

  const int n = LENGTH(x_);
  const int n_colours = LENGTH(colours_);
  if (LENGTH(colour_groups_) != n) error("rayplot: colour_groups length must match x");
  RayPlot *p = (RayPlot *) calloc(1, sizeof(RayPlot));
  if (!p) error("rayplot: out of memory");

  p->width = asInteger(w_);
  p->height = asInteger(h_);
  p->n = n;
  p->n_colours = n_colours;
  p->x = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  p->y = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  p->colour_groups = (int*) malloc(sizeof(int) * (n > 0 ? n : 1));
  p->colours = (Color*) malloc(sizeof(Color) * (n_colours > 0 ? n_colours : 1));
  if (!p->x || !p->y || !p->colour_groups || !p->colours) {
    free(p->x);
    free(p->y);
    free(p->colour_groups);
    free(p->colours);
    free(p);
    error("rayplot: out of memory"); 
  }
  p->point_radius = *(REAL(point_radius));
  double* xr = REAL(x_);
  double* yr = REAL(y_);
  int* cgr = INTEGER(colour_groups_);
  memcpy(p->x, xr, sizeof(double) * n);
  memcpy(p->y, yr, sizeof(double) * n);
  memcpy(p->colour_groups, cgr, sizeof(int) * n);

  for (int i = 0; i < n_colours; i++) {
    const char *hex = CHAR(STRING_ELT(colours_, i));
    if (*hex == '#') hex++;
    unsigned long rgb = strtoul(hex, NULL, 16);
    p->colours[i] = GetColor((unsigned int)((rgb << 8) | 0xFFu));
  }

  p->xmin = p->xmax = (n > 0) ? xr[0] : 0.0;
  p->ymin = p->ymax = (n > 0) ? yr[0] : 0.0;
  for (int i = 1; i < n; i++) {
    if (xr[i] < p->xmin) p->xmin = xr[i];
    if (xr[i] > p->xmax) p->xmax = xr[i];
    if (yr[i] < p->ymin) p->ymin = yr[i];
    if (yr[i] > p->ymax) p->ymax = yr[i];
  }

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(p->width, p->height, CHAR(STRING_ELT(title_, 0)));
  SetTargetFPS(60);
  p->is_open = true;
  fit_view(p);

  SEXP ext = PROTECT(R_MakeExternalPtr(p, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ext, rayplot_finalize, TRUE);
  UNPROTECT(1);
  return ext;
}

SEXP rayplot_step_(SEXP ext) {
  RayPlot *p = (RayPlot *) R_ExternalPtrAddr(ext);
  if (p == NULL || !p->is_open) return R_NilValue;
  if (IsWindowResized()) {
    int new_w = GetScreenWidth();
    int new_h = GetScreenHeight();
    double old_avail_w = p->width  - MARGIN_LEFT;
    double old_avail_h = p->height - MARGIN_BOTTOM;
    double new_avail_w = new_w - MARGIN_LEFT;
    double new_avail_h = new_h - MARGIN_BOTTOM;
    p->scale_x *= new_avail_w / old_avail_w;
    p->scale_y *= new_avail_h / old_avail_h;
    p->width  = new_w;
    p->height = new_h;
  }
  handle_input(p);
  draw_frame(p);
  return R_NilValue;
}

SEXP rayplot_should_close_(SEXP ext) {
  RayPlot *p = (RayPlot *) R_ExternalPtrAddr(ext);
  if (p == NULL || !p->is_open) return ScalarLogical(TRUE);
  return ScalarLogical(WindowShouldClose());
}

SEXP rayplot_close_(SEXP ext) {
  RayPlot *p = (RayPlot *) R_ExternalPtrAddr(ext);
  if (p == NULL) return R_NilValue;
  if (p->is_open) {
    p->is_open = false;
    CloseWindow();
  }
  return R_NilValue;
}

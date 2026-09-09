#include "view_2D.h"

/* ------------------------------------------------------------------ */
/* view: coordinate transform + axes + pan/zoom, no layer data       */
/* ------------------------------------------------------------------ */
void data_to_screen(RayView *v, double dx, double dy, float *sx, float *sy) {
  *sx = (float)(MARGIN_LEFT + (dx - v->ox) * v->scale_x);
  *sy = (float)((v->height - MARGIN_BOTTOM) - (dy - v->oy) * v->scale_y);
}

void screen_to_data(RayView *v, double sx, double sy, double *dx, double *dy) {
  *dx = (sx - MARGIN_LEFT) / v->scale_x + v->ox;
  *dy = ((v->height - MARGIN_BOTTOM) - sy) / v->scale_y + v->oy;
}

void fit_view(RayView *v) {
  double xr = v->xmax - v->xmin;
  double yr = v->ymax - v->ymin;
  if (xr <= 0) xr = 1.0;
  if (yr <= 0) yr = 1.0;
  const double pad = 0.08; // 8 % Rand
  const double avail_w = v->width  - MARGIN_LEFT;
  const double avail_h = v->height - MARGIN_BOTTOM;
  v->scale_x = avail_w * (1 - 2*pad) / xr;
  v->scale_y = avail_h * (1 - 2*pad) / yr;
  v->ox = v->xmin - (avail_w / v->scale_x - xr) / 2.0;
  v->oy = v->ymin - (avail_h / v->scale_y - yr) / 2.0;
  v->view_init = true;
}

double nice_step(double range, int max_ticks) {
  if (range <= 0) return 1.0;
  const double raw = range / max_ticks;
  const double mag = pow(10.0, floor(log10(raw)));
  const double norm = raw / mag;
  double step;
  if (norm < 1.5) {
    step = 1.0;
  } else if (norm < 3.0) {
    step = 2.0;
  } else if (norm < 7.0) {
    step = 5.0;
  } else {
    step = 10.0;
  }
  return step * mag;
}

#define TICK_FONT 13.0f

void draw_axes(RayView *v, const RayFont *font, const double *tick_pos,
               char *const *tick_labels, int n_ticks) {
  int plot_left = (int)MARGIN_LEFT;
  int plot_right = v->width;
  int plot_top = 0;
  int plot_bottom = (int)(v->height - MARGIN_BOTTOM);
  Color grid = COL_GRID;
  Color axis = COL_AXIS;
  Color label = COL_LABEL;
  double dx_left;
  double dy_top;
  double dx_right;
  double dy_bottom;
  screen_to_data(v, plot_left, plot_top, &dx_left, &dy_top);
  screen_to_data(v, plot_right, plot_bottom, &dx_right, &dy_bottom);

  if (v->x_categorical && tick_pos != NULL && n_ticks > 0) {
    /* categorical x: one labelled tick per supplied position */
    for (int i = 0; i < n_ticks; i++) {
      float sx;
      float sy;
      data_to_screen(v, tick_pos[i], 0.0, &sx, &sy);
      if (sx < plot_left || sx > plot_right) continue;
      DrawLine((int)sx, plot_top, (int)sx, plot_bottom, grid);
      const float tw = ray_measure(font, tick_labels[i], TICK_FONT).x;
      ray_text(font, tick_labels[i], sx - tw / 2.0f, plot_bottom + 6.0f, TICK_FONT, label);
    }
  } else {
    /* continuous x: nice-stepped grid + ticks */
    const double xstep = nice_step(dx_right - dx_left, MAX_TICKS);
    for (double xt = ceil(dx_left / xstep) * xstep; xt <= dx_right + 1e-9; xt += xstep) {
      float sx;
      float sy;
      data_to_screen(v, xt, 0.0, &sx, &sy);
      if (sx < plot_left || sx > plot_right) continue;
      DrawLine((int)sx, plot_top, (int)sx, plot_bottom, grid);
      char buf[32];
      snprintf(buf, sizeof(buf), "%g", xt);
      const float tw = ray_measure(font, buf, TICK_FONT).x;
      ray_text(font, buf, sx - tw / 2.0f, plot_bottom + 6.0f, TICK_FONT, label);
    }
  }

  /* horizontale grid + y ticks*/
  const double ystep = nice_step(dy_top - dy_bottom, MAX_TICKS);
  for (double yt = ceil(dy_bottom / ystep) * ystep; yt <= dy_top + 1e-9; yt += ystep) {
    float sx;
    float sy;
    data_to_screen(v, 0.0, yt, &sx, &sy);
    if (sy < plot_top || sy > plot_bottom) continue;
    DrawLine(plot_left, (int)sy, plot_right, (int)sy, grid);
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", yt);
    const float tw = ray_measure(font, buf, TICK_FONT).x;
    ray_text(font, buf, plot_left - tw - 6.0f, sy - TICK_FONT / 2.0f, TICK_FONT, label);
  }

  DrawLine(plot_left, plot_top, plot_left, plot_bottom, axis);
  DrawLine(plot_left, plot_bottom, plot_right, plot_bottom, axis);
}

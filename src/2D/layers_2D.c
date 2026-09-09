#include "layers_2D.h"

/* ------------------------------------------------------------------ */
/* layers                                                             */
/* ------------------------------------------------------------------ */
static Color layer_colour(const RayLayer *layer, int i) {
  int g = layer->colour_groups[i];
  if (g < 0 || g >= layer->n_colours) g = 0; /* defensive: R validates, direct .Call may not */
  return layer->colours[g];
}

static void draw_scatter_layer(RayView *v, const RayLayer *layer) {
  const ScatterPlot *s = &layer->scatter_plot;
  for (int i = 0; i < s->n; i++) {
    float sx;
    float sy;
    data_to_screen(v, s->x[i], s->y[i], &sx, &sy);
    if (sx < MARGIN_LEFT - 10 || sx > v->width + 10) continue; /* clip */
    if (sy < -10 || sy > v->height - MARGIN_BOTTOM + 10) continue;
    DrawCircleV((Vector2){ sx, sy }, s->point_radius, layer_colour(layer, i));
  }
}

/* writes a tooltip into out[] and returns true when the mouse is over a scatter point */
static bool scatter_layer_hit(RayView *v, const RayLayer *layer, Vector2 m,
                              char *out, size_t outsz) {
  const ScatterPlot *s = &layer->scatter_plot;
  for (int i = 0; i < s->n; i++) {
    float sx;
    float sy;
    data_to_screen(v, s->x[i], s->y[i], &sx, &sy);
    if (CheckCollisionCircles(m, s->point_radius, (Vector2){ sx, sy }, s->point_radius)) {
      snprintf(out, outsz, "X: %.2f,\nY: %.2f", s->x[i], s->y[i]);
      return true;
    }
  }
  return false;
}

static void draw_box_layer(RayView *v, const RayLayer *layer) {
  const BoxPlot *b = &layer->box_plot;
  Color stroke = COL_BOX_STROKE;
  double hw = b->box_width * 0.5;
  double capw = hw * 0.5;
  for (int i = 0; i < b->n_boxes; i++) {
    Color fill = layer_colour(layer, i);
    fill.a = 255;
    float xl;
    float yq3;
    data_to_screen(v, b->center[i] - hw, b->quantile3[i], &xl, &yq3);
    float xr;
    float yq1;
    data_to_screen(v, b->center[i] + hw, b->quantile1[i], &xr, &yq1);
    float xc;
    float ymed;
    data_to_screen(v, b->center[i], b->median[i], &xc, &ymed);
    float xcl;
    float ywl;
    data_to_screen(v, b->center[i] - capw, b->whisker_low[i], &xcl, &ywl);
    float xcr;
    float ywh;
    data_to_screen(v, b->center[i] + capw, b->whisker_high[i], &xcr, &ywh);
    Rectangle rec = {
      xl,
      yq3,
      xr - xl,
      yq1 - yq3
    };
    DrawRectangleRec(rec, fill);
    DrawRectangleLinesEx(rec, 1.0f, stroke);
    DrawLineEx(
      (Vector2){ xl, ymed },
      (Vector2){ xr, ymed },
      2.0f, stroke
    ); // median
    DrawLineEx(
      (Vector2){ xc, yq3 },
      (Vector2){ xc, ywh },
      1.0f, stroke
    ); // upper whisker
    DrawLineEx(
      (Vector2){ xc, yq1 },
      (Vector2){ xc, ywl },
      1.0f, stroke
    ); // lower whisker
    DrawLineEx(
      (Vector2){ xcl, ywh },
      (Vector2){ xcr, ywh },
      1.0f, stroke
    ); // upper cap
    DrawLineEx(
      (Vector2){ xcl, ywl },
      (Vector2){ xcr, ywl },
      1.0f, stroke
    ); // lower cap

    Color oc = layer_colour(layer, i);
    for (int j = b->outlier_start[i]; j < b->outlier_start[i + 1]; j++) {
      float ox;
      float oy;
      data_to_screen(v, b->center[i], b->outliers[j], &ox, &oy);
      DrawCircleV((Vector2){ ox, oy }, 3.0f, oc);
    }
  }
}

/* writes a tooltip into out[] and returns true when the mouse is over a box or outlier */
static bool box_layer_hit(RayView *v, const RayLayer *layer, Vector2 m,
                          char *out, size_t outsz) {
  const BoxPlot *b = &layer->box_plot;
  double hw = b->box_width * 0.5;
  for (int i = 0; i < b->n_boxes; i++) {
    for (int j = b->outlier_start[i]; j < b->outlier_start[i + 1]; j++) {
      float ox;
      float oy;
      data_to_screen(v, b->center[i], b->outliers[j], &ox, &oy);
      if (CheckCollisionPointCircle(m, (Vector2){ ox, oy }, 4.0f)) {
        snprintf(out, outsz, "outlier: %.4g", b->outliers[j]);
        return true;
      }
    }
    float xl;
    float yq3;
    data_to_screen(v, b->center[i] - hw, b->quantile3[i], &xl, &yq3);
    float xr;
    float yq1;
    data_to_screen(v, b->center[i] + hw, b->quantile1[i], &xr, &yq1);
    Rectangle rec = {
      xl,
      yq3,
      xr - xl,
      yq1 - yq3
    };
    if (CheckCollisionPointRec(m, rec)) {
      snprintf(out, outsz,
               "median: %.4g\nQ1: %.4g   Q3: %.4g\nwhisker: %.4g .. %.4g",
               b->median[i], b->quantile1[i], b->quantile3[i],
               b->whisker_low[i], b->whisker_high[i]);
      return true;
    }
  }
  return false;
}

/* fills bounds for one layer; returns false if the layer has nothing to bound */
bool layer_bounds(const RayLayer *layer,
                         double *xmin, double *xmax, double *ymin, double *ymax) {
  switch (layer->type) {
    case LAYER_SCATTER: {
      const ScatterPlot *s = &layer->scatter_plot;
      if (s->n < 1) return false;
      *xmin = *xmax = s->x[0];
      *ymin = *ymax = s->y[0];
      for (int i = 1; i < s->n; i++) {
        if (s->x[i] < *xmin) *xmin = s->x[i];
        if (s->x[i] > *xmax) *xmax = s->x[i];
        if (s->y[i] < *ymin) *ymin = s->y[i];
        if (s->y[i] > *ymax) *ymax = s->y[i];
      }
      return true;
    }
    case LAYER_BOX: {
      const BoxPlot *b = &layer->box_plot;
      if (b->n_boxes < 1) return false;
      const double hw = b->box_width * 0.5;
      *xmin = b->center[0] - hw;
      *xmax = b->center[0] + hw;
      *ymin = b->whisker_low[0];
      *ymax = b->whisker_high[0];
      for (int i = 0; i < b->n_boxes; i++) {
        if (b->center[i] - hw < *xmin) *xmin = b->center[i] - hw;
        if (b->center[i] + hw > *xmax) *xmax = b->center[i] + hw;
        if (b->whisker_low[i]  < *ymin) *ymin = b->whisker_low[i];
        if (b->whisker_high[i] > *ymax) *ymax = b->whisker_high[i];
      }
      const int total = b->outlier_start[b->n_boxes];
      for (int j = 0; j < total; j++) {
        if (b->outliers[j] < *ymin) *ymin = b->outliers[j];
        if (b->outliers[j] > *ymax) *ymax = b->outliers[j];
      }
      return true;
    }
  }
  return false;
}

void draw_frame(RayPlot *p) {
  BeginDrawing();
  ClearBackground(COL_BG);
  draw_axes(&p->view, &p->font, p->tick_pos, p->tick_labels, p->n_ticks);
  for (int k = 0; k < p->n_layers; k++) {
    switch (p->layers[k].type) {
      case LAYER_SCATTER: draw_scatter_layer(&p->view, &p->layers[k]); break;
      case LAYER_BOX:     draw_box_layer(&p->view, &p->layers[k]);     break;
    }
  }
  ray_text(&p->font, "drag: pan   wheel: zoom   esc/X: close",
           10.0f, 10.0f, 15.0f, COL_HELP);
  EndDrawing();
}

void handle_input(RayPlot *p) {
  RayView *v = &p->view;

  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 d = GetMouseDelta();
    v->ox -= d.x / v->scale_x;
    v->oy += d.y / v->scale_y;
  }

  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    Vector2 mp = GetMousePosition();
    double dx_before, dy_before;
    screen_to_data(v, mp.x, mp.y, &dx_before, &dy_before);
    double factor = (wheel > 0) ? 1.1 : (1.0 / 1.1);
    v->scale_x *= factor;
    v->scale_y *= factor;
    double dx_after, dy_after;
    screen_to_data(v, mp.x, mp.y, &dx_after, &dy_after);
    v->ox += dx_before - dx_after;
    v->oy += dy_before - dy_after;
  }

  Vector2 m = GetMousePosition();
  char tooltip[160];
  bool have_tip = false;
  for (int k = 0; k < p->n_layers && !have_tip; k++) {
    switch (p->layers[k].type) {
      case LAYER_SCATTER: have_tip = scatter_layer_hit(v, &p->layers[k], m, tooltip, sizeof tooltip); break;
      case LAYER_BOX:     have_tip = box_layer_hit(v, &p->layers[k], m, tooltip, sizeof tooltip);     break;
    }
  }
  if (have_tip) {
    const float fs = 15.0f;
    const float pad = 6.0f;
    Vector2 ts = ray_measure(&p->font, tooltip, fs);
    float tx = m.x + 12.0f;
    float ty = m.y + 4.0f;
    if (tx + ts.x + pad > p->view.width) tx = m.x - 12.0f - ts.x;
    if (ty + ts.y + pad > p->view.height) ty = p->view.height - ts.y - pad;
    Rectangle card = { tx - pad, ty - pad, ts.x + 2.0f * pad, ts.y + 2.0f * pad };
    DrawRectangleRounded(card, 0.25f, 6, COL_TIP_BG);
    ray_text(&p->font, tooltip, tx, ty, fs, COL_TIP_TEXT);
  }
}

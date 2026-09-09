#include <R.h>
#include <Rinternals.h>
#include "types_2D.h"
#include "view_2D.h"
#include "layers_2D.h"
#include "lifecycle_2D.h"
#include "helper_2D.h"

SEXP rayplot_open_(SEXP layers_, SEXP w_, SEXP h_, SEXP title_, SEXP font_) {
  SetTraceLogLevel(LOG_ERROR);
  if (TYPEOF(layers_) != VECSXP) {
    error("rayplot: layers must be a list");
  }
  const int n_layers = LENGTH(layers_);
  if (n_layers < 1) {
    error("rayplot: need at least one layer");
  }

  RayPlot *p = (RayPlot *) calloc(1, sizeof(RayPlot));
  if (!p) {
    error("rayplot: out of memory");
  }
  p->layers = (RayLayer *) calloc(n_layers, sizeof(RayLayer));
  if (!p->layers) {
    free(p);
    error("rayplot: out of memory");
  }
  p->n_layers = n_layers;
  p->view.width = asInteger(w_);
  p->view.height = asInteger(h_);

  SEXP ext = PROTECT(R_MakeExternalPtr(p, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ext, rayplot_finalize, TRUE);

  for (int k = 0; k < n_layers; k++) {
    SEXP spec = VECTOR_ELT(layers_, k);
    if (TYPEOF(spec) != VECSXP) {
      error("rayplot: layer %d is not a list", k + 1);
    }
    SEXP type_ = list_elt(spec, "type");
    if (type_ == R_NilValue || TYPEOF(type_) != STRSXP || LENGTH(type_) < 1) {
      error("rayplot: layer %d has no type", k + 1);
    }
    const char *type = CHAR(STRING_ELT(type_, 0));
    if (strcmp(type, "scatter") == 0) {
      build_scatter_layer(&p->layers[k], spec);
    } else if (strcmp(type, "box") == 0) {
      build_box_layer(&p->layers[k], spec);
    } else {
      error("rayplot: unknown layer type '%s'", type);
    }
    adopt_ticks(p, spec);
  }

  /* union data bounds over all layers */
  bool any = false;
  for (int k = 0; k < n_layers; k++) {
    double lxmin;
    double lxmax;
    double lymin;
    double lymax;
    if (!layer_bounds(&p->layers[k], &lxmin, &lxmax, &lymin, &lymax)) continue;
    if (!any) {
      p->view.xmin = lxmin;
      p->view.xmax = lxmax;
      p->view.ymin = lymin;
      p->view.ymax = lymax;
      any = true;
    } else {
      if (lxmin < p->view.xmin) p->view.xmin = lxmin;
      if (lxmax > p->view.xmax) p->view.xmax = lxmax;
      if (lymin < p->view.ymin) p->view.ymin = lymin;
      if (lymax > p->view.ymax) p->view.ymax = lymax;
    }
  }
  if (!any) {
    p->view.xmin = p->view.ymin = 0.0;
    p->view.xmax = p->view.ymax = 1.0;
  }

  if (IsWindowReady()) CloseWindow(); /* raylib is single-window: drop any stale one */
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(p->view.width, p->view.height, CHAR(STRING_ELT(title_, 0)));
  SetTargetFPS(60);
  p->is_open = true;

  const char *font_path =
    (font_ != R_NilValue && TYPEOF(font_) == STRSXP && LENGTH(font_) >= 1)
      ? CHAR(STRING_ELT(font_, 0)) : NULL;
  rayfont_load(&p->font, font_path);

  fit_view(&p->view);

  UNPROTECT(1);
  return ext;
}

SEXP rayplot_step_(SEXP ext) {
  RayPlot *p = (RayPlot *) R_ExternalPtrAddr(ext);
  if (p == NULL || !p->is_open) return R_NilValue;
  RayView *v = &p->view;
  if (IsWindowResized()) {
    const int new_w = GetScreenWidth();
    const int new_h = GetScreenHeight();
    const double old_avail_w = v->width  - MARGIN_LEFT;
    const double old_avail_h = v->height - MARGIN_BOTTOM;
    const double new_avail_w = new_w - MARGIN_LEFT;
    const double new_avail_h = new_h - MARGIN_BOTTOM;
    v->scale_x *= new_avail_w / old_avail_w;
    v->scale_y *= new_avail_h / old_avail_h;
    v->width  = new_w;
    v->height = new_h;
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
  p->is_open = false;
  if (IsWindowReady()) {
    rayfont_unload(&p->font);
    CloseWindow();
  }
  return R_NilValue;
}

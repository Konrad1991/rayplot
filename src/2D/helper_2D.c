#include "helper_2D.h"
/* ------------------------------------------------------------------ */
/* R list helpers + layer builders                                    */
/* ------------------------------------------------------------------ */
SEXP list_elt(SEXP list, const char *name) {
  SEXP names = getAttrib(list, R_NamesSymbol);
  if (names == R_NilValue) return R_NilValue;
  for (int i = 0; i < LENGTH(names); i++) {
    if (strcmp(CHAR(STRING_ELT(names, i)), name) == 0) {
      return VECTOR_ELT(list, i);
    }
  }
  return R_NilValue;
}

static Color hex_to_color(const char *hex) {
  if (*hex == '#') hex++;
  unsigned long rgb = strtoul(hex, NULL, 16);
  return GetColor((unsigned int)((rgb << 8) | 0xFFu));
}

void build_scatter_layer(RayLayer *layer, SEXP spec) {
  layer->type = LAYER_SCATTER;
  SEXP x_ = list_elt(spec, "x");
  SEXP y_ = list_elt(spec, "y");
  SEXP cg_ = list_elt(spec, "colour_groups");
  SEXP col_ = list_elt(spec, "colours");
  SEXP pr_ = list_elt(spec, "point_radius");
  if (x_ == R_NilValue || y_ == R_NilValue || cg_ == R_NilValue ||
    col_ == R_NilValue || pr_ == R_NilValue) {
    error("rayplot: scatter layer missing a field (x/y/colour_groups/colours/point_radius)");
  }
  if (TYPEOF(x_) != REALSXP || TYPEOF(y_) != REALSXP) {
    error("rayplot: scatter x/y must be double");
  }
  if (TYPEOF(cg_) != INTSXP) {
    error("rayplot: scatter colour_groups must be integer");
  }
  if (TYPEOF(col_) != STRSXP) {
    error("rayplot: scatter colours must be character");
  }
  const int n = LENGTH(x_);
  const int n_colours = LENGTH(col_);
  if (LENGTH(y_) != n) {
    error("rayplot: scatter x and y length differ");
  }
  if (LENGTH(cg_) != n) {
    error("rayplot: scatter colour_groups length must match x");
  }
  if (n_colours < 1) {
    error("rayplot: scatter needs at least one colour");
  }
  layer->n_colours = n_colours;
  ScatterPlot *s = &layer->scatter_plot;
  s->n = n;
  s->point_radius = asReal(pr_);
  s->x = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  s->y = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  layer->colour_groups = (int*) malloc(sizeof(int) * (n > 0 ? n : 1));
  layer->colours = (Color*) malloc(sizeof(Color) * n_colours);
  if (!s->x || !s->y || !layer->colour_groups || !layer->colours) {
    error("rayplot: out of memory");
  }
  memcpy(s->x, REAL(x_), sizeof(double) * n);
  memcpy(s->y, REAL(y_), sizeof(double) * n);
  memcpy(layer->colour_groups, INTEGER(cg_), sizeof(int) * n);
  for (int i = 0; i < n_colours; i++) {
    layer->colours[i] = hex_to_color(CHAR(STRING_ELT(col_, i)));
  }
}

void build_box_layer(RayLayer *layer, SEXP spec) {
  layer->type = LAYER_BOX;
  SEXP center_ = list_elt(spec, "centers");
  SEXP q1_ = list_elt(spec, "first_quantiles");
  SEXP med_ = list_elt(spec, "medians");
  SEXP q3_ = list_elt(spec, "third_quantiles");
  SEXP wl_ = list_elt(spec, "whisker_low");
  SEXP wh_ = list_elt(spec, "whisker_high");
  SEXP out_ = list_elt(spec, "outliers");
  SEXP ostart_ = list_elt(spec, "outlier_start");
  SEXP bw_ = list_elt(spec, "box_width");
  SEXP cg_ = list_elt(spec, "colour_groups");
  SEXP col_ = list_elt(spec, "colours");
  if (center_ == R_NilValue || q1_ == R_NilValue || med_ == R_NilValue ||
    q3_ == R_NilValue || wl_ == R_NilValue || wh_ == R_NilValue ||
    out_ == R_NilValue || ostart_ == R_NilValue || bw_ == R_NilValue ||
    cg_ == R_NilValue || col_ == R_NilValue) {
    error("rayplot: box layer missing a field");
  }
  if (TYPEOF(center_) != REALSXP || TYPEOF(q1_) != REALSXP || TYPEOF(med_) != REALSXP ||
    TYPEOF(q3_) != REALSXP || TYPEOF(wl_) != REALSXP || TYPEOF(wh_) != REALSXP ||
    TYPEOF(out_) != REALSXP) {
    error("rayplot: box numeric fields must be double");
  }
  if (TYPEOF(ostart_) != INTSXP) {
    error("rayplot: box outlier_start must be integer");
  }
  if (TYPEOF(cg_) != INTSXP) {
    error("rayplot: box colour_groups must be integer");
  }
  if (TYPEOF(col_) != STRSXP) {
    error("rayplot: box colours must be character");
  }
  const int nb = LENGTH(center_);
  const int n_colours = LENGTH(col_);
  const int n_out = LENGTH(out_);
  if (LENGTH(q1_) != nb || LENGTH(med_) != nb || LENGTH(q3_) != nb ||
    LENGTH(wl_) != nb || LENGTH(wh_) != nb || LENGTH(cg_) != nb) {
    error("rayplot: box per-box fields must all have length n_boxes");
  }
  if (LENGTH(ostart_) != nb + 1) {
    error("rayplot: box outlier_start must have length n_boxes + 1");
  }
  if (n_colours < 1) {
    error("rayplot: box needs at least one colour");
  }
  if (INTEGER(ostart_)[nb] != n_out) {
    error("rayplot: box outlier_start[n_boxes] must equal length(outliers)");
  }
  layer->n_colours = n_colours;
  BoxPlot *b = &layer->box_plot;
  b->n_boxes = nb;
  b->box_width = asReal(bw_);
  b->center = (double*) malloc(sizeof(double) * (nb > 0 ? nb : 1));
  b->quantile1 = (double*) malloc(sizeof(double) * (nb > 0 ? nb : 1));
  b->median = (double*) malloc(sizeof(double) * (nb > 0 ? nb : 1));
  b->quantile3 = (double*) malloc(sizeof(double) * (nb > 0 ? nb : 1));
  b->whisker_low = (double*) malloc(sizeof(double) * (nb > 0 ? nb : 1));
  b->whisker_high = (double*) malloc(sizeof(double) * (nb > 0 ? nb : 1));
  b->outliers = (double*) malloc(sizeof(double) * (n_out > 0 ? n_out : 1));
  b->outlier_start = (int*)    malloc(sizeof(int) * (nb + 1));
  layer->colour_groups = (int*)   malloc(sizeof(int)   * (nb > 0 ? nb : 1));
  layer->colours = (Color*) malloc(sizeof(Color) * n_colours);
  if (!b->center || !b->quantile1 || !b->median || !b->quantile3 ||
    !b->whisker_low || !b->whisker_high || !b->outliers || !b->outlier_start ||
    !layer->colour_groups || !layer->colours) {
    error("rayplot: out of memory");
  }
  memcpy(b->center, REAL(center_), sizeof(double) * nb);
  memcpy(b->quantile1, REAL(q1_), sizeof(double) * nb);
  memcpy(b->median, REAL(med_), sizeof(double) * nb);
  memcpy(b->quantile3, REAL(q3_), sizeof(double) * nb);
  memcpy(b->whisker_low, REAL(wl_), sizeof(double) * nb);
  memcpy(b->whisker_high, REAL(wh_), sizeof(double) * nb);
  memcpy(b->outliers, REAL(out_), sizeof(double) * n_out);
  memcpy(b->outlier_start, INTEGER(ostart_), sizeof(int) * (nb + 1));
  memcpy(layer->colour_groups, INTEGER(cg_), sizeof(int) * nb);
  for (int i = 0; i < n_colours; i++) {
    layer->colours[i] = hex_to_color(CHAR(STRING_ELT(col_, i)));
  }
}

void adopt_ticks(RayPlot *p, SEXP spec) {
  if (p->tick_pos != NULL) return;
  SEXP tp = list_elt(spec, "tick_pos");
  SEXP tl = list_elt(spec, "tick_labels");
  if (tp == R_NilValue || tl == R_NilValue) return;
  if (TYPEOF(tp) != REALSXP || TYPEOF(tl) != STRSXP || LENGTH(tp) != LENGTH(tl)) {
    error("rayplot: tick_pos / tick_labels malformed");
  }
  const int nt = LENGTH(tp);
  p->tick_pos = (double*) malloc(sizeof(double) * (nt > 0 ? nt : 1));
  p->tick_labels = (char**)  calloc(nt > 0 ? nt : 1, sizeof(char*));
  if (!p->tick_pos || !p->tick_labels) {
    error("rayplot: out of memory");
  }
  p->n_ticks = nt;
  memcpy(p->tick_pos, REAL(tp), sizeof(double) * nt);
  for (int i = 0; i < nt; i++) {
    const char *s = CHAR(STRING_ELT(tl, i));
    size_t len = strlen(s) + 1;
    p->tick_labels[i] = (char*) malloc(len);
    if (!p->tick_labels[i]) {
      error("rayplot: out of memory");
    }
    memcpy(p->tick_labels[i], s, len);
  }
  p->view.x_categorical = true;
}

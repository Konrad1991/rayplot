#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "types_3D.h"

#define ORBIT_MIN_PITCH (-1.55334f) // just short of +/-90 degrees, avoids gimbal flip
#define ORBIT_MAX_PITCH   1.55334f
#define ORBIT_MIN_DISTANCE 1.0f
#define ORBIT_MAX_DISTANCE 200.0f

/* pixel radius used for the screen-space nearest-vertex hover test on
 * line3d/area3d/surface3d layers (see line_layer_hit3d() et al.) */
#define HOVER_PIXEL_RADIUS 10.0f

/* tooltip card, matches the 2D renderer's COL_TIP_BG/COL_TIP_TEXT look */
#define COL_TIP_BG3D   ((Color){ 30, 35, 45, 235 })
#define COL_TIP_TEXT3D ((Color){ 240, 242, 246, 255 })

/* default point/line/area colour, matches the old hardcoded (30, 90, 200) blue */
#define DEFAULT_COLOUR3D ((Color){ 30, 90, 200, 255 })

static void orbit_camera_update_position(RayPlot3D *p) {
  float cp = cosf(p->orbit_pitch);
  p->camera.position.x = p->camera.target.x + p->orbit_distance * cp * cosf(p->orbit_yaw);
  p->camera.position.y = p->camera.target.y + p->orbit_distance * sinf(p->orbit_pitch);
  p->camera.position.z = p->camera.target.z + p->orbit_distance * cp * sinf(p->orbit_yaw);
}

// Safely map data from raw arbitrary ranges to a clean visual 3D space (0.0 to 10.0)
static Vector3 map_to_3d_space(RayPlot3D *p, double dx, double dy, double dz) {
  float sx = (p->xmax - p->xmin > 0) ? (float)((dx - p->xmin) / (p->xmax - p->xmin) * 10.0) : 5.0f;
  float sy = (p->ymax - p->ymin > 0) ? (float)((dy - p->ymin) / (p->ymax - p->ymin) * 10.0) : 5.0f;
  float sz = (p->zmax - p->zmin > 0) ? (float)((dz - p->zmin) / (p->zmax - p->zmin) * 10.0) : 5.0f;
  return (Vector3){ sx, sy, sz };
}

/* inverse of map_to_3d_space(): turns a point in the 0..10 display cube
 * (e.g. a ray/mesh hit point that sits ON the rendered surface, not on a
 * grid vertex) back into data units so a hover tooltip can show exactly
 * what's under the cursor. */
static void unmap_from_3d_space(RayPlot3D *p, Vector3 v, double *dx, double *dy, double *dz) {
  *dx = (p->xmax - p->xmin > 0) ? p->xmin + (double)v.x / 10.0 * (p->xmax - p->xmin) : p->xmin;
  *dy = (p->ymax - p->ymin > 0) ? p->ymin + (double)v.y / 10.0 * (p->ymax - p->ymin) : p->ymin;
  *dz = (p->zmax - p->zmin > 0) ? p->zmin + (double)v.z / 10.0 * (p->zmax - p->zmin) : p->zmin;
}

/* ------------------------------------------------------------------ */
/* R list / spec helpers                                              */
/* ------------------------------------------------------------------ */
static SEXP list_elt3d(SEXP list, const char *name) {
  SEXP names = getAttrib(list, R_NamesSymbol);
  if (names == R_NilValue) return R_NilValue;
  for (int i = 0; i < LENGTH(names); i++) {
    if (strcmp(CHAR(STRING_ELT(names, i)), name) == 0) {
      return VECTOR_ELT(list, i);
    }
  }
  return R_NilValue;
}

static Color hex_to_color3d(const char *hex) {
  if (*hex == '#') hex++;
  unsigned long rgb = strtoul(hex, NULL, 16);
  return GetColor((unsigned int)((rgb << 8) | 0xFFu));
}

/* "colour" is optional on every layer spec (translate_point3d() in ggplot.R
 * doesn't set one); missing -> fallback. */
static Color spec_colour3d(SEXP spec, Color fallback) {
  SEXP col_ = list_elt3d(spec, "colour");
  if (col_ == R_NilValue || TYPEOF(col_) != STRSXP || LENGTH(col_) < 1) return fallback;
  return hex_to_color3d(CHAR(STRING_ELT(col_, 0)));
}

/* optional per-point/per-vertex colour override: R resolves either a
 * categorical group->palette lookup or a continuous value->gradient mapping
 * down to plain "#RRGGBB" hex before it gets here, so this only ever deals
 * with final colours. Missing "colours" field -> NULL (caller falls back to
 * the layer's flat colour). */
static Color* copy_colours3d(SEXP spec, const char *ctx, int n) {
  SEXP col_ = list_elt3d(spec, "colours");
  if (col_ == R_NilValue) return NULL;
  if (TYPEOF(col_) != STRSXP) error("rayplot3d: %s colours must be character", ctx);
  if (LENGTH(col_) != n) error("rayplot3d: %s colours must have one entry per point", ctx);
  Color *out = (Color*) malloc(sizeof(Color) * (n > 0 ? n : 1));
  if (!out) error("rayplot3d: out of memory");
  for (int i = 0; i < n; i++) out[i] = hex_to_color3d(CHAR(STRING_ELT(col_, i)));
  return out;
}

static void copy_xyz(SEXP spec, const char *ctx, int *n_out, double **x_out, double **y_out, double **z_out) {
  SEXP x_ = list_elt3d(spec, "x");
  SEXP y_ = list_elt3d(spec, "y");
  SEXP z_ = list_elt3d(spec, "z");
  if (x_ == R_NilValue || y_ == R_NilValue || z_ == R_NilValue) {
    error("rayplot3d: %s layer missing x/y/z", ctx);
  }
  if (TYPEOF(x_) != REALSXP || TYPEOF(y_) != REALSXP || TYPEOF(z_) != REALSXP) {
    error("rayplot3d: %s x/y/z must be double", ctx);
  }
  const int n = LENGTH(x_);
  if (LENGTH(y_) != n || LENGTH(z_) != n) {
    error("rayplot3d: %s x/y/z length differ", ctx);
  }
  double *x = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  double *y = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  double *z = (double*) malloc(sizeof(double) * (n > 0 ? n : 1));
  if (!x || !y || !z) { free(x); free(y); free(z); error("rayplot3d: out of memory"); }
  memcpy(x, REAL(x_), sizeof(double) * n);
  memcpy(y, REAL(y_), sizeof(double) * n);
  memcpy(z, REAL(z_), sizeof(double) * n);
  *n_out = n; *x_out = x; *y_out = y; *z_out = z;
}

static void build_scatter_layer3d(RayLayer3D *layer, SEXP spec) {
  layer->type = LAYER3D_SCATTER;
  ScatterPlot3D *s = &layer->scatter;
  copy_xyz(spec, "scatter3d", &s->n, &s->x, &s->y, &s->z);
  SEXP pr_ = list_elt3d(spec, "point_radius");
  if (pr_ == R_NilValue) error("rayplot3d: scatter3d layer missing point_radius");
  s->point_radius = asReal(pr_);
  s->colours = copy_colours3d(spec, "scatter3d", s->n);
  layer->colour = spec_colour3d(spec, DEFAULT_COLOUR3D);
}

static void build_line_layer3d(RayLayer3D *layer, SEXP spec) {
  layer->type = LAYER3D_LINE;
  LinePlot3D *l = &layer->line;
  copy_xyz(spec, "line3d", &l->n, &l->x, &l->y, &l->z);
  l->colours = copy_colours3d(spec, "line3d", l->n);
  layer->colour = spec_colour3d(spec, DEFAULT_COLOUR3D);
}

static void build_area_layer3d(RayLayer3D *layer, SEXP spec) {
  layer->type = LAYER3D_AREA;
  AreaPlot3D *a = &layer->area;
  copy_xyz(spec, "area3d", &a->n, &a->x, &a->y, &a->z);
  SEXP base_ = list_elt3d(spec, "base_y");
  if (base_ == R_NilValue) error("rayplot3d: area3d layer missing base_y");
  a->base_y = asReal(base_);
  a->colours = copy_colours3d(spec, "area3d", a->n);
  SEXP alpha_ = list_elt3d(spec, "alpha");
  double alpha = (alpha_ == R_NilValue) ? 0.35 : asReal(alpha_);
  if (alpha < 0.0) alpha = 0.0; if (alpha > 1.0) alpha = 1.0;
  layer->colour = spec_colour3d(spec, DEFAULT_COLOUR3D);
  layer->colour.a = (unsigned char)(alpha * 255.0);
  /* per-point hex colours come back fully opaque from copy_colours3d(); bake
   * the same alpha in so the "colours" path stays as translucent as flat. */
  if (a->colours) {
    for (int i = 0; i < a->n; i++) a->colours[i].a = layer->colour.a;
  }
}

/* row_z/col_x share the R spec's "colour"/"alpha" convention with area3d;
 * height is a flat, COLUMN-MAJOR n_rows*n_cols buffer (R's native matrix
 * order), so an R matrix can be handed over with plain as.double(). */
static void build_surface_layer3d(RayLayer3D *layer, SEXP spec) {
  layer->type = LAYER3D_SURFACE;
  SurfacePlot3D *s = &layer->surface;
  SEXP rz_ = list_elt3d(spec, "row_z");
  SEXP cx_ = list_elt3d(spec, "col_x");
  SEXP h_  = list_elt3d(spec, "height");
  if (rz_ == R_NilValue || cx_ == R_NilValue || h_ == R_NilValue) {
    error("rayplot3d: surface3d layer missing row_z/col_x/height");
  }
  if (TYPEOF(rz_) != REALSXP || TYPEOF(cx_) != REALSXP || TYPEOF(h_) != REALSXP) {
    error("rayplot3d: surface3d row_z/col_x/height must be double");
  }
  const int n_rows = LENGTH(rz_);
  const int n_cols = LENGTH(cx_);
  if (n_rows < 2 || n_cols < 2) {
    error("rayplot3d: surface3d needs at least 2 rows and 2 columns");
  }
  if ((double) LENGTH(h_) != (double) n_rows * (double) n_cols) {
    error("rayplot3d: surface3d height must have length(row_z) * length(col_x) entries");
  }
  s->n_rows = n_rows;
  s->n_cols = n_cols;
  s->row_z  = (double*) malloc(sizeof(double) * n_rows);
  s->col_x  = (double*) malloc(sizeof(double) * n_cols);
  s->height = (double*) malloc(sizeof(double) * (size_t) n_rows * (size_t) n_cols);
  if (!s->row_z || !s->col_x || !s->height) error("rayplot3d: out of memory");
  memcpy(s->row_z, REAL(rz_), sizeof(double) * n_rows);
  memcpy(s->col_x, REAL(cx_), sizeof(double) * n_cols);
  memcpy(s->height, REAL(h_), sizeof(double) * (size_t) n_rows * (size_t) n_cols);

  const int total = n_rows * n_cols;
  s->colours = copy_colours3d(spec, "surface3d", total);

  SEXP alpha_ = list_elt3d(spec, "alpha");
  double alpha = (alpha_ == R_NilValue) ? 0.6 : asReal(alpha_);
  if (alpha < 0.0) alpha = 0.0; if (alpha > 1.0) alpha = 1.0;
  layer->colour = spec_colour3d(spec, DEFAULT_COLOUR3D);
  layer->colour.a = (unsigned char)(alpha * 255.0);
  if (s->colours) {
    for (int i = 0; i < total; i++) s->colours[i].a = layer->colour.a;
  }
}

static void free_layer3d(RayLayer3D *layer) {
  switch (layer->type) {
    case LAYER3D_SCATTER:
      free(layer->scatter.x); free(layer->scatter.y); free(layer->scatter.z);
      free(layer->scatter.colours);
      break;
    case LAYER3D_LINE:
      free(layer->line.x); free(layer->line.y); free(layer->line.z);
      free(layer->line.colours);
      break;
    case LAYER3D_AREA:
      free(layer->area.x); free(layer->area.y); free(layer->area.z);
      free(layer->area.colours);
      break;
    case LAYER3D_SURFACE:
      free(layer->surface.row_z); free(layer->surface.col_x); free(layer->surface.height);
      free(layer->surface.colours);
      break;
  }
}

/* fills bounds for one layer (including an area's base_y, so the ribbon's
 * bottom edge is never mapped outside the 0..10 display cube); returns
 * false if the layer has nothing to bound. */
static bool layer_bounds3d(const RayLayer3D *layer,
                           double *xmin, double *xmax,
                           double *ymin, double *ymax,
                           double *zmin, double *zmax) {
  int n; const double *x, *y, *z; double extra_y; bool has_extra_y = false;
  switch (layer->type) {
    case LAYER3D_SURFACE: {
      const SurfacePlot3D *s = &layer->surface;
      if (s->n_rows < 1 || s->n_cols < 1) return false;
      *xmin = *xmax = s->col_x[0];
      for (int j = 1; j < s->n_cols; j++) {
        if (s->col_x[j] < *xmin) *xmin = s->col_x[j];
        if (s->col_x[j] > *xmax) *xmax = s->col_x[j];
      }
      *zmin = *zmax = s->row_z[0];
      for (int i = 1; i < s->n_rows; i++) {
        if (s->row_z[i] < *zmin) *zmin = s->row_z[i];
        if (s->row_z[i] > *zmax) *zmax = s->row_z[i];
      }
      const int total = s->n_rows * s->n_cols;
      *ymin = *ymax = s->height[0];
      for (int k = 1; k < total; k++) {
        if (s->height[k] < *ymin) *ymin = s->height[k];
        if (s->height[k] > *ymax) *ymax = s->height[k];
      }
      return true;
    }
    case LAYER3D_SCATTER: {
      const ScatterPlot3D *s = &layer->scatter;
      n = s->n; x = s->x; y = s->y; z = s->z;
      break;
    }
    case LAYER3D_LINE: {
      const LinePlot3D *l = &layer->line;
      n = l->n; x = l->x; y = l->y; z = l->z;
      break;
    }
    case LAYER3D_AREA: {
      const AreaPlot3D *a = &layer->area;
      n = a->n; x = a->x; y = a->y; z = a->z;
      extra_y = a->base_y; has_extra_y = true;
      break;
    }
    default:
      return false;
  }
  if (n < 1) return false;
  *xmin = *xmax = x[0];
  *ymin = *ymax = y[0];
  *zmin = *zmax = z[0];
  for (int i = 1; i < n; i++) {
    if (x[i] < *xmin) *xmin = x[i]; if (x[i] > *xmax) *xmax = x[i];
    if (y[i] < *ymin) *ymin = y[i]; if (y[i] > *ymax) *ymax = y[i];
    if (z[i] < *zmin) *zmin = z[i]; if (z[i] > *zmax) *zmax = z[i];
  }
  if (has_extra_y) {
    if (extra_y < *ymin) *ymin = extra_y;
    if (extra_y > *ymax) *ymax = extra_y;
  }
  return true;
}

/* ------------------------------------------------------------------ */
/* drawing                                                            */
/* ------------------------------------------------------------------ */
static void draw_scatter_layer3d(RayPlot3D *p, const RayLayer3D *layer) {
  const ScatterPlot3D *s = &layer->scatter;
  for (int i = 0; i < s->n; i++) {
    Vector3 position = map_to_3d_space(p, s->x[i], s->y[i], s->z[i]);
    Color c = s->colours ? s->colours[i] : layer->colour;
    DrawSphere(position, s->point_radius, c);
    DrawSphereWires(position, s->point_radius, 8, 8, (Color){ 0, 0, 0, 40 });
  }
}

static void draw_line_layer3d(RayPlot3D *p, const RayLayer3D *layer) {
  const LinePlot3D *l = &layer->line;
  if (l->colours) {
    /* per-vertex colour: GL_LINES interpolates between each segment's two
     * endpoint colours in hardware, giving a smooth gradient along the
     * trace instead of one flat colour per segment. */
    rlBegin(RL_LINES);
    for (int i = 0; i + 1 < l->n; i++) {
      Vector3 a = map_to_3d_space(p, l->x[i], l->y[i], l->z[i]);
      Vector3 b = map_to_3d_space(p, l->x[i + 1], l->y[i + 1], l->z[i + 1]);
      Color ca = l->colours[i], cb = l->colours[i + 1];
      rlColor4ub(ca.r, ca.g, ca.b, ca.a); rlVertex3f(a.x, a.y, a.z);
      rlColor4ub(cb.r, cb.g, cb.b, cb.a); rlVertex3f(b.x, b.y, b.z);
    }
    rlEnd();
    return;
  }
  for (int i = 0; i + 1 < l->n; i++) {
    Vector3 a = map_to_3d_space(p, l->x[i], l->y[i], l->z[i]);
    Vector3 b = map_to_3d_space(p, l->x[i + 1], l->y[i + 1], l->z[i + 1]);
    DrawLine3D(a, b, layer->colour);
  }
}

/* emits (n-1) quads as rlgl triangles between two parallel point/colour
 * arrays (e.g. a ribbon's top/bottom edge, or two adjacent surface rows).
 * Winding mirrors DrawTriangleStrip3D's own strip decomposition. Must be
 * called between rlBegin(RL_TRIANGLES)/rlEnd() by the caller. */
static void emit_quad_strip_rlgl(const Vector3 *top, const Color *top_c,
                                 const Vector3 *bot, const Color *bot_c, int n) {
  for (int i = 0; i + 1 < n; i++) {
    Vector3 t0 = top[i], t1 = top[i + 1], b0 = bot[i], b1 = bot[i + 1];
    Color ct0 = top_c[i], ct1 = top_c[i + 1], cb0 = bot_c[i], cb1 = bot_c[i + 1];

    rlColor4ub(ct0.r, ct0.g, ct0.b, ct0.a); rlVertex3f(t0.x, t0.y, t0.z);
    rlColor4ub(cb0.r, cb0.g, cb0.b, cb0.a); rlVertex3f(b0.x, b0.y, b0.z);
    rlColor4ub(ct1.r, ct1.g, ct1.b, ct1.a); rlVertex3f(t1.x, t1.y, t1.z);

    rlColor4ub(cb0.r, cb0.g, cb0.b, cb0.a); rlVertex3f(b0.x, b0.y, b0.z);
    rlColor4ub(cb1.r, cb1.g, cb1.b, cb1.a); rlVertex3f(b1.x, b1.y, b1.z);
    rlColor4ub(ct1.r, ct1.g, ct1.b, ct1.a); rlVertex3f(t1.x, t1.y, t1.z);
  }
}

/* ribbon-to-a-plane: a triangle strip between the line and a floor at
 * y = base_y, drawn as one immediate-mode DrawTriangleStrip3D call (built
 * fresh every frame, same as everything else here -- no persistent Mesh),
 * plus a full-opacity top edge so the trace itself stays legible through
 * the translucent fill. */
static void draw_area_layer3d(RayPlot3D *p, const RayLayer3D *layer) {
  const AreaPlot3D *a = &layer->area;
  if (a->n < 2) return;
  Vector3 *top = (Vector3*) malloc(sizeof(Vector3) * a->n);
  Vector3 *bot = (Vector3*) malloc(sizeof(Vector3) * a->n);
  if (!top || !bot) { free(top); free(bot); return; }
  for (int i = 0; i < a->n; i++) {
    top[i] = map_to_3d_space(p, a->x[i], a->y[i], a->z[i]);
    bot[i] = map_to_3d_space(p, a->x[i], a->base_y, a->z[i]);
  }

  if (a->colours) {
    rlBegin(RL_TRIANGLES);
    emit_quad_strip_rlgl(top, a->colours, bot, a->colours, a->n);
    rlEnd();
  } else {
    Vector3 *pts = (Vector3*) malloc(sizeof(Vector3) * 2 * a->n);
    if (pts) {
      for (int i = 0; i < a->n; i++) { pts[2 * i] = top[i]; pts[2 * i + 1] = bot[i]; }
      DrawTriangleStrip3D(pts, 2 * a->n, layer->colour);
      free(pts);
    }
  }

  for (int i = 0; i + 1 < a->n; i++) {
    Color edge = a->colours ? a->colours[i] : layer->colour;
    edge.a = 255;
    DrawLine3D(top[i], top[i + 1], edge);
  }
  free(top); free(bot);
}

#define SURF_AT(s, row, col) ((s)->height[(size_t)(col) * (size_t)(s)->n_rows + (size_t)(row)])

/* shared-grid surface: a strip of triangles between each pair of adjacent
 * rows (e.g. consecutive MS scans, already binned onto a common column
 * grid on the R side), so neighbouring traces connect into one continuous
 * sheet instead of each row getting its own vertical curtain (draw_area_layer3d).
 * One DrawTriangleStrip3D per row pair, rebuilt fresh every frame like
 * everything else here. */
#define SURF_COL_AT(s, row, col) ((s)->colours[(size_t)(col) * (size_t)(s)->n_rows + (size_t)(row)])

static void draw_surface_layer3d(RayPlot3D *p, const RayLayer3D *layer) {
  const SurfacePlot3D *s = &layer->surface;
  Vector3 *row_top = (Vector3*) malloc(sizeof(Vector3) * s->n_cols);
  Vector3 *row_bot = (Vector3*) malloc(sizeof(Vector3) * s->n_cols);
  Color *col_top = s->colours ? (Color*) malloc(sizeof(Color) * s->n_cols) : NULL;
  Color *col_bot = s->colours ? (Color*) malloc(sizeof(Color) * s->n_cols) : NULL;
  if (!row_top || !row_bot || (s->colours && (!col_top || !col_bot))) {
    free(row_top); free(row_bot); free(col_top); free(col_bot);
    return;
  }

  if (s->colours) rlBegin(RL_TRIANGLES);
  for (int i = 0; i + 1 < s->n_rows; i++) {
    for (int j = 0; j < s->n_cols; j++) {
      row_top[j] = map_to_3d_space(p, s->col_x[j], SURF_AT(s, i, j),     s->row_z[i]);
      row_bot[j] = map_to_3d_space(p, s->col_x[j], SURF_AT(s, i + 1, j), s->row_z[i + 1]);
      if (s->colours) {
        col_top[j] = SURF_COL_AT(s, i, j);
        col_bot[j] = SURF_COL_AT(s, i + 1, j);
      }
    }
    if (s->colours) {
      emit_quad_strip_rlgl(row_top, col_top, row_bot, col_bot, s->n_cols);
    } else {
      Vector3 *pts = (Vector3*) malloc(sizeof(Vector3) * 2 * s->n_cols);
      if (pts) {
        for (int j = 0; j < s->n_cols; j++) { pts[2 * j] = row_top[j]; pts[2 * j + 1] = row_bot[j]; }
        DrawTriangleStrip3D(pts, 2 * s->n_cols, layer->colour);
        free(pts);
      }
    }
  }
  if (s->colours) rlEnd();

  free(row_top); free(row_bot); free(col_top); free(col_bot);
}

/* writes a tooltip into out[] and returns true when the mouse ray hits a
 * scatter point in any layer. *best_dist is both the caller's "no hit yet"
 * sentinel and the running nearest-to-camera match (raylib's hit.distance is
 * a true distance along the ray), so it can be threaded across scatter,
 * area and surface layers alike to find one globally nearest "solid" hit --
 * whichever surface is actually closest to the viewer under the cursor,
 * the same way clicking through a real 3D scene would resolve occlusion. */
static bool scatter_layer_hit3d(RayPlot3D *p, const RayLayer3D *layer, Ray mouse_ray,
                                Vector3 *out_pos, float *out_radius, float *best_dist,
                                char *out, size_t outsz) {
  const ScatterPlot3D *s = &layer->scatter;
  bool hit = false;
  for (int i = 0; i < s->n; i++) {
    Vector3 position = map_to_3d_space(p, s->x[i], s->y[i], s->z[i]);
    RayCollision c = GetRayCollisionSphere(mouse_ray, position, s->point_radius);
    if (c.hit && c.distance < *best_dist) {
      *best_dist = c.distance;
      *out_pos = position;
      *out_radius = (float) s->point_radius;
      hit = true;
      snprintf(out, outsz, "X: %.3f\nY: %.3f\nZ: %.3f", s->x[i], s->y[i], s->z[i]);
    }
  }
  return hit;
}

/* area3d/surface3d are filled faces, so hovering should work anywhere over
 * the visible fill (the "peak"), not just near a data vertex -- these test
 * the mouse ray against the exact same quads emit_quad_strip_rlgl()/
 * draw_area_layer3d()/draw_surface_layer3d() render (GetRayCollisionQuad
 * splits p1-p2-p3-p4 into triangles (p1,p2,p4)+(p2,p3,p4), matching that
 * code's (top0,bot0,top1)+(bot0,bot1,top1) winding), so a hit is reported
 * exactly where the rendered surface is under the cursor. */

/* area3d: the ribbon between the data curve and its base plane. The
 * tooltip reports the *top curve's* value interpolated at the hit's
 * position along the segment (not the literal fill point, which could be
 * anywhere between the curve and the base) -- that's the actual data users
 * are hovering to read off a peak. */
static bool area_layer_hit3d(RayPlot3D *p, const RayLayer3D *layer, Ray mouse_ray,
                             Vector3 *out_pos, float *best_dist, char *out, size_t outsz) {
  const AreaPlot3D *a = &layer->area;
  bool hit = false;
  for (int i = 0; i + 1 < a->n; i++) {
    Vector3 top0 = map_to_3d_space(p, a->x[i],     a->y[i],     a->z[i]);
    Vector3 top1 = map_to_3d_space(p, a->x[i + 1], a->y[i + 1], a->z[i + 1]);
    Vector3 bot0 = map_to_3d_space(p, a->x[i],     a->base_y,   a->z[i]);
    Vector3 bot1 = map_to_3d_space(p, a->x[i + 1], a->base_y,   a->z[i + 1]);
    RayCollision c = GetRayCollisionQuad(mouse_ray, top0, bot0, bot1, top1);
    if (c.hit && c.distance < *best_dist) {
      Vector3 dir = Vector3Subtract(top1, top0);
      float denom = Vector3DotProduct(dir, dir);
      float u = (denom > 1e-8f) ? Vector3DotProduct(Vector3Subtract(c.point, top0), dir) / denom : 0.0f;
      if (u < 0.0f) u = 0.0f; if (u > 1.0f) u = 1.0f;
      *best_dist = c.distance;
      *out_pos = c.point;
      hit = true;
      double ix = a->x[i] + (a->x[i + 1] - a->x[i]) * u;
      double iy = a->y[i] + (a->y[i + 1] - a->y[i]) * u;
      double iz = a->z[i] + (a->z[i + 1] - a->z[i]) * u;
      snprintf(out, outsz, "X: %.3f\nY: %.3f\nZ: %.3f\nbase: %.3f", ix, iy, iz, a->base_y);
    }
  }
  return hit;
}

/* surface3d: the hit point sits exactly on the rendered mesh (same
 * triangles as draw_surface_layer3d()), so unmapping it straight back to
 * data space gives the true interpolated (x, height, z) under the cursor --
 * no separate bilinear math needed, it's the same interpolation the GPU
 * already drew. */
static bool surface_layer_hit3d(RayPlot3D *p, const RayLayer3D *layer, Ray mouse_ray,
                                Vector3 *out_pos, float *best_dist, char *out, size_t outsz) {
  const SurfacePlot3D *s = &layer->surface;
  bool hit = false;
  for (int i = 0; i + 1 < s->n_rows; i++) {
    for (int j = 0; j + 1 < s->n_cols; j++) {
      Vector3 t0 = map_to_3d_space(p, s->col_x[j],     SURF_AT(s, i, j),         s->row_z[i]);
      Vector3 t1 = map_to_3d_space(p, s->col_x[j + 1], SURF_AT(s, i, j + 1),     s->row_z[i]);
      Vector3 b0 = map_to_3d_space(p, s->col_x[j],     SURF_AT(s, i + 1, j),     s->row_z[i + 1]);
      Vector3 b1 = map_to_3d_space(p, s->col_x[j + 1], SURF_AT(s, i + 1, j + 1), s->row_z[i + 1]);
      RayCollision c = GetRayCollisionQuad(mouse_ray, t0, b0, b1, t1);
      if (c.hit && c.distance < *best_dist) {
        *best_dist = c.distance;
        *out_pos = c.point;
        hit = true;
        double ix, iy, iz;
        unmap_from_3d_space(p, c.point, &ix, &iy, &iz);
        snprintf(out, outsz, "X: %.3f\nY: %.3f\nZ: %.3f", ix, iy, iz);
      }
    }
  }
  return hit;
}

/* line3d has no face to raycast, so it stays a screen-space nearest-vertex
 * test (projected via the current camera): pick the vertex whose on-screen
 * position is closest to the cursor, within a pixel radius. *best_d2 is
 * both the caller's pixel-radius cutoff and the running best match. */
static bool line_layer_hit3d(RayPlot3D *p, const RayLayer3D *layer, Vector2 mouse,
                             Vector3 *out_pos, float *best_d2, char *out, size_t outsz) {
  const LinePlot3D *l = &layer->line;
  bool hit = false;
  for (int i = 0; i < l->n; i++) {
    Vector3 world = map_to_3d_space(p, l->x[i], l->y[i], l->z[i]);
    Vector2 screen = GetWorldToScreen(world, p->camera);
    float dx = screen.x - mouse.x, dy = screen.y - mouse.y;
    float d2 = dx * dx + dy * dy;
    if (d2 < *best_d2) {
      *best_d2 = d2;
      *out_pos = world;
      hit = true;
      snprintf(out, outsz, "X: %.3f\nY: %.3f\nZ: %.3f", l->x[i], l->y[i], l->z[i]);
    }
  }
  return hit;
}

/* ------------------------------------------------------------------ */
/* lifecycle                                                          */
/* ------------------------------------------------------------------ */
static void rayplot3D_finalize(SEXP ext) {
  RayPlot3D *p = (RayPlot3D *)R_ExternalPtrAddr(ext);
  if (p == NULL) return;
  if (p->is_open && IsWindowReady()) {
    CloseWindow();
  }
  p->is_open = false;
  if (p->layers) {
    for (int k = 0; k < p->n_layers; k++) free_layer3d(&p->layers[k]);
    free(p->layers);
  }
  free(p);
  R_ClearExternalPtr(ext);
}

SEXP rayplot3D_open_(SEXP layers_, SEXP w_, SEXP h_, SEXP title_,
                     SEXP xlab_, SEXP ylab_, SEXP zlab_) {
  SetTraceLogLevel(LOG_ERROR);
  if (TYPEOF(layers_) != VECSXP) {
    error("rayplot3d: layers must be a list");
  }
  const int n_layers = LENGTH(layers_);
  if (n_layers < 1) {
    error("rayplot3d: need at least one layer");
  }

  RayPlot3D *p = (RayPlot3D *)calloc(1, sizeof(RayPlot3D));
  if (!p) error("rayplot3d: out of memory");
  p->layers = (RayLayer3D *) calloc(n_layers, sizeof(RayLayer3D));
  if (!p->layers) { free(p); error("rayplot3d: out of memory"); }
  p->n_layers = n_layers;

  p->camera.target = (Vector3){ 5.0f, 5.0f, 5.0f };
  p->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
  p->camera.fovy = 45.0f;
  p->camera.projection = CAMERA_PERSPECTIVE;

  p->orbit_yaw = 45.0f * DEG2RAD;
  p->orbit_pitch = 35.0f * DEG2RAD;
  p->orbit_distance = 22.5f;
  orbit_camera_update_position(p);

  p->width  = asInteger(w_);
  p->height = asInteger(h_);

  snprintf(p->xlab, sizeof p->xlab, "%s", CHAR(STRING_ELT(xlab_, 0)));
  snprintf(p->ylab, sizeof p->ylab, "%s", CHAR(STRING_ELT(ylab_, 0)));
  snprintf(p->zlab, sizeof p->zlab, "%s", CHAR(STRING_ELT(zlab_, 0)));

  SEXP ext = PROTECT(R_MakeExternalPtr(p, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ext, rayplot3D_finalize, TRUE);

  for (int k = 0; k < n_layers; k++) {
    SEXP spec = VECTOR_ELT(layers_, k);
    if (TYPEOF(spec) != VECSXP) {
      error("rayplot3d: layer %d is not a list", k + 1);
    }
    SEXP type_ = list_elt3d(spec, "type");
    if (type_ == R_NilValue || TYPEOF(type_) != STRSXP || LENGTH(type_) < 1) {
      error("rayplot3d: layer %d has no type", k + 1);
    }
    const char *type = CHAR(STRING_ELT(type_, 0));
    if (strcmp(type, "scatter3d") == 0) {
      build_scatter_layer3d(&p->layers[k], spec);
    } else if (strcmp(type, "line3d") == 0) {
      build_line_layer3d(&p->layers[k], spec);
    } else if (strcmp(type, "area3d") == 0) {
      build_area_layer3d(&p->layers[k], spec);
    } else if (strcmp(type, "surface3d") == 0) {
      build_surface_layer3d(&p->layers[k], spec);
    } else {
      error("rayplot3d: unknown layer type '%s'", type);
    }
  }

  /* union data bounds over all layers, across all 3 axes */
  bool any = false;
  for (int k = 0; k < n_layers; k++) {
    double lxmin, lxmax, lymin, lymax, lzmin, lzmax;
    if (!layer_bounds3d(&p->layers[k], &lxmin, &lxmax, &lymin, &lymax, &lzmin, &lzmax)) continue;
    if (!any) {
      p->xmin = lxmin; p->xmax = lxmax;
      p->ymin = lymin; p->ymax = lymax;
      p->zmin = lzmin; p->zmax = lzmax;
      any = true;
    } else {
      if (lxmin < p->xmin) p->xmin = lxmin; if (lxmax > p->xmax) p->xmax = lxmax;
      if (lymin < p->ymin) p->ymin = lymin; if (lymax > p->ymax) p->ymax = lymax;
      if (lzmin < p->zmin) p->zmin = lzmin; if (lzmax > p->zmax) p->zmax = lzmax;
    }
  }
  if (!any) {
    p->xmin = p->ymin = p->zmin = 0.0;
    p->xmax = p->ymax = p->zmax = 1.0;
  }

  if (IsWindowReady()) CloseWindow(); /* raylib is single-window: drop any stale one */
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(p->width, p->height, CHAR(STRING_ELT(title_, 0)));
  SetTargetFPS(60);
  p->is_open = true;

  UNPROTECT(1);
  return ext;
}

SEXP rayplot3D_step_(SEXP ext) {
  RayPlot3D *p = (RayPlot3D *)R_ExternalPtrAddr(ext);
  if (p == NULL || !p->is_open) return R_NilValue;

  if (IsWindowResized()) {
    p->width = GetScreenWidth();
    p->height = GetScreenHeight();
  }
  // -------------------------------------------------------------
  // Orbit camera: rotate/zoom/pan all act around camera.target (the data),
  // so the point cloud stays centered on screen instead of drifting off
  // or spinning around the viewer, as it did with the old FPS-style camera.

  // 1. Rotate around the target via LEFT Click Drag
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 mouse_delta = GetMouseDelta();
    p->orbit_yaw   -= mouse_delta.x * 0.005f;
    p->orbit_pitch += mouse_delta.y * 0.005f;
    if (p->orbit_pitch < ORBIT_MIN_PITCH) p->orbit_pitch = ORBIT_MIN_PITCH;
    if (p->orbit_pitch > ORBIT_MAX_PITCH) p->orbit_pitch = ORBIT_MAX_PITCH;
  }

  // 2. Pan the target via RIGHT Click Drag, moving along the camera's own
  //    screen-space right/up axes so the drag direction always matches the
  //    current view, and scaling with distance so pan speed feels constant
  //    on screen whether zoomed in or out.
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    Vector2 mouse_delta = GetMouseDelta();
    Vector3 forward = Vector3Normalize(Vector3Subtract(p->camera.target, p->camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, p->camera.up));
    Vector3 up = Vector3CrossProduct(right, forward);
    float pan_scale = p->orbit_distance * 0.0015f;
    Vector3 pan = Vector3Add(
      Vector3Scale(right, -mouse_delta.x * pan_scale),
      Vector3Scale(up,     mouse_delta.y * pan_scale)
    );
    p->camera.target = Vector3Add(p->camera.target, pan);
  }

  // 3. Zoom via Scroll Wheel, scaled by current distance so it feels
  //    consistent whether zoomed far in or far out.
  float mouse_wheel = GetMouseWheelMove();
  if (mouse_wheel != 0.0f) {
    p->orbit_distance -= mouse_wheel * p->orbit_distance * 0.1f;
    if (p->orbit_distance < ORBIT_MIN_DISTANCE) p->orbit_distance = ORBIT_MIN_DISTANCE;
    if (p->orbit_distance > ORBIT_MAX_DISTANCE) p->orbit_distance = ORBIT_MAX_DISTANCE;
  }

  // 4. Re-derive the camera position from target + spherical coordinates.
  orbit_camera_update_position(p);
  // -------------------------------------------------------------

  Vector2 mouse_pos = GetMousePosition();
  Ray mouse_ray = GetScreenToWorldRay(mouse_pos, p->camera);

  bool collision_detected = false;
  Vector3 matched_visual_pos = { 0.0f, 0.0f, 0.0f };
  float matched_radius = 0.1f;
  char tooltip_str[128];

  /* "solid" layers (scatter spheres, area ribbon faces, surface mesh faces)
   * all yield a true distance along the ray, so whichever one is actually
   * closest to the camera under the cursor wins -- correct occlusion
   * instead of scatter always taking priority regardless of depth. */
  {
    float best_dist = INFINITY;
    for (int k = 0; k < p->n_layers; k++) {
      bool hit = false;
      switch (p->layers[k].type) {
        case LAYER3D_SCATTER:
          hit = scatter_layer_hit3d(p, &p->layers[k], mouse_ray, &matched_visual_pos,
                                    &matched_radius, &best_dist, tooltip_str, sizeof tooltip_str);
          break;
        case LAYER3D_AREA:
          hit = area_layer_hit3d(p, &p->layers[k], mouse_ray, &matched_visual_pos,
                                 &best_dist, tooltip_str, sizeof tooltip_str);
          break;
        case LAYER3D_SURFACE:
          hit = surface_layer_hit3d(p, &p->layers[k], mouse_ray, &matched_visual_pos,
                                    &best_dist, tooltip_str, sizeof tooltip_str);
          break;
        default:
          break;
      }
      if (hit) { collision_detected = true; if (p->layers[k].type != LAYER3D_SCATTER) matched_radius = 0.12f; }
    }
  }

  /* line3d has no face to raycast, so it only gets a screen-space
   * nearest-vertex fallback, and only when nothing solid was hit. */
  if (!collision_detected) {
    float best_d2 = HOVER_PIXEL_RADIUS * HOVER_PIXEL_RADIUS;
    for (int k = 0; k < p->n_layers; k++) {
      if (p->layers[k].type != LAYER3D_LINE) continue;
      if (line_layer_hit3d(p, &p->layers[k], mouse_pos, &matched_visual_pos,
                           &best_d2, tooltip_str, sizeof tooltip_str)) {
        collision_detected = true;
        matched_radius = 0.12f;
      }
    }
  }

  BeginDrawing();
  ClearBackground((Color){ 245, 245, 245, 255 });

  BeginMode3D(p->camera);
    DrawGrid(20, 1.0f);

    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 12, 0, 0 }, RED);
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 0, 12, 0 }, GREEN);
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 0, 0, 12 }, BLUE);

    // surfaces/areas first (translucent fill), then lines, then points on
    // top, so points/lines stay visible through/above the fill
    for (int k = 0; k < p->n_layers; k++) {
      if (p->layers[k].type == LAYER3D_SURFACE) draw_surface_layer3d(p, &p->layers[k]);
    }
    for (int k = 0; k < p->n_layers; k++) {
      if (p->layers[k].type == LAYER3D_AREA) draw_area_layer3d(p, &p->layers[k]);
    }
    for (int k = 0; k < p->n_layers; k++) {
      if (p->layers[k].type == LAYER3D_LINE) draw_line_layer3d(p, &p->layers[k]);
    }
    for (int k = 0; k < p->n_layers; k++) {
      if (p->layers[k].type == LAYER3D_SCATTER) draw_scatter_layer3d(p, &p->layers[k]);
    }

    if (collision_detected) {
      DrawSphereWires(matched_visual_pos, matched_radius * 1.3f, 10, 10, RED);
    }
  EndMode3D();

  /* axis titles, anchored at the same points as the RED/GREEN/BLUE
   * reference lines drawn inside BeginMode3D(), projected to screen space
   * so the text stays flat/readable as the camera orbits. */
  {
    const float fs = 18.0f;
    Vector2 xs = GetWorldToScreen((Vector3){ 12.5f, 0, 0 }, p->camera);
    Vector2 ys = GetWorldToScreen((Vector3){ 0, 12.5f, 0 }, p->camera);
    Vector2 zs = GetWorldToScreen((Vector3){ 0, 0, 12.5f }, p->camera);
    DrawText(p->xlab, (int)xs.x, (int)xs.y, fs, RED);
    DrawText(p->ylab, (int)ys.x, (int)ys.y, fs, DARKGREEN);
    DrawText(p->zlab, (int)zs.x, (int)zs.y, fs, BLUE);
  }

  DrawText("Left-Click + Drag: Orbit  |  Right-Click + Drag: Pan  |  Scroll Wheel: Zoom", 15, 15, 16, DARKGRAY);

  if (collision_detected) {
    const float fs = 16.0f;
    const float pad = 6.0f;
    Vector2 ts = MeasureTextEx(GetFontDefault(), tooltip_str, fs, 2.0f);
    float tx = mouse_pos.x + 15.0f;
    float ty = mouse_pos.y + 4.0f;
    if (tx + ts.x + pad > p->width) tx = mouse_pos.x - 15.0f - ts.x;
    if (ty + ts.y + pad > p->height) ty = p->height - ts.y - pad;
    Rectangle card = { tx - pad, ty - pad, ts.x + 2.0f * pad, ts.y + 2.0f * pad };
    DrawRectangleRounded(card, 0.25f, 6, COL_TIP_BG3D);
    DrawTextEx(GetFontDefault(), tooltip_str, (Vector2){ tx, ty }, fs, 2.0f, COL_TIP_TEXT3D);
  }

  EndDrawing();
  return R_NilValue;
}

SEXP rayplot3D_close_(SEXP ext) {
  RayPlot3D *p = (RayPlot3D *) R_ExternalPtrAddr(ext);
  if (p == NULL) return R_NilValue;
  p->is_open = false;
  if (IsWindowReady()) CloseWindow();
  return R_NilValue;
}

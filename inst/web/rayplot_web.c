/* rayplot WebAssembly entry point (Model A: R serialises, the browser renders).
 *
 * Built with Emscripten by build.sh -- NOT part of the R package compile.
 * Reuses the desktop render core unchanged: view_2D.c, layers_2D.c, text_2D.c.
 *
 * R hands us a little-endian binary blob (see rayplot_serialize() in R/web.R):
 *
 *   magic     char[4]   "RPLT"
 *   version   u32       1
 *   width     u32
 *   height    u32
 *   n_ticks   u32
 *     tick_pos    f64[n_ticks]
 *     tick_labels n_ticks * (u32 len, char utf8[len])
 *   n_layers  u32
 *   layers    n_layers * {
 *     type       u32            0 = scatter, 1 = box
 *     n_colours  u32
 *     colours    u8[n_colours][4]   r,g,b,a
 *     -- scatter --
 *       n            u32
 *       point_radius f64
 *       x            f64[n]
 *       y            f64[n]
 *       colour_grp   i32[n]
 *     -- box --
 *       n_boxes      u32
 *       box_width    f64
 *       center,q1,median,q3,whisker_low,whisker_high  each f64[n_boxes]
 *       colour_grp   i32[n_boxes]
 *       n_outliers   u32
 *       outliers     f64[n_outliers]
 *       outlier_start i32[n_boxes + 1]
 *   }
 */
#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "types_2D.h"
#include "view_2D.h"
#include "layers_2D.h"
#include "text_2D.h"

static RayPlot g_plot;

/* ---- cursor over the blob, little-endian reads ---- */
typedef struct { const uint8_t *p; } Cur;

static uint32_t rd_u32(Cur *c) { uint32_t v; memcpy(&v, c->p, 4); c->p += 4; return v; }
static double   rd_f64(Cur *c) { double   v; memcpy(&v, c->p, 8); c->p += 8; return v; }

static double *rd_f64_arr(Cur *c, uint32_t n) {
  double *a = (double *) malloc(sizeof(double) * (n ? n : 1));
  memcpy(a, c->p, sizeof(double) * n);
  c->p += sizeof(double) * n;
  return a;
}
static int *rd_i32_arr(Cur *c, uint32_t n) {
  int *a = (int *) malloc(sizeof(int) * (n ? n : 1));
  memcpy(a, c->p, sizeof(int) * n);   /* int is 32-bit under wasm32 */
  c->p += sizeof(int) * n;
  return a;
}

static void parse_blob(const uint8_t *data, int len) {
  (void) len;
  Cur cur = { data };
  Cur *c = &cur;

  c->p += 4;                 /* "RPLT" */
  (void) rd_u32(c);          /* version */
  g_plot.view.width  = (int) rd_u32(c);
  g_plot.view.height = (int) rd_u32(c);

  uint32_t n_ticks = rd_u32(c);
  if (n_ticks > 0) {
    g_plot.n_ticks = (int) n_ticks;
    g_plot.tick_pos = rd_f64_arr(c, n_ticks);
    g_plot.tick_labels = (char **) calloc(n_ticks, sizeof(char *));
    for (uint32_t i = 0; i < n_ticks; i++) {
      uint32_t l = rd_u32(c);
      char *s = (char *) malloc(l + 1);
      memcpy(s, c->p, l);
      s[l] = '\0';
      c->p += l;
      g_plot.tick_labels[i] = s;
    }
    g_plot.view.x_categorical = true;
  }

  uint32_t n_layers = rd_u32(c);
  g_plot.n_layers = (int) n_layers;
  g_plot.layers = (RayLayer *) calloc(n_layers ? n_layers : 1, sizeof(RayLayer));

  for (uint32_t k = 0; k < n_layers; k++) {
    RayLayer *L = &g_plot.layers[k];
    uint32_t type  = rd_u32(c);
    uint32_t n_col = rd_u32(c);
    L->n_colours = (int) n_col;
    L->colours = (Color *) malloc(sizeof(Color) * (n_col ? n_col : 1));
    memcpy(L->colours, c->p, 4u * n_col);   /* Color is r,g,b,a bytes */
    c->p += 4u * n_col;

    if (type == 0u) {
      L->type = LAYER_SCATTER;
      uint32_t n = rd_u32(c);
      L->scatter_plot.n = (int) n;
      L->scatter_plot.point_radius = rd_f64(c);
      L->scatter_plot.x = rd_f64_arr(c, n);
      L->scatter_plot.y = rd_f64_arr(c, n);
      L->colour_groups = rd_i32_arr(c, n);
    } else {
      L->type = LAYER_BOX;
      BoxPlot *b = &L->box_plot;
      uint32_t nb = rd_u32(c);
      b->n_boxes = (int) nb;
      b->box_width = rd_f64(c);
      b->center       = rd_f64_arr(c, nb);
      b->quantile1    = rd_f64_arr(c, nb);
      b->median       = rd_f64_arr(c, nb);
      b->quantile3    = rd_f64_arr(c, nb);
      b->whisker_low  = rd_f64_arr(c, nb);
      b->whisker_high = rd_f64_arr(c, nb);
      L->colour_groups = rd_i32_arr(c, nb);
      uint32_t no = rd_u32(c);
      b->outliers = rd_f64_arr(c, no);
      b->outlier_start = rd_i32_arr(c, nb + 1u);
    }
  }
}

/* release everything parse_blob() malloc'd, so an update doesn't leak */
static void free_plot_contents(void) {
  for (int k = 0; k < g_plot.n_layers; k++) {
    RayLayer *L = &g_plot.layers[k];
    if (L->type == LAYER_SCATTER) {
      free(L->scatter_plot.x);
      free(L->scatter_plot.y);
    } else {
      free(L->box_plot.center);
      free(L->box_plot.quantile1);
      free(L->box_plot.median);
      free(L->box_plot.quantile3);
      free(L->box_plot.whisker_low);
      free(L->box_plot.whisker_high);
      free(L->box_plot.outliers);
      free(L->box_plot.outlier_start);
    }
    free(L->colour_groups);
    free(L->colours);
  }
  free(g_plot.layers);
  g_plot.layers = NULL;
  g_plot.n_layers = 0;

  free(g_plot.tick_pos);
  g_plot.tick_pos = NULL;
  for (int i = 0; i < g_plot.n_ticks; i++) free(g_plot.tick_labels[i]);
  free(g_plot.tick_labels);
  g_plot.tick_labels = NULL;
  g_plot.n_ticks = 0;
  g_plot.view.x_categorical = false;
}

static void fit_to_data(void) {
  bool any = false;
  double xmn = 0, xmx = 1, ymn = 0, ymx = 1;
  for (int k = 0; k < g_plot.n_layers; k++) {
    double a, bb, cc, d;
    if (!layer_bounds(&g_plot.layers[k], &a, &bb, &cc, &d)) continue;
    if (!any) { xmn = a; xmx = bb; ymn = cc; ymx = d; any = true; }
    else {
      if (a  < xmn) xmn = a;
      if (bb > xmx) xmx = bb;
      if (cc < ymn) ymn = cc;
      if (d  > ymx) ymx = d;
    }
  }
  g_plot.view.xmin = xmn; g_plot.view.xmax = xmx;
  g_plot.view.ymin = ymn; g_plot.view.ymax = ymx;
}

static void frame(void) {
  if (!g_plot.is_open) return;
  /* Fixed-size canvas on the web (see template.html) -- no resize handling,
     which keeps raylib's mouse mapping 1:1 with the drawing buffer. */
  handle_input(&g_plot);
  draw_frame(&g_plot);
}

static void load_blob(const uint8_t *data, int len) {
  free_plot_contents();
  parse_blob(data, len);
  fit_to_data();
  fit_view(&g_plot.view);
}

/* Feed a serialised layer spec to the renderer. `data` points into the wasm
 * heap (JS fills it via HEAPU8.set()); `len` is its length in bytes.
 *
 * The first call also creates the window and starts the render loop; every
 * later call just swaps in the new data. Both entry points are safe to call
 * in either order -- rayplot_web_update() bootstraps if it somehow runs first. */
EMSCRIPTEN_KEEPALIVE
void rayplot_web_start(const uint8_t *data, int len) {
  if (!g_plot.is_open) {
    load_blob(data, len);

    /* HIGHDPI: render at the device pixel ratio so it isn't blurry on retina
       screens; raylib keeps GetMousePosition() in logical (CSS) coordinates. */
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(g_plot.view.width, g_plot.view.height, "rayplot");
    rayfont_load(&g_plot.font, "Lato-Regular.ttf");   /* --embed-file'd */
    g_plot.is_open = true;

    /* 0 = register the RAF loop and return to the JS caller (no stack-unwind
       exception, unlike the "1" form used when driving from C main()). */
    emscripten_set_main_loop(frame, 0, 0);
  } else {
    load_blob(data, len);
  }
}

EMSCRIPTEN_KEEPALIVE
void rayplot_web_update(const uint8_t *data, int len) {
  rayplot_web_start(data, len);
}

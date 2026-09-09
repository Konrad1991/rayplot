#include "lifecycle_2D.h"

/* ------------------------------------------------------------------ */
/* lifecycle                                                          */
/* ------------------------------------------------------------------ */
void free_layer(RayLayer *layer) {
  switch (layer->type) {
    case LAYER_SCATTER:
      free(layer->scatter_plot.x);
      free(layer->scatter_plot.y);
      break;
    case LAYER_BOX:
      free(layer->box_plot.center);
      free(layer->box_plot.quantile1);
      free(layer->box_plot.median);
      free(layer->box_plot.quantile3);
      free(layer->box_plot.whisker_low);
      free(layer->box_plot.whisker_high);
      free(layer->box_plot.outliers);
      free(layer->box_plot.outlier_start);
      break;
  }
  free(layer->colour_groups);
  free(layer->colours);
}

void rayplot_finalize(SEXP ext) {
  RayPlot *p = (RayPlot *) R_ExternalPtrAddr(ext);
  if (p == NULL) return;
  if (p->is_open && IsWindowReady()) {
    rayfont_unload(&p->font);
    CloseWindow();
  }
  p->is_open = false;
  if (p->layers) {
    for (int k = 0; k < p->n_layers; k++) {
      free_layer(&p->layers[k]);
    }
    free(p->layers);
  }
  free(p->tick_pos);
  if (p->tick_labels) {
    for (int i = 0; i < p->n_ticks; i++) {
      free(p->tick_labels[i]);
    }
    free(p->tick_labels);
  }
  free(p);
  R_ClearExternalPtr(ext);
}

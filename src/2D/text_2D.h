#ifndef TEXT_2D_RAYPLOT_H
#define TEXT_2D_RAYPLOT_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* light theme                                                        */
/* ------------------------------------------------------------------ */
#define COL_BG         (Color){ 248, 249, 251, 255 }   /* window background   */
#define COL_GRID       (Color){ 228, 231, 237, 255 }   /* grid lines         */
#define COL_AXIS       (Color){ 120, 128, 141, 255 }   /* x / y axis lines   */
#define COL_LABEL      (Color){ 88,  96,  110, 255 }   /* tick labels        */
#define COL_HELP       (Color){ 150, 157, 168, 255 }   /* help hint text     */
#define COL_BOX_STROKE (Color){ 52,  59,  72,  255 }   /* boxplot outline    */
#define COL_TIP_BG     (Color){ 30,  35,  45,  235 }   /* tooltip card       */
#define COL_TIP_TEXT   (Color){ 240, 242, 246, 255 }   /* tooltip text       */

/* ------------------------------------------------------------------ */
/* optional TTF font, falls back to raylib's built-in bitmap font     */
/* ------------------------------------------------------------------ */
typedef struct {
  Font font;
  bool ok;
  float spacing;
} RayFont;

/* load a TTF; path may be NULL/"" -> stays on the built-in font */
void rayfont_load(RayFont *f, const char *path);
void rayfont_unload(RayFont *f);

void ray_text(const RayFont *f, const char *s, float x, float y,
              float size, Color c);
Vector2 ray_measure(const RayFont *f, const char *s, float size);

#endif /* TEXT_2D_RAYPLOT_H */

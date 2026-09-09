#include "text_2D.h"

/* atlas is baked once at this size and scaled down when drawn, so small
   label sizes stay crisp */
#define ATLAS_SIZE 48

void rayfont_load(RayFont *f, const char *path) {
  f->font = GetFontDefault();
  f->ok = false;
  f->spacing = 0.0f;
  if (path == NULL || path[0] == '\0' || !FileExists(path)) return;

  Font loaded = LoadFontEx(path, ATLAS_SIZE, NULL, 0);
  if (loaded.texture.id == 0 || loaded.glyphCount == 0) {
    if (loaded.texture.id != 0) UnloadFont(loaded);
    return;
  }
  SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
  f->font = loaded;
  f->ok = true;
  f->spacing = 0.5f;
}

void rayfont_unload(RayFont *f) {
  if (f->ok) {
    UnloadFont(f->font);
    f->ok = false;
  }
}

void ray_text(const RayFont *f, const char *s, float x, float y,
              float size, Color c) {
  if (f != NULL && f->ok) {
    DrawTextEx(f->font, s, (Vector2){ x, y }, size, f->spacing, c);
  } else {
    DrawText(s, (int)x, (int)y, (int)size, c);
  }
}

Vector2 ray_measure(const RayFont *f, const char *s, float size) {
  if (f != NULL && f->ok) {
    return MeasureTextEx(f->font, s, size, f->spacing);
  }
  return (Vector2){ (float)MeasureText(s, (int)size), size };
}

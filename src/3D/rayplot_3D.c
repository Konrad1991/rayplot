#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"

typedef struct {
  bool is_open;
  int width;
  int height;
  int n;
  double* x;
  double* y;
  double* z;

  // Real-world bounding boxes for scaling data
  double xmin, xmax;
  double ymin, ymax;
  double zmin, zmax;

  Camera3D camera;
  float point_radius;

  // Orbit-camera state: the camera position is derived each frame from
  // these spherical coordinates around camera.target, so rotate/zoom/pan
  // always act on the data instead of spinning the camera in place.
  float orbit_yaw;
  float orbit_pitch;
  float orbit_distance;
} RayPlot3D;

#define ORBIT_MIN_PITCH (-1.55334f) // just short of +/-90 degrees, avoids gimbal flip
#define ORBIT_MAX_PITCH   1.55334f
#define ORBIT_MIN_DISTANCE 1.0f
#define ORBIT_MAX_DISTANCE 200.0f

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

static void rayplot3D_finalize(SEXP ext) {
  RayPlot3D *p = (RayPlot3D *)R_ExternalPtrAddr(ext);
  if (p == NULL) return;
  if (p->is_open && IsWindowReady()) {
    CloseWindow();
  }
  p->is_open = false;
  free(p->x); free(p->y); free(p->z);
  free(p);
  R_ClearExternalPtr(ext);
}

SEXP rayplot3D_open_(SEXP x_, SEXP y_, SEXP z_, SEXP w_, SEXP h_, SEXP title_, SEXP point_radius) {
  SetTraceLogLevel(LOG_ERROR);
  int n = LENGTH(x_);
  RayPlot3D *p = (RayPlot3D *)calloc(1, sizeof(RayPlot3D));
  if (!p) error("rayplot3d: out of memory");

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
  p->n      = n;
  p->point_radius = (float)*(REAL(point_radius));

  p->x = (double *)malloc(sizeof(double) * (n > 0 ? n : 1));
  p->y = (double *)malloc(sizeof(double) * (n > 0 ? n : 1));
  p->z = (double *)malloc(sizeof(double) * (n > 0 ? n : 1));
  if (!p->x || !p->y || !p->z) { free(p->x); free(p->y); free(p->z); free(p); error("rayplot3d: out of memory"); }

  double *xr = REAL(x_), *yr = REAL(y_), *zr = REAL(z_);
  memcpy(p->x, xr, sizeof(double) * n);
  memcpy(p->y, yr, sizeof(double) * n);
  memcpy(p->z, zr, sizeof(double) * n);

  // Compute boundaries across all 3 dimensions
  p->xmin = p->xmax = (n > 0) ? xr[0] : 0.0;
  p->ymin = p->ymax = (n > 0) ? yr[0] : 0.0;
  p->zmin = p->zmax = (n > 0) ? zr[0] : 0.0;
  for (int i = 1; i < n; i++) {
    if (xr[i] < p->xmin) p->xmin = xr[i]; if (xr[i] > p->xmax) p->xmax = xr[i];
    if (yr[i] < p->ymin) p->ymin = yr[i]; if (yr[i] > p->ymax) p->ymax = yr[i];
    if (zr[i] < p->zmin) p->zmin = zr[i]; if (zr[i] > p->zmax) p->zmax = zr[i];
  }

  if (IsWindowReady()) CloseWindow(); /* raylib is single-window: drop any stale one */
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(p->width, p->height, CHAR(STRING_ELT(title_, 0)));
  SetTargetFPS(60);
  p->is_open = true;

  SEXP ext = PROTECT(R_MakeExternalPtr(p, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ext, rayplot3D_finalize, TRUE);
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
  int matched_index = -1;

  for (int i = 0; i < p->n; i++) {
    Vector3 point_pos = map_to_3d_space(p, p->x[i], p->y[i], p->z[i]);
    RayCollision hit = GetRayCollisionSphere(mouse_ray, point_pos, p->point_radius);
    if (hit.hit) {
      collision_detected = true;
      matched_visual_pos = point_pos;
      matched_index = i;
      break;
    }
  }

  BeginDrawing();
  ClearBackground((Color){ 245, 245, 245, 255 });
  
  BeginMode3D(p->camera);
    DrawGrid(20, 1.0f);
    
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 12, 0, 0 }, RED);   
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 0, 12, 0 }, GREEN); 
    DrawLine3D((Vector3){ 0, 0, 0 }, (Vector3){ 0, 0, 12 }, BLUE);  

    for (int i = 0; i < p->n; i++) {
      Vector3 position = map_to_3d_space(p, p->x[i], p->y[i], p->z[i]);
      DrawSphere(position, p->point_radius, (Color){ 30, 90, 200, 255 });
      DrawSphereWires(position, p->point_radius, 8, 8, (Color){ 0, 0, 0, 40 });
    }

    if (collision_detected) {
      DrawSphereWires(matched_visual_pos, p->point_radius * 1.3f, 10, 10, RED);
    }
  EndMode3D();

  DrawText("Left-Click + Drag: Orbit  |  Right-Click + Drag: Pan  |  Scroll Wheel: Zoom", 15, 15, 16, DARKGRAY);

  if (collision_detected && matched_index != -1) {
    char tooltip_str[128];
    snprintf(tooltip_str, sizeof(tooltip_str), "X: %.3f\nY: %.3f\nZ: %.3f", 
             p->x[matched_index], p->y[matched_index], p->z[matched_index]);
    
    DrawTextEx(GetFontDefault(), tooltip_str, (Vector2){ mouse_pos.x + 15, mouse_pos.y }, 16.0f, 2.0f, BLACK);
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

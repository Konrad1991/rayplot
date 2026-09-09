#!/usr/bin/env bash
#
# Build the rayplot WebAssembly renderer with Emscripten.
#
#   Prereq:  a working Emscripten SDK on PATH (`emcc --version`).
#   Run:     inst/web/build.sh            (from anywhere)
#   Output:  inst/web/rayplot.js + inst/web/rayplot.wasm
#
# These outputs are what R/web.R -> rayplot_web() copies next to the generated
# HTML page. They are NOT built by `R CMD INSTALL`; check them in (or rebuild
# on demand) once you have Emscripten.
#
# STATUS: written against raylib 5.5's documented PLATFORM_WEB flags but not
# exercised in CI. If the module fails to start with an Asyncify error, add
# `-sASYNCIFY` to EM_FLAGS below.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
pkg="$(cd "$here/../.." && pwd)"
raylib="$pkg/src/raylib"
core="$pkg/src/2D"
out="$pkg/inst/web"
font="$pkg/inst/fonts/Lato-Regular.ttf"

command -v emcc >/dev/null || { echo "emcc not found on PATH"; exit 1; }

RAYLIB_SRC=(
  "$raylib/rcore.c"
  "$raylib/rshapes.c"
  "$raylib/rtextures.c"
  "$raylib/rtext.c"
  "$raylib/rmodels.c"
  "$raylib/utils.c"
)

# render core shared with the desktop build (no R headers)
CORE_SRC=(
  "$core/view_2D.c"
  "$core/layers_2D.c"
  "$core/text_2D.c"
  "$here/rayplot_web.c"
)

EM_FLAGS=(
  -Os -std=gnu11 -Wall
  -I"$core" -I"$raylib"
  -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2
  -sUSE_GLFW=3
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=33554432
  -sEXPORTED_FUNCTIONS=_rayplot_web_start,_rayplot_web_update,_malloc,_free
  -sEXPORTED_RUNTIME_METHODS=ccall,HEAPU8
  -sENVIRONMENT=web
  --embed-file "${font}@Lato-Regular.ttf"
)

mkdir -p "$out"
emcc "${RAYLIB_SRC[@]}" "${CORE_SRC[@]}" "${EM_FLAGS[@]}" -o "$out/rayplot.js"

echo "wrote $out/rayplot.js and $out/rayplot.wasm"

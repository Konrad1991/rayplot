# rayplot — WebAssembly renderer (Model A)

R builds the plot spec and serialises it; the browser renders it with an
Emscripten build of the raylib render core. R is not involved once the page
loads.

```
ggplot(p) ──ggplot_build──► per-layer spec ──rayplot_serialize()──► binary blob
                                                                        │ base64
                                                                        ▼
                                          rayplot_web() writes  page.html
                                                                        │ loads
                                    rayplot.js + rayplot.wasm ◄──────────┘
                                          │ _rayplot_web_start(ptr,len)
                                          ▼
                          same view_2D / layers_2D / text_2D as the desktop build
```

## Files

| file | |
|---|---|
| `rayplot_web.c` | Emscripten entry point: parses the blob, runs `emscripten_set_main_loop`. Blob format documented at the top of the file. |
| `build.sh` | compiles `rayplot_web.c` + the shared render core + raylib (`PLATFORM_WEB`) into `rayplot.js` / `rayplot.wasm`. |
| `template.html` | page shell; `rayplot_web()` substitutes `@@SPEC_B64@@`, `@@WIDTH@@`, `@@HEIGHT@@`, `@@TITLE@@`. |
| `rayplot.js`, `rayplot.wasm` | build outputs (not produced by `R CMD INSTALL`). |

## Build

Needs a working [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
on `PATH`:

```sh
source /path/to/emsdk/emsdk_env.sh
inst/web/build.sh
```

Then from R:

```r
rayplot_web(p, "plot.html")
```

Browsers block `fetch()` of `.wasm` over `file://`, so serve the folder:

```sh
cd $(dirname plot.html) && python3 -m http.server
```

## Shiny (no iframe)

`rayplot_web()` above writes a standalone page. Inside Shiny you instead keep
one `<canvas>` and push updates over the existing websocket:

```r
ui <- fluidPage(
  rayplotCanvas("plot", 900, 620)          # canvas + JS dependency
)
server <- function(input, output, session) {
  observe({
    p <- ggplot(...) + ...
    rayplot_send(p, "plot", width = 900L, height = 620L)   # few-KB message
  })
}
```

* `rayplotCanvas()` attaches `shiny-rayplot.js` as an `htmlDependency`, which
  also serves `rayplot.js` / `rayplot.wasm` from this directory.
* `shiny-rayplot.js` boots the Emscripten module against the canvas and
  registers a `"rayplot"` custom-message handler.
* First `rayplot_send()` calls `rayplot_web_start`, later ones
  `rayplot_web_update` — which frees the old spec, parses the new blob and
  refits, without re-creating the window/loop.
* One canvas per page (raylib is single-window inside the module too).

## Status

Compiles and runs (raylib 5.5, `PLATFORM_WEB`, GLFW3, GLES2). Verified: desktop
window, standalone page, and the Shiny message plumbing. Known rough edges:

* fixed canvas size — no responsive resize wiring yet;
* `rayplot_web_update` always refits (pan/zoom resets on every update);
* add `-sASYNCIFY` to `EM_FLAGS` if a future change introduces a blocking call;
* `-DGRAPHICS_API_OPENGL_ES3` if you hit ES2 shader limits.

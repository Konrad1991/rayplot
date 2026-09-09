# Shiny integration (approach B): one <canvas>, the wasm module loaded once, and
# each update pushed as a small serialised blob over Shiny's websocket -- no
# iframe, no temp files, no page reload. See inst/web/shiny-rayplot.js.

rayplot_dependency <- function() {
  if (!requireNamespace("htmltools", quietly = TRUE)) {
    stop("rayplot's Shiny helpers need the 'htmltools' package.", call. = FALSE)
  }
  htmltools::htmlDependency(
    name = "rayplot-web",
    version = as.character(utils::packageVersion("rayplot")),
    src = c(file = system.file("web", package = "rayplot")),
    script = "shiny-rayplot.js"
  )
}

# UI: a <canvas> the wasm renderer draws into, with the JS dependency attached.
# `width`/`height` are the drawing-buffer size in pixels and must match the
# values passed to rayplot_send().
rayplotCanvas <- function(id = "rayplot", width = 800L, height = 600L) {
  if (!requireNamespace("htmltools", quietly = TRUE)) {
    stop("rayplotCanvas() needs the 'htmltools' package.", call. = FALSE)
  }
  width  <- as.integer(width)
  height <- as.integer(height)
  stopifnot(
    "id must be a single non-empty string" = is.character(id) && length(id) == 1L && nzchar(id),
    "width and height must be positive integers" =
      !is.na(width) && !is.na(height) && width >= 1L && height >= 1L
  )
  htmltools::tagList(
    rayplot_dependency(),
    htmltools::tags$canvas(
      id = id, width = width, height = height, tabindex = "0",
      style = htmltools::css(
        display = "block", outline = "none",
        background = "#f8f9fb",
        border = "1px solid #e4e7ed", `border-radius` = "6px"
      )
    )
  )
}

# Server: (re)draw `p` in the canvas `id`. Sends only the serialised spec
# (a few KB) over the existing Shiny websocket.
rayplot_send <- function(p, id = "rayplot", ..., width = 800L, height = 600L,
                         session = NULL) {
  if (is.null(session)) {
    if (!requireNamespace("shiny", quietly = TRUE)) {
      stop("rayplot_send() needs the 'shiny' package, or an explicit `session`.",
           call. = FALSE)
    }
    session <- shiny::getDefaultReactiveDomain()
  }
  if (is.null(session)) {
    stop("rayplot_send() must be called from within a Shiny session.", call. = FALSE)
  }
  width  <- as.integer(width)
  height <- as.integer(height)

  layers <- rayplot_spec(p, ...)
  blob   <- rayplot_serialize(layers, width, height)
  session$sendCustomMessage("rayplot", list(
    id  = id,
    b64 = gsub("[[:space:]]", "", jsonlite::base64_enc(blob))
  ))
  invisible(id)
}

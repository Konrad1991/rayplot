allowed_types <- function() {
  c("scatter", "box")
}

# raylib has a single global window/GL context, so rayplot allows exactly one
# open window at a time (2D or 3D). The live handle lives here; opening a new
# window closes the previous one first.
rayplot_env <- new.env(parent = emptyenv())
rayplot_env$handle <- NULL

close_active <- function() {
  h <- rayplot_env$handle
  rayplot_env$handle <- NULL
  if (is.null(h)) return(invisible(NULL))
  if (identical(attr(h, "rayplot_kind"), "3d")) {
    .Call("rayplot3D_close_", h)
  } else {
    .Call("rayplot_close_", h)
  }
  invisible(NULL)
}

register_window <- function(handle, kind, fps, step_call, close_call) {
  attr(handle, "rayplot_kind") <- kind
  rayplot_env$handle <- handle
  delay <- 1 / fps
  step <- function() {
    if (!identical(rayplot_env$handle, handle)) return(invisible())  # superseded / closed
    if (isTRUE(.Call("rayplot_should_close_", handle))) {
      .Call(close_call, handle)
      if (identical(rayplot_env$handle, handle)) rayplot_env$handle <- NULL
      return(invisible())
    }
    .Call(step_call, handle)
    later::later(step, delay)
  }
  later::later(step, 0)
  invisible(handle)
}

rayplot <- function(p, ..., width = 800L, height = 600L, title = "rayplot", fps = 60) {
  # Two input styles:
  #   rayplot(ggplot_object)          -> translated via resolve_layers()
  #   rayplot(scatter_layer(...), ..) -> layer specs passed straight through
  layers <- if (inherits(p, "ggplot")) resolve_layers(p) else c(list(p), list(...))

  # a ggplot with an aes(z = ) point/line/area layer, or a geom_tile()/
  # geom_raster() 3D surface layer, resolves to one or more 3D specs (a
  # grouped geom_line()/geom_area() gives one line3d/area3d layer per group,
  # e.g. one curtain per LC/GC-MS scan)
  is_3d_layer <- length(layers) >= 1L && all(vapply(
    layers, function(l) {
      is.list(l) && is.character(l$type) && length(l$type) == 1L &&
        l$type %in% c("scatter3d", "line3d", "area3d", "surface3d")
    }, logical(1L)
  ))
  if (is_3d_layer) {
    lbl <- attr(layers, "labels3d")
    if (is.null(lbl)) lbl <- list(x = "x", y = "y", z = "z")
    return(open_rayplot3d(layers, width, height, title, fps, lbl$x, lbl$y, lbl$z))
  }

  ok <- vapply(
    layers, function(l) {
      is.list(l) && is.character(l$type) &&
      length(l$type) == 1L && l$type %in% allowed_types()
    }, logical(1L)
  )
  stopifnot(
    "every layer must be a layer spec (e.g. scatter_layer(x, y)) or a ggplot object" = all(ok)
  )

  width  <- as.integer(width)
  height <- as.integer(height)
  stopifnot(
    "width and height must be positive integers" =
    !is.na(width) && !is.na(height) && width >= 1L && height >= 1L
  )

  close_active()  # raylib is single-window: close any existing window first

  font <- system.file("fonts", "Lato-Regular.ttf", package = "rayplot")
  handle <- .Call("rayplot_open_", layers, width, height,
                  as.character(title), as.character(font))
  register_window(handle, "2d", fps, "rayplot_step_", "rayplot_close_")
}

rayplot_close <- function(handle = NULL) {
  if (is.null(handle)) return(close_active())
  if (identical(attr(handle, "rayplot_kind"), "3d")) {
    .Call("rayplot3D_close_", handle)
  } else {
    .Call("rayplot_close_", handle)
  }
  if (identical(rayplot_env$handle, handle)) rayplot_env$handle <- NULL
  invisible(NULL)
}

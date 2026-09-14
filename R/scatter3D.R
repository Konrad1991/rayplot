# 3D layer specs, one per geometry. Each is a plain list with a "type" tag
# that src/3D/rayplot_3D.c's rayplot3D_open_() dispatches on -- mirrors how
# scatter_layer()/boxplot 2D layers work (R/scatter.R, R/boxplot.R).

is_hex_colour <- function(x) !anyNA(x) && all(grepl("^#?[0-9A-Fa-f]{6}$", x))

# every *_layer() below takes an optional `colours` (plural) vector/matrix,
# one entry per point/vertex, on top of the single flat `colour`. When set
# it overrides the flat colour for that layer. Use colour_groups3d() or
# colour_value3d() below to build it from a categorical grouping or a
# continuous value.
check_colours3d <- function(colours, n, arg = "colours") {
  if (is.null(colours)) return(invisible(NULL))
  stopifnot(
    "colours must have one entry per point" = length(colours) == n
  )
  if (!is_hex_colour(colours)) {
    stop(sprintf("%s must be hex codes like '#1E5AC8'", arg), call. = FALSE)
  }
  invisible(NULL)
}

scatter3d_layer <- function(x, y, z, colour = "#1E5AC8", colours = NULL, point_radius = 0.1) {
  x <- as.double(x); y <- as.double(y); z <- as.double(z)
  stopifnot(
    "x, y and z must have the same length" =
      length(x) == length(y) && length(y) == length(z),
    "need at least one point" = length(x) >= 1L,
    "colour must be a hex code like '#1E5AC8'" =
      length(colour) == 1L && is_hex_colour(colour)
  )
  check_colours3d(colours, length(x))
  point_radius <- as.double(point_radius)
  stopifnot(
    "point_radius must be a positive number" =
      length(point_radius) == 1L && !is.na(point_radius) && point_radius > 0.0
  )
  list(
    type = "scatter3d",
    x = x, y = y, z = z,
    colour = as.character(colour),
    colours = if (is.null(colours)) NULL else as.character(colours),
    point_radius = point_radius
  )
}

line3d_layer <- function(x, y, z, colour = "#1E5AC8", colours = NULL) {
  x <- as.double(x); y <- as.double(y); z <- as.double(z)
  stopifnot(
    "x, y and z must have the same length" =
      length(x) == length(y) && length(y) == length(z),
    "need at least two points" = length(x) >= 2L,
    "colour must be a hex code like '#1E5AC8'" =
      length(colour) == 1L && is_hex_colour(colour)
  )
  check_colours3d(colours, length(x))
  list(
    type = "line3d", x = x, y = y, z = z,
    colour = as.character(colour),
    colours = if (is.null(colours)) NULL else as.character(colours)
  )
}

# ribbon-to-a-plane: fills between the (x, y, z) trace and the floor at
# y = base_y (default 0, matching geom_area()'s default ymin), so a 3D line
# gets a translucent "curtain" dropped straight down to the ground grid.
# `colours[i]`, if given, applies to both the trace point and its
# corresponding point on the floor.
area3d_layer <- function(x, y, z, base_y = 0, colour = "#1E5AC8", colours = NULL, alpha = 0.35) {
  x <- as.double(x); y <- as.double(y); z <- as.double(z)
  stopifnot(
    "x, y and z must have the same length" =
      length(x) == length(y) && length(y) == length(z),
    "need at least two points" = length(x) >= 2L,
    "colour must be a hex code like '#1E5AC8'" =
      length(colour) == 1L && is_hex_colour(colour),
    "alpha must be a single number in [0, 1]" =
      length(alpha) == 1L && !is.na(alpha) && alpha >= 0 && alpha <= 1,
    "base_y must be a single number" = length(base_y) == 1L && !is.na(base_y)
  )
  check_colours3d(colours, length(x))
  list(
    type = "area3d",
    x = x, y = y, z = z,
    base_y = as.double(base_y),
    colour = as.character(colour),
    colours = if (is.null(colours)) NULL else as.character(colours),
    alpha = as.double(alpha)
  )
}

# shared-grid surface: rows share one column grid (col_x), so adjacent rows
# connect into one continuous sheet instead of each getting its own
# vertical curtain (area3d_layer()). `height` is a length(row_z) x
# length(col_x) matrix -- e.g. binned MS scans x a shared m/z grid.
# `colours`, if given, is a matrix with the same dimensions as `height`
# (e.g. built with colour_value3d(height) for a colour-by-height look).
surface3d_layer <- function(row_z, col_x, height, colour = "#1E5AC8", colours = NULL, alpha = 0.6) {
  row_z <- as.double(row_z)
  col_x <- as.double(col_x)
  stopifnot(
    "need at least 2 rows and 2 columns" = length(row_z) >= 2L && length(col_x) >= 2L,
    "height must be a matrix with nrow(height) == length(row_z) and ncol(height) == length(col_x)" =
      is.matrix(height) && nrow(height) == length(row_z) && ncol(height) == length(col_x),
    "colour must be a hex code like '#1E5AC8'" =
      length(colour) == 1L && is_hex_colour(colour),
    "alpha must be a single number in [0, 1]" =
      length(alpha) == 1L && !is.na(alpha) && alpha >= 0 && alpha <= 1
  )
  if (!is.null(colours)) {
    stopifnot(
      "colours must be a matrix with the same dimensions as height" =
        is.matrix(colours) && identical(dim(colours), dim(height))
    )
    if (!is_hex_colour(colours)) stop("colours must be hex codes like '#1E5AC8'", call. = FALSE)
  }
  list(
    type = "surface3d",
    row_z = row_z, col_x = col_x,
    # as.double()/as.character() on a matrix keep R's native column-major
    # order, which is exactly what rayplot3D_open_() expects
    # (height[col * n_rows + row])
    height = as.double(height),
    colour = as.character(colour),
    colours = if (is.null(colours)) NULL else as.character(colours),
    alpha = as.double(alpha)
  )
}

# categorical: map a factor/discrete grouping to a hex vector via a palette,
# mirroring 2D's colour_groups/colours (R/scatter.R).
colour_groups3d <- function(groups, palette) {
  g <- as.integer(as.factor(groups))
  stopifnot(
    "palette must have at least as many colours as groups" = length(palette) >= max(g),
    "palette must be hex codes like '#1E5AC8'" = is_hex_colour(palette)
  )
  as.character(palette)[g]
}

# continuous: map a numeric value to a hex colour via a gradient. Uses base
# R's colorRampPalette (no extra dependency) -- pass your own `palette`
# stops for something like viridis; the default is a viridis-like ramp.
colour_value3d <- function(values, palette = c("#440154", "#3B528B", "#21908C", "#5DC863", "#FDE725"),
                           range = NULL) {
  values_dim <- dim(values)
  values <- as.double(values)
  if (is.null(range)) range <- range(values, finite = TRUE)
  stopifnot(
    "palette must be hex codes like '#1E5AC8'" = is_hex_colour(palette),
    "range must be a length-2 numeric vector" = length(range) == 2L && !anyNA(range)
  )
  ramp <- grDevices::colorRampPalette(palette)(256L)
  idx <- as.integer(round((values - range[1]) / (range[2] - range[1]) * 255)) + 1L
  idx[!is.finite(idx)] <- 1L
  idx[idx < 1L] <- 1L
  idx[idx > 256L] <- 256L
  out <- ramp[idx]
  dim(out) <- values_dim  # preserve a matrix shape (e.g. for surface3d_layer)
  out
}

# shared 3D open path, used by rayplot3D_scatter()/rayplot3D_line()/
# rayplot3D_area() and by rayplot() when a ggplot carries an aes(z = ) point
# layer (or a geom_tile()/geom_raster() 3D surface). xlab/ylab/zlab label the
# visual X (right)/Y (up)/Z (depth) axes drawn in the render loop -- for a
# ggplot input rayplot() fills these in from attr(layers, "labels3d"),
# derived from the plot's own aesthetic labels.
open_rayplot3d <- function(layers, width, height, title, fps,
                           xlab = "x", ylab = "y", zlab = "z") {
  width  <- as.integer(width)
  height <- as.integer(height)
  stopifnot(
    "need at least one layer" = length(layers) >= 1L,
    "width and height must be positive integers" =
      !is.na(width) && !is.na(height) && width >= 1L && height >= 1L
  )

  close_active()  # raylib is single-window: close any existing window first

  handle <- .Call(
    "rayplot3D_open_", layers, width, height, as.character(title),
    as.character(xlab), as.character(ylab), as.character(zlab)
  )
  register_window(handle, "3d", fps, "rayplot3D_step_", "rayplot3D_close_")
}

rayplot3D_scatter <- function(x, y, z, width = 800L, height = 600L,
                              title = "rayplot3D", point_radius = 0.1, fps = 60,
                              xlab = "x", ylab = "y", zlab = "z") {
  open_rayplot3d(
    list(scatter3d_layer(x, y, z, point_radius = point_radius)),
    width, height, title, fps, xlab, ylab, zlab
  )
}

rayplot3D_line <- function(x, y, z, width = 800L, height = 600L,
                           title = "rayplot3D", colour = "#1E5AC8", fps = 60,
                           xlab = "x", ylab = "y", zlab = "z") {
  open_rayplot3d(
    list(line3d_layer(x, y, z, colour = colour)),
    width, height, title, fps, xlab, ylab, zlab
  )
}

rayplot3D_area <- function(x, y, z, base_y = 0, width = 800L, height = 600L,
                           title = "rayplot3D", colour = "#1E5AC8", alpha = 0.35, fps = 60,
                           xlab = "x", ylab = "y", zlab = "z") {
  open_rayplot3d(
    list(area3d_layer(x, y, z, base_y = base_y, colour = colour, alpha = alpha)),
    width, height, title, fps, xlab, ylab, zlab
  )
}

rayplot3D_close <- function(handle = NULL) {
  rayplot_close(handle)
}

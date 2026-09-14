resolve_layers <- function(p) {
  pb <- ggplot2::ggplot_build(p)
  data <- pb@data
  glayers <- pb@plot@layers
  n <- length(data)
  stopifnot("rayplot() needs at least one layer" = n >= 1L)

  # 3D dispatch: a point layer carrying a usable z aesthetic, or a
  # tile/raster layer carrying a z (or a stat-produced `value`, e.g. from
  # stat_summary_2d()) switches the whole plot to the orbit-camera renderer.
  is_3d_point <- vapply(seq_len(n), function(i) {
    inherits(glayers[[i]]$geom, "GeomPoint") &&
      !is.null(data[[i]]$z) && any(is.finite(data[[i]]$z))
  }, logical(1L))
  is_3d_surface <- vapply(seq_len(n), function(i) {
    (inherits(glayers[[i]]$geom, "GeomTile") || inherits(glayers[[i]]$geom, "GeomRaster")) &&
      ((!is.null(data[[i]]$z) && any(is.finite(data[[i]]$z))) ||
       (!is.null(data[[i]]$value) && any(is.finite(data[[i]]$value))))
  }, logical(1L))
  if (any(is_3d_point) || any(is_3d_surface)) {
    if (n != 1L) {
      stop(
        "rayplot: a 3D layer (aes(z = ) on geom_point, or geom_tile()/",
        "geom_raster() with a z or stat-produced `value`) must be the ",
        "plot's only layer",
        call. = FALSE
      )
    }
    lbl <- pb@plot$labels
    if (is_3d_point[[1L]]) {
      out <- list(translate_point3d(data[[1L]]))
      # visual X/Y(up)/Z(depth) match ggplot's x/y/z directly for points
      attr(out, "labels3d") <- list(x = lbl$x %||% "x", y = lbl$y %||% "y", z = lbl$z %||% "z")
      return(out)
    }
    out <- list(translate_surface3d(data[[1L]]))
    # a surface's height (visual Y/up) is ggplot's z (the value being
    # plotted), and ggplot's y becomes visual depth (Z) -- see
    # translate_surface3d()'s row_z/col_x/height layout
    attr(out, "labels3d") <- list(x = lbl$x %||% "x", y = lbl$z %||% "value", z = lbl$y %||% "y")
    return(out)
  }

  ticks <- panel_ticks(pb)

  lapply(seq_len(n), function(i) {
    d <- data[[i]]
    geom <- class(glayers[[i]]$geom)[1L]
    switch(geom,
      GeomPoint = translate_point(d),
      GeomBoxplot = translate_boxplot(d, ticks),
      stop(
        sprintf(
          "rayplot: unsupported geom '%s' in layer %d", geom, i
        )
      )
    )
  })
}

# geom_point built data with a z column -> 3D scatter spec.
translate_point3d <- function(d) {
  keep <- is.finite(d$x) & is.finite(d$y) & is.finite(d$z)
  size <- if (is.null(d$size)) NA_real_ else as.double(d$size[[1L]])
  list(
    type = "scatter3d",
    x = as.double(d$x[keep]),
    y = as.double(d$y[keep]),
    z = as.double(d$z[keep]),
    colours = if (is.null(d$colour)) NULL else as_hex6(d$colour[keep]),
    point_radius = if (is.na(size) || size <= 0) 0.15 else 0.1 * size
  )
}

# geom_tile()/geom_raster() built data with a z (or stat-produced `value`)
# column -> 3D surface spec. Height comes from the raw numeric z/value;
# colour comes from fill, already resolved to hex by ggplot_build() -- so
# aes(z = intensity, fill = intensity) puts the same field on both height
# and colour without rayplot having to reverse-engineer a number out of a
# colour (fill's raw value doesn't survive scale mapping).
translate_surface3d <- function(d) {
  elev <- if (!is.null(d$z)) d$z else d$value
  if (is.null(elev)) {
    stop(
      "rayplot: a geom_tile()/geom_raster() 3D surface needs a numeric ",
      "aes(z = ) (or a stat that outputs `value`, e.g. stat_summary_2d())",
      call. = FALSE
    )
  }

  # a bin with no raw points in it (e.g. stat_summary_2d(..., drop = FALSE)
  # over a sparse region) comes back with a non-finite elev (NA or -Inf from
  # fun(numeric(0))) -- treat that as "no signal", i.e. height 0, rather
  # than dropping the cell and breaking the rectangular grid the mesh needs.
  keep <- is.finite(d$x) & is.finite(d$y)
  x <- d$x[keep]; y <- d$y[keep]
  elev <- elev[keep]
  elev[!is.finite(elev)] <- 0
  fill <- if (is.null(d$fill)) NULL else d$fill[keep]

  col_x <- sort(unique(x))
  row_z <- sort(unique(y))
  n_rows <- length(row_z); n_cols <- length(col_x)
  stopifnot(
    "rayplot: 3D surface needs at least a 2x2 x/y grid" =
      n_rows >= 2L && n_cols >= 2L,
    "rayplot: 3D surface data must form a complete x/y grid (no missing cells) -- pass drop = FALSE to stat_summary_2d()/stat_bin_2d() so every bin is kept" =
      length(x) == n_rows * n_cols
  )
  row_i <- match(y, row_z)
  col_j <- match(x, col_x)

  height <- matrix(0, n_rows, n_cols)
  height[cbind(row_i, col_j)] <- elev

  colours <- matrix("#1E5AC8", n_rows, n_cols)
  if (!is.null(fill)) colours[cbind(row_i, col_j)] <- as_hex6(fill)

  alpha <- if (is.null(d$alpha) || all(is.na(d$alpha))) 0.85 else as.double(d$alpha[[1L]])

  list(
    type = "surface3d",
    row_z = as.double(row_z),
    col_x = as.double(col_x),
    height = as.double(height),
    colours = as.character(colours),
    alpha = alpha
  )
}

# "#RRGGBBAA" / "#RRGGBB" / "red" / NA  ->  "#RRGGBB"
as_hex6 <- function(x) {
  x[is.na(x)] <- "#000000"
  rgb <- grDevices::col2rgb(x)
  sprintf("#%02X%02X%02X", rgb[1L, ], rgb[2L, ], rgb[3L, ])
}

# per-row hex -> list(codes = 0-based integer, palette = unique hex codes)
fold_colours <- function(hex) {
  f <- factor(as_hex6(hex))
  list(codes = as.integer(f) - 1L, palette = levels(f))
}

translate_point <- function(d) {
  col <- fold_colours(if (is.null(d$colour)) "#1E5AC8" else d$colour)
  size <- if (is.null(d$size)) 4.0 else as.double(d$size[[1L]]) * 2
  list(
    type = "scatter",
    x = as.double(d$x),
    y = as.double(d$y),
    colour_groups = col$codes,
    colours = col$palette,
    point_radius = if (is.na(size) || size <= 0) 4.0 else size
  )
}

translate_boxplot <- function(d, ticks) {
  col <- fold_colours(if (is.null(d$fill)) "#1E5AC8" else d$fill)

  out <- d$outliers
  if (is.null(out)) out <- vector("list", nrow(d))
  out <- lapply(out, function(v) as.double(v[!is.na(v)]))
  flat <- as.double(unlist(out, use.names = FALSE))
  if (is.null(flat)) flat <- numeric(0)

  list(
    type = "box",
    centers = as.double(d$x),
    first_quantiles = as.double(d$lower),
    medians = as.double(d$middle),
    third_quantiles = as.double(d$upper),
    whisker_low = as.double(d$ymin),
    whisker_high = as.double(d$ymax),
    outliers = flat,
    outlier_start = as.integer(c(0, cumsum(lengths(out)))),
    box_width = as.double(stats::median(d$xmax - d$xmin)),
    colour_groups = col$codes,
    colours = col$palette,
    tick_pos = ticks$pos,
    tick_labels = ticks$labels
  )
}

panel_ticks <- function(pb) {
  pp <- pb@layout$panel_params[[1L]]
  labels <- tryCatch(pp$x$get_labels(), error = function(e) NULL)
  breaks <- tryCatch(pp$x$breaks, error = function(e) NULL)
  pos <- attr(breaks, "pos")
  if (is.null(pos)) pos <- suppressWarnings(as.double(breaks))
  keep <- !is.na(pos) & !is.na(labels)
  if (length(keep) == 0L || !any(keep)) return(list(pos = NULL, labels = NULL))
  list(pos = as.double(pos)[keep], labels = as.character(labels)[keep])
}

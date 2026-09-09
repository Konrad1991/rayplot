resolve_layers <- function(p) {
  pb <- ggplot2::ggplot_build(p)
  data <- pb@data
  glayers <- pb@plot@layers
  n <- length(data)
  stopifnot("rayplot() needs at least one layer" = n >= 1L)

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

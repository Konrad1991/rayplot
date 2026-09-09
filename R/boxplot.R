calc_slot_gap <- function(n_slots) {
  if (n_slots > 1L) {
    0.8 / n_slots 
  } else {
    0.0
  }
}

calc_default_box_width <- function(n_slots) {
  if (n_slots > 1L) {
    calc_slot_gap(n_slots) * 0.9 
  } else {
    0.6
  }
}

dodge_offset <- function(di, n_slots) {
  if (n_slots > 1L) {
    (di - (n_slots + 1) / 2) * calc_slot_gap(n_slots)
  } else {
    di * 0
  }
}

as_color_codes <- function(colour_groups, n) {
  if (!is.numeric(colour_groups)) colour_groups <- as.factor(colour_groups)
  if (length(colour_groups) == 1L) colour_groups <- rep(colour_groups, n)
  if (length(colour_groups) != n)
    stop("colour_groups must have length 1 or length(x)")
  as.integer(colour_groups)
}

box_positions <- function(x, colour_groups = 1L) {
  colour_groups <- as_color_codes(colour_groups, length(x))
  n_slots <- max(colour_groups)
  as.integer(as.factor(x)) + dodge_offset(colour_groups, n_slots)
}

box_jitter <- function(x, colour_groups = 1L, box_width = NULL, frac = 0.4) {
  colour_groups <- as_color_codes(colour_groups, length(x))
  n_slots <- max(colour_groups)
  if (is.null(box_width)) {
    box_width <- calc_default_box_width(n_slots)
  }
  pos <- as.integer(as.factor(x)) + dodge_offset(colour_groups, n_slots)
  amount <- box_width * frac
  pos + stats::runif(length(pos), -amount, amount)
}

box_layer <- function(x, y, colour_groups_ = 1L,
                      colours = "#1E5AC8", box_width = NULL) {
  y <- as.double(y)
  x <- as.factor(x)
  stopifnot(
    "x and y length do not match" = length(x) == length(y),
    "colours must have at least one entry" = length(colours) >= 1L,
    "colours must be hex codes like '#1E5AC8'" =
    !anyNA(colours) && all(grepl("^#?[0-9A-Fa-f]{6}$", colours))
  )
  colour_groups <- as_color_codes(colour_groups_, length(y))

  colours <- as.character(colours)
  keep <- !(is.na(x) | is.na(y) | is.na(colour_groups))
  if (!all(keep)) {
    x <- x[keep]
    y <- y[keep]
    colour_groups <- colour_groups[keep]
  }
  stopifnot("no non-NA points to plot" = length(y) >= 1L)
  rng <- range(colour_groups)
  if (rng[1L] < 1L || rng[2L] > length(colours)) {
    stop(sprintf("colour_groups range [%d, %d] outside 1:%d (number of colours)",
      rng[1L], rng[2L], length(colours)))
  }
  x_levels <- levels(x)
  n_slots  <- max(colour_groups)
  if (is.null(box_width)) {
    box_width <- calc_default_box_width(n_slots)
  }
  box_width <- as.double(box_width)
  stopifnot(
    "box_width must be a positive number" =
    length(box_width) == 1L && !is.na(box_width) && box_width > 0.0
  )

  centers <- numeric(0)
  first_quantiles <- numeric(0)
  medians <- numeric(0)
  third_quantiles <- numeric(0)
  lower_whiskers <- numeric(0)
  higher_whiskers <- numeric(0)
  colour_groups_res <- integer(0)
  out_list <- list()

  xi <- as.integer(x)

  for (gi in seq_along(x_levels)) {
    for (di in seq_len(n_slots)) {
      y_temp <- y[xi == gi & colour_groups == di]
      if (!length(y_temp)) next
      bs <- grDevices::boxplot.stats(y_temp)
      st <- bs$stats
      centers <- c(centers, gi + dodge_offset(di, n_slots))
      first_quantiles  <- c(first_quantiles,  st[2])
      medians <- c(medians, st[3])
      third_quantiles <- c(third_quantiles, st[4])
      lower_whiskers  <- c(lower_whiskers,  st[1])
      higher_whiskers  <- c(higher_whiskers,  st[5])
      colour_groups_res  <- c(colour_groups_res, di)
      out_list <- c(out_list, list(as.double(bs$out)))
    }
  }
  stopifnot("no boxes to draw" = length(centers) >= 1L)

  outlier_start <- as.integer(c(0, cumsum(lengths(out_list))))
  outliers <- as.double(unlist(out_list, use.names = FALSE))
  if (is.null(outliers)) outliers <- numeric(0)

  list(
    type = "box",
    centers = as.double(centers),
    first_quantiles = as.double(first_quantiles),
    medians = as.double(medians),
    third_quantiles = as.double(third_quantiles),
    whisker_low = as.double(lower_whiskers),
    whisker_high = as.double(higher_whiskers),
    outliers = outliers,
    outlier_start = outlier_start,
    box_width = box_width,
    colour_groups = as.integer(colour_groups_res) - 1L,
    colours = colours,
    tick_pos = as.double(seq_along(x_levels)),
    tick_labels = x_levels
  )
}

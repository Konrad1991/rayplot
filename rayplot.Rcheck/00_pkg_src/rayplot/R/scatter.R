scatter_layer <- function(x, y, colour_groups = 1L, colours = "#1E5AC8",
                          point_radius = 4.0) {
  x <- as.double(x)
  y <- as.double(y)
  stopifnot(
    "x and y length do not match" = length(x) == length(y),
    "colour_groups must have length 1 or length(x)" = length(colour_groups) == length(x),
    "colours must have at least one entry" = length(colours) >= 1L,
    "colours must be hex codes like '#1E5AC8'" =
    !anyNA(colours) && all(grepl("^#?[0-9A-Fa-f]{6}$", colours))
  )

  if (!is.numeric(colour_groups)) colour_groups <- as.factor(colour_groups)
  if (length(colour_groups) == 1L) colour_groups <- rep(colour_groups, length(x))

  colour_groups <- as.integer(colour_groups)
  colours <- as.character(colours)

  keep <- !(is.na(x) | is.na(y) | is.na(colour_groups))
  if (!all(keep)) {
    x <- x[keep]
    y <- y[keep]
    colour_groups <- colour_groups[keep]
  }
  stopifnot("no non-NA points to plot" = length(x) >= 1L)

  rng <- range(colour_groups)
  if (rng[1L] < 1L || rng[2L] > length(colours)) {
    stop(sprintf("colour_groups range [%d, %d] outside 1:%d (number of colours)",
      rng[1L], rng[2L], length(colours)))
  }
  point_radius <- as.double(point_radius)
  stopifnot(
    "point_radius must be a positive number" =
    length(point_radius) == 1L && !is.na(point_radius) && point_radius > 0.0
  )

  list(
    type = "scatter",
    x = x, y = y,
    colour_groups = colour_groups - 1L,   # 0-based for C
    colours = colours,
    point_radius = point_radius
  )
}

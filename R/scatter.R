.rayplot_env <- new.env(parent = emptyenv())
.rayplot_env$running <- list()

rayplot_scatter <- function(x, y, colour_groups = 1L, colours = "#1E5AC8",
                             width = 800L, height = 600L,
                             title = "rayplot", point_radius = 4.0, fps = 60) {

  x <- as.double(x)
  y <- as.double(y)
  if (length(x) != length(y)) stop("x and y length do not match")

  ## colour_groups: accept factor / character / numeric, recycle a scalar,
  ## then fold to 1-based integer codes.
  if (!is.numeric(colour_groups)) colour_groups <- as.factor(colour_groups)
  if (length(colour_groups) == 1L) colour_groups <- rep(colour_groups, length(x))
  if (length(colour_groups) != length(x))
    stop("colour_groups must have length 1 or length(x)")
  colour_groups <- as.integer(colour_groups)

  ## colours: non-empty character of #RRGGBB (with or without '#').
  colours <- as.character(colours)
  if (!length(colours)) stop("colours must have at least one entry")
  if (anyNA(colours) || !all(grepl("^#?[0-9A-Fa-f]{6}$", colours)))
    stop("colours must be hex codes like '#1E5AC8'")

  ## drop rows with NA in any of x / y / colour_groups
  keep <- !(is.na(x) | is.na(y) | is.na(colour_groups))
  if (!all(keep)) {
    x <- x[keep]; y <- y[keep]; colour_groups <- colour_groups[keep]
  }
  if (!length(x)) stop("no non-NA points to plot")

  ## every group must index into colours; convert to 0-based for C
  rng <- range(colour_groups)
  if (rng[1L] < 1L || rng[2L] > length(colours))
    stop(sprintf("colour_groups range [%d, %d] outside 1:%d (number of colours)",
                 rng[1L], rng[2L], length(colours)))
  colour_groups <- colour_groups - 1L

  width  <- as.integer(width)
  height <- as.integer(height)
  if (is.na(width) || is.na(height) || width < 1L || height < 1L)
    stop("width and height must be positive integers")
  point_radius <- as.double(point_radius)
  if (length(point_radius) != 1L || is.na(point_radius) || point_radius <= 0)
    stop("point_radius must be a positive number")

  handle <- .Call(
    "rayplot_open_", x, y,
    width, height,
    colour_groups, colours,
    as.character(title), point_radius
  )

  key <- .handle_key(handle)
  .rayplot_env$running[[key]] <- TRUE
  delay <- 1 / fps
  step <- function() {
    if (is.null(.rayplot_env$running[[key]])) return(invisible())
    if (isTRUE(.Call("rayplot_should_close_", handle))) {
      .Call("rayplot_close_", handle)
      .rayplot_env$running[[key]] <- NULL
      return(invisible())
    }
    .Call("rayplot_step_", handle)
    later::later(step, delay)
  }
  later::later(step, 0)
  invisible(handle)
}

rayplot_close <- function(handle = NULL) {
  if (is.null(handle)) {
    for (key in names(.rayplot_env$running)) {
      .rayplot_env$running[[key]] <- NULL
    }
    invisible(return(NULL))
  }
  key <- .handle_key(handle)
  .rayplot_env$running[[key]] <- NULL
  .Call("rayplot_close_", handle)
  invisible(NULL)
}
.handle_key <- function(handle) {
  format(handle)
}

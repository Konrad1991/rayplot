rayplot3D_scatter <- function(x, y, z, width = 800L, height = 600L,
                              title = "rayplot3D", point_radius = 0.1, fps = 60) {
  x <- as.double(x)
  y <- as.double(y)
  z <- as.double(z)
  stopifnot(
    "x, y and z must have the same length" =
      length(x) == length(y) && length(y) == length(z),
    "need at least one point" = length(x) >= 1L
  )

  width  <- as.integer(width)
  height <- as.integer(height)
  stopifnot(
    "width and height must be positive integers" =
    !is.na(width) && !is.na(height) && width >= 1L && height >= 1L
  )

  close_active()  # raylib is single-window: close any existing window first

  handle <- .Call(
    "rayplot3D_open_", x, y, z,
    width, height, as.character(title), as.double(point_radius)
  )
  register_window(handle, "3d", fps, "rayplot3D_step_", "rayplot3D_close_")
}

rayplot3D_close <- function(handle = NULL) {
  rayplot_close(handle)
}

.rayplot3D_env <- new.env(parent = emptyenv())
.rayplot3D_env$running <- list()

rayplot3D_scatter <- function(x, y, z, width = 800L, height = 600L,
                             title = "rayplot3D", point_radius = 0.1, fps = 60) {

  x <- as.double(x)
  y <- as.double(y)
  z <- as.double(z)

  handle <- .Call(
    "rayplot3D_open_", x, y, z,
    as.integer(width), as.integer(height),
    as.character(title), point_radius
  )

  key <- .handle_key(handle)
  .rayplot3D_env$running[[key]] <- TRUE
  delay <- 1 / fps
  step <- function() {
    if (is.null(.rayplot3D_env$running[[key]])) return(invisible())
    if (isTRUE(.Call("rayplot_should_close_", handle))) {
      .Call("rayplot3D_close_", handle)
      .rayplot3D_env$running[[key]] <- NULL
      return(invisible())
    }
    .Call("rayplot3D_step_", handle)
    later::later(step, delay)
  }
  later::later(step, 0)
  invisible(handle)
}

rayplot3D_close <- function(handle = NULL) {
  if (is.null(handle)) {
    for (key in names(.rayplot3D_env$running)) {
      .rayplot3D_env$running[[key]] <- NULL
    }
    invisible(return(NULL))
  }
  key <- .handle_key(handle)
  .rayplot3D_env$running[[key]] <- NULL
  .Call("rayplot3D_close_", handle)
  invisible(NULL)
}
.handle_key <- function(handle) {
  format(handle)
}

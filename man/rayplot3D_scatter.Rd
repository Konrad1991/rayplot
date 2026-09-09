\name{rayplot3D_scatter}
\alias{rayplot3D_scatter}
\alias{rayplot3D_close}
\title{Interactive 3D scatter plot (experimental)}
\description{
  Open a non-blocking \pkg{raylib} window showing a 3D point cloud with an orbit
  camera. This entry point does not use \pkg{ggplot2}; supply the coordinate
  vectors directly. The render loop is driven from R's event loop via
  \pkg{later}.
}
\usage{
rayplot3D_scatter(x, y, z, width = 800L, height = 600L,
                  title = "rayplot3D", point_radius = 0.1, fps = 60)

rayplot3D_close(handle = NULL)
}
\arguments{
  \item{x, y, z}{Numeric vectors of equal length giving the point coordinates.}
  \item{width, height}{Window size in pixels.}
  \item{title}{Window title.}
  \item{point_radius}{Sphere radius for each point, in data units.}
  \item{fps}{Target frames per second for the redraw loop.}
  \item{handle}{The external-pointer handle returned by
    \code{rayplot3D_scatter()}. When \code{NULL}, \code{rayplot3D_close()} closes
    whichever window is currently open.}
}
\details{
  Interaction: \strong{left-drag} orbits the camera, \strong{right-drag} pans,
  and the \strong{scroll wheel} zooms. Press \code{Esc} or close the window to
  quit.

  This is experimental: the API is minimal (a single scatter geometry, no axes
  labels or legends) and may change.

  raylib has a single global GL context, so only one rayplot window can be open;
  calling this or \code{\link{rayplot}} again closes the current window first.
}
\value{
  Invisibly, an external-pointer \code{handle} identifying the window. Pass it to
  \code{rayplot3D_close()} to close that window.
}
\seealso{
  \code{\link{rayplot}} for 2D plots from \code{ggplot} objects.
}
\examples{
\dontrun{
h <- rayplot3D_scatter(
  CO2$conc, CO2$uptake, as.numeric(CO2$Treatment),
  point_radius = 0.1
)

rayplot3D_close(h)
}
}

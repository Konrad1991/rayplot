\name{rayplot}
\alias{rayplot}
\alias{rayplot_close}
\title{Interactive raylib plot from a ggplot object}
\description{
  Open a non-blocking \pkg{raylib} window that renders a plot with mouse pan,
  scroll-wheel zoom and hover tooltips. The render loop is driven from R's event
  loop via \pkg{later}, so the console stays usable while the window is open.
}
\usage{
rayplot(p, ..., width = 800L, height = 600L, title = "rayplot", fps = 60)

rayplot_close(handle = NULL)
}
\arguments{
  \item{p}{Either a \code{ggplot} object, or the first of one or more layer
    specifications (see \sQuote{Details}).}
  \item{\dots}{Further layer specifications, used only when \code{p} is itself a
    layer specification rather than a \code{ggplot} object.}
  \item{width, height}{Window size in pixels. Positive integers.}
  \item{title}{Window title.}
  \item{fps}{Target frames per second for the redraw loop.}
  \item{handle}{The external-pointer handle returned by \code{rayplot()}. When
    \code{NULL}, \code{rayplot_close()} closes whichever window is currently
    open.}
}
\details{
  \strong{ggplot input (the common case).} When \code{p} inherits from
  \code{"ggplot"}, \code{rayplot()} calls \code{\link[ggplot2]{ggplot_build}} and
  translates each built layer into a typed specification that the C renderer
  consumes. Aesthetics are resolved by ggplot first, so scales, positions
  (\code{position_dodge}, \code{position_jitterdodge}, \dots) and computed
  statistics all apply. Each layer carries its own colour palette, taken from
  that layer's \code{fill} or \code{colour} column.

  Supported geoms are \code{geom_point} and \code{geom_boxplot}, in cartesian
  coordinates with a single panel. Any other geom, coord or facet raises an
  error.

  \strong{Direct layer input.} \code{p} (and \code{\dots}) may instead be
  \code{list}s of the form \code{list(type = "scatter", x = , y = , \dots)} or
  \code{list(type = "box", \dots)}. This bypasses ggplot entirely and is used
  mainly for testing the renderer.

  \strong{Interaction.} Drag to pan, scroll to zoom, hover a point or box for a
  tooltip, press \code{Esc} or close the window to quit. Everything is in data
  coordinates.

  \strong{One window at a time.} raylib has a single global GL context, so only
  one rayplot window (2D or 3D) can be open. Calling \code{rayplot()} or
  \code{\link{rayplot3D_scatter}} again closes the current window first.
}
\value{
  Invisibly, an external-pointer \code{handle} identifying the window. Pass it to
  \code{rayplot_close()} to close that window; \code{rayplot_close()} with no
  argument closes whichever window is open.
}
\seealso{
  \code{\link[ggplot2]{ggplot_build}}.
}
\examples{
\dontrun{
library(ggplot2)

## scatter
rayplot(ggplot(mtcars, aes(wt, mpg, colour = factor(cyl))) + geom_point())

## dodged boxplot + points, different palette per layer
p <- ggplot(CO2, aes(
    x = factor(conc), y = uptake,
    group = interaction(conc, Treatment, Type)
  )) +
  geom_boxplot(aes(fill = interaction(Treatment, Type)),
               position = position_dodge(width = 0.8)) +
  geom_point(aes(colour = interaction(Treatment, Type)),
             position = position_dodge(width = 0.8)) +
  scale_fill_brewer(palette = "Set2") +
  scale_colour_grey(start = 0.1, end = 0.5) +
  theme(legend.position = "none")

h <- rayplot(p, title = "CO2 uptake")

## keep working in the console, then:
rayplot_close(h)
}
}

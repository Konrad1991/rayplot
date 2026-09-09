\name{rayplot_web}
\alias{rayplot_web}
\alias{rayplot_spec}
\alias{rayplot_serialize}
\title{Export a plot as a WebAssembly page}
\description{
  Render a plot in the browser instead of a native window. \R{} builds the same
  per-layer specification that \code{\link{rayplot}} feeds the C renderer,
  serialises it to a compact binary blob, and writes a self-contained HTML page
  that draws it with an Emscripten build of the raylib render core. Pan, zoom and
  hover tooltips work client-side; \R{} is not involved once the page is open.
}
\usage{
rayplot_web(p, file, ..., width = 800L, height = 600L, title = "rayplot",
            assets = system.file("web", package = "rayplot"))

rayplot_spec(p, ...)

rayplot_serialize(layers, width = 800L, height = 600L)
}
\arguments{
  \item{p}{A \code{ggplot} object, or layer specifications, exactly as for
    \code{\link{rayplot}}.}
  \item{file}{Path of the \code{.html} page to write.}
  \item{\dots}{Further layer specifications when \code{p} is one.}
  \item{width, height}{Canvas size in pixels.}
  \item{title}{Page \code{<title>}.}
  \item{assets}{Directory containing \code{rayplot.js}, \code{rayplot.wasm} and
    \code{template.html}. The default is the installed \code{inst/web}. The
    WebAssembly module is \emph{not} built by \code{R CMD INSTALL}; build it once
    with \code{inst/web/build.sh} (needs the Emscripten SDK).}
  \item{layers}{A list of layer specifications, e.g. from \code{rayplot_spec()}.}
}
\details{
  \code{rayplot_spec()} returns the list of per-layer specifications.
  \code{rayplot_serialize()} turns that list into the little-endian binary blob
  consumed by \code{inst/web/rayplot_web.c} (format documented at the top of that
  file). \code{rayplot_web()} base64-encodes the blob into the HTML page and, if
  \code{rayplot.js} / \code{rayplot.wasm} are present in \code{assets}, copies
  them next to \code{file}.

  Only \code{geom_point} and \code{geom_boxplot} layers are supported, as for
  \code{\link{rayplot}}.
}
\value{
  \code{rayplot_web()} returns the output path, invisibly.
  \code{rayplot_spec()} a list of layer specifications.
  \code{rayplot_serialize()} a raw vector.
}
\seealso{\code{\link{rayplot}}}
\examples{
\dontrun{
library(ggplot2)
p <- ggplot(mtcars, aes(wt, mpg, colour = factor(cyl))) + geom_point()

rayplot_web(p, "mtcars.html")
# then serve the folder and open mtcars.html, e.g.:
#   python3 -m http.server
}
}

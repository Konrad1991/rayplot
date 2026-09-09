\name{rayplotCanvas}
\alias{rayplotCanvas}
\alias{rayplot_send}
\title{Embed a rayplot renderer in a Shiny app}
\description{
  Draw plots inside a Shiny app with the WebAssembly build of the raylib render
  core, updating them over Shiny's websocket without reloading the page or using
  an \code{iframe}. \code{rayplotCanvas()} places the drawing surface in the UI;
  \code{rayplot_send()} pushes a plot to it from the server.
}
\usage{
rayplotCanvas(id = "rayplot", width = 800L, height = 600L)

rayplot_send(p, id = "rayplot", ..., width = 800L, height = 600L,
             session = NULL)
}
\arguments{
  \item{id}{Element id shared by the canvas and the \code{rayplot_send()} calls
    that target it.}
  \item{width, height}{Drawing-buffer size in pixels. The value passed to
    \code{rayplotCanvas()} and to \code{rayplot_send()} must match.}
  \item{p}{A \code{ggplot} object (or layer specifications), as for
    \code{\link{rayplot}}.}
  \item{\dots}{Further layer specifications when \code{p} is one.}
  \item{session}{The Shiny session; defaults to the current reactive domain.}
}
\details{
  The WebAssembly module (\code{inst/web/rayplot.js} + \code{rayplot.wasm}) is
  \emph{not} built by \code{R CMD INSTALL}. Build it once with
  \code{inst/web/build.sh} (needs the Emscripten SDK); \code{rayplotCanvas()}
  serves it via an \code{\link[htmltools]{htmlDependency}}.

  The first \code{rayplot_send()} for a canvas creates the renderer; later calls
  swap in the new data (each resets pan/zoom to fit). Only one canvas per page
  is supported (raylib uses a single GL context). \code{shiny} and
  \code{htmltools} are needed only for these helpers.

  Only \code{geom_point} and \code{geom_boxplot} layers are supported.
}
\value{
  \code{rayplotCanvas()} a \code{\link[htmltools]{tagList}} for the UI.
  \code{rayplot_send()} the \code{id}, invisibly.
}
\seealso{\code{\link{rayplot_web}} for a standalone HTML page; \code{\link{rayplot}} for the desktop window.}
\examples{
\dontrun{
library(shiny)
library(ggplot2)

ui <- fluidPage(
  selectInput("pal", "Palette", c("Set2", "Dark2", "Paired")),
  rayplotCanvas("plot", 900, 620)
)

server <- function(input, output, session) {
  observe({
    p <- ggplot(CO2, aes(factor(conc), uptake,
                         group = interaction(conc, Treatment, Type))) +
      geom_boxplot(aes(fill = interaction(Treatment, Type)),
                   position = position_dodge(width = 0.8)) +
      geom_point(aes(colour = interaction(Treatment, Type)),
                 position = position_dodge(width = 0.8)) +
      scale_fill_brewer(palette = input$pal) +
      theme(legend.position = "none")
    rayplot_send(p, "plot", width = 900L, height = 620L)
  })
}

shinyApp(ui, server)
}
}

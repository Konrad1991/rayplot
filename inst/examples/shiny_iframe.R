# rayplot in Shiny -- approach A: rayplot_web() writes an HTML page per update,
# shown in an <iframe>. Simple and self-contained, but every update reloads the
# iframe (re-instantiates the wasm module). Prefer shiny_canvas.R for anything
# frequently updated.
#
# Needs the wasm build first:  bash inst/web/build.sh   (Emscripten SDK)
#
#   shiny::runApp(system.file("examples/shiny_iframe.R", package = "rayplot"))

library(shiny)
library(ggplot2)
library(rayplot)

# a served directory for the generated page + wasm assets
assets <- file.path(tempdir(), "rayplot_web")
dir.create(assets, showWarnings = FALSE)
addResourcePath("rpweb", assets)
file.copy(
  list.files(system.file("web", package = "rayplot"),
             pattern = "rayplot\\.(js|wasm)$", full.names = TRUE),
  assets, overwrite = TRUE
)

ui <- fluidPage(
  selectInput("pal", "Palette", c("Set2", "Dark2", "Paired")),
  uiOutput("frame")
)

server <- function(input, output, session) {
  output$frame <- renderUI({
    p <- ggplot(CO2, aes(factor(conc), uptake,
                         group = interaction(conc, Treatment, Type))) +
      geom_boxplot(aes(fill = interaction(Treatment, Type)),
                   position = position_dodge(width = 0.8)) +
      geom_point(aes(colour = interaction(Treatment, Type)),
                 position = position_dodge(width = 0.8)) +
      scale_fill_brewer(palette = input$pal) +
      theme(legend.position = "none")

    rayplot_web(p, file.path(assets, "plot.html"), width = 900L, height = 620L)

    tags$iframe(
      src = paste0("rpweb/plot.html?t=", as.integer(Sys.time())),
      width = 920, height = 660,
      style = "border:1px solid #e4e7ed;border-radius:6px"
    )
  })
}

shinyApp(ui, server)

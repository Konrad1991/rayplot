# rayplot in Shiny -- approach B: one <canvas>, the wasm module loaded once,
# each update pushed as a few-KB websocket message. No iframe, no page reload.
#
# Needs the wasm build first:  bash inst/web/build.sh   (Emscripten SDK)
#
#   shiny::runApp(system.file("examples/shiny_canvas.R", package = "rayplot"))

library(shiny)
library(ggplot2)
library(rayplot)

ui <- fluidPage(
  titlePanel("rayplot in Shiny (WebAssembly)"),
  sidebarLayout(
    sidebarPanel(
      selectInput("pal", "Fill palette", c("Set2", "Dark2", "Paired", "Set1")),
      sliderInput("dodge", "Dodge width", 0.4, 1.0, 0.8, step = 0.1),
      checkboxInput("pts", "Show points", TRUE)
    ),
    mainPanel(
      rayplotCanvas("plot", width = 900L, height = 620L)
    )
  )
)

server <- function(input, output, session) {
  observe({
    p <- ggplot(CO2, aes(factor(conc), uptake,
                         group = interaction(conc, Treatment, Type))) +
      geom_boxplot(aes(fill = interaction(Treatment, Type)),
                   position = position_dodge(width = input$dodge)) +
      scale_fill_brewer(palette = input$pal) +
      theme(legend.position = "none")

    if (isTRUE(input$pts)) {
      p <- p + geom_point(aes(colour = interaction(Treatment, Type)),
                          position = position_dodge(width = input$dodge))
    }

    rayplot_send(p, "plot", width = 900L, height = 620L)
  })
}

shinyApp(ui, server)

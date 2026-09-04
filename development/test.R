install.packages(".", types = "source", repos = NULL)

okabe_ito <- c(
  "#E69F00",  # orange
  "#56B4E9",  # sky blue
  "#009E73",  # bluish green
  "#F0E442",  # yellow
  "#0072B2",  # blue
  "#D55E00",  # vermillion
  "#CC79A7",  # reddish purple
  "#000000"   # black
)
colours <- palette.colors(
  length(levels(CO2$Treatment)), "okabe_ito"
)
rayplot:::rayplot_scatter(
  CO2$conc, CO2$uptake, CO2$Treatment, colours
)
a <- 1

rayplot:::rayplot3D_scatter(
  CO2$conc, CO2$uptake, as.numeric(CO2$Treatment),
  point_radius = 0.1
)

x_data <- c(100, 250, 400, 550, 700)
y_data <- c(5,   25,  12,  48,  31)
z_data <- c(0.1, 0.9, 0.4, 0.7, 0.2)
rayplot:::rayplot3D_scatter(x_data, y_data, z_data, point_radius = 0.25)

system('find -name "*.o" | xargs rm')
system('find -name "*.so" | xargs rm')
install.packages(".", types = "source", repos = NULL)

library(ggplot2)
library(rayplot)
p <- ggplot(CO2, aes(
  x = factor(conc), y = uptake,
  group = interaction(conc, Treatment, Type)
)) +
  geom_boxplot(
    aes(fill = interaction(Treatment, Type)),
    position = position_dodge(width = 0.8)
  ) +
  geom_point(
    aes(colour = interaction(Treatment, Type)),
    position = position_dodge(width = 0.8),
  ) +
  scale_fill_brewer(palette = "Set2") +
  scale_colour_grey(start = 0.1, end = 0.5) +
  theme(legend.position = "none")
rayplot(p)
rayplot_close()

rosenbrock <- function(x, y) (1 - x)^2 + 100*(y - x^2)^2
grid <- expand.grid(
  x = seq(-2, 2, length.out = 500),
  y = seq(-2, 2, length.out = 500)
)
grid$error <- rosenbrock(grid$x, grid$y)
p <- ggplot(grid,
  aes(x = x, y = y,
    z = log1p(error), fill = log1p(error))) +
  geom_tile() +
  scale_fill_viridis_c()
rayplot(p)

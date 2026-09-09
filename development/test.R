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

p <- ggplot(mtcars, aes(x = wt, y = mpg, z = hp)) +
  geom_point(size = 2)
rayplot(p)
p <- ggplot(CO2,
  aes(x = conc, y = uptake, z = Treatment)) +
  geom_point(size = 2)
rayplot(p)

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

p <- ggplot(CO2,
  aes(x = conc, y = uptake, z = Treatment, colour = Type)) +
  geom_point(size = 2)
rayplot(p)
rayplot_close()

ms <- readRDS("development/data/ms_sample.rds")
n_mz_bins <- 300
mz_bin_width <- diff(range(ms$mz)) / n_mz_bins
rt_bin_width <- min(diff(sort(unique(ms$retention_time)))) / 2
p <- ggplot(ms, aes(x = mz, y = retention_time, z = intensity)) +
  stat_summary_2d(binwidth = c(mz_bin_width, rt_bin_width), fun = max, drop = FALSE)
rayplot(p)
rayplot3D_close()

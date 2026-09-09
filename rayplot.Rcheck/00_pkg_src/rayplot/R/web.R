# WebAssembly export (Model A): R builds the layer spec and serialises it; the
# browser renders it with the Emscripten build of the raylib core. See
# inst/web/ for the C entry point, build script and HTML template.

# Layer specs for a plot, identical to what rayplot() feeds the C renderer.
rayplot_spec <- function(p, ...) {
  if (inherits(p, "ggplot")) resolve_layers(p) else c(list(p), list(...))
}

# Serialise layer specs to the little-endian binary blob documented in
# inst/web/rayplot_web.c. Returns a raw vector.
rayplot_serialize <- function(layers, width = 800L, height = 600L) {
  con <- rawConnection(raw(0), "wb")
  on.exit(close(con))

  u32 <- function(v) writeBin(as.integer(v), con, size = 4L, endian = "little")
  f64 <- function(v) writeBin(as.double(v),  con, size = 8L, endian = "little")
  i32 <- function(v) writeBin(as.integer(v), con, size = 4L, endian = "little")
  str <- function(s) {
    b <- charToRaw(enc2utf8(s))
    u32(length(b)); writeBin(b, con)
  }
  cols <- function(hex) {
    m <- grDevices::col2rgb(hex)
    for (j in seq_len(ncol(m))) {
      writeBin(as.raw(c(m[1L, j], m[2L, j], m[3L, j], 255L)), con)
    }
  }

  writeBin(charToRaw("RPLT"), con)
  u32(1L)                       # version
  u32(width); u32(height)

  # ticks: from the first layer that carries them
  tp <- NULL; tl <- NULL
  for (l in layers) {
    if (!is.null(l$tick_pos)) { tp <- l$tick_pos; tl <- l$tick_labels; break }
  }
  if (is.null(tp)) {
    u32(0L)
  } else {
    u32(length(tp)); f64(tp)
    for (s in tl) str(as.character(s))
  }

  u32(length(layers))
  for (l in layers) {
    if (identical(l$type, "scatter")) {
      u32(0L)
      u32(length(l$colours)); cols(l$colours)
      u32(length(l$x)); f64(l$point_radius)
      f64(l$x); f64(l$y); i32(l$colour_groups)
    } else if (identical(l$type, "box")) {
      u32(1L)
      u32(length(l$colours)); cols(l$colours)
      u32(length(l$centers)); f64(l$box_width)
      f64(l$centers); f64(l$first_quantiles); f64(l$medians); f64(l$third_quantiles)
      f64(l$whisker_low); f64(l$whisker_high); i32(l$colour_groups)
      u32(length(l$outliers)); f64(l$outliers); i32(l$outlier_start)
    } else {
      stop("rayplot_web: unsupported layer type '", l$type, "'")
    }
  }

  rawConnectionValue(con)
}

# Write a self-contained HTML page that renders `p` in the browser.
#
# `file`   destination .html path.
# `assets` directory holding rayplot.js / rayplot.wasm / template.html
#          (default: the installed inst/web). Build them with inst/web/build.sh.
rayplot_web <- function(p, file, ..., width = 800L, height = 600L,
                        title = "rayplot",
                        assets = system.file("web", package = "rayplot")) {
  width  <- as.integer(width)
  height <- as.integer(height)
  stopifnot(
    "width and height must be positive integers" =
      !is.na(width) && !is.na(height) && width >= 1L && height >= 1L,
    "assets directory not found" = nzchar(assets) && dir.exists(assets)
  )

  layers <- rayplot_spec(p, ...)
  blob   <- rayplot_serialize(layers, width, height)
  # base64_enc() wraps lines; the blob goes into a JS string literal, so it must
  # be one unbroken line.
  b64    <- gsub("[[:space:]]", "", jsonlite::base64_enc(blob))

  tmpl <- readChar(file.path(assets, "template.html"),
                   file.info(file.path(assets, "template.html"))$size, useBytes = TRUE)
  html <- tmpl
  html <- gsub("@@SPEC_B64@@", b64, html, fixed = TRUE)
  html <- gsub("@@WIDTH@@",  format(width),  html, fixed = TRUE)
  html <- gsub("@@HEIGHT@@", format(height), html, fixed = TRUE)
  html <- gsub("@@TITLE@@",  title, html, fixed = TRUE)
  writeChar(html, file, eos = NULL, useBytes = TRUE)

  wasm_bits <- file.path(assets, c("rayplot.js", "rayplot.wasm"))
  if (all(file.exists(wasm_bits))) {
    file.copy(wasm_bits, dirname(normalizePath(file, mustWork = FALSE)),
              overwrite = TRUE)
  } else {
    warning("rayplot.js / rayplot.wasm not found in '", assets,
            "'. Build them with inst/web/build.sh, then copy them next to '",
            file, "'.", call. = FALSE)
  }

  invisible(normalizePath(file, mustWork = FALSE))
}

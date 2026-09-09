/* Shiny <-> rayplot wasm glue (approach B: no iframe).
 *
 * Loaded once as an htmlDependency by rayplotCanvas(). It boots the Emscripten
 * module against the page's <canvas>, then listens for "rayplot" custom
 * messages from R (rayplot_send()) carrying a base64 layer-spec blob and feeds
 * them to the running renderer.
 */
(function () {
  "use strict";

  var RAYPLOT_JS;
  try {
    RAYPLOT_JS = new URL("rayplot.js", document.currentScript.src).href;
  } catch (e) {
    RAYPLOT_JS = "rayplot.js";
  }

  var mod = null;       // the Emscripten Module, once runtime is ready
  var started = false;  // has rayplot_web_start run yet
  var pending = null;   // latest blob received before the module was ready
  var booted = false;   // have we injected rayplot.js

  function b64ToBytes(b64) {
    var bin = atob(b64), n = bin.length, out = new Uint8Array(n);
    for (var i = 0; i < n; i++) out[i] = bin.charCodeAt(i);
    return out;
  }

  function feed(bytes) {
    var ptr = mod._malloc(bytes.length);
    mod.HEAPU8.set(bytes, ptr);
    mod.ccall(started ? "rayplot_web_update" : "rayplot_web_start",
              null, ["number", "number"], [ptr, bytes.length]);
    mod._free(ptr);
    started = true;
  }

  function boot(canvas) {
    booted = true;
    // CSS size must equal the drawing-buffer size or raylib's mouse
    // coordinates are scaled wrong.
    canvas.style.width  = canvas.getAttribute("width") + "px";
    canvas.style.height = canvas.getAttribute("height") + "px";

    window.Module = {
      canvas: canvas,
      print: function (t) { console.log(t); },
      printErr: function (t) { console.error(t); },
      onRuntimeInitialized: function () {
        mod = window.Module;
        if (pending) { feed(pending); pending = null; }
      }
    };
    var s = document.createElement("script");
    s.src = RAYPLOT_JS;
    s.onerror = function () { console.error("[rayplot] failed to load " + RAYPLOT_JS); };
    document.head.appendChild(s);
  }

  function onMessage(msg) {
    var canvas = document.getElementById(msg.id || "rayplot");
    if (!canvas) { console.warn("[rayplot] no canvas #" + (msg.id || "rayplot")); return; }
    if (!booted) boot(canvas);
    var bytes = b64ToBytes(msg.b64);
    if (mod) feed(bytes); else pending = bytes;   // coalesce until ready
  }

  function register() {
    if (window.Shiny && Shiny.addCustomMessageHandler) {
      Shiny.addCustomMessageHandler("rayplot", onMessage);
    } else {
      setTimeout(register, 50);
    }
  }
  register();
})();

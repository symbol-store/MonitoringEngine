/* Shadows the reused chibi source tree's host-generated chibi/install.h, whose
 * baked-in macOS paths and .dylib suffix are wrong for the browser. */
#define sexp_so_extension ".so"
#define sexp_default_module_path "."
#define sexp_platform "emscripten"
#define sexp_architecture "wasm32"
#define sexp_version "0.12.0"
#define sexp_release_name "magnesium"

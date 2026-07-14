#include "ExpressionParser.hpp"

#include <emscripten/bind.h>

#include <string>
#include <unistd.h>

namespace {

// chibi's static-module matcher only resolves keys like "srfi/69/hash" when cwd
// is the module dir with module path "." -- so chdir here (in C++, after the
// preloaded FS is mounted), not in a JS preRun hook.
sexp initializeContextInModuleDir() {
  chdir("/chibi");
  return boss::initialize_boss_context();
}

struct WasmContext {
  boss::BossContextGuard guard{initializeContextInModuleDir()};
  sexp env = guard.ctx == nullptr ? nullptr : sexp_context_env(guard.ctx);
};

WasmContext& sharedContext() {
  static WasmContext context;
  return context;
}

boss::EvalResult evaluate(std::string const& expression, bool pretty) {
  WasmContext& context = sharedContext();
  if(context.guard.ctx == nullptr) {
    return {true, "failed to initialize BOSS chibi context"};
  }
  return boss::evaluate_expression(context.guard.ctx, context.env, expression, pretty);
}

} // namespace

EMSCRIPTEN_BINDINGS(boss) {
  emscripten::value_object<boss::EvalResult>("EvalResult")
      .field("isError", &boss::EvalResult::is_error)
      .field("text", &boss::EvalResult::text);
  emscripten::function("evaluate", &evaluate);
}

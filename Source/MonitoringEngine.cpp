#include <BOSS.hpp>
#include <Expression.hpp>
#include <ExpressionUtilities.hpp>
#include <Utilities.hpp>
#include <deque>
#include <sstream>

using std::string_literals::operator""s;
using boss::utilities::operator""_;
using boss::ComplexExpression;
using boss::Expression;
using boss::Span;
using boss::Symbol;
using namespace boss::utilities::experimental::sentinel;
using namespace boss::utilities::experimental;

static Expression evaluate(Expression&& e) {
  static auto log = std::deque<Expression>();
  return std::move(e) < "GetEntryPoint"_(Symbol_) >= [](auto, auto dynamics, auto) -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      if(get<Symbol>(e->delegate).getName() == "index") {
        return new BOSSExpression(R"---(<!DOCTYPE html>
<meta charset="utf-8">
<button id="refresh">Refresh</button>
<script type="importmap">
{
  "imports": {
    "@codemirror/state": "https://esm.sh/*@codemirror/state@6.7.1",
    "@codemirror/view": "https://esm.sh/*@codemirror/view@6.43.6",
    "@codemirror/language": "https://esm.sh/*@codemirror/language@6.12.4",
    "@codemirror/legacy-modes/": "https://esm.sh/*@codemirror/legacy-modes@6.5.3/",
    "@lezer/common": "https://esm.sh/*@lezer/common@1.5.2",
    "@lezer/highlight": "https://esm.sh/*@lezer/highlight@1.2.3",
    "@lezer/lr": "https://esm.sh/*@lezer/lr@1.4.10",
    "@marijn/find-cluster-break": "https://esm.sh/@marijn/find-cluster-break@1.0.3",
    "style-mod": "https://esm.sh/style-mod@4.1.3",
    "w3c-keyname": "https://esm.sh/w3c-keyname@2.2.8",
    "crelt": "https://esm.sh/crelt@1.0.7"
  }
}
</script>
<script type="module">
import { EditorView } from "@codemirror/view";
import { EditorState } from "@codemirror/state";
import { StreamLanguage, foldGutter, foldService, syntaxHighlighting, defaultHighlightStyle } from "@codemirror/language";
import { commonLisp } from "@codemirror/legacy-modes/mode/commonlisp";

const lispFold = foldService.of((state, lineStart, lineEnd) => {
  const s = state.doc.toString();
  let open = -1;
  for (let i = lineStart; i < lineEnd; i++) {
    if (s[i] === "(") { open = i; break; }
    if (s[i] === ";" || s[i] === '"') break;
  }
  if (open === -1) return null;

  let depth = 0, inString = false;
  for (let i = open; i < s.length; i++) {
    const ch = s[i];
    if (inString) {
      if (ch === "\\") i++;
      else if (ch === '"') inString = false;
      continue;
    }
    if (ch === '"') inString = true;
    else if (ch === ";") { while (i < s.length && s[i] !== "\n") i++; }
    else if (ch === "(") depth++;
    else if (ch === ")") {
      if (--depth === 0) {
        return i > lineEnd ? { from: lineEnd, to: i + 1 } : null;
      }
    }
  }
  return null;
});

const view = new EditorView({
  state: EditorState.create({
    doc: ";; loading…",
    extensions: [
      foldGutter({ openText: "▾", closedText: "▸" }),
      StreamLanguage.define(commonLisp),
      syntaxHighlighting(defaultHighlightStyle),
      lispFold,
      EditorState.readOnly.of(true),
      EditorView.editable.of(false),
    ],
  }),
  parent: document.body,
});

function setDoc(text) {
  view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text } });
}

async function loadCode() {
  const SOURCE = "/EvaluateInEngines/List/Monitoring/:/report/////text";
  setDoc(";; loading…");
  try {

  
    const res = await fetch(SOURCE, { cache: "no-store" });
    if (!res.ok) throw new Error(res.status + " " + res.statusText);
    setDoc(await res.text());
  } catch (e) {
    setDoc(";; failed to load " + SOURCE + "\n;; " + e);
  }
}

document.getElementById("refresh").addEventListener("click", loadCode);
loadCode();
</script>
)---");
      }
      auto logString = std::stringstream();
      for(auto& it : log)
        logString << boss::pretty << it << std::endl;
      if(log.empty())
        logString << ";; no pending queries" << std::endl;
      return new BOSSExpression(logString.str());
    };
  } < Any_ >= [](Symbol&& head, auto&& statics, auto&& dynamics, auto&& spans) {
    auto result =
        ComplexExpression(head, std::move(statics), std::move(dynamics), std::move(spans));
    if(log.size() > 10)
      log.pop_back();
    log.push_front(result.clone(boss::expressions::CloneReason::FOR_TESTING));
    return std::move(result);
  };
}

extern "C" BOSSExpression* evaluate(BOSSExpression* e) {
  return new BOSSExpression {.delegate = evaluate(std::move(e->delegate))};
};

#include <BOSS.hpp>
#include <Expression.hpp>
#include <ExpressionUtilities.hpp>
#include <Utilities.hpp>
#include <sstream>

using std::string_literals::operator""s;
using boss::utilities::operator""_;
using boss::ComplexExpression;
using boss::Expression;
using boss::Span;
using boss::Symbol;
using namespace boss::utilities::experimental::sentinel;
using namespace boss::utilities::experimental;

template <bool isRoot> static Expression evaluate(Expression&& e) {
  static auto log = std::vector<Expression>();
  return std::move(e) < "GetEntryPoint"_(Symbol_) >= [](auto, auto dynamics, auto) -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      auto logString = std::stringstream();
      logString << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>MonitoringEngine: All Clear</title>
</head>
<body>
  <h1>Everything is fine.</h1>
        <pre>
)";
      for(auto& it : log)
        logString << it << std::endl;
      logString << "</pre></body></html>";
      return new BOSSExpression(logString.str());
    };
  } < Any_ >= [](Symbol&& head, auto&& statics, auto&& dynamics, auto&& spans) {
    auto result =
        ComplexExpression(head, std::move(statics), std::move(dynamics), std::move(spans));
    log.push_back(result.clone(boss::expressions::CloneReason::FOR_TESTING));
    return std::move(result);
  };
}

extern "C" BOSSExpression* evaluate(BOSSExpression* e) {
  return new BOSSExpression {.delegate = evaluate<true>(std::move(e->delegate))};
};

#include <BOSS.hpp>
#include <Expression.hpp>
#include <ExpressionUtilities.hpp>
#include <Utilities.hpp>

using std::string_literals::operator""s;
using boss::utilities::operator""_;
using boss::ComplexExpression;
using boss::Expression;
using boss::Span;
using boss::Symbol;
using namespace boss::utilities::experimental::sentinel;
using namespace boss::utilities::experimental;

static Expression evaluate(Expression&& e) {
  return std::move(e)<"GetEntryPoint"_(Symbol_) >= Recurse(evaluate)>[](auto, auto dynamics,
                                                                        auto) -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      return new BOSSExpression(R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>MonitoringEngine: All Clear</title>
</head>
<body>
  <h1>Everything is fine.</h1>
  <p>The MonitoringEngine is up. The MonitoringEngine is running. The MonitoringEngine has checked that the MonitoringEngine is up and running, and reports the following finding: it is.</p>
  <p>This page was hand-delivered to you by a process which is, as previously mentioned, completely fine. Should that change, this page will of course be the first to know &mdash; and the last to admit it.</p>
</body>
</html>
)"s);
    };
  };
}

extern "C" BOSSExpression* evaluate(BOSSExpression* e) {
  return new BOSSExpression {.delegate = evaluate(std::move(e->delegate))};
};

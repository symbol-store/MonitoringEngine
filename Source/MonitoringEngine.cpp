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

static Expression evaluate(Expression&& e) {
  static auto log = std::vector<Expression>();
  auto logAndReturn = [](Expression&& e) -> Expression {
    log.push_back(e.clone(boss::expressions::CloneReason::FOR_TESTING));
    return e;
  };
  return std::move(e)<"GetEntryPoint"_(Symbol_) >= Recurse(evaluate)>[](auto, auto dynamics,
                                                                        auto) -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      auto logString = std::stringstream(R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>MonitoringEngine: All Clear</title>
</head>
<body>
  <h1>Everything is fine.</h1>
  <p>The MonitoringEngine is up. The MonitoringEngine is running. The MonitoringEngine has checked that the MonitoringEngine is up and running, and reports the following finding: it is.</p>
        <p>This page was hand-delivered to you by a process which is, as previously mentioned, completely fine. Should that change, this page will of course be the first to know &mdash; and the last to admit it.</p>
        <pre>
        )");
      for(auto& it : log)
        logString << it << std::endl;
      logString << "</pre></body></html>";
      return new BOSSExpression(logString.str());
    };
  } >= Recurse(logAndReturn);
}

extern "C" BOSSExpression* evaluate(BOSSExpression* e) {
  return new BOSSExpression {.delegate = evaluate(std::move(e->delegate))};
};

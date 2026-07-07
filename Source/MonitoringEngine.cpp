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

extern const char indexHtml[];

static Expression evaluate(Expression&& e) {
  static auto log = std::deque<Expression>();
  return std::move(e) < "GetEntryPoint"_(Symbol_) >= [](auto, auto dynamics, auto) -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      if(get<Symbol>(e->delegate).getName() == "index") {
        return new BOSSExpression(indexHtml);
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

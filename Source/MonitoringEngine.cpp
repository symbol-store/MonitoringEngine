#include <BOSS.hpp>
#include <Expression.hpp>
#include <ExpressionUtilities.hpp>
#include <Utilities.hpp>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <semaphore>
#include <sstream>

using boss::utilities::operator""_;
using boss::ComplexExpression;
using boss::Expression;
using boss::Symbol;
using namespace boss::utilities::experimental::sentinel;
using namespace boss::utilities::experimental;

extern const char indexHtml[];

static Expression evaluate(Expression&& e) {
  static struct {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::pair<std::reference_wrapper<Expression>,
                         std::reference_wrapper<std::binary_semaphore>>>
        log;
    void emplace_back(std::reference_wrapper<Expression> e,
                      std::reference_wrapper<std::binary_semaphore> b) {
      {
        std::lock_guard guard(mutex);
        log.emplace_back(e, b);
      }
      condition.notify_one();
    }
    std::pair<std::reference_wrapper<Expression>, std::reference_wrapper<std::binary_semaphore>>
    wait_and_pop_front() {
      std::unique_lock lock(mutex);
      condition.wait(lock, [this] { return !log.empty(); });
      auto result = std::move(log.front());
      log.pop_front();
      return result;
    }

    std::reference_wrapper<Expression> wait_and_front_expression() {
      std::unique_lock lock(mutex);
      condition.wait(lock, [this] { return !log.empty(); });
      auto result = log.front().first;
      return result;
    }
  } log;
  return std::move(e) < "GetEntryPoint"_(Symbol_) >=
             [](auto, auto dynamics,
                auto) -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      return new BOSSExpression(
          (std::stringstream() << Expression(
               std::move(e->delegate) < ""_() >= [&](auto&&...) -> Expression {
                 return ""_;
               } < "approve"_ >= [&](auto&&...) -> Expression {
                 log.wait_and_pop_front().second.get().release();
                 return std::move(log.wait_and_front_expression().get());
               } < "reject"_ >= [&](auto&&...) -> Expression {
                 auto [expression, blocked] = log.wait_and_pop_front();
                 expression.get() = "RejectedByUser"_();
                 blocked.get().release();
                 return std::move(log.wait_and_front_expression().get());
               } < "index"_ >= [&](auto&&...) -> Expression {
                 auto f = std::ifstream("index.html");
                 return f ? (std::stringstream() << f.rdbuf()).str() : indexHtml;
               } < "refresh"_ >= [&](auto&&...) -> Expression {
                 return std::move(log.wait_and_front_expression().get());
               }))
              .str());
    };
  } < Any_ >= boss::utilities::overload( //
                             [&](Expression&& result) {
                               std::binary_semaphore signal {0};
                               log.emplace_back(result, signal);
                               signal.acquire();
                               return std::move(result);
                             },
                             [&](auto&&... args) {
                               auto result = Expression(ComplexExpression(std::move(args)...));
                               std::binary_semaphore signal {0};
                               log.emplace_back(result, signal);
                               signal.acquire();
                               return std::move(result);
                             }

              );
}

extern "C" BOSSExpression* evaluate(BOSSExpression* e) {
  return new BOSSExpression {.delegate = evaluate(std::move(e->delegate))};
};

#include <BOSS.hpp>
#include <Expression.hpp>
#include <ExpressionUtilities.hpp>
#include <Utilities.hpp>
#include <algorithm>
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
using boss::utilities::overload;

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
      using ss = std::stringstream;
      using namespace boss::expressions;
      return new BOSSExpression(Expression(
          std::move(e->delegate) < ""_() >= [&](auto&&...) -> Expression {
            return ""_;
          } < "approve"_ >= [&](auto&&...) -> Expression {
            log.wait_and_pop_front().second.get().release();
            return (ss() << boss::pretty << std::move(log.wait_and_front_expression().get())).str();
          } < "reject"_ >= [&](auto&&...) -> Expression {
            auto [expression, blocked] = log.wait_and_pop_front();
            expression.get() = "RejectedByUser"_();
            blocked.get().release();
            return (ss() << boss::pretty << std::move(log.wait_and_front_expression().get())).str();
          } < "refresh"_ >= [&](auto&&...) -> Expression {
            return (ss() << boss::pretty << std::move(log.wait_and_front_expression().get())).str();
          } < "index"_ >= [&](auto&&...) -> Expression {
            return indexHtml; //
          } < Symbol_ >= [](auto&&... args) -> Expression {
            return std::invoke(
                overload(
                    [](Expression&& matched) -> Expression {
                      auto const& name = std::get<Symbol>(matched).getName();
                      if(auto f = std::ifstream(name, std::ios::binary | std::ios::ate)) {
                        const std::streamsize size = f.tellg();
                        f.seekg(0, std::ios::beg);
                        auto data = std::vector<int8_t>(size);
                        f.read(reinterpret_cast<char*>(data.data()), size);
                        if(std::find(data.begin(), data.end(), int8_t {0}) == data.end())
                          return std::string(data.begin(), data.end());
                        else {
                          auto spans = ExpressionSpanArguments {};
                          spans.emplace_back(atoms::Span<int8_t>(std::move(data)));
                          return ComplexExpression("Binary"_, {}, {}, std::move(spans));
                        }
                      }
                      return "CouldNotOpenFile"_(std::move(matched));
                    },
                    [](auto&&... s) -> Expression { return "UnsupportedOperation"_(); }),
                std::move(args)...);
          }));
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

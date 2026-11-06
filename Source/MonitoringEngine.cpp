#include <BOSS.hpp>
#include <Expression.hpp>
#include <ExpressionUtilities.hpp>
#include <Utilities.hpp>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <optional>
#include <semaphore>
#include <set>
#include <sstream>
#include <zlib.h>

using boss::utilities::operator""_;
using boss::ComplexExpression;
using boss::Expression;
using boss::Symbol;
using namespace boss::utilities::experimental::sentinel;
using namespace boss::utilities::experimental;
using boss::utilities::overload;

extern const char indexHtml[];
extern const unsigned char bossJs[];
extern const unsigned long bossJs_len, bossJs_rawlen;
extern const unsigned char bossWasm[];
extern const unsigned long bossWasm_len, bossWasm_rawlen;
extern const unsigned char bossData[];
extern const unsigned long bossData_len, bossData_rawlen;
extern const unsigned char plotJs[];
extern const unsigned long plotJs_len, plotJs_rawlen;

namespace {
std::vector<int8_t> inflateGz(unsigned char const* d, unsigned long n, unsigned long raw) {
  auto out = std::vector<int8_t>(raw);
  z_stream zs {};
  if(inflateInit2(&zs, 15 + 32) != Z_OK) // 15+32: auto-detect the gzip header
    return {};
  zs.next_in = const_cast<Bytef*>(d);
  zs.avail_in = n;
  zs.next_out = reinterpret_cast<Bytef*>(out.data());
  zs.avail_out = raw;
  auto rc = inflate(&zs, Z_FINISH);
  inflateEnd(&zs);
  out.resize(rc == Z_STREAM_END ? zs.total_out : 0);
  return out;
}

std::vector<int8_t>& bossWasmBytes() {
  static auto bytes = inflateGz(bossWasm, bossWasm_len, bossWasm_rawlen);
  return bytes;
}
std::vector<int8_t>& bossDataBytes() {
  static auto bytes = inflateGz(bossData, bossData_len, bossData_rawlen);
  return bytes;
}
std::string& bossJsText() {
  static auto text = [] {
    auto v = inflateGz(bossJs, bossJs_len, bossJs_rawlen);
    return std::string(v.begin(), v.end());
  }();
  return text;
}
std::string& plotJsText() {
  static auto text = [] {
    auto v = inflateGz(plotJs, plotJs_len, plotJs_rawlen);
    return std::string(v.begin(), v.end());
  }();
  return text;
}

using SessionID = std::int64_t;

bool isExemptFromApproval(Symbol const& head) {
  static auto const exemptHeads = std::set<std::string> {"GetEngineDescription"};
  return exemptHeads.contains(head.getName());
}
} // namespace

static Expression evaluate(Expression&& e) {
  static struct {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::pair<std::reference_wrapper<Expression>,
                         std::reference_wrapper<std::binary_semaphore>>>
        log;
    SessionID nextSession = 0;
    SessionID currentSession = 0;
    void holdQuery(Expression& e, std::binary_semaphore& b) {
      {
        std::lock_guard guard(mutex);
        log.emplace_back(e, b);
      }
      condition.notify_one();
    }
    bool waitForHeldQuery(std::unique_lock<std::mutex>& lock) {
      auto const session = currentSession;
      condition.wait(lock, [&] { return !log.empty() || currentSession != session; });
      return currentSession == session;
    }

    std::optional<std::pair<std::reference_wrapper<Expression>,
                            std::reference_wrapper<std::binary_semaphore>>>
    takeOldestHeldQuery() {
      std::unique_lock lock(mutex);
      if(!waitForHeldQuery(lock))
        return {};
      auto result = std::move(log.front());
      log.pop_front();
      return result;
    }

    std::optional<std::reference_wrapper<Expression>> peekOldestHeldQuery() {
      std::unique_lock lock(mutex);
      if(!waitForHeldQuery(lock))
        return {};
      return log.front().first;
    }

    // Makes a new session current and returns its id. Then wakes every wait: a wait whose
    // session is no longer current returns an empty optional instead of a query, and the report
    // command that waited replies (ConnectionClosed) and leaves the held queries in the queue.
    // The next query therefore wakes the new monitor's wait instead of a wait whose socket is
    // gone.
    SessionID connect() {
      auto session = SessionID {};
      {
        std::scoped_lock lock(mutex);
        session = currentSession = ++nextSession;
      }
      condition.notify_all();
      return session;
    }

    // Ends the given session if that session is still current, and wakes every wait in the
    // same way as connect.
    void close(SessionID session) {
      {
        std::scoped_lock lock(mutex);
        if(currentSession != session)
          return;
        currentSession = 0;
      }
      condition.notify_all();
    }
  } log;
  return std::move(e) < "GetEntryPoint"_(Symbol_) >= [](auto, auto dynamics, auto)
             -> Expression {
    static auto mode = std::get<Symbol>(dynamics[0]);
    return (long long)+[](BOSSExpression* e) -> BOSSExpression* {
      using ss = std::stringstream;
      using namespace boss::expressions;
      auto binarySpan = [](std::vector<int8_t>& data) -> Expression {
        auto spans = ExpressionSpanArguments {};
        spans.emplace_back(atoms::Span<int8_t>(data.data(), data.size(), {})); // non-owning view
        return ComplexExpression("Binary"_, {}, {}, std::move(spans));
      };
      auto nextQuery = []() -> Expression {
        auto front = log.peekOldestHeldQuery();
        if(!front)
          return "ConnectionClosed"_();
        return (ss() << boss::pretty << front->get()).str();
      };
      auto releaseFront = [](auto&& replaceQuery, auto&& reply) -> Expression {
        auto front = log.takeOldestHeldQuery();
        if(!front)
          return "ConnectionClosed"_();
        auto [query, blocked] = *front;
        replaceQuery(query);
        blocked.get().release();
        return reply();
      };
      return new BOSSExpression(
          std::move(e->delegate) < ""_() >= [&](auto&&...) -> Expression {
            return ""_;
          } < "approve"_ >= [&](auto&&...) -> Expression {
            return releaseFront([](Expression&) {}, nextQuery);
          } < "reject"_ >= [&](auto&&...) -> Expression {
            return releaseFront(
                [](Expression& query) { query = "RejectedByUserWithoutGivenReason"_(); },
                nextQuery);
          } < "RejectedByUserWithReason"_(String_) >= [&](auto, auto dynamics, auto) -> Expression {
            return releaseFront(
                [&](Expression& query) {
                  query = "RejectedByUserWithReason"_(get<std::string>(dynamics[0]));
                },
                nextQuery);
          } < "Approve"_(Any_) >= [&](auto, auto dynamics, auto) -> Expression {
            auto approved = (ss() << boss::pretty << dynamics[0]).str();
            return releaseFront([&](Expression& query) { query = std::move(dynamics[0]); },
                                [&] { return approved; });
          } < "connect"_ >= [&](auto&&...) -> Expression {
            return "Session"_(log.connect(), "Connected"_());
          } < "refresh"_ >= [&](auto&&...) -> Expression {
            return nextQuery(); //
          } < "index"_ >= [&](auto&&...) -> Expression {
            return indexHtml; //
          } < "boss.js"_ >= [&](auto&&...) -> Expression {
            return "ContentType"_("text/javascript", bossJsText()); //
          } < "plot.js"_ >= [&](auto&&...) -> Expression {
            return "ContentType"_("text/javascript", plotJsText()); //
          } < "boss.wasm"_ >= [&](auto&&...) -> Expression {
            return "ContentType"_("application/wasm", binarySpan(bossWasmBytes())); //
          } < "boss.data"_ >= [&](auto&&...) -> Expression {
            return binarySpan(bossDataBytes()); //
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
                        if(std::find(data.begin(), data.end(), 0) == data.end())
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
          });
    };
  } < "ConnectionClosed"_(Integer_) >= [](auto, auto dynamics, auto)
                 -> Expression {
    log.close(get<SessionID>(dynamics[0]));
    return "ConnectionClosed"_(std::move(dynamics[0]));
  } < Any_ >= boss::utilities::overload( //
                        [&](Expression&& result) {
                          std::binary_semaphore signal {0};
                          log.holdQuery(result, signal);
                          signal.acquire();
                          return std::move(result);
                        },
                        [&](auto&&... args) {
                          auto result = Expression(ComplexExpression(std::move(args)...));
                          if(isExemptFromApproval(std::get<ComplexExpression>(result).getHead()))
                            return result;
                          std::binary_semaphore signal {0};
                          log.holdQuery(result, signal);
                          signal.acquire();
                          return result;
                        }

              );
}

extern "C" BOSSExpression* evaluate(BOSSExpression* e) {
  return new BOSSExpression {.delegate = evaluate(std::move(e->delegate))};
};

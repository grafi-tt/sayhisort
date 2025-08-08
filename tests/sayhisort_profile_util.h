#ifndef SAYHISORT_PROFILE_UTIL_H
#define SAYHISORT_PROFILE_UTIL_H

// Requires C++23.
// Generalized code isn't directly related to sayhisort logic.
// Just playing to create handy micro profiling utility.

#include <cstddef>
#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <chrono>
#include <cstdint>

namespace sayhisort::test {
namespace {

/**
 * Internal core to record and report stats
 */

std::size_t g_report_indent;

std::ostream& WriteReportIndent(std::ostream& os, std::size_t offset = 0) {
    for (std::size_t i = 0; i < g_report_indent + offset; ++i) {
        os << "  ";
    }
    return os;
}

class ReportEntity {
public:
    ReportEntity(std::ostream& os) : os_{os} {}
    template <typename... NameT, typename... ValueT>
    void operator()(std::tuple<NameT...> name, std::tuple<ValueT...> value) {
        WriteReportIndent(os_, 1);
        std::apply([this](const auto&... args) { (os_ << ... << args); }, name);
        os_ << ": ";
        std::apply([this](const auto&... args) { (os_ << ... << args); }, value);
        os_ << "\n";
    }
    template <typename NameT, typename ValueT>
    void operator()(const NameT& name, const ValueT& value) {
        this->operator()(std::forward_as_tuple(name), std::forward_as_tuple(value));
    }

private:
    std::ostream& os_;
};

auto& GetRegistry() {
    static std::multimap<std::string, std::tuple<void*, void (*)(void*, ReportEntity), bool (*)(const void*)>,
                         std::less<>>
        registry;
    return registry;
}

void RegisterReporterImpl(std::string_view key, void* p, void (*reporter)(void*, ReportEntity),
                          bool (*is_empty)(const void*)) {
    GetRegistry().emplace(key, std::tuple{p, reporter, is_empty});
}

template <typename StatT>
void RegisterReporter(std::string_view key, StatT& stat) {
    RegisterReporterImpl(
        key, &stat,
        [](void* p, ReportEntity report_entity) {
            StatT& s = *static_cast<StatT*>(p);
            s.report(report_entity);
            s = StatT{};
        },
        [](const void* p) {
            const StatT& s = *static_cast<const StatT*>(p);
            return s == StatT{};
        });
}

template <std::size_t N>
struct StaticString {
    constexpr StaticString() : invalid{true} {}
    constexpr StaticString(auto&&) : invalid{true} {}
    constexpr StaticString(const char (&s)[N]) : invalid{false} {
        for (std::size_t i = 0; i < N; ++i) {
            data[i] = s[i];
        }
    }
    constexpr std::string_view view() const { return std::string_view{data, N - 1}; }
    bool invalid;
    char data[N] = {};
};

template <typename StatT, StaticString K, bool = K.invalid>
class StatStore {
public:
    StatT& value(std::string_view) { return value_; }

private:
    template <typename, StaticString>
    friend class StatAccessor;

    StatStore() { RegisterReporter<StatT>(K.view(), value_); }
    StatT value_{};
};

template <typename StatT, StaticString K>
class StatStore<StatT, K, true> {
public:
    StatT& value(std::string_view key) {
        auto it = stat_map_.find(key);
        if (it == stat_map_.end()) {
            it = stat_map_.emplace(std::piecewise_construct, std::tuple{key}, std::tuple{}).first;
            RegisterReporter<StatT>(key, it->second);
        }
        return it->second;
    }

private:
    template <typename, StaticString>
    friend class StatAccessor;

    StatStore() {}
    std::map<std::string, StatT, std::less<>> stat_map_;
};

template <typename StatT, StaticString K>
class StatAccessor {
public:
    StatT* get(std::string_view key) {
        if consteval {
            return nullptr;
        } else {
            return &store_.value(key);
        }
    }

private:
    static inline StatStore<StatT, K> store_;
};

template <typename StatT, typename TraceActionT>
class ScopedRecorder {
public:
    constexpr ScopedRecorder(StatT* stat) : stat_{stat} {
        if (!std::is_constant_evaluated() && stat_) {
            new (&tr_act_) TraceActionT{};
            tr_act_.begin();
        }
    }
    constexpr ~ScopedRecorder() {
        if (!std::is_constant_evaluated() && stat_) {
            stat_->update(tr_act_.end());
            tr_act_.~TraceActionT();
        }
    }

private:
    union {
        TraceActionT tr_act_;
    };
    StatT* stat_;
};

#define SAYHISORT_GENSYM(name) SAYHISORT_GENSYM_HELPER1(name, __LINE__)
#define SAYHISORT_GENSYM_HELPER1(name, line) SAYHISORT_GENSYM_HELPER2(name, line)
#define SAYHISORT_GENSYM_HELPER2(name, line) _sayhisort_macro_##name##_##line

/**
 * Public API
 */

void Report(std::ostream& os) {
    ReportEntity report_entity{os};
    const std::string* old_key = nullptr;
    for (const auto& [key, data] : GetRegistry()) {
        const auto& [p, reporter, is_empty] = data;
        if (!is_empty(p)) {
            if (!old_key || *old_key != key) {
                WriteReportIndent(os) << key << ":\n";
                old_key = &key;
            }
            reporter(p, report_entity);
        }
    }
    os.flush();
}

void Report(std::ostream& os, std::string_view key, bool push_indent = false) {
    ReportEntity report_entity{os};
    WriteReportIndent(os) << key << ":\n";
    auto& registry = GetRegistry();
    auto [it0, it1] = registry.equal_range(key);
    while (it0 != it1) {
        const auto& data = it0++->second;
        const auto& [p, reporter, _] = data;
        reporter(p, report_entity);
    }
    g_report_indent += push_indent;
}

void PopReportIndent() {
    --g_report_indent;
}

#define SAYHISORT_GET_STAT(stat, key)                                                                       \
    ::sayhisort::test::StatAccessor<stat,                                                                   \
                                    std::is_same_v<decltype(key), const char (&)[sizeof(key)]>&& requires { \
                                        std::type_identity_t<char[sizeof(key) + std::size_t{1}]>{key};      \
                                    } ? ::sayhisort::test::StaticString<sizeof(key) + !sizeof(key)>{key}    \
                                      : ::sayhisort::test::StaticString<sizeof(key) + !sizeof(key)>{}>{}    \
        .get(std::is_constant_evaluated() ? "" : (key))

#define SAYHISORT_SCOPED_RECORDER(stat, tr_act, key)                                                     \
    [[maybe_unused]] ::sayhisort::test::ScopedRecorder<stat, tr_act> SAYHISORT_GENSYM(scoped_recorder) { \
        SAYHISORT_GET_STAT(stat, key)                                                                    \
    }

/**
 * High-level util to trace execution time
 */

class SumTime {
public:
    void update(uint64_t ns) { sum_ += ns; }
    void report(ReportEntity report_entity) const { report_entity("elapsed_time_ms", sum_ / (1000.0 * 1000.0)); }
    friend auto operator<=>(SumTime, SumTime) = default;

private:
    uint64_t sum_ = 0;
};

class PerfTracer {
public:
    void begin() { start_ = std::chrono::steady_clock::now(); }
    uint64_t end() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_{};
};

#define SAYHISORT_PERF_TRACE(...) \
    SAYHISORT_SCOPED_RECORDER(::sayhisort::test::SumTime, ::sayhisort::test::PerfTracer, __VA_ARGS__)

}  // namespace
}  // namespace sayhisort::test

#endif  // SAYHISORT_PROFILE_UTIL_H

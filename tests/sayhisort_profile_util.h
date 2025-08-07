#ifndef SAYHISORT_PROFILE_UTIL_H
#define SAYHISORT_PROFILE_UTIL_H

// Requires C++23.
// Generalized code isn't directly related to sayhisort logic.
// Just playing to create handy micro profiling utility.

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <chrono>
#include <cstdint>

namespace sayhisort::test {

/**
 * Internal core to record and report stats
 */

void RegisterReporter(std::string_view key, void* p, void (*reporter)(std::ostream&, void*, void (*)(std::ostream&)));

template <typename StatT>
void RegisterReporter(std::string_view key, void* p) {
    RegisterReporter(key, p, [](std::ostream& os, void* p, void (*yield)(std::ostream&)) {
        StatT& stat = *static_cast<StatT*>(p);
        stat.Report(os, yield);
        stat = StatT{};
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
    constexpr std::string_view view() const { return std::string_view{data, N}; }
    bool invalid;
    char data[N] = {};
};

template <typename StatT, StaticString K, bool = K.invalid>
class StatStore {
public:
    StatT& value() { return value_; }

private:
    template <typename, StaticString, bool>
    friend class Recorder;

    StatStore() { RegisterReporter<StatT>(K.view(), &value_); }
    StatT value_;
};

template <typename StatT, StaticString K>
class StatStore<StatT, K, true> {
public:
    StatT& value(std::string_view key) {
        auto it = stat_map_.find(key);
        if (it == stat_map_.end()) {
            it = stat_map_.emplace(std::piecewise_construct, std::tuple{key}, std::tuple{}).first;
            RegisterReporter<StatT>(key, &it->second);
        }
        return it->second;
    }

private:
    template <typename, StaticString, bool>
    friend class Recorder;

    StatStore() {}
    std::map<std::string, StatT, std::less<>> stat_map_;
};

#define SAYHISORT_GENSYM(name) SAYHISORT_GENSYM_HELPER1(name, __LINE__)
#define SAYHISORT_GENSYM_HELPER1(name, line) SAYHISORT_GENSYM_HELPER2(name, line)
#define SAYHISORT_GENSYM_HELPER2(name, line) _sayhisort_macro_##name##_##line

/**
 * Public low-level API
 */

template <typename StatT, StaticString K, bool = K.invalid>
class Recorder {
public:
    template <typename ActionT>
    constexpr Recorder(ActionT&& a) {
        if !consteval {
            store_.value().update(std::forward<ActionT&&>(a));
        }
    }

private:
    static inline StatStore<StatT, K> store_;
};

template <typename StatT, StaticString K>
class Recorder<StatT, K, true> {
public:
    template <typename ActionT>
    constexpr Recorder(std::string_view key, ActionT&& a) {
        if !consteval {
            store_.value(key).update(std::forward<ActionT&&>(a));
        }
    }

private:
    static inline StatStore<StatT, K> store_;
};

template <typename StatT, typename TraceActionT, StaticString K, bool = K.invalid>
class ScopedRecorder {
public:
    constexpr ScopedRecorder(const char*, bool enabled) {
        if !consteval {
            new (&tr_act_) TraceActionT{};
            if (enabled) {
                tr_act_.begin();
            }
        }
    }
    constexpr ~ScopedRecorder() {
        if !consteval {
            if (tr_act_.active()) {
                Recorder<StatT, K>{tr_act_.end()};
            }
            tr_act_.~TraceActionT();
        }
    }

private:
    union {
        TraceActionT tr_act_;
    };
};

template <typename StatT, typename TraceActionT, StaticString K>
class ScopedRecorder<StatT, TraceActionT, K, true> {
public:
    constexpr ScopedRecorder(std::string key, bool enabled) : key_{std::move(key)} {
        if !consteval {
            new (&tr_act_) TraceActionT{};
            if (enabled) {
                tr_act_.begin();
            }
        }
    }
    constexpr ~ScopedRecorder() {
        if !consteval {
            if (tr_act_.active()) {
                Recorder<StatT, K>{key_, tr_act_.end()};
            }
            tr_act_.~TraceActionT();
        }
    }

private:
    union {
        TraceActionT tr_act_;
    };
    std::string key_;
};

void PushReport(std::string_view key, std::ostream&);
void PopReport();

void Report(std::ostream&);

#define SAYHISORT_SCOPED_RECORDER(...) SAYHISORT_SCOPED_RECORDER_HELPER(__VA_ARGS__, true)
#define SAYHISORT_SCOPED_RECORDER_HELPER(stat, tr_act, key, pred, ...)                                          \
    [[maybe_unused]] ::sayhisort::test::ScopedRecorder<                                                         \
        stat, tr_act,                                                                                           \
        std::is_same_v<decltype(key), const char (&)[sizeof(key)]>&& requires {                                 \
            std::type_identity_t<char[sizeof(key) + 1]>{};                                                      \
        } ? ::sayhisort::test::StaticString<sizeof(key)>{key} : ::sayhisort::test::StaticString<sizeof(key)>{}> \
    SAYHISORT_GENSYM(scoped_recorder) {                                                                         \
        std::is_constant_evaluated() ? "" : (key), !std::is_constant_evaluated() && (pred)                      \
    }

/**
 * High-level util to trace execution time
 */

class SumTime {
public:
    void update(uint64_t ns) { sum_ += ns; }
    void Report(std::ostream& os, void (*yield)(std::ostream&)) const;

private:
    uint64_t sum_ = 0;
};

class PerfTracer {
public:
    void begin() { start_ = std::chrono::steady_clock::now(); }
    uint64_t end() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_).count();
    }
    bool active() const { return start_ != std::chrono::steady_clock::time_point{}; }

private:
    std::chrono::steady_clock::time_point start_{};
};

#define SAYHISORT_PERF_TRACE(...) \
    SAYHISORT_SCOPED_RECORDER(::sayhisort::test::SumTime, ::sayhisort::test::PerfTracer, __VA_ARGS__)

}  // namespace sayhisort::test

#endif  // SAYHISORT_PROFILE_UTIL_H

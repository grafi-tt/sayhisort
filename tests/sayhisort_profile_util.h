#ifndef SAYHISORT_PROFILE_UTIL_H
#define SAYHISORT_PROFILE_UTIL_H

// Requires C++20.
// Generalized code isn't directly related to sayhisort logic.
// Just playing to create handy micro profiling utility.

#include <concepts>
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

/**
 * Public type interface
 */

class Reporter {
public:
    virtual ~Reporter() = default;
    virtual void write_entity(void (*name_writer)(std::ostream& os, const void* name), const void* name,
                              void (*value_writer)(std::ostream& os, const void* value), const void* value) = 0;
    virtual void push_key(std::string_view key) = 0;
    virtual void pop() = 0;
};

template <typename StatT>
concept Stat = std::equality_comparable<StatT> && requires(const StatT& s, Reporter& reporter) {
    StatT{};
    s.report(reporter);
};

template <typename ActionT, typename StatT>
concept Action = requires(const ActionT& a, StatT s) { s.update(a); };

template <typename TraceActionT, typename StatT>
concept TraceAction = requires(TraceActionT t, StatT s) {
    t.begin();
    s.update(t.end());
};

/**
 * Internal core to process polymorphic stats
 */

struct StatEntry {
    void* stat;
    bool* disabled;
    void (*report)(Reporter&, void*);
    bool (*is_empty)(const void*);
};

inline auto& GetStatRegistry() {
    static std::multimap<std::string, StatEntry, std::less<>> registry;
    return registry;
}

inline void RegisterStatImpl(std::string_view key, StatEntry entry) {
    GetStatRegistry().emplace(key, entry);
}

template <Stat StatT>
void RegisterStat(std::string_view key, std::pair<StatT, bool>& value) {
    RegisterStatImpl(key, {&value.first, &value.second,
                           [](Reporter& reporter, void* p) {
                               StatT& s = *static_cast<StatT*>(p);
                               s.report(reporter);
                               s = StatT{};
                           },
                           [](const void* p) {
                               const StatT& s = *static_cast<const StatT*>(p);
                               return s == StatT{};
                           }});
}

/**
 * Internal core to store stats with zero-overhead access for constexpr key
 */

template <std::size_t N>
struct StaticString {
    static inline constexpr std::size_t size = N == 0 ? 1 : N;
    constexpr StaticString(auto&&) {}
    constexpr StaticString(const char (&s)[size]) {
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = s[i];
        }
    }
    constexpr static bool invalid() { return N == 0; }
    constexpr std::string_view view() const { return std::string_view{data, size - 1}; }
    char data[size] = {};
};

StaticString(auto&&) -> StaticString<0>;

template <std::size_t N>
StaticString(const char (&s)[N]) -> StaticString<N>;

template <typename StatT, StaticString K, bool = K.invalid()>
class StatStore {
    // zero-overhead impl
public:
    std::pair<StatT, bool>& value(std::string_view) { return value_; }

private:
    template <typename, StaticString>
    friend class StatAccessor;

    StatStore() { RegisterStat<StatT>(K.view(), value_); }
    std::pair<StatT, bool> value_{};
};

template <typename StatT, StaticString K>
class StatStore<StatT, K, true> {
    // dynamic key impl
public:
    std::pair<StatT, bool>& value(std::string_view key) {
        auto it = stat_map_.find(key);
        if (it == stat_map_.end()) {
            it = stat_map_.emplace(std::piecewise_construct, std::tuple{key}, std::tuple{}).first;
            RegisterStat<StatT>(key, it->second);
        }
        return it->second;
    }

private:
    template <typename, StaticString>
    friend class StatAccessor;

    StatStore() {}
    std::map<std::string, std::pair<StatT, bool>, std::less<>> stat_map_;
};

template <typename StatT, StaticString K>
class StatAccessor {
public:
    constexpr StatT* get(std::string_view key) {
        if (std::is_constant_evaluated()) {
            return nullptr;
        } else {
            auto& [stat, disabled] = store_.value(key);
            if (disabled) {
                return nullptr;
            } else {
                return &stat;
            }
        }
    }

private:
    static inline StatStore<StatT, K> store_;
};

/*
 * Internal low-level API
 */

#define SAYHISORT_CONCAT(a, b) SAYHISORT_CONCAT_HELPER(a, b)
#define SAYHISORT_CONCAT_HELPER(a, b) a##b

#define SAYHISORT_GENSYM(name) SAYHISORT_CONCAT(_sayhisort_macro_##name##_, __LINE__)

#define SAYHISORT_GET_STAT(key, StatT)                                                  \
    ::sayhisort::test::StatAccessor<StatT, ::sayhisort::test::StaticString{key}>{}.get( \
        std::is_constant_evaluated() ? std::string_view{} : std::string_view{key})

template <Stat S, Action<S> A>
constexpr void Record(S* stat, const A& act) {
    if (stat) {
        stat->update(act);
    }
}

template <Stat S, TraceAction<S> T>
class ScopedRecorder {
public:
    constexpr ScopedRecorder(S* stat) : stat_{stat} {
        if (stat_) {
            new (&tr_act_) T{};
            tr_act_.begin();
        }
    }
    constexpr ~ScopedRecorder() {
        if (stat_) {
            stat_->update(tr_act_.end());
            tr_act_.~T();
        }
    }

private:
    union {
        T tr_act_;
    };
    S* stat_;
};

/**
 * Public API
 */

#define SAYHISORT_RECORD(key, StatT, act) Record(SAYHISORT_GET_STAT(key, StatT), act)

#define SAYHISORT_SCOPED_RECORDER(key, StatT, TraceActionT)                                                     \
    [[maybe_unused]] ::sayhisort::test::ScopedRecorder<StatT, TraceActionT> SAYHISORT_GENSYM(scoped_recorder) { \
        SAYHISORT_GET_STAT(key, StatT)                                                                          \
    }

inline void EnableRecords(bool enabled = true) {
    for (auto& kv : GetStatRegistry()) {
        auto& entry = kv.second;
        *entry.disabled = !enabled;
    }
}

inline void EnableRecords(std::string_view key, bool enabled = true) {
    auto& registry = GetStatRegistry();
    auto [it0, it1] = registry.equal_range(key);
    while (it0 != it1) {
        auto& entry = it0++->second;
        *entry.disabled = !enabled;
    }
}

inline void EnableRecords(const char* key, bool enabled = true) {
    // Add overload to prevent const char* pointer casted to bool (enabled flag.)
    EnableRecords(std::string_view{key}, enabled);
}

inline void DisableRecords() {
    EnableRecords(false);
}

inline void DisableRecords(std::string_view key) {
    EnableRecords(key, false);
}

inline void Report(Reporter& reporter) {
    const std::string* old_key = nullptr;
    for (const auto& [key, entry] : GetStatRegistry()) {
        if (!entry.is_empty(entry.stat)) {
            if (!old_key || *old_key != key) {
                if (old_key) {
                    reporter.pop();
                }
                reporter.push_key(key);
                old_key = &key;
            }
            entry.report(reporter, entry.stat);
        }
    }
    if (old_key) {
        reporter.pop();
    }
}

inline void Report(Reporter& reporter, std::string_view key, bool push = false) {
    reporter.push_key(key);
    auto& registry = GetStatRegistry();
    auto [it0, it1] = registry.equal_range(key);
    while (it0 != it1) {
        auto& entry = it0++->second;
        entry.report(reporter, entry.stat);
    }
    if (!push) {
        reporter.pop();
    }
}

/**
 * High-level util to trace execution time
 */

class YamlReporter : public Reporter {
public:
    YamlReporter(std::ostream& os) : os_{os} {}

    void write_entity(void (*name_writer)(std::ostream& os, const void* name), const void* name,
                      void (*value_writer)(std::ostream& os, const void* value), const void* value) override {
        for (size_t i = 0; i < indent_; ++i) {
            os_ << "  ";
        }
        name_writer(os_, name);
        os_ << ": ";
        value_writer(os_, value);
        os_ << "\n";
    }

    void push_key(std::string_view key) override {
        for (size_t i = 0; i < indent_; ++i) {
            os_ << "  ";
        }
        os_ << key << ":\n";
        ++indent_;
    }

    void pop() { --indent_; }

private:
    std::ostream& os_;
    std::size_t indent_ = 0;
};

class SumTime {
public:
    void update(uint64_t ns) { sum_ += ns; }
    void report(Reporter& reporter) const {
        double value = sum_ / (1000.0 * 1000.0);
        reporter.write_entity(
            [](std::ostream& os, const void* name) { os << static_cast<const char*>(name); }, "elapsed_time_ms",
            [](std::ostream& os, const void* value) { os << *static_cast<const double*>(value); }, &value);
    }
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

#define SAYHISORT_PERF_TRACE(key) \
    SAYHISORT_SCOPED_RECORDER(key, ::sayhisort::test::SumTime, ::sayhisort::test::PerfTracer)

}  // namespace sayhisort::test

#endif  // SAYHISORT_PROFILE_UTIL_H

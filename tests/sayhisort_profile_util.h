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
 * Internal core to report stats
 */

inline std::size_t g_report_indent = 0;

inline std::ostream& WriteReportIndent(std::ostream& os, std::size_t offset = 0) {
    for (std::size_t i = 0; i < g_report_indent + offset; ++i) {
        os << "  ";
    }
    return os;
}

class EntityWriter {
public:
    EntityWriter(std::ostream& os) : os_{os} {}
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

/**
 * Internal core to process polymorphic stats
 */

struct StatEntry {
    void* stat;
    bool* disabled;
    void (*reporter)(void*, EntityWriter);
    bool (*is_empty)(const void*);
};

inline auto& GetStatRegistry() {
    static std::multimap<std::string, StatEntry, std::less<>> registry;
    return registry;
}

inline void RegisterStatImpl(std::string_view key, StatEntry entry) {
    GetStatRegistry().emplace(key, entry);
}

template <typename StatT>
void RegisterStat(std::string_view key, std::pair<StatT, bool>& value) {
    RegisterStatImpl(key, {&value.first, &value.second,
                           [](void* p, EntityWriter write_entity) {
                               StatT& s = *static_cast<StatT*>(p);
                               s.report(write_entity);
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
    constexpr StaticString() : invalid{true} {}
    constexpr StaticString(auto&&) : invalid{true} {}
    constexpr StaticString(const char (&s)[size]) : invalid{false} {
        for (std::size_t i = 0; i < size; ++i) {
            data[i] = s[i];
        }
    }
    constexpr std::string_view view() const { return std::string_view{data, size - 1}; }
    bool invalid;
    char data[size] = {};
};

template <typename StatT, StaticString K, bool = K.invalid>
class StatStore {
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
        if consteval {
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
 * Helper macro definitions
 */

#define SAYHISORT_GET_STAT(key, StatT)                                                                      \
    ::sayhisort::test::StatAccessor<StatT,                                                                  \
                                    std::is_same_v<decltype(key), const char (&)[sizeof(key)]>&& requires { \
                                        std::type_identity_t<char[sizeof(key) + std::size_t{1}]>{key};      \
                                    } ? ::sayhisort::test::StaticString<sizeof(key)>{key}                   \
                                      : ::sayhisort::test::StaticString<sizeof(key)>{}>{}                   \
        .get(std::is_constant_evaluated() ? "" : (key))

#define SAYHISORT_GENSYM(name) SAYHISORT_GENSYM_HELPER1(name, __LINE__)
#define SAYHISORT_GENSYM_HELPER1(name, line) SAYHISORT_GENSYM_HELPER2(name, line)
#define SAYHISORT_GENSYM_HELPER2(name, line) _sayhisort_macro_##name##_##line

#define SAYHISORT_CALL_CONCAT(name1, name2, ...) SAYHISORT_CALL_CONCAT_HELPER1(name1, name2, __VA_ARGS__)
#define SAYHISORT_CALL_CONCAT_HELPER1(name1, name2, ...) SAYHISORT_CALL_CONCAT_HELPER2(name1, name2, __VA_ARGS__)
#define SAYHISORT_CALL_CONCAT_HELPER2(name1, name2, ...) SAYHISORT_CALL_CONCAT_HELPER3(name1##name2(__VA_ARGS__))
#define SAYHISORT_CALL_CONCAT_HELPER3(call) call

/*
 * Helper for RAII style tracing
 */

template <typename StatT, typename TraceActionT>
class ScopedRecorder {
public:
    constexpr ScopedRecorder(StatT* stat) : stat_{stat} {
        if (stat_) {
            new (&tr_act_) TraceActionT{};
            tr_act_.begin();
        }
    }
    constexpr ~ScopedRecorder() {
        if (stat_) {
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

/**
 * Public API
 */

// To disable profile:
//   #define SAYHISORT_DISABLE_PROFILE 1
// To re-enable profile:
//   #undef SAYHISORT_DISABLE_PROFILE
// When 0 is set, profile is still enabled:
//   #define SAYHISORT_DISABLE_PROFILE 0

#define SAYHISORT_RECORD(...) SAYHISORT_CALL_CONCAT(SAYHISORT_RECORD_, SAYHISORT_DISABLE_PROFILE, __VA_ARGS__)
#define SAYHISORT_RECORD_0(key, StatT, action)                                                        \
    for (StatT * SAYHISORT_GENSYM(record) = SAYHISORT_GET_STAT(key, StatT); SAYHISORT_GENSYM(record); \
         SAYHISORT_GENSYM(record) = nullptr)                                                          \
    SAYHISORT_GENSYM(record)->update(action)
#define SAYHISORT_RECORD_1(...)
#define SAYHISORT_RECORD_SAYHISORT_DISABLE_PROFILE(...) SAYHISORT_RECORD_0(__VA_ARGS__)

#define SAYHISORT_SCOPED_RECORDER(...) \
    SAYHISORT_CALL_CONCAT(SAYHISORT_SCOPED_RECORDER_, SAYHISORT_DISABLE_PROFILE, __VA_ARGS__)
#define SAYHISORT_SCOPED_RECORDER_0(key, StatT, TraceActionT)                                                   \
    [[maybe_unused]] ::sayhisort::test::ScopedRecorder<StatT, TraceActionT> SAYHISORT_GENSYM(scoped_recorder) { \
        SAYHISORT_GET_STAT(key, StatT)                                                                          \
    }
#define SAYHISORT_SCOPED_RECORDER_1(...)
#define SAYHISORT_SCOPED_RECORDER_SAYHISORT_DISABLE_PROFILE(...) SAYHISORT_SCOPED_RECORDER_0(__VA_ARGS__)

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

inline void Report(std::ostream& os) {
    EntityWriter write_entity{os};
    const std::string* old_key = nullptr;
    for (const auto& [key, entry] : GetStatRegistry()) {
        if (!entry.is_empty(entry.stat)) {
            if (!old_key || *old_key != key) {
                WriteReportIndent(os) << key << ":\n";
                old_key = &key;
            }
            entry.reporter(entry.stat, write_entity);
        }
    }
    os.flush();
}

inline void Report(std::ostream& os, std::string_view key, bool push_indent = false) {
    EntityWriter write_entity{os};
    WriteReportIndent(os) << key << ":\n";
    auto& registry = GetStatRegistry();
    auto [it0, it1] = registry.equal_range(key);
    while (it0 != it1) {
        auto& entry = it0++->second;
        entry.reporter(entry.stat, write_entity);
    }
    g_report_indent += push_indent;
}

inline void PopReportIndent() {
    --g_report_indent;
}

/**
 * High-level util to trace execution time
 */

class SumTime {
public:
    void update(uint64_t ns) { sum_ += ns; }
    void report(EntityWriter write_entity) const { write_entity("elapsed_time_ms", sum_ / (1000.0 * 1000.0)); }
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

}  // namespace
}  // namespace sayhisort::test

#endif  // SAYHISORT_PROFILE_UTIL_H

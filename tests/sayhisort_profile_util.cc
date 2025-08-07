#include "sayhisort_profile_util.h"

#include <ostream>

namespace sayhisort::test {
namespace {

auto& GetRegistry() {
    static std::multimap<
        std::string, std::tuple<void*, void (*)(std::ostream&, void*, void (*)(std::ostream&)), bool (*)(const void*)>,
        std::less<>>
        registry;
    return registry;
}

std::size_t g_indent = 0;

std::ostream& PutIndent(std::ostream& os, std::size_t offset = 0) {
    for (std::size_t i = 0; i < g_indent + offset; ++i) {
        os << "  ";
    }
    return os;
}

void YieldReport(std::ostream& os) {
    static int state = 0;
    switch (state++) {
        case 0:
            PutIndent(os, 1);
            break;
        case 1:
            os << ": ";
            break;
        default:
            os << "\n";
            state = 0;
            break;
    }
}

}  // namespace

void RegisterReporterImpl(std::string_view key, void* p,
                          void (*reporter)(std::ostream&, void*, void (*)(std::ostream&)),
                          bool (*is_empty)(const void*)) {
    GetRegistry().emplace(key, std::tuple{p, reporter, is_empty});
}

void Report(std::ostream& os) {
    const std::string* old_key = nullptr;
    for (const auto& [key, data] : GetRegistry()) {
        const auto& [p, reporter, is_empty] = data;
        if ((!old_key || *old_key != key) && !is_empty(p)) {
            PutIndent(os) << key << ":\n";
            old_key = &key;
        }
        reporter(os, p, YieldReport);
    }
    os.flush();
}

void Report(std::ostream& os, std::string_view key, bool push_indent) {
    PutIndent(os) << key << ":\n";
    auto& registry = GetRegistry();
    auto [it0, it1] = registry.equal_range(key);
    while (it0 != it1) {
        const auto& data = it0++->second;
        const auto& [p, reporter, _] = data;
        reporter(os, p, YieldReport);
    }
    g_indent += push_indent;
}

void PopReportIndent() {
    --g_indent;
}

void SumTime::Report(std::ostream& os, void (*yield)(std::ostream&)) const {
    if (sum_ != 0) {
        yield(os);
        yield(os << "elapsed_time_ms");
        yield(os << sum_ / (1000.0 * 1000.0));
    }
}

}  // namespace sayhisort::test

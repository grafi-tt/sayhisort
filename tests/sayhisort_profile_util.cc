#include "sayhisort_profile_util.h"

#include <ostream>

namespace sayhisort::test {
namespace {

auto& GetRegistry() {
    static std::multimap<std::string, std::tuple<void*, void (*)(std::ostream&, void*, void (*)(std::ostream&))>>
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

void RegisterReporter(std::string_view key, void* p, void (*reporter)(std::ostream&, void*, void (*)(std::ostream&))) {
    GetRegistry().emplace(key, std::tuple{p, reporter});
}

void PushReport(std::string_view key, std::ostream& os) {
    PutIndent(os) << key << ":\n";
    ++g_indent;
}

void PopReport() {
    --g_indent;
}

void Report(std::ostream& os) {
    for (const auto& [key, data] : GetRegistry()) {
        const auto& [p, reporter] = data;
        PutIndent(os) << key << ":\n";
        reporter(os, p, YieldReport);
    }
    os.flush();
}

void SumTime::Report(std::ostream& os, void (*yield)(std::ostream&)) const {
    if (sum_ != 0) {
        yield(os);
        yield(os << "elapsed_time_ms");
        yield(os << sum_ / (1000.0 * 1000.0));
    }
}

}  // namespace sayhisort::test

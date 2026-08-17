#pragma once

// Minimal, dependency-free test micro-harness (see AGENTS.md: no new
// third-party test framework for this block's small, high-value test
// set — see docs/DECISION_LOG.md).

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace nimvlets::testing {

struct TestCase {
    std::string name;
    std::function<bool()> run;
};

class TestRunner {
public:
    void Add(std::string name, std::function<bool()> fn) {
        cases_.push_back(TestCase{std::move(name), std::move(fn)});
    }

    // Returns the number of failed tests (0 == success, suitable as a
    // process exit code for CTest).
    int RunAll() const {
        int failures = 0;
        for (const auto& tc : cases_) {
            const bool ok = tc.run();
            std::cout << (ok ? "[PASS] " : "[FAIL] ") << tc.name << "\n";
            if (!ok) {
                ++failures;
            }
        }
        std::cout << (static_cast<int>(cases_.size()) - failures) << "/" << cases_.size()
                  << " tests passed\n";
        return failures;
    }

private:
    std::vector<TestCase> cases_;
};

}  // namespace nimvlets::testing

#define NIMVLETS_CHECK(cond)                                                                 \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "  CHECK failed: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

#include <JuceHeader.h>

// =============================================================================
// Minimal console UnitTest runner for devpiano_tests.
//
// When built with `cmake -DBUILD_TESTS=ON` and linked against production
// source files, this executable discovers all JUCE UnitTest instances
// registered via static global constructors and runs them.
//
// By default runs only devpiano's own tests (categories "DevPiano/<area>")
// and skips JUCE's internal "Files" category (known WSL root-user
// incompatibility with POSIX access(W_OK)). JUCE's own internal tests add
// ~95s (e.g. AudioProcessorGraph's large render sequence) and are opt-in via
// --include-juce; override the Files skip with --include-files.
//
// Returns EXIT_FAILURE if any test fails, EXIT_SUCCESS otherwise.
// =============================================================================

class ConsoleTestRunner final : public juce::UnitTestRunner {
public:
    ConsoleTestRunner() {
        setPassesAreLogged(true);
        setAssertOnFailure(false);
    }

    void logMessage(const juce::String& message) override {
        juce::Logger::writeToLog(message);
        std::cout << message << std::endl;
    }

    int computeTotalPasses() const noexcept {
        int total = 0;
        for (int i = 0; i < getNumResults(); ++i) {
            if (const auto* result = getResult(i)) {
                total += result->passes;
            }
        }
        return total;
    }

    int computeTotalFailures() const noexcept {
        int total = 0;
        for (int i = 0; i < getNumResults(); ++i) {
            if (const auto* result = getResult(i)) {
                total += result->failures;
            }
        }
        return total;
    }
};

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    juce::ConsoleApplication app;

    bool includeFiles = false;
    bool includeJuce = false;
    juce::String categoryFilter;
    juce::String nameFilter;
    juce::Array<juce::String> skipCategories = { "Files" };

    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        if (arg == "--verbose" || arg == "-v") {
            // verbose mode enabled (logs already print to stdout by default)
        } else if (arg == "--include-files") {
            includeFiles = true;
        } else if (arg == "--include-juce") {
            includeJuce = true;
        } else if (arg == "--skip-category" && i + 1 < argc) {
            skipCategories.add(juce::String(argv[++i]));
        } else if (arg == "--category" && i + 1 < argc) {
            categoryFilter = juce::String(argv[++i]);
        } else if (arg == "--name" && i + 1 < argc) {
            nameFilter = juce::String(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: devpiano_tests [options]\n"
                      << "  --verbose, -v           Verbose output\n"
                      << "  --category <name>       Run only tests in the given category\n"
                      << "  --name <name>           Run only tests with the given name\n"
                      << "  --skip-category <name>  Skip tests in the given category\n"
                      << "  --include-files         Don't skip JUCE Files category\n"
                      << "  --include-juce          Also run JUCE's own internal tests\n"
                      << "                          (default: project tests only, fast)\n"
                      << "  --help, -h              Show this help\n"
                      << "\n"
                      << "  --category and --name are mutually exclusive; passing both is an error.\n"
                      << "  A filter matching no tests is an error (no silent pass).\n";
            return 0;
        }
    }

    // TEST-020：互斥过滤参数同时给出 → 显式报错，而非 category 静默优先。
    if (categoryFilter.isNotEmpty() && nameFilter.isNotEmpty()) {
        std::cout << "Error: --category and --name are mutually exclusive; pass only one.\n";
        return EXIT_FAILURE;
    }

    if (includeFiles) {
        skipCategories.removeAllInstancesOf("Files");
    }

    ConsoleTestRunner runner;

    auto allTests = juce::UnitTest::getAllTests();

    if (allTests.isEmpty()) {
        std::cout << "No tests registered." << '\n';
        return 0;
    }

    // Build the filtered test list
    juce::Array<juce::UnitTest*> testsToRun;

    if (categoryFilter.isNotEmpty()) {
        testsToRun = juce::UnitTest::getTestsInCategory(categoryFilter);
    } else if (nameFilter.isNotEmpty()) {
        testsToRun = juce::UnitTest::getTestsWithName(nameFilter);
    } else if (includeJuce) {
        // Full suite including JUCE's own internal tests (slow: ~100s).
        for (auto* t : allTests) {
            if (!skipCategories.contains(t->getCategory())) {
                testsToRun.add(t);
            }
        }
    } else {
        // Default: project tests only (fast). Categories follow the
        // "DevPiano/<area>" scheme; "Files" stays skippable by default
        // (WSL root POSIX access(W_OK) quirk).
        const juce::StringArray projectCategories
            = { "DevPiano/Core", "DevPiano/Recording", "DevPiano/Engine", "DevPiano/UI", "Files" };
        for (auto* t : allTests) {
            if (projectCategories.contains(t->getCategory()) && !skipCategories.contains(t->getCategory())) {
                testsToRun.add(t);
            }
        }
    }

    if (testsToRun.isEmpty()) {
        std::cout << "No tests matched after filtering." << '\n';
        return EXIT_FAILURE; // a typo'd filter must not silently pass CI
    }

    std::cout << "Running " << testsToRun.size() << " test(s)...\n" << '\n';
    try {
        runner.runTests(testsToRun);
    } catch (const std::exception& e) {
        std::cerr << "Fatal Exception in unit test runner: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Fatal Unknown Exception in unit test runner." << std::endl;
        return EXIT_FAILURE;
    }
    const auto numPasses = runner.computeTotalPasses();
    const auto numFailures = runner.computeTotalFailures();

    std::cout << "\n=== Results ===\n"
              << "Passed: " << numPasses << "\n"
              << "Failed: " << numFailures << std::endl;
    return (numFailures > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

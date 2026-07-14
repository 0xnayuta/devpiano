#include <JuceHeader.h>

// =============================================================================
// Minimal console UnitTest runner for devpiano_tests.
//
// When built with `cmake -DBUILD_TESTS=ON` and linked against production
// source files, this executable discovers all JUCE UnitTest instances
// registered via static global constructors and runs them.
//
// By default skips JUCE's internal "Files" category (known WSL root-user
// incompatibility with POSIX access(W_OK)). Override with --include-files.
//
// Returns EXIT_FAILURE if any test fails, EXIT_SUCCESS otherwise.
// =============================================================================

class ConsoleTestRunner final : public juce::UnitTestRunner {
public:
    explicit ConsoleTestRunner(bool verbose = false)
        : verboseMode(verbose) {
    }

    void logMessage(const juce::String& message) override {
        juce::Logger::writeToLog(message);

        if (verboseMode)
            std::cout << message << std::endl;
    }

    int computeTotalPasses() const noexcept {
        int total = 0;
        for (int i = 0; i < getNumResults(); ++i)
            if (const auto* result = getResult(i))
                total += result->passes;
        return total;
    }

    int computeTotalFailures() const noexcept {
        int total = 0;
        for (int i = 0; i < getNumResults(); ++i)
            if (const auto* result = getResult(i))
                total += result->failures;
        return total;
    }

private:
    bool verboseMode = false;
};

int main(int argc, char** argv) {
    juce::ConsoleApplication app;

    bool verbose = false;
    bool includeFiles = false;
    juce::String categoryFilter;
    juce::String nameFilter;
    juce::Array<juce::String> skipCategories = { "Files" };

    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);

        if (arg == "--verbose" || arg == "-v")
            verbose = true;
        else if (arg == "--include-files")
            includeFiles = true;
        else if (arg == "--skip-category" && i + 1 < argc)
            skipCategories.add(juce::String(argv[++i]));
        else if (arg == "--category" && i + 1 < argc)
            categoryFilter = juce::String(argv[++i]);
        else if (arg == "--name" && i + 1 < argc)
            nameFilter = juce::String(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: devpiano_tests [options]\n"
                      << "  --verbose, -v           Verbose output\n"
                      << "  --category <name>       Run only tests in the given category\n"
                      << "  --name <name>           Run only tests with the given name\n"
                      << "  --skip-category <name>  Skip tests in the given category\n"
                      << "  --include-files         Don't skip JUCE Files category\n"
                      << "  --help, -h              Show this help\n";
            return 0;
        }
    }

    if (includeFiles)
        skipCategories.removeAllInstancesOf("Files");

    ConsoleTestRunner runner(verbose);

    auto allTests = juce::UnitTest::getAllTests();

    if (allTests.isEmpty()) {
        std::cout << "No tests registered." << std::endl;
        return 0;
    }

    // Build the filtered test list
    juce::Array<juce::UnitTest*> testsToRun;

    if (categoryFilter.isNotEmpty()) {
        testsToRun = juce::UnitTest::getTestsInCategory(categoryFilter);
    } else if (nameFilter.isNotEmpty()) {
        testsToRun = juce::UnitTest::getTestsWithName(nameFilter);
    } else {
        for (auto* t : allTests)
            if (!skipCategories.contains(t->getCategory()))
                testsToRun.add(t);
    }

    if (testsToRun.isEmpty()) {
        std::cout << "No tests matched after filtering." << std::endl;
        return 0;
    }

    runner.runTests(testsToRun);

    const auto numPasses = runner.computeTotalPasses();
    const auto numFailures = runner.computeTotalFailures();

    std::cout << "\n=== Results ===\n"
              << "Passed: " << numPasses << "\n"
              << "Failed: " << numFailures << "\n";

    return (numFailures > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

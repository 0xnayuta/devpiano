#include <JuceHeader.h>

// =============================================================================
// Minimal console UnitTest runner for devpiano_tests.
//
// When built with `cmake -DBUILD_TESTS=ON` and linked against production
// source files, this executable discovers all JUCE UnitTest instances
// registered via static global constructors and runs them.
//
// Returns EXIT_FAILURE if any test fails, EXIT_SUCCESS otherwise.
// =============================================================================

class ConsoleTestRunner final : public juce::UnitTestRunner
{
public:
    explicit ConsoleTestRunner (bool verbose = false) : verboseMode (verbose) {}

    void logMessage (const juce::String& message) override
    {
        juce::Logger::writeToLog (message);

        if (verboseMode)
            std::cout << message << std::endl;
    }

    int computeTotalPasses() const noexcept
    {
        int total = 0;
        for (int i = 0; i < getNumResults(); ++i)
            if (const auto* result = getResult (i))
                total += result->passes;
        return total;
    }

    int computeTotalFailures() const noexcept
    {
        int total = 0;
        for (int i = 0; i < getNumResults(); ++i)
            if (const auto* result = getResult (i))
                total += result->failures;
        return total;
    }

private:
    bool verboseMode = false;
};

int main (int argc, char** argv)
{
    juce::ConsoleApplication app;

    bool verbose = false;
    juce::String categoryFilter;
    juce::String nameFilter;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--verbose" || arg == "-v")
            verbose = true;
        else if (arg == "--category" && i + 1 < argc)
            categoryFilter = juce::String (argv[++i]);
        else if (arg == "--name" && i + 1 < argc)
            nameFilter = juce::String (argv[++i]);
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: devpiano_tests [options]\n"
                      << "  --verbose, -v         Verbose output\n"
                      << "  --category <name>     Run only tests in the given category\n"
                      << "  --name <name>         Run only tests with the given name\n"
                      << "  --help, -h            Show this help\n";
            return 0;
        }
    }

    ConsoleTestRunner runner (verbose);

    auto allTests = juce::UnitTest::getAllTests();

    if (allTests.isEmpty())
    {
        std::cout << "No tests registered." << std::endl;
        return 0;
    }

    if (categoryFilter.isNotEmpty())
    {
        auto filtered = juce::UnitTest::getTestsInCategory (categoryFilter);
        runner.runTests (filtered);
    }
    else if (nameFilter.isNotEmpty())
    {
        auto filtered = juce::UnitTest::getTestsWithName (nameFilter);
        runner.runTests (filtered);
    }
    else
    {
        runner.runAllTests();
    }

    const auto numPasses   = runner.computeTotalPasses();
    const auto numFailures = runner.computeTotalFailures();

    std::cout << "\n=== Results ===\n"
              << "Passed: " << numPasses << "\n"
              << "Failed: " << numFailures << "\n";

    return (numFailures > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

#include <JuceHeader.h>
#include <iostream>
#include <optional>

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

namespace {

struct TestRunStats {
    int passes = 0;
    int failures = 0;
};

class ConsoleTestRunner final : public juce::UnitTestRunner {
public:
    explicit ConsoleTestRunner(bool verbose = false) {
        setPassesAreLogged(verbose);
        setAssertOnFailure(false);
    }

    void logMessage(const juce::String& message) override {
        juce::Logger::writeToLog(message);
        std::cout << message << '\n';
    }

    [[nodiscard]] TestRunStats computeStats() const noexcept {
        TestRunStats stats;
        for (int i = 0; i < getNumResults(); ++i) {
            if (const auto* result = getResult(i)) {
                stats.passes += result->passes;
                stats.failures += result->failures;
            }
        }
        return stats;
    }
};

struct CommandLineOptions {
    CommandLineOptions() = default;
    CommandLineOptions(const CommandLineOptions&) = default;
    CommandLineOptions& operator=(const CommandLineOptions&) = default;

    bool verbose = false;
    bool includeFiles = false;
    bool includeJuce = false;
    bool showHelp = false;
    juce::String categoryFilter;
    juce::String nameFilter;
    juce::Array<juce::String> skipCategories = { "Files" };

    static std::optional<CommandLineOptions> parse(int argc, char** argv, juce::String& errorMessage) {
        CommandLineOptions options;

        for (int i = 1; i < argc; ++i) {
            const juce::String arg(argv[i]);
            if (arg == "--verbose" || arg == "-v") {
                options.verbose = true;
            } else if (arg == "--include-files") {
                options.includeFiles = true;
                options.includeJuce = true;
            } else if (arg == "--include-juce") {
                options.includeJuce = true;
            } else if (arg == "--skip-category" && i + 1 < argc) {
                options.skipCategories.add(juce::String(argv[++i]));
            } else if (arg == "--category" && i + 1 < argc) {
                options.categoryFilter = juce::String(argv[++i]);
            } else if (arg == "--name" && i + 1 < argc) {
                options.nameFilter = juce::String(argv[++i]);
            } else if (arg == "--help" || arg == "-h") {
                options.showHelp = true;
                return options;
            }
        }

        // TEST-020: Mutually exclusive filter flags must report error explicitly.
        if (options.categoryFilter.isNotEmpty() && options.nameFilter.isNotEmpty()) {
            errorMessage = "Error: --category and --name are mutually exclusive; pass only one.";
            return std::nullopt;
        }

        if (options.includeFiles) {
            options.skipCategories.removeAllInstancesOf("Files");
        }

        return options;
    }
};

void printUsage() {
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
}

[[nodiscard]] juce::Array<juce::UnitTest*> filterTestsToRun(const CommandLineOptions& options,
                                                            const juce::Array<juce::UnitTest*>& allTests) {
    if (options.categoryFilter.isNotEmpty()) {
        return juce::UnitTest::getTestsInCategory(options.categoryFilter);
    }
    if (options.nameFilter.isNotEmpty()) {
        return juce::UnitTest::getTestsWithName(options.nameFilter);
    }

    juce::Array<juce::UnitTest*> testsToRun;
    if (options.includeJuce) {
        for (auto* t : allTests) {
            if (!options.skipCategories.contains(t->getCategory())) {
                testsToRun.add(t);
            }
        }
    } else {
        for (auto* t : allTests) {
            const auto cat = t->getCategory();
            if ((cat.startsWith("DevPiano/") || cat == "Files") && !options.skipCategories.contains(cat)) {
                testsToRun.add(t);
            }
        }
    }
    return testsToRun;
}

} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    juce::ConsoleApplication app;

    juce::String parseError;
    const auto options = CommandLineOptions::parse(argc, argv, parseError);
    if (!options.has_value()) {
        std::cout << parseError << '\n';
        return EXIT_FAILURE;
    }
    if (options->showHelp) {
        printUsage();
        return EXIT_SUCCESS;
    }

    const auto allTests = juce::UnitTest::getAllTests();
    if (allTests.isEmpty()) {
        std::cout << "No tests registered.\n";
        return EXIT_SUCCESS;
    }

    const auto testsToRun = filterTestsToRun(*options, allTests);
    if (testsToRun.isEmpty()) {
        std::cout << "No tests matched after filtering.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Running " << testsToRun.size() << " test(s)...\n\n";

    ConsoleTestRunner runner(options->verbose);
    try {
        runner.runTests(testsToRun);
    } catch (const std::exception& e) {
        std::cerr << "Fatal Exception in unit test runner: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Fatal Unknown Exception in unit test runner.\n";
        return EXIT_FAILURE;
    }

    const auto stats = runner.computeStats();
    std::cout << "\n=== Results ===\n"
              << "Passed: " << stats.passes << "\n"
              << "Failed: " << stats.failures << '\n';

    return (stats.failures > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

#include "cli_args.h"
#include "tau5logger.h"
#include <QString>
#include <QStringList>
#include <QList>

using namespace Tau5CLI;

// Test context for collecting multiple failures
struct TestContext {
    bool passed = true;
    QStringList failures;

    void fail(const QString& message) {
        passed = false;
        failures.append(message);
    }
};

// Test result tracking
struct TestResult {
    QString testName;
    bool passed;
    QString message;
};

static QList<TestResult> testResults;

// Helper macro for tests - collects failures but continues testing
#define TEST_ASSERT(ctx, condition, message) \
    if (!(condition)) { \
        (ctx).fail(QString("Assertion failed: %1").arg(message)); \
    }

// Macro for tests that should stop on first failure
#define TEST_REQUIRE(ctx, condition, message) \
    if (!(condition)) { \
        (ctx).fail(QString("Required condition failed: %1").arg(message)); \
        return (ctx).passed; \
    }

#define RUN_TEST(testFunc) \
    { \
        QString testName = #testFunc; \
        TestContext ctx; \
        testFunc(ctx); \
        QString message = ctx.passed ? "Passed" : ctx.failures.join("; "); \
        testResults.append({testName, ctx.passed, message}); \
        if (ctx.passed) { \
            Tau5Logger::instance().info(QString("  ✓ %1").arg(testName)); \
        } else { \
            Tau5Logger::instance().error(QString("  ✗ %1: %2").arg(testName).arg(message)); \
        } \
    }

// Test helper to simulate command line arguments
struct ArgSimulator {
    std::vector<char*> args;
    std::vector<std::string> storage;
    bool finalized = false;

    void add(const char* arg) {
        storage.push_back(arg);
        finalized = false;
    }

    void finalize() {
        args.clear();
        for (auto& str : storage) {
            args.push_back(const_cast<char*>(str.c_str()));
        }
        args.push_back(nullptr); // Real argv is null-terminated
        finalized = true;
    }

    int argc() {
        if (!finalized) finalize();
        return args.size() - 1; // Don't count the null terminator
    }

    char** argv() {
        if (!finalized) finalize();
        return args.data();
    }

    void clear() {
        args.clear();
        storage.clear();
        finalized = false;
    }
};

// Tests for basic argument parsing
bool testHelpFlag(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--help");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.showHelp == true, "--help should set showHelp");
    TEST_ASSERT(ctx, args.hasError == false, "No error expected");
    return ctx.passed;
}

bool testVersionFlag(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--version");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.showVersion == true, "--version should set showVersion");
    TEST_ASSERT(ctx, args.hasError == false, "No error expected");
    return ctx.passed;
}

bool testDevtoolsFlag(TestContext& ctx) {
#ifdef TAU5_RELEASE_BUILD
    // In release builds, --devtools parsing succeeds but environment enforcement overrides it
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--devtools");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    // Parsing sets the flags initially
    TEST_ASSERT(ctx, args.devtools == true, "--devtools flag is parsed");

    // But in release mode, environment gets overridden
    applyEnvironmentVariables(args, "gui");
    TEST_ASSERT(ctx, qgetenv("MIX_ENV") == "prod", "MIX_ENV should be prod in release");
    TEST_ASSERT(ctx, qgetenv("TAU5_ELIXIR_REPL_ENABLED") != "true", "REPL should be disabled in release");
    TEST_ASSERT(ctx, qgetenv("TAU5_TIDEWAVE_ENABLED") != "true", "Tidewave should be disabled in release");

    qunsetenv("MIX_ENV");
    return ctx.passed;
#else
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--devtools");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.devtools == true, "--devtools should be set");
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Dev, "--devtools should set env to Dev");
    TEST_ASSERT(ctx, args.mcp == true, "--devtools should enable mcp");
    TEST_ASSERT(ctx, args.tidewave == true, "--devtools should enable tidewave");
    TEST_ASSERT(ctx, args.chromeDevtools == true, "--devtools should enable Chrome DevTools");
    TEST_ASSERT(ctx, args.repl == true, "--devtools should enable REPL");

    // Test environment variables are set correctly
    applyEnvironmentVariables(args, "gui");
    TEST_ASSERT(ctx, qgetenv("MIX_ENV") == "dev", "MIX_ENV should be dev");
    TEST_ASSERT(ctx, qgetenv("TAU5_ELIXIR_REPL_ENABLED") == "true", "REPL should be enabled via environment");
    TEST_ASSERT(ctx, qgetenv("TAU5_TIDEWAVE_ENABLED") == "true", "Tidewave should be enabled via environment");
    TEST_ASSERT(ctx, qgetenv("TAU5_DEVTOOLS_ENABLED") == "true", "DevTools should be enabled via environment");

    // Clean up environment variables
    qunsetenv("MIX_ENV");
    qunsetenv("TAU5_ELIXIR_REPL_ENABLED");
    qunsetenv("TAU5_TIDEWAVE_ENABLED");
    qunsetenv("TAU5_DEVTOOLS_ENABLED");

    return ctx.passed;
#endif
}

bool testEnvironmentFlags(TestContext& ctx) {
#ifdef TAU5_RELEASE_BUILD
    // Environment flags have been removed - environment is determined by build type
    // Release builds always use prod, dev builds always use dev
    // This test is not applicable to the new design
    return true;
#else
    // In development builds, environment flags are also removed
    // but we can test that the default is dev mode
    ArgSimulator sim;
    sim.add("tau5");

    CommonArgs args;
    for (int i = 1; i < sim.argc(); ++i) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
    }

    // In dev builds, default should be Default which becomes dev when applied
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Default, "Default environment in dev build");

    return ctx.passed;
#endif
}

bool testPortArguments(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--port-local");
    sim.add("8080");
    sim.add("--port-public");
    sim.add("443");
    sim.add("--port-mcp");
    sim.add("5555");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        const char* currentArg = sim.argv()[i];
        int oldI = i;
        parseSharedArg(currentArg, nextArg, i, args);
        if (args.hasError) {
            TEST_ASSERT(ctx, false, QString("Error parsing %1: %2").arg(currentArg).arg(QString::fromStdString(args.errorMessage)));
            return ctx.passed;
        }
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.portLocal == 8080, QString("--port-local should be 8080, got %1").arg(args.portLocal));
    TEST_ASSERT(ctx, args.portPublic == 443, QString("--port-public should be 443, got %1").arg(args.portPublic));
    TEST_ASSERT(ctx, args.portMcp == 5555, QString("--port-mcp should be 5555, got %1").arg(args.portMcp));
    return ctx.passed;
}

bool testInvalidPort(TestContext& ctx) {

    // Test port too high
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("99999");

        CommonArgs args;
        for (int i = 1; i < sim.argc(); ++i) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
        }

        TEST_ASSERT(ctx, args.hasError == true, "Port > 65535 should cause error");
        TEST_ASSERT(ctx, args.errorMessage.find("65535") != std::string::npos, "Error should mention port limit");
    }

    // Test non-numeric port
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("abc");

        CommonArgs args;
        for (int i = 1; i < sim.argc(); ++i) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
        }

        TEST_ASSERT(ctx, args.hasError == true, "Non-numeric port should cause error");
    }

    return ctx.passed;
}

bool testModeFlags(TestContext& ctx) {

    // Test --mode-node
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--mode-node");

        CommonArgs args;
        for (int i = 1; i < sim.argc(); ++i) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
        }

        TEST_ASSERT(ctx, args.mode == CommonArgs::Mode::Node, "--mode-node should set Node mode");
    }

    // Test --mode-central
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--mode-central");

        CommonArgs args;
        for (int i = 1; i < sim.argc(); ++i) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
        }

        TEST_ASSERT(ctx, args.mode == CommonArgs::Mode::Central, "--mode-central should set Central mode");
    }

    return ctx.passed;
}

bool testDisableFlags(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--no-midi");
    sim.add("--no-link");
    sim.add("--no-discovery");
    sim.add("--no-nifs");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.noMidi == true, "--no-midi should disable MIDI");
    TEST_ASSERT(ctx, args.noLink == true, "--no-link should disable Link");
    TEST_ASSERT(ctx, args.noDiscovery == true, "--no-discovery should disable discovery");
    TEST_ASSERT(ctx, args.noNifs == true, "--no-nifs should disable all NIFs");
    return ctx.passed;
}

bool testValidationConflicts(TestContext& ctx) {

    // Test port conflicts
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("3000");
        sim.add("--port-public");
        sim.add("3000");  // Same port

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        bool valid = validateArguments(args);
        TEST_ASSERT(ctx, valid == false, "Same local and public port should fail validation");
        TEST_ASSERT(ctx, args.errorMessage.find("cannot be the same") != std::string::npos,
            QString("Error should mention ports cannot be the same, got: %1").arg(QString::fromStdString(args.errorMessage)));
    }

    // Test tidewave without mcp
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--tidewave");
        // Note: not setting --mcp

        CommonArgs args;
        for (int i = 1; i < sim.argc(); ++i) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
        }

        // tidewave should auto-enable mcp
        TEST_ASSERT(ctx, args.mcp == true, "Tidewave should auto-enable MCP");
    }

    return ctx.passed;
}

bool testEnvironmentVariableApplication(TestContext& ctx) {

    // Clear any existing env vars
    qunsetenv("MIX_ENV");
    qunsetenv("TAU5_MODE");
    qunsetenv("TAU5_MCP_PORT");
    qunsetenv("TAU5_MIDI_ENABLED");

    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--mcp");
    sim.add("--port-mcp");
    sim.add("5555");
    sim.add("--no-midi");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    // Apply environment variables
    applyEnvironmentVariables(args, "test");

    // Check they were set correctly
#ifdef TAU5_RELEASE_BUILD
    TEST_ASSERT(ctx, qgetenv("MIX_ENV") == "prod", "MIX_ENV should be prod in release");
#else
    TEST_ASSERT(ctx, qgetenv("MIX_ENV") == "dev", "MIX_ENV should be dev in dev build");
#endif
    TEST_ASSERT(ctx, qgetenv("TAU5_MODE") == "test", "TAU5_MODE should be test");
    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_PORT") == "5555", "TAU5_MCP_PORT should be 5555");
    TEST_ASSERT(ctx, qgetenv("TAU5_MIDI_ENABLED") == "false", "TAU5_MIDI_ENABLED should be false");

    // Clean up
    qunsetenv("MIX_ENV");
    qunsetenv("TAU5_MODE");
    qunsetenv("TAU5_MCP_PORT");
    qunsetenv("TAU5_MIDI_ENABLED");

    return ctx.passed;
}

bool testServerPathArgument(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--server-path");
    sim.add("/custom/server/path");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.serverPath == "/custom/server/path", "--server-path should set custom path");
    return ctx.passed;
}

bool testCheckFlag(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--check");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.check == true, "--check should be set");
    return ctx.passed;
}

bool testCombinedFlags(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--devtools");
    sim.add("--verbose");
    sim.add("--port-local");
    sim.add("3000");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.devtools == true, "devtools should be set");
    TEST_ASSERT(ctx, args.verbose == true, "verbose should be set");
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Dev, "env should be Dev");
    TEST_ASSERT(ctx, args.mcp == true, "mcp should be enabled");
    TEST_ASSERT(ctx, args.tidewave == true, "tidewave should be enabled");
    TEST_ASSERT(ctx, args.portLocal == 3000, "port should be 3000");
    return ctx.passed;
}

// Edge case tests
bool testUnknownFlag(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--unknown-flag");
    sim.add("--also-unknown");  // This should not be recognized either

    CommonArgs args;
    int i = 1;
    int unrecognized = 0;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        bool recognized = parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (!recognized) {
            unrecognized++;
        }
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, unrecognized == 2, QString("Should have 2 unrecognized flags, got %1").arg(unrecognized));
    TEST_ASSERT(ctx, args.hasError == false, "Unknown flags shouldn't cause errors in parseSharedArg");
    return ctx.passed;
}

bool testMissingArguments(TestContext& ctx) {

    // Test missing port number
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        // No port number follows

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.hasError == true, "Missing port should cause error");
        TEST_ASSERT(ctx, args.errorMessage.find("requires a port number") != std::string::npos,
            QString("Error should mention missing port, got: %1").arg(QString::fromStdString(args.errorMessage)));
    }

    // Test missing server path
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--server-path");
        // No path follows

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.hasError == true, "Missing path should cause error");
        TEST_ASSERT(ctx, args.errorMessage.find("requires a path") != std::string::npos,
            QString("Error should mention missing path, got: %1").arg(QString::fromStdString(args.errorMessage)));
    }

    return ctx.passed;
}

bool testDuplicateFlags(TestContext& ctx) {

    // Environment flags have been removed, test duplicate ports instead
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("3000");
        sim.add("--port-local");
        sim.add("4000");  // Duplicate port flag

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // Last one wins
        TEST_ASSERT(ctx, args.portLocal == 4000, "Last port value should win");
        TEST_ASSERT(ctx, args.hasError == false, "No error on duplicate port flags");
    }

    // Test duplicate ports
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("8080");
        sim.add("--port-local");
        sim.add("9090");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // Last one wins
        TEST_ASSERT(ctx, args.portLocal == 9090, "Last port value should win");
    }

    // Test conflicting modes
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--mode-node");
        sim.add("--mode-central");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.mode == CommonArgs::Mode::Central, "Last mode should win");
    }

    return ctx.passed;
}

bool testFlagOrdering(TestContext& ctx) {

    // Environment flags have been removed, test port conflict ordering instead
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("3000");
        sim.add("--port-mcp");
        sim.add("3000");  // Same port for MCP - should fail validation

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // Validate should catch the conflict
        bool valid = validateArguments(args);
        TEST_ASSERT(ctx, valid == false, "Same port for local and MCP should fail validation");
        TEST_ASSERT(ctx, args.errorMessage.find("cannot be the same") != std::string::npos,
                    QString("Error should mention ports cannot be the same, got: %1").arg(QString::fromStdString(args.errorMessage)));
    }

    // Test different ports work fine
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("3000");
        sim.add("--port-mcp");
        sim.add("4000");  // Different port

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        bool valid = validateArguments(args);
        TEST_ASSERT(ctx, valid == true, "Different ports should be valid");
        TEST_ASSERT(ctx, args.portLocal == 3000, "Local port should be 3000");
        TEST_ASSERT(ctx, args.portMcp == 4000, "MCP port should be 4000");
    }

    return ctx.passed;
}

bool testPortEdgeCases(TestContext& ctx) {

    // Test port 0 (explicitly disabled)
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("0");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.portLocal == 0, "Port 0 should be accepted (means disabled)");
        TEST_ASSERT(ctx, args.hasError == false, "Port 0 should not cause error");
    }

    // Test negative port
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("-1");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.hasError == true, "Negative port should cause error");
    }

    // Test maximum valid port
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("65535");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.portLocal == 65535, "Port 65535 should be accepted");
        TEST_ASSERT(ctx, args.hasError == false, "Port 65535 should not cause error");
    }

    // Test leading zeros
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("00080");  // Should parse as 80 in base 10 (not octal)

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.portLocal == 80, QString("Port with leading zeros should parse as decimal 80, got %1").arg(args.portLocal));
    }

    return ctx.passed;
}

// Test default values when no flags are provided
bool testDefaultValues(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    // No flags - just the program name

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
    }

    // Check all defaults
    TEST_ASSERT(ctx, !args.showHelp, "Help should be false by default");
    TEST_ASSERT(ctx, !args.showVersion, "Version should be false by default");
    TEST_ASSERT(ctx, !args.check, "Check should be false by default");
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Default, "Environment should be Default");
    TEST_ASSERT(ctx, args.mode == CommonArgs::Mode::Default, "Mode should be Default");
    TEST_ASSERT(ctx, args.portLocal == 0, "Local port should be 0 (auto)");
    TEST_ASSERT(ctx, args.portPublic == 0, "Public port should be 0 (disabled)");
    TEST_ASSERT(ctx, args.portMcp == 0, "MCP port should be 0 (disabled)");
    TEST_ASSERT(ctx, !args.mcp, "MCP should be disabled by default");
    TEST_ASSERT(ctx, !args.tidewave, "Tidewave should be disabled by default");
    TEST_ASSERT(ctx, !args.chromeDevtools, "Chrome DevTools should be disabled by default");
    TEST_ASSERT(ctx, !args.repl, "REPL should be disabled by default");
    TEST_ASSERT(ctx, !args.verbose, "Verbose should be disabled by default");
    TEST_ASSERT(ctx, !args.noMidi, "MIDI should be enabled by default (noMidi=false)");
    TEST_ASSERT(ctx, !args.noLink, "Link should be enabled by default (noLink=false)");
    TEST_ASSERT(ctx, !args.noDiscovery, "Discovery should be enabled by default (noDiscovery=false)");
    TEST_ASSERT(ctx, !args.hasError, "No error expected");

    return ctx.passed;
}

// Test numeric edge cases for port parsing
bool testPortWhitespaceHandling(TestContext& ctx) {
    // Test whitespace in port numbers - currently accepted by QString::toUInt
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add(" 8080");  // Leading whitespace

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;  // Increment if parseSharedArg didn't
        }

        // QString::toUInt actually trims whitespace, so this works
        TEST_ASSERT(ctx, args.portLocal == 8080 || args.hasError,
                    "Whitespace should either be trimmed or cause error");
    }

    // Test plus prefix - also accepted by QString::toUInt
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-local");
        sim.add("+8080");  // Plus prefix

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;  // Increment if parseSharedArg didn't
        }

        // QString::toUInt accepts +prefix
        TEST_ASSERT(ctx, args.portLocal == 8080 || args.hasError,
                    "Plus prefix should either be accepted as 8080 or cause error");
    }

    return ctx.passed;
}


// Test release build flag rejection
bool testReleaseBuildFlagRejection(TestContext& ctx) {
#ifdef TAU5_RELEASE_BUILD
    // Test that development flags are properly rejected in release builds

    // Test --env-dev rejection
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--env-dev");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // In release mode, dev environment should be forced to prod
        TEST_ASSERT(ctx, args.env != CommonArgs::Env::Dev,
                    "Release build should not allow dev environment");
    }

    // Test --env-test rejection
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--env-test");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.env != CommonArgs::Env::Test,
                    "Release build should not allow test environment");
    }

    // Test --with-tidewave rejection
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--with-tidewave");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, !args.tidewave,
                    "Release build should not allow tidewave MCP server");
    }

    // Test --with-repl rejection
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--with-repl");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, !args.repl,
                    "Release build should not allow Elixir REPL");
    }

    // Test --devtools rejection (combines multiple dev features)
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--devtools");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // In release builds, flags are parsed but environment enforcement overrides them
        TEST_ASSERT(ctx, args.devtools == true,
                    "Release build parses --devtools flag");
        // After environment application, these would be overridden
        applyEnvironmentVariables(args, "gui");
        TEST_ASSERT(ctx, qgetenv("MIX_ENV") == "prod",
                    "Release build forces prod environment");
        qunsetenv("MIX_ENV");
    }

    // Test --chrome-devtools rejection
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--chrome-devtools");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // Chrome DevTools flag is parsed but not enabled in prod environment
        TEST_ASSERT(ctx, args.chromeDevtools == true,
                    "Chrome DevTools flag is parsed");
    }

    // Test --check is ALLOWED in release builds (for CI/CD)
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--check");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.check,
                    "Release build SHOULD allow health check flag for CI/CD");
    }
#else
    // In development builds, verify that dev flags ARE allowed
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--devtools");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Dev,
                    "Development build should allow --devtools");
        TEST_ASSERT(ctx, args.tidewave,
                    "Development build should enable tidewave with --devtools");
        TEST_ASSERT(ctx, args.repl,
                    "Development build should enable REPL with --devtools");
    }
#endif

    return ctx.passed;
}

// Main test runner
// Returns 0 if all pass, or the number of failures
int runCliArgumentTests(int& totalTests, int& passedTests) {
    Tau5Logger::instance().info("\n[CLI Argument Tests]");

    testResults.clear();

    // Run all tests
    RUN_TEST(testHelpFlag);
    RUN_TEST(testVersionFlag);
    RUN_TEST(testDefaultValues);
    RUN_TEST(testDevtoolsFlag);
    RUN_TEST(testEnvironmentFlags);
    RUN_TEST(testPortArguments);
    RUN_TEST(testInvalidPort);
    RUN_TEST(testModeFlags);
    RUN_TEST(testDisableFlags);
    RUN_TEST(testValidationConflicts);
    RUN_TEST(testEnvironmentVariableApplication);
    RUN_TEST(testServerPathArgument);
    RUN_TEST(testCheckFlag);
    RUN_TEST(testCombinedFlags);

    // Edge case tests
    RUN_TEST(testUnknownFlag);
    RUN_TEST(testMissingArguments);
    RUN_TEST(testDuplicateFlags);
    RUN_TEST(testFlagOrdering);
    RUN_TEST(testPortEdgeCases);
    RUN_TEST(testPortWhitespaceHandling);

    // Release build safety tests
    RUN_TEST(testReleaseBuildFlagRejection);


    // Count results
    int passed = 0;
    int failed = 0;

    for (const auto& result : testResults) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }

    // Summary
    totalTests = passed + failed;
    passedTests = passed;
    Tau5Logger::instance().info(QString("\nCLI Tests: %1 passed, %2 failed")
        .arg(passed)
        .arg(failed));

    return failed;
}
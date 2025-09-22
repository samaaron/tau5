#include "cli_args.h"
#include "tau5logger.h"
#include "common.h"
#include <QString>
#include <QStringList>
#include <QList>
#include <map>

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
    QByteArray actualEnv = qgetenv("MIX_ENV");
    if (actualEnv != "prod") {
        Tau5Logger::instance().error(QString("DEBUG: MIX_ENV is '%1' (len=%2), expected 'prod'. args.env=%3")
            .arg(QString::fromUtf8(actualEnv))
            .arg(actualEnv.length())
            .arg(static_cast<int>(args.env)));
    }
    TEST_ASSERT(ctx, actualEnv == "prod", "MIX_ENV should be prod in release");
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

    // Chrome CDP is now configured via static variables (simulating what main.cpp does)
    quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
    Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::enabled == true, "Chrome CDP should be enabled");
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::port == 9220, "Chrome CDP port should be 9220 (default channel 0)");

    // Clean up environment variables
    qunsetenv("MIX_ENV");
    qunsetenv("TAU5_ELIXIR_REPL_ENABLED");
    qunsetenv("TAU5_TIDEWAVE_ENABLED");

    return ctx.passed;
#endif
}

bool testServerModeControl(TestContext& ctx) {
    // Test --dev-with-release-server flag
#ifdef TAU5_RELEASE_BUILD
    // In release builds, server is always in prod mode
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--dev-with-release-server");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod, 
                    "Release build always uses prod server");
    }
#else
    // In dev builds, test server mode control
    {
        // Default should be dev mode
        ArgSimulator sim;
        sim.add("tau5");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Dev, 
                    "Default server mode should be dev in dev builds");
    }
    
    {
        // --dev-with-release-server should set prod mode
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--dev-with-release-server");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod, 
                    "--dev-with-release-server should set prod server mode");
        TEST_ASSERT(ctx, args.serverModeExplicitlySet == true,
                    "--dev-with-release-server should mark server mode as explicitly set");
    }
#endif
    return ctx.passed;
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
    sim.add("--dev-port-chrome-cdp");
    sim.add("9224");

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
    TEST_ASSERT(ctx, args.portChrome == 9224, QString("--dev-port-chrome-cdp should be 9224, got %1").arg(args.portChrome));
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
        sim.add("--dev-tidewave");
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
    sim.add("--dev-server-path");
    sim.add("/custom/server/path");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;  // Only increment if parseSharedArg didn't
    }

    TEST_ASSERT(ctx, args.serverPath == "/custom/server/path", "--dev-server-path should set custom path");
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

bool testCheckWithEnvironmentOverrides(TestContext& ctx) {
    // Test that --check works with environment variable overrides
    // This simulates running: TAU5_MCP_PORT=5556 tau5 --check
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
    
    TEST_ASSERT(ctx, args.check == true, "--check should be set");
    // Environment variables aren't parsed by CLI args, they're handled at runtime
    // This test verifies that --check flag doesn't conflict with env vars
    TEST_ASSERT(ctx, !args.hasError, "No errors should occur with --check");
    return ctx.passed;
}

bool testControlledEnvironmentSecurity(TestContext& ctx) {
    // Test that external environment variables don't leak through to the server
    // via the ServerConfig immutable configuration pattern

    // Set some "external" environment variables that should NOT be passed to server
    qputenv("TAU5_MCP_PORT", "9999");
    qputenv("TAU5_EXTERNAL_VAR", "should_not_appear");
    qputenv("SECRET_KEY_BASE", "leaked_secret");
    qputenv("PHX_SECRET", "another_leak");
    qputenv("RANDOM_VAR", "external_pollution");

    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--mcp");
    sim.add("--port-mcp");
    sim.add("5555");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    // Create ServerConfig - this is the immutable config that generates clean env vars
    ServerConfig config(args, "test");

    // Get the environment variables that would be passed to the server
    std::map<std::string, std::string> serverEnv = config.generateEnvironmentVars();

    // Verify that ONLY the expected variables from CLI args are in the server environment
    TEST_ASSERT(ctx, serverEnv["TAU5_MCP_PORT"] == "5555",
                "Server should get MCP port 5555 from CLI args, not 9999 from external env");
    TEST_ASSERT(ctx, serverEnv["TAU5_MCP_ENABLED"] == "true",
                "Server should have MCP enabled from CLI args");
    TEST_ASSERT(ctx, serverEnv.find("TAU5_EXTERNAL_VAR") == serverEnv.end(),
                "External env var TAU5_EXTERNAL_VAR should NOT appear in server env");
    TEST_ASSERT(ctx, serverEnv.find("SECRET_KEY_BASE") == serverEnv.end(),
                "External env var SECRET_KEY_BASE should NOT appear in server env");
    TEST_ASSERT(ctx, serverEnv.find("PHX_SECRET") == serverEnv.end(),
                "External env var PHX_SECRET should NOT appear in server env");
    TEST_ASSERT(ctx, serverEnv.find("RANDOM_VAR") == serverEnv.end(),
                "External env var RANDOM_VAR should NOT appear in server env");

    // Verify the config is truly immutable - args shouldn't affect the config after creation
    CommonArgs modifiedArgs = args;
    modifiedArgs.portMcp = 7777;
    TEST_ASSERT(ctx, config.getMcpPort() == 5555,
                "ServerConfig should remain immutable after creation");

    // Clean up
    qunsetenv("TAU5_MCP_PORT");
    qunsetenv("TAU5_EXTERNAL_VAR");
    qunsetenv("SECRET_KEY_BASE");
    qunsetenv("PHX_SECRET");
    qunsetenv("RANDOM_VAR");

    return ctx.passed;
}

bool testEnvironmentIsolation(TestContext& ctx) {
    // Test that only expected environment variables are set
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

    // Clear ALL TAU5_* environment variables for a clean test environment
    QStringList env = QProcess::systemEnvironment();
    for (const QString& var : env) {
        if (var.startsWith("TAU5_")) {
            QString varName = var.split('=').first();
            qunsetenv(varName.toLocal8Bit().data());
        }
    }
    // Also clear related vars
    qunsetenv("MIX_ENV");
    qunsetenv("PHX_SECRET");
    
    // Apply environment based on CLI
    applyEnvironmentVariables(args, "test");
    
#ifdef TAU5_RELEASE_BUILD
    // In release builds, --devtools doesn't enable MCP or Chrome DevTools
    // so these environment variables should NOT be set
    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_ENABLED") == "false",
                "MCP should not be enabled by --devtools in release builds");
    TEST_ASSERT(ctx, !qgetenv("TAU5_MCP_PORT").isEmpty(),
                "TAU5_MCP_PORT should still be set (for potential use)");
    // Chrome CDP configuration (simulating what main.cpp does)
    quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
    Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::enabled == false,
                "Chrome CDP should not be enabled in release builds");
#else
    // In dev builds, --devtools enables MCP and Chrome DevTools
    TEST_ASSERT(ctx, !qgetenv("TAU5_MCP_PORT").isEmpty(), 
                "--devtools should set TAU5_MCP_PORT");
    // Configure Chrome CDP from parsed args
    quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
    Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::port == 9220,
                "Chrome CDP port should be 9220 (default channel 0)");
#endif
    
    // Clean up
    qunsetenv("TAU5_MCP_PORT");
    qunsetenv("TAU5_TIDEWAVE_ENABLED");
    qunsetenv("TAU5_ELIXIR_REPL_ENABLED");
    
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
    
#ifdef TAU5_RELEASE_BUILD
    // In release builds, --devtools forces prod environment and doesn't enable other flags
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod, "env should be Prod in release");
    TEST_ASSERT(ctx, args.mcp == false, "mcp should not be enabled in release");
    TEST_ASSERT(ctx, args.tidewave == false, "tidewave should not be enabled in release");
#else
    // In dev builds, --devtools enables dev mode and related features
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Dev, "env should be Dev");
    TEST_ASSERT(ctx, args.mcp == true, "mcp should be enabled");
    TEST_ASSERT(ctx, args.tidewave == true, "tidewave should be enabled");
#endif
    
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
        sim.add("--dev-server-path");
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
    
    // In release builds, environment defaults to Prod; in dev builds it defaults to Default
#ifdef TAU5_RELEASE_BUILD
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod, "Environment should be Prod in release builds");
#else
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Dev, "Environment should be Dev in dev builds");
#endif
    
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
        sim.add("--dev-chrome-cdp");

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

    // Test that --dev-with-release-server + --devtools still doesn't enable dev features in release
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--dev-with-release-server");
        sim.add("--devtools");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // In release builds, server is always prod mode
        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod,
                    "Release build forces prod environment even with --dev-with-release-server");
        
        // Dev features should NOT be enabled in release builds
        TEST_ASSERT(ctx, !args.mcp,
                    "MCP should not be enabled in release build");
        TEST_ASSERT(ctx, !args.tidewave,
                    "Tidewave should not be enabled in release build");
        TEST_ASSERT(ctx, !args.chromeDevtools,
                    "Chrome DevTools should not be enabled in release build");
        TEST_ASSERT(ctx, !args.repl,
                    "REPL should not be enabled in release build");
        
        // Verify via environment variables too
        applyEnvironmentVariables(args, "gui");
        TEST_ASSERT(ctx, qgetenv("MIX_ENV") == "prod",
                    "MIX_ENV should be prod in release");
        TEST_ASSERT(ctx, qgetenv("TAU5_ELIXIR_REPL_ENABLED") != "true",
                    "REPL should not be enabled via environment");
        TEST_ASSERT(ctx, qgetenv("TAU5_TIDEWAVE_ENABLED") != "true",
                    "Tidewave should not be enabled via environment");
        
        qunsetenv("MIX_ENV");
        qunsetenv("TAU5_ELIXIR_REPL_ENABLED");
        qunsetenv("TAU5_TIDEWAVE_ENABLED");
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

// Test friend token functionality
bool testFriendTokenBasic(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--friend-token");
    sim.add("mySecretToken123");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, args.friendToken == "mySecretToken123", 
                "Friend token should be set correctly");
    TEST_ASSERT(ctx, args.portPublic == 7005, 
                "Friend token should auto-enable public endpoint on default port");
    TEST_ASSERT(ctx, !args.hasError, "No error expected");

    // Test environment variables
    applyEnvironmentVariables(args, "test");
    TEST_ASSERT(ctx, qgetenv("TAU5_FRIEND_MODE") == "true",
                "TAU5_FRIEND_MODE should be set");
    TEST_ASSERT(ctx, qgetenv("TAU5_FRIEND_TOKEN") == "mySecretToken123",
                "TAU5_FRIEND_TOKEN should be set correctly");
    TEST_ASSERT(ctx, qgetenv("TAU5_PUBLIC_PORT") == "7005",
                "TAU5_PUBLIC_PORT should be set");

    // Clean up
    qunsetenv("TAU5_FRIEND_MODE");
    qunsetenv("TAU5_FRIEND_TOKEN");
    qunsetenv("TAU5_PUBLIC_PORT");

    return ctx.passed;
}

bool testFriendTokenWithExplicitPort(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--port-public");
    sim.add("8080");
    sim.add("--friend-token");
    sim.add("anotherToken456");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, args.friendToken == "anotherToken456",
                "Friend token should be set correctly");
    TEST_ASSERT(ctx, args.portPublic == 8080,
                "Explicit port should override default");
    TEST_ASSERT(ctx, !args.hasError, "No error expected");

    // Test environment variables
    applyEnvironmentVariables(args, "test");
    TEST_ASSERT(ctx, qgetenv("TAU5_PUBLIC_PORT") == "8080",
                "Explicit port should be used");
    TEST_ASSERT(ctx, qgetenv("TAU5_FRIEND_TOKEN") == "anotherToken456",
                "Friend token should be set");

    // Clean up
    qunsetenv("TAU5_FRIEND_MODE");
    qunsetenv("TAU5_FRIEND_TOKEN");
    qunsetenv("TAU5_PUBLIC_PORT");

    return ctx.passed;
}

bool testFriendTokenAutoGeneration(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--friend-token");
    // No token value provided - should auto-generate

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, !args.hasError,
                "No error expected when --friend-token has no value");
    TEST_ASSERT(ctx, !args.friendToken.empty(),
                "Friend token should be auto-generated");
    TEST_ASSERT(ctx, args.friendToken.length() == 32,
                "Auto-generated token should be 32 characters");
    TEST_ASSERT(ctx, args.portPublic == 7005,
                "Public endpoint should be enabled on default port");

    // Test that the token contains only valid characters
    for (char c : args.friendToken) {
        bool isValid = (c >= '0' && c <= '9') || 
                      (c >= 'A' && c <= 'Z') || 
                      (c >= 'a' && c <= 'z');
        TEST_ASSERT(ctx, isValid,
                    "Auto-generated token should only contain alphanumeric characters");
    }

    return ctx.passed;
}

bool testFriendTokenOrderIndependence(TestContext& ctx) {
    // Test that order of --port-public and --friend-token doesn't matter
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--friend-token");
        sim.add("token1");
        sim.add("--port-public");
        sim.add("9000");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.friendToken == "token1",
                    "Friend token should be set");
        TEST_ASSERT(ctx, args.portPublic == 9000,
                    "Port should be 9000 when specified after friend-token");
    }

    // Reverse order
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--port-public");
        sim.add("9001");
        sim.add("--friend-token");
        sim.add("token2");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.friendToken == "token2",
                    "Friend token should be set");
        TEST_ASSERT(ctx, args.portPublic == 9001,
                    "Port should be 9001 when specified before friend-token");
    }

    return ctx.passed;
}

// Main test runner
// Returns 0 if all pass, or the number of failures
// Test that --dev-with-release-server overrides server mode from --devtools
// but keeps GUI in dev mode
bool testReleaseServerWithDevtools(TestContext& ctx) {
#ifndef TAU5_RELEASE_BUILD
    // This test only makes sense in dev builds where we can choose server mode
    {
        // Test --devtools --dev-with-release-server (order 1)
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--devtools");
        sim.add("--dev-with-release-server");
        
        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }
        
        // Server should be in prod mode due to --dev-with-release-server
        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod,
                    "--dev-with-release-server should force server to prod mode even with --devtools");
        
        // But GUI dev features should still be enabled from --devtools
        TEST_ASSERT(ctx, args.mcp == true,
                    "MCP should still be enabled from --devtools");
        TEST_ASSERT(ctx, args.tidewave == true,
                    "Tidewave should still be enabled from --devtools");
        TEST_ASSERT(ctx, args.chromeDevtools == true,
                    "Chrome DevTools should still be enabled from --devtools");
        TEST_ASSERT(ctx, args.repl == true,
                    "REPL should still be enabled from --devtools");
        TEST_ASSERT(ctx, args.debugPane == true,
                    "Debug pane should still be enabled from --devtools");
    }
    
    {
        // Test reverse order: --dev-with-release-server --devtools
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--dev-with-release-server");
        sim.add("--devtools");
        
        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }
        
        // Server should still be in prod mode (--dev-with-release-server takes precedence)
        TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod,
                    "--dev-with-release-server should force server to prod mode regardless of order");
        
        // GUI dev features should still be enabled
        TEST_ASSERT(ctx, args.mcp == true,
                    "MCP should be enabled (reverse order)");
        TEST_ASSERT(ctx, args.tidewave == true,
                    "Tidewave should be enabled (reverse order)");
        TEST_ASSERT(ctx, args.chromeDevtools == true,
                    "Chrome DevTools should be enabled (reverse order)");
        TEST_ASSERT(ctx, args.repl == true,
                    "REPL should be enabled (reverse order)");
        TEST_ASSERT(ctx, args.debugPane == true,
                    "Debug pane should be enabled (reverse order)");
    }
#endif
    return ctx.passed;
}

bool testCheckWithReleaseServer(TestContext& ctx) {
#ifndef TAU5_RELEASE_BUILD
    ArgSimulator sim;
    sim.add("tau5-node");
    sim.add("--check");
    sim.add("--dev-with-release-server");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, args.check == true, "--check flag should be set");
    TEST_ASSERT(ctx, args.env == CommonArgs::Env::Prod,
                "--dev-with-release-server should set env to Prod");
    TEST_ASSERT(ctx, args.serverModeExplicitlySet == true,
                "serverModeExplicitlySet should be true with --dev-with-release-server");
    TEST_ASSERT(ctx, !args.hasError, "No errors should occur with valid flags");
#endif
    return ctx.passed;
}

// Channel tests
bool testChannelDefault(TestContext& ctx) {
    ArgSimulator sim;
    sim.add("tau5");

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, args.channel == 0, "Default channel should be 0");
    return ctx.passed;
}

bool testChannelValidValues(TestContext& ctx) {
    // Test each valid channel value
    for (int ch = 0; ch <= 9; ch++) {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--channel");
        sim.add(std::to_string(ch).c_str());

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.channel == ch, QString("Channel should be %1").arg(ch));
        TEST_ASSERT(ctx, !args.hasError, QString("Channel %1 should not cause error").arg(ch));
    }
    return ctx.passed;
}

bool testChannelInvalidValues(TestContext& ctx) {
    // Test channel < 0
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--channel");
        sim.add("-1");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.hasError == true, "Channel -1 should cause error");
        TEST_ASSERT(ctx, args.errorMessage.find("0 and 9") != std::string::npos,
                    "Error should mention valid range");
    }

    // Test channel > 9
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--channel");
        sim.add("10");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.hasError == true, "Channel 10 should cause error");
        TEST_ASSERT(ctx, args.errorMessage.find("0 and 9") != std::string::npos,
                    "Error should mention valid range");
    }

    // Test non-numeric channel
    {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--channel");
        sim.add("abc");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        TEST_ASSERT(ctx, args.hasError == true, "Non-numeric channel should cause error");
    }

    return ctx.passed;
}

bool testChannelPortDefaults(TestContext& ctx) {
    // Test that channel modifies default ports when MCP/Chrome are enabled
    for (int ch = 0; ch <= 9; ch++) {
        ArgSimulator sim;
        sim.add("tau5");
        sim.add("--channel");
        sim.add(std::to_string(ch).c_str());
        sim.add("--mcp");
        sim.add("--dev-chrome-cdp");

        CommonArgs args;
        int i = 1;
        while (i < sim.argc()) {
            const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
            int oldI = i;
            parseSharedArg(sim.argv()[i], nextArg, i, args);
            if (i == oldI) i++;
        }

        // Apply environment variables to check the resulting port values
        applyEnvironmentVariables(args);

        QString expectedMcpPort = QString::number(5550 + ch);
        quint16 expectedChromeCdpPort = 9220 + ch;

        TEST_ASSERT(ctx, qgetenv("TAU5_MCP_PORT") == expectedMcpPort.toUtf8(),
                    QString("MCP port should be %1 for channel %2").arg(expectedMcpPort).arg(ch));

        // Configure Chrome CDP from parsed args
        quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
        Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
        TEST_ASSERT(ctx, Tau5Common::ChromeCDP::port == expectedChromeCdpPort,
                    QString("Chrome CDP port should be %1 for channel %2").arg(expectedChromeCdpPort).arg(ch));

        // Clean up
        qunsetenv("TAU5_MCP_PORT");
        qunsetenv("TAU5_MCP_ENABLED");
    }
    return ctx.passed;
}

bool testMcpDisabledByDefault(TestContext& ctx) {
    // Test that MCP is explicitly disabled when not requested
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--channel");
    sim.add("2");
    // Note: NOT adding --mcp or --devtools

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, args.channel == 2, "Channel should be 2");
    TEST_ASSERT(ctx, args.mcp == false, "MCP should not be enabled");
    TEST_ASSERT(ctx, args.chromeDevtools == false, "Chrome DevTools should not be enabled");

    // Apply environment variables and check
    applyEnvironmentVariables(args);

    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_ENABLED") == "false",
                "TAU5_MCP_ENABLED should be explicitly set to false");
    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_PORT") == "5552",
                "TAU5_MCP_PORT should still be set to channel-based default (5552)");
    // Configure Chrome CDP from parsed args
    quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
    Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::enabled == false,
                "Chrome CDP should be disabled");
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::port == 9222,
                "Chrome CDP port should be 9222 (9220 + channel 2)");

    // Clean up
    qunsetenv("TAU5_MCP_ENABLED");
    qunsetenv("TAU5_MCP_PORT");

    return ctx.passed;
}

bool testChannelAloneDoesNotEnableServices(TestContext& ctx) {
    // Test that using --channel alone doesn't enable any services
    // This test will likely FAIL if there's a bug where channel enables services

    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--channel");
    sim.add("3");
    // ONLY channel, nothing else

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    // After parsing, these should all be false
    TEST_ASSERT(ctx, args.channel == 3, "Channel should be 3");
    TEST_ASSERT(ctx, args.devtools == false, "--devtools flag should not be set");
    TEST_ASSERT(ctx, args.mcp == false, "MCP should not be enabled");
    TEST_ASSERT(ctx, args.chromeDevtools == false, "Chrome DevTools should not be enabled");
    TEST_ASSERT(ctx, args.tidewave == false, "Tidewave should not be enabled");
    TEST_ASSERT(ctx, args.repl == false, "REPL should not be enabled");

    // Port values should still be 0 (not set)
    TEST_ASSERT(ctx, args.portMcp == 0, "MCP port should be 0 (not explicitly set)");
    TEST_ASSERT(ctx, args.portChrome == 0, "Chrome port should be 0 (not explicitly set)");

    // After applying environment variables
    applyEnvironmentVariables(args);

    // Services should be explicitly disabled
    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_ENABLED") == "false",
                "MCP should be explicitly disabled in environment");

    // MCP port should still be set to channel defaults
    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_PORT") == "5553",
                "MCP port should be 5553 (5550 + channel 3)");

    // Configure Chrome CDP from parsed args
    quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
    Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::enabled == false,
                "Chrome CDP should be disabled");
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::port == 9223,
                "Chrome CDP port should be 9223 (9220 + channel 3)");

    // Clean up
    qunsetenv("TAU5_MCP_ENABLED");
    qunsetenv("TAU5_MCP_PORT");

    return ctx.passed;
}

bool testChannelWithExplicitPorts(TestContext& ctx) {
    // Test that explicit ports override channel-based defaults
    ArgSimulator sim;
    sim.add("tau5");
    sim.add("--channel");
    sim.add("5");  // Channel 5 would give 5555 and 9225
    sim.add("--port-mcp");
    sim.add("6666");  // But this explicit port should win
    sim.add("--dev-port-chrome-cdp");
    sim.add("7777");  // And this one too

    CommonArgs args;
    int i = 1;
    while (i < sim.argc()) {
        const char* nextArg = (i + 1 < sim.argc()) ? sim.argv()[i + 1] : nullptr;
        int oldI = i;
        parseSharedArg(sim.argv()[i], nextArg, i, args);
        if (i == oldI) i++;
    }

    TEST_ASSERT(ctx, args.channel == 5, "Channel should be 5");
    TEST_ASSERT(ctx, args.portMcp == 6666, "Explicit MCP port should override channel default");
    TEST_ASSERT(ctx, args.portChrome == 7777, "Explicit Chrome port should override channel default");

    // Apply environment variables to verify
    applyEnvironmentVariables(args);

    TEST_ASSERT(ctx, qgetenv("TAU5_MCP_PORT") == "6666",
                "Explicit MCP port should be used in environment");
    // Configure Chrome CDP from parsed args
    quint16 chromeCdpPort = args.portChrome > 0 ? args.portChrome : (9220 + args.channel);
    Tau5Common::ChromeCDP::configure(args.chromeDevtools, chromeCdpPort);
    TEST_ASSERT(ctx, Tau5Common::ChromeCDP::port == 7777,
                "Explicit Chrome CDP port should be used");

    // Clean up
    qunsetenv("TAU5_MCP_PORT");
    qunsetenv("TAU5_MCP_ENABLED");

    return ctx.passed;
}

int runCliArgumentTests(int& totalTests, int& passedTests) {
    Tau5Logger::instance().info("\n[CLI Argument Tests]");

    testResults.clear();

    // Run all tests
    RUN_TEST(testHelpFlag);
    RUN_TEST(testVersionFlag);
    RUN_TEST(testDefaultValues);
    RUN_TEST(testDevtoolsFlag);
    RUN_TEST(testServerModeControl);
    RUN_TEST(testPortArguments);
    RUN_TEST(testInvalidPort);
    RUN_TEST(testModeFlags);
    RUN_TEST(testDisableFlags);
    RUN_TEST(testValidationConflicts);
    RUN_TEST(testEnvironmentVariableApplication);
    RUN_TEST(testServerPathArgument);
    RUN_TEST(testCheckFlag);
    RUN_TEST(testCheckWithEnvironmentOverrides);
    RUN_TEST(testControlledEnvironmentSecurity);
    RUN_TEST(testEnvironmentIsolation);
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

    // Friend token tests
    RUN_TEST(testFriendTokenBasic);
    RUN_TEST(testFriendTokenWithExplicitPort);
    RUN_TEST(testFriendTokenAutoGeneration);
    RUN_TEST(testFriendTokenOrderIndependence);
    
    // Server mode precedence tests
    RUN_TEST(testReleaseServerWithDevtools);
    RUN_TEST(testCheckWithReleaseServer);

    // Channel tests
    RUN_TEST(testChannelDefault);
    RUN_TEST(testChannelValidValues);
    RUN_TEST(testChannelInvalidValues);
    RUN_TEST(testChannelPortDefaults);
    RUN_TEST(testMcpDisabledByDefault);
    RUN_TEST(testChannelAloneDoesNotEnableServices);
    RUN_TEST(testChannelWithExplicitPorts);

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
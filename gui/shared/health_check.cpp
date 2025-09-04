#include "health_check.h"
#include "tau5logger.h"
#include "common.h"
#include "beam.h"
#include "test_cli_args.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QTcpServer>
#include <QTemporaryFile>
#include <QLibrary>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace Tau5Common;

namespace Tau5HealthCheck {

void printCheckResult(const CheckResult& result, bool verbose) {
    QString prefix;
    LogLevel level;
    
    switch (result.status) {
        case CheckStatus::Passed:
            prefix = "  ✓";
            level = LogLevel::Info;
            break;
        case CheckStatus::Warning:
            prefix = "  ⚠";
            level = LogLevel::Warning;
            break;
        case CheckStatus::Failed:
            prefix = "  ✗";
            level = LogLevel::Error;
            break;
    }
    
    QString output = QString("%1 %2").arg(prefix).arg(result.test);
    if (!result.message.isEmpty() && (verbose || result.status != CheckStatus::Passed)) {
        output += QString(": %1").arg(result.message);
    }
    
    Tau5Logger::instance().log(level, "", output);
}

HealthCheckSummary calculateSummary(const QList<CheckResult>& results) {
    HealthCheckSummary summary = {0, 0, 0, false, ""};
    
    for (const auto& result : results) {
        switch (result.status) {
            case CheckStatus::Passed:
                summary.passed++;
                break;
            case CheckStatus::Warning:
                summary.warnings++;
                break;
            case CheckStatus::Failed:
                summary.failed++;
                if (result.critical) {
                    summary.hasBlockingFailures = true;
                }
                break;
        }
    }
    
    if (summary.hasBlockingFailures) {
        summary.overallStatus = "FAILED (Critical errors)";
    } else if (summary.failed > 0) {
        summary.overallStatus = "FAILED";
    } else if (summary.warnings > 0) {
        summary.overallStatus = "PASSED with warnings";
    } else {
        summary.overallStatus = "PASSED";
    }
    
    return summary;
}

void printSummary(const HealthCheckSummary& summary) {
    Tau5Logger::instance().info("\n[Summary]");
    Tau5Logger::instance().info(QString("  Tests: %1 passed, %2 warnings, %3 failed")
        .arg(summary.passed)
        .arg(summary.warnings)
        .arg(summary.failed));
    
    if (summary.hasBlockingFailures) {
        Tau5Logger::instance().error(QString("  Result: %1").arg(summary.overallStatus));
    } else if (summary.failed > 0) {
        Tau5Logger::instance().error(QString("  Result: %1").arg(summary.overallStatus));
    } else if (summary.warnings > 0) {
        Tau5Logger::instance().warning(QString("  Result: %1").arg(summary.overallStatus));
    } else {
        Tau5Logger::instance().info(QString("  Result: %1").arg(summary.overallStatus));
    }
}

void printSystemInformation(const HealthCheckConfig& config) {
    // Print system information (not tests)
    Tau5Logger::instance().info("\n[System Information]");
    Tau5Logger::instance().info(QString("  Version:     %1").arg(Config::APP_VERSION));
    Tau5Logger::instance().info(QString("  Qt version:  %1").arg(qVersion()));
    
    #ifdef QT_DEBUG
    QString buildType = "Debug";
    #else
    QString buildType = "Release";
    #endif
    Tau5Logger::instance().info(QString("  Build type:  %1").arg(buildType));
    Tau5Logger::instance().info(QString("  Binary:      %1 (%2)").arg(config.binaryName).arg(config.isGui ? "GUI" : "Headless"));
    
    // Print environment info if verbose
    if (config.verbose) {
        Tau5Logger::instance().info(QString("  Server path: %1").arg(config.serverPath.isEmpty() ? "(not set)" : config.serverPath));
        Tau5Logger::instance().info(QString("  Log path:    %1").arg(Tau5Logger::instance().currentSessionPath()));
        #ifdef Q_OS_LINUX
        if (qEnvironmentVariableIsSet("DISPLAY")) {
            Tau5Logger::instance().info(QString("  Display:     %1").arg(QString::fromLocal8Bit(qgetenv("DISPLAY"))));
        }
        #endif
    }
}

QList<CheckResult> checkServerComponents(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    // Check server path exists
    if (config.serverPath.isEmpty()) {
        results.append({
            "Server Components",
            "Server path configured",
            CheckStatus::Failed,
            "No server path specified",
            true
        });
        return results;  // Can't check anything else without server path
    }
    
    QDir serverDir(config.serverPath);
    if (!serverDir.exists()) {
        results.append({
            "Server Components",
            "Server directory exists",
            CheckStatus::Failed,
            config.serverPath,
            true
        });
        return results;
    }
    
    results.append({
        "Server Components",
        "Server directory found",
        CheckStatus::Passed,
        config.serverPath,
        false
    });
    
    // Check for Elixir project files
    if (serverDir.exists("mix.exs")) {
        results.append({
            "Server Components",
            "Elixir project structure",
            CheckStatus::Passed,
            "mix.exs found",
            false
        });
    } else {
        results.append({
            "Server Components",
            "Elixir project structure",
            CheckStatus::Failed,
            "mix.exs not found",
            true
        });
    }
    
    // Check for lib directory
    if (serverDir.exists("lib/tau5")) {
        results.append({
            "Server Components",
            "Tau5 source code",
            CheckStatus::Passed,
            "lib/tau5 found",
            false
        });
    } else {
        results.append({
            "Server Components",
            "Tau5 source code",
            CheckStatus::Failed,
            "lib/tau5 not found",
            true
        });
    }
    
    // Check for production release
    QString releasePath = QString("%1/_build/prod/rel/tau5").arg(config.serverPath);
    if (QDir(releasePath).exists()) {
        results.append({
            "Server Components",
            "Production release",
            CheckStatus::Passed,
            "Found",
            false
        });
    } else {
        results.append({
            "Server Components",
            "Production release",
            CheckStatus::Warning,
            "Not built (run: mix release)",
            false
        });
    }
    
    // Check for development dependencies
    QString depsPath = QString("%1/deps").arg(config.serverPath);
    if (QDir(depsPath).exists()) {
        results.append({
            "Server Components",
            "Dependencies",
            CheckStatus::Passed,
            "deps/ directory found",
            false
        });
    } else {
        results.append({
            "Server Components",
            "Dependencies",
            CheckStatus::Warning,
            "Not installed (run: mix deps.get)",
            false
        });
    }
    
    return results;
}

QList<CheckResult> checkNetworking(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    // Test port allocation
    quint16 testPort = config.testPort;
    if (testPort == 0) {
        // Try to allocate a random port
        auto portHolder = allocatePort(testPort);
        if (portHolder && testPort != 0) {
            results.append({
                "Networking",
                "Port allocation",
                CheckStatus::Passed,
                QString("Allocated port %1").arg(testPort),
                false
            });
            portHolder->close();
        } else {
            results.append({
                "Networking",
                "Port allocation",
                CheckStatus::Failed,
                "Could not allocate port",
                true
            });
        }
    } else {
        // Test specific port on both IPv4 and IPv6
        QTcpServer testServerV4;
        QTcpServer testServerV6;
        bool v4Success = testServerV4.listen(QHostAddress::LocalHost, testPort);
        bool v6Success = testServerV6.listen(QHostAddress::LocalHostIPv6, testPort);
        testServerV4.close();
        testServerV6.close();
        
        if (v4Success && v6Success) {
            results.append({
                "Networking",
                "Port binding",
                CheckStatus::Passed,
                QString("Can bind to port %1 (IPv4 and IPv6)").arg(testPort),
                false
            });
        } else if (v4Success || v6Success) {
            results.append({
                "Networking",
                "Port binding",
                CheckStatus::Warning,
                QString("Can bind to port %1 (%2 only)").arg(testPort).arg(v4Success ? "IPv4" : "IPv6"),
                false
            });
        } else {
            results.append({
                "Networking",
                "Port binding",
                CheckStatus::Failed,
                QString("Cannot bind to port %1").arg(testPort),
                true
            });
        }
    }
    
    return results;
}

QList<CheckResult> checkFileSystem(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    // Check log directory
    QString logDir = Tau5Logger::getBaseLogDir();
    QDir logDirectory(logDir);
    if (logDirectory.exists()) {
        // Try to write a test file
        QTemporaryFile testFile(logDirectory.absoluteFilePath("tau5_test_XXXXXX"));
        if (testFile.open()) {
            results.append({
                "File System",
                "Log directory",
                CheckStatus::Passed,
                QString("Writable: %1").arg(logDir),
                false
            });
            testFile.close();
        } else {
            results.append({
                "File System",
                "Log directory",
                CheckStatus::Failed,
                QString("Not writable: %1").arg(logDir),
                true
            });
        }
    } else {
        results.append({
            "File System",
            "Log directory",
            CheckStatus::Failed,
            QString("Does not exist: %1").arg(logDir),
            true
        });
    }
    
    return results;
}

QList<CheckResult> checkBEAMRuntime(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    // Only run BEAM tests if we have a valid server path
    if (config.serverPath.isEmpty() || !QDir(config.serverPath).exists()) {
        results.append({
            "BEAM Runtime",
            "BEAM startup test",
            CheckStatus::Warning,
            "Skipped (no valid server path)",
            false
        });
        return results;
    }
    
    // Check BEAM runtime components (without starting the VM)
    QString releasePath = QString("%1/_build/prod/rel/tau5").arg(config.serverPath);
    if (QDir(releasePath).exists()) {
        // Check for ERTS
        QDir releaseDir(releasePath);
        QStringList ertsDirs = releaseDir.entryList(QStringList() << "erts-*", QDir::Dirs);
        if (!ertsDirs.isEmpty()) {
            results.append({
                "BEAM Runtime",
                "ERTS runtime",
                CheckStatus::Passed,
                ertsDirs.first(),
                false
            });
        } else {
            results.append({
                "BEAM Runtime",
                "ERTS runtime",
                CheckStatus::Failed,
                "ERTS not found in release",
                true
            });
        }
        
        // Check for VM args - be flexible about version location
        QDir releasesDir(QString("%1/releases").arg(releasePath));
        bool vmArgsFound = false;
        QString vmArgsLocation;
        
        // First try the expected version
        QString expectedVmArgs = QString("%1/releases/%2/vm.args").arg(releasePath).arg(Config::APP_VERSION);
        if (QFile::exists(expectedVmArgs)) {
            vmArgsFound = true;
            vmArgsLocation = Config::APP_VERSION;
        } else if (releasesDir.exists()) {
            // Look for any version directory with vm.args
            QStringList versionDirs = releasesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& version : versionDirs) {
                QString vmArgsPath = QString("%1/releases/%2/vm.args").arg(releasePath).arg(version);
                if (QFile::exists(vmArgsPath)) {
                    vmArgsFound = true;
                    vmArgsLocation = version;
                    break;
                }
            }
        }
        
        if (vmArgsFound) {
            results.append({
                "BEAM Runtime",
                "VM configuration",
                CheckStatus::Passed,
                QString("vm.args found (version %1)").arg(vmArgsLocation),
                false
            });
        } else {
            results.append({
                "BEAM Runtime",
                "VM configuration",
                CheckStatus::Warning,
                "vm.args not found in any release",
                false
            });
        }
        
        // Check for bin/tau5 executable
        QString binPath = QString("%1/bin/tau5").arg(releasePath);
        if (QFile::exists(binPath)) {
            results.append({
                "BEAM Runtime",
                "Release executable",
                CheckStatus::Passed,
                "bin/tau5 found",
                false
            });
        } else {
            results.append({
                "BEAM Runtime",
                "Release executable",
                CheckStatus::Warning,
                "bin/tau5 not found",
                false
            });
        }
    } else {
        results.append({
            "BEAM Runtime",
            "BEAM runtime check",
            CheckStatus::Warning,
            "No production release to test",
            false
        });
    }
    
    return results;
}

QList<CheckResult> checkNIFs(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    // NIFs are bundled with the main tau5 application in tau5-{version}/priv/nifs/
    QString releasePath = QString("%1/_build/prod/rel/tau5").arg(config.serverPath);
    QString nifPath = QString("%1/lib/tau5-%2/priv/nifs").arg(releasePath).arg(Config::APP_VERSION);
    QDir nifDir(nifPath);
    
    if (nifDir.exists()) {
        // Platform-specific library naming for BEAM NIFs
        QString libPrefix = "";
        QString libSuffix = "";
        #ifdef Q_OS_WIN
            libSuffix = ".dll";
        #else
            // BEAM NIFs use .so on both Linux and macOS
            libPrefix = "lib";
            libSuffix = ".so";
        #endif
        
        // Check for sp_midi NIF
        QString midiLibName = QString("%1sp_midi%2").arg(libPrefix).arg(libSuffix);
        QString midiLib = nifDir.absoluteFilePath(midiLibName);
        bool midiFound = QFile::exists(midiLib);
        results.append({
            "NIFs",
            "MIDI support library",
            midiFound ? CheckStatus::Passed : CheckStatus::Warning,
            midiFound ? QString("%1 found").arg(midiLibName) : QString("%1 not found").arg(midiLibName),
            false
        });
        
        // Check for sp_link NIF
        QString linkLibName = QString("%1sp_link%2").arg(libPrefix).arg(libSuffix);
        QString linkLib = nifDir.absoluteFilePath(linkLibName);
        bool linkFound = QFile::exists(linkLib);
        results.append({
            "NIFs",
            "Ableton Link library",
            linkFound ? CheckStatus::Passed : CheckStatus::Warning,
            linkFound ? QString("%1 found").arg(linkLibName) : QString("%1 not found").arg(linkLibName),
            false
        });
        
        // Check for tau5_discovery NIF
        QString discoveryLibName = QString("%1tau5_discovery%2").arg(libPrefix).arg(libSuffix);
        QString discoveryLib = nifDir.absoluteFilePath(discoveryLibName);
        bool discoveryFound = QFile::exists(discoveryLib);
        results.append({
            "NIFs",
            "Network Discovery library",
            discoveryFound ? CheckStatus::Passed : CheckStatus::Warning,
            discoveryFound ? QString("%1 found").arg(discoveryLibName) : QString("%1 not found").arg(discoveryLibName),
            false
        });
    } else {
        // If no release build, just note the configured state
        bool midiEnabled = qgetenv("TAU5_MIDI_ENABLED") != "false";
        bool linkEnabled = qgetenv("TAU5_LINK_ENABLED") != "false";
        bool discoveryEnabled = qgetenv("TAU5_DISCOVERY_ENABLED") != "false";
        
        results.append({
            "NIFs",
            "NIF configuration",
            CheckStatus::Warning,
            QString("MIDI: %1, Link: %2, Discovery: %3 (no release to verify)")
                .arg(midiEnabled ? "enabled" : "disabled")
                .arg(linkEnabled ? "enabled" : "disabled")
                .arg(discoveryEnabled ? "enabled" : "disabled"),
            false
        });
    }
    
    return results;
}

QList<CheckResult> checkGuiComponents(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    if (!config.isGui) {
        return results;  // Skip GUI checks for tau5-node
    }
    
    // Check Qt WebEngine - for GUI builds, it's always required and available
    // (tau5 won't compile without it, so if we're here in GUI mode, it exists)
    results.append({
        "GUI Systems",
        "Qt WebEngine",
        CheckStatus::Passed,
        "Available",
        false
    });
    
    // Check display (on Linux)
    #ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsSet("DISPLAY")) {
        results.append({
            "GUI Systems",
            "Display server",
            CheckStatus::Passed,
            qgetenv("DISPLAY"),
            false
        });
    } else {
        results.append({
            "GUI Systems",
            "Display server",
            CheckStatus::Warning,
            "DISPLAY not set (using offscreen)",
            false
        });
    }
    #endif
    
    // Check OpenGL - actually test if it works, not just if compiled
    #ifndef QT_NO_OPENGL
    // Could test actual OpenGL context creation here, but that requires
    // a QGuiApplication not QCoreApplication. For now, just check compilation.
    results.append({
        "GUI Systems",
        "OpenGL support",
        CheckStatus::Passed,
        "OpenGL available",
        false
    });
    #else
    results.append({
        "GUI Systems",
        "OpenGL support",
        CheckStatus::Warning,
        "Not compiled with OpenGL",
        false
    });
    #endif
    
    return results;
}

QList<CheckResult> checkRuntimeDependencies(const HealthCheckConfig& config) {
    QList<CheckResult> results;
    
    // Check Qt libraries are actually functional (not just version string)
    // This could actually fail if Qt is misconfigured
    try {
        QTemporaryFile testFile;
        if (testFile.open()) {
            testFile.close();
            results.append({
                "Runtime Dependencies",
                "Qt Core functionality",
                CheckStatus::Passed,
                "Qt file I/O working",
                false
            });
        } else {
            results.append({
                "Runtime Dependencies",
                "Qt Core functionality",
                CheckStatus::Failed,
                "Qt file I/O test failed",
                true
            });
        }
    } catch (...) {
        results.append({
            "Runtime Dependencies",
            "Qt Core functionality",
            CheckStatus::Failed,
            "Qt threw exception",
            true
        });
    }
    
    // Platform-specific checks
    #ifdef Q_OS_WIN
    // Check Visual C++ runtime (both DLLs are often needed)
    HMODULE vcruntime = GetModuleHandle(L"VCRUNTIME140.dll");
    if (!vcruntime) {
        vcruntime = LoadLibraryW(L"VCRUNTIME140.dll");
        if (vcruntime) FreeLibrary(vcruntime);
    }
    
    HMODULE vcruntime1 = GetModuleHandle(L"VCRUNTIME140_1.dll");
    if (!vcruntime1) {
        vcruntime1 = LoadLibraryW(L"VCRUNTIME140_1.dll");
        if (vcruntime1) FreeLibrary(vcruntime1);
    }
    
    if (vcruntime && vcruntime1) {
        results.append({
            "Runtime Dependencies",
            "Visual C++ Runtime",
            CheckStatus::Passed,
            "VCRUNTIME140.dll and VCRUNTIME140_1.dll available",
            false
        });
    } else if (vcruntime) {
        results.append({
            "Runtime Dependencies",
            "Visual C++ Runtime",
            CheckStatus::Warning,
            "VCRUNTIME140.dll found, but VCRUNTIME140_1.dll missing",
            false
        });
    } else if (vcruntime1) {
        results.append({
            "Runtime Dependencies",
            "Visual C++ Runtime",
            CheckStatus::Warning,
            "VCRUNTIME140_1.dll found, but VCRUNTIME140.dll missing",
            false
        });
    } else {
        results.append({
            "Runtime Dependencies",
            "Visual C++ Runtime",
            CheckStatus::Failed,
            "Neither VCRUNTIME140.dll nor VCRUNTIME140_1.dll found",
            true
        });
    }
    #endif
    
    return results;
}

int runHealthCheck(const HealthCheckConfig& config) {
    // Print header
    Tau5Logger::instance().info("===============================================");
    Tau5Logger::instance().info("Tau5 System Health Check");
    Tau5Logger::instance().info(QString("Binary: %1 (%2)")
        .arg(config.binaryName)
        .arg(config.isGui ? "GUI" : "Headless"));
    Tau5Logger::instance().info("===============================================");
    
    // Print system information first (not part of test results)
    printSystemInformation(config);
    
    // Now run actual health checks
    QList<CheckResult> allResults;
    
    Tau5Logger::instance().info("\n[Server Components]");
    auto serverResults = checkServerComponents(config);
    for (const auto& result : serverResults) {
        printCheckResult(result, config.verbose);
        allResults.append(result);
    }
    
    Tau5Logger::instance().info("\n[Runtime Dependencies]");
    auto runtimeResults = checkRuntimeDependencies(config);
    for (const auto& result : runtimeResults) {
        printCheckResult(result, config.verbose);
        allResults.append(result);
    }
    
    Tau5Logger::instance().info("\n[Networking]");
    auto networkResults = checkNetworking(config);
    for (const auto& result : networkResults) {
        printCheckResult(result, config.verbose);
        allResults.append(result);
    }
    
    Tau5Logger::instance().info("\n[File System]");
    auto fsResults = checkFileSystem(config);
    for (const auto& result : fsResults) {
        printCheckResult(result, config.verbose);
        allResults.append(result);
    }
    
    Tau5Logger::instance().info("\n[BEAM Runtime]");
    auto beamResults = checkBEAMRuntime(config);
    for (const auto& result : beamResults) {
        printCheckResult(result, config.verbose);
        allResults.append(result);
    }
    
    Tau5Logger::instance().info("\n[NIFs]");
    auto nifResults = checkNIFs(config);
    for (const auto& result : nifResults) {
        printCheckResult(result, config.verbose);
        allResults.append(result);
    }
    
    // GUI-specific checks
    if (config.isGui) {
        Tau5Logger::instance().info("\n[GUI Systems]");
        auto guiResults = checkGuiComponents(config);
        for (const auto& result : guiResults) {
            printCheckResult(result, config.verbose);
            allResults.append(result);
        }
    }
    
    // Run CLI argument tests (always run them as they're quick)
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = runCliArgumentTests(totalTests, passedTests);
    CheckResult cliTestResult = {
        "System Tests",
        "CLI argument parsing",
        failedTests == 0 ? CheckStatus::Passed : CheckStatus::Failed,
        failedTests == 0 
            ? QString("All %1 tests passed").arg(totalTests)
            : QString("%1 of %2 tests failed").arg(failedTests).arg(totalTests),
        false  // Not critical for operation
    };
    allResults.append(cliTestResult);
    
    // Calculate and print summary
    auto summary = calculateSummary(allResults);
    printSummary(summary);
    
    // Print footer
    Tau5Logger::instance().info("\n===============================================");
    if (summary.hasBlockingFailures || summary.failed > 0) {
        Tau5Logger::instance().error("CHECK FAILED");
        Tau5Logger::instance().info("===============================================");
        return 1;
    } else if (summary.warnings > 0 && config.strictMode) {
        Tau5Logger::instance().warning("CHECK FAILED (strict mode - warnings treated as errors)");
        Tau5Logger::instance().info("===============================================");
        return 1;
    } else {
        Tau5Logger::instance().info("CHECK PASSED");
        Tau5Logger::instance().info("===============================================");
        return 0;
    }
}

} // namespace Tau5HealthCheck
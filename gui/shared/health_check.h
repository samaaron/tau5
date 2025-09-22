#ifndef TAU5_HEALTH_CHECK_H
#define TAU5_HEALTH_CHECK_H

#include <QString>
#include <QList>
#include <QCoreApplication>

namespace Tau5CLI {
    class ServerConfig;
}

namespace Tau5HealthCheck {

    enum class CheckStatus {
        Passed,
        Warning,
        Failed
    };

    struct CheckResult {
        QString category;
        QString test;
        CheckStatus status;
        QString message;
        bool critical;  // If true, failure means system won't work
    };

    struct HealthCheckConfig {
        QString serverPath;
        QString binaryName;     // "tau5" or "tau5-node"
        bool isGui;            // tau5 vs tau5-node
        bool verbose;
        bool strictMode;       // Fail on warnings for CI
        bool runTests;         // Run unit tests
        quint16 testPort;      // Port to test allocation (0 = auto)
        const Tau5CLI::ServerConfig* serverConfig;  // Optional server configuration
    };

    struct HealthCheckSummary {
        int passed;
        int warnings;
        int failed;
        bool hasBlockingFailures;
        QString overallStatus;
    };

    // Main health check function
    // Returns exit code (0 = success, 1 = failure)
    int runHealthCheck(const HealthCheckConfig& config);

    // Individual check categories (exposed for testing)
    void printSystemInformation(const HealthCheckConfig& config);
    QList<CheckResult> checkServerComponents(const HealthCheckConfig& config);
    QList<CheckResult> checkRuntimeDependencies(const HealthCheckConfig& config);
    QList<CheckResult> checkNetworking(const HealthCheckConfig& config);
    QList<CheckResult> checkFileSystem(const HealthCheckConfig& config);
    QList<CheckResult> checkBEAMRuntime(const HealthCheckConfig& config);
    QList<CheckResult> checkNIFs(const HealthCheckConfig& config, const Tau5CLI::ServerConfig* serverConfig = nullptr);
    
    // GUI-specific checks
    QList<CheckResult> checkGuiComponents(const HealthCheckConfig& config);
    
    // Utility functions
    void printCheckResult(const CheckResult& result, bool verbose);
    void printSummary(const HealthCheckSummary& summary);
    HealthCheckSummary calculateSummary(const QList<CheckResult>& results);
}

#endif // TAU5_HEALTH_CHECK_H
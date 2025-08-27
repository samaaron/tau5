#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <iostream>
#include <QString>

namespace Tau5Common {
    
    // Exit codes for tau5 applications
    enum class ExitCode : int {
        SUCCESS = 0,
        
        // General errors (1-19)
        GENERAL_ERROR = 1,
        INVALID_ARGUMENTS = 2,
        CONFIGURATION_ERROR = 3,
        
        // File/Directory errors (20-39)
        SERVER_DIR_NOT_FOUND = 20,
        CONFIG_FILE_NOT_FOUND = 21,
        PERMISSION_DENIED = 22,
        
        // Network errors (40-59)
        PORT_ALLOCATION_FAILED = 40,
        PORT_IN_USE = 41,
        NETWORK_INIT_FAILED = 42,
        HEARTBEAT_PORT_FAILED = 43,
        
        // Process errors (60-79)
        BEAM_START_FAILED = 60,
        BEAM_CRASHED = 61,
        PROCESS_SPAWN_FAILED = 62,
        SIGNAL_HANDLER_FAILED = 63,
        
        // Qt/GUI errors (80-99)
        QT_INIT_FAILED = 80,
        WEBENGINE_INIT_FAILED = 81,
        WINDOW_CREATE_FAILED = 82,
        
        // Logger errors (100-109)
        LOGGER_INIT_FAILED = 100,
        LOG_DIR_CREATE_FAILED = 101,
        
        // MCP Server errors (110-119)
        MCP_SERVER_FAILED = 110,
        MCP_CONNECTION_FAILED = 111,
        
        // Security errors (120-129)
        TOKEN_GENERATION_FAILED = 120,
        SECRET_KEY_TOO_SHORT = 121,
        STDIN_CONFIG_FAILED = 122
    };
    
    // Convert exit code to string description
    inline const char* exitCodeToString(ExitCode code) {
        switch (code) {
            case ExitCode::SUCCESS: return "Success";
            case ExitCode::GENERAL_ERROR: return "General error";
            case ExitCode::INVALID_ARGUMENTS: return "Invalid command line arguments";
            case ExitCode::CONFIGURATION_ERROR: return "Configuration error";
            
            case ExitCode::SERVER_DIR_NOT_FOUND: return "Server directory not found";
            case ExitCode::CONFIG_FILE_NOT_FOUND: return "Configuration file not found";
            case ExitCode::PERMISSION_DENIED: return "Permission denied";
            
            case ExitCode::PORT_ALLOCATION_FAILED: return "Failed to allocate network port";
            case ExitCode::PORT_IN_USE: return "Port already in use";
            case ExitCode::NETWORK_INIT_FAILED: return "Network initialization failed";
            case ExitCode::HEARTBEAT_PORT_FAILED: return "Failed to allocate heartbeat port";
            
            case ExitCode::BEAM_START_FAILED: return "Failed to start BEAM/Erlang VM";
            case ExitCode::BEAM_CRASHED: return "BEAM/Erlang VM crashed";
            case ExitCode::PROCESS_SPAWN_FAILED: return "Failed to spawn process";
            case ExitCode::SIGNAL_HANDLER_FAILED: return "Failed to setup signal handlers";
            
            case ExitCode::QT_INIT_FAILED: return "Qt initialization failed";
            case ExitCode::WEBENGINE_INIT_FAILED: return "WebEngine initialization failed";
            case ExitCode::WINDOW_CREATE_FAILED: return "Failed to create window";
            
            case ExitCode::LOGGER_INIT_FAILED: return "Logger initialization failed";
            case ExitCode::LOG_DIR_CREATE_FAILED: return "Failed to create log directory";
            
            case ExitCode::MCP_SERVER_FAILED: return "MCP server failed";
            case ExitCode::MCP_CONNECTION_FAILED: return "MCP connection failed";
            
            case ExitCode::TOKEN_GENERATION_FAILED: return "Failed to generate security token";
            case ExitCode::SECRET_KEY_TOO_SHORT: return "Secret key too short";
            case ExitCode::STDIN_CONFIG_FAILED: return "Failed to write configuration to stdin";
            
            default: return "Unknown error";
        }
    }
    
    // Helper to exit with proper error code and message
    inline void exitWithError(ExitCode code, const QString& additionalInfo = QString()) {
        if (!additionalInfo.isEmpty()) {
            std::cerr << exitCodeToString(code) << ": " << additionalInfo.toStdString() << std::endl;
        } else {
            std::cerr << exitCodeToString(code) << std::endl;
        }
        std::exit(static_cast<int>(code));
    }
}

#endif // ERROR_CODES_H
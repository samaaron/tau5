# Tau5 Core Library - Shared components for GUI and CLI
cmake_minimum_required(VERSION 3.21)

# Define the core library
add_library(tau5_core STATIC
    beam.cpp
    beam.h
    tau5logger.cpp
    tau5logger.h
    common.cpp
    common.h
    health_check.cpp
    health_check.h
    test_cli_args.cpp
    test_cli_args.h
    error_codes.h
    cli_args.h
)

# Set properties for the library
set_target_properties(tau5_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Configure include directories
target_include_directories(tau5_core
    PUBLIC
        # When building
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For parent directory access
        # When installed
        $<INSTALL_INTERFACE:include/tau5>
    PRIVATE
        ${CMAKE_BINARY_DIR}  # For generated files
)

# Link Qt dependencies
target_link_libraries(tau5_core
    PUBLIC
        Qt::Core
        Qt::Network
        Qt::Concurrent  # Needed for QtConcurrent::run in beam.cpp
)

# Add compile definitions
target_compile_definitions(tau5_core
    PUBLIC
        TAU5_VERSION="${PROJECT_VERSION}"
    PRIVATE
        TAU5_CORE_IMPL
)

# Add server path definition from parent CMakeLists
if(DEFINED TAU5_SERVER_PATH_DEFAULT)
    target_compile_definitions(tau5_core PRIVATE TAU5_SERVER_PATH_DEFAULT="${TAU5_SERVER_PATH_DEFAULT}")
endif()

# Platform-specific configurations
if(WIN32)
    target_compile_definitions(tau5_core PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(APPLE)
    target_compile_definitions(tau5_core PRIVATE TARGET_OS_MAC)
endif()

# Export compile commands for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional: Precompiled headers for faster builds
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
    target_precompile_headers(tau5_core
        PRIVATE
            <QObject>
            <QString>
            <QProcess>
            <QTimer>
            <QDebug>
    )
endif()

# Install rules (for future packaging)
install(TARGETS tau5_core
    EXPORT tau5_core_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include/tau5
)

# Install headers
install(FILES
    beam.h
    tau5logger.h
    common.h
    error_codes.h
    DESTINATION include/tau5
)

# Export targets for use by other projects
install(EXPORT tau5_core_targets
    FILE tau5CoreTargets.cmake
    NAMESPACE Tau5::
    DESTINATION lib/cmake/tau5
)

# Generate and install package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tau5CoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Create config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/tau5CoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tau5CoreConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tau5CoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tau5CoreConfigVersion.cmake"
    DESTINATION lib/cmake/tau5
)
cmake_minimum_required(VERSION 3.16)
project(tau5-spectra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

# MCP Server executable
add_executable(tau5-spectra
    tau5_spectra.cpp
    mcpserver_stdio.h
    mcpserver_stdio.cpp
    cdpclient.h
    cdpclient.cpp
    tidewaveproxy.h
    tidewaveproxy.cpp
)

target_include_directories(tau5-spectra PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tau5-spectra
    tau5_core  # Link to our static library for tau5logger
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

# Set output directory to be alongside the main executable
set_target_properties(tau5-spectra PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32)
    # Deploy Qt dependencies for the MCP executable
    add_custom_command(TARGET tau5-spectra POST_BUILD
        COMMAND ${CMAKE_PREFIX_PATH}/bin/windeployqt $<TARGET_FILE:tau5-spectra>
    )
endif()

# Install target
install(TARGETS tau5-spectra
    RUNTIME DESTINATION .
)
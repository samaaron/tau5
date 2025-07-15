cmake_minimum_required(VERSION 3.16)
project(tau5-devtools-mcp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

# MCP Server executable
add_executable(tau5-devtools-mcp
    tau5_devtools_mcp.cpp
    mcpserver_stdio.h
    mcpserver_stdio.cpp
    cdpclient.h
    cdpclient.cpp
    ../logger.h
    ../logger.cpp
)

target_include_directories(tau5-devtools-mcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tau5-devtools-mcp
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

# Set output directory to be alongside the main executable
set_target_properties(tau5-devtools-mcp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
)

if(WIN32)
    # Deploy Qt dependencies for the MCP executable
    add_custom_command(TARGET tau5-devtools-mcp POST_BUILD
        COMMAND ${CMAKE_PREFIX_PATH}/bin/windeployqt $<TARGET_FILE:tau5-devtools-mcp>
    )
endif()

# Install target
install(TARGETS tau5-devtools-mcp
    RUNTIME DESTINATION .
)
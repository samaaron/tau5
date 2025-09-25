cmake_minimum_required(VERSION 3.10)

message(STATUS "CMakeLists: Tau5")

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13")
endif()

# Read version from VERSION file at project root
file(READ "${CMAKE_SOURCE_DIR}/../VERSION" VERSION_FILE_CONTENT)
string(STRIP "${VERSION_FILE_CONTENT}" BASE_VERSION)
if(NOT BASE_VERSION)
    set(BASE_VERSION "0.0.0")
endif()

project("Tau5"
  LANGUAGES CXX C
  DESCRIPTION "Code. Art. Together."
  VERSION ${BASE_VERSION}
  HOMEPAGE_URL "https://sonic-pi.net"
)

# Get commit hash
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

# Check if we're on a tag
execute_process(
    COMMAND git describe --exact-match --tags HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE TAG_RESULT
)

# Build the full version string
if(TAG_RESULT EQUAL 0 AND GIT_TAG)
    # On a tag - use tag version
    set(FULL_VERSION "${GIT_TAG}")
else()
    # Not on a tag - use base version + commit
    set(FULL_VERSION "${BASE_VERSION}")
endif()

# Pass version info to compiler
add_compile_definitions(TAU5_VERSION="${BASE_VERSION}")
add_compile_definitions(TAU5_COMMIT="${GIT_COMMIT_HASH}")
add_compile_definitions(TAU5_FULL_VERSION="${FULL_VERSION}")

message(STATUS "Building Tau5 version: ${GIT_VERSION} (commit: ${GIT_COMMIT_HASH})")

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(QTAPP_ROOT ${CMAKE_CURRENT_LIST_DIR})

if((${BUILD_32BIT}) AND(DEFINED ENV{QT_INSTALL_LOCATION32}))
  message(STATUS "Setting up a 32 bit Qt Build")
  message(STATUS "Setting prefix to user-defined Qt (32-bit) install location: $ENV{QT_INSTALL_LOCATION32}")
  set(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION32})
elseif(DEFINED ENV{QT_INSTALL_LOCATION})
  message(STATUS "Setting prefix to user-defined Qt install location: $ENV{QT_INSTALL_LOCATION}")
  set(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION})
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_TOOLS_DIR ${CMAKE_PREFIX_PATH}/bin)

set_property(GLOBAL PROPERTY AUTOMOC_FOLDER Automoc)
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Automoc)

option(BUILD_NODE_ONLY "Build only the tau5-node CLI without GUI dependencies" OFF)

# Set consistent output directories for all platforms
# All standalone executables go in build/bin
# macOS app bundles will still go in the default location (build/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)

# On Windows, also set library output to the same directory so DLLs are found
if(WIN32)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
endif()

# Build mode configuration
option(TAU5_RELEASE_BUILD "Build for release/distribution (not development)" OFF)

# Server path configuration
# TAU5_SERVER_PATH can be set to override the default path (relative to binary location)
# If not set, development defaults are used based on the platform
if(DEFINED TAU5_SERVER_PATH)
  # Explicit path provided - use it for both dev and release builds
  set(TAU5_SERVER_PATH_DEFAULT "${TAU5_SERVER_PATH}")
  message(STATUS "Using explicit server path: ${TAU5_SERVER_PATH_DEFAULT}")
elseif(TAU5_RELEASE_BUILD)
  # Release build but no explicit path - this is an error
  message(FATAL_ERROR "TAU5_RELEASE_BUILD requires TAU5_SERVER_PATH to be set for the release directory structure")
elseif(CMAKE_INSTALL_PREFIX STREQUAL "/usr/local" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr")
  # Check if we're in the tau5 source tree (development build)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../server/mix.exs")
    # Development build - use relative path from binary location
    if(APPLE)
      # From gui/build/bin/Tau5.app/Contents/MacOS/Tau5 -> server
      set(TAU5_SERVER_PATH_DEFAULT "../../../../../../server")
    elseif(WIN32)
      # From gui/build/bin/tau5.exe -> server
      set(TAU5_SERVER_PATH_DEFAULT "../../../server")
    else()
      # From gui/build/bin/tau5 -> server
      set(TAU5_SERVER_PATH_DEFAULT "../../../server")
    endif()
    message(STATUS "DEVELOPMENT BUILD - Server at source tree: ${TAU5_SERVER_PATH_DEFAULT}")
  else()
    # Installation build - assume standard layout
    set(TAU5_SERVER_PATH_DEFAULT "${CMAKE_INSTALL_PREFIX}/share/tau5/server")
    message(STATUS "Installation build - server path: ${TAU5_SERVER_PATH_DEFAULT}")
  endif()
else()
  # Non-standard install prefix - assume development build
  # Use relative path from binary location
  if(APPLE)
    # From gui/build/bin/Tau5.app/Contents/MacOS/Tau5 -> server
    set(TAU5_SERVER_PATH_DEFAULT "../../../../../../server")
  elseif(WIN32)
    # From gui/build/bin/tau5.exe -> server
    set(TAU5_SERVER_PATH_DEFAULT "../../../server")
  else()
    # From gui/build/bin/tau5 -> server
    set(TAU5_SERVER_PATH_DEFAULT "../../../server")
  endif()
  message(STATUS "Development build - server path relative to binary: ${TAU5_SERVER_PATH_DEFAULT}")
endif()

# Pass release flag to C++ code if this is a release build
if(TAU5_RELEASE_BUILD)
  add_compile_definitions(TAU5_RELEASE_BUILD=1)
endif()

if(BUILD_NODE_ONLY)
  add_compile_definitions(BUILD_NODE_ONLY=1)
  find_package(Qt6 COMPONENTS Core Network Concurrent REQUIRED)
else()
  find_package(Qt6 QUIET)
  find_package(Qt6 COMPONENTS Core Widgets Gui Network Xml Svg SvgWidgets LinguistTools WebEngineWidgets WebSockets Concurrent REQUIRED)

  # Check Qt version for GUI build
  if(Qt6_VERSION VERSION_LESS "6.6.0")
    message(FATAL_ERROR "tau5-gui requires Qt 6.6.0 or later. Found Qt ${Qt6_VERSION}.\n"
                        "For systems with older Qt only the non-gui tau5-node is supported.")
  endif()

  # Find QTermWidget for terminal pane (dev builds only)
  if(NOT TAU5_RELEASE_BUILD)
    find_package(qtermwidget6)
    if(qtermwidget6_FOUND)
      message(STATUS "QTermWidget6 found - Terminal pane will be available")
      set(TERMINAL_PANE_AVAILABLE ON)
    else()
      message(STATUS "QTermWidget6 not found - Terminal pane will not be available")
      set(TERMINAL_PANE_AVAILABLE OFF)
    endif()
  endif()
endif()

if(NOT BUILD_NODE_ONLY)
  if(NOT APPLE)
    set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(QWINDOWKIT_BUILD_WIDGETS ON CACHE BOOL "" FORCE)
    set(QWINDOWKIT_BUILD_QUICK OFF CACHE BOOL "" FORCE)
    set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "" FORCE)
    set(QWINDOWKIT_INSTALL OFF CACHE BOOL "" FORCE)
    set(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS ON CACHE BOOL "" FORCE)
    set(QWINDOWKIT_ENABLE_STYLE_AGENT ON CACHE BOOL "" FORCE)

    add_subdirectory(external/qwindowkit-1.4.0)
  endif()
endif()


# Set executable names based on build type
if(TAU5_RELEASE_BUILD)
  set(APP_NAME tau5-gui)
  set(NODE_APP_NAME tau5-node)
else()
  set(APP_NAME tau5-gui-dev)
  set(NODE_APP_NAME tau5-node-dev)
endif()
set(MACOS_APP_NAME "Tau5")

# Add the shared library subdirectory
add_subdirectory(shared)

# GUI-specific sources
set(GUI_SOURCES
  ${QTAPP_ROOT}/main.cpp
  ${QTAPP_ROOT}/mainwindow.cpp
  ${QTAPP_ROOT}/mainwindow.h
  ${QTAPP_ROOT}/widgets/phxwebview.h
  ${QTAPP_ROOT}/widgets/phxwebview.cpp
  ${QTAPP_ROOT}/widgets/phxwidget.h
  ${QTAPP_ROOT}/widgets/phxwidget.cpp
  ${QTAPP_ROOT}/widgets/mainphxwidget.h
  ${QTAPP_ROOT}/widgets/mainphxwidget.cpp
  ${QTAPP_ROOT}/widgets/tau5devbridge.h
  ${QTAPP_ROOT}/widgets/tau5devbridge.cpp
  ${QTAPP_ROOT}/widgets/shaderpage.h
  ${QTAPP_ROOT}/widgets/shaderpage.cpp
  ${QTAPP_ROOT}/widgets/shadercode.h
  ${QTAPP_ROOT}/widgets/phxurlinterceptor.h
  ${QTAPP_ROOT}/widgets/phxurlinterceptor.cpp
  ${QTAPP_ROOT}/widgets/sandboxedwebview.h
  ${QTAPP_ROOT}/widgets/sandboxedwebview.cpp
  ${QTAPP_ROOT}/widgets/devwebview.h
  ${QTAPP_ROOT}/widgets/devwebview.cpp
  ${QTAPP_ROOT}/widgets/controllayer.h
  ${QTAPP_ROOT}/widgets/controllayer.cpp
  ${QTAPP_ROOT}/widgets/consoleoverlay.h
  ${QTAPP_ROOT}/widgets/consoleoverlay.cpp
  ${QTAPP_ROOT}/widgets/transitionoverlay.h
  ${QTAPP_ROOT}/widgets/transitionoverlay.cpp
  ${QTAPP_ROOT}/lib/fontloader.h
  ${QTAPP_ROOT}/lib/fontloader.cpp
  ${QTAPP_ROOT}/styles/StyleManager.h
  ${QTAPP_ROOT}/styles/StyleManager.cpp
  ${QTAPP_ROOT}/shortcuts/ShortcutManager.h
  ${QTAPP_ROOT}/shortcuts/ShortcutManager.cpp
)

# CLI-specific sources
set(NODE_SOURCES
  ${QTAPP_ROOT}/tau5-node.cpp
)

# GUI sources only (shared components come from tau5_core library)
set(SOURCES ${GUI_SOURCES})

if(NOT APPLE)
  list(APPEND SOURCES
    ${QTAPP_ROOT}/widgets/customtitlebar.h
    ${QTAPP_ROOT}/widgets/customtitlebar.cpp
  )
endif()

if(BUILD_DEBUG_PANE)
  include(${QTAPP_ROOT}/widgets/debugpane/CMakeLists.txt)

  list(APPEND SOURCES
    ${QTAPP_ROOT}/widgets/debugpane.h
    ${QTAPP_ROOT}/widgets/debugpane.cpp
    ${QTAPP_ROOT}/widgets/debugwidget.h
    ${QTAPP_ROOT}/widgets/debugwidget.cpp
    ${QTAPP_ROOT}/widgets/logwidget.h
    ${QTAPP_ROOT}/widgets/logwidget.cpp
    ${DEBUGPANE_SOURCES}
  )
endif()

# Add terminal pane for dev builds if QTermWidget is available
if(NOT TAU5_RELEASE_BUILD AND TERMINAL_PANE_AVAILABLE)
  list(APPEND SOURCES
    ${QTAPP_ROOT}/widgets/terminalpane.h
    ${QTAPP_ROOT}/widgets/terminalpane.cpp
  )
endif()

SET(RESOURCES
  ${QTAPP_ROOT}/Tau5.qrc
  ${QTAPP_ROOT}/images/app.icns
)

if(NOT BUILD_NODE_ONLY)
  if(APPLE)
    add_executable(${APP_NAME} MACOSX_BUNDLE ${SOURCES} ${RESOURCES})
    # Put the app bundle in bin/ directory for consistency
    set_target_properties(${APP_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
      BUNDLE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
  else()
    add_executable(${APP_NAME} WIN32 ${SOURCES} ${RESOURCES})
  endif()
endif()


if(NOT BUILD_NODE_ONLY AND APPLE)
  set_target_properties(
    ${APP_NAME} PROPERTIES
    OUTPUT_NAME "Tau5"
    MACOSX_RPATH TRUE
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME TRUE
    MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
    MACOSX_BUNDLE_GUI_IDENTIFIER "net.sonic-pi.app"
    MACOSX_BUNDLE_INFO_STRING "A code-based music creation and performance tool"
    MACOSX_BUNDLE_ICON_FILE "app.icns"
    MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    RESOURCE "images/app.icns"
    LINK_FLAGS "-framework AppKit"
    OUTPUT_NAME ${MACOS_APP_NAME}
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND macdeployqt "${CMAKE_BINARY_DIR}/bin/${MACOS_APP_NAME}.app"
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND plutil -replace NSHighResolutionCapable -bool true "${CMAKE_BINARY_DIR}/bin/${MACOS_APP_NAME}.app/Contents/Info.plist"
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND plutil -replace NSRequiresAquaSystemAppearance -bool false "${CMAKE_BINARY_DIR}/bin/${MACOS_APP_NAME}.app/Contents/Info.plist"
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND plutil -replace NSMicrophoneUsageDescription -string "Tau5 needs microphone access for its live audio input features." "${CMAKE_BINARY_DIR}/bin/${MACOS_APP_NAME}.app/Contents/Info.plist"
  )
endif()

if(NOT BUILD_NODE_ONLY)
  message(STATUS "App Root: ${APP_ROOT}")
  set_property(TARGET ${APP_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # Qt requires this?
endif()

if(NOT BUILD_NODE_ONLY)
  target_include_directories(${APP_NAME}
    PRIVATE
    ${QTAPP_ROOT}
    ${QTAPP_ROOT}/widgets
    ${QTAPP_ROOT}/lib
    ${QTAPP_ROOT}/styles
    ${CMAKE_BINARY_DIR}
    include)

  target_link_libraries(${APP_NAME}
    PRIVATE
    tau5_core  # Link to our static library
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Network
    Qt::Svg
    Qt::SvgWidgets
    Qt::WebEngineWidgets
    Qt::WebSockets)

  # Link QWindowKit for Windows/Linux
  if(NOT APPLE)
    target_link_libraries(${APP_NAME}
      PRIVATE
      QWindowKit::Widgets)
  endif()

  # Link QTermWidget for dev builds if available
  if(NOT TAU5_RELEASE_BUILD AND TERMINAL_PANE_AVAILABLE)
    target_link_libraries(${APP_NAME}
      PRIVATE
      qtermwidget6)
  endif()

  # Add compile definitions based on build options
  if(BUILD_DEBUG_PANE)
    target_compile_definitions(${APP_NAME} PRIVATE BUILD_WITH_DEBUG_PANE)
  endif()

  # Add terminal pane compile definition for dev builds
  if(NOT TAU5_RELEASE_BUILD AND TERMINAL_PANE_AVAILABLE)
    target_compile_definitions(${APP_NAME} PRIVATE BUILD_WITH_TERMINAL_PANE)
  endif()
  
  # Add server path definition
  target_compile_definitions(${APP_NAME} PRIVATE TAU5_SERVER_PATH_DEFAULT="${TAU5_SERVER_PATH_DEFAULT}")
endif()

# Build tau5-node CLI executable
add_executable(${NODE_APP_NAME} ${NODE_SOURCES})

set_property(TARGET ${NODE_APP_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${NODE_APP_NAME}
  PRIVATE
  ${QTAPP_ROOT}
  ${QTAPP_ROOT}/shared
  ${CMAKE_BINARY_DIR}
  include)

target_link_libraries(${NODE_APP_NAME}
  PRIVATE
  tau5_core
  Qt::Core
  Qt::Network)

# Configure tau5-node based on build context
if(NOT BUILD_NODE_ONLY AND TARGET ${APP_NAME})
  # Building with GUI - tau5-node is for the app bundle/release structure
  if(APPLE)
    # On macOS, don't output tau5-node to bin/ when building with GUI
    # It will only exist inside the app bundle
    set_target_properties(${NODE_APP_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/temp"
      RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/temp"
      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/temp"
      RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/temp"
      RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/temp"
    )
    
    # tau5-node in app bundle needs path to server
    # From Tau5.app/Contents/MacOS/tau5-node -> server
    target_compile_definitions(${NODE_APP_NAME} PRIVATE TAU5_SERVER_PATH_DEFAULT="${TAU5_SERVER_PATH_DEFAULT}")
    
    # Copy tau5-node into the app bundle
    add_custom_command(
      TARGET ${APP_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:${NODE_APP_NAME}>
              "${CMAKE_BINARY_DIR}/bin/${MACOS_APP_NAME}.app/Contents/MacOS/${NODE_APP_NAME}"
    )
    # Ensure tau5-node is built before tau5
    add_dependencies(${APP_NAME} ${NODE_APP_NAME})
  else()
    # Windows/Linux - tau5-node goes to bin/ alongside tau5
    # Use the configured server path
    target_compile_definitions(${NODE_APP_NAME} PRIVATE TAU5_SERVER_PATH_DEFAULT="${TAU5_SERVER_PATH_DEFAULT}")
  endif()
else()
  # Standalone tau5-node build (BUILD_NODE_ONLY=ON or no GUI target)
  # Use the configured server path
  target_compile_definitions(${NODE_APP_NAME} PRIVATE TAU5_SERVER_PATH_DEFAULT="${TAU5_SERVER_PATH_DEFAULT}")
endif()

# Deploy Qt binaries to the output on windows, and copy the CRT to the release folder
if(NOT BUILD_NODE_ONLY AND WIN32)
  # Visual Studio
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP true)
  include(InstallRequiredSystemLibraries)
  # Copy runtime libs to build directory
  file(COPY ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  # Run windeployqt to deploy Qt dependencies (required for release builds)
  find_program(WINDEPLOYQT windeployqt 
    HINTS 
      ${CMAKE_PREFIX_PATH}/bin 
      ${Qt6_DIR}/../../../bin
      $ENV{Qt6_DIR}/bin
      $ENV{QT_INSTALL_LOCATION}/bin
    REQUIRED)
  add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${WINDEPLOYQT} $<TARGET_FILE:${APP_NAME}>)
endif() # Win32

# Build options
option(BUILD_MCP_SERVER "Build the MCP DevTools server" OFF)
option(BUILD_DEBUG_PANE "Include debug pane in the build" ON)

# Build DevTools MCP server as a separate executable (optional)
if(BUILD_MCP_SERVER)
  add_subdirectory(spectra)
endif()

# Print build configuration summary
message(STATUS "")
message(STATUS "==============================================")
message(STATUS "Tau5 Build Configuration Summary")
message(STATUS "==============================================")
if(TAU5_RELEASE_BUILD)
  message(STATUS "Build Type: RELEASE/DISTRIBUTION")
  message(STATUS "Server Path: ${TAU5_SERVER_PATH_DEFAULT}")
  message(STATUS "Binaries will expect server at the configured path")
else()
  message(STATUS "Build Type: DEVELOPMENT")
  message(STATUS "Server Path: ${TAU5_SERVER_PATH_DEFAULT}")
  message(STATUS "Binaries will look for server in source tree")
endif()
message(STATUS "Optimization: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "==============================================")
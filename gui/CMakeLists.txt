cmake_minimum_required(VERSION 3.10)

message(STATUS "CMakeLists: Sonic Pi Tau5")

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13")
endif()

project("Tau5"
  LANGUAGES CXX C
  DESCRIPTION "Colloaborative Live Coding for Everyone"
  VERSION 5
  HOMEPAGE_URL "https://sonic-pi.net"
)

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(QTAPP_ROOT ${CMAKE_CURRENT_LIST_DIR})

# Set cmake prefix path so it can find the Qt libraries
if((${BUILD_32BIT}) AND(DEFINED ENV{QT_INSTALL_LOCATION32}))
  message(STATUS "Setting up a 32 bit Qt Build")
  message(STATUS "Setting prefix to user-defined Qt (32-bit) install location: $ENV{QT_INSTALL_LOCATION32}")
  set(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION32})
elseif(DEFINED ENV{QT_INSTALL_LOCATION})
  message(STATUS "Setting prefix to user-defined Qt install location: $ENV{QT_INSTALL_LOCATION}")
  set(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION})
endif()

# Qt Setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_TOOLS_DIR ${CMAKE_PREFIX_PATH}/bin)

set_property(GLOBAL PROPERTY AUTOMOC_FOLDER Automoc)
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Automoc)

find_package(Qt6 QUIET)
find_package(Qt6 COMPONENTS Core Widgets Gui Network Xml Svg SvgWidgets LinguistTools WebEngineWidgets WebSockets OpenGLWidgets REQUIRED)


set(APP_NAME tau5)
set(MACOS_APP_NAME "Tau5")

set(SOURCES
  ${QTAPP_ROOT}/main.cpp
  ${QTAPP_ROOT}/mainwindow.cpp
  ${QTAPP_ROOT}/mainwindow.h
  ${QTAPP_ROOT}/logger.h
  ${QTAPP_ROOT}/logger.cpp
  ${QTAPP_ROOT}/widgets/phxwebview.h
  ${QTAPP_ROOT}/widgets/phxwebview.cpp
  ${QTAPP_ROOT}/widgets/phxwidget.h
  ${QTAPP_ROOT}/widgets/phxwidget.cpp
  ${QTAPP_ROOT}/widgets/phxurlinterceptor.h
  ${QTAPP_ROOT}/widgets/phxurlinterceptor.cpp
  ${QTAPP_ROOT}/widgets/sandboxedwebview.h
  ${QTAPP_ROOT}/widgets/sandboxedwebview.cpp
  ${QTAPP_ROOT}/widgets/controllayer.h
  ${QTAPP_ROOT}/widgets/controllayer.cpp
  ${QTAPP_ROOT}/widgets/loadingoverlay.h
  ${QTAPP_ROOT}/widgets/loadingoverlay.cpp
  ${QTAPP_ROOT}/lib/beam.h
  ${QTAPP_ROOT}/lib/beam.cpp
  ${QTAPP_ROOT}/lib/fontloader.h
  ${QTAPP_ROOT}/lib/fontloader.cpp
  ${QTAPP_ROOT}/styles/StyleManager.h
  ${QTAPP_ROOT}/styles/StyleManager.cpp
  ${QTAPP_ROOT}/shortcuts/ShortcutManager.h
  ${QTAPP_ROOT}/shortcuts/ShortcutManager.cpp
)

# Conditionally add debug pane sources
if(BUILD_DEBUG_PANE)
  # Include the debugpane module
  include(${QTAPP_ROOT}/widgets/debugpane/CMakeLists.txt)
  
  list(APPEND SOURCES
    ${QTAPP_ROOT}/widgets/debugpane.h
    ${QTAPP_ROOT}/widgets/debugpane.cpp
    ${DEBUGPANE_SOURCES}
  )
endif()

SET(RESOURCES
  ${QTAPP_ROOT}/Tau5.qrc
  ${QTAPP_ROOT}/images/app.icns
)

# Create the Qt version of the app
if(APPLE)
  add_executable(${APP_NAME} MACOSX_BUNDLE ${SOURCES} ${RESOURCES})
else()
  add_executable(${APP_NAME} WIN32 ${SOURCES} ${RESOURCES}) # Win32 ignored on non-windows
endif()

# Set output directory to build root
set_target_properties(${APP_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(APPLE)
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
    COMMAND macdeployqt "${CMAKE_BINARY_DIR}/${MACOS_APP_NAME}.app"
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND plutil -replace NSHighResolutionCapable -bool true "${CMAKE_BINARY_DIR}/${MACOS_APP_NAME}.app/Contents/Info.plist"
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND plutil -replace NSRequiresAquaSystemAppearance -bool false "${CMAKE_BINARY_DIR}/${MACOS_APP_NAME}.app/Contents/Info.plist"
  )

  add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND plutil -replace NSMicrophoneUsageDescription -string "Sonic Pi Tau5 needs microphone access for its live audio input features." "${CMAKE_BINARY_DIR}/${MACOS_APP_NAME}.app/Contents/Info.plist"
  )
endif()

message(STATUS "App Root: ${APP_ROOT}")

set_property(TARGET ${APP_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # Qt requires this?

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
  Qt::Core
  Qt::Gui
  Qt::Widgets
  Qt::Network
  Qt::Svg
  Qt::SvgWidgets
  Qt::WebEngineWidgets
  Qt::WebSockets
  Qt::OpenGLWidgets)

# Add compile definitions based on build options
if(BUILD_DEBUG_PANE)
  target_compile_definitions(${APP_NAME} PRIVATE BUILD_WITH_DEBUG_PANE)
endif()

# Deploy Qt binaries to the output on windows, and copy the CRT to the release folder
if(WIN32)
  # Visual Studio
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP true)
  include(InstallRequiredSystemLibraries)
  # Copy runtime libs to build directory
  file(COPY ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  # Run winddeployqt if it can be found, to ensure installed dependencies
  add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_PREFIX_PATH}/bin/windeployqt $<TARGET_FILE:${APP_NAME}>)
endif() # Win32

# Build options
option(BUILD_MCP_SERVER "Build the MCP DevTools server" OFF)
option(BUILD_DEBUG_PANE "Include debug pane in the build" ON)

# Build DevTools MCP server as a separate executable (optional)
if(BUILD_MCP_SERVER)
  add_subdirectory(gui-dev-mcp)
endif()
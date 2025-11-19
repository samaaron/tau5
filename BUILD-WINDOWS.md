# Windows build instructions for Tau5

## Install Dependencies

Tau5 is known to work with the following dependencies:

1. Visual Studio 2022 with C++ development tools
2. Qt6 (see Qt Version Requirements below)
3. CMake (3.30+)
4. Elixir (1.17+)

### Qt Version Requirements

**Supported Qt versions:** 6.6.0 - 6.10.0
- **Minimum:** Qt 6.6.0 (required for tau5-gui)
- **Recommended:** Qt 6.10.0 (used in CI builds)
- **Maximum tested:** Qt 6.10.0

### Visual Studio

Install Visual Studio 2022 Community Edition or higher from https://visualstudio.microsoft.com/downloads/

During installation, make sure to select:
- Desktop development with C++
- Windows SDK (latest version)
- CMake tools for Windows (optional, if not installing CMake separately)

### Qt6

Install Qt via the Qt online installer from https://www.qt.io/download-open-source

In the "Select Components" window, make sure the following are checked (using version 6.10.0 or your chosen version between 6.6.0 and 6.10.0):

* Qt -> Qt 6.10.x -> MSVC 2022 64-bit
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt Positioning
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt WebChannel
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt WebEngine
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt WebSockets

Add Qt to your system PATH. For example, if you installed Qt to C:\Qt:
```cmd
setx PATH "%PATH%;C:\Qt\6.10.0\msvc2022_64\bin"
setx Qt6_DIR "C:\Qt\6.10.0\msvc2022_64\lib\cmake"
```

### CMake

Download and install CMake from https://cmake.org/download/

Choose "Add CMake to the system PATH" during installation.

### Elixir

Download and install Elixir from https://elixir-lang.org/install.html#windows

The Windows installer will also install Erlang/OTP which is required.

## Fetch Tau5

Clone the Tau5 repository to a directory of your choice (e.g., C:\Users\YourName\Development):

```cmd
cd C:\Users\YourName\Development
git clone https://github.com/samaaron/tau5
```

## Build Tau5

### Development Build

To build Tau5 in development mode:

```cmd
cd C:\Users\YourName\Development\tau5
bin\win\dev-build-all.bat
```

### Release Build

To create a release build:

```cmd
cd C:\Users\YourName\Development\tau5
bin\win\build-release.bat
```

## Run Tau5

### Development Mode

To run Tau5 in development mode with the GUI:

```cmd
cd C:\Users\YourName\Development\tau5
bin\win\dev-tau5-gui.bat
```

To run the headless node version:

```cmd
cd C:\Users\YourName\Development\tau5
bin\win\dev-tau5-node.bat
```

### Release Mode

After building a release, the executables will be in the `release\Tau5-for-Windows-*` directory:
- `tau5-gui.exe` - The full GUI application
- `tau5-node.exe` - The headless node version

## Troubleshooting

### Qt Version Mismatch
If you encounter errors about missing Qt methods (like `setOffTheRecord`), ensure you're using a Qt version within the supported range (6.6.0 - 6.10.0).

### Build Errors
Make sure you're using the "x64 Native Tools Command Prompt for VS 2022" or have run `vcvarsall.bat x64` to set up the Visual Studio environment variables.

### Missing Dependencies
The build scripts will check for required dependencies and provide error messages if something is missing.
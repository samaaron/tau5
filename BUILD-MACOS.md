# Build instructions for Sonic Pi Tau5

## Install deps

Tau5 is known to work with the following dependencies:

1. Xcode (15.2+) and command line tools
2. Qt6 (6.6.0 - 6.10.0, see Qt Version Requirements below)
3. CMake (4+)
4. Elixir(1.17+)

### Xcode

Open the App Store and install the latest Xcode (at least 15.2). Also install the command line tools which will give you access to a compiler necessary to build the app.

You'll know if you have things setup if you can run `gcc -v` from the terminal to print version information about gcc.

### Qt6

#### Qt Version Requirements

**Supported Qt versions:** 6.6.0 - 6.10.0
- **Minimum:** Qt 6.6.0 (required for tau5-gui)
- **Recommended:** Qt 6.10.0 (used in CI builds)
- **Maximum tested:** Qt 6.10.0

We maintain a narrow Qt version range to ensure compatibility and avoid deprecated API issues.

Install Qt via the Qt online installer. This can be found on Qt's open source development page - https://www.qt.io/download-open-source

In the "Select Components" window make sure the following are checked (replace 6.10.x with your chosen version between 6.6.0 and 6.10.0):

* Extensions -> Qt WebEngine
* Qt -> Qt 6.10.x -> Desktop
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt Positioning
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt WebChannel
* Qt -> Qt 6.10.x -> Additional Libraries -> Qt WebSockets

Update your `PATH` and `Qt6_DIR` environment variables. For example, if you're using zsh, add the following to your `~/.zshrc` (updating the version number to match the version you installed).

```
export PATH=~/Qt/6.10.0/macos/bin:$PATH
export Qt6_DIR=~/Qt/6.10.0/macos/lib/cmake
```

### CMake && Elixir

These can easily be downloaded via [Homebrew](https://brew.sh) if you're using it:

```
brew install cmake elixir
```

Spend a quick moment to test these. It's important that you can run both the `cmake` and `iex` commands from your terminal.


## Fetch Tau5

Find a nice directory to put Tau5 in. I typically use `~/Development`. Change into it and clone the Tau5 Github repository:

```
cd ~/Development
git clone https://github.com/samaaron/tau5
```

## Tau5 dev vs Tau5 release

Tau5 builds in two forms - development builds and release builds. Development builds have additional development tools - both built into the GUI and provided as services such as the MCP servers. The release builds are optimised for use rather than development.

## Build Tau5 dev

```
cd ~/Development/tau5
./bin/mac/dev-build-all.sh
```

This will then give you access to the dev GUI and dev node binaries. You can start them with the handy scripts:

GUI:

```
cd ~/Development/tau5
./bin/mac/tau5-gui-dev.sh
```

Node:

```
cd ~/Development/tau5
./bin/mac/tau5-node-dev.sh
```

## Build Tau5 release

```
cd ~/Development/tau5
./bin/mac/build-release.sh
```

## Run Tau5 in development mode

To run Tau5 in dev mode, just run the `tau5-gui-dev.sh` script

```
cd ~/Development/tau5
./bin/mac/tau5-gui-dev.sh
```



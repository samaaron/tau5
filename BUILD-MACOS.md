# Build instructions for Sonic Pi Tau5

## Install Deps

Tau5 is known to work with the following depencies:

1. Xcode (15.2+) and command line tools
2. Qt6 (6.8+)
3. CMake (3.30+)
4. Elixir(1.17+)

### Xcode

Open the App store and install the latest Xcode (at least 15.2). Also install the command line tools which will give you access to a compile necessy to build the app.

You'll know if you have things setup if you can run `gcc -v` from the terminal to print version information about gcc.

### Qt6

Install Qt via the Qt Online Installer. This can be found on Qt's Open Source Development page - https://www.qt.io/download-open-source

In the "Select Components" window make sure the following are checked:

* Extensions -> Qt WebEngine
* Qt -> Qt 6.8.x -> Desktop
* Qt -> Qt 6.8.x -> Additional Libraries -> Qt Positioning
* Qt -> Qt 6.8.x -> Additional Libraries -> Qt WebChannel

Update your `PATH` and `Qt6_DIR` environment variables. For example, if you're using zsh, add the following to your `~/.zshrc` (updating the version number to match the version you installed).

```
export PATH=~/Qt/6.7.3/macos/bin:$PATH
export Qt6_DIR=~/Qt/6.7.3/macos/lib/cmake
```

### Cmake && Elixir

These can easily be downloaded via [Homebrew](https://brew.sh) if you're using it:

```
brew install cmake elixir
```

Spend a quick moment to test these. It's important that you can run both the `cmake` and `iex` commands from your Terminal.


## Fetch Tau5

Find a nice directory to put Tau5 in. I typically use `~/Develolopment`. Change into it and clone the Tau5 github repository:

```
cd ~/Development
git clone https://github.com/samaaron/tau5
```

## Build Tau5

Now you just need to change into the `tau5` directory and run the build script:

```
cd ~/Development/tau5
./bin/mac/build.sh
```

## Run Tau5 in Development Mode

To run Tau5 in dev mode, just run the `tau5-dev.sh` script

```
cd ~/Development/tau5
./bin/mac/tau5.sh
```

## Run Tau5 in Production Mode

To run the Tau5 app in production mode, simply open the `Release` directory in the Finder:

```
open ~/Development/tau5/Release
```

then double click the `Tau5` app.


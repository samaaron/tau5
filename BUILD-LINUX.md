# Linux build instructions for Tau5

## Install Deps

Tau5 is known to work with the following depencies:

1. gcc or clang
2. Qt6 (see Qt Version Requirements below)
3. CMake (3.30+)
4. Elixir(1.17+)

To install dependencies:
- For full GUI build: `./bin/linux/install-gui-deps.sh`
- For headless/node only: `./bin/linux/install-node-deps.sh`

Both scripts cover Debian, Redhat, Arch and OpenSuse based distributions.

**Note, that you'll need to use `sudo` so like all other scripts that requre `sudo` that you didn't write yourself - definitely look through the file first to make sure you're happy.**

Once the dependencies are installed double check that your version of Elixir is recent enough. Unfortunately most linux distros don't yet ship with a sufficiently recent version of Elixir. If this is the case for you, you might want to try using [asdf](https://github.com/asdf-vm/asdf).

### Qt Version Requirements

**For tau5-gui (full GUI build):**
- Minimum: Qt 6.6.0
- Maximum tested: Qt 6.9.1
- Recommended: Qt 6.9.1 (used in CI for all GUI builds)

**For tau5-node (headless/node only):**
- Minimum: Qt 6.2.4 (available in Ubuntu 22.04 LTS repositories)
- Maximum tested: Qt 6.9.1

**Important Notes:**
- Ubuntu 22.04 LTS system packages provide Qt 6.2.4, which is only sufficient for tau5-node builds
- For GUI builds on Ubuntu 22.04, you must install Qt 6.6.0 or later from qt.io or use the install scripts





## Fetch Tau5

Find a nice directory to put Tau5 in. I typically use `~/Develolopment`. Change into it and clone the Tau5 github repository:

```
cd ~/Development
git clone https://github.com/samaaron/tau5
```

## Build Tau5Dev

To build Tau5 in dev mode (with developer tools and auto-loading phoenix server) you just need to change into the `tau5` directory and run the build script:

```
cd ~/Development/tau5
./bin/linux/dev-build-all.sh
```

## Run Tau5 in Development Mode

To run Tau5 in dev mode, just run the `tau5-gui-dev.sh` script

```
cd ~/Development/tau5
./bin/linux/tau5-gui-dev.sh
```





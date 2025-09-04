# Linux build instructions for Tau5

## Install Deps

Tau5 is known to work with the following depencies:

1. gcc or clang
2. Qt6 (6.8+)
3. CMake (3.30+)
4. Elixir(1.17+)

To install dependencies:
- For full GUI build: `./bin/linux/install-gui-deps.sh` 
- For headless/node only: `./bin/linux/install-node-deps.sh`

Both scripts cover Debian, Redhat, Arch and OpenSuse based distributions.

**Note, that you'll need to use `sudo` so like all other scripts that requre `sudo` that you didn't write yourself - definitely look through the file first to make sure you're happy.**

Once the dependencies are installed double check that your version of Elixir is recent enough. Unfortunately most linux distros don't yet ship with a sufficiently recent version of Elixir. If this is the case for you, you might want to try using [asdf](https://github.com/asdf-vm/asdf).

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
./bin/linux/dev-build-all.sh
```

## Run Tau5 in Development Mode

To run Tau5 in dev mode, just run the `tau5-dev.sh` script

```
cd ~/Development/tau5
./bin/linux/tau5.sh
```




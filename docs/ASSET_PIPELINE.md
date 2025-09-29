# Phoenix LiveView Asset Pipeline with esbuild and Tailwind

## Overview

The tau5 project uses the modern Phoenix 1.7+ asset pipeline which relies on **esbuild** for JavaScript bundling and **Tailwind CSS** for styling. This approach eliminates the need for Node.js package managers in production while providing a fast, efficient build system.

## Key Components

### 1. esbuild Configuration (`config/config.exs`)

```elixir
config :esbuild,
  version: "0.25.4",
  default: [
    args: ~w(
      js/app.js
      --bundle
      --splitting
      --format=esm
      --target=es2021
      --outdir=../priv/static/assets
      --public-path=/assets
      --loader:.css=css
      --loader:.woff2=file
      --loader:.woff=file
      --loader:.ttf=file
    ),
    cd: Path.expand("../assets", __DIR__),
    env: %{"NODE_PATH" => Path.expand("../deps", __DIR__)}
  ]
```

#### Configuration Breakdown:

- **`version`**: Specifies the esbuild version (managed by the Elixir wrapper)
- **`args`**: Command-line arguments passed to esbuild:
  - `--bundle`: Bundle all imports
  - `--splitting --format=esm`: Enable code splitting with ESM output
  - `--target=es2021`: Target modern browsers
  - `--outdir`: Where bundled assets are placed (`priv/static/assets`)
  - `--public-path=/assets`: Ensures chunk/worker URLs resolve correctly
  - `--loader`: Configure handling of CSS/fonts
- **`cd`**: Working directory (`assets`)
- **`env`**: Includes `NODE_PATH` so esbuild can resolve Phoenix JS deps shipped in `deps/`

### 2. Tailwind Configuration

```elixir
config :tailwind,
  version: "3.4.13",
  default: [
    args: ~w(
      --config=tailwind.config.js
      --input=css/app.css
      --output=../priv/static/assets/tailwind.css
    ),
    cd: Path.expand("../assets", __DIR__)
  ]
```

**Tailwind config example (`assets/tailwind.config.js`):**

```js
module.exports = {
  content: [
    "./js/**/*.{js,ts}",
    "../lib/tau5_web/**/*.{ex,heex}",
  ],
  safelist: [
    /phx-(no-feedback|click-loading|submit-loading|change-loading)/,
  ],
  theme: { extend: {} },
  plugins: [],
}
```

Include LiveView templates in `content` and safelist `phx-*` classes so interactive states aren’t purged in production.

### 3. Mix Aliases (`mix.exs`)

```elixir
defp aliases do
  [
    setup: ["deps.get", "assets.setup", "assets.build"],
    "assets.setup": ["tailwind.install --if-missing", "esbuild.install --if-missing"],
    "assets.build": [
      "tailwind default",
      "esbuild default"
    ],
    "assets.deploy": [
      "assets.setup",
      "tailwind default --minify",
      "esbuild default --minify",
      "phx.digest"
    ]
  ]
end
```

### 4. Development Watchers (`config/dev.exs`)

```elixir
config :tau5, Tau5Web.Endpoint,
  watchers: [
    esbuild: {Esbuild, :install_and_run, [:default, ~w(--sourcemap=inline --watch)]},
    tailwind: {Tailwind, :install_and_run, [:default, ~w(--watch)]}
  ]
```

### 5. Live Reload Configuration

```elixir
config :tau5, Tau5Web.Endpoint,
  live_reload: [
    patterns: [
      ~r"priv/static/(?!uploads/).*\.(js|css|png|jpeg|jpg|gif|svg|ico|webp|avif)$",
      ~r"lib/tau5_web/(controllers|live|components)/.*(ex|heex)$"
    ]
  ]
```

## Asset Directory Structure

```
server/
├── assets/
│   ├── js/
│   │   ├── app.js
│   │   └── lib/
│   ├── css/
│   │   └── app.css
│   ├── vendor/
│   └── tailwind.config.js
└── priv/
    └── static/
        └── assets/
            ├── app.js
            ├── tailwind.css
            └── js/
```

## How It Works

### Development Workflow

1. **Start the server**: `mix phx.server`
2. **Watchers activate**: esbuild and Tailwind start watching for changes
3. **Edit files**: Modify JS in `assets/js/` or CSS in `assets/css/`
4. **Automatic rebuild**: Watchers detect changes
5. **Live reload**: Browser refreshes with new assets

### Build Process

1. **JavaScript**: esbuild bundles `assets/js/app.js` + imports → `priv/static/assets/app.js`
2. **CSS**: Tailwind compiles `assets/css/app.css` → `priv/static/assets/tailwind.css`
3. **Static Files**: Phoenix serves from `priv/static/`, configured by `Plug.Static`

### Production Notes

- Run `mix assets.deploy` to build + minify + digest assets
- Enable gzip in your endpoint:

```elixir
plug Plug.Static,
  at: "/",
  from: :tau5,
  gzip: true,
  only: Tau5Web.static_paths()
```

## Working with Dependencies

### Vendored (recommended)

```js
import topbar from "../vendor/topbar";
```

### NPM (optional)

```bash
cd assets
npm install package-name
```

```js
import packageName from "package-name";
```

Tau5 vendors deps to avoid Node.js in production.

## Advanced Configurations

### Multiple esbuild Profiles

```elixir
config :esbuild,
  default: [...],
  monaco_worker: [
    args: ~w(
      vendor/monaco-editor/esm/vs/editor/editor.worker.js
      --bundle
      --target=es2021
      --outdir=../priv/static/assets/js/monaco-worker
      --public-path=/assets
    ),
    cd: Path.expand("../assets", __DIR__)
  ]
```

### Monaco Editor

- **Bundle workers separately** (as above).
- **Dynamic import** the editor:
  ```js
  const monaco = await import("../vendor/monaco-editor/esm/vs/editor/editor.main.js");
  ```
- **Worker location** (if you relocate files):
  ```js
  self.MonacoEnvironment = {
    getWorkerUrl: (_id, _label) => "/assets/js/monaco-worker/editor.worker.js"
  }
  ```
- **CSS collisions**: Import Monaco’s CSS after Tailwind resets or scope appropriately.

### Extra Loaders

If needed:

```
--loader:.wasm=file
--loader:.mp3=file
--loader:.wav=file
```

## Troubleshooting

### Assets not updating
1. Check watchers running
2. Verify file paths
3. Check browser console

### Styles missing in prod
- Confirm Tailwind `content` includes `lib/*_web/**/*.heex`
- Ensure `phx-*` classes are safelisted

### Module not found
- Fix import paths
- Use relative for vendored deps
- Check `NODE_PATH` for Phoenix deps

### Production build issues
- Run `mix assets.deploy` locally
- Check `priv/static/` has outputs
- Verify `phx.digest` ran
- Ensure `Plug.Static` is serving with gzip

## Key Takeaways

1. **No Node.js required in production**
2. **Fast builds** with esbuild
3. **Automatic in dev** via watchers
4. **Tailwind safelist is critical** for LiveView classes
5. **Flexible**: vendored deps, multiple profiles, Monaco support

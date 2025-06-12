defmodule Tau5.MixProject do
  use Mix.Project

  def project do
    [
      app: :tau5,
      version: "0.1.0",
      elixir: "~> 1.14",
      elixirc_paths: elixirc_paths(Mix.env()),
      start_permanent: Mix.env() == :prod,
      aliases: aliases(),
      deps: deps()
    ]
  end

  # Configuration for the OTP application.
  #
  # Type `mix help compile.app` for more information.
  def application do
    [
      mod: {Tau5.Application, []},
      extra_applications: [:logger, :runtime_tools]
    ]
  end

  # Specifies which paths to compile per environment.
  defp elixirc_paths(:test), do: ["lib", "test/support"]
  defp elixirc_paths(_), do: ["lib"]

  # Specifies your project dependencies.
  #
  # Type `mix help deps` for examples and options.
  defp deps do
    [
      {:phoenix, "~> 1.7"},
      {:phoenix_html, "~> 4.2"},
      {:phoenix_live_reload, "~> 1.6", only: :dev},
      {:phoenix_live_view, "~> 1.0"},
      {:floki, ">= 0.30.0", only: :test},
      {:phoenix_live_dashboard, "0.8.7"},
      {:esbuild, "~> 0.9", runtime: Mix.env() == :dev},
      {:tailwind, "~> 0.3", runtime: Mix.env() == :dev},
      {:heroicons,
       github: "tailwindlabs/heroicons",
       tag: "v2.2.0",
       sparse: "optimized",
       app: false,
       compile: false,
       depth: 1},
      {:swoosh, "~> 1.5"},
      {:finch, "~> 0.13"},
      {:telemetry_metrics, "~> 1.1"},
      {:telemetry_poller, "~> 1.2"},
      {:gettext, "~> 0.20"},
      {:jason, "~> 1.4"},
      {:bandit, "~> 1.6"},
      {:uuid, "~> 1.1"},
      {:net_address, "~> 0.3"},
      {:exqlite, "~> 0.27"},
      {:sp_midi,
       git: "https://github.com/sonic-pi-net/sp_midi.git",
       ref: "ed3cd8f74ac5ead3e88cd1a1744e45682319bae0",
       app: false,
       compile: false},
      {:sp_link,
       git: "https://github.com/sonic-pi-net/sp_link.git",
       ref: "e83df382e757c4da965869d8a7758e7d4b1ef0c7",
       app: false,
       compile: false},
      {:tau5_discovery,
       git: "https://github.com/samaaron/tau5_discovery.git",
       ref: "0edb982c1b4d4b6c12284f6d326a4a17fb39aa12",
       app: false,
       compile: false}
    ]
  end

  # Aliases are shortcuts or tasks specific to the current project.
  # For example, to install project dependencies and perform other setup tasks, run:
  #
  #     $ mix setup
  #
  # See the documentation for `Mix` for more info on aliases.
  defp aliases do
    [
      clean: ["clean", "nifs.clean"],
      setup: ["deps.get", "assets.setup", "assets.build", "nifs.compile"],
      "assets.setup": ["tailwind.install --if-missing", "esbuild.install --if-missing"],
      "assets.build": ["tailwind default", "esbuild default", "esbuild monaco_worker"],
      "assets.deploy": [
        "assets.setup",
        "tailwind default --minify",
        "esbuild default --minify",
        "esbuild monaco_worker --minify",
        "phx.digest"
      ]
    ]
  end
end

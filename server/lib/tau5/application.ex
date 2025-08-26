defmodule Tau5.Application do
  use Application
  require Logger

  @impl true
  def start(_type, _args) do
    if System.get_env("TAU5_USE_STDIN_CONFIG") == "true" do
      case Tau5.SecureConfig.read_stdin_config() do
        {:ok, secrets} when map_size(secrets) > 0 -> 
          Application.put_env(:tau5, :session_token, secrets.session_token)
          Application.put_env(:tau5, :heartbeat_token, secrets.heartbeat_token)
          Application.put_env(:tau5, :heartbeat_port, secrets.heartbeat_port)
          
          endpoint_config = Application.get_env(:tau5, Tau5Web.Endpoint)
          new_endpoint_config = Keyword.put(endpoint_config, :secret_key_base, secrets.secret_key_base)
          Application.put_env(:tau5, Tau5Web.Endpoint, new_endpoint_config)
          
        {:ok, _} ->
          :ok
          
        :error ->
          Logger.error("Failed to read secure config from stdin, halting")
          System.halt(1)
      end
    end
    
    http_port = Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port]

    children = [
      Tau5.ConfigRepo,
      Tau5.ConfigRepoMigrator,
      Tau5Web.Telemetry,
      {Phoenix.PubSub, name: Tau5.PubSub},
      {Finch, name: Tau5.Finch},
      Hermes.Server.Registry,
      {Tau5MCP.Server, transport: :streamable_http},
      Tau5Web.Endpoint,
      # Kill switch must be temporary - ensures system dies if kill switch fails
      %{
        id: Tau5.KillSwitch,
        start: {Tau5.KillSwitch, :start_link, [[]]},
        restart: :temporary
      },
      Tau5.Heartbeat,
      Tau5.Link,
      Tau5.MIDI,
      {Tau5.Discovery, %{http_port: http_port}}
    ]

    opts = [strategy: :one_for_one, name: Tau5.Supervisor]

    case Supervisor.start_link(children, opts) do
      {:ok, pid} ->
        # Initialize MCP activity logger
        Tau5MCP.ActivityLogger.init()

        # Print ASCII art directly to stdio without any formatting/prefixes
        # Using explicit string concatenation to preserve exact spacing
        ascii_art =
          "\n" <>
            "                           ╘\n" <>
            "                    ─       ╛▒╛\n" <>
            "                     ▐╫       ▄█├\n" <>
            "              ─╟╛      █▄      ╪▓▀\n" <>
            "    ╓┤┤┤┤┤┤┤┤┤  ╩▌      ██      ▀▓▌\n" <>
            "     ▐▒   ╬▒     ╟▓╘    ─▓█      ▓▓├\n" <>
            "     ▒╫   ▒╪      ▓█     ▓▓─     ▓▓▄\n" <>
            "    ╒▒─  │▒       ▓█     ▓▓     ─▓▓─\n" <>
            "    ╬▒   ▄▒ ╒    ╪▓═    ╬▓╬     ▌▓▄\n" <>
            "    ╥╒   ╦╥     ╕█╒    ╙▓▐     ▄▓╫\n" <>
            "               ▐╩     ▒▒      ▀▀\n" <>
            "                    ╒╪      ▐▄\n" <>
            "\n" <>
            "        ______           ______\n" <>
            "       /_  __/___  __  _/ ____/\n" <>
            "        / / / __ `/ / / /___ \\\n" <>
            "       / / / /_/ / /_/ /___/ /\n" <>
            "      /_/  \\__,_/\\__,_/_____/\n" <>
            "\n" <>
            "     Collaborative Live Coding\n" <>
            "           for Everyone\n\n"

        :ok = :io.put_chars(:standard_io, ascii_art)

        Logger.info(
          "[TAU5 SERVER READY] - Elixir OTP supervision tree started with opts: #{inspect(opts)}"
        )

        {:ok, pid}

      error ->
        error
    end
  end

  @impl true
  def config_change(changed, _new, removed) do
    Tau5Web.Endpoint.config_change(changed, removed)
    :ok
  end
end

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

          endpoint_config = Application.get_env(:tau5, Tau5Web.Endpoint)

          new_endpoint_config =
            endpoint_config
            |> Keyword.put(:secret_key_base, secrets.secret_key_base)

          Application.put_env(:tau5, Tau5Web.Endpoint, new_endpoint_config)

        {:ok, _} ->
          :ok

        :error ->
          Logger.error("Failed to read secure config from stdin, halting")
          System.halt(1)
      end
    end

    http_port = 0

    public_endpoint_enabled = Application.get_env(:tau5, :public_endpoint_enabled, false)
    public_port = Application.get_env(:tau5, :public_port, 0)

    friend_mode = Application.get_env(:tau5, :friend_mode_enabled, false)
    friend_token = Application.get_env(:tau5, :friend_token)

    if friend_mode do
      Logger.info(
        "Friend mode enabled with token: #{if friend_token, do: "[CONFIGURED]", else: "[NOT SET]"}"
      )
    end

    public_endpoint_port =
      if public_port > 0 or public_endpoint_enabled do
        case Tau5.PortFinder.configure_endpoint_port(Tau5Web.PublicEndpoint) do
          {:ok, port} ->
            Logger.info("PublicEndpoint: Successfully configured on port #{port}")
            port

          {:error, reason} ->
            Logger.warning("PublicEndpoint: Could not configure port: #{inspect(reason)}")
            nil
        end
      else
        nil
      end

    should_start_public_endpoint = public_endpoint_port != nil

    base_children = [
      Tau5.ConfigRepo,
      Tau5.ConfigRepoMigrator,
      Tau5Web.Telemetry,
      {Phoenix.PubSub, name: Tau5.PubSub},
      {Finch, name: Tau5.Finch},
      Hermes.Server.Registry,
      {Tau5MCP.Server, transport: :streamable_http}
    ]

    local_endpoint_children =
      if System.get_env("TAU5_NO_LOCAL_ENDPOINT") == "true" do
        Logger.info("Local endpoint disabled via TAU5_NO_LOCAL_ENDPOINT")
        []
      else
        [Tau5Web.Endpoint]
      end

    public_endpoint_children =
      if should_start_public_endpoint do
        [
          Tau5Web.PublicEndpoint,
          {Tau5.PublicEndpoint, [enabled: public_endpoint_enabled]}
        ]
      else
        []
      end

    mcp_endpoint_children =
      if Application.get_env(:tau5, :mcp_enabled, true) do
        [Tau5Web.MCPEndpoint]
      else
        []
      end

    heartbeat_children =
      if heartbeat_enabled?() do
        [
          %{
            id: Tau5.KillSwitch,
            start: {Tau5.KillSwitch, :start_link, [[]]},
            restart: :temporary
          },
          Tau5.Heartbeat
        ]
      else
        []
      end

    additional_children =
      heartbeat_children ++
        [
          Tau5.Link,
          Tau5.MIDI,
          {Tau5.Discovery, %{http_port: http_port}}
        ]

    children =
      base_children ++
        local_endpoint_children ++
        public_endpoint_children ++ mcp_endpoint_children ++ additional_children

    opts = [strategy: :one_for_one, name: Tau5.Supervisor]

    case Supervisor.start_link(children, opts) do
      {:ok, pid} ->
        Tau5MCP.ActivityLogger.init()

        if System.get_env("TAU5_TIDEWAVE_ENABLED", "false") in ["1", "true", "yes"] do
          TidewaveMCP.ActivityLogger.init()
        end

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
            "        Code. Art. Together.\n\n"

        :ok = :io.put_chars(:standard_io, ascii_art)

        Task.start(fn -> Tau5.StartupInfo.report_server_info() end)

        Logger.info(
          "[TAU5 SERVER READY] - Elixir OTP supervision tree started with opts: #{inspect(opts)}"
        )

        {:ok, pid}

      {:error, {:shutdown, {:failed_to_start_child, child, {:shutdown, {:failed_to_start_child, sub_child, :eaddrinuse}}}}} ->
        # Port already in use error during endpoint startup
        endpoint_name = extract_endpoint_name(child, sub_child)
        Tau5.StartupInfo.report_startup_error(
          "#{endpoint_name}: port already in use"
        )

      {:error, {:shutdown, {:failed_to_start_child, child, :eaddrinuse}}} ->
        # Direct port conflict
        child_name = 
          case child do
            Tau5.Heartbeat -> "Heartbeat server"
            atom when is_atom(atom) -> inspect(atom) |> String.replace("Elixir.", "")
            _ -> inspect(child)
          end
        
        Tau5.StartupInfo.report_startup_error(
          "#{child_name}: port already in use"
        )

      {:error, {:shutdown, {:failed_to_start_child, child, error}}} ->
        # Other startup errors
        child_name = 
          case child do
            atom when is_atom(atom) -> inspect(atom) |> String.replace("Elixir.", "")
            _ -> inspect(child)
          end
        
        error_message = 
          case error do
            {:shutdown, details} -> "startup failed: #{inspect(details)}"
            _ -> inspect(error)
          end
        
        Tau5.StartupInfo.report_startup_error(
          "#{child_name}: #{error_message}"
        )

      error ->
        # Catch-all for any other errors
        Tau5.StartupInfo.report_startup_error(
          "Application startup failed: #{inspect(error)}"
        )
    end
  end

  @impl true
  def config_change(changed, _new, removed) do
    Tau5Web.Endpoint.config_change(changed, removed)
    :ok
  end

  defp extract_endpoint_name(child, sub_child) do
    # Try to extract a meaningful name from the nested error structure
    endpoint_name = 
      case {child, sub_child} do
        {Tau5Web.Endpoint, _} -> "Local Endpoint"
        {Tau5Web.PublicEndpoint, _} -> "Public Endpoint"
        {Tau5Web.MCPEndpoint, _} -> "MCP Endpoint"
        {_, {name, _}} when is_atom(name) -> inspect(name)
        {_, name} when is_atom(name) -> inspect(name)
        _ -> "#{inspect(child)} (#{inspect(sub_child)})"
      end
    
    endpoint_name
  end

  defp heartbeat_enabled? do
    case System.get_env("TAU5_HEARTBEAT_ENABLED") do
      "true" -> true
      "false" -> false
      _ -> Application.get_env(:tau5, :heartbeat_enabled, true)
    end
  end
end

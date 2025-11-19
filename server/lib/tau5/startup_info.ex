defmodule Tau5.StartupInfo do
  @moduledoc """
  Reports server startup information to the parent process (GUI/tau5-node).
  This is done as a separate task after the supervision tree starts to ensure
  all endpoints are properly initialized and have their actual ports allocated.
  """

  require Logger

  def report_server_info() do
    http_port = get_http_port()

    # Wait for Phoenix to actually be ready to serve requests before signaling GUI
    if http_port > 0 do
      wait_for_http_ready(http_port)
    end

    pid = System.pid()
    heartbeat_port = Tau5.Heartbeat.get_port()
    mcp_port = get_mcp_port()

    IO.puts(
      "[TAU5_SERVER_INFO:PID=#{pid},HTTP_PORT=#{http_port},HEARTBEAT_PORT=#{heartbeat_port},MCP_PORT=#{mcp_port}]"
    )

    Logger.info(
      "Server started - PID: #{pid}, HTTP: #{http_port}, Heartbeat: #{heartbeat_port}, MCP: #{mcp_port}"
    )
  end

  defp wait_for_http_ready(port, attempt \\ 1) do
    # Request the health endpoint to verify Phoenix is ready
    # Include session token to bypass InternalEndpointSecurity
    token = Application.get_env(:tau5, :session_token)
    url = if token do
      ~c"http://127.0.0.1:#{port}/health?token=#{token}"
    else
      ~c"http://127.0.0.1:#{port}/health"
    end

    # Use full response format and disable automatic redirect following
    http_options = [{:timeout, 5000}, {:connect_timeout, 2000}, {:autoredirect, false}]
    options = [{:body_format, :binary}]

    result = :httpc.request(:get, {url, []}, http_options, options)

    case result do
      {:ok, {{_, 200, _}, _, _}} ->
        if attempt > 1, do: IO.write(" ready\n")
        Logger.debug("Phoenix ready after #{attempt * 100}ms")
        :ok

      {:error, reason} when attempt < 80 ->
        # Show progress dots
        if attempt == 1, do: IO.write("Waiting for Phoenix")
        if rem(attempt, 10) == 0 do
          Logger.debug("Health check error (attempt #{attempt}): #{inspect(reason)}")
        end
        IO.write(".")

        # Wait 100ms and retry (max 8 seconds total)
        Process.sleep(100)
        wait_for_http_ready(port, attempt + 1)

      other when attempt < 80 ->
        # Show progress dots for unexpected responses
        if attempt == 1, do: IO.write("Waiting for Phoenix")
        if rem(attempt, 10) == 0 do
          Logger.debug("Health check unexpected response (attempt #{attempt}): #{inspect(other)}")
        end
        IO.write(".")

        # Wait 100ms and retry (max 8 seconds total)
        Process.sleep(100)
        wait_for_http_ready(port, attempt + 1)

      _ ->
        IO.write(" timeout\n")
        Logger.warning("Phoenix not responding after 8 seconds, last result: #{inspect(result)}")
        :ok
    end
  end

  @doc """
  Reports a startup error to the parent process and then halts.
  """
  def report_startup_error(message) do
    IO.puts("[TAU5_SERVER_ERROR:#{message}]")
    Logger.error("Server startup failed: #{message}")
    System.halt(1)
  end

  defp get_http_port do
    if System.get_env("TAU5_NO_LOCAL_ENDPOINT") == "true" do
      0
    else
      get_actual_server_port()
    end
  end

  defp get_mcp_port do
    if mcp_enabled?() do
      case System.get_env("TAU5_MCP_PORT") do
        port_str when is_binary(port_str) ->
          case Integer.parse(port_str) do
            {port, ""} -> port
            _ -> Application.get_env(:tau5, :mcp_port, 5555)
          end

        _ ->
          Application.get_env(:tau5, :mcp_port, 5555)
      end
    else
      0
    end
  end

  defp mcp_enabled? do
    case System.get_env("TAU5_MCP_ENABLED") do
      "true" -> true
      "false" -> false
      _ -> Application.get_env(:tau5, :mcp_enabled, true)
    end
  end

  defp get_actual_server_port do
    case Tau5Web.Endpoint.server_info(:http) do
      {:ok, {_address, port}} when is_integer(port) and port > 0 ->
        Logger.debug("Got actual port from server_info: #{port}")
        port

      {:error, reason} ->
        Logger.warning("Could not get server info: #{inspect(reason)}")
        get_configured_port()

      other ->
        Logger.warning("Unexpected server_info response: #{inspect(other)}")
        get_configured_port()
    end
  end

  defp get_configured_port do
    case Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port] do
      0 ->
        Logger.warning("Could not determine actual allocated port")
        0

      port when is_integer(port) ->
        port

      _ ->
        0
    end
  end
end

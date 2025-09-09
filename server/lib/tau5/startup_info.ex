defmodule Tau5.StartupInfo do
  @moduledoc """
  Reports server startup information to the parent process (GUI/tau5-node).
  This is done as a separate task after the supervision tree starts to ensure
  all endpoints are properly initialized and have their actual ports allocated.
  """

  require Logger

  def report_server_info() do
    Process.sleep(1000)

    pid = System.pid()
    http_port = get_http_port()
    heartbeat_port = Tau5.Heartbeat.get_port()
    mcp_port = get_mcp_port()

    IO.puts(
      "[TAU5_SERVER_INFO:PID=#{pid},HTTP_PORT=#{http_port},HEARTBEAT_PORT=#{heartbeat_port},MCP_PORT=#{mcp_port}]"
    )

    Logger.info(
      "Server started - PID: #{pid}, HTTP: #{http_port}, Heartbeat: #{heartbeat_port}, MCP: #{mcp_port}"
    )
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

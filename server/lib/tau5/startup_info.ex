defmodule Tau5.StartupInfo do
  @moduledoc """
  Reports server startup information to the parent process (GUI/tau5-node).
  This is done as a separate task after the supervision tree starts to ensure
  all endpoints are properly initialized and have their actual ports allocated.
  """

  require Logger

  def report_server_info do
    # Longer delay to ensure endpoint is fully started and port is bound
    Process.sleep(1000)

    pid = System.pid()

    # Check if local endpoint is disabled
    if System.get_env("TAU5_NO_LOCAL_ENDPOINT") == "true" do
      # Report with port 0 to indicate no local endpoint
      IO.puts("[TAU5_SERVER_INFO:PID=#{pid},PORT=0]")
      Logger.info("Server started - PID: #{pid}, No local endpoint")
    else
      configured_port = Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port]
      Logger.debug("Configured port: #{configured_port}")

      port = get_actual_server_port()

      # Send both PID and port in a single atomic message
      # Format: [TAU5_SERVER_INFO:PID=12345,PORT=4000]
      IO.puts("[TAU5_SERVER_INFO:PID=#{pid},PORT=#{port}]")
      Logger.info("Server started - PID: #{pid}, Port: #{port}")
    end
  end

  defp get_actual_server_port do
    # Get the actual port from the running endpoint using Phoenix's server_info/1
    # This is the idiomatic way to get the actual bound port

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
        # Port 0 means random allocation, but we couldn't get the actual port
        Logger.warning("Could not determine actual allocated port")
        0

      port when is_integer(port) ->
        port

      _ ->
        0
    end
  end
end

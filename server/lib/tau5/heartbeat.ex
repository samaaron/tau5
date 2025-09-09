defmodule Tau5.Heartbeat do
  @moduledoc """
  UDP heartbeat handler - receives heartbeats and resets the kill switch.
  This module ONLY handles UDP communication and validation.
  The actual kill mechanism is in Tau5.KillSwitch.
  """
  use GenServer
  require Logger

  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts, name: __MODULE__)
  end

  @doc """
  Returns the allocated heartbeat port, or 0 if disabled.
  """
  def get_port do
    if Process.whereis(__MODULE__) do
      GenServer.call(__MODULE__, :get_port)
    else
      0
    end
  end

  def init(_opts) do
    if heartbeat_enabled?() do
      # Allow manual override via environment variable
      requested_port = 
        case System.get_env("TAU5_HEARTBEAT_PORT") do
          nil -> 0
          "" -> 0
          port_str ->
            case Integer.parse(port_str) do
              {port, ""} when port >= 0 and port <= 65535 -> port
              _ ->
                Logger.warning("Invalid TAU5_HEARTBEAT_PORT: #{port_str}, using random allocation")
                0
            end
        end

      {socket, port} =
        case :gen_udp.open(requested_port, [
               :binary,
               {:active, true},
               {:ip, {127, 0, 0, 1}}
             ]) do
          {:ok, sock} ->
            case :inet.port(sock) do
              {:ok, p} ->
                Logger.debug("Heartbeat GenServer opened UDP socket on port #{p}")
                {sock, p}

              {:error, reason} ->
                :gen_udp.close(sock)
                Tau5.StartupInfo.report_startup_error(
                  "Failed to get UDP port: #{inspect(reason)}"
                )
            end

          {:error, :eaddrinuse} when requested_port > 0 ->
            Tau5.StartupInfo.report_startup_error(
              "Heartbeat port #{requested_port} is already in use. Another Tau5 instance may be running with the same heartbeat port."
            )

          {:error, reason} ->
            Tau5.StartupInfo.report_startup_error(
              "Failed to open UDP socket on port #{requested_port}: #{inspect(reason)}"
            )
        end

      token =
        Application.get_env(:tau5, :heartbeat_token) ||
          System.get_env("TAU5_HEARTBEAT_TOKEN", "")

      if token == "" do
        Tau5.StartupInfo.report_startup_error(
          "TAU5_HEARTBEAT_TOKEN missing or empty - heartbeat security requires a valid token"
        )
      end

      Logger.info("UDP heartbeat listener started on port #{port}")
      Logger.debug("Heartbeat token configured: #{String.slice(token, 0..7)}...")

      {:ok,
       %{
         socket: socket,
         port: port,
         token: token,
         valid_count: 0,
         invalid_count: 0
       }}
    else
      Logger.info("Heartbeat monitoring disabled")
      {:ok, %{enabled: false}}
    end
  end

  def handle_info({:udp, _socket, _ip, _port, data}, state) do
    case String.trim(data) do
      "HEARTBEAT:" <> token when token == state.token ->
        Tau5.KillSwitch.reset()

        if state.valid_count == 0 do
          Logger.info("First heartbeat received - kill switch reset")
        end

        {:noreply, %{state | valid_count: state.valid_count + 1}}

      "HEARTBEAT:" <> _wrong_token ->
        Logger.warning("Received heartbeat with wrong token")
        {:noreply, %{state | invalid_count: state.invalid_count + 1}}

      other ->
        Logger.warning("Received unexpected UDP data: #{inspect(other)}")
        {:noreply, %{state | invalid_count: state.invalid_count + 1}}
    end
  end

  def handle_info({:udp_error, _socket, reason}, _state) do
    Logger.error("FATAL: UDP socket error: #{inspect(reason)}")
    Logger.error("Shutting down due to UDP error")
    System.halt(0)
  end

  def handle_info(msg, state) do
    Logger.warning("Heartbeat GenServer received unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  def handle_call(:get_port, _from, state) do
    port = Map.get(state, :port, 0)
    {:reply, port, state}
  end

  defp heartbeat_enabled? do
    case System.get_env("TAU5_HEARTBEAT_ENABLED") do
      "true" -> true
      "false" -> false
      _ -> Application.get_env(:tau5, :heartbeat_enabled, true)
    end
  end
end

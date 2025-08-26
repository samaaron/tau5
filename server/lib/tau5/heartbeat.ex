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

  def init(_opts) do
    if heartbeat_enabled?() do
      case open_udp_socket() do
        {:ok, socket} ->
          token = Application.get_env(:tau5, :heartbeat_token) || System.get_env("TAU5_HEARTBEAT_TOKEN", "")
          if token == "" do
            Logger.error("FATAL: TAU5_HEARTBEAT_TOKEN missing or empty - refusing to start")
            Logger.error("Heartbeat security requires a valid token")
            System.halt(0)
          end
          
          Logger.info("UDP heartbeat listener started successfully")
          send_pid_to_gui()
          
          {:ok, %{
            socket: socket,
            token: token,
            valid_count: 0,
            invalid_count: 0
          }}
        
        {:error, reason} ->
          Logger.error("FATAL: Cannot bind to UDP heartbeat port: #{inspect(reason)}")
          Logger.error("Shutting down immediately - heartbeat setup failed")
          System.halt(0)
      end
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
        else
          Logger.debug("Heartbeat #{state.valid_count + 1} - kill switch reset")
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

  defp open_udp_socket do
    port_str = Application.get_env(:tau5, :heartbeat_port) || System.get_env("TAU5_HEARTBEAT_PORT")
    
    if port_str do
      case Integer.parse(port_str) do
        {port, ""} ->
          Logger.info("Opening UDP heartbeat listener on port #{port}")
          
          # Bind to IPv4 loopback only for security
          case :gen_udp.open(port, [:binary, {:active, true}, {:reuseaddr, true}, {:ip, {127,0,0,1}}]) do
            {:ok, socket} ->
              case :inet.port(socket) do
                {:ok, actual_port} ->
                  Logger.info("UDP socket successfully bound to port #{actual_port}")
                  {:ok, socket}
                {:error, reason} ->
                  Logger.error("Failed to verify UDP socket port: #{inspect(reason)}")
                  {:error, reason}
              end
            
            {:error, reason} = error ->
              Logger.error("Failed to bind to UDP port #{port}: #{inspect(reason)}")
              error
          end
        
        _ ->
          Logger.error("Invalid TAU5_HEARTBEAT_PORT: #{port_str}")
          {:error, :invalid_port}
      end
    else
      Logger.error("TAU5_HEARTBEAT_PORT not set - GUI did not provide heartbeat port")
      {:error, :no_port}
    end
  end

  defp send_pid_to_gui do
    pid = System.pid()
    IO.puts("[TAU5_BEAM_PID:#{pid}]")
    Logger.info("Sent BEAM PID #{pid} to GUI")
  end

  defp heartbeat_enabled? do
    # Check environment variable first, then application config
    case System.get_env("TAU5_HEARTBEAT_ENABLED") do
      "true" -> true
      "false" -> false
      _ -> Application.get_env(:tau5, :heartbeat_enabled, true)
    end
  end
end
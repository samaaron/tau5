defmodule Tau5.Heartbeat do
  use GenServer
  require Logger

  @heartbeat_timeout 10_000
  @check_interval 1_000
  @heartbeat_command "TAU5_HEARTBEAT"

  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts, name: __MODULE__)
  end

  def init(_opts) do
    send_pid_to_gui()
    start_stdin_reader()
    Process.send_after(self(), :check_heartbeat, @check_interval)
    
    {:ok, %{last_heartbeat: System.monotonic_time(:millisecond)}}
  end

  def beat do
    GenServer.cast(__MODULE__, :heartbeat)
  end

  def handle_cast(:heartbeat, state) do
    {:noreply, %{state | last_heartbeat: System.monotonic_time(:millisecond)}}
  end

  def handle_info(:check_heartbeat, state) do
    current_time = System.monotonic_time(:millisecond)
    time_since_last = current_time - state.last_heartbeat

    if time_since_last > @heartbeat_timeout do
      Logger.warning("No heartbeat received for #{time_since_last}ms. Shutting down...")
      Process.sleep(100)
      System.halt(0)
    else
      Process.send_after(self(), :check_heartbeat, @check_interval)
    end

    {:noreply, state}
  end

  defp send_pid_to_gui do
    pid = System.pid()
    IO.puts("[TAU5_BEAM_PID:#{pid}]")
    Logger.info("Sent BEAM PID #{pid} to GUI")
  end

  defp start_stdin_reader do
    Task.start(fn ->
      IO.stream(:stdio, :line)
      |> Stream.map(&String.trim/1)
      |> Stream.filter(fn line -> 
        line == @heartbeat_command
      end)
      |> Stream.each(fn _line ->
        Tau5.Heartbeat.beat()
      end)
      |> Stream.run()
    end)
  end
end
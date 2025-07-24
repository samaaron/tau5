defmodule Tau5.Heartbeat do
  use GenServer
  require Logger

  @initial_grace_period 40_000
  @check_interval 10_000
  @max_missed_heartbeats 4
  @heartbeat_command "TAU5_HEARTBEAT"

  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts, name: __MODULE__)
  end

  def init(_opts) do
    if heartbeat_enabled?() do
      send_pid_to_gui()
      start_stdin_reader()
      # Start checking after initial grace period
      Process.send_after(self(), :start_checking, @initial_grace_period)

      {:ok,
       %{
         last_heartbeat: System.monotonic_time(:millisecond),
         enabled: true,
         checking: false,
         missed_heartbeats: 0
       }}
    else
      Logger.info("Heartbeat monitoring disabled")
      {:ok, %{enabled: false}}
    end
  end

  def beat do
    GenServer.cast(__MODULE__, :heartbeat)
  end

  def handle_cast(:heartbeat, %{enabled: false} = state) do
    {:noreply, state}
  end

  def handle_cast(:heartbeat, state) do
    # Reset missed heartbeats counter when we receive a heartbeat
    {:noreply,
     %{state | last_heartbeat: System.monotonic_time(:millisecond), missed_heartbeats: 0}}
  end

  def handle_info(:start_checking, state) do
    Logger.info("Starting heartbeat monitoring after grace period")
    Process.send_after(self(), :check_heartbeat, @check_interval)
    {:noreply, %{state | checking: true}}
  end

  def handle_info(:check_heartbeat, %{enabled: false} = state) do
    {:noreply, state}
  end

  def handle_info(:check_heartbeat, %{checking: false} = state) do
    {:noreply, state}
  end

  def handle_info(:check_heartbeat, state) do
    current_time = System.monotonic_time(:millisecond)
    time_since_last = current_time - state.last_heartbeat

    new_state =
      if time_since_last > @check_interval do
        missed = state.missed_heartbeats + 1

        Logger.warning(
          "Missed heartbeat #{missed}/#{@max_missed_heartbeats} (#{time_since_last}ms since last)"
        )

        if missed >= @max_missed_heartbeats do
          Logger.error("No heartbeat received for #{missed} checks. Shutting down...")
          Process.sleep(100)
          System.halt(0)
        end

        %{state | missed_heartbeats: missed}
      else
        # Got a heartbeat recently, reset counter
        if state.missed_heartbeats > 0 do
          Logger.info("Heartbeat recovered after #{state.missed_heartbeats} missed checks")
        end

        %{state | missed_heartbeats: 0}
      end

    Process.send_after(self(), :check_heartbeat, @check_interval)
    {:noreply, new_state}
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

  defp heartbeat_enabled? do
    Application.get_env(:tau5, :heartbeat_enabled, true)
  end
end

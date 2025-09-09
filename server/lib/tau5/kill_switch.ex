defmodule Tau5.KillSwitch do
  @moduledoc """
  A dead-simple kill switch that shuts down the system if not reset periodically.
  This is completely isolated from networking - it's just a timer that kills.
  """
  use GenServer
  require Logger

  @default_grace_period 10_000
  @default_check_interval 10_000
  @default_max_missed_checks 4

  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts, name: __MODULE__)
  end

  def init(_opts) do
    if enabled?() do
      Process.flag(:trap_exit, true)

      grace_period = max(0, get_env_int("TAU5_HB_GRACE_MS", @default_grace_period))
      check_interval = max(1000, get_env_int("TAU5_HB_INTERVAL_MS", @default_check_interval))
      max_missed = max(1, get_env_int("TAU5_HB_MAX_MISSES", @default_max_missed_checks))

      Logger.debug("Kill switch armed - will activate in #{grace_period}ms")
      Logger.debug("Kill switch config: interval=#{check_interval}ms, max_misses=#{max_missed}")

      Process.send_after(self(), :start_monitoring, grace_period)

      {:ok,
       %{
         last_reset: System.monotonic_time(:millisecond),
         monitoring: false,
         missed_checks: 0,
         check_interval: check_interval,
         max_missed_checks: max_missed
       }}
    else
      Logger.info("Kill switch disabled")
      {:ok, %{enabled: false}}
    end
  end

  def terminate(reason, state) do
    if state[:enabled] != false do
      if reason not in [:normal, :shutdown] and
           not match?({:shutdown, _}, reason) do
        if System.get_env("MIX_ENV") == "dev" do
          hard_kill_self()
        else
          System.halt(0)
        end
      end
    end

    :ok
  end

  def reset do
    GenServer.cast(__MODULE__, :reset)
  end

  def handle_cast(:reset, %{enabled: false} = state) do
    {:noreply, state}
  end

  def handle_cast(:reset, state) do
    new_state = %{state | last_reset: System.monotonic_time(:millisecond), missed_checks: 0}

    if state[:missed_checks] && state.missed_checks > 1 do
      Logger.info(
        "Kill switch: normal operation restored after #{state.missed_checks} missed checks"
      )
    end

    {:noreply, new_state}
  end

  def handle_info(:start_monitoring, state) do
    Logger.debug("Kill switch activated - monitoring started")
    Process.send_after(self(), :check, state.check_interval)
    {:noreply, %{state | monitoring: true}}
  end

  def handle_info(:check, %{enabled: false} = state) do
    {:noreply, state}
  end

  def handle_info(:check, %{monitoring: false} = state) do
    {:noreply, state}
  end

  def handle_info(:check, state) do
    current_time = System.monotonic_time(:millisecond)
    time_since_reset = current_time - state.last_reset

    new_state =
      if time_since_reset > state.check_interval do
        missed = state.missed_checks + 1

        Logger.warning(
          "Kill switch: missed check #{missed}/#{state.max_missed_checks} " <>
            "(#{time_since_reset}ms since last reset)"
        )

        if missed >= state.max_missed_checks do
          spawn(fn ->
            Logger.error("KILL SWITCH TRIGGERED - No reset for #{missed} checks")
            Logger.error("Shutting down NOW")
            Process.sleep(100)

            if System.get_env("MIX_ENV") == "dev" do
              hard_kill_self()
            else
              System.halt(0)
            end
          end)

          %{state | missed_checks: missed}
        else
          %{state | missed_checks: missed}
        end
      else
        %{state | missed_checks: 0}
      end

    Process.send_after(self(), :check, state.check_interval)
    {:noreply, new_state}
  end

  def handle_info(_msg, state) do
    {:noreply, state}
  end

  defp enabled? do
    case System.get_env("TAU5_HEARTBEAT_ENABLED") do
      "true" -> true
      "false" -> false
      _ -> Application.get_env(:tau5, :heartbeat_enabled, true)
    end
  end

  defp hard_kill_self do
    pid = System.pid() |> String.to_integer()

    case :os.type() do
      {:win32, _} ->
        System.cmd("taskkill", ["/PID", Integer.to_string(pid), "/F"])

      _ ->
        System.cmd("kill", ["-9", Integer.to_string(pid)])
    end
  end

  defp get_env_int(var_name, default) do
    case System.get_env(var_name) do
      nil ->
        default

      str ->
        case Integer.parse(str) do
          {val, ""} ->
            val

          _ ->
            Logger.warning("Invalid #{var_name}: #{str}, using default #{default}")
            default
        end
    end
  end
end

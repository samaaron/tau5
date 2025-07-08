defmodule DebugShutdownTest do
  use ExUnit.Case

  test "diagnose what's preventing shutdown" do
    IO.puts("\n=== Checking processes ===")

    check_process(Tau5.Supervisor, "Supervisor")
    check_process(Tau5.ConfigRepo, "ConfigRepo")
    check_process(Tau5.Link, "Link")
    check_process(Tau5.MIDI, "MIDI")
    check_process(Tau5.Discovery, "Discovery")
    check_process(Tau5Web.Endpoint, "Endpoint")

    # Check if any processes are trapping exits
    IO.puts("\n=== Checking for exit trapping ===")

    for {name, pid} <- Process.list() |> Enum.map(&{&1, Process.info(&1, :trap_exit)}) do
      case pid do
        {:trap_exit, true} -> IO.puts("Process #{inspect(name)} is trapping exits")
        _ -> :ok
      end
    end

    IO.puts("\n=== Attempting graceful shutdown ===")

    IO.puts("Stopping endpoint...")
    Supervisor.stop(Tau5Web.Endpoint)
    Process.sleep(100)

    for name <- [Tau5.Link, Tau5.MIDI, Tau5.Discovery] do
      IO.puts("Stopping #{name}...")

      try do
        GenServer.stop(name, :normal, 1000)
      catch
        :exit, reason -> IO.puts("  Failed: #{inspect(reason)}")
      end
    end

    # Stop repo
    IO.puts("Stopping ConfigRepo...")
    Supervisor.stop(Tau5.ConfigRepo)

    Process.sleep(500)

    IO.puts("\n=== After shutdown attempts ===")

    for name <- [
          Tau5.Supervisor,
          Tau5.ConfigRepo,
          Tau5.Link,
          Tau5.MIDI,
          Tau5.Discovery,
          Tau5Web.Endpoint
        ] do
      case Process.whereis(name) do
        nil -> IO.puts("#{name}: stopped")
        pid -> IO.puts("#{name}: STILL ALIVE - #{inspect(pid)}")
      end
    end

    IO.puts("\nForcing exit...")
    :erlang.halt(0, [{:flush, false}])
  end

  defp check_process(name, label) do
    case Process.whereis(name) do
      nil ->
        IO.puts("#{label}: not running")

      pid ->
        info = Process.info(pid, [:current_function, :status, :message_queue_len, :trap_exit])
        IO.puts("#{label}: #{inspect(pid)}")
        IO.puts("  Info: #{inspect(info)}")
    end
  end
end

defmodule DebugShutdownTest do
  use ExUnit.Case

  test "check processes and force exit" do
    # Run immediately, not in a spawn
    IO.puts("\n=== Checking processes ===")

    processes = [
      Tau5.Supervisor,
      Tau5.ConfigRepo,
      Tau5.Link,
      Tau5.MIDI,
      Tau5.Discovery,
      Tau5Web.Endpoint
    ]

    for name <- processes do
      case Process.whereis(name) do
        nil -> IO.puts("#{name}: not running")
        pid -> IO.puts("#{name}: still alive - #{inspect(pid)}")
      end
    end

    # Schedule a forced exit
    Process.send_after(self(), :force_exit, 1000)

    receive do
      :force_exit ->
        IO.puts("\nForcing exit...")
        System.halt(0)
    end
  end
end

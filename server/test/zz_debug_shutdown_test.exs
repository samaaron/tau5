# test/zz_debug_shutdown_test.exs
defmodule DebugShutdownTest do
  use ExUnit.Case

  test "check processes and force exit" do
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

    IO.puts("\nStopping application...")
    Application.stop(:tau5)
    Process.sleep(100)

    IO.puts("Forcing immediate exit...")
    :erlang.halt(0, [{:flush, false}])
  end
end

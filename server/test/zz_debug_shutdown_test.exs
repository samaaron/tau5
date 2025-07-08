defmodule DebugShutdownTest do
  use ExUnit.Case

  test "force shutdown after tests" do
    # This test will run last
    spawn(fn ->
      Process.sleep(1000)

      IO.puts("\n=== Checking processes from test ===")

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

      IO.puts("\nForcing exit...")
      System.halt(0)
    end)

    assert true
  end
end

# test/zz_debug_shutdown_test.exs
defmodule DebugShutdownTest do
  use ExUnit.Case

  test "find what's preventing shutdown" do
    # Get initial process count
    initial_count = length(Process.list())
    IO.puts("\n=== Initial process count: #{initial_count} ===")

    # Stop the application properly
    IO.puts("\n=== Stopping application ===")
    Application.stop(:tau5)
    Process.sleep(500)

    # Check what's still running
    remaining = Process.list()
    IO.puts("\n=== After stopping app: #{length(remaining)} processes remain ===")

    # Find processes that aren't system processes
    app_processes =
      Enum.filter(remaining, fn pid ->
        case Process.info(pid, [:group_leader, :registered_name, :initial_call]) do
          nil ->
            false

          info ->
            # Skip system processes (low PIDs) and test processes
            pid_num =
              pid
              |> :erlang.pid_to_list()
              |> to_string()
              |> String.split(".")
              |> Enum.at(1)
              |> String.to_integer()

            initial_call_str = inspect(info[:initial_call] || "")
            pid_num > 1000 && !String.contains?(initial_call_str, "test")
        end
      end)

    IO.puts("\n=== Non-system processes still running: #{length(app_processes)} ===")

    # Show first 10 processes in detail
    app_processes
    |> Enum.take(10)
    |> Enum.each(fn pid ->
      info = Process.info(pid, [:registered_name, :initial_call, :current_function, :dictionary])
      IO.puts("\nProcess #{inspect(pid)}:")
      IO.puts("  Name: #{inspect(info[:registered_name])}")
      IO.puts("  Initial call: #{inspect(info[:initial_call])}")
      IO.puts("  Current function: #{inspect(info[:current_function])}")

      # Check if it's an Ecto connection
      if info[:dictionary] && Keyword.get(info[:dictionary], :"$ancestors") do
        ancestors = Keyword.get(info[:dictionary], :"$ancestors", [])
        IO.puts("  Ancestors: #{inspect(ancestors |> Enum.take(3))}")
      end
    end)

    # Check for specific known processes
    IO.puts("\n=== Checking for known process types ===")

    for pid <- remaining do
      case Process.info(pid, [:registered_name, :initial_call]) do
        nil ->
          :ok

        info ->
          name = inspect(info[:registered_name])
          initial = inspect(info[:initial_call])

          cond do
            String.contains?(name, "DBConnection") -> IO.puts("DB Connection: #{inspect(pid)}")
            String.contains?(name, "Ecto") -> IO.puts("Ecto: #{inspect(pid)}")
            String.contains?(name, "Finch") -> IO.puts("Finch: #{inspect(pid)}")
            String.contains?(initial, "telemetry") -> IO.puts("Telemetry: #{inspect(pid)}")
            true -> :ok
          end
      end
    end

    # Force exit
    IO.puts("\nForcing exit...")
    :erlang.halt(0, [{:flush, false}])
  end
end

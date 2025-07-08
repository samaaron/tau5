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

            pid_num > 1000 && !String.contains?(to_string(info[:initial_call] || ""), "test")
        end
      end)

    IO.puts("\n=== Non-system processes still running: #{length(app_processes)} ===")

    for pid <- app_processes do
      info = Process.info(pid, [:registered_name, :initial_call, :current_function, :dictionary])
      IO.puts("\nProcess #{inspect(pid)}:")
      IO.puts("  Name: #{inspect(info[:registered_name])}")
      IO.puts("  Initial call: #{inspect(info[:initial_call])}")
      IO.puts("  Current function: #{inspect(info[:current_function])}")

      # Check if it's an Ecto connection
      if info[:dictionary] && Keyword.get(info[:dictionary], :"$initial_call") do
        IO.puts(
          "  Dict initial call: #{inspect(Keyword.get(info[:dictionary], :"$initial_call"))}"
        )
      end
    end

    # Force exit
    IO.puts("\nForcing exit...")
    :erlang.halt(0, [{:flush, false}])
  end
end

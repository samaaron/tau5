ExUnit.start()

spawn(fn ->
  Process.sleep(5000)

  IO.puts("\n=== Force checking processes after 5 seconds ===")

  processes = [
    Tau5.Supervisor,
    Tau5.ConfigRepo,
    Tau5.ConfigRepoMigrator,
    Tau5.Link,
    Tau5.MIDI,
    Tau5.Discovery,
    Tau5Web.Endpoint,
    Tau5.PubSub,
    Tau5.Finch
  ]

  for name <- processes do
    case Process.whereis(name) do
      nil -> IO.puts("#{name}: not running")
      pid -> IO.puts("#{name}: still alive - #{inspect(pid)}")
    end
  end

  if sup_pid = Process.whereis(Tau5.Supervisor) do
    IO.puts("\nSupervisor children:")
    children = Supervisor.which_children(Tau5.Supervisor)

    for {id, pid, _, _} <- children do
      IO.puts("  #{id}: #{inspect(pid)}")
    end
  end

  IO.puts("\nForcing exit...")
  System.halt(0)
end)

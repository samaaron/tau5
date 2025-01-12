defmodule Tau5.Discovery do
  use Supervisor

  def start_link(args) do
    Supervisor.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(args) do
    children = [
      {Tau5.Discovery.ServerSupervisor, []},
      {Tau5.Discovery.NetworkInterfaceWatcher, args}
    ]

    Supervisor.init(children, strategy: :rest_for_one)
  end

  def known_nodes do
    DynamicSupervisor.which_children(Tau5.Discovery.ServerSupervisor)
    |> Enum.flat_map(fn
      {:ok, pid, _type, _modules} ->
        GenServer.call(pid, :known_nodes)

      _ ->
        []
    end)
  end
end

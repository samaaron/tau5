defmodule Tau5.Discovery do
  use Supervisor
  require Logger

  @interval 10_000

  def start_link(args) do
    Supervisor.start_link(__MODULE__, args, name: __MODULE__)
  end

  def init(args) do
    children = []
    Task.start(fn -> monitor_network(args) end)
    Supervisor.init(children, strategy: :one_for_one)
  end

  defp monitor_network(%{uuid: uuid, metadata: metadata} = args) do
    state = %{interfaces: %{}, uuid: uuid, metadata: metadata}
    loop(state)
  end

  defp loop(state) do
    current_interfaces = MapSet.new(get_local_ips())
    state_interfaces = MapSet.new(Map.keys(state.interfaces))

    new_interfaces = MapSet.difference(current_interfaces, state_interfaces)
    removed_interfaces = MapSet.difference(state_interfaces, current_interfaces)

    state =
      Enum.reduce(new_interfaces, state, fn interface, acc ->
        case start_discovery_server(interface, acc.uuid, acc.metadata) do
          {:ok, pid} ->
            Logger.info("Started server for new interface #{inspect(interface)}")
            %{acc | interfaces: Map.put(acc.interfaces, interface, pid)}

          {:error, reason} ->
            Logger.error(
              "Failed to start server for interface #{inspect(interface)}: #{inspect(reason)}"
            )

            acc
        end
      end)

    state =
      Enum.reduce(removed_interfaces, state, fn interface, acc ->
        if pid = Map.get(acc.interfaces, interface) do
          Logger.info("Stopping server for removed interface #{inspect(interface)}")
          Supervisor.terminate_child(__MODULE__, pid)
          %{acc | interfaces: Map.delete(acc.interfaces, interface)}
        else
          acc
        end
      end)

    :timer.sleep(@interval)
    loop(state)
  end

  defp get_local_ips do
    {:ok, ifs} = :inet.getif()

    ifs
    |> Enum.map(&elem(&1, 0))
    # Exclude localhost
    |> Enum.reject(&(&1 == {127, 0, 0, 1}))
  end

  defp start_discovery_server(interface, uuid, metadata) do
    child_spec = %{
      id: {:discovery, interface},
      start:
        {Tau5.Discovery.Server, :start_link,
         [%{uuid: uuid, metadata: metadata, interface: interface}]},
      type: :worker
    }

    Supervisor.start_child(__MODULE__, child_spec)
  end

  def known_nodes do
    Supervisor.which_children(__MODULE__)
    |> Enum.flat_map(fn
      {_, pid, :worker, [Tau5.Discovery.Server]} ->
        GenServer.call(pid, :known_nodes)

      _ ->
        []
    end)
    |> Enum.into(%{})
  end
end

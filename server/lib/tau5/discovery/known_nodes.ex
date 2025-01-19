defmodule Tau5.Discovery.KnownNodes do
  use GenServer
  require Logger

  @cleanup_interval 5000
  @max_node_age 60_000

  def start_link([]) do
    GenServer.start_link(__MODULE__, [], name: __MODULE__)
  end

  @impl true
  def init([]) do
    schedule_cleanup()
    {:ok, %{}}
  end

  def add_node(discovery_interface, hostname, ip, uuid, info, transient) do
    Logger.debug(
      "Adding node #{inspect(hostname)} (#{uuid}) on interface #{inspect(discovery_interface)}"
    )

    GenServer.cast(__MODULE__, {:add, discovery_interface, hostname, ip, uuid, info, transient})
  end

  def nodes do
    GenServer.call(__MODULE__, :nodes)
  end

  def nodes_on_interface(interface) do
    GenServer.call(__MODULE__, {:nodes_on_interface, interface})
  end

  def non_transient_nodes do
    GenServer.call(__MODULE__, :non_transient_nodes)
  end

  def non_transient_nodes_on_interface(interface) do
    GenServer.call(__MODULE__, {:non_transient_nodes_on_interface, interface})
  end

  def nodes_on_interface_to_json_encodable(interface) do
    nodes = nodes_on_interface(interface)
    to_json_encodable(nodes)
  end

  @impl true
  def handle_cast({:add, address, _hostname, address, _uuid, _info, _transient}, state) do
    # Don't add self
    {:noreply, state}
  end

  def handle_cast({:add, discovery_interface, hostname, ip, uuid, info, transient}, state) do
    last_seen = :os.system_time(:millisecond)

    existing_node = Map.get(state, [discovery_interface, hostname, ip, uuid])

    state =
      case existing_node do
        [_info, _last_seen, false] ->
          # If the existing node is not transient, just update the last seen time
          Map.put(state, [discovery_interface, hostname, ip, uuid], [
            info,
            last_seen,
            false
          ])

        [_info, _last_seen, true] ->
          # existing node is transient. Update it to false if the incoming node is not transient
          if !transient do
            Map.put(state, [discovery_interface, hostname, ip, uuid], [
              info,
              last_seen,
              false
            ])
          else
            Map.put(state, [discovery_interface, hostname, ip, uuid], [
              info,
              last_seen,
              true
            ])
          end

        nil ->
          # If the node does not exist, add it
          Map.put(state, [discovery_interface, hostname, ip, uuid], [info, last_seen, transient])

        _ ->
        # Error
        Logger.error("Known Nodes - unknown node state: #{inspect(existing_node)}")
        state
      end

    # state should probably have the structure:
    # %{} discovery_interface -> [hostname, ip, uuid] -> [info, last_seen, transient]

    {:noreply, state}
  end

  @impl true
  def handle_call(:nodes, _from, state) do
    nodes = find_all_nodes(state)
    {:reply, nodes, state}
  end

  def handle_call(:non_transient_nodes, _from, state) do
    nodes = find_non_transient_nodes(state)
    {:reply, nodes, state}
  end

  @impl true
  def handle_call({:non_transient_nodes_on_interface, interface}, _from, state) do
    nodes = find_non_transient_nodes_on_interface(interface, state)
    {:reply, nodes, state}
  end

  def handle_call({:nodes_on_interface, interface}, _from, state) do
    nodes = find_nodes_on_interface(interface, state)
    {:reply, nodes, state}
  end

  @impl true
  def handle_info(:cleanup, known_nodes) do
    now = :os.system_time(:millisecond)
    cutoff = now - @max_node_age

    new_known_nodes =
      Enum.filter(known_nodes, fn {_key, [_info, last_seen, _transient]} ->
        last_seen > cutoff
      end)
      |> Enum.into(%{})

    {:noreply, new_known_nodes}
  end

  def schedule_cleanup do
    Process.send_after(self(), :cleanup, @cleanup_interval)
  end

  defp find_all_nodes(state) do
    state
  end

  def find_non_transient_nodes(state) do
    state
    |> Enum.filter(fn {[_discovery_interface, _hostname, _ip, _uuid],
                       [_info, _last_seen, transient]} ->
      !transient
    end)
    |> Enum.into(%{})
  end

  defp find_non_transient_nodes_on_interface(interface, state) do
    state
    |> Enum.filter(fn {[discovery_interface, _hostname, _ip, _uuid], _info} ->
      discovery_interface == interface
    end)
    |> Enum.filter(fn {_key, [_info, _last_seen, transient]} ->
      !transient
    end)
    |> Enum.into(%{})
  end

  defp find_nodes_on_interface(interface, state) do
    state
    |> Enum.filter(fn {[discovery_interface, _hostname, _ip, _uuid], _info} ->
      discovery_interface == interface
    end)
    |> Enum.into(%{})
  end

  ## Converts internal node list representation to a JSON-encodable format. This looks as follows
  ## [

  defp to_json_encodable(state) do
    state =
      state
      |> Enum.map(fn {[_discovery_interface, hostname, ip, uuid], [info, _last_seen, _transient]} ->
        [hostname, Tuple.to_list(ip), uuid, info]
      end)

    state
  end
end

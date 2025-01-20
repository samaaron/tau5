defmodule Tau5.Discovery.KnownNodes do
  use GenServer
  alias Phoenix.PubSub

  require Logger

  @cleanup_interval 5000
  @max_node_age 60_000
  @known_nodes_topic "known_nodes"

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  @impl true
  def init(%{
        node_uuid: node_uuid,
        hostname: hostname,
        metadata: metadata,
        multicast_addr: multicast_addr,
        discovery_port: discovery_port
      }) do
    state = [
      %{
        node_uuid: node_uuid,
        hostname: hostname,
        metadata: metadata,
        multicast_addr: multicast_addr,
        discovery_port: discovery_port
      },
      %{}
    ]

    schedule_cleanup()
    {:ok, state}
  end

  def add_node(discovery_interface, hostname, ip, uuid, info, transitive) do
    Logger.debug(
      "Adding node #{inspect(hostname)} (#{uuid}) on interface #{inspect(discovery_interface)}"
    )

    GenServer.cast(__MODULE__, {:add, discovery_interface, hostname, ip, uuid, info, transitive})
  end

  def nodes do
    GenServer.call(__MODULE__, :nodes)
  end

  def nodes_on_interface(interface) do
    GenServer.call(__MODULE__, {:nodes_on_interface, interface})
  end

  def non_transitive_nodes do
    GenServer.call(__MODULE__, :non_transitive_nodes)
  end

  def non_transitive_nodes_on_interface(interface) do
    GenServer.call(__MODULE__, {:non_transitive_nodes_on_interface, interface})
  end

  def nodes_on_interface_to_json_encodable(interface) do
    nodes = nodes_on_interface(interface)
    to_json_encodable(nodes)
  end

  @impl true
  def handle_cast(
        {:add, _discovery_interface, _hostname, _ip, uuid, _info, _transitive},
        [%{node_uuid: uuid}, _nodes] = state
      ) do
    # Don't add self
    {:noreply, state}
  end

  def handle_cast({:add, discovery_interface, hostname, ip, uuid, info, transitive}, [
        node_info,
        nodes
      ]) do
    last_seen = :os.system_time(:millisecond)

    existing_node = Map.get(nodes, [discovery_interface, hostname, ip, uuid])

    new_nodes =
      case existing_node do
        [_info, _last_seen, false] ->
          # If the existing node is not transitive, just update the last seen time

          Map.put(nodes, [discovery_interface, hostname, ip, uuid], [
            info,
            last_seen,
            false
          ])

        [_info, _last_seen, true] ->
          # existing node is transitive. Update it to false if the incoming node is not transitive
          if !transitive do
            Map.put(nodes, [discovery_interface, hostname, ip, uuid], [
              info,
              last_seen,
              false
            ])
          else
            Map.put(nodes, [discovery_interface, hostname, ip, uuid], [
              info,
              last_seen,
              true
            ])
          end

        nil ->
          # If the node does not exist, add it
          Map.put(nodes, [discovery_interface, hostname, ip, uuid], [info, last_seen, transitive])

        _ ->
          # Error
          Logger.error("Known Nodes - unknown node state: #{inspect(existing_node)}")
          nodes
      end

    PubSub.broadcast(Tau5.PubSub, @known_nodes_topic, {:nodes_updated, new_nodes})
    # state should probably have the structure:
    # %{} discovery_interface -> [hostname, ip] -> [uuid, info, last_seen, transitive]

    {:noreply, [node_info, new_nodes]}
  end

  @impl true
  def handle_call(:nodes, _from, [_node_info, nodes] = state) do
    all_nodes = find_all_nodes(nodes)
    {:reply, all_nodes, state}
  end

  def handle_call(:non_transitive_nodes, _from, [_node_info, nodes] = state) do
    nt_nodes = find_non_transitive_nodes(nodes)
    {:reply, nt_nodes, state}
  end

  @impl true
  def handle_call(
        {:non_transitive_nodes_on_interface, interface},
        _from,
        [_node_info, nodes] = state
      ) do
    nti_nodes = find_non_transitive_nodes_on_interface(interface, nodes)
    {:reply, nti_nodes, state}
  end

  def handle_call({:nodes_on_interface, interface}, _from, [_node_info, nodes] = state) do
    i_nodes = find_nodes_on_interface(interface, nodes)
    {:reply, i_nodes, state}
  end

  @impl true
  def handle_info(:cleanup, [node_info, nodes]) do
    now = :os.system_time(:millisecond)
    cutoff = now - @max_node_age

    new_known_nodes =
      Enum.filter(nodes, fn {_key, [_info, last_seen, _transitive]} ->
        last_seen > cutoff
      end)
      |> Enum.into(%{})

    if new_known_nodes != nodes do
      PubSub.broadcast(Tau5.PubSub, @known_nodes_topic, {:nodes_updated, new_known_nodes})
    end

    schedule_cleanup()
    {:noreply, [node_info, new_known_nodes]}
  end

  def schedule_cleanup do
    Process.send_after(self(), :cleanup, @cleanup_interval)
  end

  defp find_all_nodes(nodes) do
    nodes
  end

  def find_non_transitive_nodes(nodes) do
    nodes
    |> Enum.filter(fn {[_discovery_interface, _hostname, _ip, _uuid],
                       [_info, _last_seen, transitive]} ->
      !transitive
    end)
    |> Enum.into(%{})
  end

  defp find_non_transitive_nodes_on_interface(interface, nodes) do
    nodes
    |> Enum.filter(fn {[discovery_interface, _hostname, _ip, _uuid], _info} ->
      discovery_interface == interface
    end)
    |> Enum.filter(fn {_key, [_info, _last_seen, transitive]} ->
      !transitive
    end)
    |> Enum.into(%{})
  end

  defp find_nodes_on_interface(interface, nodes) do
    nodes
    |> Enum.filter(fn {[discovery_interface, _hostname, _ip, _uuid], _info} ->
      discovery_interface == interface
    end)
    |> Enum.into(%{})
  end

  defp to_json_encodable(nodes) do
    nodes =
      nodes
      |> Enum.map(fn {[_discovery_interface, hostname, ip, uuid], [info, _last_seen, _transitive]} ->
        [hostname, Tuple.to_list(ip), uuid, info]
      end)

    nodes
  end
end

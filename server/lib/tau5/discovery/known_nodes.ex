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

  def add_node(discovery_interface, hostname, ip, uuid, info) do
    Logger.info(
      "Adding node #{inspect(hostname)} (#{uuid}) on interface #{inspect(discovery_interface)}"
    )

    GenServer.cast(__MODULE__, {:add, discovery_interface, hostname, ip, uuid, info})
  end

  def nodes do
    GenServer.call(__MODULE__, :nodes)
  end

  def nodes_on_interface(interface) do
    GenServer.call(__MODULE__, {:nodes_on_interface, interface})
  end

  def nodes_on_interface_to_json_encodable(interface) do
    nodes = nodes_on_interface(interface)
    to_json_encodable(nodes)
  end

  @impl true
  def handle_cast({:add, discovery_interface, hostname, ip, uuid, info}, state) do
    last_seen = :os.system_time(:millisecond)
    state = Map.put(state, [discovery_interface, hostname, ip, uuid], [info, last_seen])
    {:noreply, state}
  end

  @impl true
  def handle_call(:nodes, _from, state) do
    nodes = find_all_nodes(state)
    {:reply, nodes, state}
  end

  @impl true
  def handle_call({:nodes_on_interface, interface}, _from, state) do
    nodes = find_nodes_on_interface(interface, state)
    {:reply, nodes, state}
  end

  @impl true
  def handle_info(:cleanup, known_nodes) do
    now = :os.system_time(:millisecond)
    cutoff = now - @max_node_age

    new_known_nodes =
      Enum.filter(known_nodes, fn {_key, [_info, last_seen]} ->
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

  defp find_nodes_on_interface(interface, state) do
    Enum.filter(state, fn {[discovery_interface, _hostname, _ip, _uuid], _info} ->
      discovery_interface == interface
    end)
    |> Enum.into(%{})
  end

  ## Converts internal node list representation to a JSON-encodable format. This looks as follows
  ## [

  defp to_json_encodable(state) do
    state =
      state
      |> Enum.map(fn {[_discovery_interface, hostname, ip, uuid], [info, _last_seen]} ->
        [hostname, Tuple.to_list(ip), uuid, info]
      end)

    state
  end
end

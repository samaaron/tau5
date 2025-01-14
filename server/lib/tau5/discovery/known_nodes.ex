defmodule Tau5.Discovery.KnownNodes do
  use GenServer

  @cleanup_interval 5000
  @max_node_age 60_000

  def start_link([]) do
    GenServer.start_link(__MODULE__, [], name: __MODULE__)
  end

  def init([]) do
    schedule_cleanup()
    {:ok, %{}}
  end

  def add_node(uuid, info) do
    GenServer.cast(__MODULE__, {:add, uuid, info})
  end

  def nodes do
    GenServer.call(__MODULE__, :nodes)
  end

  def handle_cast({:add, uuid, info}, known_nodes) do
    last_seen = :os.system_time(:millisecond)

    info =
      Map.new(info)
      |> Map.put(:last_seen, last_seen)

    {:noreply, Map.put(known_nodes, uuid, info)}
  end

  def handle_call(:nodes, _from, known_nodes) do
    {:reply, known_nodes, known_nodes}
  end

  def handle_info(:cleanup, known_nodes) do
    now = :os.system_time(:millisecond)
    cutoff = now - @max_node_age
    new_known_nodes = Enum.filter(known_nodes, fn {_, info} -> info.last_seen > cutoff end)
    new_known_nodes = Map.new(new_known_nodes)
    schedule_cleanup()
    {:noreply, new_known_nodes}
  end

  def schedule_cleanup do
    Process.send_after(self(), :cleanup, @cleanup_interval)
  end
end

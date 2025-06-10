defmodule Tau5.Discovery do
  use GenServer
  alias Phoenix.PubSub
  require Logger

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  def list() do
    :tau5_discovery.list()
  end

  @impl true
  def init(%{node_uuid: uuid}) do
    Logger.info("Starting Discovery with UUID: #{uuid}")
    :tau5_discovery.init()
    Logger.info("Discovery NIF loaded: #{inspect(:tau5_discovery.is_nif_loaded())} ")
    :tau5_discovery.set_notification_pid()
    :tau5_discovery.start(uuid)
    {:ok, %{}}
  end

  @impl true
  def handle_info({:peers_changed, peers}, state) do
    Logger.info("Peers changed: #{inspect(peers)}")
    PubSub.broadcast(Tau5.PubSub, "peers-changed", {:peers_changed, peers})
    {:noreply, state}
  end
end

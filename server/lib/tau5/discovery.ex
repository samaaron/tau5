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
  def init(%{http_port: http_port}) do
    temp_name = UniqueNamesGenerator.generate([:adjectives, :animals])
    name = Tau5.Settings.get_or_put("node_name", temp_name)
    uuid = Tau5.Settings.get_or_put("node_uuid", UUID.uuid4())
    opts = %{node_uuid: uuid, http_port: http_port, name: name}

    if Application.get_env(:tau5, :discovery_enabled, true) do
      Logger.info("Starting Discovery with UUID: #{uuid}")
      :tau5_discovery.init()
      Logger.info("Discovery NIF loaded: #{inspect(:tau5_discovery.is_nif_loaded())} ")
      :tau5_discovery.set_notification_pid()
      :tau5_discovery.start(Jason.encode!(opts))
      Logger.info("Discovery options: #{inspect(opts)}")
    else
      Logger.info("Discovery NIF disabled")
    end

    {:ok, %{}}
  end

  @impl true
  def handle_info({:peers_changed, peers}, state) do
    Logger.info("Peers changed: #{inspect(peers)}")
    PubSub.broadcast(Tau5.PubSub, "peers-changed", {:peers_changed, peers})
    {:noreply, state}
  end
end

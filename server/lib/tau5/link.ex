defmodule Tau5.Link do
  use GenServer
  alias Phoenix.PubSub
  require Logger

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: __MODULE__)
  end

  def tempo() do
    :sp_link.get_tempo()
  end

  def num_peers() do
    :sp_link.get_num_peers()
  end

  @impl true
  def init(_args) do
    Logger.info("Starting Link")
    Logger.info("SP Link NIF loaded: #{inspect(:sp_link.is_nif_loaded())} ")

    unless :sp_link.is_nif_initialized() do
      :sp_link.init_nif(60.0)
    end

    :sp_link.set_callback_pid(self())
    :sp_link.enable(true)
    {:ok, []}
  end

  @impl true
  def handle_info({:link_tempo, tempo}, state) do
    Logger.info("Link tempo update: #{tempo}")
    PubSub.broadcast(Tau5.PubSub, "link-tempo", {:link_tempo, tempo})
    {:noreply, state}
  end

  @impl true
  def handle_info({:link_num_peers, num_peers}, state) do
    Logger.info("Link num peers update: #{num_peers}")
    PubSub.broadcast(Tau5.PubSub, "link-num-peers", {:link_num_peers, num_peers})
    {:noreply, state}
  end

  @impl true
  def handle_info({:link_start}, state) do
    Logger.info("Link start")
    PubSub.broadcast(Tau5.PubSub, "link-start", :link_start)
    {:noreply, state}
  end

  @impl true
  def handle_info({:link_stop}, state) do
    Logger.info("Link stop")
    PubSub.broadcast(Tau5.PubSub, "link-stop", :link_stop)
    {:noreply, state}
  end
end

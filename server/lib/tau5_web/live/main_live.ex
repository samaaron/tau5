defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view
  require Logger
  require Tau5.MonacoEditor

  @link_num_peers_topic "link-num-peers"
  @link_tempo_topic "link-tempo"

  @impl true
  def mount(_params, _session, socket) do
    if connected?(socket) do
      Phoenix.PubSub.subscribe(Tau5.PubSub, @link_num_peers_topic)
      Phoenix.PubSub.subscribe(Tau5.PubSub, @link_tempo_topic)
    end

    {:ok,
     assign(socket,
       link_tempo: Tau5.Link.tempo(),
       link_num_peers: Tau5.Link.num_peers()
     )}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div>
      <h1 class="text-red-500">love</h1>
    </div>
    """
  end

  @impl true
  def handle_info({:link_num_peers, num_peers}, socket) do
    {:noreply, assign(socket, :link_num_peers, num_peers)}
  end

  @impl true
  def handle_info({:link_tempo, tempo}, socket) do
    {:noreply, assign(socket, :link_tempo, tempo)}
  end

  @impl true
  def handle_info(:link_start, socket) do
    {:noreply, socket}
  end

  @impl true
  def handle_info(:link_stop, socket) do
    {:noreply, socket}
  end
end

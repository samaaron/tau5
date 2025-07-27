defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view
  require Logger
  alias Tau5.Layout

  @link_num_peers_topic "link-num-peers"
  @link_tempo_topic "link-tempo"

  @impl true
  def mount(_params, _session, socket) do
    if connected?(socket) do
      Phoenix.PubSub.subscribe(Tau5.PubSub, @link_num_peers_topic)
      Phoenix.PubSub.subscribe(Tau5.PubSub, @link_tempo_topic)
    end

    # Create initial layout with deep nesting using color widgets
    initial_layout = 
      Layout.new_split(
        :horizontal,
        Layout.new_leaf(%{color: "#FF6B6B"}),  # Red
        Layout.new_split(
          :vertical,
          Layout.new_leaf(%{color: "#4ECDC4"}),  # Teal
          Layout.new_split(
            :horizontal,
            Layout.new_leaf(%{color: "#45B7D1"}),  # Blue
            Layout.new_split(
              :vertical,
              Layout.new_leaf(%{color: "#96CEB4"}),  # Green
              Layout.new_split(
                :horizontal,
                Layout.new_leaf(%{color: "#FFEAA7"}),  # Yellow
                Layout.new_leaf(%{color: "#DDA0DD"}),  # Plum
                0.5
              ),
              0.5
            ),
            0.5
          ),
          0.5
        ),
        0.3
      )

    {:ok,
     assign(socket,
       link_tempo: Tau5.Link.tempo(),
       link_num_peers: Tau5.Link.num_peers(),
       pane_layout: initial_layout
     )}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div class="fixed inset-0 bg-black">
      <%= render_pane(assigns, @pane_layout) %>
    </div>
    """
  end

  # Recursive function to render the layout tree
  defp render_pane(assigns, %{type: :leaf, opts: opts, id: widget_id}) do
    assigns = assign(assigns, :color, opts[:color] || "#4ECDC4")
    assigns = assign(assigns, :widget_id, widget_id)
    
    ~H"""
    <div class="w-full h-full">
      <.live_component 
        module={Tau5Web.Widgets.ColorWidget} 
        id={@widget_id}
        color={@color}
      />
    </div>
    """
  end

  defp render_pane(assigns, %{type: :split, orientation: orientation, ratio: ratio, left: left, right: right, id: node_id}) do
    assigns = assign(assigns, :orientation, orientation)
    assigns = assign(assigns, :ratio, ratio)
    assigns = assign(assigns, :left, left)
    assigns = assign(assigns, :right, right)
    assigns = assign(assigns, :node_id, node_id)
    assigns = assign(assigns, :flex_direction, if(orientation == :horizontal, do: "flex-row", else: "flex-col"))
    
    ~H"""
    <div class={"flex #{@flex_direction} w-full h-full"}>
      <div class="pane-child" style={"flex: 0 0 #{@ratio * 100}%;"}>
        <%= render_pane(assigns, @left) %>
      </div>
      <div 
        id={"splitter-#{@node_id}"}
        class={[
          "splitter",
          @orientation == :horizontal && "splitter-vertical",
          @orientation == :vertical && "splitter-horizontal"
        ]}
        phx-hook="Splitter" 
        data-node-id={@node_id}
        data-orientation={@orientation}
      />
      <div class="pane-child" style={"flex: 1 1 #{(1 - @ratio) * 100}%;"}>
        <%= render_pane(assigns, @right) %>
      </div>
    </div>
    """
  end

  @impl true
  def handle_event("resize_split", %{"id" => id, "ratio" => ratio}, socket) do
    new_layout = Layout.update_ratio(socket.assigns.pane_layout, id, ratio)
    {:noreply, assign(socket, :pane_layout, new_layout)}
  end

  @impl true
  def handle_info({:split_pane, node_id, orientation}, socket) do
    # When splitting a pane, add a new color widget with random color
    case Layout.split_node(socket.assigns.pane_layout, node_id, orientation) do
      {:ok, new_layout} ->
        {:noreply, assign(socket, :pane_layout, new_layout)}
      {:error, :not_found} ->
        {:noreply, socket}
    end
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

defmodule Tau5Web.MainLive do
  @moduledoc """
  LiveView for the tiled layout system.
  Shows panels with indices and provides controls for layout operations.
  """

  use Tau5Web, :live_view
  alias Tau5.TiledLayout

  @impl true
  def mount(_params, _session, socket) do
    # Access tier info should already be in socket assigns from AccessTierHook
    # But we need to provide defaults in case it's not there (production issue)
    socket =
      socket
      |> assign_new(:endpoint_type, fn -> "local" end)
      |> assign_new(:access_tier, fn -> "full" end)
      |> assign_new(:features, fn ->
        %{
          admin_tools: true,
          pairing: true,
          fs_access: true,
          mutate: true,
          console_access: true,
          lua_privileged: true,
          midi_access: true,
          link_access: true
        }
      end)
      |> assign(:layout_state, TiledLayout.new())
      |> assign(:show_controls, true)
      |> assign(:show_lua_console, false)
      |> assign(:widget_type, :transform)

    {:ok, socket}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div class="layout-container">
      <!-- Tau5 Cube in top right -->
      <div style="position: fixed; top: 2rem; right: 0rem; width: 150px; height: 150px; z-index: 1000; pointer-events: auto;">
        <canvas
          id="tau5-cube-canvas"
          phx-hook="Tau5ShaderCanvas"
          style="width: 100%; height: 100%; background: transparent;"
        />
      </div>

      <div class="layout-header">
        <div class="layout-controls">
          <span class={["access-badge", "access-#{@endpoint_type}"]}>
            <%= cond do %>
              <% @endpoint_type == "local" -> %>
                <i class="codicon codicon-lock"></i> Local
              <% @access_tier == "friend" -> %>
                <i class="codicon codicon-shield"></i> Public (Friend)
              <% true -> %>
                <i class="codicon codicon-unlock"></i> Public
            <% end %>
          </span>

          <div class="separator"></div>

          <button
            phx-click="split_h"
            phx-value-panel={to_string(@layout_state.active)}
            title="Split Horizontal"
          >
            <i class="codicon codicon-split-horizontal"></i>
          </button>
          <button
            phx-click="split_v"
            phx-value-panel={to_string(@layout_state.active)}
            title="Split Vertical"
          >
            <i class="codicon codicon-split-vertical"></i>
          </button>
          <button
            phx-click="close"
            phx-value-panel={to_string(@layout_state.active)}
            title="Close Panel"
          >
            <i class="codicon codicon-close"></i>
          </button>

          <div class="separator"></div>

          <button phx-click="layout_even_h" title="Even Horizontal Layout">
            <i class="codicon codicon-layout-panel-justify"></i>
          </button>
          <button phx-click="layout_main_v" title="Main Vertical Layout">
            <i class="codicon codicon-layout-sidebar-left"></i>
          </button>
          <button phx-click="layout_tiled" title="Tiled Layout">
            <i class="codicon codicon-layout"></i>
          </button>

          <div class="separator"></div>

          <button
            phx-click="toggle_widget_type"
            title={if @widget_type == :shader, do: "Switch to Transform", else: "Switch to Shader"}
          >
            <%= if @widget_type == :shader do %>
              <span>Shader</span>
            <% else %>
              <span>Blend</span>
            <% end %>
          </button>

          <button
            phx-click="zoom"
            phx-value-panel={to_string(@layout_state.active)}
            class={if @layout_state.zoom != nil, do: "active"}
            title={if @layout_state.zoom != nil, do: "Exit Zoom", else: "Zoom Panel"}
          >
            <i class={
              if @layout_state.zoom != nil,
                do: "codicon codicon-screen-normal",
                else: "codicon codicon-screen-full"
            }>
            </i>
          </button>

          <%= if @features.console_access do %>
            <div class="separator"></div>

            <button
              phx-click="toggle_lua_console"
              class={if @show_lua_console, do: "active"}
              title="Toggle Lua Console"
            >
              <i class="codicon codicon-terminal"></i>
            </button>
          <% end %>
        </div>

        <div class="layout-info">
          <span>Panels: {TiledLayout.panel_count(@layout_state)}</span>
          <span>Active: {@layout_state.active}</span>
          <%= if @layout_state.zoom != nil do %>
            <span class="zoomed">ZOOMED: Panel {@layout_state.zoom}</span>
          <% end %>
        </div>
      </div>

      <div
        class={["layout-body", @layout_state.zoom != nil && "zoom-mode"]}
        data-zoomed-panel={@layout_state.zoom}
      >
        {render_tree(assigns, @layout_state.tree, @layout_state)}
      </div>
    </div>

    <.live_component module={Tau5Web.LuaShellLive} id="lua-shell" visible={@show_lua_console} />
    """
  end

  # Render the tree recursively
  defp render_tree(assigns, tree, layout) do
    do_render_tree(assigns, tree, layout)
  end

  defp do_render_tree(assigns, %{type: :panel, id: panel_id}, layout) do
    panel_index = find_panel_index(layout.index, panel_id)
    is_active = panel_index == layout.active
    is_zoomed = layout.zoom == panel_index

    assigns =
      assigns
      |> assign(:panel_id, panel_id)
      |> assign(:panel_index, panel_index)
      |> assign(:is_active, is_active)
      |> assign(:is_zoomed, is_zoomed)
      |> assign(:widget_type, assigns[:widget_type] || :transform)

    ~H"""
    <div
      class={[
        "panel",
        @is_active && "active",
        @is_zoomed && "zoomed-panel"
      ]}
      data-panel-index={@panel_index}
      phx-click="focus"
      phx-value-panel={to_string(@panel_index)}
    >
      <%= if @widget_type == :shader do %>
        <.live_component
          module={Tau5Web.Widgets.ShaderPanelWidget}
          id={@panel_id}
          panel_id={@panel_id}
          index={@panel_index}
          active={@is_active}
        />
      <% else %>
        <.live_component
          module={Tau5Web.Widgets.TransformWidget}
          id={@panel_id}
          panel_id={@panel_id}
          index={@panel_index}
          active={@is_active}
        />
      <% end %>
    </div>
    """
  end

  defp do_render_tree(assigns, %{type: :split} = split, layout) do
    assigns =
      assigns
      |> assign(:split, split)
      |> assign(:layout, layout)
      |> assign(:flex_class, if(split.direction == :horizontal, do: "flex-row", else: "flex-col"))

    ~H"""
    <div class={["split", @flex_class]}>
      <div class="split-child" style={"flex: 0 0 #{@split.ratio * 100}%"}>
        {do_render_tree(assigns, @split.left, @layout)}
      </div>

      <div
        class={["splitter", splitter_class(@split.direction)]}
        id={"splitter-#{@split.id}"}
        phx-hook="Splitter"
        data-split-id={@split.id}
        data-direction={to_string(@split.direction)}
      >
      </div>

      <div class="split-child" style={"flex: 1 1 #{(1 - @split.ratio) * 100}%"}>
        {do_render_tree(assigns, @split.right, @layout)}
      </div>
    </div>
    """
  end

  # Event handlers

  @impl true
  def handle_event("split_h", %{"panel" => panel}, socket) do
    panel_index = String.to_integer(panel)
    new_layout = TiledLayout.split_horizontal(socket.assigns.layout_state, panel_index)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("split_v", %{"panel" => panel}, socket) do
    panel_index = String.to_integer(panel)
    new_layout = TiledLayout.split_vertical(socket.assigns.layout_state, panel_index)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("close", %{"panel" => panel}, socket) do
    panel_index = String.to_integer(panel)
    new_layout = TiledLayout.close_panel(socket.assigns.layout_state, panel_index)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("focus", %{"panel" => panel}, socket) do
    panel_index = String.to_integer(panel)
    new_layout = TiledLayout.focus_panel(socket.assigns.layout_state, panel_index)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("layout_even_h", _, socket) do
    new_layout = TiledLayout.apply_even_horizontal(socket.assigns.layout_state)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("layout_main_v", _, socket) do
    new_layout = TiledLayout.apply_main_vertical(socket.assigns.layout_state)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("layout_tiled", _, socket) do
    new_layout = TiledLayout.apply_tiled(socket.assigns.layout_state)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("toggle_lua_console", _, socket) do
    {:noreply, assign(socket, :show_lua_console, !socket.assigns.show_lua_console)}
  end

  @impl true
  def handle_event("toggle_widget_type", _, socket) do
    new_type = if socket.assigns.widget_type == :shader, do: :transform, else: :shader
    {:noreply, assign(socket, :widget_type, new_type)}
  end

  @impl true
  def handle_event("zoom", %{"panel" => panel}, socket) do
    layout = socket.assigns.layout_state

    new_layout =
      if layout.zoom != nil do
        TiledLayout.unzoom(layout)
      else
        panel_index = String.to_integer(panel)
        TiledLayout.zoom_panel(layout, panel_index)
      end

    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  @impl true
  def handle_event("resize_split", %{"id" => split_id, "ratio" => ratio}, socket) do
    new_layout = TiledLayout.update_ratio(socket.assigns.layout_state, split_id, ratio)
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  # Keyboard shortcuts removed

  @impl true
  def handle_event(_, _, socket), do: {:noreply, socket}

  # Keyboard shortcuts removed - using button controls only

  # Helper functions

  defp find_panel_index(index_map, panel_id) do
    Enum.find_value(index_map, fn {idx, id} ->
      if id == panel_id, do: idx
    end)
  end

  defp splitter_class(:horizontal), do: "splitter-vertical"
  defp splitter_class(:vertical), do: "splitter-horizontal"
end

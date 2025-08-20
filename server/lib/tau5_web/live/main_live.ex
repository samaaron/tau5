defmodule Tau5Web.MainLive do
  @moduledoc """
  LiveView for the tiled layout system.
  Shows panels with indices and provides controls for layout operations.
  """
  
  use Tau5Web, :live_view
  alias Tau5.TiledLayout

  @impl true
  def mount(_params, _session, socket) do
    {:ok,
     socket
     |> assign(:layout_state, TiledLayout.new())
     |> assign(:show_controls, true)
     |> assign(:show_lua_console, false)}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div class="layout-container">
      
      <div class="layout-header">
        <div class="layout-controls">
          <button phx-click="split_h" phx-value-panel={to_string(@layout_state.active)} title="Split Horizontal">
            <i class="codicon codicon-split-horizontal"></i>
          </button>
          <button phx-click="split_v" phx-value-panel={to_string(@layout_state.active)} title="Split Vertical">
            <i class="codicon codicon-split-vertical"></i>
          </button>
          <button phx-click="close" phx-value-panel={to_string(@layout_state.active)} title="Close Panel">
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
          
          <button phx-click="zoom" phx-value-panel={to_string(@layout_state.active)} class={if @layout_state.zoom != nil, do: "active"} title={if @layout_state.zoom != nil, do: "Exit Zoom", else: "Zoom Panel"}>
            <i class={if @layout_state.zoom != nil, do: "codicon codicon-screen-normal", else: "codicon codicon-screen-full"}></i>
          </button>
          
          <div class="separator"></div>
          
          <button phx-click="toggle_lua_console" class={if @show_lua_console, do: "active"} title="Toggle Lua Console">
            <i class="codicon codicon-terminal"></i>
          </button>
        </div>
        
        <div class="layout-info">
          <span>Panels: <%= TiledLayout.panel_count(@layout_state) %></span>
          <span>Active: <%= @layout_state.active %></span>
          <%= if @layout_state.zoom != nil do %>
            <span class="zoomed">ZOOMED: Panel <%= @layout_state.zoom %></span>
          <% end %>
        </div>
      </div>
      
      <div class={["layout-body", @layout_state.zoom != nil && "zoom-mode"]} 
           phx-window-keydown="keydown"
           data-zoomed-panel={@layout_state.zoom}>
        <%= render_tree(assigns, @layout_state.tree, @layout_state) %>
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
    
    ~H"""
    <div class={[
           "panel", 
           @is_active && "active",
           @is_zoomed && "zoomed-panel"
         ]} 
         data-panel-index={@panel_index}
         phx-click="focus" 
         phx-value-panel={to_string(@panel_index)}>
      <.live_component
        module={Tau5Web.Widgets.ShaderPanelWidget}
        id={@panel_id}
        panel_id={@panel_id}
        index={@panel_index}
        active={@is_active}
      />
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
        <%= do_render_tree(assigns, @split.left, @layout) %>
      </div>
      
      <div class={["splitter", splitter_class(@split.direction)]}
           id={"splitter-#{@split.id}"}
           phx-hook="Splitter"
           data-split-id={@split.id}
           data-direction={to_string(@split.direction)}>
      </div>
      
      <div class="split-child" style={"flex: 1 1 #{(1 - @split.ratio) * 100}%"}>
        <%= do_render_tree(assigns, @split.right, @layout) %>
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

  @impl true
  def handle_event("keydown", %{"key" => key}, socket) do
    handle_keyboard(key, socket)
  end

  @impl true
  def handle_event(_, _, socket), do: {:noreply, socket}

  # Keyboard shortcuts
  defp handle_keyboard(key, socket) do
    layout = socket.assigns.layout_state
    
    new_layout = 
      case key do
        "h" -> TiledLayout.split_horizontal(layout, layout.active)
        "v" -> TiledLayout.split_vertical(layout, layout.active)
        "x" -> TiledLayout.close_panel(layout, layout.active)
        "z" -> 
          if layout.zoom != nil do
            TiledLayout.unzoom(layout)
          else
            TiledLayout.zoom_panel(layout, layout.active)
          end
        "e" -> TiledLayout.apply_even_horizontal(layout)
        "m" -> TiledLayout.apply_main_vertical(layout)
        "t" -> TiledLayout.apply_tiled(layout)
        _ -> layout
      end
    
    {:noreply, assign(socket, :layout_state, new_layout)}
  end

  # Helper functions

  defp find_panel_index(index_map, panel_id) do
    Enum.find_value(index_map, fn {idx, id} -> 
      if id == panel_id, do: idx
    end)
  end

  defp splitter_class(:horizontal), do: "splitter-vertical"
  defp splitter_class(:vertical), do: "splitter-horizontal"
end
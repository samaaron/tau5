defmodule Tau5Web.Widgets.ColorWidget do
  use Tau5Web, :live_component

  def render(assigns) do
    ~H"""
    <div 
      class="color-widget" 
      style={"background-color: #{@color}; width: 100%; height: 100%; display: flex; align-items: center; justify-content: center; border-radius: 4px; min-height: 60px; min-width: 60px;"}
    >
      <span 
        class="color-label" 
        style="color: white; text-shadow: 1px 1px 2px rgba(0,0,0,0.7); font-family: monospace; font-size: clamp(10px, 2.5vw, 14px); font-weight: bold; padding: 4px; word-break: break-all;"
      >
        {@color}
      </span>
    </div>
    """
  end

  def update(assigns, socket) do
    {:ok, 
     socket
     |> assign(assigns)
     |> assign_new(:color, fn -> "#4ECDC4" end)
    }
  end
end
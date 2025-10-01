defmodule Tau5Web.Widgets.TransformWidget do
  @moduledoc """
  A widget that applies CSS blend modes to create color effects.
  Simple color layers that demonstrate different blending modes.
  """

  use Tau5Web, :live_component

  @doc false
  def render(assigns) do
    ~H"""
    <div
      id={"transform-container-#{@id}"}
      class="transform-widget"
      style="width: 100%; height: 100%; position: relative; overflow: hidden; background: #1a1a1a; cursor: default;"
    >
      <!-- Panel info in top-left -->
      <div style="position: absolute; top: 20px; left: 20px; z-index: 10;">
        <div style="font-size: 48px; font-weight: bold; color: #fff; font-family: 'Cascadia Code PL', monospace;">
          {@index}
        </div>
        <div style="font-size: 12px; color: #888; margin-top: 5px; font-family: monospace;">
          {String.slice(@panel_id, 0, 8)}
        </div>
      </div>

      <!-- Blend overlay -->
      <div
        id={"blend-overlay-#{@id}"}
        class={"blend-overlay #{if @active, do: "active"}"}
        style={blend_overlay_style(@index)}
      />

      <!-- Blend mode label -->
      <div style="position: absolute; bottom: 20px; left: 20px; z-index: 100;">
        <div style="color: white; font-size: 12px; font-family: monospace; padding: 8px 12px; background: rgba(0,0,0,0.7); border-radius: 4px;">
          mode: {get_blend_mode(@index)}
        </div>
      </div>

      <style>
        .blend-overlay {
          opacity: 0.7;
        }

        .blend-overlay.active {
          opacity: 1;
        }

        .transform-widget:hover .blend-overlay {
          opacity: 0.8;
        }
      </style>
    </div>
    """
  end

  @doc false
  def update(assigns, socket) do
    {:ok, assign(socket, assigns)}
  end

  @blend_modes [
    {:multiply, "linear-gradient(45deg, rgba(255, 0, 0, 0.5), rgba(0, 0, 255, 0.5))"},
    {:screen, "linear-gradient(135deg, rgba(255, 255, 0, 0.6), rgba(0, 255, 255, 0.6))"},
    {:overlay, "linear-gradient(90deg, rgba(255, 0, 255, 0.5), rgba(0, 255, 0, 0.5))"},
    {:"hard-light", "radial-gradient(circle, rgba(255, 128, 0, 0.7), rgba(128, 0, 255, 0.7))"},
    {:"soft-light", "linear-gradient(180deg, rgba(100, 200, 255, 0.6), rgba(255, 100, 200, 0.6))"},
    {:"color-dodge", "linear-gradient(45deg, rgba(255, 255, 255, 0.8), rgba(0, 0, 0, 0.8))"},
    {:"color-burn", "linear-gradient(225deg, rgba(0, 0, 0, 0.7), rgba(255, 255, 255, 0.7))"},
    {:difference, "linear-gradient(90deg, rgba(255, 0, 128, 0.6), rgba(0, 128, 255, 0.6))"},
    {:exclusion, "linear-gradient(270deg, rgba(255, 200, 0, 0.5), rgba(0, 200, 255, 0.5))"},
    {:hue, "linear-gradient(45deg, rgba(180, 0, 180, 0.6), rgba(0, 180, 180, 0.6))"},
    {:saturation, "linear-gradient(135deg, rgba(255, 100, 100, 0.7), rgba(100, 100, 255, 0.7))"},
    {:luminosity, "linear-gradient(180deg, rgba(200, 200, 200, 0.5), rgba(50, 50, 50, 0.5))"}
  ]

  defp blend_overlay_style(index) do
    {mode, gradient} = get_blend_config(index)

    """
    position: absolute;
    inset: 0;
    background: #{gradient};
    mix-blend-mode: #{mode};
    transition: opacity 0.3s ease;
    """
  end

  defp get_blend_config(index) do
    Enum.at(@blend_modes, rem(index, length(@blend_modes)))
  end

  defp get_blend_mode(index) do
    {mode, _gradient} = get_blend_config(index)
    to_string(mode)
  end
end
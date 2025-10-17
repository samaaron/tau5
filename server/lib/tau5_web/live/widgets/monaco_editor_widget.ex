defmodule Tau5Web.Widgets.MonacoEditorWidget do
  @moduledoc """
  A widget that embeds the Monaco Editor for code editing.
  """

  use Tau5Web, :live_component

  @doc false
  def render(assigns) do
    ~H"""
    <div
      id={"monaco-container-#{@id}"}
      class="monaco-widget"
      style="width: 100%; height: 100%; position: relative; overflow: hidden; background: transparent;"
    >
      <!-- Monaco editor container -->
      <div
        id={"monaco-editor-#{@id}"}
        phx-hook="MonacoEditor"
        phx-update="ignore"
        data-editor-id={@id}
        data-panel-index={@index}
        style="width: 100%; height: 100%; position: absolute; top: 0; left: 0;"
      >
      </div>

      <!-- Panel info overlay (top-right) -->
      <div style="position: absolute; top: 10px; right: 10px; z-index: 10; pointer-events: none;">
        <div style="font-size: 14px; font-weight: bold; color: rgba(255,255,255,0.5); font-family: 'Cascadia Code PL', monospace;">
          Panel {@index}
        </div>
      </div>
    </div>
    """
  end

  @doc false
  def update(assigns, socket) do
    socket =
      socket
      |> assign(assigns)
      |> assign_new(:initial_value, fn -> get_initial_code() end)

    {:ok, socket}
  end

  # Provide initial code
  defp get_initial_code do
    """
    -- Welcome to Tau5!
    -- This is a Lua code editor powered by the Monaco Editor.
    """
  end

end

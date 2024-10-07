defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view

  def mount(_params, _session, socket) do
    {:ok, socket}
  end

  def render(assigns) do
    assigns = assign(assigns, :monaco_id, "monaco-editor-id")

    ~H"""
    <h1 class="text-xl text-orange-400"> Welcome to Tau5 </h1>
    <p class="text-green-300 text-x"> Code. Art. Live.</p>
          <img src="/images/tau5-hirez.png" alt="Tau5 Logo" width="100" />

      <div
        id="Tau5Editor"
        phx-hook="Tau5EditorHook"
        phx-update="ignore"
        data-editor_id="editor"
        data-language="lua"
        data-path="foo"
        data-content="-- Tau5 Editor --"
        data-editor-name="editor name"
      >

        <div class="">
          <div class="h-full" id={@monaco_id} monaco-code-editor></div>

        </div>

      </div>
    """
  end
end

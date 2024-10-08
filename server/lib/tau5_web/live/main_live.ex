defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view

  def mount(_params, _session, socket) do
    {:ok, socket}
  end

  def render(assigns) do
    assigns = assign(assigns, :monaco_id, "monaco-editor-id")

    ~H"""
    <div class="flex items-center justify-between">
      <div>
        <h1 class="inline-block p-1 text-xl text-orange-400 text-opacity-75 bg-black rounded-sm bg-opacity-70">
          Welcome to Tau5
        </h1>
        <p class="text-xl text-white mix-blend-difference">Code. Art. Live.</p>
      </div>
      <img class="mix-blend-difference" src="/images/tau5-bw.png" alt="Tau5 Logo" width="50" />
    </div>
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

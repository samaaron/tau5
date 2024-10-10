defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view
  require Logger
  require Tau5.MonacoEditor

  def mount(_params, _session, socket) do
    {:ok,
     assign(socket,
       code:
         "// Tau5 Editor\n\nosc(10, 0.1, 0.8)\n  .rotate(0.1)\n  .modulate(osc(10), 0.5)\n  .out()"
     )}
  end

  def render(assigns) do
    assigns = assign(assigns, :monaco_id, "monaco-editor-id")
    assigns = assign(assigns, :editor_id, "editor-id-placeholder")

    ~H"""
    <div class="flex items-center justify-between">
      <div>
        <h1 class="inline-block p-1 text-xl text-black text-opacity-75 bg-white rounded-sm mix-blend-difference bg-opacity-70">
          Welcome to Tau5
        </h1>

        <p class="pb-5 text-xl text-white mix-blend-difference">Code. Art. Live.</p>
      </div>
      <img class="mix-blend-difference" src="/images/tau5-bw.png" alt="Tau5 Logo" width="50" />
    </div>

    <div>
      <div>
        <div
          class="inline-block bg-black rounded-sm bg-opacity-70 mix-blend-difference hover:mix-blend-normal"
          phx-click="play_button_clicked"
        >
          <.icon name="hero-play-circle" class="w-8 h-8 text-white hover:text-orange-400" />
        </div>

        <div class="inline-block bg-black rounded-sm bg-opacity-70 mix-blend-difference hover:mix-blend-normal">
          <.icon name="hero-stop" class="w-8 h-8 text-white hover:text-orange-400" />
        </div>
        <div class="inline-block bg-black rounded-sm bg-opacity-70 mix-blend-difference hover:mix-blend-normal">
          <.icon
            name="hero-arrow-path-rounded-square"
            class="w-8 h-8 text-white hover:text-orange-400"
          />
        </div>
      </div>

      <div
        id="Tau5Editor"
        phx-hook="Tau5EditorHook"
        phx-update="ignore"
        data-editor_id="editor"
        data-language="js"
        data-path="foo"
        data-content={@code}
        data-editor-name="editor name"
      >
        <div class="">
          <div
            class="flex-grow h-full max-h-[80vh] min-h-[200px] overflow-y-auto monaco-editor-container"
            id={@monaco_id}
            monaco-code-editor
          >
          </div>
        </div>
      </div>
    </div>
    """
  end

  @impl true
  def handle_event("play_button_clicked", _params, socket) do
    Logger.info("Play button clicked")
    {:noreply, push_event(socket, "update_hydra", %{sketch: socket.assigns.code})}
  end

  @impl true
  def handle_event("update_code_diff", %{"id" => id, "changes" => changes}, socket) do
    current_code = socket.assigns.code

    updated_code =
      Tau5.MonacoEditor.apply_event_on_did_change_model_content(current_code, changes)

    {:noreply, assign(socket, :code, updated_code)}
  end
end

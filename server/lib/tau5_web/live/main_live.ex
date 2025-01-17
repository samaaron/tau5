defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view
  require Logger
  require Tau5.MonacoEditor

  @impl true
  def mount(_params, _session, socket) do
    {:ok,
     assign(socket,
       code:
         "// Tau5 Editor\n\nosc(10, 0.1, 0.8)\n  .rotate(0.1)\n  .modulate(osc(10), 0.5)\n  .out()"
     )}
  end

  @impl true
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
        id={@editor_id}
        phx-hook="Tau5EditorHook"
        phx-update="ignore"
        data-language="js"
        data-path="foo"
        data-content={@code}
        data-editor-name="editor name"
        class="relative z-10 h-[300px] min-h-[100px] max-h-[80vh]"
      >
        <canvas hydra class="absolute top-0 left-0 z-0 w-full h-full"></canvas>
        <div class="relative top-0 left-0 z-10 w-full h-full" id={@monaco_id} monaco-code-editor>
        </div>
        
        <div resize-handle class="absolute bottom-0 right-0 z-20 w-4 h-4 cursor-se-resize"></div>
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
  def handle_event(
        "monaco_on_did_change_model_content",
        %{"id" => _id, "changes" => changes},
        socket
      ) do
    current_code = socket.assigns.code

    updated_code =
      Tau5.MonacoEditor.apply_event_on_did_change_model_content(current_code, changes)

    {:noreply, assign(socket, :code, updated_code)}
  end
end

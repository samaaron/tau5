defmodule Tau5Web.LuaShellLive do
  @moduledoc """
  Quake 2-style Lua shell console for interactive Lua execution.
  """

  use Tau5Web, :live_component
  alias Tau5.LuaEvaluator

  @max_history 100
  @max_output_lines 500

  @impl true
  def mount(socket) do
    {:ok,
     socket
     |> assign(:visible, false)
     |> assign(:input, "")
     |> assign(:history, [])
     |> assign(:history_index, -1)
     |> assign(:prompt, "> ")
     |> assign(:height, nil)
     |> stream(:output_lines, [], dom_id: &"output-#{&1.id}")}
  end

  @impl true
  def update(assigns, socket) do
    {:ok, assign(socket, assigns)}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div
      id="lua-shell-container"
      class={["lua-shell-container", @visible && "visible"]}
      phx-hook="LuaShell"
      style={@height && "height: #{@height}"}
    >
      <div class="shell-resize-handle" id="shell-resize-handle"></div>
      <div class="lua-shell">
        <div class="shell-header">
          <span class="shell-title">TAU5 CONSOLE</span>
          <button class="shell-close" phx-click="toggle_console" phx-target={@myself} title="Close">
            [X]
          </button>
        </div>

        <div class="shell-output" id="shell-output" phx-update="stream">
          <div
            :for={{dom_id, line} <- @streams.output_lines}
            id={dom_id}
            class={["output-line", line.type]}
          >
            <%= if line.type == :input do %>
              <span class="prompt" aria-hidden="true"><%= Map.get(line, :prompt, "> ") %></span><span class="command"><%= line.text %></span>
            <% else %>
              <span class="output-text"><%= raw(format_output_html(line)) %></span>
            <% end %>
          </div>
        </div>

        <div class="shell-input-container">
          <span class="prompt">{@prompt}</span>
          <form phx-submit="execute" phx-target={@myself} class="shell-input-form">
            <input
              type="text"
              id="lua-shell-input"
              name="command"
              value={@input}
              class="shell-input"
              phx-target={@myself}
              autocomplete="off"
              spellcheck="false"
              phx-mounted={@visible && JS.focus()}
            />
          </form>
        </div>

        <div class="shell-scanlines"></div>
      </div>
    </div>
    """
  end

  @impl true
  def handle_event("toggle_console", _, socket) do
    visible = !socket.assigns.visible

    socket =
      socket
      |> assign(:visible, visible)
      |> push_event("toggle_console", %{visible: visible})

    if visible do
      {:noreply, push_event(socket, "focus_input", %{})}
    else
      {:noreply, socket}
    end
  end

  @impl true
  def handle_event("execute", %{"command" => ""}, socket) do
    {:noreply, socket}
  end

  def handle_event("execute", %{"command" => command}, socket) do
    if String.trim(command) == "" do
      {:noreply, socket}
    else
      socket = add_output_line_with_prompt(socket, command, :input, "> ")
      
      history = [command | socket.assigns.history] |> Enum.take(@max_history)
      
      socket =
        socket
        |> assign(:history, history)
        |> assign(:history_index, -1)
        |> assign(:input, "")
        |> execute_lua(command)
        |> push_event("clear_input", %{})
      
      {:noreply, push_event(socket, "scroll_to_bottom", %{})}
    end
  end
  

  defp execute_lua(socket, code) do
    {result, was_expression} = case LuaEvaluator.evaluate("return " <> code) do
      {:ok, _} = success -> 
        {success, true}
      {:error, _} ->
        {LuaEvaluator.evaluate(code), false}
    end

    case result do
      {:ok, result_value} ->
        if was_expression or result_value != "nil" do
          add_output_line(socket, result_value, :result)
        else
          socket
        end

      {:error, error} ->
        add_output_line(socket, error, :error)
    end
  end


  defp add_output_line(socket, text, type) do
    id = System.unique_integer([:positive])
    line = %{id: id, text: text, type: type}

    stream_insert(socket, :output_lines, line, limit: @max_output_lines)
  end
  
  defp add_output_line_with_prompt(socket, text, type, prompt) do
    id = System.unique_integer([:positive])
    line = %{id: id, text: text, type: type, prompt: prompt}

    stream_insert(socket, :output_lines, line, limit: @max_output_lines)
  end

  def format_output_html(%{type: type, text: text}) do
    case type do
      :error ->
        Phoenix.HTML.html_escape(text) |> Phoenix.HTML.safe_to_string()
      
      :result ->
        format_lua_value(text)
      
      _ ->
        Phoenix.HTML.html_escape(text) |> Phoenix.HTML.safe_to_string()
    end
  end

  defp format_lua_value(text) do
    escaped = Phoenix.HTML.html_escape(text) |> Phoenix.HTML.safe_to_string()
    
    escaped
    |> String.replace(~r/\b(true|false)\b/, "<span class=\"lua-boolean\">\\1</span>")
    |> String.replace(~r/\bnil\b/, "<span class=\"lua-nil\">nil</span>")
    |> String.replace(~r/\b(\d+\.?\d*)\b/, "<span class=\"lua-number\">\\1</span>")
    |> String.replace(~r/&quot;([^&]*)&quot;/, "<span class=\"lua-string\">&quot;\\1&quot;</span>")
    |> String.replace(~r/&#39;([^&]*)&#39;/, "<span class=\"lua-string\">&#39;\\1&#39;</span>")
  end

end

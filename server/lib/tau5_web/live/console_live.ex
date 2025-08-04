defmodule Tau5Web.ConsoleLive do
  use Tau5Web, :live_view
  require Logger

  @impl true
  def mount(_params, _session, socket) do
    if connected?(socket) do
      {:ok, evaluator} = start_evaluator()

      {:ok,
       socket
       |> assign(:page_title, "Tau5 Console")
       |> assign(:evaluator, evaluator)
       |> assign(:terminal_output, welcome_message())
       |> assign(:command_history, [])
       |> assign(:history_index, -1)
       |> assign(:current_input, "")
       |> assign(:prompt, "tau5(1)> ")
       |> assign(:counter, 1)}
    else
      {:ok,
       socket
       |> assign(:page_title, "Tau5 Console")
       |> assign(:terminal_output, "Connecting...")
       |> assign(:command_history, [])
       |> assign(:history_index, -1)
       |> assign(:current_input, "")
       |> assign(:prompt, "tau5(1)> ")
       |> assign(:counter, 1)}
    end
  end

  defp start_evaluator do
    parent = self()

    {:ok,
     spawn_link(fn ->
       Process.put(:tau5_parent, parent)
       bindings = []
       env = :elixir.env_for_eval(file: "tau5")
       evaluator_loop(bindings, env, 1)
     end)}
  end

  defp evaluator_loop(bindings, env, counter) do
    receive do
      {:eval, from, code} ->
        {:ok, output_pid} = StringIO.open("")
        original_gl = Process.group_leader()
        Process.group_leader(self(), output_pid)

        try do
          # Update line number
          env = %{env | line: counter}

          {result, new_bindings} = Code.eval_string(code, bindings, env)

          Process.group_leader(self(), original_gl)
          {:ok, {_, output}} = StringIO.close(output_pid)

          send(
            from,
            {:eval_result,
             %{
               success: true,
               result: result,
               output: output,
               bindings: new_bindings
             }}
          )

          evaluator_loop(new_bindings, env, counter + 1)
        rescue
          error ->
            Process.group_leader(self(), original_gl)
            {:ok, {_, output}} = StringIO.close(output_pid)

            error_message = Exception.format(:error, error, __STACKTRACE__)

            send(
              from,
              {:eval_result,
               %{
                 success: false,
                 error: error_message,
                 output: output
               }}
            )

            evaluator_loop(bindings, env, counter + 1)
        catch
          kind, error ->
            Process.group_leader(self(), original_gl)
            {:ok, {_, output}} = StringIO.close(output_pid)

            error_message = Exception.format(kind, error, __STACKTRACE__)

            send(
              from,
              {:eval_result,
               %{
                 success: false,
                 error: error_message,
                 output: output
               }}
            )

            evaluator_loop(bindings, env, counter + 1)
        end

      :stop ->
        :ok
    end
  end

  defp welcome_message do
    """
    Tau5 Console - Interactive Elixir (#{System.version()})

    """
  end

  @impl true
  def render(assigns) do
    ~H"""
    <link phx-track-static rel="stylesheet" href={~p"/assets/console.css"} />
    <div class="tau5-terminal">
      <div class="tau5-terminal-output" id="terminal-output" phx-hook="TerminalScroll">
        <pre class="whitespace-pre-wrap"><%= raw(@terminal_output) %></pre>
      </div>
      
      <div class="tau5-input-line">
        <span class="tau5-prompt">{@prompt}</span>
        <form phx-submit="execute_command" class="flex flex-1">
          <input
            type="text"
            name="command"
            value={@current_input}
            phx-keydown="handle_keydown"
            phx-change="update_input"
            class="tau5-terminal-input"
            autocomplete="off"
            autofocus
            spellcheck="false"
            id="terminal-input"
          />
        </form>
      </div>
    </div>
    """
  end

  @impl true
  def handle_event("execute_command", %{"command" => command}, socket) do
    if String.trim(command) != "" do
      history = [command | socket.assigns.command_history] |> Enum.take(100)

      escaped_command = command |> Phoenix.HTML.html_escape() |> Phoenix.HTML.safe_to_string()

      output =
        socket.assigns.terminal_output <>
          ~s(<span class="tau5-prompt">#{socket.assigns.prompt}</span>) <>
          escaped_command <> "\n"

      if evaluator = socket.assigns[:evaluator] do
        send(evaluator, {:eval, self(), command})
      end

      {:noreply,
       socket
       |> assign(terminal_output: output)
       |> assign(command_history: history)
       |> assign(history_index: -1)
       |> assign(current_input: "")}
    else
      {:noreply, socket}
    end
  end

  @impl true
  def handle_event("update_input", %{"command" => input}, socket) do
    {:noreply, assign(socket, current_input: input)}
  end

  @impl true
  def handle_event("handle_keydown", %{"key" => "ArrowUp"}, socket) do
    history = socket.assigns.command_history
    current_index = socket.assigns.history_index

    if length(history) > 0 do
      new_index = min(current_index + 1, length(history) - 1)
      command = Enum.at(history, new_index, "")

      {:noreply,
       socket
       |> assign(history_index: new_index)
       |> assign(current_input: command)
       |> push_event("update_input_value", %{value: command})}
    else
      {:noreply, socket}
    end
  end

  @impl true
  def handle_event("handle_keydown", %{"key" => "ArrowDown"}, socket) do
    current_index = socket.assigns.history_index

    if current_index > -1 do
      new_index = current_index - 1

      command =
        if new_index == -1 do
          ""
        else
          Enum.at(socket.assigns.command_history, new_index, "")
        end

      {:noreply,
       socket
       |> assign(history_index: new_index)
       |> assign(current_input: command)
       |> push_event("update_input_value", %{value: command})}
    else
      {:noreply, socket}
    end
  end

  @impl true
  def handle_event("handle_keydown", _params, socket) do
    {:noreply, socket}
  end

  @impl true
  def handle_info(
        {:eval_result, %{success: true, result: result, output: output, bindings: _bindings}},
        socket
      ) do
    formatted_result = format_result(result)

    full_output = if output != "", do: output, else: ""

    full_output =
      full_output <> ~s(<span style="color: #4169E1;">) <> formatted_result <> ~s(</span>\n)

    counter = socket.assigns.counter + 1
    new_prompt = "tau5(#{counter})> "

    {:noreply,
     socket
     |> assign(terminal_output: socket.assigns.terminal_output <> full_output)
     |> assign(prompt: new_prompt)
     |> assign(counter: counter)
     |> push_event("scroll_to_bottom", %{})}
  end

  @impl true
  def handle_info({:eval_result, %{success: false, error: error, output: output}}, socket) do
    full_output = if output != "", do: output, else: ""
    escaped_error = error |> Phoenix.HTML.html_escape() |> Phoenix.HTML.safe_to_string()

    full_output =
      full_output <> ~s(<span class="tau5-output-error">) <> escaped_error <> ~s(</span>\n)

    {:noreply,
     socket
     |> assign(terminal_output: socket.assigns.terminal_output <> full_output)
     |> push_event("scroll_to_bottom", %{})}
  end

  defp format_result(result) do
    result
    |> inspect(
      pretty: true,
      width: 80,
      syntax_colors: [
        atom: :cyan,
        string: :green,
        number: :yellow,
        boolean: :magenta,
        nil: :magenta,
        regex: :red
      ]
    )
    |> IO.ANSI.format()
    |> IO.iodata_to_binary()
    |> ansi_to_html()
  end


  defp ansi_to_html(text) do
    text
    |> Phoenix.HTML.html_escape()
    |> Phoenix.HTML.safe_to_string()
    |> String.replace(~r/\e\[36m/, ~s(<span class="tau5-atom">))
    |> String.replace(~r/\e\[32m/, ~s(<span class="tau5-string">))
    |> String.replace(~r/\e\[33m/, ~s(<span class="tau5-number">))
    |> String.replace(~r/\e\[35m/, ~s(<span class="tau5-keyword">))
    |> String.replace(~r/\e\[31m/, ~s(<span class="tau5-regex">))
    |> String.replace(~r/\e\[0m/, ~s(</span>))
    |> String.replace(~r/\e\[\d+m/, "")
  end

  @impl true
  def terminate(_reason, socket) do
    if evaluator = socket.assigns[:evaluator] do
      send(evaluator, :stop)
    end

    :ok
  end
end

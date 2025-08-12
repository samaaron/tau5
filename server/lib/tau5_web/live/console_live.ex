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
       |> assign(:multiline_mode, false)
       |> assign(:counter, 1)
       |> assign(:prompt, "tau5(1)> ")}
    else
      {:ok,
       socket
       |> assign(:page_title, "Tau5 Console")
       |> assign(:terminal_output, "Connecting...")
       |> assign(:command_history, [])
       |> assign(:history_index, -1)
       |> assign(:current_input, "")
       |> assign(:multiline_mode, false)
       |> assign(:counter, 1)
       |> assign(:prompt, "tau5(1)> ")}
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

  defp code_complete?(code) do
    try do
      case Code.string_to_quoted(code) do
        {:ok, _} -> true
        {:error, {_line, _message, _token}} -> false
      end
    rescue
      _ -> false
    end
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
        <form phx-submit="execute_command" class="flex-1">
          <textarea
            :if={@multiline_mode}
            name="command"
            phx-keydown="handle_keydown"
            phx-change="update_input"
            phx-hook="ConsoleInput"
            phx-mounted={JS.focus()}
            class="tau5-terminal-input"
            autocomplete="off"
            autofocus
            spellcheck="false"
            id="terminal-input"
            rows={max(2, length(String.split(@current_input, "\n")))}
          >{@current_input}</textarea>
          <input
            :if={!@multiline_mode}
            type="text"
            name="command"
            value={@current_input}
            phx-keydown="handle_keydown"
            phx-change="update_input"
            phx-hook="ConsoleInput"
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
  def handle_event("update_input", %{"command" => input}, socket) do
    {:noreply, assign(socket, current_input: input)}
  end

  @impl true
  def handle_event("execute_command", %{"command" => command}, socket) do
    cond do
      String.trim(command) == "" and not socket.assigns.multiline_mode ->
        {:noreply, socket}
      
      socket.assigns.multiline_mode ->
        cond do
          String.ends_with?(command, "\n\n") ->
            execute_code(socket, String.trim_trailing(command))
          
          String.ends_with?(command, "\n") and code_complete?(String.trim_trailing(command)) ->
            execute_code(socket, String.trim_trailing(command))
          
          true ->
            {:noreply, socket}
        end
      
      true ->
        if code_complete?(command) do
          execute_code(socket, command)
        else
          enter_multiline_mode(socket, command)
        end
    end
  end
  
  defp enter_multiline_mode(socket, initial_line) do
    {:noreply,
     socket
     |> assign(multiline_mode: true)
     |> assign(current_input: initial_line <> "\n")
     |> push_event("focus_input", %{})}
  end
  
  defp execute_code(socket, code) do
    history = 
      if String.trim(code) != "" do
        [code | socket.assigns.command_history] |> Enum.take(100)
      else
        socket.assigns.command_history
      end
    
    output = 
      if socket.assigns.multiline_mode do
        lines = String.split(code, "\n")
        
        formatted_output = lines
        |> Enum.with_index()
        |> Enum.map_join("\n", fn {line, idx} ->
          escaped = line |> Phoenix.HTML.html_escape() |> Phoenix.HTML.safe_to_string()
          prompt = if idx == 0, do: socket.assigns.prompt, else: "      "
          ~s(<span class="tau5-prompt">#{prompt}</span>#{escaped})
        end)
        
        socket.assigns.terminal_output <> formatted_output <> "\n"
      else
        escaped_code = code |> Phoenix.HTML.html_escape() |> Phoenix.HTML.safe_to_string()
        socket.assigns.terminal_output <>
        ~s(<span class="tau5-prompt">#{socket.assigns.prompt}</span>) <>
        escaped_code <> "\n"
      end
    
    if evaluator = socket.assigns[:evaluator] do
      send(evaluator, {:eval, self(), code})
    end
    
    {:noreply,
     socket
     |> assign(terminal_output: output)
     |> assign(command_history: history)
     |> assign(history_index: -1)
     |> assign(current_input: "")
     |> assign(multiline_mode: false)}
  end

  @impl true
  def handle_event("handle_keydown", %{"key" => "ArrowUp"}, socket) do
    navigate_history(socket, :up)
  end

  @impl true
  def handle_event("handle_keydown", %{"key" => "ArrowDown"}, socket) do
    navigate_history(socket, :down)
  end
  
  defp navigate_history(socket, direction) do
    history = socket.assigns.command_history
    current_index = socket.assigns.history_index
    
    new_index = case direction do
      :up -> min(current_index + 1, length(history) - 1)
      :down -> max(current_index - 1, -1)
    end
    
    cond do
      direction == :up and length(history) == 0 ->
        {:noreply, socket}
        
      direction == :down and current_index == -1 ->
        {:noreply, socket}
        
      true ->
        {command, is_multiline} = 
          if new_index == -1 do
            {"", false}
          else
            cmd = Enum.at(history, new_index, "")
            {cmd, String.contains?(cmd, "\n")}
          end
        
        mode_changed = is_multiline != socket.assigns.multiline_mode
        
        {:noreply,
         socket
         |> assign(history_index: new_index)
         |> assign(current_input: command)
         |> assign(multiline_mode: is_multiline)
         |> push_event(if(mode_changed, do: "focus_input", else: "update_input_value"), 
                      if(mode_changed, do: %{}, else: %{value: command}))}
    end
  end
  
  @impl true
  def handle_event("handle_keydown", %{"key" => "force_execute"}, socket) do
    execute_code(socket, socket.assigns.current_input)
  end
  
  @impl true
  def handle_event("handle_keydown", %{"key" => "insert_newline"}, socket) do
    current = socket.assigns.current_input
    
    if socket.assigns.multiline_mode do
      {:noreply, assign(socket, current_input: current <> "\n")}
    else
      enter_multiline_mode(socket, current)
    end
  end
  
  @impl true
  def handle_event("handle_keydown", %{"key" => "cancel_multiline"}, socket) do
    if socket.assigns.multiline_mode do
      {:noreply,
       socket
       |> assign(current_input: "")
       |> assign(multiline_mode: false)}
    else
      {:noreply, assign(socket, current_input: "")}
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

    new_counter = socket.assigns.counter + 1

    {:noreply,
     socket
     |> assign(terminal_output: socket.assigns.terminal_output <> full_output)
     |> assign(counter: new_counter)
     |> assign(prompt: "tau5(#{new_counter})> ")
     |> push_event("scroll_to_bottom", %{})}
  end

  @impl true
  def handle_info({:eval_result, %{success: false, error: error, output: output}}, socket) do
    full_output = if output != "", do: output, else: ""
    escaped_error = error |> Phoenix.HTML.html_escape() |> Phoenix.HTML.safe_to_string()

    full_output =
      full_output <> ~s(<span class="tau5-output-error">) <> escaped_error <> ~s(</span>\n)

    # Note: Counter doesn't increment on errors per test expectations
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

defmodule Tau5.LuaEvaluator do
  @moduledoc """
  Handles Lua code evaluation in a sandboxed environment with memory and security restrictions.
  """

  require Logger
  import Kernel, except: [spawn_opt: 2]
  import :erlang, only: [spawn_opt: 2]

  @config %{
    timeout_ms: 10,
    max_heap_size: 100_000,
    max_output_size: 10_000
  }

  @lua_denylist [
    [:io],
    [:file],
    [:os],
    [:package],
    [:require],
    [:dofile],
    [:load],
    [:loadfile],
    [:loadstring],
    [:debug],
    [:getmetatable],
    [:setmetatable],
    [:rawget],
    [:rawset]
  ]

  @doc """
  Evaluates Lua code in a sandboxed environment.

  Returns `{:ok, result}` or `{:error, reason}`.
  """
  def evaluate(code) do
    parent = self()
    {pid, ref} = spawn_lua_task(parent, code)
    await_lua_result(pid, ref)
  end

  @doc """
  Checks Lua code syntax without executing it.

  Returns `:ok` if syntax is valid, `{:error, reason}` otherwise.

  Uses Lua.parse_chunk/1 which only parses without execution.
  """
  def check_syntax(code) do
    # Try as expression first (with return), then as statement
    case Lua.parse_chunk("return " <> code) do
      {:ok, _chunk} ->
        :ok

      {:error, _} ->
        # Fall back to parsing as statement
        case Lua.parse_chunk(code) do
          {:ok, _chunk} ->
            :ok

          {:error, errors} ->
            {:error, "Syntax error: #{format_parse_errors(errors)}"}
        end
    end
  end

  defp format_parse_errors(errors) when is_list(errors) do
    errors
    |> Enum.map(&to_string/1)
    |> Enum.join("; ")
  end

  defp format_parse_errors(error), do: to_string(error)

  defp spawn_lua_task(parent, code) do
    pid =
      spawn_opt(fn -> run_lua_eval(parent, code) end, [
        {:message_queue_data, :off_heap},
        {:max_heap_size,
         %{
           size: @config.max_heap_size,
           kill: true,
           include_shared_binaries: true,
           error_logger: false
         }}
      ])

    {pid, Process.monitor(pid)}
  end

  defp await_lua_result(pid, ref) do
    receive do
      {:lua_result, result} ->
        Process.demonitor(ref, [:flush])
        result

      {:DOWN, ^ref, :process, ^pid, :killed} ->
        {:error, "Memory limit exceeded (max #{@config.max_heap_size} bytes)"}

      {:DOWN, ^ref, :process, ^pid, {:memory_exceeded, bytes}} ->
        {:error, "Memory limit exceeded: #{bytes} bytes"}

      {:DOWN, ^ref, :process, ^pid, {:execution_timeout, ms}} ->
        {:error, "Execution timed out after #{ms}ms"}

      {:DOWN, ^ref, :process, ^pid, reason} ->
        {:error, "Process crashed: #{inspect(reason)}"}
    after
      # Give reasonable time for VM setup + execution
      # The actual Lua timeout is handled inside the process
      1000 ->
        Process.exit(pid, :kill)
        Process.demonitor(ref, [:flush])
        
        # Flush any late messages
        receive do
          {:lua_result, _} -> :ok
          {:DOWN, ^ref, :process, ^pid, _} -> :ok
        after
          0 -> :ok
        end
        
        {:error, "Process setup timed out"}
    end
  end

  defp run_lua_eval(parent, code) do
    Process.flag(:trap_exit, true)

    result =
      try do
        lua =
          setup_sandbox()
          |> setup_memory_functions()
          |> disable_print()

        memory_checker = spawn(fn -> memory_check_loop(self(), @config.max_heap_size) end)

        try do
          eval_code(lua, code)
        after
          Process.exit(memory_checker, :normal)
        end
      rescue
        e in Lua.CompilerException ->
          {:error, "Syntax error: #{sanitize_error(e.errors)}"}

        e in Lua.RuntimeException ->
          {:error, "Runtime error: #{sanitize_error(e.message)}"}

        e ->
          Logger.debug("Lua evaluation error: #{inspect(e)}")
          {:error, "Runtime error: execution failed"}
      catch
        :exit, {:memory_exceeded, bytes} ->
          {:error, "Memory limit exceeded: #{bytes} bytes"}
          
        kind, reason ->
          {:error, "Process #{kind}: #{inspect(reason)}"}
      end

    send(parent, {:lua_result, result})
  end

  defp setup_sandbox do
    Lua.new(sandboxed: @lua_denylist)
  end

  defp disable_print(lua) do
    # Replace print with no-op function since Lua has no stdout
    lua
    |> Lua.set!(["print"], fn _args ->
      # Print does nothing - no stdout in embedded Lua
      []
    end)
  end

  defp eval_code(lua, code) do
    # Try as expression first (with return), then as statement if that fails to compile
    {code_to_eval, _is_expression} = 
      case Lua.parse_chunk("return " <> code) do
        {:ok, _} -> {"return " <> code, true}
        {:error, _} -> {code, false}
      end
    
    # Time only the actual Lua execution
    task = Task.async(fn ->
      try do
        {:ok, Lua.eval!(lua, code_to_eval)}
      rescue
        e in Lua.RuntimeException ->
          {:error, {:runtime, e.message}}
        e in Lua.CompilerException ->
          {:error, {:compiler, e.errors}}
      end
    end)
    
    case Task.yield(task, @config.timeout_ms) || Task.shutdown(task, :brutal_kill) do
      {:ok, {:ok, {values, _}}} ->
        formatted = format_result(values)

        if byte_size(formatted) > @config.max_output_size do
          {:error, "Output too large (max #{@config.max_output_size} bytes)"}
        else
          {:ok, formatted}
        end
      
      {:ok, {:error, {:runtime, message}}} ->
        {:error, "Runtime error: #{sanitize_error(message)}"}
      
      {:ok, {:error, {:compiler, errors}}} ->
        {:error, "Syntax error: #{sanitize_error(errors)}"}
      
      nil ->
        # Task timed out
        Process.exit(self(), {:execution_timeout, @config.timeout_ms})
        {:error, "Execution timed out after #{@config.timeout_ms}ms"}
    end
  end

  defp format_result(values) do
    result =
      case values do
        [] ->
          "nil"

        [single] ->
          inspect(single, limit: 10_000, printable_limit: 4096)

        multiple ->
          multiple
          |> Enum.map(&inspect(&1, limit: 10_000, printable_limit: 4096))
          |> Enum.join(", ")
      end

    if byte_size(result) > @config.max_output_size do
      String.slice(result, 0, @config.max_output_size) <> "... (truncated)"
    else
      result
    end
  end

  defp setup_memory_functions(lua_state) do
    Lua.set!(lua_state, ["check_memory"], fn _ ->
      case check_memory(@config.max_heap_size) do
        {:error, used} ->
          [false, "Memory limit exceeded: #{used} bytes used"]

        :ok ->
          case :erlang.process_info(self(), :memory) do
            {:memory, used} -> [true, used]
            _ -> [false, "unknown"]
          end
      end
    end)
  end

  defp check_memory(max_bytes) do
    case :erlang.process_info(self(), :memory) do
      {:memory, used} when used > max_bytes -> {:error, used}
      {:memory, _} -> :ok
      _ -> {:error, :unknown}
    end
  end

  defp memory_check_loop(monitored_pid, max_bytes) do
    case Process.info(monitored_pid, :memory) do
      {:memory, used} when used > max_bytes ->
        Process.exit(monitored_pid, {:memory_exceeded, used})

      {:memory, _} ->
        Process.sleep(100)
        memory_check_loop(monitored_pid, max_bytes)

      nil ->
        :ok
    end
  end

  defp sanitize_error(error) when is_binary(error) do
    error
    |> String.split("\n")
    |> Enum.map(&sanitize_line/1)
    |> Enum.filter(&(&1 != nil))
    |> Enum.join("\n")
    |> String.trim()
    |> limit_error_size()
  end

  defp sanitize_error(error) when is_list(error) do
    error
    |> Enum.map(&to_string/1)
    |> Enum.join(", ")
    |> sanitize_error()
  end

  defp sanitize_error(error) do
    error
    |> inspect(limit: 100, printable_limit: 100)
    |> sanitize_error()
  end

  defp sanitize_line(line) do
    line = String.trim(line)

    cond do
      # Filter out internal state dumps
      String.contains?(line, "{:luerl,") ->
        nil

      String.contains?(line, "{:tstruct,") ->
        nil

      String.contains?(line, "{:tref,") ->
        nil

      String.contains?(line, "#Reference<") ->
        nil

      String.contains?(line, "#Function<") ->
        nil

      String.contains?(line, "{:erl_mfa,") ->
        nil

      String.contains?(line, "{:erl_func,") ->
        nil

      String.contains?(line, "{:array,") ->
        nil

      String.contains?(line, "{:table,") ->
        nil

      String.contains?(line, "{:lua_func,") ->
        nil

      String.contains?(line, "%{max:") ->
        nil

      # Keep user-friendly error messages
      String.contains?(line, "Lua runtime error:") ->
        line
        |> String.replace("Lua runtime error:", "")
        |> String.trim()

      String.contains?(line, "is sandboxed") ->
        line

      String.contains?(line, "invalid index") ->
        extract_user_friendly_error(line)

      String.contains?(line, "syntax error") ->
        line

      String.contains?(line, "unexpected") ->
        line

      String.contains?(line, "attempt to") ->
        line

      String.contains?(line, "bad argument") ->
        line

      line == "" ->
        nil

      # Skip lines that look like internal details
      String.starts_with?(line, "{") ->
        nil

      String.starts_with?(line, "[") ->
        nil

      String.starts_with?(line, "%") ->
        nil

      true ->
        line
    end
  end

  defp extract_user_friendly_error(line) do
    case Regex.run(~r/invalid index\s+["']([^"']+)["']/, line) do
      [_, index] ->
        "invalid index: #{index}"

      _ ->
        case Regex.run(~r/invalid index in .+?: ["']([^"']+)["']/, line) do
          [_, index] -> "invalid index: #{index}"
          _ -> "invalid index"
        end
    end
  end

  defp limit_error_size(error) do
    max_size = 200

    if String.length(error) > max_size do
      String.slice(error, 0, max_size) <> "..."
    else
      error
    end
  end
end

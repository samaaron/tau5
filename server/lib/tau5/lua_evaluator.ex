defmodule Tau5.LuaEvaluator do
  @moduledoc """
  Handles Lua code evaluation in a sandboxed environment with memory and security restrictions.
  """

  require Logger
  import Kernel, except: [spawn_opt: 2]
  import :erlang, only: [spawn_opt: 2]

  @config %{
    timeout_ms: 5_000,
    max_heap_size: 1_000_000,
    max_output_size: 1_000_000
  }

  @lua_denylist [
    [:io], [:file], [:os], [:package], [:require], [:dofile],
    [:load], [:loadfile], [:loadstring], [:debug],
    [:getmetatable], [:setmetatable], [:rawget], [:rawset]
  ]

  @max_output_size 10_000
  @max_heap_size 10 * 1024 * 1024  # 10MB

  @doc """
  Evaluates Lua code in a sandboxed environment.

  Returns `{:ok, result}` or `{:error, reason}`.
  """
  def evaluate(code, opts \\ []) do
    parent = self()
    {pid, ref} = spawn_lua_task(parent, code, opts)
    await_lua_result(pid, ref)
  end

  defp spawn_lua_task(parent, code, opts) do
    pid =
      spawn_opt(fn -> run_lua_eval(parent, code, opts) end, [
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

      {:DOWN, ^ref, :process, ^pid, reason} ->
        {:error, "Process crashed: #{inspect(reason)}"}
    after
      @config.timeout_ms ->
        Process.exit(pid, :kill)
        Process.demonitor(ref, [:flush])
        {:error, "Execution timed out after #{@config.timeout_ms / 1000}s"}
    end
  end

  defp run_lua_eval(parent, code, opts) do
    Process.flag(:trap_exit, true)

    result =
      try do
        lua = setup_sandbox() 
              |> setup_memory_functions()
              |> setup_print_function(opts[:print_callback])
              
        memory_checker = spawn(fn -> memory_check_loop(self(), @config.max_heap_size) end)

        try do
          eval_code(lua, code)
        after
          Process.exit(memory_checker, :normal)
        end
      rescue
        e in Lua.CompilerException -> {:error, "Syntax error: #{sanitize_error(e.errors)}"}
        e in Lua.RuntimeException -> {:error, "Runtime error: #{sanitize_error(e.message)}"}
        e ->
          Logger.debug("Lua evaluation error: #{inspect(e)}")
          {:error, "Runtime error: execution failed"}
      end

    send(parent, {:lua_result, result})
  end

  defp setup_sandbox do
    Lua.new(sandboxed: @lua_denylist)
  end

  defp setup_print_function(lua, nil) do
    # Even without a callback, replace print and eprint to prevent stdout output
    lua
    |> Lua.set!(["print"], fn _args ->
      # Silently ignore prints when no callback is provided
      []
    end)
    |> Lua.set!(["eprint"], fn _args ->
      # Silently ignore eprints when no callback is provided
      []
    end)
  end
  
  defp setup_print_function(lua, print_callback) do
    # Helper function to format arguments
    format_args = fn args ->
      args 
      |> Enum.map(fn arg ->
        case arg do
          nil -> "nil"
          true -> "true"
          false -> "false"
          arg when is_binary(arg) -> arg
          arg when is_number(arg) -> to_string(arg)
          arg when is_list(arg) -> inspect(arg)
          arg when is_tuple(arg) -> inspect(arg)
          arg when is_map(arg) -> inspect(arg)
          _ -> inspect(arg)
        end
      end)
      |> Enum.join("\t")
    end
    
    lua
    |> Lua.set!(["print"], fn args ->
      message = format_args.(args)
      print_callback.(message)
      []
    end)
    |> Lua.set!(["eprint"], fn args ->
      message = format_args.(args)
      # Prefix error prints to distinguish them
      print_callback.("[ERROR] " <> message)
      []
    end)
  end

  defp eval_code(lua, code) do
    case Lua.eval!(lua, code) do
      {values, _} ->
        formatted = format_result(values)

        if byte_size(formatted) > @config.max_output_size do
          {:error, "Output too large (max #{@config.max_output_size} bytes)"}
        else
          {:ok, formatted}
        end
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

    if byte_size(result) > @max_output_size do
      String.slice(result, 0, @max_output_size) <> "... (truncated)"
    else
      result
    end
  end

  defp setup_memory_functions(lua_state) do
    Lua.set!(lua_state, ["check_memory"], fn _ ->
      case check_memory(@max_heap_size) do
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

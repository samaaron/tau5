defmodule Tau5MCP.LuaEvaluator do
  @moduledoc """
  Handles Lua code evaluation in a sandboxed environment with security restrictions.
  """

  require Logger

  @timeout_ms 5000

  @doc """
  Evaluates Lua code in a sandboxed environment.
  
  Returns {:ok, result} or {:error, reason}
  """
  def evaluate(code, opts \\ []) do
    print_callback = Keyword.get(opts, :print_callback, &default_print_callback/1)
    
    Task.async(fn ->
      try do
        # Lua.new() already sandboxes dangerous functions by default
        # We can add additional restrictions if needed
        lua_state = Lua.new(sandboxed: [
          [:io], [:file], [:os], [:package], [:require], [:dofile],
          [:load], [:loadfile], [:loadstring], [:debug],
          [:getmetatable], [:setmetatable], [:rawget], [:rawset]
        ])
        lua_state = setup_print(lua_state, print_callback)
        
        case Lua.eval!(lua_state, code) do
          {values, _new_state} -> {:ok, format_result(values)}
        end
      rescue
        e in Lua.CompilerException ->
          {:error, "Syntax error: #{inspect(e.errors)}"}
        e ->
          {:error, "Runtime error: #{inspect(e)}"}
      end
    end)
    |> Task.await(@timeout_ms)
  catch
    :exit, {:timeout, _} ->
      {:error, "Execution timeout (#{@timeout_ms / 1000} seconds)"}
  end

  defp setup_print(lua_state, callback) do
    Lua.set!(lua_state, ["print"], fn args ->
      message = args |> Enum.map(&to_string/1) |> Enum.join("\t")
      callback.(message)
      []
    end)
  end

  defp format_result(values) do
    case values do
      [] -> "nil"
      [single] -> inspect(single)
      multiple -> multiple |> Enum.map(&inspect/1) |> Enum.join(", ")
    end
  end

  defp default_print_callback(message) do
    Logger.info("[Lua output] #{message}")
  end
end
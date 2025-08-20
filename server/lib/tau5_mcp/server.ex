defmodule Tau5MCP.Server do
  use Hermes.Server, capabilities: [:tools]

  alias Hermes.Server.Response
  alias Tau5.LuaEvaluator

  require Logger

  @impl true
  def server_info do
    %{"name" => "tau5-mcp", "version" => "0.1.0"}
  end

  @impl true
  def init(client_info, frame) do
    Logger.debug("[#{__MODULE__}] => Initialized MCP connection with #{inspect(client_info)}")

    {:ok,
     frame
     |> assign(counter: 0)
     |> register_tool("lua_eval",
       input_schema: %{
         code: {:required, :string, description: "Lua code to evaluate"}
       },
       annotations: %{read_only: false},
       description: "Evaluates Lua code and returns the result."
     )}
  end

  @impl true
  def handle_tool_call("lua_eval", %{code: code} = params, frame) do
    Logger.info("[#{__MODULE__}] => lua_eval tool was called #{frame.assigns.counter + 1}")
    
    request_id = frame.request.id
    start_time = System.monotonic_time(:millisecond)
    
    try do
      case LuaEvaluator.evaluate(code) do
        {:ok, result} ->
          duration = System.monotonic_time(:millisecond) - start_time
          Tau5MCP.ActivityLogger.log_activity("lua_eval", request_id, params, :success, duration)
          
          resp = Response.text(Response.tool(), result)
          {:reply, resp, assign(frame, counter: frame.assigns.counter + 1)}
          
        {:error, reason} ->
          duration = System.monotonic_time(:millisecond) - start_time
          Tau5MCP.ActivityLogger.log_activity("lua_eval", request_id, params, :error, duration, reason)
          
          resp = Response.text(Response.tool(), "Error: #{reason}")
          {:reply, resp, assign(frame, counter: frame.assigns.counter + 1)}
      end
    rescue
      exception ->
        duration = System.monotonic_time(:millisecond) - start_time
        error_msg = "Uncaught exception: #{Exception.message(exception)}"
        Tau5MCP.ActivityLogger.log_activity("lua_eval", request_id, params, :exception, duration, error_msg)
        
        Logger.error("[#{__MODULE__}] Uncaught exception in lua_eval: #{inspect(exception)}")
        
        resp = Response.text(Response.tool(), "Internal error: #{Exception.message(exception)}")
        {:reply, resp, assign(frame, counter: frame.assigns.counter + 1)}
    catch
      kind, reason ->
        duration = System.monotonic_time(:millisecond) - start_time
        error_msg = "Process #{kind}: #{inspect(reason)}"
        Tau5MCP.ActivityLogger.log_activity("lua_eval", request_id, params, :crash, duration, error_msg)
        
        Logger.error("[#{__MODULE__}] Process #{kind} in lua_eval: #{inspect(reason)}")
        
        resp = Response.text(Response.tool(), "Process error: #{kind}")
        {:reply, resp, assign(frame, counter: frame.assigns.counter + 1)}
    end
  end
end
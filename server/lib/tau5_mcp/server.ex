defmodule Tau5MCP.Server do
  use Hermes.Server, capabilities: [:tools]

  alias Hermes.Server.Response
  alias Tau5MCP.LuaEvaluator

  require Logger
  
  import Hermes.Server, only: [send_log_message: 3]

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
       description: "Evaluates Lua code and returns the result. Print statements are sent as server events."
     )}
  end

  @impl true
  def handle_tool_call("lua_eval", %{code: code}, frame) do
    Logger.info("[#{__MODULE__}] => lua_eval tool was called #{frame.assigns.counter + 1}")
    
    # Define print callback that sends log messages
    print_callback = fn message ->
      # Send as MCP log message for proper async handling via SSE
      send_log_message(frame, :info, "[Lua] #{message}")
      
      Logger.debug("[#{__MODULE__}] Lua print output: #{message}")
    end
    
    # Evaluate the Lua code
    case LuaEvaluator.evaluate(code, print_callback: print_callback) do
      {:ok, result} ->
        resp = Response.text(Response.tool(), result)
        {:reply, resp, assign(frame, counter: frame.assigns.counter + 1)}
        
      {:error, reason} ->
        resp = Response.text(Response.tool(), "Error: #{reason}")
        {:reply, resp, assign(frame, counter: frame.assigns.counter + 1)}
    end
  end
end
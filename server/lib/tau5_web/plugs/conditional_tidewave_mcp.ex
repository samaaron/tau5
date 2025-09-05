defmodule Tau5Web.Plugs.ConditionalTidewaveMCP do
  @moduledoc """
  Conditionally forwards MCP requests to Tidewave.MCP.Server.
  """
  
  require Logger

  def init(opts), do: opts

  def call(conn, _opts) do
    cond do
      not enabled?() ->
        Logger.debug("Tidewave MCP disabled by environment variable")
        conn
        |> Plug.Conn.put_status(404)
        |> Phoenix.Controller.json(%{error: "Tidewave MCP server is not enabled"})
        
      not Code.ensure_loaded?(Tidewave.MCP.Server) ->
        Logger.debug("Tidewave.MCP.Server module not loaded")
        conn
        |> Plug.Conn.put_status(404)
        |> Phoenix.Controller.json(%{error: "Tidewave MCP server module not available"})
        
      true ->
        try do
          Tidewave.MCP.Server.init_tools()
        rescue
          ArgumentError -> :ok
        end
        
        port = Application.get_env(:tau5, Tau5Web.MCPEndpoint)[:http][:port] || 5555
        
        tidewave_config = %{
          allow_remote_access: false,
          allowed_origins: ["//localhost:#{port}"],
          autoformat: true,
          inspect_opts: [limit: :infinity, printable_limit: :infinity]
        }
        
        conn = Plug.Conn.put_private(conn, :tidewave_config, tidewave_config)
        
        Logger.debug("Calling Tidewave.MCP.Server.handle_http_message")
        try do
          Tidewave.MCP.Server.handle_http_message(conn)
        rescue
          error ->
            Logger.error("Error calling Tidewave.MCP.Server: #{inspect(error)}")
            Logger.error(Exception.format_stacktrace())
            conn
            |> Plug.Conn.put_status(500)
            |> Phoenix.Controller.json(%{error: "Internal server error"})
        end
    end
  end

  defp enabled? do
    System.get_env("TAU5_TIDEWAVE_ENABLED", "false") in ["1", "true", "yes"]
  end
end
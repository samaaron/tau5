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
        port = Application.get_env(:tau5, Tau5Web.MCPEndpoint)[:http][:port] || 5555

        tidewave_config = %{
          allow_remote_access: false,
          allowed_origins: ["//localhost:#{port}"],
          autoformat: true,
          inspect_opts: [limit: :infinity, printable_limit: :infinity]
        }

        conn = Plug.Conn.put_private(conn, :tidewave_config, tidewave_config)

        if logging_enabled?() do
          Tau5Web.Plugs.TidewaveLogger.call(conn, tidewave_config)
        else
          Logger.debug("Calling Tidewave.MCP.Server.handle_http_message")

          try do
            # Using apply/3 here prevents compile-time dependency
            apply(Tidewave.MCP.Server, :handle_http_message, [conn])
          rescue
            UndefinedFunctionError ->
              Logger.error("Tidewave.MCP.Server.handle_http_message/1 is not defined")

              conn
              |> Plug.Conn.put_status(501)
              |> Phoenix.Controller.json(%{error: "MCP handler not implemented"})

            error ->
              Logger.error("Error calling Tidewave.MCP.Server: #{inspect(error)}")
              Logger.error(Exception.format_stacktrace())

              conn
              |> Plug.Conn.put_status(500)
              |> Phoenix.Controller.json(%{error: "Internal server error"})
          end
        end
    end
  end

  defp enabled? do
    System.get_env("TAU5_TIDEWAVE_ENABLED", "false") in ["1", "true", "yes"]
  end

  defp logging_enabled? do
    System.get_env("TAU5_TIDEWAVE_LOGGING", "true") in ["1", "true", "yes"]
  end
end

defmodule Tau5Web.Plugs.ConditionalTidewave do
  @moduledoc """
  Conditionally applies the Tidewave plug based on runtime environment variables.
  This allows enabling/disabling Tidewave MCP without recompilation.
  """

  def init(opts), do: opts

  def call(conn, _opts) do
    if enabled?() and Code.ensure_loaded?(Tidewave) do
      # Initialize and call Tidewave plug at runtime
      # Get the current server port dynamically
      port = Application.get_env(:tau5, Tau5Web.Endpoint)[:http][:port]
      
      tidewave_opts = [
        allow_remote_access: false,
        allowed_origins: ["//localhost:#{port}"],
        autoformat: true
      ]
      
      tidewave_opts
      |> Tidewave.init()
      |> then(&Tidewave.call(conn, &1))
    else
      conn
    end
  end

  defp enabled? do
    System.get_env("TAU5_ENABLE_DEV_MCP", "false") in ["1", "true", "yes"]
  end
end
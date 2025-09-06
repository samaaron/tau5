defmodule Tau5Web.MCPRouter do
  @moduledoc """
  Dedicated router for MCP endpoints.
  Handles only MCP-related routes without the overhead of the main application router.
  """
  use Phoenix.Router

  alias Hermes.Server.Transport.StreamableHTTP

  pipeline :mcp do
    plug :accepts, ["json", "event-stream"]
  end

  scope "/tau5/mcp" do
    pipe_through(:mcp)

    forward("/", StreamableHTTP.Plug, server: Tau5MCP.Server)
  end

  if Mix.env() == :dev do
    scope "/tidewave/mcp" do
      pipe_through(:mcp)

      forward("/", Tau5Web.Plugs.ConditionalTidewaveMCP)
    end
  end

  match :*, "/*path", Tau5Web.MCPFallbackController, :not_found
end

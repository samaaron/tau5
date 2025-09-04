defmodule Tau5Web.Plugs.InternalEndpointSecurity do
  @moduledoc """
  Security plug for the internal endpoint.
  Ensures that:
  1. Connection is from localhost
  2. Valid app token is provided (except for static assets)
  3. Protects sensitive routes like /dev/*
  """
  import Plug.Conn
  require Logger

  def init(opts), do: opts

  def call(conn, _opts) do
    # Always check localhost first
    if not is_local?(conn.remote_ip) do
      conn
      |> put_resp_content_type("text/html")
      |> send_resp(403, forbidden_page("This endpoint is only accessible from localhost"))
      |> halt()
    else
      # For localhost, check token (from query params, header, or session)
      conn = conn |> fetch_query_params() |> fetch_session()
      
      case check_and_store_token(conn) do
        {:ok, conn} ->
          conn
          
        {:error, reason} ->
          conn
          |> put_resp_content_type("text/html")
          |> send_resp(403, forbidden_page(reason))
          |> halt()
      end
    end
  end

  defp check_and_store_token(conn) do
    expected_token = Application.get_env(:tau5, :session_token) || System.get_env("TAU5_SESSION_TOKEN")
    
    # Check if we already have a valid session
    session_token = get_session(conn, "app_token")
    
    # Check for new token in query params or header (NOT session)
    # Handle multiple token params (take first if array)
    query_token = case Map.get(conn.query_params, "token") do
      [first | _] -> first  # Multiple params - take first
      single -> single      # Single param or nil
    end
    
    provided_token = 
      query_token ||
      get_req_header(conn, "x-tau5-token") |> List.first()
    
    
    cond do
      # In dev mode without a token set, allow access (for development convenience)
      is_nil(expected_token) and Mix.env() == :dev ->
        {:ok, conn}
        
      # In prod mode, token is required
      is_nil(expected_token) ->
        Logger.error("No TAU5_SESSION_TOKEN set - internal endpoint is not secure!")
        {:error, "Server not configured with session token"}
        
      # Valid session already exists
      not is_nil(session_token) and session_token == expected_token ->
        {:ok, conn}
        
      # New valid token provided - store it in session  
      not is_nil(provided_token) and provided_token == expected_token ->
        conn = put_session(conn, "app_token", expected_token)
        {:ok, conn}
        
      # Invalid or missing token
      true ->
        Logger.warning("InternalEndpoint - Invalid or missing app token")
        {:error, "Invalid or missing app token. This endpoint requires the Tau5 app."}
    end
  end

  # Only allow 127.0.0.x range for IPv4 localhost (not entire 127.0.0.0/8)
  defp is_local?({127, 0, 0, n}) when n in 1..255, do: true
  defp is_local?({0, 0, 0, 0, 0, 0, 0, 1}), do: true  # IPv6 localhost
  defp is_local?({0, 0, 0, 0, 0, 65535, 32512, 1}), do: true  # IPv4-mapped IPv6 localhost
  defp is_local?(_), do: false

  defp forbidden_page(reason) do
    """
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8">
      <title>Access Denied - Tau5</title>
      <style>
        body {
          background: #0a0a0a;
          color: #e0e0e0;
          font-family: system-ui, -apple-system, sans-serif;
          display: flex;
          align-items: center;
          justify-content: center;
          height: 100vh;
          margin: 0;
        }
        .container {
          text-align: center;
          padding: 2rem;
          background: rgba(52, 52, 52, 0.7);
          border-radius: 8px;
          border: 1px solid rgba(255, 255, 255, 0.1);
          max-width: 500px;
        }
        h1 {
          color: #ff6b6b;
          margin-bottom: 1rem;
        }
        p {
          line-height: 1.6;
          color: #b0b0b0;
        }
        .reason {
          background: rgba(0, 0, 0, 0.3);
          padding: 1rem;
          border-radius: 4px;
          margin-top: 1rem;
          font-family: monospace;
          font-size: 0.9rem;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>🔒 Access Denied</h1>
        <p>This is the Tau5 internal endpoint.</p>
        <div class="reason">#{reason}</div>
        <p style="margin-top: 2rem; font-size: 0.85rem;">
          Please use the Tau5 desktop application to access this server.
        </p>
      </div>
    </body>
    </html>
    """
  end
end
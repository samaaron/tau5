defmodule Tau5Web.Plugs.RequireInternalEndpoint do
  @moduledoc """
  Plug that ensures the request is coming through the internal endpoint.
  Used to protect sensitive routes like /dev/* that should never be 
  accessible through the public endpoint.
  """
  import Plug.Conn
  require Logger

  def init(opts), do: opts

  def call(conn, _opts) do
    if conn.private.phoenix_endpoint == Tau5Web.Endpoint do
      # Request is from internal endpoint, allow
      conn
    else
      # Request is from public endpoint, deny
      Logger.warning(
        "Attempted to access internal-only route #{conn.request_path} from public endpoint"
      )

      conn
      |> put_resp_content_type("text/html")
      |> send_resp(404, not_found_page())
      |> halt()
    end
  end

  defp not_found_page do
    """
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8">
      <title>Not Found - Tau5</title>
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
        }
        h1 {
          color: #ffa500;
          margin-bottom: 1rem;
        }
        p {
          color: #b0b0b0;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>404 - Not Found</h1>
        <p>The requested resource does not exist on this server.</p>
      </div>
    </body>
    </html>
    """
  end
end

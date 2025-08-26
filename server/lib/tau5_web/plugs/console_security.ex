defmodule Tau5Web.Plugs.ConsoleSecurity do
  import Plug.Conn
  require Logger
  
  def init(opts), do: opts
  
  def call(conn, _opts) do
    if conn.request_path == "/dev/console" do
      conn = fetch_query_params(conn)
      
      with :ok <- check_console_enabled(),
           :ok <- check_localhost(conn),
           :ok <- check_token(conn) do
        conn
      else
        {:error, reason} ->
          conn
          |> put_resp_content_type("text/plain")
          |> send_resp(403, reason)
          |> halt()
      end
    else
      conn
    end
  end
  
  defp check_console_enabled do
    if Application.get_env(:tau5, :console_enabled, false) do
      :ok
    else
      Logger.info("Tau5 Console - Access blocked, TAU5_ENABLE_DEV_REPL not set")
      {:error, """
      The Tau5 Elixir REPL console is disabled for security.
      
      To enable the console, set the TAU5_ENABLE_DEV_REPL=1 environment variable
      before starting Tau5, then restart the server.
      
      This feature should only be enabled in trusted development environments.
      """}
    end
  end
  
  defp check_localhost(conn) do
    if is_local?(conn.remote_ip) do
      :ok
    else
      Logger.warning("Tau5 Console - Rejected non-localhost connection from #{inspect(conn.remote_ip)}")
      {:error, """
      For security reasons, the Tau5 console does not accept remote connections.
      
      The console is only accessible from localhost (127.0.0.1, ::1).
      """}
    end
  end
  
  defp check_token(conn) do
    expected_token = Application.get_env(:tau5, :session_token) || System.get_env("TAU5_SESSION_TOKEN")
    provided_token = conn.query_params["token"]
    
    cond do
      is_nil(expected_token) ->
        Logger.warning("No TAU5_SESSION_TOKEN environment variable set")
        {:error, "Server not configured for Tau5 console access"}
        
      provided_token == expected_token ->
        :ok
        
      true ->
        Logger.warning("Tau5 Console - Invalid token provided")
        {:error, """
        Invalid or missing session token.
        
        The Tau5 console can only be accessed by the GUI instance that started this server.
        """}
    end
  end
  
  defp is_local?({127, 0, 0, _}), do: true
  defp is_local?({0, 0, 0, 0, 0, 0, 0, 1}), do: true
  defp is_local?({0, 0, 0, 0, 0, 65535, 32512, 1}), do: true
  defp is_local?(_), do: false
end
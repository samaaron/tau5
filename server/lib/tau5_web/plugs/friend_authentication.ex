defmodule Tau5Web.Plugs.FriendAuthentication do
  @moduledoc """
  Plug that handles friend mode authentication for remote access.

  Checks for friend tokens in:
  1. X-Tau5-Friend-Token header
  2. friend_token query parameter (for initial browser access)

  When a valid token is found, it sets the access tier to "friend" 
  instead of the default "restricted" for public endpoints.
  """

  import Plug.Conn
  import Bitwise
  require Logger

  @header_name "x-tau5-friend-token"
  @query_param "friend_token"

  def init(opts), do: opts

  def call(conn, _opts) do
    if friend_mode_enabled?() do
      conn
      |> fetch_session()
      |> authenticate_friend()
    else
      conn
    end
  end

  defp friend_mode_enabled? do
    Application.get_env(:tau5, :friend_mode_enabled, false)
  end

  defp authenticate_friend(conn) do
    expected_token = Application.get_env(:tau5, :friend_token)

    cond do
      # No token configured - skip authentication
      is_nil(expected_token) or expected_token == "" ->
        conn

      # Check authentication methods in order
      true ->
        provided_token = get_provided_token(conn)

        if secure_compare(provided_token, expected_token) do
          conn
          |> set_friend_session()
          |> assign(:friend_authenticated, true)
          |> assign(:access_tier, "friend")
        else
          # Check if friend token is required for all access
          if Application.get_env(:tau5, :friend_require_token, false) && provided_token == nil do
            conn
            |> put_status(401)
            |> put_resp_content_type("text/plain")
            |> send_resp(401, "Authentication required")
            |> halt()
          else
            # Invalid or no token - continue with restricted access
            conn
          end
        end
    end
  end

  defp get_provided_token(conn) do
    # Try header first (most secure)
    case get_req_header(conn, @header_name) do
      [token | _] ->
        token

      [] ->
        # Try query parameter (for initial browser access)
        conn = fetch_query_params(conn)
        Map.get(conn.query_params, @query_param)
    end
  end

  defp set_friend_session(conn) do
    # Set a session flag so LiveView can detect friend auth
    conn
    |> fetch_session()
    |> put_session("friend_authenticated", true)
  end

  defp secure_compare(nil, _expected), do: false
  defp secure_compare(_provided, nil), do: false

  defp secure_compare(provided, expected) do
    # Constant-time comparison to prevent timing attacks
    provided_bytes = :erlang.binary_to_list(provided)
    expected_bytes = :erlang.binary_to_list(expected)

    if length(provided_bytes) != length(expected_bytes) do
      false
    else
      provided_bytes
      |> Enum.zip(expected_bytes)
      |> Enum.reduce(0, fn {a, b}, acc -> acc ||| bxor(a, b) end)
      |> Kernel.==(0)
    end
  end
end

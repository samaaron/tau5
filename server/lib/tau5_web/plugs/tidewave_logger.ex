defmodule Tau5Web.Plugs.TidewaveLogger do
  @moduledoc """
  Logging wrapper for Tidewave plug that captures MCP tool calls.

  This module intercepts Tidewave MCP requests in development mode to provide
  detailed logging of all tool calls. It uses a custom adapter pattern to cache
  the request body, allowing both logging and downstream processing.

  Note: This is only used in dev mode, so the complexity is acceptable for the
  benefits of complete request/response logging.
  """

  require Logger

  @behaviour Plug

  # Maximum body size to log (1MB) - larger requests are passed through without logging
  @max_body_size 1_000_000

  def init(opts), do: opts

  def call(conn, _tidewave_opts) do
    Logger.debug("TidewaveLogger: Intercepting Tidewave request")
    start_time = System.monotonic_time(:millisecond)

    {:ok, body, conn} = read_body_cached(conn)
    request_info = parse_request(body)

    conn =
      if request_info do
        Plug.Conn.register_before_send(conn, fn conn ->
          duration = System.monotonic_time(:millisecond) - start_time
          spawn(fn -> log_tool_call(request_info, conn, duration) end)
          conn
        end)
      else
        conn
      end

    try do
      apply(Tidewave.MCP.Server, :handle_http_message, [conn])
    rescue
      error ->
        require Logger
        Logger.error("Error calling Tidewave.MCP.Server: #{inspect(error)}")
        Logger.error(Exception.format_stacktrace())

        conn
        |> Plug.Conn.put_status(500)
        |> Phoenix.Controller.json(%{error: "Internal server error"})
    end
  end

  defp read_body_cached(conn) do
    case Plug.Conn.read_body(conn, length: @max_body_size) do
      {:ok, body, conn} when byte_size(body) <= @max_body_size ->
        conn = Plug.Conn.put_private(conn, :raw_body, body)
        {:ok, body, update_conn_for_cached_body(conn, body)}

      {:ok, _large_body, conn} ->
        Logger.debug("TidewaveLogger: Skipping large request body (>#{@max_body_size} bytes)")
        {:ok, "", conn}

      {:more, _partial, conn} ->
        {:ok, "", conn}

      {:error, _reason} ->
        {:ok, "", conn}
    end
  end

  defp update_conn_for_cached_body(conn, body) do
    adapter = {__MODULE__.CachedBodyAdapter, {conn.adapter, body}}
    %{conn | adapter: adapter}
  end

  defmodule CachedBodyAdapter do
    @moduledoc """
    Custom adapter that allows us to intercept and cache the request body
    for logging purposes while still allowing Tidewave to read it.

    This works by wrapping the original Plug adapter and returning our cached
    body when read_req_body is called, then delegating all other adapter
    functions to the original adapter.

    This is only used in dev mode, so the complexity is acceptable for the
    benefit of complete request/response logging.
    """

    # Check Plug version compatibility on module compilation
    # Updated to check for versions > 1.20.0 as we've tested with 1.18.x successfully
    @plug_version Application.spec(:plug, :vsn) |> to_string() |> Version.parse!()
    if Version.compare(@plug_version, "1.20.0") == :gt do
      require Logger

      Logger.warning(
        "TidewaveLogger: Plug version #{@plug_version} may not be compatible with CachedBodyAdapter. Please test thoroughly."
      )
    end

    def read_req_body({original_adapter, body}, _opts) do
      {:ok, body, {original_adapter, ""}}
    end

    def send_resp({original_adapter, _}, status, headers, body) do
      {mod, state} = original_adapter
      mod.send_resp(state, status, headers, body)
    end

    def send_file({original_adapter, _}, status, headers, path, offset, length) do
      {mod, state} = original_adapter
      mod.send_file(state, status, headers, path, offset, length)
    end

    def send_chunked({original_adapter, _}, status, headers) do
      {mod, state} = original_adapter
      mod.send_chunked(state, status, headers)
    end

    def chunk({original_adapter, _}, chunk) do
      {mod, state} = original_adapter
      mod.chunk(state, chunk)
    end

    def inform({original_adapter, _}, status, headers) do
      {mod, state} = original_adapter
      mod.inform(state, status, headers)
    end

    def upgrade({original_adapter, _}, protocol, opts) do
      {mod, state} = original_adapter
      mod.upgrade(state, protocol, opts)
    end

    def push({original_adapter, _}, path, headers) do
      {mod, state} = original_adapter
      mod.push(state, path, headers)
    end

    def get_peer_data({original_adapter, _}) do
      {mod, state} = original_adapter
      mod.get_peer_data(state)
    end

    def get_http_protocol({original_adapter, _}) do
      {mod, state} = original_adapter
      mod.get_http_protocol(state)
    end
  end

  defp parse_request(body) when is_binary(body) and byte_size(body) > 0 do
    case Jason.decode(body) do
      {:ok, %{"method" => method, "params" => params} = request} ->
        %{
          method: method,
          params: params,
          request_id: Map.get(request, "id")
        }

      _ ->
        nil
    end
  rescue
    _ -> nil
  end

  defp parse_request(_), do: nil

  defp log_tool_call(%{method: method} = request_info, conn, duration) do
    response = extract_response(conn)
    status = determine_status(conn, response)

    result =
      case {status, response} do
        {:success, %{"result" => res}} -> res
        {:success, _} -> inspect(response)
        _ -> nil
      end

    TidewaveMCP.ActivityLogger.log_activity(
      method,
      request_info[:request_id] || "unknown",
      request_info[:params] || %{},
      status,
      duration,
      extract_error(response),
      result
    )
  end

  defp extract_response(conn) do
    case conn.resp_body do
      nil ->
        nil

      body when is_binary(body) ->
        case Jason.decode(body) do
          {:ok, decoded} -> decoded
          _ -> nil
        end

      body when is_list(body) ->
        try do
          body
          |> IO.iodata_to_binary()
          |> Jason.decode()
          |> case do
            {:ok, decoded} -> decoded
            _ -> nil
          end
        rescue
          _ -> nil
        end

      _ ->
        nil
    end
  rescue
    _ -> nil
  end

  defp determine_status(conn, response) do
    cond do
      conn.status >= 500 -> :error
      conn.status >= 400 -> :error
      response && Map.has_key?(response, "error") -> :error
      response && Map.has_key?(response, "result") -> :success
      true -> :success
    end
  end

  defp extract_error(nil), do: nil
  defp extract_error(%{"error" => error}), do: inspect(error)
  defp extract_error(_), do: nil
end

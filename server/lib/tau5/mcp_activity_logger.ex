defmodule Tau5.MCPActivityLogger do
  @moduledoc """
  Base functionality for MCP activity logging.
  Provides common logging implementation for different MCP servers.
  """

  require Logger

  defmacro __using__(opts) do
    log_filename = Keyword.fetch!(opts, :log_filename)
    session_type = Keyword.fetch!(opts, :session_type)

    quote do
      require Logger

      @doc """
      Initialize the logger - called once on startup.
      Writes a session marker to the log file.
      """
      def init do
        log_path = get_log_path()
        ensure_log_dir!(log_path)

        session_entry = %{
          timestamp: DateTime.utc_now() |> DateTime.to_iso8601(),
          tool: "_session",
          status: "started",
          params: %{type: unquote(session_type)}
        }

        case Jason.encode(session_entry) do
          {:ok, json} ->
            File.write(log_path, "#{json}\n", [:append])
            Logger.info("#{__MODULE__} session marker written to: #{log_path}")

          {:error, _} ->
            Logger.warning("Failed to write #{__MODULE__} session marker")
        end
      end

      @doc """
      Log an MCP activity as a single complete entry.
      """
      def log_activity(
            tool,
            request_id,
            params,
            status,
            duration_ms,
            error_details \\ nil,
            response \\ nil
          ) do
        params_size = calculate_params_size(params)

        entry = %{
          timestamp: DateTime.utc_now() |> DateTime.to_iso8601(),
          tool: tool,
          request_id: request_id,
          params: params,
          params_size: params_size,
          status: status,
          duration_ms: duration_ms
        }

        entry =
          if status in [:error, :exception, :crash, :syntax_error] && error_details do
            Map.put(entry, :error, to_string(error_details))
          else
            entry
          end

        entry =
          if status == :success && response do
            entry
            |> Map.put(:response, response)
            |> Map.put(:response_size, calculate_response_size(response))
          else
            entry
          end

        case Jason.encode(entry) do
          {:ok, json} ->
            log_path = get_log_path()
            File.write(log_path, "#{json}\n", [:append])

          {:error, reason} ->
            Logger.warning("Failed to log MCP activity: #{inspect(reason)}")
        end
      rescue
        e ->
          Logger.warning("Error logging MCP activity: #{inspect(e)}")
      end

      @doc """
      Log a raw MCP message (request or response) for debugging.
      """
      def log_raw_message(direction, message) do
        entry = %{
          timestamp: DateTime.utc_now() |> DateTime.to_iso8601(),
          direction: direction,
          raw_message: message
        }

        case Jason.encode(entry) do
          {:ok, json} ->
            log_path = get_log_path()
            File.write(log_path, "#{json}\n", [:append])

          {:error, _reason} ->
            :ok
        end
      rescue
        _e -> :ok
      end

      defp calculate_params_size(params) do
        case Jason.encode(params) do
          {:ok, json} -> byte_size(json)
          {:error, _} -> 0
        end
      end

      defp calculate_response_size(response) do
        case Jason.encode(response) do
          {:ok, json} -> byte_size(json)
          {:error, _} -> 0
        end
      end

      defp get_log_path do
        log_dir =
          System.get_env("TAU5_LOG_DIR") ||
            Path.join([System.user_home!(), ".local", "share", "Tau5", "logs"])

        Path.join(log_dir, unquote(log_filename))
      end

      defp ensure_log_dir!(log_path) do
        log_path
        |> Path.dirname()
        |> File.mkdir_p!()
      end

      # TODO: Future enhancement - implement log rotation
      # defp rotate_if_needed(log_path) do
      #   max_size = 50_000_000  # 50MB
      #   case File.stat(log_path) do
      #     {:ok, %{size: size}} when size > max_size ->
      #       timestamp = DateTime.utc_now() |> DateTime.to_iso8601(:basic)
      #       rotated_path = "#{log_path}.#{timestamp}"
      #       File.rename(log_path, rotated_path)
      #       Logger.info("Rotated log to #{rotated_path}")
      #     _ -> 
      #       :ok
      #   end
      # end
    end
  end
end

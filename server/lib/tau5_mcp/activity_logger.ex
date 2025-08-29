defmodule Tau5MCP.ActivityLogger do
  @moduledoc """
  Logs MCP tool activity to a JSONL file.
  """
  
  require Logger
  
  @doc """
  Initialize the logger - called once on startup.
  Writes a session marker to the log file.
  """
  def init do
    log_path = get_log_path()
    ensure_log_dir!(log_path)
    
    # Write a session marker
    session_entry = %{
      timestamp: DateTime.utc_now() |> DateTime.to_iso8601(),
      tool: "_session",
      status: "started",
      params: %{type: "server_session"}
    }
    
    case Jason.encode(session_entry) do
      {:ok, json} ->
        File.write(log_path, "#{json}\n", [:append])
        Logger.info("MCP session marker written to: #{log_path}")
      {:error, _} ->
        Logger.warning("Failed to write session marker")
    end
  end
  
  @doc """
  Log an MCP activity as a single complete entry.
  Logs everything in one entry when the tool completes.
  """
  def log_activity(tool, request_id, params, status, duration_ms, error_details \\ nil, response \\ nil) do
    # Calculate params size
    params_size = calculate_params_size(params)
    
    # Build the complete entry with all information
    entry = %{
      timestamp: DateTime.utc_now() |> DateTime.to_iso8601(),
      tool: tool,
      request_id: request_id,
      params: params,
      params_size: params_size,
      status: status,
      duration_ms: duration_ms
    }
    
    # Add error details for error/exception/crash statuses
    entry = if status in [:error, :exception, :crash] && error_details do
      Map.put(entry, :error, to_string(error_details))
    else
      entry
    end
    
    # Add response for successful calls
    entry = if status == :success && response do
      entry
      |> Map.put(:response, response)
      |> Map.put(:response_size, calculate_response_size(response))
    else
      entry
    end
    
    # Encode as JSON and append to file
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
  
  # Calculate the size of params when encoded as JSON
  defp calculate_params_size(params) do
    case Jason.encode(params) do
      {:ok, json} -> byte_size(json)
      {:error, _} -> 0
    end
  end
  
  # Calculate the size of response when encoded as JSON
  defp calculate_response_size(response) do
    case Jason.encode(response) do
      {:ok, json} -> byte_size(json)
      {:error, _} -> 0
    end
  end
  
  defp get_log_path do
    log_dir = System.get_env("TAU5_LOG_DIR") || 
              Path.join([System.user_home!(), ".local", "share", "Tau5", "logs"])
    Path.join(log_dir, "mcp-tau5.log")
  end
  
  defp ensure_log_dir!(log_path) do
    log_path
    |> Path.dirname()
    |> File.mkdir_p!()
  end
end
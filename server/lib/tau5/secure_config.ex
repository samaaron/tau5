defmodule Tau5.SecureConfig do
  @moduledoc """
  Reads secure configuration from stdin to avoid exposing secrets
  in environment variables or command line arguments.

  Expected format: 5 lines containing session token, heartbeat token,
  heartbeat port, application port, and secret key base.
  """

  require Logger

  @timeout 5000

  def read_stdin_config do
    if stdin_config_enabled?() do
      Logger.info("SecureConfig: Reading configuration from stdin...")

      case read_with_timeout(@timeout) do
        {:ok, [session_token, heartbeat_token, heartbeat_port, app_port, secret_key_base]} ->
          Logger.info("SecureConfig: Successfully read 5 secret values from stdin")

          # Just return the values - let the caller decide what to do with them
          {:ok,
           %{
             session_token: session_token,
             heartbeat_token: heartbeat_token,
             heartbeat_port: heartbeat_port,
             app_port: app_port,
             secret_key_base: secret_key_base
           }}

        {:ok, lines} ->
          Logger.error("SecureConfig: Expected exactly 5 lines, got #{length(lines)}")
          :error

        {:error, :timeout} ->
          Logger.error("SecureConfig: Timeout reading stdin config after #{@timeout}ms")
          :error

        {:error, reason} ->
          Logger.error("SecureConfig: Failed to read stdin config: #{inspect(reason)}")
          :error
      end
    else
      Logger.debug("SecureConfig: Stdin config disabled, using environment variables")
      {:ok, %{}}
    end
  end

  defp stdin_config_enabled? do
    System.get_env("TAU5_USE_STDIN_CONFIG") == "true"
  end

  defp read_with_timeout(timeout) do
    task =
      Task.async(fn ->
        read_stdin_lines()
      end)

    case Task.yield(task, timeout) || Task.shutdown(task) do
      {:ok, result} -> result
      nil -> {:error, :timeout}
    end
  end

  defp read_stdin_lines do
    lines =
      for _ <- 1..5 do
        case IO.gets("") do
          {:error, reason} ->
            {:error, reason}

          :eof ->
            {:error, :eof}

          line when is_binary(line) ->
            String.trim(line)
        end
      end

    # Check if any reads failed
    case Enum.find(lines, &match?({:error, _}, &1)) do
      {:error, reason} ->
        {:error, reason}

      nil ->
        {:ok, lines}
    end
  end
end

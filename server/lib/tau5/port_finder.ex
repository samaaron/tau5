defmodule Tau5.PortFinder do
  @moduledoc """
  Utility module to find an available port for the public endpoint.
  Tries a sequence of ports starting from the preferred one.
  """
  require Logger

  @max_port_attempts 20
  @base_port 7005

  @doc """
  Find an available port for the public endpoint, starting from port 7005.
  Returns {:ok, port} or {:error, :no_available_port}
  """
  def find_available_port do
    find_available_port(@base_port, @max_port_attempts)
  end

  defp find_available_port(_port, 0) do
    {:error, :no_available_port}
  end

  defp find_available_port(port, attempts_left) do
    case try_port(port) do
      :ok ->
        Logger.info("PublicEndpoint: Found available port #{port}")
        {:ok, port}

      {:error, :eaddrinuse} ->
        Logger.debug("PublicEndpoint: Port #{port} is in use, trying next...")
        find_available_port(port + 1, attempts_left - 1)

      {:error, reason} ->
        Logger.warning("PublicEndpoint: Failed to bind to port #{port}: #{inspect(reason)}")
        find_available_port(port + 1, attempts_left - 1)
    end
  end

  defp try_port(port) do
    # For now, just check IPv4 availability
    case :gen_tcp.listen(port, [:inet, {:reuseaddr, true}]) do
      {:ok, socket} ->
        :gen_tcp.close(socket)
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  @doc """
  Configure the endpoint with an appropriate port.
  If a port is already configured, verify it's available.
  Otherwise, find an available port.
  """
  def configure_endpoint_port(endpoint_module) do
    # Get the current endpoint configuration
    current_config = Application.get_env(:tau5, endpoint_module, [])
    http_config = Keyword.get(current_config, :http, [])
    configured_port = Keyword.get(http_config, :port, 0)
    
    Logger.info("PublicEndpoint: Checking port configuration - configured port: #{configured_port}")

    result = 
      if configured_port > 0 do
        # A specific port was configured - verify it's available
        case try_port(configured_port) do
          :ok ->
            Logger.info("PublicEndpoint: Using configured port #{configured_port}")
            {:ok, configured_port}
          
          {:error, reason} ->
            Logger.error("PublicEndpoint: Configured port #{configured_port} is not available: #{inspect(reason)}")
            {:error, :port_unavailable}
        end
      else
        # No port configured, find an available one
        Logger.info("PublicEndpoint: No port configured, auto-finding from base port #{@base_port}")
        case find_available_port() do
          {:ok, port} = result ->
            # Update the endpoint configuration with the found port
            new_http_config =
              http_config
              |> Keyword.put(:port, port)
              |> Keyword.put(:ip, {0, 0, 0, 0, 0, 0, 0, 0})

            new_config = Keyword.put(current_config, :http, new_http_config)
            Application.put_env(:tau5, endpoint_module, new_config)
            
            Logger.info("PublicEndpoint: Configured to use auto-discovered port #{port}")
            result
          
          error ->
            error
        end
      end

    result
  end
end
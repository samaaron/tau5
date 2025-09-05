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
  Update the endpoint configuration with the found port.
  This is called before the endpoint starts.
  """
  def configure_endpoint_port(endpoint_module) do
    # Check if a specific port was configured
    configured_port = Application.get_env(:tau5, :public_port, 0)
    
    port = if configured_port > 0 do
      # Use the configured port directly
      Logger.info("PublicEndpoint: Using configured port #{configured_port}")
      {:ok, configured_port}
    else
      # Find an available port
      find_available_port()
    end
    
    case port do
      {:ok, port} ->
        current_config = Application.get_env(:tau5, endpoint_module, [])
        http_config = Keyword.get(current_config, :http, [])
        
        # Ensure we bind to all interfaces for the public endpoint
        new_http_config = http_config
        |> Keyword.put(:port, port)
        |> Keyword.put(:ip, {0, 0, 0, 0, 0, 0, 0, 0})  # Bind to all interfaces
        
        new_config = Keyword.put(current_config, :http, new_http_config)
        
        Application.put_env(:tau5, endpoint_module, new_config)
        
        Logger.info("PublicEndpoint: Configured to use port #{port} on all interfaces")
        {:ok, port}
      
      {:error, :no_available_port} ->
        Logger.error("PublicEndpoint: Could not find an available port after #{@max_port_attempts} attempts")
        {:error, :no_available_port}
    end
  end
end
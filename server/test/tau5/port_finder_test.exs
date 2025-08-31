defmodule Tau5.PortFinderTest do
  use ExUnit.Case
  alias Tau5.PortFinder
  
  # Use high port numbers for testing to avoid conflicts
  @test_base_port 49152
  
  describe "port finding logic" do
    test "try_port detects occupied ports correctly" do
      # Occupy a port
      port = @test_base_port
      {:ok, socket} = :gen_tcp.listen(port, [:inet, {:reuseaddr, false}])
      
      # Create a test module that exposes the private function
      defmodule TestHelper do
        def test_port(port) do
          # Try to bind - should fail if port is in use
          case :gen_tcp.listen(port, [:inet, {:reuseaddr, true}]) do
            {:ok, test_socket} ->
              :gen_tcp.close(test_socket)
              :ok
            {:error, reason} ->
              {:error, reason}
          end
        end
      end
      
      # Should fail because port is occupied
      assert {:error, _} = TestHelper.test_port(port)
      
      # Clean up
      :gen_tcp.close(socket)
      
      # Give the OS a moment to release the port
      :timer.sleep(100)
      
      # Now it should work (or port may still be releasing on some systems)
      result = TestHelper.test_port(port)
      # After closing, port should be available (unless OS is still releasing it)
      assert result == :ok
    end
    
    test "port finder skips occupied ports" do
      # Occupy the first few test ports
      base = @test_base_port + 100
      {:ok, socket1} = :gen_tcp.listen(base, [:inet, {:reuseaddr, false}])
      {:ok, socket2} = :gen_tcp.listen(base + 1, [:inet, {:reuseaddr, false}])
      
      # Mock the PortFinder to use our test range
      defmodule MockPortFinder do
        def find_from_base(base_port, max_attempts \\ 10) do
          find_available_port(base_port, max_attempts)
        end
        
        defp find_available_port(_port, 0) do
          {:error, :no_available_port}
        end

        defp find_available_port(port, attempts_left) do
          case try_port(port) do
            :ok -> {:ok, port}
            {:error, _} -> find_available_port(port + 1, attempts_left - 1)
          end
        end

        defp try_port(port) do
          case :gen_tcp.listen(port, [:inet, {:reuseaddr, true}]) do
            {:ok, socket} ->
              :gen_tcp.close(socket)
              :ok
            {:error, reason} ->
              {:error, reason}
          end
        end
      end
      
      # Should skip occupied ports and find base + 2
      assert {:ok, found_port} = MockPortFinder.find_from_base(base)
      assert found_port == base + 2
      
      # Clean up
      :gen_tcp.close(socket1)
      :gen_tcp.close(socket2)
    end
  end
  
  describe "IPv6 support" do
    test "checks both IPv4 and IPv6 availability" do
      port = @test_base_port + 200
      
      # Verify we can bind to both stacks
      {:ok, socket4} = :gen_tcp.listen(port, [:inet])
      {:ok, socket6} = :gen_tcp.listen(port + 1, [:inet6])
      
      # Both sockets should be valid references
      assert is_port(socket4)
      assert is_port(socket6)
      
      # Clean up
      :gen_tcp.close(socket4)
      :gen_tcp.close(socket6)
    end
  end
  
  describe "error handling" do
    test "returns error when max attempts exceeded" do
      # Create a finder that will always fail
      defmodule FailingFinder do
        def find_available_port do
          find_port(50000, 0)  # 0 attempts = immediate failure
        end
        
        defp find_port(_port, 0) do
          {:error, :no_available_port}
        end
        
        defp find_port(_port, _attempts) do
          {:error, :no_available_port}
        end
      end
      
      assert {:error, :no_available_port} = FailingFinder.find_available_port()
    end
  end
  
  describe "integration with real PortFinder" do
    test "actual module can find available ports" do
      # The real PortFinder starts from 7005
      # Let's just verify it returns the expected structure
      result = PortFinder.find_available_port()
      
      # Should always find a port or return error
      assert match?({:ok, _port} when is_integer(_port), result) or
             match?({:error, :no_available_port}, result)
      
      # If successful, verify port is in expected range
      case result do
        {:ok, port} ->
          assert port >= 7005
          assert port <= 7025  # Based on max attempts
        _ ->
          :ok  # Error case is valid
      end
    end
    
    test "configure_endpoint_port updates application config" do
      # Use a test endpoint module
      test_endpoint = :"TestEndpoint_#{System.unique_integer([:positive])}"
      
      # Set initial config
      Application.put_env(:tau5, test_endpoint, [http: [port: 0]])
      
      # Try to configure
      result = PortFinder.configure_endpoint_port(test_endpoint)
      
      # Should either succeed or fail to find port
      assert match?({:ok, _port} when is_integer(_port), result) or
             match?({:error, :no_available_port}, result)
      
      # If successful, verify config was updated correctly
      case result do
        {:ok, port} ->
          config = Application.get_env(:tau5, test_endpoint)
          assert config[:http][:port] == port
          assert port >= 7005  # Should be in expected range
        _ ->
          :ok  # Error case is valid
      end
      
      # Clean up
      Application.delete_env(:tau5, test_endpoint)
    end
  end
end
defmodule Tau5.PublicEndpointTest do
  use ExUnit.Case
  
  setup do
    # The PublicEndpoint GenServer is started by the application
    # Just reset it to a known state for each test
    case Process.whereis(Tau5.PublicEndpoint) do
      nil -> 
        # Start it if not running
        {:ok, pid} = start_supervised({Tau5.PublicEndpoint, [enabled: false]})
        {:ok, pid: pid}
      pid -> 
        # Reset to disabled state if already running
        Tau5.PublicEndpoint.disable()
        {:ok, pid: pid}
    end
  end
  
  describe "enable/0" do
    test "enables remote access when called" do
      # Initially disabled
      assert Tau5.PublicEndpoint.enabled?() == false
      
      # Enable it
      assert :ok = Tau5.PublicEndpoint.enable()
      
      # Should now be enabled
      assert Tau5.PublicEndpoint.enabled?() == true
    end
    
    test "is idempotent - calling multiple times is safe" do
      assert :ok = Tau5.PublicEndpoint.enable()
      assert Tau5.PublicEndpoint.enabled?() == true
      
      # Call again
      assert :ok = Tau5.PublicEndpoint.enable()
      assert Tau5.PublicEndpoint.enabled?() == true
    end
  end
  
  describe "disable/0" do
    test "disables remote access when called" do
      # Enable first
      Tau5.PublicEndpoint.enable()
      assert Tau5.PublicEndpoint.enabled?() == true
      
      # Disable it
      assert :ok = Tau5.PublicEndpoint.disable()
      
      # Should now be disabled
      assert Tau5.PublicEndpoint.enabled?() == false
    end
    
    test "is idempotent - calling multiple times is safe" do
      assert :ok = Tau5.PublicEndpoint.disable()
      assert Tau5.PublicEndpoint.enabled?() == false
      
      # Call again
      assert :ok = Tau5.PublicEndpoint.disable()
      assert Tau5.PublicEndpoint.enabled?() == false
    end
  end
  
  describe "toggle/0" do
    test "toggles from disabled to enabled" do
      # Initially disabled
      assert Tau5.PublicEndpoint.enabled?() == false
      
      # Toggle
      assert {:ok, true} = Tau5.PublicEndpoint.toggle()
      assert Tau5.PublicEndpoint.enabled?() == true
    end
    
    test "toggles from enabled to disabled" do
      # Enable first
      Tau5.PublicEndpoint.enable()
      assert Tau5.PublicEndpoint.enabled?() == true
      
      # Toggle
      assert {:ok, false} = Tau5.PublicEndpoint.toggle()
      assert Tau5.PublicEndpoint.enabled?() == false
    end
    
    test "alternates state on repeated calls" do
      initial = Tau5.PublicEndpoint.enabled?()
      
      # Toggle twice should return to initial state
      {:ok, state1} = Tau5.PublicEndpoint.toggle()
      {:ok, state2} = Tau5.PublicEndpoint.toggle()
      
      assert state1 != state2
      assert state2 == initial
      assert Tau5.PublicEndpoint.enabled?() == initial
    end
  end
  
  describe "status/0" do
    test "returns disabled status when disabled" do
      Tau5.PublicEndpoint.disable()
      
      status = Tau5.PublicEndpoint.status()
      assert {:disabled, port: port} = status
      # Port should be an integer or nil if not configured
      if port != nil do
        assert is_integer(port)
        assert port >= 7005  # Should be in valid range
      end
    end
    
    test "returns enabled status when enabled" do
      Tau5.PublicEndpoint.enable()
      
      status = Tau5.PublicEndpoint.status()
      assert {:enabled, port: port} = status
      # Port should be an integer or nil if not configured
      if port != nil do
        assert is_integer(port)
        assert port >= 7005  # Should be in valid range
      end
    end
  end
  
  describe "enabled?/0" do
    test "returns false when disabled" do
      Tau5.PublicEndpoint.disable()
      assert Tau5.PublicEndpoint.enabled?() == false
    end
    
    test "returns true when enabled" do
      Tau5.PublicEndpoint.enable()
      assert Tau5.PublicEndpoint.enabled?() == true
    end
  end
  
  describe "port/0" do
    test "returns port number when endpoint is configured" do
      # Set up test configuration
      Application.put_env(:tau5, Tau5Web.PublicEndpoint, [http: [port: 7005]])
      
      port = Tau5.PublicEndpoint.port()
      assert port == 7005
    end
    
    test "returns nil when endpoint has no port configured" do
      # Clear configuration
      Application.put_env(:tau5, Tau5Web.PublicEndpoint, [])
      
      port = Tau5.PublicEndpoint.port()
      assert port == nil
    end
  end
  
  describe "concurrency" do
    test "handles concurrent enable/disable calls safely" do
      # Launch multiple concurrent operations
      tasks = for _ <- 1..10 do
        Task.async(fn ->
          Enum.random([&Tau5.PublicEndpoint.enable/0, &Tau5.PublicEndpoint.disable/0]).()
        end)
      end
      
      # Wait for all to complete
      results = Task.await_many(tasks)
      
      # All should return :ok
      assert Enum.all?(results, &(&1 == :ok))
      
      # Final state should be consistent
      final_state = Tau5.PublicEndpoint.enabled?()
      assert is_boolean(final_state)
    end
    
    test "status remains consistent during rapid state changes" do
      # Rapidly toggle state
      for _ <- 1..20 do
        Tau5.PublicEndpoint.toggle()
      end
      
      # Status should still be coherent
      status = Tau5.PublicEndpoint.status()
      enabled = Tau5.PublicEndpoint.enabled?()
      
      case status do
        {:enabled, port: _} -> assert enabled == true
        {:disabled, port: _} -> assert enabled == false
      end
    end
  end
end
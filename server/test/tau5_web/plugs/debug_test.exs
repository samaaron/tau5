defmodule Tau5Web.Plugs.DebugTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.InternalEndpointSecurity
  
  test "basic localhost with token should work" do
    # Use the test token from config/test.exs
    token = Application.get_env(:tau5, :session_token)
    
    conn = 
      build_conn(:get, "/?token=#{token}")
      |> Plug.Test.init_test_session(%{})
      |> fetch_query_params()
      |> Map.put(:remote_ip, {127, 0, 0, 1})
    
    result = InternalEndpointSecurity.call(conn, [])
    
    refute result.halted, "Should not be halted"
  end
end
defmodule Tau5Web.Plugs.DebugTest do
  use Tau5Web.ConnCase
  alias Tau5Web.Plugs.InternalEndpointSecurity
  
  test "basic localhost with token should work" do
    System.put_env("TAU5_SESSION_TOKEN", "test123")
    
    conn = 
      build_conn(:get, "/?token=test123")
      |> Plug.Test.init_test_session(%{})
      |> fetch_query_params()
      |> Map.put(:remote_ip, {127, 0, 0, 1})
    
    IO.inspect(conn.query_params, label: "Query params")
    
    result = InternalEndpointSecurity.call(conn, [])
    
    IO.inspect(result.halted, label: "Halted")
    IO.inspect(result.status, label: "Status")
    if result.halted do
      # Extract the error reason from the HTML
      case Regex.run(~r/<div class="reason">(.+?)<\/div>/s, result.resp_body) do
        [_, reason] -> IO.puts("Error reason: #{reason}")
        _ -> IO.puts("Could not extract error reason")
      end
    end
    
    refute result.halted, "Should not be halted"
    
    System.delete_env("TAU5_SESSION_TOKEN")
  end
end
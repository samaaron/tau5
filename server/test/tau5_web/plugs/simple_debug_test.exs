defmodule SimpleDebugTest do
  use ExUnit.Case

  test "check environment and token" do
    System.put_env("TAU5_SESSION_TOKEN", "mytoken")

    expected = Application.get_env(:tau5, :session_token) || System.get_env("TAU5_SESSION_TOKEN")

    IO.puts("Expected token from env: #{inspect(expected)}")
    IO.puts("Direct env check: #{inspect(System.get_env("TAU5_SESSION_TOKEN"))}")
    IO.puts("Mix env: #{Mix.env()}")

    System.delete_env("TAU5_SESSION_TOKEN")
  end
end

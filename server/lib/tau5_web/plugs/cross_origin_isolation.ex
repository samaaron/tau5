defmodule Tau5Web.Plugs.CrossOriginIsolation do
  @moduledoc """
  Applies Cross-Origin Isolation headers required for SharedArrayBuffer.

  These headers enable SuperSonic (WebAssembly audio engine) to use
  SharedArrayBuffer for real-time audio processing.

  Required headers:
  - Cross-Origin-Opener-Policy: same-origin
  - Cross-Origin-Embedder-Policy: require-corp
  - Cross-Origin-Resource-Policy: cross-origin

  Note: These headers apply app-wide and may affect loading of external resources.
  """

  import Plug.Conn

  def init(opts), do: opts

  def call(conn, _opts) do
    conn
    |> put_resp_header("cross-origin-opener-policy", "same-origin")
    |> put_resp_header("cross-origin-embedder-policy", "require-corp")
    |> put_resp_header("cross-origin-resource-policy", "cross-origin")
  end
end

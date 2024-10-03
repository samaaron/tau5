defmodule Tau5Web.MainLive do
  use Tau5Web, :live_view

  def mount(_params, _session, socket) do
    {:ok, socket}
  end

  def render(assigns) do
    ~L"""
      <img src="/images/tau5-hirez.png" alt="Tau5 Logo" width="500" />
    """
  end
end

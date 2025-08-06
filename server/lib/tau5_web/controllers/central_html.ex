defmodule Tau5Web.CentralHTML do
  @moduledoc """
  This module contains pages rendered by CentralController.
  """
  use Tau5Web, :html

  embed_templates "central_html/*"
end
defmodule TidewaveMCP.ActivityLogger do
  @moduledoc """
  Logs Tidewave MCP tool activity to a JSONL file.
  """
  
  use Tau5.MCPActivityLogger,
    log_filename: "mcp-tidewave.log",
    session_type: "tidewave_session"
end
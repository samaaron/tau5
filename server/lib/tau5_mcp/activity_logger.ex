defmodule Tau5MCP.ActivityLogger do
  @moduledoc """
  Logs Tau5 MCP tool activity to a JSONL file.
  """
  
  use Tau5.MCPActivityLogger, 
    log_filename: "mcp-tau5.log",
    session_type: "server_session"
end
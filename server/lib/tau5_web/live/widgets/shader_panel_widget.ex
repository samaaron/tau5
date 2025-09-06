defmodule Tau5Web.Widgets.ShaderPanelWidget do
  @moduledoc """
  A panel widget that displays a WebGL shader with the panel index.
  Each instance gets a unique shader variation based on its index.
  """

  use Tau5Web, :live_component

  @doc false
  def render(assigns) do
    ~H"""
    <div
      class="shader-panel-widget"
      style="width: 100%; height: 100%; position: relative; background: #000;"
    >
      <script id={"vertex-shader-#{@id}"} type="x-shader/x-vertex">
        attribute vec2 aPos;
        void main() {
          gl_Position = vec4(aPos, 0.0, 1.0);
        }
      </script>

      <script id={"fragment-shader-#{@id}"} type="x-shader/x-fragment">
        <%= Phoenix.HTML.raw(fragment_shader(@index)) %>
      </script>

      <canvas
        id={"shader-canvas-#{@id}"}
        class="shader-canvas"
        phx-hook="ShaderCanvas"
        data-vertex-shader-id={"vertex-shader-#{@id}"}
        data-fragment-shader-id={"fragment-shader-#{@id}"}
        style="width: 100%; height: 100%; display: block;"
      >
      </canvas>

      <div
        class="shader-overlay"
        style="position: absolute; top: 10px; left: 10px; color: white; font-family: 'Cascadia Code PL', 'Cascadia Code', monospace; pointer-events: none;"
      >
        <div style={
          if @active,
            do: "font-size: 24px; font-weight: bold; font-style: italic; opacity: 1.0; color: white;",
            else:
              "font-size: 24px; font-weight: bold; font-style: italic; opacity: 0.3; color: white;"
        }>
          {@index}
        </div>
        <div style={
          if @active,
            do: "font-size: 10px; opacity: 0.7; color: white;",
            else: "font-size: 10px; opacity: 0.2; color: white;"
        }>
          {String.slice(@panel_id, 0, 8)}
        </div>
      </div>
    </div>
    """
  end

  @doc false
  def update(assigns, socket) do
    {:ok, assign(socket, assigns)}
  end

  # Different shaders based on panel index
  defp fragment_shader(0) do
    """
    precision mediump float;
    uniform float time;
    uniform vec2 resolution;

    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 p = uv * 2.0 - 1.0;
      p.x *= resolution.x / resolution.y;
      
      float t = time * 0.5;
      vec3 col = vec3(0.0);
      
      for(float i = 0.0; i < 3.0; i++) {
        float a = i * 2.094;
        vec2 q = p + vec2(cos(a + t), sin(a + t)) * 0.3;
        col[int(i)] = 0.5 + 0.5 * sin(length(q) * 10.0 - t * 2.0);
      }
      
      gl_FragColor = vec4(col, 1.0);
    }
    """
  end

  defp fragment_shader(1) do
    """
    precision mediump float;
    uniform float time;
    uniform vec2 resolution;

    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      float wave = sin(uv.x * 10.0 + time) * sin(uv.y * 10.0 + time * 1.3);
      vec3 col = vec3(0.0, wave * 0.5 + 0.5, 1.0 - wave * 0.5);
      gl_FragColor = vec4(col, 1.0);
    }
    """
  end

  defp fragment_shader(2) do
    """
    precision mediump float;
    uniform float time;
    uniform vec2 resolution;

    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 p = uv * 2.0 - 1.0;
      p.x *= resolution.x / resolution.y;
      
      float d = length(p);
      vec3 col = vec3(0.0);
      col.r = 0.5 + 0.5 * sin(d * 20.0 - time * 2.0);
      col.g = 0.5 + 0.5 * sin(d * 20.0 - time * 2.0 + 2.094);
      col.b = 0.5 + 0.5 * sin(time);
      
      gl_FragColor = vec4(col, 1.0);
    }
    """
  end

  defp fragment_shader(_) do
    # Default grid shader for panels 3+
    """
    precision mediump float;
    uniform float time;
    uniform vec2 resolution;

    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 grid = fract(uv * 10.0);
      
      float line = 0.0;
      if(grid.x < 0.05 || grid.y < 0.05) line = 1.0;
      
      vec3 col = vec3(line * 0.2);
      col.r += 0.5 + 0.5 * sin(time + uv.x * 10.0);
      col.b += 0.5 + 0.5 * cos(time + uv.y * 10.0);
      
      gl_FragColor = vec4(col, 1.0);
    }
    """
  end
end

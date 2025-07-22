export class Tau5Shader {
  constructor(canvas) {
    this.canvas = canvas;
    this.gl = null;
    this.program = null;
    this.startTime = Date.now();
    this.uniforms = {};
    this.animationFrame = null;
  }

  async init() {
    // Get WebGL context
    this.gl = this.canvas.getContext('webgl') || this.canvas.getContext('experimental-webgl');
    if (!this.gl) {
      console.error('WebGL not supported');
      return false;
    }

    // Create shaders
    const vertexShader = this.createShader(this.gl.VERTEX_SHADER, this.vertexShaderSource);
    const fragmentShader = this.createShader(this.gl.FRAGMENT_SHADER, this.fragmentShaderSource);
    
    if (!vertexShader || !fragmentShader) {
      return false;
    }

    // Create program
    this.program = this.gl.createProgram();
    this.gl.attachShader(this.program, vertexShader);
    this.gl.attachShader(this.program, fragmentShader);
    this.gl.linkProgram(this.program);

    if (!this.gl.getProgramParameter(this.program, this.gl.LINK_STATUS)) {
      console.error('Failed to link program:', this.gl.getProgramInfoLog(this.program));
      return false;
    }

    // Get uniform locations
    this.uniforms.time = this.gl.getUniformLocation(this.program, 'time');
    this.uniforms.resolution = this.gl.getUniformLocation(this.program, 'resolution');

    // Enable alpha blending
    this.gl.enable(this.gl.BLEND);
    this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);

    // Create vertex buffer
    const vertices = new Float32Array([
      -1.0, -1.0,
       1.0, -1.0,
      -1.0,  1.0,
       1.0,  1.0
    ]);

    const vertexBuffer = this.gl.createBuffer();
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, vertexBuffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, vertices, this.gl.STATIC_DRAW);

    const aPos = this.gl.getAttribLocation(this.program, 'aPos');
    this.gl.enableVertexAttribArray(aPos);
    this.gl.vertexAttribPointer(aPos, 2, this.gl.FLOAT, false, 0, 0);


    return true;
  }


  createShader(type, source) {
    const shader = this.gl.createShader(type);
    this.gl.shaderSource(shader, source);
    this.gl.compileShader(shader);

    if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS)) {
      console.error('Shader compile error:', this.gl.getShaderInfoLog(shader));
      this.gl.deleteShader(shader);
      return null;
    }

    return shader;
  }

  get vertexShaderSource() {
    return `
      attribute vec2 aPos;
      void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
      }
    `;
  }

  get fragmentShaderSource() {
    return `
      precision mediump float;
      uniform float time;
      uniform vec2 resolution;
      
      #define PI 3.14159265359
      
      mat2 rot(float a) {
        float s = sin(a), c = cos(a);
        return mat2(c, -s, s, c);
      }
      
      vec3 cubeWireframe(vec2 p) {
        vec3 col = vec3(0.0);
        
        float rotSpeed = 0.1;
        float t = time * rotSpeed;
        vec3 angles = vec3(t, t * 0.7, t * 0.3);
        
        // Define cube vertices
        vec3 verts[8];
        verts[0] = vec3(-1.0, -1.0, -1.0);
        verts[1] = vec3( 1.0, -1.0, -1.0);
        verts[2] = vec3( 1.0,  1.0, -1.0);
        verts[3] = vec3(-1.0,  1.0, -1.0);
        verts[4] = vec3(-1.0, -1.0,  1.0);
        verts[5] = vec3( 1.0, -1.0,  1.0);
        verts[6] = vec3( 1.0,  1.0,  1.0);
        verts[7] = vec3(-1.0,  1.0,  1.0);
        
        float scale = 0.3;
        
        // Project vertices
        vec2 proj[8];
        for(int i = 0; i < 8; i++) {
          vec3 v = verts[i];
          // Rotate
          v.yz = rot(angles.x) * v.yz;
          v.xz = rot(angles.y) * v.xz;
          v.xy = rot(angles.z) * v.xy;
          // Perspective projection
          float z = 4.0 + v.z;
          proj[i] = v.xy * (2.0 / z) * scale;
        }
        
        float d = 1e10;
        vec2 e;
        
        // Draw edges - front face
        e = proj[1] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
        e = proj[2] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
        e = proj[3] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
        e = proj[0] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
        
        // Back face
        e = proj[5] - proj[4]; d = min(d, length(p - proj[4] - e * clamp(dot(p - proj[4], e) / dot(e, e), 0.0, 1.0)));
        e = proj[6] - proj[5]; d = min(d, length(p - proj[5] - e * clamp(dot(p - proj[5], e) / dot(e, e), 0.0, 1.0)));
        e = proj[7] - proj[6]; d = min(d, length(p - proj[6] - e * clamp(dot(p - proj[6], e) / dot(e, e), 0.0, 1.0)));
        e = proj[4] - proj[7]; d = min(d, length(p - proj[7] - e * clamp(dot(p - proj[7], e) / dot(e, e), 0.0, 1.0)));
        
        // Connecting edges
        e = proj[4] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
        e = proj[5] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
        e = proj[6] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
        e = proj[7] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
        
        vec3 cubeColor = vec3(1.0, 0.65, 0.0); // Orange
        col += cubeColor * smoothstep(0.02, 0.0, d) * 2.0;
        
        return col;
      }
      
      void main() {
        vec2 uv = gl_FragCoord.xy / resolution.xy;
        vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
        
        // Transparent background
        vec3 col = vec3(0.0);
        
        // Add cube wireframe
        col += cubeWireframe(p);
        
        // Vignette
        col *= 1.0 - length(p) * 0.5;
        
        // Output with alpha based on brightness
        float alpha = max(col.r, max(col.g, col.b));
        gl_FragColor = vec4(col, alpha);
      }
    `;
  }

  resize() {
    const dpr = window.devicePixelRatio || 1;
    const width = this.canvas.clientWidth * dpr;
    const height = this.canvas.clientHeight * dpr;
    
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width;
      this.canvas.height = height;
      this.gl.viewport(0, 0, width, height);
    }
  }

  render() {
    if (!this.gl || !this.program) return;

    this.resize();

    this.gl.clearColor(0, 0, 0, 0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);

    this.gl.useProgram(this.program);

    // Set uniforms
    const time = (Date.now() - this.startTime) / 1000.0;
    this.gl.uniform1f(this.uniforms.time, time);
    this.gl.uniform2f(this.uniforms.resolution, this.canvas.width, this.canvas.height);


    // Draw
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
  }

  start() {
    const animate = () => {
      this.render();
      this.animationFrame = requestAnimationFrame(animate);
    };
    animate();
  }

  stop() {
    if (this.animationFrame) {
      cancelAnimationFrame(this.animationFrame);
      this.animationFrame = null;
    }
  }

  destroy() {
    this.stop();
    if (this.gl && this.program) {
      this.gl.deleteProgram(this.program);
    }
  }
}
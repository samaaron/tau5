export class Tau5Shader {
  constructor(canvas) {
    this.canvas = canvas;
    this.gl = null;
    this.program = null;
    this.startTime = Date.now();
    this.uniforms = {};
    this.animationFrame = null;

    // Mouse interaction state
    this.isDragging = false;
    this.lastMouseX = 0;
    this.lastMouseY = 0;

    // Camera rotation (view space) - controlled by mouse
    this.cameraVelocityX = 0;  // Pitch velocity
    this.cameraVelocityY = 0;  // Yaw velocity
    this.cameraPitch = 0;      // Camera pitch (up/down)
    this.cameraYaw = 0;        // Camera yaw (left/right)

    // Auto rotation (object space) - automatic spinning
    this.autoRotationTime = 0;
    this.baseSpeed = 0.05; // Auto rotation speed
    this.damping = 0.985; // Inertia damping factor (higher = longer spin)
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
    this.uniforms.autoRotation = this.gl.getUniformLocation(this.program, 'autoRotation');
    this.uniforms.cameraRotation = this.gl.getUniformLocation(this.program, 'cameraRotation');

    // Enable alpha blending
    this.gl.enable(this.gl.BLEND);
    this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);
    
    // Set up mouse event listeners
    this.setupMouseEvents();

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
      uniform vec3 autoRotation;
      uniform vec2 cameraRotation;

      #define PI 3.14159265359

      mat2 rot(float a) {
        float s = sin(a), c = cos(a);
        return mat2(c, -s, s, c);
      }

      vec3 cubeWireframe(vec2 p) {
        vec3 col = vec3(0.0);

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

          // Apply auto-rotation (object space)
          v.yz *= rot(autoRotation.x);
          v.xz *= rot(autoRotation.y);
          v.xy *= rot(autoRotation.z);

          // Apply camera rotation (view space) - this gives perfect screen-space control
          v.xz *= rot(cameraRotation.y); // Yaw (horizontal mouse movement)
          v.yz *= rot(cameraRotation.x); // Pitch (vertical mouse movement)

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
        
        vec3 cubeColor = vec3(1.0, 0.843, 0.0); // Golden yellow (#FFD700)
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

  setupMouseEvents() {
    // Mouse down - start dragging
    this.canvas.addEventListener('mousedown', (e) => {
      this.isDragging = true;
      this.lastMouseX = e.clientX;
      this.lastMouseY = e.clientY;
      this.cameraVelocityX = 0;
      this.cameraVelocityY = 0;
      this.canvas.style.cursor = 'grabbing';
    });

    // Mouse move - update rotation
    window.addEventListener('mousemove', (e) => {
      if (!this.isDragging) return;

      const deltaX = e.clientX - this.lastMouseX;
      const deltaY = e.clientY - this.lastMouseY;

      // Camera rotation - perfect screen-space control
      const rotationSpeed = 0.01;

      // Update camera velocities (inverted Y for intuitive control)
      this.cameraVelocityX = -deltaY * rotationSpeed;  // Vertical drag -> pitch (inverted)
      this.cameraVelocityY = deltaX * rotationSpeed;   // Horizontal drag -> yaw

      // Apply camera rotations
      this.cameraPitch += this.cameraVelocityX;
      this.cameraYaw += this.cameraVelocityY;

      this.lastMouseX = e.clientX;
      this.lastMouseY = e.clientY;
    });

    // Mouse up - stop dragging
    window.addEventListener('mouseup', () => {
      this.isDragging = false;
      this.canvas.style.cursor = 'grab';
    });

    // Add hover cursor
    this.canvas.style.cursor = 'grab';
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

    // Auto-rotation (always active)
    const time = (Date.now() - this.startTime) / 1000.0;
    this.autoRotationTime = time * this.baseSpeed;

    // Update camera physics when not dragging
    if (!this.isDragging) {
      // Apply inertia to camera rotation
      this.cameraPitch += this.cameraVelocityX;
      this.cameraYaw += this.cameraVelocityY;

      // Apply damping to camera velocity
      this.cameraVelocityX *= this.damping;
      this.cameraVelocityY *= this.damping;

      // Stop when velocity is very small
      const minVelocity = 0.00001;
      if (Math.abs(this.cameraVelocityX) < minVelocity) this.cameraVelocityX = 0;
      if (Math.abs(this.cameraVelocityY) < minVelocity) this.cameraVelocityY = 0;
    }

    // Set uniforms
    this.gl.uniform1f(this.uniforms.time, time);
    this.gl.uniform2f(this.uniforms.resolution, this.canvas.width, this.canvas.height);

    // Auto-rotation angles (object space)
    const autoRotX = this.autoRotationTime;
    const autoRotY = this.autoRotationTime * 0.7;
    const autoRotZ = this.autoRotationTime * 0.3;
    this.gl.uniform3f(this.uniforms.autoRotation, autoRotX, autoRotY, autoRotZ);

    // Camera rotation (view space)
    this.gl.uniform2f(this.uniforms.cameraRotation, this.cameraPitch, this.cameraYaw);

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
    // Remove cursor style
    this.canvas.style.cursor = '';
  }
}
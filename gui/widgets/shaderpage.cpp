#include "shaderpage.h"
#include "shadercode.h"

QString ShaderPage::s_cachedHtml;
bool ShaderPage::s_initialized = false;

const QString& ShaderPage::getHtml()
{
    if (!s_initialized) {
        s_cachedHtml = generateHtml();
        s_initialized = true;
    }
    return s_cachedHtml;
}

QString ShaderPage::generateHtml()
{
    QString vertexShader = ShaderCode::vertexShader;
    QString fragmentShader = ShaderCode::fragmentShader;
    
    return R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    html, body { 
      width: 100%; 
      height: 100%; 
      overflow: hidden; 
      background: #000; 
    }
    #canvas { 
      position: fixed; 
      top: 0; 
      left: 0; 
      width: 100%; 
      height: 100%; 
      display: block;
    }
    #logo-img { display: none; }
  </style>
</head>
<body>
  <canvas id="canvas"></canvas>
  <img id="logo-img" src="qrc:/images/tau5-bw-hirez.png" />
  
  <script>
    const vertexShaderSource = `)" + vertexShader + R"(`;
    const fragmentShaderSource = `)" + fragmentShader + R"(`;
    
    async function init() {
      const canvas = document.getElementById('canvas');
      const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
      
      if (!gl) {
        console.error('WebGL not supported');
        document.body.style.background = 'linear-gradient(135deg, #1a0033 0%, #330066 100%)';
        document.body.innerHTML = '<div style="position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); color: #ffa500; font-family: monospace; font-size: 24px; text-align: center;">TAU5<br><span style="font-size: 14px;">Loading...</span></div>';
        window.webGLFailed = true;
        return;
      }
      
      function compileShader(source, type) {
        const shader = gl.createShader(type);
        gl.shaderSource(shader, source);
        gl.compileShader(shader);
        
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
          console.error('Shader compilation error:', gl.getShaderInfoLog(shader));
          gl.deleteShader(shader);
          return null;
        }
        return shader;
      }
      
      const vertexShader = compileShader(vertexShaderSource, gl.VERTEX_SHADER);
      const fragmentShader = compileShader(fragmentShaderSource, gl.FRAGMENT_SHADER);
      
      const program = gl.createProgram();
      gl.attachShader(program, vertexShader);
      gl.attachShader(program, fragmentShader);
      gl.linkProgram(program);
      
      if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
        console.error('Program linking error:', gl.getProgramInfoLog(program));
      }
      
      gl.useProgram(program);
      
      // Set up geometry
      const vertices = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
      const buffer = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
      
      const aPos = gl.getAttribLocation(program, 'aPos');
      gl.enableVertexAttribArray(aPos);
      gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);
      
      // Get uniform locations
      const timeUniform = gl.getUniformLocation(program, 'time');
      const resolutionUniform = gl.getUniformLocation(program, 'resolution');
      const logoTextureUniform = gl.getUniformLocation(program, 'logoTexture');
      const fadeUniform = gl.getUniformLocation(program, 'fadeValue');
      const cameraRotationUniform = gl.getUniformLocation(program, 'cameraRotation');
      
      // Expose gl and fadeUniform to window for fade animation
      window.gl = gl;
      window.fadeUniform = fadeUniform;
      
      // Load logo texture
      const logoImg = document.getElementById('logo-img');
      const logoTexture = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, logoTexture);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      
      logoImg.onload = function() {
        gl.bindTexture(gl.TEXTURE_2D, logoTexture);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, logoImg);
      };
      
      if (logoImg.complete) {
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, logoImg);
      }
      
      // Mouse interaction state
      let isDragging = false;
      let lastMouseX = 0;
      let lastMouseY = 0;
      let cameraVelocityX = 0;
      let cameraVelocityY = 0;
      let cameraPitch = 0;
      let cameraYaw = 0;
      const damping = 0.985;
      const minVelocity = 0.00001;
      
      // Mouse events
      function getEventPosition(e) {
        if (e.touches && e.touches.length > 0) {
          return { x: e.touches[0].clientX, y: e.touches[0].clientY };
        }
        return { x: e.clientX, y: e.clientY };
      }
      
      function startDrag(e) {
        e.preventDefault();
        isDragging = true;
        const pos = getEventPosition(e);
        lastMouseX = pos.x;
        lastMouseY = pos.y;
        cameraVelocityX = 0;
        cameraVelocityY = 0;
        canvas.style.cursor = 'grabbing';
      }
      
      function moveDrag(e) {
        if (!isDragging) return;
        e.preventDefault();
        
        const pos = getEventPosition(e);
        const deltaX = pos.x - lastMouseX;
        const deltaY = pos.y - lastMouseY;
        
        const rotationSpeed = 0.01;
        cameraVelocityX = -deltaY * rotationSpeed;
        cameraVelocityY = deltaX * rotationSpeed;
        
        cameraPitch += cameraVelocityX;
        cameraYaw += cameraVelocityY;
        
        lastMouseX = pos.x;
        lastMouseY = pos.y;
      }
      
      function endDrag(e) {
        isDragging = false;
        canvas.style.cursor = 'grab';
      }
      
      canvas.addEventListener('mousedown', startDrag);
      window.addEventListener('mousemove', moveDrag);
      window.addEventListener('mouseup', endDrag);
      canvas.addEventListener('touchstart', startDrag, { passive: false });
      canvas.addEventListener('touchmove', moveDrag, { passive: false });
      canvas.addEventListener('touchend', endDrag, { passive: false });
      canvas.addEventListener('touchcancel', endDrag, { passive: false });
      canvas.style.cursor = 'grab';
      
      function resize() {
        const dpr = window.devicePixelRatio || 1;
        const width = window.innerWidth;
        const height = window.innerHeight;
        
        canvas.width = width * dpr;
        canvas.height = height * dpr;
        canvas.style.width = width + 'px';
        canvas.style.height = height + 'px';
        
        gl.viewport(0, 0, canvas.width, canvas.height);
      }
      resize();
      window.addEventListener('resize', resize);
      
      const startTime = Date.now();
      
      function render() {
        const time = (Date.now() - startTime) / 1000.0;
        
        if (!isDragging) {
          cameraPitch += cameraVelocityX;
          cameraYaw += cameraVelocityY;
          cameraVelocityX *= damping;
          cameraVelocityY *= damping;
          
          if (Math.abs(cameraVelocityX) < minVelocity) cameraVelocityX = 0;
          if (Math.abs(cameraVelocityY) < minVelocity) cameraVelocityY = 0;
        }
        
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);
        
        gl.uniform1f(timeUniform, time);
        gl.uniform2f(resolutionUniform, canvas.width, canvas.height);
        gl.uniform1i(logoTextureUniform, 0);
        gl.uniform1f(fadeUniform, 0.0);
        gl.uniform2f(cameraRotationUniform, cameraPitch, cameraYaw);
        
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, logoTexture);
        
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
        
        requestAnimationFrame(render);
      }
      
      render();
    }
    
    init().catch(err => console.error('Failed to initialize:', err));
  </script>
</body>
</html>)";
}